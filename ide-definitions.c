void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition position) {
	Definitions *defs = &ted->definitions;
	if (lsp) {
		// send that request
		LSPRequest request = {.type = LSP_REQUEST_DEFINITION};
		request.data.definition.position = position;
		LSPRequestID id = lsp_send_request(lsp, &request);
		defs->last_request_lsp = lsp->id;
		defs->last_request_id = id;
		defs->last_request_time = ted->frame_time;
	} else {
		// just go to the tag
		tag_goto(ted, name);
	}
}

void definition_cancel_lookup(Ted *ted) {
	Definitions *defs = &ted->definitions;
	defs->last_request_lsp = 0;
	defs->last_request_id = 0;
}

void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response) {
	if (response->request.type != LSP_REQUEST_DEFINITION)
		return;
	
	const LSPResponseDefinition *response_def = &response->data.definition;
	Definitions *defs = &ted->definitions;
	
	if (defs->last_request_lsp != lsp->id
		|| response->request.id != defs->last_request_id) {
		// response to an old request
		return;
	}
	
	defs->last_request_lsp = 0;
	defs->last_request_id = 0;
	
	if (!arr_len(response_def->locations)) {
		// no definition. do the error cursor.
		ted_flash_error_cursor(ted);
		return;
	}
	LSPLocation location = response_def->locations[0];
	const char *path = lsp_document_path(lsp, location.document);
	if (!ted_open_file(ted, path)) {	
		ted_flash_error_cursor(ted);
		return;
	}
	LSPDocumentPosition position = lsp_location_start_position(location);
	ted_go_to_lsp_document_position(ted, lsp, position);
}

void definitions_frame(Ted *ted) {
	Definitions *defs = &ted->definitions;
	if (defs->last_request_lsp && timespec_sub(ted->frame_time, defs->last_request_time) > 0.2) {
		ted->cursor = ted->cursor_wait;
	}
}
