static const char *lsp_language_id(Language lang) {
	switch (lang) {
	case LANG_CONFIG:
	case LANG_TED_CFG:
	case LANG_NONE:
		return "text";
	case LANG_C:
		return "c";
	case LANG_CPP:
		return "cpp";
	case LANG_JAVA:
		return "java";
	case LANG_JAVASCRIPT:
		return "javascript";
	case LANG_JSON:
		return "json";
	case LANG_TYPESCRIPT:
		return "typescript";
	case LANG_MARKDOWN:
		return "markdown";
	case LANG_GO:
		return "go";
	case LANG_RUST:
		return "rust";
	case LANG_PYTHON:
		return "python";
	case LANG_HTML:
		return "html";
	case LANG_TEX:
		return "latex";
	case LANG_XML:
		return "xml";
	case LANG_COUNT: break;
	}
	assert(0);
	return "text";
}

typedef struct {
	LSP *lsp;
	StrBuilder builder;
	bool is_first;
} JSONWriter;

static JSONWriter json_writer_new(LSP *lsp) {
	return (JSONWriter){
		.lsp = lsp,
		.builder = str_builder_new(),
		.is_first = true
	};
}

static void write_obj_start(JSONWriter *o) {
	str_builder_append(&o->builder, "{");
	o->is_first = true;
}

static void write_obj_end(JSONWriter *o) {
	str_builder_append(&o->builder, "}");
	o->is_first = false;
}

static void write_arr_start(JSONWriter *o) {
	str_builder_append(&o->builder, "[");
	o->is_first = true;
}

static void write_arr_end(JSONWriter *o) {
	str_builder_append(&o->builder, "]");
	o->is_first = false;
}

static void write_arr_elem(JSONWriter *o) {
	if (o->is_first) {
		o->is_first = false;
	} else {
		str_builder_append(&o->builder, ",");
	}
}

static void write_escaped(JSONWriter *o, const char *string) {
	StrBuilder *b = &o->builder;
	size_t output_index = str_builder_len(b);
	size_t capacity = 2 * strlen(string) + 1;
	// append a bunch of null bytes which will hold the escaped string
	str_builder_append_null(b, capacity);
	char *out = str_builder_get_ptr(b, output_index);
	// do the escaping
	size_t length = json_escape_to(out, capacity, string);
	// shrink down to just the escaped text
	str_builder_shrink(&o->builder, output_index + length);
}

static void write_string(JSONWriter *o, const char *string) {
	str_builder_append(&o->builder, "\"");
	write_escaped(o, string);
	str_builder_append(&o->builder, "\"");
}

static void write_key(JSONWriter *o, const char *key) {
	// NOTE: no keys in the LSP spec need escaping.
	str_builder_appendf(&o->builder, "%s\"%s\":", o->is_first ? "" : ",", key);
	o->is_first = false;
}

static void write_key_obj_start(JSONWriter *o, const char *key) {
	write_key(o, key);
	write_obj_start(o);
}

static void write_key_arr_start(JSONWriter *o, const char *key) {
	write_key(o, key);
	write_arr_start(o);
}

static void write_arr_elem_obj_start(JSONWriter *o) {
	write_arr_elem(o);
	write_obj_start(o);
}

static void write_arr_elem_arr_start(JSONWriter *o) {
	write_arr_elem(o);
	write_arr_start(o);
}

static void write_number(JSONWriter *o, double number) {
	str_builder_appendf(&o->builder, "%g", number);
}

static void write_key_number(JSONWriter *o, const char *key, double number) {
	write_key(o, key);
	write_number(o, number);
}

static void write_arr_elem_number(JSONWriter *o, double number) {
	write_arr_elem(o);
	write_number(o, number);
}

static void write_null(JSONWriter *o) {
	str_builder_append(&o->builder, "null");
}

static void write_key_null(JSONWriter *o, const char *key) {
	write_key(o, key);
	write_null(o);
}

static void write_bool(JSONWriter *o, bool b) {
	str_builder_append(&o->builder, b ? "true" : "false");
}

static void write_key_bool(JSONWriter *o, const char *key, bool b) {
	write_key(o, key);
	write_bool(o, b);
}

static void write_arr_elem_null(JSONWriter *o) {
	write_arr_elem(o);
	write_null(o);
}

static void write_key_string(JSONWriter *o, const char *key, const char *s) {
	write_key(o, key);
	write_string(o, s);
}

static void write_arr_elem_string(JSONWriter *o, const char *s) {
	write_arr_elem(o);
	write_string(o, s);
}

static void write_file_uri_direct(JSONWriter *o, const char *path) {
	str_builder_append(&o->builder, "\"file://");
	write_escaped(o, path);
	str_builder_append(&o->builder, "\"");
}

static void write_file_uri(JSONWriter *o, LSPDocumentID document) {
	write_file_uri_direct(o, lsp_document_path(o->lsp, document));
}

static void write_key_file_uri(JSONWriter *o, const char *key, LSPDocumentID document) {
	write_key(o, key);
	write_file_uri(o, document);
}

static void write_key_file_uri_direct(JSONWriter *o, const char *key, const char *path) {
	write_key(o, key);
	write_file_uri_direct(o, path);
}

static void write_position(JSONWriter *o, LSPPosition position) {
	write_obj_start(o);
		write_key_number(o, "line", (double)position.line);
		write_key_number(o, "character", (double)position.character);
	write_obj_end(o);
}

static void write_key_position(JSONWriter *o, const char *key, LSPPosition position) {
	write_key(o, key);
	write_position(o, position);
}

static void write_range(JSONWriter *o, LSPRange range) {
	write_obj_start(o);
		write_key_position(o, "start", range.start);
		write_key_position(o, "end", range.end);
	write_obj_end(o);
}

static void write_key_range(JSONWriter *o, const char *key, LSPRange range) {
	write_key(o, key);
	write_range(o, range);
}

static void write_workspace_folder(JSONWriter *o, LSPDocumentID folder) {
	write_obj_start(o);
		write_key_file_uri(o, "uri", folder);
		write_key_string(o, "name", lsp_document_path(o->lsp, folder));
	write_obj_end(o);
}

static void write_workspace_folders(JSONWriter *o, LSPDocumentID *workspace_folders) {
	write_arr_start(o);
		arr_foreach_ptr(workspace_folders, LSPDocumentID, folder) {
			write_arr_elem(o);
			write_workspace_folder(o, *folder);
		}
	write_arr_end(o);
}

static void write_document_position(JSONWriter *o, LSPDocumentPosition pos) {
	write_key_obj_start(o, "textDocument");
		write_key_file_uri(o, "uri", pos.document);
	write_obj_end(o);
	write_key_position(o, "position", pos.pos);
}

static const char *lsp_request_method(LSPRequest *request) {
	switch (request->type) {
	case LSP_REQUEST_NONE: break;
	case LSP_REQUEST_INITIALIZE:
		return "initialize";
	case LSP_REQUEST_INITIALIZED:
		return "initialized";
	case LSP_REQUEST_SHUTDOWN:
		return "shutdown";
	case LSP_REQUEST_EXIT:
		return "exit";
	case LSP_REQUEST_SHOW_MESSAGE:
		return "window/showMessage";
	case LSP_REQUEST_LOG_MESSAGE:
		return "window/logMessage";
	case LSP_REQUEST_DID_OPEN:
		return "textDocument/didOpen";
	case LSP_REQUEST_DID_CLOSE:
		return "textDocument/didClose";
	case LSP_REQUEST_DID_CHANGE:
		return "textDocument/didChange";
	case LSP_REQUEST_COMPLETION:
		return "textDocument/completion";
	case LSP_REQUEST_SIGNATURE_HELP:
		return "textDocument/signatureHelp";
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		return "workspace/workspaceFolders";
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
		return "workspace/didChangeWorkspaceFolders";
	case LSP_REQUEST_JDTLS_CONFIGURATION:
		return "workspace/didChangeConfiguration";
	}
	assert(0);
	return "$/ignore";
}

static bool request_type_is_notification(LSPRequestType type) {
	switch (type) {
	case LSP_REQUEST_NONE: break;
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_DID_CHANGE:
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
	case LSP_REQUEST_JDTLS_CONFIGURATION:
		return true;
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_SIGNATURE_HELP:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		return false;
	}
	assert(0);
	return false;
}


static const size_t max_header_size = 64;
static JSONWriter message_writer_new(LSP *lsp) {
	JSONWriter writer = json_writer_new(lsp);
	// this is where our header will go
	str_builder_append_null(&writer.builder, max_header_size);
	return writer;	
}

static void message_writer_write_and_free(LSP *lsp, JSONWriter *o) {
	StrBuilder builder = o->builder;
	
	// this is kind of hacky but it lets us send the whole request with one write call.
	// probably not *actually* needed. i thought it would help fix an error but it didn't.
	size_t content_length = str_builder_len(&builder) - max_header_size;
	char content_length_str[32];
	sprintf(content_length_str, "%zu", content_length);
	size_t header_size = strlen("Content-Length: \r\n\r\n") + strlen(content_length_str);
	char *header = &builder.str[max_header_size - header_size];
	strcpy(header, "Content-Length: ");
	strcat(header, content_length_str);
	// we specifically DON'T want a null byte
	memcpy(header + strlen(header), "\r\n\r\n", 4);
	
	char *content = header;
	#if LSP_SHOW_C2S
		printf("%s%s%s\n",term_bold(stdout),content,term_clear(stdout));
	#endif
	
	// @TODO: does write always write the full amount? probably not. this should be fixed.
	process_write(&lsp->process, content, strlen(content));

	str_builder_free(&builder);
}

// NOTE: don't call lsp_request_free after calling this function.
//  I will do it for you.
static void write_request(LSP *lsp, LSPRequest *request) {
	JSONWriter writer = message_writer_new(lsp);
	JSONWriter *o = &writer;
	
	write_obj_start(o);
	write_key_string(o, "jsonrpc", "2.0");
	
	bool is_notification = request_type_is_notification(request->type);
	if (!is_notification) {
		u32 id = lsp->request_id++;
		request->id = id;
		write_key_number(o, "id", id);
	}
	write_key_string(o, "method", lsp_request_method(request));
	
	switch (request->type) {
	case LSP_REQUEST_NONE:
	// these are server-to-client-only requests
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		assert(0);
		break;
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_EXIT:
		// no params
		break;
	case LSP_REQUEST_INITIALIZED:
		write_key_obj_start(o, "params");
		write_obj_end(o);
		break;
	case LSP_REQUEST_INITIALIZE: {
		write_key_obj_start(o, "params");
			write_key_number(o, "processId", process_get_id());
			write_key_obj_start(o, "capabilities");
				// here are the client capabilities for ted
				write_key_obj_start(o, "textDocument");
					write_key_obj_start(o, "completion");
						// completion capabilities
						write_key_obj_start(o, "completionItem");
							write_key_bool(o, "snippetSupport", false);
							write_key_bool(o, "commitCharactersSupport", false);
							write_key_arr_start(o, "documentationFormat");
								// we dont really support markdown
								write_arr_elem_string(o, "plaintext");
							write_arr_end(o);
							write_key_bool(o, "deprecatedSupport", true);
							write_key_bool(o, "preselectSupport", false);
							write_key_obj_start(o, "tagSupport");
								write_key_arr_start(o, "valueSet");
									// currently the only tag in the spec
									write_arr_elem_number(o, 1);
								write_arr_end(o);
							write_obj_end(o);
							write_key_bool(o, "insertReplaceSupport", false);
						write_obj_end(o);
						// "completion item kinds" supported by ted
						// (these are the little icons displayed for function/variable/etc.)
						write_key_obj_start(o, "completionItemKind");
							write_key_arr_start(o, "valueSet");
								for (int i = LSP_COMPLETION_KIND_MIN;
									i <= LSP_COMPLETION_KIND_MAX; ++i) {
									write_arr_elem_number(o, i);
								}
							write_arr_end(o);
						write_obj_end(o);
						write_key_bool(o, "contextSupport", true);
					write_obj_end(o);
					write_key_obj_start(o, "signatureHelp");
						write_key_obj_start(o, "signatureInformation");
							write_key_obj_start(o, "parameterInformation");
								write_key_bool(o, "labelOffsetSupport", true);
							write_obj_end(o);
							write_key_bool(o, "activeParameterSupport", true);
						write_obj_end(o);
						// we don't have context support because sending the activeSignatureHelp member is annoying
						//write_key_bool(o, "contextSupport", true);
					write_obj_end(o);
				write_obj_end(o);
				write_key_obj_start(o, "workspace");
					write_key_bool(o, "workspaceFolders", true);
				write_obj_end(o);
			write_obj_end(o);
			SDL_LockMutex(lsp->workspace_folders_mutex);
			write_key_file_uri(o, "rootUri", lsp->workspace_folders[0]);
			write_key(o, "workspaceFolders");
			write_workspace_folders(o, lsp->workspace_folders);
			SDL_UnlockMutex(lsp->workspace_folders_mutex);
			write_key_obj_start(o, "clientInfo");
				write_key_string(o, "name", "ted");
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_OPEN: {
		const LSPRequestDidOpen *open = &request->data.open;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", open->document);
				write_key_string(o, "languageId", lsp_language_id(open->language));
				write_key_number(o, "version", 0);
				write_key_string(o, "text", open->file_contents);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_CLOSE: {
		const LSPRequestDidClose *close = &request->data.close;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", close->document);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_CHANGE: {
		LSPRequestDidChange *change = &request->data.change;
		SDL_LockMutex(lsp->document_mutex);
			assert(change->document < arr_len(lsp->document_data));
			LSPDocumentData *document = &lsp->document_data[change->document];
			u32 version = ++document->version_number;
		SDL_UnlockMutex(lsp->document_mutex);
		
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_number(o, "version", version);
				write_key_file_uri(o, "uri", change->document);
			write_obj_end(o);
			write_key_arr_start(o, "contentChanges");
				arr_foreach_ptr(change->changes, LSPDocumentChangeEvent, event) {
					write_arr_elem(o);
					write_obj_start(o);
						write_key_range(o, "range", event->range);
						write_key_string(o, "text", event->text ? event->text : "");
					write_obj_end(o);
				}
			write_arr_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_COMPLETION: {
		const LSPRequestCompletion *completion = &request->data.completion;
		write_key_obj_start(o, "params");
			write_document_position(o, completion->position);
			const LSPCompletionContext *context = &completion->context;
			LSPCompletionTriggerKind trigger_kind = context->trigger_kind;
			if (trigger_kind != LSP_TRIGGER_NONE) {
				write_key_obj_start(o, "context");
					write_key_number(o, "triggerKind", trigger_kind);
					if (trigger_kind == LSP_TRIGGER_CHARACTER)
						write_key_string(o, "triggerCharacter", context->trigger_character);
				write_obj_end(o);
			}
		write_obj_end(o);
	} break;
	case LSP_REQUEST_SIGNATURE_HELP: {
		const LSPRequestSignatureHelp *help = &request->data.signature_help;
		write_key_obj_start(o, "params");
			write_document_position(o, help->position);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS: {
		const LSPRequestDidChangeWorkspaceFolders *w = &request->data.change_workspace_folders;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "event");
				write_key_arr_start(o, "added");
					arr_foreach_ptr(w->added, LSPDocumentID, added) {
						write_arr_elem(o);
						write_workspace_folder(o, *added);
					}
				write_arr_end(o);
				write_key_arr_start(o, "removed");
					arr_foreach_ptr(w->removed, LSPDocumentID, removed) {
						write_arr_elem(o);
						write_workspace_folder(o, *removed);
					}
				write_arr_end(o);
			write_obj_end(o);
		write_obj_end(o);
		} break;
	case LSP_REQUEST_JDTLS_CONFIGURATION:
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "settings");
				write_key_obj_start(o, "java");
					write_key_obj_start(o, "signatureHelp");
						write_key_bool(o, "enabled", true);
					write_obj_end(o);
				write_obj_end(o);
			write_obj_end(o);
		write_obj_end(o);
		break;
	}
	
	write_obj_end(o);
	
	message_writer_write_and_free(lsp, o);
	
	if (is_notification) {
		lsp_request_free(request);
	} else {
		SDL_LockMutex(lsp->messages_mutex);
		arr_add(lsp->requests_sent, *request);
		SDL_UnlockMutex(lsp->messages_mutex);
	}
}

// NOTE: don't call lsp_response_free after calling this function.
//  I will do it for you.
static void write_response(LSP *lsp, LSPResponse *response) {
	
	JSONWriter writer = message_writer_new(lsp);
	JSONWriter *o = &writer;
	LSPRequest *request = &response->request;
	
	write_obj_start(o);
		if (request->id_string)
			write_key_string(o, "id", request->id_string);
		else
			write_key_number(o, "id", request->id);
		write_key_string(o, "jsonrpc", "2.0");
		write_key(o, "result");
		switch (response->request.type) {
		case LSP_REQUEST_WORKSPACE_FOLDERS:
			SDL_LockMutex(lsp->workspace_folders_mutex);
				write_workspace_folders(o, lsp->workspace_folders);
			SDL_UnlockMutex(lsp->workspace_folders_mutex);
			break;
		case LSP_REQUEST_SHOW_MESSAGE:
			write_null(o);
			break;
		default:
			// this is not a valid client-to-server response.
			assert(0);
			break;
		}
	write_obj_end(o);
	
	message_writer_write_and_free(lsp, o);
	lsp_response_free(response);
}

static void write_message(LSP *lsp, LSPMessage *message) {
	switch (message->type) {
	case LSP_REQUEST:
		write_request(lsp, &message->u.request);
		break;
	case LSP_RESPONSE:
		write_response(lsp, &message->u.response);
		break;
	}
	// it's okay to free the message here since the request/response part has been zeroed.
	// (as i'm writing this, this won't do anything but it might in the future)
	lsp_message_free(message);
}
