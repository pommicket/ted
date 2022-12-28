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

static void signature_help_clear(SignatureHelp *help) {
	for (int i = 0; i < help->signature_count; ++i) {
		Signature sig = help->signatures[i];
		free(sig.label_pre);
		free(sig.label_active);
		free(sig.label_post);
	}
	memset(help->signatures, 0, sizeof help->signatures);
}

void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response) {
	if (response->request.type != LSP_REQUEST_SIGNATURE_HELP)
		return;
	SignatureHelp *help = &ted->signature_help;
	const LSPResponseSignatureHelp *lsp_help = &response->data.signature_help;
	u32 signature_count = arr_len(lsp_help->signatures);
	if (signature_count > SIGNATURE_HELP_MAX)
		signature_count = SIGNATURE_HELP_MAX;
	
	signature_help_clear(help);
	for (u32 s = 0; s < signature_count; ++s) {
		Signature *signature = &help->signatures[s];
		LSPSignatureInformation *lsp_signature = &lsp_help->signatures[s];
		
		const char *label = lsp_response_string(response, lsp_signature->label);
		size_t start = unicode_utf16_to_utf8_offset(label, lsp_signature->active_start);
		size_t end   = unicode_utf16_to_utf8_offset(label, lsp_signature->active_end);
		if (start == (size_t)-1) {
			assert(0);
			start = 0;
		}
		if (end == (size_t)-1) {
			assert(0);
			end = 0;
		}
		u32 active_start = (u32)start;
		u32 active_end = (u32)end;
		signature->label_pre = strn_dup(label, active_start);
		signature->label_active = strn_dup(label + active_start, active_end - active_start);
		signature->label_post = str_dup(label + active_end);
		printf("%s*%s*%s\n",signature->label_pre,signature->label_active,signature->label_post);
	}
	
	help->signature_count = (u16)signature_count;
}
