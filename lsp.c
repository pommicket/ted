static void lsp_request_free(LSPRequest *r);
static void lsp_response_free(LSPResponse *r);

#define lsp_set_error(lsp, ...) do {\
		SDL_LockMutex(lsp->error_mutex);\
		strbuf_printf(lsp->error, __VA_ARGS__);\
		SDL_UnlockMutex(lsp->error_mutex);\
	} while (0)
#include "lsp-write.c"
#include "lsp-parse.c"

bool lsp_get_error(LSP *lsp, char *error, size_t error_size, bool clear) {
	bool has_err = false;
	SDL_LockMutex(lsp->error_mutex);
	has_err = *lsp->error != '\0';
	if (error_size)
		str_cpy(error, error_size, lsp->error);
	if (clear)
		*lsp->error = '\0';
	SDL_UnlockMutex(lsp->error_mutex);
	return has_err;
}


static void lsp_document_change_event_free(LSPDocumentChangeEvent *event) {
	free(event->text);
}

static void lsp_request_free(LSPRequest *r) {
	switch (r->type) {
	case LSP_REQUEST_NONE:
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_DID_CLOSE:
		break;
	case LSP_REQUEST_DID_OPEN: {
		LSPRequestDidOpen *open = &r->data.open;
		free(open->file_contents);
		} break;
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
		free(r->data.message.message);
		break;
	case LSP_REQUEST_DID_CHANGE: {
		LSPRequestDidChange *c = &r->data.change;
		arr_foreach_ptr(c->changes, LSPDocumentChangeEvent, event)
			lsp_document_change_event_free(event);
		arr_free(c->changes);
		} break;
	}
}

static void lsp_response_free(LSPResponse *r) {
	arr_free(r->string_data);
	switch (r->request.type) {
	case LSP_REQUEST_COMPLETION:
		arr_free(r->data.completion.items);
		break;
	default:
		break;
	}
	lsp_request_free(&r->request);
}

void lsp_message_free(LSPMessage *message) {
	switch (message->type) {
	case LSP_REQUEST:
		lsp_request_free(&message->u.request);
		break;
	case LSP_RESPONSE:
		lsp_response_free(&message->u.response);
		break;
	}
	memset(message, 0, sizeof *message);
}


// figure out if data begins with a complete LSP response.
static bool has_response(const char *data, size_t data_len, u64 *p_offset, u64 *p_size) {
	const char *content_length = strstr(data, "Content-Length");
	if (!content_length) return false;
	const char *p = content_length + strlen("Content-Length");
	if (!p[0] || !p[1] || !p[2]) return false;
	p += 2;
	size_t size = (size_t)atoll(p);
	*p_size = size;
	const char *header_end = strstr(content_length, "\r\n\r\n");
	if (!header_end) return false;
	header_end += 4;
	u64 offset = (u64)(header_end - data);
	*p_offset = offset;
	return offset + size <= data_len;
}

void lsp_send_request(LSP *lsp, const LSPRequest *request) {
	SDL_LockMutex(lsp->requests_mutex);
	arr_add(lsp->requests_client2server, *request);
	SDL_UnlockMutex(lsp->requests_mutex);
}

const char *lsp_response_string(const LSPResponse *response, LSPString string) {
	assert(string.offset < arr_len(response->string_data));
	return &response->string_data[string.offset];
}

// receive responses/requests/notifications from LSP, up to max_size bytes.
static void lsp_receive(LSP *lsp, size_t max_size) {

	{
		// read stderr. if all goes well, we shouldn't get anything over stderr.
		char stderr_buf[1024] = {0};
		for (size_t i = 0; i < (max_size + sizeof stderr_buf) / sizeof stderr_buf; ++i) {
			ssize_t nstderr = process_read_stderr(&lsp->process, stderr_buf, sizeof stderr_buf - 1);
			if (nstderr > 0) {
				// uh oh
				stderr_buf[nstderr] = '\0';
				fprintf(stderr, "\x1b[1m\x1b[93m%s\x1b[0m", stderr_buf);
			} else {
				break;
			}
		}
	}

	size_t received_so_far = arr_len(lsp->received_data);
	arr_reserve(lsp->received_data, received_so_far + max_size + 1);
	long long bytes_read = process_read(&lsp->process, lsp->received_data + received_so_far, max_size);
	if (bytes_read <= 0) {
		// no data
		return;
	}
	received_so_far += (size_t)bytes_read;
	// kind of a hack. this is needed because arr_set_len zeroes the data.
	arr_hdr_(lsp->received_data)->len = (u32)received_so_far;
	lsp->received_data[received_so_far] = '\0';// null terminate
	#if 0
	printf("\x1b[3m%s\x1b[0m\n",lsp->received_data);
	#endif
	
	u64 response_offset=0, response_size=0;
	while (has_response(lsp->received_data, received_so_far, &response_offset, &response_size)) {
		if (response_offset + response_size > arr_len(lsp->received_data)) {
			// we haven't received this whole response yet.
			break;
		}
		
		char *copy = strn_dup(lsp->received_data + response_offset, response_size);
		JSON json = {0};
		if (json_parse(&json, copy)) {
			assert(json.text == copy);
			json.is_text_copied = true;
			process_message(lsp, &json);
		} else {
			lsp_set_error(lsp, "couldn't parse response JSON: %s", json.error);
			json_free(&json);
		}
		size_t leftover_data_len = arr_len(lsp->received_data) - (response_offset + response_size);
		
		//printf("arr_cap = %u response_offset = %u, response_size = %zu, leftover len = %u\n",
		//	arr_hdr_(lsp->received_data)->cap,
		//	response_offset, response_size, leftover_data_len);
		memmove(lsp->received_data, lsp->received_data + response_offset + response_size,
			leftover_data_len);
		arr_set_len(lsp->received_data, leftover_data_len);
		arr_reserve(lsp->received_data, leftover_data_len + 1);
		lsp->received_data[leftover_data_len] = '\0';
	}
}

// send requests.
static bool lsp_send(LSP *lsp) {
	if (!lsp->initialized) {
		// don't send anything before the server is initialized.
		return false;
	}
	
	LSPRequest *requests = NULL;
	SDL_LockMutex(lsp->requests_mutex);
	size_t n_requests = arr_len(lsp->requests_client2server);
	requests = calloc(n_requests, sizeof *requests);
	memcpy(requests, lsp->requests_client2server, n_requests * sizeof *requests);
	arr_clear(lsp->requests_client2server);
	SDL_UnlockMutex(lsp->requests_mutex);

	bool quit = false;
	for (size_t i = 0; i < n_requests; ++i) {
		LSPRequest *r = &requests[i];
		if (!quit) {
			// this could slow down lsp_free if there's a gigantic request.
			// whatever.
			write_request(lsp, r);
		}
		
		if (SDL_SemTryWait(lsp->quit_sem) == 0) {
			quit = true;
			// important that we don't break here so all the requests get freed.
		}
	}

	free(requests);
	return quit;
}


// Do any necessary communication with the LSP.
// This writes requests and reads (and parses) responses.
static int lsp_communication_thread(void *data) {
	LSP *lsp = data;
	while (1) {
		bool quit = lsp_send(lsp);
		if (quit) break;
		
		lsp_receive(lsp, (size_t)10<<20);
		if (SDL_SemWaitTimeout(lsp->quit_sem, 5) == 0)
			break;	
	}
	
	if (lsp->initialized) {
		LSPRequest shutdown = {
			.type = LSP_REQUEST_SHUTDOWN,
			.data = {0}
		};
		LSPRequest exit = {
			.type = LSP_REQUEST_EXIT,
			.data = {0}
		};
		write_request(lsp, &shutdown);
		// i give you ONE MILLISECOND to send your fucking shutdown response
		time_sleep_ms(1);
		write_request(lsp, &exit);
		// i give you ONE MILLISECOND to terminate
		// I WILL KILL YOU IF IT TAKES ANY LONGER
		time_sleep_ms(1);
		
		#if 0
		char buf[1024]={0};
		long long n = process_read(&lsp->process, buf, sizeof buf);
		if (n>0) {
			buf[n]=0;
			printf("%s\n",buf);
		}
		n = process_read_stderr(&lsp->process, buf, sizeof buf);
		if (n>0) {
			buf[n]=0;
			printf("\x1b[1m%s\x1b[0m\n",buf);
		}
		#endif
	}
	return 0;
}

u32 lsp_document_id(LSP *lsp, const char *path) {
	u32 *value = str_hash_table_get(&lsp->document_ids, path);
	if (!value) {
		u32 id = arr_len(lsp->document_paths);
		value = str_hash_table_insert(&lsp->document_ids, path);
		*value = id;
		arr_add(lsp->document_paths, str_dup(path));
	}
	return *value;
}

bool lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = false,
		.stderr_blocking = false,
		.separate_stderr = true,
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
	LSPRequest initialize = {
		.type = LSP_REQUEST_INITIALIZE
	};
	// immediately send the request rather than queueing it.
	// this is a small request, so it shouldn't be a problem.
	write_request(lsp, &initialize);
	
	str_hash_table_create(&lsp->document_ids, sizeof(u32));
	lsp->quit_sem = SDL_CreateSemaphore(0);	
	lsp->messages_mutex = SDL_CreateMutex();
	lsp->requests_mutex = SDL_CreateMutex();
	lsp->communication_thread = SDL_CreateThread(lsp_communication_thread, "LSP communicate", lsp);
	return true;
}

bool lsp_next_message(LSP *lsp, LSPMessage *message) {
	bool any = false;
	SDL_LockMutex(lsp->messages_mutex);
	if (arr_len(lsp->messages)) {
		*message = lsp->messages[0];
		arr_remove(lsp->messages, 0);
		any = true;
	}
	SDL_UnlockMutex(lsp->messages_mutex);
	return any;
}

void lsp_free(LSP *lsp) {
	SDL_SemPost(lsp->quit_sem);
	SDL_WaitThread(lsp->communication_thread, NULL);
	SDL_DestroyMutex(lsp->messages_mutex);
	SDL_DestroyMutex(lsp->requests_mutex);
	SDL_DestroySemaphore(lsp->quit_sem);
	process_kill(&lsp->process);
	arr_free(lsp->received_data);
	str_hash_table_clear(&lsp->document_ids);
	for (size_t i = 0; i < arr_len(lsp->document_paths); ++i)
		free(lsp->document_paths[i]);
	arr_clear(lsp->document_paths);
	arr_foreach_ptr(lsp->messages, LSPMessage, message) {
		lsp_message_free(message);
	}
	arr_free(lsp->messages);
}

void lsp_document_changed(LSP *lsp, const char *document, LSPDocumentChangeEvent change) {
	// @TODO(optimization, eventually): batch changes (using the contentChanges array)
	LSPRequest request = {.type = LSP_REQUEST_DID_CHANGE};
	LSPRequestDidChange *c = &request.data.change;
	c->document = lsp_document_id(lsp, document);
	arr_add(c->changes, change);
	lsp_send_request(lsp, &request);
}
