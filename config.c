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
	SECTION_KEYBOARD
} Section;

// Returns the key combination described by str. filename and line_number should be passed inm to generate nice errors.
static u32 config_parse_key_combo(Ted *ted, char const *str, char const *filename, uint line_number) {
	u32 modifier = 0;
	// read modifier
	while (true) {
		if (util_is_prefix(str, "Ctrl+")) {
			if (modifier & KEY_MODIFIER_CTRL) {
				ted_seterr(ted, "%s:%u: Ctrl+ written twice", filename, line_number);
				return 0;
			}
			modifier |= KEY_MODIFIER_CTRL;
			str += strlen("Ctrl+");
		} else if (util_is_prefix(str, "Shift+")) {
			if (modifier & KEY_MODIFIER_SHIFT) {
				ted_seterr(ted, "%s:%u: Shift+ written twice", filename, line_number);
				return 0;
			}
			modifier |= KEY_MODIFIER_SHIFT;
			str += strlen("Shift+");
		} else if (util_is_prefix(str, "Alt+")) {
			if (modifier & KEY_MODIFIER_ALT) {
				ted_seterr(ted, "%s:%u: Alt+ written twice", filename, line_number);
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
			{"Tilde", "~", SDL_SCANCODE_GRAVE, 1}
		};

		// @OPTIMIZE: sort key_names; do a binary search
		for (size_t i = 0; i < arr_count(key_names); ++i) {
			KeyName const *k = &key_names[i];
			if (streq(str, k->keyname1) || (k->keyname2 && streq(str, k->keyname2))) {
				scancode = k->scancode;
				if (k->shift) {
					if (modifier & KEY_MODIFIER_SHIFT) {
						ted_seterr(ted, "%s:%u: Shift+%s is redundant.", filename, line_number, str);
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
					ted_seterr(ted, "%s:%u: Invalid scancode number: %s", filename, line_number, str);
					return 0;
				}
			} else {
				ted_seterr(ted, "%s:%u: Unrecognized key name: %s.", filename, line_number, str);
				return 0;
			}
		}
	}
	return (u32)scancode << 3 | modifier;
}

void config_read(Ted *ted, char const *filename) {
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		uint line_number = 1;
		int line_cap = 4096;
		char *line = ted_malloc(ted, (size_t)line_cap);
		if (line) {
			Section section = SECTION_NONE;

			while (fgets(line, line_cap, fp)) {
				char *newline = strchr(line, '\n');
				if (newline || feof(fp)) {
					if (newline) *newline = '\0';

					bool error = false;

					// ok, we've now read a line.
					switch (line[0]) {
					case '#': // comment
					case '\0': // blank line
						break;
					case '[': { // section header
					#define SECTION_HEADER_HELP "Section headers should look like this: [section-name]"
						char *closing = strchr(line, ']');
						if (!closing) {
							ted_seterr(ted, "%s:%u: Unmatched [. " SECTION_HEADER_HELP, filename, line_number);
							error = true;
						} else if (closing[1] != '\0') {
							ted_seterr(ted, "%s:%u: Text after section. " SECTION_HEADER_HELP, filename, line_number);
							error = true;
						} else {
							*closing = '\0';
							char *section_name = line + 1;
							if (streq(section_name, "keyboard")) {
								section = SECTION_KEYBOARD;
							} else {
								ted_seterr(ted, "%s:%u: Unrecognized section: [%s].", filename, line_number, section_name);
								error = true;
							}
						}
					} break;
					default: {
						char *equals = strchr(line, '=');
						if (equals) {
							char *key = line;
							*equals = '\0';
							char *value = key + 1;
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
								ted_seterr(ted, "%s:%u: Empty property name. This line should look like: key = value", filename, line_number);
							} else {
								switch (section) {
								case SECTION_NONE:
									ted_seterr(ted, "%s:%u: Line outside of any section."
										"Try putting a section header, e.g. [keyboard] before this line?", filename, line_number);
									break;
								case SECTION_KEYBOARD: {
									u32 key_combo = config_parse_key_combo(ted, key, filename, line_number);
									(void)key_combo; // @TODO
								} break;
								}
							}
						} else {
							ted_seterr(ted, "%s:%u: Invalid line syntax."
								"Lines should either look like [section-name] or key = value", filename, line_number);
						}
					} break;
					}

					if (error) break;

					++line_number;
				} else {
					ted_seterr(ted, "%s:%u: Line is too long.", filename, line_number);
					break;
				}
			}
		}
		free(line);
		if (ferror(fp))
			ted_seterr(ted, "Error reading %s.", filename);
		fclose(fp);
	} else {
		ted_seterr(ted, "Couldn't open file %s.", filename);
	}
}
