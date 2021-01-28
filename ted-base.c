// this is a macro so we get -Wformat warnings
#define ted_seterr(buffer, ...) \
	snprintf(ted->error, sizeof ted->error - 1, __VA_ARGS__)

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

// returns buffer of new file, or NULL on failure
static WarnUnusedResult TextBuffer *ted_open_file(Ted *ted, char const *filename) {
	TextBuffer *open_to = &ted->main_buffer;
	if (buffer_load_file(open_to, filename)) {
		ted->active_buffer = open_to;
		return open_to;
	} else {
		return NULL;
	}
}

static void menu_open(Ted *ted, Menu menu);
static void menu_escape(Ted *ted);
