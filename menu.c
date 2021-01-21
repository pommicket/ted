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

// where is the ith entry in the file selector on the screen?
// returns false if it's completely offscreen
static bool file_selector_entry_pos(Ted const *ted, FileSelector const *fs,
	u32 i, Rect *r) {
	Rect bounds = fs->bounds;
	float char_height = text_font_char_height(ted->font);
	*r = rect(V2(bounds.pos.x, bounds.pos.y + char_height * (float)i), 
		V2(bounds.size.x, char_height));
	return rect_clip_to_rect(r, bounds);
}

// clear the entries in the file selector
static void file_selector_clear_entries(FileSelector *fs) {
	for (u32 i = 0; i < fs->n_entries; ++i) {
		free(fs->entries[i].name);
	}
	free(fs->entries);
	fs->entries = NULL;
	fs->n_entries = 0;
}

static void file_selector_free(FileSelector *fs) {
	file_selector_clear_entries(fs);
}

// returns the entry of the selected file, or a NULL entry (check .name == NULL)
// if none was selected
static FileEntry file_selector_update(Ted *ted, FileSelector *fs, String32 const search_term32) {
	char *search_term = search_term32.len ? str32_to_utf8_cstr(search_term32) : NULL;
	
	// check if an entry was clicked on
	for (u32 i = 0; i < fs->n_entries; ++i) {
		Rect r = {0};
		if (file_selector_entry_pos(ted, fs, i, &r)) {
			for (u32 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
				if (rect_contains_point(r, ted->mouse_clicks[SDL_BUTTON_LEFT][c])) {
					// this option was selected
					free(search_term);
					return fs->entries[i];
				}
			}
		} else break;
	}
	
	// free previous entries
	file_selector_clear_entries(fs);
	// get new entries
	char **files = fs_list_directory(".");
	if (files) {
		u32 nfiles;
		for (nfiles = 0; files[nfiles]; ++nfiles);
		if (search_term && *search_term) {
			// filter entries based on search term
			u32 in, out = 0;
			for (in = 0; in < nfiles; ++in) {
				if (stristr(files[in], search_term)) {
					free(files[out]);
					files[out++] = files[in];
				}
			}
			nfiles = out;
		}
		
		if (nfiles) {
			FileEntry *entries = ted_calloc(ted, nfiles, sizeof *entries);
			if (entries) {
				fs->n_entries = nfiles;
				fs->entries = entries;
				for (u32 i = 0; i < nfiles; ++i) {
					entries[i].name = files[i];
					entries[i].type = fs_path_type(files[i]);
				}
			}
		}
	} else {
		#if DEBUG
		static bool have_warned;
		if (!have_warned) {
			debug_println("Warning: fs_list_directory failed.");
			have_warned = true;
		}
		#endif
	}
	
	free(search_term);
	FileEntry ret = {0};
	return ret;
}

static void file_selector_render(Ted const *ted, FileSelector const *fs) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	Rect bounds = fs->bounds;
	u32 n_entries = fs->n_entries;
	FileEntry const *entries = fs->entries;
	Font *font = ted->font;
	float char_height = text_font_char_height(ted->font);
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);
	float x = x1, y = y1;
	for (u32 i = 0; i < n_entries; ++i) {
		// highlight entry user is mousing over
		if (y >= y2) break;
		Rect r = rect4(x, y, x2, minf(y + char_height, y2));
		y += char_height;
		if (rect_contains_point(r, ted->mouse_pos)) {
			glBegin(GL_QUADS);
			gl_color_rgba(colors[COLOR_MENU_HL]);
			rect_render(r);
			glEnd();
		}
	}
	
	x = x1; y = y1;
	TextRenderState text_render_state = {.min_x = x1, .max_x = x2, .min_y = y1, .max_y = y2, .render = true};
	// render file names themselves
	for (u32 i = 0; i < n_entries; ++i) {
		if (y >= y2) break;
		switch (entries[i].type) {
		case FS_FILE:
			gl_color_rgba(colors[COLOR_TEXT]);
			break;
		case FS_DIRECTORY:
			gl_color_rgba(colors[COLOR_TEXT_FOLDER]);
		default:
			gl_color_rgba(colors[COLOR_TEXT_OTHER]);
			break;
		}
		text_render_with_state(font, &text_render_state, entries[i].name, x, y);
		y += char_height;
	}
}

static void menu_update(Ted *ted, Menu menu) {
	switch (menu) {
	case MENU_NONE: break;
	case MENU_OPEN: {
		FileEntry selected_entry = file_selector_update(ted, &ted->file_selector,
			buffer_get_line(&ted->line_buffer, 0));
		if (selected_entry.name) {
			// open that file!
			if (ted_open_file(ted, selected_entry.name)) {
				menu_close(ted, false);
				file_selector_free(&ted->file_selector);
			}
		}
	} break;
	}
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

		FileSelector *fs = &ted->file_selector;
		fs->bounds = rect4(menu_x1, line_buffer_y2, menu_x2, menu_y2);
		file_selector_render(ted, fs);
	}
}
