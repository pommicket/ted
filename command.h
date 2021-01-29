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

	// insertion
	CMD_TAB, // insert '\t'
	CMD_NEWLINE, // insert '\n' + autoindent -- also used to submit line buffers

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
	CMD_NEW,
	CMD_UNDO,
	CMD_REDO,
	CMD_COPY,
	CMD_CUT,
	CMD_PASTE,

	CMD_TAB_CLOSE,
	CMD_TAB_NEXT,
	CMD_TAB_PREV,

	CMD_TEXT_SIZE_INCREASE,
	CMD_TEXT_SIZE_DECREASE,

	CMD_ESCAPE, // by default this is the escape key. closes menus, etc.

	CMD_COUNT
} ENUM_U16_END(Command);

typedef struct {
	char const *name;
	Command cmd;
} CommandName;
static CommandName const command_names[CMD_COUNT] = {
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
	{"select-all", CMD_SELECT_ALL},
	{"page-up", CMD_PAGE_UP},
	{"page-down", CMD_PAGE_DOWN},
	{"tab", CMD_TAB},
	{"newline", CMD_NEWLINE},
	{"backspace", CMD_BACKSPACE},
	{"delete", CMD_DELETE},
	{"backspace-word", CMD_BACKSPACE_WORD},
	{"delete-word", CMD_DELETE_WORD},
	{"open", CMD_OPEN},
	{"new", CMD_NEW},
	{"save", CMD_SAVE},
	{"save-as", CMD_SAVE_AS},
	{"undo", CMD_UNDO},
	{"redo", CMD_REDO},
	{"copy", CMD_COPY},
	{"cut", CMD_CUT},
	{"paste", CMD_PASTE},
	{"tab-close", CMD_TAB_CLOSE},
	{"tab-next", CMD_TAB_NEXT},
	{"tab-prev", CMD_TAB_PREV},
	{"increase-text-size", CMD_TEXT_SIZE_INCREASE},
	{"decrease-text-size", CMD_TEXT_SIZE_DECREASE},
	{"escape", CMD_ESCAPE}
};

