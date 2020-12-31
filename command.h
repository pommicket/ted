ENUM_U16 {
	CMD_UNKNOWN,
	CMD_NOOP,
	// movement commands
	CMD_LEFT,
	CMD_RIGHT,
	CMD_UP,
	CMD_DOWN,
	CMD_SELECT_LEFT,
	CMD_SELECT_RIGHT,
	CMD_SELECT_UP,
	CMD_SELECT_DOWN,
	CMD_LEFT_WORD,
	CMD_RIGHT_WORD,
	CMD_SELECT_LEFT_WORD,
	CMD_SELECT_RIGHT_WORD,
	CMD_START_OF_LINE,
	CMD_END_OF_LINE,
	CMD_SELECT_START_OF_LINE,
	CMD_SELECT_END_OF_LINE,
	CMD_START_OF_FILE,
	CMD_END_OF_FILE,
	CMD_SELECT_START_OF_FILE,
	CMD_SELECT_END_OF_FILE,

	// deletion
	CMD_BACKSPACE,
	CMD_DELETE,
	CMD_BACKSPACE_WORD,
	CMD_DELETE_WORD,

	CMD_SAVE,
	CMD_UNDO,
	CMD_REDO,

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
	{"backspace", CMD_BACKSPACE},
	{"delete", CMD_DELETE},
	{"backspace-word", CMD_BACKSPACE_WORD},
	{"delete-word", CMD_DELETE_WORD},
	{"save", CMD_SAVE},
	{"undo", CMD_UNDO},
	{"redo", CMD_REDO}
};

