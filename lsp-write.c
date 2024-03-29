// writing messages to the LSP server

#define LSP_INTERNAL 1
#include "lsp.h"
#include "util.h"

#define write_bool lsp_write_bool // prevent naming conflict

typedef struct {
	u64 number;
	char *identifier;
} LanguageId;
static LanguageId *language_ids = NULL; // dynamic array
void lsp_register_language(u64 id, const char *lsp_identifier) {
	LanguageId *lid = arr_addp(language_ids);
	lid->number = id;
	lid->identifier = str_dup(lsp_identifier);
}

static const char *lsp_language_id(u64 lang) {
	arr_foreach_ptr(language_ids, LanguageId, lid) {
		if (lid->number == lang) {
			return lid->identifier;
		}
	}
	assert(0);
	return "text";
}

typedef struct {
	LSP *lsp;
	StrBuilder *builder;
	bool is_first;
	size_t length_idx;
	size_t content_start_idx;
} JSONWriter;

static JSONWriter json_writer_new(LSP *lsp, StrBuilder *builder) {
	return (JSONWriter){
		.lsp = lsp,
		.builder = builder,
		.is_first = true
	};
}

static void write_obj_start(JSONWriter *o) {
	str_builder_append(o->builder, "{");
	o->is_first = true;
}

static void write_obj_end(JSONWriter *o) {
	str_builder_append(o->builder, "}");
	o->is_first = false;
}

static void write_arr_start(JSONWriter *o) {
	str_builder_append(o->builder, "[");
	o->is_first = true;
}

static void write_arr_end(JSONWriter *o) {
	str_builder_append(o->builder, "]");
	o->is_first = false;
}

static void write_arr_elem(JSONWriter *o) {
	if (o->is_first) {
		o->is_first = false;
	} else {
		str_builder_append(o->builder, ",");
	}
}

static void write_escaped(JSONWriter *o, const char *string) {
	StrBuilder *b = o->builder;
	size_t output_index = str_builder_len(b);
	size_t capacity = 2 * strlen(string) + 1;
	// append a bunch of null bytes which will hold the escaped string
	str_builder_append_null(b, capacity);
	char *out = str_builder_get_ptr(b, output_index);
	// do the escaping
	size_t length = json_escape_to(out, capacity, string);
	// shrink down to just the escaped text
	str_builder_shrink(o->builder, output_index + length);
}

static void write_string(JSONWriter *o, const char *string) {
	str_builder_append(o->builder, "\"");
	write_escaped(o, string);
	str_builder_append(o->builder, "\"");
}

static void write_key(JSONWriter *o, const char *key) {
	// NOTE: no keys in the LSP spec need escaping.
	str_builder_appendf(o->builder, "%s\"%s\":", o->is_first ? "" : ",", key);
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
	str_builder_appendf(o->builder, "%g", number);
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
	str_builder_append(o->builder, "null");
}

static void write_key_null(JSONWriter *o, const char *key) {
	write_key(o, key);
	write_null(o);
}

static void write_bool(JSONWriter *o, bool b) {
	str_builder_append(o->builder, b ? "true" : "false");
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

static void write_file_uri(JSONWriter *o, LSPDocumentID document) {
	const char *path = lsp_document_path(o->lsp, document);
	str_builder_append(o->builder, "\"file://");
	#if _WIN32
		// why the fuck is there another slash it makes no goddamn sense
		str_builder_append(o->builder, "/");
	#endif
	for (const char *p = path; *p; ++p) {
		char c = *p;
		#if _WIN32
		// file URIs use slashes: https://en.wikipedia.org/wiki/File_URI_scheme
		if (c == '\\') c = '/';
		#endif
		
		// see https://www.rfc-editor.org/rfc/rfc3986#page-12
		// these are the only allowed un-escaped characters in URIs
		bool escaped = !(
			   (c >= '0' && c <= '9')
			|| (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| c == '_' || c == '-' || c == '.' || c == '~' || c == '/'
			#if _WIN32
			|| c == ':' // i dont think you're supposed to escape the : in C:\...
			#endif
			);
		if (escaped) {
			str_builder_appendf(o->builder, "%%%02x", (uint8_t)c);
		} else {
			str_builder_appendf(o->builder, "%c", c);
		}
	}
	str_builder_append(o->builder, "\"");
}

static void write_key_file_uri(JSONWriter *o, const char *key, LSPDocumentID document) {
	write_key(o, key);
	write_file_uri(o, document);
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
	case LSP_REQUEST_CANCEL:
		return "$/cancelRequest";
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
	case LSP_REQUEST_PUBLISH_DIAGNOSTICS:
		return "textDocument/publishDiagnostics";
	case LSP_REQUEST_HOVER:
		return "textDocument/hover";
	case LSP_REQUEST_REFERENCES:
		return "textDocument/references";
	case LSP_REQUEST_DEFINITION:
		return "textDocument/definition";
	case LSP_REQUEST_DECLARATION:
		return "textDocument/declaration";
	case LSP_REQUEST_TYPE_DEFINITION:
		return "textDocument/typeDefinition";
	case LSP_REQUEST_IMPLEMENTATION:
		return "textDocument/implementation";
	case LSP_REQUEST_HIGHLIGHT:
		return "textDocument/documentHighlight";
	case LSP_REQUEST_DOCUMENT_LINK:
		return "textDocument/documentLink";
	case LSP_REQUEST_RENAME:
		return "textDocument/rename";
	case LSP_REQUEST_WORKSPACE_FOLDERS:
		return "workspace/workspaceFolders";
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
		return "workspace/didChangeWorkspaceFolders";
	case LSP_REQUEST_CONFIGURATION:
		return "workspace/didChangeConfiguration";
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		return "workspace/symbol";
	case LSP_REQUEST_RANGE_FORMATTING:
		return "textDocument/rangeFormatting";
	case LSP_REQUEST_FORMATTING:
		return "textDocument/formatting";
	}
	assert(0);
	return "$/ignore";
}

static JSONWriter message_writer_new(LSP *lsp, StrBuilder *builder) {
	JSONWriter writer = json_writer_new(lsp, builder);
	str_builder_append(builder, "Content-Length: ");
	writer.length_idx = str_builder_len(builder);
	str_builder_append(builder, "XXXXXXXXXX\r\n\r\n");
	writer.content_start_idx = str_builder_len(builder);
	return writer;	
}

static void message_writer_finish(JSONWriter *o) {
	StrBuilder *builder = o->builder;
	char content_len_str[32] = {0};
	size_t content_len = str_builder_len(builder) - o->content_start_idx;
	if (content_len > 9999999999)
		return; // fuckin what
	strbuf_printf(content_len_str, "%zu", content_len);
	assert(strlen(content_len_str) <= 10);
	char *content_len_out = &builder->str[o->length_idx];
	memcpy(content_len_out, content_len_str, strlen(content_len_str));
	// slide everything over
	// ideally, we would just use %10zu,
	// but rust-analyzer rejects extra whitespace
	// (even though its legal in HTTP)
	memmove(content_len_out + strlen(content_len_str),
		content_len_out + 10,
		4 /* \r\n\r\n */ + content_len);
	str_builder_shrink(builder, str_builder_len(builder) - (10 - strlen(content_len_str)));
}

static void write_symbol_tag_support(JSONWriter *o) {
	write_key_obj_start(o, "tagSupport");
		write_key_arr_start(o, "valueSet");
			for (int i = LSP_SYMBOL_TAG_MIN; i <= LSP_SYMBOL_TAG_MAX; ++i)
				write_arr_elem_number(o, i);
		write_arr_end(o);
	write_obj_end(o);
}


static void write_completion_item_kind_support(JSONWriter *o) {
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
}

static void write_symbol_kind_support(JSONWriter *o) {
	write_key_obj_start(o, "symbolKind");
		write_key_arr_start(o, "valueSet");
			for (int i = LSP_SYMBOL_KIND_MIN;
				i <= LSP_SYMBOL_KIND_MAX;
				++i) {
				write_arr_elem_number(o, i);
			}
		write_arr_end(o);
	write_obj_end(o);
}

// NOTE: don't call lsp_request_free after calling this function.
//  I will do it for you.
void write_request(LSP *lsp, LSPRequest *request, StrBuilder *builder) {
	JSONWriter writer = message_writer_new(lsp, builder);
	JSONWriter *o = &writer;
	
	write_obj_start(o);
	write_key_string(o, "jsonrpc", "2.0");
	
	if (request->id) { // i.e. if this is a request as opposed to a notification
		write_key_number(o, "id", request->id);
	}
	write_key_string(o, "method", lsp_request_method(request));
	
	switch (request->type) {
	case LSP_REQUEST_NONE:
	// these are server-to-client-only requests
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
	case LSP_REQUEST_PUBLISH_DIAGNOSTICS:
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
							write_symbol_tag_support(o);
							write_key_bool(o, "insertReplaceSupport", false);
						write_obj_end(o);
						write_completion_item_kind_support(o);
						write_key_bool(o, "contextSupport", true);
					write_obj_end(o);
					
					// signature help capabilities
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
					
					// hover capabilities
					write_key_obj_start(o, "hover");
						write_key_arr_start(o, "contentFormat");
							write_arr_elem_string(o, "plaintext");
						write_arr_end(o);
					write_obj_end(o);
					
					// definition capabilities
					write_key_obj_start(o, "definition");
						// NOTE: LocationLink support doesn't seem useful to us right now.
					write_obj_end(o);
					
					// document link capabilities
					write_key_obj_start(o, "documentLink");
						write_key_bool(o, "tooltipSupport", true);
					write_obj_end(o);
					
					// publish diagnostics capabilities
					write_key_obj_start(o, "publishDiagnostics");
						write_key_bool(o, "codeDescriptionSupport", true);
					write_obj_end(o);
				write_obj_end(o);
				write_key_obj_start(o, "workspace");
					write_key_bool(o, "workspaceFolders", true);
					write_key_obj_start(o, "workspaceEdit");
						write_key_bool(o, "documentChanges", true);
						write_key_arr_start(o, "resourceOperations");
							write_arr_elem_string(o, "create");
							write_arr_elem_string(o, "rename");
							write_arr_elem_string(o, "delete");
						write_arr_end(o);
					write_obj_end(o);
					write_key_obj_start(o, "symbol");
						write_symbol_kind_support(o);
						write_symbol_tag_support(o);
						// resolve is kind of a pain to implement. i'm not doing it yet.
					write_obj_end(o);
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
	case LSP_REQUEST_CANCEL: {
		const LSPRequestCancel *cancel = &request->data.cancel;
		write_key_obj_start(o, "params");
			write_key_number(o, "id", cancel->id);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DID_OPEN: {
		const LSPRequestDidOpen *open = &request->data.open;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", open->document);
				write_key_string(o, "languageId", lsp_language_id(open->language));
				write_key_number(o, "version", 0);
				write_key_string(o, "text", lsp_request_string(request, open->file_contents));
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
						if (event->use_range)
							write_key_range(o, "range", event->range);
						write_key_string(o, "text", lsp_request_string(request, event->text));
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
	case LSP_REQUEST_HOVER: {
		const LSPRequestHover *hover = &request->data.hover;
		write_key_obj_start(o, "params");
			write_document_position(o, hover->position);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION: {
		const LSPRequestDefinition *def = &request->data.definition;
		write_key_obj_start(o, "params");
			write_document_position(o, def->position);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_HIGHLIGHT: {
		const LSPRequestHighlight *hl = &request->data.highlight;
		write_key_obj_start(o, "params");
			write_document_position(o, hl->position);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_REFERENCES: {
		const LSPRequestReferences *refs = &request->data.references;
		write_key_obj_start(o, "params");
			write_document_position(o, refs->position);
			write_key_obj_start(o, "context");
				// why is this includeDeclaration thing which has nothing to do with context
				// why is it in an object called context
				// there's no other members of the ReferenceContext interface. just this.
				// why, LSP, why
				write_key_bool(o, "includeDeclaration", refs->include_declaration);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_DOCUMENT_LINK: {
		const LSPRequestDocumentLink *lnk = &request->data.document_link;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", lnk->document);
			write_obj_end(o);
		write_obj_end(o);
	} break;
	case LSP_REQUEST_RENAME: {
		const LSPRequestRename *rename = &request->data.rename;
		write_key_obj_start(o, "params");
			write_document_position(o, rename->position);
			write_key_string(o, "newName", lsp_request_string(request, rename->new_name));
		write_obj_end(o);
	} break;
	case LSP_REQUEST_WORKSPACE_SYMBOLS: {
		const LSPRequestWorkspaceSymbols *syms = &request->data.workspace_symbols;
		write_key_obj_start(o, "params");
			write_key_string(o, "query", lsp_request_string(request, syms->query));
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
	case LSP_REQUEST_CONFIGURATION: {
		const LSPRequestConfiguration *config = &request->data.configuration;
		write_key_obj_start(o, "params");
			write_key(o, "settings");
			str_builder_append(o->builder, lsp_request_string(request, config->settings));
		write_obj_end(o);
		} break;
	case LSP_REQUEST_FORMATTING:
	case LSP_REQUEST_RANGE_FORMATTING: {
		const LSPRequestFormatting *formatting = &request->data.formatting;
		write_key_obj_start(o, "params");
			write_key_obj_start(o, "textDocument");
				write_key_file_uri(o, "uri", formatting->document);
			write_obj_end(o);
			write_key_obj_start(o, "options");
				write_key_number(o, "tabSize", formatting->tab_width);
				write_key_bool(o, "insertSpaces", formatting->indent_with_spaces);
			write_obj_end(o);
			if (formatting->use_range) {
				write_key_range(o, "range", formatting->range);
			}
		write_obj_end(o);
		} break;
	}
	
	write_obj_end(o);
	
	message_writer_finish(o);
	
	if (request->id) {
		SDL_LockMutex(lsp->messages_mutex);
		arr_add(lsp->requests_sent, *request);
		SDL_UnlockMutex(lsp->messages_mutex);
	} else {
		lsp_request_free(request);
	}
}

// NOTE: don't call lsp_response_free after calling this function.
//  I will do it for you.
static void write_response(LSP *lsp, LSPResponse *response, StrBuilder *builder) {
	
	JSONWriter writer = message_writer_new(lsp, builder);
	JSONWriter *o = &writer;
	LSPRequest *request = &response->request;
	
	write_obj_start(o);
		const char *id_string = lsp_request_string(request, request->id_string);
		if (*id_string)
			write_key_string(o, "id", id_string);
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
	
	message_writer_finish(o);
	lsp_response_free(response);
}

void write_message(LSP *lsp, LSPMessage *message, StrBuilder *builder) {
	switch (message->type) {
	case LSP_REQUEST:
		write_request(lsp, &message->request, builder);
		break;
	case LSP_RESPONSE:
		write_response(lsp, &message->response, builder);
		break;
	}
}

#undef write_bool

void lsp_write_quit(void) {
	arr_foreach_ptr(language_ids, LanguageId, lid) {
		free(lid->identifier);
	}
	arr_clear(language_ids);
}
