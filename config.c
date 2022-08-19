// Read a configuration file.
// Config files are formatted as several sections, each containing key = value pairs.
// e.g.:
// [section1]
// thing1 = 33
// thing2 = 454
// [section2]
// asdf = 123

typedef enum {
	SECTION_NONE,
	SECTION_CORE,
	SECTION_KEYBOARD,
	SECTION_COLORS,
	SECTION_EXTENSIONS
} Section;

// all worth it for the -Wformat warnings
#define config_err(cfg, ...) do { snprintf((cfg)->ted->error, sizeof (cfg)->ted->error - 1, "%s:%u: ",  (cfg)->filename, (cfg)->line_number), \
	snprintf((cfg)->ted->error + strlen((cfg)->ted->error), sizeof (cfg)->ted->error - 1 - strlen((cfg)->ted->error), __VA_ARGS__), \
	(cfg)->error = true; } while (0)

typedef struct {
	Ted *ted;
	char const *filename;
	u32 line_number; // currently processing this line number
	bool error;
} ConfigReader;

// Returns the key combination described by str.
static u32 config_parse_key_combo(ConfigReader *cfg, char const *str) {
	u32 modifier = 0;
	// read modifier
	while (true) {
		if (str_is_prefix(str, "Ctrl+")) {
			if (modifier & KEY_MODIFIER_CTRL) {
				config_err(cfg, "Ctrl+ written twice");
				return 0;
			}
			modifier |= KEY_MODIFIER_CTRL;
			str += strlen("Ctrl+");
		} else if (str_is_prefix(str, "Shift+")) {
			if (modifier & KEY_MODIFIER_SHIFT) {
				config_err(cfg, "Shift+ written twice");
				return 0;
			}
			modifier |= KEY_MODIFIER_SHIFT;
			str += strlen("Shift+");
		} else if (str_is_prefix(str, "Alt+")) {
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

		// @OPTIMIZE: sort key_names (and split keyname1/2); do a binary search
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


// all the "control" pointers here are relative to a NULL Settings object.
typedef struct {
	char const *name;
	const bool *control;
	bool per_language; // allow per-language control
} OptionBool;
typedef struct {
	char const *name;
	const u8 *control;
	u8 min, max;
	bool per_language;
} OptionU8;
typedef struct {
	char const *name;
	const float *control;
	float min, max;
	bool per_language;
} OptionFloat;
typedef struct {
	char const *name;
	const u16 *control;
	u16 min, max;
	bool per_language;
} OptionU16;
typedef struct {
	char const *name;
	const char *control;
	size_t buf_size;
	bool per_language;
} OptionString;

typedef enum {
	OPTION_BOOL = 1,
	OPTION_U8,
	OPTION_U16,
	OPTION_FLOAT,
	OPTION_STRING
} OptionType;
typedef struct {
	OptionType type;
	const char *name;
	bool per_language;
	union {
		OptionU8 _u8;
		OptionBool _bool;
		OptionU16 _u16;
		OptionFloat _float;
		OptionString _string;
	} u;
} OptionAny;

static void option_bool_set(Settings *settings, const OptionBool *opt, bool value) {
	*(bool *)((char *)settings + (size_t)opt->control) = value;
}
static void option_u8_set(Settings *settings, const OptionU8 *opt, u8 value) {
	if (value >= opt->min && value <= opt->max)
		*(u8 *)((char *)settings + (size_t)opt->control) = value;
}
static void option_u16_set(Settings *settings, const OptionU16 *opt, u16 value) {
	if (value >= opt->min && value <= opt->max)
		*(u16 *)((char *)settings + (size_t)opt->control) = value;
}
static void option_float_set(Settings *settings, const OptionFloat *opt, float value) {
	if (value >= opt->min && value <= opt->max)
		*(float *)((char *)settings + (size_t)opt->control) = value;
}
static void option_string_set(Settings *settings, const OptionString *opt, const char *value) {
	char *control = (char *)settings + (size_t)opt->control;
	str_cpy(control, opt->buf_size, value);
}

// two passes are done
//   pass 0 reads global settings
//   pass 1 reads language-specific settings
void config_read(Ted *ted, char const *filename, int pass) {
	ConfigReader cfg_reader = {
		.ted = ted,
		.filename = filename,
		.line_number = 1,
		.error = false
	};
	ConfigReader *cfg = &cfg_reader;
	Settings *settings = ted->settings;
	
	// core options
	// (these go at the start so they don't need to be re-computed each time)
	const Settings *nullset = NULL;
	OptionBool const options_bool[] = {
		{"auto-indent", &nullset->auto_indent, true},
		{"auto-add-newline", &nullset->auto_add_newline, true},
		{"auto-reload", &nullset->auto_reload, true},
		{"auto-reload-config", &nullset->auto_reload_config, false},
		{"syntax-highlighting", &nullset->syntax_highlighting, true},
		{"line-numbers", &nullset->line_numbers, true},
		{"restore-session", &nullset->restore_session, false},
		{"regenerate-tags-if-not-found", &nullset->regenerate_tags_if_not_found, true},
		{"indent-with-spaces", &nullset->indent_with_spaces, true},
	};
	OptionU8 const options_u8[] = {
		{"tab-width", &nullset->tab_width, 1, 100, true},
		{"cursor-width", &nullset->cursor_width, 1, 100, true},
		{"undo-save-time", &nullset->undo_save_time, 1, 200, true},
		{"border-thickness", &nullset->border_thickness, 1, 30, false},
		{"padding", &nullset->padding, 0, 100, false},
		{"scrolloff", &nullset->scrolloff, 1, 100, true},
		{"tags-max-depth", &nullset->tags_max_depth, 1, 100, false},
	};
	OptionU16 const options_u16[] = {
		{"text-size", &nullset->text_size, TEXT_SIZE_MIN, TEXT_SIZE_MAX, true},
		{"max-menu-width", &nullset->max_menu_width, 10, U16_MAX, false},
		{"error-display-time", &nullset->error_display_time, 0, U16_MAX, false},
	};
	OptionFloat const options_float[] = {
		{"cursor-blink-time-on", &nullset->cursor_blink_time_on, 0, 1000, true},
		{"cursor-blink-time-off", &nullset->cursor_blink_time_off, 0, 1000, true},
	};
	OptionString const options_string[] = {
		{"build-default-command", nullset->build_default_command, sizeof nullset->build_default_command, true},
	};
	
	OptionAny all_options[1000] = {0};
	OptionAny *all_options_end = all_options;
	for (size_t i = 0; i < arr_count(options_bool); ++i) {
		OptionAny *opt = all_options_end++;
		opt->type = OPTION_BOOL;
		opt->name = options_bool[i].name;
		opt->per_language = options_bool[i].per_language;
		opt->u._bool = options_bool[i];
	}
	for (size_t i = 0; i < arr_count(options_u8); ++i) {
		OptionAny *opt = all_options_end++;
		opt->type = OPTION_U8;
		opt->name = options_u8[i].name;
		opt->per_language = options_u8[i].per_language;
		opt->u._u8 = options_u8[i];
	}
	for (size_t i = 0; i < arr_count(options_u16); ++i) {
		OptionAny *opt = all_options_end++;
		opt->type = OPTION_U16;
		opt->name = options_u16[i].name;
		opt->per_language = options_u16[i].per_language;
		opt->u._u16 = options_u16[i];
	}
	for (size_t i = 0; i < arr_count(options_float); ++i) {
		OptionAny *opt = all_options_end++;
		opt->type = OPTION_FLOAT;
		opt->name = options_float[i].name;
		opt->per_language = options_float[i].per_language;
		opt->u._float = options_float[i];
	}
	for (size_t i = 0; i < arr_count(options_string); ++i) {
		OptionAny *opt = all_options_end++;
		opt->type = OPTION_STRING;
		opt->name = options_string[i].name;
		opt->per_language = options_string[i].per_language;
		opt->u._string = options_string[i];
	}

	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		ted_seterr(ted, "Couldn't open config file %s.", filename);
		return;
	}
	
	char line[4096] = {0};
	int line_cap = sizeof line;
	
	Section section = SECTION_NONE;
	Language language = LANG_NONE;
	bool skip_section = false;
	
	while (fgets(line, line_cap, fp)) {
		char *newline = strchr(line, '\n');
		if (!newline && !feof(fp)) {
			config_err(cfg, "Line is too long.");
			break;
		}
		
		if (newline) *newline = '\0';
		char *carriage_return = strchr(line, '\r');
		if (carriage_return) *carriage_return = '\0';

		// ok, we've now read a line.
		switch (line[0]) {
		case '#': // comment
		case '\0': // blank line
			break;
		case '[': { // section header
		#define SECTION_HEADER_HELP "Section headers should look like this: [section-name]"
			char *closing = strchr(line, ']');
			if (!closing) {
				config_err(cfg, "Unmatched [. " SECTION_HEADER_HELP);
			} else if (closing[1] != '\0') {
				config_err(cfg, "Text after section. " SECTION_HEADER_HELP);
			} else {
				*closing = '\0';
				char *section_name = line + 1;
				char *dot = strchr(section_name, '.');
				
				if (dot) {
					*dot = '\0';
					language = language_from_str(section_name);
					if (!language) {
						config_err(cfg, "Unrecognized language: %s.", section_name);
						break; // skip section name check
					}
					section_name = dot + 1;
				} else {
					language = 0;
				}
				
				if (streq(section_name, "keyboard")) {
					section = SECTION_KEYBOARD;
				} else if (streq(section_name, "colors")) {
					section = SECTION_COLORS;
				} else if (streq(section_name, "core")) {
					section = SECTION_CORE;
				} else if (streq(section_name, "extensions")) {
					section = SECTION_EXTENSIONS;
				} else {
					config_err(cfg, "Unrecognized section: [%s].", section_name);
					break;
				}
				
				skip_section = false;
				if (language) {
					switch (section) {
					case SECTION_CORE:
					case SECTION_COLORS:
					case SECTION_KEYBOARD:
						break;
					default:
						config_err(cfg, "%s settings cannot be configured for individual languages.",
							section_name);
						break;
					}
					if (pass == 0) {
						skip_section = true;
					}
				} else {
					if (pass == 1) {
						skip_section = true;
					}
				}
				if (pass == 1) {
					settings = &ted->settings_by_language[language];
				}
			}
		} break;
		default: {
			if (skip_section) break;
			
			char *equals = strchr(line, '=');
			if (equals) {
				char const *key = line;
				*equals = '\0';
				char const *value = equals + 1;
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
				} else {
					switch (section) {
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
							config_err(cfg, "No such color option: %s", key);
						}
					} break;
					case SECTION_KEYBOARD: {
						// lines like Ctrl+Down = 10 :down
						u32 key_combo = config_parse_key_combo(cfg, key);
						KeyAction *action = &settings->key_actions[key_combo];
						llong argument = 1;
						if (isdigit(*value)) {
							// read the argument
							char *endp;
							argument = strtoll(value, &endp, 10);
							value = endp;
						} else if (*value == '"') {
							// string argument
							int backslashes = 0;
							char const *p;
							for (p = value + 1; *p; ++p) {
								bool done = false;
								switch (*p) {
								case '\\':
									++backslashes;
									break;
								case '"':
									if (backslashes % 2 == 0)
										done = true;
									break;
								}
								if (done) break;
							}
							if (!*p) {
								config_err(cfg, "String doesn't end.");
								break;
							}
							if (ted->nstrings < TED_MAX_STRINGS) {
								char *str = strn_dup(value + 1, (size_t)(p - (value + 1)));
								argument = ted->nstrings | ARG_STRING;
								ted->strings[ted->nstrings++] = str;
							}
							value = p + 1;
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
					#define BOOL_HELP "(should be yes/no/on/off)"
						if (streq(value, "yes") || streq(value, "on")) {
							is_bool = true;
							boolean = true;
						} else if (streq(value, "no") || streq(value, "off")) {
							is_bool = true;
							boolean = false;
						}

						// go through all options
						bool recognized = false;
						for (size_t i = 0; i < arr_count(all_options) && !recognized; ++i) {
							OptionAny const *any = &all_options[i];
							if (any->type == 0) break;
							if (streq(key, any->name)) {
								recognized = true;
								
								if (language != 0 && !any->per_language) {
									config_err(cfg, "Option %s cannot be controlled for individual languages.", key);
									break;
								}
								
								switch (any->type) {
								case OPTION_BOOL: {
									OptionBool const *option = &any->u._bool;
									if (is_bool)
										option_bool_set(settings, option, boolean);
									else
										config_err(cfg, "Invalid %s: %s. This should be yes, no, on, or off.", option->name, value);
								} break;
								case OPTION_U8: {
									OptionU8 const *option = &any->u._u8;
									if (is_integer && integer >= option->min && integer <= option->max)
										option_u8_set(settings, option, (u8)integer);
									else
										config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", option->name, value, option->min, option->max);
								} break;
								case OPTION_U16: {
									OptionU16 const *option = &any->u._u16;
									if (is_integer && integer >= option->min && integer <= option->max)
										option_u16_set(settings, option, (u16)integer);
									else
										config_err(cfg, "Invalid %s: %s. This should be an integer from %u to %u.", option->name, value, option->min, option->max);
								} break;
								case OPTION_FLOAT: {
									OptionFloat const *option = &any->u._float;
									if (is_floating && floating >= option->min && floating <= option->max)
										option_float_set(settings, option, (float)floating);
									else
										config_err(cfg, "Invalid %s: %s. This should be a number from %g to %g.", option->name, value, option->min, option->max);
								} break;
								case OPTION_STRING: {
									OptionString const *option = &any->u._string;
									if (strlen(value) >= option->buf_size) {
										config_err(cfg, "%s is too long (length: %zu, maximum length: %zu).", key, strlen(value), option->buf_size - 1);
									} else {
										option_string_set(settings, option, value);
									}
								} break;
								}
							}
						}
						
						// this is probably a bad idea:
						//if (!recognized)
						//	config_err(cfg, "Unrecognized option: %s", key);
						// because if we ever remove an option in the future
						// everyone will get errors
					} break;
					}
				}
			} else {
				config_err(cfg, "Invalid line syntax. "
					"Lines should either look like [section-name] or key = value");
			}
		} break;
		}
		if (cfg->error) break;

		++cfg->line_number;
	}
	
	
	if (ferror(fp))
		ted_seterr(ted, "Error reading %s.", filename);
	fclose(fp);
}

void config_free(Ted *ted) {
	for (u16 i = 0; i < LANG_COUNT; ++i) {
		free(ted->settings_by_language[0].language_extensions[i]);
		for (u16 l = 0; l < LANG_COUNT; ++l) {
			// these are just aliases to settings_by_language[0].language_extensions[i]
			// (you cant change language extensions on a per language basis. that would be weird.)
			ted->settings_by_language[l].language_extensions[i] = NULL;
		}
	}
	for (u32 i = 0; i < ted->nstrings; ++i) {
		free(ted->strings[i]);
	}
}
