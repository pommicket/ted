static float find_menu_height(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);
	Settings const *settings = &ted->settings;
	float padding = settings->padding;

	return char_height_bold + char_height + 4 * padding;
}

static void find_menu_frame(Ted *ted) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	float const char_height = text_font_char_height(font),
		char_height_bold = text_font_char_height(font_bold);

	Settings const *settings = &ted->settings;
	float const padding = settings->padding;
	float const menu_height = find_menu_height(ted);
	float const window_width = ted->window_width, window_height = ted->window_height;
	u32 const *colors = settings->colors;

	Rect menu_bounds = rect(V2(padding, window_height - menu_height), V2(window_width - 2*padding, menu_height - padding));

	gl_geometry_rect_border(menu_bounds, 1, colors[COLOR_BORDER]);

	gl_geometry_draw();
	(void)char_height; (void)char_height_bold;
}
