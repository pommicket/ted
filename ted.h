#define TED_PATH_MAX 256
#define TED_UNTITLED "Untitled" // what to call untitled buffers

#define TEXT_SIZE_MIN 6
#define TEXT_SIZE_MAX 70

// these all say "CPP" but really they're C/C++
enum {
	SYNTAX_STATE_CPP_MULTI_LINE_COMMENT = 0x1u, // are we in a multi-line comment? (delineated by /* */)
	SYNTAX_STATE_CPP_SINGLE_LINE_COMMENT = 0x2u, // if you add a \ to the end of a single-line comment, it is continued to the next line.
	SYNTAX_STATE_CPP_PREPROCESSOR = 0x4u, // similar to above
	SYNTAX_STATE_CPP_STRING = 0x8u,
	SYNTAX_STATE_CPP_RAW_STRING = 0x10u,
};

enum {
	SYNTAX_STATE_RUST_COMMENT_DEPTH_MASK = 0xfu, // in rust, /* */ comments can nest.
	SYNTAX_STATE_RUST_COMMENT_DEPTH_MUL  = 0x1u,
	SYNTAX_STATE_RUST_COMMENT_DEPTH_BITS = 4, // number of bits we allocate for the comment depth.
	SYNTAX_STATE_RUST_STRING = 0x10u,
	SYNTAX_STATE_RUST_STRING_IS_RAW = 0x20u,
};

enum {
	SYNTAX_STATE_PYTHON_STRING = 0x01u, // multiline strings (''' and """)
	SYNTAX_STATE_PYTHON_STRING_DBL_QUOTED = 0x02u, // is this a """ string, as opposed to a ''' string?
};

typedef u8 SyntaxState;

ENUM_U16 {
	LANG_NONE,
	LANG_C,
	LANG_CPP,
	LANG_RUST,
	LANG_PYTHON,
	LANG_COUNT
} ENUM_U16_END(Language);

typedef struct {
	Language lang;
	char const *name;
} LanguageName;

static LanguageName const language_names[] = {
	{LANG_NONE, "None"},
	{LANG_C, "C"},
	{LANG_CPP, "C++"},
	{LANG_RUST, "Rust"},
	{LANG_PYTHON, "Python"},
};

ENUM_U8 {
	SYNTAX_NORMAL,
	SYNTAX_KEYWORD,
	SYNTAX_BUILTIN,
	SYNTAX_COMMENT,
	SYNTAX_PREPROCESSOR,
	SYNTAX_STRING,
	SYNTAX_CHARACTER,
	SYNTAX_CONSTANT,
} ENUM_U8_END(SyntaxCharType);

typedef struct {
	float cursor_blink_time_on, cursor_blink_time_off;
	u32 colors[COLOR_COUNT];
	u16 text_size;
	u16 max_menu_width;
	u16 error_display_time;
	bool auto_indent;
	bool auto_add_newline;
	bool syntax_highlighting;
	bool line_numbers;
	bool auto_reload;
	u8 tab_width;
	u8 cursor_width;
	u8 undo_save_time;
	u8 border_thickness;
	u8 padding;
	u8 scrolloff;
	char build_default_command[256];
	// [i] = comma-separated string of file extensions for language i, or NULL for none
	char *language_extensions[LANG_COUNT];
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
	SyntaxState syntax;
	u32 len;
	char32_t *str;
} Line;

// this refers to replacing prev_len characters (found in prev_text) at pos with new_len characters
typedef struct {
	bool chain; // should this + the next edit be treated as one?
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
	struct timespec last_write_time; // last write time to filename.
	BufferPos cursor_pos;
	BufferPos selection_pos; // if selection is true, the text between selection_pos and cursor_pos is selected.
	bool is_line_buffer; // "line buffers" are buffers which can only have one line of text (used for inputs)
	bool selection;
	bool store_undo_events; // set to false to disable undo events
	// This is set to true whenever a change is made to the buffer, and never set to false by buffer_ functions.
	// (Distinct from buffer_unsaved_changes)
	bool modified;
	bool will_chain_edits;
	bool chaining_edits; // are we chaining undo events together?
	bool view_only;
	bool line_buffer_submitted; // (line buffers only) set to true when submitted. you have to reset it to false.
	// If set to true, buffer will be scrolled to the cursor position next frame.
	// This is to fix the problem that x1,y1,x2,y2 are not updated until the buffer is rendered.
	bool center_cursor_next_frame; 
	float x1, y1, x2, y2;
	u32 nlines;
	u32 lines_capacity;

	u32 undo_history_write_pos; // where in the undo history was the last write? used by buffer_unsaved_changes
	u32 first_line_on_screen, last_line_on_screen; // which lines are on screen? updated when buffer_render is called.

	// to cache syntax highlighting properly, it is important to keep track of the
	// first and last line modified since last frame.
	u32 frame_earliest_line_modified;
	u32 frame_latest_line_modified;

	Line *lines;
	char error[256];
	BufferEdit *undo_history; // dynamic array of undo history
	BufferEdit *redo_history; // dynamic array of redo history
} TextBuffer;

ENUM_U16 {
	MENU_NONE,
	MENU_OPEN,
	MENU_SAVE_AS,
	MENU_WARN_UNSAVED, // warn about unsaved changes
	MENU_ASK_RELOAD, // prompt about whether to reload file which has ben changed by another program
	MENU_GOTO_DEFINITION,
	MENU_GOTO_LINE,
} ENUM_U16_END(Menu);

typedef struct {
	char const *name;
	u32 color;
} SelectorEntry;

typedef struct {
	SelectorEntry *entries;
	u32 n_entries;
	Rect bounds;
	u32 cursor; // index where the selector thing is
	float scroll;
	bool enable_cursor;
} Selector;

// file entries for file selectors
typedef struct {
	char *name; // just the file name
	char *path; // full path
	FsType type;
} FileEntry;

typedef struct {
	Selector sel;
	Rect bounds;
	u32 n_entries;
	FileEntry *entries;
	char cwd[TED_PATH_MAX];
	bool create_menu; // this is for creating files, not opening files
} FileSelector;

// a node is a collection of tabs OR a split of two nodes
typedef struct Node {
	u16 *tabs; // dynamic array of indices into ted->buffers, or NULL if this is a split
	v2 size; // size of node, as a percentage of the window dimensions
	float split_pos; // number from 0 to 1 indicating where the split is.
	u16 active_tab; // index of active tab in tabs.
	bool split_vertical; // is the split vertical? if false, this split looks like a|b
	u16 split_a; // split left/upper half; index into ted->nodes
	u16 split_b; // split right/lower half
} Node;

#define TED_MAX_BUFFERS 256
#define TED_MAX_NODES 256
// max tabs per node
#define TED_MAX_TABS 100

typedef struct {
	BufferPos start;
	BufferPos end;
} FindResult;

typedef struct {
	char *filename;
	BufferPos pos;
	u32 build_output_line; // which line in the build output corresponds to this error
} BuildError;

typedef struct Ted {
	SDL_Window *window;
	Font *font_bold;
	Font *font;
	TextBuffer *active_buffer;
	// buffer we are currently drag-to-selecting in, if any
	TextBuffer *drag_buffer;
	// while a menu or something is open, there is no active buffer. when the menu is closed,
	// the old active buffer needs to be restored. that's what this stores.
	TextBuffer *prev_active_buffer; 
	Node *active_node;
	Settings settings;
	float window_width, window_height;
	u32 key_modifier; // which of shift, alt, ctrl are down right now.
	v2 mouse_pos;
	u32 mouse_state;
	u8 nmouse_clicks[4]; // nmouse_clicks[i] = length of mouse_clicks[i]
	v2 mouse_clicks[4][32]; // mouse_clicks[SDL_BUTTON_RIGHT], for example, is all the right mouse-clicks that have happened this frame
	// number of times mouse was clicked at each position
	u8 mouse_click_times[4][32];
	int scroll_total_x, scroll_total_y; // total amount scrolled in the x and y direction this frame
	Menu menu;
	FileSelector file_selector;
	Selector tag_selector; // for "go to definition of..." menu
	TextBuffer line_buffer; // general-purpose line buffer for inputs -- used for menus
	TextBuffer find_buffer; // use for "find" term in find/find+replace
	TextBuffer replace_buffer; // "replace" for find+replace
	TextBuffer build_buffer; // buffer for build output (view only)
	double error_time; // time error box was opened (in seconds -- see time_get_seconds)
	KeyAction key_actions[KEY_COMBO_COUNT];
	bool search_cwd; // should the working directory be searched for files? set to true if the executable isn't "installed"
	bool quit; // if set to true, the window will close next frame. NOTE: this doesn't check for unsaved changes!!
	bool find; // is the find or find+replace menu open?
	bool replace; // is the find+replace menu open?
	bool find_regex, find_case_sensitive; // find options
	u32 find_flags; // flags used last time search term was compiled
	pcre2_code *find_code;
	pcre2_match_data *find_match_data;
	FindResult *find_results;
	bool find_invalid_pattern; // invalid regex?
	Command warn_unsaved; // if non-zero, the user is trying to execute this command, but there are unsaved changes
	bool build_shown; // are we showing the build output?
	bool building; // is the build process running?
	
	BuildError *build_errors; // dynamic array of build errors
	u32 build_error; // build error we are currently "on"
	
	// used by menus to keep track of the scroll position so we can return to it.
	v2d prev_active_buffer_scroll;
	
	SDL_Cursor *cursor_arrow, *cursor_ibeam, *cursor_resize_h, *cursor_resize_v, *cursor_hand, *cursor_move;
	SDL_Cursor *cursor; // which cursor to use this frame
	
	// node containing tab user is dragging around, NULL if user is not dragging a tab
	Node *dragging_tab_node;
	// index in dragging_tab_node->tabs
	u16 dragging_tab_idx;
	v2 dragging_tab_origin; // where the tab is being dragged from (i.e. mouse pos at start of drag action)
	
	// if not NULL, points to the node whose split the user is currently resizing.
	Node *resizing_split;
	
	char **tag_selector_entries; // an array of all tags (see tag_selector_open)

	// points to a selector if any is open, otherwise NULL.
	Selector *selector_open;

	Process build_process;
	// When we read the stdout from the build process, the tail end of the read could be an
	// incomplete UTF-8 code point. This is where we store that "tail end" until more
	// data is available. (This is up to 3 bytes, null terminated)
	char build_incomplete_codepoint[4];
	char warn_unsaved_names[TED_PATH_MAX]; // comma-separated list of files with unsaved changes (only applicable if warn_unsaved != 0)
	char warn_overwrite[TED_PATH_MAX]; // file name user is trying to overwrite
	char ask_reload[TED_PATH_MAX]; // file name which we want to reload
	char local_data_dir[TED_PATH_MAX];
	char global_data_dir[TED_PATH_MAX];
	char home[TED_PATH_MAX];
	char cwd[TED_PATH_MAX]; // current working directory
	char build_dir[TED_PATH_MAX]; // directory where we run the build command
	char tags_dir[TED_PATH_MAX]; // where we are reading tags from
	bool nodes_used[TED_MAX_NODES];
	Node nodes[TED_MAX_NODES];
	// NOTE: the buffer at index 0 is reserved as a "null buffer" and should not be used.
	bool buffers_used[TED_MAX_BUFFERS];
	TextBuffer buffers[TED_MAX_BUFFERS];
	char window_title[256];
	char error[512];
	char error_shown[512]; // error display in box on screen
} Ted;

void command_execute(Ted *ted, Command c, i64 argument);
