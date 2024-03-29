// color names and functions for dealing with colors

#include "ted-internal.h"

typedef struct {
	ColorSetting setting;
	const char *name;
} ColorName;

static ColorName color_names[] = {
	{COLOR_UNKNOWN, "unknown"},
	{COLOR_TEXT, "text"},
	{COLOR_TEXT_SECONDARY, "text-secondary"},
	{COLOR_BG, "bg"},
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
	{COLOR_TODO, "todo"},
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
	u32 r = 0, g = 0, b = 0, a = 0xff;
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


static float color_relative_luminance(const float rgb[3]) {
	// see https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
	float c[3];
	for (int i = 0; i < 3; ++i) {
		const float x = rgb[i];
		c[i] = x <= 0.03928f ? x * (1.0f / 12.92f) : powf((x + 0.055f) * (1.0f / 1.055f), 2.4f);
	}
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

float color_contrast_ratio(const float rgb1[3], const float rgb2[3]) {
	// see https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef
	float l1 = color_relative_luminance(rgb1);
	float l2 = color_relative_luminance(rgb2);
	if (l1 < l2) {
		float temp = l1;
		l1 = l2;
		l2 = temp;
	}
	return (l1 + 0.05f) / (l2 + 0.05f);
}

float color_contrast_ratio_u32(u32 color1, u32 color2) {
	float rgb1[4], rgb2[4];
	color_u32_to_floats(color1, rgb1);
	color_u32_to_floats(color2, rgb2);
	return color_contrast_ratio(rgb1, rgb2);
}

void color_u32_to_floats(u32 rgba, float floats[4]) {
	floats[0] = (float)((rgba >> 24) & 0xff) / 255.f;
	floats[1] = (float)((rgba >> 16) & 0xff) / 255.f;
	floats[2] = (float)((rgba >>  8) & 0xff) / 255.f;
	floats[3] = (float)((rgba >>  0) & 0xff) / 255.f;
}

vec4 color_u32_to_vec4(u32 rgba) {
	float c[4];
	color_u32_to_floats(rgba, c);
	return (vec4){c[0], c[1], c[2], c[3]};
}

u32 color_vec4_to_u32(vec4 color) {
	return (u32)(color.x * 255) << 24
		| (u32)(color.y * 255) << 16
		| (u32)(color.z * 255) << 8
		| (u32)(color.w * 255);
}

static vec4 color_rgba_to_hsva(vec4 rgba) {
	float R = rgba.x, G = rgba.y, B = rgba.z, A = rgba.w;
	float M = maxf(R, maxf(G, B));
	float m = minf(R, minf(G, B));
	float C = M - m;
	float H = 0;
	if (C == 0)
		H = 0;
	else if (M == R)
		H = fmodf((G - B) / C, 6);
	else if (M == G)
		H = (B - R) / C + 2;
	else if (M == B)
		H = (R - G) / C + 4;
	H *= 60;
	float V = M;
	float S = V == 0 ? 0 : C / V;
	return (vec4){H, S, V, A};
}

static vec4 color_hsva_to_rgba(vec4 hsva) {
	float H = hsva.x, S = hsva.y, V = hsva.z, A = hsva.w;
	H /= 60;
	float C = S * V;
	float X = C * (1 - fabsf(fmodf(H, 2) - 1));
	float R, G, B;
	if (H <= 1)
		R=C, G=X, B=0;
	else if (H <= 2)
		R=X, G=C, B=0;
	else if (H <= 3)
		R=0, G=C, B=X;
	else if (H <= 4)
		R=0, G=X, B=C;
	else if (H <= 5)
		R=X, G=0, B=C;
	else
		R=C, G=0, B=X;
	
	float m = V-C;
	R += m;
	G += m;
	B += m;
	return (vec4){R, G, B, A};
}

u32 color_interpolate(float x, u32 color1, u32 color2) {
	x = x * x * (3 - 2*x); // hermite interpolation

	vec4 c1 = color_u32_to_vec4(color1), c2 = color_u32_to_vec4(color2);
	// to make it interpolate more nicely, convert to hsv, interpolate in that space, then convert back
	c1 = color_rgba_to_hsva(c1);
	c2 = color_rgba_to_hsva(c2);
	// v_1/2 named differently to avoid shadowing
	float h1 = c1.x, s1 = c1.y, v_1 = c1.z, a1 = c1.w;
	float h2 = c2.x, s2 = c2.y, v_2 = c2.z, a2 = c2.w;
	
	float s_out = lerpf(x, s1, s2);
	float v_out = lerpf(x, v_1, v_2);
	float a_out = lerpf(x, a1, a2);
	
	float h_out;
	// because hue is on a circle, we need to make sure we take the shorter route around the circle
	if (fabsf(h1 - h2) < 180) {
		h_out = lerpf(x, h1, h2);
	} else if (h1 > h2) {
		h_out = lerpf(x, h1, h2 + 360);
	} else {
		h_out = lerpf(x, h1 + 360, h2);
	}
	h_out = fmodf(h_out, 360);
	
	vec4 c_out = (vec4){h_out, s_out, v_out, a_out};
	c_out = color_hsva_to_rgba(c_out);
	return color_vec4_to_u32(c_out);
}
