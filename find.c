#define FIND_MAX_GROUPS 50

static u32 find_compilation_flags(Ted *ted) {
	return (ted->find_case_sensitive ? 0 : PCRE2_CASELESS)
		| (ted->find_regex ? 0 : PCRE2_LITERAL);
}

static u32 find_replace_flags(Ted *ted) {
	return (ted->find_regex ? 0 : PCRE2_SUBSTITUTE_LITERAL);
}

static void ted_seterr_to_pcre2_err(Ted *ted, int err) {
	char32_t buf[256] = {0};
	size_t len = (size_t)pcre2_get_error_message(err, buf, arr_count(buf) - 1);
	char *error_cstr = str32_to_utf8_cstr(str32(buf, len));
	if (error_cstr) {
		ted_seterr(ted, "Search error: %s.", error_cstr);
		free(error_cstr);
	}
}

static bool find_compile_pattern(Ted *ted) {
	TextBuffer *find_buffer = &ted->find_buffer;
	String32 term = buffer_get_line(find_buffer, 0);
	if (term.len) {
		pcre2_match_data *match_data = pcre2_match_data_create(FIND_MAX_GROUPS, NULL);
		if (match_data) {
			int error = 0;
			PCRE2_SIZE error_pos = 0;
			pcre2_code *code = pcre2_compile(term.str, term.len, find_compilation_flags(ted), &error, &error_pos, NULL);
			if (code) {
				ted->find_code = code;
				ted->find_match_data = match_data;
				ted->find_invalid_pattern = false;
				return true;
			} else {
				ted->find_invalid_pattern = true;
			}
			pcre2_match_data_free(match_data);
		} else {
			ted_seterr(ted, "Out of memory.");
		}
	} else {
		ted->find_invalid_pattern = false;
	}
	return false;
}

static void find_free_pattern(Ted *ted) {
	if (ted->find_code) {
		pcre2_code_free(ted->find_code);
		ted->find_code = NULL;
	}
	if (ted->find_match_data) {
		pcre2_match_data_free(ted->find_match_data);
		ted->find_match_data = NULL;
	}
	arr_clear(ted->find_results);
}

static float find_menu_height(Ted *ted) {
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	Settings const *settings = &ted->settings;
	float padding = settings->padding;

	return 3 * char_height + (padding + char_height) * ted->replace + 6 * padding;
}

// finds the next match in the buffer, returning false if there is no match this line.
// sets *match_start and *match_end (if not NULL) to the start and end of the match, respectively
// advances *pos to the end of the match or the start of the next line if there is no match.
// direction should be either +1 (forwards) or -1 (backwards)
static WarnUnusedResult bool find_match(Ted *ted, BufferPos *pos, u32 *match_start, u32 *match_end, int direction) {
	TextBuffer *buffer = ted->prev_active_buffer;
	assert(buffer);
	String32 str = buffer_get_line(buffer, pos->line);
	PCRE2_SIZE *groups = pcre2_get_ovector_pointer(ted->find_match_data);
	
	int ret;
	if (direction == +1)
		ret = pcre2_match(ted->find_code, str.str, str.len, pos->index, 0, ted->find_match_data, NULL);
	else {
		// unfortunately PCRE does not have a backwards option, so we need to do the search multiple times
		u32 last_pos = 0;
		ret = -1;
		while (1) {
			int next_ret = pcre2_match(ted->find_code, str.str, pos->index, last_pos, 0, ted->find_match_data, NULL);
			if (next_ret > 0) {
				ret = next_ret;
				if (groups[0] == groups[1]) ++groups[1]; // ensure we don't have an infinite loop
				last_pos = (u32)groups[1];
			} else break;
		}
	}
	if (ret > 0) {
		if (groups[0] == groups[1]) ++groups[1];
		if (match_start) *match_start = (u32)groups[0];
		if (match_end)   *match_end   = (u32)groups[1];
		pos->index = (u32)groups[1];
		return true;
	} else {
		pos->line += (u32)((i64)buffer->nlines + direction);
		pos->line %= buffer->nlines;
		if (direction == +1)
			pos->index = 0;
		else
			pos->index = (u32)buffer_get_line(buffer, pos->line).len;
		return false;
	}
}

// check if the search term needs to be recompiled
static void find_update(Ted *ted, bool force) {
	TextBuffer *find_buffer = &ted->find_buffer;
	u32 flags = find_compilation_flags(ted);
	if (!force
		&& !find_buffer->modified // check if buffer has been modified,
		&& ted->find_flags == flags) // or checkboxes have been (un)checked
		return;
	ted->find_flags = flags;
	TextBuffer *buffer = ted->prev_active_buffer;

	find_free_pattern(ted);

	if (find_compile_pattern(ted)) {
		BufferPos pos = buffer_start_of_file(buffer);
		BufferPos best_scroll_candidate = {U32_MAX, U32_MAX};
		BufferPos cursor_pos = buffer->cursor_pos;
		// find all matches
		for (u32 nsearches = 0; nsearches < buffer->nlines; ++nsearches) {
			u32 match_start, match_end;
			while (find_match(ted, &pos, &match_start, &match_end, +1)) {
				BufferPos match_start_pos = {.line = pos.line, .index = match_start};
				BufferPos match_end_pos = {.line = pos.line, .index = match_end};
				FindResult result = {match_start_pos, match_end_pos};
				arr_add(ted->find_results, result);
				if (best_scroll_candidate.line == U32_MAX 
				|| (buffer_pos_cmp(best_scroll_candidate, cursor_pos) < 0 && buffer_pos_cmp(match_start_pos, cursor_pos) >= 0))
					best_scroll_candidate = match_start_pos;
			}
		}
		find_buffer->modified = false;
		if (best_scroll_candidate.line != U32_MAX) // scroll to first match (if there is one)
			buffer_scroll_to_pos(buffer, best_scroll_candidate);
	} else {
		buffer_scroll_to_cursor(buffer);
	}
}

// returns the index of the match we are "on", or U32_MAX for none.
static u32 find_match_idx(Ted *ted) {
	TextBuffer *buffer = ted->prev_active_buffer;
	if (!buffer->selection) return U32_MAX;
	u32 match_idx = U32_MAX;
	arr_foreach_ptr(ted->find_results, FindResult, result) {
		if (buffer_pos_eq(result->start, buffer->selection_pos)
			&& buffer_pos_eq(result->end, buffer->cursor_pos))
			match_idx = (u32)(result - ted->find_results);
	}
	return match_idx;
}


static void find_next_in_direction(Ted *ted, int direction) {
	TextBuffer *buffer = ted->prev_active_buffer;
	
	BufferPos pos = direction == +1 || !buffer->selection ? buffer->cursor_pos : buffer->selection_pos;
	u32 nlines = buffer->nlines;
	
	
	// we need to search the starting line twice, because we might start at a non-zero index
	for (size_t nsearches = 0; nsearches < nlines + 1; ++nsearches) {
		u32 match_start, match_end;
		if (find_match(ted, &pos, &match_start, &match_end, direction)) {
			BufferPos pos_start = {.line = pos.line, .index = match_start};
			BufferPos pos_end = {.line = pos.line, .index = match_end};
			buffer_cursor_move_to_pos(buffer, pos_start);
			buffer_select_to_pos(buffer, pos_end);
			break;
		}
	}
}

// returns true if successful
static bool find_replace_match(Ted *ted, u32 match_idx) {
	find_update(ted, false);
	if (!ted->find_code) return false;
	
	bool success = false;
	FindResult match = ted->find_results[match_idx];
	TextBuffer *buffer = ted->prev_active_buffer;
	assert(match.start.line == match.end.line);
	String32 line = buffer_get_line(buffer, match.start.line);
	String32 replacement = buffer_get_line(&ted->replace_buffer, 0);
	// we are currently highlighting the find pattern, let's replace it
	
	// get size of buffer needed.
	PCRE2_SIZE output_size = 0;
	u32 flags = find_replace_flags(ted);
	char32_t *str = line.str + match.start.index;
	u32 len = match.end.index - match.start.index;
	
	int ret = pcre2_substitute(ted->find_code, str, len, 0,
		PCRE2_SUBSTITUTE_OVERFLOW_LENGTH|flags, ted->find_match_data, NULL, replacement.str,
		replacement.len, NULL, &output_size);
	char32_t *output_buffer = output_size
		? calloc(output_size, sizeof *output_buffer)
		: NULL;
	if (output_buffer || !output_size) {
		ret = pcre2_substitute(ted->find_code, str, len, 0,
			flags, ted->find_match_data, NULL, replacement.str,
			replacement.len, output_buffer, &output_size);
		if (ret > 0) {
			buffer->selection = false; // stop selecting match
			buffer_delete_chars_at_pos(buffer, match.start, len);
			if (output_buffer)
				buffer_insert_text_at_pos(buffer, match.start, str32(output_buffer, output_size));
			
			// remove this match
			arr_remove(ted->find_results, match_idx);
			
			i64 diff = (i64)output_size - len; // change in number of characters
			// @OPTIMIZE: binary search for find results on the right line
			for (u32 i = 0; i < arr_len(ted->find_results); ++i) {
				FindResult *result = &ted->find_results[i];
				if (result->start.line == match.start.line && result->start.index > match.end.index) {
					// fix indices of other find results
					result->start.index = (u32)(result->start.index + diff);
					result->end.index = (u32)(result->end.index + diff);
				}
			}
			success = true;
		} else if (ret < 0) {
			ted_seterr_to_pcre2_err(ted, ret);
		}
		free(output_buffer);
	} else {
		ted_seterr(ted, "Out of memory.");
	}
	return success;
}

// replace the match we are currently highlighting, or do nothing if there is no highlighted match
static void find_replace(Ted *ted) {
	TextBuffer *buffer = ted->prev_active_buffer;
	u32 match_idx = find_match_idx(ted);
	if (match_idx != U32_MAX) {
		buffer_cursor_move_to_pos(buffer, ted->find_results[match_idx].start); // move to start of match
		find_replace_match(ted, match_idx);
	}
}
	
// go to next find result
static void find_next(Ted *ted) {
	if (ted->replace)
		find_replace(ted);
	find_next_in_direction(ted, +1);
}

static void find_prev(Ted *ted) {
	find_next_in_direction(ted, -1);
}

static void find_replace_all(Ted *ted) {
	TextBuffer *buffer = ted->prev_active_buffer;
	if (ted->replace) {
		find_next(ted);
		u32 match_idx = find_match_idx(ted);
		if (match_idx != U32_MAX) {
			{
				FindResult *last_result = arr_lastp(ted->find_results);
				buffer_cursor_move_to_pos(buffer, last_result->start);
			}
			// NOTE: we don't need to increment i because the matches will be removed from the find_results array.
			for (u32 i = match_idx; i < arr_len(ted->find_results); ) {
				if (!find_replace_match(ted, i))
					break;
			}
			find_update(ted, true);
		}
	}
}

static void find_menu_frame(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float const char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);

	Settings const *settings = &ted->settings;
	float const padding = settings->padding;
	float const menu_height = find_menu_height(ted);
	float const window_width = ted->window_width, window_height = ted->window_height;
	u32 const *colors = settings->colors;
	bool const replace = ted->replace;
	
	TextBuffer *buffer = ted->prev_active_buffer, *find_buffer = &ted->find_buffer, *replace_buffer = &ted->replace_buffer;
	assert(buffer);
	
	u32 first_rendered_line = buffer_first_rendered_line(buffer);
	u32 last_rendered_line = buffer_last_rendered_line(buffer);
	

	float x1 = padding, y1 = window_height - menu_height + padding, x2 = window_width - padding, y2 = window_height - padding;

	Rect menu_bounds = rect4(x1, y1, x2, y2);

	x1 += padding;
	y1 += padding;
	x2 -= padding;
	y2 -= padding;
	
	char const *prev_text = "Previous", *next_text = "Next";
	char const *replace_text = "Replace", *replace_find_text = "Replace+find", *replace_all_text = "Replace all";
	v2 prev_size = text_get_size_v2(font, prev_text);
	v2 next_size = text_get_size_v2(font, next_text);
	v2 replace_size = text_get_size_v2(font, replace_text);
	v2 replace_find_size = text_get_size_v2(font, replace_find_text);
	v2 replace_all_size = text_get_size_v2(font, replace_all_text);
	
	float x = x1, y = y2 - char_height;
	// compute positions of buttons
	Rect button_prev = rect(V2(x, y), prev_size);
	x += button_prev.size.x + padding;
	Rect button_next = rect(V2(x, y), next_size);
	x += button_next.size.x + padding;
	Rect button_replace = rect(V2(x, y), replace_size);
	x += button_replace.size.x + padding;
	Rect button_replace_find = rect(V2(x, y), replace_find_size);
	x += button_replace_find.size.x + padding;
	Rect button_replace_all = rect(V2(x, y), replace_all_size);
	x += button_replace_all.size.x + padding;
	
	if (button_update(ted, button_prev))
		find_next_in_direction(ted, -1);
	if (button_update(ted, button_next))
		find_next_in_direction(ted, +1);
	if (replace) {
		if (button_update(ted, button_replace_find))
			find_next(ted);
		if (button_update(ted, button_replace))
			find_replace(ted);
		if (button_update(ted, button_replace_all))
			find_replace_all(ted);
	}
	
	find_update(ted, false);
	arr_foreach_ptr(ted->find_results, FindResult, result) {
		// highlight matches
		BufferPos p1 = result->start, p2 = result->end;
		if (p2.line >= first_rendered_line && p1.line <= last_rendered_line) {
			v2 pos1 = buffer_pos_to_pixels(buffer, p1);
			v2 pos2 = buffer_pos_to_pixels(buffer, p2);
			pos2.y += char_height;
			Rect hl_rect = rect4(pos1.x, pos1.y, pos2.x, pos2.y);
			if (buffer_clip_rect(buffer, &hl_rect))
				gl_geometry_rect(hl_rect, colors[COLOR_FIND_HL]);
		}
	}
	
	char const *find_text = "Find...", *replace_with_text = "Replace with";
	float text_width = 0;
	text_get_size(font_bold, replace ? replace_with_text : find_text, &text_width, NULL);
	

	Rect find_buffer_bounds = rect4(x1 + text_width + padding, y1, x2 - padding, y1 + char_height);
	Rect replace_buffer_bounds = rect_translate(find_buffer_bounds, V2(0, char_height + padding));

	gl_geometry_rect(menu_bounds, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(menu_bounds, 1, colors[COLOR_BORDER]);
	
	button_render(ted, button_prev, prev_text, colors[COLOR_TEXT]);
	button_render(ted, button_next, next_text, colors[COLOR_TEXT]);
	if (replace) {
		button_render(ted, button_replace, replace_text, colors[COLOR_TEXT]);
		button_render(ted, button_replace_find, replace_find_text, colors[COLOR_TEXT]);
		button_render(ted, button_replace_all, replace_all_text, colors[COLOR_TEXT]);
	}
	
	{
		float w = 0, h = 0;
		char str[32];
		u32 match_idx = find_match_idx(ted);
		if (match_idx == U32_MAX) {
			strbuf_printf(str, U32_FMT " matches", arr_len(ted->find_results));
		} else if (buffer->selection) {
			strbuf_printf(str, U32_FMT " of " U32_FMT, match_idx + 1, arr_len(ted->find_results));
		}
		text_get_size(font, str, &w, &h);
		text_utf8(font, str, x2 - w, rect_ymid(find_buffer_bounds) - h * 0.5f, colors[COLOR_TEXT]);
		x2 -= w;
		find_buffer_bounds.size.x -= w;
	}

	text_utf8(font_bold, find_text, x1, y1, colors[COLOR_TEXT]);
	y1 += char_height_bold + padding;
	
	if (replace) {
		text_utf8(font_bold, replace_with_text, x1, y1, colors[COLOR_TEXT]);
		y1 += char_height_bold + padding;
	}
	
	gl_geometry_draw();
	text_render(font_bold);
	
	x = x1;
	x += checkbox_frame(ted, &ted->find_case_sensitive, "Case sensitive", V2(x, y1)).x + 2*padding;
	x += checkbox_frame(ted, &ted->find_regex, "Regular expression", V2(x, y1)).x + 2*padding;

	if (replace) {
		// check if the find or replace line buffer was clicked on
		for (u32 i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
			v2 point = ted->mouse_clicks[SDL_BUTTON_LEFT][i];
			if (rect_contains_point(find_buffer_bounds, point))
				ted->active_buffer = find_buffer;
			else if (rect_contains_point(replace_buffer_bounds, point))
				ted->active_buffer = replace_buffer;
			
		}
	}

	buffer_render(find_buffer, find_buffer_bounds);
	if (replace) buffer_render(replace_buffer, replace_buffer_bounds);
	
	String32 term = buffer_get_line(find_buffer, 0);
	
	if (ted->find_invalid_pattern)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_NO] & 0xFFFFFF3F); // invalid regex
	else if (term.len && !ted->find_results)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_CANCEL] & 0xFFFFFF3F); // no matches
	gl_geometry_draw();

}

static void find_open(Ted *ted, bool replace) {
	if (!ted->find && ted->active_buffer) {
		ted->prev_active_buffer = ted->active_buffer;
		ted->active_buffer = &ted->find_buffer;
		ted->find = true;
		buffer_select_all(ted->active_buffer);
	}
	if (!ted->replace && replace) {
		ted->replace = true;
	}
	find_update(ted, true);
}

static void find_close(Ted *ted) {
	ted->find = false;
	ted->active_buffer = ted->prev_active_buffer;
	find_free_pattern(ted);
}
