// color names and functions for dealing with colors

#include "ted.h"

typedef struct {
	ColorSetting setting;
	const char *name;
} ColorName;

static ColorName color_names[] = {
	{COLOR_UNKNOWN, "unknown"},
	{COLOR_TEXT, "text"},
	{COLOR_TEXT_SECONDARY, "text-secondary"},
	{COLOR_BG, "bg"},
	{COLOR_HL, "hl"},
	{COLOR_CURSOR, "cursor"},
	{COLOR_CURSOR_ERROR, "cursor-error"},
	{COLOR_CURSOR_LINE_BG, "cursor-line-bg"},
	{COLOR_VIEW_ONLY_CURSOR, "view-only-cursor"},
	{COLOR_VIEW_ONLY_SELECTION_BG, "view-only-selection-bg"},
	{COLOR_MATCHING_BRACKET_HL, "matching-bracket-hl"},
	{COLOR_BORDER, "border"},
	{COLOR_TEXT_FOLDER, "text-folder"},
	{COLOR_TEXT_OTHER, "text-other"},
	{COLOR_SELECTION_BG, "selection-bg"},
	{COLOR_MENU_BACKDROP, "menu-backdrop"},
	{COLOR_MENU_BG, "menu-bg"},
	{COLOR_MENU_HL, "menu-hl"},
	{COLOR_ERROR_BG, "error-bg"},
	{COLOR_ERROR_BORDER, "error-border"},
	{COLOR_INFO_BG, "info-bg"},
	{COLOR_INFO_BORDER, "info-border"},
	{COLOR_WARNING_BG, "warning-bg"},
	{COLOR_WARNING_BORDER, "warning-border"},
	{COLOR_ACTIVE_TAB_HL, "active-tab-hl"},
	{COLOR_SELECTED_TAB_HL, "selected-tab-hl"},
	{COLOR_FIND_HL, "find-hl"},
	{COLOR_KEYWORD, "keyword"},
	{COLOR_BUILTIN, "builtin"},
	{COLOR_COMMENT, "comment"},
	{COLOR_PREPROCESSOR, "preprocessor"},
	{COLOR_STRING, "string"},
	{COLOR_CHARACTER, "character"},
	{COLOR_CONSTANT, "constant"},
	{COLOR_AUTOCOMPLETE_BG, "autocomplete-bg"},
	{COLOR_AUTOCOMPLETE_HL, "autocomplete-hl"},
	{COLOR_AUTOCOMPLETE_BORDER, "autocomplete-border"},
	{COLOR_AUTOCOMPLETE_VARIABLE, "autocomplete-variable"},
	{COLOR_AUTOCOMPLETE_FUNCTION, "autocomplete-function"},
	{COLOR_AUTOCOMPLETE_TYPE, "autocomplete-type"},
	{COLOR_HOVER_BORDER, "hover-border"},
	{COLOR_HOVER_BG, "hover-bg"},
	{COLOR_HOVER_TEXT, "hover-text"},
	{COLOR_HOVER_HL, "hover-hl"},
	{COLOR_HL_WRITE, "hl-write"},
	{COLOR_YES, "yes"},
	{COLOR_NO, "no"},
	{COLOR_CANCEL, "cancel"},
	{COLOR_LINE_NUMBERS, "line-numbers"},
	{COLOR_CURSOR_LINE_NUMBER, "cursor-line-number"},
	{COLOR_LINE_NUMBERS_SEPARATOR, "line-numbers-separator"},
};

static_assert_if_possible(arr_count(color_names) == COLOR_COUNT)

int color_name_cmp(const void *av, const void *bv) {
	const ColorName *a = av, *b = bv;
	return strcmp(a->name, b->name);
}

void color_init(void) {
	qsort(color_names, arr_count(color_names), sizeof *color_names, color_name_cmp);
}

ColorSetting color_setting_from_str(const char *str) {
	int lo = 0;
	int hi = COLOR_COUNT;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		int cmp = strcmp(color_names[mid].name, str);
		if (cmp < 0) {
			lo = mid + 1;
		} else if (cmp > 0) {
			hi = mid;
		} else {
			return color_names[mid].setting;
		}
	}
	return COLOR_UNKNOWN;
}

const char *color_setting_to_str(ColorSetting s) {
	for (int i = 0; i < COLOR_COUNT; ++i) {
		ColorName const *n = &color_names[i];
		if (n->setting == s)
			return n->name;
	}
	return "???";
}

// converts #rrggbb/#rrggbbaa to a color. returns false if it's not in the right format.
Status color_from_str(const char *str, u32 *color) {
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


ColorSetting color_for_symbol_kind(SymbolKind kind) {
	switch (kind) {
	case SYMBOL_CONSTANT:
		return COLOR_CONSTANT;
	case SYMBOL_TYPE:
		return COLOR_AUTOCOMPLETE_TYPE;
	case SYMBOL_FIELD:
	case SYMBOL_VARIABLE:
		return COLOR_AUTOCOMPLETE_VARIABLE;
	case SYMBOL_FUNCTION:
		return COLOR_AUTOCOMPLETE_FUNCTION;
	case SYMBOL_OTHER:
		return COLOR_TEXT;
	case SYMBOL_KEYWORD:
		return COLOR_KEYWORD;
	}
	return COLOR_TEXT;
}

u32 color_blend(u32 bg, u32 fg) {
	u32 r1 = bg >> 24;
	u32 g1 = (bg >> 16) & 0xff;
	u32 b1 = (bg >> 8) & 0xff;
	u32 r2 = fg >> 24;
	u32 g2 = (fg >> 16) & 0xff;
	u32 b2 = (fg >> 8) & 0xff;
	u32 a2 = fg & 0xff;
	u32 r = (r1 * (255 - a2) + r2 * a2 + 127) / 255;
	u32 g = (g1 * (255 - a2) + g2 * a2 + 127) / 255;
	u32 b = (b1 * (255 - a2) + b2 * a2 + 127) / 255;
	return r << 24 | g << 16 | b << 8 | 0xff;
}

u32 color_apply_opacity(u32 color, float opacity) {
	opacity = clampf(opacity, 0.0f, 1.0f);
	return (color & 0xffffff00) | (u32)((color & 0xff) * opacity);
}
