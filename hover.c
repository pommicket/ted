// LSP hover information (textDocument/hover request)

void hover_close(Ted *ted) {
	Hover *hover = &ted->hover;
	hover->time = 0.0;
	hover->open = false;
	free(hover->text);
	hover->text = NULL;
}

void hover_send_request(Ted *ted) {
	// find the buffer where the mouse is
	for (int i = 0; i < TED_MAX_BUFFERS; ++i) {
		TextBuffer *buffer = &ted->buffers[i];
		if (!buffer->filename) continue;
		LSP *lsp = buffer_lsp(buffer);
		if (!lsp) continue;
		BufferPos mouse_pos = {0};
		if (buffer_pixels_to_pos(buffer, ted->mouse_pos, &mouse_pos)) {
			// send the request
			LSPRequest request = {.type = LSP_REQUEST_HOVER};
			LSPRequestHover *h = &request.data.hover;
			h->position = buffer_pos_to_lsp_document_position(buffer, mouse_pos);
			lsp_send_request(lsp, &request);
			break;
		}
	}
}

void hover_process_lsp_response(Ted *ted, LSPResponse *response) {
	if (!response) return;
	if (response->request.type != LSP_REQUEST_HOVER) return;
	
	Hover *hover = &ted->hover;
	LSPResponseHover *hover_response = &response->data.hover;
	free(hover->text);
	hover->text = NULL;
	
	const char *contents = lsp_response_string(response, hover_response->contents);
	if (*contents) {
		hover->text = str_dup(contents);
		char *p = hover->text + strlen(hover->text) - 1;
		// remove trailing whitespace
		// (rust-analyzer gives us trailing newlines for local variables)
		for (; p > hover->text && isspace(*p); --p)
			*p = '\0';
	}
}

void hover_frame(Ted *ted, double dt) {
	Hover *hover = &ted->hover;
	
	if (ted->autocomplete.open) {
		hover_close(ted);
	}
	
	if (!hover->open) {
		hover->time += dt;
		if (hover->time > 1.0) {
			hover_send_request(ted);
			hover->open = true;
		}
		return;
	}
	
	if (!hover->text)
		return;
	
	const char *text = hover->text;
	u16 lines = 0; // number of lines of text
	for (int i = 0; text[i]; ++i)
		if (text[i] == '\n')
			++lines;
	
	//Font *font = ted->font;
	//float width = 200, height = lines * font->char_height;
	
	
}
