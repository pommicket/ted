#if __unix__
#include <fcntl.h>
#endif

static float file_selector_entries_start_y(Ted const *ted, FileSelector const *fs) {
	Rect bounds = fs->bounds;
	float padding = ted->settings.padding;
	float char_height = text_font_char_height(ted->font);
	float char_height_bold = text_font_char_height(ted->font_bold);
	return bounds.pos.y
		+ char_height_bold + padding // make room for cwd
		+ char_height * 1.5f; // make room for line buffer
}

// number of file entries that can be displayed on the screen
static u32 file_selector_n_display_entries(Ted const *ted, FileSelector const *fs) {
	float char_height = text_font_char_height(ted->font);
	float entries_h = rect_y2(fs->bounds) - file_selector_entries_start_y(ted, fs);
	return (u32)(entries_h / char_height);
}

static void file_selector_clamp_scroll(Ted const *ted, FileSelector *fs) {
	float max_scroll = (float)fs->n_entries - (float)file_selector_n_display_entries(ted, fs);
	if (max_scroll < 0) max_scroll = 0;
	fs->scroll = clampf(fs->scroll, 0, max_scroll);
}

static void file_selector_scroll_to_selected(Ted const *ted, FileSelector *fs) {
	u32 n_display_entries = file_selector_n_display_entries(ted, fs);
	float scrolloff = ted->settings.scrolloff;
	float min_scroll = (float)fs->selected - ((float)n_display_entries - scrolloff);
	float max_scroll = (float)fs->selected - scrolloff;
	fs->scroll = clampf(fs->scroll, min_scroll, max_scroll);
	file_selector_clamp_scroll(ted, fs);
}

// where is the ith entry in the file selector on the screen?
// returns false if it's completely offscreen
static bool file_selector_entry_pos(Ted const *ted, FileSelector const *fs,
	u32 i, Rect *r) {
	Rect bounds = fs->bounds;
	float char_height = text_font_char_height(ted->font);
	*r = rect(V2(bounds.pos.x, file_selector_entries_start_y(ted, fs)
		- char_height * fs->scroll
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
	memset(fs, 0, sizeof *fs);
}

static void file_selector_up(Ted const *ted, FileSelector *fs, i64 n) {
	i64 selected = fs->selected - n;
	selected = mod_i64(selected, fs->n_entries);
	fs->selected = (u32)selected;
	file_selector_scroll_to_selected(ted, fs);
}

static void file_selector_down(Ted const *ted, FileSelector *fs, i64 n) {
	i64 selected = fs->selected + n;
	selected = mod_i64(selected, fs->n_entries);
	fs->selected = (u32)selected;
	file_selector_scroll_to_selected(ted, fs);
}

static int qsort_file_entry_cmp(void const *av, void const *bv, void *search_termv) {
	char const *search_term = search_termv;
	FileEntry const *a = av, *b = bv;
	// put directories first
	if (a->type == FS_DIRECTORY && b->type != FS_DIRECTORY) {
		return -1;
	}
	if (a->type != FS_DIRECTORY && b->type == FS_DIRECTORY) {
		return +1;
	}
	if (search_term) {
		bool a_prefix = str_is_prefix(a->name, search_term);
		bool b_prefix = str_is_prefix(b->name, search_term);
		if (a_prefix && !b_prefix) {
			return -1;
		}
		if (b_prefix && !a_prefix) {
			return +1;
		}
	}
	
	return strcmp_case_insensitive(a->name, b->name);
}

static Status file_selector_cd_(Ted const *ted, FileSelector *fs, char const *path, int symlink_depth);

// cd to the directory `name`. `name` cannot include any path separators.
static Status file_selector_cd1(Ted const *ted, FileSelector *fs, char const *name, size_t name_len, int symlink_depth) {
	char *const cwd = fs->cwd;

	if (name_len == 0 || (name_len == 1 && name[0] == '.')) {
		// no name, or .
		return true;
	}

	if (name_len == 1 && name[0] == '~') {
		// just in case the user's HOME happens to be accidentally set to, e.g. '/foo/~', make
		// sure we don't recurse infinitely
		if (symlink_depth < 32) {
			return file_selector_cd_(ted, fs, ted->home, symlink_depth + 1);
		} else {
			return false;
		}
	}

	if (name_len == 2 && name[0] == '.' && name[1] == '.') {
		// ..
		char *last_sep = strrchr(cwd, PATH_SEPARATOR);
		if (last_sep) {
			if (last_sep == cwd // this is the starting "/" of a path
			#if _WIN32
				|| (last_sep == cwd + 2 && cwd[1] == ':') // this is the \ of C:\  .
			#endif
				) {
				last_sep[1] = '\0'; // include the last separator
			} else {
				last_sep[0] = '\0';
			}
		}
	} else {
		char path[TED_PATH_MAX];
		// join fs->cwd with name to get full path
		str_printf(path, TED_PATH_MAX, "%s%s%*s", cwd, 
				cwd[strlen(cwd) - 1] == PATH_SEPARATOR ?
				"" : PATH_SEPARATOR_STR,
				(int)name_len, name);
		if (fs_path_type(path) != FS_DIRECTORY) {
			// trying to cd to something that's not a directory!
			return false;
		}

		#if __unix__
		if (symlink_depth < 32) { // on my system, MAXSYMLINKS is 20, so this should be plenty
			char link_to[TED_PATH_MAX];
			ssize_t bytes = readlink(path, link_to, sizeof link_to);
			if (bytes != -1) {
				// this is a symlink
				link_to[bytes] = '\0';
				return file_selector_cd_(ted, fs, link_to, symlink_depth + 1);
			}
		} else {
			return false;
		}
		#else
		(void)symlink_depth;
		#endif

		// add path separator to end if not already there (which could happen in the case of /)
		if (cwd[strlen(cwd) - 1] != PATH_SEPARATOR)
			str_cat(cwd, sizeof fs->cwd, PATH_SEPARATOR_STR);
		// add name itself
		strn_cat(cwd, sizeof fs->cwd, name, name_len);
	}
	return true;
	
}

static Status file_selector_cd_(Ted const *ted, FileSelector *fs, char const *path, int symlink_depth) {
	char *const cwd = fs->cwd;
	if (path[0] == '\0') return true;

	if (path[0] == PATH_SEPARATOR
	#if _WIN32
	|| path[1] == ':' && path[2] == PATH_SEPARATOR
	#endif
		) {
		// absolute path (e.g. /foo, c:\foo)
		// start out by replacing cwd with the start of the absolute path
		cwd[0] = '\0';
		if (path[0] == PATH_SEPARATOR) {
			str_cat(cwd, sizeof fs->cwd, PATH_SEPARATOR_STR);
			path += 1;
		}
		#if _WIN32
		else {
			strn_cat(cwd, sizeof fs->cwd, path, 3);
			path += 3;
		}
		#endif
	}

	char const *p = path;

	while (*p) {
		size_t len = strcspn(p, PATH_SEPARATOR_STR);
		if (!file_selector_cd1(ted, fs, p, len, symlink_depth))
			return false;
		p += len;
		p += strspn(p, PATH_SEPARATOR_STR);
	}
	return true;
}

// go to the directory `path`. make sure `path` only contains path separators like PATH_SEPARATOR, not any
// other members of ALL_PATH_SEPARATORS
// returns false if this path doesn't exist or isn't a directory
static bool file_selector_cd(Ted const *ted, FileSelector *fs, char const *path) {
	fs->selected = 0;
	return file_selector_cd_(ted, fs, path, 0);
}

// returns the name of the selected file, or NULL
// if none was selected. the returned pointer should be freed.
static char *file_selector_update(Ted *ted, FileSelector *fs) {
	fs->open = true;

	TextBuffer *line_buffer = &ted->line_buffer;
	String32 search_term32 = buffer_get_line(line_buffer, 0);
	char *const cwd = fs->cwd;

	if (cwd[0] == '\0') {
		// set the file selector's directory to our current directory.
		str_cpy(cwd, sizeof fs->cwd, ted->cwd);
	}
	

	// check if the search term contains a path separator. if so, cd to the dirname.
	u32 first_path_sep = U32_MAX, last_path_sep = U32_MAX;
	for (u32 i = 0; i < search_term32.len; ++i) {
		char32_t c = search_term32.str[i];
		if (c < CHAR_MAX && strchr(ALL_PATH_SEPARATORS, (char)c)) {
			last_path_sep = i;
			if (first_path_sep == U32_MAX)
				first_path_sep = i;
		}
	}

	if (last_path_sep != U32_MAX) {
		bool include_last_path_sep = last_path_sep == 0;
		String32 dir_name32 = str32_substr(search_term32, 0, last_path_sep + include_last_path_sep);
		char *dir_name = str32_to_utf8_cstr(dir_name32);
		if (dir_name) {
			// replace all members of ALL_PATH_SEPARATORS with PATH_SEPARATOR in dir_name (i.e. change / to \ on windows)
			for (char *p = dir_name; *p; ++p)
				if (strchr(ALL_PATH_SEPARATORS, *p))
					*p = PATH_SEPARATOR;

			if (file_selector_cd(ted, fs, dir_name)) {
				buffer_delete_chars_at_pos(line_buffer, buffer_start_of_file(line_buffer), last_path_sep + 1); // delete up to and including the last path separator
				buffer_clear_undo_redo(line_buffer);
			} else {
				BufferPos pos = {.line = 0, .index = first_path_sep};
				size_t nchars = search_term32.len - first_path_sep;
				buffer_delete_chars_at_pos(line_buffer, pos, (i64)nchars);
			}
			free(dir_name);
		}
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
						buffer_clear(line_buffer); // clear search term
						break;
					default: break;
					}
				}
			}
		} else on_screen = false;
		
	}
	
	// user pressed enter in search bar
	if (submitted && fs->selected < fs->n_entries) {
		FileEntry *entry = &fs->entries[fs->selected];
		switch (entry->type) {
		case FS_FILE:
			free(search_term);
			if (entry->path) return str_dup(entry->path);
			break;
		case FS_DIRECTORY:
			file_selector_cd(ted, fs, entry->name);
			buffer_clear(line_buffer); // clear search term
			break;
		default: break;
		}
	}
	
	// free previous entries
	file_selector_clear_entries(fs);
	// get new entries
	char **files;
	// if the directory we're in gets deleted, go back a directory.
	for (u32 i = 0; i < 100; ++i) {
		files = fs_list_directory(cwd);
		if (files) break;
		else if (i == 0) {
			if (fs_path_type(cwd) == FS_NON_EXISTENT)
				ted_seterr(ted, "%s is not a directory.", cwd);
			else
				ted_seterr(ted, "Can't list directory %s.", cwd);
		}
		file_selector_cd(ted, fs, "..");
	}

	if (files) {
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
				if (fs->selected >= fs->n_entries) fs->selected = nfiles - 1;
				fs->entries = entries;
				for (u32 i = 0; i < nfiles; ++i) {
					char *name = files[i];
					entries[i].name = name;
					// add cwd to start of file name
					size_t path_size = strlen(name) + strlen(cwd) + 3;
					char *path = ted_calloc(ted, 1, path_size);
					if (path) {
						snprintf(path, path_size - 1, "%s%s%s", cwd, PATH_SEPARATOR_STR, name);
						entries[i].path = path;
						entries[i].type = fs_path_type(path);
					} else {
						entries[i].path = NULL; // what can we do?
						entries[i].type = FS_NON_EXISTENT;
					}
				}
			}
			qsort_with_context(entries, nfiles, sizeof *entries, qsort_file_entry_cmp, search_term);
		}

		free(files);
	} else {
		ted_seterr(ted, "Couldn't list directory '%s'.", cwd);
	}
	
	// apply scroll
	float scroll_speed = 2.5f;
	fs->scroll += scroll_speed * (float)ted->scroll_total_y;
	file_selector_clamp_scroll(ted, fs);
	
	free(search_term);
	return NULL;
}

static void file_selector_render(Ted *ted, FileSelector *fs) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	Rect bounds = fs->bounds;
	u32 n_entries = fs->n_entries;
	FileEntry const *entries = fs->entries;
	Font *font = ted->font, *font_bold = ted->font_bold;
	float padding = settings->padding;
	float char_height = text_font_char_height(font);
	float char_height_bold = text_font_char_height(font_bold);
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	// current working directory
	gl_color_rgba(colors[COLOR_TEXT]);
	text_render(font_bold, fs->cwd, x1, y1);
	y1 += char_height_bold + padding;

	// search buffer
	float line_buffer_height = char_height * 1.5f;
	buffer_render(&ted->line_buffer, x1, y1, x2, y1 + line_buffer_height);
	y1 += line_buffer_height;


	for (u32 i = 0; i < n_entries; ++i) {
		// highlight entry user is hovering over/selecting
		Rect r;
		if (!file_selector_entry_pos(ted, fs, i, &r)) break;
		if (rect_contains_point(r, ted->mouse_pos) || fs->selected == i) {
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
