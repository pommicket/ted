// UTF-32 string
typedef struct {
	size_t len;
	char32_t *str;
} String32;

void s32_free(String32 *s) {
	free(s->str);
	s->str = NULL;
	s->len = 0;
}

// the string returned should be s32_free'd.
// this will return an empty string if the allocation failed or the string is invalid UTF-8
String32 s32_from_utf8(char const *utf8) {
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

