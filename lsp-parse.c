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

static bool parse_completion(LSP *lsp, const JSON *json, LSPResponse *response) {
	// deal with textDocument/completion response.
	// result: CompletionItem[] | CompletionList | null
	LSPResponseCompletion *completion = &response->data.completion;
	
	JSONValue result = json_get(json, "result");
	JSONValue items_value = {0};
	switch (result.type) {
	case JSON_NULL:
		// no completions
		return true;
	case JSON_ARRAY:
		items_value = result;
		break;
	case JSON_OBJECT:
		items_value = json_object_get(json, result.val.object, "items");
		break;
	default:
		lsp_set_error(lsp, "Weird result type for textDocument/completion response: %s.", json_type_to_str(result.type));
		break;		
	}
		
	if (!lsp_expect_array(lsp, items_value, "completion list"))
		return false;
	
	JSONArray items = items_value.val.array;
	
	arr_set_len(completion->items, items.len);
	
	for (u32 i = 0; i < items.len; ++i) {
		LSPCompletionItem *item = &completion->items[i];
		
		JSONValue item_value = json_array_get(json, items, i);
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
		
		JSONValue sort_text_value = json_object_get(json, item_object, "sortText");
		if (sort_text_value.type == JSON_STRING) {
			// LSP allows using a different string for sorting.
			item->sort_text = lsp_response_add_json_string(response,
				json, sort_text_value.val.string);
		}
		
		JSONValue filter_text_value = json_object_get(json, item_object, "filterText");
		if (filter_text_value.type == JSON_STRING) {
			// LSP allows using a different string for filtering.
			item->filter_text = lsp_response_add_json_string(response,
				json, filter_text_value.val.string);
		}
		
		JSONValue text_type_value = json_object_get(json, item_object, "insertTextFormat");
		if (text_type_value.type == JSON_NUMBER) {
			double type = text_type_value.val.number;
			if (type != LSP_TEXT_EDIT_PLAIN && type != LSP_TEXT_EDIT_SNIPPET) {
				lsp_set_error(lsp, "Bad InsertTextFormat: %g", type);
				return false;
			}
			item->text_edit.type = (LSPTextEditType)type;
		}
		
		// @TODO: detail
		
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
		json_string_get(json, error.val.string, err, sizeof err);;
		
		if (streq(err, "waiting for cargo metadata or cargo check")) {
			// fine. be that way. i'll resend the goddamn request.
			// i'll keep bombarding you with requests.
			// maybe next time you should abide by the standard and only send an initialize response when youre actually ready to handle my requests. fuck you.
			if (response_to.type) {
				lsp_send_request(lsp, &response_to);
				// don't free
				memset(&response_to, 0, sizeof response_to);
			}
		} else {
			lsp_set_error(lsp, "%s", err);
		}
		goto ret;
	}
	
	JSONValue result = json_get(json, "result");
	if (result.type != JSON_UNDEFINED) {
		if (response_to.type == LSP_REQUEST_INITIALIZE) {
			// it's the response to our initialize request!
			// let's send back an "initialized" request (notification) because apparently
			// that's something we need to do.
			LSPRequest initialized = {
				.type = LSP_REQUEST_INITIALIZED,
				.data = {0},
			};
			write_request(lsp, &initialized);
			// we can now send requests which have nothing to do with initialization
			lsp->initialized = true;
		} else {
			LSPResponse response = {0};
			bool success = false;
			response.request = response_to;
			switch (response_to.type) {
			case LSP_REQUEST_COMPLETION:
				success = parse_completion(lsp, json, &response);
				break;
			default:
				// it's some response we don't care about
				break;
			}
			if (success) {
				SDL_LockMutex(lsp->messages_mutex);
				LSPMessage *message = arr_addp(lsp->messages);
				message->type = LSP_RESPONSE;
				message->u.response = response;
				SDL_UnlockMutex(lsp->messages_mutex);
				response_to.type = 0; // don't free
			} else {
				lsp_response_free(&response);
			}
		}
	} else if (json_has(json, "method")) {
		LSPRequest request = {0};
		if (parse_server2client_request(lsp, json, &request)) {
			SDL_LockMutex(lsp->messages_mutex);
			LSPMessage *message = arr_addp(lsp->messages);
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
