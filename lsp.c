static void lsp_request_free(LSPRequest *r);
#include "lsp-write-request.c"

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

#define lsp_set_error(lsp, ...) do {\
		SDL_LockMutex(lsp->error_mutex);\
		strbuf_printf(lsp->error, __VA_ARGS__);\
		SDL_UnlockMutex(lsp->error_mutex);\
	} while (0)


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

static WarnUnusedResult bool lsp_expect_type(LSP *lsp, JSONValue value, JSONValueType type, const char *what) {
	if (value.type != type) {
		lsp_set_error(lsp, "Expected %s for %s, got %s",
			json_type_to_str(type),
			what,
			json_type_to_str(value.type));
		return false;
	}
	return true;
}

static WarnUnusedResult bool lsp_expect_object(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_OBJECT, what);
}

static WarnUnusedResult bool lsp_expect_array(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_ARRAY, what);
}

static WarnUnusedResult bool lsp_expect_string(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_STRING, what);
}

static WarnUnusedResult bool lsp_expect_number(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_NUMBER, what);
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

static bool parse_server2client_request(LSP *lsp, JSON *json, LSPRequest *request) {
	JSONValue method_value = json_get(json, "method");
	if (!lsp_expect_string(lsp, method_value, "request method"))
		return false;
	
	char method[64] = {0};
	json_string_get(json, method_value.val.string, method, sizeof method);
	
	if (streq(method, "window/showMessage")) {
		request->type = LSP_REQUEST_SHOW_MESSAGE;
		goto window_message;
	} else if (streq(method, "window/logMessage")) {
		request->type = LSP_REQUEST_LOG_MESSAGE;
		window_message:;
		JSONValue type = json_get(json, "params.type");
		JSONValue message = json_get(json, "params.message");
		if (!lsp_expect_number(lsp, type, "MessageType"))
			return false;
		if (!lsp_expect_string(lsp, message, "message string"))
			return false;
		
		int mtype = (int)type.val.number;
		if (mtype < 1 || mtype > 4) {
			lsp_set_error(lsp, "Bad MessageType: %g", type.val.number);
			return false;
		}
		
		LSPRequestMessage *m = &request->data.message;
		m->type = (LSPWindowMessageType)mtype;
		m->message = json_string_get_alloc(json, message.val.string);
		return true;
	} else if (str_has_prefix(method, "$/")) {
		// we can safely ignore this
	} else {
		lsp_set_error(lsp, "Unrecognized request method: %s", method);
	}
	return false;
}

const char *lsp_response_string(const LSPResponse *response, LSPString string) {
	assert(string.offset < arr_len(response->string_data));
	return &response->string_data[string.offset];
}

static LSPString lsp_response_add_json_string(LSPResponse *response, const JSON *json, JSONString string) {
	u32 offset = arr_len(response->string_data);
	arr_set_len(response->string_data, offset + string.len + 1);
	json_string_get(json, string, response->string_data + offset, string.len + 1);
	return (LSPString){
		.offset = offset
	};
}

static int completion_qsort_cmp(void *context, const void *av, const void *bv) {
	const LSPResponse *response = context;
	const LSPCompletionItem *a = av, *b = bv;
	const char *a_sort_text = lsp_response_string(response, a->sort_text);
	const char *b_sort_text = lsp_response_string(response, b->sort_text);
	int sort_text_cmp = strcmp(a_sort_text, b_sort_text);
	if (sort_text_cmp != 0)
		return sort_text_cmp;
	// for some reason, rust-analyzer outputs identical sortTexts
	// i have no clue what that means.
	// the LSP "specification" is not very specific.
	// we'll sort by label in this case.
	// this is what VSCode seems to do.
	// i hate microsofot.
	const char *a_label = lsp_response_string(response, a->label);
	const char *b_label = lsp_response_string(response, b->label);
	return strcmp(a_label, b_label);
}

static bool parse_position(LSP *lsp, const JSON *json, JSONValue pos_value, LSPPosition *pos) {
	if (!lsp_expect_object(lsp, pos_value, "document position"))
		return false;
	JSONObject pos_object = pos_value.val.object;
	JSONValue line = json_object_get(json, pos_object, "line");
	JSONValue character = json_object_get(json, pos_object, "character");
	if (!lsp_expect_number(lsp, line, "document line number")
		|| !lsp_expect_number(lsp, character, "document column number"))
		return false;
	pos->line = (u32)line.val.number;
	pos->character = (u32)line.val.number;
	return true;
}

static bool parse_range(LSP *lsp, const JSON *json, JSONValue range_value, LSPRange *range) {
	if (!lsp_expect_object(lsp, range_value, "document range"))
		return false;
	JSONObject range_object = range_value.val.object;
	JSONValue start = json_object_get(json, range_object, "start");
	JSONValue end = json_object_get(json, range_object, "end");
	return parse_position(lsp, json, start, &range->start)
		&& parse_position(lsp, json, end, &range->end);
}

static bool parse_completion(LSP *lsp, const JSON *json, LSPResponse *response) {
	// deal with textDocument/completion response.
	// result: CompletionItem[] | CompletionList | null
	LSPResponseCompletion *completion = &response->data.completion;
	
	JSONValue result = json_get(json, "result");
	JSONValue items_value = {0};
	switch (result.type) {
	case JSON_NULL:
		// no completions
		return true;
	case JSON_ARRAY:
		items_value = result;
		break;
	case JSON_OBJECT:
		items_value = json_object_get(json, result.val.object, "items");
		break;
	default:
		lsp_set_error(lsp, "Weird result type for textDocument/completion response: %s.", json_type_to_str(result.type));
		break;		
	}
		
	if (!lsp_expect_array(lsp, items_value, "completion list"))
		return false;
	
	JSONArray items = items_value.val.array;
	
	arr_set_len(completion->items, items.len);
	
	for (u32 i = 0; i < items.len; ++i) {
		LSPCompletionItem *item = &completion->items[i];
		
		JSONValue item_value = json_array_get(json, items, i);
		if (!lsp_expect_object(lsp, item_value, "completion list"))
			return false;
		JSONObject item_object = item_value.val.object;
		
		JSONValue label_value = json_object_get(json, item_object, "label");
		if (!lsp_expect_string(lsp, label_value, "completion label"))
			return false;
		JSONString label = label_value.val.string;
		item->label = lsp_response_add_json_string(response, json, label);
		
		// defaults
		item->sort_text = item->label;
		item->filter_text = item->label;
		item->text_edit = (LSPTextEdit) {
			.type = LSP_TEXT_EDIT_PLAIN,
			.at_cursor = true,
			.range = {0},
			.new_text = item->label
		};
		
		JSONValue sort_text_value = json_object_get(json, item_object, "sortText");
		if (sort_text_value.type == JSON_STRING) {
			// LSP allows using a different string for sorting.
			item->sort_text = lsp_response_add_json_string(response,
				json, sort_text_value.val.string);
		}
		
		JSONValue filter_text_value = json_object_get(json, item_object, "filterText");
		if (filter_text_value.type == JSON_STRING) {
			// LSP allows using a different string for filtering.
			item->filter_text = lsp_response_add_json_string(response,
				json, filter_text_value.val.string);
		}
		
		JSONValue text_type_value = json_object_get(json, item_object, "insertTextFormat");
		if (text_type_value.type == JSON_NUMBER) {
			double type = text_type_value.val.number;
			if (type != LSP_TEXT_EDIT_PLAIN && type != LSP_TEXT_EDIT_SNIPPET) {
				lsp_set_error(lsp, "Bad InsertTextFormat: %g", type);
				return false;
			}
			item->text_edit.type = (LSPTextEditType)type;
		}
		
		// @TODO: detail
		
		// @TODO(eventually): additionalTextEdits
		//  (try to find a case where this comes up)
		
		// what should happen when this completion is selected?
		JSONValue text_edit_value = json_object_get(json, item_object, "textEdit");
		if (text_edit_value.type == JSON_OBJECT) {
			JSONObject text_edit = text_edit_value.val.object;
			item->text_edit.at_cursor = false;
			
			JSONValue range = json_object_get(json, text_edit, "range");
			if (!parse_range(lsp, json, range, &item->text_edit.range))
				return false;
				
			JSONValue new_text_value = json_object_get(json, text_edit, "newText");
			if (!lsp_expect_string(lsp, new_text_value, "completion newText"))
				return false;
			item->text_edit.new_text = lsp_response_add_json_string(response,
				json, new_text_value.val.string);
		} else {
			// not using textEdit. check insertText.
			JSONValue insert_text_value = json_object_get(json, item_object, "insertText");
			if (insert_text_value.type == JSON_STRING) {
				// string which will be inserted if this completion is selected
				item->text_edit.new_text = lsp_response_add_json_string(response,
					json, insert_text_value.val.string);
			}
		}
		
	}
	
	qsort_with_context(completion->items, items.len, sizeof *completion->items,
		completion_qsort_cmp, response);
	
	return true;
}


static void process_message(LSP *lsp, JSON *json) {
		
	#if 0
	printf("\x1b[3m");
	json_debug_print(json);
	printf("\x1b[0m\n");
	#endif
	JSONValue id_value = json_get(json, "id");
	
	// get the request associated with this (if any)
	LSPRequest response_to = {0};
	if (id_value.type == JSON_NUMBER) {
		u64 id = (u64)id_value.val.number;
		arr_foreach_ptr(lsp->requests_sent, LSPRequest, req) {
			if (req->id == id) {
				response_to = *req;
				arr_remove(lsp->requests_sent, (u32)(req - lsp->requests_sent));
				break;
			}
		}
	}
	
	JSONValue error = json_get(json, "error.message");
	if (error.type == JSON_STRING) {
		char err[256] = {0};
		json_string_get(json, error.val.string, err, sizeof err);;
		
		if (streq(err, "waiting for cargo metadata or cargo check")) {
			// fine. be that way. i'll resend the goddamn request.
			// i'll keep bombarding you with requests.
			// maybe next time you should abide by the standard and only send an initialize response when youre actually ready to handle my requests. fuck you.
			if (response_to.type) {
				lsp_send_request(lsp, &response_to);
				// don't free
				memset(&response_to, 0, sizeof response_to);
			}
		} else {
			lsp_set_error(lsp, "%s", err);
		}
		goto ret;
	}
	
	JSONValue result = json_get(json, "result");
	if (result.type != JSON_UNDEFINED) {
		if (response_to.type == LSP_REQUEST_INITIALIZE) {
			// it's the response to our initialize request!
			// let's send back an "initialized" request (notification) because apparently
			// that's something we need to do.
			LSPRequest initialized = {
				.type = LSP_REQUEST_INITIALIZED,
				.data = {0},
			};
			write_request(lsp, &initialized);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
		} else {
			LSPResponse response = {0};
			bool success = false;
			response.request = response_to;
			switch (response_to.type) {
			case LSP_REQUEST_COMPLETION:
				success = parse_completion(lsp, json, &response);
				break;
			default:
				// it's some response we don't care about
				break;
			}
			if (success) {
				SDL_LockMutex(lsp->messages_mutex);
				LSPMessage *message = arr_addp(lsp->messages);
				message->type = LSP_RESPONSE;
				message->u.response = response;
				SDL_UnlockMutex(lsp->messages_mutex);
				response_to.type = 0; // don't free
			} else {
				lsp_response_free(&response);
			}
		}
	} else if (json_has(json, "method")) {
		LSPRequest request = {0};
		if (parse_server2client_request(lsp, json, &request)) {
			SDL_LockMutex(lsp->messages_mutex);
			LSPMessage *message = arr_addp(lsp->messages);
			message->type = LSP_REQUEST;
			message->u.request = request;
			SDL_UnlockMutex(lsp->messages_mutex);
		}
	} else {
		lsp_set_error(lsp, "Bad message from server (no result, no method).");
	}
	ret:
	lsp_request_free(&response_to);
	json_free(json);
	
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
