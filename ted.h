// the main header file for ted.
// this contains almost all of the function declarations.

#ifndef TED_H_
#define TED_H_

#include "base.h"
#include "util.h"
#include "os.h"
#include "unicode.h"
#include "ds.h"
#include "lsp.h"
#include "text.h"
#include "colors.h"
#include "command.h"
#include "lib/glcorearb.h"
#include "sdl-inc.h"

#define TED_VERSION "2.0"
#define TED_VERSION_FULL "ted v. " TED_VERSION
#define TED_PATH_MAX 256
#define TED_UNTITLED "Untitled" // what to call untitled buffers
#define TED_CFG "ted.cfg" // config filename

#define TEXT_SIZE_MIN 6
#define TEXT_SIZE_MAX 70
// max number of LSPs running at once
#define TED_LSP_MAX 200


// ---- syntax state constants ----
// syntax state is explained in development.md

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

enum {
	SYNTAX_STATE_TEX_DOLLAR = 0x01u, // inside math $ ... $
	SYNTAX_STATE_TEX_DOLLARDOLLAR = 0x02u, // inside math $$ ... $$
	SYNTAX_STATE_TEX_VERBATIM = 0x04u, // inside \begin{verbatim} ... \end{verbatim}
};

enum {
	SYNTAX_STATE_MARKDOWN_CODE = 0x01u, // inside ``` ``` code section
};

enum {
	SYNTAX_STATE_HTML_COMMENT = 0x01u
};

enum {
	SYNTAX_STATE_JAVASCRIPT_TEMPLATE_STRING = 0x01u,
	SYNTAX_STATE_JAVASCRIPT_MULTILINE_COMMENT = 0x02u,
};

enum {
	SYNTAX_STATE_JAVA_MULTILINE_COMMENT = 0x01u
};

enum {
	SYNTAX_STATE_GO_RAW_STRING = 0x01u, // backtick-enclosed string
	SYNTAX_STATE_GO_MULTILINE_COMMENT = 0x02u
};

enum {
	SYNTAX_STATE_TED_CFG_STRING = 0x01u,
};

typedef u8 SyntaxState;

// types of syntax highlighting
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

#define SYNTAX_MATH SYNTAX_STRING // for tex
#define SYNTAX_CODE SYNTAX_PREPROCESSOR // for markdown
#define SYNTAX_LINK SYNTAX_CONSTANT // for markdown

// special keycodes for mouse X1 & X2 buttons.
enum {
	KEYCODE_X1 = 1<<20,
	KEYCODE_X2
};
// a "key combo" is some subset of {control, shift, alt} + some key.
#define KEY_COMBO_COUNT (SCANCODE_COUNT << 3)
#define KEY_MODIFIER_CTRL_BIT 0
#define KEY_MODIFIER_SHIFT_BIT 1
#define KEY_MODIFIER_ALT_BIT 2
#define KEY_MODIFIER_CTRL ((u32)1<<KEY_MODIFIER_CTRL_BIT)
#define KEY_MODIFIER_SHIFT ((u32)1<<KEY_MODIFIER_SHIFT_BIT)
#define KEY_MODIFIER_ALT ((u32)1<<KEY_MODIFIER_ALT_BIT)
// annoyingly SDL sets bit 30 for some keycodes
#define KEY_COMBO(modifier, key) ((u32)(modifier) \
	| ((u32)(key >> 30) << 3)\
	| ((u32)(key) & ~(1u<<30)) << 4)

// what happens when we press this key combo
typedef struct {
	u32 key_combo;
	Command command;
	i64 argument;
} KeyAction;

// a SettingsContext is a context where a specific set of settings are applied.
// this corresponds to [PATH//LANGUAGE.(section)] in config files
typedef struct {
	Language language; // these settings apply to this language.
	char *path; // these settings apply to all paths which start with this string, or all paths if path=NULL
} SettingsContext;

// need to use reference counting for this because of Settings:
// We copy parent settings to children
// e.g.
//     [core]
//     bg-texture = "blablabla.png"
//     [Javascript.core]
//     some random shit
// the main Settings' bg_texture will get copied to javascript's Settings,
// so we need to be extra careful about when we delete textures.
typedef struct {
	u32 ref_count;
	GLuint texture;
} GlRcTexture;

// reference-counted shader-array-buffer combo.
typedef struct {
	u32 ref_count;
	GLuint shader;
	GLuint array;
	GLuint buffer;
} GlRcSAB;


// all of ted's settings
typedef struct {
	// NOTE: to add more options to ted, add fields here,
	// and change the settings_<type> global constant near the top of config.c
	
	SettingsContext context;
	u32 colors[COLOR_COUNT];
	float cursor_blink_time_on, cursor_blink_time_off;
	float hover_time;
	u32 max_file_size;
	u32 max_file_size_view_only;
	u16 framerate_cap;
	u16 text_size;
	u16 max_menu_width;
	u16 error_display_time;
	bool auto_indent;
	bool auto_add_newline;
	bool syntax_highlighting;
	bool line_numbers;
	bool auto_reload;
	bool auto_reload_config;
	bool restore_session;
	bool regenerate_tags_if_not_found;
	bool indent_with_spaces;
	bool trigger_characters;
	bool identifier_trigger_characters;
	bool signature_help_enabled;
	bool lsp_enabled;
	bool hover_enabled;
	bool highlight_enabled;
	bool highlight_auto;
	bool vsync;
	u8 tab_width;
	u8 cursor_width;
	u8 undo_save_time;
	u8 border_thickness;
	u8 padding;
	u8 scrolloff;
	u8 tags_max_depth;
	GlRcSAB *bg_shader;
	GlRcTexture *bg_texture;
	char bg_shader_text[4096];
	char bg_shader_image[TED_PATH_MAX];
	char root_identifiers[4096];
	char lsp[512];
	char build_default_command[256];
	// [i] = comma-separated string of file extensions for language i, or NULL for none
	char *language_extensions[LANG_COUNT];
	KeyAction *key_actions; // dynamic array, sorted by KEY_COMBO(modifier, key)
} Settings;

// a position in the buffer
typedef struct {
	u32 line;
	u32 index; // index of character in line (not the same as column, since a tab is settings->tab_width columns)
} BufferPos;

// a single line in a buffer
typedef struct {
	SyntaxState syntax;
	u32 len;
	char32_t *str;
} Line;

// sections of ted.cfg
typedef enum {
	SECTION_NONE,
	SECTION_CORE,
	SECTION_KEYBOARD,
	SECTION_COLORS,
	SECTION_EXTENSIONS
} ConfigSection;

// this structure is used temporarily when loading settings
// it's needed because we want more specific contexts to be dealt with last.
typedef struct {
	int index; // index in order of which part was read first.
	SettingsContext context;
	ConfigSection section;
	char *file;
	u32 line;
	char *text;
	u32 settings; // index into ted->all_settings. only used in config_parse
} ConfigPart;

// this refers to replacing prev_len characters (found in prev_text) at pos with new_len characters
typedef struct {
	bool chain; // should this + the next edit be treated as one?
	BufferPos pos;
	u32 new_len;
	u32 prev_len;
	char32_t *prev_text;
	double time; // time at start of edit (i.e. the time just before the edit), in seconds since epoch
} BufferEdit;

// a buffer - this includes line buffers, unnamed buffers, the build buffer, etc.
typedef struct {
	char *filename; // NULL if this buffer doesn't correspond to a file (e.g. line buffers)
	struct Ted *ted; // we keep a back-pointer to the ted instance so we don't have to pass it in to every buffer function
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	double last_write_time; // last write time to filename.
	i16 manual_language; // 1 + the language the buffer has been manually set to, or 0 if it hasn't been manually set to anything
	BufferPos cursor_pos;
	BufferPos selection_pos; // if selection is true, the text between selection_pos and cursor_pos is selected.
	bool is_line_buffer; // "line buffers" are buffers which can only have one line of text (used for inputs)
	bool selection; // is anything selected?
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
	float x1, y1, x2, y2; // buffer's rectangle on screen
	u32 nlines;
	u32 lines_capacity;
	
	// which LSP this document is open in
	LSPID lsp_opened_in;

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

typedef enum {
	MENU_NONE,
	MENU_OPEN,
	MENU_SAVE_AS,
	MENU_WARN_UNSAVED, // warn about unsaved changes
	MENU_ASK_RELOAD, // prompt about whether to reload file which has ben changed by another program
	MENU_GOTO_DEFINITION,
	MENU_GOTO_LINE,
	MENU_COMMAND_SELECTOR,
	MENU_SHELL, // run a shell command
} Menu;


// an entry in a selector menu (e.g. the "open" menu)
typedef struct {
	const char *name;
	// if not NULL, this will show on the right side of the entry.
	const char *detail;
	u32 color;
	// use this for whatever you want
	u64 userdata;
} SelectorEntry;

// a selector menu (e.g. the "open" menu)
typedef struct {
	SelectorEntry *entries;
	u32 n_entries;
	Rect bounds;
	u32 cursor; // index where the selector thing is
	float scroll;
	// whether or not we should let the user select entries using a cursor.
	bool enable_cursor;
} Selector;

// file entries for file selectors
typedef struct {
	char *name; // just the file name
	char *path; // full path
	FsType type;
} FileEntry;

// a selector menu for files (e.g. the "open" menu)
typedef struct {
	Selector sel;
	Rect bounds;
	u32 n_entries;
	FileEntry *entries;
	char cwd[TED_PATH_MAX];
	bool create_menu; // this is for creating files, not opening files
} FileSelector;

// options for a pop-up menu
typedef enum {
	POPUP_NONE,
	POPUP_YES = 1<<1,
	POPUP_NO = 1<<2,
	POPUP_CANCEL = 1<<3,
} PopupOption;

// pop-up with "yes" and "no" buttons
#define POPUP_YES_NO (POPUP_YES | POPUP_NO)
// pop-up with "yes", "no", and "cancel" buttons
#define POPUP_YES_NO_CANCEL (POPUP_YES | POPUP_NO | POPUP_CANCEL)

// a node is a collection of tabs OR a split of two nodes
typedef struct {
	u16 *tabs; // dynamic array of indices into ted->buffers, or NULL if this is a split
	float split_pos; // number from 0 to 1 indicating where the split is.
	u16 active_tab; // index of active tab in tabs.
	bool split_vertical; // is the split vertical? if false, this split looks like a|b
	u16 split_a; // split left/upper half; index into ted->nodes
	u16 split_b; // split right/lower half
} Node;

// max number of buffers open at one time
#define TED_MAX_BUFFERS 256
// max number of nodes open at one time
#define TED_MAX_NODES 256
// max tabs per node
#define TED_MAX_TABS 100
// max strings in all config files
#define TED_MAX_STRINGS 1000

typedef struct {
	BufferPos start;
	BufferPos end;
} FindResult;

typedef struct {
	char *filename;
	BufferPos pos;
	u32 build_output_line; // which line in the build output corresponds to this error
} BuildError;

// LSPSymbolKinds are translated to these. this is a much coarser categorization
typedef enum {
	SYMBOL_OTHER,
	SYMBOL_FUNCTION,
	SYMBOL_FIELD,
	SYMBOL_TYPE,
	SYMBOL_VARIABLE,
	SYMBOL_CONSTANT,
	SYMBOL_KEYWORD
} SymbolKind;

// a single autocompletion suggestion
typedef struct {
	char *label;
	char *filter;
	char *text;
	char *detail; // this can be NULL!
	char *documentation; // this can be NULL!
	bool deprecated;
	SymbolKind kind;
} Autocompletion;

enum {
	// autocomplete was triggered by :autocomplete command
	TRIGGER_INVOKED = 0x12000,
	// autocomplete list needs to be updated because more characters were typed
	TRIGGER_INCOMPLETE = 0x12001,
};

// data needed for autocompletion
typedef struct {
	bool open; // is the autocomplete window open?
	bool is_list_complete; // should the completions array be updated when more characters are typed?
	
	// what trigger caused the last request for completions:
	// either a character code (for trigger characters),
	// or one of the TRIGGER_* constants above
	uint32_t trigger;
	
	LSPID last_request_lsp;
	LSPRequestID last_request_id;
	// when we sent the request to the LSP for completions
	//  (this is used to figure out when we should display "Loading...")
	double last_request_time;
	
	Autocompletion *completions; // dynamic array of all completions
	u32 *suggested; // dynamic array of completions to be suggested (indices into completions)
	BufferPos last_pos; // position of cursor last time completions were generated. if this changes, we need to recompute completions.
	i32 cursor; // which completion is currently selected (index into suggested)
	i32 scroll;
	
	Rect rect; // rectangle where the autocomplete menu is (needed to avoid interpreting autocomplete clicks as other clicks)
} Autocomplete;

// data needed for finding usages
typedef struct {
	LSPID last_request_lsp;
	LSPRequestID last_request_id;
	double last_request_time;
} Usages;

// a single signature in the signature help.
typedef struct {
	// displayed normal
	char *label_pre;
	// displayed bold
	char *label_active;
	// displayed normal
	char *label_post;
} Signature;

// max # of signatures to display at a time.
#define SIGNATURE_HELP_MAX 5

// "signature help" (LSP) is thing that shows the current parameter, etc.
typedef struct {
	// should we resend a signature help request this frame?
	bool retrigger;
	// if signature_count = 0, signature help is closed
	u16 signature_count;
	Signature signatures[SIGNATURE_HELP_MAX];
} SignatureHelp;

// "hover" information from LSP server
typedef struct {
	// is some hover info being displayed?
	bool open;
	// text to display
	char *text;
	// where the hover data is coming from.
	// we use this to check if we need to refresh it.
	LSPDocumentPosition requested_position;
	LSPID requested_lsp;
	LSPRange range;
	double time; // how long the cursor has been hovering for
} Hover;

// symbol information for the definitions menu
typedef struct {
	char *name;
	char *detail;
	u32 color;
	bool from_lsp;
	LSPDocumentPosition position; // only set if from_lsp = true
} SymbolInfo;

// determines which thing associated with a symbol to go to
typedef enum {
	GOTO_DECLARATION,
	GOTO_DEFINITION,
	GOTO_IMPLEMENTATION,
	GOTO_TYPE_DEFINITION,
} GotoType;

typedef struct {
	LSPID last_request_lsp; // used for cancellation
	// ID of the last request which was sent out.
	// used to process responses in chronological order (= ID order).
	// if we got a response for the last request, or no requests have been made,
	// last_request_id is set to 0.
	LSPRequestID last_request_id;
	double last_request_time;
	
	char *last_request_query; // last query string which we sent a request for
	Selector selector; // for "go to definition of..." menu
	SymbolInfo *all_definitions; // an array of all definitions (gotten from workspace/symbols) for "go to definition" menu
} Definitions;

// "highlight" information from LSP server
typedef struct {
	LSPHighlight *highlights;
	LSPRequestID last_request_id;
	LSPID last_request_lsp;
	LSPDocumentPosition requested_position;
} Highlights;

// more severe message types should have higher numbers.
// they will override less severe messages.
typedef enum {
	MESSAGE_INFO,
	MESSAGE_WARNING,
	MESSAGE_ERROR
} MessageType;

// (almost) all data used by the ted application
typedef struct Ted {
	LSP *lsps[TED_LSP_MAX + 1];
	// current time (see time_get_seconds), as of the start of this frame
	double frame_time;
	
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
	Settings *all_settings; // dynamic array of Settings. use Settings.context to figure out which one to use.
	Settings *default_settings; // settings to use when no buffer is open
	float window_width, window_height;
	u32 key_modifier; // which of shift, alt, ctrl are down right now.
	vec2 mouse_pos;
	u32 mouse_state;
	u8 nmouse_clicks[4]; // nmouse_clicks[i] = length of mouse_clicks[i]
	vec2 mouse_clicks[4][32]; // mouse_clicks[SDL_BUTTON_RIGHT], for example, is all the right mouse-clicks that have happened this frame
	// number of times mouse was clicked at each position
	u8 mouse_click_times[4][32];
	u8 nmouse_releases[4];
	vec2 mouse_releases[4][32];
	int scroll_total_x, scroll_total_y; // total amount scrolled in the x and y direction this frame
	Menu menu; // currently open menu, or MENU_NONE if no menu is open.
	FileSelector file_selector;
	Selector command_selector;
	TextBuffer line_buffer; // general-purpose line buffer for inputs -- used for menus
	TextBuffer find_buffer; // use for "find" term in find/find+replace
	TextBuffer replace_buffer; // "replace" for find+replace
	TextBuffer build_buffer; // buffer for build output (view only)
	TextBuffer argument_buffer; // used for command selector
	double cursor_error_time; // time which the cursor error animation started (cursor turns red, e.g. when there's no autocomplete suggestion)
	bool search_start_cwd; // should start_cwd be searched for files? set to true if the executable isn't "installed"
	char start_cwd[TED_PATH_MAX];
	bool quit; // if set to true, the window will close next frame. NOTE: this doesn't check for unsaved changes!!
	bool find; // is the find or find+replace menu open?
	bool replace; // is the find+replace menu open?
	bool find_regex, find_case_sensitive; // find options
	u32 find_flags; // flags used last time search term was compiled
	struct pcre2_real_code_32 *find_code;
	struct pcre2_real_match_data_32 *find_match_data;
	FindResult *find_results;
	bool find_invalid_pattern; // invalid regex?
	Command warn_unsaved; // if non-zero, the user is trying to execute this command, but there are unsaved changes
	bool build_shown; // are we showing the build output?
	bool building; // is the build process running?
	Autocomplete autocomplete;
	SignatureHelp signature_help;
	Hover hover;
	Definitions definitions;
	Highlights highlights;
	Usages usages;
	
	FILE *log;
	
	BuildError *build_errors; // dynamic array of build errors
	u32 build_error; // build error we are currently "on"
	
	// used by menus to keep track of the scroll position so we can return to it.
	vec2d prev_active_buffer_scroll;
	
	SDL_Cursor *cursor_arrow, *cursor_ibeam, *cursor_wait,
		*cursor_resize_h, *cursor_resize_v, *cursor_hand, *cursor_move;
	// which cursor to use this frame
	// this should be set to one of the cursor_* members above, or NULL for no cursor
	SDL_Cursor *cursor;
	
	// node containing tab user is dragging around, NULL if user is not dragging a tab
	Node *dragging_tab_node;
	// index in dragging_tab_node->tabs
	u16 dragging_tab_idx;
	vec2 dragging_tab_origin; // where the tab is being dragged from (i.e. mouse pos at start of drag action)
	
	// if not NULL, points to the node whose split the user is currently resizing.
	Node *resizing_split;
	
	char **shell_history; // dynamic array of history of commands run with :shell (UTF-8)
	u32 shell_history_pos; // for keeping track of where we are in the shell history.

	// points to a selector if any is open, otherwise NULL.
	Selector *selector_open;
	
	float build_output_height; // what % of the screen the build output takes up
	bool resizing_build_output;
	
	double last_save_time; // last time a save command was executed. used for bg-shaders.

	Process *build_process;
	// When we read the stdout from the build process, the tail end of the read could be an
	// incomplete UTF-8 code point. This is where we store that "tail end" until more
	// data is available. (This is up to 3 bytes, null terminated)
	char build_incomplete_codepoint[4];
	char **build_queue; // allows execution of multiple commands -- needed for tags generation
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
	// nodes[0] is always the "root node", if any buffers are open.
	Node nodes[TED_MAX_NODES];
	// NOTE: the buffer at index 0 is reserved as a "null buffer" and should not be used.
	bool buffers_used[TED_MAX_BUFFERS];
	TextBuffer buffers[TED_MAX_BUFFERS];
	// config file strings
	u32 nstrings;
	char *strings[TED_MAX_STRINGS];
	char window_title[256];
	
	// little box used to display errors and info.
	double message_time; // time message box was opened
	MessageType message_type;
	char message[512];
	MessageType message_shown_type;
	char message_shown[512];
} Ted;

// === buffer.c ===
bool buffer_haserr(TextBuffer *buffer);
const char *buffer_geterr(TextBuffer *buffer);
void buffer_clearerr(TextBuffer *buffer);
void buffer_clear_undo_redo(TextBuffer *buffer);
bool buffer_empty(TextBuffer *buffer);
const char *buffer_get_filename(TextBuffer *buffer);
bool buffer_is_untitled(TextBuffer *buffer);
bool buffer_is_named_file(TextBuffer *buffer);
void buffer_create(TextBuffer *buffer, Ted *ted);
void line_buffer_create(TextBuffer *buffer, Ted *ted);
bool buffer_unsaved_changes(TextBuffer *buffer);
char32_t buffer_char_at_pos(TextBuffer *buffer, BufferPos p);
char32_t buffer_char_before_pos(TextBuffer *buffer, BufferPos pos);
char32_t buffer_char_after_pos(TextBuffer *buffer, BufferPos pos);
char32_t buffer_char_before_cursor(TextBuffer *buffer);
char32_t buffer_char_after_cursor(TextBuffer *buffer);
BufferPos buffer_pos_start_of_file(TextBuffer *buffer);
BufferPos buffer_pos_end_of_file(TextBuffer *buffer);
// ensures that `p` refers to a valid position, moving it if needed.
void buffer_pos_validate(TextBuffer *buffer, BufferPos *p);
bool buffer_pos_valid(TextBuffer *buffer, BufferPos p);
Language buffer_language(TextBuffer *buffer);
// clip the rectangle so it's all inside the buffer. returns true if there's any rectangle left.
bool buffer_clip_rect(TextBuffer *buffer, Rect *r);
LSP *buffer_lsp(TextBuffer *buffer);
// Get the settings used for this buffer.
Settings *buffer_settings(TextBuffer *buffer);
// NOTE: this string will be invalidated when the line is edited!!!
// only use it briefly!!
String32 buffer_get_line(TextBuffer *buffer, u32 line_number);
size_t buffer_get_text_at_pos(TextBuffer *buffer, BufferPos pos, char32_t *text, size_t nchars);
String32 buffer_get_str32_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars);
char *buffer_get_utf8_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars);
size_t buffer_contents_utf8(TextBuffer *buffer, char *out);
char *buffer_contents_utf8_alloc(TextBuffer *buffer);
void buffer_check_valid(TextBuffer *buffer);
void buffer_free(TextBuffer *buffer);
void buffer_clear(TextBuffer *buffer);
void buffer_text_dimensions(TextBuffer *buffer, u32 *lines, u32 *columns);
float buffer_display_lines(TextBuffer *buffer);
float buffer_display_cols(TextBuffer *buffer);
void buffer_scroll(TextBuffer *buffer, double dx, double dy);
vec2 buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos);
bool buffer_pixels_to_pos(TextBuffer *buffer, vec2 pixel_coords, BufferPos *pos);
void buffer_scroll_to_pos(TextBuffer *buffer, BufferPos pos);
void buffer_scroll_center_pos(TextBuffer *buffer, BufferPos pos);
void buffer_scroll_to_cursor(TextBuffer *buffer);
void buffer_center_cursor(TextBuffer *buffer);
i64 buffer_pos_move_left(TextBuffer *buffer, BufferPos *pos, i64 by);
i64 buffer_pos_move_right(TextBuffer *buffer, BufferPos *pos, i64 by);
i64 buffer_pos_move_up(TextBuffer *buffer, BufferPos *pos, i64 by);
i64 buffer_pos_move_down(TextBuffer *buffer, BufferPos *pos, i64 by);
void buffer_cursor_move_to_pos(TextBuffer *buffer, BufferPos pos);
i64 buffer_cursor_move_left(TextBuffer *buffer, i64 by);
i64 buffer_cursor_move_right(TextBuffer *buffer, i64 by);
i64 buffer_cursor_move_up(TextBuffer *buffer, i64 by);
i64 buffer_cursor_move_down(TextBuffer *buffer, i64 by);
i64 buffer_pos_move_up_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by);
i64 buffer_pos_move_down_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by);
i64 buffer_cursor_move_up_blank_lines(TextBuffer *buffer, i64 by);
i64 buffer_cursor_move_down_blank_lines(TextBuffer *buffer, i64 by);
i64 buffer_pos_move_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
i64 buffer_pos_move_left_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
i64 buffer_pos_move_right_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
i64 buffer_cursor_move_left_words(TextBuffer *buffer, i64 nwords);
i64 buffer_cursor_move_right_words(TextBuffer *buffer, i64 nwords);
String32 buffer_word_at_pos(TextBuffer *buffer, BufferPos pos);
String32 buffer_word_at_cursor(TextBuffer *buffer);
char *buffer_word_at_cursor_utf8(TextBuffer *buffer);
BufferPos buffer_pos_start_of_line(TextBuffer *buffer, u32 line);
BufferPos buffer_pos_end_of_line(TextBuffer *buffer, u32 line);
void buffer_cursor_move_to_start_of_line(TextBuffer *buffer);
void buffer_cursor_move_to_end_of_line(TextBuffer *buffer);
void buffer_cursor_move_to_start_of_file(TextBuffer *buffer);
void buffer_cursor_move_to_end_of_file(TextBuffer *buffer);
LSPDocumentID buffer_lsp_document_id(TextBuffer *buffer);
LSPPosition buffer_pos_to_lsp_position(TextBuffer *buffer, BufferPos pos);
LSPDocumentPosition buffer_pos_to_lsp_document_position(TextBuffer *buffer, BufferPos pos);
BufferPos buffer_pos_from_lsp(TextBuffer *buffer, LSPPosition lsp_pos);
LSPPosition buffer_cursor_pos_as_lsp_position(TextBuffer *buffer);
LSPDocumentPosition buffer_cursor_pos_as_lsp_document_position(TextBuffer *buffer);
BufferPos buffer_insert_text_at_pos(TextBuffer *buffer, BufferPos pos, String32 str);
void buffer_insert_char_at_pos(TextBuffer *buffer, BufferPos pos, char32_t c);
void buffer_select_to_pos(TextBuffer *buffer, BufferPos pos);
// Like shift+left in most editors, move cursor nchars chars to the left, selecting everything in between
void buffer_select_left(TextBuffer *buffer, i64 nchars);
void buffer_select_right(TextBuffer *buffer, i64 nchars);
void buffer_select_down(TextBuffer *buffer, i64 nchars);
void buffer_select_up(TextBuffer *buffer, i64 nchars);
void buffer_select_down_blank_lines(TextBuffer *buffer, i64 by);
void buffer_select_up_blank_lines(TextBuffer *buffer, i64 by);
void buffer_select_left_words(TextBuffer *buffer, i64 nwords);
void buffer_select_right_words(TextBuffer *buffer, i64 nwords);
void buffer_select_to_start_of_line(TextBuffer *buffer);
void buffer_select_to_end_of_line(TextBuffer *buffer);
void buffer_select_to_start_of_file(TextBuffer *buffer);
void buffer_select_to_end_of_file(TextBuffer *buffer);
void buffer_select_word(TextBuffer *buffer);
void buffer_select_line(TextBuffer *buffer);
void buffer_select_all(TextBuffer *buffer);
void buffer_disable_selection(TextBuffer *buffer);
void buffer_page_up(TextBuffer *buffer, i64 npages);
void buffer_page_down(TextBuffer *buffer, i64 npages);
void buffer_select_page_up(TextBuffer *buffer, i64 npages);
void buffer_select_page_down(TextBuffer *buffer, i64 npages);
void buffer_delete_chars_at_pos(TextBuffer *buffer, BufferPos pos, i64 nchars_);
i64 buffer_delete_chars_between(TextBuffer *buffer, BufferPos p1, BufferPos p2);
i64 buffer_delete_selection(TextBuffer *buffer);
void buffer_insert_text_at_cursor(TextBuffer *buffer, String32 str);
void buffer_insert_char_at_cursor(TextBuffer *buffer, char32_t c);
void buffer_insert_utf8_at_cursor(TextBuffer *buffer, const char *utf8);
void buffer_insert_tab_at_cursor(TextBuffer *buffer);
void buffer_newline(TextBuffer *buffer);
void buffer_delete_chars_at_cursor(TextBuffer *buffer, i64 nchars);
i64 buffer_backspace_at_pos(TextBuffer *buffer, BufferPos *pos, i64 ntimes);
i64 buffer_backspace_at_cursor(TextBuffer *buffer, i64 ntimes);
void buffer_delete_words_at_pos(TextBuffer *buffer, BufferPos pos, i64 nwords);
void buffer_delete_words_at_cursor(TextBuffer *buffer, i64 nwords);
void buffer_backspace_words_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nwords);
void buffer_backspace_words_at_cursor(TextBuffer *buffer, i64 nwords);
void buffer_undo(TextBuffer *buffer, i64 ntimes);
void buffer_redo(TextBuffer *buffer, i64 ntimes);
void buffer_start_edit_chain(TextBuffer *buffer);
void buffer_end_edit_chain(TextBuffer *buffer);
void buffer_copy_or_cut(TextBuffer *buffer, bool cut);
void buffer_copy(TextBuffer *buffer);
void buffer_cut(TextBuffer *buffer);
void buffer_paste(TextBuffer *buffer);
bool buffer_load_file(TextBuffer *buffer, const char *filename);
void buffer_reload(TextBuffer *buffer);
bool buffer_externally_changed(TextBuffer *buffer);
void buffer_new_file(TextBuffer *buffer, const char *filename);
bool buffer_save(TextBuffer *buffer);
bool buffer_save_as(TextBuffer *buffer, const char *new_filename);
u32 buffer_first_rendered_line(TextBuffer *buffer);
u32 buffer_last_rendered_line(TextBuffer *buffer);
void buffer_goto_word_at_cursor(TextBuffer *buffer, GotoType type);
bool buffer_handle_click(Ted *ted, TextBuffer *buffer, vec2 click, u8 times);
void buffer_render(TextBuffer *buffer, Rect r);
void buffer_indent_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
void buffer_dedent_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
void buffer_indent_selection(TextBuffer *buffer);
void buffer_dedent_selection(TextBuffer *buffer);
void buffer_indent_cursor_line(TextBuffer *buffer);
void buffer_dedent_cursor_line(TextBuffer *buffer);
void buffer_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
void buffer_uncomment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
void buffer_toggle_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
void buffer_toggle_comment_selection(TextBuffer *buffer);
// make sure to call gl_geometry_draw after this
void buffer_highlight_lsp_range(TextBuffer *buffer, LSPRange range);
bool buffer_pos_eq(BufferPos p1, BufferPos p2);

// returns:
// -1 if p1 comes before p2
// +1 if p1 comes after p2
// 0  if p1 = p2
// faster than buffer_pos_diff (constant time)
int buffer_pos_cmp(BufferPos p1, BufferPos p2);

// === build.c ===
// clear build errors and stop
void build_stop(Ted *ted);
// call before adding anything to the build queue
void build_queue_start(Ted *ted);
// add a command to the build queue. call build_queue_start before this.
void build_queue_command(Ted *ted, const char *command);
// call this after calling build_queue_start, build_queue_command.
// make sure you set ted->build_dir before running this!
void build_queue_finish(Ted *ted);
// set up the build output buffer.
void build_setup_buffer(Ted *ted);
// run a single command in the build window.
// make sure you set ted->build_dir before running this!
void build_start_with_command(Ted *ted, const char *command);
// figure out which build command to run, and run it.
void build_start(Ted *ted);
// go to next build error
void build_next_error(Ted *ted);
// go to previous build error
void build_prev_error(Ted *ted);
// find build errors in build buffer.
void build_check_for_errors(Ted *ted);
void build_frame(Ted *ted, float x1, float y1, float x2, float y2);

// === colors.c ===
ColorSetting color_setting_from_str(const char *str);
const char *color_setting_to_str(ColorSetting s);
Status color_from_str(const char *str, u32 *color);
ColorSetting color_for_symbol_kind(SymbolKind kind);

// === command.c ===
Command command_from_str(const char *str);
const char *command_to_str(Command c);
void command_execute(Ted *ted, Command c, i64 argument);

// === config.c ===
// first, we read all config files, then we parse them.
// this is because we want less specific settings (e.g. settings applied
// to all languages instead of one particular language) to be applied first,
// then more specific settings are based off of those.
// EXAMPLE:
//   ---config file 1---
//     [Javascript.core]
//     syntax-highlighting = off
//     (inherits tab-width = 4)
//     [CSS.core]
//     tab-width = 2 (overrides tab-width = 4)
//   ---config file 2---
//     [core]
//     tab-width = 4
void config_read(Ted *ted, ConfigPart **parts, const char *filename);
void config_parse(Ted *ted, ConfigPart **pparts);
void config_free(Ted *ted);
char *settings_get_root_dir(Settings *settings, const char *path);
long context_score(const char *path, Language lang, const SettingsContext *context);

// === find.c ===
TextBuffer *find_search_buffer(Ted *ted);
float find_menu_height(Ted *ted);
void find_update(Ted *ted, bool force);
void find_replace(Ted *ted);
void find_next(Ted *ted);
void find_prev(Ted *ted);
void find_replace_all(Ted *ted);
void find_menu_frame(Ted *ted, Rect menu_bounds);
void find_open(Ted *ted, bool replace);
void find_close(Ted *ted);

// === gl.c ===
extern float gl_window_width, gl_window_height;
// set by main()
extern int gl_version_major, gl_version_minor;

// macro trickery to avoid having to write everything multiple times
#define gl_for_each_proc(do)\
	do(DRAWARRAYS, DrawArrays)\
	do(GENTEXTURES, GenTextures)\
	do(DELETETEXTURES, DeleteTextures)\
	do(GENERATEMIPMAP, GenerateMipmap)\
	do(TEXIMAGE2D, TexImage2D)\
	do(BINDTEXTURE, BindTexture)\
	do(TEXPARAMETERI, TexParameteri)\
	do(GETERROR, GetError)\
	do(GETINTEGERV, GetIntegerv)\
	do(ENABLE, Enable)\
	do(DISABLE, Disable)\
	do(BLENDFUNC, BlendFunc)\
	do(VIEWPORT, Viewport)\
	do(CLEARCOLOR, ClearColor)\
	do(CLEAR, Clear)\
	do(FINISH, Finish)\
	do(CREATESHADER, CreateShader)\
	do(DELETESHADER, DeleteShader)\
	do(CREATEPROGRAM, CreateProgram)\
	do(SHADERSOURCE, ShaderSource)\
	do(GETSHADERIV, GetShaderiv)\
	do(GETSHADERINFOLOG, GetShaderInfoLog)\
	do(COMPILESHADER, CompileShader)\
	do(CREATEPROGRAM, CreateProgram)\
	do(DELETEPROGRAM, DeleteProgram)\
	do(ATTACHSHADER, AttachShader)\
	do(LINKPROGRAM, LinkProgram)\
	do(GETPROGRAMIV, GetProgramiv)\
	do(GETPROGRAMINFOLOG, GetProgramInfoLog)\
	do(USEPROGRAM, UseProgram)\
	do(GETATTRIBLOCATION, GetAttribLocation)\
	do(GETUNIFORMLOCATION, GetUniformLocation)\
	do(GENBUFFERS, GenBuffers)\
	do(DELETEBUFFERS, DeleteBuffers)\
	do(BINDBUFFER, BindBuffer)\
	do(BUFFERDATA, BufferData)\
	do(VERTEXATTRIBPOINTER, VertexAttribPointer)\
	do(ENABLEVERTEXATTRIBARRAY, EnableVertexAttribArray)\
	do(DISABLEVERTEXATTRIBARRAY, DisableVertexAttribArray)\
	do(GENVERTEXARRAYS, GenVertexArrays)\
	do(DELETEVERTEXARRAYS, DeleteVertexArrays)\
	do(BINDVERTEXARRAY, BindVertexArray)\
	do(ACTIVETEXTURE, ActiveTexture)\
	do(UNIFORM1F, Uniform1f)\
	do(UNIFORM2F, Uniform2f)\
	do(UNIFORM3F, Uniform3f)\
	do(UNIFORM4F, Uniform4f)\
	do(UNIFORM1I, Uniform1i)\
	do(UNIFORM2I, Uniform2i)\
	do(UNIFORM3I, Uniform3i)\
	do(UNIFORM4I, Uniform4i)\
	do(UNIFORMMATRIX4FV, UniformMatrix4fv)\
	do(DEBUGMESSAGECALLBACK, DebugMessageCallback)\
	do(DEBUGMESSAGECONTROL, DebugMessageControl)\

#define gl_declare_proc(upper, lower) extern PFNGL##upper##PROC gl##lower;
gl_for_each_proc(gl_declare_proc)
#undef gl_declare_proc

void gl_get_procs(void);
GlRcSAB *gl_rc_sab_new(GLuint shader, GLuint array, GLuint buffer);
void gl_rc_sab_incref(GlRcSAB *s);
void gl_rc_sab_decref(GlRcSAB **ps);
GlRcTexture *gl_rc_texture_new(GLuint texture);
void gl_rc_texture_incref(GlRcTexture *t);
void gl_rc_texture_decref(GlRcTexture **pt);
GLuint gl_compile_shader(char error_buf[256], const char *code, GLenum shader_type);
GLuint gl_link_program(char error_buf[256], GLuint *shaders, size_t count);
GLuint gl_compile_and_link_shaders(char error_buf[256], const char *vshader_code, const char *fshader_code);
// prints a debug message if `attrib` is not found
GLuint gl_attrib_location(GLuint program, const char *attrib);
// prints a debug message if `uniform` is not found
GLint gl_uniform_location(GLuint program, const char *uniform);
void gl_geometry_init(void);
void gl_geometry_rect(Rect r, u32 color_rgba);
void gl_geometry_rect_border(Rect r, float border_thickness, u32 color);
void gl_geometry_draw(void);
GLuint gl_load_texture_from_image(const char *path);

// === ide-autocomplete.c ===
// open autocomplete
// trigger should either be a character (e.g. '.') or one of the TRIGGER_* constants.
void autocomplete_open(Ted *ted, uint32_t trigger);
void autocomplete_process_lsp_response(Ted *ted, const LSPResponse *response); 
void autocomplete_select_cursor_completion(Ted *ted);
void autocomplete_scroll(Ted *ted, i32 by);
void autocomplete_next(Ted *ted);
void autocomplete_prev(Ted *ted);
void autocomplete_close(Ted *ted);
void autocomplete_update_suggested(Ted *ted);
void autocomplete_frame(Ted *ted);

// === ide-definitions.c ===
// go to the definition of `name`.
// if `lsp` is NULL, tags will be used.
// Note: the document position is required for LSP requests because of overloading (where the name
// alone isn't sufficient)
void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition pos, GotoType type);
void definition_cancel_lookup(Ted *ted);
void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response);
void definitions_selector_open(Ted *ted);
void definitions_selector_update(Ted *ted);
void definitions_selector_render(Ted *ted, Rect bounds);
void definitions_selector_close(Ted *ted);
void definitions_frame(Ted *ted);

// === ide-highlights.c ===
void highlights_close(Ted *ted);
void highlights_process_lsp_response(Ted *ted, LSPResponse *response);
void highlights_frame(Ted *ted);

// === ide-hover.c ===
void hover_close(Ted *ted);
void hover_process_lsp_response(Ted *ted, LSPResponse *response);
void hover_frame(Ted *ted, double dt);

// === ide-signature-help.c ===
void signature_help_send_request(Ted *ted);
void signature_help_retrigger(Ted *ted);
void signature_help_open(Ted *ted, char32_t trigger);
bool signature_help_is_open(Ted *ted);
void signature_help_close(Ted *ted);
void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response);
void signature_help_frame(Ted *ted);

// === ide-usages.c ===
void usages_cancel_lookup(Ted *ted);
void usages_find(Ted *ted);
void usages_process_lsp_response(Ted *ted, LSPResponse *response);
void usages_frame(Ted *ted);

// === menu.c ===
void menu_close(Ted *ted);
void menu_open(Ted *ted, Menu menu);
void menu_escape(Ted *ted);
float menu_get_width(Ted *ted);
Rect menu_rect(Ted *ted);
void menu_update(Ted *ted);
void menu_render(Ted *ted);
// move to next/previous command
void menu_shell_move(Ted *ted, int direction);
// move to previous command
void menu_shell_up(Ted *ted);
// move to next command
void menu_shell_down(Ted *ted);

// === node.c ===
void node_switch_to_tab(Ted *ted, Node *node, u16 new_tab_index);
void node_tab_next(Ted *ted, Node *node, i64 n);
void node_tab_prev(Ted *ted, Node *node, i64 n);
void node_tab_switch(Ted *ted, Node *node, i64 tab);
// swap the position of two tabs
void node_tabs_swap(Node *node, u16 tab1, u16 tab2);
void node_free(Node *node);
// returns index of parent in ted->nodes, or -1 if this is the root node.
i32 node_parent(Ted *ted, u16 node_idx);
// join this node with its sibling
void node_join(Ted *ted, Node *node);
void node_close(Ted *ted, u16 node_idx);
// close tab, WITHOUT checking for unsaved changes!
// returns true if the node is still open
bool node_tab_close(Ted *ted, Node *node, u16 index);
void node_frame(Ted *ted, Node *node, Rect r);
void node_split(Ted *ted, Node *node, bool vertical);
void node_split_switch(Ted *ted);
void node_split_swap(Ted *ted);

// === session.c ===
void session_write(Ted *ted);
void session_read(Ted *ted);

// === syntax.c ===
Language language_from_str(const char *str);
const char *language_to_str(Language language);
const char *language_comment_start(Language l);
const char *language_comment_end(Language l);
ColorSetting syntax_char_type_to_color_setting(SyntaxCharType t);
char32_t syntax_matching_bracket(Language lang, char32_t c);
bool syntax_is_opening_bracket(Language lang, char32_t c);
void syntax_highlight(SyntaxState *state, Language lang, const char32_t *line, u32 line_len, SyntaxCharType *char_types);

// === tags.c ===
void tags_generate(Ted *ted, bool run_in_build_window);
// find all tags beginning with the given prefix, returning them into `*out`, writing at most out_size entries.
// you may pass NULL for `out`, in which case just the number of matching tags is returned
// (still maxing out at `out_size`).
// each element in `out` should be freed when you're done with them.
size_t tags_beginning_with(Ted *ted, const char *prefix, char **out, size_t out_size);
bool tag_goto(Ted *ted, const char *tag);
// get all tags in the tags file as SymbolInfos.
SymbolInfo *tags_get_symbols(Ted *ted);

// === ted.c ===
// for fatal errors
void die(PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
// display a message to the user
void ted_set_message(Ted *ted, MessageType type, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(3, 4);
// display an error to the user
void ted_error(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
// display a warning to the user
void ted_warn(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
// display information to the user
void ted_info(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
// for information that should be logged
void ted_log(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
// set error to "out of memory" message.
void ted_out_of_mem(Ted *ted);
void *ted_malloc(Ted *ted, size_t size);
void *ted_calloc(Ted *ted, size_t n, size_t size);
void *ted_realloc(Ted *ted, void *p, size_t new_size);
Status ted_get_file(Ted const *ted, const char *name, char *out, size_t outsz);
// get full path relative to ted->cwd.
void ted_path_full(Ted *ted, const char *relpath, char *abspath, size_t abspath_size);
void ted_reset_active_buffer(Ted *ted);
void ted_error_from_buffer(Ted *ted, TextBuffer *buffer);
// Returns the buffer containing the file at `path`, or NULL if there is none.
TextBuffer *ted_get_buffer_with_file(Ted *ted, const char *path);
bool ted_save_all(Ted *ted);
void ted_reload_all(Ted *ted);
// Load all the fonts ted will use.
void ted_load_fonts(Ted *ted);
char *ted_get_root_dir_of(Ted *ted, const char *path);
char *ted_get_root_dir(Ted *ted);
// the settings of the active buffer, or the default settings if there is no active buffer
Settings *ted_active_settings(Ted *ted);
Settings *ted_get_settings(Ted *ted, const char *path, Language language);
LSP *ted_get_lsp_by_id(Ted *ted, LSPID id);
LSP *ted_get_lsp(Ted *ted, const char *path, Language language);
LSP *ted_active_lsp(Ted *ted);
u32 ted_color(Ted *ted, ColorSetting color);
bool ted_open_file(Ted *ted, const char *filename);
bool ted_new_file(Ted *ted, const char *filename);
// returns the index of an available buffer, or -1 if none are available 
i32 ted_new_buffer(Ted *ted);
// Returns the index of an available node, or -1 if none are available 
i32 ted_new_node(Ted *ted);
// Opposite of ted_new_buffer
// Make sure you set active_buffer to something else if you delete it!
void ted_delete_buffer(Ted *ted, u16 index);
// save all changes to all buffers with unsaved changes.
bool ted_save_all(Ted *ted);
// sets the active buffer to this buffer, and updates active_node, etc. accordingly
// you can pass NULL to buffer to make it so no buffer is active.
void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer);
// switch to this node
void ted_node_switch(Ted *ted, Node *node);
void ted_load_configs(Ted *ted, bool reloading);
void ted_press_key(Ted *ted, SDL_Keycode keycode, SDL_Keymod modifier);
bool ted_get_mouse_buffer_pos(Ted *ted, TextBuffer **pbuffer, BufferPos *ppos);
void ted_flash_error_cursor(Ted *ted);
void ted_go_to_position(Ted *ted, const char *path, u32 line, u32 index, bool is_lsp);
void ted_go_to_lsp_document_position(Ted *ted, LSP *lsp, LSPDocumentPosition position);
void ted_cancel_lsp_request(Ted *ted, LSPID lsp, LSPRequestID request);
// how tall is a line buffer?
float ted_line_buffer_height(Ted *ted);
// check for orphaned nodes and node cycles
void ted_check_for_node_problems(Ted *ted);
// convert LSPWindowMessageType to MessageType
MessageType ted_message_type_from_lsp(LSPWindowMessageType type);
// get colors to use for message box
void ted_color_settings_for_message_type(MessageType type, ColorSetting *bg_color, ColorSetting *border_color);

// === ui.c ===
void selector_up(Ted *ted, Selector *s, i64 n);
void selector_down(Ted *ted, Selector *s, i64 n);
// sort entries alphabetically
void selector_sort_entries_by_name(Selector *s);
// returns a null-terminated UTF-8 string of the entry selected, or NULL if none was.
// also, sel->cursor will be set to the index of the entry, even if the mouse was used.
// you should call free() on the return value.
char *selector_update(Ted *ted, Selector *s);
// NOTE: also renders the line buffer
void selector_render(Ted *ted, Selector *s);
void file_selector_free(FileSelector *fs);
// returns the name of the selected file, or NULL if none was selected.
// the returned pointer should be freed.
char *file_selector_update(Ted *ted, FileSelector *fs);
void file_selector_render(Ted *ted, FileSelector *fs);
vec2 button_get_size(Ted *ted, const char *text);
void button_render(Ted *ted, Rect button, const char *text, u32 color);
// returns true if the button was clicked on.
bool button_update(Ted *ted, Rect button);
PopupOption popup_update(Ted *ted, u32 options);
void popup_render(Ted *ted, u32 options, const char *title, const char *body);
vec2 checkbox_frame(Ted *ted, bool *value, const char *label, vec2 pos);

#endif
