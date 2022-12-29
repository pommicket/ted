// LSP hover information (textDocument/hover request)

void hover_close(Ted *ted) {
	Hover *hover = &ted->hover;
	hover->open = false;
	free(hover->text);
	hover->text = NULL;
}

static bool get_hover_position(Ted *ted, LSPDocumentPosition *pos, LSP **lsp) {
	// find the buffer where the mouse is
	for (int i = 0; i < TED_MAX_BUFFERS; ++i) {
		TextBuffer *buffer = &ted->buffers[i];
		if (!buffer->filename) continue;
		LSP *l = buffer_lsp(buffer);
		if (!l) continue;
		BufferPos mouse_pos = {0};
		if (buffer_pixels_to_pos(buffer, ted->mouse_pos, &mouse_pos)) {
			*pos = buffer_pos_to_lsp_document_position(buffer, mouse_pos);
			*lsp = l;
			return true;
		}
	}
	return false;
}
void hover_send_request(Ted *ted) {
	Hover *hover = &ted->hover;
	LSPRequest request = {.type = LSP_REQUEST_HOVER};
	LSPRequestHover *h = &request.data.hover;
	LSP *lsp = NULL;
	if (get_hover_position(ted, &h->position, &lsp)) {
		hover->requested_position = h->position;
		hover->requested_lsp = lsp->id;
		lsp_send_request(lsp, &request);
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
	
	bool shift_down = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LSHIFT]
		|| SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RSHIFT];
	
	if (!shift_down) {
		hover_close(ted);
	}
	
	(void)dt;
	if (!hover->open) {
		if (shift_down) {
			hover_send_request(ted);
			hover->open = true;
		}
		return;
	}
	
	{
		LSPDocumentPosition pos={0};
		LSP *lsp=0;
		if (get_hover_position(ted, &pos, &lsp)) {
			if (lsp->id != hover->requested_lsp
				|| !lsp_document_position_eq(pos, hover->requested_position)) {
				// refresh hover
				hover_send_request(ted);
			}
		} else {
			hover_close(ted);
			return;
		}
	}
	
	if (!hover->text)
		return;
	
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	const char *text = hover->text;
	Font *font = ted->font;
	float x = ted->mouse_pos.x, y = ted->mouse_pos.y;
	TextRenderState state = text_render_state_default;
	state.x = state.min_x = x;
	state.y = state.min_y = y;
	state.render = false;
	state.wrap = true;
	state.max_x = x + 400;
	state.max_y = ted->window_height;
	text_utf8_with_state(font, &state, text);
	float width = (float)(state.x_largest - x);
	float height = (float)(state.y_largest - y) + font->char_height;
	if (height > 300) {
		height = 300;
	}
	state.x = x;
	state.y = y;
	state.render = true;
	state.max_y = y + height;
	gl_geometry_rect(rect_xywh(x, y, width, height), colors[COLOR_AUTOCOMPLETE_BG]);
	rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
	text_utf8_with_state(font, &state, text);
	gl_geometry_draw();
	text_render(font);
}
