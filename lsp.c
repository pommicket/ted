typedef enum {
	LSP_INITIALIZE,
	LSP_OPEN,
	LSP_CHANGE,
	LSP_CLOSE,
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
	char error[256];
} LSP;


static void send_request_content(LSP *lsp, const char *content) {
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

static void send_request(LSP *lsp, const LSPRequest *request) {
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
		send_request_content(lsp, content);
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
		
		send_request_content(lsp, did_open);
	} break;
	default:
		// @TODO
		abort();
	}
}

static bool recv_response(LSP *lsp, JSON *json) {
	static char response_data[500000];
	// i think this should always read all the response data.
	long long bytes_read = process_read(&lsp->process, response_data, sizeof response_data);
	if (bytes_read < 0) {
		strbuf_printf(lsp->error, "Read error.");
		return false;
	}
	response_data[bytes_read] = '\0';
	
	char *body_start = strstr(response_data, "\r\n\r\n");
	if (body_start) {
		body_start += 4;
		if (!json_parse(json, body_start)) {
			strbuf_cpy(lsp->error, json->error);
			return false;
		}
		JSONValue result = json_get(json, "result");
		if (result.type == JSON_UNDEFINED) {
			// uh oh
			JSONValue error = json_get(json, "error.message");
			if (error.type == JSON_STRING) {
				json_string_get(json, &error.val.string, lsp->error, sizeof lsp->error);
			} else {
				strbuf_printf(lsp->error, "Server error (no message)");
			}
		}
	} else {
		strbuf_printf(lsp->error, "No response body.");
		return false;
	}
	
	return true;
}

bool lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = true
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
	char init_request[1024];
	strbuf_printf(init_request,
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
			"\"processId\":%d,"
			"\"capabilities\":{}"
		"}}",
		process_get_id());
	send_request_content(lsp, init_request);
	JSON  json = {0};
	if (!recv_response(lsp, &json)) {
		return false;
	}
	json_debug_print(&json);
	return true;
}
