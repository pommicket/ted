#include "ted-internal.h"
 
no_warn_start
#if DEBUG
#include "lib/stb_rect_pack.h"
#include "lib/stb_truetype.h"
#else
#include "stb_truetype.c"
#endif
no_warn_end

typedef struct {
	vec2 pos;
	vec2 tex_coord;
	vec4 color;
} TextVertex;

typedef struct {
	TextVertex vert1, vert2, vert3;
} TextTriangle;

typedef struct {
	char32_t c;
	int glyph_index;
	u32 texture;
	stbtt_packedchar data;
} CharInfo;

// characters are split into this many "buckets" according to
// their least significant bits. this is to create a Budget Hash Map™.
// must be a power of 2.
#define CHAR_BUCKET_COUNT (1 << 12)

#define FONT_TEXTURE_WIDTH 512 // width of each texture
#define FONT_TEXTURE_HEIGHT 512 // height of each texture

typedef struct {
	GLuint tex;
	bool needs_update;
	unsigned char *pixels;
	stbtt_fontinfo stb_info;
	stbtt_pack_context pack_context;
	TextTriangle *triangles;
} FontTexture;

struct Font {
	bool force_monospace;
	float char_height;
	stbtt_fontinfo stb_info;
	FontTexture *textures; // dynamic array of textures
	CharInfo *char_info[CHAR_BUCKET_COUNT]; // each entry is a dynamic array of char info
	// TTF data (i.e. the contents of the TTF file)
	u8 *ttf_data;
	Font *fallback;
};

const TextRenderState text_render_state_default = {
	.render = true,
	.wrap = false,
	.x = 0, .y = 0,
	.min_x = -FLT_MAX, .max_x = +FLT_MAX,
	.min_y = -FLT_MAX, .max_y = +FLT_MAX,
	.color = {1, 0, 1, 1},
	.x_largest = -FLT_MAX, .y_largest = -FLT_MAX,
	.prev_glyph = 0,
	.x_render_offset = 0,
};

static char text_err[200];
void text_clear_err(void) {
	text_err[0] = '\0';
}

bool text_has_err(void) {
	return text_err[0] != '\0';
}

const char *text_get_err(void) {
	return text_err;
}

static void text_set_err(const char *fmt, ...) {
	if (!text_has_err()) {
		va_list args;
		va_start(args, fmt);
		vsnprintf(text_err, sizeof text_err - 1, fmt, args);
		va_end(args);
	}
}

void text_font_set_fallback(Font *font, Font *fallback) {
	font->fallback = fallback;
}

static GLuint text_program;
static GLuint text_vbo, text_vao;
static GLuint text_v_pos, text_v_color, text_v_tex_coord;
static GLint  text_u_sampler;
static GLint  text_u_window_size;

bool text_init(void) {
	const char *vshader_code = "attribute vec4 v_color;\n\
attribute vec2 v_pos;\n\
attribute vec2 v_tex_coord;\n\
uniform vec2 u_window_size;\n\
OUT vec4 color;\n\
OUT vec2 tex_coord;\n\
void main() {\n\
	color = v_color;\n\
	tex_coord = v_tex_coord;\n\
	vec2 p = v_pos * (2.0 / u_window_size);\n\
	gl_Position = vec4(p.x - 1.0, 1.0 - p.y, 0.0, 1.0);\n\
}\n\
";
	const char *fshader_code = "IN vec4 color;\n\
IN vec2 tex_coord;\n\
uniform sampler2D sampler;\n\
void main() {\n\
	vec4 tex_color = texture2D(sampler, tex_coord);\n\
	gl_FragColor = vec4(1.0, 1.0, 1.0, tex_color.x) * color;\n\
}\n\
";

	text_program = gl_compile_and_link_shaders(NULL, vshader_code, fshader_code);
	text_v_pos = gl_attrib_location(text_program, "v_pos");
	text_v_color = gl_attrib_location(text_program, "v_color");
	text_v_tex_coord = gl_attrib_location(text_program, "v_tex_coord");
	text_u_sampler = gl_uniform_location(text_program, "sampler");
	text_u_window_size = gl_uniform_location(text_program, "u_window_size");
	glGenBuffers(1, &text_vbo);
	glGenVertexArrays(1, &text_vao);

	return true;
}

static u32 char_bucket_index(char32_t c) {
	return c & (CHAR_BUCKET_COUNT - 1);
}


static FontTexture *font_new_texture(Font *font) {
	PROFILE_TIME(start);
	unsigned char *pixels = calloc(FONT_TEXTURE_WIDTH, FONT_TEXTURE_HEIGHT);
	if (!pixels) {
		text_set_err("Not enough memory for font bitmap.");
		return NULL;
	}
	FontTexture *texture = arr_addp(font->textures);
	stbtt_PackBegin(&texture->pack_context, pixels, FONT_TEXTURE_WIDTH, FONT_TEXTURE_HEIGHT,
		FONT_TEXTURE_WIDTH, 1, NULL);
	glGenTextures(1, &texture->tex);
	PROFILE_TIME(end);
	texture->pixels = pixels;
	#if PROFILE
		printf("- create font texture: %.1fms\n", 1e3 * (end - start));
	#endif
	return texture;
}

static void font_texture_update_if_needed(FontTexture *texture) {
	if (texture->needs_update) {
		PROFILE_TIME(start);
		glBindTexture(GL_TEXTURE_2D, texture->tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_TEXTURE_WIDTH, FONT_TEXTURE_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, texture->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		PROFILE_TIME(end);
		
		#if PROFILE
		printf("- update font texture: %.1fms\n", 1e3 * (end - start));
		#endif
		texture->needs_update = false;
	}
}

static void font_texture_free(FontTexture *texture) {
	glDeleteTextures(1, &texture->tex);
	arr_free(texture->triangles);
	if (texture->pixels) {
		free(texture->pixels);
		stbtt_PackEnd(&texture->pack_context);
	}
	memset(texture, 0, sizeof *texture);
}

// on success, *info is filled out.
// success includes cases where c is not defined by the font so a substitute character is used.
// failure only indicates something very bad.
static Status text_load_char(Font *font, char32_t c, CharInfo *info) {
	u32 bucket = char_bucket_index(c);
	arr_foreach_ptr(font->char_info[bucket], CharInfo, i) {
		if (i->c == c) {
			// already loaded
			*info = *i;
			return true;
		}
	}
	
	glGetError(); // clear error
	memset(info, 0, sizeof *info);
	
	info->glyph_index = stbtt_FindGlyphIndex(&font->stb_info, (int)c);
	if (c != UNICODE_BOX_CHARACTER && info->glyph_index == 0) {
		// this code point is not defined by the font
		
		// use the box character
		if (!text_load_char(font, UNICODE_BOX_CHARACTER, info))
			return false;
		info->c = c;
		arr_add(font->char_info[bucket], *info);
		return true;
	}
	
	if (!font->textures) {
		if (!font_new_texture(font))
			return false;
	}
	
	int success = 0;
	FontTexture *texture = arr_lastp(font->textures);
	for (int i = 0; i < 2; i++) {
		info->c = c;
		info->texture = arr_len(font->textures) - 1;
		success = stbtt_PackFontRange(&texture->pack_context, font->ttf_data, 0, font->char_height,
			(int)c, 1, &info->data);
		if (success) break;
		// texture is full; create a new one
		stbtt_PackEnd(&texture->pack_context);
		font_texture_update_if_needed(texture);
		free(texture->pixels);
		texture->pixels = NULL;
		debug_println("Create new texture for font %p (triggered by U+%04X)", (void *)font, c);
		texture = font_new_texture(font);
		if (!texture)
			return false;
	}
	
	if (!success) {
		// a brand new texture couldn't fit the character.
		// something has gone horribly wrong.
		font_texture_free(texture);
		arr_remove_last(font->textures);
		text_set_err("Error rasterizing character %lc", (wchar_t)c);
		return false;
	}
	
	texture->needs_update = true;
	
	arr_add(font->char_info[bucket], *info);
	return true;
}

Font *text_font_load(const char *ttf_filename, float font_size) {
	Font *font = NULL;
	FILE *ttf_file = fopen(ttf_filename, "rb");
	
	text_clear_err();

	if (!ttf_file) {
		text_set_err("Couldn't open font file.", ttf_filename);
		return NULL;
	}
	
	fseek(ttf_file, 0, SEEK_END);
	u32 file_size = (u32)ftell(ttf_file);
	fseek(ttf_file, 0, SEEK_SET);
	if (file_size >= (50UL<<20)) { // fonts aren't usually bigger than 50 MB
		text_set_err("Font file too big (%u megabytes).", (unsigned)(file_size >> 20));
	}
	
	u8 *file_data = NULL;
	if (!text_has_err()) {
		file_data = calloc(1, file_size);
		font = calloc(1, sizeof *font);
		if (!file_data || !font) {
			text_set_err("Not enough memory for font.");
		}
	}
	
	if (!text_has_err()) {
		size_t bytes_read = fread(file_data, 1, file_size, ttf_file);
		if (bytes_read != file_size) {
			text_set_err("Couldn't read font file.");
		}
	}

	
	if (!text_has_err()) {
		font->char_height = font_size;
		font->ttf_data = file_data;
		if (!stbtt_InitFont(&font->stb_info, file_data, 0)) {
			text_set_err("Couldn't process font file - is this a valid TTF file?");
		}
	}
	
	if (text_has_err()) {
		free(file_data);
		free(font);
		font = NULL;
	}
	fclose(ttf_file);
	return font;
}

void text_font_set_force_monospace(Font *font, bool force) {
	font->force_monospace = force;
}

float text_font_char_height(Font *font) {
	return font->char_height;
}

float text_font_char_width(Font *font, char32_t c) {
	CharInfo info = {0};
	if (text_load_char(font, c, &info)) {
		if (!info.glyph_index && font->fallback)
			return text_font_char_width(font->fallback, c);
		return info.data.xadvance;
	} else {
		return 0;
	}
}

void text_render(Font *font) {
	arr_foreach_ptr(font->textures, FontTexture, texture) {
		size_t ntriangles = arr_len(texture->triangles);
		if (!ntriangles) continue;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture->tex);
		font_texture_update_if_needed(texture);
		// render these triangles
		if (gl_version_major >= 3)
			glBindVertexArray(text_vao);
		glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ntriangles * sizeof(TextTriangle)), texture->triangles, GL_STREAM_DRAW);
		glVertexAttribPointer(text_v_pos, 2, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, pos));
		glEnableVertexAttribArray(text_v_pos);
		glVertexAttribPointer(text_v_tex_coord, 2, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, tex_coord));
		glEnableVertexAttribArray(text_v_tex_coord);
		glVertexAttribPointer(text_v_color, 4, GL_FLOAT, 0, sizeof(TextVertex), (void *)offsetof(TextVertex, color));
		glEnableVertexAttribArray(text_v_color);
		glUseProgram(text_program);
		glUniform1i(text_u_sampler, 0);
		glUniform2f(text_u_window_size, gl_window_width, gl_window_height);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(3 * ntriangles));
		arr_clear(texture->triangles);
		glBindTexture(GL_TEXTURE_2D, 0);
		
		// if i remove this i get
		//    Texture state usage warning: The texture object (0) bound to texture image unit 0 does not have a defined base level and cannot be used for texture mapping
		// (even with no other draw calls) which is really weird but whatever this is probably good practice anyways.
		glUseProgram(0);
	}
	
	if (font->fallback) {
		text_render(font->fallback);
	}
}

void text_char_with_state(Font *font, TextRenderState *state, char32_t c) {
	bool wrapped = false;
top:
	if (wrapped) {
		// fuck
		return;
	}
	CharInfo info = {0};

	if (c >= 0x40000 && c < 0xE0000){
		// these Unicode code points are currently unassigned. replace them with a Unicode box.
		// (specifically, we don't want to use extra memory for pages which
		// won't even have any valid characters in them)
		c = UNICODE_BOX_CHARACTER;
	}
	if (c >= UNICODE_CODE_POINTS) c = UNICODE_BOX_CHARACTER; // code points this big should never appear in valid Unicode
	
	if (!text_load_char(font, c, &info))
		return;
	
	if (!info.glyph_index && font->fallback) {
		text_char_with_state(font->fallback, state, c);
		return;
	}
	
	
	const float char_height = font->char_height;
	
	stbtt_aligned_quad q = {0};
	
	if (state->wrap && c == '\n') {
		state->x = state->min_x;
		state->y += char_height;
		goto ret;
	}
	
	if (!font->force_monospace && state->prev_glyph && info.glyph_index) {
		// kerning
		state->x += (float)stbtt_GetGlyphKernAdvance(&font->stb_info,
			(int)state->prev_glyph, (int)info.glyph_index)
			* stbtt_ScaleForPixelHeight(&font->stb_info, font->char_height);
	}
	
	{
		float x, y;
		x = (float)(state->x - floor(state->x));
		y = (float)(state->y - floor(state->y));
		y += char_height * 0.75f;
		stbtt_GetPackedQuad(&info.data, FONT_TEXTURE_WIDTH, FONT_TEXTURE_HEIGHT, 0, &x, &y, &q, 0);
		y -= char_height * 0.75f;
		
		q.x0 += (float)floor(state->x);
		q.y0 += (float)floor(state->y);
		q.x1 += (float)floor(state->x);
		q.y1 += (float)floor(state->y);
		
		if (font->force_monospace) {
			state->x += text_font_char_width(font, ' ');
		} else {
			state->x = x + floor(state->x);
			state->y = y + floor(state->y);
		}
	}
	
	float s0 = q.s0, t0 = q.t0;
	float s1 = q.s1, t1 = q.t1;
	float x0 = roundf(q.x0 + state->x_render_offset), y0 = roundf(q.y0);
	float x1 = roundf(q.x1 + state->x_render_offset), y1 = roundf(q.y1);
	const float min_x = state->min_x, max_x = state->max_x;
	const float min_y = state->min_y, max_y = state->max_y;
	
	if (state->wrap && x1 >= max_x) {
		state->x = min_x;
		state->y += char_height;
		wrapped = true;
		goto top;
	}

	if (x0 > max_x || y0 > max_y || x1 < min_x || y1 < min_y)
		goto ret;
	if (x0 < min_x) {
		// left side of character is clipped
		s0 = (min_x-x0) / (x1-x0) * (s1-s0) + s0;
		x0 = min_x;
	}
	if (x1 > max_x) {
		// right side of character is clipped
		s1 = (max_x-x0) / (x1-x0) * (s1-s0) + s0;
		x1 = max_x;
	}
	if (y0 < min_y) {
		// top side of character is clipped
		t0 = (min_y-y0) / (y1-y0) * (t1-t0) + t0;
		y0 = min_y;
	}
	if (y1 > max_y) {
		// bottom side of character is clipped
		t1 = (max_y-y0) / (y1-y0) * (t1-t0) + t0;
		y1 = max_y;
	}
	if (state->render) {
		float r = state->color[0], g = state->color[1], b = state->color[2], a = state->color[3];
		TextVertex v_1 = {{x0, y0}, {s0, t0}, {r, g, b, a}};
		TextVertex v_2 = {{x0, y1}, {s0, t1}, {r, g, b, a}};
		TextVertex v_3 = {{x1, y1}, {s1, t1}, {r, g, b, a}};
		TextVertex v_4 = {{x1, y0}, {s1, t0}, {r, g, b, a}};
		TextTriangle triangle1 = {v_1, v_2, v_3};
		TextTriangle triangle2 = {v_3, v_4, v_1};
		arr_add(font->textures[info.texture].triangles, triangle1);
		arr_add(font->textures[info.texture].triangles, triangle2);
	}
	ret:
	state->x_largest = maxd(state->x, state->x_largest);
	state->y_largest = maxd(state->y, state->y_largest);
	state->prev_glyph = info.glyph_index;
}

void text_utf8_with_state(Font *font, TextRenderState *state, const char *str) {
	const char *end = str + strlen(str);
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

static vec2 text_render_utf8_internal(Font *font, const char *text, double x, double y, u32 color, bool render) {
	TextRenderState render_state = text_render_state_default;
	render_state.render = render;
	render_state.x = x;
	render_state.y = y;
	color_u32_to_floats(color, render_state.color);
	text_utf8_with_state(font, &render_state, text);
	return (vec2){
		maxf(0.0f, (float)(render_state.x_largest - x)),
		maxf(0.0f, (float)(render_state.y_largest - y))
	};
}

void text_utf8(Font *font, const char *text, double x, double y, u32 color) {
	text_render_utf8_internal(font, text, x, y, color, true);
}

void text_utf8_anchored(Font *font, const char *text, double x, double y, u32 color, Anchor anchor) {
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

void text_get_size(Font *font, const char *text, float *width, float *height) {
	double x = 0, y = 0;
	vec2 size = text_render_utf8_internal(font, text, x, y, 0, false);
	if (width)  *width = size.x;
	if (height) *height = size.y + font->char_height;
}

vec2 text_get_size_vec2(Font *font, const char *text) {
	vec2 v;
	text_get_size(font, text, &v.x, &v.y);
	return v;
}


void text_get_size32(Font *font, const char32_t *text, u64 len, float *width, float *height) {
	TextRenderState render_state = text_render_state_default;
	render_state.render = false;
	for (u64 i = 0; i < len; ++i) {
		text_char_with_state(font, &render_state, text[i]);
	}
	if (width) *width = (float)render_state.x;
	if (height) *height = (float)render_state.y + font->char_height * (2/3.0f);
}


static void font_free_char_info(Font *font) {
	for (u32 i = 0; i < CHAR_BUCKET_COUNT; i++) {
		arr_free(font->char_info[i]);
	}
}

static void font_free_textures(Font *font) {
	arr_foreach_ptr(font->textures, FontTexture, texture) {
		font_texture_free(texture);
	}
	arr_clear(font->textures);
}

void text_state_break_kerning(TextRenderState *state) {
	state->prev_glyph = 0;
}

void text_font_change_size(Font *font, float new_size) {
	font_free_textures(font);
	font_free_char_info(font);
	font->char_height = new_size;
	if (font->fallback)
		text_font_change_size(font->fallback, new_size);
}

void text_font_free(Font *font) {
	free(font->ttf_data);
	font_free_textures(font);
	font_free_char_info(font);
	memset(font, 0, sizeof *font);
	free(font);
}
