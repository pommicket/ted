// @TODO: documentation
//  @TODO :  make sure offsets are utf-16!
// @TODO:  maximum queue size for requests/responses just in case?

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

typedef struct {
	LSPMessageType type;
	union {
		LSPRequest request;
		JSON response;
	} u;
} LSPMessage;

typedef struct {
	Process process;
	u64 request_id;
	LSPMessage *messages;
	SDL_mutex *messages_mutex;
	LSPRequest *requests_client2server;
	LSPRequest *requests_server2client;
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

// technically there are "requests" and "notifications"
// notifications are different in that they don't have IDs and don't return responses.
// the distinction isn't super important to us though.
// returns the ID of the request
static u64 write_request(LSP *lsp, const LSPRequest *request) {
	unsigned long long id = lsp->request_id++;
	
	StrBuilder builder = str_builder_new();
	
	u32 max_header_size = 64;
	// this is where our header will go
	str_builder_append_null(&builder, max_header_size);
	
	str_builder_append(&builder, "{\"jsonrpc\":\"2.0\",");
	
	switch (request->type) {
	case LSP_NONE:
	// these are server-to-client-only requests
	case LSP_SHOW_MESSAGE:
	case LSP_LOG_MESSAGE:
		assert(0);
		break;
	case LSP_INITIALIZE: {
		str_builder_appendf(&builder,
			"\"id\":%llu,\"method\":\"initialize\",\"params\":{"
				"\"processId\":%d,"
				"\"capabilities\":{}"
		"}", id, process_get_id());
	} break;
	case LSP_INITIALIZED:
		str_builder_append(&builder, "\"method\":\"initialized\"");
		break;
	case LSP_OPEN: {
		const LSPRequestOpen *open = &request->data.open;
		char *escaped_filename = json_escape(open->filename);
		char *escaped_text = json_escape(open->file_contents);
		
		str_builder_appendf(&builder,
			"\"id\":%llu,\"method\":\"textDocument/open\",\"params\":{"
				"textDocument:{"
					"uri:\"file://%s\","
					"languageId:\"%s\","
					"version:1,"
					"text:\"%s\"}}",
			id,
			escaped_filename,
			lsp_language_id(open->language),
			escaped_text);
		free(escaped_text);
		free(escaped_filename);
	} break;
	case LSP_COMPLETION: {
		const LSPRequestCompletion *completion = &request->data.completion;
		char *escaped_path = json_escape(completion->position.path);
		str_builder_appendf(&builder,"\"id\":%llu,\"method\":\"textDocument/completion\",\"params\":{"
				"textDocument:\"%s\","
				"position:{"
					"line:%lu,"
					"character:%lu"
				"}"
		"}",
			id,
			escaped_path,
			(ulong)completion->position.line,
			(ulong)completion->position.character);
		free(escaped_path);
	} break;
	case LSP_SHUTDOWN:
		str_builder_appendf(&builder, "\"id\":%llu,\"method\":\"shutdown\"", id);
		break;
	case LSP_EXIT:
		str_builder_appendf(&builder, "\"method\":\"exit\"");
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
	
	return (u64)id;
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
	JSONValue method = json_get(json, "method");
	if (method.type != JSON_STRING) {
		lsp_set_error(lsp, "Bad type for request method: %s", json_type_to_str(method.type));
	}
	
	char str[64] = {0};
	json_string_get(json, &method.val.string, str, sizeof str);
	
	if (streq(str, "window/showMessage")) {
		request->type = LSP_SHOW_MESSAGE;
		goto window_message;
	} else if (streq(str, "window/logMessage")) {
		request->type = LSP_LOG_MESSAGE;
		window_message:;
		JSONValue type = json_get(json, "params.type");
		JSONValue message = json_get(json, "params.message");
		if (type.type != JSON_NUMBER) {
			lsp_set_error(lsp, "Expected MessageType, got %s", json_type_to_str(type.type));
			return false;
		}
		if (message.type != JSON_STRING) {
			lsp_set_error(lsp, "Expected message string, got %s", json_type_to_str(message.type));
			return false;
		}
		
		int mtype = (int)type.val.number;
		if (mtype < 1 || mtype > 4) {
			lsp_set_error(lsp, "Bad MessageType: %g", type.val.number);
			return false;
		}
		
		LSPRequestMessage *m = &request->data.message;
		m->type = (LSPWindowMessageType)mtype;
		m->message = json_string_get_alloc(json, &message.val.string);
		return true;
	} else if (str_has_prefix(str, "$/")) {
		// we can safely ignore this
	} else {
		lsp_set_error(lsp, "Unrecognized request method: %s", str);
	}
	return false;
}

static void process_message(LSP *lsp, JSON *json) {
		
	#if 1
	printf("\x1b[3m");
	json_debug_print(json);
	printf("\x1b[0m\n");
	#endif
	bool keep_json = false;
	
	JSONValue error = json_get(json, "error.message");
	if (error.type == JSON_STRING) {
		char err[256] = {0};
		json_string_get(json, &error.val.string, err, sizeof err);;
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
			u64 initialized_id = write_request(lsp, &initialized);
			// this should be the second request.
			(void)initialized_id;
			assert(initialized_id == 1);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
		} else {
			SDL_LockMutex(lsp->messages_mutex);
			LSPMessage *message = arr_addp(lsp->messages);
			message->type = LSP_RESPONSE;
			message->u.response = *json;
			SDL_UnlockMutex(lsp->messages_mutex);
			keep_json = true;
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
	if (!keep_json)
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

void lsp_message_free(LSPMessage *message) {
	switch (message->type) {
	case LSP_REQUEST:
		lsp_request_free(&message->u.request);
		break;
	case LSP_RESPONSE:
		json_free(&message->u.response);
		break;
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
