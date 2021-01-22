// where is the ith entry in the file selector on the screen?
// returns false if it's completely offscreen
static bool file_selector_entry_pos(Ted const *ted, FileSelector const *fs,
	u32 i, Rect *r) {
	Rect bounds = fs->bounds;
	float char_height = text_font_char_height(ted->font);
	*r = rect(V2(bounds.pos.x, bounds.pos.y 
		+ char_height // make room for cwd
		+ char_height * 1.5f // make room for line buffer
		+ char_height * (float)i), 
		V2(bounds.size.x, char_height));
	return rect_clip_to_rect(r, bounds);
}

// clear the entries in the file selector
static void file_selector_clear_entries(FileSelector *fs) {
	for (u32 i = 0; i < fs->n_entries; ++i) {
		free(fs->entries[i].name);
		free(fs->entries[i].path);
	}
	free(fs->entries);
	fs->entries = NULL;
	fs->n_entries = 0;
}

static void file_selector_free(FileSelector *fs) {
	file_selector_clear_entries(fs);
	arr_clear(fs->cwd);
}

static int qsort_file_entry_cmp(void const *av, void const *bv) {
	FileEntry const *a = av, *b = bv;
	// put directories first
	if (a->type > b->type) {
		return -1;
	}
	if (a->type < b->type) {
		return +1;
	}
	return strcmp_case_insensitive(a->name, b->name);
}

// change directory of file selector.
void file_selector_cd(Ted *ted, FileSelector *fs, char const *path) {
	// @TODO: handle .. properly
	if (path[0] == PATH_SEPARATOR
#if _WIN32
	// check for, e.g. C:\ at start of path
		|| (strlen(path) >= 3 && path[1] == ':' && path[2] == '\\')
#endif
	) {
		// this is an absolute path. discard our previous cwd.
		arr_clear(fs->cwd);
	}
	if (strlen(fs->cwd) > 0 && fs->cwd[strlen(fs->cwd) - 1] != PATH_SEPARATOR) {
		// add path separator to end if not already there
		arr_append_str(fs->cwd, PATH_SEPARATOR_STR);
	}
	arr_append_str(fs->cwd, path);

	// clear search term
	buffer_clear(&ted->line_buffer);
}

// returns the name of the selected file, or NULL
// if none was selected. the returned pointer should be freed.
static char *file_selector_update(Ted *ted, FileSelector *fs) {
	String32 search_term32 = buffer_get_line(&ted->line_buffer, 0);
	if (!fs->cwd) {
		// set the file selector's directory to our current directory.
		arr_append_str(fs->cwd, ted->cwd);
	}
	char *search_term = search_term32.len ? str32_to_utf8_cstr(search_term32) : NULL;

	bool submitted = fs->submitted;
	fs->submitted = false;
	
	bool on_screen = true;
	for (u32 i = 0; i < fs->n_entries; ++i) {
		Rect r = {0};
		FileEntry *entry = &fs->entries[i];
		char *name = entry->name, *path = entry->path;
		FsType type = entry->type;

		// check if this entry was clicked on
		if (on_screen && file_selector_entry_pos(ted, fs, i, &r)) {
			for (u32 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
				if (rect_contains_point(r, ted->mouse_clicks[SDL_BUTTON_LEFT][c])) {
					// this option was selected
					switch (type) {
					case FS_FILE:
						free(search_term);
						if (path) return str_dup(path);
						break;
					case FS_DIRECTORY:
						file_selector_cd(ted, fs, name);
						break;
					default: break;
					}
				}
			}
		} else on_screen = false;
		
		// check if we submitted this entry
		if (submitted && streq(search_term, name)) {
			switch (type) {
			case FS_FILE:
				free(search_term);
				if (path) return str_dup(path);
				break;
			case FS_DIRECTORY:
				file_selector_cd(ted, fs, name);
				break;
			default: break;
			}
		}
	}
	
	// user pressed enter after typing a non-existent file into the search bar
	if (submitted) {
		// don't do anything for now
	}
	
	// free previous entries
	file_selector_clear_entries(fs);
	// get new entries
	char **files = fs_list_directory(fs->cwd);
	if (files) {
		char const *cwd = fs->cwd;
		bool cwd_has_path_sep = cwd[strlen(cwd) - 1] == PATH_SEPARATOR;
		u32 nfiles;
		for (nfiles = 0; files[nfiles]; ++nfiles);

		// filter entries
		bool increment = true;
		for (u32 i = 0; i < nfiles; i += increment, increment = true) {
			// remove if the file name does not contain the search term,
			bool remove = search_term && *search_term && !stristr(files[i], search_term);
			// or if this is just the current directory
			remove |= streq(files[i], ".");
			if (remove) {
				// remove this one
				free(files[i]);
				--nfiles;
				if (nfiles) {
					files[i] = files[nfiles];
				}
				increment = false;
			}
		}
		
		if (nfiles) {
			FileEntry *entries = ted_calloc(ted, nfiles, sizeof *entries);
			if (entries) {
				fs->n_entries = nfiles;
				fs->entries = entries;
				for (u32 i = 0; i < nfiles; ++i) {
					char *name = files[i];
					entries[i].name = name;
					// add cwd to start of file name
					size_t path_size = strlen(name) + strlen(cwd) + 3;
					char *path = ted_calloc(ted, 1, path_size);
					if (path) {
						snprintf(path, path_size - 1, "%s%s%s", cwd, cwd_has_path_sep ? "" : PATH_SEPARATOR_STR, name);
						entries[i].path = path;
						entries[i].type = fs_path_type(path);
					} else {
						entries[i].path = NULL; // what can we do?
						entries[i].type = FS_NON_EXISTENT;
					}
				}
			}
			qsort(entries, nfiles, sizeof *entries, qsort_file_entry_cmp);
		}

		free(files);
	} else {
		ted_seterr(ted, "Couldn't list directory '%s'.", fs->cwd);
	}
	
	free(search_term);
	return NULL;
}

static void file_selector_render(Ted *ted, FileSelector *fs) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	Rect bounds = fs->bounds;
	u32 n_entries = fs->n_entries;
	FileEntry const *entries = fs->entries;
	Font *font = ted->font;
	float char_height = text_font_char_height(ted->font);
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	// current working directory @TODO


	// search buffer
	float line_buffer_height = char_height * 1.5f;
	buffer_render(&ted->line_buffer, x1, y1, x2, y1 + line_buffer_height);
	y1 += line_buffer_height;


	for (u32 i = 0; i < n_entries; ++i) {
		// highlight entry user is mousing over
		Rect r;
		if (!file_selector_entry_pos(ted, fs, i, &r)) break;
		if (rect_contains_point(r, ted->mouse_pos)) {
			glBegin(GL_QUADS);
			gl_color_rgba(colors[COLOR_MENU_HL]);
			rect_render(r);
			glEnd();
		}
	}
	
	TextRenderState text_render_state = {.min_x = x1, .max_x = x2, .min_y = y1, .max_y = y2, .render = true};
	// render file names themselves
	for (u32 i = 0; i < n_entries; ++i) {
		Rect r;
		if (!file_selector_entry_pos(ted, fs, i, &r)) break;
		float x = r.pos.x, y = r.pos.y;
		switch (entries[i].type) {
		case FS_FILE:
			gl_color_rgba(colors[COLOR_TEXT]);
			break;
		case FS_DIRECTORY:
			gl_color_rgba(colors[COLOR_TEXT_FOLDER]);
			break;
		default:
			gl_color_rgba(colors[COLOR_TEXT_OTHER]);
			break;
		}
		text_render_with_state(font, &text_render_state, entries[i].name, x, y);
	}
}

