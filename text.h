#ifndef TEXT_H_
#define TEXT_H_

typedef struct Font Font;

char const *text_get_err(void);
extern Font *text_font_load(char const *ttf_filename, float font_size);
extern void text_render(Font *font, char const *text, float x, float y);
extern void text_get_size(Font *font, char const *text, float *width, float *height);

#endif
