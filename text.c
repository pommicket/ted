#include "base.h"
#include "text.h"
#include "unicode.h"
#if DEBUG
typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
} stbtt_bakedchar;
typedef struct
{
   float x0,y0,s0,t0; // top-left
   float x1,y1,s1,t1; // bottom-right
} stbtt_aligned_quad;

extern void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int opengl_fillrule); 
extern int stbtt_BakeFontBitmap(const unsigned char *data, int offset, float pixel_height, unsigned char *pixels, int pw, int ph, int first_char, int num_chars, stbtt_bakedchar *chardata); 
#else
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
no_warn_start
#include "lib/stb_truetype.h"
no_warn_end
#endif


#include <stdlib.h>

// We split up code points into a bunch of pages, so we don't have to load all of the font at
// once into one texture.
#define CHAR_PAGE_SIZE 2048
#define CHAR_PAGE_COUNT UNICODE_CODE_POINTS / CHAR_PAGE_SIZE

typedef struct {
	v2 pos;
	v2 tex_coord;
	v4 color;
} TextVertex;

typedef struct {
	TextVertex v1, v2, v3;
} TextTriangle;

struct Font {
	bool force_monospace;
	float char_width; // width of the character 'a'. calculated when font is loaded.
	float char_height;
	GLuint textures[CHAR_PAGE_COUNT];
	int tex_widths[CHAR_PAGE_COUNT], tex_heights[CHAR_PAGE_COUNT];
	stbtt_bakedchar *char_pages[CHAR_PAGE_COUNT]; // character pages. NULL if the page hasn't been loaded yet.
	// TTF data (i.e. the contents of the TTF file)
	u8 *ttf_data;
	TextTriangle *triangles[CHAR_PAGE_COUNT]; // triangles to render for each page
};

TextRenderState const text_render_state_default = {
	.render = true,
	.wrap = false,
	.x = 0, .y = 0,
	.min_x = -FLT_MAX, .max_x = +FLT_MAX,
	.min_y = -FLT_MAX, .max_y = +FLT_MAX,
	.color = {1, 0, 1, 1},
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

static GLuint text_program;
static GLuint text_vbo, text_vao;
static GLuint text_v_pos, text_v_color, text_v_tex_coord;
static GLint  text_u_sampler;

static bool text_init(void) {
	char const *vshader_code = "attribute vec4 v_color;\n\
attribute vec2 v_pos;\n\
attribute vec2 v_tex_coord;\n\
varying vec4 color;\n\
varying vec2 tex_coord;\n\
void main() {\n\
	color = v_color;\n\
	tex_coord = v_tex_coord;\n\
	gl_Position = vec4(v_pos, 0.0, 1.0);\n\
}\n\
";
	char const *fshader_code = "varying vec4 color;\n\
varying vec2 tex_coord;\n\
uniform sampler2D sampler;\n\
void main() {\n\
	vec4 tex_color = texture2D(sampler, tex_coord);\n\
	gl_FragColor = vec4(1.0, 1.0, 1.0, tex_color.x) * color;\n\
}\n\
";

	text_program = gl_compile_and_link_shaders(NULL, vshader_code, fshader_code);
	text_v_pos = gl_attrib_loc(text_program, "v_pos");
	text_v_color = gl_attrib_loc(text_program, "v_color");
	text_v_tex_coord = gl_attrib_loc(text_program, "v_tex_coord");
	text_u_sampler = gl_uniform_loc(text_program, "sampler");
	glGenBuffers(1, &text_vbo);
	glGenVertexArrays(1, &text_vao);

	return true;
}

static Status text_load_char_page(Font *font, int page) {
	if (font->char_pages[page]) {
		// already loaded
		return true;
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
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmap_width, bitmap_height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			#if DEBUG
				debug_println("Loaded font page %p:%03d with %dx%d bitmap as texture %u.", (void *)font, page, bitmap_width, bitmap_height, texture);
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
		return false;
	}
	return true;
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
					if (text_load_char_page(font, 0)) { // load page with Latin text, etc.
						// calculate width of the character 'a'
						stbtt_aligned_quad q = {0};
						float x = 0, y = 0;
						stbtt_GetBakedQuad(font->char_pages[0], font->tex_widths[0], font->tex_heights[0],
							'a', &x, &y, &q, 1);
						font->char_width = x;
					}
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

void text_font_set_force_monospace(Font *font, bool force) {
	font->force_monospace = force;
}

float text_font_char_height(Font *font) {
	return font->char_height;
}

float text_font_char_width(Font *font) {
	return font->char_width;
}

void text_render(Font *font) {
	for (uint i = 0; i < CHAR_PAGE_COUNT; ++i) {
		if (font->triangles[i]) {
			// render these triangles
			size_t ntriangles = arr_len(font->triangles[i]);

			// convert coordinates to NDC
			for (size_t t = 0; t < ntriangles; ++t) {
				TextTriangle *triangle = &font->triangles[i][t];
				gl_convert_to_ndc(&triangle->v1.pos);
				gl_convert_to_ndc(&triangle->v2.pos);
				gl_convert_to_ndc(&triangle->v3.pos);
			}
		
			if (gl_version_major >= 3)
				glBindVertexArray(text_vao);
			glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ntriangles * sizeof(TextTriangle)), font->triangles[i], GL_STREAM_DRAW);
			glVertexAttribPointer(text_v_pos, 2, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, pos));
			glEnableVertexAttribArray(text_v_pos);
			glVertexAttribPointer(text_v_tex_coord, 2, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, tex_coord));
			glEnableVertexAttribArray(text_v_tex_coord);
			glVertexAttribPointer(text_v_color, 4, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, color));
			glEnableVertexAttribArray(text_v_color);
			glUseProgram(text_program);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, font->textures[i]);
			glUniform1i(text_u_sampler, 0);
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(3 * ntriangles));
			arr_clear(font->triangles[i]);
		}
	}
}

void text_char_with_state(Font *font, TextRenderState *state, char32_t c) {
top:
	if (c >= 0x40000 && c < 0xE0000){
		// these Unicode code points are currently unassigned. replace them with a Unicode box.
		// (specifically, we don't want to use extra memory for pages which
		// won't even have any valid characters in them)
		c = UNICODE_BOX_CHARACTER;
	}
	if (c >= UNICODE_CODE_POINTS) c = UNICODE_BOX_CHARACTER; // code points this big should never appear in valid Unicode
	uint page = c / CHAR_PAGE_SIZE;
	uint index = c % CHAR_PAGE_SIZE;
	if (state->render) {
		if (!font->char_pages[page])
			if (!text_load_char_page(font, (int)page))
				return;
	}
	stbtt_bakedchar *char_data = font->char_pages[page];
	float const char_height = font->char_height;
	float const char_width = font->char_width;
	if (char_data) { // if page was successfully loaded
		stbtt_aligned_quad q = {0};
		
		{
			float x, y;
			x = (float)(state->x - floor(state->x));
			y = (float)(state->y - floor(state->y));
			y += char_height * 0.75f;
			stbtt_GetBakedQuad(char_data, font->tex_widths[page], font->tex_heights[page],
				(int)index, &x, &y, &q, 1);
			y -= char_height * 0.75f;
			
			q.x0 += (float)floor(state->x);
			q.y0 += (float)floor(state->y);
			q.x1 += (float)floor(state->x);
			q.y1 += (float)floor(state->y);
			
			if (font->force_monospace) {
				state->x += char_width; // ignore actual character width
			} else {
				state->x = x + floor(state->x);
				state->y = y + floor(state->y);
			}
		}
		
		float s0 = q.s0, t0 = q.t0;
		float s1 = q.s1, t1 = q.t1;
		float x0 = q.x0, y0 = q.y0;
		float x1 = q.x1, y1 = q.y1;
		float const min_x = state->min_x, max_x = state->max_x;
		float const min_y = state->min_y, max_y = state->max_y;
		
		if (state->wrap && x1 >= max_x) {
			state->x = min_x;
			state->y += char_height;
			goto top;
		}

		if (x0 > max_x || y0 > max_y || x1 < min_x || y1 < min_y)
			return;
		if (x0 < min_x) {
			// left side of character is clipped
			s0 = (min_x-x0) / (x1-x0) * (s1-s0) + s0;
			x0 = min_x;
		}
		if (x1 >= max_x) {
			// right side of character is clipped
			s1 = (max_x-1-x0) / (x1-x0) * (s1-s0) + s0;
			x1 = max_x-1;
		}
		if (y0 < min_y) {
			// top side of character is clipped
			t0 = (min_y-y0) / (y1-y0) * (t1-t0) + t0;
			y0 = min_y;
		}
		if (y1 >= max_y) {
			// bottom side of character is clipped
			t1 = (max_y-1-y0) / (y1-y0) * (t1-t0) + t0;
			y1 = max_y-1;
		}
		if (state->render) {
			float r = state->color[0], g = state->color[1], b = state->color[2], a = state->color[3];
			TextVertex v_1 = {{x0, y0}, {s0, t0}, {r, g, b, a}};
			TextVertex v_2 = {{x0, y1}, {s0, t1}, {r, g, b, a}};
			TextVertex v_3 = {{x1, y1}, {s1, t1}, {r, g, b, a}};
			TextVertex v_4 = {{x1, y0}, {s1, t0}, {r, g, b, a}};
			TextTriangle triangle1 = {v_1, v_2, v_3};
			TextTriangle triangle2 = {v_3, v_4, v_1};
			arr_add(font->triangles[page], triangle1);
			arr_add(font->triangles[page], triangle2);
		}
	}
}

void text_utf8_with_state(Font *font, TextRenderState *state, char const *str) {
	char const *end = str + strlen(str);
	while (str != end) {
		char32_t c = 0;
		size_t ret = unicode_utf8_to_utf32(&c, str, (size_t)(end - str));
		if (ret == 0) {
			break;
		} else if (ret >= (size_t)-2) {
			// invalid UTF-8
			text_char_with_state(font, state, '?');
			++str;
		} else {
			str += ret;
			text_char_with_state(font, state, c);
		}
	}
}

static void text_render_utf8_internal(Font *font, char const *text, double *x, double *y, u32 color, bool render) {
	TextRenderState render_state = text_render_state_default;
	render_state.render = render;
	render_state.x = *x;
	render_state.y = *y;
	rgba_u32_to_floats(color, render_state.color);
	text_utf8_with_state(font, &render_state, text);
	*x = render_state.x;
	*y = render_state.y;
}

void text_utf8(Font *font, char const *text, double x, double y, u32 color) {
	text_render_utf8_internal(font, text, &x, &y, color, true);
}

void text_utf8_anchored(Font *font, char const *text, double x, double y, u32 color, Anchor anchor) {
	float w = 0, h = 0; // width, height of text
	text_get_size(font, text, &w, &h);
	float hw = w * 0.5f, hh = h * 0.5f; // half-width, half-height
	switch (anchor) {
	case ANCHOR_TOP_LEFT: break;
	case ANCHOR_TOP_MIDDLE: x -= hw; break;
	case ANCHOR_TOP_RIGHT: x -= w; break;
	case ANCHOR_MIDDLE_LEFT: y -= hh; break;
	case ANCHOR_MIDDLE: x -= hw; y -= hh; break;
	case ANCHOR_MIDDLE_RIGHT: x -= w; y -= hh; break;
	case ANCHOR_BOTTOM_LEFT: y -= h; break;
	case ANCHOR_BOTTOM_MIDDLE: x -= hw; y -= h; break;
	case ANCHOR_BOTTOM_RIGHT: x -= w; y -= h; break;
	}
	text_utf8(font, text, x, y, color);
}

void text_get_size(Font *font, char const *text, float *width, float *height) {
	double x = 0, y = 0;
	text_render_utf8_internal(font, text, &x, &y, 0, false);
	if (width)  *width = (float)x;
	if (height) *height = (float)y + font->char_height;
}

v2 text_get_size_v2(Font *font, char const *text) {
	v2 v;
	text_get_size(font, text, &v.x, &v.y);
	return v;
}


void text_get_size32(Font *font, char32_t const *text, u64 len, float *width, float *height) {
	TextRenderState render_state = text_render_state_default;
	render_state.render = false;
	for (u64 i = 0; i < len; ++i) {
		text_char_with_state(font, &render_state, text[i]);
	}
	if (width) *width = (float)render_state.x;
	if (height) *height = (float)render_state.y + font->char_height * (2/3.0f);
}

void text_font_free(Font *font) {
	free(font->ttf_data);
	stbtt_bakedchar **char_pages = font->char_pages;
	for (int i = 0; i < CHAR_PAGE_COUNT; ++i) {
		if (char_pages[i]) {
			free(char_pages[i]);
		}
		arr_clear(font->triangles[i]);
	}
	free(font);
}
