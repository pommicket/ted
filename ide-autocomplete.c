// auto-completion for ted

#include "ted-internal.h"

#define TAGS_MAX_COMPLETIONS 200 // max # of tag completions to scroll through
#define AUTOCOMPLETE_NCOMPLETIONS_VISIBLE 10 // max # of completions to show at once

/// a single autocompletion suggestion
typedef struct Autocompletion Autocompletion;

struct Autocompletion {
	char *label;
	char *filter;
	char *text;
	/// this can be NULL!
	char *detail;
	/// this can be NULL!
	char *documentation;
	bool deprecated;
	SymbolKind kind; 
};

struct Autocomplete {
	/// is the autocomplete box open?
	bool open;
	/// should the completions array be updated when more characters are typed?
	bool is_list_complete;
	
	/// what trigger caused the last request for completions:
	/// either a character code (for trigger characters),
	/// or one of the `TRIGGER_*` constants above
	uint32_t trigger;
	
	LSPServerRequestID last_request;
	/// when we sent the request to the LSP for completions
	///  (this is used to figure out when we should display "Loading...")
	double last_request_time;
	
	/// dynamic array of all completions
	Autocompletion *completions;
	/// dynamic array of completions to be suggested (indices into completions)
	u32 *suggested;
	/// position of cursor last time completions were generated. if this changes, we need to recompute completions.
	BufferPos last_pos; 
	/// which completion is currently selected (index into suggested)
	i32 cursor;
	i32 scroll;
	
	/// was the last request for phantom completion?
	bool last_request_phantom;
	/// current phantom completion to be displayed
	char *phantom;
	/// rectangle where the autocomplete menu is (needed to avoid interpreting autocomplete clicks as other clicks)
	Rect rect;
};

void autocomplete_init(Ted *ted) {
	ted->autocomplete = calloc(1, sizeof *ted->autocomplete);
}

bool autocomplete_is_open(Ted *ted) {
	return ted->autocomplete->open;
}

bool autocomplete_has_phantom(Ted *ted) {
	return ted->autocomplete->phantom != NULL;

}

bool autocomplete_box_contains_point(Ted *ted, vec2 point) {
	return rect_contains_point(ted->autocomplete->rect, point);
}

static void autocomplete_clear_completions(Autocomplete *ac) {
	arr_foreach_ptr(ac->completions, Autocompletion, completion) {
		free(completion->label);
		free(completion->text);
		free(completion->filter);
		free(completion->detail);
		free(completion->documentation);
	}
	arr_clear(ac->completions);
	arr_clear(ac->suggested);
}

static void autocomplete_clear_phantom(Autocomplete *ac) {
	free(ac->phantom);
	ac->phantom = NULL;
}

// should a phantom completion be displayed?
static bool autocomplete_should_display_phantom(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	TextBuffer *buffer = ted->active_buffer;
	bool show = !ac->open
		&& buffer
		&& !buffer_is_view_only(buffer)
		&& !buffer_is_line_buffer(buffer)
		&& buffer_settings(buffer)->phantom_completions
		&& is32_word(buffer_char_before_cursor(buffer))
		&& !is32_word(buffer_char_at_cursor(buffer));
	if (!show)
		autocomplete_clear_phantom(ac);
	return show;
}

// do the actual completion
static void autocomplete_complete(Ted *ted, Autocompletion completion) {
	TextBuffer *buffer = ted->active_buffer;
	buffer_start_edit_chain(buffer); // don't merge with other edits
	if (is32_word(buffer_char_before_cursor(buffer)))
		buffer_backspace_words_at_cursor(buffer, 1); // delete whatever text was already typed
	buffer_insert_utf8_at_cursor(buffer, completion.text);
	buffer_end_edit_chain(buffer);
	autocomplete_close(ted);
}

void autocomplete_select_completion(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	if (ac->open) {
		size_t nsuggestions = arr_len(ac->suggested);
		if (nsuggestions) {
			i64 cursor = mod_i64(ac->cursor, (i64)nsuggestions);
			autocomplete_complete(ted, ac->completions[ac->suggested[cursor]]);
		}
	} else if (ac->phantom) {
		Autocompletion fake_completion = {
			.text = ac->phantom
		};
		autocomplete_complete(ted, fake_completion);
	}
}

static void autocomplete_correct_scroll(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	i32 scroll = ac->scroll;
	scroll = min_i32(scroll, (i32)arr_len(ac->suggested) - AUTOCOMPLETE_NCOMPLETIONS_VISIBLE);
	scroll = max_i32(scroll, 0);
	ac->scroll = scroll;
}

void autocomplete_scroll(Ted *ted, i32 by) {
	Autocomplete *ac = ted->autocomplete;
	ac->scroll += by;
	autocomplete_correct_scroll(ted);
}

static void autocomplete_move_cursor(Ted *ted, i32 by) {
	Autocomplete *ac = ted->autocomplete;
	u32 ncompletions = arr_len(ac->suggested);
	if (ncompletions == 0)
		return;
	i32 cursor = ac->cursor;
	cursor += by;
	cursor = (i32)mod_i32(cursor, (i32)ncompletions);
	ac->cursor = cursor;
	ac->scroll = ac->cursor - AUTOCOMPLETE_NCOMPLETIONS_VISIBLE / 2;
	autocomplete_correct_scroll(ted);
}

void autocomplete_next(Ted *ted) {
	autocomplete_move_cursor(ted, 1);
}

void autocomplete_prev(Ted *ted) {
	autocomplete_move_cursor(ted, -1);
}

void autocomplete_close(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	ac->open = false;
	autocomplete_clear_phantom(ac);
	autocomplete_clear_completions(ac);
	ted_cancel_lsp_request(ted, &ac->last_request);
}

static void autocomplete_update_suggested(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	arr_clear(ac->suggested);
	char *word = buffer_word_at_cursor_utf8(ted->active_buffer);
	for (u32 i = 0; i < arr_len(ac->completions); ++i) {
		Autocompletion *completion = &ac->completions[i];
		if (str_has_prefix(completion->filter, word))
			arr_add(ac->suggested, i); // suggest this one
	}
	free(word);
}

static bool autocomplete_using_lsp(Ted *ted) {
	return ted->active_buffer && buffer_lsp(ted->active_buffer) != NULL;
}

static void autocomplete_no_suggestions(Ted *ted) {
	Autocomplete *ac = ted->autocomplete;
	if (ac->trigger == TRIGGER_INVOKED)
		ted_flash_error_cursor(ted);
	autocomplete_close(ted);
}

static void autocomplete_send_completion_request(Ted *ted, TextBuffer *buffer, BufferPos pos, uint32_t trigger, bool phantom) {
	if (!buffer_is_named_file(buffer))
		return; // no can do
	
	LSP *lsp = buffer_lsp(buffer);
	Autocomplete *ac = ted->autocomplete;

	ted_cancel_lsp_request(ted, &ac->last_request);

	LSPRequest request = {
		.type = LSP_REQUEST_COMPLETION
	};
	
	LSPCompletionTriggerKind lsp_trigger = LSP_TRIGGER_CHARACTER;
	switch (trigger) {
	case TRIGGER_INVOKED: lsp_trigger = LSP_TRIGGER_INVOKED; break;
	case TRIGGER_INCOMPLETE: lsp_trigger = LSP_TRIGGER_INCOMPLETE; break;
	}
	
	request.data.completion = (LSPRequestCompletion) {
		.position = {
			.document = buffer_lsp_document_id(buffer),
			.pos = buffer_pos_to_lsp_position(buffer, pos)
		},
		.context = {
			.trigger_kind = lsp_trigger,
			.trigger_character = {0},
		}
	};
	if (trigger < UNICODE_CODE_POINTS)
		unicode_utf32_to_utf8(request.data.completion.context.trigger_character, trigger);
	ac->last_request = lsp_send_request(lsp, &request);
	if (ac->last_request.id) {
		ac->last_request_time = ted->frame_time;
		ac->last_request_phantom = phantom;
		// *technically sepaking* this can mess things up if a complete
		// list arrives only after the user has typed some stuff
		// (in that case we'll send a TriggerKind = incomplete request even though it makes no sense).
		// but i don't think any servers will have a problem with that.
		ac->is_list_complete = false;
	}
}

static void autocomplete_find_completions(Ted *ted, uint32_t trigger, bool phantom) {
	Autocomplete *ac = ted->autocomplete;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	BufferPos pos = buffer_cursor_pos(buffer);
	if (buffer_pos_eq(pos, ac->last_pos))
		return; // no need to update completions.
	ac->trigger = trigger;
	ac->last_pos = pos;

	LSP *lsp = buffer_lsp(buffer);
	if (lsp) {
		if (ac->is_list_complete && trigger == TRIGGER_INCOMPLETE) {
			// the list of completions we got from the LSP server is complete,
			// so we just need to call autocomplete_update_suggested,
			// we don't need to send a new request.
		} else {
			autocomplete_send_completion_request(ted, buffer, pos, trigger, phantom);
		}
	} else {
		// tag completion
		autocomplete_clear_completions(ac);
		autocomplete_clear_phantom(ac);
		
		char *word_at_cursor = str32_to_utf8_cstr(buffer_word_at_cursor(buffer));
		if (phantom) {
			if (autocomplete_should_display_phantom(ted)) {
				char *completions[2] = {NULL, NULL};
				if (tags_beginning_with(ted, word_at_cursor, completions, 2, false) == 1) {
					// show phantom
					ac->phantom = completions[0];
					free(completions[1]);
				} else {
					free(completions[0]);
					free(completions[1]);
				}
			}
		} else {
			char **completions = calloc(TAGS_MAX_COMPLETIONS, sizeof *completions);
			u32 ncompletions = (u32)tags_beginning_with(ted, word_at_cursor, completions, TAGS_MAX_COMPLETIONS, true);
			
			arr_set_len(ac->completions, ncompletions);
			
			for (size_t i = 0; i < ncompletions; ++i) {
				ac->completions[i].label = completions[i];
				ac->completions[i].text = str_dup(completions[i]);
				ac->completions[i].filter = str_dup(completions[i]);
				arr_add(ac->suggested, (u32)i);
			}
			free(completions);
			
			// if we got the full list of tags beginning with `word_at_cursor`,
			// then we don't need to call `tags_beginning_with` again.
			ac->is_list_complete = ncompletions == TAGS_MAX_COMPLETIONS;
		}
		free(word_at_cursor);
	}
	
	autocomplete_update_suggested(ted);
}

static SymbolKind lsp_completion_kind_to_ted(LSPCompletionKind kind) {
	switch (kind) {
	case LSP_COMPLETION_TEXT:
	case LSP_COMPLETION_MODULE:
	case LSP_COMPLETION_UNIT:
	case LSP_COMPLETION_COLOR:
	case LSP_COMPLETION_FILE:
	case LSP_COMPLETION_REFERENCE:
	case LSP_COMPLETION_FOLDER:
	case LSP_COMPLETION_OPERATOR:
		return SYMBOL_OTHER;
	
	case LSP_COMPLETION_METHOD:
	case LSP_COMPLETION_FUNCTION:
	case LSP_COMPLETION_CONSTRUCTOR:
		return SYMBOL_FUNCTION;
	
	case LSP_COMPLETION_FIELD:
	case LSP_COMPLETION_PROPERTY:
		return SYMBOL_FIELD;
	
	case LSP_COMPLETION_VARIABLE:
		return SYMBOL_VARIABLE;
	
	case LSP_COMPLETION_CLASS:
	case LSP_COMPLETION_INTERFACE:
	case LSP_COMPLETION_ENUM:
	case LSP_COMPLETION_STRUCT:
	case LSP_COMPLETION_EVENT:
	case LSP_COMPLETION_TYPEPARAMETER:
		return SYMBOL_TYPE;
	
	case LSP_COMPLETION_VALUE:
	case LSP_COMPLETION_ENUMMEMBER:
	case LSP_COMPLETION_CONSTANT:
		return SYMBOL_CONSTANT;
	
	case LSP_COMPLETION_KEYWORD:
	case LSP_COMPLETION_SNIPPET:
		return SYMBOL_KEYWORD;
	}
	assert(0);
	return SYMBOL_OTHER;
}


void autocomplete_process_lsp_response(Ted *ted, const LSPResponse *response) {
	const LSPRequest *request = &response->request;
	if (request->type != LSP_REQUEST_COMPLETION)
		return;
	Autocomplete *ac = ted->autocomplete;
	if (request->id != ac->last_request.id)
		return; // old request
	ac->last_request.id = 0;
	if (!ac->open && !ac->last_request_phantom) {
		// user hit escape or down or something before completions arrived.
		return;
	}
	if (ac->open && ac->last_request_phantom) {
		// shouldn't be possible, since we should never request phantom completions while autocomplete is open
		assert(0);
		return;
	}
		
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer)
		return;
	
	const LSPResponseCompletion *completion = &response->data.completion;
	size_t ncompletions = arr_len(completion->items);
	if (ac->last_request_phantom) {
		if (!autocomplete_should_display_phantom(ted))
			return;
		
		// check for phantom completion
		// ideally we would just check if ncompletions == 1,
		// but some completions might not start with the word at the cursor,
		// and it's best to filter those out.
		char *word_at_cursor = buffer_word_at_cursor_utf8(buffer);
		if (*word_at_cursor) {
			int ncandidates = 0;
			const char *candidate = NULL;
			for (size_t i = 0; i < ncompletions; ++i) {
				const LSPCompletionItem *lsp_completion = &completion->items[i];
				const char *new_text = lsp_response_string(response, lsp_completion->text_edit.new_text);
				if (str_has_prefix(new_text, word_at_cursor) && (
					// ignore completions with duplicate text
					!candidate || !streq(candidate, new_text)
					)) {
					candidate = new_text;
					++ncandidates;
					if (ncandidates >= 2) break;
				}
			}
			
			// only show phantom if there is exactly 1 possible completion.
			if (ncandidates == 1) {
				free(ac->phantom);
				ac->phantom = str_dup(candidate);
			} else {
				autocomplete_clear_phantom(ac);
			}
		}
		free(word_at_cursor);
		return;
	}
	
	autocomplete_clear_completions(ac);
	arr_set_len(ac->completions, ncompletions);
	for (size_t i = 0; i < ncompletions; ++i) {
		const LSPCompletionItem *lsp_completion = &completion->items[i];
		Autocompletion *ted_completion = &ac->completions[i];
		ted_completion->label = str_dup(lsp_response_string(response, lsp_completion->label));
		ted_completion->filter = str_dup(lsp_response_string(response, lsp_completion->filter_text));
		// NOTE: here we don't deal with snippets.
		// right now we are sending "snippetSupport: false" in the capabilities,
		// so this should be okay.
		ted_completion->text = str_dup(lsp_response_string(response, lsp_completion->text_edit.new_text));
		const char *detail = lsp_response_string(response, lsp_completion->detail);
		ted_completion->detail = *detail ? str_dup(detail) : NULL;
		ted_completion->kind = lsp_completion_kind_to_ted(lsp_completion->kind);
		ted_completion->deprecated = lsp_completion->deprecated;
		const char *documentation = lsp_response_string(response, lsp_completion->documentation);
		ted_completion->documentation = *documentation ? str_dup(documentation) : NULL;
		
	}
	if (ac->last_request_phantom) {
		assert(ncompletions == 1);
		free(ac->phantom);
		ac->phantom = str_dup(ac->completions[0].text);
		autocomplete_clear_completions(ac);
		return;
	}
	
	ac->is_list_complete = completion->is_complete;
	
	autocomplete_update_suggested(ted);
	switch (arr_len(ac->suggested)) {
	case 0:
		autocomplete_no_suggestions(ted);
		return;
	case 1:
		// if autocomplete was invoked by Ctrl+Space, and there's only one completion, select it.
		if (ac->trigger == TRIGGER_INVOKED)
			autocomplete_complete(ted, ac->completions[ac->suggested[0]]);
		return;	
	}
}

void autocomplete_open(Ted *ted, uint32_t trigger) {
	Autocomplete *ac = ted->autocomplete;
	TextBuffer *buffer = ted->active_buffer;
	if (ac->open) return;
	if (!buffer) return;
	if (!buffer_is_named_file(buffer)) return;
	if (buffer_is_view_only(buffer)) return;
	autocomplete_clear_phantom(ac);
	const Settings *settings = buffer_settings(buffer);
	bool regenerated = false;
	
	find_completions:
	ted->cursor_error_time = 0;
	ac->last_pos = (BufferPos){U32_MAX,0};
	ac->cursor = 0;
	autocomplete_find_completions(ted, trigger, false);
	
	
	if (arr_len(ac->completions) == 0) {
		if (autocomplete_using_lsp(ted)) {
			ac->open = true;
		} else if (settings->regenerate_tags_if_not_found && !regenerated) {
			regenerated = true;
			tags_generate(ted, false);
			goto find_completions;
		} else { 
			autocomplete_no_suggestions(ted);
		}
		return;
	}
	
	bool multiple_completions = false;
	for (u32 i = 1; i < arr_len(ac->completions); ++i) {
		if (!streq(ac->completions[i].text, ac->completions[0].text)) {
			multiple_completions = true;
			break;
		}
	}
	
	if (!multiple_completions) {
		autocomplete_complete(ted, ac->completions[0]);
		// (^ this calls autocomplete_close)
		return;
	}
	
	// open autocomplete menu
	ac->open = true;
}

static void autocomplete_find_phantom(Ted *ted) {
	if (!autocomplete_should_display_phantom(ted))
		return;
	autocomplete_find_completions(ted, TRIGGER_INVOKED, true);
}

static char symbol_kind_icon(SymbolKind k) {
	switch (k) {
	case SYMBOL_FUNCTION:
		return 'f';
	case SYMBOL_FIELD:
		return 'm';
	case SYMBOL_TYPE:
		return 't';
	case SYMBOL_CONSTANT:
		return 'c';
	case SYMBOL_VARIABLE:
		return 'v';
	case SYMBOL_KEYWORD:
	case SYMBOL_OTHER:
		return ' ';
	}
	assert(0);
	return ' ';
}

void autocomplete_frame(Ted *ted) {
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer) return;
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	const Settings *settings = buffer_settings(buffer);
	const u32 *colors = settings->colors;
	const float padding = settings->padding;
	
	autocomplete_find_phantom(ted);
	
	Autocomplete *ac = ted->autocomplete;
	if (autocomplete_should_display_phantom(ted) && ac->phantom) {
		// display phantom completion
		char *word_at_cursor = buffer_word_at_cursor_utf8(buffer);
		if (*word_at_cursor && str_has_prefix(ac->phantom, word_at_cursor)) {
			const char *completion = ac->phantom + strlen(word_at_cursor);
			if (*completion) {
				vec2 pos = buffer_pos_to_pixels(buffer, buffer_cursor_pos(buffer));
				#if 0
				vec2 size = text_get_size_vec2(font, completion);
				// this makes the text below the phantom less visible.
				// doesn't look very good, so I'm not doing it.
				Rect r = rect(pos, size);
				u32 bg_color = color_apply_opacity(color_blend(colors[COLOR_BG], colors[COLOR_CURSOR_LINE_BG]), 0.8f);
				gl_geometry_rect(r, bg_color);
				#endif
				u32 text_color = color_apply_opacity(colors[COLOR_TEXT], 0.5);
				text_utf8(font, completion, pos.x, pos.y, text_color);
				gl_geometry_draw();
				text_render(font);
			} else {
				// this phantom is no longer relevant
				autocomplete_clear_phantom(ac);
			}
		} else {
			// this phantom is no longer relevant
			autocomplete_clear_phantom(ac);
		}
		free(word_at_cursor);
		return;
	}
	if (!ac->open)
		return;

	autocomplete_find_completions(ted, TRIGGER_INCOMPLETE, false);
	
	size_t ncompletions = arr_len(ac->suggested);
	bool waiting_for_lsp = ac->last_request.id != 0;
	
	if (waiting_for_lsp && ncompletions == 0) {
		double now = ted->frame_time;
		if (now - ac->last_request_time < 0.2) {
			// don't show "Loading..." unless we've actually been loading for a bit of time
			return;
		}
	}
	
	if (!waiting_for_lsp && ncompletions == 0) {
		// no completions. close menu.
		autocomplete_close(ted);
		return;
	}
	
	ac->cursor = ncompletions ? (i32)mod_i64(ac->cursor, (i64)ncompletions) : 0;
	
	autocomplete_correct_scroll(ted);
	i32 scroll = ac->scroll;
	u32 ncompletions_visible = min_u32((u32)ncompletions, AUTOCOMPLETE_NCOMPLETIONS_VISIBLE);
	
	float menu_width = 400, menu_height = (float)ncompletions_visible * char_height;
	
	if (waiting_for_lsp && ncompletions == 0) {
		menu_height = 200.f;
	}
	
	vec2 cursor_pos = buffer_pos_to_pixels(buffer, buffer_cursor_pos(buffer));
	bool open_up = cursor_pos.y > rect_ymid(buffer_rect(buffer)); // should the completion menu open upwards?
	bool open_left = cursor_pos.x > rect_xmid(buffer_rect(buffer));
	float x = cursor_pos.x, start_y = cursor_pos.y;
	if (open_left) x -= menu_width;
	if (open_up)
		start_y -= menu_height;
	else
		start_y += char_height; // put menu below cursor
	{
		Rect menu_rect = rect_xywh(x, start_y, menu_width, menu_height);
		gl_geometry_rect(menu_rect, colors[COLOR_AUTOCOMPLETE_BG]);
		gl_geometry_rect_border(menu_rect, 1, colors[COLOR_AUTOCOMPLETE_BORDER]);
		ac->rect = menu_rect;
	}
	
	i32 mouse_entry = scroll + (i32)((ted_mouse_pos(ted).y - start_y) / char_height);
	
	Autocompletion *document = NULL;
	if (ncompletions) {
		assert(ac->cursor >= 0 && ac->cursor < (i32)ncompletions);
		// highlight cursor entry
		Rect r = rect_xywh(x, start_y + (float)(ac->cursor - scroll) * char_height, menu_width, char_height);
		if (rect_contains_point(ac->rect, rect_center(r))) {
			gl_geometry_rect(r, colors[COLOR_AUTOCOMPLETE_HL]);
			document = &ac->completions[ac->suggested[ac->cursor]];
		}
	}
	if (mouse_entry >= 0 && mouse_entry < (i32)ncompletions
		&& ted_mouse_in_rect(ted, ac->rect)) {
		// highlight moused over entry
		Rect r = rect_xywh(x, start_y + (float)(mouse_entry - scroll) * char_height, menu_width, char_height);
		gl_geometry_rect(r, colors[COLOR_AUTOCOMPLETE_HL]);
		ted->cursor = ted->cursor_hand;
		document = &ac->completions[ac->suggested[mouse_entry]];
	}
	
	float border_thickness = settings->border_thickness;
	
	
	if (document && document->documentation) {
		// document that entry!!
		
		// we've got some wacky calculations to figure out the
		// bounding rect for the documentation
		float doc_width = open_left ? ac->rect.pos.x - 2*padding
			: rect_x2(buffer_rect(buffer)) - (ac->rect.pos.x + ac->rect.size.x + 2*padding);
		if (doc_width > 800) doc_width = 800;
		float doc_height = rect_y2(buffer_rect(buffer)) - (ac->rect.pos.y + 2*padding);
		if (doc_height > char_height * 20) doc_height = char_height * 20;
		
		// if the rect is too small, there's no point in showing it
		if (doc_width >= 200) {
			float doc_x = open_left ? ac->rect.pos.x - doc_width - padding
				: ac->rect.pos.x + ac->rect.size.x + padding;
			float doc_y = ac->rect.pos.y;
			Rect r = rect_xywh(doc_x, doc_y, doc_width, doc_height);
			gl_geometry_rect(r, colors[COLOR_AUTOCOMPLETE_BG]);
			gl_geometry_rect_border(r, border_thickness, colors[COLOR_AUTOCOMPLETE_BORDER]);
			
			// draw the text!
			TextRenderState text_state = text_render_state_default;
			text_state.min_x = doc_x + padding;
			text_state.max_x = doc_x + doc_width - padding;
			text_state.max_y = doc_y + doc_height;
			text_state.x = doc_x + padding;
			text_state.y = doc_y + padding;
			text_state.wrap = true;
			color_u32_to_floats(colors[COLOR_TEXT], text_state.color);
			text_utf8_with_state(font, &text_state, document->documentation);
		}
	}
		
	arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], MouseClick, click) {
		vec2 pos = click->pos;
		if (rect_contains_point(ac->rect, pos)) {
			i32 entry = scroll + (i32)((pos.y - start_y) / char_height);
			if (entry >= 0 && entry < (i32)ncompletions) {
				// entry was clicked on! use this completion.
				autocomplete_complete(ted, ac->completions[ac->suggested[entry]]);
				return;
			}
		}
	}
	
	float y = start_y;
	TextRenderState state = text_render_state_default;
	state.min_x = x + padding; state.min_y = y; state.max_x = x + menu_width - padding; state.max_y = y + menu_height;
	color_u32_to_floats(colors[COLOR_TEXT], state.color);
	
	if (waiting_for_lsp && ncompletions == 0) {
		state.x = x + padding; state.y = y;
		text_utf8_with_state(font, &state, "Loading...");
	} else {
		for (size_t i = 0; i < ncompletions_visible; ++i) {
			const Autocompletion *completion = &ac->completions[ac->suggested[(i32)i + scroll]];
			
			state.x = x; state.y = y;
			if (i != ncompletions_visible-1) {
				gl_geometry_rect(rect_xywh(x, y + char_height,
					menu_width, border_thickness),
					colors[COLOR_AUTOCOMPLETE_BORDER]);
			}
			
			ColorSetting label_color = color_for_symbol_kind(completion->kind);
			if (!settings->syntax_highlighting)
				label_color = COLOR_TEXT;
			
			color_u32_to_floats(colors[label_color], state.color);
			
			// draw icon
			char icon_text[2] = {symbol_kind_icon(completion->kind), 0};
			state.x += padding;
			text_utf8_with_state(font, &state, icon_text);
			state.x += padding;
			gl_geometry_rect(rect_xywh((float)state.x, (float)state.y, border_thickness, char_height),
				colors[COLOR_AUTOCOMPLETE_BORDER]);
			state.x += padding;
			
			float label_x = (float)state.x;
			text_utf8_with_state(font, &state, completion->label);
			
			const char *detail = completion->detail;
			if (detail) {
				// draw detail
				double label_end_x = state.x;
				
				char show_text[128] = {0};
				
				int amount_detail = 0;
				for (; ; ++amount_detail) {
					if (unicode_is_continuation_byte((u8)detail[amount_detail]))
						continue; // don't cut off text in the middle of a code point.
					
					char text[128] = {0};
					strbuf_printf(text, "%.*s%s", amount_detail, detail,
						(size_t)amount_detail == strlen(detail) ? "" : "...");
					double width = text_get_size_vec2(font, text).x;
					if (label_end_x + width + 2 * padding < state.max_x) {
						strbuf_cpy(show_text, text);
					}
					// don't break if not, since we want to use "blabla"
					//  even if "blabl..." is too long
					
					if (!detail[amount_detail]) break;
				}
				if (amount_detail >= 3) {
					text_utf8_anchored(font, show_text, state.max_x, state.y,
						colors[COLOR_COMMENT], ANCHOR_TOP_RIGHT);
				}
			}
			
			if (completion->deprecated) {
				gl_geometry_rect(
					rect_xywh(
						label_x, y + (char_height - border_thickness) * 0.5f,
						(float)state.x - label_x, 1
					),
					colors[label_color]
				);
			}
			
			y += char_height;
		}
	}
	
	gl_geometry_draw();
	text_render(font);
}

void autocomplete_quit(Ted *ted) {
	autocomplete_close(ted);
	free(ted->autocomplete);
	ted->autocomplete = NULL;
}
