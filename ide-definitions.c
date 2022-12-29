void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition position) {
	if (lsp) {
		// send that request
		LSPRequest request = {.type = LSP_REQUEST_DEFINITION};
		request.data.definition.position = position;
		lsp_send_request(lsp, &request);
	} else {
		// just go to the tag
		tag_goto(ted, name);
	}
}

void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response) {
	if (response->request.type != LSP_REQUEST_DEFINITION)
		return;
	
	const LSPResponseDefinition *response_def = &response->data.definition;
	Definitions *defs = &ted->definitions;
	
	if (defs->last_response_lsp == lsp->id
		&& response->request.id < defs->last_response_id) {
		// we just processed a later response, so let's ignore this
		return;
	}
	defs->last_response_lsp = lsp->id;
	defs->last_response_id = response->request.id;
	
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
