#define TAGS_MAX_COMPLETIONS 200 // max # of tag completions to scroll through
#define AUTOCOMPLETE_NCOMPLETIONS_VISIBLE 10 // max # of completions to show at once

static void autocomplete_clear_completions(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
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

// do the actual completion
static void autocomplete_complete(Ted *ted, Autocompletion completion) {
	TextBuffer *buffer = ted->active_buffer;
	buffer_start_edit_chain(buffer); // don't merge with other edits
	if (is_word(buffer_char_before_cursor(buffer)))
		buffer_backspace_words_at_cursor(buffer, 1); // delete whatever text was already typed
	buffer_insert_utf8_at_cursor(buffer, completion.text);
	buffer_end_edit_chain(buffer);
	autocomplete_close(ted);
}

static void autocomplete_select_cursor_completion(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	if (ac->open) {
		size_t nsuggestions = arr_len(ac->suggested);
		if (nsuggestions) {
			i64 cursor = mod_i64(ac->cursor, (i64)nsuggestions);
			autocomplete_complete(ted, ac->completions[ac->suggested[cursor]]);
			autocomplete_close(ted);
		}
	}
}


static void autocomplete_next(Ted *ted) {
	++ted->autocomplete.cursor;
}

static void autocomplete_prev(Ted *ted) {
	--ted->autocomplete.cursor;
}

void autocomplete_close(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	if (ac->open) {
		ac->open = false;
		ac->waiting_for_lsp = false;
		autocomplete_clear_completions(ted);
	}
}

void autocomplete_update_suggested(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	arr_clear(ac->suggested);
	char *word = str32_to_utf8_cstr(
		buffer_word_at_cursor(ted->active_buffer)
	);
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
	ted->cursor_error_time = time_get_seconds();
	autocomplete_close(ted);
}

static void autocomplete_find_completions(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	TextBuffer *buffer = ted->active_buffer;
	BufferPos pos = buffer->cursor_pos;
	if (buffer_pos_eq(pos, ac->last_pos))
		return; // no need to update completions.
	ac->last_pos = pos;

	LSP *lsp = buffer_lsp(buffer);
	if (lsp) {
		LSPRequest request = {
			.type = LSP_REQUEST_COMPLETION
		};
		request.data.completion = (LSPRequestCompletion) {
			.position = {
				.document = lsp_document_id(lsp, buffer->filename),
				.pos = buffer_pos_to_lsp(buffer, pos)
			}
		};
		lsp_send_request(lsp, &request);
		if (!ac->completions)
			ac->waiting_for_lsp = true;
	} else {
		// tag completion
		autocomplete_clear_completions(ted);
		
		char *word_at_cursor = str32_to_utf8_cstr(buffer_word_at_cursor(buffer));
		char **completions = calloc(TAGS_MAX_COMPLETIONS, sizeof *completions);
		u32 ncompletions = (u32)tags_beginning_with(ted, word_at_cursor, completions, TAGS_MAX_COMPLETIONS);
		free(word_at_cursor);
		
		arr_set_len(ac->completions, ncompletions);
		
		for (size_t i = 0; i < ncompletions; ++i) {
			ac->completions[i].label = completions[i];
			ac->completions[i].text = str_dup(completions[i]);
			ac->completions[i].filter = str_dup(completions[i]);
			arr_add(ac->suggested, (u32)i);
		}
		free(completions);
	}
	
	autocomplete_update_suggested(ted);
}

static void autocomplete_process_lsp_response(Ted *ted, const LSPResponse *response) {
	Autocomplete *ac = &ted->autocomplete;
	bool was_waiting = ac->waiting_for_lsp;
	ac->waiting_for_lsp = false;
	if (!ac->open) {
		// user hit escape or down or something before completions arrived.
		return;
	}
	
	const LSPRequest *request = &response->request;
	if (request->type == LSP_REQUEST_COMPLETION) {
		const LSPResponseCompletion *completion = &response->data.completion;
		size_t ncompletions = arr_len(completion->items);
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
			ted_completion->documentation = documentation ? str_dup(documentation) : NULL;
			
		}
	}
	autocomplete_update_suggested(ted);
	switch (arr_len(ac->suggested)) {
	case 0:
		autocomplete_no_suggestions(ted);
		return;
	case 1:
		// if we just finished loading suggestions, and there's only one suggestion, use it
		if (was_waiting)
			autocomplete_complete(ted, ac->completions[ac->suggested[0]]);
		return;	
	}
}

// open autocomplete, or just do the completion if there's only one suggestion
static void autocomplete_open(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	if (ac->open) return;
	if (!ted->active_buffer) return;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer->filename) return;
	if (buffer->view_only) return;
	
	ted->cursor_error_time = 0;
	ac->last_pos = (BufferPos){0,0};
	ac->cursor = 0;
	autocomplete_find_completions(ted);
	
	switch (arr_len(ac->completions)) {
	case 0:
		if (autocomplete_using_lsp(ted)) {
			ac->open = true;
		} else {
			autocomplete_no_suggestions(ted);
		}
		break;
	case 1:
		autocomplete_complete(ted, ac->completions[0]);
		// (^ this calls autocomplete_close)
		break;
	default:
		// open autocomplete menu
		ac->open = true;
		break;
	}
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
}

static void autocomplete_frame(Ted *ted) {
	Autocomplete *ac = &ted->autocomplete;
	TextBuffer *buffer = ted->active_buffer;
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	Settings const *settings = buffer_settings(buffer);
	u32 const *colors = settings->colors;
	float const padding = settings->padding;

	autocomplete_find_completions(ted);

	Autocompletion completions[AUTOCOMPLETE_NCOMPLETIONS_VISIBLE] = {0};
	size_t ncompletions = 0;
	arr_foreach_ptr(ac->suggested, u32, suggestion) {
		Autocompletion *completion = &ac->completions[*suggestion];
		completions[ncompletions++] = *completion;
		if (ncompletions == AUTOCOMPLETE_NCOMPLETIONS_VISIBLE)
			break;
	}
	
	float menu_width = 400, menu_height = (float)ncompletions * char_height;
	
	if (ac->waiting_for_lsp) {
		menu_height = 200.f;
	}
	
	if (!ac->waiting_for_lsp && ncompletions == 0) {
		// no completions. close menu.
		autocomplete_close(ted);
		return;
	}
	
	ac->cursor = ncompletions ? (i32)mod_i64(ac->cursor, (i64)ncompletions) : 0;
	
	v2 cursor_pos = buffer_pos_to_pixels(buffer, buffer->cursor_pos);
	bool open_up = cursor_pos.y > 0.5f * (buffer->y1 + buffer->y2); // should the completion menu open upwards?
	bool open_left = cursor_pos.x > 0.5f * (buffer->x1 + buffer->x2);
	float x = cursor_pos.x, start_y = cursor_pos.y;
	if (open_left) x -= menu_width;
	if (open_up)
		start_y -= menu_height;
	else
		start_y += char_height; // put menu below cursor
	{
		Rect menu_rect = rect(V2(x, start_y), V2(menu_width, menu_height));
		gl_geometry_rect(menu_rect, colors[COLOR_AUTOCOMPLETE_BG]);
		gl_geometry_rect_border(menu_rect, 1, colors[COLOR_AUTOCOMPLETE_BORDER]);
		ac->rect = menu_rect;
	}
	
	u16 cursor_entry = (u16)((ted->mouse_pos.y - start_y) / char_height);
	
	Autocompletion *document = NULL;
	if (cursor_entry < ncompletions) {
		// highlight moused over entry
		Rect r = rect(V2(x, start_y + cursor_entry * char_height), V2(menu_width, char_height));
		gl_geometry_rect(r, colors[COLOR_AUTOCOMPLETE_HL]);
		ted->cursor = ted->cursor_hand;
		document = &completions[cursor_entry];
	} else if (ncompletions) {
		// highlight cursor entry
		Rect r = rect(V2(x, start_y + (float)ac->cursor * char_height), V2(menu_width, char_height));
		gl_geometry_rect(r, colors[COLOR_AUTOCOMPLETE_HL]);
		document = &completions[ac->cursor];
	}
	
	float border_thickness = settings->border_thickness;
	
	
	if (document) {
		// document that entry!!
		
		// we've go tsome wacky calculations to figure out the
		// bounding rect for the documentation
		float doc_width = open_left ? ac->rect.pos.x - 2*padding
			: buffer->x2 - (ac->rect.pos.x + ac->rect.size.x + 2*padding);
		if (doc_width > 800) doc_width = 800;
		float doc_height = buffer->y2 - (ac->rect.pos.y + 2*padding);
		if (doc_height > char_height * 20) doc_height = char_height * 20;
		
		// if the rect is too small, there's no point in showing it
		if (doc_width >= 200) {
			float doc_x = open_left ? ac->rect.pos.x - doc_width - padding
				: ac->rect.pos.x + ac->rect.size.x + padding;
			float doc_y = ac->rect.pos.y;
			Rect r = rect(V2(doc_x, doc_y), V2(doc_width, doc_height));
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
			rgba_u32_to_floats(colors[COLOR_TEXT], text_state.color);
			text_utf8_with_state(font, &text_state, document->documentation);
		}
	}
		
	
	for (uint i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
		v2 click = ted->mouse_clicks[SDL_BUTTON_LEFT][i];
		if (rect_contains_point(ac->rect, click)) {
			u16 entry = (u16)((click.y - start_y) / char_height);
			if (entry < ncompletions) {
				// entry was clicked on! use this completion.
				autocomplete_complete(ted, ac->completions[ac->suggested[entry]]);
			}
		}
	}
	
	float y = start_y;
	TextRenderState state = text_render_state_default;
	state.min_x = x + padding; state.min_y = y; state.max_x = x + menu_width - padding; state.max_y = y + menu_height;
	rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
	
	if (ac->waiting_for_lsp) {
		state.x = x + padding; state.y = y;
		text_utf8_with_state(font, &state, "Loading...");
	} else {
		for (size_t i = 0; i < ncompletions; ++i) {
			const Autocompletion *completion = &completions[i];
			
			state.x = x; state.y = y;
			if (i != ncompletions-1) {
				gl_geometry_rect(rect(V2(x, y + char_height),
					V2(menu_width, border_thickness)),
					colors[COLOR_AUTOCOMPLETE_BORDER]);
			}
			
			ColorSetting label_color = color_for_symbol_kind(completion->kind);
			if (!settings->syntax_highlighting)
				label_color = COLOR_TEXT;
			
			rgba_u32_to_floats(colors[label_color], state.color);
			
			// draw icon
			char icon_text[2] = {symbol_kind_icon(completion->kind), 0};
			state.x += padding;
			text_utf8_with_state(font, &state, icon_text);
			state.x += padding;
			gl_geometry_rect(rect(V2((float)state.x, (float)state.y), V2(border_thickness, char_height)),
				colors[COLOR_AUTOCOMPLETE_BORDER]);
			state.x += padding;
			
			float label_x = (float)state.x;
			text_utf8_with_state(font, &state, completion->label);
			
			const char *detail = completion->detail;
			if (detail) {
				double label_end_x = state.x;
				
				char show_text[128] = {0};
				
				int amount_detail = 0;
				for (; ; ++amount_detail) {
					if (unicode_is_continuation_byte((u8)detail[amount_detail]))
						continue; // don't cut off text in the middle of a code point.
					
					char text[128] = {0};
					strbuf_printf(text, "%.*s%s", amount_detail, detail,
						(size_t)amount_detail == strlen(detail) ? "" : "...");
					double width = text_get_size_v2(font, text).x;
					if (label_end_x + width + 2 * padding < state.max_x) {
						strbuf_cpy(show_text, text);
					}
					// don't break if not, since we want to use "blabla"
					//  even if "blabl..." is too long
					
					if (!detail[amount_detail]) break;
				}
				if (amount_detail >= 3) {
					//rgba_u32_to_floats(colors[COLOR_COMMENT], state.color);
					text_utf8_anchored(font, show_text, state.max_x, state.y,
						colors[COLOR_COMMENT], ANCHOR_TOP_RIGHT);
				}
			}
			
			if (completion->deprecated) {
				gl_geometry_rect(rect(V2(label_x, y + (char_height - border_thickness) * 0.5f),
					V2((float)state.x - label_x, 1)),
					colors[label_color]);
			}
			
			y += char_height;
		}
	}
	
	gl_geometry_draw();
	text_render(font);
}
