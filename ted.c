// various core ted functions (opening files, displaying errors, etc.)

#include "ted-internal.h"
#if _WIN32
	#include <SDL_syswm.h>
#endif

void die(const char *fmt, ...) {
	char buf[256] = {0};
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof buf - 1, fmt, args);
	va_end(args);
	
	// show a message box, and if that fails, print it
	if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", buf, NULL) < 0) {
		debug_println("%s\n", buf);
	}
	
	exit(EXIT_FAILURE);
}

float ted_window_width(Ted *ted) {
	return ted->window_width;
}

float ted_window_height(Ted *ted) {
	return ted->window_height;
}

static void ted_vset_message(Ted *ted, MessageType type, const char *fmt, va_list args) {
	char message[sizeof ted->message] = {0};
	vsnprintf(message, sizeof message - 1, fmt, args);
	
	// output error to log file
	char tstr[256];
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	strftime(tstr, sizeof tstr, "%Y-%m-%d %H:%M:%S", tm);
	ted_log(ted, "[ERROR %s] %s\n", tstr, message);
	
	if (type >= ted->message_type) {
		ted->message_type = type;
		strbuf_cpy(ted->message, message);
	}
}

TextBuffer *ted_get_active_buffer(Ted *ted) {
	return ted->active_buffer;
}

TextBuffer *ted_get_active_buffer_behind_menu(Ted *ted) {
	return ted->prev_active_buffer;
}

void ted_set_window_title(Ted *ted, const char *title) {
	strbuf_cpy(ted->window_title, title);
}

bool ted_is_key_down(Ted *ted, SDL_Keycode key) {
	// not currently used but there might be a reason for it in the future
	(void)ted;
	
	const Uint8 *kbd_state = SDL_GetKeyboardState(NULL);
	for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
		if (kbd_state[i] && SDL_GetKeyFromScancode((SDL_Scancode)i) == key) {
			return true;
		}
	}
	return false;
}

bool ted_is_key_combo_down(Ted *ted, KeyCombo combo) {
	if (!ted_is_key_down(ted, KEY_COMBO_KEY(combo)))
		return false;
	if (KEY_COMBO_MODIFIER(combo) != ted_get_key_modifier(ted))
		return false;
	return true;
}

bool ted_is_ctrl_down(Ted *ted) {
	return ted_is_key_down(ted, SDLK_LCTRL) || ted_is_key_down(ted, SDLK_RCTRL);
}

bool ted_is_shift_down(Ted *ted) {
	return ted_is_key_down(ted, SDLK_LSHIFT) || ted_is_key_down(ted, SDLK_RSHIFT);
}

bool ted_is_alt_down(Ted *ted) {
	return ted_is_key_down(ted, SDLK_LALT) || ted_is_key_down(ted, SDLK_RALT);
}

u32 ted_get_key_modifier(Ted *ted) {
	u32 ctrl_down = ted_is_ctrl_down(ted);
	u32 shift_down = ted_is_shift_down(ted);
	u32 alt_down = ted_is_alt_down(ted);
	return ctrl_down << KEY_MODIFIER_CTRL_BIT
		| shift_down << KEY_MODIFIER_SHIFT_BIT
		| alt_down << KEY_MODIFIER_ALT_BIT;
}

bool ted_clicked_in_rect(Ted *ted, Rect rect) {
	arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], MouseClick, click) {
		if (rect_contains_point(rect, click->pos))
			return true;
	}
	return false;
}

void ted_set_message(Ted *ted, MessageType type, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ted_vset_message(ted, type, fmt, args);
	va_end(args);
}

void ted_error(Ted *ted, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ted_vset_message(ted, MESSAGE_ERROR, fmt, args);
	va_end(args);
}

void ted_warn(Ted *ted, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ted_vset_message(ted, MESSAGE_WARNING, fmt, args);
	va_end(args);
}

void ted_info(Ted *ted, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ted_vset_message(ted, MESSAGE_INFO, fmt, args);
	va_end(args);
}

void ted_log(Ted *ted, const char *fmt, ...) {
	if (!ted->log) return;
	
	va_list args;
	va_start(args, fmt);
	vfprintf(ted->log, fmt, args);
	va_end(args);
}


void ted_error_from_buffer(Ted *ted, TextBuffer *buffer) {
	if (*buffer->error)
		ted_error(ted, "%s", buffer->error);
}

void ted_out_of_mem(Ted *ted) {
	ted_error(ted, "Out of memory.");
}

void *ted_malloc(Ted *ted, size_t size) {
	void *ret = malloc(size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

void *ted_calloc(Ted *ted, size_t n, size_t size) {
	void *ret = calloc(n, size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

void *ted_realloc(Ted *ted, void *p, size_t new_size) {
	void *ret = realloc(p, new_size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

char *ted_get_root_dir_of(Ted *ted, const char *path) {
	Settings *settings = ted_active_settings(ted);
	return settings_get_root_dir(settings, path);
}

// get the project root directory (based on the active buffer or ted->cwd if there's no active buffer).
// the return value should be freed
char *ted_get_root_dir(Ted *ted) {
	TextBuffer *buffer = ted->active_buffer;
	if (buffer && buffer_is_named_file(buffer)) {
		return ted_get_root_dir_of(ted, buffer->path);
	} else {
		return ted_get_root_dir_of(ted, ted->cwd);
	}
}

Settings *ted_active_settings(Ted *ted) {
	if (ted->active_buffer)
		return buffer_settings(ted->active_buffer);
	Settings *settings = ted->default_settings;
	int settings_score = 0;
	arr_foreach_ptr(ted->all_settings, Settings, s) {
		const SettingsContext *c = &s->context;
		if (c->language != 0) continue;
		if (!c->path || !str_has_prefix(ted->cwd, c->path)) continue;
		int score = (int)strlen(c->path);
		if (score > settings_score) {
			settings = s;
			settings_score = score;
		}
	}
	return settings;
}

Settings *ted_get_settings(Ted *ted, const char *path, Language language) {
	long best_score = 0;
	Settings *settings = ted->default_settings;
	arr_foreach_ptr(ted->all_settings, Settings, s) {
		long score = context_score(path, language, &s->context);
		if (score > best_score) {
			best_score = score;
			settings = s;
		}
	}
	return settings;
}

LSP *ted_get_lsp_by_id(Ted *ted, LSPID id) {
	if (id == 0) return NULL;
	for (int i = 0; ted->lsps[i]; ++i) {
		LSP *lsp = ted->lsps[i];
		if (lsp->id == id)
			return lsp->exited ? NULL : lsp;
	}
	return NULL;
}

LSP *ted_get_lsp(Ted *ted, const char *path, Language language) {
	Settings *settings = ted_get_settings(ted, path, language);
	if (!settings->lsp_enabled)
		return NULL;
	
	int i;
	for (i = 0; i < TED_LSP_MAX; ++i) {
		LSP *lsp = ted->lsps[i];
		if (!lsp) break;
		if (!streq(lsp->command, settings->lsp)) continue;
		
		if (!lsp->initialized) {
			// withhold judgement until this server initializes.
			// we shouldn't call lsp_try_add_root_dir yet because it doesn't know
			// if the server supports workspaceFolders.
			return NULL;
		}
		if (lsp_covers_path(lsp, path) && lsp->exited) {
			// this server died. give up.
			return NULL;
		}
		
		// check if root matches up or if we can add a workspace folder
		char *root = ted_get_root_dir_of(ted, path);
		bool success = lsp_try_add_root_dir(lsp, root);
		free(root);
		if (success) return lsp;
	}
	if (i == TED_LSP_MAX)
		return NULL; // why are there so many LSP open???
	if (*settings->lsp) {
		// start up this LSP
		FILE *log = settings->lsp_log ? ted->log : NULL;
		char *root_dir = settings_get_root_dir(settings, path);
		ted->lsps[i] = lsp_create(root_dir, settings->lsp, settings->lsp_configuration, log);
		free(root_dir);
		// don't actually return it yet, since it's still initializing (see above)
	}
	
	return NULL;
}

LSP *ted_active_lsp(Ted *ted) {
	if (!ted->active_buffer) {
		char *root = ted_get_root_dir(ted);
		for (int i = 0; ted->lsps[i]; ++i) {
			LSP *lsp = ted->lsps[i];
			if (lsp_covers_path(lsp, root))
				return lsp;
		}
		free(root);
		return NULL;
	}
	return buffer_lsp(ted->active_buffer);
}

u32 ted_active_color(Ted *ted, ColorSetting color) {
	return ted_active_settings(ted)->colors[color];
}

void ted_path_full(Ted *ted, const char *relpath, char *abspath, size_t abspath_size) {
	path_full(ted->cwd, relpath, abspath, abspath_size);
}

static bool ted_is_regular_buffer(Ted *ted, TextBuffer *buffer) {
	return arr_index_of(ted->buffers, buffer) >= 0;
}

Status ted_get_file(Ted const *ted, const char *name, char *out, size_t outsz) {
	if (path_is_absolute(name)) {
		str_cpy(out, outsz, name);
		if (fs_file_exists(out))
			return true;
	}
	if (ted->search_start_cwd) {
		// check in start_cwd
		path_full(ted->start_cwd, name, out, outsz);
		if (fs_file_exists(out))
			return true;
	}
	if (*ted->local_data_dir) {
		str_printf(out, outsz, "%s" PATH_SEPARATOR_STR "%s", ted->local_data_dir, name);
		if (fs_file_exists(out))
			return true;
	}
	if (*ted->global_data_dir) {
		str_printf(out, outsz, "%s" PATH_SEPARATOR_STR "%s", ted->global_data_dir, name);
		if (fs_file_exists(out))
			return true;
	}
	return false;
}

static Font *ted_load_single_font(Ted *ted, const char *filename) {
	char path[TED_PATH_MAX];
	if (!ted_get_file(ted, filename, path, sizeof path)) {
		ted_error(ted, "Couldn't find font file '%s'", filename);
		return NULL;
	}

	arr_foreach_ptr(ted->all_fonts, LoadedFont, f) {
		if (paths_eq(path, f->path))
			return f->font;
	} 
	
	Font *font = text_font_load(path, ted_active_settings(ted)->text_size);
	if (!font) {
		ted_error(ted, "Couldn't load font '%s': %s\n", path, text_get_err());
		return NULL;
	}
	
	LoadedFont *f = arr_addp(ted->all_fonts);
	f->path = str_dup(path);
	f->font = font;
	return font;
}

static Font *ted_load_multifont(Ted *ted, const char *filenames) {
	char filename[TED_PATH_MAX];
	Font *first_font = NULL;
	Font *curr_font = NULL;
	
	while (*filenames) {
		while (*filenames == ',') ++filenames;
		size_t len = strcspn(filenames, ",");
		strn_cpy(filename, sizeof filename, filenames, len);
		str_trim(filename);
		if (*filename) {
			Font *font = ted_load_single_font(ted, filename);
			if (!first_font)
				first_font = font;
			if (curr_font)
				text_font_set_fallback(curr_font, font);
			curr_font = font;
		}
		filenames += len;
	}
	
	return first_font;
}

static float ted_get_ui_scaling(Ted *ted) {
#if _WIN32
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!SDL_GetWindowWMInfo(ted->window, &wm_info))
		return 1;
	HWND hwnd = wm_info.info.win.window;
	UINT dpi = GetDpiForWindow(hwnd);
	if (!dpi)
		return 1;
	return (float)dpi / 96.0f;
#else
	(void)ted;
	return 1;
#endif
}

void ted_load_fonts(Ted *ted) {
	{
		// compute text size
		float scaling = ted_get_ui_scaling(ted);
		arr_foreach_ptr(ted->all_settings, Settings, set) {
			u16 size = (u16)roundf(scaling * (float)set->text_size_no_dpi);
			set->text_size = clamp_u16(size, TEXT_SIZE_MIN, TEXT_SIZE_MAX);
		}
	}
	
	ted_free_fonts(ted);
	Settings *settings = ted_active_settings(ted);
	ted->font = ted_load_multifont(ted, settings->font);
	if (!ted->font) {
		ted->font = ted_load_multifont(ted, "assets/font.ttf");
		if (!ted->font)
			die("Couldn't load default font: %s.", ted->message);
	}
	ted->font_bold = ted_load_multifont(ted, settings->font_bold);
	if (!ted->font_bold) {
		ted->font_bold = ted->font;
	}
}

void ted_change_text_size(Ted *ted, float new_size) {
	arr_foreach_ptr(ted->all_fonts, LoadedFont, f) {
		text_font_change_size(f->font, new_size);
	}
}

void ted_free_fonts(Ted *ted) {
	arr_foreach_ptr(ted->all_fonts, LoadedFont, f) {
		free(f->path);
		text_font_free(f->font);
	}
	arr_clear(ted->all_fonts);
	ted->font = NULL;
	ted->font_bold = NULL;
}

// get node and tab containing buffer
static Node *ted_buffer_location_in_node_tree(Ted *ted, TextBuffer *buffer, u16 *tab_idx) {
	arr_foreach_ptr(ted->nodes, NodePtr, pnode) {
		Node *node = *pnode;
		i32 index = arr_index_of(node->tabs, buffer);
		if (index >= 0) {
			if (tab_idx)
				*tab_idx = (u16)index;
			return node;
		}
	}
	return NULL;
}

void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer) {
	TextBuffer *search_buffer = find_search_buffer(ted);
	ted->active_buffer = buffer;
	autocomplete_close(ted);
	if (buffer != search_buffer) {
		if (ted->find)
			find_update(ted, true); // make sure find results are for this file
	}

	if (ted_is_regular_buffer(ted, buffer)) {
		ted->prev_active_buffer = buffer;
		u16 active_tab=0;
		Node *node = ted_buffer_location_in_node_tree(ted, buffer, &active_tab);
		if (!node) {
			assert(0);
			return;
		}
		node->active_tab = active_tab;
		ted->active_node = node;
		signature_help_retrigger(ted);
	} else {
		ted->active_node = NULL;
	}
	
}

void ted_reset_active_buffer(Ted *ted) {
	if (arr_len(ted->nodes)) {
		Node *node = ted->nodes[0];
		while (!node->tabs)
			node = node->split_a; // arbitrarily pick split_a.
		ted_switch_to_buffer(ted, node->tabs[node->active_tab]);
	} else {
		// there's nothing to set it to
		ted_switch_to_buffer(ted, NULL);
	}
}


void ted_delete_buffer(Ted *ted, TextBuffer *buffer) {
	if (buffer == ted->active_buffer)
		ted_switch_to_buffer(ted, NULL); // make sure we don't set the active buffer to something invalid
	if (buffer == ted->prev_active_buffer)
		ted->prev_active_buffer = NULL;
	buffer_free(buffer);
	arr_remove_item(ted->buffers, buffer);
}

Node *ted_new_node(Ted *ted) {
	if (arr_len(ted->nodes) >= TED_NODE_MAX) {
		ted_error(ted, "Too many nodes.");
		return NULL;
	}
	Node *node = ted_calloc(ted, 1, sizeof *node);
	if (!node) return NULL;
	arr_add(ted->nodes, node);
	return node;
}

TextBuffer *ted_new_buffer(Ted *ted) {
	if (arr_len(ted->buffers) >= TED_BUFFER_MAX) {
		ted_error(ted, "Too many buffers.");
		return NULL;
	}
	TextBuffer *buffer = buffer_new(ted);
	if (!buffer) return NULL;
	arr_add(ted->buffers, buffer);
	return buffer;
}

float ted_line_buffer_height(Ted *ted) {
	const float char_height = text_font_char_height(ted->font);
	return char_height + 2 * ted_active_settings(ted)->border_thickness;
}

void ted_node_switch(Ted *ted, Node *node) {
	while (!node->tabs) {
		node = node->split_a;
	}
	ted_switch_to_buffer(ted, node->tabs[node->active_tab]);
	ted->active_node = node;
}

// Open a new buffer. Fills out *tab to the index of the tab used.
static TextBuffer *ted_open_buffer(Ted *ted, u16 *tab) {
	TextBuffer *new_buffer = ted_new_buffer(ted);
	if (!new_buffer) return NULL;
	Node *node = ted->active_node;
	if (!node) {
		if (!arr_len(ted->nodes)) {
			// no nodes open; create a root node
			node = ted->active_node = ted_new_node(ted);
		} else if (ted->prev_active_buffer) {
			// opening a file while a menu is open
			// it may happen.... (currently happens for rename symbol)
			node = ted_buffer_location_in_node_tree(ted, ted->prev_active_buffer, NULL);
		} else {
			// idk what is going on
			ted_error(ted, "internal error: can't figure out where to put this buffer.");
			ted_delete_buffer(ted, new_buffer);
			return NULL;
		}
	}
	
	if (arr_len(node->tabs) >= TED_MAX_TABS) {
		ted_error(ted, "Too many tabs.");
		ted_delete_buffer(ted, new_buffer);
		return false;
	}
	
	arr_add(node->tabs, new_buffer);
	node->active_tab = (u16)(arr_len(node->tabs) - 1);
	*tab = node->active_tab;
	ted_switch_to_buffer(ted, new_buffer);
	return new_buffer;
}

TextBuffer *ted_get_buffer_with_file(Ted *ted, const char *path) {
	if (!path) return NULL;
	if (!path_is_absolute(path)) {
		assert(0);
		return NULL;
	}
	
	arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
		TextBuffer *buffer = *pbuffer;
		if (buffer->path && paths_eq(path, buffer->path)) {
			return buffer;
		}
	}
	return NULL;
}

bool ted_open_file(Ted *ted, const char *filename) {
	if (!filename) {
		assert(0);
		return false;
	}
	
	char path[TED_PATH_MAX];
	ted_path_full(ted, filename, path, sizeof path);

	// first, check if file is already open
	TextBuffer *already_open = ted_get_buffer_with_file(ted, path);
	if (already_open) {
		ted_switch_to_buffer(ted, already_open);
		return true;
	}
	
	// not open; we need to load it
	u16 tab_idx;
	TextBuffer *buffer = NULL;
	if (ted->active_buffer && !ted->active_buffer->path
		&& ted_is_regular_buffer(ted, ted->active_buffer)
		&& buffer_empty(ted->active_buffer)) {
		// the active buffer is just an empty untitled buffer. open it here.
		return buffer_load_file(ted->active_buffer, path);
	} else if ((buffer = ted_open_buffer(ted, &tab_idx))) {
		if (buffer_load_file(buffer, path)) {
			return true;
		} else {
			ted_error_from_buffer(ted, buffer);
			node_tab_close(ted, ted->active_node, tab_idx);
			return false;
		}
	} else {
		return false;
	}
}

bool ted_new_file(Ted *ted, const char *filename) {
	u16 tab_idx=0;
	char path[TED_PATH_MAX];
	if (filename)
		ted_path_full(ted, filename, path, sizeof path);
	else
		*path = '\0';
	TextBuffer *buffer = ted_get_buffer_with_file(ted, path);
	if (buffer) {
		ted_switch_to_buffer(ted, buffer);
		return true;
	} else if ((buffer = ted_open_buffer(ted, &tab_idx))) {
		buffer_new_file(buffer, *path ? path : NULL);
		if (!buffer_has_error(buffer)) {
			return true;
		} else {
			ted_error_from_buffer(ted, buffer);
			node_tab_close(ted, ted->active_node, tab_idx);
			return false;
		}
	} else {
		return false;
	}
}


bool ted_save_all(Ted *ted) {
	bool success = true;
	arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
		TextBuffer *buffer = *pbuffer;
		if (buffer_unsaved_changes(buffer)) {
			if (!buffer->path) {
				ted_switch_to_buffer(ted, buffer);
				menu_open(ted, MENU_SAVE_AS);
				success = false; // we haven't saved this buffer yet; we've just opened the "save as" menu.
				break;
			} else {
				if (!buffer_save(buffer)) {
					success = false;
					ted_error_from_buffer(ted, buffer);
				}
			}
		}
	}
	return success;
}

void ted_reload_all(Ted *ted) {
	arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
		TextBuffer *buffer = *pbuffer;
		if (!buffer_unsaved_changes(buffer)) {
			buffer_reload(buffer);
		}
	}
	if (menu_is_open(ted, MENU_ASK_RELOAD)) {
		menu_close(ted);
	}
}

float ted_get_menu_width(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	return minf(settings->max_menu_width, ted->window_width - 2.0f * settings->padding);
}

// load/reload configs
void ted_load_configs(Ted *ted) {
	
	// copy global config to local config
	char local_config_filename[TED_PATH_MAX];
	strbuf_printf(local_config_filename, "%s" PATH_SEPARATOR_STR TED_CFG, ted->local_data_dir);
	char global_config_filename[TED_PATH_MAX];
	strbuf_printf(global_config_filename, "%s" PATH_SEPARATOR_STR TED_CFG, ted->global_data_dir);
	if (!fs_file_exists(local_config_filename)) {
		if (fs_file_exists(global_config_filename)) {
			if (!copy_file(global_config_filename, local_config_filename)) {
				die("Couldn't copy config %s to %s.", global_config_filename, local_config_filename);
			}
		} else {
			die("ted's backup config file, %s, does not exist. Try reinstalling ted?", global_config_filename);
		}
	}
	
	
	ConfigPart *parts = NULL;
	config_read(ted, &parts, global_config_filename);
	config_read(ted, &parts, local_config_filename);
	if (ted->search_start_cwd) {
		// read config in start_cwd
		char start_cwd_filename[TED_PATH_MAX];
		strbuf_printf(start_cwd_filename, "%s" PATH_SEPARATOR_STR TED_CFG, ted->start_cwd);
		
		config_read(ted, &parts, start_cwd_filename);
	}
	config_parse(ted, &parts);
}

void ted_reload_configs(Ted *ted) {
	config_free(ted);
	ted_load_configs(ted);
	// reset text size
	ted_load_fonts(ted);
}

void ted_press_key(Ted *ted, SDL_Keycode keycode, SDL_Keymod modifier) {
	KeyCombo key_combo = KEY_COMBO(
		(u32)((modifier & (KMOD_LCTRL|KMOD_RCTRL)) != 0) << KEY_MODIFIER_CTRL_BIT |
		(u32)((modifier & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0) << KEY_MODIFIER_SHIFT_BIT |
		(u32)((modifier & (KMOD_LALT|KMOD_RALT)) != 0) << KEY_MODIFIER_ALT_BIT,
		keycode);
	
	const KeyAction *const key_actions = ted_active_settings(ted)->key_actions;
	u32 lo = 0;
	u32 hi = arr_len(key_actions);
	while (lo < hi) {
		u32 mid = (lo + hi) / 2;
		if (key_actions[mid].key_combo.value < key_combo.value) {
			lo = mid + 1;
		} else if (key_actions[mid].key_combo.value > key_combo.value) {
			hi = mid;
		} else {
			const CommandContext context = {0};
			command_execute_ex(ted, key_actions[mid].command, &key_actions[mid].argument, &context);
			return;
		}
	}
	// nothing bound to this key
}

bool ted_get_mouse_buffer_pos(Ted *ted, TextBuffer **pbuffer, BufferPos *ppos) {
	arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuf) {
		TextBuffer *buffer = *pbuf;
		BufferPos pos = {0};
		if (buffer_pixels_to_pos(buffer, ted->mouse_pos, &pos)) {
			if (ppos) *ppos = pos;
			if (pbuffer) *pbuffer = buffer;
			return true;
		}
	}
	return false;
}

void ted_flash_error_cursor(Ted *ted) {
	ted->cursor_error_time = ted->frame_time;
}

void ted_go_to_lsp_document_position(Ted *ted, LSP *lsp, LSPDocumentPosition position) {
	if (!lsp) lsp = ted_active_lsp(ted);
	const char *path = lsp_document_path(lsp, position.document);
	if (ted_open_file(ted, path)) {
		TextBuffer *buffer = ted->active_buffer;
		BufferPos pos = buffer_pos_from_lsp(buffer, position.pos);
		buffer_cursor_move_to_pos(buffer, pos);
		buffer->center_cursor_next_frame = true;
	} else {
		ted_flash_error_cursor(ted);
	}
}

void ted_cancel_lsp_request(Ted *ted, LSPServerRequestID *request) {
	if (!request) return;
	LSP *lsp_obj = ted_get_lsp_by_id(ted, request->lsp);
	if (!lsp_obj) return;
	lsp_cancel_request(lsp_obj, request->id);
	memset(request, 0, sizeof *request);
}


static void mark_node_reachable(Ted *ted, Node *node, bool *reachable) {
	i32 i = arr_index_of(ted->nodes, node);
	if (i < 0) return;
	if (reachable[i]) {
		ted_error(ted, "Node %d reachable in 2 different ways\nThis should never happen.", i);
		ted_log(ted, "Node %d reachable in 2 different ways\n", i);
		node_close(ted, node);
		return;
	}
	reachable[i] = true;
	if (!node->tabs) {
		mark_node_reachable(ted, node->split_a, reachable);
		mark_node_reachable(ted, node->split_b, reachable);
	}
}

void ted_check_for_node_problems(Ted *ted) {
	bool *reachable = ted_calloc(ted, arr_len(ted->nodes), 1);
	if (arr_len(ted->nodes))
		mark_node_reachable(ted, ted->nodes[0], reachable);
	for (u32 i = 0; i < arr_len(ted->nodes); ++i) {
		if (!reachable[i]) {
			ted_error(ted, "ORPHANED NODE %u\nThis should never happen.", i);
			ted_log(ted, "ORPHANED NODE %u\n", i);
			node_close(ted, ted->nodes[i]);
			--i;
		}
	}
	free(reachable);
}

MessageType ted_message_type_from_lsp(LSPWindowMessageType type) {
	switch (type) {
	case LSP_WINDOW_MESSAGE_ERROR: return MESSAGE_ERROR;
	case LSP_WINDOW_MESSAGE_WARNING: return MESSAGE_WARNING;
	case LSP_WINDOW_MESSAGE_INFO:
	case LSP_WINDOW_MESSAGE_LOG:
		return MESSAGE_INFO;
	}
	assert(0);
	return MESSAGE_ERROR;
}

void ted_color_settings_for_message_type(MessageType type, ColorSetting *bg_color, ColorSetting *border_color) {
	switch (type) {
	case MESSAGE_ERROR:
		*bg_color = COLOR_ERROR_BG;
		*border_color = COLOR_ERROR_BORDER;
		break;
	case MESSAGE_WARNING:
		*bg_color = COLOR_WARNING_BG;
		*border_color = COLOR_WARNING_BORDER;
		break;
	case MESSAGE_INFO:
		*bg_color = COLOR_INFO_BG;
		*border_color = COLOR_INFO_BORDER;
		break;
	}
}


u64 ted_add_edit_notify(Ted *ted, EditNotify notify, void *context) {
	EditNotifyInfo info = {
		.fn = notify,
		.context = context,
		.id = ++ted->edit_notify_id,
	};
	arr_add(ted->edit_notifys, info);
	return info.id;
}

void ted_remove_edit_notify(Ted *ted, EditNotifyID id) {
	u32 i;
	for (i = 0; i < arr_len(ted->edit_notifys); ++i) {
		if (ted->edit_notifys[i].id == id) {
			arr_remove(ted->edit_notifys, i);
			break;
		}
	}
}

void ted_close_buffer(Ted *ted, TextBuffer *buffer) {
	if (!buffer) return;

	u16 tab_idx=0;
	Node *node = ted_buffer_location_in_node_tree(ted, buffer, &tab_idx);
	node_tab_close(ted, node, tab_idx);
}

bool ted_close_buffer_with_file(Ted *ted, const char *path) {
	TextBuffer *buffer = ted_get_buffer_with_file(ted, path);
	if (!buffer) return false;
	ted_close_buffer(ted, buffer);
	return true;
}
