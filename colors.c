#include "ted.h"

typedef struct {
	ColorSetting setting;
	const char *name;
} ColorName;

static ColorName const color_names[] = {
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
	{COLOR_ERROR_TEXT, "error-text"},
	{COLOR_ERROR_BG, "error-bg"},
	{COLOR_ERROR_BORDER, "error-border"},
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
	{COLOR_YES, "yes"},
	{COLOR_NO, "no"},
	{COLOR_CANCEL, "cancel"},
	{COLOR_LINE_NUMBERS, "line-numbers"},
	{COLOR_CURSOR_LINE_NUMBER, "cursor-line-number"},
	{COLOR_LINE_NUMBERS_SEPARATOR, "line-numbers-separator"},
};

static_assert_if_possible(arr_count(color_names) == COLOR_COUNT)

ColorSetting color_setting_from_str(const char *str) {
	// @TODO(optimize): sort color_names, binary search
	for (int i = 0; i < COLOR_COUNT; ++i) {
		ColorName const *n = &color_names[i];
		if (streq(n->name, str))
			return n->setting;
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
