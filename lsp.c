typedef struct {
	Process process;
} LSP;

static void send_message(LSP *lsp, const char *content, size_t content_size) {
}

void lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = true
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
}
