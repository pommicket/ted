#include "ted.h"

void rename_symbol_clear(Ted *ted) {
	RenameSymbol *rs = &ted->rename_symbol;
	ted_cancel_lsp_request(ted, &rs->request_id);
	free(rs->new_name);
	rs->new_name = NULL;
	if (ted->menu == MENU_RENAME_SYMBOL)
		menu_close(ted);
}

void rename_symbol_frame(Ted *ted) {
	RenameSymbol *rs = &ted->rename_symbol;
	TextBuffer *buffer = ted->prev_active_buffer;
	LSP *lsp = buffer ? buffer_lsp(buffer) : NULL;
	
	if (ted->menu != MENU_RENAME_SYMBOL || !buffer || !lsp) {
		rename_symbol_clear(ted);
		return;
	}
	
	if (rs->new_name && !rs->request_id.id) {
		// send the request
		LSPRequest request = {.type = LSP_REQUEST_RENAME};
		LSPRequestRename *data = &request.data.rename;
		data->position = buffer_pos_to_lsp_document_position(buffer, buffer->cursor_pos);
		data->new_name = str_dup(rs->new_name);
		rs->request_id = lsp_send_request(lsp, &request);
	}
	
	// we're just waitin'
	ted->cursor = ted->cursor_wait;
}

void rename_symbol_process_lsp_response(Ted *ted, LSPResponse *response) {
	RenameSymbol *rs = &ted->rename_symbol;
	if (response->request.type != LSP_REQUEST_RENAME
		|| response->request.id != rs->request_id.id) {
		return;
	}
	
	LSPResponseRename *data = &response->data.rename;
	LSP *lsp = ted_get_lsp_by_id(ted, rs->request_id.lsp);
	if (!lsp) {
		// LSP crashed or something
		return;
	}
	
	assert(rs->new_name);
	
	bool perform_changes = true;
	arr_foreach_ptr(data->changes, LSPWorkspaceChange, change) {
		if (change->type != LSP_CHANGE_EDIT) {
			// TODO(eventually) : support these
			ted_error(ted, "rename is too complicated for ted to perform.");
			perform_changes = false;
		}
	}
	
	if (perform_changes) {
		arr_foreach_ptr(data->changes, LSPWorkspaceChange, change) {
			switch (change->type) {
			case LSP_CHANGE_EDIT: {
				LSPWorkspaceChangeEdit *change_data = &change->data.edit;
				const char *path = lsp_document_path(lsp, change_data->document);
				if (!ted_open_file(ted, path)) goto done;
				
				TextBuffer *buffer = ted_get_buffer_with_file(ted, path);
				if (!buffer) {
					// this should never happen since we just
					// successfully opened it
					assert(0);
					goto done;
				}
				
				LSPTextEdit *edit = &change_data->edit;
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
		done:;
	}
	
	rename_symbol_clear(ted);
}
