#include "ted-internal.h"

struct Formatting {
	LSPServerRequestID last_request_id;
};

void format_init(Ted *ted) {
	ted->formatting = calloc(1, sizeof (Formatting));
}

static void format_common(Ted *ted, bool selection) {
	Formatting *formatting = ted->formatting;
	ted_cancel_lsp_request(ted, &formatting->last_request_id);
	TextBuffer *buffer = ted_active_buffer(ted);
	if (!buffer) return;
	if (selection && !buffer_has_selection(buffer)) return;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	Settings *settings = buffer_settings(buffer);
	LSPRequest request = {
		.type = selection ? LSP_REQUEST_RANGE_FORMATTING : LSP_REQUEST_FORMATTING
	};
	LSPRequestFormatting *req_data = &request.data.formatting;
	req_data->document = buffer_lsp_document_id(buffer);
	req_data->indent_with_spaces = settings->indent_with_spaces;
	req_data->tab_width = settings->tab_width;
	if (selection) {
		req_data->use_range = true;
		req_data->range = buffer_selection_as_lsp_range(buffer);
	}
	formatting->last_request_id = lsp_send_request(lsp, &request);
}

void format_selection(Ted *ted) {
	format_common(ted, true);
}

void format_file(Ted *ted) {
	format_common(ted, false);
}

void format_cancel_request(Ted *ted) {
	Formatting *formatting = ted->formatting;
	ted_cancel_lsp_request(ted, &formatting->last_request_id);
}

void format_process_lsp_response(Ted *ted, const LSPResponse *response) {
	Formatting *formatting = ted->formatting;
	const LSPRequest *request = &response->request;
	if (request->id != formatting->last_request_id.id)
		return;
	if (!(request->type == LSP_REQUEST_RANGE_FORMATTING
		|| request->type == LSP_REQUEST_FORMATTING)) {
		return;
	}
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) return;
	if (buffer_lsp_document_id(buffer) != request->data.formatting.document)
		return; // switched document

	buffer_deselect(buffer);
	const LSPResponseFormatting *f = &response->data.formatting;
	buffer_start_edit_chain(buffer);
	buffer_apply_lsp_text_edits(buffer, response, f->edits, arr_len(f->edits));
	buffer_end_edit_chain(buffer);
}

void format_quit(Ted *ted) {
	free(ted->formatting);
	ted->formatting = NULL;
}
