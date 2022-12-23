typedef enum {
	COLOR_UNKNOWN,

	COLOR_TEXT,
	COLOR_TEXT_SECONDARY,
	COLOR_BG,
	COLOR_HL,
	COLOR_CURSOR,
	COLOR_CURSOR_ERROR,
	COLOR_CURSOR_LINE_BG,
	COLOR_SELECTION_BG,
	COLOR_VIEW_ONLY_CURSOR,
	COLOR_VIEW_ONLY_SELECTION_BG,
	COLOR_MATCHING_BRACKET_HL,
	COLOR_BORDER,
	COLOR_TEXT_FOLDER,
	COLOR_TEXT_OTHER,
	COLOR_MENU_BACKDROP,
	COLOR_MENU_BG,
	COLOR_MENU_HL,
	COLOR_ERROR_TEXT,
	COLOR_ERROR_BG,
	COLOR_ERROR_BORDER,
	COLOR_ACTIVE_TAB_HL,
	COLOR_SELECTED_TAB_HL,
	COLOR_FIND_HL,

	COLOR_YES,
	COLOR_NO,
	COLOR_CANCEL,

	COLOR_KEYWORD,
	COLOR_BUILTIN,
	COLOR_COMMENT,
	COLOR_PREPROCESSOR,
	COLOR_STRING,
	COLOR_CHARACTER,
	COLOR_CONSTANT,

	COLOR_LINE_NUMBERS,
	COLOR_CURSOR_LINE_NUMBER,
	COLOR_LINE_NUMBERS_SEPARATOR,
	

	COLOR_COUNT
} ColorSetting;

typedef struct {
	ColorSetting setting;
	char const *name;
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
	{COLOR_YES, "yes"},
	{COLOR_NO, "no"},
	{COLOR_CANCEL, "cancel"},
	{COLOR_LINE_NUMBERS, "line-numbers"},
	{COLOR_CURSOR_LINE_NUMBER, "cursor-line-number"},
	{COLOR_LINE_NUMBERS_SEPARATOR, "line-numbers-separator"},
};

static_assert_if_possible(arr_count(color_names) == COLOR_COUNT)
