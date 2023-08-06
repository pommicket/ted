#include "ted-internal.h"

struct RenameSymbol {
	LSPServerRequestID request_id;
};

void rename_symbol_init(Ted *ted) {
	ted->rename_symbol = calloc(1, sizeof *ted->rename_symbol);
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
		data->position = buffer_pos_to_lsp_document_position(buffer, buffer->cursor_pos);
		data->new_name = str_dup(new_name);
		rs->request_id = lsp_send_request(lsp, &request);
	}
	
}

bool rename_symbol_is_loading(Ted *ted) {
	return ted->rename_symbol->request_id.id != 0;
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
