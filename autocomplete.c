
#define AUTOCOMPLETE_NCOMPLETIONS 10 // max # of completions to show

// get the thing to be completed (and what buffer it's in)
// returns false if this is a read only buffer or something
// call free() on *startp when you're done with it
static Status autocomplete_get(Ted *ted, char **startp, TextBuffer **bufferp) {
	TextBuffer *buffer = ted->active_buffer;
	if (buffer && !buffer->view_only && !buffer->is_line_buffer) {
		buffer->selection = false;
		if (is_word(buffer_char_after_cursor(buffer)))
			buffer_cursor_move_right_words(buffer, 1);
		else
			buffer_scroll_to_cursor(buffer);
		char *start = str32_to_utf8_cstr(buffer_word_at_cursor(buffer));
		if (*start) {
			*startp = start;
			*bufferp = buffer;
			return true;	
		} else {
			// no word at cursor
			free(start);
			return false;
		}
	} else {
		return false;
	}	
}

// do the actual completion
static void autocomplete_complete(Ted *ted, char *start, TextBuffer *buffer, char *completion) {
	char *str = completion + strlen(start);
	buffer_start_edit_chain(buffer); // don't merge with other edits
	buffer_insert_utf8_at_cursor(buffer, str);
	buffer_end_edit_chain(buffer);
	ted->autocomplete = false;
}

static void autocomplete_select_cursor_completion(Ted *ted) {
	char *start; TextBuffer *buffer;
	if (autocomplete_get(ted, &start, &buffer)) {
		char *completions[AUTOCOMPLETE_NCOMPLETIONS];
		size_t ncompletions = tags_beginning_with(ted, start, completions, arr_count(completions));
		if (ncompletions) {
			i64 cursor = mod_i64(ted->autocomplete_cursor, (i64)ncompletions);
			autocomplete_complete(ted, start, buffer, completions[cursor]);
			for (size_t i = 0; i < ncompletions; ++i)
				free(completions[i]);
		}
		free(start);
	}
}

// open autocomplete, or just do the completion if there's only one suggestion
static void autocomplete_open(Ted *ted) {
	char *start;
	TextBuffer *buffer;
	ted->cursor_error_time = 0;
	ted->autocomplete_cursor = 0;
	if (autocomplete_get(ted, &start, &buffer)) {
		char *completions[2] = {0};
		size_t ncompletions = tags_beginning_with(ted, start, completions, 2);
		switch (ncompletions) {
		case 0:
			ted->cursor_error_time = time_get_seconds();
			break;
		case 1:
			autocomplete_complete(ted, start, buffer, completions[0]);
			break;
		case 2:
			// open autocomplete selection menu
			ted->autocomplete = true;
			break;
		default: assert(0); break;
		}
		free(completions[0]);
		free(completions[1]);
		free(start);
	}
}

static void autocomplete_frame(Ted *ted) {
	
	char *start;
	TextBuffer *buffer;
	if (autocomplete_get(ted, &start, &buffer)) {
		Font *font = ted->font;
		float char_height = text_font_char_height(font);
		Settings const *settings = buffer_settings(buffer);
		u32 const *colors = settings->colors;
		float const padding = settings->padding;
	
		char *completions[AUTOCOMPLETE_NCOMPLETIONS];
		size_t ncompletions = tags_beginning_with(ted, start, completions, arr_count(completions));
		float menu_width = 400, menu_height = (float)ncompletions * char_height + 2 * padding;
		
		if (ncompletions == 0) {
			// no completions. close menu.
			ted->autocomplete = false;
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
					autocomplete_complete(ted, start, buffer, completions[entry]);
				}
			}
		}
		
		float y = start_y;
		TextRenderState state = text_render_state_default;
		state.min_x = x + padding; state.min_y = y; state.max_x = x + menu_width - padding; state.max_y = y + menu_height;
		rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
		for (size_t i = 0; i < ncompletions; ++i) {
			state.x = x + padding; state.y = y;
			text_utf8_with_state(font, &state, completions[i]);
			y += char_height;
		}
		
		gl_geometry_draw();
		text_render(font);
		
		for (size_t i = 0; i < ncompletions; ++i)
			free(completions[i]);
	
		free(start);
	} else {
		ted->autocomplete = false;
	}
}
