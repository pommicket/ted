// @TODO:  maximum queue size for requests/responses just in case?

typedef enum {
	LSP_INITIALIZE,
	LSP_OPEN,
} LSPRequestType;

typedef struct {
	// buffer language
	Language language;
	// these will be free'd.
	char *filename;
	char *file_contents;
} LSPRequestOpen;

typedef struct {
	LSPRequestType type;
	union {
		LSPRequestOpen open;
	} data;
} LSPRequest;

typedef struct {
	Process process;
	JSON *responses;
	SDL_mutex *responses_mutex;
	LSPRequest *requests;
	SDL_mutex *requests_mutex;
	
	SDL_Thread *communication_thread;
	SDL_sem *quit_sem;
	char *received_data; // dynamic array
	char error[256];
} LSP;


static void write_request_content(LSP *lsp, const char *content) {
	char header[128];
	size_t content_size = strlen(content);
	strbuf_printf(header, "Content-Length: %zu\r\n\r\n", content_size); 
	process_write(&lsp->process, header, strlen(header));
	process_write(&lsp->process, content, content_size);
}

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

static void write_request(LSP *lsp, const LSPRequest *request) {
	static unsigned long long id;
	++id;
	
	switch (request->type) {
	case LSP_INITIALIZE: {
		char content[1024];
		strbuf_printf(content,
			"{\"jsonrpc\":\"2.0\",\"id\":%llu,\"method\":\"initialize\",\"params\":{"
				"\"processId\":%d,"
				"\"capabilities\":{}"
		"}}", id, process_get_id());
		write_request_content(lsp, content);
	} break;
	case LSP_OPEN: {
		const LSPRequestOpen *open = &request->data.open;
		char *escaped_filename = json_escape(open->filename);
		char *did_open = a_sprintf(
			"{\"jsonrpc\":\"2.0\",\"id\":%llu,\"method\":\"textDocument/open\",\"params\":{"
				"textDocument:{"
					"uri:\"file://%s\","
					"languageId:\"%s\","
					"version:1,"
					"text:\"",
			id, escaped_filename, lsp_language_id(open->language));
		free(escaped_filename);
		
		size_t did_open_sz = strlen(did_open) + 2 * strlen(open->file_contents) + 16;
		did_open = realloc(did_open, did_open_sz);
		
		size_t n = json_escape_to(did_open + strlen(did_open),
			did_open_sz - 10 - strlen(did_open),
			open->file_contents);
		char *p = did_open + n;
		sprintf(p, "\"}}}");
		
		free(did_open);
		
		write_request_content(lsp, did_open);
	} break;
	default:
		// @TODO
		abort();
	}
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
	arr_add(lsp->requests, *request);
	SDL_UnlockMutex(lsp->requests_mutex);
}

// receive responses from LSP, up to max_size bytes.
static void lsp_receive(LSP *lsp, size_t max_size) {
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
	
	u64 response_offset=0, response_size=0;
	while (has_response(lsp->received_data, received_so_far, &response_offset, &response_size)) {
		char *copy = strn_dup(lsp->received_data + response_offset, response_size);
		JSON json = {0};
		json_parse(&json, copy);
		assert(json.text == copy);
		json.is_text_copied = true;
		
		JSONValue result = json_get(&json, "result");
		if (result.type == JSON_UNDEFINED) {
			// uh oh
			JSONValue error = json_get(&json, "error.message");
			if (error.type == JSON_STRING) {
				json_string_get(&json, &error.val.string, lsp->error, sizeof lsp->error);
			} else {
				strbuf_printf(lsp->error, "Server error (no message)");
			}
		} else {
			SDL_LockMutex(lsp->responses_mutex);
			arr_add(lsp->responses, json);
			SDL_UnlockMutex(lsp->responses_mutex);
		}
		
		size_t leftover_data_len = arr_len(lsp->received_data) - (response_offset + response_size);
		memmove(lsp->received_data, lsp->received_data + response_offset + response_size,
			leftover_data_len);
		arr_set_len(lsp->received_data, leftover_data_len);
		arr_reserve(lsp->received_data, leftover_data_len + 1);
		lsp->received_data[leftover_data_len] = '\0';
	}
}

static void free_request(LSPRequest *r) {
	switch (r->type) {
	case LSP_INITIALIZE:
		break;
	case LSP_OPEN: {
		LSPRequestOpen *open = &r->data.open;
		free(open->filename);
		free(open->file_contents);
		} break;
	}
}

// Do any necessary communication with the LSP.
// This writes requests and reads (and parses) responses.
static int lsp_communication_thread(void *data) {
	LSP *lsp = data;
	while (1) {
		LSPRequest *requests = NULL;
		SDL_LockMutex(lsp->requests_mutex);
		while (arr_len(lsp->requests)) {
			arr_add(requests, lsp->requests[0]);
			arr_remove(lsp->requests, 0);
		}
		SDL_UnlockMutex(lsp->requests_mutex);
		
		bool quit = false;
		arr_foreach_ptr(requests, LSPRequest, r) {
			if (!quit) {
				// this could slow down lsp_free if there's a gigantic request.
				// whatever.
				write_request(lsp, r);
			}
			free_request(r);
			
			if (SDL_SemTryWait(lsp->quit_sem) == 0) {
				quit = true;
				// important that we don't break here so all the requests get freed.
			}
		}
		
		arr_free(requests);
		
		lsp_receive(lsp, (size_t)10<<20);
		if (quit || SDL_SemWaitTimeout(lsp->quit_sem, 5) == 0)
			break;	
	}
	return 0;
}

bool lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = false
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
	LSPRequest initialize = {
		.type = LSP_INITIALIZE
	};
	lsp_send_request(lsp, &initialize);
	
	lsp->quit_sem = SDL_CreateSemaphore(0);	
	lsp->responses_mutex = SDL_CreateMutex();
	lsp->communication_thread = SDL_CreateThread(lsp_communication_thread, "LSP communicate", lsp);
	return true;
}

bool lsp_next_response(LSP *lsp, JSON *json) {
	bool any = false;
	SDL_LockMutex(lsp->responses_mutex);
	if (arr_len(lsp->responses)) {
		*json = lsp->responses[0];
		arr_remove(lsp->responses, 0);
		any = true;
	}
	SDL_UnlockMutex(lsp->responses_mutex);
	return any;
}

void lsp_free(LSP *lsp) {
	SDL_SemPost(lsp->quit_sem);
	SDL_WaitThread(lsp->communication_thread, NULL);
	SDL_DestroySemaphore(lsp->quit_sem);
	process_kill(&lsp->process);
	arr_free(lsp->received_data);
}
