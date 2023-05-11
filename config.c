// Read a ted configuration file.
// Config files are formatted as several sections, each containing key = value pairs.
// e.g.:
// [section1]
// thing1 = 33
// thing2 = 454
// [section2]
// asdf = 123

#include "ted.h"

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
	const char *control;
	size_t buf_size;
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
	{"auto-add-newline", &settings_zero.auto_add_newline, true},
	{"auto-reload", &settings_zero.auto_reload, true},
	{"auto-reload-config", &settings_zero.auto_reload_config, false},
	{"syntax-highlighting", &settings_zero.syntax_highlighting, true},
	{"line-numbers", &settings_zero.line_numbers, true},
	{"restore-session", &settings_zero.restore_session, false},
	{"regenerate-tags-if-not-found", &settings_zero.regenerate_tags_if_not_found, true},
	{"indent-with-spaces", &settings_zero.indent_with_spaces, true},
	{"trigger-characters", &settings_zero.trigger_characters, true},
	{"identifier-trigger-characters", &settings_zero.identifier_trigger_characters, true},
	{"phantom-completions", &settings_zero.phantom_completions, true},
	{"signature-help-enabled", &settings_zero.signature_help_enabled, true},
	{"lsp-enabled", &settings_zero.lsp_enabled, true},
	{"lsp-log", &settings_zero.lsp_log, true},
	{"hover-enabled", &settings_zero.hover_enabled, true},
	{"vsync", &settings_zero.vsync, false},
	{"highlight-enabled", &settings_zero.highlight_enabled, true},
	{"highlight-auto", &settings_zero.highlight_auto, true},
	{"save-backup", &settings_zero.save_backup, true},
	{"crlf-windows", &settings_zero.crlf_windows, true},
};
static const SettingU8 settings_u8[] = {
	{"tab-width", &settings_zero.tab_width, 1, 100, true},
	{"cursor-width", &settings_zero.cursor_width, 1, 100, true},
	{"undo-save-time", &settings_zero.undo_save_time, 1, 200, true},
	{"border-thickness", &settings_zero.border_thickness, 1, 30, false},
	{"padding", &settings_zero.padding, 0, 100, false},
	{"scrolloff", &settings_zero.scrolloff, 1, 100, true},
	{"tags-max-depth", &settings_zero.tags_max_depth, 1, 100, false},
};
static const SettingU16 settings_u16[] = {
	{"text-size", &settings_zero.text_size, TEXT_SIZE_MIN, TEXT_SIZE_MAX, false},
	{"max-menu-width", &settings_zero.max_menu_width, 10, U16_MAX, false},
	{"error-display-time", &settings_zero.error_display_time, 0, U16_MAX, false},
	{"framerate-cap", &settings_zero.framerate_cap, 3, 1000, false},
};
static const SettingU32 settings_u32[] = {
	{"max-file-size", &settings_zero.max_file_size, 100, 2000000000, false},
	{"max-file-size-view-only", &settings_zero.max_file_size_view_only, 100, 2000000000, false},
};
static const SettingFloat settings_float[] = {
	{"cursor-blink-time-on", &settings_zero.cursor_blink_time_on, 0, 1000, true},
	{"cursor-blink-time-off", &settings_zero.cursor_blink_time_off, 0, 1000, true},
	{"hover-time", &settings_zero.hover_time, 0, INFINITY, true},
	{"ctrl-scroll-adjust-text-size", &settings_zero.ctrl_scroll_adjust_text_size, -10, 10, true},
};
static const SettingString settings_string[] = {
	{"build-default-command", settings_zero.build_default_command, sizeof settings_zero.build_default_command, true},
	{"build-command", settings_zero.build_command, sizeof settings_zero.build_command, true},
	{"root-identifiers", settings_zero.root_identifiers, sizeof settings_zero.root_identifiers, true},
	{"lsp", settings_zero.lsp, sizeof settings_zero.lsp, true},
	{"lsp-configuration", settings_zero.lsp_configuration, sizeof settings_zero.lsp_configuration, true},
	{"comment-start", settings_zero.comment_start, sizeof settings_zero.comment_start, true},
	{"comment-end", settings_zero.comment_end, sizeof settings_zero.comment_end, true},
};
static const SettingKeyCombo settings_key_combo[] = {
	{"hover-key", &settings_zero.hover_key, true},
	{"highlight-key", &settings_zero.highlight_key, true},
};


static void setting_bool_set(Settings *settings, const SettingBool *set, bool value) {
	*(bool *)((char *)settings + ((char*)set->control - (char*)&settings_zero)) = value;
}
static void setting_u8_set(Settings *settings, const SettingU8 *set, u8 value) {
	if (value >= set->min && value <= set->max)
		*(u8 *)((char *)settings + ((char*)set->control - (char*)&settings_zero)) = value;
}
static void setting_u16_set(Settings *settings, const SettingU16 *set, u16 value) {
	if (value >= set->min && value <= set->max)
		*(u16 *)((char *)settings + ((char*)set->control - (char*)&settings_zero)) = value;
}
static void setting_u32_set(Settings *settings, const SettingU32 *set, u32 value) {
	if (value >= set->min && value <= set->max)
		*(u32 *)((char *)settings + ((char*)set->control - (char*)&settings_zero)) = value;
}
static void setting_float_set(Settings *settings, const SettingFloat *set, float value) {
	if (value >= set->min && value <= set->max)
		*(float *)((char *)settings + ((char*)set->control - (char*)&settings_zero)) = value;
}
static void setting_string_set(Settings *settings, const SettingString *set, const char *value) {
	char *control = (char *)settings + (set->control - (char*)&settings_zero);
	str_cpy(control, set->buf_size, value);
}
static void setting_key_combo_set(Settings *settings, const SettingKeyCombo *set, KeyCombo value) {
	KeyCombo *control = (KeyCombo *)((char *)settings + ((char*)set->control - (char*)&settings_zero));
	*control = value;
}


typedef struct {
	Ted *ted;
	const char *filename;
	u32 line_number; // currently processing this line number
	bool error;
} ConfigReader;

static void config_err(ConfigReader *cfg, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
static void config_err(ConfigReader *cfg, const char *fmt, ...) {
	if (cfg->error) return;
	cfg->error = true;
	char error[1024] = {0};
	strbuf_printf(error, "%s:%u: ", cfg->filename, cfg->line_number);
	va_list args;
	va_start(args, fmt);
	vsnprintf(error + strlen(error), sizeof error - strlen(error) - 1, fmt, args);
	va_end(args);
	ted_error(cfg->ted, "%s", error);
}

static void context_copy(SettingsContext *dest, const SettingsContext *src) {
	*dest = *src;
	if (src->path)
		dest->path = str_dup(src->path);
}

long context_score(const char *path, Language lang, const SettingsContext *context) {
	long score = 0;
	
	// currently contexts are ranked by:
	//   1. path matching, the more specific the better
	//   2. language
	
	if (context->language) {
		if (lang == context->language) {
			score += 1;
		} else {
			// dont use this. it's language-specific and for the wrong language.
			return INT_MIN;
		}
	}
	
	if (context->path) {
		if (path && str_has_path_prefix(path, context->path)) {
			score += 2 * (long)strlen(context->path);
		} else {
			// dont use this. it's path-specific and for the wrong path.
			return INT_MIN;
		}
	}
	
	return score;
}

/* does being in the context of `parent` imply you are in the context of `child`? */
static bool context_is_parent(const SettingsContext *parent, const SettingsContext *child) {
	if (child->language == 0 && parent->language != 0)
		return false;
	if (parent->language != 0 && child->language != 0 && parent->language != child->language)
		return false;
	if (parent->path) {
		if (!child->path)
			return false;
		if (!str_has_prefix(child->path, parent->path))
			return false;
	}
	return true;
}

static void settings_copy(Settings *dest, const Settings *src) {
	*dest = *src;
	
	gl_rc_sab_incref(dest->bg_shader);
	gl_rc_texture_incref(dest->bg_texture);
	
	context_copy(&dest->context, &src->context);
	dest->language_extensions = arr_copy(src->language_extensions);
	dest->key_actions = arr_copy(src->key_actions);
}

static void context_free(SettingsContext *ctx) {
	free(ctx->path);
	memset(ctx, 0, sizeof *ctx);
}

static void config_part_free(ConfigPart *part) {
	context_free(&part->context);
	arr_clear(part->text);
	free(part->file);
	memset(part, 0, sizeof *part);
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


static void parse_section_header(ConfigReader *cfg, char *line, ConfigPart *part) {
	#define SECTION_HEADER_HELP "Section headers should look like this: [(path//)(language.)section-name]"
	Ted *ted = cfg->ted;
	char *closing = strchr(line, ']');
	if (!closing) {
		config_err(cfg, "Unmatched [. " SECTION_HEADER_HELP);
		return;
	} else if (closing[1] != '\0') {
		config_err(cfg, "Text after section. " SECTION_HEADER_HELP);
		return;
	} else {
		*closing = '\0';
		char *section = line + 1;
		char *path_end = strstr(section, "//");
		if (path_end) {
			size_t path_len = (size_t)(path_end - section);
			char path[TED_PATH_MAX];
			path[0] = '\0';
			
			// expand ~
			if (section[0] == '~') {
				str_cpy(path, sizeof path, ted->home);
				++section;
				--path_len;
			}
			strn_cat(path, sizeof path, section, path_len);
			#if _WIN32
			// replace forward slashes with backslashes
			for (char *p = path; *p; ++p)
				if (*p == '/')
					*p = '\\';
			#endif
			part->context.path = str_dup(path);
			section = path_end + 2;
		}
		
		char *dot = strchr(section, '.');
		
		if (dot) {
			*dot = '\0';
			Language language = part->context.language = language_from_str(section);
			if (!language) {
				config_err(cfg, "Unrecognized language: %s.", section);
				return;
			}
			section = dot + 1;
		}
		
		if (streq(section, "keyboard")) {
			part->section = SECTION_KEYBOARD;
		} else if (streq(section, "colors")) {
			part->section = SECTION_COLORS;
		} else if (streq(section, "core")) {
			part->section = SECTION_CORE;
		} else if (streq(section, "extensions")) {
			if (part->context.language != 0 || part->context.path) {
				config_err(cfg, "Extensions section cannot be language- or path-specific.");
				return;
			}
			part->section = SECTION_EXTENSIONS;
		} else {
			config_err(cfg, "Unrecognized section: [%s].", section);
			return;
		}
	}
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
	if (path[0] == '~' && strchr(ALL_PATH_SEPARATORS, path[1])) {
		str_printf(expanded, expanded_sz, "%s" PATH_SEPARATOR_STR "%s", ted->home, path + 1);
	} else if (!path_is_absolute(path)) {
		if (!ted_get_file(ted, path, expanded, expanded_sz)) {
			str_cpy(expanded, expanded_sz, path);
		}
	} else {
		str_cpy(expanded, expanded_sz, path);
	}
	
}

static void config_read_(Ted *ted, ConfigPart **parts, const char *path, const char ***include_stack) {
	// check for, e.g. %include ted.cfg inside ted.cfg
	arr_foreach_ptr(*include_stack, const char *, p_include) {
		if (streq(path, *p_include)) {
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
			strbuf_catf(text, " includes %s", path);
			ted_error(ted, "%s", text);
			return;
		}
	}
	arr_add(*include_stack, path);
	
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		ted_error(ted, "Couldn't open config file %s: %s.", path, strerror(errno));
		return;
	}
	
	
	ConfigReader cfg_reader = {
		.ted = ted,
		.filename = path,
		.line_number = 1,
		.error = false
	};
	ConfigReader *cfg = &cfg_reader;
	
	ConfigPart *part = NULL;
	
	char line[4096] = {0};
	while (fgets(line, sizeof line, fp)) {
		char *newline = strchr(line, '\n');
		if (!newline && !feof(fp)) {
			config_err(cfg, "Line is too long.");
			break;
		}
		
		if (newline) *newline = '\0';
		char *carriage_return = strchr(line, '\r');
		if (carriage_return) *carriage_return = '\0';
		
		if (line[0] == '[') {
			// a new part!
			part = arr_addp(*parts);
			part->index = (int)arr_len(*parts);
			part->file = str_dup(path);
			part->line = cfg->line_number + 1;
			parse_section_header(&cfg_reader, line, part);
		} else if (line[0] == '%') {
			if (str_has_prefix(line, "%include ")) {
				char included[TED_PATH_MAX];
				char expanded[TED_PATH_MAX];
				strbuf_cpy(included, line + strlen("%include "));
				while (*included && isspace(included[strlen(included) - 1]))
					included[strlen(included) - 1] = '\0';
				get_config_path(ted, expanded, sizeof expanded, included);
				config_read_(ted, parts, expanded, include_stack);
			}
		} else if (part) {
			for (int i = 0; line[i]; ++i) {
				arr_add(part->text, line[i]);
			}
			arr_add(part->text, '\n');
		} else {
			const char *p = line;
			while (isspace(*p)) ++p;
			if (*p == '\0' || *p == '#') {
				// blank line
			} else {
				config_err(cfg, "Config has text before first section header.");
			}
		}
		++cfg->line_number;
	}
	
	if (ferror(fp))
		ted_error(ted, "Error reading %s.", path);
	fclose(fp);
	arr_remove_last(*include_stack);
}

void config_read(Ted *ted, ConfigPart **parts, const char *filename) {
	const char **include_stack = NULL;
	config_read_(ted, parts, filename, &include_stack);
}

// IMPORTANT REQUIREMENT FOR THIS FUNCTION:
//     - less specific contexts compare as less
//            (i.e. if context_is_parent(a.context, b.context), then we return -1, and vice versa.)
//     - this gives a total ordering; ties are broken by order of appearance
static int config_part_cmp(const ConfigPart *ap, const ConfigPart *bp) {
	const SettingsContext *a = &ap->context, *b = &bp->context;
	if (a->language == 0 && b->language != 0)
		return -1;
	if (a->language != 0 && b->language == 0)
		return +1;
	const char *a_path = a->path ? a->path : "";
	const char *b_path = b->path ? b->path : "";
	size_t a_path_len = strlen(a_path), b_path_len = strlen(b_path);
	if (a_path_len < b_path_len)
		return -1;
	if (a_path_len > b_path_len)
		return 1;
	
	// done with specificity, now on to identicalness
	if (a->language < b->language)
		return -1;
	if (a->language > b->language)
		return +1;
	int cmp = strcmp(a_path, b_path);
	if (cmp != 0) return cmp;
	if (ap->index < bp->index)
		return -1;
	if (ap->index > bp->index)
		return +1;
	return 0;
	
}

static int config_part_qsort_cmp(const void *av, const void *bv) {
	return config_part_cmp(av, bv);
}

static char *config_read_string(Ted *ted, ConfigReader *cfg, char **ptext) {
	char *p;
	int backslashes = 0;
	u32 start_line = cfg->line_number;
	char delimiter = **ptext;
	char *start = *ptext + 1;
	char *str = NULL;
	for (p = start; ; ++p) {
		switch (*p) {
		case '\\':
			++backslashes;
			++p;
			switch (*p) {
			case '\\':
			case '"':
			case '`':
				break;
			case 'n':
				arr_add(str, '\n');
				continue;
			case 't':
				arr_add(str, '\t');
				continue;
			case '[':
				arr_add(str, '[');
				continue;
			case '\0':
				goto null;
			default:
				config_err(cfg, "Unrecognized escape sequence: '\\%c'.", *p);
				*ptext += strlen(*ptext);
				arr_clear(str);
				return NULL;
			}
			break;
		case '\n':
			++cfg->line_number;
			break;
		case '\0':
		null:
			cfg->line_number = start_line;
			config_err(cfg, "String doesn't end.");
			*ptext += strlen(*ptext);
			arr_clear(str);
			return NULL;
		}
		if (*p == delimiter)
			break;
		arr_add(str, *p);
	}
	
	char *s = NULL;
	if (ted->nstrings < TED_MAX_STRINGS) {
		s = strn_dup(str, arr_len(str));
		ted->strings[ted->nstrings++] = s;
	}
	arr_clear(str);
	*ptext = p + 1;
	return s;
}

static void settings_load_bg_shader(Ted *ted, Settings **applicable_settings, const char *bg_shader_text) {
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
	
	GlRcSAB *bg_shader = gl_rc_sab_new(shader, array, buffer);
	bg_shader->ref_count = arr_len(applicable_settings);
	arr_foreach_ptr(applicable_settings, Settings *, psettings) {
		Settings *settings = *psettings;
		// decrease refcount on previous shader
		gl_rc_sab_decref(&settings->bg_shader);
		settings->bg_shader = bg_shader;
	}
}


static void settings_load_bg_texture(Ted *ted, Settings **applicable_settings, const char *path) {
	char expanded[TED_PATH_MAX];
	get_config_path(ted, expanded, sizeof expanded, path);
	
	GLuint texture = gl_load_texture_from_image(expanded);
	if (!texture) {
		ted_error(ted, "Couldn't load image %s", path);
		return;
	}
	
	GlRcTexture *bg_texture = gl_rc_texture_new(texture);
	bg_texture->ref_count = arr_len(applicable_settings);
	arr_foreach_ptr(applicable_settings, Settings *, psettings) {
		Settings *settings = *psettings;
		// decrease refcount on previous texture
		gl_rc_texture_decref(&settings->bg_texture);
		settings->bg_texture = bg_texture;
	}
	
}

// reads a single "line" of the config file, but it may include a multiline string,
// so it may read multiple lines.
// applicable_settings is a dynamic array of all settings objects to update
static void config_parse_line(ConfigReader *cfg, Settings **applicable_settings, const ConfigPart *part, char **pline) {
	char *line = *pline;
	Ted *ted = cfg->ted;
	
	char *newline = strchr(line, '\n');
	if (!newline) {
		config_err(cfg, "No newline at end of file?");
		*pline += strlen(*pline);
		return;
	}
	
	if (newline) *newline = '\0';
	char *carriage_return = strchr(line, '\r');
	if (carriage_return) *carriage_return = '\0';
	*pline = newline + 1;
	
	if (part->section == 0) {
		// there was an error reading this section. don't bother with anything else.
		return;
	}
	
	switch (line[0]) {
	case '#': // comment
	case '\0': // blank line
		return;
	}
	
	char *equals = strchr(line, '=');
	if (!equals) {
		config_err(cfg, "Invalid line syntax. "
			"Lines should either look like [section-name] or key = value");
		return;
	}

	char *key = line;
	*equals = '\0';
	char *value = equals + 1;
	while (isspace(*key)) ++key;
	while (isspace(*value)) ++value;
	if (equals != line) {
		for (char *p = equals - 1; p > line; --p) {
			// remove trailing spaces after key
			if (isspace(*p)) *p = '\0';
			else break;
		}
	}
	if (key[0] == '\0') {
		config_err(cfg, "Empty property name. This line should look like: key = value");
		return;
	}
	
	switch (part->section) {
	case SECTION_NONE:
		config_err(cfg, "Line outside of any section."
			"Try putting a section header, e.g. [keyboard] before this line?");
		break;
	case SECTION_COLORS: {
		ColorSetting setting = color_setting_from_str(key);
		if (setting != COLOR_UNKNOWN) {
			u32 color = 0;
			if (color_from_str(value, &color)) {
				arr_foreach_ptr(applicable_settings, Settings *, psettings) {
					(*psettings)->colors[setting] = color;
				}
			} else {
				config_err(cfg, "'%s' is not a valid color. Colors should look like #rgb, #rgba, #rrggbb, or #rrggbbaa.", value);
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
		KeyCombo key_combo = config_parse_key_combo(cfg, key);
		KeyAction action = {0};
		action.key_combo = key_combo;
		CommandArgument argument = {
			.number = 1, // default argument = 1
			.string = NULL
		};
		if (isdigit(*value)) {
			// read the argument
			char *endp;
			argument.number = strtoll(value, &endp, 10);
			value = endp;
		} else if (*value == '"' || *value == '`') {
			// string argument
			
			// restore newline to handle multi-line strings
			// a little bit hacky oh well
			*newline = '\n';
			argument.string = config_read_string(ted, cfg, &value);
			
			newline = strchr(value, '\n');
			if (!newline) {
				config_err(cfg, "No newline at end of file?");
				*pline += strlen(*pline);
				return;
			}
			*newline = '\0';
			*pline = newline + 1;
		}
		while (isspace(*value)) ++value; // skip past space following argument
		if (*value == ':') {
			// read the command
			Command command = command_from_str(value + 1);
			if (command != CMD_UNKNOWN) {
				action.command = command;
				action.argument = argument;
			} else {
				config_err(cfg, "Unrecognized command %s", value);
			}
		} else {
			config_err(cfg, "Expected ':' for key action. This line should look something like: %s = :command.", key);
		}
		
		arr_foreach_ptr(applicable_settings, Settings *, psettings) {
			Settings *settings = *psettings;
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
		}
	} break;
	case SECTION_EXTENSIONS: {
		Language lang = language_from_str(key);
		if (lang == LANG_NONE) {
			config_err(cfg, "Invalid programming language: %s.", key);
		} else {
			char *exts = calloc(1, strlen(value) + 1);
			char *dst = exts;
			// get rid of whitespace in extension list
			for (const char *src = value; *src; ++src)
				if (!isspace(*src))
					*dst++ = *src;
			*dst = 0;
			arr_foreach_ptr(applicable_settings, Settings *, psettings) {
				Settings *settings = *psettings;
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
			free(exts);
		}
	} break;
	case SECTION_CORE: {
		const char *endptr;
		long long const integer = strtoll(value, (char **)&endptr, 10);
		bool const is_integer = *endptr == '\0';
		double const floating = strtod(value, (char **)&endptr);
		bool const is_floating = *endptr == '\0';
		bool is_bool = false;
		bool boolean = false;
	#define BOOL_HELP "(should be yes/no/on/off/true/false)"
		if (streq(value, "yes") || streq(value, "on") || streq(value, "true")) {
			is_bool = true;
			boolean = true;
		} else if (streq(value, "no") || streq(value, "off") || streq(value, "false")) {
			is_bool = true;
			boolean = false;
		}
		
		if (value[0] == '"' || value[0] == '`') {
			// restore newline to handle multi-line strings
			// a little bit hacky oh well
			*newline = '\n';
			
			char *string = config_read_string(ted, cfg, &value);
			
			newline = strchr(value, '\n');
			if (!newline) {
				config_err(cfg, "No newline at end of file?");
				*pline += strlen(*pline);
				return;
			}
			*newline = '\0';
			*pline = newline + 1;
			if (string)
				value = string;
		}

		
		SettingAny const *setting_any = NULL;
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
				settings_load_bg_shader(ted, applicable_settings, value);
			else if (streq(key, "bg-texture"))
				settings_load_bg_texture(ted, applicable_settings, value);
			// it's probably a bad idea to error on unrecognized settings
			// because if we ever remove a setting in the future
			// everyone will get errors
			break;
		}
		
		arr_foreach_ptr(applicable_settings, Settings *, psettings) {
			Settings *settings = *psettings;
			if (part->context.language != 0 && !setting_any->per_language) {
				config_err(cfg, "Setting %s cannot be controlled for individual languages.", key);
				break;
			}
			
			switch (setting_any->type) {
			case SETTING_BOOL: {
				const SettingBool *setting = &setting_any->u._bool;
				if (is_bool)
					setting_bool_set(settings, setting, boolean);
				else
					config_err(cfg, "Invalid %s: %s. This should be yes, no, on, or off.", setting->name, value);
			} break;
			case SETTING_U8: {
				const SettingU8 *setting = &setting_any->u._u8;
				if (is_integer && integer >= setting->min && integer <= setting->max)
					setting_u8_set(settings, setting, (u8)integer);
				else
					config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
			} break;
			case SETTING_U16: {
				const SettingU16 *setting = &setting_any->u._u16;
				if (is_integer && integer >= setting->min && integer <= setting->max)
					setting_u16_set(settings, setting, (u16)integer);
				else
					config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
			} break;
			case SETTING_U32: {
				const SettingU32 *setting = &setting_any->u._u32;
				if (is_integer && integer >= setting->min && integer <= setting->max)
					setting_u32_set(settings, setting, (u32)integer);
				else
					config_err(cfg, "Invalid %s: %s. This should be an integer from %" PRIu32 " to %" PRIu32 ".",
						setting->name, value, setting->min, setting->max);
			} break;
			case SETTING_FLOAT: {
				const SettingFloat *setting = &setting_any->u._float;
				if (is_floating && floating >= setting->min && floating <= setting->max)
					setting_float_set(settings, setting, (float)floating);
				else
					config_err(cfg, "Invalid %s: %s. This should be a number from %g to %g.", setting->name, value, setting->min, setting->max);
			} break;
			case SETTING_STRING: {
				const SettingString *setting = &setting_any->u._string;
				if (strlen(value) >= setting->buf_size) {
					config_err(cfg, "%s is too long (length: %zu, maximum length: %zu).", key, strlen(value), setting->buf_size - 1);
				} else {
					setting_string_set(settings, setting, value);
				}
			} break;
			case SETTING_KEY_COMBO: {
				const SettingKeyCombo *setting = &setting_any->u._key;
				KeyCombo combo = config_parse_key_combo(cfg, value);
				if (combo.value) {
					setting_key_combo_set(settings, setting, combo);
				}
			} break;
			}
		}
		
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

void config_parse(Ted *ted, ConfigPart **pparts) {
	config_init_settings();
	
	ConfigReader cfg_reader = {
		.ted = ted,
		.filename = NULL,
		.line_number = 1,
		.error = false
	};
	ConfigReader *cfg = &cfg_reader;
	
	
	ConfigPart *const parts = *pparts;
	qsort(parts, arr_len(parts), sizeof *parts, config_part_qsort_cmp);
	
	const char **paths = NULL;
	Language *languages = NULL;
	arr_add(languages, 0);
	// find all paths and languages referenced in config files
	arr_foreach_ptr(parts, ConfigPart, part) {
		bool already_have = false;
		if (part->context.path) {
			for (u32 i = 0; i < arr_len(paths); ++i) {
				if (paths_eq(paths[i], part->context.path)) {
					already_have = true;
					break;
				}
			}
			if (!already_have)
				arr_add(paths, part->context.path);
		}
		already_have = false;
		for (u32 i = 0; i < arr_len(languages); ++i) {
			if (languages[i] == part->context.language) {
				already_have = true;
				break;
			}
		}
		if (!already_have)
			arr_add(languages, part->context.language);
	}
	arr_foreach_ptr(languages, Language, lang) {
		// pathless settings
		{
			Settings *settings = arr_addp(ted->all_settings);
			settings->context.language = *lang;
		}
		
		arr_foreach_ptr(paths, const char *, path) {
			Settings *settings = arr_addp(ted->all_settings);
			settings->context.language = *lang;
			settings->context.path = str_dup(*path);
		}
	}
	arr_free(paths);
	arr_free(languages);
	
	arr_foreach_ptr(parts, ConfigPart, part) {
		cfg->filename = part->file;
		cfg->line_number = part->line;
		arr_add(part->text, '\0'); // null termination
		char *line = part->text;
		while (*line) {
			Settings **applicable_settings = NULL;
			arr_foreach_ptr(ted->all_settings, Settings, settings) {
				if (context_is_parent(&part->context, &settings->context)) {
					arr_add(applicable_settings, settings);
				}
			}
			config_parse_line(cfg, applicable_settings, part, &line);
			arr_free(applicable_settings);
			
			if (cfg->error) break;
	
			++cfg->line_number;
		}
		
		
	}
	
	arr_foreach_ptr(ted->all_settings, Settings, s) {
		SettingsContext *ctx = &s->context;
		if (ctx->language == 0 && (!ctx->path || !*ctx->path)) {
			ted->default_settings = s;
			break;
		}
	}
	
	arr_foreach_ptr(parts, ConfigPart, part) {
		config_part_free(part);
	}
	
	arr_clear(*pparts);
	
	arr_foreach_ptr(ted->all_settings, Settings, s) {
		// sort key_actions by key_combo.
		arr_qsort(s->key_actions, key_action_qsort_cmp_combo);
	}
}

static int gluint_cmp(const void *av, const void *bv) {
	const GLuint *ap = av, *bp = bv;
	GLuint a = *ap, b = *bp;
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

static void gluint_eliminate_duplicates(GLuint **arr) {
	arr_qsort(*arr, gluint_cmp);
	
	GLuint *start = *arr;
	GLuint *end = *arr + arr_len(*arr);
	GLuint *out = start;
	const GLuint *in = start;
	
	while (in < end) {
		if (in == start || in[0] != in[-1])
			*out++ = *in;
		++in;
	}
	size_t count = (size_t)(out - *arr);
	arr_set_len(*arr, count);
}

void config_free(Ted *ted) {
	arr_foreach_ptr(ted->all_settings, Settings, settings) {
		context_free(&settings->context);
		arr_free(settings->language_extensions);
		gl_rc_sab_decref(&settings->bg_shader);
		gl_rc_texture_decref(&settings->bg_texture);
		arr_free(settings->key_actions);
	}
	
	
	arr_clear(ted->all_settings);
	
	for (u32 i = 0; i < ted->nstrings; ++i) {
		free(ted->strings[i]);
		ted->strings[i] = NULL;
	}
	ted->nstrings = 0;
	ted->default_settings = NULL;
}


static char *last_separator(char *path) {
	for (int i = (int)strlen(path) - 1; i >= 0; --i)
		if (strchr(ALL_PATH_SEPARATORS, path[i]))
			return &path[i];
	return NULL;
}

char *settings_get_root_dir(Settings *settings, const char *path) {
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
				const char *ident_name = settings->root_identifiers;
				while (*ident_name) {
					const char *separators = ", \t\n\r\v";
					size_t ident_len = strcspn(ident_name, separators);
					if (strlen(entry_name) == ident_len && strncmp(entry_name, ident_name, ident_len) == 0) {
						// we found an identifier!
						u32 score = U32_MAX - (u32)(ident_name - settings->root_identifiers);
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
