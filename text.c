#include "base.h"
#include "text.h"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
no_warn_start
#include "stb_truetype.h"
no_warn_end
#include <stdarg.h>
#include <stdlib.h>
#include <GL/gl.h>

struct Font {
	float char_height;
	GLuint texture;
	u32 nchars;
	stbtt_bakedchar chars[];
};

static char text_err[200];
static void text_clear_err(void) {
	text_err[0] = '\0';
}

static bool text_has_err(void) {
	return text_err[0] != '\0';
}

char const *text_get_err(void) {
	return text_err;
}

static void text_set_err(char const *fmt, ...) {
	if (!text_has_err()) {
		va_list args;
		va_start(args, fmt);
		vsnprintf(text_err, sizeof text_err - 1, fmt, args);
		va_end(args);
	}
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
			u8 *file_data = calloc(1, file_size);
			font = calloc(1, sizeof *font + nchars * sizeof *font->chars);
			if (file_data && font) {
				if (fread(file_data, 1, file_size, ttf_file) == file_size) {
					font->nchars = nchars;
					font->char_height = font_size;

					for (int bitmap_width = 256, bitmap_height = 256; bitmap_width <= 4096; bitmap_width *= 2, bitmap_height *= 2) {
						u8 *bitmap = calloc((size_t)bitmap_width, (size_t)bitmap_height);
						if (bitmap) {
							int err = stbtt_BakeFontBitmap(file_data, 0, font->char_height, bitmap,
								bitmap_width, bitmap_height, 0, (int)font->nchars, font->chars);
							if (err > 0) {
								// font converted to bitmap successfully.
								GLuint texture = 0;
								glGenTextures(1, &texture);
								glBindTexture(GL_TEXTURE_2D, texture);
								glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, bitmap_width, bitmap_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
								glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							#if DEBUG
								debug_println("Loaded font %s with %dx%d bitmap.", ttf_filename, bitmap_width, bitmap_height);
							#endif
								font->texture = texture;
								if (glGetError()) {
									text_set_err("Couldn't create texture for font.");
								}
							}
						} else {
							text_set_err("Not enough memory for font bitmap.");
						}
						free(bitmap);
						if (font->texture) { // if font loaded successfully
							break;
						}
					}
					if (!font->texture && !text_has_err()) {
						text_set_err("Couldn't convert font to bitmap.");
					}
				} else {
					text_set_err("Couldn't read font file.", ttf_filename);
				}
			} else {
				text_set_err("Not enough memory for font.");
			}
			free(file_data);
			if (text_has_err()) {
				free(font);
				font = NULL;
			}
			fclose(ttf_file);
		} else {
			text_set_err("Font file too big (%u megabytes).", (uint)(file_size >> 20));
		}
	} else {
		text_set_err("Couldn't open font file.", ttf_filename);
	}
	return font;
}

#if 0
void text_render(Font *font, char const *text, float x, float y) {
	
}

void text_get_size(Font *font, char const *text, float *width, float *height) {
}
#endif
