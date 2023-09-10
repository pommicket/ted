// main file for dealing with LSP servers

#define LSP_INTERNAL 1
#include "lsp.h"
#include "util.h"

static LSPMutex request_id_mutex;

u32 lsp_get_id(const LSP *lsp) {
	return lsp->id;
}

// it's nice to have request IDs be totally unique, including across LSP servers.
static LSPRequestID get_request_id(void) {
	// it's important that this never returns 0, since that's reserved for "no ID"
	static LSPRequestID last_request_id;
	assert(request_id_mutex);
	SDL_LockMutex(request_id_mutex);
		u32 id = ++last_request_id;
	SDL_UnlockMutex(request_id_mutex);
	return id;
}

bool lsp_get_error(LSP *lsp, char *error, size_t error_size, bool clear) {
	bool has_err = false;
	SDL_LockMutex(lsp->error_mutex);
	has_err = *lsp->error != '\0';
	if (error_size)
		str_cpy(error, error_size, lsp->error);
	if (clear)
		*lsp->error = '\0';
	SDL_UnlockMutex(lsp->error_mutex);
	return has_err;
}

bool lsp_string_is_empty(LSPString string) {
	return string.offset == 0;
}

char *lsp_message_alloc_string(LSPMessageBase *message, size_t len, LSPString *string) {
	u32 offset = arr_len(message->string_data);
	if (len == 0) {
		offset = 0;
	} else if (offset == 0) {
		// reserve offset 0 for empty string
		arr_add(message->string_data, 0);
		offset = 1;
	}
	arr_set_len(message->string_data, offset + len + 1);
	string->offset = offset;
	return message->string_data + offset;
}

static LSPString lsp_message_add_string(LSPMessageBase *message, const char *string) {
	LSPString ret = {0};
	size_t len = strlen(string);
	if (len == 0) {
		return ret;
	}
	char *dest = lsp_message_alloc_string(message, len, &ret);
	memcpy(dest, string, len);
	return ret;
}
static LSPString lsp_message_add_json_string(LSPMessageBase *message, const JSON *json, JSONString string) {
	LSPString ret = {0};
	size_t len = string.len;
	if (len == 0) {
		return ret;
	}
	char *dest = lsp_message_alloc_string(message, len, &ret);
	json_string_get(json, string, dest, len + 1);
	return ret;
}
LSPString lsp_message_add_string32(LSPMessageBase *message, String32 string) {
	LSPString ret = {0};
	size_t len32 = string.len;
	if (len32 == 0) {
		return ret;
	}
	char *dest = lsp_message_alloc_string(message, len32 * 4 + 1, &ret);
	str32_to_utf8_cstr_in_place(string, dest);
	return ret;
}

LSPString lsp_request_add_string(LSPRequest *request, const char *string) {
	return lsp_message_add_string(&request->base, string);
}
LSPString lsp_response_add_string(LSPResponse *response, const char *string) {
	return lsp_message_add_string(&response->base, string);
}
LSPString lsp_response_add_json_string(LSPResponse *response, const JSON *json, JSONString string) {
	return lsp_message_add_json_string(&response->base, json, string);
}
LSPString lsp_request_add_json_string(LSPRequest *request, const JSON *json, JSONString string) {
	return lsp_message_add_json_string(&request->base, json, string);
}


static void lsp_message_base_free(LSPMessageBase *base) {
	arr_free(base->string_data);
}

void lsp_request_free(LSPRequest *r) {
	lsp_message_base_free(&r->base);
	switch (r->type) {
	case LSP_REQUEST_NONE:
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_SIGNATURE_HELP:
	case LSP_REQUEST_HOVER:
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION:
	case LSP_REQUEST_REFERENCES:
	case LSP_REQUEST_HIGHLIGHT:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
	case LSP_REQUEST_DOCUMENT_LINK:
	case LSP_REQUEST_CONFIGURATION:
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_FORMATTING:
	case LSP_REQUEST_RANGE_FORMATTING:
		break;
	case LSP_REQUEST_PUBLISH_DIAGNOSTICS: {
		LSPRequestPublishDiagnostics *pub = &r->data.publish_diagnostics;
		arr_free(pub->diagnostics);
		} break;
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_RENAME:
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		break;
	case LSP_REQUEST_DID_CHANGE: {
		LSPRequestDidChange *c = &r->data.change;
		arr_free(c->changes);
		} break;
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS: {
		LSPRequestDidChangeWorkspaceFolders *w = &r->data.change_workspace_folders;
		arr_free(w->added);
		arr_free(w->removed);
		} break;
	}
	memset(r, 0, sizeof *r);
}

void lsp_response_free(LSPResponse *r) {
	lsp_message_base_free(&r->base);
	switch (r->request.type) {
	case LSP_REQUEST_COMPLETION:
		arr_free(r->data.completion.items);
		break;
	case LSP_REQUEST_SIGNATURE_HELP:
		arr_free(r->data.signature_help.signatures);
		break;
	case LSP_REQUEST_DEFINITION:
		arr_free(r->data.definition.locations);
		break;
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		arr_free(r->data.workspace_symbols.symbols);
		break;
	case LSP_REQUEST_RENAME: {
		LSPResponseRename *rename = &r->data.rename;
		arr_foreach_ptr(rename->changes, LSPWorkspaceChange, c) {
			if (c->type == LSP_CHANGE_EDITS) {
				arr_free(c->data.edit.edits);
			}
		}
		arr_free(r->data.rename.changes);
		} break;
	case LSP_REQUEST_HIGHLIGHT:
		arr_free(r->data.highlight.highlights);
		break;
	case LSP_REQUEST_REFERENCES:
		arr_free(r->data.references.locations);
		break;
	case LSP_REQUEST_DOCUMENT_LINK:
		arr_free(r->data.document_link.links);
		break;
	case LSP_REQUEST_FORMATTING:
		arr_free(r->data.formatting.edits);
		break;
	default:
		break;
	}
	lsp_request_free(&r->request);
	memset(r, 0, sizeof *r);
}

void lsp_message_free(LSPMessage *message) {
	switch (message->type) {
	case LSP_REQUEST:
		lsp_request_free(&message->request);
		break;
	case LSP_RESPONSE:
		lsp_response_free(&message->response);
		break;
	}
	memset(message, 0, sizeof *message);
}


// figure out if data begins with a complete LSP response.
static bool has_response(const char *data, size_t data_len, u64 *p_offset, u64 *p_size) {
	const char *content_length = strstr(data, "Content-Length");
	if (!content_length) return false;
	const char *p = content_length + strlen("Content-Length");
	if (!p[0] || !p[1] || !p[2]) return false;
	p += 2;
	size_t size = (size_t)atoll(p);
	*p_size = size;
	const char *header_end = strstr(content_length, "\r\n\r\n");
	if (!header_end) return false;
	header_end += 4;
	u64 offset = (u64)(header_end - data);
	*p_offset = offset;
	return offset + size <= data_len;
}

static bool lsp_supports_request(LSP *lsp, const LSPRequest *request) {
	LSPCapabilities *cap = &lsp->capabilities;
	switch (request->type) {
	case LSP_REQUEST_NONE:
	// return false for server-to-client requests since we should never send them
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
	case LSP_REQUEST_PUBLISH_DIAGNOSTICS:
		return false;
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_DID_CLOSE:
		return cap->open_close_support;
	case LSP_REQUEST_DID_CHANGE:
		return cap->sync_support;
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_CONFIGURATION:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_EXIT:
		return true;
	case LSP_REQUEST_COMPLETION:
		return cap->completion_support;
	case LSP_REQUEST_SIGNATURE_HELP:
		return cap->signature_help_support;
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
		return cap->workspace_folders_support;
	case LSP_REQUEST_HOVER:
		return cap->hover_support;
	case LSP_REQUEST_DEFINITION:
		return cap->definition_support;
	case LSP_REQUEST_DECLARATION:
		return cap->declaration_support;
	case LSP_REQUEST_TYPE_DEFINITION:
		return cap->type_definition_support;
	case LSP_REQUEST_IMPLEMENTATION:
		return cap->implementation_support;
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
		return cap->workspace_symbols_support;
	case LSP_REQUEST_RENAME:
		return cap->rename_support;
	case LSP_REQUEST_HIGHLIGHT:
		return cap->highlight_support;
	case LSP_REQUEST_REFERENCES:
		return cap->references_support;
	case LSP_REQUEST_DOCUMENT_LINK:
		return cap->document_link_support;
	case LSP_REQUEST_FORMATTING:
		return cap->formatting_support;
	case LSP_REQUEST_RANGE_FORMATTING:
		return cap->range_formatting_support;
	}
	assert(0);
	return false;
}

void lsp_send_message(LSP *lsp, LSPMessage *message) {
	SDL_LockMutex(lsp->messages_mutex);
	arr_add(lsp->messages_client2server, *message);
	SDL_UnlockMutex(lsp->messages_mutex);
}

static bool request_type_is_notification(LSPRequestType type) {
	switch (type) {
	case LSP_REQUEST_NONE: break;
	case LSP_REQUEST_INITIALIZED:
	case LSP_REQUEST_EXIT:
	case LSP_REQUEST_CANCEL:
	case LSP_REQUEST_DID_OPEN:
	case LSP_REQUEST_DID_CLOSE:
	case LSP_REQUEST_DID_CHANGE:
	case LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS:
	case LSP_REQUEST_CONFIGURATION:
	case LSP_REQUEST_PUBLISH_DIAGNOSTICS:
		return true;
	case LSP_REQUEST_INITIALIZE:
	case LSP_REQUEST_SHUTDOWN:
	case LSP_REQUEST_SHOW_MESSAGE:
	case LSP_REQUEST_LOG_MESSAGE:
	case LSP_REQUEST_COMPLETION:
	case LSP_REQUEST_HIGHLIGHT:
	case LSP_REQUEST_SIGNATURE_HELP:
	case LSP_REQUEST_HOVER:
	case LSP_REQUEST_DEFINITION:
	case LSP_REQUEST_DECLARATION:
	case LSP_REQUEST_TYPE_DEFINITION:
	case LSP_REQUEST_IMPLEMENTATION:
	case LSP_REQUEST_REFERENCES:
	case LSP_REQUEST_RENAME:
	case LSP_REQUEST_WORKSPACE_SYMBOLS:
	case LSP_REQUEST_WORKSPACE_FOLDERS:
	case LSP_REQUEST_DOCUMENT_LINK:
	case LSP_REQUEST_FORMATTING:
	case LSP_REQUEST_RANGE_FORMATTING:
		return false;
	}
	assert(0);
	return false;
}

LSPServerRequestID lsp_send_request(LSP *lsp, LSPRequest *request) {
	if (!lsp_supports_request(lsp, request)) {
		lsp_request_free(request);
		return (LSPServerRequestID){0};
	}
	
	bool is_notification = request_type_is_notification(request->type);
	if (!is_notification)
		request->id = get_request_id();
	LSPMessage message = {0};
	request->base.type = LSP_REQUEST;
	message.request = *request;
	lsp_send_message(lsp, &message);
	return (LSPServerRequestID) {
		.lsp = lsp->id,
		.id = request->id
	};
}

void lsp_send_response(LSP *lsp, LSPResponse *response) {
	LSPMessage message = {0};
	response->base.type = LSP_RESPONSE;
	message.response = *response;
	lsp_send_message(lsp, &message);
}

static const char *lsp_message_string(const LSPMessageBase *message, LSPString string) {
	// important that we have this check here, since
	// it's possible that message->string_data is NULL (i.e. if no strings were ever added)
	if (string.offset == 0) {
		return "";
	}
	assert(string.offset < arr_len(message->string_data));
	return &message->string_data[string.offset];
}

const char *lsp_response_string(const LSPResponse *response, LSPString string) {
	return lsp_message_string(&response->base, string);
}

const char *lsp_request_string(const LSPRequest *request, LSPString string) {
	return lsp_message_string(&request->base, string);
}

// receive responses/requests/notifications from LSP, up to max_size bytes.
// returns false if the process exited
static bool lsp_receive(LSP *lsp, size_t max_size) {

	if (lsp->process) {
		// read stderr. if all goes well, we shouldn't get anything over stderr.
		char stderr_buf[1024] = {0};
		for (size_t i = 0; i < (max_size + sizeof stderr_buf) / sizeof stderr_buf; ++i) {
			long long nstderr = process_read_stderr(lsp->process, stderr_buf, sizeof stderr_buf - 1);
			if (nstderr > 0) {
				// uh oh
				stderr_buf[nstderr] = '\0';
				if (lsp->log) {
					fprintf(lsp->log, "LSP SERVER STDERR\n%s\n\n", stderr_buf);
				}
				eprint("%s%s%s%s", term_bold(stderr), term_yellow(stderr), stderr_buf, term_clear(stderr));
			} else {
				break;
			}
		}
	}
	
	if (lsp->process) {
		// check process status
		ProcessExitInfo info = {0};
		int status = process_check_status(&lsp->process, &info);
		if (status != 0) {
			bool not_found =
			#if _WIN32
				false
			#else
				info.exit_code == 127
			#endif
				;
			
			if (not_found) {
				// don't give an error if the server is not found.
				// still log it though.
				if (lsp->log)
					fprintf(lsp->log, "LSP server exited: %s. Probably the server is not installed.",
						info.message);
			} else {
				lsp_set_error(lsp, "Can't access LSP server: %s\n"
					"Run ted in a terminal or set lsp-log = on for more details.\n"
					"Run the :lsp-reset command to restart the server."
					, info.message);
			}
			return false;
		}
		
	}

	size_t received_so_far = arr_len(lsp->received_data);
	arr_reserve(lsp->received_data, received_so_far + max_size + 1);
	long long bytes_read = lsp->socket
		? socket_read(lsp->socket, lsp->received_data + received_so_far, max_size)
		: process_read(lsp->process, lsp->received_data + received_so_far, max_size);
	
	if (bytes_read == -1) {
		// no data
		return true;
	}
	if (bytes_read == 0) {
		lsp_set_error(lsp, "LSP server closed connection unexpectedly.");
		return false;
	}
	if (bytes_read < 0) {
		if (lsp->log)
			fprintf(lsp->log, "Error reading from server (errno = %d).\n", errno);
		return true;
	}
	
	received_so_far += (size_t)bytes_read;
	// kind of a hack. this is needed because arr_set_len zeroes the data.
	arr_hdr_(lsp->received_data)->len = (u32)received_so_far;
	lsp->received_data[received_so_far] = '\0';// null terminate
	#if LSP_SHOW_S2C
	const int limit = 1000;
	debug_println("%s%.*s%s%s",term_italics(stdout),limit,lsp->received_data,
		strlen(lsp->received_data) > (size_t)limit ? "..." : "",
		term_clear(stdout));
	#endif
	
	u64 response_offset=0, response_size=0;
	while (has_response(lsp->received_data, received_so_far, &response_offset, &response_size)) {
		if (response_offset + response_size > arr_len(lsp->received_data)) {
			// we haven't received this whole response yet.
			break;
		}
		
		char *copy = strn_dup(lsp->received_data + response_offset, response_size);
		if (lsp->log) {
			fprintf(lsp->log, "LSP MESSAGE FROM SERVER TO CLIENT\n%s\n\n", copy);
		}
		JSON json = {0};
		if (json_parse(&json, copy)) {
			assert(json.text == copy);
			json.is_text_copied = true;
			process_message(lsp, &json);
		} else {
			lsp_set_error(lsp, "couldn't parse response JSON: %s", json.error);
			json_free(&json);
		}
		size_t leftover_data_len = arr_len(lsp->received_data) - (response_offset + response_size);
		
		//printf("arr_cap = %u response_offset = %u, response_size = %zu, leftover len = %u\n",
		//	arr_hdr_(lsp->received_data)->cap,
		//	response_offset, response_size, leftover_data_len);
		memmove(lsp->received_data, lsp->received_data + response_offset + response_size,
			leftover_data_len);
		arr_set_len(lsp->received_data, leftover_data_len);
		arr_reserve(lsp->received_data, leftover_data_len + 1);
		lsp->received_data[leftover_data_len] = '\0';
	}
	return true;
}

/// send requests.
///
/// returns `false` if we should quit.
static bool lsp_send(LSP *lsp) {
	if (!lsp->initialized) {
		// don't send anything before the server is initialized.
		return true;
	}
	
	LSPMessage *messages = NULL;
	SDL_LockMutex(lsp->messages_mutex);
	size_t n_messages = arr_len(lsp->messages_client2server);
	messages = calloc(n_messages, sizeof *messages);
	memcpy(messages, lsp->messages_client2server, n_messages * sizeof *messages);
	
	#if __GNUC__ && !__clang__
	#pragma GCC diagnostic push
	// i don't know why GCC is giving me this. some compiler bug.
	#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
	#endif
	arr_clear(lsp->messages_client2server);
	#if __GNUC__ && !__clang__
	#pragma GCC diagnostic pop
	#endif
	
	SDL_UnlockMutex(lsp->messages_mutex);

	bool alive = true;
	for (size_t i = 0; i < n_messages; ++i) {
		LSPMessage *m = &messages[i];
		bool send = alive;
		if (send && m->type == LSP_REQUEST
			&& i + 1 < n_messages
			&& messages[i + 1].type == LSP_REQUEST) {
			const LSPRequest *r = &m->request;
			const LSPRequest *next = &messages[i + 1].request;
			if (r->type == LSP_REQUEST_DID_CHANGE
				&& next->type == LSP_REQUEST_DID_CHANGE
				&& arr_len(r->data.change.changes) == 1
				&& arr_len(next->data.change.changes) == 1
				&& !r->data.change.changes[0].use_range
				&& !next->data.change.changes[1].use_range
				&& r->data.change.document == next->data.change.document) {
				// we don't need to send this request, since it's made
				// irrelevant by the next request.
				// (specifically, they're both full-document-content
				//  didChange notifications)
				// this helps godot's language server a lot
				// since it's super slow because it tries to publish diagnostics
				// on every change.
				send = false;
			}
		}

		if (send) {
			write_message(lsp, m);
		} else {
			lsp_message_free(m);
		}
		
		if (SDL_SemTryWait(lsp->quit_sem) == 0) {
			alive = false;
		}
	}

	free(messages);
	return alive;
}


// Do any necessary communication with the LSP.
// This writes requests and reads (and parses) responses.
static int lsp_communication_thread(void *data) {
	LSP *lsp = data;
	
	if (lsp->port) {
		lsp->socket = socket_connect_tcp(NULL, lsp->port);
		const char *error = socket_get_error(lsp->socket);
		if (*error) {
			lsp_set_error(lsp, "%s", error);
			return 0;
		}
	}

	LSPRequest initialize = {
		.type = LSP_REQUEST_INITIALIZE
	};
	initialize.id = get_request_id();
	write_request(lsp, &initialize);
	
	const double send_delay = lsp->send_delay;
	double last_send = -DBL_MAX;
	while (1) {
		bool send = true;
		if (send_delay > 0) {
			double t = time_get_seconds();
			if (t - last_send > send_delay) {
				last_send = t;
			} else {
				send = false;
			}
		}
		if (send && !lsp_send(lsp))
			break;
		if (!lsp_receive(lsp, (size_t)10<<20))
			break;
		if (SDL_SemWaitTimeout(lsp->quit_sem, 5) == 0)
			break;	
	}
	
	lsp->exited = true;
	if (!lsp->process) {
		// process already exited
		return 0;
	}
	
	if (lsp->initialized) {
		LSPRequest shutdown = {
			.type = LSP_REQUEST_SHUTDOWN,
			.data = {{0}}
		};
		LSPRequest exit = {
			.type = LSP_REQUEST_EXIT,
			.data = {{0}}
		};
		// just spam these things
		// we're supposed to be nice and wait for the shutdown
		// response, but who gives a fuck
		write_request(lsp, &shutdown);
		write_request(lsp, &exit);
		
		#if 0
		char buf[1024]={0};
		long long n = process_read(&lsp->process, buf, sizeof buf);
		if (n>0) {
			buf[n]=0;
			printf("%s\n",buf);
		}
		n = process_read_stderr(&lsp->process, buf, sizeof buf);
		if (n>0) {
			buf[n]=0;
			printf("\x1b[1m%s\x1b[0m\n",buf);
		}
		#endif
	}
	return 0;
}

u32 lsp_document_id(LSP *lsp, const char *path) {
	if (!path) {
		assert(0);
		return 0;
	}
	SDL_LockMutex(lsp->document_mutex);
		u32 *value = str_hash_table_get(&lsp->document_ids, path);
		if (!value) {
			u32 id = arr_len(lsp->document_data);
			value = str_hash_table_insert(&lsp->document_ids, path);
			*value = id;
			LSPDocumentData *data = arr_addp(lsp->document_data);
			data->path = str_dup(path);
		}
		u32 id = *value;
	SDL_UnlockMutex(lsp->document_mutex);
	return id;
}

const char *lsp_document_path(LSP *lsp, LSPDocumentID document) {
	SDL_LockMutex(lsp->document_mutex);
		if (document >= arr_len(lsp->document_data)) {
			assert(0);
			return "";
		}
		// it's okay to keep a pointer to this around without the mutex locked
		// we'll never change the path of a document ID.
		const char *path = lsp->document_data[document].path;
	SDL_UnlockMutex(lsp->document_mutex);
	return path;
}

LSP *lsp_create(const LSPSetup *setup) {
	LSP *lsp = calloc(1, sizeof *lsp);
	if (!lsp) return NULL;
	if (!request_id_mutex)
		request_id_mutex = SDL_CreateMutex();
	
	const char *const command = setup->command;
	const u16 port = setup->port;
	const char *const root_dir = setup->root_dir;
	const char *const configuration = setup->configuration;
	
	static LSPID curr_id = 1;
	lsp->id = curr_id++;
	lsp->log = setup->log;
	lsp->port = port;
	lsp->send_delay = setup->send_delay;

	debug_println("Starting up LSP %p (ID %u) `%s` (port %u) in %s",
			(void *)lsp, (unsigned)lsp->id, command ? command : "(no command)", port, root_dir);
	
	str_hash_table_create(&lsp->document_ids, sizeof(u32));
	lsp->command = str_dup(command);
	if (configuration && *configuration)
		lsp->configuration_to_send = str_dup(configuration);
	lsp->quit_sem = SDL_CreateSemaphore(0);	
	lsp->error_mutex = SDL_CreateMutex();
	lsp->messages_mutex = SDL_CreateMutex();
	
	// document ID 0 is reserved
	LSPDocumentID zero_id = lsp_document_id(lsp, "");
	(void)zero_id;
	assert(zero_id == 0);
	
	arr_add(lsp->workspace_folders, lsp_document_id(lsp, root_dir));
	lsp->workspace_folders_mutex = SDL_CreateMutex();
	
	if (command) {
		ProcessSettings settings = {
			.separate_stderr = true,
			.working_directory = root_dir,
		};
		lsp->process = process_run_ex(command, &settings);
		const char *error = lsp->process ? process_geterr(lsp->process) : NULL;
		if (error) {
			// don't show an error box if the server is not installed
			#if _WIN32
				if (strstr(error, " 2)")) {
					if (lsp->log) fprintf(lsp->log, "Couldn't start LSP server %s: file not found.", command);
					debug_println("error: %s", error);
				} else
			#endif
				lsp_set_error(lsp, "Couldn't start LSP server: %s", error);
			lsp->exited = true;
			process_kill(&lsp->process);
			return lsp;
		}
	}
	
	lsp->communication_thread = SDL_CreateThread(lsp_communication_thread, "LSP communicate", lsp);
	return lsp;
}

bool lsp_try_add_root_dir(LSP *lsp, const char *new_root_dir) {
	assert(lsp->initialized);

	bool got_it = false;
	SDL_LockMutex(lsp->workspace_folders_mutex);
		arr_foreach_ptr(lsp->workspace_folders, LSPDocumentID, folder) {
			if (str_has_path_prefix(new_root_dir, lsp_document_path(lsp, *folder))) {
				got_it = true;
				break;
			}
		}
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	if (got_it) return true;
	
	if (!lsp->capabilities.workspace_folders_support) {
		return false;
	}
	
	// send workspace/didChangeWorkspaceFolders notification
	LSPRequest req = {.type = LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS};
	LSPRequestDidChangeWorkspaceFolders *w = &req.data.change_workspace_folders;
	LSPDocumentID document_id = lsp_document_id(lsp, new_root_dir);
	arr_add(w->added, document_id);
	lsp_send_request(lsp, &req);
	// *technically* this is incorrect because if the server *just now sent* a
	// workspace/workspaceFolders request, we'd give it back inconsistent information.
	// i don't care.
	SDL_LockMutex(lsp->workspace_folders_mutex);
		arr_add(lsp->workspace_folders, document_id);
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	return true;
}

bool lsp_next_message(LSP *lsp, LSPMessage *message) {
	bool any = false;
	SDL_LockMutex(lsp->messages_mutex);
	if (arr_len(lsp->messages_server2client)) {
		*message = lsp->messages_server2client[0];
		arr_remove(lsp->messages_server2client, 0);
		any = true;
	}
	SDL_UnlockMutex(lsp->messages_mutex);
	return any;
}

void lsp_free(LSP *lsp) {
	SDL_SemPost(lsp->quit_sem);
	if (lsp->communication_thread)
		SDL_WaitThread(lsp->communication_thread, NULL);
	SDL_DestroyMutex(lsp->messages_mutex);
	SDL_DestroyMutex(lsp->workspace_folders_mutex);
	SDL_DestroyMutex(lsp->error_mutex);
	SDL_DestroySemaphore(lsp->quit_sem);
	process_kill(&lsp->process);
	socket_close(&lsp->socket);
	arr_free(lsp->received_data);
	
	str_hash_table_clear(&lsp->document_ids);
	for (size_t i = 0; i < arr_len(lsp->document_data); ++i)
		free(lsp->document_data[i].path);
	arr_free(lsp->document_data);
	
	arr_foreach_ptr(lsp->messages_server2client, LSPMessage, message)
		lsp_message_free(message);
	arr_free(lsp->messages_server2client);
	
	arr_foreach_ptr(lsp->messages_client2server, LSPMessage, message)
		lsp_message_free(message);
	arr_free(lsp->messages_client2server);
	
	arr_foreach_ptr(lsp->requests_sent, LSPRequest, r)
		lsp_request_free(r);
	arr_free(lsp->requests_sent);
	
	arr_free(lsp->workspace_folders);
	
	arr_free(lsp->completion_trigger_chars);
	arr_free(lsp->signature_help_trigger_chars);
	arr_free(lsp->signature_help_retrigger_chars);
	free(lsp->command);
	free(lsp->configuration_to_send);
	memset(lsp, 0, sizeof *lsp);
	free(lsp);
}

int lsp_position_cmp(LSPPosition a, LSPPosition b) {
	if (a.line < b.line)
		return -1;
	if (a.line > b.line)
		return 1;
	if (a.character < b.character)
		return -1;
	if (a.character > b.character)
		return 1;
	return 0;
}

bool lsp_position_eq(LSPPosition a, LSPPosition b) {
	return a.line == b.line && a.character == b.character;
}

bool lsp_ranges_overlap(LSPRange a, LSPRange b) {
	if (lsp_position_cmp(a.end, b.start) <= 0)
		return false;
	if (lsp_position_cmp(b.end, a.start) <= 0)
		return false;
	return true;
}

bool lsp_document_position_eq(LSPDocumentPosition a, LSPDocumentPosition b) {
	return a.document == b.document && lsp_position_eq(a.pos, b.pos);
}


LSPDocumentPosition lsp_location_start_position(LSPLocation location) {
	return (LSPDocumentPosition) {
		.document = location.document,
		.pos = location.range.start
	};
}

LSPDocumentPosition lsp_location_end_position(LSPLocation location) {
	return (LSPDocumentPosition) {
		.document = location.document,
		.pos = location.range.end
	};
}

const uint32_t *lsp_completion_trigger_chars(LSP *lsp) {
	return lsp->completion_trigger_chars;
}

const uint32_t *lsp_signature_help_trigger_chars(LSP *lsp) {
	return lsp->signature_help_trigger_chars;
}

const uint32_t *lsp_signature_help_retrigger_chars(LSP *lsp) {
	return lsp->signature_help_retrigger_chars;
}

bool lsp_covers_path(LSP *lsp, const char *path) {
	bool ret = false;
	SDL_LockMutex(lsp->workspace_folders_mutex);
	arr_foreach_ptr(lsp->workspace_folders, LSPDocumentID, folder) {
		if (str_has_path_prefix(path, lsp_document_path(lsp, *folder))) {
			ret = true;
			break;
		}
	}
	SDL_UnlockMutex(lsp->workspace_folders_mutex);
	return ret;
}

void lsp_cancel_request(LSP *lsp, LSPRequestID id) {
	if (!id) return;
	if (!lsp) return;
	bool sent = false;
	SDL_LockMutex(lsp->messages_mutex);
		for (u32 i = 0; i < arr_len(lsp->requests_sent); ++i) {
			LSPRequest *req = &lsp->requests_sent[i];
			if (req->id == id) {
				// we sent this request but haven't received a response
				sent = true;
				arr_remove(lsp->requests_sent, i);
				break;
			}
		}
		
		for (u32 i = 0; i < arr_len(lsp->messages_client2server); ++i) {
			LSPMessage *message = &lsp->messages_client2server[i];
			if (message->type == LSP_REQUEST && message->request.id == id) {
				// we haven't sent this request yet
				arr_remove(lsp->messages_client2server, i);
				break;
			}
		
		}
	SDL_UnlockMutex(lsp->messages_mutex);
	if (sent) {
		LSPRequest request = {.type = LSP_REQUEST_CANCEL};
		request.data.cancel.id = id;
		lsp_send_request(lsp, &request);
	}
}

bool lsp_has_exited(LSP *lsp) {
	return lsp->exited;
}

bool lsp_is_initialized(LSP *lsp) {
	return lsp->initialized;
}

bool lsp_has_incremental_sync_support(LSP *lsp) {
	return lsp->capabilities.incremental_sync_support;
}

const char *lsp_get_command(LSP *lsp) {
	return lsp->command;
}

u16 lsp_get_port(LSP *lsp) {
	return lsp->port;
}

void lsp_quit(void) {
	if (request_id_mutex) {
		SDL_DestroyMutex(request_id_mutex);
		request_id_mutex = NULL;
	}
	lsp_write_quit();
}
