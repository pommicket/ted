void highlights_send_request(Ted *ted, TextBuffer *buffer) {
	Highlights *hls = &ted->highlights;
	if (!buffer) return;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	LSPDocumentPosition pos = buffer_cursor_pos_as_lsp_document_position(buffer);
	LSPRequest request = {.type = LSP_REQUEST_HIGHLIGHT};
	request.data.highlight.position = pos;
	hls->last_request_id = lsp_send_request(lsp, &request);
}

void highlights_close(Ted *ted) {
	arr_clear(ted->highlights.highlights);
}

void highlights_process_lsp_response(Ted *ted, LSPResponse *response) {
	Highlights *hls = &ted->highlights;
	if (response->request.type != LSP_REQUEST_HIGHLIGHT)
		return; // not a highlight request
	if (response->request.id != hls->last_request_id)
		return; // old request
	const LSPResponseHighlight *hl_response = &response->data.highlight;
	arr_set_len(hls->highlights, arr_len(hl_response->highlights));
	// type-safe memcpy
	for (u32 i = 0; i < arr_len(hl_response->highlights); ++i)
		hls->highlights[i] = hl_response->highlights[i];
}

void highlights_frame(Ted *ted) {
	Highlights *hls = &ted->highlights;
		
}
