// UTF-32 string
typedef struct {
	char32_t *str;
	size_t len;
} String32;

String32 str32(char32_t *str, size_t len) {
	String32 s = {str, len};
	return s;
}

String32 str32_substr(String32 s, size_t from, size_t len) {
	return str32(s.str + from, len);
}

// frees string and sets it to ""
void str32_free(String32 *s) {
	free(s->str);
	s->str = NULL;
	s->len = 0;
}

// the string returned should be str32_free'd.
// this will return an empty string if the allocation failed or the string is invalid UTF-8
String32 str32_from_utf8(char const *utf8) {
	String32 string = {NULL, 0};
	size_t len = strlen(utf8);
	if (len) {
		// the wide string uses at most as many "characters" (elements?) as the UTF-8 string
		char32_t *widestr = calloc(len, sizeof *widestr);
		if (widestr) {
			char32_t *wide_p = widestr;
			char const *utf8_p = utf8;
			char const *utf8_end = utf8_p + len;
			while (utf8_p < utf8_end) {
				char32_t c = 0;
				size_t n = unicode_utf8_to_utf32(&c, utf8_p, (size_t)(utf8_end - utf8_p));
				if (n == 0 // null character. this shouldn't happen.
					|| n >= (size_t)(-2) // invalid UTF-8
					) {
					free(widestr);
					widestr = wide_p = NULL;
					break;
				} else {
					// n bytes consumed
					*wide_p++ = c;
					utf8_p += n;
				}
			}
			string.str = widestr;
			string.len = (size_t)(wide_p - widestr);
		}
	}
	return string;
}

// returns a null-terminated UTF-8 string
// the string returned should be free'd
// this will return NULL on failure
static char *str32_to_utf8_cstr(String32 s) {
	char *utf8 = calloc(4 * s.len + 1, 1); // each codepoint takes up at most 4 bytes in UTF-8, + we need a terminating null byte
	if (utf8) {
		char *p = utf8;
		for (size_t i = 0; i < s.len; ++i) {
			size_t bytes = unicode_utf32_to_utf8(p, s.str[i]);
			if (bytes == (size_t)-1) {
				// invalid UTF-32 code point
				free(utf8);
				return NULL;
			} else {
				p += bytes;
			}
		}
		*p = '\0';
	}
	return utf8;
}

// compare s to the ASCII string `ascii`
static int str32_cmp_ascii(String32 s, char const *ascii) {
	for (size_t i = 0; i < s.len; ++i) {
		assert((char32_t)ascii[i] < 128);
		if ((char32_t)ascii[i] == '\0')
			return -1; // ascii is a prefix of s
		if (s.str[i] > (char32_t)ascii[i])
			return +1;
		if (s.str[i] < (char32_t)ascii[i])
			return -1;
	}
	if (ascii[s.len]) {
		// s is a prefix of ascii
		return +1;
	}
	return 0;
}

// check if s starts with the ASCII string `ascii`
static int str32_has_ascii_prefix(String32 s, char const *ascii) {
	for (size_t i = 0; i < s.len; ++i) {
		assert((char32_t)ascii[i] < 128);
		if ((char32_t)ascii[i] == '\0')
			return true; // ascii is a prefix of s
		if (s.str[i] > (char32_t)ascii[i])
			return false;
		if (s.str[i] < (char32_t)ascii[i])
			return false;
	}
	if (ascii[s.len]) {
		// s is a prefix of ascii
		return false;
	}
	// s is the same as ascii
	return true;
}

// returns the index of the given character in the string, or the length of the string if it's not found.
size_t str32chr(String32 s, char32_t c) {
	for (size_t i = 0; i < s.len; ++i) {
		if (s.str[i] == c)
			return i;
	}
	return s.len;
}

// returns number of instances of c in s
size_t str32_count_char(String32 s, char32_t c) {
	size_t total = 0;
	for (size_t i = 0; i < s.len; ++i) {
		total += s.str[i] == c;
	}
	return total;
}

// returns number of characters deleted from s
size_t str32_remove_all_instances_of_char(String32 *s, char32_t c) {
	char32_t *str = s->str;
	size_t ndeleted = 0;
	size_t len = s->len;
	size_t out = 0;
	for (size_t in = 0; in < len; ++in) {
		if (str[in] == c) {
			++ndeleted;
		} else {
			str[out++] = str[in];
		}
	}
	s->len = out;
	return ndeleted;
}

// returns the length of the longest prefix of `s` containing only
// ASCII characters in the C-string `charset`.
size_t str32_ascii_spn(String32 s, char const *charset) {
	for (u32 i = 0; i < s.len; ++i) {
		if (s.str[i] >= 128)
			return i; // non-ASCII character in s, so that can't be in charset.
		bool found = false;
		for (char const *p = charset; *p; ++p) {
			assert((char32_t)*p < 128);
			if ((char32_t)*p == s.str[i]) {
				found = true;
				break;
			}
		}
		if (!found) return i;
	}
	return s.len;
}

bool is32_space(char32_t c) {
	return c <= WINT_MAX && iswspace((wint_t)c);
}

bool is32_alpha(char32_t c) {
	return c <= WINT_MAX && iswalpha((wint_t)c);
}

bool is32_alnum(char32_t c) {
	return c <= WINT_MAX && iswalnum((wint_t)c);
}

bool is32_digit(char32_t c) {
	return c <= WINT_MAX && iswdigit((wint_t)c);
}

bool is32_graph(char32_t c) {
	return c <= WINT_MAX && iswgraph((wint_t)c);
}

// could this character appear in a C-style identifier?
bool is32_ident(char32_t c) {
	return c <= WINT_MAX && (iswalnum((wint_t)c) || c == '_');
}
