void usages_cancel_lookup(Ted *ted) {
	Usages *usages = &ted->usages;
	if (usages->last_request_id) {
		LSP *lsp = ted_get_lsp_by_id(ted, usages->last_request_lsp);
		lsp_cancel_request(lsp, usages->last_request_id);
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


void usages_process_lsp_response(Ted *ted, LSPResponse *response) {
	Usages *usages = &ted->usages;
	if (response->request.type != LSP_REQUEST_REFERENCES)
		return; // not for us
	if (response->request.id != usages->last_request_id)
		return;
	LSP *lsp = ted_get_lsp_by_id(ted, usages->last_request_lsp);
	LSPResponseReferences *refs = &response->data.references;
	if (lsp && arr_len(refs->locations)) { 
		TextBuffer *buffer = &ted->build_buffer;
		build_setup_buffer(ted);
		ted->build_shown = true;
		arr_foreach_ptr(refs->locations, LSPLocation, location) {
			char text[1024];
			strbuf_printf(text, "%s:%u: \n",
				lsp_document_path(lsp, location->document),
				location->range.start.line + 1);
			buffer_insert_utf8_at_cursor(buffer, text);
			buffer_cursor_move_to_end_of_file(buffer);
		}
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
