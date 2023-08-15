// various UI elements used by ted

#include "ted-internal.h"

#if __unix__
#include <fcntl.h>
#include <unistd.h>
#endif

struct Selector {
	SelectorEntry *entries;
	char *search_term;
	Rect bounds;
	u32 cursor;
	float scroll;
	bool enable_cursor;
};

struct FileSelector {
	char title[32];
	Selector sel;
	Rect bounds;
	char cwd[TED_PATH_MAX];
	/// indicates that this is for creating files, not opening files
	bool create_menu;
};

static Status file_selector_cd_(Ted *ted, FileSelector *fs, const char *path, int symlink_depth);

static void selector_init(Selector *s) {
	s->enable_cursor = true;
}

Selector *selector_new(void) {
	Selector *s = calloc(1, sizeof *s);
	selector_init(s);
	return s;
}

void selector_free(Selector *s) {
	selector_clear(s);
	free(s);
}

void selector_clear_entries(Selector *s) {
	arr_foreach_ptr(s->entries, SelectorEntry, e) {
		free((void *)e->name);
		free((void *)e->detail);
	}
	arr_clear(s->entries);
}

void selector_clear(Selector *s) {
	selector_clear_entries(s);
	free(s->search_term);
	s->search_term = NULL;
	s->scroll = 0;
	s->cursor = 0;
}

void selector_set_cursor(Selector *s, u32 pos) {
	s->cursor = pos;
}

u32 selector_get_cursor(Selector *s) {
	return s->cursor;
}

Status selector_get_entry(Selector *s, u32 index, SelectorEntry *entry) {
	if (index >= arr_len(s->entries))
		return false;
	*entry = s->entries[index];
	return true;
}

void selector_set_bounds(Selector *s, Rect bounds) {
	s->bounds = bounds;
}

Status selector_get_cursor_entry(Selector *s, SelectorEntry *entry) {
	return selector_get_entry(s, s->cursor, entry);
}

FileSelector *file_selector_new(void) {
	FileSelector *s = calloc(1, sizeof *s);
	selector_init(&s->sel);
	return s;
}

void file_selector_set_create(FileSelector *s, bool create) {
	s->create_menu = create;
}

void file_selector_free(FileSelector *s) {
	file_selector_clear(s);
	free(s);
}

void file_selector_set_bounds(FileSelector *s, Rect bounds) {
	s->bounds = bounds;
}

void file_selector_set_title(FileSelector *s, const char *title) {
	strbuf_cpy(s->title, title);
}

static float selector_entries_start_y(Ted *ted, const Selector *s) {
	float padding = ted_active_settings(ted)->padding;

	return s->bounds.pos.y
		+ ted_line_buffer_height(ted) + padding; // make room for line buffer
}

// number of entries that can be displayed on the screen
static u32 selector_max_displayable_entries(Ted *ted, const Selector *s) {
	float char_height = text_font_char_height(ted->font);
	float entries_h = rect_y2(s->bounds) - selector_entries_start_y(ted, s);
	return (u32)(entries_h / char_height);
}

static bool selector_show_entry(Selector *s, const SelectorEntry *e) {
	return !s->search_term || strstr_case_insensitive(e->name, s->search_term);
}

static u32 selector_filtered_entry_count(Selector *s) {
	u32 count = 0;
	arr_foreach_ptr(s->entries, const SelectorEntry, e) {
		count += selector_show_entry(s, e);
	}
	return count;
}

static void selector_clamp_scroll(Ted *ted, Selector *s) {
	float max_scroll = (float)selector_filtered_entry_count(s) - (float)selector_max_displayable_entries(ted, s);
	if (max_scroll < 0) max_scroll = 0;
	s->scroll = clampf(s->scroll, 0, max_scroll);
}

static void selector_scroll_to_cursor(Ted *ted, Selector *s) {
	u32 max_entries = selector_max_displayable_entries(ted, s);
	float scrolloff = ted_active_settings(ted)->scrolloff;
	float min_scroll = (float)s->cursor - ((float)max_entries - scrolloff);
	float max_scroll = (float)s->cursor - scrolloff;
	s->scroll = clampf(s->scroll, min_scroll, max_scroll);
	selector_clamp_scroll(ted, s);
}

static void selector_move(Ted *ted, Selector *s, i32 direction) {
	assert(direction == -1 || direction == 1);
	if (!s->enable_cursor || selector_filtered_entry_count(s) == 0) {
		// can't do anything
		return;
	}
	
	do
		s->cursor = (u32)mod_i64((i64)s->cursor + direction, arr_len(s->entries));
	while (!selector_show_entry(s, &s->entries[s->cursor]));
	selector_scroll_to_cursor(ted, s);
}

void selector_up(Ted *ted, Selector *s) {
	selector_move(ted, s, -1);
}

void selector_down(Ted *ted, Selector *s) {
	selector_move(ted, s, 1);
}

static int selectory_entry_cmp_name(const void *av, const void *bv) {
	SelectorEntry const *a = av, *b = bv;
	return strcoll(a->name, b->name);
}

void selector_sort_entries_by_name(Selector *s) {
	qsort(s->entries, arr_len(s->entries), sizeof *s->entries, selectory_entry_cmp_name);
}

static void selector_entry_rect_clip(Ted *ted, Selector *s, Rect *r) {
	Rect entry_bounds = s->bounds;
	entry_bounds.pos.y = selector_entries_start_y(ted, s);
	entry_bounds.size.y -= entry_bounds.pos.y - s->bounds.pos.y;
	rect_clip_to_rect(r, entry_bounds);
}

static Rect selector_entry_rect_first(Ted *ted, Selector *s) {
	float char_height = text_font_char_height(ted->font);
	Rect r = {
		.pos = {
			s->bounds.pos.x,
			selector_entries_start_y(ted, s) - char_height * s->scroll
		},
		.size = {
			s->bounds.size.x,
			char_height,
		}
	};
	selector_entry_rect_clip(ted, s, &r);
	return r;
}

static void selector_entry_rect_next(Ted *ted, Selector *s, Rect *r) {
	const float char_height = text_font_char_height(ted->font);
	r->pos.y += char_height;
	r->size = (vec2) { s->bounds.size.x, char_height };
	selector_entry_rect_clip(ted, s, r);
}

char *selector_update(Ted *ted, Selector *s) {
	char *ret = NULL;
	TextBuffer *line_buffer = ted->line_buffer;
	{
		free(s->search_term);
		s->search_term = buffer_get_line_utf8(line_buffer, 0);
	}
	
	ted->selector_open = s;
	Rect entry_rect = selector_entry_rect_first(ted, s);
	arr_foreach_ptr(s->entries, const SelectorEntry, e) {
		if (!selector_show_entry(s, e)) continue;
		
		// check if this entry was clicked on
		if (ted_clicked_in_rect(ted, entry_rect)) {
			// this option was selected
			s->cursor = (u32)(e - s->entries); // indicate the index of the selected entry using s->cursor
			ret = str_dup(e->name);
			break;
		}
		selector_entry_rect_next(ted, s, &entry_rect);
	}

	if (line_buffer_is_submitted(line_buffer)) {
		line_buffer_clear_submitted(line_buffer);
		if (!ret) {
			if (s->enable_cursor) {
				// select this option
				if (s->cursor < arr_len(s->entries))
					ret = str_dup(s->entries[s->cursor].name);

			} else {
				// user typed in submission
				ret = buffer_get_line_utf8(line_buffer, 0);
			}
		}
	}

	// apply scroll
	float scroll_speed = 2.5f;
	s->scroll += scroll_speed * (float)ted->scroll_total_y;
	selector_clamp_scroll(ted, s);
	return ret;
}

void selector_render(Ted *ted, Selector *s) {
	const Settings *settings = ted_active_settings(ted);
	Font *font = ted->font;
	float padding = settings->padding;

	Rect bounds = s->bounds;
	
	if (arr_len(s->entries)) {
		// clamp cursor
		s->cursor = clamp_u32(s->cursor, 0, arr_len(s->entries) - 1);
		
		// make sure cursor points to an entry in the filtered list
		u32 prev = U32_MAX;
		for (u32 i = 0; i < arr_len(s->entries); ++i) {
			if (selector_show_entry(s, &s->entries[i]))
				prev = i;
			if (i >= s->cursor && prev != U32_MAX) {
				s->cursor = prev;
				break;
			}
		}
		if (!selector_show_entry(s, &s->entries[s->cursor])) {
			s->cursor = prev;
		}
	}
	
	
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	{
		float line_buffer_height = ted_line_buffer_height(ted);
		buffer_render(ted->line_buffer, rect4(x1, y1, x2, y1 + line_buffer_height));
		y1 += line_buffer_height;
	}
	
	TextRenderState text_state = text_render_state_default;
	text_state.min_x = x1;
	text_state.max_x = x2;
	text_state.min_y = y1;
	text_state.max_y = y2;
	text_state.render = true;

	{
	// render entries themselves
	Rect r = selector_entry_rect_first(ted, s);
	for (u32 i = 0; i < arr_len(s->entries); ++i) {
		const SelectorEntry *entry = &s->entries[i];
		if (!selector_show_entry(s, entry)) continue;
		if (r.size.x * r.size.y > 0) {
			float x = r.pos.x, y = r.pos.y;
			text_state.x = x; text_state.y = y;
			
			if (rect_contains_point(r, ted->mouse_pos) || (s->enable_cursor && s->cursor == i)) {
				// highlight it
				gl_geometry_rect(r, settings_color(settings, COLOR_MENU_HL));
			}
			
			// draw name
			settings_color_floats(settings, entry->color ? entry->color : COLOR_TEXT, text_state.color);
			text_state_break_kerning(&text_state);
			text_utf8_with_state(font, &text_state, entry->name);
			
			if (entry->detail) {
				// draw detail
				float detail_size = text_get_size_vec2(font, entry->detail).x;
				TextRenderState detail_state = text_state;
				detail_state.x = maxd(text_state.x + 2 * padding, x2 - detail_size);
				
				settings_color_floats(settings, COLOR_COMMENT, detail_state.color);
				text_utf8_with_state(font, &detail_state, entry->detail);
			}
		}
		selector_entry_rect_next(ted, s, &r);
	}
	gl_geometry_draw();
	text_render(font);
	}
}

void file_selector_clear(FileSelector *fs) {
	selector_clear(&fs->sel);
	memset(fs, 0, sizeof *fs);
}

static int file_selector_entry_cmp(void *context, const SelectorEntry *a, const SelectorEntry *b) {
	(void)context;
	FsType a_type = (FsType)a->userdata, b_type = (FsType)b->userdata;
	// put directories first
	if (a_type == FS_DIRECTORY && b_type != FS_DIRECTORY) {
		return -1;
	}
	if (a_type != FS_DIRECTORY && b_type == FS_DIRECTORY) {
		return +1;
	}
	return strcmp_case_insensitive(a->name, b->name);
}

// cd to the directory `name`. `name` cannot include any path separators.
static Status file_selector_cd1(Ted *ted, FileSelector *fs, const char *name, size_t name_len, int symlink_depth) {
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
		strbuf_cpy(path, cwd);
		if (path[strlen(path) - 1] != PATH_SEPARATOR)
			strbuf_catf(path, "%c", PATH_SEPARATOR);
		strbuf_catf(path, "%*s", (int)name_len, name);
		
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
			str_catf(cwd, sizeof fs->cwd, "%c", PATH_SEPARATOR);
		// add name itself
		strn_cat(cwd, sizeof fs->cwd, name, name_len);
	}
	return true;
	
}

static Status file_selector_cd_(Ted *ted, FileSelector *fs, const char *path, int symlink_depth) {
	char *const cwd = fs->cwd;
	if (path[0] == '\0') return true;

	if (path_is_absolute(path)) {
		// absolute path (e.g. /foo, c:\foo)
		// start out by replacing cwd with the start of the absolute path
		if (path[0] == PATH_SEPARATOR) {
			char root[TED_PATH_MAX];
			// necessary because the full path of \ on windows isn't just \, it's c:\ or something
			char pathsep[] = {PATH_SEPARATOR, '\0'};
			ted_path_full(ted, pathsep, root, sizeof root);
			strcpy(cwd, root);
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

	const char *p = path;

	while (*p) {
		size_t len = strcspn(p, ALL_PATH_SEPARATORS);
		if (!file_selector_cd1(ted, fs, p, len, symlink_depth))
			return false;
		p += len;
		p += strspn(p, ALL_PATH_SEPARATORS);
	}
	
	return true;
}

// go to the directory `path`. make sure `path` only contains path separators like PATH_SEPARATOR, not any
// other members of ALL_PATH_SEPARATORS
// returns false if this path doesn't exist or isn't a directory
static bool file_selector_cd(Ted *ted, FileSelector *fs, const char *path) {
	fs->sel.cursor = 0;
	fs->sel.scroll = 0;
	return file_selector_cd_(ted, fs, path, 0);
}

static ColorSetting color_setting_for_file_type(FsType type) {
	switch (type) {
        case FS_FILE: return COLOR_TEXT;
        case FS_DIRECTORY: return COLOR_TEXT_FOLDER;
        default: return COLOR_TEXT_OTHER;
	}
}

void selector_sort_entries(Selector *s, int (*compar)(void *context, const SelectorEntry *e1, const SelectorEntry *e2), void *context) {
	qsort_with_context(s->entries, arr_len(s->entries), sizeof *s->entries, (int (*) (void *, const void *, const void *))compar, context);
}

char *file_selector_update(Ted *ted, FileSelector *fs) {
	TextBuffer *line_buffer = ted->line_buffer;
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
				buffer_delete_chars_at_pos(line_buffer, buffer_pos_start_of_file(line_buffer), last_path_sep + 1); // delete up to and including the last path separator
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
		path_full(cwd, option_chosen, path, sizeof path);
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
	selector_clear_entries(&fs->sel);
	
	// get new entries
	FsDirectoryEntry **files;
	// if the directory we're in gets deleted, go back a directory.
	for (u32 i = 0; i < 100; ++i) {
		files = fs_list_directory(cwd);
		if (files) break;
		else if (i == 0) {
			if (fs_path_type(cwd) == FS_NON_EXISTENT)
				ted_error(ted, "%s is not a directory.", cwd);
			else
				ted_error(ted, "Can't list directory %s.", cwd);
		}
		file_selector_cd(ted, fs, "..");
	}

	if (files) {
		for (u32 i = 0; files[i]; ++i) {
			char *name = files[i]->name;
			if (streq(name, ".")) {
				continue;
			}
			SelectorEntry entry = {
				.color = color_setting_for_file_type(files[i]->type),
				.name = name,
				.userdata = files[i]->type,
			};
			selector_add_entry(&fs->sel, &entry);
		}
		selector_sort_entries(&fs->sel, file_selector_entry_cmp, NULL);

		for (u32 i = 0; files[i]; ++i)
			free(files[i]);
		free(files);
		// set cwd to this (if no buffers are open, the "open" menu should use the last file selector's cwd)
		strbuf_cpy(ted->cwd, cwd);
	} else {
		ted_error(ted, "Couldn't list directory '%s'.", cwd);
	}
	
	return NULL;
}

void file_selector_render(Ted *ted, FileSelector *fs) {
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	Rect bounds = fs->bounds;
	Font *font = ted->font, *font_bold = ted->font_bold;
	float padding = settings->padding;
	float char_height = text_font_char_height(font);
	
	if (*fs->title) {
		text_utf8(font_bold, fs->title, bounds.pos.x, bounds.pos.y, colors[COLOR_TEXT]);
		rect_shrink_top(&bounds, text_font_char_height(font_bold) * 0.75f + padding);
	}

	// current working directory
	{
		const char *cwd = fs->cwd;
		const float text_width = text_get_size_vec2(font, cwd).x;
		TextRenderState	state = text_render_state_default;
		state.x = bounds.pos.x;
		if (text_width > bounds.size.x) {
			// very long cwd
			// make sure the end of the cwd is shown
			state.x = rect_x2(bounds) - text_width - padding;
		}
		state.y = bounds.pos.y;
		rgba_u32_to_floats(colors[COLOR_TEXT], state.color);
		state.min_x = bounds.pos.x;
		state.max_x = rect_x2(bounds);
		
		text_utf8_with_state(font, &state, fs->cwd);
		rect_shrink_top(&bounds, char_height + padding);
	}

	// render selector
	Selector *sel = &fs->sel;
	sel->bounds = bounds;
	selector_render(ted, sel);
	text_render(font_bold);
}

vec2 button_get_size(Ted *ted, const char *text) {
	float border_thickness = ted_active_settings(ted)->border_thickness;
	return vec2_add_const(text_get_size_vec2(ted->font, text), 2 * border_thickness);
}

void button_render(Ted *ted, Rect button, const char *text, u32 color) {
	const u32 *colors = ted_active_settings(ted)->colors;
	
	if (rect_contains_point(button, ted->mouse_pos)) {
		// highlight button when hovering over it
		u32 new_color = (color & 0xffffff00) | ((color & 0xff) / 3);
		gl_geometry_rect(button, new_color);
	}
	
	gl_geometry_rect_border(button, ted_active_settings(ted)->border_thickness, colors[COLOR_BORDER]);
	gl_geometry_draw();

	vec2 pos = rect_center(button);
	text_utf8_anchored(ted->font, text, pos.x, pos.y, color, ANCHOR_MIDDLE);
	text_render(ted->font);
}

bool button_update(Ted *ted, Rect button) {
	return ted_clicked_in_rect(ted, button);
}

static void popup_get_rects(Ted const *ted, u32 options, Rect *popup, Rect *button_yes, Rect *button_no, Rect *button_cancel) {
	float window_width = ted->window_width, window_height = ted->window_height;
	
	*popup = rect_centered((vec2){window_width * 0.5f, window_height * 0.5f}, (vec2){300, 200});
	float button_height = 30;
	u16 nbuttons = util_popcount(options);
	float button_width = popup->size.x / nbuttons;
	popup->size = vec2_clamp(popup->size, (vec2){0, 0}, (vec2){window_width, window_height});
	Rect r = rect_xywh(popup->pos.x, rect_y2(*popup) - button_height, button_width, button_height);
	if (options & POPUP_YES) {
		*button_yes = r;
		r.pos.x += button_width;
	}
	if (options & POPUP_NO) {
		*button_no = r;
		r.pos.x += button_width;
	}
	if (options & POPUP_CANCEL) {
		*button_cancel = r;
		r.pos.x += button_width;
	}	
}

PopupOption popup_update(Ted *ted, u32 options) {
	Rect r = {0}, button_yes = {0}, button_no = {0}, button_cancel = {0};
	popup_get_rects(ted, options, &r, &button_yes, &button_no, &button_cancel);
	if (button_update(ted, button_yes))
		return POPUP_YES;
	if (button_update(ted, button_no))
		return POPUP_NO;
	if (button_update(ted, button_cancel))
		return POPUP_CANCEL;
	return POPUP_NONE;
}

void popup_render(Ted *ted, u32 options, const char *title, const char *body) {
	float window_width = ted->window_width;
	Font *font = ted->font;
	Font *font_bold = ted->font_bold;
	Rect r, button_yes, button_no, button_cancel;
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	const float char_height_bold = text_font_char_height(font_bold);
	const float padding = settings->padding;
	const float border_thickness = settings->border_thickness;
	
	popup_get_rects(ted, options, &r, &button_yes, &button_no, &button_cancel);
	
	
	float y = r.pos.y;
	
	// popup rectangle
	gl_geometry_rect(r, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(r, border_thickness, colors[COLOR_BORDER]);
	// line separating text from body
	gl_geometry_rect(rect_xywh(r.pos.x, y + char_height_bold, r.size.x, border_thickness), colors[COLOR_BORDER]);
	
	if (options & POPUP_YES) button_render(ted, button_yes, "Yes", colors[COLOR_YES]);
	if (options & POPUP_NO) button_render(ted, button_no, "No", colors[COLOR_NO]);
	if (options & POPUP_CANCEL) button_render(ted, button_cancel, "Cancel", colors[COLOR_CANCEL]);

	// title text
	vec2 title_size = {0};
	text_get_size(font_bold, title, &title_size.x, &title_size.y);
	vec2 title_pos = {(window_width - title_size.x) * 0.5f, y};
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
vec2 checkbox_frame(Ted *ted, bool *value, const char *label, vec2 pos) {
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	float checkbox_size = char_height;
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	float padding = settings->padding;
	float border_thickness = settings->border_thickness;
	
	Rect checkbox_rect = rect(pos, (vec2){checkbox_size, checkbox_size});
	
	if (ted_clicked_in_rect(ted, checkbox_rect)) {
		*value = !*value;
	}
	
	checkbox_rect.pos = vec2_add(checkbox_rect.pos, (vec2){0.5f, 0.5f});
	gl_geometry_rect_border(checkbox_rect, border_thickness, colors[COLOR_TEXT]);
	if (*value) {
		Rect r = checkbox_rect;
		rect_shrink(&r, border_thickness + 2);
		gl_geometry_rect(r, colors[COLOR_TEXT]);
	}
	
	vec2 text_pos = vec2_add(pos, (vec2){checkbox_size + padding * 0.5f, 0});
	vec2 size = text_get_size_vec2(font, label);
	text_utf8(font, label, text_pos.x, text_pos.y, colors[COLOR_TEXT]);
	
	gl_geometry_draw();
	text_render(font);
	return vec2_add(size, (vec2){checkbox_size + padding * 0.5f, 0});
}

void selector_add_entry(Selector *s, const SelectorEntry *entry) {
	SelectorEntry s_entry = {
		.color = entry->color,
		.name = str_dup(entry->name),
		.detail = str_dup(entry->detail),
		.userdata = entry->userdata,
	};
	arr_add(s->entries, s_entry);
}
