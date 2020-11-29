#ifndef UNICODE_H_
#define UNICODE_H_
#define UNICODE_BOX_CHARACTER 0x2610
#define UNICODE_CODE_POINTS 0x110000 // number of Unicode code points

static bool unicode_is_start_of_code_point(u8 byte) {
	// see https://en.wikipedia.org/wiki/UTF-8#Encoding
	// continuation bytes are of the form 10xxxxxx
	return (byte & 0xC0) != 0x80;
}

#endif
