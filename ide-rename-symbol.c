#include "ted-internal.h"

struct RenameSymbol {
	LSPServerRequestID request_id;
};

static void rename_symbol_clear(Ted *ted) {
	RenameSymbol *rs = ted->rename_symbol;
	ted_cancel_lsp_request(ted, &rs->request_id);
}

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
		data->position = buffer_cursor_pos_as_lsp_document_position(buffer);
		data->new_name = lsp_request_add_string(&request, new_name);
		rs->request_id = lsp_send_request(lsp, &request);
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
	if (rs->request_id.id) {
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
	if (response->request.type != LSP_REQUEST_RENAME
		|| response->request.id != rs->request_id.id) {
		return;
	}
	LSP *lsp = ted_get_lsp_by_id(ted, rs->request_id.lsp);

	if (menu_is_open(ted, MENU_RENAME_SYMBOL))
		menu_close(ted);
	const LSPResponseRename *data = &response->data.rename;
	if (!lsp) {
		// LSP crashed or something
		return;
	}
	TextBuffer *const start_buffer = ted_active_buffer(ted);
	
	arr_foreach_ptr(data->changes, const LSPWorkspaceChange, change) {
		if (change->type == LSP_CHANGE_DELETE && change->data.delete.recursive) {
			ted_error(ted, "refusing to perform rename because it involves a recursive deletion\n"
				"I'm too scared to go through with this");
			return;
		}
	}
	
	arr_foreach_ptr(data->changes, const LSPWorkspaceChange, change) {
		switch (change->type) {
		case LSP_CHANGE_EDITS: {
			const LSPWorkspaceChangeEdit *change_data = &change->data.edit;
			const char *path = lsp_document_path(lsp, change_data->document);
			if (!ted_open_file(ted, path)) goto done;
			
			TextBuffer *buffer = ted_get_buffer_with_file(ted, path);
			// chain all edits together so they can be undone with one ctrl+z
			buffer_start_edit_chain(buffer);
			
			if (!buffer) {
				// this should never happen since we just
				// successfully opened it
				assert(0);
				goto done;
			}
			
			buffer_apply_lsp_text_edits(buffer, response, change_data->edits, arr_len(change_data->edits));
			}
			break;
		case LSP_CHANGE_RENAME: {
			const LSPWorkspaceChangeRename *rename = &change->data.rename;
			const char *old = lsp_document_path(lsp, rename->old);
			const char *new = lsp_document_path(lsp, rename->new);
			FsType new_type = fs_path_type(new);
			if (new_type == FS_DIRECTORY) {
				ted_error(ted, "Aborting rename since it's asking to overwrite a directory.");
				goto done;
			}
			
			if (rename->ignore_if_exists && new_type != FS_NON_EXISTENT) {
				break;
			}
			if (!rename->overwrite && new_type != FS_NON_EXISTENT) {
				ted_error(ted, "Aborting rename since it would overwrite a file.");
				goto done;
			}
			os_rename_overwrite(old, new);
			if (ted_close_buffer_with_file(ted, old))
				ted_open_file(ted, new);
		} break;
		case LSP_CHANGE_DELETE: {
			const LSPWorkspaceChangeDelete *delete = &change->data.delete;
			const char *path = lsp_document_path(lsp, delete->document);
			remove(path);
			ted_close_buffer_with_file(ted, path);
		} break;
		case LSP_CHANGE_CREATE: {
			const LSPWorkspaceChangeCreate *create = &change->data.create;
			const char *path = lsp_document_path(lsp, create->document);
			FILE *fp = fopen(path, create->overwrite ? "wb" : "ab");
			if (fp) fclose(fp);
			ted_open_file(ted, path);
		} break;
		}
	}
	done:

	{
		// end all edit chains in all buffers
		// they're almost definitely all created by us
		arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
			buffer_end_edit_chain(*pbuffer);
		}
		
		ted_save_all(ted);
	}
	ted_switch_to_buffer(ted, start_buffer);
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
