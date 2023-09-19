/// \file
/// functions for dealing with UTF-8/UTF-16/UTF-32.
///
/// this file is entirely self-contained.

#ifndef UNICODE_H_
#define UNICODE_H_
/// useful for "this character couldn't be rendered / is invalid UTF-8"
#define UNICODE_BOX_CHARACTER 0x2610
/// number of Unicode code points
#define UNICODE_CODE_POINTS 0x110000

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static bool unicode_is_start_of_code_point(uint8_t byte) {
	// see https://en.wikipedia.org/wiki/UTF-8#Encoding
	// continuation bytes are of the form 10xxxxxx
	return (byte & 0xC0) != 0x80;
}

static bool unicode_is_continuation_byte(uint8_t byte) {
	return (byte & 0xC0) == 0x80;
}

/// A lot like mbrtoc32. Doesn't depend on the locale though, for one thing.
///
/// *c will be filled with the next UTF-8 code point in `str`. `bytes` refers to the maximum
/// number of bytes that can be read from `str` (note: this function will never read past a null
/// byte, even if `bytes` indicates that it could).
/// Returns:\n
/// `0` - if a null character was encountered or if `bytes == 0`\n
/// `(size_t)-1` - on invalid UTF-8\n
/// `(size_t)-2` - on incomplete code point (str should be longer)\n
/// other - the number of bytes read from `str`.
static size_t unicode_utf8_to_utf32(uint32_t *c, const char *str, size_t bytes) {
	*c = 0;
	if (bytes == 0) {
		return 0;
	}
	// it's easier to do things with unsigned integers
	const uint8_t *p = (const uint8_t *)str;

	uint8_t first_byte = *p;
	
	if (first_byte & 0x80) {
		if ((first_byte & 0xE0) == 0xC0) {
			// two-byte code point
			if (bytes >= 2) {
				++p;
				uint32_t second_byte = *p;
				if ((second_byte & 0xC0) != 0x80) return (size_t)-1;
				uint32_t value = ((uint32_t)first_byte & 0x1F) << 6
					| (second_byte & 0x3F);
				if (value < 128) {
					// overlong
					return (size_t)-1;
				}
				*c = (uint32_t)value;
				return 2;
			} else {
				// incomplete code point
				return (size_t)-2;
			}
		}
		if ((first_byte & 0xF0) == 0xE0) {
			// three-byte code point
			if (bytes >= 3) {
				++p;
				uint32_t second_byte = *p;
				if ((second_byte & 0xC0) != 0x80) return (size_t)-1;
				++p;
				uint32_t third_byte = *p;
				if ((third_byte & 0xC0) != 0x80) return (size_t)-1;
				uint32_t value = ((uint32_t)first_byte & 0x0F) << 12
					| (second_byte & 0x3F) << 6
					| (third_byte & 0x3F);
				if ((value < 0xD800 || value > 0xDFFF) && value >= 0x800) {
					*c = (uint32_t)value;
					return 3;
				} else {
					// overlong or UTF-16 surrogate halves
					return (size_t)-1;
				}
			} else {
				// incomplete
				return (size_t)-2;
			}
		}
		if ((first_byte & 0xF8) == 0xF0) {
			// four-byte code point
			if (bytes >= 4) {
				++p;
				uint32_t second_byte = *p;
				if ((second_byte & 0xC0) != 0x80) return (size_t)-1;
				++p;
				uint32_t third_byte = *p;
				if ((third_byte & 0xC0) != 0x80) return (size_t)-1;
				++p;
				uint32_t fourth_byte = *p;
				if ((fourth_byte & 0xC0) != 0x80) return (size_t)-1;
				uint32_t value = ((uint32_t)first_byte & 0x07) << 18
					| (second_byte & 0x3F) << 12
					| (third_byte  & 0x3F) << 6
					| (fourth_byte & 0x3F);
				if (value >= 0x10000 && value <= 0x10FFFF) {
					*c = (uint32_t)value;
					return 4;
				} else {
					// overlong or value too large.
					return (size_t)-1;
				}
			} else {
				// incomplete
				return (size_t)-2;
			}
		}
		// invalid UTF-8
		return (size_t)-1;
	} else {
		// ASCII character
		if (first_byte == 0) {
			return 0;
		}
		*c = first_byte;
		return 1;
	}
}

/// A lot like c32rtomb
///
/// Converts a UTF-32 codepoint to a UTF-8 string. Writes at most 4 bytes to s.
/// NOTE: It is YOUR JOB to null-terminate your string if the UTF-32 isn't null-terminated!
/// Returns the number of bytes written to `s`, or `(size_t)-1` on invalid UTF-32.
static size_t unicode_utf32_to_utf8(char *s, uint32_t c32) {
	uint8_t *p = (uint8_t *)s;
	if (c32 <= 0x7F) {
		// ASCII
		*p = (uint8_t)c32;
		return 1;
	} else if (c32 <= 0x7FF) {
		// two bytes needed
		*p++ = (uint8_t)(0xC0 | (c32 >> 6));
		*p   = (uint8_t)(0x80 | (c32 & 0x3F));
		return 2;
	} else if (c32 <= 0xFFFF) {
		if (c32 < 0xD800 || c32 > 0xDFFF) {
			*p++ = (uint8_t)(0xE0 | ( c32 >> 12));
			*p++ = (uint8_t)(0x80 | ((c32 >> 6) & 0x3F));
			*p   = (uint8_t)(0x80 | ( c32       & 0x3F));
			return 3;
		} else {
			// UTF-16 surrogate halves
			*p = 0;
			return (size_t)-1;
		}
	} else if (c32 <= 0x10FFFF) {
		*p++ = (uint8_t)(0xF0 | ( c32 >> 18));
		*p++ = (uint8_t)(0x80 | ((c32 >> 12) & 0x3F));
		*p++ = (uint8_t)(0x80 | ((c32 >>  6) & 0x3F));
		*p   = (uint8_t)(0x80 | ( c32        & 0x3F));
		return 4;
	} else {
		// code point too big
		*p = 0;
		return (size_t)-1;
	}
}


// get the number of UTF-16 codepoints needed to encode `str`.
///
// returns `(size_t)-1` on bad UTF-8
static size_t unicode_utf16_len(const char *str) {
	size_t len = 0;
	uint32_t c = 0;
	while (*str) {
		size_t n = unicode_utf8_to_utf32(&c, str, 4);
		if (n >= (size_t)-2)
			return (size_t)-1;
		if (c >= 0x10000)
			len += 2;
		else
			len += 1;
		str += n;
	}
	return len;
}

// get the number of UTF-32 codepoints needed to encode `str`.
///
// returns `(size_t)-1` on bad UTF-8
static size_t unicode_utf32_len(const char *str) {
	size_t len = 0;
	uint32_t c = 0;
	while (*str) {
		size_t n = unicode_utf8_to_utf32(&c, str, 4);
		if (n >= (size_t)-2)
			return (size_t)-1;
		++len;
		str += n;
	}
	return len;
}

/// returns the UTF-8 offset from `str` which corresponds to a UTF-16 offset of
/// `utf16_offset` (rounds down if `utf16_offset` is in the middle of a codepoint).
///
/// returns `strlen(str)` if `utf16_offset == unicode_utf16_len(str)`
/// returns `(size_t)-1` on bad UTF-8, or if `utf16_offset > unicode_utf16_len(str)`
static size_t unicode_utf16_to_utf8_offset(const char *str, size_t utf16_offset) {
	size_t offset = 0;
	uint32_t c = 0;
	while (*str) {
		size_t n = unicode_utf8_to_utf32(&c, str, 4);
		if (n >= (size_t)-2)
			return (size_t)-1;
		size_t u = c >= 0x10000 ? 2 : 1;
		if (utf16_offset < u)
			return offset;
		utf16_offset -= u;
		offset += n;
		str += n;
	}
	if (utf16_offset == 0)
		return offset;
	return SIZE_MAX;
}

static bool unicode_is_valid_utf8(const char *cstr) {
	uint32_t c = 0;
	while (*cstr) {
		size_t n = unicode_utf8_to_utf32(&c, cstr, 4);
		if (n >= (size_t)-2)
			return false;
		cstr += n;
	}
	return true;
}

#endif // UNICODE_H_
