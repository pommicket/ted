// main file for dealing with LSP servers

#define LSP_INTERNAL 1
#include "lsp.h"
#include "util.h"

const char *language_to_str(Language language);

static LSPMutex id_mutex;

// it's nice to have request IDs be totally unique, including across LSP servers.
static LSPRequestID get_request_id(void) {
	// it's important that this never returns 0, since that's reserved for "no ID"
	static LSPRequestID last_request_id;
	LSPRequestID id = 0;
	assert(id_mutex);
	SDL_LockMutex(id_mutex);
		id = ++last_request_id;
	SDL_UnlockMutex(id_mutex);
	return id;
}

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

void lsp_request_free(LSPRequest *r) {
	free(r->id_string);
	switch (r->type) {
	case LSP_REQUEST_NONE:
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_SIGNATURE_HELP:
	case LSP_REQUEST_HOVER:
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION:
	case LSP_REQUEST_REFERENCES:
	case LSP_REQUEST_HIGHLIGHT:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		break;
	case LSP_REQUEST_CONFIGURATION: {
		LSPRequestConfiguration *config = &r->data.configuration;
		free(config->settings);
		} break;
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
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS: {
		LSPRequestDidChangeWorkspaceFolders *w = &r->data.change_workspace_folders;
		arr_free(w->added);
		arr_free(w->removed);
		} break;
	case LSP_REQUEST_RENAME:
		free(r->data.rename.new_name);
		break;
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		free(r->data.workspace_symbols.query);
		break;
	}
	memset(r, 0, sizeof *r);
}

void lsp_response_free(LSPResponse *r) {
	arr_free(r->string_data);
	switch (r->request.type) {
	case LSP_REQUEST_COMPLETION:
		arr_free(r->data.completion.items);
		break;
	case LSP_REQUEST_SIGNATURE_HELP:
		arr_free(r->data.signature_help.signatures);
		break;
	case LSP_REQUEST_DEFINITION:
		arr_free(r->data.definition.locations);
		break;
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		arr_free(r->data.workspace_symbols.symbols);
		break;
	case LSP_REQUEST_RENAME:
		arr_free(r->data.rename.changes);
		break;
	case LSP_REQUEST_HIGHLIGHT:
		arr_free(r->data.highlight.highlights);
		break;
	case LSP_REQUEST_REFERENCES:
		arr_free(r->data.references.locations);
		break;
	default:
		break;
	}
	lsp_request_free(&r->request);
	free(r->error);
	memset(r, 0, sizeof *r);
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

static bool lsp_supports_request(LSP *lsp, const LSPRequest *request) {
	LSPCapabilities *cap = &lsp->capabilities;
	switch (request->type) {
	case LSP_REQUEST_NONE:
	// return false for server-to-client requests since we should never send them
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		return false;
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_DID_CHANGE:
	case LSP_REQUEST_CONFIGURATION:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_EXIT:
		return true;
	case LSP_REQUEST_COMPLETION:
		return cap->completion_support;
	case LSP_REQUEST_SIGNATURE_HELP:
		return cap->signature_help_support;
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
		return cap->workspace_folders_support;
	case LSP_REQUEST_HOVER:
		return cap->hover_support;
	case LSP_REQUEST_DEFINITION:
		return cap->definition_support;
	case LSP_REQUEST_DECLARATION:
		return cap->declaration_support;
	case LSP_REQUEST_TYPE_DEFINITION:
		return cap->type_definition_support;
	case LSP_REQUEST_IMPLEMENTATION:
		return cap->implementation_support;
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		return cap->workspace_symbols_support;
	case LSP_REQUEST_RENAME:
		return cap->rename_support;
	case LSP_REQUEST_HIGHLIGHT:
		return cap->highlight_support;
	case LSP_REQUEST_REFERENCES:
		return cap->references_support;
	}
	assert(0);
	return false;
}

void lsp_send_message(LSP *lsp, LSPMessage *message) {
	SDL_LockMutex(lsp->messages_mutex);
	arr_add(lsp->messages_client2server, *message);
	SDL_UnlockMutex(lsp->messages_mutex);
}

static bool request_type_is_notification(LSPRequestType type) {
	switch (type) {
	case LSP_REQUEST_NONE: break;
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_DID_CHANGE:
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
	case LSP_REQUEST_CONFIGURATION:
		return true;
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_HIGHLIGHT:
	case LSP_REQUEST_SIGNATURE_HELP:
	case LSP_REQUEST_HOVER:
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION:
	case LSP_REQUEST_REFERENCES:
	case LSP_REQUEST_RENAME:
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		return false;
	}
	assert(0);
	return false;
}

LSPServerRequestID lsp_send_request(LSP *lsp, LSPRequest *request) {
	if (!lsp_supports_request(lsp, request)) {
		lsp_request_free(request);
		return (LSPServerRequestID){0};
	}
	
	bool is_notification = request_type_is_notification(request->type);
	if (!is_notification)
		request->id = get_request_id();
	LSPMessage message = {.type = LSP_REQUEST};
	message.u.request = *request;
	lsp_send_message(lsp, &message);
	return (LSPServerRequestID) {
		.lsp = lsp->id,
		.id = request->id
	};
}

void lsp_send_response(LSP *lsp, LSPResponse *response) {
	LSPMessage message = {.type = LSP_RESPONSE};
	message.u.response = *response;
	lsp_send_message(lsp, &message);
}

const char *lsp_response_string(const LSPResponse *response, LSPString string) {
	assert(string.offset < arr_len(response->string_data));
	return &response->string_data[string.offset];
}

// receive responses/requests/notifications from LSP, up to max_size bytes.
// returns false if the process exited
static bool lsp_receive(LSP *lsp, size_t max_size) {

	{
		// read stderr. if all goes well, we shouldn't get anything over stderr.
		char stderr_buf[1024] = {0};
		for (size_t i = 0; i < (max_size + sizeof stderr_buf) / sizeof stderr_buf; ++i) {
			ssize_t nstderr = process_read_stderr(lsp->process, stderr_buf, sizeof stderr_buf - 1);
			if (nstderr > 0) {
				// uh oh
				stderr_buf[nstderr] = '\0';
				if (lsp->log) {
					fprintf(lsp->log, "LSP SERVER STDERR\n%s\n\n", stderr_buf);
				}
				eprint("%s%s%s%s", term_bold(stderr), term_yellow(stderr), stderr_buf, term_clear(stderr));
			} else {
				break;
			}
		}
	}
	
	{
		// check process status
		ProcessExitInfo info = {0};
		int status = process_check_status(&lsp->process, &info);
		if (status != 0) {
			bool not_found =
			#if _WIN32
			#error "@TODO: what status does cmd return if the program is not found?"
			#else
				info.exit_code == 127;
			#endif
			
			if (not_found) {
				// don't give an error if the server is not found.
				// still log it though.
				if (lsp->log)
					fprintf(lsp->log, "LSP server exited: %s. Probably the server is not installed.",
						info.message);
			} else {
				lsp_set_error(lsp, "Can't access LSP server: %s\n"
					"Run ted in a terminal or set lsp-log = on for more details."
					, info.message);
			}
			return false;
		}
		
	}

	size_t received_so_far = arr_len(lsp->received_data);
	arr_reserve(lsp->received_data, received_so_far + max_size + 1);
	long long bytes_read = process_read(lsp->process, lsp->received_data + received_so_far, max_size);
	if (bytes_read <= 0) {
		// no data
		return true;
	}
	received_so_far += (size_t)bytes_read;
	// kind of a hack. this is needed because arr_set_len zeroes the data.
	arr_hdr_(lsp->received_data)->len = (u32)received_so_far;
	lsp->received_data[received_so_far] = '\0';// null terminate
	#if LSP_SHOW_S2C
	printf("%s%s%s\n",term_italics(stdout),lsp->received_data,term_clear(stdout));
	#endif
	
	u64 response_offset=0, response_size=0;
	while (has_response(lsp->received_data, received_so_far, &response_offset, &response_size)) {
		if (response_offset + response_size > arr_len(lsp->received_data)) {
			// we haven't received this whole response yet.
			break;
		}
		
		char *copy = strn_dup(lsp->received_data + response_offset, response_size);
		if (lsp->log) {
			fprintf(lsp->log, "LSP MESSAGE FROM SERVER TO CLIENT\n%s\n\n", copy);
		}
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
	return true;
}

// send requests.
static bool lsp_send(LSP *lsp) {
	if (!lsp->initialized) {
		// don't send anything before the server is initialized.
		return false;
	}
	
	LSPMessage *messages = NULL;
	SDL_LockMutex(lsp->messages_mutex);
	size_t n_messages = arr_len(lsp->messages_client2server);
	messages = calloc(n_messages, sizeof *messages);
	memcpy(messages, lsp->messages_client2server, n_messages * sizeof *messages);
	
	#if __GNUC__ && !__clang__
	#pragma GCC diagnostic push
	// i don't know why GCC is giving me this. some compiler bug.
	#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
	#endif
	arr_clear(lsp->messages_client2server);
	#if __GNUC__ && !__clang__
	#pragma GCC diagnostic pop
	#endif
	
	SDL_UnlockMutex(lsp->messages_mutex);

	bool quit = false;
	for (size_t i = 0; i < n_messages; ++i) {
		LSPMessage *m = &messages[i];
		if (quit) {
			lsp_message_free(m);
		} else {
			write_message(lsp, m);
		}
		
		if (SDL_SemTryWait(lsp->quit_sem) == 0) {
			quit = true;
		}
	}

	free(messages);
	return quit;
}


// Do any necessary communication with the LSP.
// This writes requests and reads (and parses) responses.
static int lsp_communication_thread(void *data) {
	LSP *lsp = data;
	while (1) {
		bool quit = lsp_send(lsp);
		if (quit) break;
		
		if (!lsp_receive(lsp, (size_t)10<<20))
			break;
		if (SDL_SemWaitTimeout(lsp->quit_sem, 5) == 0)
			break;	
	}
	
	lsp->exited = true;
	
	if (!lsp->process) {
		// process already exited
		return 0;
	}
	
	if (lsp->initialized) {
		LSPRequest shutdown = {
			.type = LSP_REQUEST_SHUTDOWN,
			.data = {{0}}
		};
		LSPRequest exit = {
			.type = LSP_REQUEST_EXIT,
			.data = {{0}}
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
	if (!path) {
		assert(0);
		return 0;
	}
	SDL_LockMutex(lsp->document_mutex);
		u32 *value = str_hash_table_get(&lsp->document_ids, path);
		if (!value) {
			u32 id = arr_len(lsp->document_data);
			value = str_hash_table_insert(&lsp->document_ids, path);
			*value = id;
			LSPDocumentData *data = arr_addp(lsp->document_data);
			data->path = str_dup(path);
		}
		u32 id = *value;
	SDL_UnlockMutex(lsp->document_mutex);
	return id;
}

const char *lsp_document_path(LSP *lsp, LSPDocumentID document) {
	SDL_LockMutex(lsp->document_mutex);
		if (document >= arr_len(lsp->document_data)) {
			assert(0);
			return "";
		}
		// it's okay to keep a pointer to this around without the mutex locked
		// we'll never change the path of a document ID.
		const char *path = lsp->document_data[document].path;
	SDL_UnlockMutex(lsp->document_mutex);
	return path;
}

LSP *lsp_create(const char *root_dir, const char *command, const char *configuration, FILE *log) {
	LSP *lsp = calloc(1, sizeof *lsp);
	if (!lsp) return NULL;
	
	if (!id_mutex)
		id_mutex = SDL_CreateMutex();
	
	static LSPID curr_id = 1;
	lsp->id = curr_id++;
	lsp->log = log;
	
	#if DEBUG
		printf("Starting up LSP %p (ID %u) `%s` in %s\n",
			(void *)lsp, (unsigned)lsp->id, command, root_dir);
	#endif
	
	str_hash_table_create(&lsp->document_ids, sizeof(u32));
	lsp->command = str_dup(command);
	if (configuration && *configuration)
		lsp->configuration_to_send = str_dup(configuration);
	lsp->quit_sem = SDL_CreateSemaphore(0);	
	lsp->error_mutex = SDL_CreateMutex();
	lsp->messages_mutex = SDL_CreateMutex();
	
	// document ID 0 is reserved
	LSPDocumentID zero_id = lsp_document_id(lsp, "");
	assert(zero_id == 0);
	
	arr_add(lsp->workspace_folders, lsp_document_id(lsp, root_dir));
	lsp->workspace_folders_mutex = SDL_CreateMutex();
	
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = false,
		.stderr_blocking = false,
		.separate_stderr = true,
		.working_directory = root_dir,
	};
	lsp->process = process_run_ex(command, &settings);
	LSPRequest initialize = {
		.type = LSP_REQUEST_INITIALIZE
	};
	initialize.id = get_request_id();
	// immediately send the request rather than queueing it.
	// this is a small request, so it shouldn't be a problem.
	write_request(lsp, &initialize);
	lsp->communication_thread = SDL_CreateThread(lsp_communication_thread, "LSP communicate", lsp);
	return lsp;
}

bool lsp_try_add_root_dir(LSP *lsp, const char *new_root_dir) {
	assert(lsp->initialized);

	bool got_it = false;
	SDL_LockMutex(lsp->workspace_folders_mutex);
		arr_foreach_ptr(lsp->workspace_folders, LSPDocumentID, folder) {
			if (str_has_path_prefix(new_root_dir, lsp_document_path(lsp, *folder))) {
				got_it = true;
				break;
			}
		}
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	if (got_it) return true;
	
	if (!lsp->capabilities.workspace_folders_support) {
		return false;
	}
	
	// send workspace/didChangeWorkspaceFolders notification
	LSPRequest req = {.type = LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS};
	LSPRequestDidChangeWorkspaceFolders *w = &req.data.change_workspace_folders;
	LSPDocumentID document_id = lsp_document_id(lsp, new_root_dir);
	arr_add(w->added, document_id);
	lsp_send_request(lsp, &req);
	// *technically* this is incorrect because if the server *just now sent* a
	// workspace/workspaceFolders request, we'd give it back inconsistent information.
	// i don't care.
	SDL_LockMutex(lsp->workspace_folders_mutex);
		arr_add(lsp->workspace_folders, document_id);
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	return true;
}

bool lsp_next_message(LSP *lsp, LSPMessage *message) {
	bool any = false;
	SDL_LockMutex(lsp->messages_mutex);
	if (arr_len(lsp->messages_server2client)) {
		*message = lsp->messages_server2client[0];
		arr_remove(lsp->messages_server2client, 0);
		any = true;
	}
	SDL_UnlockMutex(lsp->messages_mutex);
	return any;
}

void lsp_free(LSP *lsp) {
	SDL_SemPost(lsp->quit_sem);
	SDL_WaitThread(lsp->communication_thread, NULL);
	SDL_DestroyMutex(lsp->messages_mutex);
	SDL_DestroyMutex(lsp->workspace_folders_mutex);
	SDL_DestroyMutex(lsp->error_mutex);
	SDL_DestroySemaphore(lsp->quit_sem);
	process_kill(&lsp->process);
	
	arr_free(lsp->received_data);
	
	str_hash_table_clear(&lsp->document_ids);
	for (size_t i = 0; i < arr_len(lsp->document_data); ++i)
		free(lsp->document_data[i].path);
	arr_free(lsp->document_data);
	
	arr_foreach_ptr(lsp->messages_server2client, LSPMessage, message)
		lsp_message_free(message);
	arr_free(lsp->messages_server2client);
	
	arr_foreach_ptr(lsp->messages_client2server, LSPMessage, message)
		lsp_message_free(message);
	arr_free(lsp->messages_client2server);
	
	arr_foreach_ptr(lsp->requests_sent, LSPRequest, r)
		lsp_request_free(r);
	arr_free(lsp->requests_sent);
	
	arr_free(lsp->workspace_folders);
	
	arr_free(lsp->completion_trigger_chars);
	arr_free(lsp->signature_help_trigger_chars);
	arr_free(lsp->signature_help_retrigger_chars);
	free(lsp->command);
	free(lsp->configuration_to_send);
	memset(lsp, 0, sizeof *lsp);
	free(lsp);
}

void lsp_document_changed(LSP *lsp, const char *document, LSPDocumentChangeEvent change) {
	// @TODO(optimization, eventually): batch changes (using the contentChanges array)
	LSPRequest request = {.type = LSP_REQUEST_DID_CHANGE};
	LSPRequestDidChange *c = &request.data.change;
	c->document = lsp_document_id(lsp, document);
	arr_add(c->changes, change);
	lsp_send_request(lsp, &request);
}

bool lsp_position_eq(LSPPosition a, LSPPosition b) {
	return a.line == b.line && a.character == b.character;
}

bool lsp_document_position_eq(LSPDocumentPosition a, LSPDocumentPosition b) {
	return a.document == b.document && lsp_position_eq(a.pos, b.pos);
}


LSPDocumentPosition lsp_location_start_position(LSPLocation location) {
	return (LSPDocumentPosition) {
		.document = location.document,
		.pos = location.range.start
	};
}

LSPDocumentPosition lsp_location_end_position(LSPLocation location) {
	return (LSPDocumentPosition) {
		.document = location.document,
		.pos = location.range.end
	};
}

bool lsp_covers_path(LSP *lsp, const char *path) {
	bool ret = false;
	SDL_LockMutex(lsp->workspace_folders_mutex);
	arr_foreach_ptr(lsp->workspace_folders, LSPDocumentID, folder) {
		if (str_has_path_prefix(path, lsp_document_path(lsp, *folder))) {
			ret = true;
			break;
		}
	}
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	return ret;
}

void lsp_cancel_request(LSP *lsp, LSPRequestID id) {
	if (!id) return;
	if (!lsp) return;
	bool sent = false;
	SDL_LockMutex(lsp->messages_mutex);
		for (u32 i = 0; i < arr_len(lsp->requests_sent); ++i) {
			LSPRequest *req = &lsp->requests_sent[i];
			if (req->id == id) {
				// we sent this request but haven't received a response
				sent = true;
				arr_remove(lsp->requests_sent, i);
				break;
			}
		}
		
		for (u32 i = 0; i < arr_len(lsp->messages_client2server); ++i) {
			LSPMessage *message = &lsp->messages_client2server[i];
			if (message->type == LSP_REQUEST && message->u.request.id == id) {
				// we haven't sent this request yet
				arr_remove(lsp->messages_client2server, i);
				break;
			}
		
		}
	SDL_UnlockMutex(lsp->messages_mutex);
	if (sent) {
		LSPRequest request = {.type = LSP_REQUEST_CANCEL};
		request.data.cancel.id = id;
		lsp_send_request(lsp, &request);
	}
}
