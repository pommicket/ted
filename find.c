// @TODO : show ted->find_match_pos

#define FIND_MAX_GROUPS 50

static bool find_compile_pattern(Ted *ted, bool error_on_invalid_pattern) {
	TextBuffer *find_buffer = &ted->find_buffer;
	String32 term = buffer_get_line(find_buffer, 0);
	if (term.len) {
		pcre2_match_data *match_data = pcre2_match_data_create(FIND_MAX_GROUPS, NULL);
		if (match_data) {
			int error = 0;
			PCRE2_SIZE error_pos = 0;
			pcre2_code *code = pcre2_compile(term.str, term.len, PCRE2_LITERAL, &error, &error_pos, NULL);
			if (code) {
				ted->find_code = code;
				ted->find_match_data = match_data;
				ted->find_invalid_pattern = false;
				return true;
			} else {
				ted->find_invalid_pattern = true;
				if (error_on_invalid_pattern) {
					char32_t buf[256] = {0};
					size_t len = (size_t)pcre2_get_error_message(error, buf, sizeof buf - 1);
					char *error_cstr = str32_to_utf8_cstr(str32(buf, len));
					if (error_cstr) {
						ted_seterr(ted, "Invalid search term (position %zu): %s.", (size_t)error_pos, error_cstr);
						free(error_cstr);
					}
				}
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
}

static void find_open(Ted *ted) {
	if (!ted->find && ted->active_buffer) {
		ted->prev_active_buffer = ted->active_buffer;
		ted->active_buffer = &ted->find_buffer;
		ted->find = true;
		ted->find_match_count = 0;
		buffer_clear(&ted->find_buffer);
	}
}

static void find_close(Ted *ted) {
	ted->find = false;
	ted->active_buffer = ted->prev_active_buffer;
	find_free_pattern(ted);
}

static float find_menu_height(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);
	Settings const *settings = &ted->settings;
	float padding = settings->padding;

	return char_height_bold + char_height + 2 * padding;
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
				last_pos = (u32)groups[1];
			} else break;
		}
	}
	if (ret > 0) {
		if (match_start) *match_start = (u32)groups[0];
		if (match_end)   *match_end   = (u32)groups[1];
		pos->index = (u32)groups[1];
		return true;
	} else {
		pos->line += buffer->nlines + direction;
		pos->line %= buffer->nlines;
		if (direction == +1)
			pos->index = 0;
		else
			pos->index = (u32)buffer_get_line(buffer, pos->line).len;
		return false;
	}
}

static void find_update(Ted *ted, bool error_on_invalid_pattern) {
	TextBuffer *find_buffer = &ted->find_buffer;
	if (!find_buffer->modified) return;
	
	TextBuffer *buffer = ted->prev_active_buffer;

	find_free_pattern(ted);
	if (find_compile_pattern(ted, error_on_invalid_pattern)) {
		BufferPos pos = buffer_start_of_file(buffer);
		BufferPos best_scroll_candidate = {U32_MAX, U32_MAX};
		BufferPos cursor_pos = buffer->cursor_pos;
		u32 match_count = 0;
		// count number of matches
		for (u32 nsearches = 0; nsearches < buffer->nlines; ++nsearches) {
			u32 match_start;
			while (find_match(ted, &pos, &match_start, NULL, +1)) {
				BufferPos match_start_pos = {.line = pos.line, .index = match_start};
				if (best_scroll_candidate.line == U32_MAX 
				|| (buffer_pos_cmp(best_scroll_candidate, cursor_pos) < 0 && buffer_pos_cmp(match_start_pos, cursor_pos) >= 0))
					best_scroll_candidate = match_start_pos;
				++match_count;
			}
		}
		ted->find_match_count = match_count;
		find_buffer->modified = false;
		if (best_scroll_candidate.line != U32_MAX) // scroll to first match (if there is one)
			buffer_scroll_to_pos(buffer, best_scroll_candidate);
	} else {
		ted->find_match_count = 0;
		buffer_scroll_to_cursor(buffer);
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
	
	TextBuffer *buffer = ted->prev_active_buffer, *find_buffer = &ted->find_buffer;
	assert(buffer);
	
	u32 first_rendered_line = buffer_first_rendered_line(buffer);
	u32 last_rendered_line = buffer_last_rendered_line(buffer);
	BufferPos pos = {first_rendered_line, 0};
	
	find_update(ted, false);
	
	for (u32 nsearches = 0; nsearches < buffer->nlines && pos.line >= first_rendered_line && pos.line <= last_rendered_line; ++nsearches) {
		// highlight matches

		u32 match_start, match_end;
		while (find_match(ted, &pos, &match_start, &match_end, +1)) {
			BufferPos p1 = {.line = pos.line, .index = match_start};
			BufferPos p2 = {.line = pos.line, .index = match_end};
			v2 pos1 = buffer_pos_to_pixels(buffer, p1);
			v2 pos2 = buffer_pos_to_pixels(buffer, p2);
			pos2.y += char_height;
			Rect hl_rect = rect4(pos1.x, pos1.y, pos2.x, pos2.y);
			if (buffer_clip_rect(buffer, &hl_rect))
				gl_geometry_rect(hl_rect, colors[COLOR_FIND_HL]);
		}
	}
	
	
	float x1 = padding, y1 = window_height - menu_height, x2 = window_width - padding, y2 = window_height - padding;

	Rect menu_bounds = rect4(x1, y1, x2, y2);
	Rect find_buffer_bounds = rect4(x1 + padding, y1 + char_height_bold, x2 - padding, y1 + char_height_bold + char_height);

	gl_geometry_rect(menu_bounds, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(menu_bounds, 1, colors[COLOR_BORDER]);
	{
		float w = 0, h = 0;
		char str[32];
		strbuf_printf(str, U32_FMT " matches", ted->find_match_count);
		text_get_size(font, str, &w, &h);
		text_utf8(font, str, x2 - (w + padding), rect_ymid(find_buffer_bounds) - h * 0.5f, colors[COLOR_TEXT]);
		x2 -= w + padding;
		find_buffer_bounds.size.x -= w + padding;
	}

	text_utf8(font_bold, "Find...", x1 + padding, y1, colors[COLOR_TEXT]);

	y1 += char_height_bold;
	
	gl_geometry_draw();
	text_render(font_bold);

	buffer_render(find_buffer, find_buffer_bounds);
	
	String32 term = buffer_get_line(find_buffer, 0);
	
	if (ted->find_invalid_pattern)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_NO] & 0xFFFFFF3F); // invalid regex
	else if (term.len && ted->find_match_count == 0)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_CANCEL] & 0xFFFFFF3F); // no matches
	gl_geometry_draw();

}


static void find_next_in_direction(Ted *ted, int direction) {
	TextBuffer *buffer = ted->prev_active_buffer;
	
	BufferPos pos = direction == +1 || !buffer->selection ? buffer->cursor_pos : buffer->selection_pos;
	u32 nlines = buffer->nlines;
	
	find_update(ted, true);
	
	// we need to search the starting line twice, because we might start at a non-zero index
	for (size_t nsearches = 0; nsearches < nlines + 1; ++nsearches) {
		u32 match_start, match_end;
		if (find_match(ted, &pos, &match_start, &match_end, direction)) {
			BufferPos pos_start = {.line = pos.line, .index = match_start};
			BufferPos pos_end = {.line = pos.line, .index = match_end};
			buffer_cursor_move_to_pos(buffer, pos_start);
			buffer_select_to_pos(buffer, pos_end);
			ted->find_match_pos += ted->find_match_count + direction;
			ted->find_match_pos %= ted->find_match_count;
			break;
		}
	}
}

// go to next find result
static void find_next(Ted *ted) {
	find_next_in_direction(ted, +1);
}

static void find_prev(Ted *ted) {
	find_next_in_direction(ted, -1);
}