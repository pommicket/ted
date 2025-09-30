#include "ted-internal.h"

struct RenameSymbol {
	LSPServerRequestID request_id;
	bool sent_rename;
	bool have_range;
	BufferPos range_start;
	BufferPos range_end;
};

static void rename_symbol_clear(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	ted_cancel_lsp_request(ted, &rs->request_id);
	rs->sent_rename = false;
	rs->have_range = false;
}

void rename_symbol_quit(Ted *ted) {
	rename_symbol_clear(ted);
	free(ted->rename_symbol);
	ted->rename_symbol = NULL;
}

void rename_symbol_start(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	TextBuffer *buffer = ted_active_buffer(ted);
	LSP *lsp = ted_active_lsp(ted);
	if (!buffer || !lsp) {
		ted_flash_error_cursor(ted);
		return;
	}
	if (lsp_has_prepare_rename(lsp)) {
		LSPRequest request = {.type = LSP_REQUEST_PREPARE_RENAME};
		LSPRequestPrepareRename *data = &request.data.prepare_rename;
		data->position = buffer_cursor_pos_as_lsp_document_position(buffer);
		rs->request_id = lsp_send_request(lsp, &request);
	}
	menu_open(ted, MENU_RENAME_SYMBOL);
}

void rename_symbol_at_cursor(Ted *ted, TextBuffer *buffer, const char *new_name) {
	if (!buffer) return;
	RenameSymbol *rs = ted->rename_symbol;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	if (!rs->sent_rename) {
		// send the request
		LSPRequest request = {.type = LSP_REQUEST_RENAME};
		LSPRequestRename *data = &request.data.rename;
		data->position = buffer_cursor_pos_as_lsp_document_position(buffer);
		data->new_name = lsp_request_add_string(&request, new_name);
		rs->request_id = lsp_send_request(lsp, &request);
		rs->sent_rename = true;
	}
}

void rename_symbol_frame(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	if (rs->request_id.id) {
		// we're just waitin'
		ted->cursor = ted->cursor_wait;
	}
}

static void rename_symbol_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
}

static void rename_symbol_menu_update(Ted *ted) {
	TextBuffer *line_buffer = ted->line_buffer;
	if (line_buffer_is_submitted(line_buffer)) {
		char *new_name = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
		rename_symbol_at_cursor(ted, ted->prev_active_buffer, new_name);
		free(new_name);
	}
}

static void rename_symbol_menu_render(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	// highlight symbol
	TextBuffer *buffer = ted->prev_active_buffer;
	if (!buffer) {
		menu_close(ted);
		return;
	}
	if (rs->sent_rename) {
		// already entered a new name
		return;
	}
	const Settings *settings = buffer_settings(buffer);
	const float padding = settings->padding;
	const float line_buffer_height = ted_line_buffer_height(ted);
	
	u32 sym_start=0, sym_end=0;
	BufferPos cursor_pos = buffer_cursor_pos(buffer);
	buffer_word_span_at_pos(buffer, cursor_pos, &sym_start, &sym_end);
	BufferPos bpos0 = {
		.line = cursor_pos.line,
		.index = sym_start
	};
	BufferPos bpos1 = {
		.line = cursor_pos.line,
		.index = sym_end
	};
	if (rs->have_range) {
		bpos0 = rs->range_start;
		bpos1 = rs->range_end;
	}
	// symbol should span from pos0 to pos1
	vec2 p0 = buffer_pos_to_pixels(buffer, bpos0);
	vec2 p1 = buffer_pos_to_pixels(buffer, bpos1);
	p1.y += text_font_char_height(buffer_font(buffer));
	Rect highlight = rect_endpoints(p0, p1);
	gl_geometry_rect_border(highlight, settings->border_thickness, settings_color(settings, COLOR_BORDER));
	gl_geometry_rect(highlight, settings_color(settings, COLOR_HOVER_HL));
	
	const float width = ted_get_menu_width(ted);
	const float height = line_buffer_height + 2 * padding;
	Rect bounds = {
		.pos = {(ted->window_width - width) / 2, padding},
		.size = {width, height},
	};
	gl_geometry_rect(bounds, settings_color(settings, COLOR_MENU_BG));
	gl_geometry_rect_border(bounds, settings->border_thickness, settings_color(settings, COLOR_BORDER));
	gl_geometry_draw();
	rect_shrink(&bounds, padding);
	const char *text = "Rename symbol to...";
	text_utf8(ted->font_bold, text, bounds.pos.x, bounds.pos.y, settings_color(settings, COLOR_TEXT));
	rect_shrink_left(&bounds, text_get_size_vec2(ted->font_bold, text).x + padding);
	text_render(ted->font_bold);
	
	buffer_render(ted->line_buffer, bounds);
}

static bool rename_symbol_menu_close(Ted *ted) {
	rename_symbol_clear(ted);
	buffer_clear(ted->line_buffer);
	return true;
}

void rename_symbol_process_lsp_response(Ted *ted, const LSPResponse *response) {
	RenameSymbol *rs = ted->rename_symbol;
	if (response->request.id != rs->request_id.id) {
		return;
	}
	if (response->request.type == LSP_REQUEST_PREPARE_RENAME) {
		if (lsp_response_is_error(response)) {
			// prepareRename returned an error
			menu_close(ted);
			ted_flash_error_cursor(ted);
			return;
		}
		const LSPResponsePrepareRename *prep = &response->data.prepare_rename;
		if (prep->use_range) {
			TextBuffer *buffer = ted_active_buffer_behind_menu(ted);
			if (!buffer) return;
			rs->range_start = buffer_pos_from_lsp(buffer, prep->range.start);
			rs->range_end = buffer_pos_from_lsp(buffer, prep->range.end);
			rs->have_range = true;
		}
		return;
	}
	if (response->request.type != LSP_REQUEST_RENAME)
		return;
	LSP *lsp = ted_get_lsp_by_id(ted, rs->request_id.lsp);

	if (menu_is_open(ted, MENU_RENAME_SYMBOL))
		menu_close(ted);
	if (lsp_response_is_error(response)) {
		ted_flash_error_cursor(ted);
		return;
	}
	const LSPResponseRename *data = &response->data.rename;
	if (!lsp) {
		// LSP crashed or something
		return;
	}
	ted_perform_workspace_edit(ted, lsp, response, data);
}

void rename_symbol_init(Ted *ted) {
	ted->rename_symbol = calloc(1, sizeof *ted->rename_symbol);
	MenuInfo menu = {
		.open = rename_symbol_menu_open,
		.close = rename_symbol_menu_close,
		.update = rename_symbol_menu_update,
		.render = rename_symbol_menu_render,
	};
	strbuf_cpy(menu.name, MENU_RENAME_SYMBOL);
	menu_register(ted, &menu);
}
