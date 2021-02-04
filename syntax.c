#include "keywords.h"

// all characters that can appear in a number
#define SYNTAX_DIGITS "0123456789.xXoObBlLuUiIabcdefABCDEF_"


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
	case SYNTAX_BUILTIN: return COLOR_BUILTIN;
	}
	return COLOR_TEXT;
}

static inline bool syntax_keyword_matches(char32_t *text, size_t len, char const *keyword) {
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

// lookup the given string in the keywords table
static Keyword const *syntax_keyword_lookup(Keyword const *const *all_keywords, size_t n_all_keywords, char32_t *str, size_t len) {
	if (!len) return NULL;
	if (str[0] >= n_all_keywords) return NULL;

	Keyword const *keywords = all_keywords[str[0]];

	if (keywords) {
		for (size_t k = 0; keywords[k].str; ++k) {
			if (syntax_keyword_matches(str, len, keywords[k].str)) {
				return &keywords[k];
			}
		}
	}
	return NULL;
}

// does i continue the number literal from i-1
static inline bool syntax_number_continues(char32_t *line, u32 line_len, u32 i) {
	if (line[i] == '.' && ((i && line[i-1] == '.') || (i < line_len-1 && line[i+1] == '.')))
		return false; // can't have two .s in a row
	return (line[i] < CHAR_MAX && 
		(strchr(SYNTAX_DIGITS, (char)line[i])
		|| (i && line[i-1] == 'e' && (line[i] == '+' || line[i] == '-'))));
}

// find how long this keyword would be (if this is a keyword)
static inline u32 syntax_keyword_len(Language lang, char32_t *line, u32 i, u32 line_len) {
	u32 keyword_end;
	for (keyword_end = i; 
		keyword_end < line_len 
		&& (is32_ident(line[keyword_end]) 
		|| (lang == LANG_RUST && line[keyword_end] == '!')) // for rust builtin macros		
		; ++keyword_end);
	return keyword_end - i;
}	


static void syntax_highlight_c_cpp(SyntaxState *state_ptr, bool cpp, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool in_preprocessor = (state & SYNTAX_STATE_CPP_PREPROCESSOR) != 0;
	bool in_string = (state & SYNTAX_STATE_CPP_STRING) != 0;
	bool in_single_line_comment = (state & SYNTAX_STATE_CPP_SINGLE_LINE_COMMENT) != 0;
	bool in_multi_line_comment = (state & SYNTAX_STATE_CPP_MULTI_LINE_COMMENT) != 0;
	bool in_raw_string = (state & SYNTAX_STATE_CPP_RAW_STRING);
	bool in_char = false;
	bool in_number = false;
	bool raw_string_ending = false;
	
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {

		// are there 1/2 characters left in the line?
		bool has_1_char =  i + 1 < line_len;
		bool has_2_chars = i + 2 < line_len;
		
		bool dealt_with = false;
		
		char32_t c = line[i];
		
		if (in_raw_string) {
			if (has_2_chars && c == ')' && line[1] == '"') {
				raw_string_ending = true;
			}
			if (char_types)
				char_types[i] = SYNTAX_STRING;
			if (raw_string_ending && c == '"')
				in_raw_string = false;
			dealt_with = true;
		} else switch (c) {
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
					in_multi_line_comment = true; // /*
			} else if (in_multi_line_comment) {
				if (i && line[i - 1] == '*') {
					// */
					in_multi_line_comment = false;
					if (char_types) {
						dealt_with = true;
						char_types[i] = SYNTAX_COMMENT;
					}
				}
			}
			break;
		case '"':
			if (in_string && backslashes % 2 == 0) {
				in_string = false;
				if (char_types) {
					dealt_with = true;
					char_types[i] = SYNTAX_STRING;
				}
			} else if (!in_multi_line_comment && !in_single_line_comment && !in_char) {
				in_string = true;
			}
			break;
		case '\'':
			if (in_char && backslashes % 2 == 0) {
				in_char = false;
				if (char_types) {
					dealt_with = true;
					char_types[i] = SYNTAX_CHARACTER;
				}
			} else if (!in_multi_line_comment && !in_single_line_comment && !in_string) {
				in_char = true;
			}
			break;
		case ANY_DIGIT:
			// a number!
			if (char_types && !in_single_line_comment && !in_multi_line_comment && !in_string && !in_number && !in_char) {
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
			
			if (!in_single_line_comment && !in_multi_line_comment && !in_string && c == 'R' && has_2_chars && line[i + 1] == '"' && line[i + 2] == '(') {
				// raw string
				in_raw_string = true;
				raw_string_ending = false;
				break;
			}
			
			// keywords don't matter for advancing the state
			if (char_types && !in_single_line_comment && !in_multi_line_comment && !in_number && !in_string && !in_preprocessor && !in_char) {
				u32 keyword_len = syntax_keyword_len(cpp ? LANG_CPP : LANG_C, line, i, line_len);
				Keyword const *keyword = NULL;
				if (cpp)
					keyword = syntax_keyword_lookup(syntax_all_keywords_cpp, arr_count(syntax_all_keywords_cpp),
						&line[i], keyword_len);
				if (!keyword)
					keyword = syntax_keyword_lookup(syntax_all_keywords_c, arr_count(syntax_all_keywords_c),
					&line[i], keyword_len);
				
				if (keyword) {
					SyntaxCharType type = keyword->type;
					for (size_t j = 0; j < keyword_len; ++j) {
						char_types[i++] = type;
					}
					--i; // we'll increment i from the for loop
					dealt_with = true;
					break;
				}
			}
		} break;
		}
		if (c != '\\') backslashes = 0;
		if (in_number && !syntax_number_continues(line, line_len, i)) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_single_line_comment || in_multi_line_comment)
				type = SYNTAX_COMMENT;
			else if (in_string)
				type = SYNTAX_STRING;
			else if (in_char)
				type = SYNTAX_CHARACTER;
			else if (in_number)
				type = SYNTAX_CONSTANT;
			else if (in_preprocessor)
				type = SYNTAX_PREPROCESSOR;

			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		  ((backslashes && in_single_line_comment) * SYNTAX_STATE_CPP_SINGLE_LINE_COMMENT)
		| ((backslashes && in_preprocessor) * SYNTAX_STATE_CPP_PREPROCESSOR)
		| ((backslashes && in_string) * SYNTAX_STATE_CPP_STRING)
		| (in_multi_line_comment * SYNTAX_STATE_CPP_MULTI_LINE_COMMENT)
		| (in_raw_string * SYNTAX_STATE_CPP_RAW_STRING)
	);
}

static void syntax_highlight_rust(SyntaxState *state, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	u32 comment_depth = (((u32)*state & SYNTAX_STATE_RUST_COMMENT_DEPTH_MASK) / SYNTAX_STATE_RUST_COMMENT_DEPTH_MUL);
	bool in_string = (*state & SYNTAX_STATE_RUST_STRING) != 0;
	bool string_is_raw = (*state & SYNTAX_STATE_RUST_STRING_IS_RAW) != 0;
	bool in_number = false;
	uint backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool dealt_with = false;
		bool has_1_char = i + 1 < line_len;
		bool has_2_chars = i + 2 < line_len;
		
		switch (c) {
		case '/':
			if (!in_string) {
				if (i && line[i-1] == '*') {
					// */
					if (comment_depth)
						--comment_depth;
					if (char_types) {
						char_types[i] = SYNTAX_COMMENT;
						dealt_with = true;
					}
				} else if (has_1_char && line[i+1] == '*') {
					// /*
					++comment_depth;
				} else if (!comment_depth && has_1_char && line[i+1] == '/') {
					// //
					// just handle it all now
					if (char_types) {
						for (u32 j = i; j < line_len; ++j)
							char_types[j] = SYNTAX_COMMENT;
					}
					i = line_len - 1;
					dealt_with = true;
					break;
				}
			}
			break;
		case '"':
			if (!comment_depth) {
				if (in_string) {
					if (backslashes % 2 == 0) {
						if (!string_is_raw || (has_1_char && line[i+1] == '#')) {
							// end of string literal
							in_string = false;
							if (char_types) {
								char_types[i] = SYNTAX_STRING;
								dealt_with = true;
							}
							string_is_raw = false;
						}
					}
				} else {
					// start of string literal
					in_string = true;
					if (i && line[i-1] == '#')
						string_is_raw = true;
				}
			}
			break;
		case '\'': {
			if (!comment_depth && !in_string && has_2_chars) {
				// figure out if this is a character or a lifetime
				u32 char_end;
				backslashes = line[i+1] == '\\';
				for (char_end = i + 2; char_end < line_len; ++char_end) {
					if (line[char_end] == '\'' && backslashes % 2 == 0) {
						break;
					}
					if (line[char_end] < CHAR_MAX
					&& line[char_end - 1] != '\\'
					&& !strchr("abcdefABCDEF0123456789", (char)line[char_end]))
						break;
				}
				if (char_end < line_len && line[char_end] == '\'') {
					// a character literal
					if (char_types) {
						for (u32 j = i; j <= char_end; ++j)
							char_types[j] = SYNTAX_CHARACTER;
						dealt_with = true;
					}
					i = char_end;
				} else {
					// a lifetime or something else
				}
			}
		} break;
		case '\\':
			++backslashes;
			break;
		case ANY_DIGIT:
			// a number!
			if (char_types && !comment_depth && !in_string && !in_number) {
				in_number = true;
				if (i && (is32_ident(line[i - 1])
					|| (line[i-1] == '.' && !(i >= 2 && line[i-2] == '.')))
				) {
					// actually, this isn't a number. it's something like a*6* or u3*2*.
					// also, don't highlight the 0 in tuple.0
					in_number = false;
				}
			}
			break;
		default: {
			if ((i && is32_ident(line[i - 1])) || !is32_ident(c))
				break; // can't be a keyword on its own.
			
			if (char_types && !in_string && !comment_depth && !in_number) {
				u32 keyword_len = syntax_keyword_len(LANG_RUST, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_rust, arr_count(syntax_all_keywords_rust),
					&line[i], keyword_len);
				if (keyword) {
					SyntaxCharType type = keyword->type;
					for (size_t j = 0; j < keyword_len; ++j) {
						char_types[i++] = type;
					}
					--i; // we'll increment i from the for loop
					dealt_with = true;
					break;
				}
			}
		} break;
		}
		if (c != '\\') backslashes = 0;
		if (in_number && !syntax_number_continues(line, line_len, i))
			in_number = false;
		
		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (comment_depth) {
				type = SYNTAX_COMMENT;
			} else if (in_string) {
				type = SYNTAX_STRING;
			} else if (in_number) {
				type = SYNTAX_CONSTANT;
			}
			char_types[i] = type;
		}
		
	}
	
	u32 max_comment_depth = ((u32)1<<SYNTAX_STATE_RUST_COMMENT_DEPTH_BITS);	
	if (comment_depth >= max_comment_depth)
		comment_depth = max_comment_depth;
	*state = (SyntaxState)(
		  (comment_depth * SYNTAX_STATE_RUST_COMMENT_DEPTH_MUL)
		| (in_string * SYNTAX_STATE_RUST_STRING)
		| (string_is_raw * SYNTAX_STATE_RUST_STRING_IS_RAW)
	);
}

static void syntax_highlight_python(SyntaxState *state, char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	(void)state;
	bool in_string = (*state & SYNTAX_STATE_PYTHON_STRING) != 0;
	bool string_is_dbl_quoted = (*state & SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED) != 0;
	bool string_is_multiline = true;
	bool in_number = false;
	uint backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool dealt_with = false;
		switch (c) {
		case '#':
			if (!in_string) {
				// comment
				if (char_types) {
					for (u32 j = i; j < line_len; ++j)
						char_types[j] = SYNTAX_COMMENT;
					dealt_with = true;
				}
				i = line_len - 1;
			}
			break;
		case '\'':
		case '"': {
			bool dbl_quoted = c == '"';
			bool is_triple = i < line_len - 2 &&
				line[i+1] == c && line[i+2] == c;
			if (in_string) {
				if (!string_is_multiline || is_triple) {
					// end of string
					if (string_is_dbl_quoted == dbl_quoted && backslashes % 2 == 0) {
						in_string = false;
						if (char_types) {
							char_types[i] = SYNTAX_STRING;
							if (string_is_multiline) {
								// highlight all three ending quotes
								char_types[++i] = SYNTAX_STRING;
								char_types[++i] = SYNTAX_STRING;
							}
							dealt_with = true;
						}
					}
				}
			} else {
				// start of string
				string_is_dbl_quoted = dbl_quoted;
				in_string = true;
				string_is_multiline = is_triple;
			}
		} break;
		case ANY_DIGIT:
			if (char_types && !in_string && !in_number) {
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
		case '\\':
			++backslashes;
			break;
		default:
			if ((i && is32_ident(line[i - 1])) || !is32_ident(c))
				break; // can't be a keyword on its own.
			
			if (char_types && !in_string && !in_number) {
				u32 keyword_len = syntax_keyword_len(LANG_PYTHON, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_python, arr_count(syntax_all_keywords_python),
					&line[i], keyword_len);
				if (keyword) {
					SyntaxCharType type = keyword->type;
					for (size_t j = 0; j < keyword_len; ++j) {
						char_types[i++] = type;
					}
					--i; // we'll increment i from the for loop
					dealt_with = true;
					break;
				}
			}
			break;
		}
		if (c != '\\') backslashes = 0;
		if (in_number && !syntax_number_continues(line, line_len, i))
			in_number = false;
		
		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_string)
				type = SYNTAX_STRING;
			else if (in_number)
				type = SYNTAX_CONSTANT;
			char_types[i] = type;
		}
	}
	*state = 0;
	if (in_string && string_is_multiline) {
		*state |= SYNTAX_STATE_PYTHON_STRING
			| (SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED * string_is_dbl_quoted);
	}
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
	case LANG_RUST:
		syntax_highlight_rust(state, line, line_len, char_types);
		break;
	case LANG_PYTHON:
		syntax_highlight_python(state, line, line_len, char_types);
		break;
	case LANG_COUNT: assert(0); break;
	}
}
