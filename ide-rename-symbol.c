#include "ted-internal.h"

struct RenameSymbol {
	LSPServerRequestID request_id;
};

void rename_symbol_quit(Ted *ted) {
	rename_symbol_clear(ted);
	free(ted->rename_symbol);
	ted->rename_symbol = NULL;
}

void rename_symbol_at_cursor(Ted *ted, TextBuffer *buffer, const char *new_name) {
	if (!buffer) return;
	RenameSymbol *rs = ted->rename_symbol;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	
	if (!rs->request_id.id) {
		// send the request
		LSPRequest request = {.type = LSP_REQUEST_RENAME};
		LSPRequestRename *data = &request.data.rename;
		data->position = buffer_pos_to_lsp_document_position(buffer, buffer->cursor_pos);
		data->new_name = str_dup(new_name);
		rs->request_id = lsp_send_request(lsp, &request);
	}
	
}

void rename_symbol_clear(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	ted_cancel_lsp_request(ted, &rs->request_id);
}

void rename_symbol_frame(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	if (rs->request_id.id) {
		// we're just waitin'
		ted->cursor = ted->cursor_wait;
	}
}

static void rename_symbol_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, &ted->line_buffer);
}

static void rename_symbol_menu_update(Ted *ted) {
	TextBuffer *line_buffer = &ted->line_buffer;
	if (line_buffer->line_buffer_submitted) {
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
	if (rs->request_id.id) {
		// already entered a new name
		return;
	}
	const Settings *settings = buffer_settings(buffer);
	const float padding = settings->padding;
	const u32 *colors = settings->colors;
	const float line_buffer_height = ted_line_buffer_height(ted);
	
	u32 sym_start=0, sym_end=0;
	BufferPos cursor_pos = buffer->cursor_pos;
	buffer_word_span_at_pos(buffer, cursor_pos, &sym_start, &sym_end);
	BufferPos bpos0 = {
		.line = cursor_pos.line,
		.index = sym_start
	};
	BufferPos bpos1 = {
		.line = cursor_pos.line,
		.index = sym_end
	};
	// symbol should span from pos0 to pos1
	vec2 p0 = buffer_pos_to_pixels(buffer, bpos0);
	vec2 p1 = buffer_pos_to_pixels(buffer, bpos1);
	p1.y += text_font_char_height(buffer_font(buffer));
	Rect highlight = rect_endpoints(p0, p1);
	gl_geometry_rect_border(highlight, settings->border_thickness, colors[COLOR_BORDER]);
	gl_geometry_rect(highlight, colors[COLOR_HOVER_HL]);
	
	const float width = ted_get_menu_width(ted);
	const float height = line_buffer_height + 2 * padding;
	Rect bounds = {
		.pos = {(ted->window_width - width) / 2, padding},
		.size = {width, height},
	};
	gl_geometry_rect(bounds, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(bounds, settings->border_thickness, colors[COLOR_BORDER]);
	gl_geometry_draw();
	rect_shrink(&bounds, padding);
	const char *text = "Rename symbol to...";
	text_utf8(ted->font_bold, text, bounds.pos.x, bounds.pos.y, colors[COLOR_TEXT]);
	rect_shrink_left(&bounds, text_get_size_vec2(ted->font_bold, text).x + padding);
	text_render(ted->font_bold);
	
	buffer_render(&ted->line_buffer, bounds);
}

static bool rename_symbol_menu_close(Ted *ted) {
	rename_symbol_clear(ted);
	buffer_clear(&ted->line_buffer);
	return true;
}

void rename_symbol_process_lsp_response(Ted *ted, const LSPResponse *response) {
	RenameSymbol *rs = ted->rename_symbol;
	if (response->request.type != LSP_REQUEST_RENAME
		|| response->request.id != rs->request_id.id) {
		return;
	}
	
	const LSPResponseRename *data = &response->data.rename;
	LSP *lsp = ted_get_lsp_by_id(ted, rs->request_id.lsp);
	if (!lsp) {
		// LSP crashed or something
		return;
	}
	
	bool perform_changes = true;
	arr_foreach_ptr(data->changes, const LSPWorkspaceChange, change) {
		if (change->type != LSP_CHANGE_EDIT) {
			// TODO(eventually) : support these
			ted_error(ted, "rename is too complicated for ted to perform.");
			perform_changes = false;
		}
	}
	
	if (perform_changes) {
		
		arr_foreach_ptr(data->changes, const LSPWorkspaceChange, change) {
			switch (change->type) {
			case LSP_CHANGE_EDIT: {
				const LSPWorkspaceChangeEdit *change_data = &change->data.edit;
				const char *path = lsp_document_path(lsp, change_data->document);
				if (!ted_open_file(ted, path)) goto done;
				
				TextBuffer *buffer = ted_get_buffer_with_file(ted, path);
				if (!buffer->will_chain_edits) {
					// chain all edits together so they can be undone with one ctrl+z
					buffer_start_edit_chain(buffer);
				}
				
				if (!buffer) {
					// this should never happen since we just
					// successfully opened it
					assert(0);
					goto done;
				}
				
				
				const LSPTextEdit *edit = &change_data->edit;
				BufferPos start = buffer_pos_from_lsp(buffer, edit->range.start);
				BufferPos end = buffer_pos_from_lsp(buffer, edit->range.end);
				buffer_delete_chars_between(buffer, start, end);
				buffer_insert_utf8_at_pos(buffer, start, lsp_response_string(response, edit->new_text));
				
				}
				break;
			case LSP_CHANGE_RENAME:
			case LSP_CHANGE_DELETE:
			case LSP_CHANGE_CREATE:
				assert(0);
				break;
			}
		}
		done:
		
		// end all edit chains in all buffers
		// they're almost definitely all created by us
		for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
			if (ted->buffers_used[i]) {
				TextBuffer *buffer = &ted->buffers[i];
				buffer_end_edit_chain(buffer);
			}
		}
	}
	
	rename_symbol_clear(ted);
	if (menu_is_open(ted, MENU_RENAME_SYMBOL))
		menu_close(ted);
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
