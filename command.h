// i | ARG_STRING = ted->strings[i]
#define ARG_STRING 0x4000000000000000

ENUM_U16 {
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

	CMD_GOTO_DEFINITION, // "go to definition of..."
	CMD_GOTO_LINE, // open "goto line..." menu

	CMD_SPLIT_HORIZONTAL,
	CMD_SPLIT_VERTICAL,
	CMD_SPLIT_JOIN,
	CMD_SPLIT_SWITCH, // switch to the other side of a split
	CMD_SPLIT_SWAP, // swap which side is which in a split.

	CMD_ESCAPE, // by default this is the escape key. closes menus, etc.

	CMD_COUNT
} ENUM_U16_END(Command);

typedef struct {
	char const *name;
	Command cmd;
} CommandName;
static CommandName const command_names[] = {
	{"unknown", CMD_UNKNOWN},
	{"noop", CMD_NOOP},
	{"left", CMD_LEFT},
	{"right", CMD_RIGHT},
	{"up", CMD_UP},
	{"down", CMD_DOWN},
	{"select-left", CMD_SELECT_LEFT},
	{"select-right", CMD_SELECT_RIGHT},
	{"select-up", CMD_SELECT_UP},
	{"select-down", CMD_SELECT_DOWN},
	{"left-word", CMD_LEFT_WORD},
	{"right-word", CMD_RIGHT_WORD},
	{"select-left-word", CMD_SELECT_LEFT_WORD},
	{"select-right-word", CMD_SELECT_RIGHT_WORD},
	{"start-of-line", CMD_START_OF_LINE},
	{"end-of-line", CMD_END_OF_LINE},
	{"select-start-of-line", CMD_SELECT_START_OF_LINE},
	{"select-end-of-line", CMD_SELECT_END_OF_LINE},
	{"start-of-file", CMD_START_OF_FILE},
	{"end-of-file", CMD_END_OF_FILE},
	{"select-start-of-file", CMD_SELECT_START_OF_FILE},
	{"select-end-of-file", CMD_SELECT_END_OF_FILE},
	{"select-page-up", CMD_SELECT_PAGE_UP},
	{"select-page-down", CMD_SELECT_PAGE_DOWN},
	{"select-all", CMD_SELECT_ALL},
	{"page-up", CMD_PAGE_UP},
	{"page-down", CMD_PAGE_DOWN},
	{"tab", CMD_TAB},
	{"backtab", CMD_BACKTAB},
	{"insert-text", CMD_INSERT_TEXT},
	{"newline", CMD_NEWLINE},
	{"newline-back", CMD_NEWLINE_BACK},
	{"comment-selection", CMD_COMMENT_SELECTION},
	{"backspace", CMD_BACKSPACE},
	{"delete", CMD_DELETE},
	{"backspace-word", CMD_BACKSPACE_WORD},
	{"delete-word", CMD_DELETE_WORD},
	{"open", CMD_OPEN},
	{"new", CMD_NEW},
	{"save", CMD_SAVE},
	{"save-as", CMD_SAVE_AS},
	{"save-all", CMD_SAVE_ALL},
	{"reload-all", CMD_RELOAD_ALL},
	{"quit", CMD_QUIT},
	{"set-language", CMD_SET_LANGUAGE},
	{"command-selector", CMD_COMMAND_SELECTOR},
	{"open-config", CMD_OPEN_CONFIG},
	{"undo", CMD_UNDO},
	{"redo", CMD_REDO},
	{"copy", CMD_COPY},
	{"cut", CMD_CUT},
	{"paste", CMD_PASTE},
	{"autocomplete", CMD_AUTOCOMPLETE},
	{"autocomplete-back", CMD_AUTOCOMPLETE_BACK},
	{"find", CMD_FIND},
	{"find-replace", CMD_FIND_REPLACE},
	{"tab-close", CMD_TAB_CLOSE},
	{"tab-switch", CMD_TAB_SWITCH},
	{"tab-next", CMD_TAB_NEXT},
	{"tab-prev", CMD_TAB_PREV},
	{"tab-move-left", CMD_TAB_MOVE_LEFT},
	{"tab-move-right", CMD_TAB_MOVE_RIGHT},
	{"increase-text-size", CMD_TEXT_SIZE_INCREASE},
	{"decrease-text-size", CMD_TEXT_SIZE_DECREASE},
	{"view-only", CMD_VIEW_ONLY},
	{"build", CMD_BUILD},
	{"build-prev-error", CMD_BUILD_PREV_ERROR},
	{"build-next-error", CMD_BUILD_NEXT_ERROR},
	{"shell", CMD_SHELL},
	{"generate-tags", CMD_GENERATE_TAGS},
	{"goto-definition", CMD_GOTO_DEFINITION},
	{"goto-line", CMD_GOTO_LINE},
	{"split-horizontal", CMD_SPLIT_HORIZONTAL},
	{"split-vertical", CMD_SPLIT_VERTICAL},
	{"split-join", CMD_SPLIT_JOIN},
	{"split-switch", CMD_SPLIT_SWITCH},
	{"split-swap", CMD_SPLIT_SWAP},
	{"escape", CMD_ESCAPE},
};

static_assert_if_possible(arr_count(command_names) == CMD_COUNT)

