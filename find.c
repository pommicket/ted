static void find_open(Ted *ted) {
	if (ted->active_buffer) {
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
}

static float find_menu_height(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);
	Settings const *settings = &ted->settings;
	float padding = settings->padding;

	return char_height_bold + char_height + 2 * padding;
}

#define FIND_MAX_GROUPS 50

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
	
	String32 term = buffer_get_line(find_buffer, 0);
	if (term.len) {
		pcre2_match_data *match_data = pcre2_match_data_create(FIND_MAX_GROUPS, NULL);
		if (match_data) {
			PCRE2_SIZE *groups = pcre2_get_ovector_pointer(match_data);
			// compile search term
			int error = 0; PCRE2_SIZE error_pos = 0;
			pcre2_code *code = pcre2_compile(term.str, term.len, PCRE2_LITERAL, &error, &error_pos, NULL);
			if (code) {
				if (find_buffer->modified) { // if search term has been changed,
					// recompute match count
					BufferPos best_scroll_candidate = {U32_MAX, U32_MAX}; // pos we will scroll to (scroll to first match)
					BufferPos cursor_pos = buffer->cursor_pos;
					
					u32 match_count = 0;
					for (u32 line_idx = 0, end = buffer->nlines; line_idx < end; ++line_idx) {
						Line *line = &buffer->lines[line_idx];
						char32_t *str = line->str;
						u32 len = line->len;
						u32 start_index = 0;
						while (start_index < len) {
							int ret = pcre2_match(code, str, len, start_index, 0, match_data, NULL);
							if (ret > 0) {
								// a match!
								
								BufferPos match_start_pos = {.line = line_idx, .index = (u32)groups[0]};
								if (best_scroll_candidate.line == U32_MAX 
								|| (buffer_pos_cmp(best_scroll_candidate, cursor_pos) < 0 && buffer_pos_cmp(match_start_pos, cursor_pos) >= 0))
									best_scroll_candidate = match_start_pos;
									
								u32 match_end = (u32)groups[1];
								++match_count;
								start_index = match_end;
							} else break;
						}
					}
					ted->find_match_count = match_count;
					find_buffer->modified = false;
					if (best_scroll_candidate.line != U32_MAX)
						buffer_scroll_to_pos(buffer, best_scroll_candidate);
				}
				
				// highlight matches
				for (u32 line_idx = buffer_first_rendered_line(buffer), end = buffer_last_rendered_line(buffer);
					line_idx < end; ++line_idx) {
					Line *line = &buffer->lines[line_idx];
					char32_t *str = line->str;
					u32 len = line->len;
					u32 start_index = 0;
					while (start_index < len) {
						int ret = pcre2_match(code, str, len, start_index, 0, match_data, NULL);
						if (ret > 0) {
							u32 match_start = (u32)groups[0];
							u32 match_end = (u32)groups[1];
							BufferPos p1 = {.line = line_idx, .index = match_start};
							BufferPos p2 = {.line = line_idx, .index = match_end};
							v2 pos1 = buffer_pos_to_pixels(buffer, p1);
							v2 pos2 = buffer_pos_to_pixels(buffer, p2);
							pos2.y += char_height;
							gl_geometry_rect(rect4(pos1.x, pos1.y, pos2.x, pos2.y), colors[COLOR_FIND_HL]);
							start_index = match_end;
						} else break;
					}
				}
				pcre2_code_free(code);
				ted->find_invalid_pattern = false;
			} else {
				ted->find_invalid_pattern = true;
			}
			pcre2_match_data_free(match_data);
		}
	} else if (find_buffer->modified) {
		ted->find_match_count = 0;
		ted->find_invalid_pattern = false;
		buffer_scroll_to_cursor(buffer);
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

	buffer_render(&ted->find_buffer, find_buffer_bounds);
	
	if (ted->find_invalid_pattern)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_NO] & 0xFFFFFF3F); // invalid regex
	else if (term.len && ted->find_match_count == 0)
		gl_geometry_rect(find_buffer_bounds, colors[COLOR_CANCEL] & 0xFFFFFF3F); // no matches
	gl_geometry_draw();

}


// go to next find result
static void find_next(Ted *ted) {
	TextBuffer *buffer = ted->prev_active_buffer;
	
	// create match data
	pcre2_match_data *match_data = pcre2_match_data_create(FIND_MAX_GROUPS, NULL);
	if (match_data) {
		String32 term = buffer_get_line(&ted->find_buffer, 0);
		int error = 0;
		PCRE2_SIZE error_pos = 0;
		// compile the search term
		pcre2_code *code = pcre2_compile(term.str, term.len, PCRE2_LITERAL, &error, &error_pos, NULL);
		if (code) {
			// do the searching
			BufferPos pos = buffer->cursor_pos;
			size_t nsearches = 0;
			u32 nlines = buffer->nlines;

			// we need to search the starting line twice, because we might start at a non-zero index
			while (nsearches < nlines + 1) {
				Line *line = &buffer->lines[pos.line];
				char32_t *str = line->str;
				size_t len = line->len;
				int ret = pcre2_match(code, str, len, pos.index, 0, match_data, NULL);
				if (ret > 0) {
					PCRE2_SIZE *groups = pcre2_get_ovector_pointer(match_data);
					u32 match_start = (u32)groups[0];
					u32 match_end = (u32)groups[1];
					BufferPos pos_start = {.line = pos.line, .index = match_start};
					BufferPos pos_end = {.line = pos.line, .index = match_end};
					buffer_cursor_move_to_pos(buffer, pos_start);
					buffer_select_to_pos(buffer, pos_end);
					break;
				}

				++nsearches;
				pos.index = 0;
				pos.line += 1;
				pos.line %= nlines;
			}
			pcre2_code_free(code);
		} else {
			char32_t buf[256] = {0};
			size_t len = (size_t)pcre2_get_error_message(error, buf, sizeof buf - 1);
			char *error_cstr = str32_to_utf8_cstr(str32(buf, len));
			if (error_cstr) {
				ted_seterr(ted, "Invalid search term (position %zu): %s.", (size_t)error_pos, error_cstr);
				free(error_cstr);
			}
		}
		pcre2_match_data_free(match_data);
	} else {
		ted_seterr(ted, "Out of memory.");
	}
}
