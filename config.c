// Read a ted configuration file.
// Config files are formatted as several sections, each containing key = value pairs.
// e.g.:
// [section1]
// thing1 = 33
// thing2 = 454
// [section2]
// asdf = 123

#include "ted-internal.h"
#include "pcre-inc.h"

/// Sections of `ted.cfg`
typedef enum {
	SECTION_NONE,
	SECTION_CORE,
	SECTION_KEYBOARD,
	SECTION_COLORS,
	SECTION_EXTENSIONS
} ConfigSection;

// all the "control" pointers here are relative to `settings_zero`.
typedef struct {
	const char *name;
	const bool *control;
	bool per_language; // allow per-language control
} SettingBool;
typedef struct {
	const char *name;
	const u8 *control;
	u8 min, max;
	bool per_language;
} SettingU8;
typedef struct {
	const char *name;
	const float *control;
	float min, max;
	bool per_language;
} SettingFloat;
typedef struct {
	const char *name;
	const u16 *control;
	u16 min, max;
	bool per_language;
} SettingU16;
typedef struct {
	const char *name;
	const u32 *control;
	u32 min, max;
	bool per_language;
} SettingU32;
typedef struct {
	const char *name;
	RcStr *const *control;
	bool per_language;
} SettingString;
typedef struct {
	const char *name;
	const KeyCombo *control;
	bool per_language;
} SettingKeyCombo;

typedef enum {
	SETTING_BOOL = 1,
	SETTING_U8,
	SETTING_U16,
	SETTING_U32,
	SETTING_FLOAT,
	SETTING_STRING,
	SETTING_KEY_COMBO
} SettingType;
typedef struct {
	SettingType type;
	const char *name;
	bool per_language;
	union {
		SettingU8 _u8;
		SettingBool _bool;
		SettingU16 _u16;
		SettingU32 _u32;
		SettingFloat _float;
		SettingString _string;
		SettingKeyCombo _key;
	} u;
} SettingAny;

// core settings
static const Settings settings_zero = {0};
static const SettingBool settings_bool[] = {
	{"auto-indent", &settings_zero.auto_indent, true},
#define SETTING_AUTO_ADD_NEWLINE {"auto-add-newline", &settings_zero.auto_add_newline, true}
	SETTING_AUTO_ADD_NEWLINE,
#define SETTING_REMOVE_TRAILING_WHITESPACE {"remove-trailing-whitespace", &settings_zero.remove_trailing_whitespace, true}
	SETTING_REMOVE_TRAILING_WHITESPACE,
	{"auto-reload", &settings_zero.auto_reload, true},
	{"auto-reload-config", &settings_zero.auto_reload_config, false},
	{"syntax-highlighting", &settings_zero.syntax_highlighting, true},
	{"line-numbers", &settings_zero.line_numbers, true},
	{"restore-session", &settings_zero.restore_session, false},
	{"regenerate-tags-if-not-found", &settings_zero.regenerate_tags_if_not_found, true},
#define SETTING_INDENT_WITH_SPACES {"indent-with-spaces", &settings_zero.indent_with_spaces, true}
	SETTING_INDENT_WITH_SPACES,
	{"trigger-characters", &settings_zero.trigger_characters, true},
	{"identifier-trigger-characters", &settings_zero.identifier_trigger_characters, true},
	{"phantom-completions", &settings_zero.phantom_completions, true},
	{"signature-help-enabled", &settings_zero.signature_help_enabled, true},
	{"document-links", &settings_zero.document_links, true},
	{"lsp-enabled", &settings_zero.lsp_enabled, true},
	{"lsp-log", &settings_zero.lsp_log, true},
	{"hover-enabled", &settings_zero.hover_enabled, true},
	{"vsync", &settings_zero.vsync, false},
	{"highlight-enabled", &settings_zero.highlight_enabled, true},
	{"highlight-auto", &settings_zero.highlight_auto, true},
	{"save-backup", &settings_zero.save_backup, true},
#define SETTING_CRLF {"crlf", &settings_zero.crlf, true}
	SETTING_CRLF,
#define SETTING_CRLF_WINDOWS {"crlf-windows", &settings_zero.crlf_windows, true}
	{"crlf-windows", &settings_zero.crlf_windows, true},
	{"jump-to-build-error", &settings_zero.jump_to_build_error, true},
	{"force-monospace", &settings_zero.force_monospace, true},
	{"show-diagnostics", &settings_zero.show_diagnostics, true},
};
static const SettingBool setting_auto_add_newline = SETTING_AUTO_ADD_NEWLINE;
static const SettingBool setting_indent_with_spaces = SETTING_INDENT_WITH_SPACES;
static const SettingBool setting_remove_trailing_whitespace = SETTING_REMOVE_TRAILING_WHITESPACE;
static const SettingBool setting_crlf = SETTING_CRLF;
static const SettingBool setting_crlf_windows = SETTING_CRLF_WINDOWS;
static const SettingU8 settings_u8[] = {
#define SETTING_TAB_WIDTH {"tab-width", &settings_zero.tab_width, 1, 100, true}
	SETTING_TAB_WIDTH,
	{"cursor-width", &settings_zero.cursor_width, 1, 100, true},
	{"undo-save-time", &settings_zero.undo_save_time, 1, 200, true},
	{"border-thickness", &settings_zero.border_thickness, 1, 30, false},
	{"padding", &settings_zero.padding, 0, 100, false},
	{"scrolloff", &settings_zero.scrolloff, 1, 100, true},
	{"tags-max-depth", &settings_zero.tags_max_depth, 1, 100, false},
};
static const SettingU8 setting_tab_width = SETTING_TAB_WIDTH;
static const SettingU16 settings_u16[] = {
	{"text-size", &settings_zero.text_size_no_dpi, TEXT_SIZE_MIN, TEXT_SIZE_MAX, false},
	{"max-menu-width", &settings_zero.max_menu_width, 10, U16_MAX, false},
	{"error-display-time", &settings_zero.error_display_time, 0, U16_MAX, false},
	{"framerate-cap", &settings_zero.framerate_cap, 3, 1000, false},
	{"lsp-port", &settings_zero.lsp_port, 0, 65535, true},
};
const SettingU16 setting_text_size_dpi_aware = {NULL, &settings_zero.text_size, 0, U16_MAX, false};
static const SettingU32 settings_u32[] = {
	{"max-file-size", &settings_zero.max_file_size, 100, 2000000000, false},
	{"max-file-size-view-only", &settings_zero.max_file_size_view_only, 100, 2000000000, false},
};
static const SettingFloat settings_float[] = {
	{"cursor-blink-time-on", &settings_zero.cursor_blink_time_on, 0, 1000, true},
	{"cursor-blink-time-off", &settings_zero.cursor_blink_time_off, 0, 1000, true},
	{"hover-time", &settings_zero.hover_time, 0, INFINITY, true},
	{"ctrl-scroll-adjust-text-size", &settings_zero.ctrl_scroll_adjust_text_size, -10, 10, true},
	{"lsp-delay", &settings_zero.lsp_delay, 0, 100, true},
};
static const SettingString settings_string[] = {
	{"build-default-command", &settings_zero.build_default_command, true},
	{"build-command", &settings_zero.build_command, true},
	{"root-identifiers", &settings_zero.root_identifiers, true},
	{"lsp", &settings_zero.lsp, true},
	{"lsp-configuration", &settings_zero.lsp_configuration, true},
	{"comment-start", &settings_zero.comment_start, true},
	{"comment-end", &settings_zero.comment_end, true},
	{"font", &settings_zero.font, false},
	{"font-bold", &settings_zero.font_bold, false},
};
static const SettingKeyCombo settings_key_combo[] = {
	{"hover-key", &settings_zero.hover_key, true},
	{"highlight-key", &settings_zero.highlight_key, true},
};


bool config_applies_to(Config *cfg, const char *path, Language language) {
	if (cfg->language && language != cfg->language)
		return false;
	if (cfg->path) {
		if (!path)
			return false;
		pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8(cfg->path, NULL);
		bool match = pcre2_match_8(cfg->path, (const u8 *)path, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL) > 0;
		pcre2_match_data_free_8(match_data);
		if (!match)
			return false;
	}
	return true;
}

// if this returns true, `a` and `b` have the same context (`a` only applies if `b` does)
static bool config_definitely_has_same_context(const Config *a, const Config *b) {
	if (a->language != b->language)
		return false;
	if (a->path_regex && !b->path_regex)
		return false;
	if (!a->path_regex && b->path_regex)
		return false;
	if (a->path_regex && !streq(a->path_regex, b->path_regex))
		return false;
	return true;
}


static void config_set_setting(Config *cfg, ptrdiff_t offset, const void *value, size_t size) {
	memmove((char *)&cfg->settings + offset, value, size);
	memset(&cfg->settings_set[offset], 1, size);
}

static void config_set_bool(Config *cfg, const SettingBool *set, bool value) {
	config_set_setting(cfg, (char*)set->control - (char*)&settings_zero, &value, sizeof value);
}
static void config_set_u8(Config *cfg, const SettingU8 *set, u8 value) {
	if (value >= set->min && value <= set->max)
		config_set_setting(cfg, (char*)set->control - (char*)&settings_zero, &value, sizeof value);
}
static void config_set_u16(Config *cfg, const SettingU16 *set, u16 value) {
	if (value >= set->min && value <= set->max)
		config_set_setting(cfg, (char*)set->control - (char*)&settings_zero, &value, sizeof value);
}
static void config_set_u32(Config *cfg, const SettingU32 *set, u32 value) {
	if (value >= set->min && value <= set->max)
		config_set_setting(cfg, (char*)set->control - (char*)&settings_zero, &value, sizeof value);
}
static void config_set_float(Config *cfg, const SettingFloat *set, float value) {
	if (value >= set->min && value <= set->max)
		config_set_setting(cfg, (char*)set->control - (char*)&settings_zero, &value, sizeof value);
}
static void config_set_key_combo(Config *cfg, const SettingKeyCombo *set, KeyCombo value) {
	config_set_setting(cfg, (char *)set->control - (char *)&settings_zero, &value, sizeof value);
}
static void config_set_string(Config *cfg, const SettingString *set, const char *value) {
	assert(value);
	Settings *settings = &cfg->settings;
	const ptrdiff_t offset = ((char *)set->control - (char*)&settings_zero);
	RcStr **control = (RcStr **)((char *)settings + offset);
	if (*control) rc_str_decref(control);
	RcStr *rc = rc_str_new(value, -1);
	config_set_setting(cfg, offset, &rc, sizeof (RcStr *));
}
static void config_set_color(Config *cfg, ColorSetting setting, u32 color) {
	config_set_setting(cfg, (char *)&settings_zero.colors[setting] - (char *)&settings_zero, &color, sizeof color);
}



typedef struct {
	Ted *ted;
	const char *filename;
	FILE *fp;
	ConfigSection section;
	u32 line_number; // currently processing this line number
	bool error;
} ConfigReader;

static void config_verr(ConfigReader *cfg, const char *fmt, va_list args) {
	if (cfg->error) return;
	cfg->error = true;
	char error[1024] = {0};
	strbuf_printf(error, "%s:%u: ", cfg->filename, cfg->line_number);
	vsnprintf(error + strlen(error), sizeof error - strlen(error) - 1, fmt, args);
	ted_error(cfg->ted, "%s", error);
}

static void config_err(ConfigReader *cfg, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
static void config_err(ConfigReader *cfg, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	config_verr(cfg, fmt, args);
	va_end(args);
}

static void config_debug_err(ConfigReader *cfg, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
static void config_debug_err(ConfigReader *cfg, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	#if DEBUG
		config_verr(cfg, fmt, args);
	#else
		ted_vlog(cfg->ted, fmt, args);
	#endif
	va_end(args);
}

static void settings_free_set(Settings *settings, const bool *set) {
	if (set[offsetof(Settings, language_extensions)])
		arr_free(settings->language_extensions);
	if (set[offsetof(Settings, key_actions)]) {
		arr_foreach_ptr(settings->key_actions, KeyAction, act) {
			free((void *)act->argument.string);
		}
		arr_free(settings->key_actions);
	}
	if (set[offsetof(Settings, bg_shader)])
		gl_rc_sab_decref(&settings->bg_shader);
	if (set[offsetof(Settings, bg_texture)])
		gl_rc_texture_decref(&settings->bg_texture);
	for (size_t i = 0; i < arr_count(settings_string); i++) {
		const SettingString *s = &settings_string[i];
		const ptrdiff_t offset = (char *)s->control - (char *)&settings_zero;
		if (set[offset]) {
			RcStr **rc = (RcStr **)((char *)settings + offset);
			rc_str_decref(rc);
		}
	}
}

void settings_free(Settings *settings) {
	static bool all_set[sizeof(Settings)];
	memset(all_set, 1, sizeof all_set);
	settings_free_set(settings, all_set);
	memset(settings, 0, sizeof *settings);
}

static void config_free(Config *cfg) {
	settings_free(&cfg->settings);
	pcre2_code_free_8(cfg->path);
	free(cfg->path_regex);
	rc_str_decref(&cfg->source);
	memset(cfg, 0, sizeof *cfg);
}


i32 config_priority(const Config *cfg) {
	size_t path_len = cfg->path ? strlen(cfg->path_regex) : 0;
	return (i32)path_len * 2 + (cfg->language != 0);
}

static KeyAction key_action_copy(const KeyAction *src) {
	KeyAction cpy = *src;
	if (cpy.argument.string)
		cpy.argument.string = str_dup(cpy.argument.string);
	return cpy;
}

void config_merge_into(Settings *dest, const Config *src_cfg) {
	const Settings *src = &src_cfg->settings;
	char *destc = (char *)dest;
	const char *srcc = (const char *)src;
	const bool *set = src_cfg->settings_set;
	settings_free_set(dest, set);
	for (size_t i = 0; i < sizeof(Settings); i++) {
		if (set[i])
			destc[i] = srcc[i];
	}
	// increment reference counts for things we've borrowed from src
	if (set[offsetof(Settings, bg_shader)])
		gl_rc_sab_incref(dest->bg_shader);
	if (set[offsetof(Settings, bg_texture)])
		gl_rc_texture_incref(dest->bg_texture);
	for (size_t i = 0; i < arr_count(settings_string); i++) {
		const SettingString *s = &settings_string[i];
		ptrdiff_t offset = (char *)s->control - (char *)&settings_zero;
		if (!set[offset]) continue;
		RcStr *rc = *(RcStr **)((char *)dest + offset);
		rc_str_incref(rc);
	}
	// we should never copy these from src
	assert(!set[offsetof(Settings, language_extensions)]);
	assert(!set[offsetof(Settings, key_actions)]);

	// merge language_extensions and key_actions
	arr_foreach_ptr(src->language_extensions, LanguageExtension, ext)
		arr_add(dest->language_extensions, *ext);
	arr_foreach_ptr(src->key_actions, KeyAction, act)
		arr_add(dest->key_actions, key_action_copy(act));
}

static void config_err_unexpected_eof(ConfigReader *reader) {
	config_err(reader, "Unexpected EOF (no newline at end of file?)");
}

static char config_getc_no_err_on_eof(ConfigReader *reader) {
	int c = getc(reader->fp);
	if (c == 0) {
		config_err(reader, "Null byte in config file");
	}
	if (c == EOF) {
		c = 0;
	}
	if (c == '\n') {
		reader->line_number += 1;
	}
	return (char)c;
}

static char config_getc(ConfigReader *reader) {
	char c = config_getc_no_err_on_eof(reader);
	if (!c)
		config_err_unexpected_eof(reader);
	return c;
}

static void config_ungetc(ConfigReader *reader, char c) {
	if (c == '\n')
		reader->line_number -= 1;
	if (c)
		ungetc(c, reader->fp);
}

static void config_skip_space(ConfigReader *reader) {
	char c;
	while ((c = config_getc(reader))) {
		if (!isspace(c) || c == '\n') {
			config_ungetc(reader, c);
			break;
		}
	}
}

static void config_read_to_eol(ConfigReader *reader, char *buf, size_t bufsz) {
	assert(bufsz < INT_MAX);
	if (!fgets(buf, (int)bufsz, reader->fp)) {
		config_err_unexpected_eof(reader);
		*buf = '\0';
	}
	if (strchr(buf, '\n'))
		reader->line_number += 1;
	buf[strcspn(buf, "\r\n")] = '\0';
}

static SDL_Keycode config_parse_key(ConfigReader *cfg, const char *str) {
	SDL_Keycode keycode = SDL_GetKeyFromName(str);
	if (keycode != SDLK_UNKNOWN)
		return keycode;
	typedef struct {
		const char *keyname1;
		const char *keyname2; // alternate key name
		SDL_Keycode keycode;
	} KeyName;
	static KeyName const key_names[] = {
		{"X1", NULL, KEYCODE_X1},
		{"X2", NULL, KEYCODE_X2},
		{"Enter", NULL, SDLK_RETURN},
		{"Equals", "Equal", SDLK_EQUALS},
	};
	for (size_t i = 0; i < arr_count(key_names); ++i) {
		KeyName const *k = &key_names[i];
		if (streq_case_insensitive(str, k->keyname1) || (k->keyname2 && streq_case_insensitive(str, k->keyname2))) {
			keycode = k->keycode;
			break;
		}
	}
	if (keycode != SDLK_UNKNOWN)
		return keycode;
		
	if (isdigit(str[0])) { // direct keycode numbers, e.g. Ctrl+24 or Ctrl+08
		char *endp;
		long n = strtol(str, &endp, 10);
		if (*endp == '\0' && n > 0) {
			return (SDL_Keycode)n;
		} else {
			config_err(cfg, "Invalid keycode number: %s", str);
			return 0;
		}
	} else {
		config_err(cfg, "Unrecognized key name: %s.", str);
		return 0;
	}
}
// Returns the key combination described by str.
static KeyCombo config_parse_key_combo(ConfigReader *cfg, const char *str) {
	u32 modifier = 0;
	// read modifier
	while (true) {
		if (str_has_prefix(str, "Ctrl+")) {
			if (modifier & KEY_MODIFIER_CTRL) {
				config_err(cfg, "Ctrl+ written twice");
				return (KeyCombo){0};
			}
			modifier |= KEY_MODIFIER_CTRL;
			str += strlen("Ctrl+");
		} else if (str_has_prefix(str, "Shift+")) {
			if (modifier & KEY_MODIFIER_SHIFT) {
				config_err(cfg, "Shift+ written twice");
				return (KeyCombo){0};
			}
			modifier |= KEY_MODIFIER_SHIFT;
			str += strlen("Shift+");
		} else if (str_has_prefix(str, "Alt+")) {
			if (modifier & KEY_MODIFIER_ALT) {
				config_err(cfg, "Alt+ written twice");
				return (KeyCombo){0};
			}
			modifier |= KEY_MODIFIER_ALT;
			str += strlen("Alt+");
		} else break;
	}

	// read key
	SDL_Keycode keycode = config_parse_key(cfg, str);
	if (keycode == SDLK_UNKNOWN)
		return (KeyCombo){0};
	return KEY_COMBO(modifier, keycode);
}


static bool settings_initialized = false;
static SettingAny settings_all[1000] = {0};

static void config_init_settings(void) {
	if (settings_initialized) return;
	
	SettingAny *opt = settings_all;
	for (size_t i = 0; i < arr_count(settings_bool); ++i) {
		opt->type = SETTING_BOOL;
		opt->name = settings_bool[i].name;
		opt->per_language = settings_bool[i].per_language;
		opt->u._bool = settings_bool[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_u8); ++i) {
		opt->type = SETTING_U8;
		opt->name = settings_u8[i].name;
		opt->per_language = settings_u8[i].per_language;
		opt->u._u8 = settings_u8[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_u16); ++i) {
		opt->type = SETTING_U16;
		opt->name = settings_u16[i].name;
		opt->per_language = settings_u16[i].per_language;
		opt->u._u16 = settings_u16[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_u32); ++i) {
		opt->type = SETTING_U32;
		opt->name = settings_u32[i].name;
		opt->per_language = settings_u32[i].per_language;
		opt->u._u32 = settings_u32[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_float); ++i) {
		opt->type = SETTING_FLOAT;
		opt->name = settings_float[i].name;
		opt->per_language = settings_float[i].per_language;
		opt->u._float = settings_float[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_string); ++i) {
		opt->type = SETTING_STRING;
		opt->name = settings_string[i].name;
		opt->per_language = settings_string[i].per_language;
		opt->u._string = settings_string[i];
		++opt;
	}
	for (size_t i = 0; i < arr_count(settings_key_combo); ++i) {
		opt->type = SETTING_KEY_COMBO;
		opt->name = settings_key_combo[i].name;
		opt->per_language = settings_key_combo[i].per_language;
		opt->u._key = settings_key_combo[i];
		++opt;
	}
	settings_initialized = true;
}


static void get_config_path(Ted *ted, char *expanded, size_t expanded_sz, const char *path) {
	assert(path != expanded);
	
	expanded[0] = '\0';
	if (path[0] == '~' && is_path_separator(path[1])) {
		str_printf(expanded, expanded_sz, "%s%c%s", ted->home, PATH_SEPARATOR, path + 1);
	} else if (!path_is_absolute(path)) {
		if (!ted_get_file(ted, path, expanded, expanded_sz)) {
			str_cpy(expanded, expanded_sz, path);
		}
	} else {
		str_cpy(expanded, expanded_sz, path);
	}
	
}

// only reads fp for multi-line strings
// return value should be freed.
static char *config_read_string(ConfigReader *reader, char delimiter) {
	char *str = NULL;
	while (true) {
		char c = config_getc(reader);
		if (c == delimiter)
			break;
		switch (c) {
		case '\\':
			c = config_getc(reader);
			switch (c) {
			case '\\':
			case '"':
			case '`':
				break;
			case 'n':
				arr_add(str, '\n');
				continue;
			case 'r':
				arr_add(str, '\r');
				continue;
			case 't':
				arr_add(str, '\t');
				continue;
			case '[':
				arr_add(str, '[');
				continue;
			default:
				config_err(reader, "Unrecognized escape sequence: '\\%c'.", c);
				arr_clear(str);
				return NULL;
			}
			break;
		default:
			arr_add(str, c);
			break;
		}
	}
	
	char *s = strn_dup(str, arr_len(str));
	arr_free(str);
	return s;
}

static void settings_load_bg_shader(Ted *ted, Config *cfg, const char *bg_shader_text) {
	char vshader[8192] ;
	strbuf_printf(vshader, "attribute vec2 v_pos;\n\
OUT vec2 t_pos;\n\
void main() { \n\
	gl_Position = vec4(v_pos * 2.0 - 1.0, 0.0, 1.0);\n\
	t_pos = v_pos;\n\
}");
	char fshader[8192];
	strbuf_printf(fshader, "IN vec2 t_pos;\n\
uniform float t_time;\n\
uniform float t_save_time;\n\
uniform vec2 t_aspect;\n\
uniform sampler2D t_texture;\n\
#line 1\n\
%s", bg_shader_text);
	
	
	char error[512] = {0};
	GLuint shader = gl_compile_and_link_shaders(error, vshader, fshader);
	if (*error)
		ted_error(ted, "%s", error);
	if (!shader) return;
	
	GLuint buffer = 0, array = 0;
	glGenBuffers(1, &buffer);
	if (gl_version_major >= 3) {
		glGenVertexArrays(1, &array);
		glBindVertexArray(array);
	}
	
	
	float buffer_data[][2] = {
		{0,0},
		{1,0},
		{1,1},
		{0,0},
		{1,1},
		{0,1}
	};

	GLuint v_pos = (GLuint)glGetAttribLocation(shader, "v_pos");
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof buffer_data, buffer_data, GL_STATIC_DRAW);
	glVertexAttribPointer(v_pos, 2, GL_FLOAT, 0, 2 * sizeof(float), 0);
	glEnableVertexAttribArray(v_pos);

	cfg->settings.bg_shader = gl_rc_sab_new(shader, array, buffer);
}


static void settings_load_bg_texture(Ted *ted, Config *cfg, const char *path) {
	char expanded[TED_PATH_MAX];
	get_config_path(ted, expanded, sizeof expanded, path);
	
	GLuint texture = gl_load_texture_from_image(expanded);
	if (!texture) {
		ted_error(ted, "Couldn't load image %s", path);
		return;
	}

	cfg->settings.bg_texture = gl_rc_texture_new(texture);	
}

static void config_parse_line(ConfigReader *reader, Config *cfg) {
	Ted *ted = reader->ted;
	if (reader->section == 0) {
		// there was an error reading this section. don't bother with anything else.
		return;
	}
	char key[128] = {0};
	char c;
	for (size_t i = 0; i < sizeof key - 1; ++i) {
		c = config_getc(reader);
		if (!c) break;
		if (c == '=') break;
		if (c == '\n') break;
		key[i] = c;
	}
	str_trim(key);
	if (key[0] == 0) {
		return;
	}
	if (c != '=') {
		config_err(reader, "Unexpected end-of-line (expected key = value)");
		return;
	}
	config_skip_space(reader);
	
	switch (reader->section) {
	case SECTION_NONE:
		config_err(reader, "Line outside of any section."
			"Try putting a section header, e.g. [keyboard] before this line?");
		break;
	case SECTION_COLORS: {
		ColorSetting setting = color_setting_from_str(key);
		if (setting != COLOR_UNKNOWN) {
			char value[32] = {0};
			config_read_to_eol(reader, value, sizeof value);
			str_trim(value);
			u32 color = 0;
			if (color_from_str(value, &color)) {
				config_set_color(cfg, setting, color);
			} else {
				config_err(reader, "'%s' is not a valid color. Colors should look like #rgb, #rgba, #rrggbb, or #rrggbbaa.", value);
			}
		} else {
			// don't actually produce this error.
			// we have removed colors in the past and might again in the future.
		#if 0
			config_err(cfg, "No such color setting: %s", key);
		#endif
		}
	} break;
	case SECTION_KEYBOARD: {
		// lines like Ctrl+Down = 10 :down
		KeyCombo key_combo = config_parse_key_combo(reader, key);
		KeyAction action = {0};
		action.key_combo = key_combo;
		CommandArgument argument = {
			.number = 1, // default argument = 1
			.string = NULL
		};
		c = config_getc(reader);
		if (isdigit(c)) {
			// read the argument
			char num[32] = {c};
			for (size_t i = 1; i < sizeof num - 1; i++) {
				c = config_getc(reader);
				if (!c || c == ' ') break;
				num[i] = c;
			}
			argument.number = atoll(num);
			config_skip_space(reader);
			c = config_getc(reader);
		} else if (c == '"' || c == '`') {
			// string argument
			argument.string = config_read_string(reader, (char)c);
			config_skip_space(reader);
			c = config_getc(reader);
		}
		if (c == ':') {
			char cmd_str[64];
			config_read_to_eol(reader, cmd_str, sizeof cmd_str);
			// read the command
			Command command = command_from_str(cmd_str);
			if (command != CMD_UNKNOWN) {
				action.command = command;
				action.argument = argument;
			} else {
				config_err(reader, "Unrecognized command %s", cmd_str);
			}
		} else {
			config_err(reader, "Expected ':' for key action. This line should look something like: %s = :command.", key);
		}
		
		Settings *settings = &cfg->settings;
		bool have = false;
		// check if we already have an action for this key combo
		arr_foreach_ptr(settings->key_actions, KeyAction, act) {
			if (act->key_combo.value == key_combo.value) {
				*act = action;
				have = true;
				break;
			}
		}
		// if this is a new key combo, add an element to the key_actions array
		if (!have)
			arr_add(settings->key_actions, action);
	} break;
	case SECTION_EXTENSIONS: {
		Language lang = language_from_str(key);
		if (lang == LANG_NONE) {
			config_err(reader, "Invalid programming language: %s.", key);
		} else {
			char exts[2048];
			config_read_to_eol(reader, exts, sizeof exts);
			char *dst = exts;
			// get rid of whitespace in extension list
			for (const char *src = exts; *src; ++src)
				if (!isspace(*src))
					*dst++ = *src;
			*dst = 0;
			Settings *settings = &cfg->settings;
			// remove old extensions
			u32 *indices = NULL;
			arr_foreach_ptr(settings->language_extensions, LanguageExtension, ext) {
				if (ext->language == lang) {
					arr_add(indices, (u32)(ext - settings->language_extensions));
				}
			}
			for (u32 i = 0; i < arr_len(indices); ++i)
				arr_remove(settings->language_extensions, indices[i] - i);
			arr_free(indices);
			
			char *p = exts;
			while (*p) {
				while (*p == ',')
					++p;
				if (*p == '\0')
					break;
				size_t len = strcspn(p, ",");
				LanguageExtension *ext = arr_addp(settings->language_extensions);
				ext->language = lang;
				memcpy(ext->extension, p, len);
				p += len;
			}
		}
	} break;
	case SECTION_CORE: {
		char *needs_freeing = NULL;
		char *value = NULL;
		char line_buf[2048];
		c = config_getc(reader);
		const char *endptr;
		i64 integer = 0;
		double floating = 0;
		bool is_integer = false;
		bool is_floating = false;
		bool is_bool = false;
		bool boolean = false;
		if (c == '"' || c == '`') {
			char *string = config_read_string(reader, c);
			if (!string) break;
			needs_freeing = string;
			value = string;
		} else {
			config_ungetc(reader, c);
			config_read_to_eol(reader, line_buf, sizeof line_buf);
			value = line_buf;
			integer = (i64)strtoll(value, (char **)&endptr, 10);
			is_integer = *endptr == '\0';
			floating = strtod(value, (char **)&endptr);
			is_floating = *endptr == '\0';
			if (streq(value, "yes") || streq(value, "on") || streq(value, "true")) {
				is_bool = true;
				boolean = true;
			} else if (streq(value, "no") || streq(value, "off") || streq(value, "false")) {
				is_bool = true;
				boolean = false;
			}
		}
		const SettingAny *setting_any = NULL;
		for (u32 i = 0; i < arr_count(settings_all); ++i) {
			SettingAny const *s = &settings_all[i];
			if (s->type == 0) break;
			if (streq(key, s->name)) {
				setting_any = s;
				break;
			}
		}
		
		if (!setting_any) {
			if (streq(key, "bg-shader"))
				settings_load_bg_shader(ted, cfg, value);
			else if (streq(key, "bg-texture"))
				settings_load_bg_texture(ted, cfg, value);
			// it's probably a bad idea to error on unrecognized settings
			// because if we ever remove a setting in the future
			// everyone will get errors
			break;
		}
		
		if (cfg->language != 0 && !setting_any->per_language) {
			config_err(reader, "Setting %s cannot be controlled for individual languages.", key);
			break;
		}
		
		switch (setting_any->type) {
		case SETTING_BOOL: {
			const SettingBool *setting = &setting_any->u._bool;
			if (is_bool)
				config_set_bool(cfg, setting, boolean);
			else
				config_err(reader, "Invalid %s: %s. This should be yes, no, on, or off.", setting->name, value);
		} break;
		case SETTING_U8: {
			const SettingU8 *setting = &setting_any->u._u8;
			if (is_integer && integer >= setting->min && integer <= setting->max)
				config_set_u8(cfg, setting, (u8)integer);
			else
				config_err(reader, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
		} break;
		case SETTING_U16: {
			const SettingU16 *setting = &setting_any->u._u16;
			if (is_integer && integer >= setting->min && integer <= setting->max)
				config_set_u16(cfg, setting, (u16)integer);
			else
				config_err(reader, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
		} break;
		case SETTING_U32: {
			const SettingU32 *setting = &setting_any->u._u32;
			if (is_integer && integer >= setting->min && integer <= setting->max)
				config_set_u32(cfg, setting, (u32)integer);
			else
				config_err(reader, "Invalid %s: %s. This should be an integer from %" PRIu32 " to %" PRIu32 ".",
					setting->name, value, setting->min, setting->max);
		} break;
		case SETTING_FLOAT: {
			const SettingFloat *setting = &setting_any->u._float;
			if (is_floating && floating >= setting->min && floating <= setting->max)
				config_set_float(cfg, setting, (float)floating);
			else
				config_err(reader, "Invalid %s: %s. This should be a number from %g to %g.", setting->name, value, setting->min, setting->max);
		} break;
		case SETTING_STRING: {
			const SettingString *setting = &setting_any->u._string;
			config_set_string(cfg, setting, value);
		} break;
		case SETTING_KEY_COMBO: {
			const SettingKeyCombo *setting = &setting_any->u._key;
			KeyCombo combo = config_parse_key_combo(reader, value);
			if (combo.value) {
				config_set_key_combo(cfg, setting, combo);
			}
		} break;
		}
		free(needs_freeing);
	} break;
	}
}

static int key_action_qsort_cmp_combo(const void *av, const void *bv) {
	const KeyAction *a = av, *b = bv;
	if (a->key_combo.value < b->key_combo.value)
		return -1;
	if (a->key_combo.value > b->key_combo.value)
		return 1;
	return 0;
}

void settings_finalize(Ted *ted, Settings *settings) {
	arr_qsort(settings->key_actions, key_action_qsort_cmp_combo);
	settings->text_size = clamp_u16((u16)roundf((float)settings->text_size_no_dpi * ted_get_ui_scaling(ted)), TEXT_SIZE_MIN, TEXT_SIZE_MAX);
#if _WIN32
	settings->crlf |= settings->crlf_windows;
#endif
}

static void config_compile_regex(Config *cfg, ConfigReader *reader) {
	if (cfg->path_regex) {
		const char *regex = cfg->path_regex;
		#if _WIN32
		char fixed[8192];
		// replace forward slashes with backslashes
		//  (but actually double backslashes because this is a regex)
		for (size_t in = 0, out = 0; out < sizeof fixed - 4 && regex[in]; ++in) {
			if (regex[in] == '/') {
				fixed[out++] = '\\';
				fixed[out++] = '\\';
			} else {
				fixed[out++] = regex[in];
			}
			fixed[out] = 0;
		}
		regex = fixed;
		#endif
		// compile regex
		int error_code = 0;
		PCRE2_SIZE error_offset = 0;
		cfg->path = pcre2_compile_8((const u8 *)regex, PCRE2_ZERO_TERMINATED, PCRE2_ANCHORED, &error_code, &error_offset, NULL);
		if (!cfg->path) {
			config_err(reader, "Bad regex (at offset %u): %s", (unsigned)error_offset, regex);
			free(cfg->path_regex); cfg->path_regex = NULL;
		}
	}
}

static bool regex_char_needs_escaping(char c) {
	return strchr("\\^.$|()[]*+?{}-", c);
}

static void config_read_ted_cfg(Ted *ted, RcStr *cfg_path_rc, const char ***include_stack) {
	const char *const cfg_path = rc_str(cfg_path_rc, "");
	const bool is_local = str_has_suffix(cfg_path, ".ted.cfg")
		&& is_path_separator(cfg_path[strlen(cfg_path) - 9]);
	// check for, e.g. %include ted.cfg inside ted.cfg
	arr_foreach_ptr(*include_stack, const char *, p_include) {
		if (streq(cfg_path, *p_include)) {
			char text[1024];
			strbuf_cpy(text, "%include loop in config files: ");
			strbuf_cat(text, (*include_stack)[0]);
			for (u32 i = 1; i < arr_len(*include_stack); ++i) {
				if (i > 1)
					strbuf_cat(text, ", which");
				strbuf_catf(text, " includes %s", (*include_stack)[i]);
			}
			if (arr_len(*include_stack) > 1)
				strbuf_cat(text, ", which");
			strbuf_catf(text, " includes %s", cfg_path);
			ted_error(ted, "%s", text);
			return;
		}
	}
	arr_add(*include_stack, cfg_path);
	
	FILE *fp = fopen(cfg_path, "rb");
	if (!fp) {
		return;
	}
	
	ConfigReader reader_data = {
		.ted = ted,
		.filename = cfg_path,
		.line_number = 1,
		.fp = fp,
		.error = false
	};
	ConfigReader *reader = &reader_data;
	
	Config *cfg = NULL;
	
	while (true) {
		char c = config_getc_no_err_on_eof(reader);
		if (!c) break;
		if (c == '[') {
			// a new section!
			#define SECTION_HEADER_HELP "Section headers should look like this: [(path//)(language.)section-name]"
			char header[256];
			config_read_to_eol(reader, header, sizeof header);
			char path[TED_PATH_MAX]; path[0] = '\0';
			if (is_local) {
				// prepend directory
				char dirname[TED_PATH_MAX];
				strbuf_cpy(dirname, cfg_path);
				path_dirname(dirname);
				for (size_t i = 0, out = 0; dirname[i] && out < sizeof path - 3; i++) {
					if (regex_char_needs_escaping(dirname[i])) {
						path[out++] = '\\';
					}
					path[out++] = dirname[i];
					path[out] = '\0';
				}
			}
			Language language = 0;
			if (strlen(header) == 0 || header[strlen(header) - 1] != ']') {
				config_err(reader, "Section header doesn't end with ]\n" SECTION_HEADER_HELP);
				break;
			} else {
				header[strlen(header) - 1] = 0;
				char *p = header;
				char *path_end = strstr(p, "//");
				if (path_end) {
					if (is_local) {
						// important we do this here and not above so that
						// if we have /foo/.ted.cfg
						// [colors]
						// bg = #f00
						//  ^^ this should apply to the path "/foo" (not just "/foo/something")
						strbuf_catf(path, "%c", PATH_SEPARATOR);
					}
					size_t path_len = (size_t)(path_end - header);
					path[0] = '\0';
					// expand ~
					if (p[0] == '~') {
						str_cpy(path, sizeof path, ted->home);
						++p;
						--path_len;
					}
					strn_cat(path, sizeof path, p, path_len);
					p = path_end + 2;
				}
				
				char *dot = strchr(p, '.');
				if (dot) {
					*dot = '\0';
					language = language_from_str(p);
					if (!language) {
						config_err(reader, "Unrecognized language: %s.", p);
					}
					p = dot + 1;
				}
				
				if (streq(p, "keyboard")) {
					reader->section = SECTION_KEYBOARD;
				} else if (streq(p, "colors")) {
					reader->section = SECTION_COLORS;
				} else if (streq(p, "core")) {
					reader->section = SECTION_CORE;
				} else if (streq(p, "extensions")) {
					if (language != 0 || *path) {
						config_err(reader, "Extensions section cannot be language- or path-specific.");
						break;
					}
					reader->section = SECTION_EXTENSIONS;
				} else {
					config_err(reader, "Unrecognized section: [%s].", p);
				}
			}
			Config new_cfg = {
				.language = language,
				.path_regex = *path ? path : NULL,
			};
			cfg = NULL;
			// search for config with same context to update
			arr_foreach_ptr(ted->all_configs, Config, conf) {
				if (config_definitely_has_same_context(conf, &new_cfg)) {
					cfg = conf;
				}
			}
			if (!cfg) {
				// create new config
				cfg = arr_addp(ted->all_configs);
				cfg->path_regex = *path ? str_dup(path) : NULL;
				cfg->language = language;
				cfg->format = CONFIG_TED_CFG;
				cfg->source = rc_str_copy(cfg_path_rc);
				config_compile_regex(cfg, reader);
			}
		} else if (c == '%') {
			char line[2048];
			config_read_to_eol(reader, line, sizeof line);
			if (str_has_prefix(line, "include ")) {
				char included[TED_PATH_MAX];
				char expanded[TED_PATH_MAX];
				strbuf_cpy(included, line + strlen("include "));
				str_trim(included);
				get_config_path(ted, expanded, sizeof expanded, included);
				RcStr *expanded_rc = rc_str_new(expanded, -1);
				config_read_ted_cfg(ted, expanded_rc, include_stack);
				rc_str_decref(&expanded_rc);
			} else {
				config_err(reader, "Unrecognized directive: %s", line);
			}
		} else if (isspace(c)) {
			// whitespace
		} else if (c == '#') {
			// comment
			char buf[4096];
			config_read_to_eol(reader, buf, sizeof buf);
		} else if (cfg) {
			config_ungetc(reader, c);
			config_parse_line(reader, cfg);
		} else {
			config_err(reader, "Config has text before first section header.");
		}
	}
	if (ferror(fp))
		ted_error(ted, "Error reading %s.", cfg_path);
	fclose(fp);
	arr_remove_last(*include_stack);
}

static void regex_append_literal_char(StrBuilder *b, char c) {
	if (regex_char_needs_escaping(c))
		str_builder_appendf(b, "\\%c", c);
	else
		str_builder_appendf(b, "%c", c);
}

// handles 000-999 as 000|001|002|...|999 for example (which is why we need strings)
static void regex_append_number_str_range(StrBuilder *b, const char *s1, const char *s2) {
	assert(strlen(s1) == strlen(s2));
	assert(strcmp(s1, s2) <= 0);
	if (s1[0] == 0) return;
	if (s1[0] == s2[0]) {
		int common_prefix = 1;
		while (s1[common_prefix] && s1[common_prefix] == s2[common_prefix])
			common_prefix += 1;
		str_builder_appendf(b, "%.*s", common_prefix, s1);
		if (s1[common_prefix]) {
			str_builder_append(b, "(");
			regex_append_number_str_range(b, &s1[common_prefix], &s2[common_prefix]);
			str_builder_append(b, ")");
		}
		return;
	}
	assert(s1[0] < s2[0]);
	if (!s1[1]) {
		// single digits
		str_builder_appendf(b, "[%c-%c]", s1[0], s2[0]);
		return;
	}
	bool first = true;
	char midstart = s1[0] + 1, midend = s2[0] - 1;
	char s[24];
	strcpy(s, s1);
	for (size_t i = 1; s[i]; i++) {
		s[i] = '9';
	}
	if (strspn(s1 + 1, "0") == strlen(s1 + 1)) {
		// s is 100 or something so we can just do 1[0-9]{2}
		--midstart;
	} else {
		if (!first) str_builder_append(b, "|");
		first = false;
		regex_append_number_str_range(b, s1, s);
	}
	strcpy(s, s2);
	for (size_t i = 1; s[i]; i++) {
		s[i] = '0';
	}
	if (strspn(s2 + 1, "9") == strlen(s2 + 1)) {
		++midend;
	} else {
		if (!first) str_builder_append(b, "|");
		first = false;
		regex_append_number_str_range(b, s, s2);
	}
	// the middle digits
	if (midstart <= midend) {
		unsigned nleft = (unsigned)strlen(s1) - 1;
		if (!first) str_builder_append(b, "|");
		first = false;
		if (midstart == midend)
			str_builder_appendf(b, "%c", midstart);
		else
			str_builder_appendf(b, "[%c-%c]", midstart, midend);
		str_builder_append(b, "[0-9]");
		if (nleft > 1) {
			str_builder_appendf(b, "{%u}", nleft);
		}
	}
}

// absolutely crazy code to convert number range to regex
static void regex_append_number_range(StrBuilder *b, i64 num1, i64 num2) {
	if (num1 > num2) {
		// switcheroo
		regex_append_number_range(b, num2, num1);
		return;
	}
	if (num1 < 0 && num2 >= 0) {
		// split into just-positive and just-negative
		regex_append_number_range(b, num1, -1);
		str_builder_append(b, "|");
		regex_append_number_range(b, 0, num2);
		return;
	}
	if (num1 < 0 && num2 < 0) {
		// all negatives
		str_builder_append(b, "-");
		str_builder_append(b, "(");
		regex_append_number_range(b, -num2, -num1);
		str_builder_append(b, ")");
		return;
	}
	// hoo ray we don't need to deal with negatives anymore
	assert(num1 >= 0 && num2 >= 0);
	if (num2 > 999999999999999999) {
		// bull shit forget it
		str_builder_append(b, "//"); // will never match a valid path
		return;
	}
	char s1[24], s2[24];
	strbuf_printf(s1, "%" PRId64, num1);
	strbuf_printf(s2, "%" PRId64, num2);
	if (strlen(s1) != strlen(s2)) {
		// e.g. split 45-333 into 45-99 and 100-333
		assert(strlen(s1) < strlen(s2));
		for (size_t i = 0; s1[i]; i++)
			s1[i] = '9';
		i64 n = (i64)atoll(s1);
		regex_append_number_range(b, num1, n);
		str_builder_append(b, "|");
		regex_append_number_range(b, n + 1, num2);
		return;
	}
	regex_append_number_str_range(b, s1, s2);
}

static char *editorconfig_glob_to_regex(ConfigReader *reader, const char *glob) {
	StrBuilder builder = str_builder_new(), *b = &builder;
	
	{
		// add base path
		char dirname[4096];	
		strbuf_cpy(dirname, reader->filename);
		path_dirname(dirname);
		if (!dirname[0]) {
			assert(0);
			goto error;
		}
		if (!is_path_separator(dirname[strlen(dirname) - 1])) {
			strbuf_catf(dirname, "%c", PATH_SEPARATOR);
		}
		for (const char *p = dirname; *p; ++p)
			regex_append_literal_char(b, *p);
	}
	if (!strchr(glob, '/')) {
		// allow any bull shit directory before the glob
		str_builder_append(b, "(.*/)?");
	}

	int brace_level = 0;
	for (size_t i = 0; glob[i]; i++) {
		assert(brace_level >= 0);
		switch (glob[i]) {
		case '\\':
			if (glob[i+1]) {
				regex_append_literal_char(b, glob[i+1]);
				i += 1;
			} else {
				regex_append_literal_char(b, '\\');
			}
			break;
		case '*':
			if (glob[i+1] == '*') {
				// **
				str_builder_append(b, ".*");
			} else {
				// *
				str_builder_append(b, "[^/]*");
			}
			break;
		case '?':
			str_builder_append(b, "[^/]");
			break;
		case '/':
			if (str_has_prefix(&glob[i], "/**/")) {
				// allow just a single slash
				// (not in spec, but editorconfig library has this)
				str_builder_append(b, "/(.*/)?");
			} else {
				regex_append_literal_char(b, '/');
			}
			break;
		case '[':
			str_builder_append(b, "[");
			i += 1;
			if (glob[i] == '!') {
				str_builder_append(b, "^");
				i += 1;
			}
			for (; glob[i]; i++) {
				if (glob[i] == ']') break;
				if (glob[i] == '\\') {
					i += 1;
					if (!glob[i]) break;
				}
				regex_append_literal_char(b, glob[i]);
			}
			str_builder_append(b, "]");
			if (!glob[i]) {
				// we don't wanna show some error message if editorconfig glob spec changes
				config_debug_err(reader, "glob has [ with no matching ]");
				goto error;
			}
			break;
		case '{': {
			i64 num1 = 0, num2 = 0;
			int bytes = 0;
			if (sscanf(&glob[i], "{%" SCNd64 "..%" SCNd64 "}%n", &num1, &num2, &bytes) == 2
				&& bytes > 0) {
				i += (unsigned)bytes - 1;
				str_builder_append(b, "(");
				regex_append_number_range(b, num1, num2);
				str_builder_append(b, ")");
				break;
			}
			bool has_comma = false;
			size_t j;
			for (j = i; glob[j]; j++) {
				if (glob[j] == '}') break;
				if (glob[j] == ',')
					has_comma = true;
				if (glob[j] == '\\') {
					j += 1;
					if (!glob[j]) break;
				}
			}
			if (has_comma && glob[j]) {
				str_builder_append(b, "(");
				brace_level += 1;
			} else {
				regex_append_literal_char(b, '{');
			}
			} break;
		case ',':
			if (brace_level > 0) {
				str_builder_append(b, "|");
			} else {
				regex_append_literal_char(b, ',');
			}
			break;
		case '}':
			if (brace_level == 0) {
				regex_append_literal_char(b, '}');
			} else {
				str_builder_append(b, ")");
				brace_level -= 1;
			}
			break;
		default:
			regex_append_literal_char(b, glob[i]);
			break;
		}
	}
	if (brace_level) {
		config_debug_err(reader, "glob has { with no matching }");
		goto error;
	}
	// make sure end is anchored
	str_builder_append(b, "$");
	char *ret = str_dup(b->str);
	str_builder_free(&builder);
	return ret;
error:
	str_builder_free(&builder);
	return NULL;
}

static void config_read_editorconfig(Ted *ted, RcStr *path_rc) {
	const char *const path = rc_str(path_rc, "");
	FILE *fp = fopen(path, "r");
	if (!fp) return;
	ConfigReader reader_data = {
		.ted = ted,
		.filename = path,
		.line_number = 1,
		.fp = fp,
	};
	ConfigReader *const reader = &reader_data;
	Config *cfg = NULL;
	char line[4096];
	bool is_root = false;
	while (true) {
		char c = config_getc_no_err_on_eof(reader);
		if (!c) break;
		if (isspace(c)) continue;
		switch (c) {
		case '#':
		case ';':
			config_read_to_eol(reader, line, sizeof line);
			break;
		case '[':
			// new section
			config_read_to_eol(reader, line, sizeof line);
			str_trim(line);
			if (strlen(line) == 0 || line[strlen(line) - 1] != ']') {
				config_debug_err(reader, "Unmatched [");
				break;
			}
			line[strlen(line) - 1] = 0;
			cfg = arr_addp(ted->all_configs);
			cfg->source = rc_str_copy(path_rc);
			cfg->is_editorconfig_root = is_root;
			cfg->format = CONFIG_EDITORCONFIG;
			cfg->path_regex = editorconfig_glob_to_regex(reader, line);
			if (!cfg->path_regex) {
				// regex failed to compile
				cfg->path_regex = str_dup("//"); // never matches a valid path
			}
			config_compile_regex(cfg, reader);
			break;
		default: {
			char key[64] = {c};
			for (size_t i = 1; i < sizeof key - 1; i++) {
				c = config_getc(reader);
				if (!c || c == '\n') break;
				if (c == '=') break;
				key[i] = c;
			}
			if (c != '=') {
				config_err(reader, "expected key = value but didn't find =");
				break;
			}
			str_trim(key);
			str_ascii_to_lowercase(key);
			char value[64] = {0};
			for (size_t i = 0; i < sizeof value - 1; i++) {
				c = config_getc(reader);
				if (!c || c == '\n') break;
				value[i] = c;
			}
			str_trim(value);
			if (streq(key, "root")) {
				str_ascii_to_lowercase(value);
				if (cfg) {
					config_debug_err(reader, "root cannot be set outside of preamble");
					break;
				}
				if (streq(value, "true"))
					is_root = true;
				else
					is_root = false;
				break;
			}
			const int value_int = atoi(value);
			if (cfg && streq(key, "indent_style")) {
				str_ascii_to_lowercase(value);
				if (streq(value, "tab"))
					config_set_bool(cfg, &setting_indent_with_spaces, false);
				else if (streq(value, "space"))
					config_set_bool(cfg, &setting_indent_with_spaces, true);
			} else if (cfg && (streq(key, "indent_size") || streq(key, "tab_width")) && value_int > 0 && value_int < 255) {
				config_set_u8(cfg, &setting_tab_width, (u8)value_int);
			} else if (cfg && streq(key, "end_of_line")) {
				str_ascii_to_lowercase(value);
				if (streq(value, "lf")) {
					config_set_bool(cfg, &setting_crlf, false);
					config_set_bool(cfg, &setting_crlf_windows, false);
				} else if (streq(value, "crlf")) {
					config_set_bool(cfg, &setting_crlf, true);
				}
				// who the fuck uses CR line endings in the 21st century??
			} else if (cfg && streq(key, "insert_final_newline")) {
				str_ascii_to_lowercase(value);
				if (streq(value, "true")) {
					config_set_bool(cfg, &setting_auto_add_newline, true);
				} else if (streq(value, "false")) {
					config_set_bool(cfg, &setting_auto_add_newline, false);
				}
			} else if (cfg && streq(key, "trim_trailing_whitespace")) {
				str_ascii_to_lowercase(value);
				if (streq(value, "true")) {
					config_set_bool(cfg, &setting_remove_trailing_whitespace, true);
				} else if (streq(value, "false")) {
					config_set_bool(cfg, &setting_remove_trailing_whitespace, false);
				}
			}
			}
			break;
		}
	}
	fclose(fp);
}

void config_free_all(Ted *ted) {
	arr_foreach_ptr(ted->all_configs, Config, cfg) {
		config_free(cfg);
	}
	arr_clear(ted->all_configs);
	settings_free(&ted->default_settings);
}

void config_read(Ted *ted, const char *path, ConfigFormat format) {
	const char **include_stack = NULL;
	config_init_settings();
	// check if we've already read this
	arr_foreach_ptr(ted->all_configs, Config, c) {
		if (streq(rc_str(c->source, ""), path))
			return;
	}
	RcStr *source_rc = rc_str_new(path, -1);
	// add dummy config so even if `path` doesn't exist we don't try to read it again
	Config *dummy = arr_addp(ted->all_configs);
	dummy->source = rc_str_copy(source_rc);
	dummy->language = LANG_INVALID;
	switch (format) {
	case CONFIG_NONE:
		assert(0);
		break;
	case CONFIG_TED_CFG:
		config_read_ted_cfg(ted, source_rc, &include_stack);
		// force recompute default settings
		strcpy(ted->default_settings_cwd, "//");
		break;
	case CONFIG_EDITORCONFIG:
		config_read_editorconfig(ted, source_rc);
		break;
	}
	rc_str_decref(&source_rc);
}


static char *last_separator(char *path) {
	for (int i = (int)strlen(path) - 1; i >= 0; --i)
		if (is_path_separator(path[i]))
			return &path[i];
	return NULL;
}

char *settings_get_root_dir(const Settings *settings, const char *path) {
	char best_path[TED_PATH_MAX];
	*best_path = '\0';
	u32 best_path_score = 0;
	char pathbuf[TED_PATH_MAX];
	strbuf_cpy(pathbuf, path);
	
	while (1) {
		FsDirectoryEntry **entries = fs_list_directory(pathbuf);
		if (entries) { // note: this may actually be NULL on the first iteration if `path` is a file
			for (int e = 0; entries[e]; ++e) {
				const char *entry_name = entries[e]->name;
				const char *root_identifiers = rc_str(settings->root_identifiers, "");
				const char *ident_name = root_identifiers;
				while (*ident_name) {
					const char *separators = ", \t\n\r\v";
					size_t ident_len = strcspn(ident_name, separators);
					if (strlen(entry_name) == ident_len && strncmp(entry_name, ident_name, ident_len) == 0) {
						// we found an identifier!
						u32 score = U32_MAX - (u32)(ident_name - root_identifiers);
						if (score > best_path_score) {
							best_path_score = score;
							strbuf_cpy(best_path, pathbuf);
						}
					}
					ident_name += ident_len;
					ident_name += strspn(ident_name, separators);
				}
			}
			fs_dir_entries_free(entries);
		}
		
		char *p = last_separator(pathbuf);
		if (!p)
			break;
		*p = '\0';
		if (!last_separator(pathbuf))
			break; // we made it all the way to / (or c:\ or whatever)
	}
	
	if (*best_path) {
		return str_dup(best_path);
	} else {
		// didn't find any identifiers.
		// just return
		//  - `path` if it's a directory
		//  - the directory containing path if it's a file
		if (fs_path_type(path) == FS_DIRECTORY) {
			return str_dup(path);
		}
		strbuf_cpy(pathbuf, path);
		char *sep = last_separator(pathbuf);
		*sep = '\0';
		return str_dup(pathbuf);
	}
}

u32 settings_color(const Settings *settings, ColorSetting color) {
	if (color >= COLOR_COUNT) {
		assert(0);
		return 0xff00ffff;
	}
	return settings->colors[color];
}

void settings_color_floats(const Settings *settings, ColorSetting color, float f[4]) {
	color_u32_to_floats(settings_color(settings, color), f);
	
}

u16 settings_tab_width(const Settings *settings) {
	return settings->tab_width;
}

bool settings_indent_with_spaces(const Settings *settings) {
	return settings->indent_with_spaces;
}

bool settings_auto_indent(const Settings *settings) {
	return settings->auto_indent;
}

float settings_border_thickness(const Settings *settings) {
	return settings->border_thickness;
}

float settings_padding(const Settings *settings) {
	return settings->padding;
}

static void editorconfig_glob_test_expect(Ted *ted, const char *pattern, const char *path, bool result) {
	// fake config reader
	ConfigReader reader = {
		.ted = ted,
		.filename = "/test.editorconfig",
		.line_number = 1,
	}; 
	char *regex = editorconfig_glob_to_regex(&reader, pattern);
	int error_code = 0;
	PCRE2_SIZE error_offset = 0;
	pcre2_code_8 *code = pcre2_compile_8((const u8 *)regex, PCRE2_ZERO_TERMINATED, PCRE2_ANCHORED, &error_code, &error_offset, NULL);
	if (!code) {
		println("bad regex produced from editorconfig glob (error code %d at offset %u): %s",
			error_code, (unsigned)error_offset, regex);
		exit(1);
	}
	pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8(code, NULL);
	bool match = pcre2_match_8(code, (const u8 *)path, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL) > 0;
	pcre2_match_data_free_8(match_data);
	if (!match && result) {
		println("expected editorconfig glob \"%s\" to match \"%s\" but it didn't. regex was: %s",
			pattern, path, regex);
		exit(1);
	}
	if (match && !result) {
		println("expected editorconfig glob \"%s\" not to match \"%s\" but it did. regex was: %s",
			pattern, path, regex);
		exit(1);
	}
		
}

static void config_test_editorconfig_glob_to_regex(Ted *ted) {
	static const struct {
		const char *pattern;
		const char *path;
		bool result;
	} tests[] = {
		{"foo", "/foo", 1},
		{"foo", "/a/foo", 1},
		{"food", "/a/foo", 0},
		{"*.py", "/a/b/x.py", 1},
		{"a/*.py", "/a/x.py", 1},
		{"a/*.py", "/b/x.py", 0},
		{"a/*.py", "/a/b/x.py", 0},
		{"a/**.py", "/a/b/x.py", 1},
		{"[xyz]", "/y", 1},
		{"[xyz]", "/z", 1},
		{"[xyz]", "/a", 0},
		{"[!xyz]", "/y", 0},
		{"[!xyz]", "/z", 0},
		{"[!xyz]", "/a", 1},
		{"{x,y,z}", "/x", 1},
		{"{x,y,z}", "/a", 0},
		{"{foo,bar}", "/foo", 1},
		{"{foo,bar}", "/bar", 1},
		{"{foo,bar,xylum,plant species}", "/barricade", 0},
		{"{foo{,s,t}}", "/foo", 1},
		{"{foo{,s,t}}", "/foot", 1},
		{"{foo{,s,t}}", "/foos", 1},
		{"{foo{,s,t}}", "/foob", 0},
		{",", "/,", 1},
		{",", "/\\,", 0},
		{"\\{", "/{", 1},
		{"\\{", "/\\{", 0},
		{"[\\[]", "/[", 1},
		{"[\\[]", "/]", 0},
		{"[\\]]", "/[", 0},
		{"[\\]]", "/]", 1},
		{"[!\\[]", "/[", 0},
		{"[!\\[]", "/]", 1},
		{"[!\\]]", "/[", 1},
		{"[!\\]]", "/]", 0},
		{"\\[\\]", "/[]", 1},
		{"\\[\\]", "/.gitignore", 0},
		{"{1..100}", "/00", 0},
		{"{1..100}", "/11", 1},
		{"{1..100}", "/1", 1},
		{"{1..100}", "/100", 1},
		{"{1..100}", "/90", 1},
		{"{1..100}", "/35", 1},
		{"{1..100}", "/7", 1},
		{"{1..100}", "/101", 0},
		{"{-325..-320}", "/-323", 1},
		{"{-325..-320}", "/-325", 1},
		{"{-325..-320}", "/-320", 1},
		{"{-325..-320}", "/-300", 0},
		{"{0..99999999}", "/34345", 1},
		{"{0..99999999}", "/0", 1},
		{"{0..99999999}", "/balls", 0},
		{"{0..99999999}", "/99999999999", 0},
		{"{0..0}", "/0", 1},
		{"x{625..629}", "/x627", 1},
		{"x{625..629}", "/x637", 0},
		{"{..}", "/{..}", 1},
		{"{a\\,b}", "/{a,b}", 1},
		{"\\{a,b}", "/{a,b}", 1},
	};
	for (size_t i = 0; i < arr_count(tests); i++) {
		editorconfig_glob_test_expect(ted, tests[i].pattern, tests[i].path, tests[i].result);
	}
}

void config_test(Ted *ted) {
	config_test_editorconfig_glob_to_regex(ted);
}
