// this file deals with ctrl+click "go to definition", and
// the definitions menu (Ctrl+D)

#include "ted-internal.h"

struct Definitions {
	LSPServerRequestID last_request;
	double last_request_time;
	/// last query string which we sent a request for
	char *last_request_query;
	/// for "go to definition of..." menu
	Selector *selector;
	/// an array of all definitions (gotten from workspace/symbols) for "go to definition" menu
	SymbolInfo *all_definitions;
};

void definition_cancel_lookup(Ted *ted) {
	Definitions *defs = ted->definitions;
	ted_cancel_lsp_request(ted, &defs->last_request);
}

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

void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition position, GotoType type) {
	Definitions *defs = ted->definitions;
	if (lsp) {
		// cancel old request
		definition_cancel_lookup(ted);
		LSPRequestType request_type = LSP_REQUEST_DEFINITION;
		switch (type) {
		case GOTO_DEFINITION:
			request_type = LSP_REQUEST_DEFINITION;
			break;
		case GOTO_DECLARATION:
			request_type = LSP_REQUEST_DECLARATION;
			break;
		case GOTO_TYPE_DEFINITION:
			request_type = LSP_REQUEST_TYPE_DEFINITION;
			break;
		case GOTO_IMPLEMENTATION:
			request_type = LSP_REQUEST_IMPLEMENTATION;
			break;
		}
		// send that request
		LSPRequest request = {.type = request_type};
		request.data.definition.position = position;
		LSPServerRequestID id = lsp_send_request(lsp, &request);
		if (id.id == 0 && request.type == LSP_REQUEST_IMPLEMENTATION) {
			// if we can't go to the implementation, try going to the definition
			request.type = LSP_REQUEST_DEFINITION;
			id = lsp_send_request(lsp, &request);
		}
		defs->last_request = id;
		defs->last_request_time = ted->frame_time;
	} else {
		// just go to the tag
		tag_goto(ted, name);
	}
}

void definitions_frame(Ted *ted) {
	Definitions *defs = ted->definitions;
	if (defs->last_request.id && ted->frame_time - defs->last_request_time > 0.2) {
		ted->cursor = ted->cursor_wait;
	}
}

static void definitions_clear_entries(Definitions *defs) {
	arr_foreach_ptr(defs->all_definitions, SymbolInfo, def) {
		free(def->name);
		free(def->detail);
	}
	arr_clear(defs->all_definitions);
	selector_clear(defs->selector);
}

static int definition_entry_qsort_cmp(const void *av, const void *bv) {
	const SymbolInfo *a = av, *b = bv;
	// first, sort by length
	size_t a_len = strlen(a->name), b_len = strlen(b->name);
	if (a_len < b_len) return -1;
	if (a_len > b_len) return 1;
	// then sort alphabetically
	int cmp = strcmp(a->name, b->name);
	if (cmp) return cmp;
	// then sort by detail
	if (!a->detail && b->detail) return -1;
	if (a->detail && !b->detail) return 1;
	if (!a->detail && !b->detail) return 0;
	return strcmp(a->detail, b->detail);
}

void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response) {
	Definitions *defs = ted->definitions;
	if (response->request.id != defs->last_request.id) {
		// response to an old/irrelevant request
		return;
	}
	
	defs->last_request.id = 0;
	
	switch (response->request.type) {
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION: {
		// handle textDocument/definition or textDocument/declaration response
		const LSPResponseDefinition *response_def = &response->data.definition;
		
		if (!arr_len(response_def->locations)) {
			// no definition.
			const char* link = NULL;
			
			// check for document links
			TextBuffer *buffer = ted->active_buffer;
			if (buffer) {
				BufferPos pos = buffer_pos_from_lsp(buffer, response->request.data.definition.position.pos);
				link = document_link_at_buffer_pos(ted, pos);
				if (link) {
					open_with_default_application(link);
					return;
				}
			}
			
			// do the error cursor.
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
		// handle workspace/symbol response
		const LSPResponseWorkspaceSymbols *response_syms = &response->data.workspace_symbols;
		const LSPSymbolInformation *symbols = response_syms->symbols;
		const Settings *settings = ted_active_settings(ted);
		const u32 *colors = settings->colors;
		
		definitions_clear_entries(defs);
		arr_set_len(defs->all_definitions, arr_len(symbols));
		for (size_t i = 0; i < arr_len(symbols); ++i) {
			const LSPSymbolInformation *symbol = &symbols[i];
			SymbolInfo *def = &defs->all_definitions[i];
			
			def->name = str_dup(lsp_response_string(response, symbol->name));
			SymbolKind kind = symbol_kind_to_ted(symbol->kind);
			def->color = colors[color_for_symbol_kind(kind)];
			def->from_lsp = true;
			def->position = lsp_location_start_position(symbol->location);
			const char *container_name = lsp_response_string(response, symbol->container);
			const char *filename = path_filename(lsp_document_path(lsp, def->position.document));
			bool has_container = *container_name != 0;
			def->detail = a_sprintf("%s%s%s:%" PRIu32,
				container_name,
				has_container ? ", " : "",
				filename,
				def->position.pos.line + 1);
		}
		
		} break;
	default:
		debug_println("?? bad request type in %s : %u:%u", __func__,  response->request.id, response->request.type);
		break;
	}
}

void definitions_send_request_if_needed(Ted *ted) {
	LSP *lsp = buffer_lsp(ted->prev_active_buffer);
	if (!lsp)
		return;
	Definitions *defs = ted->definitions;
	char *query = buffer_contents_utf8_alloc(ted->line_buffer);
	if (defs->last_request_query && strcmp(defs->last_request_query, query) == 0) {
		free(query);
		return; // no need to update symbols
	}
	LSPRequest request = {.type = LSP_REQUEST_WORKSPACE_SYMBOLS};
	LSPRequestWorkspaceSymbols *syms = &request.data.workspace_symbols;
	syms->query = str_dup(query);
	// cancel old request
	definition_cancel_lookup(ted);
	defs->last_request = lsp_send_request(lsp, &request);
	defs->last_request_time = ted->frame_time;
	free(defs->last_request_query);
	defs->last_request_query = query;
}

static void definitions_selector_open(Ted *ted) {
	Definitions *defs = ted->definitions;
	definitions_clear_entries(defs);
	LSP *lsp = ted->prev_active_buffer
		? buffer_lsp(ted->prev_active_buffer)
		: ted_active_lsp(ted);
	
	if (lsp) {
		definitions_send_request_if_needed(ted);
	} else {
		defs->all_definitions = tags_get_symbols(ted);
	}
	ted_switch_to_buffer(ted, ted->line_buffer);
	buffer_select_all(ted->active_buffer);
	selector_set_cursor(defs->selector, 0);
}


static bool definitions_selector_close(Ted *ted) {
	Definitions *defs = ted->definitions;
	definitions_clear_entries(defs);
	ted_cancel_lsp_request(ted, &defs->last_request);
	free(defs->last_request_query);
	defs->last_request_query = NULL;
	return true;
}

static void definitions_selector_update(Ted *ted) {
	Definitions *defs = ted->definitions;
	Selector *sel = defs->selector;
	
	// send new request if search term has changed.
	// this is needed because e.g. clangd gives an incomplete list
	definitions_send_request_if_needed(ted);
	
	char *chosen = selector_update(ted, sel);
	if (chosen) {
		// for LSP go-to-definition, we ignore `chosen` and use the cursor instead.
		// this is because a single symbol can have multiple definitions,
		// e.g. with overloading.
		SelectorEntry cursor_entry = {0};
		if (!selector_get_cursor_entry(sel, &cursor_entry)) {
			assert(0); // shouldn't happen since `chosen` is true.
			return;
		}
		
		u64 def_idx = cursor_entry.userdata;
		if (def_idx >= arr_len(defs->all_definitions)) {
			assert(0);
			return;
		}
		SymbolInfo *info = &defs->all_definitions[def_idx];
		if (info->from_lsp) {
			// NOTE: we need to get this before calling menu_close,
			// since that clears selector_all_definitions
			LSPDocumentPosition position = info->position;
			
			menu_close(ted);
			ted_go_to_lsp_document_position(ted, NULL, position);
		} else {
			menu_close(ted);
			tag_goto(ted, chosen);
		}
		free(chosen);
	}
}

static void definitions_selector_render(Ted *ted) {
	Rect bounds = selection_menu_render_bg(ted);
	Definitions *defs = ted->definitions;
	Selector *sel = defs->selector;
	selector_set_bounds(sel, bounds);
	selector_render(ted, sel);
}

void definitions_init(Ted *ted) {
	MenuInfo info = {
		.open = definitions_selector_open,
		.close = definitions_selector_close,
		.render = definitions_selector_render,
		.update = definitions_selector_update,
	};
	strbuf_cpy(info.name, MENU_GOTO_DEFINITION);
	menu_register(ted, &info);
	
	Definitions *defs = ted->definitions = ted_calloc(ted, 1, sizeof *ted->definitions);
	defs->selector = selector_new();
}

void definitions_quit(Ted *ted) {
	Definitions *defs = ted->definitions;
	selector_free(defs->selector);
	free(defs);
	ted->definitions = NULL;
}
