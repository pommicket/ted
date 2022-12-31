// LSP hover information (textDocument/hover request)

void hover_close(Ted *ted) {
	Hover *hover = &ted->hover;
	hover->open = false;
	free(hover->text);
	hover->text = NULL;
}

static bool get_hover_position(Ted *ted, LSPDocumentPosition *pos, TextBuffer **pbuffer, LSP **lsp) {
	BufferPos mouse_pos = {0};
	TextBuffer *buffer = NULL;
	if (ted_get_mouse_buffer_pos(ted, &buffer, &mouse_pos)) {
		LSP *l = buffer_lsp(buffer);
		if (!l) return false;
		if (pos) *pos = buffer_pos_to_lsp_document_position(buffer, mouse_pos);
		if (pbuffer) *pbuffer = buffer;
		if (lsp) *lsp = l;
		return true;
	}
	
	return false;
}

void hover_send_request(Ted *ted) {
	Hover *hover = &ted->hover;
	LSPRequest request = {.type = LSP_REQUEST_HOVER};
	LSPRequestHover *h = &request.data.hover;
	LSP *lsp = NULL;
	if (get_hover_position(ted, &h->position, NULL, &lsp)) {
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
	
	TextBuffer *buffer=0;
	LSPDocumentPosition pos={0};
	LSP *lsp=0;
	get_hover_position(ted, &pos, &buffer, &lsp);
	
	if (hover->text // we already have hover text
		&& (
		lsp->id != hover->requested_lsp // this request is from a different LSP
		|| !lsp_document_position_eq(response->request.data.hover.position, pos) // this request is for a different position
		)) {
		// this is a stale request. ignore it
		return;
	}
	
	free(hover->text); hover->text = NULL;
	
	hover->range = hover_response->range;
	
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
	const Settings *settings = ted_active_settings(ted);
	if (!settings->hover_enabled)
		return;
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
	
	TextBuffer *buffer=0;
	{
		LSPDocumentPosition pos={0};
		LSP *lsp=0;
		if (get_hover_position(ted, &pos, &buffer, &lsp)) {
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
	
	
	
	const float padding = settings->padding;
	const float border = settings->border_thickness;
	const u32 *colors = settings->colors;
	const char *text = hover->text;
	Font *font = ted->font;
	float x = ted->mouse_pos.x, y = ted->mouse_pos.y + font->char_height;
	float char_height = font->char_height;
	
	ted_highlight_lsp_range(ted, buffer, hover->range);
	
	if (hover->text) {
		float max_width = 400;
		TextRenderState state = text_render_state_default;
		state.x = state.min_x = x;
		state.y = state.min_y = y;
		state.render = false;
		state.wrap = true;
		state.max_x = x + max_width;
		state.max_y = ted->window_height;
		text_utf8_with_state(font, &state, text);
		float width = (float)(state.x_largest - x);
		float height = (float)(state.y_largest - y) + char_height;
		if (height > 300) {
			height = 300;
		}
		
		if (x + width > ted->window_width)
			x -= width; // open left
		if (y + height > ted->window_height)
			y -= height + char_height * 2; // open up
		state.x = state.min_x = x;
		state.y = state.min_y = y;
		state.max_x = x + max_width;
		state.y = y;
		state.render = true;
		state.max_y = y + height;
		
		Rect rect = rect_xywh(x - padding, y - padding, width + 2*padding, height + 2*padding);
		gl_geometry_rect(rect, colors[COLOR_HOVER_BG]);
		gl_geometry_rect_border(rect, border, colors[COLOR_HOVER_BORDER]);
		rgba_u32_to_floats(colors[COLOR_HOVER_TEXT], state.color);
		text_utf8_with_state(font, &state, text);
	}
	
	gl_geometry_draw();
	text_render(font);
}
