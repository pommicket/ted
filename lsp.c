// @TODO:
// - document this file. maybe make lsp.h
// - maximum queue size for requests/responses just in case?
// - delete old LSPRequestTrackedInfos
//    (if the server never sends a response)
// - TESTING: make rust-analyzer-slow (waits 10s before sending response)

typedef enum {
	LSP_REQUEST,
	LSP_RESPONSE
} LSPMessageType;

typedef enum {
	LSP_NONE,
	
	// client-to-server
	LSP_INITIALIZE,
	LSP_INITIALIZED,
	LSP_OPEN,
	LSP_COMPLETION,
	LSP_SHUTDOWN,
	LSP_EXIT,
	
	// server-to-client
	LSP_SHOW_MESSAGE,
	LSP_LOG_MESSAGE
} LSPRequestType;

typedef struct {
	// buffer language
	Language language;
	// freed by lsp_request_free
	char *filename;
	// freed by lsp_request_free
	char *file_contents;
} LSPRequestOpen;

typedef enum {
	ERROR = 1,
	WARNING = 2,
	INFO = 3,
	LOG = 4
} LSPWindowMessageType;

typedef struct {
	LSPWindowMessageType type;
	// freed by lsp_request_free
	char *message;
} LSPRequestMessage;

typedef struct {
	// freed by lsp_request_free
	char *path;
	u32 line;
	// the **UTF-16** "character" offset within the line
	u32 character;
} LSPDocumentPosition;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestCompletion;

typedef struct {
	LSPRequestType type;
	union {
		LSPRequestOpen open;
		LSPRequestCompletion completion;
		// for LSP_SHOW_MESSAGE and LSP_LOG_MESSAGE
		LSPRequestMessage message;
	} data;
} LSPRequest;

// info we want to keep track of about a request,
// so we can deal with the response appropriately.
typedef struct {
	u64 id;
	LSPRequestType type;
} LSPRequestTrackedInfo;

typedef struct {
	u32 offset;
} LSPString;

typedef struct {
	u32 line;
	u32 character;
} LSPPosition;

typedef struct {
	LSPPosition start;
	LSPPosition end;
} LSPRange;

// see InsertTextFormat in the LSP spec.
typedef enum {
	// plain text
	LSP_TEXT_EDIT_PLAIN = 1,
	// snippet   e.g. "some_method($1, $2)$0"
	LSP_TEXT_EDIT_SNIPPET = 2
} LSPTextEditType;

typedef struct {
	LSPTextEditType type;

	// if set to true, `range` should be ignored
	//  -- this is a completion which uses insertText.
	// how to handle this:
	// "VS Code when code complete is requested in this example
	// `con<cursor position>` and a completion item with an `insertText` of
	// `console` is provided it will only insert `sole`"
	bool at_cursor;
	
	LSPRange range;
	LSPString new_text;
} LSPTextEdit;

typedef struct {
	LSPString label;
	LSPTextEdit text_edit;
	// note: the items are sorted here in this file,
	// so you probably don't need to access this.
	LSPString sort_text;
} LSPCompletionItem;

typedef struct {
	// dynamic array
	LSPCompletionItem *items;
} LSPResponseCompletion;

typedef LSPRequestType LSPResponseType;
typedef struct {
	LSPResponseType type;
	// LSP responses tend to have a lot of strings.
	// to avoid doing a ton of allocations+frees,
	// they're all stored here.
	char *string_data;
	union {
		LSPResponseCompletion completion;
	} data;
} LSPResponse;

typedef struct {
	LSPMessageType type;
	union {
		LSPRequest request;
		LSPResponse response;
	} u;
} LSPMessage;

typedef struct {
	Process process;
	u64 request_id;
	LSPMessage *messages;
	SDL_mutex *messages_mutex;
	LSPRequest *requests_client2server;
	LSPRequest *requests_server2client;
	// only applicable for client-to-server requests
	LSPRequestTrackedInfo *requests_tracked_info;
	SDL_mutex *requests_mutex;
	bool initialized; // has the response to the initialize request been sent?
	SDL_Thread *communication_thread;
	SDL_sem *quit_sem;
	char *received_data; // dynamic array
	SDL_mutex *error_mutex;
	char error[256];
} LSP;

// returns true if there's an error.
// returns false and sets error to "" if there's no error.
// if clear = true, the error will be cleared.
// you may set error = NULL, error_size = 0, clear = true to just clear the error
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

static const char *lsp_language_id(Language lang) {
	switch (lang) {
	case LANG_CONFIG:
	case LANG_TED_CFG:
	case LANG_NONE:
		return "text";
	case LANG_C:
		return "c";
	case LANG_CPP:
		return "cpp";
	case LANG_JAVA:
		return "java";
	case LANG_JAVASCRIPT:
		return "javascript";
	case LANG_MARKDOWN:
		return "markdown";
	case LANG_GO:
		return "go";
	case LANG_RUST:
		return "rust";
	case LANG_PYTHON:
		return "python";
	case LANG_HTML:
		return "html";
	case LANG_TEX:
		return "latex";
	case LANG_COUNT: break;
	}
	assert(0);
	return "text";
}


static void lsp_position_free(LSPDocumentPosition *position) {
	free(position->path);
}

static void lsp_request_free(LSPRequest *r) {
	switch (r->type) {
	case LSP_NONE:
		assert(0);
		break;
	case LSP_INITIALIZE:
	case LSP_INITIALIZED:
	case LSP_SHUTDOWN:
	case LSP_EXIT:
		break;
	case LSP_COMPLETION: {
		LSPRequestCompletion *completion = &r->data.completion;
		lsp_position_free(&completion->position);
		} break;
	case LSP_OPEN: {
		LSPRequestOpen *open = &r->data.open;
		free(open->filename);
		free(open->file_contents);
		} break;
	case LSP_SHOW_MESSAGE:
	case LSP_LOG_MESSAGE:
		free(r->data.message.message);
		break;
	}
}

static void lsp_response_free(LSPResponse *r) {
	arr_free(r->string_data);
	switch (r->type) {
	case LSP_COMPLETION:
		arr_free(r->data.completion.items);
		break;
	default:
		break;
	}
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

// technically there are "requests" and "notifications"
// notifications are different in that they don't have IDs and don't return responses.
// this function handles both.
// returns the ID of the request
static void write_request(LSP *lsp, const LSPRequest *request) {
	
	StrBuilder builder = str_builder_new();
	
	u32 max_header_size = 64;
	// this is where our header will go
	str_builder_append_null(&builder, max_header_size);
	
	str_builder_append(&builder, "{\"jsonrpc\":\"2.0\",");
	
	bool is_notification = request->type == LSP_INITIALIZED
		|| request->type == LSP_EXIT;
	if (!is_notification) {
		unsigned long long id = lsp->request_id++;
		str_builder_appendf(&builder, "\"id\":%llu,", id);
		LSPRequestTrackedInfo info = {
			.id = id,
			.type = request->type
		};
		SDL_LockMutex(lsp->requests_mutex);
		arr_add(lsp->requests_tracked_info, info);
		SDL_UnlockMutex(lsp->requests_mutex);
	}
	
	switch (request->type) {
	case LSP_NONE:
	// these are server-to-client-only requests
	case LSP_SHOW_MESSAGE:
	case LSP_LOG_MESSAGE:
		assert(0);
		break;
	case LSP_INITIALIZE: {
		str_builder_appendf(&builder,
			"\"method\":\"initialize\",\"params\":{"
				"\"processId\":%d,"
				"\"capabilities\":{}"
		"}", process_get_id());
	} break;
	case LSP_INITIALIZED:
		str_builder_append(&builder, "\"method\":\"initialized\"");
		break;
	case LSP_OPEN: {
		const LSPRequestOpen *open = &request->data.open;
		char *escaped_filename = json_escape(open->filename);
		char *escaped_text = json_escape(open->file_contents);
		
		str_builder_appendf(&builder,
			"\"method\":\"textDocument/open\",\"params\":{"
				"textDocument:{"
					"uri:\"file://%s\","
					"languageId:\"%s\","
					"version:1,"
					"text:\"%s\"}}",
			escaped_filename,
			lsp_language_id(open->language),
			escaped_text);
		free(escaped_text);
		free(escaped_filename);
	} break;
	case LSP_COMPLETION: {
		const LSPRequestCompletion *completion = &request->data.completion;
		char *escaped_path = json_escape(completion->position.path);
		str_builder_appendf(&builder,"\"method\":\"textDocument/completion\",\"params\":{"
				"\"textDocument\":{\"uri\":\"file://%s\"},"
				"\"position\":{"
					"\"line\":%lu,"
					"\"character\":%lu"
				"}"
		"}",
			escaped_path,
			(ulong)completion->position.line,
			(ulong)completion->position.character);
		free(escaped_path);
	} break;
	case LSP_SHUTDOWN:
		str_builder_append(&builder, "\"method\":\"shutdown\"");
		break;
	case LSP_EXIT:
		str_builder_append(&builder, "\"method\":\"exit\"");
		break;
	}
	
	str_builder_append(&builder, "}");
	
	// this is kind of hacky but it lets us send the whole request with one write call.
	// probably not *actually* needed. i thought it would help fix an error but it didn't.
	size_t content_length = str_builder_len(&builder) - max_header_size;
	char content_length_str[32];
	sprintf(content_length_str, "%zu", content_length);
	size_t header_size = strlen("Content-Length: \r\n\r\n") + strlen(content_length_str);
	char *header = &builder.str[max_header_size - header_size];
	strcpy(header, "Content-Length: ");
	strcat(header, content_length_str);
	// we specifically DON'T want a null byte
	memcpy(header + strlen(header), "\r\n\r\n", 4);
	
	char *content = header;
	#if 1
		printf("\x1b[1m%s\x1b[0m\n",content);
	#endif
	
	// @TODO: does write always write the full amount? probably not. this should be fixed.
	process_write(&lsp->process, content, strlen(content));

	str_builder_free(&builder);
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
		request->type = LSP_SHOW_MESSAGE;
		goto window_message;
	} else if (streq(method, "window/logMessage")) {
		request->type = LSP_LOG_MESSAGE;
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
	response->type = LSP_COMPLETION;
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
		
	JSONValue error = json_get(json, "error.message");
	if (error.type == JSON_STRING) {
		char err[256] = {0};
		json_string_get(json, error.val.string, err, sizeof err);;
		lsp_set_error(lsp, "%s", err);
		goto ret;
	}
	
	JSONValue result = json_get(json, "result");
	if (result.type != JSON_UNDEFINED) {
		JSONValue id = json_get(json, "id");
	
		if (id.type != JSON_NUMBER) {
			// what
			lsp_set_error(lsp, "Response with no ID.");
			goto ret;
		}
		if (id.val.number == 0) {
			// it's the response to our initialize request!
			// let's send back an "initialized" request (notification) because apparently
			// that's something we need to do.
			LSPRequest initialized = {
				.type = LSP_INITIALIZED,
				.data = {0},
			};
			write_request(lsp, &initialized);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
		} else {
			u64 id_no = (u64)id.val.number;
			LSPRequestTrackedInfo tracked_info = {0};
			SDL_LockMutex(lsp->requests_mutex);
			arr_foreach_ptr(lsp->requests_tracked_info, LSPRequestTrackedInfo, info) {
				if (info->id == id_no) {
					// hey its the thing
					tracked_info = *info;
					arr_remove(lsp->requests_tracked_info, (u32)(info - lsp->requests_tracked_info));
					break;
				}
			}
			SDL_UnlockMutex(lsp->requests_mutex);
			
			LSPResponse response = {0};
			bool success = false;
			switch (tracked_info.type) {
			case LSP_COMPLETION:
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
	#if 1
	printf("\x1b[3m%s\x1b[0m\n",lsp->received_data);
	#endif
	
	u64 response_offset=0, response_size=0;
	while (has_response(lsp->received_data, received_so_far, &response_offset, &response_size)) {
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
		lsp_request_free(r);
		
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
			.type = LSP_SHUTDOWN,
			.data = {0}
		};
		LSPRequest exit = {
			.type = LSP_EXIT,
			.data = {0}
		};
		write_request(lsp, &shutdown);
		// i give you ONE MILLISECOND to send your fucking shutdown response
		time_sleep_ms(1);
		write_request(lsp, &exit);
		// i give you ONE MILLISECOND to terminate
		// I WILL KILL YOU IF IT TAKES ANY LONGER
		time_sleep_ms(1);
		
		#if 1
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

bool lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = false,
		.stderr_blocking = false,
		.separate_stderr = true,
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
	LSPRequest initialize = {
		.type = LSP_INITIALIZE
	};
	// immediately send the request rather than queueing it.
	// this is a small request, so it shouldn't be a problem.
	write_request(lsp, &initialize);
	
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
	SDL_DestroySemaphore(lsp->quit_sem);
	process_kill(&lsp->process);
	arr_free(lsp->received_data);
	arr_foreach_ptr(lsp->messages, LSPMessage, message) {
		lsp_message_free(message);
	}
	arr_free(lsp->messages);
}
