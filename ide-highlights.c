// highlight uses of identifier (LSP request textDocument/highlight)

#include "ted.h"

void highlights_close(Ted *ted) {
	Highlights *hls = &ted->highlights;
	arr_clear(hls->highlights);
	ted_cancel_lsp_request(ted, &hls->last_request);
	hls->requested_position = (LSPDocumentPosition){0};
}

static void highlights_send_request(Ted *ted) {
	TextBuffer *buffer = ted->active_buffer;
	Highlights *hls = &ted->highlights;
	if (!buffer) {
		highlights_close(ted);
		return;
	}
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) {
		highlights_close(ted);
		return;
	}
	LSPDocumentPosition pos = buffer_cursor_pos_as_lsp_document_position(buffer);
	LSPRequest request = {.type = LSP_REQUEST_HIGHLIGHT};
	request.data.highlight.position = pos;
	
	ted_cancel_lsp_request(ted, &hls->last_request);
	hls->last_request = lsp_send_request(lsp, &request);
	hls->requested_position = pos;
}


void highlights_process_lsp_response(Ted *ted, LSPResponse *response) {
	Highlights *hls = &ted->highlights;
	if (response->request.type != LSP_REQUEST_HIGHLIGHT)
		return; // not a highlight request
	if (response->request.id != hls->last_request.id)
		return; // old request
	const LSPResponseHighlight *hl_response = &response->data.highlight;
	arr_set_len(hls->highlights, arr_len(hl_response->highlights));
	// type-safe memcpy
	for (u32 i = 0; i < arr_len(hl_response->highlights); ++i)
		hls->highlights[i] = hl_response->highlights[i];
}

void highlights_frame(Ted *ted) {
	Highlights *hls = &ted->highlights;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) {
		highlights_close(ted);
		return;
	}
	const Settings *settings = buffer_settings(buffer);
	bool key_down = ted_is_key_combo_down(ted, settings->highlight_key);
	if (!settings->highlight_enabled
		|| (!settings->highlight_auto && !key_down)) {
		highlights_close(ted);
		return;
	}
	
	LSPDocumentPosition pos = buffer_cursor_pos_as_lsp_document_position(buffer);
	if (!lsp_document_position_eq(pos, hls->requested_position)) {
		// cursor moved or something. let's resend the request.
		highlights_send_request(ted);
	}
	
	arr_foreach_ptr(hls->highlights, LSPHighlight, hl) {
		ColorSetting color = COLOR_HOVER_HL;
		if (hl->kind == LSP_HIGHLIGHT_WRITE)
			color = COLOR_HL_WRITE;
		buffer_highlight_lsp_range(buffer, hl->range, color);
	}
	gl_geometry_draw();
}
