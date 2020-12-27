#ifndef TEXT_H_
#define TEXT_H_

#include <uchar.h>

// A text-rendering interface.
// You can either use the simple API (text_render)
// or the character-by-character API (text_chars_begin, text_chars_end, text_render_char)


typedef struct Font Font;

typedef struct {
	float x, y;
	// points at which the text should be cut off
	float min_x, max_x, min_y, max_y;
} TextRenderState;

extern bool text_has_err(void);
// Get the current error. Errors will NOT be overwritten with newer errors.
extern char const *text_get_err(void);
// Clear the current error.
extern void text_clear_err(void);
// Load a TTF font found in ttf_filename with the given font size (character pixel height)
extern Font *text_font_load(char const *ttf_filename, float font_size);
// Height of a character of this font in pixels.
extern float text_font_char_height(Font *font);
// Width of the character 'a' of this font in pixels.
// This is meant to be only used for monospace fonts.
extern float text_font_char_width(Font *font);
// Render some UTF-8 text to the screen (simple interface).
extern void text_render(Font *font, char const *text, float x, float y);
// Get the dimensions of some text.
extern void text_get_size(Font *font, char const *text, float *width, float *height);
// Write text, but using a state, starting at (x, y) -- state->x and state->y are ignored. This allows you to control min/max_x/y.
extern void text_render_with_state(Font *font, TextRenderState *state, char const *text, float x, float y);
// Begin writing characters.
extern void text_chars_begin(Font *font);
// Finish writing characters.
extern void text_chars_end(Font *font);
// Render a single character.
extern void text_render_char(Font *font, TextRenderState *state, char32_t c);
// Free memory used by font.
extern void text_font_free(Font *font);

#endif
