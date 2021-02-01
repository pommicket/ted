typedef struct {
	bool multi_line_comment:1; // are we in a multi-line comment? (delineated by /* */)
	bool continued_single_line_comment:1; // if you add a \ to the end of a single-line comment, it is continued to the next line.
	bool continued_preprocessor:1; // similar to above
	bool continued_string:1;
} SyntaxStateC;

typedef union {
	SyntaxStateC c;
} SyntaxState;

ENUM_U16 {
	LANG_C
} ENUM_U16_END(Language);

ENUM_U8 {
	SYNTAX_NORMAL,
	SYNTAX_KEYWORD,
	SYNTAX_COMMENT,
	SYNTAX_PREPROCESSOR,
	SYNTAX_STRING,
	SYNTAX_CHARACTER,
	SYNTAX_NUMBER
} ENUM_U8_END(SyntaxCharType);

// NOTE: returns the color setting, not the color
ColorSetting syntax_char_type_to_color(SyntaxCharType t) {
	switch (t) {
	case SYNTAX_NORMAL: return COLOR_TEXT;
	case SYNTAX_KEYWORD: return COLOR_KEYWORD;
	case SYNTAX_COMMENT: return COLOR_COMMENT;
	case SYNTAX_PREPROCESSOR: return COLOR_PREPROCESSOR;
	case SYNTAX_STRING: return COLOR_STRING;
	case SYNTAX_CHARACTER: return COLOR_CHARACTER;
	case SYNTAX_NUMBER: return COLOR_NUMBER;
	}
}

static void syntax_highlight_c(SyntaxStateC *state, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	(void)state;
	bool in_preprocessor = state->continued_preprocessor;
	bool in_string = state->continued_string;
	bool in_single_line_comment = state->continued_single_line_comment; // this kind of comment :)
	bool in_multi_line_comment = state->multi_line_comment;
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {
		SyntaxCharType type = SYNTAX_NORMAL;
		// necessary for the final " of a string to be highlighted
		bool in_string_now = in_string;
		bool in_multi_line_comment_now = in_multi_line_comment;

		// are there 1/2 characters left in the line?
		bool has_1_char = i + 1 < line_len;

		switch (line[i]) {
		case '#':
			if (!in_single_line_comment && !in_multi_line_comment)
				in_preprocessor = true;
			break;
		case '\\':
			++backslashes;
			break;
		case '/':
			if (!in_multi_line_comment && !in_single_line_comment && !in_string && has_1_char) {
				if (line[i + 1] == '/')
					in_single_line_comment = true; // //
				else if (line[i + 1] == '*')
					in_multi_line_comment = in_multi_line_comment_now = true; // /*
			} else if (in_multi_line_comment) {
				if (i && line[i - 1] == '*') {
					// */
					in_multi_line_comment = false;
				}
			}
			break;
		case '"':
			if (in_string && backslashes % 2 == 0)
				in_string = false;
			else if (!in_multi_line_comment && !in_single_line_comment)
				in_string = in_string_now = true;
			break;
		case '<': // preprocessor string, e.g. <stdio.h>
			if (in_preprocessor)
				in_string = in_string_now = true;
			break;
		case '>':
			if (in_preprocessor && in_string)
				in_string = false;
			break;
		}
		if (line[i] != '\\') backslashes = 0;

		if (in_single_line_comment || in_multi_line_comment_now)
			type = SYNTAX_COMMENT;
		else if (in_string_now)
			type = SYNTAX_STRING;
		else if (in_preprocessor)
			type = SYNTAX_PREPROCESSOR;

		if (char_types) {
			char_types[i] = type;
		}
	}
	state->continued_single_line_comment = backslashes && in_single_line_comment;
	state->continued_preprocessor = backslashes && in_preprocessor;
	state->continued_string = backslashes && in_string;

	state->multi_line_comment = in_multi_line_comment;
}

// This is the main syntax highlighting function. It will determine which colors to use for each character.
// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	switch (lang) {
	case LANG_C:
		syntax_highlight_c(&state->c, line, line_len, char_types);
		break;
	}
}
