// Read a configuration file.
// Config files are formatted as several sections, each containing key = value pairs.
// e.g.:
// [section1]
// thing1 = 33
// thing2 = 454
// [section2]
// asdf = 123

// all the "control" pointers here are relative to a NULL Settings object.
typedef struct {
	char const *name;
	const bool *control;
	bool per_language; // allow per-language control
} SettingBool;
typedef struct {
	char const *name;
	const u8 *control;
	u8 min, max;
	bool per_language;
} SettingU8;
typedef struct {
	char const *name;
	const float *control;
	float min, max;
	bool per_language;
} SettingFloat;
typedef struct {
	char const *name;
	const u16 *control;
	u16 min, max;
	bool per_language;
} SettingU16;
typedef struct {
	char const *name;
	const u32 *control;
	u32 min, max;
	bool per_language;
} SettingU32;
typedef struct {
	char const *name;
	const char *control;
	size_t buf_size;
	bool per_language;
} SettingString;

typedef enum {
	SETTING_BOOL = 1,
	SETTING_U8,
	SETTING_U16,
	SETTING_U32,
	SETTING_FLOAT,
	SETTING_STRING
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
	} u;
} SettingAny;

// core settings
static Settings const settings_zero = {0};
static SettingBool const settings_bool[] = {
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
	{"signature-help-enabled", &settings_zero.signature_help_enabled, true},
	{"lsp-enabled", &settings_zero.lsp_enabled, true},
	{"hover-enabled", &settings_zero.hover_enabled, true},
	{"vsync", &settings_zero.vsync, false},
	{"highlight-enabled", &settings_zero.highlight_enabled, true},
	{"highlight-auto", &settings_zero.highlight_auto, true},
};
static SettingU8 const settings_u8[] = {
	{"tab-width", &settings_zero.tab_width, 1, 100, true},
	{"cursor-width", &settings_zero.cursor_width, 1, 100, true},
	{"undo-save-time", &settings_zero.undo_save_time, 1, 200, true},
	{"border-thickness", &settings_zero.border_thickness, 1, 30, false},
	{"padding", &settings_zero.padding, 0, 100, false},
	{"scrolloff", &settings_zero.scrolloff, 1, 100, true},
	{"tags-max-depth", &settings_zero.tags_max_depth, 1, 100, false},
};
static SettingU16 const settings_u16[] = {
	{"text-size", &settings_zero.text_size, TEXT_SIZE_MIN, TEXT_SIZE_MAX, false},
	{"max-menu-width", &settings_zero.max_menu_width, 10, U16_MAX, false},
	{"error-display-time", &settings_zero.error_display_time, 0, U16_MAX, false},
	{"framerate-cap", &settings_zero.framerate_cap, 3, 1000, false},
};
static SettingU32 const settings_u32[] = {
	{"max-file-size", &settings_zero.max_file_size, 100, 2000000000, false},
	{"max-file-size-view-only", &settings_zero.max_file_size_view_only, 100, 2000000000, false},
};
static SettingFloat const settings_float[] = {
	{"cursor-blink-time-on", &settings_zero.cursor_blink_time_on, 0, 1000, true},
	{"cursor-blink-time-off", &settings_zero.cursor_blink_time_off, 0, 1000, true},
	{"hover-time", &settings_zero.hover_time, 0, INFINITY, true},
};
static SettingString const settings_string[] = {
	{"build-default-command", settings_zero.build_default_command, sizeof settings_zero.build_default_command, true},
	{"bg-shader", settings_zero.bg_shader_text, sizeof settings_zero.bg_shader_text, true},
	{"bg-texture", settings_zero.bg_shader_image, sizeof settings_zero.bg_shader_image, true},
	{"root-identifiers", settings_zero.root_identifiers, sizeof settings_zero.root_identifiers, true},
	{"lsp", settings_zero.lsp, sizeof settings_zero.lsp, true},
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





// all worth it for the -Wformat warnings
#define config_err(cfg, ...) do {\
	if ((cfg)->error) break;\
	 snprintf((cfg)->ted->error, sizeof (cfg)->ted->error - 1, "%s:%u: ",  (cfg)->filename, (cfg)->line_number), \
	snprintf((cfg)->ted->error + strlen((cfg)->ted->error), sizeof (cfg)->ted->error - 1 - strlen((cfg)->ted->error), __VA_ARGS__), \
	(cfg)->error = true; } while (0)

typedef struct {
	Ted *ted;
	char const *filename;
	u32 line_number; // currently processing this line number
	bool error;
} ConfigReader;

static void context_copy(SettingsContext *dest, const SettingsContext *src) {
	*dest = *src;
	if (src->path)
		dest->path = str_dup(src->path);
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
	for (u32 i = 0; i < LANG_COUNT; ++i) {
		if (src->language_extensions[i])
			dest->language_extensions[i] = str_dup(src->language_extensions[i]);
	}
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

// Returns the key combination described by str.
static u32 config_parse_key_combo(ConfigReader *cfg, char const *str) {
	u32 modifier = 0;
	// read modifier
	while (true) {
		if (str_has_prefix(str, "Ctrl+")) {
			if (modifier & KEY_MODIFIER_CTRL) {
				config_err(cfg, "Ctrl+ written twice");
				return 0;
			}
			modifier |= KEY_MODIFIER_CTRL;
			str += strlen("Ctrl+");
		} else if (str_has_prefix(str, "Shift+")) {
			if (modifier & KEY_MODIFIER_SHIFT) {
				config_err(cfg, "Shift+ written twice");
				return 0;
			}
			modifier |= KEY_MODIFIER_SHIFT;
			str += strlen("Shift+");
		} else if (str_has_prefix(str, "Alt+")) {
			if (modifier & KEY_MODIFIER_ALT) {
				config_err(cfg, "Alt+ written twice");
				return 0;
			}
			modifier |= KEY_MODIFIER_ALT;
			str += strlen("Alt+");
		} else break;
	}

	// read key
	SDL_Scancode scancode = SDL_GetScancodeFromName(str);
	if (scancode == SDL_SCANCODE_UNKNOWN) {
		typedef struct {
			char const *keyname1;
			char const *keyname2; // alternate key name
			SDL_Scancode scancode;
			bool shift;
		} KeyName;
		static KeyName const key_names[] = {
			{"Apostrophe", "Single Quote", SDL_SCANCODE_APOSTROPHE, 0},
			{"Backslash", 0, SDL_SCANCODE_BACKSLASH, 0},
			{"Comma", 0, SDL_SCANCODE_COMMA, 0},
			{"Equals", 0, SDL_SCANCODE_EQUALS, 0},
			{"Grave", "Backtick", SDL_SCANCODE_GRAVE, 0},
			{"Keypad Plus", 0, SDL_SCANCODE_KP_PLUS, 0},
			{"Keypad Minus", 0, SDL_SCANCODE_KP_MINUS, 0},
			{"Keypad Divide", 0, SDL_SCANCODE_KP_DIVIDE, 0},
			{"Keypad Multiply", 0, SDL_SCANCODE_KP_MULTIPLY, 0},
			{"Left Bracket", 0, SDL_SCANCODE_LEFTBRACKET, 0}, // [
			{"Right Bracket", 0, SDL_SCANCODE_RIGHTBRACKET, 0}, // ]
			{"Dash", 0, SDL_SCANCODE_MINUS, 0},
			{"Minus", 0, SDL_SCANCODE_MINUS, 0},
			{"Period", 0, SDL_SCANCODE_PERIOD, 0},
			{"Semicolon", 0, SDL_SCANCODE_SEMICOLON, 0},
			{"Slash", 0, SDL_SCANCODE_SLASH, 0},
			{"Enter", 0, SDL_SCANCODE_RETURN, 0},
			{"Keypad Return", 0, SDL_SCANCODE_KP_ENTER, 0},
			{"Exclaim", "Exclamation Mark", SDL_SCANCODE_1, 1},
			{"!", 0, SDL_SCANCODE_1, 1},
			{"At", "@", SDL_SCANCODE_2, 1},
			{"Hash", "#", SDL_SCANCODE_3, 1},
			{"Dollar", "$", SDL_SCANCODE_4, 1},
			{"Percent", "%", SDL_SCANCODE_5, 1},
			{"Caret", "^", SDL_SCANCODE_6, 1},
			{"Ampersand", "&", SDL_SCANCODE_7, 1},
			{"Asterisk", "*", SDL_SCANCODE_8, 1},
			{"Left Paren", "(", SDL_SCANCODE_9, 1},
			{"Right Paren", ")", SDL_SCANCODE_0, 1},
			{"Underscore", "_", SDL_SCANCODE_MINUS, 1},
			{"Plus", "+", SDL_SCANCODE_EQUALS, 1},
			{"Left Brace", "{", SDL_SCANCODE_LEFTBRACKET, 1},
			{"Right Brace", "}", SDL_SCANCODE_RIGHTBRACKET, 1},
			{"Pipe", "|", SDL_SCANCODE_BACKSLASH, 1},
			{"Colon", ":", SDL_SCANCODE_SEMICOLON, 1},
			{"Double Quote", "\"", SDL_SCANCODE_APOSTROPHE, 1},
			{"Less Than", "<", SDL_SCANCODE_COMMA, 1},
			{"Greater Than", ">", SDL_SCANCODE_PERIOD, 1},
			{"Question Mark", "?", SDL_SCANCODE_SLASH, 1},
			{"Question", 0, SDL_SCANCODE_SLASH, 1},
			{"Tilde", "~", SDL_SCANCODE_GRAVE, 1},
			{"X1", "x1", SCANCODE_MOUSE_X1, 0},
			{"X2", "x2", SCANCODE_MOUSE_X2, 0}
		};

		// @TODO(optimize): sort key_names (and split keyname1/2); do a binary search
		for (size_t i = 0; i < arr_count(key_names); ++i) {
			KeyName const *k = &key_names[i];
			if (streq(str, k->keyname1) || (k->keyname2 && streq(str, k->keyname2))) {
				scancode = k->scancode;
				if (k->shift) {
					if (modifier & KEY_MODIFIER_SHIFT) {
						config_err(cfg, "Shift+%s is redundant.", str);
						return 0;
					}
					modifier |= KEY_MODIFIER_SHIFT;
				}
				break;
			}
		}
		if (scancode == SDL_SCANCODE_UNKNOWN) {
			if (isdigit(str[0])) { // direct scancode numbers, e.g. Ctrl+24 or Ctrl+08
				char *endp;
				long n = strtol(str, &endp, 10);
				if (*endp == '\0' && n > 0 && n < SCANCODE_COUNT) {
					scancode = (SDL_Scancode)n;
				} else {
					config_err(cfg, "Invalid scancode number: %s", str);
					return 0;
				}
			} else {
				config_err(cfg, "Unrecognized key name: %s.", str);
				return 0;
			}
		}
	}
	return (u32)scancode << 3 | modifier;
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
	settings_initialized = true;
}

void config_read(Ted *ted, ConfigPart **parts, char const *filename) {
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		ted_seterr(ted, "Couldn't open config file %s: %s.", filename, strerror(errno));
		return;
	}
	
	ConfigReader cfg_reader = {
		.ted = ted,
		.filename = filename,
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
			part->file = str_dup(filename);
			part->line = cfg->line_number + 1;
			parse_section_header(&cfg_reader, line, part);
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
		ted_seterr(ted, "Error reading %s.", filename);
	fclose(fp);
}

// IMPORTANT REQUIREMENT FOR THIS FUNCTION:
//     - less specific contexts compare as less
//            (i.e. if context_is_parent(a.context, b.context), then we return -1, and vice versa.)
// if total = true, this gives a total ordering
// if total = false, parts with identical contexts will compare equal.
static int config_part_cmp(const ConfigPart *ap, const ConfigPart *bp, bool total) {
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
	if (total) {
		if (ap->index < bp->index)
			return -1;
		if (ap->index > bp->index)
			return +1;
	}
	return 0;
	
}

static int config_part_qsort_cmp(const void *av, const void *bv) {
	return config_part_cmp(av, bv, true);
}

static i64 config_read_string(Ted *ted, ConfigReader *cfg, char **ptext) {
	char *p;
	int backslashes = 0;
	u32 start_line = cfg->line_number;
	char *start = *ptext + 1;
	char *str = NULL;
	for (p = start; ; ++p) {
		bool done = false;
		switch (*p) {
		case '\\':
			++backslashes;
			++p;
			switch (*p) {
			case '\\':
			case '"':
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
				return -1;
			}
			break;
		case '"':
			done = true;
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
			return -1;
		}
		if (done) break;
		arr_add(str, *p);
	}
	
	i64 str_idx = -1;
	if (ted->nstrings < TED_MAX_STRINGS) {
		char *s = strn_dup(str, arr_len(str));
		str_idx = ted->nstrings;
		ted->strings[ted->nstrings++] = s;
	}
	arr_clear(str);
	*ptext = p + 1;
	return str_idx;
}

static void settings_load_bg_shader(Ted *ted, Settings *s) {
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
%s", s->bg_shader_text);
	
	gl_rc_sab_decref(&s->bg_shader);
	
	GLuint shader = gl_compile_and_link_shaders(ted->error, vshader, fshader);
	if (shader) {
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
		
		s->bg_shader = gl_rc_sab_new(shader, array, buffer);
	}
}

static void settings_load_bg_texture(Ted *ted, Settings *s) {
	gl_rc_texture_decref(&s->bg_texture);
	
	const char *path = s->bg_shader_image;
	char expanded[TED_PATH_MAX];
	expanded[0] = '\0';
	if (path[0] == '~') {
		strbuf_cpy(expanded, ted->home);
		++path;
	}
	strbuf_cat(expanded, path);
	
	GLuint texture = gl_load_texture_from_image(expanded);
	if (texture) {
		s->bg_texture = gl_rc_texture_new(texture);
	} else {
		ted_seterr(ted, "Couldn't load image %s", path);
	}
}

// reads a single "line" of the config file, but it may include a multiline string,
// so it may read multiple lines.
static void config_parse_line(ConfigReader *cfg, Settings *settings, const ConfigPart *part, char **pline) {
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
				settings->colors[setting] = color;
			} else {
				config_err(cfg, "'%s' is not a valid color. Colors should look like #rgb, #rgba, #rrggbb, or #rrggbbaa.", value);
			}
		} else {
		#if DEBUG
			config_err(cfg, "No such color setting: %s", key);
		#endif
		}
	} break;
	case SECTION_KEYBOARD: {
		// lines like Ctrl+Down = 10 :down
		u32 key_combo = config_parse_key_combo(cfg, key);
		KeyAction *action = &settings->key_actions[key_combo];
		llong argument = 1; // default argument = 1
		if (isdigit(*value)) {
			// read the argument
			char *endp;
			argument = strtoll(value, &endp, 10);
			value = endp;
		} else if (*value == '"') {
			// string argument
			
			// restore newline to handle multi-line strings
			// a little bit hacky oh well
			*newline = '\n';
			argument = config_read_string(ted, cfg, &value);
			
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
				action->command = command;
				action->argument = argument;
				action->line_number = cfg->line_number;
			} else {
				config_err(cfg, "Unrecognized command %s", value);
			}
		} else {
			config_err(cfg, "Expected ':' for key action. This line should look something like: %s = :command.", key);
		}
	} break;
	case SECTION_EXTENSIONS: {
		Language lang = language_from_str(key);
		if (lang == LANG_NONE) {
			config_err(cfg, "Invalid programming language: %s.", key);
		} else {
			char *new_str = malloc(strlen(value) + 1);
			if (!new_str) {
				config_err(cfg, "Out of memory.");
			} else {
				char *dst = new_str;
				// get rid of whitespace in extension list
				for (char const *src = value; *src; ++src)
					if (!isspace(*src))
						*dst++ = *src;
				*dst = 0;
				if (settings->language_extensions[lang])
					free(settings->language_extensions[lang]);
				settings->language_extensions[lang] = new_str;
			}
		}
	} break;
	case SECTION_CORE: {
		char const *endptr;
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
		
		if (value[0] == '"') {
			// restore newline to handle multi-line strings
			// a little bit hacky oh well
			*newline = '\n';
			
			i64 string = config_read_string(ted, cfg, &value);
			
			newline = strchr(value, '\n');
			if (!newline) {
				config_err(cfg, "No newline at end of file?");
				*pline += strlen(*pline);
				return;
			}
			*newline = '\0';
			*pline = newline + 1;
			if (string >= 0 && string < TED_MAX_STRINGS) {
				value = ted->strings[string];
			}
		}

		// go through all settings
		bool recognized = false;
		for (size_t i = 0; i < arr_count(settings_all) && !recognized; ++i) {
			SettingAny const *any = &settings_all[i];
			if (any->type == 0) break;
			if (streq(key, any->name)) {
				recognized = true;
				
				if (part->context.language != 0 && !any->per_language) {
					config_err(cfg, "Setting %s cannot be controlled for individual languages.", key);
					break;
				}
				
				switch (any->type) {
				case SETTING_BOOL: {
					const SettingBool *setting = &any->u._bool;
					if (is_bool)
						setting_bool_set(settings, setting, boolean);
					else
						config_err(cfg, "Invalid %s: %s. This should be yes, no, on, or off.", setting->name, value);
				} break;
				case SETTING_U8: {
					const SettingU8 *setting = &any->u._u8;
					if (is_integer && integer >= setting->min && integer <= setting->max)
						setting_u8_set(settings, setting, (u8)integer);
					else
						config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
				} break;
				case SETTING_U16: {
					const SettingU16 *setting = &any->u._u16;
					if (is_integer && integer >= setting->min && integer <= setting->max)
						setting_u16_set(settings, setting, (u16)integer);
					else
						config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", setting->name, value, setting->min, setting->max);
				} break;
				case SETTING_U32: {
					const SettingU32 *setting = &any->u._u32;
					if (is_integer && integer >= setting->min && integer <= setting->max)
						setting_u32_set(settings, setting, (u32)integer);
					else
						config_err(cfg, "Invalid %s: %s. This should be an integer from %" PRIu32 " to %" PRIu32 ".",
							setting->name, value, setting->min, setting->max);
				} break;
				case SETTING_FLOAT: {
					const SettingFloat *setting = &any->u._float;
					if (is_floating && floating >= setting->min && floating <= setting->max)
						setting_float_set(settings, setting, (float)floating);
					else
						config_err(cfg, "Invalid %s: %s. This should be a number from %g to %g.", setting->name, value, setting->min, setting->max);
				} break;
				case SETTING_STRING: {
					const SettingString *setting = &any->u._string;
					if (strlen(value) >= setting->buf_size) {
						config_err(cfg, "%s is too long (length: %zu, maximum length: %zu).", key, strlen(value), setting->buf_size - 1);
					} else {
						setting_string_set(settings, setting, value);
					}
				} break;
				}
			}
		}
		
		if (streq(key, "bg-shader"))
			settings_load_bg_shader(ted, settings);
		if (streq(key, "bg-texture"))
			settings_load_bg_texture(ted, settings);
		
		// this is probably a bad idea:
		//if (!recognized)
		//	config_err(cfg, "Unrecognized setting: %s", key);
		// because if we ever remove a setting qin the future
		// everyone will get errors
	} break;
	}
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
	
	Settings *settings = NULL;
	
	arr_foreach_ptr(parts, ConfigPart, part) {
		cfg->filename = part->file;
		cfg->line_number = part->line;
		
		if (part == parts || config_part_cmp(part, part - 1, false) != 0) {
			// new settings
			settings = arr_addp(ted->all_settings);
			
			// go backwards to find most specific parent
			ConfigPart *parent = part;
			while (1) {
				if (parent <= parts) {
					parent = NULL;
					break;
				}
				--parent;
				if (context_is_parent(&parent->context, &part->context)) {
					// copy parent's settings
					settings_copy(settings, &ted->all_settings[parent->settings]);
					break;
				}
			}
			
			context_free(&settings->context);
			context_copy(&settings->context, &part->context);
		}
		part->settings = arr_len(ted->all_settings) - 1;
		
		
		arr_add(part->text, '\0'); // null termination
		char *line = part->text;
		while (*line) {
			config_parse_line(cfg, settings, part, &line);
	
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
		for (u32 i = 0; i < LANG_COUNT; ++i)
			free(settings->language_extensions[i]);
		gl_rc_sab_decref(&settings->bg_shader);
		gl_rc_texture_decref(&settings->bg_texture);
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

// returns the best guess for the root directory of the project containing `path` (which should be an absolute path)
// the return value should be freed.
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
