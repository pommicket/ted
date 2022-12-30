static WarnUnusedResult bool lsp_expect_type(LSP *lsp, JSONValue value, JSONValueType type, const char *what) {
	if (value.type != type) {
		lsp_set_error(lsp, "Expected %s for %s, got %s",
			json_type_to_str(type),
			what,
			json_type_to_str(value.type));
		return false;
	}
	return true;
}

static WarnUnusedResult bool lsp_expect_object(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_OBJECT, what);
}

static WarnUnusedResult bool lsp_expect_array(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_ARRAY, what);
}

static WarnUnusedResult bool lsp_expect_string(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_STRING, what);
}

static WarnUnusedResult bool lsp_expect_number(LSP *lsp, JSONValue value, const char *what) {
	return lsp_expect_type(lsp, value, JSON_NUMBER, what);
}


static LSPString lsp_response_add_json_string(LSPResponse *response, const JSON *json, JSONString string) {
	u32 offset = arr_len(response->string_data);
	arr_set_len(response->string_data, offset + string.len + 1);
	json_string_get(json, string, response->string_data + offset, string.len + 1);
	return (LSPString){
		.offset = offset
	};
}

static int completion_qsort_cmp(void *context, const void *av, const void *bv) {
	const LSPResponse *response = context;
	const LSPCompletionItem *a = av, *b = bv;
	const char *a_sort_text = lsp_response_string(response, a->sort_text);
	const char *b_sort_text = lsp_response_string(response, b->sort_text);
	int sort_text_cmp = strcmp(a_sort_text, b_sort_text);
	if (sort_text_cmp != 0)
		return sort_text_cmp;
	// for some reason, rust-analyzer outputs identical sortTexts
	// i have no clue what that means.
	// the LSP "specification" is not very specific.
	// we'll sort by label in this case.
	// this is what VSCode seems to do.
	// i hate microsofot.
	const char *a_label = lsp_response_string(response, a->label);
	const char *b_label = lsp_response_string(response, b->label);
	return strcmp(a_label, b_label);
}

static bool parse_position(LSP *lsp, const JSON *json, JSONValue pos_value, LSPPosition *pos) {
	if (!lsp_expect_object(lsp, pos_value, "document position"))
		return false;
	JSONObject pos_object = pos_value.val.object;
	JSONValue line = json_object_get(json, pos_object, "line");
	JSONValue character = json_object_get(json, pos_object, "character");
	if (!lsp_expect_number(lsp, line, "document line number")
		|| !lsp_expect_number(lsp, character, "document column number"))
		return false;
	pos->line = (u32)line.val.number;
	pos->character = (u32)character.val.number;
	return true;
}

static bool parse_range(LSP *lsp, const JSON *json, JSONValue range_value, LSPRange *range) {
	if (!lsp_expect_object(lsp, range_value, "document range"))
		return false;
	JSONObject range_object = range_value.val.object;
	JSONValue start = json_object_get(json, range_object, "start");
	JSONValue end = json_object_get(json, range_object, "end");
	bool success = parse_position(lsp, json, start, &range->start)
		&& parse_position(lsp, json, end, &range->end);
	if (!success)
		memset(range, 0, sizeof *range);
	return success;
}

static bool parse_document_uri(LSP *lsp, const JSON *json, JSONValue value, LSPDocumentID *id) {
	if (!lsp_expect_string(lsp, value, "URI"))
		return false;
	char *string = json_string_get_alloc(json, value.val.string);
	if (!str_has_prefix(string, "file://")) {
		lsp_set_error(lsp, "Can't process non-local URI %s",
			string);
		free(string);
		return false;
	}
	*id = lsp_document_id(lsp, string + strlen("file://"));
	free(string);
	return true;
}


static uint32_t *parse_trigger_characters(const JSON *json, JSONArray trigger_chars) {
	uint32_t *array = NULL;
	arr_reserve(array, trigger_chars.len);
	for (u32 i = 0; i < trigger_chars.len; ++i) {
		char character[8] = {0};
		json_string_get(json,
			json_array_get_string(json, trigger_chars, i),
			character,
			sizeof character);
		if (*character) {
			// the fact that they're called "trigger characters" makes
			// me think multi-character triggers aren't allowed
			// even though that would be nice in some languages,
			// e.g. "::"
			char32_t c = 0;
			unicode_utf8_to_utf32(&c, character, strlen(character));
			if (c) arr_add(array, c);
		}
	}
	return array;
}

static void parse_capabilities(LSP *lsp, const JSON *json, JSONObject capabilities) {
	LSPCapabilities *cap = &lsp->capabilities;
	
	// check CompletionOptions
	JSONValue completion_value = json_object_get(json, capabilities, "completionProvider");
	if (completion_value.type == JSON_OBJECT) {
		cap->completion_support = true;
		JSONObject completion = completion_value.val.object;
		
		JSONArray trigger_chars = json_object_get_array(json, completion, "triggerCharacters");
		lsp->completion_trigger_chars = parse_trigger_characters(json, trigger_chars);
	}
	
	// check SignatureHelpOptions
	JSONValue signature_help_value = json_object_get(json, capabilities, "signatureHelpProvider");
	if (signature_help_value.type == JSON_OBJECT) {
		cap->signature_help_support = true;
		JSONObject signature_help = signature_help_value.val.object;
		JSONArray trigger_chars = json_object_get_array(json, signature_help, "triggerCharacters");
		lsp->signature_help_trigger_chars = parse_trigger_characters(json, trigger_chars);
		JSONArray retrigger_chars = json_object_get_array(json, signature_help, "retriggerCharacters");
		lsp->signature_help_retrigger_chars = parse_trigger_characters(json, retrigger_chars);
		// rust-analyzer doesn't have ) or > as a retrigger char which is really weird
		arr_add(lsp->signature_help_retrigger_chars, ')');
		arr_add(lsp->signature_help_retrigger_chars, '>');
	}
	
	// check for hover support
	JSONValue hover_value = json_object_get(json, capabilities, "hoverProvider");
	if (hover_value.type != JSON_UNDEFINED) {
		cap->hover_support = true;
	}
	
	// check for definition support
	JSONValue definition_value = json_object_get(json, capabilities, "definitionProvider");
	if (definition_value.type != JSON_UNDEFINED) {
		cap->definition_support = true;
	}
	
	// check for workspace/symbol support
	JSONValue workspace_symbol_value = json_object_get(json, capabilities, "workspaceSymbolProvider");
	if (workspace_symbol_value.type != JSON_UNDEFINED) {
		cap->workspace_symbols_support = true;
	}
	
	JSONObject workspace = json_object_get_object(json, capabilities, "workspace");
	// check WorkspaceFoldersServerCapabilities
	JSONObject workspace_folders = json_object_get_object(json, workspace, "workspaceFolders");
	if (json_object_get_bool(json, workspace_folders, "supported", false)) {
		cap->workspace_folders_support = true;
	}
}

static JSONString get_markup_content(const JSON *json, JSONValue markup_value) {
	// some fields are of type string | MarkupContent (e.g. completion documentation)
	// this converts either one to a string.
	if (markup_value.type == JSON_STRING) {
		return markup_value.val.string;
	} else if (markup_value.type == JSON_OBJECT) {
		return json_object_get_string(json, markup_value.val.object, "value");
	} else {
		return (JSONString){0};
	}
}

static bool parse_completion(LSP *lsp, const JSON *json, LSPResponse *response) {
	// deal with textDocument/completion response.
	// result: CompletionItem[] | CompletionList | null
	LSPResponseCompletion *completion = &response->data.completion;
	
	JSONValue result = json_get(json, "result");
	JSONValue items_value = {0};
	completion->is_complete = true; // default
	
	switch (result.type) {
	case JSON_NULL:
		// no completions
		return true;
	case JSON_ARRAY:
		items_value = result;
		break;
	case JSON_OBJECT:
		items_value = json_object_get(json, result.val.object, "items");
		completion->is_complete = !json_object_get_bool(json, result.val.object, "isIncomplete", false);
		break;
	default:
		lsp_set_error(lsp, "Weird result type for textDocument/completion response: %s.", json_type_to_str(result.type));
		break;		
	}
		
	if (!lsp_expect_array(lsp, items_value, "completion list"))
		return false;
	
	JSONArray items = items_value.val.array;
	
	arr_set_len(completion->items, items.len);
	
	for (u32 item_idx = 0; item_idx < items.len; ++item_idx) {
		LSPCompletionItem *item = &completion->items[item_idx];
		
		JSONValue item_value = json_array_get(json, items, item_idx);
		if (!lsp_expect_object(lsp, item_value, "completion list"))
			return false;
		JSONObject item_object = item_value.val.object;
		
		JSONValue label_value = json_object_get(json, item_object, "label");
		if (!lsp_expect_string(lsp, label_value, "completion label"))
			return false;
		JSONString label = label_value.val.string;
		item->label = lsp_response_add_json_string(response, json, label);
		
		// defaults
		item->sort_text = item->label;
		item->filter_text = item->label;
		item->text_edit = (LSPTextEdit) {
			.type = LSP_TEXT_EDIT_PLAIN,
			.at_cursor = true,
			.range = {{0}, {0}},
			.new_text = item->label
		};
		
		double kind = json_object_get_number(json, item_object, "kind");
		if (isfinite(kind) && kind >= LSP_COMPLETION_KIND_MIN && kind <= LSP_COMPLETION_KIND_MAX) {
			item->kind = (LSPCompletionKind)kind;
		}
		
		JSONString sort_text = json_object_get_string(json, item_object, "sortText");
		if (sort_text.pos) {
			// LSP allows using a different string for sorting.
			item->sort_text = lsp_response_add_json_string(response, json, sort_text);
		}
		
		JSONValue deprecated = json_object_get(json, item_object, "deprecated");
		if (deprecated.type == JSON_TRUE) {
			item->deprecated = true;
		}
		
		JSONArray tags = json_object_get_array(json, item_object, "tags");
		for (u32 i = 0; i < tags.len; ++i) {
			double tag = json_array_get_number(json, tags, i);
			if (tag == LSP_SYMBOL_TAG_DEPRECATED) {
				item->deprecated = true;
			}
		}
		
		JSONString filter_text = json_object_get_string(json, item_object, "filterText");
		if (filter_text.pos) {
			// LSP allows using a different string for filtering.
			item->filter_text = lsp_response_add_json_string(response, json, filter_text);
		}
		
		double edit_type = json_object_get_number(json, item_object, "insertTextFormat");
		if (!isnan(edit_type)) {
			if (edit_type != LSP_TEXT_EDIT_PLAIN && edit_type != LSP_TEXT_EDIT_SNIPPET) {
				// maybe in the future more edit types will be added.
				// probably they'll have associated capabilities, but I think it's best to just ignore unrecognized types
				debug_println("Bad InsertTextFormat: %g", edit_type);
				edit_type = LSP_TEXT_EDIT_PLAIN;
			}
			item->text_edit.type = (LSPTextEditType)edit_type;
		}
		
		JSONValue documentation_value = json_object_get(json, item_object, "documentation");
		JSONString documentation = get_markup_content(json, documentation_value);
		if (documentation.len) {
			if (documentation.len > 1000) {
				// rust has some docs which are *20,000* bytes long
				// that's more than i'm ever gonna show on-screen!
				documentation.len = 1000;
				// okay this could break mid-code-point but whatever it would probably
				// just display ⌷.
			}
			item->documentation = lsp_response_add_json_string(response, json, documentation);
		}
		
		
		JSONString detail_text = json_object_get_string(json, item_object, "detail");
		if (detail_text.pos) {
			item->detail = lsp_response_add_json_string(response, json, detail_text);
		}
		
		// @TODO(eventually): additionalTextEdits
		//  (try to find a case where this comes up)
		
		// what should happen when this completion is selected?
		JSONValue text_edit_value = json_object_get(json, item_object, "textEdit");
		if (text_edit_value.type == JSON_OBJECT) {
			JSONObject text_edit = text_edit_value.val.object;
			item->text_edit.at_cursor = false;
			
			JSONValue range = json_object_get(json, text_edit, "range");
			if (!parse_range(lsp, json, range, &item->text_edit.range))
				return false;
				
			JSONValue new_text_value = json_object_get(json, text_edit, "newText");
			if (!lsp_expect_string(lsp, new_text_value, "completion newText"))
				return false;
			item->text_edit.new_text = lsp_response_add_json_string(response,
				json, new_text_value.val.string);
		} else {
			// not using textEdit. check insertText.
			JSONValue insert_text_value = json_object_get(json, item_object, "insertText");
			if (insert_text_value.type == JSON_STRING) {
				// string which will be inserted if this completion is selected
				item->text_edit.new_text = lsp_response_add_json_string(response,
					json, insert_text_value.val.string);
			}
		}
		
	}
	
	qsort_with_context(completion->items, items.len, sizeof *completion->items,
		completion_qsort_cmp, response);
	
	return true;
}

static bool parse_signature_help(LSP *lsp, const JSON *json, LSPResponse *response) {
	JSONObject result = json_force_object(json_get(json, "result"));
	//json_debug_print_object(json, result);printf("\n");
	LSPResponseSignatureHelp *help = &response->data.signature_help;
	
	u32 active_signature = 0;
	double active_signature_dbl = json_object_get_number(json, result, "activeSignature");
	if (isfinite(active_signature_dbl))
		active_signature = (u32)active_signature_dbl;
	double global_active_parameter = json_object_get_number(json, result, "activeParameter");
	
	JSONArray signatures = json_object_get_array(json, result, "signatures");
	if (active_signature >= signatures.len)
		active_signature = 0;
	for (u32 s = 0; s < signatures.len; ++s) {
		// parse SignatureInformation
		LSPSignatureInformation *signature_out = arr_addp(help->signatures);
		JSONObject signature_in = json_array_get_object(json, signatures, s);
		JSONString label = json_object_get_string(json, signature_in, "label");
		signature_out->label = lsp_response_add_json_string(response, json, label);
		size_t label_len_utf16 = unicode_utf16_len(lsp_response_string(response, signature_out->label));
		if (label_len_utf16 == (size_t)-1) {
			lsp_set_error(lsp, "Bad UTF-8 in SignatureInformation.label");
			return false;
		}
		
		#if 0
		// currently we don't show signature documentation
		JSONString documentation = get_markup_content(json,
			json_object_get(json, signature_in, "documentation"));
		if (documentation.len)
			signature_out->documentation = lsp_response_add_json_string(response, json, documentation);
		#endif
		
		JSONArray parameters = json_object_get_array(json, signature_in, "parameters");
		u32 active_parameter = U32_MAX;
		double active_parameter_dbl = json_object_get_number(json, signature_in, "activeParameter");
		if (isfinite(active_parameter_dbl)) {
			active_parameter = (u32)active_parameter_dbl;
		}
		if (active_parameter == U32_MAX &&
			isfinite(global_active_parameter)) {
			active_parameter = (u32)global_active_parameter;
		}
		if (active_parameter < parameters.len) {
			JSONObject parameter_info = json_array_get_object(json, parameters, active_parameter);
			JSONValue parameter_label_value = json_object_get(json, parameter_info, "label");
			u16 start = 0, end = 0;
			// parse the parameter label
			if (parameter_label_value.type == JSON_ARRAY) {
				// parameter label is specified as UTF-16 character range
				JSONArray parameter_label = parameter_label_value.val.array;
				double start_dbl = json_array_get_number(json, parameter_label, 0);
				double end_dbl = json_array_get_number(json, parameter_label, 1);
				if (!(isfinite(start_dbl) && isfinite(end_dbl))) {
					lsp_set_error(lsp, "Bad contents of ParameterInfo.label array.");
					return false;
				}
				start = (u16)start_dbl;
				end = (u16)end_dbl;
			} else if (parameter_label_value.type == JSON_STRING) {
				// parameter label is specified as substring
				JSONString parameter_label = parameter_label_value.val.string;
				char *sig_lbl = json_string_get_alloc(json, label);
				char *param_lbl = json_string_get_alloc(json, parameter_label);
				const char *pos = strstr(sig_lbl, param_lbl);
				if (pos) {
					start = (u16)(pos - sig_lbl);
					end = (u16)(start + strlen(param_lbl));
				}
				free(sig_lbl);
				free(param_lbl);
			} else {
				lsp_set_error(lsp, "Bad type for ParameterInfo.label");
				return false;
			}
			
			
			if (start > end || end > label_len_utf16) {
				lsp_set_error(lsp, "Bad range for ParameterInfo.label: %u-%u within signature label of length %u", start, end, label.len);
				return false;
			}
			
			signature_out->active_start = start;
			signature_out->active_end = end;
		}
	}
	
	if (active_signature != 0) {
		//make sure active signature is #0
		LSPSignatureInformation active = help->signatures[active_signature];
		arr_remove(help->signatures, active_signature);
		arr_insert(help->signatures, 0, active);
	}
	
	return true;
}

static bool parse_hover(LSP *lsp, const JSON *json, LSPResponse *response) {
	LSPResponseHover *hover = &response->data.hover;
	JSONValue result_value = json_get(json, "result");
	if (result_value.type == JSON_NULL)
		return true; // no results
	if (result_value.type != JSON_OBJECT) {
		lsp_set_error(lsp, "Bad result type for textDocument/hover response.");
		return false;
	}
	JSONObject result = json_force_object(result_value);
	
	JSONValue range = json_object_get(json, result, "range");
	parse_range(lsp, json, range, &hover->range);
	
	JSONValue contents = json_object_get(json, result, "contents");
	
	switch (contents.type) {
	case JSON_OBJECT:
	case JSON_STRING:
		// all good
		break;
	case JSON_ARRAY: {
		JSONArray contents_array = contents.val.array;
		if (contents_array.len == 0) {
			// the server probably should have just returned result: null.
			// but the spec doesn't seem to forbid this, so we'll handle it.
			return true;
		}
		// it's giving us multiple strings, but we'll just show the first one
		contents = json_array_get(json, contents_array, 0);
	} break;
	default:
		lsp_set_error(lsp, "Bad contents field on textDocument/hover response.");
		return false;
	}
	
	// contents should either be a MarkupContent or a MarkedString
	// i.e. it is either a string or has a member `value: string`
	JSONString contents_string = {0};
	if (contents.type == JSON_STRING) {
		contents_string = contents.val.string;
	} else {
		JSONObject contents_object = json_force_object(contents);
		JSONValue value = json_object_get(json, contents_object, "value");
		if (value.type == JSON_STRING) {
			contents_string = value.val.string;
		} else {
			lsp_set_error(lsp, "Bad contents object in textDocument/hover response.");
			return false;
		}
	}
	
	hover->contents = lsp_response_add_json_string(response, json, contents_string);
	return true;
}

// parse a Location:  {uri: DocumentUri, range: Range}
static bool parse_location(LSP *lsp, const JSON *json, JSONValue value, LSPLocation *location) {
	if (!lsp_expect_object(lsp, value, "Location"))
		return false;
	JSONObject object = value.val.object;
	JSONValue uri = json_object_get(json, object, "uri");
	if (!parse_document_uri(lsp, json, uri, &location->document))
		return false;
	JSONValue range = json_object_get(json, object, "range");
	if (!parse_range(lsp, json, range, &location->range))
		return false;
	return true;
}

static bool parse_definition(LSP *lsp, const JSON *json, LSPResponse *response) {
	JSONValue result = json_get(json, "result");
	if (result.type == JSON_NULL) {
		// no location
		return true;
	}
	LSPResponseDefinition *definition = &response->data.definition;
	if (result.type == JSON_ARRAY) {
		JSONArray locations = result.val.array;
		if (locations.len == 0)
			return true;
		for (u32 l = 0; l < locations.len; ++l) {
			JSONValue location_json = json_array_get(json, locations, l);
			LSPLocation *location = arr_addp(definition->locations);
			if (!parse_location(lsp, json, location_json, location))
				return false;
		}
		return true;
	} else {
		LSPLocation *location = arr_addp(definition->locations);
		return parse_location(lsp, json, result, location);
	}
}

// parses SymbolInformation or WorkspaceSymbol
static bool parse_symbol_information(LSP *lsp, const JSON *json, JSONValue value,
	LSPResponse *response, LSPSymbolInformation *info) {
	if (!lsp_expect_object(lsp, value, "SymbolInformation"))
		return false;
	JSONObject object = value.val.object;
	
	// parse name
	JSONValue name_value = json_object_get(json, object, "name");
	if (!lsp_expect_string(lsp, name_value, "SymbolInformation.name"))
		return false;
	JSONString name = name_value.val.string;
	info->name = lsp_response_add_json_string(response, json, name);
	
	// parse kind
	JSONValue kind_value = json_object_get(json, object, "kind");
	if (!lsp_expect_number(lsp, kind_value, "SymbolInformation.kind"))
		return false;
	double kind = kind_value.val.number;
	if (isfinite(kind) && kind >= LSP_SYMBOL_KIND_MIN && kind <= LSP_SYMBOL_KIND_MAX)
		info->kind = (LSPSymbolKind)kind;
	
	// check if deprecated
	bool deprecated = json_object_get(json, object, "deprecated").type == JSON_TRUE;
	JSONArray tags = json_object_get_array(json, object, "tags");
	for (size_t i = 0; i < tags.len; ++i) {
		if (json_array_get_number(json, tags, i) == LSP_SYMBOL_TAG_DEPRECATED)
			deprecated = true;
	}
	info->deprecated = deprecated;
	
	// parse location
	JSONValue location = json_object_get(json, object, "location");
	if (!parse_location(lsp, json, location, &info->location))
		return false;
	
	return true;
}

static bool parse_workspace_symbols(LSP *lsp, const JSON *json, LSPResponse *response) {
	LSPResponseWorkspaceSymbols *syms = &response->data.workspace_symbols;
	JSONArray result = json_force_array(json_root(json));
	arr_set_len(syms->symbols, result.len);
	for (size_t i = 0; i < result.len; ++i) {
		LSPSymbolInformation *info = &syms->symbols[i];
		JSONValue value = json_array_get(json, result, i);
		if (!parse_symbol_information(lsp, json, value, response, info))
			return false;
	}
	return true;
}

// fills request->id/id_string appropriately given the request's json
// returns true on success
static WarnUnusedResult bool parse_id(const JSON *json, LSPRequest *request) {
	JSONValue id_value = json_get(json, "id");
	switch (id_value.type) {
	case JSON_NUMBER: {
		double id = id_value.val.number;
		if (id == (u32)id) {
			request->id = (u32)id;
			return true;
		}
		} break;
	case JSON_STRING:
		request->id_string = json_string_get_alloc(json, id_value.val.string);
		return true;
	default: break;
	}
	return false;
}

// handles window/showMessage, window/logMessage, and window/showMessageRequest parameters
static bool parse_window_message(LSP *lsp, const JSON *json, LSPRequestMessage *m) {
	JSONObject params = json_force_object(json_get(json, "params"));
	JSONValue type = json_object_get(json, params, "type");
	JSONValue message = json_object_get(json, params, "message");
	if (!lsp_expect_number(lsp, type, "MessageType"))
		return false;
	if (!lsp_expect_string(lsp, message, "message string"))
		return false;
	
	int mtype = (int)type.val.number;
	if (mtype < 1 || mtype > 4) {
		lsp_set_error(lsp, "Bad MessageType: %g", type.val.number);
		return false;
	}
	
	m->type = (LSPWindowMessageType)mtype;
	m->message = json_string_get_alloc(json, message.val.string);
	return true;
}

// returns true if `request` was actually filled with a request.
static bool parse_server2client_request(LSP *lsp, JSON *json, LSPRequest *request) {
	JSONValue method_value = json_get(json, "method");
	if (!lsp_expect_string(lsp, method_value, "request method"))
		return false;
	
	char method[64] = {0};
	json_string_get(json, method_value.val.string, method, sizeof method);
	
	if (streq(method, "window/showMessage")) {
		request->type = LSP_REQUEST_SHOW_MESSAGE;
		return parse_window_message(lsp, json, &request->data.message);
	} else if (streq(method, "window/showMessageRequest")) {
		// we'll deal with the repsonse right here
		LSPResponse response = {0};
		LSPRequest *r = &response.request;
		r->type = LSP_REQUEST_SHOW_MESSAGE;
		if (!parse_id(json, request)) {
			debug_println("Bad ID in window/showMessageRequest request. This shouldn't happen.");
			return false;
		}
		lsp_send_response(lsp, &response);
		
		request->type = LSP_REQUEST_SHOW_MESSAGE;
		return parse_window_message(lsp, json, &request->data.message);
	} else if (streq(method, "window/logMessage")) {
		request->type = LSP_REQUEST_LOG_MESSAGE;
		return parse_window_message(lsp, json, &request->data.message);
	} else if (streq(method, "workspace/workspaceFolders")) {
		// we can deal with this request right here
		LSPResponse response = {0};
		request = &response.request;
		request->type = LSP_REQUEST_WORKSPACE_FOLDERS;
		if (!parse_id(json, request)) {
			debug_println("Bad ID in workspace/workspaceFolders request. This shouldn't happen.");
			return false;
		}
		lsp_send_response(lsp, &response);
		return false;
	} else if (str_has_prefix(method, "$/") || str_has_prefix(method, "telemetry/")) {
		// we can safely ignore this
	} else if (streq(method, "textDocument/publishDiagnostics")) {
		// currently ignored
	} else {
		debug_println("Unrecognized request method: %s", method);
	}
	return false;
}



static void process_message(LSP *lsp, JSON *json) {
		
	#if 0
	printf("\x1b[3m");
	json_debug_print(json);
	printf("\x1b[0m\n");
	#endif
	JSONValue id_value = json_get(json, "id");
	
	// get the request associated with this (if any)
	LSPRequest response_to = {0};
	if (id_value.type == JSON_NUMBER) {
		u64 id = (u64)id_value.val.number;
		arr_foreach_ptr(lsp->requests_sent, LSPRequest, req) {
			if (req->id == id) {
				response_to = *req;
				arr_remove(lsp->requests_sent, (u32)(req - lsp->requests_sent));
				break;
			}
		}
	}
	
	JSONValue error = json_get(json, "error.message");
	JSONValue result = json_get(json, "result");
	if (result.type != JSON_UNDEFINED || error.type == JSON_STRING) {
		// server-to-client response
		LSPResponse response = {0};
		bool add_to_messages = false;
		response.request = response_to;
		// make sure (LSPString){0} gets treated as an empty string
		arr_add(response.string_data, '\0');
		
		if (error.type == JSON_STRING) {
			response.error = json_string_get_alloc(json, error.val.string);
		}
		
		if (response.error) {
			add_to_messages = true;
		} else switch (response_to.type) {
		case LSP_REQUEST_COMPLETION:
			add_to_messages = parse_completion(lsp, json, &response);
			break;
		case LSP_REQUEST_SIGNATURE_HELP:
			add_to_messages = parse_signature_help(lsp, json, &response);
			break;
		case LSP_REQUEST_HOVER:
			add_to_messages = parse_hover(lsp, json, &response);
			break;
		case LSP_REQUEST_DEFINITION:
			add_to_messages = parse_definition(lsp, json, &response);
			break;
		case LSP_REQUEST_WORKSPACE_SYMBOLS:
			add_to_messages = parse_workspace_symbols(lsp, json, &response);
			break;
		case LSP_REQUEST_INITIALIZE: {
			// it's the response to our initialize request!
			if (result.type == JSON_OBJECT) {
				// read server capabilities
				JSONObject capabilities = json_object_get_object(json, result.val.object, "capabilities");
				parse_capabilities(lsp, json, capabilities);
			}
			
			LSPRequest initialized = {
				.type = LSP_REQUEST_INITIALIZED,
				.data = {{0}},
			};
			write_request(lsp, &initialized);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
			{
				// configure jdtls so it actually supports signature help
				// NOTE: this won't send if the LSP isn't jdtls (because of lsp_supports_request)
				LSPRequest jdtls_configuration = {
					.type = LSP_REQUEST_JDTLS_CONFIGURATION
				};
				lsp_send_request(lsp, &jdtls_configuration);
			}
			} break;
		default:
			// it's some response we don't care about
			break;
		}
		
		if (add_to_messages) {
			SDL_LockMutex(lsp->messages_mutex);
			LSPMessage *message = arr_addp(lsp->messages_server2client);
			message->type = LSP_RESPONSE;
			message->u.response = response;
			SDL_UnlockMutex(lsp->messages_mutex);
			memset(&response_to, 0, sizeof response_to); // don't free
		} else {
			lsp_response_free(&response);
		}
	} else if (json_has(json, "method")) {
		// server-to-client request
		LSPRequest request = {0};
		if (parse_server2client_request(lsp, json, &request)) {
			SDL_LockMutex(lsp->messages_mutex);
			LSPMessage *message = arr_addp(lsp->messages_server2client);
			message->type = LSP_REQUEST;
			message->u.request = request;
			SDL_UnlockMutex(lsp->messages_mutex);
		} else {
			lsp_request_free(&request);
		}
	} else {
		lsp_set_error(lsp, "Bad message from server (no result, no method).");
	}
	lsp_request_free(&response_to);
	json_free(json);
	
}
