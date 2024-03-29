/// \file
/// A text-rendering interface.
///
/// Example usage:
///
/// ```
/// Font *font = text_font_load("font.ttf", 18);
/// if (font) {
///     text_utf8(font, "Hello", 5, 5, 0xFF0000FF);
///     text_utf8(font, "Goodbye", 5, 100, 0x00FF00FF);
///     text_render(font);
/// }
/// ```

#ifndef TEXT_H_
#define TEXT_H_

#include "base.h"
#include "util.h"

/// a font
typedef struct Font Font;

/// text render state.
///
/// do not construct this directly instead use \ref text_render_state_default.
typedef struct {
	/// should the text actually be rendered (set to false to get text size)
	bool render;
	/// should the text wrap around to min_x when it reaches max_x? NOTE: this is character-by-character wrapping, not word wrap
	bool wrap;
	/// where to draw
	double x, y;
	/// points where the text should be cut off
	float min_x, max_x, min_y, max_y;
	/// `[0] = r, [1] = g, [2] = b, [3] = a`.
	float color[4];
	
	/// largest x achieved (for computing size)
	double x_largest;
	/// largest y achieved (for computing size)
	double y_largest;
	
	/// index of previous glyph rendered, or 0 if this is the first
	int prev_glyph;
	
	/// added to x for rendering
	/// this exists for complicated reasons
	/// basically we want a way of consistently getting the size
	/// of text without error from floating point imprecision
	float x_render_offset;
	
	/// used for forwards-compatibility
	char _reserved[64];
} TextRenderState;

/// text anchor
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

/// returns false on error.
bool text_init(void);
/// is there error?
bool text_has_err(void);
/// Get the current error. Errors will NOT be overwritten with newer errors.
const char *text_get_err(void);
/// Clear the current error.
void text_clear_err(void);
/// Load a TTF font found in ttf_filename with the given font size (character pixel height)
Font *text_font_load(const char *ttf_filename, float font_size);
/// Set a fallback font to use if a character is not defined by `font`.
///
/// You can pass `NULL` to clear any previous fallback.
/// Do not create a loop of fallback fonts.
void text_font_set_fallback(Font *font, Font *fallback);
/// Change size of font.
///
/// Avoid calling this function too often, since all font textures are trashed.
/// Also changes size of fallback fonts.
void text_font_change_size(Font *font, float new_size);
/// Height of a character of this font in pixels.
float text_font_char_height(Font *font);
/// Width of the given character in pixels.
float text_font_char_width(Font *font, char32_t c);
/// Force text to advance by text_font_char_width(font, ' ') pixels per character (actually, per code point).
void text_font_set_force_monospace(Font *font, bool force);
/// Get the dimensions of some text.
void text_get_size(Font *font, const char *text, float *width, float *height);
/// Get the dimensions of some text.
vec2 text_get_size_vec2(Font *font, const char *text);
/// Get the dimensions of some text.
void text_get_size32(Font *font, const char32_t *text, u64 len, float *width, float *height);
/// Draw some text.
void text_utf8(Font *font, const char *text, double x, double y, u32 color);
/// Draw some text with an anchor.
void text_utf8_anchored(Font *font, const char *text, double x, double y, u32 color, Anchor anchor);
/// Draw a single character.
void text_char_with_state(Font *font, TextRenderState *state, char32_t c);
/// Draw some UTF-8 text with a \ref TextRenderState.
void text_utf8_with_state(Font *font, TextRenderState *state, const char *str);
/// Used to indicate that the next character drawn should not
/// kern with the previous character.
///
/// Use this when you go to the next line or something.
void text_state_break_kerning(TextRenderState *state);
/// Free memory used by font.
///
/// Does NOT free the font's fallback.
void text_font_free(Font *font);
/// Render all text drawn with \ref text_utf8, etc.
///
/// This will render the fallback font and its fallback, and so on.
void text_render(Font *font);
/// The "default" text rendering state - everything you need to just render text normally.
/// This lets you do stuff like:
/// ```
/// TextRenderState state = text_render_state_default;
///    (set a few options)
/// text_render_with_state(font, &state, ...)
/// ```
extern const TextRenderState text_render_state_default;

#endif
