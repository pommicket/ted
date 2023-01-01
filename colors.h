#ifndef COLORS_H_
#define COLORS_H_

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
	
	COLOR_AUTOCOMPLETE_BG,
	COLOR_AUTOCOMPLETE_HL,
	COLOR_AUTOCOMPLETE_BORDER,
	COLOR_AUTOCOMPLETE_FUNCTION,
	COLOR_AUTOCOMPLETE_VARIABLE,
	COLOR_AUTOCOMPLETE_TYPE,
	
	COLOR_HOVER_BG,
	COLOR_HOVER_BORDER,
	COLOR_HOVER_TEXT,
	COLOR_HOVER_HL,

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

#endif // COLORS_H_
