typedef struct {
	Process process;
	char error[256];
} LSP;


static void send_request(LSP *lsp, const char *content) {
	char header[128];
	size_t content_size = strlen(content);
	strbuf_printf(header, "Content-Length: %zu\r\n\r\n", content_size); 
	process_write(&lsp->process, header, strlen(header));
	process_write(&lsp->process, content, content_size);
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
		if (result.type == JSON_NULL) {
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
	send_request(lsp, init_request);
	JSON  json = {0};
	if (!recv_response(lsp, &json)) {
		return false;
	}
	JSONValue response = json_get(&json, "result.capabilities.textDocumentSync.openClose");
	json_debug_print_value(&json, &response);printf("\n");
	return true;
}
