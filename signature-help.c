// deals with textDocument/signatureHelp LSP requests

void signature_help_open(Ted *ted, char32_t trigger) {
	(void)trigger; // for now we don't send context
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) return;
	LSP *lsp = buffer_lsp(buffer);
	LSPRequest request  = {.type = LSP_REQUEST_SIGNATURE_HELP};
	LSPRequestSignatureHelp *s = &request.data.signature_help;
	s->position = buffer_cursor_pos_as_lsp_document_position(buffer);
	lsp_send_request(lsp, &request);
}
