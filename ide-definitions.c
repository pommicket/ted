
static SymbolKind symbol_kind_to_ted(LSPSymbolKind kind) {
	switch (kind) {
	case LSP_SYMBOL_OTHER:
	case LSP_SYMBOL_FILE:
	case LSP_SYMBOL_MODULE:
	case LSB_SYMBOL_NAMESPACE:
	case LSP_SYMBOL_PACKAGE:
		return SYMBOL_OTHER;
	
	case LSP_SYMBOL_CLASS:
	case LSP_SYMBOL_TYPEPARAMETER:
	case LSP_SYMBOL_ENUM:
	case LSP_SYMBOL_INTERFACE:
	case LSP_SYMBOL_STRUCT:
	case LSP_SYMBOL_EVENT: // i have no clue what this is. let's say it's a type.
		return SYMBOL_TYPE;
	
	case LSP_SYMBOL_PROPERTY:
	case LSP_SYMBOL_FIELD:
	case LSP_SYMBOL_KEY:
		return SYMBOL_FIELD;
	
	case LSP_SYMBOL_CONSTRUCTOR:
	case LSP_SYMBOL_FUNCTION:
	case LSP_SYMBOL_OPERATOR:
	case LSP_SYMBOL_METHOD:
		return SYMBOL_FUNCTION;
	
	case LSP_SYMBOL_VARIABLE:
		return SYMBOL_VARIABLE;
	
	case LSP_SYMBOL_CONSTANT:
	case LSP_SYMBOL_STRING:
	case LSP_SYMBOL_NUMBER:
	case LSP_SYMBOL_BOOLEAN:
	case LSP_SYMBOL_ARRAY:
	case LSP_SYMBOL_OBJECT:
	case LSP_SYMBOL_ENUMMEMBER:
	case LSP_SYMBOL_NULL:
		return SYMBOL_CONSTANT;
	}
	
	return SYMBOL_OTHER;
}

void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition position) {
	Definitions *defs = &ted->definitions;
	if (lsp) {
		// send that request
		LSPRequest request = {.type = LSP_REQUEST_DEFINITION};
		request.data.definition.position = position;
		LSPRequestID id = lsp_send_request(lsp, &request);
		defs->last_request_id = id;
		defs->last_request_time = ted->frame_time;
	} else {
		// just go to the tag
		tag_goto(ted, name);
	}
}

void definition_cancel_lookup(Ted *ted) {
	Definitions *defs = &ted->definitions;
	defs->last_request_id = 0;
}

void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response) {
	if (response->request.type != LSP_REQUEST_DEFINITION)
		return;
	
	const LSPResponseDefinition *response_def = &response->data.definition;
	Definitions *defs = &ted->definitions;
	
	if (response->request.id != defs->last_request_id) {
		// response to an old request
		return;
	}
	
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
	if (defs->last_request_id && timespec_sub(ted->frame_time, defs->last_request_time) > 0.2) {
		ted->cursor = ted->cursor_wait;
	}
}
