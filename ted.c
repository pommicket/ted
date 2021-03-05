static void menu_open(Ted *ted, Menu menu);
static void find_update(Ted *ted, bool force);
static Command command_from_str(char const *str);

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

static void ted_full_path(Ted *ted, char const *relpath, char *abspath, size_t abspath_size) {
	path_full(ted->cwd, relpath, abspath, abspath_size);
}

// Check the various places a file could be, and return the full path.
static Status ted_get_file(Ted const *ted, char const *name, char *out, size_t outsz) {
	if (ted->search_cwd && fs_file_exists(name)) {
		// check in current working directory
		str_cpy(out, outsz, name);
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
		Font *font = text_font_load(font_filename, ted->settings.text_size);
		if (font) {
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
	ted->active_buffer = buffer;
	ted->autocomplete = false;
	if (ted->find) find_update(ted, true);

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
static float ted_line_buffer_height(Ted const *ted) {
	Settings const *settings = &ted->settings;
	float const char_height = text_font_char_height(ted->font);
	return char_height + 2 * settings->border_thickness;
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
	ted_full_path(ted, filename, path, sizeof path);

	// first, check if file is already open
	bool *buffers_used = ted->buffers_used;
	TextBuffer *buffers = ted->buffers;
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
		if (buffers_used[i]) {
			if (buffers[i].filename && paths_eq(path, buffers[i].filename)) {
				buffer_reload(&buffers[i]); // make sure buffer is up to date with the file
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
		ted_full_path(ted, filename, path, sizeof path);
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
					if (!buffer_save(buffer))
						success = false;
				}
			}
		}
	}
	return success;
}
