#include "keywords.h"

// returns the language this string is referring to, or LANG_NONE if it's invalid.
Language language_from_str(char const *str) {
	for (int i = 0; i < LANG_COUNT; ++i) {
		if (streq(language_names[i].name, str))
			return language_names[i].lang;
	}
	return LANG_NONE;
}

// NOTE: returns the color setting, not the color
ColorSetting syntax_char_type_to_color(SyntaxCharType t) {
	switch (t) {
	case SYNTAX_NORMAL: return COLOR_TEXT;
	case SYNTAX_KEYWORD: return COLOR_KEYWORD;
	case SYNTAX_COMMENT: return COLOR_COMMENT;
	case SYNTAX_PREPROCESSOR: return COLOR_PREPROCESSOR;
	case SYNTAX_STRING: return COLOR_STRING;
	case SYNTAX_CHARACTER: return COLOR_CHARACTER;
	case SYNTAX_CONSTANT: return COLOR_CONSTANT;
	}
	return COLOR_TEXT;
}

static inline bool keyword_matches(char32_t *text, size_t len, char const *keyword) {
	if (len == strlen(keyword)) {
		bool matches = true;
		char32_t *p = text;
		// check if `p` starts with `keyword`
		for (char const *q = keyword; *q; ++p, ++q) {
			if (*p != (char32_t)*q) {
				matches = false;
				break;
			}
		}
		return matches;
	} else {
		return false;
	}
}

static void syntax_highlight_c_cpp(SyntaxState *state_ptr, bool cpp, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool in_preprocessor = (state & SYNTAX_STATE_PREPROCESSOR) != 0;
	bool in_string = (state & SYNTAX_STATE_STRING) != 0;
	bool in_single_line_comment = (state & SYNTAX_STATE_SINGLE_LINE_COMMENT) != 0;
	bool in_multi_line_comment = (state & SYNTAX_STATE_MULTI_LINE_COMMENT) != 0;
	bool in_char = false;
	bool in_number = false;
	
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {
		SyntaxCharType type = SYNTAX_NORMAL;
		// necessary for the final " of a string to be highlighted
		bool in_string_now = in_string;
		bool in_char_now = in_char;
		bool in_multi_line_comment_now = in_multi_line_comment;

		// are there 1/2 characters left in the line?
		bool has_1_char = i + 1 < line_len;

		bool dealt_with = false;
		
		char32_t c = line[i];
		switch (c) {
		case '#':
			if (!in_single_line_comment && !in_multi_line_comment && !in_char && !in_string)
				in_preprocessor = true;
			break;
		case '\\':
			++backslashes;
			break;
		case '/':
			if (!in_multi_line_comment && !in_single_line_comment && !in_string && !in_char && has_1_char) {
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
			else if (!in_multi_line_comment && !in_single_line_comment && !in_char)
				in_string = in_string_now = true;
			break;
		case '\'':
			if (in_char && backslashes % 2 == 0)
				in_char = false;
			else if (!in_multi_line_comment && !in_single_line_comment && !in_string)
				in_char = in_char_now = true;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': // don't you wish C had case ranges...
			// a number!
			if (!in_single_line_comment && !in_multi_line_comment && !in_string && !in_number && !in_char) {
				in_number = true;
				if (i) {
					if (line[i - 1] == '.') {
						// support .6, for example
						char_types[i - 1] = SYNTAX_CONSTANT;
					} else if (is32_ident(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						in_number = false;
					}
				}
			}
			break;
		default: {
			if ((i && is32_ident(line[i - 1])) || !is32_ident(c))
				break; // can't be a keyword on its own.
			
		
			// keywords don't matter for advancing the state
			if (char_types && !in_single_line_comment && !in_multi_line_comment && !in_string && !in_preprocessor && !in_char) {
				u32 keyword_end;
				// find where this keyword would end (if this is a keyword)
				for (keyword_end = i; keyword_end < line_len && is32_ident(line[keyword_end]); ++keyword_end);
				
				u32 keyword_len = keyword_end - i;
				char const *const *keywords = c < arr_count(syntax_all_keywords_c) ? syntax_all_keywords_c[c] : NULL;
				char const *keyword = NULL;
				if (keywords)
					for (size_t k = 0; keywords[k]; ++k)
						if (keyword_matches(&line[i], keyword_len, keywords[k]))
							keyword = keywords[k];
				if (cpp && !keyword) {
					// check C++'s keywords too!
					keywords = c < arr_count(syntax_all_keywords_cpp) ? syntax_all_keywords_cpp[c] : NULL;
					if (keywords)
						for (size_t k = 0; keywords[k]; ++k)
							if (keyword_matches(&line[i], keyword_len, keywords[k]))
								keyword = keywords[k];
				}
				
				if (keyword) {
					// it's a keyword
					// let's highlight all of it now
					type = SYNTAX_KEYWORD;
					if (isupper(keyword[0]) || 
						(keyword_len == 4 && streq(keyword, "true")) ||
						(keyword_len == 5 && streq(keyword, "false")) ||
						(keyword_len == 6 && (streq(keyword, "stderr") || streq(keyword, "stdout")))
						) {
						type = SYNTAX_CONSTANT; // these are constants, not keywords
						}
					for (size_t j = 0; keyword[j]; ++j) {
						char_types[i++] = type;
					}
					--i; // we'll increment i from the for loop
					dealt_with = true;
					break;
				}
			}
		} break;
		}
		if (line[i] != '\\') backslashes = 0;
		if (in_number && !(is32_digit(line[i]) || line[i] == '.'
			|| (line[i] < CHAR_MAX && strchr("xXoObBlLuUabcdefABCDEF", (char)line[i]))
 			|| (i && line[i-1] == 'e' && (line[i] == '+' || line[i] == '-')))) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			if (in_single_line_comment || in_multi_line_comment_now)
				type = SYNTAX_COMMENT;
			else if (in_string_now)
				type = SYNTAX_STRING;
			else if (in_char_now)
				type = SYNTAX_CHARACTER;
			else if (in_number)
				type = SYNTAX_CONSTANT;
			else if (in_preprocessor)
				type = SYNTAX_PREPROCESSOR;

			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		  (backslashes && in_single_line_comment) << SYNTAX_STATE_SINGLE_LINE_COMMENT_SHIFT
		| (backslashes && in_preprocessor) << SYNTAX_STATE_PREPROCESSOR_SHIFT
		| (backslashes && in_string) << SYNTAX_STATE_STRING_SHIFT
		| in_multi_line_comment << SYNTAX_STATE_MULTI_LINE_COMMENT_SHIFT);
}

// This is the main syntax highlighting function. It will determine which colors to use for each character.
// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	switch (lang) {
	case LANG_NONE:
		if (char_types)
			memset(char_types, 0, line_len * sizeof *char_types);
		break;
	case LANG_C:
		syntax_highlight_c_cpp(state, false, line, line_len, char_types);
		break;
	case LANG_CPP:
		syntax_highlight_c_cpp(state, true, line, line_len, char_types);
		break;
	case LANG_COUNT: assert(0); break;
	}
}
