typedef struct {
	Process process;
} LSP;


static void send_request(LSP *lsp, const char *content) {
	char header[128];
	size_t content_size = strlen(content);
	strbuf_printf(header, "Content-Length: %zu\r\n\r\n", content_size); 
	process_write(&lsp->process, header, strlen(header));
	process_write(&lsp->process, content, content_size);
}

void lsp_create(LSP *lsp, const char *analyzer_command) {
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
	// @TODO: recv_response
	char response_text[4096] = {0};
	process_read(&lsp->process, response_text, sizeof response_text);
	char *rnrn = strstr(response_text, "\r\n\r\n");
	if (!rnrn) {
		//@TODO delete me
		printf("no analyzer ):\n");
		exit(0);
	}
	JSON  json = {0};
	printf("%s\n",rnrn+4);
	if (!json_parse(&json, rnrn + 4))
		printf("fail : %s\n",json.error);
//	json_debug_print(&json);printf("\n");
	
}
