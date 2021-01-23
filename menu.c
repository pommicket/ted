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
	file_selector_free(&ted->file_selector);
	buffer_clear(&ted->line_buffer);
}

// returns the rectangle of the screen coordinates of the menu
static Rect menu_rect(Ted *ted) {
	Settings *settings = &ted->settings;
	float window_width = ted->window_width, window_height = ted->window_height;
	float padding = settings->padding;
	float menu_width = settings->max_menu_width;
	menu_width = minf(menu_width, window_width - 2 * padding);
	return rect(
		V2(window_width * 0.5f - 0.5f * menu_width, padding),
		V2(menu_width, window_height - 2 * padding)
	);
}

static void menu_update(Ted *ted, Menu menu) {
	switch (menu) {
	case MENU_NONE: break;
	case MENU_OPEN: {
		char *selected_file = file_selector_update(ted, &ted->file_selector);
		if (selected_file) {
			// open that file!
			if (ted_open_file(ted, selected_file)) {
				menu_close(ted, false);
				file_selector_free(&ted->file_selector);
			}
			free(selected_file);
		}
	} break;
	}
}

static void menu_render(Ted *ted, Menu menu) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	float window_width = ted->window_width, window_height = ted->window_height;

	// render backdrop
	glBegin(GL_QUADS);
	gl_color_rgba(colors[COLOR_MENU_BACKDROP]);
	rect_render(rect(V2(0, 0), V2(window_width, window_height)));
	glEnd();

	
	if (menu == MENU_OPEN) {
		float padding = settings->padding;
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
		

		FileSelector *fs = &ted->file_selector;
		fs->bounds = rect4(menu_x1, menu_y1, menu_x2, menu_y2);
		file_selector_render(ted, fs);
	}
}
