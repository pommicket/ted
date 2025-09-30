#include "ted-internal.h"

struct CodeAction {
	LSPServerRequestID last_request;
	LSPResponse response;
};

static bool ranges_touch(BufferPos p1, BufferPos p2, BufferPos q1, BufferPos q2) {
	int cmp21 = buffer_pos_cmp(p2, q1);
	if (cmp21 < 0) {
		// p is entirely before q
		return false;
	}
	int cmp12 = buffer_pos_cmp(p1, q2);
	if (cmp12 > 0) {
		// p is entirely after q
		return false;
	}
	return true;
}

void code_action_init(Ted *ted) {
	ted->code_action = calloc(1, sizeof *ted->code_action);
}

void code_action_open(Ted *ted) {
	CodeAction *c = ted->code_action;
	ted_cancel_lsp_request(ted, &c->last_request);
	TextBuffer *buffer = ted_active_buffer(ted);
	if (!buffer) return;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	autocomplete_close(ted);
	BufferPos range_start = {0}, range_end = {0};
	LSPRange range = {0};
	if (buffer_selection_pos(buffer, &range_start)) {
		range = buffer_selection_as_lsp_range(buffer);
		range_end = buffer_cursor_pos(buffer);
		if (buffer_pos_cmp(range_start, range_end) > 0) {
			BufferPos tmp = range_start;
			range_start = range_end;
			range_end = tmp;
		}
	} else {
		range_start = range_end = buffer_cursor_pos(buffer);
		range.start = range.end = buffer_cursor_pos_as_lsp_position(buffer);
	}
	LSPRequest req = {
		.type = LSP_REQUEST_CODE_ACTION,
	};
	LSPRequestCodeAction *code_action_req = &req.data.code_action;
	code_action_req->document = buffer_lsp_document_id(buffer);
	code_action_req->range = range;
	arr_foreach_ptr(buffer_diagnostics(buffer), const Diagnostic, d) {
		if (ranges_touch(range_start, range_end, d->pos, d->end)) {
			LSPString raw = lsp_request_add_string(&req, d->raw);
			arr_add(code_action_req->raw_diagnostics, raw);
		}
	}
	c->last_request = lsp_send_request(lsp, &req);
}

bool code_action_is_open(Ted *ted) {
	CodeAction *c = ted->code_action;
	return arr_len(c->response.data.code_action.actions) != 0;
}

void code_action_close(Ted *ted) {
	CodeAction *c = ted->code_action;
	lsp_response_free(&c->response);
	memset(&c->response, 0, sizeof c->response);
}

bool code_action_process_lsp_response(Ted *ted, const LSPResponse *response) {
	CodeAction *c = ted->code_action;
	if (response->request.id != c->last_request.id
		|| response->request.type != LSP_REQUEST_CODE_ACTION) {
		return false;
	}
	if (arr_len(response->data.code_action.actions) == 0) {
		// no code actions
		code_action_close(ted);
		ted_flash_error_cursor(ted);
		return false;
	}
	lsp_response_free(&c->response);
	c->response = *response;
	return true;
}

void code_action_quit(Ted *ted) {
	CodeAction *c = ted->code_action;
	code_action_close(ted);
	free(c);
	ted->code_action = NULL;
}

static void code_action_perform(Ted *ted, const LSPCodeAction *action) {
	CodeAction *c = ted->code_action;
	const LSPResponse *response = &c->response;
	LSPServerRequestID request_id = c->last_request;
	LSP *lsp = ted_get_lsp_by_id(ted, request_id.lsp);
	ted_perform_workspace_edit(ted, lsp, response, &action->edit);
}

void code_action_frame(Ted *ted) {
	CodeAction *c = ted->code_action;
	LSPResponse *response = &c->response;
	const LSPCodeAction *code_actions = response->data.code_action.actions;
	if (arr_len(code_actions) == 0)
		return;
	TextBuffer *buffer = ted_active_buffer(ted);
	if (!buffer) {
		code_action_close(ted);
		return;
	}
	const Settings *settings = ted_active_settings(ted);
	vec2 cursor_pos = buffer_pos_to_pixels(buffer, buffer_cursor_pos(buffer));
	float x = cursor_pos.x, y = cursor_pos.y;
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	float padding = settings->padding;
	float border_thickness = settings->border_thickness;
	float panel_width = 0, panel_height = (char_height + border_thickness) * (float)arr_len(code_actions);
	arr_foreach_ptr(code_actions, const LSPCodeAction, action) {
		const char *name = lsp_response_string(response, action->name);
		float row_width = text_get_size_vec2(font, name).x
			+ char_height * 6 + padding * 2;
		if (row_width > panel_width)
			panel_width = row_width;
	}
	if (x > ted->window_width / 2) {
		x -= panel_width;
	}
	if (y > ted->window_height / 2) {
		y -= panel_height + char_height;
	} else {
		y += char_height;
	}
	Rect panel_rect = {{x,y},{panel_width,panel_height}};
	gl_geometry_rect(panel_rect, settings_color(settings, COLOR_BG));
	gl_geometry_rect_border(panel_rect, border_thickness, settings_color(settings, COLOR_AUTOCOMPLETE_BORDER));
	const LSPCodeAction *selected_action = NULL;
	arr_foreach_ptr(code_actions, const LSPCodeAction, action) {
		const char *name = lsp_response_string(response, action->name);
		Rect entry_rect = {{x, y}, {panel_width, border_thickness + char_height}};
		if (rect_contains_point(entry_rect, ted->mouse_pos)) {
			ted->cursor = ted->cursor_hand;
			gl_geometry_rect(entry_rect, settings_color(settings, COLOR_AUTOCOMPLETE_HL));			
		}
		arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], const MouseClick, click)
			if (rect_contains_point(entry_rect, click->pos))
				selected_action = action;
		if (action != code_actions) {
			Rect border = {{x, y}, {panel_width, border_thickness}};
			gl_geometry_rect(border, settings_color(settings, COLOR_AUTOCOMPLETE_BORDER));
			y += border_thickness;
		};
		text_utf8(font, name, x + padding, y, settings_color(settings, COLOR_TEXT));
		y += char_height;
	}
	gl_geometry_draw();
	text_render(font);
	if (selected_action) {
		code_action_perform(ted, selected_action);
		code_action_close(ted);
	} else {
		arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], const MouseClick, click) {
			if (!rect_contains_point(panel_rect, click->pos)) {
				code_action_close(ted);
			}
		}
	}
}
