static void menu_open(Ted *ted, Menu menu) {
	ted->menu = menu;
	ted->prev_active_buffer = ted->active_buffer;
	ted->active_buffer = NULL;

	switch (menu) {
	case MENU_NONE: assert(0); break;
	case MENU_OPEN:
		ted->active_buffer = &ted->line_buffer;
		break;
	}
}

static void menu_close(Ted *ted, bool restore_prev_active_buffer) {
	ted->menu = MENU_NONE;
	if (restore_prev_active_buffer) ted->active_buffer = ted->prev_active_buffer;
	ted->prev_active_buffer = NULL;
	
	buffer_clear(&ted->line_buffer);
}

static void menu_render(Ted *ted, Menu menu) {
	Settings *settings = &ted->settings;
	u32 *colors = settings->colors;
	Font *font = ted->font;
	float window_width = ted->window_width, window_height = ted->window_height;
	float char_height = text_font_char_height(font);

	// render backdrop
	glBegin(GL_QUADS);
	gl_color_rgba(colors[COLOR_MENU_BACKDROP]);
	rect_render(rect(V2(0, 0), V2(window_width, window_height)));
	glEnd();

	
	if (menu == MENU_OPEN) {
		char *search_term = str32_to_utf8_cstr(buffer_get_line(&ted->line_buffer, 0));
		char const *directory = ".";
		float padding = 20;
		float menu_x1 = window_width * 0.5f - 300;
		float menu_x2 = window_width * 0.5f + 300;
		menu_x1 = maxf(menu_x1, padding);
		menu_x2 = minf(menu_x2, window_width - padding);
		float menu_y1 = padding;
		float menu_y2 = window_height - padding;
		Rect menu_rect = rect4(menu_x1, menu_y1, menu_x2, menu_y2);
		float inner_padding = 10;

		// menu rectangle & border
		glBegin(GL_QUADS);
		gl_color_rgba(colors[COLOR_MENU_BG]);
		rect_render(menu_rect);
		gl_color_rgba(colors[COLOR_BORDER]);
		rect_render_border(menu_rect, settings->border_thickness);
		glEnd();
		
		menu_x1 += inner_padding;
		menu_y1 += inner_padding;
		menu_x2 -= inner_padding;
		menu_y2 -= inner_padding;
		
		float line_buffer_height = char_height * 1.5f;
		float line_buffer_x1 = menu_x1,
			line_buffer_y1 = menu_y1,
			line_buffer_x2 = menu_x2,
			line_buffer_y2 = line_buffer_y1 + line_buffer_height;

		buffer_render(&ted->line_buffer, line_buffer_x1, line_buffer_y1, line_buffer_x2, line_buffer_y2);

		char **files = fs_list_directory(directory);
		if (files) {

			u32 nfiles = 0;
			for (char **p = files; *p; ++p) ++nfiles;
			
			if (search_term && *search_term) {
				// filter files based on search term
				u32 in, out = 0;
				for (in = 0; in < nfiles; ++in) {
					if (stristr(files[in], search_term)) {
						files[out++] = files[in];
					}
				}
				nfiles = out;
			}

			qsort(files, nfiles, sizeof *files, str_qsort_case_insensitive_cmp);
			char const *file_to_open = NULL;
			{ // render file names
				float start_x = menu_x1, start_y = line_buffer_y2 + inner_padding;
				float x = start_x, y = start_y;
				
				for (u32 i = 0; i < nfiles; ++i) {
					// highlight entry user is mousing over
					if (y >= menu_y2) break;
					Rect r = rect4(x, y, menu_x2, minf(y + char_height, menu_y2));
					y += char_height;
					if (rect_contains_point(r, ted->mouse_pos)) {
						glBegin(GL_QUADS);
						gl_color_rgba(colors[COLOR_MENU_HL]);
						rect_render(r);
						glEnd();
					}
					for (u32 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
						if (rect_contains_point(r, ted->mouse_clicks[SDL_BUTTON_LEFT][c])) {
							// this file got clicked on!
							file_to_open = files[i];
						}
					}
				}
				x = start_x, y = start_y;
				TextRenderState text_render_state = {.min_x = menu_x1, .max_x = menu_x2, .min_y = menu_y1, .max_y = menu_y2, .render = true};
				gl_color_rgba(colors[COLOR_TEXT]);
				for (u32 i = 0; i < nfiles; ++i) {
					if (y >= menu_y2) break;
					text_render_with_state(font, &text_render_state, files[i], x, y);
					y += char_height;
				}
			}

			if (file_to_open) {
				ted_open_file(ted, file_to_open);
				menu_close(ted, false);
			}
	
			for (u32 i = 0; i < nfiles; ++i) free(files[i]);
			free(files);
		}
		free(search_term);
	}
}
