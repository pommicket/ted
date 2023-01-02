#ifndef TED_H_
#define TED_H_

#include "util.h"
#include "ds.h"
#include "lsp.h"
#include "base.h"
#include "text.h"
#include "colors.h"
#include "command.h"

#define TED_VERSION "2.0"
#define TED_VERSION_FULL "ted v. " TED_VERSION
#define TED_PATH_MAX 256
#define TED_UNTITLED "Untitled" // what to call untitled buffers
#define TED_CFG "ted.cfg" // config filename

#define TEXT_SIZE_MIN 6
#define TEXT_SIZE_MAX 70
// max number of LSPs running at once
#define TED_LSP_MAX 200

typedef u32 GLuint;
typedef i32 GLint;
typedef unsigned GLenum;

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


#define SCANCODE_MOUSE_X1 (SDL_NUM_SCANCODES)
#define SCANCODE_MOUSE_X2 (SDL_NUM_SCANCODES+1)
#define SCANCODE_COUNT (SDL_NUM_SCANCODES+2)
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

// a SettingsContext is a context where a specific set of settings are applied.
// this corresponds to [PATH//LANGUAGE.(section)] in config files
typedef struct {
	Language language; // these settings apply to this language.
	char *path; // these settings apply to all paths which start with this string, or all paths if path=NULL
} SettingsContext;

// need to use refcounting for this because of Settings.
// => we copy parent settings to children
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

// shader-array-buffer combo.
typedef struct {
	u32 ref_count;
	GLuint shader;
	GLuint array;
	GLuint buffer;
} GlRcSAB;



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
	KeyAction key_actions[KEY_COMBO_COUNT];
} Settings;

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

typedef struct {
	char *filename; // NULL if this buffer doesn't correspond to a file (e.g. line buffers)
	struct Ted *ted; // we keep a back-pointer to the ted instance so we don't have to pass it in to every buffer function
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	double last_write_time; // last write time to filename.
	i16 manual_language; // 1 + the language the buffer has been manually set to, or 0 if it hasn't been manually set to anything
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

ENUM_U16 {
	MENU_NONE,
	MENU_OPEN,
	MENU_SAVE_AS,
	MENU_WARN_UNSAVED, // warn about unsaved changes
	MENU_ASK_RELOAD, // prompt about whether to reload file which has ben changed by another program
	MENU_GOTO_DEFINITION,
	MENU_GOTO_LINE,
	MENU_COMMAND_SELECTOR,
	MENU_SHELL, // run a shell command
} ENUM_U16_END(Menu);

typedef struct {
	const char *name;
	// if not NULL, this will show on the right side of the entry.
	const char *detail;
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
// max strings in config file
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

typedef struct {
	LSPID last_request_lsp;
	LSPRequestID last_request_id;
	double last_request_time;
} Usages;

typedef struct {
	// displayed normal
	char *label_pre;
	// displayed bold
	char *label_active;
	// displayed normal
	char *label_post;
} Signature;

#define SIGNATURE_HELP_MAX 5

// "signature help" (LSP) is thing that shows the current parameter, etc.
typedef struct {
	// should we resend a signature help request this frame?
	bool retrigger;
	// if signature_count = 0, signature help is closed
	u16 signature_count;
	Signature signatures[SIGNATURE_HELP_MAX];
} SignatureHelp;

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

typedef struct {
	char *name;
	char *detail;
	u32 color;
	bool from_lsp;
	LSPDocumentPosition position; // only set if from_lsp = true
} SymbolInfo;

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
	SymbolInfo *selector_all_definitions; // an array of all definitions
} Definitions;

typedef struct {
	LSPHighlight *highlights;
	LSPRequestID last_request_id;
	LSPID last_request_lsp;
	LSPDocumentPosition requested_position;
} Highlights;

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
	Settings *default_settings;
	float window_width, window_height;
	u32 key_modifier; // which of shift, alt, ctrl are down right now.
	v2 mouse_pos;
	u32 mouse_state;
	u8 nmouse_clicks[4]; // nmouse_clicks[i] = length of mouse_clicks[i]
	v2 mouse_clicks[4][32]; // mouse_clicks[SDL_BUTTON_RIGHT], for example, is all the right mouse-clicks that have happened this frame
	// number of times mouse was clicked at each position
	u8 mouse_click_times[4][32];
	u8 nmouse_releases[4];
	v2 mouse_releases[4][32];
	int scroll_total_x, scroll_total_y; // total amount scrolled in the x and y direction this frame
	Menu menu;
	FileSelector file_selector;
	Selector command_selector;
	TextBuffer line_buffer; // general-purpose line buffer for inputs -- used for menus
	TextBuffer find_buffer; // use for "find" term in find/find+replace
	TextBuffer replace_buffer; // "replace" for find+replace
	TextBuffer build_buffer; // buffer for build output (view only)
	TextBuffer argument_buffer; // used for command selector
	double error_time; // time error box was opened (in seconds -- see time_get_seconds)
	double cursor_error_time; // time which the cursor error animation started (cursor turns red, e.g. when there's no autocomplete suggestion)
	bool search_start_cwd; // should start_cwd be searched for files? set to true if the executable isn't "installed"
	char start_cwd[TED_PATH_MAX];
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
	v2d prev_active_buffer_scroll;
	
	SDL_Cursor *cursor_arrow, *cursor_ibeam, *cursor_wait,
		*cursor_resize_h, *cursor_resize_v, *cursor_hand, *cursor_move;
	SDL_Cursor *cursor; // which cursor to use this frame (NULL for no cursor)
	
	// node containing tab user is dragging around, NULL if user is not dragging a tab
	Node *dragging_tab_node;
	// index in dragging_tab_node->tabs
	u16 dragging_tab_idx;
	v2 dragging_tab_origin; // where the tab is being dragged from (i.e. mouse pos at start of drag action)
	
	// if not NULL, points to the node whose split the user is currently resizing.
	Node *resizing_split;
	
	char **shell_history; // dynamic array of history of commands run with :shell (UTF-8)
	u32 shell_history_pos; // for keeping track of where we are in the shell history.

	// points to a selector if any is open, otherwise NULL.
	Selector *selector_open;
	
	float build_output_height; // what % of the screen the build output takes up
	bool resizing_build_output;
	
	double last_save_time; // last time a save command was executed. used for bg-shaders.

	Process build_process;
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
	Node nodes[TED_MAX_NODES];
	// NOTE: the buffer at index 0 is reserved as a "null buffer" and should not be used.
	bool buffers_used[TED_MAX_BUFFERS];
	TextBuffer buffers[TED_MAX_BUFFERS];
	// config file strings
	u32 nstrings;
	char *strings[TED_MAX_STRINGS];
	char window_title[256];
	char error[512];
	char error_shown[512]; // error display in box on screen
} Ted;

// === colors.c ===
ColorSetting color_setting_from_str(char const *str);
const char *color_setting_to_str(ColorSetting s);
Status color_from_str(const char *str, u32 *color);
ColorSetting color_for_symbol_kind(SymbolKind kind);
// === gl.c ===
GlRcSAB *gl_rc_sab_new(GLuint shader, GLuint array, GLuint buffer);
void gl_rc_sab_incref(GlRcSAB *s);
void gl_rc_sab_decref(GlRcSAB **ps);
GlRcTexture *gl_rc_texture_new(GLuint texture);
void gl_rc_texture_incref(GlRcTexture *t);
void gl_rc_texture_decref(GlRcTexture **pt);
GLuint gl_compile_shader(char error_buf[256], char const *code, GLenum shader_type);
GLuint gl_link_program(char error_buf[256], GLuint *shaders, size_t count);
GLuint gl_compile_and_link_shaders(char error_buf[256], char const *vshader_code, char const *fshader_code);
// prints a debug message if `attrib` is not found
GLuint gl_attrib_location(GLuint program, char const *attrib);
// prints a debug message if `uniform` is not found
GLint gl_uniform_location(GLuint program, char const *uniform);
void gl_geometry_init(void);
void gl_geometry_rect(Rect r, u32 color_rgba);
void gl_geometry_rect_border(Rect r, float border_thickness, u32 color);
void gl_geometry_draw(void);
GLuint gl_load_texture_from_image(const char *path);


char *buffer_contents_utf8_alloc(TextBuffer *buffer);
Command command_from_str(const char *str);
const char *command_to_str(Command command);
// Returns string representation of command
const char *command_to_str(Command c);
void command_execute(Ted *ted, Command c, i64 argument);
void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer);
// the settings of the active buffer, or the default settings if there is no active buffer
Settings *ted_active_settings(Ted *ted);
Settings *ted_get_settings(Ted *ted, const char *path, Language lang);
void ted_load_configs(Ted *ted, bool reloading);
LSP *ted_get_lsp(Ted *ted, const char *path, Language lang);
LSP *ted_active_lsp(Ted *ted);
LSP *ted_get_lsp_by_id(Ted *ted, u32 id);
TextBuffer *find_search_buffer(Ted *ted);
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
void config_parse(Ted *ted, ConfigPart **parts);
void config_free(Ted *ted);
char *settings_get_root_dir(Settings *settings, const char *path);
void menu_open(Ted *ted, Menu menu);
void menu_close(Ted *ted);
void find_update(Ted *ted, bool force);
void autocomplete_close(Ted *ted);
void signature_help_retrigger(Ted *ted);
// go to the definition of `name`.
// if `lsp` is NULL, tags will be used.
// Note: the document position is required for LSP requests because of overloading (where the name
// alone isn't sufficient)
void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition pos);
void definitions_selector_open(Ted *ted);
void definitions_selector_update(Ted *ted);
void definitions_selector_render(Ted *ted, Rect bounds);
void definitions_selector_close(Ted *ted);

#endif
