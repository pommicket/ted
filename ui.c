#if __unix__
#include <fcntl.h>
#endif

static float selector_entries_start_y(Ted const *ted, Selector const *s) {
	float padding = ted->settings.padding;

	return s->bounds.pos.y
		+ ted_line_buffer_height(ted) + padding; // make room for line buffer
}

// number of entries that can be displayed on the screen
static u32 selector_n_display_entries(Ted const *ted, Selector const *s) {
	float char_height = text_font_char_height(ted->font);
	float entries_h = rect_y2(s->bounds) - selector_entries_start_y(ted, s);
	return (u32)(entries_h / char_height);
}

static void selector_clamp_scroll(Ted const *ted, Selector *s) {
	float max_scroll = (float)s->n_entries - (float)selector_n_display_entries(ted, s);
	if (max_scroll < 0) max_scroll = 0;
	s->scroll = clampf(s->scroll, 0, max_scroll);
}

static void selector_scroll_to_cursor(Ted const *ted, Selector *s) {
	u32 n_display_entries = selector_n_display_entries(ted, s);
	float scrolloff = ted->settings.scrolloff;
	float min_scroll = (float)s->cursor - ((float)n_display_entries - scrolloff);
	float max_scroll = (float)s->cursor - scrolloff;
	s->scroll = clampf(s->scroll, min_scroll, max_scroll);
	selector_clamp_scroll(ted, s);
}

// where is the ith entry in the selector on the screen?
// returns false if it's completely offscreen
static bool selector_entry_pos(Ted const *ted, Selector const *s, u32 i, Rect *r) {
	Rect bounds = s->bounds;
	float char_height = text_font_char_height(ted->font);
	*r = rect(V2(bounds.pos.x, selector_entries_start_y(ted, s)
		- char_height * s->scroll
		+ char_height * (float)i), 
		V2(bounds.size.x, char_height));
	return rect_clip_to_rect(r, bounds);
}

static void selector_up(Ted const *ted, Selector *s, i64 n) {
	if (!s->enable_cursor || s->n_entries == 0) {
		// can't do anything
		return;
	}
	s->cursor = (u32)mod_i64(s->cursor - n, s->n_entries);
	selector_scroll_to_cursor(ted, s);
}

static void selector_down(Ted const *ted, Selector *s, i64 n) {
	selector_up(ted, s, -n);
}

// returns a null-terminated UTF-8 string of the option selected, or NULL if none was.
// you should call free() on the return value.
static char *selector_update(Ted *ted, Selector *s) {
	char *ret = NULL;
	TextBuffer *line_buffer = &ted->line_buffer;

	ted->selector_open = s;
	for (u32 i = 0; i < s->n_entries; ++i) {
		// check if this entry was clicked on
		Rect entry_rect;
		if (selector_entry_pos(ted, s, i, &entry_rect)) {
			for (uint c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
				if (rect_contains_point(entry_rect, ted->mouse_clicks[SDL_BUTTON_LEFT][c])) {
					// this option was selected
					ret = str_dup(s->entries[i].name);
					break;
				}
			}
		}
	}

	if (line_buffer->line_buffer_submitted) {
		line_buffer->line_buffer_submitted = false;
		if (!ret) {
			if (s->enable_cursor) {
				// select this option
				if (s->cursor < s->n_entries)
					ret = str_dup(s->entries[s->cursor].name);

			} else {
				// user typed in submission
				ret = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
			}
		}
	}

	// apply scroll
	float scroll_speed = 2.5f;
	s->scroll += scroll_speed * (float)ted->scroll_total_y;
	selector_clamp_scroll(ted, s);
	return ret;
}

// NOTE: also renders the line buffer
static void selector_render(Ted *ted, Selector *s) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	Font *font = ted->font;

	Rect bounds = s->bounds;

	for (u32 i = 0; i < s->n_entries; ++i) {
		// highlight entry user is hovering over/selecting
		Rect entry_rect;
		if (selector_entry_pos(ted, s, i, &entry_rect)) {
			rect_clip_to_rect(&entry_rect, bounds);
			if (rect_contains_point(entry_rect, ted->mouse_pos) || (s->enable_cursor && s->cursor == i)) {
				// highlight it
				gl_geometry_rect(entry_rect, colors[COLOR_MENU_HL]);
			}
		}
	}
	gl_geometry_draw();

	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);
	// search buffer
	float line_buffer_height = ted_line_buffer_height(ted);
	buffer_render(&ted->line_buffer, rect4(x1, y1, x2, y1 + line_buffer_height));
	y1 += line_buffer_height;
		
	TextRenderState text_state = text_render_state_default;
	text_state.min_x = x1;
	text_state.max_x = x2;
	text_state.min_y = y1;
	text_state.max_y = y2;
	text_state.render = true;

	// render entries themselves
	SelectorEntry *entries = s->entries;
	for (u32 i = 0; i < s->n_entries; ++i) {
		Rect r;
		if (selector_entry_pos(ted, s, i, &r)) {
			float x = r.pos.x, y = r.pos.y;
			text_state.x = x; text_state.y = y;
			rgba_u32_to_floats(entries[i].color, text_state.color);
			text_utf8_with_state(font, &text_state, entries[i].name);
		}
	}
	text_render(font);
}


// clear the entries in the file selector
static void file_selector_clear_entries(FileSelector *fs) {
	for (u32 i = 0; i < fs->n_entries; ++i) {
		free(fs->entries[i].name);
		free(fs->entries[i].path);
	}
	free(fs->entries);
	arr_free(fs->sel.entries);
	fs->entries = NULL;
	fs->n_entries = fs->sel.n_entries = 0;
}

// returns true if there are any directory entries
static bool file_selector_any_directories(FileSelector const *fs) {
	FileEntry const *entries = fs->entries;
	for (u32 i = 0, n_entries = fs->n_entries; i < n_entries; ++i) {
		if (entries[i].type == FS_DIRECTORY)
			return true;
	}
	return false;
}

static void file_selector_free(FileSelector *fs) {
	file_selector_clear_entries(fs);
	memset(fs, 0, sizeof *fs);
}

static int qsort_file_entry_cmp(void *search_termv, void const *av, void const *bv) {
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

static Status file_selector_cd_(Ted *ted, FileSelector *fs, char const *path, int symlink_depth);

// cd to the directory `name`. `name` cannot include any path separators.
static Status file_selector_cd1(Ted *ted, FileSelector *fs, char const *name, size_t name_len, int symlink_depth) {
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

static Status file_selector_cd_(Ted *ted, FileSelector *fs, char const *path, int symlink_depth) {
	char *const cwd = fs->cwd;
	if (path[0] == '\0') return true;

	if (path_is_absolute(path)) {
		// absolute path (e.g. /foo, c:\foo)
		// start out by replacing cwd with the start of the absolute path
		if (path[0] == PATH_SEPARATOR) {
			char new_cwd[TED_PATH_MAX];
			// necessary because the full path of \ on windows isn't just \, it's c:\ or something
			ted_full_path(ted, PATH_SEPARATOR_STR, new_cwd, sizeof new_cwd);
			strcpy(cwd, new_cwd);
			path += 1;
		}
		#if _WIN32
		else {
			cwd[0] = '\0';
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
static bool file_selector_cd(Ted *ted, FileSelector *fs, char const *path) {
	fs->sel.cursor = 0;
	fs->sel.scroll = 0;
	return file_selector_cd_(ted, fs, path, 0);
}

// returns the name of the selected file, or NULL
// if none was selected. the returned pointer should be freed.
static char *file_selector_update(Ted *ted, FileSelector *fs) {

	TextBuffer *line_buffer = &ted->line_buffer;
	String32 search_term32 = buffer_get_line(line_buffer, 0);
	fs->sel.enable_cursor = !fs->create_menu || search_term32.len == 0;
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
				// delete up to first path separator in line buffer
				BufferPos pos = {.line = 0, .index = first_path_sep};
				size_t nchars = search_term32.len - first_path_sep;
				buffer_delete_chars_at_pos(line_buffer, pos, (i64)nchars);
			}
			free(dir_name);
		}
	}

	char *option_chosen = selector_update(ted, &fs->sel);

	if (option_chosen) {
		char path[TED_PATH_MAX];
		strbuf_printf(path, "%s%s%s", cwd, cwd[strlen(cwd)-1] == PATH_SEPARATOR ? "" : PATH_SEPARATOR_STR, option_chosen);
		char *ret = NULL;

		switch (fs_path_type(path)) {
		case FS_NON_EXISTENT:
		case FS_OTHER:
			if (fs->create_menu)
				ret = str_dup(path); // you can only select non-existent things if this is a create menu
			break;
		case FS_FILE:
			// selected a file!
			ret = str_dup(path);
			break;
		case FS_DIRECTORY:
			// cd there
			file_selector_cd(ted, fs, option_chosen);
			buffer_clear(line_buffer);
			break;
		}
		
		free(option_chosen);
		if (ret) {
			return ret;
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

	char *search_term = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
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
				if (fs->sel.cursor >= fs->n_entries) fs->sel.cursor = nfiles - 1;
				fs->entries = entries;
				for (u32 i = 0; i < nfiles; ++i) {
					char *name = files[i];
					entries[i].name = name;
					// add cwd to start of file name
					size_t path_size = strlen(name) + strlen(cwd) + 3;
					char *path = ted_calloc(ted, 1, path_size);
					if (path) {
						snprintf(path, path_size - 1, "%s%s%s", cwd,
							cwd[strlen(cwd) - 1] == PATH_SEPARATOR ? "" : PATH_SEPARATOR_STR,
							name);
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
		// set cwd to this (if no buffers are open, the "open" menu should use the last file selector's cwd)
		strbuf_cpy(ted->cwd, cwd);
	} else {
		ted_seterr(ted, "Couldn't list directory '%s'.", cwd);
	}
	
	free(search_term);
	return NULL;
}

static void file_selector_render(Ted *ted, FileSelector *fs) {
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	Rect bounds = fs->bounds;
	Font *font = ted->font;
	float padding = settings->padding;
	float char_height = text_font_char_height(font);
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	// current working directory
	text_utf8(font, fs->cwd, x1, y1, colors[COLOR_TEXT]);
	y1 += char_height + padding;

	// render selector
	Selector *sel = &fs->sel;
	sel->bounds = rect4(x1, y1, x2, y2); // selector takes up remaining space
	arr_clear(sel->entries);
	for (u32 i = 0; i < fs->n_entries; ++i) {
		ColorSetting color = 0;
		switch (fs->entries[i].type) {
		case FS_FILE:
			color = COLOR_TEXT;
			break;
		case FS_DIRECTORY:
			color = COLOR_TEXT_FOLDER;
			break;
		default:
			color = COLOR_TEXT_OTHER;
			break;
		}
		SelectorEntry entry = {.name = fs->entries[i].name, .color = colors[color]};
		arr_add(sel->entries, entry);
	}
	if (sel->entries)
		sel->n_entries = fs->n_entries;

	selector_render(ted, sel);
}

static v2 button_get_size(Ted *ted, char const *text) {
	float border_thickness = ted->settings.border_thickness;
	return v2_add_const(text_get_size_v2(ted->font, text), 2 * border_thickness);
}

static void button_render(Ted *ted, Rect button, char const *text, u32 color) {
	u32 const *colors = ted->settings.colors;
	
	if (rect_contains_point(button, ted->mouse_pos)) {
		// highlight button when hovering over it
		u32 new_color = (color & 0xffffff00) | ((color & 0xff) / 3);
		gl_geometry_rect(button, new_color);
	}
	
	gl_geometry_rect_border(button, ted->settings.border_thickness, colors[COLOR_BORDER]);
	gl_geometry_draw();

	v2 pos = rect_center(button);
	text_utf8_anchored(ted->font, text, pos.x, pos.y, color, ANCHOR_MIDDLE);
	text_render(ted->font);
}

// returns true if the button was clicked on.
static bool button_update(Ted *ted, Rect button) {
	for (u16 i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
		if (rect_contains_point(button, ted->mouse_clicks[SDL_BUTTON_LEFT][i])) {
			return true;
		}
	}
	return false;
}

typedef enum {
	POPUP_NONE,
	POPUP_YES = 1<<1,
	POPUP_NO = 1<<2,
	POPUP_CANCEL = 1<<3,
} PopupOption;

#define POPUP_YES_NO (POPUP_YES | POPUP_NO)
#define POPUP_YES_NO_CANCEL (POPUP_YES | POPUP_NO | POPUP_CANCEL)

static void popup_get_rects(Ted const *ted, u32 options, Rect *popup, Rect *button_yes, Rect *button_no, Rect *button_cancel) {
	float window_width = ted->window_width, window_height = ted->window_height;
	
	*popup = rect_centered(V2(window_width * 0.5f, window_height * 0.5f), V2(300, 200));
	float button_height = 30;
	u16 nbuttons = util_popcount(options);
	float button_width = popup->size.x / nbuttons;
	popup->size = v2_clamp(popup->size, v2_zero, V2(window_width, window_height));
	Rect r = rect(V2(popup->pos.x, rect_y2(*popup) - button_height), V2(button_width, button_height));
	if (options & POPUP_YES) {
		*button_yes = r;
		r = rect_translate(r, V2(button_width, 0));
	}
	if (options & POPUP_NO) {
		*button_no = r;
		r = rect_translate(r, V2(button_width, 0));
	}
	if (options & POPUP_CANCEL) {
		*button_cancel = r;
		r = rect_translate(r, V2(button_width, 0));
	}	
}

static PopupOption popup_update(Ted *ted, u32 options) {
	Rect r, button_yes, button_no, button_cancel;
	popup_get_rects(ted, options, &r, &button_yes, &button_no, &button_cancel);
	if (button_update(ted, button_yes))
		return POPUP_YES;
	if (button_update(ted, button_no))
		return POPUP_NO;
	if (button_update(ted, button_cancel))
		return POPUP_CANCEL;
	return POPUP_NONE;
}

static void popup_render(Ted *ted, u32 options, char const *title, char const *body) {
	float window_width = ted->window_width;
	Font *font = ted->font;
	Font *font_bold = ted->font_bold;
	Rect r, button_yes, button_no, button_cancel;
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	float const char_height_bold = text_font_char_height(font_bold);
	float const padding = settings->padding;
	float const border_thickness = settings->border_thickness;
	
	popup_get_rects(ted, options, &r, &button_yes, &button_no, &button_cancel);
	
	
	float y = r.pos.y;
	
	// popup rectangle
	gl_geometry_rect(r, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(r, border_thickness, colors[COLOR_BORDER]);
	// line separating text from body
	gl_geometry_rect(rect(V2(r.pos.x, y + char_height_bold), V2(r.size.x, border_thickness)), colors[COLOR_BORDER]);
	
	if (options & POPUP_YES) button_render(ted, button_yes, "Yes", colors[COLOR_YES]);
	if (options & POPUP_NO) button_render(ted, button_no, "No", colors[COLOR_NO]);
	if (options & POPUP_CANCEL) button_render(ted, button_cancel, "Cancel", colors[COLOR_CANCEL]);

	// title text
	v2 title_size = {0};
	text_get_size(font_bold, title, &title_size.x, &title_size.y);
	v2 title_pos = v2_sub(V2(window_width * 0.5f, y), V2(title_size.x * 0.5f, 0));
	text_utf8(font_bold, title, title_pos.x, title_pos.y, colors[COLOR_TEXT]);
	text_render(font_bold);

	// body text
	float text_x1 = rect_x1(r) + padding;
	float text_x2 = rect_x2(r) - padding;

	TextRenderState state = text_render_state_default;
	state.min_x = text_x1;
	state.max_x = text_x2;
	state.wrap = true;
	state.x = text_x1;
	state.y = y + char_height_bold + padding;
	rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
	text_utf8_with_state(font, &state, body);

	text_render(font);
}

// returns the size of the checkbox, including the label
static v2 checkbox_frame(Ted *ted, bool *value, char const *label, v2 pos) {
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	float checkbox_size = char_height;
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	float padding = settings->padding;
	float border_thickness = settings->border_thickness;
	
	Rect checkbox_rect = rect(pos, V2(checkbox_size, checkbox_size));
	
	for (u32 i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
		if (rect_contains_point(checkbox_rect, ted->mouse_clicks[SDL_BUTTON_LEFT][i])) {
			*value = !*value;
		}
	}
	
	checkbox_rect.pos = v2_add(checkbox_rect.pos, V2(0.5f, 0.5f));
	gl_geometry_rect_border(checkbox_rect, border_thickness, colors[COLOR_TEXT]);
	if (*value) {
		gl_geometry_rect(rect_shrink(checkbox_rect, border_thickness + 2), colors[COLOR_TEXT]);
	}
	
	v2 text_pos = v2_add(pos, V2(checkbox_size + padding * 0.5f, 0));
	v2 size = text_get_size_v2(font, label);
	text_utf8(font, label, text_pos.x, text_pos.y, colors[COLOR_TEXT]);
	
	gl_geometry_draw();
	text_render(font);
	return v2_add(size, V2(checkbox_size + padding * 0.5f, 0));
}
