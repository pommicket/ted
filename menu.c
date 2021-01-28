static void menu_open(Ted *ted, Menu menu) {
	ted->menu = menu;
	ted->prev_active_buffer = ted->active_buffer;
	ted->active_buffer = NULL;
	*ted->warn_overwrite = 0; // clear warn_overwrite

	switch (menu) {
	case MENU_NONE: assert(0); break;
	case MENU_OPEN:
		ted->active_buffer = &ted->line_buffer;
		break;
	case MENU_SAVE_AS:
		ted->active_buffer = &ted->line_buffer;
		ted->file_selector.create_menu = true;
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

static void menu_escape(Ted *ted) {
	if (*ted->warn_overwrite) {
		// just close "are you sure you want to overwrite?"
		*ted->warn_overwrite = 0;
		ted->active_buffer = &ted->line_buffer;
	} else {
		menu_close(ted, true);
	}
}

static float menu_get_width(Ted *ted) {
	Settings *settings = &ted->settings;
	return minf(settings->max_menu_width, ted->window_width - 2.0f * settings->padding);
}

// returns the rectangle of the screen coordinates of the menu
static Rect menu_rect(Ted *ted) {
	Settings *settings = &ted->settings;
	float window_width = ted->window_width, window_height = ted->window_height;
	float padding = settings->padding;
	float menu_width = menu_get_width(ted);
	return rect(
		V2(window_width * 0.5f - 0.5f * menu_width, padding),
		V2(menu_width, window_height - 2 * padding)
	);
}

static void menu_update(Ted *ted, Menu menu) {
	switch (menu) {
	case MENU_NONE: break;
	case MENU_SAVE_AS: {
		if (*ted->warn_overwrite) {
			switch (popup_update(ted)) {
			case POPUP_NONE:
				// no option selected
				break;
			case POPUP_YES: {
				// overwrite it!
				TextBuffer *buffer = ted->prev_active_buffer;
				if (buffer) {
					buffer_save_as(buffer, ted->warn_overwrite);
				}
				menu_close(ted, true);
			} break;
			case POPUP_NO:
				// back to the file selector
				*ted->warn_overwrite = '\0';
				ted->active_buffer = &ted->line_buffer;
				break;
			case POPUP_CANCEL:
				// close "save as" menu
				menu_close(ted, true);
				break;
			}
		} else {
			char *selected_file = file_selector_update(ted, &ted->file_selector);
			if (selected_file) {
				TextBuffer *buffer = ted->prev_active_buffer;
				if (buffer) {
					if (fs_path_type(selected_file) != FS_NON_EXISTENT) {
						// file already exists! warn about overwriting it.
						strbuf_cpy(ted->warn_overwrite, selected_file);
						ted->active_buffer = NULL;
					} else {
						// create the new file.
						buffer_save_as(buffer, selected_file);
						menu_close(ted, true);
					}
				}
				free(selected_file);
			}
		}
	} break;
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
	Font *font_bold = ted->font_bold;
	float char_height_bold = text_font_char_height(font_bold);

	// render backdrop
	glBegin(GL_QUADS);
	gl_color_rgba(colors[COLOR_MENU_BACKDROP]);
	rect_render(rect(V2(0, 0), V2(window_width, window_height)));
	glEnd();

	if (*ted->warn_overwrite) {
		char const *filename = ted->warn_overwrite;
		char title[32] = {0}, body[256] = {0};
		char const *last_path_sep = strrchr(filename, PATH_SEPARATOR);
		strbuf_printf(title, "Overwrite %s?", last_path_sep ? last_path_sep + 1 : filename);
		strbuf_printf(body, "Are you sure you want to overwrite %s?", ted->warn_overwrite);
		popup_render(ted, title, body);
		return;
	}
	
	if (menu == MENU_OPEN || menu == MENU_SAVE_AS) {
		float padding = settings->padding;
		Rect rect = menu_rect(ted);
		float menu_x1, menu_y1, menu_x2, menu_y2;
		rect_coords(rect, &menu_x1, &menu_y1, &menu_x2, &menu_y2);

		// menu rectangle & border
		glBegin(GL_QUADS);
		gl_color_rgba(colors[COLOR_MENU_BG]);
		rect_render(rect);
		gl_color_rgba(colors[COLOR_BORDER]);
		rect_render_border(rect, settings->border_thickness);
		glEnd();
		
		menu_x1 += padding;
		menu_y1 += padding;
		menu_x2 -= padding;
		menu_y2 -= padding;
		

		gl_color_rgba(colors[COLOR_TEXT]);
		if (menu == MENU_OPEN) {
			text_render(font_bold, "Open...", menu_x1, menu_y1);
		} else if (menu == MENU_SAVE_AS) {
			text_render(font_bold, "Save as...", menu_x1, menu_y1);
		}
			
		menu_y1 += char_height_bold * 0.75f + padding;

		FileSelector *fs = &ted->file_selector;
		fs->bounds = rect4(menu_x1, menu_y1, menu_x2, menu_y2);
		file_selector_render(ted, fs);
	}
}
