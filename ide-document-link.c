#include "ted.h"

void document_link_frame(Ted *ted) {
	DocumentLink *document_link = &ted->document_link;
	
	bool key_down = ted_is_ctrl_down(ted);
	if (!key_down) {
		ted_cancel_lsp_request(ted, &document_link->last_request);
		return;
	}
	
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp)
		return;
	
	if (!document_link->last_request.id) {
		// send the request
		LSPRequest request = {.type = LSP_REQUEST_DOCUMENT_LINK};
		LSPRequestDocumentLink *lnk = &request.data.document_link;
		lnk->document = buffer_lsp_document_id(buffer);
		document_link->last_request = lsp_send_request(lsp, &request);
	}
}

void document_link_process_lsp_response(Ted *ted, const LSPResponse *response) {
	DocumentLink *document_link = &ted->document_link;
	if (response->request.type != LSP_REQUEST_DOCUMENT_LINK)
		return;
	if (!document_link->last_request.id)
		return; // request was cancelled
	
	bool key_down = ted_is_ctrl_down(ted);
	if (!key_down)
		return;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	if (buffer_lsp_document_id(buffer) != response->request.data.document_link.document)
		return; // request was for a different document
	
	const LSPResponseDocumentLink *response_data = &response->data.document_link;
	arr_foreach_ptr(response_data->links, const LSPDocumentLink, link) {
		BufferPos start = buffer_pos_from_lsp(buffer, link->range.start);
		BufferPos end = buffer_pos_from_lsp(buffer, link->range.end);
		printf("%d:%d — %d:%d\t: %s\n",
			start.line, start.index, end.line, end.index,
			lsp_response_string(response, link->target));
		
	}
}
