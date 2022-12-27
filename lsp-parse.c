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
	pos->character = (u32)line.val.number;
	return true;
}

static bool parse_range(LSP *lsp, const JSON *json, JSONValue range_value, LSPRange *range) {
	if (!lsp_expect_object(lsp, range_value, "document range"))
		return false;
	JSONObject range_object = range_value.val.object;
	JSONValue start = json_object_get(json, range_object, "start");
	JSONValue end = json_object_get(json, range_object, "end");
	return parse_position(lsp, json, start, &range->start)
		&& parse_position(lsp, json, end, &range->end);
}


static void parse_capabilities(LSP *lsp, const JSON *json, JSONObject capabilities) {
	JSONValue completion_value = json_object_get(json, capabilities, "completionProvider");
	if (completion_value.type == JSON_OBJECT) {
		lsp->provides_completion = true;
		JSONObject completion = completion_value.val.object;
		
		JSONArray trigger_chars = json_object_get_array(json, completion, "triggerCharacters");
		for (u32 i = 0; i < trigger_chars.len; ++i) {
			char character[8] = {0};
			json_string_get(json,
				json_array_get_string(json, trigger_chars, i),
				character,
				sizeof character);
			if (*character) {
				char32_t c = 0;
				unicode_utf8_to_utf32(&c, character, strlen(character));
				// the fact that they're called "trigger characters" makes
				// me think multi-character triggers aren't allowed
				// even though that would be nice in some languages,
				// e.g. "::"
				if (c) {
					arr_add(lsp->trigger_chars, c);
				}
			}
		}
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
			.range = {0},
			.new_text = item->label
		};
		
		double kind = json_object_get_number(json, item_object, "kind");
		if (isnormal(kind) && kind >= LSP_COMPLETION_KIND_MIN && kind <= LSP_COMPLETION_KIND_MAX) {
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
			if (tag == 1 /* deprecated */) {
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
		
		JSONString documentation = {0};
		JSONValue documentation_value = json_object_get(json, item_object, "documentation");
		// the "documentation" field is either just a string or an object containing
		// a type ("markdown" or "plaintext") and a string.
		if (documentation_value.type == JSON_STRING) {
			documentation = documentation_value.val.string;
		} else if (documentation_value.type == JSON_OBJECT) {
			documentation = json_object_get_string(json, documentation_value.val.object,
				"value");
		}
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

// fills request->id/id_string appropriately given the request's json
// returns true on success
static WarnUnusedResult bool parse_id(JSON *json, LSPRequest *request) {
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

// returns true if `request` was actually filled with a request.
static bool parse_server2client_request(LSP *lsp, JSON *json, LSPRequest *request) {
	JSONValue method_value = json_get(json, "method");
	if (!lsp_expect_string(lsp, method_value, "request method"))
		return false;
	
	char method[64] = {0};
	json_string_get(json, method_value.val.string, method, sizeof method);
	
	if (streq(method, "window/showMessage")) {
		request->type = LSP_REQUEST_SHOW_MESSAGE;
		goto window_message;
	} else if (streq(method, "window/logMessage")) {
		request->type = LSP_REQUEST_LOG_MESSAGE;
		window_message:;
		JSONValue type = json_get(json, "params.type");
		JSONValue message = json_get(json, "params.message");
		if (!lsp_expect_number(lsp, type, "MessageType"))
			return false;
		if (!lsp_expect_string(lsp, message, "message string"))
			return false;
		
		int mtype = (int)type.val.number;
		if (mtype < 1 || mtype > 4) {
			lsp_set_error(lsp, "Bad MessageType: %g", type.val.number);
			return false;
		}
		
		LSPRequestMessage *m = &request->data.message;
		m->type = (LSPWindowMessageType)mtype;
		m->message = json_string_get_alloc(json, message.val.string);
		return true;
	} else if (streq(method, "workspace/workspaceFolders")) {
		// we can deal with this request right here
		LSPResponse response = {0};
		request = &response.request;
		request->type = LSP_REQUEST_WORKSPACE_FOLDERS;
		if (!parse_id(json, request)) {
			// we can't even send an error response since we have no ID.
			debug_println("Bad ID in workspace/workspaceFolders request. This shouldn't happen.");
			return false;
		}
		lsp_send_response(lsp, &response);
		return false;
	} else if (str_has_prefix(method, "$/")) {
		// we can safely ignore this
	} else {
		lsp_set_error(lsp, "Unrecognized request method: %s", method);
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
	if (error.type == JSON_STRING) {
		char err[256] = {0};
		json_string_get(json, error.val.string, err, sizeof err);
		printf("%s\n",err);
		goto ret;
	}
	
	JSONValue result = json_get(json, "result");
	if (result.type != JSON_UNDEFINED) {
		// server-to-client response
		LSPResponse response = {0};
		bool add_to_messages = false;
		response.request = response_to;
		// make sure (LSPString){0} gets treated as an empty string
		arr_add(response.string_data, '\0');
		
		switch (response_to.type) {
		case LSP_REQUEST_COMPLETION:
			add_to_messages = parse_completion(lsp, json, &response);
			break;
		case LSP_REQUEST_INITIALIZE: {
			// it's the response to our initialize request!
			
			if (result.type == JSON_OBJECT) {
				// read server capabilities
				JSONObject capabilities = json_object_get_object(json, result.val.object, "capabilities");
				parse_capabilities(lsp, json, capabilities);
			}
			
			// let's send back an "initialized" request (notification) because apparently
			// that's something we need to do.
			LSPRequest initialized = {
				.type = LSP_REQUEST_INITIALIZED,
				.data = {0},
			};
			write_request(lsp, &initialized);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
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
			response_to.type = 0; // don't free
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
		}
	} else {
		lsp_set_error(lsp, "Bad message from server (no result, no method).");
	}
	ret:
	lsp_request_free(&response_to);
	json_free(json);
	
}
