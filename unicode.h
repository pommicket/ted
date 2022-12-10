#ifndef UNICODE_H_
#define UNICODE_H_
#define UNICODE_BOX_CHARACTER 0x2610
#define UNICODE_CODE_POINTS 0x110000 // number of Unicode code points

static bool unicode_is_start_of_code_point(u8 byte) {
	// see https://en.wikipedia.org/wiki/UTF-8#Encoding
	// continuation bytes are of the form 10xxxxxx
	return (byte & 0xC0) != 0x80;
}

// A lot like mbrtoc32. Doesn't depend on the locale though, for one thing.
// *c will be filled with the next UTF-8 code point in `str`. `bytes` refers to the maximum
// number of bytes that can be read from `str`.
// Returns:
// 0 - if a null character was encountered
// (size_t)-1 - on invalid UTF-8
// (size_t)-2 - on incomplete code point (str should be longer)
// other - the number of bytes read from `str`.
static size_t unicode_utf8_to_utf32(char32_t *c, char const *str, size_t bytes) {
	if (bytes == 0) {
		*c = 0;
		return 0;
	}
	// it's easier to do things with unsigned integers
	u8 const *p = (u8 const *)str;

	u8 first_byte = *p;
	
	if (first_byte & 0x80) {
		if ((first_byte & 0xE0) == 0xC0) {
			// two-byte code point
			if (bytes >= 2) {
				++p;
				u32 second_byte = *p;
				u32 value = ((u32)first_byte & 0x1F) << 6
					| (second_byte & 0x3F);
				*c = (char32_t)value;
				return 2;
			} else {
				// incomplete code point
				*c = 0;
				return (size_t)-2;
			}
		}
		if ((first_byte & 0xF0) == 0xE0) {
			// three-byte code point
			if (bytes >= 3) {
				++p;
				u32 second_byte = *p;
				++p;
				u32 third_byte = *p;
				u32 value = ((u32)first_byte & 0x0F) << 12
					| (second_byte & 0x3F) << 6
					| (third_byte & 0x3F);
				if (value < 0xD800 || value > 0xDFFF) {
					*c = (char32_t)value;
					return 3;
				} else {
					// reserved for UTF-16 surrogate halves
					*c = 0;
					return (size_t)-1;
				}
			} else {
				// incomplete
				*c = 0;
				return (size_t)-2;
			}
		}
		if ((first_byte & 0xF8) == 0xF0) {
			// four-byte code point
			if (bytes >= 4) {
				++p;
				u32 second_byte = *p;
				++p;
				u32 third_byte = *p;
				++p;
				u32 fourth_byte = *p;
				u32 value = ((u32)first_byte & 0x07) << 18
					| (second_byte & 0x3F) << 12
					| (third_byte  & 0x3F) << 6
					| (fourth_byte & 0x3F);
				if (value <= 0x10FFFF) {
					*c = (char32_t)value;
					return 4;
				} else {
					// Code points this big can't be encoded by UTF-16 and so are invalid UTF-8.
					*c = 0;
					return (size_t)-1;
				}
			} else {
				// incomplete
				*c = 0;
				return (size_t)-2;
			}
		}
		// invalid UTF-8
		*c = 0;
		return (size_t)-1;
	} else {
		// ASCII character
		if (first_byte == 0) {
			*c = 0;
			return 0;
		}
		*c = first_byte;
		return 1;
	}
}

// A lot like c32rtomb
// Converts a UTF-32 codepoint to a UTF-8 string. Writes at most 4 bytes to s.
// NOTE: It is YOUR JOB to null-terminate your string if the UTF-32 isn't null-terminated!
// Returns the number of bytes written to s, or (size_t)-1 on invalid UTF-32.
static size_t unicode_utf32_to_utf8(char *s, char32_t c32) {
	u8 *p = (u8 *)s;
	if (c32 <= 0x7F) {
		// ASCII
		*p = (u8)c32;
		return 1;
	} else if (c32 <= 0x7FF) {
		// two bytes needed
		*p++ = (u8)(0xC0 | (c32 >> 6));
		*p   = (u8)(0x80 | (c32 & 0x3F));
		return 2;
	} else if (c32 <= 0x7FFF) {
		if (c32 < 0xD800 || c32 > 0xDFFF) {
			*p++ = (u8)(0xE0 | ( c32 >> 12));
			*p++ = (u8)(0x80 | ((c32 >> 6) & 0x3F));
			*p   = (u8)(0x80 | ( c32       & 0x3F));
			return 3;
		} else {
			// UTF-16 surrogate halves
			*p = 0;
			return (size_t)-1;
		}
	} else if (c32 <= 0x10FFFF) {
		*p++ = (u8)(0xF0 | ( c32 >> 18));
		*p++ = (u8)(0x80 | ((c32 >> 12) & 0x3F));
		*p++ = (u8)(0x80 | ((c32 >>  6) & 0x3F));
		*p   = (u8)(0x80 | ( c32        & 0x3F));
		return 4;
	} else {
		// code point too big
		*p = 0;
		return (size_t)-1;
	}
}
#endif // UNICODE_H_
