#include "keywords.h"

// all characters that can appear in a number
#define SYNTAX_DIGITS "0123456789.xXoObBlLuUiIabcdefABCDEF_"


// returns the language this string is referring to, or LANG_NONE if it's invalid.
Language language_from_str(char const *str) {
	for (int i = 0; i < LANG_COUNT; ++i) {
		if (strcmp_case_insensitive(language_names[i].name, str) == 0)
			return language_names[i].lang;
	}
	return LANG_NONE;
}

const char *language_to_str(Language language) {
	for (int i = 0; i < LANG_COUNT; ++i) {
		if (language_names[i].lang == language)
			return language_names[i].name;
	}
	return "???";

}

// start of single line comment for language l -- used for comment/uncomment selection
char const *language_comment_start(Language l) {
	switch (l) {
	case LANG_C:
	case LANG_RUST:
	case LANG_CPP:
	case LANG_JAVASCRIPT:
	case LANG_JAVA:
	case LANG_GO:
		return "// ";
	case LANG_CONFIG:
	case LANG_TED_CFG:
	case LANG_PYTHON:
		return "# ";
	case LANG_TEX:
		return "% ";
	case LANG_HTML:
		return "<!-- ";
	case LANG_NONE:
	case LANG_MARKDOWN:
	case LANG_COUNT:
		break;
	}
	return "";
}

// end of single line comment for language l
char const *language_comment_end(Language l) {
	switch (l) {
	case LANG_HTML:
		return " -->";
	default:
		return "";
	}
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

static inline bool syntax_keyword_matches(char32_t const *text, size_t len, char const *keyword) {
	if (len == strlen(keyword)) {
		bool matches = true;
		char32_t const *p = text;
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

// returns ')' for '(', etc., or 0 if c is not an opening bracket
char32_t syntax_matching_bracket(Language lang, char32_t c) {
	(void)lang; // not needed yet
	switch (c) {
	case '(': return ')';
	case ')': return '(';
	case '[': return ']';
	case ']': return '[';
	case '{': return '}';
	case '}': return '{';
	}
	return 0;
}

// returns true for opening brackets, false for closing brackets/non-brackets
bool syntax_is_opening_bracket(Language lang, char32_t c) {
	(void)lang;
	switch (c) {
	case '(':
	case '[':
	case '{':
		return true;
	}
	return false;
}

// lookup the given string in the keywords table
static Keyword const *syntax_keyword_lookup(Keyword const *const *all_keywords, size_t n_all_keywords, char32_t const *str, size_t len) {
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
static inline bool syntax_number_continues(Language lang, char32_t const *line, u32 line_len, u32 i) {
	if (line[i] == '.') {
		if ((i && line[i-1] == '.') || (i < line_len-1 && line[i+1] == '.'))
			return false; // can't have two .s in a row
		if (i < line_len-1 && lang == LANG_RUST && !isdigit(line[i+1]) && line[i+1] != '_') {
			// don't highlight 0.into() weirdly
			// (in Rust, only 0123456789_ can follow a decimal point)
			return false;
		}
	}
	return (line[i] < CHAR_MAX && 
		(strchr(SYNTAX_DIGITS, (char)line[i])
		|| (i && line[i-1] == 'e' && (line[i] == '+' || line[i] == '-'))));
}

static bool is_keyword(Language lang, char32_t c) {
	if (c == '_' && lang == LANG_TEX) return false;
	if (is32_ident(c)) return true;
	switch (lang) {
	case LANG_RUST:
		// Rust builtin macros
		if (c == '!')
			return true;
		break;
	case LANG_HTML:
		if (c == '-' || c == '=')
			return true;
		break;
	default: break;
	}
	return false;
}

// find how long this keyword would be (if this is a keyword)
static inline u32 syntax_keyword_len(Language lang, char32_t const *line, u32 i, u32 line_len) {
	u32 keyword_end;
	for (keyword_end = i; keyword_end < line_len; ++keyword_end) {
		if (!is_keyword(lang, line[keyword_end]))
			break;
	}
	return keyword_end - i;
}	

static void syntax_highlight_c_cpp(SyntaxState *state_ptr, bool cpp, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
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
				if (i == 0 || !is32_digit(line[i-1])) // in C++, you can use ' as a separator, e.g. 1'000'000
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
		if (in_number && !syntax_number_continues(LANG_CPP, line, line_len, i)) {
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

static void syntax_highlight_rust(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	u32 comment_depth = (((u32)*state & SYNTAX_STATE_RUST_COMMENT_DEPTH_MASK) / SYNTAX_STATE_RUST_COMMENT_DEPTH_MUL);
	bool in_string = (*state & SYNTAX_STATE_RUST_STRING) != 0;
	bool string_is_raw = (*state & SYNTAX_STATE_RUST_STRING_IS_RAW) != 0;
	bool in_number = false;
	bool in_attribute = false;
	int backslashes = 0;
	int bracket_depth = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool dealt_with = false;
		bool has_1_char = i + 1 < line_len;
		bool has_2_chars = i + 2 < line_len;
		bool has_3_chars = i + 3 < line_len;
		
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
		case 'r':
			if (char_types && !comment_depth) {
				if (has_2_chars && line[i+1] == '#' && line[i+2] == '"') {
					// r before raw string
					char_types[i] = SYNTAX_STRING;
					dealt_with = true;
				}
			}
			goto keyword_check;
		case 'b':
			if (char_types && !comment_depth) {
				if ((has_1_char && line[i+1] == '"')
					|| (has_3_chars && line[i+1] == 'r' && line[i+2] == '#' && line[i+3] == '"')) {
					// b before byte string
					char_types[i] = SYNTAX_STRING;
					dealt_with = true;
				}
				if (has_1_char && line[i+1] == '\'') {
					// b before byte char
					char_types[i] = SYNTAX_CHARACTER;
					dealt_with = true;
				}
			}
			goto keyword_check;
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
								if (string_is_raw && has_1_char) {
									// highlighting for final #
									++i;
									char_types[i] = SYNTAX_STRING;
								}
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
					if (line[char_end] == '\\')
						++backslashes;
					else
						backslashes = 0;
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
		case '[':
			if (in_attribute && !in_string && !comment_depth) {
				++bracket_depth;
			}
			break;
		case ']':
			if (in_attribute && !in_string && !comment_depth) {
				--bracket_depth;
				if (bracket_depth < 0) {
					in_attribute = false;
				}
			}
			break;
		case '#':
			if (char_types && !in_string && !comment_depth) {
				if (i && line[i-1] == 'r') {
					if (has_1_char && line[i+1] == '"') {
						// raw string
						char_types[i] = SYNTAX_STRING;
						dealt_with = true;
					} else {
						// raw identifier
					}
					break;
				}
				if (!has_2_chars) break;
				if (line[i+1] == '[' || (line[i+1] == '!' && line[i+2] == '[')) {
					in_attribute = true;
					bracket_depth = 0;
				}
			}
			break;
		default:
		keyword_check: {
			if ((i && is32_ident(line[i - 1])) || !is32_ident(c))
				break; // can't be a keyword on its own.
			if (i >= 2 && line[i-2] == 'r' && line[i-1] == '#') {
				// raw identifier
				break;
			}
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
		if (in_number && !syntax_number_continues(LANG_RUST, line, line_len, i))
			in_number = false;
		
		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (comment_depth) {
				type = SYNTAX_COMMENT;
			} else if (in_string) {
				type = SYNTAX_STRING;
			} else if (in_number) {
				type = SYNTAX_CONSTANT;
			} else if (in_attribute) {
				type = SYNTAX_PREPROCESSOR;
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

static void syntax_highlight_python(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
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
		case 'f':
		case 'r':
		case 'b':
			if (char_types && i+1 < line_len && (line[i+1] == '\'' || line[i+1] == '"')) {
				// format/raw/byte string
				// @TODO(eventually): we don't handle raw string highlighting correctly.
				char_types[i] = SYNTAX_STRING;
				dealt_with = true;
			}
			goto keyword_check;
		case '\'':
		case '"': {
			bool dbl_quoted = c == '"';
			bool is_triple = i+2 < line_len &&
				line[i+1] == c && line[i+2] == c;
			if (in_string) {
				if (!string_is_multiline || is_triple) {
					if (string_is_dbl_quoted == dbl_quoted && backslashes % 2 == 0) {
						// end of string
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
		keyword_check:
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
		if (in_number && !syntax_number_continues(LANG_PYTHON, line, line_len, i))
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
		*state |= (SyntaxState)(
			SYNTAX_STATE_PYTHON_STRING
			| (SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED * string_is_dbl_quoted)
		);
	}
}

static bool is_tex_ident(char32_t c) {
	// digits and underscores cannot appear in tex identifiers
	return is32_ident(c) && !is32_digit(c) && c != '_';
}

static void syntax_highlight_tex(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	bool dollar = (*state & SYNTAX_STATE_TEX_DOLLAR) != 0;
	bool dollardollar = (*state & SYNTAX_STATE_TEX_DOLLARDOLLAR) != 0;
	bool verbatim = (*state & SYNTAX_STATE_TEX_VERBATIM) != 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool has_1_char = i + 1 < line_len;
		
		if (char_types)
			char_types[i] = dollar || dollardollar ? SYNTAX_MATH : SYNTAX_NORMAL;
		switch (c) {
		case '\\':
			if (has_1_char) {
				if (is32_graph(line[i+1])) {
					if (is_tex_ident(line[i+1])) {
						// command, e.g. \begin
						String32 command_str = {
							.str = (char32_t *)line + i+1,
							.len = line_len - (i+1),
						};
						bool new_verbatim = false;
						if (!dollar && !dollardollar) {
							if (!verbatim && str32_has_ascii_prefix(command_str, "begin{verbatim}")) {
								new_verbatim = true;
							} else if (verbatim && str32_has_ascii_prefix(command_str, "end{verbatim}")) {
								verbatim = false;
							}
						}
						
						if (!verbatim) {
							if (char_types) char_types[i] = SYNTAX_KEYWORD;
							for (++i; i < line_len; ++i) {
								if (is_tex_ident(line[i])) {
									if (char_types) char_types[i] = SYNTAX_KEYWORD;
								} else {
									--i;
									break;
								}
							}
							verbatim = new_verbatim;
						}
					} else if (!verbatim) {
						// something like \\, \%, etc.
						if (char_types) char_types[i] = SYNTAX_KEYWORD;
						++i;
						if (char_types) char_types[i] = SYNTAX_KEYWORD;
					}
				}
			}
			break;
		case '%':
			// comment
			if (!verbatim) {
				for (; i < line_len; ++i) {
					if (char_types)
						char_types[i] = SYNTAX_COMMENT;
				}
			}
			break;
		case '&':
			// table/matrix/etc. separator
			if (char_types && !verbatim)
				char_types[i] = SYNTAX_BUILTIN;
			break;
		case '$':
			if (!verbatim) {
				if (!dollar && has_1_char && line[i+1] == '$') {
					// $$
					if (dollardollar) {
						if (char_types) char_types[i] = SYNTAX_MATH;
						++i;
						if (char_types) char_types[i] = SYNTAX_MATH;
						dollardollar = false;
					} else {
						if (char_types) char_types[i] = SYNTAX_MATH;
						dollardollar = true;
					}
				} else if (!dollardollar) {
					// single $
					if (dollar) {
						dollar = false;
					} else {
						dollar = true;
						if (char_types) char_types[i] = SYNTAX_MATH;
					}
				}
			}
			break;
		}
	}
	
	*state = (SyntaxState)(
		(dollar * SYNTAX_STATE_TEX_DOLLAR)
		| (dollardollar * SYNTAX_STATE_TEX_DOLLARDOLLAR)
		| (verbatim * SYNTAX_STATE_TEX_VERBATIM)
	);
}

static void syntax_highlight_markdown(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	bool multiline_code = (*state & SYNTAX_STATE_MARKDOWN_CODE) != 0;
	
	*state = (multiline_code * SYNTAX_STATE_MARKDOWN_CODE);
	
	if (line_len >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
		if (multiline_code) {
			// end of multi-line code
			*state = 0;
		} else {
			// start of multi-line code
			multiline_code = true;
			*state = SYNTAX_STATE_MARKDOWN_CODE;
		}
	}

	if (!char_types) {
		return;
	}

	if (multiline_code) {
		static_assert_if_possible(sizeof *char_types == 1) // NOTE: memset is used extensively in this file this way
		memset(char_types, SYNTAX_CODE, line_len);
		return;
	}
	
	bool start_of_line = true; // is this the start of the line (not counting whitespace)
	int backslashes = 0;
	char const *format_ending = NULL; // "**" if we are inside **bold**, etc.
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool next_sol = start_of_line && is32_space(c);
		bool has_1_char = i+1 < line_len;
		bool next_is_space = has_1_char && is32_space(line[i+1]);
		
		char_types[i] = SYNTAX_NORMAL;
		if (format_ending) {
			if (streq(format_ending, "`"))
				char_types[i] = SYNTAX_CODE;
			else
				char_types[i] = SYNTAX_STRING;
		}
		
		String32 remains = {
			.str = (char32_t *)line + i,
			.len = line_len - i
		};
		if (!format_ending && str32_has_ascii_prefix(remains, "http")) {
			if (str32_has_ascii_prefix(remains, "http://")
				|| str32_has_ascii_prefix(remains, "https://")) {
				// a link!
				for (; i < line_len; ++i) {
					if (is32_space(line[i]))
						break;
					char_types[i] = SYNTAX_LINK;
				}
				if (line[i-1] < 128 && strchr(".!,", (char)line[i-1])) {
					// punctuation after URLs
					char_types[i-1] = SYNTAX_NORMAL;
				}
				goto bottom;
			}
		}
		
		switch (c) {
		case '#':
			if (start_of_line) {
				memset(char_types + i, SYNTAX_STRING, line_len - i);
				i = line_len;
			}
			break;
		case '*':
			if (start_of_line && next_is_space) {
				// bullet list item
				char_types[i] = SYNTAX_BUILTIN;
			}
			FALLTHROUGH
		case '_':
			if (backslashes % 2 == 1) {
				// \* or \_
			} else if (has_1_char && line[i+1] == c) {
				// **bold** or __bold__
				char const *end = c == '*' ? "**" : "__";
				if (format_ending) {
					if (streq(format_ending, end)) {
						char_types[i++] = SYNTAX_STRING;
						char_types[i] = SYNTAX_STRING;
						format_ending = NULL;
					}
				} else if (!next_is_space) {
					char_types[i++] = SYNTAX_STRING;
					char_types[i] = SYNTAX_STRING;
					format_ending = end;
				}
			} else {
				// *italics* or _italics_
				char const *end = c == '*' ? "*" : "_";
				if (format_ending) {
					if (streq(format_ending, end))
						format_ending = NULL;
				} else if (!next_is_space) {
					char_types[i] = SYNTAX_STRING;
					format_ending = end;
				}
			}
			break;
		case '`':
			if (backslashes % 2 == 1) {
				// \`
			} else if (format_ending) {
				if (streq(format_ending, "`"))
					format_ending = NULL;
			} else {
				char_types[i] = SYNTAX_CODE;
				format_ending = "`";
			}
			break;
		case '-':
		case '>':
			if (start_of_line && next_is_space) {
				// list item/blockquote
				char_types[i] = SYNTAX_BUILTIN;
			}
			break;
		case ANY_DIGIT:
			if (start_of_line) {
				size_t spn = str32_ascii_spn(remains, "0123456789");
				size_t end = i + spn;
				if (end < line_len && line[end] == '.') {
					// numbered list item
					for (; i <= end; ++i) {
						char_types[i] = SYNTAX_BUILTIN;
					}
				}
			}
			break;
		case '[': {
			if (backslashes % 2 == 0) {
				// [URLS](like-this.com)
				u32 j;
				for (j = i+1; j < line_len; ++j) {
					if (line[j] == ']' && backslashes % 2 == 0)
						break;
					if (line[j] == '\\')
						++backslashes;
					else
						backslashes = 0;
				}
				backslashes = 0;
				u32 closing_bracket = j;
				if (closing_bracket+2 < line_len && line[closing_bracket+1] == '(') {
					for (j = closing_bracket+2; j < line_len; ++j) {
						if (line[j] == ')' && backslashes % 2 == 0)
							break;
						if (line[j] == '\\')
							++backslashes;
						else
							backslashes = 0;
					}
					u32 closing_parenthesis = j;
					if (closing_parenthesis < line_len) {
						// hooray!
						if (i > 0 && line[i-1] == '!')
							--i; // images are links, but with ! before them
						memset(&char_types[i], SYNTAX_LINK, closing_parenthesis+1 - i);
						i = closing_parenthesis;
					}
					backslashes = 0;
					
				}
			}
		} break;
		}
	bottom:
		if (i >= line_len) break;
		
		if (line[i] != '\\')
			backslashes = 0;
		else
			++backslashes;
		
		start_of_line = next_sol;
	}
	
}

static bool is_html_tag_char(char32_t c) {
	return c == '<' || c == '/' || c == '!' || c == ':' || is32_alnum(c);
}

static void syntax_highlight_html(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	bool comment = (*state & SYNTAX_STATE_HTML_COMMENT) != 0;
	bool in_sgl_string = false; // 'string'
	bool in_dbl_string = false; // "string"
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {
		String32 remains = {
			.str = (char32_t *)line + i,
			.len = line_len - i
		};
		bool has_1_char = i + 1 < line_len;
		
		if (comment) {
			if (str32_has_ascii_prefix(remains, "-->")) {
				if (char_types)
					memset(&char_types[i], SYNTAX_COMMENT, 3);
				i += 2;
				// (don't worry, comments can't nest in HTML)
				comment = false;
			} else {
				if (char_types) char_types[i] = SYNTAX_COMMENT;
			}
		} else if (!in_sgl_string && !in_dbl_string && str32_has_ascii_prefix(remains, "<!--")) {
			comment = true;
			if (char_types) char_types[i] = SYNTAX_COMMENT;
		} else if (in_sgl_string || in_dbl_string) {
			if (char_types)
				char_types[i] = SYNTAX_STRING;
			if (line[i] == (char32_t)(in_sgl_string ? '\'' : '"') && backslashes % 2 == 0)
				in_sgl_string = in_dbl_string = false;
		} else {
			if (char_types) char_types[i] = SYNTAX_NORMAL;
			switch (line[i]) {
			case '"':
				if (i > 0 && line[i-1] == '=') {
					in_dbl_string = true;
					if (char_types)
						char_types[i] = SYNTAX_STRING;
				}
				break;
			case '\'':
				if (i > 0 && line[i-1] == '=') {
					in_sgl_string = true;
					if (char_types)
						char_types[i] = SYNTAX_STRING;
				}
				break;
			case '&':
				for (; i < line_len; ++i) {
					if (char_types)
						char_types[i] = SYNTAX_BUILTIN;
					if (line[i] == ';')
						break;
				}
				break;
			case '<':
				if (has_1_char && is_html_tag_char(line[i+1])) {
					for (; i < line_len; ++i) {
						if (!is_html_tag_char(line[i])) {
							--i;
							break;
						}
						if (char_types)
							char_types[i] = SYNTAX_KEYWORD;
					}
				}
				break;
			case '>':
				if (char_types) {
					// we want to check if the character before it is a space so that
					// > in JavaScript/PHP doesn't get picked up as a "tag".
					if (i > 0 && !is32_space(line[i-1])) {
						char_types[i] = SYNTAX_KEYWORD;
						if (line[i-1] == '/') // tags like <thing+ />
							char_types[i-1] = SYNTAX_KEYWORD;
					}
				}
				break;
			default:
				if (char_types) {
				
					if ((i && is32_ident(line[i - 1])) || !is32_ident(line[i]))
						break; // can't be a keyword on its own.
					
					u32 keyword_len = syntax_keyword_len(LANG_HTML, line, i, line_len);
					Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_html, arr_count(syntax_all_keywords_html),
						&line[i], keyword_len);
					if (keyword) {
						SyntaxCharType type = keyword->type;
						for (size_t j = 0; j < keyword_len; ++j) {
							char_types[i++] = type;
						}
						--i; // we'll increment i from the for loop
						break;
					}
				}
				break;
			}
		}
		if (i < line_len) {
			if (line[i] == '\\')
				++backslashes;
			else
				backslashes = 0;
		}
	}
	
	*state = (comment * SYNTAX_STATE_HTML_COMMENT);
}

static void syntax_highlight_config(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types, bool is_ted_cfg) {
	bool string = (*state & SYNTAX_STATE_TED_CFG_STRING) != 0;
	
	if (line_len == 0) return;
	
	if (!string && line[0] == '#') {
		if (char_types) memset(char_types, SYNTAX_COMMENT, line_len);
		return;
	}
	if (!string && line[0] == '[' && line[line_len - 1] == ']') {
		if (char_types) memset(char_types, SYNTAX_BUILTIN, line_len);
		return;
	}
	
	int backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		if (char_types)
			char_types[i] = string ? SYNTAX_STRING : SYNTAX_NORMAL;
		switch (line[i]) {
		case '"':
			if (string && backslashes % 2 == 0) {
				string = false;
			} else {
				string = true;
			}
			if (char_types)
				char_types[i] = SYNTAX_STRING;
			break;
		case '#':
			// don't try highlighting the rest of the line.
			// for ted.cfg, this could be a color, but for other cfg files,
			// it might be a comment
			if (char_types)
				memset(&char_types[i], 0, line_len - i);
			i = line_len;
			break;
		case ANY_DIGIT:
			if (char_types && i > 0 && !string) {
				if (is32_ident(line[i-1]) // something like e5
					|| line[i-1] == '+') // key combinations, e.g. Alt+0
					break;
				while (i < line_len && syntax_number_continues(LANG_CONFIG, line, line_len, i)) {
					char_types[i++] = SYNTAX_CONSTANT;
				}
			}
			break;
		default: {
			if (!char_types)
				break; // don't care
			if (i == 0) // none of the keywords in syntax_all_keywords_config should appear at the start of the line
				break;
			if (is32_ident(line[i-1]) || line[i-1] == '-' || !is32_ident(line[i]))
				break; // can't be a keyword on its own.
			u32 keyword_len = syntax_keyword_len(LANG_CONFIG, line, i, line_len);
			Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_config, arr_count(syntax_all_keywords_config),
				&line[i], keyword_len);
			if (keyword) {
				SyntaxCharType type = keyword->type;
				for (size_t j = 0; j < keyword_len; ++j) {
					char_types[i++] = type;
				}
				--i; // we'll increment i from the for loop
				break;
			}
		} break;
		}
		if (i < line_len) {
			if (line[i] == '\\')
				++backslashes;
			else
				backslashes = 0;
		}
	}
	
	if (is_ted_cfg) {
		*state = SYNTAX_STATE_TED_CFG_STRING * string;
	}
}

static void syntax_highlight_javascript(SyntaxState *state, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	(void)state;
	bool string_is_template = (*state & SYNTAX_STATE_JAVASCRIPT_TEMPLATE_STRING) != 0;
	bool in_multiline_comment = (*state & SYNTAX_STATE_JAVASCRIPT_MULTILINE_COMMENT) != 0;
	bool string_is_dbl_quoted = false;
	bool string_is_regex = false;
	bool in_number = false;
	bool in_string = string_is_template;
	uint backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool dealt_with = false;
		switch (c) {
		case '/':
			if (!in_string) {
				if (i > 0 && in_multiline_comment) {
					if (line[i-1] == '*') {
						// end of multi line comment
						in_multiline_comment = false;
						if (char_types) char_types[i] = SYNTAX_COMMENT;
						dealt_with = true;
					}
				}
				if (!dealt_with && i+1 < line_len) {
					if (line[i+1] == '/') {
						// single line comment
						if (char_types) memset(&char_types[i], SYNTAX_COMMENT, line_len - i);
						i = line_len - 1;
						dealt_with = true;
					} else if (line[i+1] == '*') {
						// multi line comment
						in_multiline_comment = true;
						if (char_types) char_types[i] = SYNTAX_COMMENT;
						dealt_with = true;
					}
				}
				if (!dealt_with && !in_multiline_comment && !in_string) {
					// this is not foolproof for detecting regex literals
					//  but should handle all "reasonable" uses of regex.
					bool is_regex = i == 0 // slash is first char in line
						|| (line[i-1] <= WCHAR_MAX && iswspace((wint_t)line[i-1])) // slash preceded by space
						|| (line[i-1] <= 128 && strchr(";({[=,:", (char)line[i-1])); // slash preceded by any of these characters
					if (is_regex) {
						in_string = true;
						string_is_regex = true;
					}
				}
			} else if (in_string && string_is_regex) {
				if (backslashes % 2 == 0) {
					// end of regex literal
					if (char_types) {
						char_types[i] = SYNTAX_STRING;
						++i;
						// flags
						for (; i < line_len; ++i) {
							if (line[i] >= 128 || !strchr("dgimsuy", (char)line[i]))
								break;
							char_types[i] = SYNTAX_STRING;	
						}
						--i; // back to last char in flags
					}
					dealt_with = true;
					in_string = false;
					string_is_regex = false;
				}
			}
			break;
		case '\'':
		case '"':
		case '`': {
			if (!in_multiline_comment) {
				bool dbl_quoted = c == '"';
				bool template = c == '`';
				if (in_string) {
					if (!string_is_regex && backslashes % 2 == 0
						&& string_is_dbl_quoted == dbl_quoted && string_is_template == template) {
						// end of string
						in_string = false;
						if (char_types) char_types[i] = SYNTAX_STRING;
						dealt_with = true;
					}
				} else {
					// start of string
					string_is_dbl_quoted = dbl_quoted;
					string_is_template = template;
					in_string = true;
				}
			}
		} break;
		case ANY_DIGIT:
			if (char_types && !in_string && !in_number && !in_multiline_comment) {
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
			
			if (char_types && !in_string && !in_number && !in_multiline_comment) {
				u32 keyword_len = syntax_keyword_len(LANG_JAVASCRIPT, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_javascript, arr_count(syntax_all_keywords_javascript),
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
		if (in_number && !syntax_number_continues(LANG_JAVASCRIPT, line, line_len, i))
			in_number = false;
		
		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_multiline_comment)
				type = SYNTAX_COMMENT;
			else if (in_string)
				type = SYNTAX_STRING;
			else if (in_number)
				type = SYNTAX_CONSTANT;
			char_types[i] = type;
		}
	}
	*state = 0;
	if (in_string && string_is_template)
		*state |= SYNTAX_STATE_JAVASCRIPT_TEMPLATE_STRING;
	if (in_multiline_comment)
		*state |= SYNTAX_STATE_JAVASCRIPT_MULTILINE_COMMENT;
}

static void syntax_highlight_java(SyntaxState *state_ptr, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool in_string = false;
	bool in_multiline_comment = (state & SYNTAX_STATE_JAVA_MULTILINE_COMMENT) != 0;
	bool in_char = false;
	bool in_number = false;
	
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {

		// are there 1/2 characters left in the line?
		bool has_1_char =  i + 1 < line_len;
		
		bool dealt_with = false;
		
		char32_t c = line[i];
		
		switch (c) {
		case '\\':
			++backslashes;
			break;
		case '/':
			if (!in_multiline_comment && !in_string && !in_char && has_1_char) {
				if (line[i + 1] == '/') {
						if (char_types) memset(&char_types[i], SYNTAX_COMMENT, line_len - i);
						i = line_len - 1;
						dealt_with = true;
				} else if (line[i + 1] == '*') {
					in_multiline_comment = true; // /*
				}
			} else if (in_multiline_comment) {
				if (i > 0 && line[i - 1] == '*' && in_multiline_comment) {
					// */
					in_multiline_comment = false;
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
			} else if (!in_multiline_comment && !in_char) {
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
			} else if (!in_multiline_comment && !in_string) {
				in_char = true;
			}
			break;
		case ANY_DIGIT:
			// a number!
			if (char_types && !in_multiline_comment && !in_string && !in_number && !in_char) {
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
			if (char_types && !in_multiline_comment && !in_number && !in_string && !in_char) {
				u32 keyword_len = syntax_keyword_len(LANG_JAVA, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_java, arr_count(syntax_all_keywords_java),
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
		if (in_number && !syntax_number_continues(LANG_JAVA, line, line_len, i)) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_multiline_comment)
				type = SYNTAX_COMMENT;
			else if (in_string)
				type = SYNTAX_STRING;
			else if (in_char)
				type = SYNTAX_CHARACTER;
			else if (in_number)
				type = SYNTAX_CONSTANT;

			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		(in_multiline_comment * SYNTAX_STATE_JAVA_MULTILINE_COMMENT)
	);
}


static void syntax_highlight_go(SyntaxState *state_ptr, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool string_is_raw = (state & SYNTAX_STATE_GO_RAW_STRING) != 0;
	bool in_string = string_is_raw;
	bool in_multiline_comment = (state & SYNTAX_STATE_GO_MULTILINE_COMMENT) != 0;
	bool in_char = false;
	bool in_number = false;
	
	int backslashes = 0;
	for (u32 i = 0; i < line_len; ++i) {

		// are there 1/2 characters left in the line?
		bool has_1_char =  i + 1 < line_len;
		
		bool dealt_with = false;
		
		char32_t c = line[i];
		
		switch (c) {
		case '\\':
			++backslashes;
			break;
		case '/':
			if (!in_multiline_comment && !in_string && !in_char && has_1_char) {
				if (line[i + 1] == '/') {
						if (char_types) memset(&char_types[i], SYNTAX_COMMENT, line_len - i);
						i = line_len - 1;
						dealt_with = true;
				} else if (line[i + 1] == '*') {
					in_multiline_comment = true; // /*
				}
			} else if (in_multiline_comment) {
				if (i > 0 && line[i - 1] == '*' && in_multiline_comment) {
					// */
					in_multiline_comment = false;
					if (char_types) {
						dealt_with = true;
						char_types[i] = SYNTAX_COMMENT;
					}
				}
			}
			break;
		case '"':
			if (in_string && !string_is_raw && backslashes % 2 == 0) {
				in_string = false;
				if (char_types) {
					dealt_with = true;
					char_types[i] = SYNTAX_STRING;
				}
			} else if (!in_multiline_comment && !in_char) {
				in_string = true;
				string_is_raw = false;
			}
			break;
		case '`':
			if (in_string && string_is_raw) {
				// end of raw string
				in_string = false;
				string_is_raw = false;
				if (char_types) char_types[i] = SYNTAX_STRING;
				dealt_with = true;
			} else if (!in_string && !in_multiline_comment && !in_char) {
				// start of raw string
				in_string = true;
				string_is_raw = true;
			}
			break;
		case '\'':
			if (in_char && backslashes % 2 == 0) {
				in_char = false;
				if (char_types) {
					dealt_with = true;
					char_types[i] = SYNTAX_CHARACTER;
				}
			} else if (!in_multiline_comment && !in_string) {
				in_char = true;
			}
			break;
		case ANY_DIGIT:
			// a number!
			if (char_types && !in_multiline_comment && !in_string && !in_number && !in_char) {
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
			if (char_types && !in_multiline_comment && !in_number && !in_string && !in_char) {
				u32 keyword_len = syntax_keyword_len(LANG_GO, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_go, arr_count(syntax_all_keywords_go),
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
		if (in_number && !syntax_number_continues(LANG_GO, line, line_len, i)) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_multiline_comment)
				type = SYNTAX_COMMENT;
			else if (in_string)
				type = SYNTAX_STRING;
			else if (in_char)
				type = SYNTAX_CHARACTER;
			else if (in_number)
				type = SYNTAX_CONSTANT;

			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		  (in_multiline_comment * SYNTAX_STATE_GO_MULTILINE_COMMENT)
		| ((in_string && string_is_raw) * SYNTAX_STATE_GO_RAW_STRING)
	);
}

// This is the main syntax highlighting function. It will determine which colors to use for each character.
// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, char32_t const *line, u32 line_len, SyntaxCharType *char_types) {
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
	case LANG_TEX:
		syntax_highlight_tex(state, line, line_len, char_types);
		break;
	case LANG_MARKDOWN:
		syntax_highlight_markdown(state, line, line_len, char_types);
		break;
	case LANG_HTML:
		syntax_highlight_html(state, line, line_len, char_types);
		break;
	case LANG_CONFIG:
		syntax_highlight_config(state, line, line_len, char_types, false);
		break;
	case LANG_TED_CFG:
		syntax_highlight_config(state, line, line_len, char_types, true);
		break;
	case LANG_JAVASCRIPT:
		syntax_highlight_javascript(state, line, line_len, char_types);
		break;
	case LANG_JAVA:
		syntax_highlight_java(state, line, line_len, char_types);
		break;
	case LANG_GO:
		syntax_highlight_go(state, line, line_len, char_types);
		break;
	case LANG_COUNT: assert(0); break;
	}
}
