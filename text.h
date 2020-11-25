#ifndef TEXT_H_
#define TEXT_H_

#include <uchar.h>

// A text-rendering interface.
// You can either use the simple API (text_render)
// or the character-by-character API (text_chars_begin, text_chars_end, text_render_char)


typedef struct Font Font;

typedef struct {
	float x, y;
	// points at which the text should be cut off in the x and y directions
	float edge_right, edge_bottom;
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
// Render some UTF-8 text to the screen (simple interface).
extern void text_render(Font *font, char const *text, float x, float y);
// Get the dimensions of some text.
extern void text_get_size(Font *font, char const *text, float *width, float *height);
// Begin writing characters.
extern void text_chars_begin(Font *font);
// Finish writing characters.
extern void text_chars_end(Font *font);
// Render a single character.
extern void text_render_char(Font *font, char32_t c, TextRenderState *state);
// Free memory used by font.
extern void text_font_free(Font *font);

#endif
