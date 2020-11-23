#include "base.h"
#include "text.h"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
no_warn_start
#include "lib/stb_truetype.h"
no_warn_end
#include <stdarg.h>
#include <stdlib.h>
#include <GL/gl.h>

#define UNICODE_CODE_POINTS 0x110000 // number of Unicode code points
// We split up code points into a bunch of pages, so we don't have to load all of the font at
// once into one texture.
#define CHAR_PAGE_SIZE 2048
#define CHAR_PAGE_COUNT UNICODE_CODE_POINTS / CHAR_PAGE_SIZE

struct Font {
	float char_height;
	GLuint textures[CHAR_PAGE_COUNT];
	int tex_widths[CHAR_PAGE_COUNT], tex_heights[CHAR_PAGE_COUNT];
	stbtt_bakedchar *char_pages[CHAR_PAGE_COUNT]; // character pages. NULL if the page hasn't been loaded yet.
	// TTF data (i.e. the contents of the TTF file)
	u8 *ttf_data;
	int curr_page;
};

static char text_err[200];
void text_clear_err(void) {
	text_err[0] = '\0';
}

bool text_has_err(void) {
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

static void text_load_char_page(Font *font, int page) {
	if (font->char_pages[page]) {
		// already loaded
		return;
	}
	font->char_pages[page] = calloc(CHAR_PAGE_SIZE, sizeof *font->char_pages[page]);
	for (int bitmap_width = 128, bitmap_height = 128; bitmap_width <= 4096; bitmap_width *= 2, bitmap_height *= 2) {
		u8 *bitmap = calloc((size_t)bitmap_width, (size_t)bitmap_height);
		if (bitmap) {
			int err = stbtt_BakeFontBitmap(font->ttf_data, 0, font->char_height, bitmap,
				bitmap_width, bitmap_height, page * CHAR_PAGE_SIZE, CHAR_PAGE_SIZE, font->char_pages[page]);
			if (err > 0) {
				// font converted to bitmap successfully.
				GLuint texture = 0;
				glGenTextures(1, &texture);
				glBindTexture(GL_TEXTURE_2D, texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, bitmap_width, bitmap_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			#if DEBUG
				debug_println("Loaded font page %d with %dx%d bitmap as texture %u.", page, bitmap_width, bitmap_height, texture);
			#endif
				font->textures[page] = texture;
				font->tex_widths[page]  = bitmap_width;
				font->tex_heights[page] = bitmap_height;
				GLenum glerr = glGetError();
				if (glerr) {
					text_set_err("Couldn't create texture for font (GL error %u).", glerr);
					if (texture) glDeleteTextures(1, &texture);
					break;
				}
			}
		} else {
			text_set_err("Not enough memory for font bitmap.");
		}
		free(bitmap);
		if (font->textures[page]) { // if font loaded successfully
			break;
		}
	}
	if (!font->textures[page] && !text_has_err()) {
		text_set_err("Couldn't convert font to bitmap.");
	}
	if (text_has_err()) {
		free(font->char_pages[page]);
		font->char_pages[page] = NULL;
	}
}

Font *text_font_load(char const *ttf_filename, float font_size) {
	Font *font = NULL;
	FILE *ttf_file = fopen(ttf_filename, "rb");
	
	text_clear_err();

	if (ttf_file) {
		fseek(ttf_file, 0, SEEK_END);
		u32 file_size = (u32)ftell(ttf_file);
		fseek(ttf_file, 0, SEEK_SET);
		if (file_size < (50UL<<20)) { // fonts aren't usually bigger than 50 MB
			u8 *file_data = calloc(1, file_size);
			font = calloc(1, sizeof *font);
			if (file_data && font) {
				size_t bytes_read = fread(file_data, 1, file_size, ttf_file);
				if (bytes_read == file_size) {
					font->char_height = font_size;
					font->ttf_data = file_data;
					text_load_char_page(font, 0); // load page with Latin text, etc.
					font->curr_page = -1;
				} else {
					text_set_err("Couldn't read font file.");
				}
			} else {
				text_set_err("Not enough memory for font.");
			}
			if (text_has_err()) {
				free(file_data);
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

typedef struct {
	float x, y;
} TextRenderState;

static void text_render_with_page(Font *font, int page) {
	if (font->curr_page != page) {
		if (font->curr_page != -1) {
			// we were rendering chars from another page.
			glEnd(); // stop doing that
		}
		text_load_char_page(font, page); // load the page if necessary
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, font->textures[page]);
		glBegin(GL_QUADS);
		font->curr_page = page;
	}
}

void text_chars_begin(Font *font) {
	text_render_with_page(font, 0); // start by loading Latin text
}

void text_chars_end(Font *font) {
	glEnd();
	glDisable(GL_TEXTURE_2D);
	font->curr_page = -1;
}

static void text_render_char_internal(Font *font, char32_t c, TextRenderState *state) {
	if (c >= 0x30000 && c < 0xE0000){
		// these Unicode code points are currently unassigned. replace them with ☐.
		// (specifically, we don't want to use extra memory for pages which
		// won't even have any valid characters in them)
		c = 0x2610;
	}
	if (c >= UNICODE_CODE_POINTS) c = 0x2610; // code points this big should never appear in valid Unicode
	uint page = c / CHAR_PAGE_SIZE;
	uint index = c % CHAR_PAGE_SIZE;
	text_render_with_page(font, (int)page);
	stbtt_bakedchar *char_data = font->char_pages[page];
	if (char_data) { // if page was successfully loaded
		stbtt_aligned_quad q = {0};
		// because stb_truetype uses down is positive, we need to negate the y
		// coordinate, pass it into the function, then negate it back.
		state->y = -state->y;
		stbtt_GetBakedQuad(char_data, font->tex_widths[page], font->tex_heights[page],
			(int)index, &state->x, &state->y, &q, 1);
		state->y = -state->y;
		glTexCoord2f(q.s0,q.t1); glVertex2f(q.x0,-q.y1);
		glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,-q.y1);
		glTexCoord2f(q.s1,q.t0); glVertex2f(q.x1,-q.y0);
		glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,-q.y0);
	}
}

void text_render_char(Font *font, char32_t c, float *x, float *y) {
	TextRenderState state = {*x, *y};
	text_render_char_internal(font, c, &state);
	*x = state.x;
	*y = state.y;
}

static void text_render_internal(Font *font, char const *text, float *x, float *y) {
	mbstate_t mbstate = {0};
	TextRenderState render_state = {*x, *y};
	text_chars_begin(font);
	char32_t c = 0;
	char const *end = text + strlen(text);
	while (text != end) {
		size_t ret = mbrtoc32(&c, text, (size_t)(end - text), &mbstate);
		if (ret == 0) break;
		if (ret == (size_t)(-2)) { // incomplete multi-byte character
			text_render_char_internal(font, '?', &render_state);
			text = end; // done reading text
		} else if (ret == (size_t)(-1)) {
			// invalid UTF-8; skip this byte
			text_render_char_internal(font, '?', &render_state);
			++text;
		} else {
			if (ret != (size_t)(-3))
				text += ret; // character consists of `ret` bytes
			switch (c) {
			default:
				text_render_char_internal(font, (char32_t)c, &render_state);
				break;
			}
		}
	}
	text_chars_end(font);
	*x = render_state.x;
	*y = render_state.y;
}

void text_render(Font *font, char const *text, float x, float y) {
	text_render_internal(font, text, &x, &y);
}

void text_get_size(Font *font, char const *text, float *width, float *height) {
	float x = 0, y = 0;
	text_render_internal(font, text, &x, &y);
	if (width)  *width = x;
	if (height) *height = y + font->char_height * (2/3.0f);
}

void text_font_free(Font *font) {
	free(font->ttf_data);
	stbtt_bakedchar **char_pages = font->char_pages;
	for (int i = 0; i < CHAR_PAGE_COUNT; ++i) {
		if (char_pages[i]) {
			free(char_pages[i]);
		}
	}
	free(font);
}