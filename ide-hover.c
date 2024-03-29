// LSP hover information (textDocument/hover request)

#include "ted-internal.h"

struct Hover {
	LSPServerRequestID last_request;
	/// is some hover info being displayed?
	bool open;
	/// text to display
	char *text;
	/// where the hover data is coming from.
	/// we use this to check if we need to refresh it.
	LSPDocumentPosition requested_position;
	/// range in document to highlight
	LSPRange range;
	/// how long the cursor has been hovering for
	double time;
};

void hover_init(Ted *ted) {
	ted->hover = calloc(1, sizeof *ted->hover);
}

static void hover_close(Ted *ted) {
	Hover *hover = ted->hover;
	hover->open = false;
	free(hover->text);
	hover->text = NULL;
	ted_cancel_lsp_request(ted, &hover->last_request);
}

void hover_quit(Ted *ted) {
	hover_close(ted);
	free(ted->hover);
	ted->hover = NULL;
}

void hover_reset_timer(Ted *ted) {
	ted->hover->time = 0.0;
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

static void hover_send_request(Ted *ted) {
	Hover *hover = ted->hover;
	
	ted_cancel_lsp_request(ted, &hover->last_request);
	LSPRequest request = {.type = LSP_REQUEST_HOVER};
	LSPRequestHover *h = &request.data.hover;
	LSP *lsp = NULL;
	if (get_hover_position(ted, &h->position, NULL, &lsp)) {
		hover->requested_position = h->position;
		hover->last_request = lsp_send_request(lsp, &request);
	}	
}

void hover_process_lsp_response(Ted *ted, const LSPResponse *response) {
	if (!response) return;
	if (response->request.type != LSP_REQUEST_HOVER) return;
	
	Hover *hover = ted->hover;
	if (response->request.id != hover->last_request.id) {
		// stale request
		return;
	}
	
	hover->last_request.id = 0;
	const LSPResponseHover *hover_response = &response->data.hover;
	
	TextBuffer *buffer=0;
	LSPDocumentPosition pos={0};
	LSP *lsp=0;
	if (!get_hover_position(ted, &pos, &buffer, &lsp)) {
		free(hover->text); hover->text = NULL;
		return;
	}
	
	if (hover->text // we already have hover text
		&& (
		lsp_get_id(lsp) != hover->last_request.lsp // this request is from a different LSP
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
		for (; p > hover->text && strchr("\n \t", *p); --p)
			*p = '\0';
	}
}

void hover_frame(Ted *ted, double dt) {
	const Settings *settings = ted_active_settings(ted);
	if (!settings->hover_enabled)
		return;
	Hover *hover = ted->hover;
	
	bool key_down = ted_is_key_combo_down(ted, settings->hover_key);
	
	bool open_hover = key_down || hover->time >= settings->hover_time;
	
	hover->time += dt;
	
	if (!open_hover)
		hover_close(ted);
	
	(void)dt;
	if (!hover->open) {
		if (open_hover) {
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
			if (lsp_get_id(lsp) != hover->last_request.lsp
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
	const char *text = hover->text;
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	float x = ted_mouse_pos(ted).x, y = ted_mouse_pos(ted).y + char_height;
	
	buffer_highlight_lsp_range(buffer, hover->range, COLOR_HOVER_HL);
	
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
		gl_geometry_rect(rect, settings_color(settings, COLOR_HOVER_BG));
		gl_geometry_rect_border(rect, border, settings_color(settings, COLOR_HOVER_BORDER));
		settings_color_floats(settings, COLOR_HOVER_TEXT, state.color);
		text_utf8_with_state(font, &state, text);
	}
	
	gl_geometry_draw();
	text_render(font);
}
