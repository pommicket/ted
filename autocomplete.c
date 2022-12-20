#define TAGS_MAX_COMPLETIONS 200 // max # of tag completions to scroll through
#define AUTOCOMPLETE_NCOMPLETIONS 10 // max # of completions to show at once

static void autocomplete_clear_completions(Ted *ted) {
	arr_foreach_ptr(ted->autocompletions, Autocompletion, completion) {
		free(completion->label);
		free(completion->text);
	}
	arr_clear(ted->autocompletions);
}

// do the actual completion
static void autocomplete_complete(Ted *ted, Autocompletion *completion) {
	TextBuffer *buffer = ted->active_buffer;
	buffer_start_edit_chain(buffer); // don't merge with other edits
	if (is_word(buffer_char_before_cursor(buffer)))
		buffer_backspace_words_at_cursor(buffer, 1); // delete whatever text was already typed
	buffer_insert_utf8_at_cursor(buffer, completion->text);
	buffer_end_edit_chain(buffer);
	autocomplete_close(ted);
}

static void autocomplete_select_cursor_completion(Ted *ted) {
	if (ted->autocomplete) {
		size_t ncompletions = arr_len(ted->autocompletions);
		if (ncompletions) {
			i64 cursor = mod_i64(ted->autocomplete_cursor, (i64)ncompletions);
			autocomplete_complete(ted, &ted->autocompletions[cursor]);
			autocomplete_close(ted);
		}
	}
}


void autocomplete_close(Ted *ted) {
	if (ted->autocomplete) {
		ted->autocomplete = false;
		autocomplete_clear_completions(ted);
	}
}

static void autocomplete_find_completions(Ted *ted) {
	TextBuffer *buffer = ted->active_buffer;
	BufferPos pos = buffer->cursor_pos;
	if (buffer_pos_eq(pos, ted->autocomplete_pos))
		return; // no need to update completions.
	ted->autocomplete_pos = pos;

	LSP *lsp = buffer_lsp(buffer);
	if (lsp) {
		LSPRequest request = {
			.type = LSP_REQUEST_COMPLETION
		};
		request.data.completion = (LSPRequestCompletion) {
			.position = {
				.document = str_dup(buffer->filename),
				.pos = buffer_pos_to_lsp(buffer, pos)
			}
		};
		lsp_send_request(lsp, &request);
	} else {
		// tag completion
		autocomplete_clear_completions(ted);
		
		char *word_at_cursor = str32_to_utf8_cstr(buffer_word_at_cursor(buffer));
		char **completions = calloc(TAGS_MAX_COMPLETIONS, sizeof *completions);
		size_t ncompletions = tags_beginning_with(ted, word_at_cursor, completions, TAGS_MAX_COMPLETIONS);
		free(word_at_cursor);
		
		arr_set_len(ted->autocompletions, ncompletions);
		
		for (size_t i = 0; i < ncompletions; ++i) {
			ted->autocompletions[i].label = completions[i];
			ted->autocompletions[i].text = str_dup(completions[i]);
		}
		free(completions);
	}
}

// open autocomplete, or just do the completion if there's only one suggestion
static void autocomplete_open(Ted *ted) {
	if (!ted->active_buffer) return;
	TextBuffer *buffer = ted->active_buffer;
	if (!buffer->filename) return;
	if (buffer->view_only) return;
	
	ted->cursor_error_time = 0;
	ted->autocomplete_pos = (BufferPos){0,0};
	ted->autocomplete_cursor = 0;
	ted->autocompletions = NULL;
	autocomplete_find_completions(ted);
	switch (arr_len(ted->autocompletions)) {
	case 0:
		ted->cursor_error_time = time_get_seconds();
		autocomplete_close(ted);
		break;
	case 1:
		autocomplete_complete(ted, &ted->autocompletions[0]);
		// (^ this calls autocomplete_close)
		break;
	default:
		// open autocomplete menu
		ted->autocomplete = true;
		break;
	}
}

static void autocomplete_frame(Ted *ted) {
	TextBuffer *buffer = ted->active_buffer;
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	Settings const *settings = buffer_settings(buffer);
	u32 const *colors = settings->colors;
	float const padding = settings->padding;

	autocomplete_find_completions(ted);

	size_t ncompletions = arr_len(ted->autocompletions);
	if (ncompletions > AUTOCOMPLETE_NCOMPLETIONS)
		ncompletions = AUTOCOMPLETE_NCOMPLETIONS;
	float menu_width = 400, menu_height = (float)ncompletions * char_height + 2 * padding;
	
	if (ncompletions == 0) {
		// no completions. close menu.
		autocomplete_close(ted);
		return;
	}
	
	ted->autocomplete_cursor = (i32)mod_i64(ted->autocomplete_cursor, (i64)ncompletions);
	
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
		gl_geometry_rect(menu_rect, colors[COLOR_MENU_BG]);
		//gl_geometry_rect_border(menu_rect, 1, colors[COLOR_BORDER]);
		ted->autocomplete_rect = menu_rect;
	}
	
	// vertical padding
	start_y += padding;
	menu_height -= 2 * padding;
	
	u16 cursor_entry = (u16)((ted->mouse_pos.y - start_y) / char_height);
	if (cursor_entry < ncompletions) {
		// highlight moused over entry
		Rect r = rect(V2(x, start_y + cursor_entry * char_height), V2(menu_width, char_height));
		gl_geometry_rect(r, colors[COLOR_MENU_HL]);
		ted->cursor = ted->cursor_hand;	
	}
	{ // highlight cursor entry
		Rect r = rect(V2(x, start_y + (float)ted->autocomplete_cursor * char_height), V2(menu_width, char_height));
		gl_geometry_rect(r, colors[COLOR_MENU_HL]);
	}
	
	for (uint i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
		v2 click = ted->mouse_clicks[SDL_BUTTON_LEFT][i];
		if (rect_contains_point(ted->autocomplete_rect, click)) {
			u16 entry = (u16)((click.y - start_y) / char_height);
			if (entry < ncompletions) {
				// entry was clicked on! use this completion.
				autocomplete_complete(ted, &ted->autocompletions[entry]);
			}
		}
	}
	
	float y = start_y;
	TextRenderState state = text_render_state_default;
	state.min_x = x + padding; state.min_y = y; state.max_x = x + menu_width - padding; state.max_y = y + menu_height;
	rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
	for (size_t i = 0; i < ncompletions; ++i) {
		state.x = x + padding; state.y = y;
		text_utf8_with_state(font, &state, ted->autocompletions[i].label);
		y += char_height;
	}
	
	gl_geometry_draw();
	text_render(font);
}
