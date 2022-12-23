
static ColorSetting color_setting_from_str(char const *str) {
	// @OPTIMIZE: sort color_names, binary search
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
	bool success = false;
	switch (strlen(str)) {
	case 4:
		success = sscanf(str, "#%01x%01x%01x", &r, &g, &b) == 3;
		// extend single hex digit to double hex digit
		r |= r << 4;
		g |= g << 4;
		b |= b << 4;
		break;
	case 5:
		success = sscanf(str, "#%01x%01x%01x%01x", &r, &g, &b, &a) == 4;
		r |= r << 4;
		g |= g << 4;
		b |= b << 4;
		a |= a << 4;
		break;
	case 7:
		success = sscanf(str, "#%02x%02x%02x", &r, &g, &b) == 3;
		break;
	case 9:
		success = sscanf(str, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4;
		break;
	}
	if (!success || r > 0xff || g > 0xff || b > 0xff || a > 0xff)
		return false;
	if (color)
		*color = (u32)r << 24 | (u32)g << 16 | (u32)b << 8 | (u32)a;
	return true;
}


static ColorSetting color_for_symbol_kind(SymbolKind kind) {
	switch (kind) {
	case SYMBOL_CONSTANT:
		return COLOR_CONSTANT;
	case SYMBOL_TYPE:
	case SYMBOL_FIELD:
	case SYMBOL_VARIABLE:
	case SYMBOL_OTHER:
	case SYMBOL_FUNCTION:
		return COLOR_TEXT;
	case SYMBOL_KEYWORD:
		return COLOR_KEYWORD;
	}
	return COLOR_TEXT;
}
