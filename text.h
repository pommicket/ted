#ifndef TEXT_H_
#define TEXT_H_

#include "base.h"
#include "util.h"

// A text-rendering interface.
// Example usage:
// Font *font = text_font_load("font.ttf", 18);
// if (font) {
//     text_utf8(font, "Hello", 5, 5, 0xFF0000FF);
//     text_utf8(font, "Goodbye", 5, 100, 0x00FF00FF);
//     text_render(font);
// }


typedef struct Font Font;

typedef struct {
	// should the text actually be rendered (set to false to get text size)
	bool render;
	bool wrap; // should the text wrap around to min_x when it reaches max_x? NOTE: this is character-by-character wrapping, not word wrap

	double x, y;
	// points where the text should be cut off
	float min_x, max_x, min_y, max_y;
	// [0] = r, [1] = g, [2] = b, [3] = a.
	float color[4];
	
	// largest x & y achieved (for computing size)
	double x_largest;
	double y_largest;
} TextRenderState;

typedef enum {
	ANCHOR_TOP_LEFT,
	ANCHOR_TOP_MIDDLE,
	ANCHOR_TOP_RIGHT,
	ANCHOR_MIDDLE_LEFT,
	ANCHOR_MIDDLE,
	ANCHOR_MIDDLE_RIGHT,
	ANCHOR_BOTTOM_LEFT,
	ANCHOR_BOTTOM_MIDDLE,
	ANCHOR_BOTTOM_RIGHT,
} Anchor;

bool text_init(void);
bool text_has_err(void);
// Get the current error. Errors will NOT be overwritten with newer errors.
const char *text_get_err(void);
// Clear the current error.
void text_clear_err(void);
// Load a TTF font found in ttf_filename with the given font size (character pixel height)
Font *text_font_load(const char *ttf_filename, float font_size);
// Height of a character of this font in pixels.
float text_font_char_height(Font *font);
// Width of the character 'a' of this font in pixels.
// This is meant to be only used for monospace fonts.
float text_font_char_width(Font *font);
// Force text to advance by text_font_char_width(font) pixels per character (actually, per code point).
void text_font_set_force_monospace(Font *font, bool force);
// Get the dimensions of some text.
void text_get_size(Font *font, const char *text, float *width, float *height);
v2 text_get_size_v2(Font *font, const char *text);
void text_get_size32(Font *font, const char32_t *text, u64 len, float *width, float *height);
void text_utf8(Font *font, const char *text, double x, double y, u32 color);
void text_utf8_anchored(Font *font, const char *text, double x, double y, u32 color, Anchor anchor);
void text_char_with_state(Font *font, TextRenderState *state, char32_t c);
void text_utf8_with_state(Font *font, TextRenderState *state, const char *str);
// Free memory used by font.
void text_font_free(Font *font);
void text_render(Font *font);

// The "default" text rendering state - everything you need to just render text normally.
// This lets you do stuff like:
// TextRenderState state = text_render_state_default;
//    (set a few options)
// text_render_with_state(font, &state, ...)
extern const TextRenderState text_render_state_default;

#endif
