
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
		// @TODO : cancel old request
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


void definitions_frame(Ted *ted) {
	Definitions *defs = &ted->definitions;
	if (defs->last_request_id && timespec_sub(ted->frame_time, defs->last_request_time) > 0.2) {
		ted->cursor = ted->cursor_wait;
	}
}

static void definitions_clear_entries(Definitions *defs) {
	arr_foreach_ptr(defs->selector_all_entries, SelectorEntry, entry) {
		free((char*)entry->name);
	}
	arr_clear(defs->selector_all_entries);
	arr_clear(defs->selector.entries);
	defs->selector.n_entries = 0;
}


void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response) {
	Definitions *defs = &ted->definitions;
	if (response->request.id != defs->last_request_id) {
		// response to an old/irrelevant request
		return;
	}
	
	defs->last_request_id = 0;
	
	switch (response->request.type) {
	case LSP_REQUEST_DEFINITION: {
		const LSPResponseDefinition *response_def = &response->data.definition;
		
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
		} break;
	case LSP_REQUEST_WORKSPACE_SYMBOLS: {
		const LSPResponseWorkspaceSymbols *response_syms = &response->data.workspace_symbols;
		const LSPSymbolInformation *symbols = response_syms->symbols;
		const Settings *settings = ted_active_settings(ted);
		const u32 *colors = settings->colors;
		
		definitions_clear_entries(defs);
		arr_set_len(defs->selector_all_entries, arr_len(symbols));
		for (size_t i = 0; i < arr_len(symbols); ++i) {
			const LSPSymbolInformation *symbol = &symbols[i];
			SelectorEntry *entry = &defs->selector_all_entries[i];
			
			entry->name = str_dup(lsp_response_string(response, symbol->name));
			SymbolKind kind = symbol_kind_to_ted(symbol->kind);
			entry->color = colors[color_for_symbol_kind(kind)];
		}
		
		} break;
	default:
		debug_println("?? bad request type in %s", __func__);
		break;
	}
}

void definitions_selector_open(Ted *ted) {
	Definitions *defs = &ted->definitions;
	definitions_clear_entries(defs);
	LSP *lsp = buffer_lsp(ted->prev_active_buffer);
	if (lsp) {
		LSPRequest request = {.type = LSP_REQUEST_WORKSPACE_SYMBOLS};
		LSPRequestWorkspaceSymbols *syms = &request.data.workspace_symbols;
		syms->query = str_dup("");
		defs->last_request_id = lsp_send_request(lsp, &request);
		defs->last_request_time = ted->frame_time;
	} else {
		defs->selector_all_entries = tags_get_entries(ted);
	}
	ted_switch_to_buffer(ted, &ted->line_buffer);
	buffer_select_all(ted->active_buffer);
	defs->selector.cursor = 0;
}


void definitions_selector_close(Ted *ted) {
	Definitions *defs = &ted->definitions;
	definitions_clear_entries(defs);
	// @TODO : cancel
	defs->last_request_id = 0;
}

char *definitions_selector_update(Ted *ted) {
	Definitions *defs = &ted->definitions;
	Selector *sel = &defs->selector;
	sel->enable_cursor = true;
	
	// create selector entries based on search term
	char *search_term = str32_to_utf8_cstr(buffer_get_line(&ted->line_buffer, 0));

	arr_clear(sel->entries);

	arr_foreach_ptr(defs->selector_all_entries, SelectorEntry, entry) {
		if (!search_term || stristr(entry->name, search_term)) {
			arr_add(sel->entries, *entry);
		}
	}

	sel->n_entries = arr_len(sel->entries);

	return selector_update(ted, sel);
}

void definitions_selector_render(Ted *ted, Rect bounds) {
	Definitions *defs = &ted->definitions;
	Selector *sel = &defs->selector;
	sel->bounds = bounds;
	selector_render(ted, sel);
}
