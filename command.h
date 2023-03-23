/// \file
/// command enum

#ifndef COMMAND_H_
#define COMMAND_H_

/// command enum
///
/// more documentation in `ted.cfg`.
typedef enum {
	CMD_UNKNOWN,
	/// do nothing
	CMD_NOOP,
	// movement and selection commands
	/// move cursor left
	CMD_LEFT,
	/// move cursor right
	CMD_RIGHT,
	/// move cursor up
	CMD_UP,
	/// move cursor down
	CMD_DOWN,
	/// move cursor left, and select
	CMD_SELECT_LEFT,
	CMD_SELECT_RIGHT,
	CMD_SELECT_UP,
	CMD_SELECT_DOWN,
	/// move cursor left a word
	CMD_LEFT_WORD,
	CMD_RIGHT_WORD,
	CMD_UP_BLANK_LINE,
	CMD_DOWN_BLANK_LINE,
	CMD_SELECT_LEFT_WORD,
	CMD_SELECT_RIGHT_WORD,
	/// move cursor to start of line
	CMD_START_OF_LINE,
	/// move cursor to end of line
	CMD_END_OF_LINE,
	/// select to start of line
	CMD_SELECT_START_OF_LINE,
	/// select to end of line
	CMD_SELECT_END_OF_LINE,
	/// move cursor to start of buffer
	CMD_START_OF_FILE,
	/// move cursor to end of buffer
	CMD_END_OF_FILE,
	/// go to previous position
	CMD_PREVIOUS_POSITION,
	CMD_SELECT_START_OF_FILE,
	CMD_SELECT_END_OF_FILE,
	/// select entire buffer
	CMD_SELECT_ALL,
	CMD_SELECT_PAGE_UP,
	CMD_SELECT_PAGE_DOWN,
	CMD_SELECT_UP_BLANK_LINE,
	CMD_SELECT_DOWN_BLANK_LINE,
	CMD_CLEAR_SELECTION,
	
	// insertion
	/// insert text
	CMD_INSERT_TEXT,
	/// insert `\t`
	CMD_TAB,
	CMD_BACKTAB,
	/// insert `\n` + autoindent -- also used to submit line buffers
	CMD_NEWLINE,
	CMD_NEWLINE_BACK,
	CMD_COMMENT_SELECTION,

	// scrolling
	/// move cursor up one page up (where one page is however tall the buffer is)
	CMD_PAGE_UP,
	CMD_PAGE_DOWN,

	// deletion
	CMD_BACKSPACE,
	CMD_DELETE,
	CMD_BACKSPACE_WORD,
	CMD_DELETE_WORD,

	/// open a file
	CMD_OPEN,
	/// save current buffer
	CMD_SAVE,
	CMD_SAVE_AS,
	/// save all open buffers with unsaved changes
	CMD_SAVE_ALL,
	CMD_NEW,
	CMD_UNDO,
	CMD_REDO,
	CMD_COMMAND_SELECTOR,
	CMD_OPEN_CONFIG,
	/// reload all buffers from file
	CMD_RELOAD_ALL,
	CMD_QUIT,
	
	// IDE features
	CMD_SET_LANGUAGE,
	CMD_AUTOCOMPLETE,
	CMD_AUTOCOMPLETE_BACK,
	CMD_FIND_USAGES,
	/// "go to definition of..." menu
	CMD_GOTO_DEFINITION,
	CMD_GOTO_DEFINITION_AT_CURSOR,
	CMD_GOTO_DECLARATION_AT_CURSOR,
	CMD_GOTO_TYPE_DEFINITION_AT_CURSOR,
	CMD_LSP_RESET,
	
	CMD_COPY,
	CMD_CUT,
	CMD_PASTE,
	CMD_FIND,
	CMD_FIND_REPLACE,

	CMD_TAB_CLOSE,
	/// argument = index of tab (starting at 0)
	CMD_TAB_SWITCH,
	CMD_TAB_NEXT,
	CMD_TAB_PREV,
	CMD_TAB_MOVE_LEFT,
	CMD_TAB_MOVE_RIGHT,

	CMD_TEXT_SIZE_INCREASE,
	CMD_TEXT_SIZE_DECREASE,

	/// toggle view-only mode
	CMD_VIEW_ONLY,

	CMD_BUILD,
	CMD_BUILD_PREV_ERROR,
	CMD_BUILD_NEXT_ERROR,
	CMD_SHELL,
	CMD_GENERATE_TAGS,

	/// open "goto line..." menu
	CMD_GOTO_LINE,

	CMD_SPLIT_HORIZONTAL,
	CMD_SPLIT_VERTICAL,
	CMD_SPLIT_JOIN,
	/// switch to the other side of a split
	CMD_SPLIT_SWITCH,
	/// swap which side is which in a split.
	CMD_SPLIT_SWAP,

	/// by default this is the escape key. closes menus, etc.
	CMD_ESCAPE,
	
	CMD_MACRO_RECORD,
	CMD_MACRO_STOP,
	CMD_MACRO_EXECUTE,

	CMD_COUNT
} Command;


#endif
