#define TED_PATH_MAX 256

#define TEXT_SIZE_MIN 6
#define TEXT_SIZE_MAX 70

typedef struct {
	float cursor_blink_time_on, cursor_blink_time_off;
	u32 colors[COLOR_COUNT];
	u16 text_size;
	u16 max_menu_width;
	u8 tab_width;
	u8 cursor_width;
	u8 undo_save_time;
	u8 border_thickness;
	u8 padding;
	u8 scrolloff;
} Settings;

#define SCANCODE_COUNT 0x120 // SDL scancodes should be less than this value.
// a "key combo" is some subset of {control, shift, alt} + some key.
#define KEY_COMBO_COUNT (SCANCODE_COUNT << 3)
#define KEY_MODIFIER_CTRL_BIT 0
#define KEY_MODIFIER_SHIFT_BIT 1
#define KEY_MODIFIER_ALT_BIT 2
#define KEY_MODIFIER_CTRL ((u32)1<<KEY_MODIFIER_CTRL_BIT)
#define KEY_MODIFIER_SHIFT ((u32)1<<KEY_MODIFIER_SHIFT_BIT)
#define KEY_MODIFIER_ALT ((u32)1<<KEY_MODIFIER_ALT_BIT)
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
	bool modified; // has the buffer been modified since it was loaded/saved?
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

// file entries for file selectors
typedef struct {
	char *name; // just the file name
	char *path; // full path
	FsType type;
} FileEntry;

typedef struct {
	Rect bounds;
	u32 n_entries;
	u32 selected; // which FileEntry is currently selected
	float scroll;
	FileEntry *entries;
	char cwd[TED_PATH_MAX];
	bool open; // is the file selector on screen?
	bool submitted; // set to true if the line buffer was just submitted this frame.
} FileSelector;

typedef struct Ted {
	Font *font_bold;
	Font *font;
	TextBuffer *active_buffer;
	// buffer we are currently drag-to-selecting in, if any
	TextBuffer *drag_buffer;
	// while a menu or something is open, there is no active buffer. when the menu is closed,
	// the old active buffer needs to be restored. that's what this stores.
	TextBuffer *prev_active_buffer; 
	Settings settings;
	float window_width, window_height;
	v2 mouse_pos;
	u8 nmouse_clicks[4]; // nmouse_clicks[i] = length of mouse_clicks[i]
	v2 mouse_clicks[4][32]; // mouse_clicks[SDL_BUTTON_RIGHT], for example, is all the right mouse-clicks that have happened this frame
	int scroll_total_x, scroll_total_y; // total amount scrolled in the x and y direction this frame
	Menu menu;
	FileSelector file_selector;
	TextBuffer line_buffer; // general-purpose line buffer for inputs -- used for menus
	TextBuffer main_buffer;
	KeyAction key_actions[KEY_COMBO_COUNT];
	char cwd[TED_PATH_MAX]; // current working directory
	char error[256];
} Ted;

// should the working directory be searched for files? set to true if the executable isn't "installed"
static bool ted_search_cwd = false;
static char const ted_global_data_dir[] = 
#if _WIN32
	"C:\\Program Files\\ted";
#else
	"/usr/share/ted";
#endif

// filled out in main()
static char ted_local_data_dir[TED_PATH_MAX];
static char ted_home[TED_PATH_MAX]; // home directory -- this is what ~ expands to
