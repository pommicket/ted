// deals with textDocument/signatureHelp LSP requests

void signature_help_send_request(Ted *ted) {
	Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help) return;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) return;
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) return;
	LSPRequest request  = {.type = LSP_REQUEST_SIGNATURE_HELP};
	LSPRequestSignatureHelp *s = &request.data.signature_help;
	s->position = buffer_cursor_pos_as_lsp_document_position(buffer);
	lsp_send_request(lsp, &request);
	ted->signature_help.retrigger = false;
}

void signature_help_retrigger(Ted *ted) {
	// don't just send the request here -- we don't want to send more than
	// one request per frame.
	ted->signature_help.retrigger = true;
}

void signature_help_open(Ted *ted, char32_t trigger) {
	(void)trigger; // for now we don't send context
	signature_help_send_request(ted);
}

bool signature_help_is_open(Ted *ted) {
	return ted->signature_help.signature_count > 0;
}

static void signature_help_clear(SignatureHelp *help) {
	for (int i = 0; i < help->signature_count; ++i) {
		Signature sig = help->signatures[i];
		free(sig.label_pre);
		free(sig.label_active);
		free(sig.label_post);
	}
	memset(help->signatures, 0, sizeof help->signatures);
}

void signature_help_close(Ted *ted) {
	signature_help_clear(&ted->signature_help);
}

void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response) {
	Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help) return;
	
	if (response->request.type != LSP_REQUEST_SIGNATURE_HELP)
		return;
	SignatureHelp *help = &ted->signature_help;
	const LSPResponseSignatureHelp *lsp_help = &response->data.signature_help;
	u32 signature_count = arr_len(lsp_help->signatures);
	if (signature_count > SIGNATURE_HELP_MAX)
		signature_count = SIGNATURE_HELP_MAX;
	
	signature_help_clear(help);
	for (u32 s = 0; s < signature_count; ++s) {
		Signature *signature = &help->signatures[s];
		LSPSignatureInformation *lsp_signature = &lsp_help->signatures[s];
		
		const char *label = lsp_response_string(response, lsp_signature->label);
		size_t start = unicode_utf16_to_utf8_offset(label, lsp_signature->active_start);
		size_t end   = unicode_utf16_to_utf8_offset(label, lsp_signature->active_end);
		if (start == (size_t)-1) {
			assert(0);
			start = 0;
		}
		if (end == (size_t)-1) {
			assert(0);
			end = 0;
		}
		u32 active_start = (u32)start;
		u32 active_end = (u32)end;
		signature->label_pre = strn_dup(label, active_start);
		signature->label_active = strn_dup(label + active_start, active_end - active_start);
		signature->label_post = str_dup(label + active_end);
	}
	
	help->signature_count = (u16)signature_count;
}

void signature_help_frame(Ted *ted) {
	Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help)
		return;
	
	SignatureHelp *help = &ted->signature_help;
	if (help->retrigger)
		signature_help_send_request(ted);
	u16 signature_count = help->signature_count;
	if (!signature_count)
		return;
	Font *font = ted->font;
	Font *font_bold = ted->font_bold;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	
	u32 *colors = settings->colors;
	float border = settings->border_thickness;
	
	float width = buffer->x2 - buffer->x1;
	float height = FLT_MAX;
	// make sure signature help doesn't take up too much space
	while (1) {
		height = font->char_height * signature_count;
		if (height < (buffer->y2 - buffer->y1) * 0.25f)
			break;
		--signature_count;
		if (signature_count == 0) return;
	}
	float x = buffer->x1, y = buffer->y2 - height;
	gl_geometry_rect(rect_xywh(x, y - border, width, border),
		colors[COLOR_AUTOCOMPLETE_BORDER]);
	gl_geometry_rect(rect_xywh(x, y, width, height),
		colors[COLOR_AUTOCOMPLETE_BG]);
	
	// draw the signatures
	for (int s = 0; s < signature_count; ++s) {
		const Signature *signature = &help->signatures[s];
		TextRenderState state = text_render_state_default;
		state.x = x;
		state.y = y;
		state.min_x = x;
		state.min_y = y;
		state.max_x = buffer->x2;
		state.max_y = buffer->y2;
		rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
		
		text_utf8_with_state(font, &state, signature->label_pre);
		text_utf8_with_state(font_bold, &state, signature->label_active);
		text_utf8_with_state(font, &state, signature->label_post);
		y += font->char_height;
	}
	
	gl_geometry_draw();
	text_render(font);
	text_render(font_bold);
}
