// this is a macro so we get -Wformat warnings
#define ted_seterr(ted, ...) \
	snprintf((ted)->error, sizeof (ted)->error - 1, __VA_ARGS__)

void ted_seterr_to_buferr(Ted *ted, TextBuffer *buffer) {
	size_t size = sizeof ted->error;
	if (sizeof buffer->error < size) size = sizeof buffer->error;
	memcpy(ted->error, buffer->error, size);
}

bool ted_haserr(Ted *ted) {
	return ted->error[0] != '\0';
}

char const *ted_geterr(Ted *ted) {
	return ted->error;
}

void ted_clearerr(Ted *ted) {
	ted->error[0] = '\0';
}


static void ted_out_of_mem(Ted *ted) {
	ted_seterr(ted, "Out of memory.");
}

static void *ted_malloc(Ted *ted, size_t size) {
	void *ret = malloc(size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

static void *ted_calloc(Ted *ted, size_t n, size_t size) {
	void *ret = calloc(n, size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

static void *ted_realloc(Ted *ted, void *p, size_t new_size) {
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
	if (buffer) {
		return ted_get_root_dir_of(ted, buffer->filename);
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
	for (int i = 0; ted->lsps[i]; ++i) {
		if (ted->lsps[i]->id == id)
			return ted->lsps[i];
	}
	return NULL;
}

// IMPORTANT NOTE ABOUT CACHING LSPs:
//     - be careful if you want to cache LSPs, e.g. adding  a LSP *lsp member to `buffer`.
//     - right now we pretend that the server has workspace folder support until the initialize response is sent.
//     - specifically, this means that:
//                 ted_get_lsp("/path1/a") =>  new LSP 0x12345
//                 ted_get_lsp("/path2/b") => same LSP 0x12345
//                 (receive initialize request, realize we don't have workspace folder support)
//                 ted_get_lsp("/path2/b") =>  new LSP 0x6789A
LSP *ted_get_lsp(Ted *ted, const char *path, Language language) {
	Settings *settings = ted_get_settings(ted, path, language);
	if (!settings->lsp_enabled)
		return NULL;
	
	int i;
	for (i = 0; i < TED_LSP_MAX; ++i) {
		LSP *lsp = ted->lsps[i];
		if (!lsp) break;
		if (lsp->language != language) continue;
		
		if (!lsp->initialized) {
			// withhold judgement until this server initializes.
			// we shouldn't call lsp_try_add_root_dir yet because it doesn't know
			// if the server supports workspaceFolders.
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
		char *root_dir = settings_get_root_dir(settings, path);
		ted->lsps[i] = lsp_create(root_dir, language, settings->lsp);
		free(root_dir);
		// don't actually return it yet, since it's still initializing (see above)
	}
	
	return NULL;
}

LSP *ted_get_active_lsp(Ted *ted) {
	if (!ted->active_buffer)
		return NULL;
	return buffer_lsp(ted->active_buffer);
}

u32 ted_color(Ted *ted, ColorSetting color) {
	return ted_active_settings(ted)->colors[color];
}

static void ted_path_full(Ted *ted, char const *relpath, char *abspath, size_t abspath_size) {
	path_full(ted->cwd, relpath, abspath, abspath_size);
}

static bool ted_is_regular_buffer(Ted *ted, TextBuffer *buffer) {
	return buffer >= ted->buffers && buffer < ted->buffers + TED_MAX_BUFFERS;
}

// Check the various places a file could be, and return the full path.
static Status ted_get_file(Ted const *ted, char const *name, char *out, size_t outsz) {
	if (ted->search_start_cwd && fs_file_exists(name)) {
		// check in start_cwd
		path_full(ted->start_cwd, name, out, outsz);
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

// Loads font from filename into *out, freeing any font that was previously there.
// *out is left unchanged on failure.
static void ted_load_font(Ted *ted, char const *filename, Font **out) {
	char font_filename[TED_PATH_MAX];
	if (ted_get_file(ted, filename, font_filename, sizeof font_filename)) {
		Font *font = text_font_load(font_filename, ted_active_settings(ted)->text_size);
		if (font) {
			// we don't properly handle variable-width fonts
			text_font_set_force_monospace(font, true);
			
			if (*out) {
				text_font_free(*out);
			}
			*out = font;
		} else {
			ted_seterr(ted, "Couldn't load font: %s", text_get_err());
			text_clear_err();
		}
	} else {
		ted_seterr(ted, "Couldn't find font file. There is probably a problem with your ted installation.");
	}
	
}

// Load all the fonts ted will use.
static void ted_load_fonts(Ted *ted) {
	ted_load_font(ted, "assets/font.ttf", &ted->font);
	ted_load_font(ted, "assets/font-bold.ttf", &ted->font_bold);
}


// sets the active buffer to this buffer, and updates active_node, etc. accordingly
// you can pass NULL to buffer to make it so no buffer is active.
void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer) {
	TextBuffer *search_buffer = find_search_buffer(ted);
	ted->active_buffer = buffer;
	autocomplete_close(ted);
	if (buffer != search_buffer) {
		if (ted->find)
			find_update(ted, true); // make sure find results are for this file
	}

	if (buffer >= ted->buffers && buffer < ted->buffers + TED_MAX_BUFFERS) {
		ted->prev_active_buffer = buffer;

		u16 idx = (u16)(buffer - ted->buffers);
		// now we need to figure out where this buffer is
		bool *nodes_used = ted->nodes_used;
		for (u16 i = 0; i < TED_MAX_NODES; ++i) {
			if (nodes_used[i]) {
				Node *node = &ted->nodes[i];
				arr_foreach_ptr(node->tabs, u16, tab) {
					if (idx == *tab) {
						node->active_tab = (u16)(tab - node->tabs);
						ted->active_node = node;
						signature_help_retrigger(ted);
						return;
					}
				}
			}
		}
		assert(0);
	} else {
		ted->active_node = NULL;
	}
	
}

// set ted->active_buffer to something nice
static void ted_reset_active_buffer(Ted *ted) {
	if (ted->nodes_used[0]) {
		Node *node = &ted->nodes[0];
		while (!node->tabs)
			node = &ted->nodes[node->split_a]; // arbitrarily pick split_a.
		ted_switch_to_buffer(ted, &ted->buffers[node->tabs[node->active_tab]]);
	} else {
		// there's nothing to set it to
		ted_switch_to_buffer(ted, NULL);
	}
}


// returns the index of an available buffer, or -1 if none are available 
static i32 ted_new_buffer(Ted *ted) {
	bool *buffers_used = ted->buffers_used;
	for (i32 i = 1; // start from 1, so as not to use the null buffer
		i < TED_MAX_BUFFERS; ++i) {
		if (!buffers_used[i]) {
			buffers_used[i] = true;
			buffer_create(&ted->buffers[i], ted);
			return i;
		}
	}
	return -1;
}

// Opposite of ted_new_buffer
// Make sure you set active_buffer to something else if you delete it!
static void ted_delete_buffer(Ted *ted, u16 index) {
	TextBuffer *buffer = &ted->buffers[index];
	if (buffer == ted->active_buffer)
		ted_switch_to_buffer(ted, NULL); // make sure we don't set the active buffer to something invalid
	if (buffer == ted->prev_active_buffer)
		ted->prev_active_buffer = NULL;
	buffer_free(buffer);
	ted->buffers_used[index] = false;
}

// Returns the index of an available node, or -1 if none are available 
static i32 ted_new_node(Ted *ted) {
	bool *nodes_used = ted->nodes_used;
	for (i32 i = 0; i < TED_MAX_NODES; ++i) {
		if (!nodes_used[i]) {
			memset(&ted->nodes[i], 0, sizeof ted->nodes[i]); // zero new node
			nodes_used[i] = true;
			return i;
		}
	}
	ted_seterr(ted, "Too many nodes.");
	return -1;
	
}

// how tall is a line buffer?
static float ted_line_buffer_height(Ted *ted) {
	float const char_height = text_font_char_height(ted->font);
	return char_height + 2 * ted_active_settings(ted)->border_thickness;
}

// switch to this node
static void ted_node_switch(Ted *ted, Node *node) {
	assert(node->tabs);
	ted_switch_to_buffer(ted, &ted->buffers[node->tabs[node->active_tab]]);
}

static bool node_tab_close(Ted *ted, Node *node, u16 index);

// Open a new buffer. Fills out *tab to the index of the tab used, and *buffer_idx to the index of the buffer.
// Returns true on success.
static Status ted_open_buffer(Ted *ted, u16 *buffer_idx, u16 *tab) {
	i32 new_buffer_index = ted_new_buffer(ted);
	if (new_buffer_index >= 0) {
		Node *node = ted->active_node;
		if (!node) {
			// no active node; let's create one!
			i32 node_idx = ted_new_node(ted);
			if (node_idx >= 0) {
				node = &ted->nodes[node_idx];
				ted->active_node = node;
			} else {
				ted_delete_buffer(ted, (u16)new_buffer_index);
				return false;
			}
		}
		if (arr_len(node->tabs) < TED_MAX_TABS) {
			arr_add(node->tabs, (u16)new_buffer_index);
			TextBuffer *new_buffer = &ted->buffers[new_buffer_index];
			node->active_tab = (u16)(arr_len(node->tabs) - 1);
			*buffer_idx = (u16)new_buffer_index;
			*tab = node->active_tab;
			ted_switch_to_buffer(ted, new_buffer);
			return true;
		} else {
			ted_seterr(ted, "Too many tabs.");
			ted_delete_buffer(ted, (u16)new_buffer_index);
			return false;
		}
	} else {
		return false;
	}
}


// Returns true on success
static bool ted_open_file(Ted *ted, char const *filename) {
	char path[TED_PATH_MAX];
	ted_path_full(ted, filename, path, sizeof path);

	// first, check if file is already open
	bool *buffers_used = ted->buffers_used;
	TextBuffer *buffers = ted->buffers;
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
		if (buffers_used[i]) {
			if (buffers[i].filename && paths_eq(path, buffers[i].filename)) {
				ted_switch_to_buffer(ted, &buffers[i]);
				return true;
			}
		}
	}
	
	// not open; we need to load it
	u16 buffer_idx, tab_idx;
	if (ted->active_buffer && buffer_is_untitled(ted->active_buffer) && buffer_empty(ted->active_buffer)) {
		// the active buffer is just an empty untitled buffer. open it here.
		return buffer_load_file(ted->active_buffer, path);
	} else if (ted_open_buffer(ted, &buffer_idx, &tab_idx)) {
		TextBuffer *buffer = &ted->buffers[buffer_idx];
		if (buffer_load_file(buffer, path)) {
			return true;
		} else {
			ted_seterr_to_buferr(ted, buffer);
			node_tab_close(ted, ted->active_node, tab_idx);
			ted_delete_buffer(ted, (u16)buffer_idx);
			return false;
		}
	} else {
		return false;
	}
}

static bool ted_new_file(Ted *ted, char const *filename) {
	u16 buffer_idx, tab_idx;
	char path[TED_PATH_MAX];
	if (filename)
		ted_path_full(ted, filename, path, sizeof path);
	else
		strbuf_cpy(path, TED_UNTITLED);
	if (ted_open_buffer(ted, &buffer_idx, &tab_idx)) {
		TextBuffer *buffer = &ted->buffers[buffer_idx];
		buffer_new_file(buffer, TED_UNTITLED);
		if (!buffer_haserr(buffer)) {
			return true;
		} else {
			ted_seterr_to_buferr(ted, buffer);
			node_tab_close(ted, ted->active_node, tab_idx);
			ted_delete_buffer(ted, (u16)buffer_idx);
			return false;
		}
	} else {
		return false;
	}
}


// save all changes to all buffers with unsaved changes.
// returns true if all buffers were saved successfully
static bool ted_save_all(Ted *ted) {
	bool success = true;
	bool *buffers_used = ted->buffers_used;
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
		if (buffers_used[i]) {
			TextBuffer *buffer = &ted->buffers[i];
			if (buffer_unsaved_changes(buffer)) {
				if (buffer->filename && buffer_is_untitled(buffer)) {
					ted_switch_to_buffer(ted, buffer);
					menu_open(ted, MENU_SAVE_AS);
					success = false; // we haven't saved this buffer yet; we've just opened the "save as" menu.
					break;
				} else {
					if (!buffer_save(buffer)) {
						success = false;
						ted_seterr_to_buferr(ted, buffer);
					}
				}
			}
		}
	}
	return success;
}

static void ted_reload_all(Ted *ted) {
	bool *buffers_used = ted->buffers_used;
	for (u64 i = 0; i < TED_MAX_BUFFERS; ++i) {
		if (buffers_used[i]) {
			TextBuffer *buffer = &ted->buffers[i];
			if (!buffer_unsaved_changes(buffer)) {
				buffer_reload(buffer);
			}
		}
	}
	if (ted->menu == MENU_ASK_RELOAD) {
		menu_close(ted);
	}
}

// load/reload configs
void ted_load_configs(Ted *ted, bool reloading) {
	if (reloading) config_free(ted);
	
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
	
	
	if (reloading) {
		// reset text size
		ted_load_fonts(ted);
	}
}

void ted_press_key(Ted *ted, SDL_Scancode scancode, SDL_Keymod modifier) {
	u32 key_combo = (u32)scancode << 3 |
		(u32)((modifier & (KMOD_LCTRL|KMOD_RCTRL)) != 0) << KEY_MODIFIER_CTRL_BIT |
		(u32)((modifier & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0) << KEY_MODIFIER_SHIFT_BIT |
		(u32)((modifier & (KMOD_LALT|KMOD_RALT)) != 0) << KEY_MODIFIER_ALT_BIT;
	if (key_combo < KEY_COMBO_COUNT) {
		KeyAction *action = &ted_active_settings(ted)->key_actions[key_combo];
		if (action->command) {
			command_execute(ted, action->command, action->argument);
		}
	}
}
