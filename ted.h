#define TED_PATH_MAX 256

#define TEXT_SIZE_MIN 6
#define TEXT_SIZE_MAX 70

typedef struct {
	float cursor_blink_time_on, cursor_blink_time_off;
	u32 colors[COLOR_COUNT];
	u16 text_size;
	u8 tab_width;
	u8 cursor_width;
	u8 undo_save_time;
	u8 border_thickness;
} Settings;

#define SCANCODE_COUNT 0x120 // SDL scancodes should be less than this value.
// a "key combo" is some subset of {control, shift, alt} + some key.
#define KEY_COMBO_COUNT (SCANCODE_COUNT << 3)
#define KEY_MODIFIER_CTRL_BIT 0
#define KEY_MODIFIER_SHIFT_BIT 1
#define KEY_MODIFIER_ALT_BIT 2
#define KEY_MODIFIER_CTRL (1<<KEY_MODIFIER_CTRL_BIT)
#define KEY_MODIFIER_SHIFT (1<<KEY_MODIFIER_SHIFT_BIT)
#define KEY_MODIFIER_ALT (1<<KEY_MODIFIER_ALT_BIT)
// ctrl+alt+c is encoded as SDL_SCANCODE_C << 3 | KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT

typedef struct KeyAction {
	u32 line_number; // config line number where this was set
	Command command; // this will be 0 (COMMAND_UNKNOWN) if there's no action for the key
	i64 argument;
} KeyAction;

// a position in the buffer
typedef struct {
	u32 line;
	u32 index; // index of character in line (not the same as column, since a tab is settings->tab_width columns)
} BufferPos;

typedef struct {
	u32 len;
	u32 capacity;
	char32_t *str;
} Line;

// this refers to replacing prev_len characters (found in prev_text) at pos with new_len characters
typedef struct BufferEdit {
	BufferPos pos;
	u32 new_len;
	u32 prev_len;
	char32_t *prev_text;
	double time; // time at start of edit (i.e. the time just before the edit), in seconds since epoch
} BufferEdit;

typedef struct {
	char *filename; // NULL if this buffer doesn't correspond to a file (e.g. line buffers)
	struct Ted *ted; // we keep a back-pointer to the ted instance so we don't have to pass it in to every buffer function
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	BufferPos cursor_pos;
	BufferPos selection_pos; // if selection is true, the text between selection_pos and cursor_pos is selected.
	bool is_line_buffer; // "line buffers" are buffers which can only have one line of text (used for inputs)
	bool selection;
	bool store_undo_events; // set to false to disable undo events
	float x1, y1, x2, y2;
	u32 nlines;
	u32 lines_capacity;
	Line *lines;
	char error[128];
	BufferEdit *undo_history; // dynamic array of undo history
	BufferEdit *redo_history; // dynamic array of redo history
} TextBuffer;

ENUM_U16 {
	MENU_NONE,
	MENU_OPEN
} ENUM_U16_END(Menu);

typedef struct Ted {
	Font *font;
	TextBuffer *active_buffer;
	// while a menu or something is open, there is no active buffer. when the menu is closed,
	// the old active buffer needs to be restored. that's what this stores.
	TextBuffer *prev_active_buffer; 
	Settings settings;
	float window_width, window_height;
	Menu menu;
	TextBuffer line_buffer; // general-purpose line buffer for inputs -- used for menus
	TextBuffer main_buffer;
	KeyAction key_actions[KEY_COMBO_COUNT];
	char error[256];
} Ted;
