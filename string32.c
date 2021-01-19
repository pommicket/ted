// UTF-32 string
typedef struct {
	size_t len;
	char32_t *str;
} String32;

void str32_free(String32 *s) {
	free(s->str);
	s->str = NULL;
	s->len = 0;
}

// the string returned should be str32_free'd.
// this will return an empty string if the allocation failed or the string is invalid UTF-8
String32 str32_from_utf8(char const *utf8) {
	String32 string = {0, NULL};
	size_t len = strlen(utf8);
	if (len) {
		// the wide string uses at most as many "characters" (elements?) as the UTF-8 string
		char32_t *widestr = calloc(len, sizeof *widestr);
		if (widestr) {
			char32_t *wide_p = widestr;
			char const *utf8_p = utf8;
			char const *utf8_end = utf8_p + len;
			mbstate_t mbstate = {0};
			while (utf8_p < utf8_end) {
				char32_t c = 0;
				size_t n = mbrtoc32(&c, utf8_p, (size_t)(utf8_end - utf8_p), &mbstate);
				if (n == 0// null character. this shouldn't happen.
					|| n == (size_t)(-2) // incomplete character
					|| n == (size_t)(-1) // invalid UTF-8
					) {
					free(widestr);
					widestr = wide_p = NULL;
					break;
				} else if (n == (size_t)(-3)) { // no bytes consumed, but a character was produced
					*wide_p++ = c;
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
		mbstate_t mbstate; memset(&mbstate, 0, sizeof mbstate);
		for (size_t i = 0; i < s.len; ++i) {
			size_t bytes = c32rtomb(p, s.str[i], &mbstate);
			if (bytes == (size_t)-1) {
				// invalid UTF-32 character
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
