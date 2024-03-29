// deals with textDocument/signatureHelp LSP requests
// this is the little thing which shows you the signature of the function and the current argument

#include "ted-internal.h"

/// a single signature in the signature help.
typedef struct {
	/// displayed normal
	char *label_pre;
	/// displayed bold
	char *label_active;
	/// displayed normal
	char *label_post;
} Signature;

struct SignatureHelp {
	LSPServerRequestID last_request;
	/// should we resend a signature help request this frame?
	bool retrigger;
	/// if signature_count = 0, signature help is closed
	u16 signature_count;
	Signature signatures[SIGNATURE_HELP_MAX];
};

void signature_help_init(Ted *ted) {
	ted->signature_help = calloc(1, sizeof *ted->signature_help);
}

static void signature_help_clear(SignatureHelp *help) {
	for (int i = 0; i < help->signature_count; ++i) {
		Signature sig = help->signatures[i];
		free(sig.label_pre);
		free(sig.label_active);
		free(sig.label_post);
	}
	memset(help->signatures, 0, sizeof help->signatures);
	help->signature_count = 0;
}

static void signature_help_send_request(Ted *ted) {
	SignatureHelp *help = ted->signature_help;
	const Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help_enabled) {
		signature_help_clear(help);
		return;
	}
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) {
		signature_help_clear(help);
		return;
	}
	LSP *lsp = buffer_lsp(buffer);
	if (!lsp) {
		signature_help_clear(help);
		return;
	}
	LSPRequest request  = {.type = LSP_REQUEST_SIGNATURE_HELP};
	LSPRequestSignatureHelp *s = &request.data.signature_help;
	s->position = buffer_cursor_pos_as_lsp_document_position(buffer);
	ted_cancel_lsp_request(ted, &help->last_request);
	help->last_request = lsp_send_request(lsp, &request);
	help->retrigger = false;
}

void signature_help_retrigger(Ted *ted) {
	// don't just send the request here -- we don't want to send more than
	// one request per frame.
	ted->signature_help->retrigger = true;
}

void signature_help_open(Ted *ted, uint32_t trigger) {
	(void)trigger; // for now we don't send context
	signature_help_send_request(ted);
}

bool signature_help_is_open(Ted *ted) {
	return ted->signature_help->signature_count > 0;
}


void signature_help_close(Ted *ted) {
	SignatureHelp *help = ted->signature_help;
	signature_help_clear(help);
	ted_cancel_lsp_request(ted, &help->last_request);
}

void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response) {
	SignatureHelp *help = ted->signature_help;
	const Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help_enabled) return;
	
	if (response->request.type != LSP_REQUEST_SIGNATURE_HELP)
		return;
	if (response->request.id != help->last_request.id) {
		// stale request
		return;
	}
	help->last_request.id = 0;
	
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
	const Settings *settings = ted_active_settings(ted);
	if (!settings->signature_help_enabled)
		return;
	
	SignatureHelp *help = ted->signature_help;
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
	
	float border = settings->border_thickness;
	
	Rect buf_rect = buffer_rect(buffer);
	float width = buf_rect.size.x;
	float height = FLT_MAX;
	const float char_height = text_font_char_height(font);
	// make sure signature help doesn't take up too much space
	while (1) {
		height = char_height * signature_count;
		if (height < buffer_rect(buffer).size.y * 0.25f)
			break;
		--signature_count;
		if (signature_count == 0) return;
	}
	float x = buf_rect.pos.x, y;
	vec2 cursor_pos = buffer_pos_to_pixels(buffer, buffer_cursor_pos(buffer));
	if (cursor_pos.y < rect_ymid(buffer_rect(buffer))) {
		// signature help on bottom
		y = rect_y2(buf_rect) - height;
		gl_geometry_rect(rect_xywh(x, y - border, width, border),
			settings_color(settings, COLOR_AUTOCOMPLETE_BORDER));
	} else {
		// signature help on top
		y = rect_y1(buf_rect);
		gl_geometry_rect(rect_xywh(x, y + height + 1 - border, width, border),
			settings_color(settings, COLOR_AUTOCOMPLETE_BORDER));
	}
	gl_geometry_rect(rect_xywh(x, y, width, height),
		settings_color(settings, COLOR_AUTOCOMPLETE_BG));
	
	// draw the signatures
	for (int s = 0; s < signature_count; ++s) {
		const Signature *signature = &help->signatures[s];
		TextRenderState state = text_render_state_default;
		state.x = x;
		state.y = y;
		state.min_x = x;
		state.min_y = y;
		state.max_x = rect_x2(buf_rect);
		state.max_y = rect_y2(buf_rect);
		settings_color_floats(settings, COLOR_TEXT, state.color);
		
		text_utf8_with_state(font, &state, signature->label_pre);
		text_utf8_with_state(font_bold, &state, signature->label_active);
		text_utf8_with_state(font, &state, signature->label_post);
		y += char_height;
	}
	
	gl_geometry_draw();
	text_render(font);
	text_render(font_bold);
}

void signature_help_quit(Ted *ted) {
	signature_help_close(ted);
	free(ted->signature_help);
	ted->signature_help = NULL;
}
