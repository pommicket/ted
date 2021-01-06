
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
		char const *directory = ".";
		float padding = 20;
		float menu_x1 = window_width * 0.5f - 300;
		float menu_x2 = window_width * 0.5f + 300;
		menu_x1 = maxf(menu_x1, padding);
		menu_x2 = minf(menu_x2, window_width - padding);
		float menu_y1 = padding;
		float menu_y2 = window_height - padding;
		Rect menu_rect = rect4(menu_x1, menu_y1, menu_x2, menu_y2);

		// menu rectangle & border
		glBegin(GL_QUADS);
		gl_color_rgba(colors[COLOR_MENU_BG]);
		rect_render(menu_rect);
		gl_color_rgba(colors[COLOR_BORDER]);
		rect_render_border(menu_rect, settings->border_thickness);
		glEnd();

		char **files = fs_list_directory(directory);
		if (files) {
			u32 nfiles = 0;
			for (char **p = files; *p; ++p) ++nfiles;
			qsort(files, nfiles, sizeof *files, str_qsort_case_insensitive_cmp);

			{ // render file names
				float x = menu_x1 + 10, y = menu_y1 + char_height * 0.75f + 10;
				TextRenderState text_render_state = {.min_x = menu_x1, .max_x = menu_x2, .min_y = menu_y1, .max_y = menu_y2};
				gl_color_rgba(colors[COLOR_TEXT]);
				for (u32 i = 0; i < nfiles; ++i) {
					text_render_with_state(font, &text_render_state, files[i], x, y);
					y += char_height;
				}
			}

			for (u32 i = 0; i < nfiles; ++i) free(files[i]);
			free(files);
		}
		
	}
}
