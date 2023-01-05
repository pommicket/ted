// command enum

#ifndef COMMAND_H_
#define COMMAND_H_

// `i | ARG_STRING` when used as an argument refers to `ted->strings[i]`
#define ARG_STRING 0x4000000000000000

typedef enum {
	CMD_UNKNOWN,
	CMD_NOOP, // do nothing
	// movement and selection commands
	CMD_LEFT, // move cursor left
	CMD_RIGHT, // move cursor right
	CMD_UP, // move cursor up
	CMD_DOWN, // move cursor down
	CMD_SELECT_LEFT, // move cursor left, and select
	CMD_SELECT_RIGHT,
	CMD_SELECT_UP,
	CMD_SELECT_DOWN,
	CMD_LEFT_WORD, // move cursor left a word
	CMD_RIGHT_WORD,
	CMD_UP_BLANK_LINE,
	CMD_DOWN_BLANK_LINE,
	CMD_SELECT_LEFT_WORD,
	CMD_SELECT_RIGHT_WORD,
	CMD_START_OF_LINE, // move cursor to start of line
	CMD_END_OF_LINE, // move cursor to end of line
	CMD_SELECT_START_OF_LINE, // select to start of line
	CMD_SELECT_END_OF_LINE, // select to end of line
	CMD_START_OF_FILE, // move cursor to start of buffer
	CMD_END_OF_FILE, // move cursor to end of buffer
	CMD_SELECT_START_OF_FILE,
	CMD_SELECT_END_OF_FILE,
	CMD_SELECT_ALL, // select entire buffer
	CMD_SELECT_PAGE_UP,
	CMD_SELECT_PAGE_DOWN,
	CMD_SELECT_UP_BLANK_LINE,
	CMD_SELECT_DOWN_BLANK_LINE,
	
	// insertion
	CMD_INSERT_TEXT, // insert text
	CMD_TAB, // insert '\t'
	CMD_BACKTAB,
	CMD_NEWLINE, // insert '\n' + autoindent -- also used to submit line buffers
	CMD_NEWLINE_BACK,
	CMD_COMMENT_SELECTION,

	// scrolling
	CMD_PAGE_UP, // move cursor up one page up (where one page is however tall the buffer is)
	CMD_PAGE_DOWN,

	// deletion
	CMD_BACKSPACE,
	CMD_DELETE,
	CMD_BACKSPACE_WORD,
	CMD_DELETE_WORD,

	CMD_OPEN, // open a file
	CMD_SAVE, // save current buffer
	CMD_SAVE_AS,
	CMD_SAVE_ALL, // save all open buffers with unsaved changes
	CMD_NEW,
	CMD_UNDO,
	CMD_REDO,
	CMD_COMMAND_SELECTOR,
	CMD_OPEN_CONFIG,
	CMD_RELOAD_ALL, // reload all buffers from file
	CMD_QUIT,
	
	// IDE features
	CMD_SET_LANGUAGE,
	CMD_AUTOCOMPLETE,
	CMD_AUTOCOMPLETE_BACK,
	CMD_FIND_USAGES,
	CMD_GOTO_DEFINITION, // "go to definition of..." menu
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
	CMD_TAB_SWITCH, // argument = index of tab (starting at 0)
	CMD_TAB_NEXT,
	CMD_TAB_PREV,
	CMD_TAB_MOVE_LEFT,
	CMD_TAB_MOVE_RIGHT,

	CMD_TEXT_SIZE_INCREASE,
	CMD_TEXT_SIZE_DECREASE,

	CMD_VIEW_ONLY, // toggle view-only mode

	CMD_BUILD,
	CMD_BUILD_PREV_ERROR,
	CMD_BUILD_NEXT_ERROR,
	CMD_SHELL,
	CMD_GENERATE_TAGS,

	CMD_GOTO_LINE, // open "goto line..." menu

	CMD_SPLIT_HORIZONTAL,
	CMD_SPLIT_VERTICAL,
	CMD_SPLIT_JOIN,
	CMD_SPLIT_SWITCH, // switch to the other side of a split
	CMD_SPLIT_SWAP, // swap which side is which in a split.

	CMD_ESCAPE, // by default this is the escape key. closes menus, etc.

	CMD_COUNT
} Command;


#endif
