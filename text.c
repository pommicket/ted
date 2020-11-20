#include "base.h"
no_warn_start
#include "stb_truetype.h"
no_warn_end
#include <stdarg.h>
#include <stdlib.h>

typedef struct {
	float char_height;
	u32 nchars;
	GLuint texture;
	stbtt_bakedchar chars[];
} Font;

static char text_err[200];
static void text_clear_err(void) {
	text_err[0] = '\0';
}
static void text_set_err(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsnprintf(text_err, sizeof text_err - 1, fmt, args);
	va_end(args);
}

static bool text_has_err(void) {
	return text_err[0] != '\0';
}

char const *text_get_err(void) {
	return text_err;
}

Font *text_font_load(char const *ttf_filename, float font_size) {
	Font *font = NULL;
	u32 nchars = 128;
	FILE *ttf_file = fopen(ttf_filename, "rb");
	
	text_clear_err();

	if (ttf_file) {
		fseek(ttf_file, 0, SEEK_END);
		u32 file_size = (u32)ftell(ttf_file);
		if (file_size < (50UL<<20)) { // fonts aren't usually bigger than 50 MB
			char *file_data = calloc(1, file_size);
			Font *font = calloc(1, sizeof *font + nchars * sizeof *font->chars);
			if (file_data && font) {
				font->nchars = nchars;
				font->char_height = font_size;
				stbtt_
			} else {
				text_set_err("Not enough memory for font.");
			}
			free(file_data);
			if (text_has_err())
				free(font);
			fclose(ttf_file);
		} else {
			text_set_err("Font file too big (%u megabytes).", (uint)(file_size >> 20));
		}
	} else {
		text_set_err("Couldn't open font file: %s.", ttf_filename);
	}
	return NULL;
}

void text_render(Font *font, char const *text, float x, float y) {
}

void text_get_size(Font *font, char const *text, float *width, float *height) {
}
