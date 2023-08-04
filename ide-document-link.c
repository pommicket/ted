#include "ted.h"

void document_link_frame(Ted *ted) {
	DocumentLink *document_link = &ted->document_link;
	
	bool key_down = ted_is_ctrl_down(ted);
	if (!key_down) {
		document_link->key_press_time = 0.0;
	} else if (document_link->key_press_time == 0.0) {
		document_link->key_press_time = ted->frame_time;
	}
	
	bool show_links = document_link->key_press_time != 0.0
		&& ted->frame_time - document_link->key_press_time > 1.0;
	
	if (!show_links) {
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
	if (response->request.type != LSP_REQUEST_DOCUMENT_LINK)
		return;
	
	(void)ted;//TODO
	const LSPResponseDocumentLink *document_link = &response->data.document_link;
	
	arr_foreach_ptr(document_link->links, const LSPDocumentLink, link) {
		printf("target: %s\n",  lsp_response_string(response, link->target));
		
	}
}
