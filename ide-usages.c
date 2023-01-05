// find usages of symbol

#include "ted.h"

void usages_cancel_lookup(Ted *ted) {
	Usages *usages = &ted->usages;
	if (usages->last_request_id) {
		ted_cancel_lsp_request(ted, usages->last_request_lsp, usages->last_request_id);
		usages->last_request_id = 0;
	}
}

void usages_find(Ted *ted) {
	Usages *usages = &ted->usages;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) return;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	
	// send the request
	LSPRequest request = {.type = LSP_REQUEST_REFERENCES};
	LSPRequestReferences *refs = &request.data.references;
	refs->include_declaration = true;
	refs->position = buffer_cursor_pos_as_lsp_document_position(buffer);
	usages_cancel_lookup(ted);
	usages->last_request_lsp = lsp->id;
	usages->last_request_id = lsp_send_request(lsp, &request);
	usages->last_request_time = ted->frame_time;
}


void usages_process_lsp_response(Ted *ted, const LSPResponse *response) {
	Usages *usages = &ted->usages;
	if (response->request.type != LSP_REQUEST_REFERENCES)
		return; // not for us
	if (response->request.id != usages->last_request_id)
		return;
	LSP *lsp = ted_get_lsp_by_id(ted, usages->last_request_lsp);
	const LSPResponseReferences *refs = &response->data.references;
	if (lsp && arr_len(refs->locations)) { 
		TextBuffer *buffer = &ted->build_buffer;
		build_setup_buffer(ted);
		ted->build_shown = true;
		char last_path[TED_PATH_MAX] = {0};
		TextBuffer *last_buffer = NULL;
		FILE *last_file = NULL;
		u32 last_line = 0;
		
		arr_foreach_ptr(refs->locations, LSPLocation, location) {
			const char *path = lsp_document_path(lsp, location->document);
			
			if (!paths_eq(path, last_path)) {
				// it's a new file!
				strbuf_cpy(last_path, path);
				if (last_file) {
					fclose(last_file);
					last_file = NULL;
				}
				last_buffer = ted_get_buffer_with_file(ted, path);
				if (!last_buffer) {
					last_file = fopen(path, "rb");
					last_line = 0;
				}
			}
			
			u32 line = location->range.start.line;
			
			char *line_text = NULL;
			if (last_buffer) {
				// read the line from the buffer
				if (line < last_buffer->nlines) {
					BufferPos pos = {.line = line, .index = 0};
					line_text = buffer_get_utf8_text_at_pos(last_buffer, pos, last_buffer->lines[line].len);
				}
			} else if (last_file) {
				// read the line from the file
				while (last_line < line) {
					int c = getc(last_file);
					if (c == '\n') ++last_line;
					if (c == EOF) {
						fclose(last_file);
						last_file = NULL;
						break;
					}
				}
				
				line_text = calloc(1, 1024);
				if (last_file && last_line == line) {
					char *p = line_text;
					for (u32 i = 0; i < 1023; ++i, ++p) {
						int c = getc(last_file);
						if (c == '\n') {
							++last_line;
							break;
						}
						if (c == EOF) {
							fclose(last_file);
							last_file = NULL;
							break;
						}
						line_text[i] = (char)c;
					}
				}
			}
			
			char text[1024];
			strbuf_printf(text, "%s:%u: %s\n",
				path,
				line + 1,
				line_text ? line_text + strspn(line_text, "\t ") : "");
			free(line_text);
			buffer_insert_utf8_at_cursor(buffer, text);
			buffer_cursor_move_to_end_of_file(buffer);
		}
		if (last_file)
			fclose(last_file);
		buffer->view_only = true;
		
		// the build_dir doesn't really matter since we're using absolute paths
		// but might as well set it to something reasonable.
		char *root = ted_get_root_dir(ted);
		strbuf_cpy(ted->build_dir, root);
		free(root);
		
		build_check_for_errors(ted);
	} else {
		ted_flash_error_cursor(ted);
	}
	usages->last_request_id = 0;
}

void usages_frame(Ted *ted) {
	Usages *usages = &ted->usages;
	if (usages->last_request_id && ted->frame_time - usages->last_request_time > 0.2)
		ted->cursor = ted->cursor_wait; // this request is takin a while
}
