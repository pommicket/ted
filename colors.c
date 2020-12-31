ENUM_U16 {
	COLOR_UNKNOWN,

	COLOR_BG,
	COLOR_CURSOR,
	COLOR_CURSOR_LINE_BG,
	COLOR_BORDER,
	COLOR_TEXT,
	COLOR_SELECTION_BG,
	COLOR_SELECTION_TEXT,

	COLOR_COUNT
} ENUM_U16_END(ColorSetting);

typedef struct {
	ColorSetting setting;
	char const *name;
} ColorName;

static ColorName const color_names[COLOR_COUNT] = {
	{COLOR_UNKNOWN, "unknown"},
	{COLOR_BG, "bg"},
	{COLOR_CURSOR, "cursor"},
	{COLOR_CURSOR_LINE_BG, "cursor-line-bg"},
	{COLOR_BORDER, "border"},
	{COLOR_TEXT, "text"},
	{COLOR_SELECTION_BG, "selection-bg"},
	{COLOR_SELECTION_TEXT, "selection-text"}
};

static ColorSetting color_setting_from_str(char const *str) {
	// @OPTIM: sort color_names, binary search
	for (int i = 0; i < COLOR_COUNT; ++i) {
		ColorName const *n = &color_names[i];
		if (streq(n->name, str))
			return n->setting;
	}
	return COLOR_UNKNOWN;
}

static char const *color_setting_to_str(ColorSetting s) {
	for (int i = 0; i < COLOR_COUNT; ++i) {
		ColorName const *n = &color_names[i];
		if (n->setting == s)
			return n->name;
	}
	return "???";
}

// converts #rrggbb/#rrggbbaa to a color. returns false if it's not in the right format.
static Status color_from_str(char const *str, u32 *color) {
	uint r = 0, g = 0, b = 0, a = 0xff;
	size_t len = strlen(str);
	if (len == 7) {
		sscanf(str, "#%02x%02x%02x", &r, &g, &b);
	} else if (len == 9) {
		sscanf(str, "#%02x%02x%02x%02x", &r, &g, &b, &a);
	} else {
		return false;
	}
	if (r > 0xff || g > 0xff || b > 0xff) return false;
	if (color)
		*color = (u32)r << 24 | (u32)g << 16 | (u32)b << 8 | (u32)a;
	return true;
}
