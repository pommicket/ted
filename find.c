static void find_open(Ted *ted) {
	ted->prev_active_buffer = ted->active_buffer;
	ted->active_buffer = &ted->find_buffer;
	ted->find = true;
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

static void find_menu_frame(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float const char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);

	Settings const *settings = &ted->settings;
	float const padding = settings->padding;
	float const menu_height = find_menu_height(ted);
	float const window_width = ted->window_width, window_height = ted->window_height;
	u32 const *colors = settings->colors;

	float x1 = padding, y1 = window_height - menu_height, x2 = window_width - padding, y2 = window_height - padding;
	Rect menu_bounds = rect4(x1, y1, x2, y2);

	gl_geometry_rect_border(menu_bounds, 1, colors[COLOR_BORDER]);

	gl_geometry_draw();

	text_utf8(font_bold, "Find...", x1 + padding, y1, colors[COLOR_TEXT]);
	text_render(font_bold);

	y1 += char_height_bold;

	buffer_render(&ted->find_buffer, rect4(x1 + padding, y1, x2 - padding, y1 + char_height));
}

#define FIND_MAX_GROUPS 100

// go to next find result
static void find_next(Ted *ted) {
	TextBuffer *buffer = ted->prev_active_buffer;
	if (buffer) {
		// create match data
		pcre2_match_data *match_data = pcre2_match_data_create(FIND_MAX_GROUPS, NULL);
		if (match_data) {
			String32 term = buffer_get_line(&ted->find_buffer, 0);
			int error = 0;
			size_t error_pos = 0;
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
			} else {
				char32_t buf[256] = {0};
				size_t len = (size_t)pcre2_get_error_message(error, buf, sizeof buf - 1);
				char *error_cstr = str32_to_utf8_cstr(str32(buf, len));
				if (error_cstr) {
					ted_seterr(ted, "Invalid search term: (at position %zu) %s.", (size_t)error_pos, error_cstr);
					free(error_cstr);
				}
			}
			pcre2_match_data_free(match_data);
		} else {
			ted_seterr(ted, "Out of memory.");
		}
	}
}
