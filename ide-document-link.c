#include "ted.h"

typedef struct DocumentLink DocumentLink;
struct DocumentLink {
	char *target;
	char *tooltip;
	BufferPos start;
	BufferPos end;
};
struct DocumentLinks {
	LSPDocumentID requested_document;
	LSPServerRequestID last_request;
	DocumentLink *links;
};

void document_link_init(Ted *ted) {
	ted->document_links = calloc(1, sizeof *ted->document_links);
}

void document_link_quit(Ted *ted) {
	document_link_clear(ted);
	free(ted->document_links);
	ted->document_links = NULL;
}

void document_link_clear(Ted *ted) {
	DocumentLinks *dl = ted->document_links;
	arr_foreach_ptr(dl->links, DocumentLink, l) {
		free(l->target);
		free(l->tooltip);
	}
	arr_clear(dl->links);
	dl->requested_document = 0;
}

static bool document_link_activation_key_down(Ted *ted) {
	return ted_is_ctrl_down(ted);
}


static Rect document_link_get_rect(Ted *ted, DocumentLink *link) {
	TextBuffer *buffer = ted->active_buffer;
	DocumentLinks *dl = ted->document_links;
	if (buffer_lsp_document_id(buffer) != dl->requested_document) {
		return (Rect){0};
	}
	
	vec2 a = buffer_pos_to_pixels(buffer, link->start);
	vec2 b = buffer_pos_to_pixels(buffer, link->end);
	if (a.y != b.y) {
		// multi-line link. let's ignore it because it'd be tough to deal with.
		return (Rect){0};
	}
	
	if (a.x > b.x) {
		// swap positions
		vec2 temp = a;
		a = b;
		b = temp;
	}
	
	float y0 = a.y;
	float char_height = text_font_char_height(buffer_font(buffer));
	return (Rect) {
		.pos = {a.x, y0},
		.size = {b.x - a.x, char_height}
	};
}

void document_link_frame(Ted *ted) {
	Settings *settings = ted_active_settings(ted);
	if (!settings->document_links) {
		document_link_clear(ted);
		return;
	}
	DocumentLinks *dl = ted->document_links;
	
	bool key_down = document_link_activation_key_down(ted);
	if (!key_down) {
		ted_cancel_lsp_request(ted, &dl->last_request);
		document_link_clear(ted);
		return;
	}
	
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp)
		return;
	
	if (!dl->last_request.id) {
		// send the request
		LSPRequest request = {.type = LSP_REQUEST_DOCUMENT_LINK};
		LSPRequestDocumentLink *lnk = &request.data.document_link;
		lnk->document = buffer_lsp_document_id(buffer);
		dl->last_request = lsp_send_request(lsp, &request);
		dl->requested_document = lnk->document;
	}
	
	arr_foreach_ptr(dl->links, DocumentLink, l) {
		Rect r = document_link_get_rect(ted, l);
		if (rect_contains_point(r, ted->mouse_pos)) {
			ted->cursor = ted->cursor_hand;
		}
	}
}

void document_link_process_lsp_response(Ted *ted, const LSPResponse *response) {
	DocumentLinks *dl = ted->document_links;
	if (response->request.type != LSP_REQUEST_DOCUMENT_LINK
		|| response->request.id != dl->last_request.id)
		return;
	if (!dl->last_request.id)
		return; // request was cancelled
	
	bool key_down = document_link_activation_key_down(ted);
	if (!key_down)
		return;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	if (buffer_lsp_document_id(buffer) != dl->requested_document)
		return; // request was for a different document
	
	const LSPResponseDocumentLink *response_data = &response->data.document_link;
	arr_foreach_ptr(response_data->links, const LSPDocumentLink, link) {
		DocumentLink *l = arr_addp(dl->links);
		l->start = buffer_pos_from_lsp(buffer, link->range.start);
		l->end = buffer_pos_from_lsp(buffer, link->range.end);
		l->target = str_dup(lsp_response_string(response, link->target));
		const char *tooltip = lsp_response_string(response, link->tooltip);
		l->tooltip = *tooltip ? str_dup(tooltip) : NULL;
	}
}

const char *document_link_at_buffer_pos(Ted *ted, BufferPos pos) {
	DocumentLinks *dl = ted->document_links;
	TextBuffer *buffer = ted->active_buffer;
	if (buffer_lsp_document_id(buffer) != dl->requested_document) {
		return NULL;
	}

	arr_foreach_ptr(dl->links, DocumentLink, l) {
		if (buffer_pos_cmp(pos, l->start) >= 0 && buffer_pos_cmp(pos, l->end) < 0)
			return l->target;
	}
	return NULL;
}
