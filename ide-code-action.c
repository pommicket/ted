#include "ted-internal.h"

static bool ranges_touch(BufferPos p1, BufferPos p2, BufferPos q1, BufferPos q2) {
	int cmp21 = buffer_pos_cmp(p2, q1);
	if (cmp21 < 0) {
		// p is entirely before q
		return false;
	}
	int cmp12 = buffer_pos_cmp(p1, q2);
	if (cmp12 > 0) {
		// p is entirely after q
		return false;
	}
	return true;
}

void code_action_start(Ted *ted) {
	TextBuffer *buffer = ted_active_buffer(ted);
	LSP *lsp = buffer_lsp(buffer);
	BufferPos range_start = {0}, range_end = {0};
	LSPRange range = {0};
	if (buffer_selection_pos(buffer, &range_start)) {
		range = buffer_selection_as_lsp_range(buffer);
		range_end = buffer_cursor_pos(buffer);
		if (buffer_pos_cmp(range_start, range_end) > 0) {
			BufferPos tmp = range_start;
			range_start = range_end;
			range_end = tmp;
		}
	} else {
		range_start = range_end = buffer_cursor_pos(buffer);
		range.start = range.end = buffer_cursor_pos_as_lsp_position(buffer);
	}
	LSPRequest req = {
		.type = LSP_REQUEST_CODE_ACTION,
	};
	LSPRequestCodeAction *code_action_req = &req.data.code_action;
	code_action_req->document = buffer_lsp_document_id(buffer);
	code_action_req->range = range;
	arr_foreach_ptr(buffer_diagnostics(buffer), const Diagnostic, d) {
		if (ranges_touch(range_start, range_end, d->pos, d->end)) {
			LSPString raw = lsp_request_add_string(&req, d->raw);
			arr_add(code_action_req->raw_diagnostics, raw);
		}
	}
	lsp_send_request(lsp, &req);
}
