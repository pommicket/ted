#include "ted-internal.h"

void code_action_start(Ted *ted) {
	TextBuffer *buffer = ted_active_buffer(ted);
	LSP *lsp = buffer_lsp(buffer);
	LSPRange range = {0};
	if (buffer_has_selection(buffer))
		range = buffer_selection_as_lsp_range(buffer);
	else
		range.start = range.end = buffer_cursor_pos_as_lsp_position(buffer);
	LSPRequest req = {
		.type = LSP_REQUEST_CODE_ACTION,
	};
	LSPRequestCodeAction *code_action_req = &req.data.code_action;
	code_action_req->document = buffer_lsp_document_id(buffer);
	code_action_req->range = range;
	lsp_send_request(lsp, &req);
}
