
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
	case LANG_COUNT: break;
	}
	assert(0);
	return "text";
}


typedef struct {
	StrBuilder builder;
	bool is_first;
} JSONWriter;

static JSONWriter json_writer_new(void) {
	return (JSONWriter){
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
		str_builder_append(&o->builder, ",");
		o->is_first = false;
	}
}

static void write_string(JSONWriter *o, const char* string) {
	// @TODO: escape in-place
	char *escaped = json_escape(string);
	str_builder_appendf(&o->builder, "\"%s\"", escaped);
	free(escaped);
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

static void write_number(JSONWriter *o, double number) {
	str_builder_appendf(&o->builder, "%g", number);
}

static void write_key_number(JSONWriter *o, const char *key, double number) {
	write_key(o, key);
	write_number(o, number);
}

static void write_null(JSONWriter *o) {
	str_builder_append(&o->builder, "null");
}

static void write_key_null(JSONWriter *o, const char *key) {
	write_key(o, key);
	write_null(o);
}

static void write_key_string(JSONWriter *o, const char *key, const char *s) {
	write_key(o, key);
	write_string(o, s);
}

static void write_file_uri(JSONWriter *o, const char *path) {
	char *escaped_path = json_escape(path);
	str_builder_appendf(&o->builder, "\"file:///%s\"", escaped_path);
	free(escaped_path);
}

static void write_key_file_uri(JSONWriter *o, const char *key, const char *path) {
	write_key(o, key);
	write_file_uri(o, path);
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

static const char *lsp_request_method(LSPRequest *request) {
	switch (request->type) {
	case LSP_REQUEST_NONE: break;
	case LSP_REQUEST_SHOW_MESSAGE:
		return "window/showMessage";
	case LSP_REQUEST_LOG_MESSAGE:
		return "window/logMessage";
	case LSP_REQUEST_INITIALIZE:
		return "initialize";
	case LSP_REQUEST_INITIALIZED:
		return "initialized";
	case LSP_REQUEST_DID_OPEN:
		return "textDocument/didOpen";
	case LSP_REQUEST_DID_CHANGE:
		return "textDocument/didChange";
	case LSP_REQUEST_COMPLETION:
		return "textDocument/completion";
	case LSP_REQUEST_SHUTDOWN:
		return "shutdown";
	case LSP_REQUEST_EXIT:
		return "exit";
	}
	assert(0);
	return "$/ignore";
}

// technically there are "requests" and "notifications"
// notifications are different in that they don't have IDs and don't return responses.
// this function handles both.
// NOTE: do not call lsp_request_free on request. freeing the request will be handled.
// returns the ID of the request
static void write_request(LSP *lsp, LSPRequest *request) {
	JSONWriter writer = json_writer_new();
	JSONWriter *o = &writer;
	
	u32 max_header_size = 64;
	// this is where our header will go
	str_builder_append_null(&o->builder, max_header_size);
	
	write_obj_start(o);
	write_key_string(o, "jsonrpc", "2.0");
	
	bool is_notification = request->type == LSP_REQUEST_INITIALIZED
		|| request->type == LSP_REQUEST_EXIT
		|| request->type == LSP_REQUEST_DID_OPEN;
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
		assert(0);
		break;
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_EXIT:
		// no params
		break;
	case LSP_REQUEST_INITIALIZE: {
		write_key_obj_start(o, "params");
			write_key_number(o, "processId", process_get_id());
			write_key_obj_start(o, "capabilities");
			write_obj_end(o);
			write_key_null(o, "rootUri");
			write_key_null(o, "workspaceFolders");
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_OPEN: {
		const LSPRequestDidOpen *open = &request->data.open;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", open->document);
				write_key_string(o, "languageId", lsp_language_id(open->language));
				write_key_number(o, "version", 1);
				write_key_string(o, "text", open->file_contents);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_CHANGE: {
		LSPRequestDidChange *change = &request->data.change;
		static unsigned long long version_number = 1; // @TODO @TEMPORARY
		++version_number;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_number(o, "version", (double)version_number);
				write_key_file_uri(o, "uri", change->document);
				write_key_arr_start(o, "contentChanges");
					arr_foreach_ptr(change->changes, LSPDocumentChangeEvent, event) {
						write_arr_elem(o);
						write_obj_start(o);
							write_key_range(o, "range", event->range);
							write_key_string(o, "text", event->text);
						write_obj_end(o);
					}
				write_arr_end(o);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_COMPLETION: {
		const LSPRequestCompletion *completion = &request->data.completion;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", completion->position.path);
			write_obj_end(o);
			write_key_position(o, "position", completion->position.pos);
		write_obj_end(o);
	} break;
	}
	
	write_obj_end(o);
	
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
	#if 1
		printf("\x1b[1m%s\x1b[0m\n",content);
	#endif
	
	// @TODO: does write always write the full amount? probably not. this should be fixed.
	process_write(&lsp->process, content, strlen(content));

	str_builder_free(&builder);
	
	if (is_notification) {
		lsp_request_free(request);
	} else {
		SDL_LockMutex(lsp->requests_mutex);
		arr_add(lsp->requests_sent, *request);
		SDL_UnlockMutex(lsp->requests_mutex);
	}
}
