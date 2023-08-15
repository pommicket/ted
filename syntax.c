// syntax highlighting for ted

#include "ted-internal.h"
#include "keywords.h"


// ---- syntax state constants ----
// syntax state is explained in development.md

// these all say "CPP" but really they're C/C++
enum {
	SYNTAX_STATE_CPP_MULTI_LINE_COMMENT = 0x1u, // are we in a multi-line comment? (delineated by /* */)
	SYNTAX_STATE_CPP_SINGLE_LINE_COMMENT = 0x2u, // if you add a \ to the end of a single-line comment, it is continued to the next line.
	SYNTAX_STATE_CPP_PREPROCESSOR = 0x4u, // similar to above
	SYNTAX_STATE_CPP_STRING = 0x8u,
	SYNTAX_STATE_CPP_RAW_STRING = 0x10u,
};

enum {
	SYNTAX_STATE_RUST_COMMENT_DEPTH_MASK = 0xfu, // in rust, /* */ comments can nest.
	SYNTAX_STATE_RUST_COMMENT_DEPTH_MUL  = 0x1u,
	SYNTAX_STATE_RUST_COMMENT_DEPTH_BITS = 4, // number of bits we allocate for the comment depth.
	SYNTAX_STATE_RUST_STRING = 0x10u,
	SYNTAX_STATE_RUST_STRING_IS_RAW = 0x20u,
};

enum {
	SYNTAX_STATE_PYTHON_STRING = 0x01u, // multiline strings (''' and """)
	SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED = 0x02u, // is this a """ string, as opposed to a ''' string?
};

enum {
	SYNTAX_STATE_TEX_DOLLAR = 0x01u, // inside math $ ... $
	SYNTAX_STATE_TEX_DOLLARDOLLAR = 0x02u, // inside math $$ ... $$
	SYNTAX_STATE_TEX_VERBATIM = 0x04u, // inside \begin{verbatim} ... \end{verbatim}
};

enum {
	SYNTAX_STATE_MARKDOWN_CODE = 0x01u, // inside ``` ``` code section
};

enum {
	SYNTAX_STATE_HTML_COMMENT = 0x01u
};

enum {
	SYNTAX_STATE_JAVASCRIPT_TEMPLATE_STRING = 0x01u,
	SYNTAX_STATE_JAVASCRIPT_MULTILINE_COMMENT = 0x02u,
};

enum {
	SYNTAX_STATE_JAVA_MULTILINE_COMMENT = 0x01u
};

enum {
	SYNTAX_STATE_GO_RAW_STRING = 0x01u, // backtick-enclosed string
	SYNTAX_STATE_GO_MULTILINE_COMMENT = 0x02u
};

enum {
	SYNTAX_STATE_TED_CFG_STRING = 0x01u, // ` or "-delimited string
	SYNTAX_STATE_TED_CFG_STRING_BACKTICK = 0x02u, // `-delimited string
};

enum {
	SYNTAX_STATE_CSS_COMMENT = 0x01u,
	SYNTAX_STATE_CSS_IN_BRACES = 0x02u,
};

typedef struct {
	Language lang;
	char *name;
} LanguageName;

static LanguageName *language_names = NULL; // dynamic array


Language language_from_str(const char *str) {
	arr_foreach_ptr(language_names, LanguageName, lname) {
		if (strcmp_case_insensitive(lname->name, str) == 0)
			return lname->lang;
	}
	return LANG_NONE;
}

bool language_is_valid(Language language) {
	arr_foreach_ptr(language_names, LanguageName, lname) {
		if (lname->lang == language)
			return true;
	}
	return false;
}

const char *language_to_str(Language language) {
	arr_foreach_ptr(language_names, LanguageName, lname) {
		if (lname->lang == language)
			return lname->name;
	}
	return "???";
}

ColorSetting syntax_char_type_to_color_setting(SyntaxCharType t) {
	switch (t) {
	case SYNTAX_NORMAL: return COLOR_TEXT;
	case SYNTAX_KEYWORD: return COLOR_KEYWORD;
	case SYNTAX_COMMENT: return COLOR_COMMENT;
	case SYNTAX_PREPROCESSOR: return COLOR_PREPROCESSOR;
	case SYNTAX_STRING: return COLOR_STRING;
	case SYNTAX_CHARACTER: return COLOR_CHARACTER;
	case SYNTAX_CONSTANT: return COLOR_CONSTANT;
	case SYNTAX_BUILTIN: return COLOR_BUILTIN;
	case SYNTAX_TODO: return COLOR_TODO;
	}
	return COLOR_TEXT;
}

static bool syntax_keyword_matches(const char32_t *text, size_t len, const char *keyword) {
	if (len == strlen(keyword)) {
		bool matches = true;
		const char32_t *p = text;
		// check if `p` starts with `keyword`
		for (const char *q = keyword; *q; ++p, ++q) {
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

char32_t syntax_matching_bracket(Language lang, char32_t c) {
	if (lang == LANG_HTML || lang == LANG_XML) {
		// for most languages, this would look weird since
		//                       v cursor
		//       if (x < 5 && y >| 6)
		//             ^ this will be highlighted as a "matching bracket"
		// but for HTML this is nice
		switch (c) {
		case '<': return '>';
		case '>': return '<';
		}
	}
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

bool syntax_is_opening_bracket(Language lang, char32_t c) {
	if (lang == LANG_HTML || lang == LANG_XML) {
		if (c == '<')
			return true;
	}
	switch (c) {
	case '(':
	case '[':
	case '{':
		return true;
	}
	return false;
}

// lookup the given string in the keywords table
static const Keyword *syntax_keyword_lookup(const KeywordList *all_keywords, const char32_t *str, size_t len) {
	if (!len) return NULL;

	const KeywordList *list = &all_keywords[str[0] % 128];
	const Keyword *keywords = list->keywords;
	size_t nkeywords = list->len;
	if (keywords) {
		for (size_t k = 0; k < nkeywords; ++k) {
			if (syntax_keyword_matches(str, len, keywords[k].str)) {
				return &keywords[k];
			}
		}
	}
	return NULL;
}

// fast function to figure out if something can be in a comment keyword (like _TODO_)
static bool can_be_comment_keyword(char32_t c) {
	return c < 128 && c >= 'A' && c <= 'Z';//fuck you ebcdic
}

// this is used to highlight comments across languages, for stuff like TODOs
static SyntaxCharType syntax_highlight_comment(const char32_t *line, u32 i, u32 line_len) {
	assert(i < line_len);
	if (!can_be_comment_keyword(line[i]))
		return SYNTAX_COMMENT; // cannot be a keyword
	
	u32 max_len = 10; // maximum length of any comment keyword
	
	// find end of keyword
	u32 end = i + 1;
	while (end < line_len && i + max_len > end && is32_alpha(line[end])) {
		if (!can_be_comment_keyword(line[end]))
			return SYNTAX_COMMENT;
		++end;
	}
	if (end < line_len && (line[end] == '_' || is32_digit(line[end])))
		return SYNTAX_COMMENT;
	
	// find start of keyword
	u32 start = i;
	while (start > 0 && start + max_len > i && is32_alpha(line[start])) {
		if (!can_be_comment_keyword(line[start]))
			return SYNTAX_COMMENT;
		--start;
	}
	if (!is32_alpha(line[start]))
		++start;
	if (start > 0 && (line[start - 1] == '_' || is32_digit(line[start - 1])))
		return SYNTAX_COMMENT;
	
	// look it up
	const Keyword *kwd = syntax_keyword_lookup(syntax_all_keywords_comment, &line[start], end - start);
	if (kwd)
		return kwd->type;
	return SYNTAX_COMMENT;
}

// does i continue the number literal from i-1
static bool syntax_number_continues(Language lang, const char32_t *line, u32 line_len, u32 i) {
	if (line[i] == '.') {
		if ((i && line[i-1] == '.') || (i < line_len-1 && line[i+1] == '.'))
			return false; // can't have two .s in a row
		if (i < line_len-1 && lang == LANG_RUST && !isdigit(line[i+1]) && line[i+1] != '_') {
			// don't highlight 0.into() weirdly
			// (in Rust, only 0123456789_ can follow a decimal point)
			return false;
		}
	}
	if (lang == LANG_CSS) {
		if (line[i] < 128 && isalpha((char)line[i])) {
			// units
			return true;
		}
		if (line[i] == '%') {
			return true;
		}
	}
	
	// for simplicity, we don't actually recognize integer suffixes.
	// we just treat any letter in any suffix as a digit.
	// so 1lllllllllbablalbal will be highlighted as a number in C,
	// but that's not legal C anyways so it doesn't really matter.
	const char *digits;
	switch (lang) {
	case LANG_RUST:
		// note: the sz is for 1usize
		digits = "0123456789.xXoObBuUiIszabcdefABCDEF_";
		break;
	case LANG_C:
	case LANG_CPP:
		digits = "0123456789.xXbBlLuUiIabcdefABCDEFpP'";
		break;
	case LANG_GLSL:
		digits = "0123456789.xXbBlLuUabcdefABCDEF_";
		break;
	case LANG_GO:
		digits = "0123456789.xXoObBabcdefABCDEFpPi_";
		break;
	case LANG_JAVASCRIPT:
	case LANG_TYPESCRIPT:
		digits = "0123456789.xXoObBabcdefABCDEFn_";
		break;
	default:
		digits = "0123456789.xXoObBabcdefABCDEF_";
		break;
	}
	
	return (line[i] < CHAR_MAX && 
		(strchr(digits, (char)line[i])
		|| (i && line[i-1] == 'e' && (line[i] == '+' || line[i] == '-'))));
}

static bool is_keyword(Language lang, char32_t c) {
	if (c == '_' && lang == LANG_TEX) return false;
	if (is32_word(c)) return true;
	switch (lang) {
	case LANG_RUST:
		// Rust builtin macros
		if (c == '!')
			return true;
		break;
	case LANG_CSS:
		if (c == '-' || c == '@' || c == '!')
			return true;
		break;
	case LANG_HTML:
	case LANG_XML:
		if (c == '-' || c == '=')
			return true;
		break;
	default: break;
	}
	return false;
}

// find how long this keyword would be (if this is a keyword)
static u32 syntax_keyword_len(Language lang, const char32_t *line, u32 i, u32 line_len) {
	u32 keyword_end;
	for (keyword_end = i; keyword_end < line_len; ++keyword_end) {
		if (!is_keyword(lang, line[keyword_end]))
			break;
	}
	if (lang == LANG_CSS && keyword_end < line_len && line[keyword_end] == ':') {
		// in CSS we allow : at the end of keywords only
		// this is so "font-family:" can be a keyword but "a:hover" is parsed as "a" ":hover"
		if (keyword_end + 1 == line_len || (!is32_word(line[keyword_end + 1]) && line[keyword_end + 1] != ':')) {
			++keyword_end;
		}
	}
	return keyword_end - i;
}	

// highlighting for C, C++, and GLSL
static void syntax_highlight_c_cpp(SyntaxState *state_ptr, const char32_t *line, u32 line_len, SyntaxCharType *char_types, Language lang) {
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
					} else if (is32_word(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						in_number = false;
					}
				}
			}
			break;
		default: {
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			
			if (!in_single_line_comment && !in_multi_line_comment && !in_string && c == 'R' && has_2_chars && line[i + 1] == '"' && line[i + 2] == '(') {
				// raw string
				in_raw_string = true;
				raw_string_ending = false;
				break;
			}
			
			// keywords don't matter for advancing the state
			if (char_types && !in_single_line_comment && !in_multi_line_comment && !in_number && !in_string && !in_preprocessor && !in_char) {
				u32 keyword_len = syntax_keyword_len(lang, line, i, line_len);
				Keyword const *keyword = NULL;
				switch (lang) {
				case LANG_CPP:
					keyword = syntax_keyword_lookup(syntax_all_keywords_cpp, &line[i], keyword_len);
					if (!keyword)
						keyword = syntax_keyword_lookup(syntax_all_keywords_c, &line[i], keyword_len);
					break;
				case LANG_GLSL:
					keyword = syntax_keyword_lookup(syntax_all_keywords_glsl, &line[i], keyword_len);
					break;
				default:
					assert(lang == LANG_C);
					keyword = syntax_keyword_lookup(syntax_all_keywords_c, &line[i], keyword_len);
					break;
				}
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
		if (in_number && !syntax_number_continues(lang, line, line_len, i)) {
			in_number = false;
		}

		if (char_types && !dealt_with) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_single_line_comment || in_multi_line_comment)
				type = syntax_highlight_comment(line, i, line_len);
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

static void syntax_highlight_rust(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
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
							char_types[j] = syntax_highlight_comment(line, j, line_len);
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
				if (i && (is32_word(line[i - 1])
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
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			if (i >= 2 && line[i-2] == 'r' && line[i-1] == '#') {
				// raw identifier
				break;
			}
			if (char_types && !in_string && !comment_depth && !in_number) {
				u32 keyword_len = syntax_keyword_len(LANG_RUST, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_rust, &line[i], keyword_len);
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
				type = syntax_highlight_comment(line, i, line_len);
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

static void syntax_highlight_python(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	(void)state;
	bool in_string = (*state & SYNTAX_STATE_PYTHON_STRING) != 0;
	bool string_is_dbl_quoted = (*state & SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED) != 0;
	bool string_is_multiline = true;
	bool in_number = false;
	u32 backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		char32_t c = line[i];
		bool dealt_with = false;
		switch (c) {
		case '#':
			if (!in_string) {
				// comment
				if (char_types) {
					for (u32 j = i; j < line_len; ++j)
						char_types[j] = syntax_highlight_comment(line, j, line_len);
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
					} else if (is32_word(line[i - 1])) {
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
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			
			if (char_types && !in_string && !in_number) {
				u32 keyword_len = syntax_keyword_len(LANG_PYTHON, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_python, &line[i], keyword_len);
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
	return is32_word(c) && !is32_digit(c) && c != '_';
}

static void syntax_highlight_tex(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
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
						char_types[i] = syntax_highlight_comment(line, i, line_len);
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

static void syntax_highlight_markdown(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
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
	const char *format_ending = NULL; // "**" if we are inside **bold**, etc.
	
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
				const char *end = c == '*' ? "**" : "__";
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
				const char *end = c == '*' ? "*" : "_";
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

// highlights XML and HTML
static void syntax_highlight_html_like(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types, Language lang) {
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
				if (char_types) char_types[i] = syntax_highlight_comment(line, i, line_len);
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
				
					if ((i && is32_word(line[i - 1])) || !is32_word(line[i]))
						break; // can't be a keyword on its own.
					
					if (lang == LANG_XML)
						break; // XML has no keywords
					
					assert(lang == LANG_HTML);
					
					u32 keyword_len = syntax_keyword_len(lang, line, i, line_len);
					Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_html, &line[i], keyword_len);
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

static void syntax_highlight_cfg(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types, bool is_ted_cfg) {
	bool string = (*state & SYNTAX_STATE_TED_CFG_STRING) != 0;
	char32_t string_delimiter = (*state & SYNTAX_STATE_TED_CFG_STRING_BACKTICK) ? '`' : '"';
	
	if (line_len == 0) return;
	
	if (!string && line[0] == '#') {
		if (char_types) {
			for (u32 i = 0; i < line_len; i++) {
				char_types[i] = syntax_highlight_comment(line, i, line_len);
			}
		}
		return;
	}
	if (!string && line[0] == '[' && line[line_len - 1] == ']') {
		if (char_types) memset(char_types, SYNTAX_BUILTIN, line_len);
		return;
	}
	
	if (!string && is_ted_cfg && line[0] == '%') {
		if (char_types) memset(char_types, SYNTAX_PREPROCESSOR, line_len);
		return;
	}
	
	int backslashes = 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		if (char_types)
			char_types[i] = string ? SYNTAX_STRING : SYNTAX_NORMAL;
		switch (line[i]) {
		case '"':
		case '`':
			if (string) {
				if (backslashes % 2 == 0 && line[i] == string_delimiter) {
					string = false;
				}
			} else {
				string = true;
				string_delimiter = line[i];
			}
			if (char_types)
				char_types[i] = SYNTAX_STRING;
			break;
		case '#':
			if (!string) {
				// don't try highlighting the rest of the line.
				// for ted.cfg, this could be a color, but for other cfg files,
				// it might be a comment
				if (char_types)
					memset(&char_types[i], 0, line_len - i);
				i = line_len;
			}
			break;
		case ANY_DIGIT:
			if (char_types && i > 0 && !string) {
				if (is32_word(line[i-1]) // something like e5
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
			if (string)
				break;
			if (i == 0) // none of the keywords in syntax_all_keywords_config should appear at the start of the line
				break;
			if (is32_word(line[i-1]) || line[i-1] == '-' || !is32_word(line[i]))
				break; // can't be a keyword on its own.
			u32 keyword_len = syntax_keyword_len(LANG_CONFIG, line, i, line_len);
			Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_config, &line[i], keyword_len);
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
		*state = 0;
		if (string) {
			*state |= SYNTAX_STATE_TED_CFG_STRING;
			if (string_delimiter == '`')
				*state |= SYNTAX_STATE_TED_CFG_STRING_BACKTICK;
		}
	}
}

// highlighting for javascript, typescript, and JSON
static void syntax_highlight_javascript_like(
	SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types, Language language) {
	(void)state;
	bool string_is_template = (*state & SYNTAX_STATE_JAVASCRIPT_TEMPLATE_STRING) != 0;
	bool in_multiline_comment = (*state & SYNTAX_STATE_JAVASCRIPT_MULTILINE_COMMENT) != 0;
	bool string_is_dbl_quoted = false;
	bool string_is_regex = false;
	bool in_number = false;
	bool in_string = string_is_template;
	u32 backslashes = 0;
	
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
						if (char_types) {
							for (u32 j = i; j < line_len; j++) {
								char_types[j] = syntax_highlight_comment(line, j, line_len);
							}
						}
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
					//  but should handle all "reasonable" uses of regex,
					// while not accidentally treating division as regex.
					bool is_regex = i == 0 // slash is first char in line
						// slash preceded by space and followed by non-space
						|| (is32_space(line[i-1]) && i + 1 < line_len && !is32_space(line[i+1]))
						// slash preceded by any of these characters
						|| (line[i-1] <= 128 && strchr(";({[=,:", (char)line[i-1]));
					
					if (i + 1 < line_len && line[i+1] == '=') {
						// slash is followed by equals, so this might be /=
						is_regex = false;
					}
					
					if (i + 1 < line_len && line[i+1] == '>') {
						// slash is followed by equals, so this might be a self-closing JSX tag
						is_regex = false;
					}
					
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
					} else if (is32_word(line[i - 1])) {
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
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			
			if (char_types && !in_string && !in_number && !in_multiline_comment) {
				u32 keyword_len = syntax_keyword_len(LANG_JAVASCRIPT, line, i, line_len);
				const KeywordList *keywords = NULL;
				switch (language) {
				case LANG_JAVASCRIPT:
					keywords = syntax_all_keywords_javascript;
					break;
				case LANG_TYPESCRIPT:
					keywords = syntax_all_keywords_typescript;
					break;
				default:
					assert(language == LANG_JSON);
					keywords = syntax_all_keywords_json;
					break;
				}
				
				Keyword const *keyword = syntax_keyword_lookup(keywords, &line[i], keyword_len);
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
				type = syntax_highlight_comment(line, i, line_len);
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

static void syntax_highlight_java(SyntaxState *state_ptr, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
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
					// //
					if (char_types) {
						for (u32 j = i; j < line_len; j++) {
							char_types[j] = syntax_highlight_comment(line, j, line_len);
						}
					}
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
					} else if (is32_word(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						in_number = false;
					}
				}
			}
			break;
		default: {
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			
			// keywords don't matter for advancing the state
			if (char_types && !in_multiline_comment && !in_number && !in_string && !in_char) {
				u32 keyword_len = syntax_keyword_len(LANG_JAVA, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_java, &line[i], keyword_len);
				
				
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
				type = syntax_highlight_comment(line, i, line_len);
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


static void syntax_highlight_go(SyntaxState *state_ptr, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
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
					// //
					if (char_types) {
						for (u32 j = 0; j < line_len; ++j)
							char_types[j] = syntax_highlight_comment(line, j, line_len);
					}
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
					} else if (is32_word(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						in_number = false;
					}
				}
			}
			break;
		default: {
			if ((i && is32_word(line[i - 1])) || !is32_word(c))
				break; // can't be a keyword on its own.
			
			// keywords don't matter for advancing the state
			if (char_types && !in_multiline_comment && !in_number && !in_string && !in_char) {
				u32 keyword_len = syntax_keyword_len(LANG_GO, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_go, &line[i], keyword_len);
				
				
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
				type = syntax_highlight_comment(line, i, line_len);
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

static void syntax_highlight_text(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	(void)state;
	(void)line;
	(void)line_len;
	if (char_types) {
		memset(char_types, 0, line_len);
	}
}

static void syntax_highlight_css(SyntaxState *state_ptr, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	SyntaxState state = *state_ptr;
	bool in_comment = (state & SYNTAX_STATE_CSS_COMMENT) != 0;
	bool in_braces = (state & SYNTAX_STATE_CSS_IN_BRACES) != 0;
	
	for (u32 i = 0; i < line_len; ++i) {
		bool has_1_char = i + 1 < line_len;
		bool dealt_with = false;
		char32_t c = line[i];
		
		if (in_comment) {
			if (c == '/' && i > 0 && line[i - 1] == '*') {
				in_comment = false;
				if (char_types) char_types[i] = SYNTAX_COMMENT;
				dealt_with = true;
			}
		} else switch (c) {
		case '/':
			if (has_1_char) {
				if (line[i + 1] == '*') {
					in_comment = true; // /*
				}
			}
			break;
		case '{':
			in_braces = true;
			break;
		case '}':
			in_braces = false;
			break;
		case '*':
			if (char_types) {
				char_types[i++] = SYNTAX_KEYWORD;
				dealt_with = true;
				goto handle_pseudo;
			}
			break;
		case '[':
			if (!in_braces) {
				if (char_types) char_types[i] = SYNTAX_KEYWORD;
				for (++i; i < line_len; ++i) {
					if (char_types)
						char_types[i] = SYNTAX_KEYWORD;
					if (line[i] == ']') break;
				}
				if (i < line_len && char_types){
					char_types[i++] = SYNTAX_KEYWORD;
				}
				dealt_with = true;
				if (char_types)
					goto handle_pseudo;
			}
			break;
		case '\'':
		case '"': {
			// i'm not handling multiline stirngs
			// wtf are you doing with multiline strings in css
			if (char_types) char_types[i] = SYNTAX_STRING;
			int backslashes = 0;
			for (++i; i < line_len; ++i) {
				if (char_types)
					char_types[i] = SYNTAX_STRING;
				if (backslashes % 2 == 0 && line[i] == c)
					break;
				if (line[i] == '\\')
					++backslashes;
				else
					backslashes = 0;
			}
			dealt_with = true;
			}
			break;
		case '.':
		case '#':
			if (!in_braces && char_types) {
				char_types[i++] = SYNTAX_KEYWORD;		
				for (; i < line_len; ++i) {
					if (is32_alnum(line[i]) || line[i] == '-' || line[i] == '_') {
						char_types[i] = SYNTAX_KEYWORD;
					} else {
						break;
					}
				}
				dealt_with = true;
				handle_pseudo:
				while (i < line_len && line[i] == ':') {
					// handle pseudo-classes and pseudo-elements
					static const char *const pseudo[] = {
						":active", ":any-link", ":autofill", ":blank", ":checked", 
						":current", ":default", ":defined", ":dir", ":disabled", 
						":empty", ":enabled", ":first", ":first-child", 
						":first-of-type", ":fullscreen", ":future", ":focus", 
						":focus-visible", ":focus-within", ":has", ":host", 
						":host-context", ":hover", ":indeterminate", ":in-range", 
						":invalid", ":is", ":lang", ":last-child", ":last-of-type", 
						":left", ":link", ":local-link", ":modal", ":not", 
						":nth-child", ":nth-col", ":nth-last-child", 
						":nth-last-col", ":nth-last-of-type", ":nth-of-type", 
						":only-child", ":only-of-type", ":optional", 
						":out-of-range", ":past", ":picture-in-picture", 
						":placeholder-shown", ":paused", ":playing", ":read-only", 
						":read-write", ":required", ":right", ":root", ":scope", 
						":state", ":target", ":target-within", ":user-invalid", 
						":valid", ":visited", ":where", "::after", "::before", 
						":after", ":before",
						"::backdrop", "::cue", "::cue-region", "::first-letter", 
						"::first-line", "::file-selector-button", 
						"::grammar-error", "::marker", "::part", "::placeholder", 
						"::selection", "::slotted", "::spelling-error", 
						"::target-text", 
					};
					String32 s32 = {
						.len = line_len - i,
						.str = (char32_t *)(line + i)
					};
					bool found = false;
					for (size_t p = 0; p < arr_count(pseudo); ++p) {
						size_t len = strlen(pseudo[p]);
						if (str32_has_ascii_prefix(s32, pseudo[p])
							&& (
							i + len >= line_len
							|| !is_keyword(LANG_CSS, line[i + len])
							)) {
							found = true;
							for (size_t j = 0; j < len; ++j) {
								assert(i < line_len);// guaranteed by has_ascii_prefix
								char_types[i++] = SYNTAX_KEYWORD;
							}
						}
					}
					if (!found) break;
				}
				
				--i; // we will increment i in the loop
			
			}
			if (c == '#' && in_braces && char_types) {
				// color which we'll treat as a number.
				goto number;
			}
			break;
		case ANY_DIGIT:
		number: {
			// a number!
			bool number = false;
			if (char_types) {
				number = true;
				if (i) {
					if (line[i - 1] == '.') {
						// support .6, for example
						char_types[i - 1] = SYNTAX_CONSTANT;
					} else if (is32_word(line[i - 1])) {
						// actually, this isn't a number. it's something like a*6* or u3*2*.
						number = false;
					}
				}
			}
			if (number) {
				char_types[i++] = SYNTAX_CONSTANT;
				while (syntax_number_continues(LANG_CSS, line, line_len, i)) {
					char_types[i] = SYNTAX_CONSTANT;
					++i;
				}
				--i; // we will increment i in the loop
				dealt_with = true;
			}
			} break;
		default: {
			if ((i && is_keyword(LANG_CSS, line[i - 1])) || !is_keyword(LANG_CSS, c) || is32_digit(c))
				break; // can't be a keyword on its own.
			
			if (char_types) {
				u32 keyword_len = syntax_keyword_len(LANG_CSS, line, i, line_len);
				Keyword const *keyword = syntax_keyword_lookup(syntax_all_keywords_css, &line[i], keyword_len);
				
				// in braces, we only accept SYNTAX_KEYWORD, and outside we accept everything else
				if (keyword && (keyword->type != SYNTAX_KEYWORD) == in_braces) {
					SyntaxCharType type = keyword->type;
					for (size_t j = 0; j < keyword_len; ++j) {
						char_types[i++] = type;
					}
					dealt_with = true;
					if (!in_braces && char_types) {
						goto handle_pseudo;
					} else {					
						--i; // we'll increment i from the for loop
					}
					break;
				}
			}
		} break;
		}
		if (char_types && !dealt_with && i < line_len) {
			SyntaxCharType type = SYNTAX_NORMAL;
			if (in_comment)
				type = syntax_highlight_comment(line, i, line_len);
			char_types[i] = type;
		}
	}
	*state_ptr = (SyntaxState)(
		  (in_comment * SYNTAX_STATE_CSS_COMMENT)
		  | (in_braces * SYNTAX_STATE_CSS_IN_BRACES)
	);
}


typedef struct {
	Language lang;
	SyntaxHighlightFunction func;
} SyntaxHighlighter;

static SyntaxHighlighter *syntax_highlighters = NULL; // dynamic array

void syntax_highlight(SyntaxState *state, Language lang, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	arr_foreach_ptr(syntax_highlighters, SyntaxHighlighter, highlighter) {
		if (highlighter->lang == lang) {
			highlighter->func(state, line, line_len, char_types);
			return;
		}
	}
	syntax_highlight_text(state, line, line_len, char_types);
}

static void syntax_highlight_c(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_c_cpp(state, line, line_len, char_types, LANG_C);
}
static void syntax_highlight_glsl(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_c_cpp(state, line, line_len, char_types, LANG_GLSL);
}
static void syntax_highlight_cpp(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_c_cpp(state, line, line_len, char_types, LANG_CPP);
}
static void syntax_highlight_xml(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_html_like(state, line, line_len, char_types, LANG_XML);
}
static void syntax_highlight_html(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_html_like(state, line, line_len, char_types, LANG_HTML);
}
static void syntax_highlight_javascript(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_javascript_like(state, line, line_len, char_types, LANG_JAVASCRIPT);
}
static void syntax_highlight_typescript(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_javascript_like(state, line, line_len, char_types, LANG_TYPESCRIPT);
}
static void syntax_highlight_json(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_javascript_like(state, line, line_len, char_types, LANG_JSON);
}
static void syntax_highlight_config(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_cfg(state, line, line_len, char_types, false);
}
static void syntax_highlight_ted_cfg(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types) {
	syntax_highlight_cfg(state, line, line_len, char_types, true);
}


void syntax_init(void) {
	static const LanguageInfo builtins[] = {
		{
			.id = LANG_TEXT,
			.name = "Text",
			.lsp_identifier = "text",
			.highlighter = syntax_highlight_text,
		},
		{
			.id = LANG_C,
			.name = "C",
			.lsp_identifier = "c",
			.highlighter = syntax_highlight_c,
		},
		{
			.id = LANG_CPP,
			.name = "C++",
			.lsp_identifier = "cpp",
			.highlighter = syntax_highlight_cpp,
		},
		{
			.id = LANG_RUST,
			.name = "Rust",
			.lsp_identifier = "rust",
			.highlighter = syntax_highlight_rust,
		},
		{
			.id = LANG_JAVA,
			.name = "Java",
			.lsp_identifier = "java",
			.highlighter = syntax_highlight_java,
		},
		{
			.id = LANG_GO,
			.name = "Go",
			.lsp_identifier = "go",
			.highlighter = syntax_highlight_go,
		},
		{
			.id = LANG_PYTHON,
			.name = "Python",
			.lsp_identifier = "python",
			.highlighter = syntax_highlight_python,
		},
		{
			.id = LANG_TEX,
			.name = "TeX",
			.lsp_identifier = "latex",
			.highlighter = syntax_highlight_tex,
		},
		{
			.id = LANG_MARKDOWN,
			.name = "Markdown",
			.lsp_identifier = "markdown",
			.highlighter = syntax_highlight_markdown,
		},
		{
			.id = LANG_HTML,
			.name = "HTML",
			.lsp_identifier = "html",
			.highlighter = syntax_highlight_html,
		},
		{
			.id = LANG_XML,
			.name = "XML",
			.lsp_identifier = "xml",
			.highlighter = syntax_highlight_xml,
		},
		{
			.id = LANG_CONFIG,
			.name = "Config",
			.lsp_identifier = "text",
			.highlighter = syntax_highlight_config,
		},
		{
			.id = LANG_TED_CFG,
			.name = "TedCfg",
			.lsp_identifier = "text",
			.highlighter = syntax_highlight_ted_cfg,
		},
		{
			.id = LANG_JAVASCRIPT,
			.name = "JavaScript",
			.lsp_identifier = "javascript",
			.highlighter = syntax_highlight_javascript,
		},
		{
			.id = LANG_TYPESCRIPT,
			.name = "TypeScript",
			.lsp_identifier = "typescript",
			.highlighter = syntax_highlight_typescript,
		},
		{
			.id = LANG_JSON,
			.name = "JSON",
			.lsp_identifier = "json",
			.highlighter = syntax_highlight_json,
		},
		{
			.id = LANG_GLSL,
			.name = "GLSL",
			// not specified as of LSP 3.17, but this seems like the natural choice
			.lsp_identifier = "glsl",
			.highlighter = syntax_highlight_glsl,
		},
		{
			.id = LANG_CSS,
			.name = "CSS",
			.lsp_identifier = "css",
			.highlighter = syntax_highlight_css
		},
		
	};
	for (size_t i = 0; i < arr_count(builtins); ++i) {
		syntax_register_language(&builtins[i]);
	}
}

void syntax_register_language(const LanguageInfo *info) {
	if (!info->id || info->id > LANG_USER_MAX) {
		debug_println("Bad language ID: %" PRIu32, info->id);
		return;
	}
	if (!info->name[0]) {
		debug_println("Language with ID %" PRIu32 " has no name.", info->id);
		return;
	}
	
	arr_foreach_ptr(language_names, LanguageName, lname) {
		if (streq(lname->name, info->name)) {
			debug_println("Language named %s registered twice.", info->name);
			return;
		}
		if (lname->lang == info->id) {
			debug_println("Language with ID %" PRIu32 " registered twice.", info->id);
			return;
		}
	}
	{
		LanguageName *lname = arr_addp(language_names);
		lname->lang = info->id;
		lname->name = str_dup(info->name);
	}
	
	
	if (info->highlighter) {
		SyntaxHighlighter *highlighter = arr_addp(syntax_highlighters);
		highlighter->lang = info->id;
		highlighter->func = info->highlighter;
	}
	
	lsp_register_language(info->id, info->lsp_identifier);
}

void syntax_quit(void) {
	arr_foreach_ptr(language_names, LanguageName, lname) {
		free(lname->name);
	}
	arr_clear(language_names);
	arr_clear(syntax_highlighters);
}
