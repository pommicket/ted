/// \file
/// the main header file for ted.
///
/// this contains almost all of the function declarations.


/// \mainpage ted doxygen documentation
/// 
/// See "files" above. You probably want to look at \ref ted.h.

#ifndef TED_H_
#define TED_H_

#ifdef TED_PLUGIN
#undef TED_PLUGIN
#define TED_PLUGIN 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/// Version number
#define TED_VERSION "2.4.3"
/// Version string
#define TED_VERSION_FULL "ted v. " TED_VERSION
/// Maximum path size ted handles.
#define TED_PATH_MAX 1024
/// Config filename
#define TED_CFG "ted.cfg"

/// Minimum text size
#define TEXT_SIZE_MIN 6
/// Maximum text size
#define TEXT_SIZE_MAX 70
/// max number of LSPs running at once
#define TED_LSP_MAX 200
/// max number of macros
#define TED_MACRO_MAX 256

// If you are adding new languages, DO NOT change the constant values
// of the previous languages. It will mess up config files which use :set-language!
enum {
	/// avoid using this and use LANG_TEXT instead.
	LANG_NONE = 0,
	/// C
	LANG_C = 1,
	/// C++
	LANG_CPP = 2,
	/// Rust
	LANG_RUST = 3,
	/// Python
	LANG_PYTHON = 4,
	/// TeX/LaTeX
	LANG_TEX = 5,
	/// Markdown
	LANG_MARKDOWN = 6,
	/// HTML
	LANG_HTML = 7,
	/// .cfg files
	LANG_CONFIG = 8,
	/// JavaScript
	LANG_JAVASCRIPT = 9,
	/// Java
	LANG_JAVA = 10,
	/// Go
	LANG_GO = 11,
	/// like \ref LANG_CONFIG, but with more highlighting for ted.cfg-specific stuff.
	LANG_TED_CFG = 12,
	/// TypeScript
	LANG_TYPESCRIPT = 13,
	/// JSON
	LANG_JSON = 14,
	/// XML
	LANG_XML = 15,
	/// GL shading language
	LANG_GLSL = 16,
	/// plain text
	LANG_TEXT = 17,
	/// CSS
	LANG_CSS = 18,
	
	/// all user-defined languages are greater than this.
	LANG_USER_MIN = 100000,
	/// all user-defined languages are less than this.
	LANG_USER_MAX = 2000000000,
};

/// A programming language
///
/// May be one of the `LANG_*` constants, or a dynamically registered language.
typedef u32 Language;

/// Current state of syntax highlighting.
typedef u32 SyntaxState;

/// types of syntax highlighting
enum SyntaxCharType {
	// do not change these numbers as it will break backwards compatibility with plugins
	SYNTAX_NORMAL = 0,
	SYNTAX_KEYWORD = 1,
	SYNTAX_BUILTIN = 2,
	SYNTAX_COMMENT = 3,
	SYNTAX_PREPROCESSOR = 4,
	SYNTAX_STRING = 5,
	SYNTAX_CHARACTER = 6,
	SYNTAX_CONSTANT = 7,
	SYNTAX_TODO = 8,
};
/// Type of syntax highlighting.
typedef u8 SyntaxCharType;
/// Function for syntax highlighting.
/// If you want to add a language to `ted`, you will need to implement this function.
///
/// `state` is used to keep track of state between lines (e.g. whether or not we are in a multiline comment)\n
/// `line` is the UTF-32 text of the line (not guaranteed to be null-terminated).\n
/// `line_len` is the length of the line, in UTF-32 codepoints.\n
/// `char_types` is either `NULL` (in which case only `state` should be updated), or a pointer to `line_len` SyntaxCharTypes, which should be filled out using the `SYNTAX_*` constants.
///
/// no guarantees are made about which order lines will be highlighted in. the only guarantee is that `*state = 0` for the first line, and for line `n > 0`,
/// `*state` was derived from calling this function on line `n-1`.
typedef void (*SyntaxHighlightFunction)(SyntaxState *state, const char32_t *line, u32 line_len, SyntaxCharType *char_types);

/// Information about a programming language
///
/// Used for dynamic language registration.
/// Please zero all the fields of the struct which you aren't using.
///
/// The fields `id` and `name` MUST NOT be 0, or `ted` will reject your language.
typedef struct {
	/// Language ID number. For user-defined languages, this must be `>= LANG_USER_MIN` and `< LANG_USER_MAX`.
	///
	/// To avoid conflict, try picking a unique number.
	Language id;
	char name[30];
	char lsp_identifier[32];
	SyntaxHighlightFunction highlighter;
	char reserved[128];
} LanguageInfo;

/// for tex
#define SYNTAX_MATH SYNTAX_STRING
/// for markdown
#define SYNTAX_CODE SYNTAX_PREPROCESSOR
/// for markdown
#define SYNTAX_LINK SYNTAX_CONSTANT

/// Settings
typedef struct Settings Settings;

/// A buffer - this includes line buffers, unnamed buffers, the build buffer, etc.
typedef struct TextBuffer TextBuffer;

/// all data used by the ted application (minus some globals in gl.c)
typedef struct Ted Ted;

/// a selector menu (e.g. the "open" menu)
typedef struct Selector Selector;

/// a selector menu for files (e.g. the "open" menu)
typedef struct FileSelector FileSelector;

/// a split or collection of tabs
typedef struct Node Node;

/// A position in the buffer
typedef struct {
	/// line number (0-indexed)
	u32 line;
	/// UTF-32 index of character in line
	///
	/// (not the same as column, since a tab is `settings->tab_width` columns)
	u32 index;
} BufferPos;

/// special keycodes for mouse X1 & X2 buttons.
enum {
	KEYCODE_X1 = 1<<20,
	KEYCODE_X2
};
/// see \ref KEY_COMBO
enum {
	KEY_MODIFIER_CTRL_BIT,
	KEY_MODIFIER_SHIFT_BIT,
	KEY_MODIFIER_ALT_BIT
};
/// see \ref KEY_COMBO
#define KEY_MODIFIER_CTRL ((u32)1<<KEY_MODIFIER_CTRL_BIT)
/// see \ref KEY_COMBO
#define KEY_MODIFIER_SHIFT ((u32)1<<KEY_MODIFIER_SHIFT_BIT)
/// see \ref KEY_COMBO
#define KEY_MODIFIER_ALT ((u32)1<<KEY_MODIFIER_ALT_BIT)
/// a "key combo" is some subset of {control, shift, alt} + some key.
typedef struct {
	/// high 32 bits = SDL_Keycode\n
	/// low 8 bits = key modifier (see e.g. \ref KEY_MODIFIER_SHIFT)\n
	/// the remaining 24 bits are currently reserved and should be 0.
	u64 value;
} KeyCombo;
/// Create \ref KeyCombo from modifier and key.
#define KEY_COMBO(modifier, key) ((KeyCombo){.value = (u64)(modifier) \
	| ((u64)(key) << 32)})
/// extract `SDL_Keycode` from \ref KeyCombo
#define KEY_COMBO_KEY(combo) ((SDL_Keycode)((combo.value) >> 32))
/// extract key modifier from \ref KeyCombo
#define KEY_COMBO_MODIFIER(combo) ((u32)((combo.value) & 0xff))

enum {
	/// autocomplete/signature help was manually triggered
	TRIGGER_INVOKED = 0x12000,
	/// autocomplete list needs to be updated because more characters were typed
	TRIGGER_INCOMPLETE = 0x12001,
	/// signtaure help needs to be updated because the cursor was moved or
	/// the buffer's contents changed.
	TRIGGER_CONTENT_CHANGE = 0x12002,
};

/// determines which thing associated with a symbol to go to
typedef enum {
	GOTO_DECLARATION,
	GOTO_DEFINITION,
	GOTO_IMPLEMENTATION,
	GOTO_TYPE_DEFINITION,
} GotoType;

/// options for a pop-up menu
typedef enum {
	POPUP_NONE,
	/// "Yes" button
	POPUP_YES = 1<<1,
	/// "No" button
	POPUP_NO = 1<<2,
	/// "Cancel" button
	POPUP_CANCEL = 1<<3,
} PopupOption;

/// pop-up with "yes" and "no" buttons
#define POPUP_YES_NO (POPUP_YES | POPUP_NO)
/// pop-up with "yes", "no", and "cancel" buttons
#define POPUP_YES_NO_CANCEL (POPUP_YES | POPUP_NO | POPUP_CANCEL)

/// type of message box to display to user
///
/// more severe message types should have higher numbers.
/// they will override less severe messages.
typedef enum {
	MESSAGE_INFO = 0x10000,
	MESSAGE_WARNING = 0x20000,
	MESSAGE_ERROR = 0x30000,
} MessageType;

typedef enum {
	/// No menu is open
	MENU_NONE,
	/// "Open file"
	MENU_OPEN,
	/// "Save file as"
	MENU_SAVE_AS,
	/// "X has unsaved changes"
	MENU_WARN_UNSAVED,
	/// "X has been changed by another program"
	MENU_ASK_RELOAD,
	/// "Go to definition of..."
	MENU_GOTO_DEFINITION,
	/// "Go to line"
	MENU_GOTO_LINE,
	/// "Command palette"
	MENU_COMMAND_SELECTOR,
	/// "Run a shell command"
	MENU_SHELL,
	/// "Rename symbol"
	MENU_RENAME_SYMBOL,
} Menu;



#if !TED_PLUGIN

typedef struct {
	const char *string;
	i64 number;
} CommandArgument;

typedef struct {
	// did this command come from executing a macro?
	bool running_macro;
} CommandContext;

/// Thing to do when a key combo is pressed.
typedef struct {
	KeyCombo key_combo;
	Command command;
	CommandArgument argument;
} KeyAction;


/// A SettingsContext is a context where a specific set of settings are applied.
/// this corresponds to `[PATH//LANGUAGE.(section)]` in config files.
typedef struct {
	/// The settings apply to this language.
	Language language;
	/// The settings apply to all paths which start with this string, or all paths if path=NULL
	char *path;
} SettingsContext;

/// Need to use reference counting for textures because of Settings:
/// We copy parent settings to children
/// e.g.
/// ```
/// [core]
/// bg-texture = "blablabla.png"
/// [Javascript.core]
/// some random shit
/// ```
/// the main Settings' bg_texture will get copied to javascript's Settings,
/// so we need to be extra careful about when we delete textures.
typedef struct {
	u32 ref_count;
	GLuint texture;
} GlRcTexture;

/// Reference-counted shader-array-buffer combo.
typedef struct {
	u32 ref_count;
	GLuint shader;
	GLuint array;
	GLuint buffer;
} GlRcSAB;

typedef struct {
	Language language;
	char extension[16];
} LanguageExtension;

/// All of ted's settings
///
/// NOTE: to add more options to ted, add fields here,
/// and change the settings_<type> global constant near the top of config.c	
struct Settings {
	SettingsContext context;
	u32 colors[COLOR_COUNT];
	float cursor_blink_time_on, cursor_blink_time_off;
	float hover_time;
	float ctrl_scroll_adjust_text_size;
	u32 max_file_size;
	u32 max_file_size_view_only;
	u16 framerate_cap;
	u16 text_size_no_dpi;
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
	bool phantom_completions;
	bool trigger_characters;
	bool identifier_trigger_characters;
	bool signature_help_enabled;
	bool lsp_enabled;
	bool lsp_log;
	bool hover_enabled;
	bool highlight_enabled;
	bool highlight_auto;
	bool document_links;
	bool vsync;
	bool save_backup;
	bool crlf_windows;
	bool jump_to_build_error;
	bool force_monospace;
	KeyCombo hover_key;
	KeyCombo highlight_key;
	u8 tab_width;
	u8 cursor_width;
	u8 undo_save_time;
	u8 border_thickness;
	u8 padding;
	u8 scrolloff;
	u8 tags_max_depth;
	GlRcSAB *bg_shader;
	GlRcTexture *bg_texture;
	/// string used to start comments
	char comment_start[16];
	/// string used to end comments
	char comment_end[16];
	/// Comma-separated list of file names which identify the project root
	char root_identifiers[4096];
	/// LSP server command
	char lsp[512];
	/// LSP "configuration" JSON
	char lsp_configuration[4096];
	/// Build command. If non-empty, this overrides running `cargo build` if `Cargo.toml` exists, etc.
	char build_command[1024];
	/// Default build command for if `Cargo.toml`, `Makefile`, etc. do not exist.
	char build_default_command[1024];
	/// Comma separated list of paths to font files.
	char font[4096];
	/// Comma separated list of paths to bold font files.
	char font_bold[4096];
	LanguageExtension *language_extensions;
	/// dynamic array, sorted by KEY_COMBO(modifier, key)
	KeyAction *key_actions;
};

/// A single line in a buffer
typedef struct Line Line;

/// This structure is used temporarily when loading settings
/// It's needed because we want more specific contexts to be dealt with last.
typedef struct ConfigPart ConfigPart;

/// A single undoable edit to a buffer
typedef struct BufferEdit BufferEdit;

struct TextBuffer {
	/// NULL if this buffer is untitled or doesn't correspond to a file (e.g. line buffers)
	char *path;
	/// we keep a back-pointer to the ted instance so we don't have to pass it in to every buffer function
	struct Ted *ted;
	/// number of characters scrolled in the x direction (multiply by space width to get pixels)
	double scroll_x;
	/// number of characters scrolled in the y direction
	double scroll_y;
	/// last write time to \ref path
	double last_write_time;
	/// the language the buffer has been manually set to, or \ref LANG_NONE if it hasn't been set to anything
	i64 manual_language;
	/// position of cursor
	BufferPos cursor_pos;
	/// if \ref selection is true, the text between \ref selection_pos and \ref cursor_pos is selected.
	BufferPos selection_pos;
	/// "previous" position of cursor, for \ref CMD_PREVIOUS_POSITION
	BufferPos prev_cursor_pos;
	/// "line buffers" are buffers which can only have one line of text (used for inputs)
	bool is_line_buffer;
	/// is anything selected?
	bool selection;
	/// set to false to disable undo events
	bool store_undo_events;
	/// This is set to true whenever a change is made to the buffer, and never set to false by buffer_ functions.
	/// (Distinct from \ref buffer_unsaved_changes)
	bool modified;
	/// will the next undo event be chained with the ones after?
	bool will_chain_edits;
	/// will the next undo event be chained with the previous one?
	bool chaining_edits;
	/// view-only mode
	bool view_only;
	/// (line buffers only) set to true when submitted. you have to reset it to false.
	bool line_buffer_submitted;
	/// If set to true, buffer will be scrolled to the cursor position next frame.
	/// This is to fix the problem that \ref x1, \ref y1, \ref x2, \ref y2 are not updated until the buffer is rendered.
	bool center_cursor_next_frame;
	/// x coordinate of left side of buffer
	float x1;
	/// y coordinate of top side of buffer
	float y1;
	/// x coordinate of right side of buffer
	float x2;
	/// y coordinate of bottom side of buffer
	float y2;
	/// number of lines in buffer
	u32 nlines;
	/// capacity of \ref lines
	u32 lines_capacity;
	
	/// cached settings index (into ted->all_settings), or -1 if has not been computed yet
	i32 settings_idx;
	
	/// which LSP this document is open in
	LSPID lsp_opened_in;
	/// determining which LSP to use for a buffer takes some work,
	/// so we don't want to do it every single frame.
	/// this keeps track of the last time we actually checked what the correct LSP is.
	double last_lsp_check;

	/// where in the undo history was the last write? used by \ref buffer_unsaved_changes
	u32 undo_history_write_pos;
	/// which lines are on screen? updated when \ref buffer_render is called.
	u32 first_line_on_screen, last_line_on_screen;

	/// to cache syntax highlighting properly, it is important to keep track of the
	/// first and last line modified since last frame.
	u32 frame_earliest_line_modified;
	/// see \ref frame_earliest_line_modified.
	u32 frame_latest_line_modified;

	/// lines
	Line *lines;
	/// last error
	char error[256];
	/// dynamic array of undo history
	BufferEdit *undo_history;
	/// dynamic array of redo history
	BufferEdit *redo_history;
};

/// an entry in a selector menu (e.g. the "open" menu)
typedef struct {
	/// label
	const char *name;
	/// if not NULL, this will show on the right side of the entry.
	const char *detail;
	/// color to draw text in
	u32 color;
	/// use this for whatever you want
	u64 userdata;
} SelectorEntry;

struct Selector {
	SelectorEntry *entries;
	u32 n_entries;
	Rect bounds;
	/// index where the selector thing is
	u32 cursor;
	float scroll;
	/// whether or not we should let the user select entries using a cursor.
	bool enable_cursor;
};

/// file entries for file selectors
typedef struct {
	/// just the file name
	char *name;
	/// full path
	char *path;
	FsType type;
} FileEntry;

struct FileSelector {
	Selector sel;
	Rect bounds;
	u32 n_entries;
	FileEntry *entries;
	char cwd[TED_PATH_MAX];
	/// indicates that this is for creating files, not opening files
	bool create_menu;
};

// A node is a collection of tabs OR a split of two node
struct Node {
	/// dynamic array of indices into ted->buffers, or `NULL` if this is a split
	u16 *tabs;
	/// number from 0 to 1 indicating where the split is.
	float split_pos;
	/// index of active tab in `tabs`.
	u16 active_tab;
	/// is the split vertical? if false, this split looks like a|b
	bool split_vertical;
	/// split left/upper half; index into `ted->nodes`
	u16 split_a;
	/// split right/lower half
	u16 split_b;
};

/// max number of buffers open at one time
#define TED_MAX_BUFFERS 256
/// max number of nodes open at one time
#define TED_MAX_NODES 256
/// max tabs per node
#define TED_MAX_TABS 100
/// max strings in all config files
#define TED_MAX_STRINGS 1000

/// "find" menu result
typedef struct {
	BufferPos start;
	BufferPos end;
} FindResult;

typedef struct {
	char *path;
	u32 line;
	u32 column;
	/// if this is 1, then column == UTF-32 index.
	/// if this is 4, for example, then column 4 in a line starting with a tab would
	/// be the character right after the tab.
	u8 columns_per_tab;
	/// which line in the build output corresponds to this error
	u32 build_output_line;
} BuildError;

/// `LSPSymbolKind`s are translated to these. this is a much coarser categorization
typedef enum {
	SYMBOL_OTHER,
	SYMBOL_FUNCTION,
	SYMBOL_FIELD,
	SYMBOL_TYPE,
	SYMBOL_VARIABLE,
	SYMBOL_CONSTANT,
	SYMBOL_KEYWORD
} SymbolKind;

/// data needed for autocompletion
typedef struct Autocomplete Autocomplete;

/// data needed for finding usages
typedef struct Usages Usages;

/// max number of signatures to display at a time.
#define SIGNATURE_HELP_MAX 5

/// "signature help" (LSP) is thing that shows the current parameter, etc.
typedef struct SignatureHelp SignatureHelp;

typedef struct DocumentLink DocumentLink;

/// "document link" information (LSP)
typedef struct {
	LSPDocumentID requested_document;
	LSPServerRequestID last_request;
	DocumentLink *links;
} DocumentLinks;

/// information for symbol rename (LSP)
typedef struct {
	char *new_name;
	LSPServerRequestID request_id;
} RenameSymbol;

/// "hover" information from LSP server
typedef struct {
	LSPServerRequestID last_request;
	/// is some hover info being displayed?
	bool open;
	/// text to display
	char *text;
	/// where the hover data is coming from.
	/// we use this to check if we need to refresh it.
	LSPDocumentPosition requested_position;
	/// range in document to highlight
	LSPRange range;
	/// how long the cursor has been hovering for
	double time;
} Hover;

/// symbol information for the definitions menu
typedef struct {
	char *name;
	char *detail;
	u32 color;
	/// is this from a LSP server (as opposed to ctags)?
	bool from_lsp;
	/// only set if `from_lsp = true`
	LSPDocumentPosition position;
} SymbolInfo;

typedef struct {
	LSPServerRequestID last_request;
	double last_request_time;	
	/// last query string which we sent a request for
	char *last_request_query;
	/// for "go to definition of..." menu
	Selector selector;
	/// an array of all definitions (gotten from workspace/symbols) for "go to definition" menu
	SymbolInfo *all_definitions;
} Definitions;

/// "highlight" information from LSP server
typedef struct {
	LSPServerRequestID last_request;
	LSPDocumentPosition requested_position;
	LSPHighlight *highlights;
} Highlights;

typedef struct {
	Command command;
	CommandArgument argument;
} Action;

typedef struct {
	// dynamic array
	Action *actions;
} Macro;

typedef struct {
	char *path;
	Font *font;
} LoadedFont;

typedef struct {
	vec2 pos;
	u8 times;
} MouseClick;

typedef struct {
	vec2 pos;
} MouseRelease;

struct Ted {
	/// all running LSP servers
	LSP *lsps[TED_LSP_MAX + 1];
	/// current time (see time_get_seconds), as of the start of this frame
	double frame_time;
	
	Macro macros[TED_MACRO_MAX];
	Macro *recording_macro;
	bool executing_macro;
	
	SDL_Window *window;
	LoadedFont *all_fonts;
	Font *font_bold;
	Font *font;
	TextBuffer *active_buffer;
	/// buffer we are currently drag-to-selecting in, if any
	TextBuffer *drag_buffer;
	/// while a menu or something is open, there is no active buffer. when the menu is closed,
	/// the old active buffer needs to be restored. that's what this stores.
	TextBuffer *prev_active_buffer; 
	Node *active_node;
	/// dynamic array of Settings. use Settings.context to figure out which one to use.
	Settings *all_settings;
	/// settings to use when no buffer is open
	Settings *default_settings;
	float window_width, window_height;
	vec2 mouse_pos;
	u32 mouse_state;
	/// `mouse_clicks[SDL_BUTTON_RIGHT]`, for example, is all the right mouse-clicks that have happened this frame
	MouseClick *mouse_clicks[4];
	MouseRelease *mouse_releases[4];
	/// total amount scrolled this frame
	int scroll_total_x, scroll_total_y; 
	/// currently open menu, or \ref MENU_NONE if no menu is open.
	Menu menu;
	FileSelector file_selector;
	Selector command_selector;
	/// general-purpose line buffer for inputs -- used for menus
	TextBuffer line_buffer;
	/// use for "find" term in find/find+replace
	TextBuffer find_buffer;
	/// "replace" for find+replace
	TextBuffer replace_buffer;
	/// buffer for build output (view only)
	TextBuffer build_buffer;
	/// used for command selector
	TextBuffer argument_buffer;
	/// time which the cursor error animation started (cursor turns red, e.g. when there's no autocomplete suggestion)
	double cursor_error_time;
	/// should start_cwd be searched for files? set to true if the executable isn't "installed"
	bool search_start_cwd;
	/// CWD `ted` was started in
	char start_cwd[TED_PATH_MAX];
	/// if set to true, the window will close next frame. NOTE: this doesn't check for unsaved changes!!
	bool quit;
	/// is the find or find+replace menu open?
	bool find;
	/// is the find+replace menu open?
	bool replace;
	/// find options
	bool find_regex, find_case_sensitive;
	/// flags used last time search term was compiled
	u32 find_flags;
	struct pcre2_real_code_32 *find_code;
	struct pcre2_real_match_data_32 *find_match_data;
	FindResult *find_results;
	/// invalid regex?
	bool find_invalid_pattern;
	/// if non-zero, the user is trying to execute this command, but there are unsaved changes
	Command warn_unsaved;
	/// are we showing the build output?
	bool build_shown;
	/// is the build process running?
	bool building;
	Autocomplete *autocomplete;
	SignatureHelp *signature_help;
	DocumentLinks document_links;
	Hover hover;
	Definitions definitions;
	Highlights highlights;
	Usages *usages;
	RenameSymbol rename_symbol;
	
	FILE *log;
	
	/// dynamic array of build errors
	BuildError *build_errors;
	/// build error we are currently "on"
	u32 build_error;
	
	/// used by menus to keep track of the scroll position so we can return to it.
	vec2d prev_active_buffer_scroll;
	
	SDL_Cursor *cursor_arrow, *cursor_ibeam, *cursor_wait,
		*cursor_resize_h, *cursor_resize_v, *cursor_hand, *cursor_move;
	/// which cursor to use this frame
	/// this should be set to one of the cursor_* members above, or NULL for no cursor
	SDL_Cursor *cursor;
	
	/// node containing tab user is dragging around, NULL if user is not dragging a tab
	Node *dragging_tab_node;
	/// index in dragging_tab_node->tabs
	u16 dragging_tab_idx;
	/// where the tab is being dragged from (i.e. mouse pos at start of drag action)
	vec2 dragging_tab_origin;
	
	/// if not `NULL`, points to the node whose split the user is currently resizing.
	Node *resizing_split;
	
	/// dynamic array of history of commands run with :shell (UTF-8)
	char **shell_history;
	/// for keeping track of where we are in the shell history.
	u32 shell_history_pos;

	// points to a selector if any is open, otherwise NULL.
	Selector *selector_open;
	
	/// what % of the screen the build output takes up
	float build_output_height;
	bool resizing_build_output;
	
	/// last time a save command was executed. used for bg-shaders.
	double last_save_time;

	Process *build_process;
	/// When we read the stdout from the build process, the tail end of the read could be an
	/// incomplete UTF-8 code point. This is where we store that "tail end" until more
	/// data is available. (This is up to 3 bytes, null terminated)
	char build_incomplete_codepoint[4];
	/// allows execution of multiple commands -- needed for tags generation
	char **build_queue;
	/// comma-separated list of files with unsaved changes (only applicable if warn_unsaved != 0)
	char warn_unsaved_names[TED_PATH_MAX];
	/// file name user is trying to overwrite
	char warn_overwrite[TED_PATH_MAX];
	/// file name which we want to reload
	char ask_reload[TED_PATH_MAX];
	char local_data_dir[TED_PATH_MAX];
	char global_data_dir[TED_PATH_MAX];
	/// home directory
	char home[TED_PATH_MAX];
	/// current working directory
	char cwd[TED_PATH_MAX];
	/// directory where we run the build command
	char build_dir[TED_PATH_MAX];
	/// where we are reading tags from
	char tags_dir[TED_PATH_MAX];
	bool nodes_used[TED_MAX_NODES];
	/// `nodes[0]` is always the "root node", if any buffers are open.
	Node nodes[TED_MAX_NODES];
	/// NOTE: the buffer at index 0 is reserved as a "null buffer" and should not be used.
	bool buffers_used[TED_MAX_BUFFERS];
	TextBuffer buffers[TED_MAX_BUFFERS];
	/// number of config file strings
	u32 nstrings;
	/// config file strings
	char *strings[TED_MAX_STRINGS];
	char window_title[256];
	
	/// little box used to display errors and info.
	char message[512];
	/// time message box was opened
	double message_time;
	MessageType message_type;
	MessageType message_shown_type;
	char message_shown[512];
};

#endif // !TED_PLUGIN

// === buffer.c ===
/// Returns `true` if the buffer is in view-only mode.
bool buffer_is_view_only(TextBuffer *buffer);
/// Set whether the buffer should be in view only mode.
void buffer_set_view_only(TextBuffer *buffer, bool view_only);
/// Get path to buffer's file. At most `bufsz` bytes are written to `buf`.
///
/// Returns the number of bytes needed to store the path including a null terminator,
/// or 1 if the buffer is unnamed.
size_t buffer_get_path(TextBuffer *buffer, char *buf, size_t bufsz);
/// Does this buffer have an error?
bool buffer_has_error(TextBuffer *buffer);
/// get buffer error
const char *buffer_get_error(TextBuffer *buffer);
/// clear buffer error
void buffer_clear_error(TextBuffer *buffer);
/// clear undo and redo history
void buffer_clear_undo_redo(TextBuffer *buffer);
/// is this buffer empty?
bool buffer_empty(TextBuffer *buffer);
/// returns the buffer's filename (not full path), or "Untitled" if this buffer is untitled.
const char *buffer_display_filename(TextBuffer *buffer);
/// does this buffer contained a named file (i.e. not a line buffer, not the build buffer, not untitled)
bool buffer_is_named_file(TextBuffer *buffer);
/// create a new empty buffer with no file name
void buffer_create(TextBuffer *buffer, Ted *ted);
/// create a new empty line buffer
void line_buffer_create(TextBuffer *buffer, Ted *ted);
/// does this buffer have unsaved changes?
bool buffer_unsaved_changes(TextBuffer *buffer);
/// returns the character after position pos, or 0 if pos is invalid
char32_t buffer_char_at_pos(TextBuffer *buffer, BufferPos pos);
/// returns the character after the cursor
char32_t buffer_char_at_cursor(TextBuffer *buffer);
/// returns the character before position pos, or 0 if pos is invalid or at the start of a line
char32_t buffer_char_before_pos(TextBuffer *buffer, BufferPos pos);
/// returns the character to the left of the cursor, or 0 if the cursor at the start of the line.
char32_t buffer_char_before_cursor(TextBuffer *buffer);
/// buffer position of start of file
BufferPos buffer_pos_start_of_file(TextBuffer *buffer);
/// buffer position of end of file
BufferPos buffer_pos_end_of_file(TextBuffer *buffer);
/// move position to matching bracket. returns the matching bracket character if there is one, otherwise returns 0 and does nothing.
char32_t buffer_pos_move_to_matching_bracket(TextBuffer *buffer, BufferPos *pos);
/// move cursor to matching bracket. returns true if cursor was to the right of a bracket.
bool buffer_cursor_move_to_matching_bracket(TextBuffer *buffer);
/// ensures that `p` refers to a valid position, moving it if needed.
void buffer_pos_validate(TextBuffer *buffer, BufferPos *p);
/// is this a valid buffer position?
bool buffer_pos_valid(TextBuffer *buffer, BufferPos p);
/// get programming language of buffer contents
Language buffer_language(TextBuffer *buffer);
/// clip the rectangle so it's all inside the buffer. returns true if there's any rectangle left.
bool buffer_clip_rect(TextBuffer *buffer, Rect *r);
/// Get the font used for this buffer.
Font *buffer_font(TextBuffer *buffer);
/// get LSP server which deals with this buffer
LSP *buffer_lsp(TextBuffer *buffer);
/// Get the settings used for this buffer.
Settings *buffer_settings(TextBuffer *buffer);
/// Get tab width for this buffer
u8 buffer_tab_width(TextBuffer *buffer);
/// Get whether or not to indent with spaces for this buffer.
bool buffer_indent_with_spaces(TextBuffer *buffer);
/// returns the number of lines in the buffer.
u32 buffer_get_num_lines(TextBuffer *buffer);
/// get line contents. does not include a newline character.
/// NOTE: this string will be invalidated when the line is edited!!!
/// only use it briefly!!
///
/// returns an empty string if `line_number` is out of range.
String32 buffer_get_line(TextBuffer *buffer, u32 line_number);
/// get line contents. does not include a newline character.
///
/// string must be freed by caller.
/// returns an empty string if `line_number` is out of range.
char *buffer_get_line_utf8(TextBuffer *buffer, u32 line_number);
/// get at most `nchars` characters starting from position `pos`.
/// returns the number of characters actually available.
/// you can pass NULL for text if you just want to know how many
/// characters *could* be accessed before the end of the file.
size_t buffer_get_text_at_pos(TextBuffer *buffer, BufferPos pos, char32_t *text, size_t nchars);
/// returns a UTF-32 string of at most `nchars` code points from `buffer` starting at `pos`
/// the string should be passed to str32_free.
String32 buffer_get_str32_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars);
/// get UTF-8 string at position, up to `nchars` code points (NOT bytes).
/// the resulting string should be freed.
char *buffer_get_utf8_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars);
/// Puts a UTF-8 string containing the contents of the buffer into out.
/// Returns the number of bytes, including a null terminator.
/// To use this function, first pass NULL for out to get the number of bytes you need to allocate.
size_t buffer_contents_utf8(TextBuffer *buffer, char *out);
/// Returns a UTF-8 string containing the contents of `buffer`.
/// The return value should be freed..
char *buffer_contents_utf8_alloc(TextBuffer *buffer);
/// perform a series of checks to make sure the buffer doesn't have any invalid values
void buffer_check_valid(TextBuffer *buffer);
/// free all resources used by the buffer
void buffer_free(TextBuffer *buffer);
/// clear buffer contents
void buffer_clear(TextBuffer *buffer);
/// returns the length of the `line_number`th line (0-indexed),
/// or 0 if `line_number` is out of range.
u32 buffer_line_len(TextBuffer *buffer, u32 line_number);
/// returns the number of lines of text in the buffer into *lines (if not NULL),
/// and the number of columns of text, i.e. the width of the longest column divided by the width of the space character, into *cols (if not NULL)
void buffer_text_dimensions(TextBuffer *buffer, u32 *lines, u32 *columns);
/// returns the number of rows of text that can fit in the buffer
float buffer_display_lines(TextBuffer *buffer);
/// returns the number of columns of text that can fit in the buffer
float buffer_display_cols(TextBuffer *buffer);
/// scroll by deltas
void buffer_scroll(TextBuffer *buffer, double dx, double dy);
/// returns the screen position of the character at the given position in the buffer.
vec2 buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos);
/// convert pixel coordinates to a position in the buffer, selecting the closest character.
/// returns false if the position is not inside the buffer, but still sets *pos to the closest character.
bool buffer_pixels_to_pos(TextBuffer *buffer, vec2 pixel_coords, BufferPos *pos);
/// scroll to `pos`, scrolling as little as possible while maintaining scrolloff.
void buffer_scroll_to_pos(TextBuffer *buffer, BufferPos pos);
/// scroll in such a way that this position is in the center of the screen
void buffer_scroll_center_pos(TextBuffer *buffer, BufferPos pos);
/// scroll so that the cursor is on screen
void buffer_scroll_to_cursor(TextBuffer *buffer);
/// scroll so that the cursor is in the center of the buffer's rectangle.
void buffer_center_cursor(TextBuffer *buffer);
/// returns the number of characters successfully moved by.
i64 buffer_pos_move_left(TextBuffer *buffer, BufferPos *pos, i64 by);
/// returns the number of characters successfully moved by.
i64 buffer_pos_move_right(TextBuffer *buffer, BufferPos *pos, i64 by);
/// returns the number of lines successfully moved by.
i64 buffer_pos_move_up(TextBuffer *buffer, BufferPos *pos, i64 by);
/// returns the number of lines successfully moved by.
i64 buffer_pos_move_down(TextBuffer *buffer, BufferPos *pos, i64 by);
/// set cursor position
void buffer_cursor_move_to_pos(TextBuffer *buffer, BufferPos pos);
/// returns the number of characters successfully moved by.
i64 buffer_cursor_move_left(TextBuffer *buffer, i64 by);
/// returns the number of characters successfully moved by.
i64 buffer_cursor_move_right(TextBuffer *buffer, i64 by);
/// returns the number of lines successfully moved by.
i64 buffer_cursor_move_up(TextBuffer *buffer, i64 by);
/// returns the number of lines successfully moved by.
i64 buffer_cursor_move_down(TextBuffer *buffer, i64 by);
/// returns the number of blank lines successfully moved by.
i64 buffer_pos_move_up_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by);
/// returns the number of blank lines successfully moved by.
i64 buffer_pos_move_down_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by);
/// returns the number of blank lines successfully moved by.
i64 buffer_cursor_move_up_blank_lines(TextBuffer *buffer, i64 by);
/// returns the number of blank lines successfully moved by.
i64 buffer_cursor_move_down_blank_lines(TextBuffer *buffer, i64 by);
/// returns the number of words successfully moved by.
i64 buffer_pos_move_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
/// returns the number of words successfully moved by.
i64 buffer_pos_move_left_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
/// returns the number of words successfully moved by.
i64 buffer_pos_move_right_words(TextBuffer *buffer, BufferPos *pos, i64 nwords);
/// returns the number of words successfully moved by.
i64 buffer_cursor_move_left_words(TextBuffer *buffer, i64 nwords);
/// returns the number of words successfully moved by.
i64 buffer_cursor_move_right_words(TextBuffer *buffer, i64 nwords);
/// move cursor to "previous" position (i.e. \ref CMD_PREVIOUS_POSITION)
void buffer_cursor_move_to_prev_pos(TextBuffer *buffer);
/// Get the start and end index of the word at the given position.
void buffer_word_span_at_pos(TextBuffer *buffer, BufferPos pos, u32 *word_start, u32 *word_end);
/// Returns a string of word characters (see is32_word) around the position,
/// or an empty string if neither of the characters to the left and right of the cursor are word characters.
/// NOTE: The string is invalidated when the buffer is changed!!!
/// The return value should NOT be freed.
String32 buffer_word_at_pos(TextBuffer *buffer, BufferPos pos);
/// Get the word at the cursor.
/// NOTE: The string is invalidated when the buffer is changed!!!
/// The return value should NOT be freed.
String32 buffer_word_at_cursor(TextBuffer *buffer);
/// Get a UTF-8 string consisting of the word at the cursor.
/// The return value should be freed.
char *buffer_word_at_cursor_utf8(TextBuffer *buffer);
/// Used for \ref CMD_INCREMENT_NUMBER and \ref CMD_DECREMENT_NUMBER
///
/// Moves `*ppos` to the start of the (new) number.
/// returns false if there was no number at `*ppos`.
bool buffer_change_number_at_pos(TextBuffer *buffer, BufferPos *ppos, i64 by);
/// Used for \ref CMD_INCREMENT_NUMBER and \ref CMD_DECREMENT_NUMBER
void buffer_change_number_at_cursor(TextBuffer *buffer, i64 argument);
/// Buffer position corresponding to the start of line `line` (0-indexed).
BufferPos buffer_pos_start_of_line(TextBuffer *buffer, u32 line);
/// Buffer position corresponding to the end of line `line` (0-indexed).
BufferPos buffer_pos_end_of_line(TextBuffer *buffer, u32 line);
/// Move cursor to the start of the line, like the Home key does.
void buffer_cursor_move_to_start_of_line(TextBuffer *buffer);
/// Move cursor to the end of the line, like the End key does.
void buffer_cursor_move_to_end_of_line(TextBuffer *buffer);
/// Move cursor to the start of the file, like Ctrl+Home does.
void buffer_cursor_move_to_start_of_file(TextBuffer *buffer);
/// Move cursor to the end of the file, like Ctrl+End does.
void buffer_cursor_move_to_end_of_file(TextBuffer *buffer);
/// Get the LSPDocumentID corresponding to the file this buffer contains.
/// The return value is only useful if `buffer_lsp(buffer) != NULL`.
LSPDocumentID buffer_lsp_document_id(TextBuffer *buffer);
/// Get LSPPosition corresponding to position in buffer.
LSPPosition buffer_pos_to_lsp_position(TextBuffer *buffer, BufferPos pos);
/// Get LSPDocumentPosition corresponding to position in buffer.
LSPDocumentPosition buffer_pos_to_lsp_document_position(TextBuffer *buffer, BufferPos pos);
/// Convert LSPPosition to BufferPos.
BufferPos buffer_pos_from_lsp(TextBuffer *buffer, LSPPosition lsp_pos);
/// Get the cursor position as an LSPPosition.
LSPPosition buffer_cursor_pos_as_lsp_position(TextBuffer *buffer);
/// Get the cursor position as an LSPDocumentPosition.
LSPDocumentPosition buffer_cursor_pos_as_lsp_document_position(TextBuffer *buffer);
/// Put text at a position. All text insertion should eventually go through this function.
BufferPos buffer_insert_text_at_pos(TextBuffer *buffer, BufferPos pos, String32 str);
/// Insert a single character at a position.
void buffer_insert_char_at_pos(TextBuffer *buffer, BufferPos pos, char32_t c);
/// Set the selection to between `buffer->cursor_pos` and `pos`,
/// and move the cursor to `pos`.
void buffer_select_to_pos(TextBuffer *buffer, BufferPos pos);
/// Like shift+left, move cursor `nchars` chars to the left, selecting everything in between.
void buffer_select_left(TextBuffer *buffer, i64 nchars);
/// Like shift+right, move cursor `nchars` chars to the right, selecting everything in between.
void buffer_select_right(TextBuffer *buffer, i64 nchars);
/// Like shift+down, move cursor `nchars` lines down, selecting everything in between.
void buffer_select_down(TextBuffer *buffer, i64 nchars);
/// Like shift+up, move cursor `nchars` lines up, selecting everything in between.
void buffer_select_up(TextBuffer *buffer, i64 nchars);
/// Move the cursor `by` lines down, selecting everything in between.
void buffer_select_down_blank_lines(TextBuffer *buffer, i64 by);
/// Move the cursor `by` lines up, selecting everything in between.
void buffer_select_up_blank_lines(TextBuffer *buffer, i64 by);
/// Move the cursor `nwords` words left, selecting everything in between.
void buffer_select_left_words(TextBuffer *buffer, i64 nwords);
/// Move the cursor `nwords` words right, selecting everything in between.
void buffer_select_right_words(TextBuffer *buffer, i64 nwords);
/// Like Shift+Home, move cursor to start of line and select everything in between.
void buffer_select_to_start_of_line(TextBuffer *buffer);
/// Like Shift+End, move cursor to end of line and select everything in between.
void buffer_select_to_end_of_line(TextBuffer *buffer);
/// Like Ctrl+Shift+Home, move cursor to start of file and select everything in between.
void buffer_select_to_start_of_file(TextBuffer *buffer);
/// Like Ctrl+Shift+End, move cursor to end of file and select everything in between.
void buffer_select_to_end_of_file(TextBuffer *buffer);
/// select the word the cursor is inside of
void buffer_select_word(TextBuffer *buffer);
/// select the line the cursor is currently on
void buffer_select_line(TextBuffer *buffer);
/// select all of the buffer's contents
void buffer_select_all(TextBuffer *buffer);
/// Remove current selection.
void buffer_deselect(TextBuffer *buffer);
/// Scroll up by `npages` pages
void buffer_page_up(TextBuffer *buffer, i64 npages);
/// Scroll down by `npages` pages
void buffer_page_down(TextBuffer *buffer, i64 npages);
/// Scroll up by `npages` pages, selecting everything in between
void buffer_select_page_up(TextBuffer *buffer, i64 npages);
/// Scroll down by `npages` pages, selecting everything in between
void buffer_select_page_down(TextBuffer *buffer, i64 npages);
/// Delete `nchars` characters starting from `pos`.
/// All text deletion should eventually go through this function.
void buffer_delete_chars_at_pos(TextBuffer *buffer, BufferPos pos, i64 nchars);
/// Delete characters between the two positions.
/// The order of `p1` and `p2` is irrelevant.
i64 buffer_delete_chars_between(TextBuffer *buffer, BufferPos p1, BufferPos p2);
/// Delete current selection.
i64 buffer_delete_selection(TextBuffer *buffer);
/// Insert UTF-32 text at the cursor, and move the cursor to the end of it.
void buffer_insert_text_at_cursor(TextBuffer *buffer, String32 str);
/// Insert a single character at the cursor, and move the cursor past it.
void buffer_insert_char_at_cursor(TextBuffer *buffer, char32_t c);
/// Insert UTF-8 text at the position.
void buffer_insert_utf8_at_pos(TextBuffer *buffer, BufferPos pos, const char *utf8);
/// Insert UTF-8 text at the cursor, and move the cursor to the end of it.
void buffer_insert_utf8_at_cursor(TextBuffer *buffer, const char *utf8);
/// Insert a "tab" at the cursor position, and move the cursor past it.
/// This inserts spaces if ted is configured to indent with spaces.
void buffer_insert_tab_at_cursor(TextBuffer *buffer);
/// Insert a newline at the cursor position.
/// If `buffer` is a line buffer, this "submits" the buffer.
/// If not, this auto-indents the next line, and moves the cursor to it.
void buffer_newline(TextBuffer *buffer);
/// Delete `nchars` characters after the cursor.
void buffer_delete_chars_at_cursor(TextBuffer *buffer, i64 nchars);
/// Delete `nchars` characters before *pos, and set *pos to just before the deleted characters.
/// Returns the number of characters actually deleted.
i64 buffer_backspace_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nchars);
/// Delete `nchars` characters before the cursor position, and set the cursor position accordingly.
/// Returns the number of characters actually deleted.
i64 buffer_backspace_at_cursor(TextBuffer *buffer, i64 nchars);
/// Delete `nwords` words after the position.
void buffer_delete_words_at_pos(TextBuffer *buffer, BufferPos pos, i64 nwords);
/// Delete `nwords` words after the cursor.
void buffer_delete_words_at_cursor(TextBuffer *buffer, i64 nwords);
/// Delete `nwords` words before *pos, and set *pos to just before the deleted words.
/// Returns the number of words actually deleted.
void buffer_backspace_words_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nwords);
/// Delete `nwords` words before the cursor position, and set the cursor position accordingly.
/// Returns the number of words actually deleted.
void buffer_backspace_words_at_cursor(TextBuffer *buffer, i64 nwords);
/// Undo `ntimes` times
void buffer_undo(TextBuffer *buffer, i64 ntimes);
/// Redo `ntimes` times
void buffer_redo(TextBuffer *buffer, i64 ntimes);
/// Start a new "edit chain". Undoing once after an edit chain will undo everything in the chain.
void buffer_start_edit_chain(TextBuffer *buffer);
/// End the edit chain.
void buffer_end_edit_chain(TextBuffer *buffer);
/// Copy the current selection to the clipboard.
void buffer_copy(TextBuffer *buffer);
/// Copy the current selection to the clipboard, and delete it.
void buffer_cut(TextBuffer *buffer);
/// Insert the clipboard contents at the cursor position.
void buffer_paste(TextBuffer *buffer);
/// Load the file `path`. If `path` is not an absolute path,
/// this function will fail.
bool buffer_load_file(TextBuffer *buffer, const char *path);
/// Reloads the file loaded in the buffer.
void buffer_reload(TextBuffer *buffer);
/// has this buffer been changed by another program since last save?
bool buffer_externally_changed(TextBuffer *buffer);
/// Clear `buffer`, and set its path to `path`.
/// if `path` is NULL, this will turn `buffer` into an untitled buffer.
void buffer_new_file(TextBuffer *buffer, const char *path);
/// Save the buffer to its current filename. This will rewrite the entire file,
/// even if there are no unsaved changes.
bool buffer_save(TextBuffer *buffer);
/// save, but with a different path
bool buffer_save_as(TextBuffer *buffer, const char *new_filename);
/// index of first line that will be displayed on screen
u32 buffer_first_rendered_line(TextBuffer *buffer);
/// index of last line that will be displayed on screen
u32 buffer_last_rendered_line(TextBuffer *buffer);
/// go to the definition/declaration/etc of the word at the cursor.
void buffer_goto_word_at_cursor(TextBuffer *buffer, GotoType type);
/// process a mouse click.
/// returns true if the event was consumed.
bool buffer_handle_click(Ted *ted, TextBuffer *buffer, vec2 click, u8 times);
/// render the buffer in the given rectangle
void buffer_render(TextBuffer *buffer, Rect r);
/// indent the given lines
void buffer_indent_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
/// de-indent the given lines
void buffer_dedent_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
/// indent the selected lines
void buffer_indent_selection(TextBuffer *buffer);
/// de-indent the selected lines
void buffer_dedent_selection(TextBuffer *buffer);
/// indent the line the cursor is on
void buffer_indent_cursor_line(TextBuffer *buffer);
/// de-indent the line the cursor is on
void buffer_dedent_cursor_line(TextBuffer *buffer);
/// comment the lines from `first_line` to `last_line`
void buffer_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
/// uncomment the lines from `first_line` to `last_line`
void buffer_uncomment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
/// comment the lines from `first_line` to `last_line`, or uncomment them if they're all commented.
void buffer_toggle_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line);
/// comment the selected lines, or uncomment them if they're all commented
void buffer_toggle_comment_selection(TextBuffer *buffer);
/// highlight an \ref LSPRange in this buffer.
///
/// make sure to call \ref gl_geometry_draw after this
void buffer_highlight_lsp_range(TextBuffer *buffer, LSPRange range, ColorSetting color);
/// returns true if `p1` and `p2` are equal
bool buffer_pos_eq(BufferPos p1, BufferPos p2);
/// returns `-1` if `p1` comes before `p2`
///
/// `+1` if `p1` comes after `p2`
///
/// `0`  if `p1` = `p2`
///
/// faster than \ref buffer_pos_diff (constant time)
int buffer_pos_cmp(BufferPos p1, BufferPos p2);
/// returns "`p2 - p1`", that is, the number of characters between `p1` and `p2`.
i64 buffer_pos_diff(TextBuffer *buffer, BufferPos p1, BufferPos p2);

// === build.c ===
/// clear build errors and stop
void build_stop(Ted *ted);
/// call before adding anything to the build queue
void build_queue_start(Ted *ted);
/// add a command to the build queue. call build_queue_start before this.
void build_queue_command(Ted *ted, const char *command);
/// call this after calling build_queue_start, build_queue_command.
/// make sure you set ted->build_dir before running this!
void build_queue_finish(Ted *ted);
/// set up the build output buffer.
void build_setup_buffer(Ted *ted);
/// run a single command in the build window.
/// make sure you set ted->build_dir before running this!
void build_start_with_command(Ted *ted, const char *command);
/// figure out which build command to run, and run it.
void build_start(Ted *ted);
/// go to next build error
void build_next_error(Ted *ted);
/// go to previous build error
void build_prev_error(Ted *ted);
/// find build errors in build buffer.
void build_check_for_errors(Ted *ted);
void build_frame(Ted *ted, float x1, float y1, float x2, float y2);

// === colors.c ===
#if !TED_PLUGIN
void color_init(void);
/// which color setting should be used for the given symbol kind.
/// this is the color used in the autocomplete selector, for example.
ColorSetting color_for_symbol_kind(SymbolKind kind);
#endif
ColorSetting color_setting_from_str(const char *str);
const char *color_setting_to_str(ColorSetting s);
Status color_from_str(const char *str, u32 *color);
/// perform SRC_ALPHA, ONE_MINUS_SRC_ALPHA blending with `bg` and `fg`.
u32 color_blend(u32 bg, u32 fg);
/// multiply color's alpha value by `opacity`.
u32 color_apply_opacity(u32 color, float opacity);

// === command.c ===
#if !TED_PLUGIN
void command_init(void);
void command_execute_ex(Ted *ted, Command c, const CommandArgument *argument, const CommandContext *context);
#endif
Command command_from_str(const char *str);
const char *command_to_str(Command c);
void command_execute(Ted *ted, Command c, i64 argument);
void command_execute_string_argument(Ted *ted, Command c, const char *string);

// === config.c ===
#if !TED_PLUGIN
/// first, we read all config files, then we parse them.
/// this is because we want less specific settings (e.g. settings applied
/// to all languages instead of one particular language) to be applied first,
/// then more specific settings are based off of those.
///
/// EXAMPLE:
/// ```
///   ---config file 1---
///     [Javascript.core]
///     syntax-highlighting = off
///     (inherits tab-width = 4)
///     [CSS.core]
///     tab-width = 2 (overrides tab-width = 4)
///   ---config file 2---
///     [core]
///     tab-width = 4
/// ```
void config_read(Ted *ted, ConfigPart **pparts, const char *filename);
void config_parse(Ted *ted, ConfigPart **pparts);
void config_free(Ted *ted);
/// how well does this settings context fit the given path and language?
/// the context with the highest score will be chosen.
long context_score(const char *path, Language lang, const SettingsContext *context);
#endif // !TED_PLUGIN
/// returns the best guess for the root directory of the project containing `path`
/// (which should be an absolute path).
/// the return value should be freed.
char *settings_get_root_dir(Settings *settings, const char *path);

// === find.c ===
/// which buffer will be searched?
TextBuffer *find_search_buffer(Ted *ted);
/// height of the find/find+replace menu in pixels
float find_menu_height(Ted *ted);
/// update find results.
/// if `force` is true, the results will be updated even if the pattern & flags have not been changed.
void find_update(Ted *ted, bool force);
/// replace the match we are currently highlighting, or do nothing if there is no highlighted match
void find_replace(Ted *ted);
/// go to next find result
void find_next(Ted *ted);
/// go to previous find result
void find_prev(Ted *ted);
/// replace all matches
void find_replace_all(Ted *ted);
void find_menu_frame(Ted *ted, Rect menu_bounds);
/// open the find/find+replace menu.
void find_open(Ted *ted, bool replace);
/// close the find/find+replace menu.
void find_close(Ted *ted);

// === gl.c ===
/// set by main()
extern float gl_window_width, gl_window_height;
/// set by main()
extern int gl_version_major, gl_version_minor;

/// macro trickery to avoid having to write every GL function multiple times
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

#if !TED_PLUGIN
/// get addresses of GL functions
void gl_get_procs(void);
/// create a new reference-counted shader-array-buffer object.
GlRcSAB *gl_rc_sab_new(GLuint shader, GLuint array, GLuint buffer);
/// increase reference count on `s`.
void gl_rc_sab_incref(GlRcSAB *s);
/// decrease reference count on `*ps`, and set `*ps` to NULL if the reference count is 0.
void gl_rc_sab_decref(GlRcSAB **ps);
/// create a new reference-counted texture.
GlRcTexture *gl_rc_texture_new(GLuint texture);
/// increase reference count on `t`.
void gl_rc_texture_incref(GlRcTexture *t);
/// decrease reference count on `*t`, and set `*t` to NULL if the reference count is 0.
void gl_rc_texture_decref(GlRcTexture **pt);
/// initialize geometry stuff
void gl_geometry_init(void);
#endif // !TED_PLUGIN
/// create and compile a shader
GLuint gl_compile_shader(char error_buf[256], const char *code, GLenum shader_type);
/// create new shader program from shaders
GLuint gl_link_program(char error_buf[256], GLuint *shaders, size_t count);
/// create a shader program from vertex shader and fragment shader source
GLuint gl_compile_and_link_shaders(char error_buf[256], const char *vshader_code, const char *fshader_code);
/// prints a debug message if `attrib` is not found
GLuint gl_attrib_location(GLuint program, const char *attrib);
/// prints a debug message if `uniform` is not found
GLint gl_uniform_location(GLuint program, const char *uniform);
/// queue a filled rectangle with the given color.
void gl_geometry_rect(Rect r, u32 color_rgba);
/// queue the border of a rectangle with the given color.
void gl_geometry_rect_border(Rect r, float border_thickness, u32 color);
/// draw all queued geometry
void gl_geometry_draw(void);
/// create an OpenGL texture object from an image file.
GLuint gl_load_texture_from_image(const char *path);

// === ide-autocomplete.c ===
#if !TED_PLUGIN
void autocomplete_init(Ted *ted);
void autocomplete_quit(Ted *ted);
void autocomplete_process_lsp_response(Ted *ted, const LSPResponse *response);
#endif
/// is the autocomplete box open?
bool autocomplete_is_open(Ted *ted);
/// is there a phantom completion being displayed?
bool autocomplete_has_phantom(Ted *ted);
/// is this point in the autocomplete box?
bool autocomplete_box_contains_point(Ted *ted, vec2 point);
/// open autocomplete
/// trigger should either be a character (e.g. '.') or one of the TRIGGER_* constants.
void autocomplete_open(Ted *ted, uint32_t trigger);
/// select the completion the cursor is on,
/// or select the phantom completion if there is one.
void autocomplete_select_completion(Ted *ted);
/// scroll completion list
void autocomplete_scroll(Ted *ted, i32 by);
/// move cursor to next completion
void autocomplete_next(Ted *ted);
/// move cursor to previous completion
void autocomplete_prev(Ted *ted);
/// close completion menu
void autocomplete_close(Ted *ted);
void autocomplete_frame(Ted *ted);

// === ide-definitions.c ===
/// go to the definition of `name`.
/// if `lsp` is NULL, tags will be used.
/// Note: the document position is required for LSP requests because of overloading (where the name
/// alone isn't sufficient)
void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition pos, GotoType type);
/// cancel the last go-to-definition / find symbols request.
void definition_cancel_lookup(Ted *ted);
void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response);
/// open the definitions menu
void definitions_selector_open(Ted *ted);
/// update the definitions menu
void definitions_selector_update(Ted *ted);
void definitions_selector_render(Ted *ted, Rect bounds);
/// close the definitions menu
void definitions_selector_close(Ted *ted);
void definitions_frame(Ted *ted);

// === ide-document-link.c ===
void document_link_frame(Ted *ted);
void document_link_process_lsp_response(Ted *ted, const LSPResponse *response);
/// get document link at this position in the active buffer.
///
/// the returned pointer won't be freed immediately, but could be on the next frame,
/// so don't keep it around long.
const char *document_link_at_buffer_pos(Ted *ted, BufferPos pos);
void document_link_clear(Ted *ted);

// === ide-highlights.c ===
void highlights_close(Ted *ted);
void highlights_process_lsp_response(Ted *ted, const LSPResponse *response);
void highlights_frame(Ted *ted);

// === ide-hover.c ===
void hover_close(Ted *ted);
void hover_process_lsp_response(Ted *ted, const LSPResponse *response);
void hover_frame(Ted *ted, double dt);

// === ide-rename-symbol.c ===
void rename_symbol_clear(Ted *ted);
void rename_symbol_frame(Ted *ted);
void rename_symbol_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-signature-help.c ===
#if !TED_PLUGIN
void signature_help_init(Ted *ted);
void signature_help_quit(Ted *ted);
void signature_help_frame(Ted *ted);
void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response);
#endif
/// figure out new signature help
void signature_help_retrigger(Ted *ted);
/// open signature help. `trigger` should either be the trigger character (e.g. ',')
/// or one of the TRIGGER_* constants.
void signature_help_open(Ted *ted, uint32_t trigger);
bool signature_help_is_open(Ted *ted);
void signature_help_close(Ted *ted);

// === ide-usages.c ===
#if !TED_PLUGIN
void usages_init(Ted *ted);
void usages_process_lsp_response(Ted *ted, const LSPResponse *response);
void usages_frame(Ted *ted);
void usages_quit(Ted *ted);
#endif
/// cancel the last "find usages" request
void usages_cancel_lookup(Ted *ted);
/// find usages for word under the cursor in the active buffer.
void usages_find(Ted *ted);

// === macro.c ===
#if !TED_PLUGIN
void macro_add(Ted *ted, Command command, const CommandArgument *argument);
#endif
void macro_start_recording(Ted *ted, u32 index);
void macro_stop_recording(Ted *ted);
void macro_execute(Ted *ted, u32 index);
void macros_free(Ted *ted);

// === menu.c ===
void menu_close(Ted *ted);
void menu_open(Ted *ted, Menu menu);
/// process a :escape command (by default this happens when the escape key is pressed)
void menu_escape(Ted *ted);
/// get width of menu in pixels
float menu_get_width(Ted *ted);
/// get rectangle which menu takes up
Rect menu_rect(Ted *ted);
void menu_update(Ted *ted);
void menu_render(Ted *ted);
/// move to next/previous command
void menu_shell_move(Ted *ted, int direction);
/// move to previous command
void menu_shell_up(Ted *ted);
/// move to next command
void menu_shell_down(Ted *ted);

// === node.c ===
void node_switch_to_tab(Ted *ted, Node *node, u16 new_tab_index);
/// go to the `n`th next tab (e.g. `n=1` goes to the next tab)
/// going past the end of the tabs will "wrap around" to the first one.
void node_tab_next(Ted *ted, Node *node, i64 n);
/// go to the `n`th previous tab (e.g. `n=1` goes to the previous tab)
/// going before the first tab will "wrap around" to the last one.
void node_tab_prev(Ted *ted, Node *node, i64 n);
/// switch to a specific tab. if `tab` is out of range, nothing happens.
void node_tab_switch(Ted *ted, Node *node, i64 tab);
/// swap the position of two tabs
void node_tabs_swap(Node *node, u16 tab1, u16 tab2);
void node_free(Node *node);
/// returns index of parent in ted->nodes, or -1 if this is the root node.
i32 node_parent(Ted *ted, u16 node_idx);
/// join this node with its sibling
void node_join(Ted *ted, Node *node);
/// close a node, WITHOUT checking for unsaved changes
void node_close(Ted *ted, u16 node_idx);
/// close tab, WITHOUT checking for unsaved changes!
/// returns true if the node is still open
bool node_tab_close(Ted *ted, Node *node, u16 index);
void node_frame(Ted *ted, Node *node, Rect r);
/// make a split
void node_split(Ted *ted, Node *node, bool vertical);
/// switch to the other side of the current split.
void node_split_switch(Ted *ted);
/// swap the two sides of the current split.
void node_split_swap(Ted *ted);

// === session.c ===
void session_write(Ted *ted);
void session_read(Ted *ted);

// === syntax.c ===
/// register a new language for `ted`.
///
/// this should be done before loading configs so language-specific settings are recognized properly.
void syntax_register_language(const LanguageInfo *info);
/// register ted's built-in languages.
void syntax_register_builtin_languages(void);
/// returns `true` if `language` is a valid language ID
bool language_is_valid(Language language);
/// read language name from `str`. returns `LANG_NONE` if `str` is invalid.
Language language_from_str(const char *str);
/// convert language to string
const char *language_to_str(Language language);
/// get the color setting associated with the given syntax highlighting type
ColorSetting syntax_char_type_to_color_setting(SyntaxCharType t);
/// returns ')' for '(', etc., or 0 if c is not an opening bracket
char32_t syntax_matching_bracket(Language lang, char32_t c);
/// returns true for opening brackets, false for closing brackets/non-brackets
bool syntax_is_opening_bracket(Language lang, char32_t c);
/// This is the main syntax highlighting function. It will determine which colors to use for each character.
/// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
/// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
/// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, const char32_t *line, u32 line_len, SyntaxCharType *char_types);
/// free up resources used by syntax.c
void syntax_quit(void);

// === tags.c ===
void tags_generate(Ted *ted, bool run_in_build_window);
/// find all tags beginning with the given prefix, returning them into `*out`, writing at most out_size entries.
/// you may pass NULL for `out`, in which case just the number of matching tags is returned
/// (still maxing out at `out_size`).
/// each element in `out` should be freed when you're done with them.
size_t tags_beginning_with(Ted *ted, const char *prefix, char **out, size_t out_size, bool error_if_tags_does_not_exist);
/// go to the definition of the given tag
bool tag_goto(Ted *ted, const char *tag);
#if !TED_PLUGIN
/// get all tags in the tags file as SymbolInfos.
SymbolInfo *tags_get_symbols(Ted *ted);
#endif

// === ted.c ===
/// for fatal errors
void die(PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
/// returns the current active buffer, or NULL if no buffer is active.
TextBuffer *ted_get_active_buffer(Ted *ted);
/// set title of ted window
void ted_set_window_title(Ted *ted, const char *title);
/// returns `true` if the given key is down
bool ted_is_key_down(Ted *ted, SDL_Keycode key);
/// returns `true` if the given \ref KeyCombo is down
bool ted_is_key_combo_down(Ted *ted, KeyCombo key_combo);
/// returns `true` if either ctrl key is down
bool ted_is_ctrl_down(Ted *ted);
/// returns `true` if either shift key is down
bool ted_is_shift_down(Ted *ted);
/// returns `true` if either alt key is down
bool ted_is_alt_down(Ted *ted);
/// see \ref KEY_MODIFIER_CTRL, etc.
u32 ted_get_key_modifier(Ted *ted);
/// was there a click in this rectangle this frame?
bool ted_clicked_in_rect(Ted *ted, Rect rect);
/// display a message to the user
void ted_set_message(Ted *ted, MessageType type, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(3, 4);
/// display an error to the user
void ted_error(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
/// display a warning to the user
void ted_warn(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
/// display information to the user
void ted_info(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
/// for information that should be logged
void ted_log(Ted *ted, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
/// set error to "out of memory" message.
void ted_out_of_mem(Ted *ted);
/// allocate memory, producing an error message and returning NULL on failure
void *ted_malloc(Ted *ted, size_t size);
/// allocate memory, producing an error message and returning NULL on failure
void *ted_calloc(Ted *ted, size_t n, size_t size);
/// allocate memory, producing an error message and returning NULL on failure
void *ted_realloc(Ted *ted, void *p, size_t new_size);
/// Check the various places a ted data file could be
/// (i.e. look for it in the local and global data directories),
/// and return the full path.
Status ted_get_file(Ted const *ted, const char *name, char *out, size_t outsz);
/// get full path relative to ted->cwd.
void ted_path_full(Ted *ted, const char *relpath, char *abspath, size_t abspath_size);
/// set ted->active_buffer to something nice
void ted_reset_active_buffer(Ted *ted);
/// set ted's error message to the buffer's error.
void ted_error_from_buffer(Ted *ted, TextBuffer *buffer);
/// Returns the buffer containing the file at `path`, or NULL if there is none.
TextBuffer *ted_get_buffer_with_file(Ted *ted, const char *path);
/// save all buffers
bool ted_save_all(Ted *ted);
/// reload all buffers from their files
void ted_reload_all(Ted *ted);
/// Load all the fonts ted will use, freeing any previous ones.
void ted_load_fonts(Ted *ted);
/// Change ted's font size. Avoid calling this super often since it trashes all current font textures.
void ted_change_text_size(Ted *ted, float new_size);
/// Free all of ted's fonts.
void ted_free_fonts(Ted *ted);
/// Get likely root directory of project containing `path`.
/// The returned value should be freed.
char *ted_get_root_dir_of(Ted *ted, const char *path);
/// Get the root directory of the project containing the active buffer's file,
/// or `ted->cwd` if no file is open.
/// The returned value should be freed.
char *ted_get_root_dir(Ted *ted);
/// the settings of the active buffer, or the default settings if there is no active buffer
Settings *ted_active_settings(Ted *ted);
/// Get the settings for a file at the given path in the given language.
Settings *ted_get_settings(Ted *ted, const char *path, Language language);
/// Get LSP by ID. Returns NULL if there is no LSP with that ID.
LSP *ted_get_lsp_by_id(Ted *ted, LSPID id);
/// Get LSP which should be used for the given path and language.
/// If no running LSP server would cover the path and language, a new one is
/// started if possible.
/// Returns NULL on failure (e.g. there is no LSP server
/// specified for the given path and language).
LSP *ted_get_lsp(Ted *ted, const char *path, Language language);
/// Get the LSP of the active buffer/ted->cwd.
/// Returns NULL if there is no such server.
LSP *ted_active_lsp(Ted *ted);
/// get the value of the given color setting, according to `ted_active_settings(ted)`.
u32 ted_active_color(Ted *ted, ColorSetting color);
/// open the given file, or switch to it if it's already open.
/// returns true on success.
bool ted_open_file(Ted *ted, const char *filename);
/// create a new buffer for the file `filename`, or open it if it's already open.
/// if `filename` is NULL, this creates an untitled buffer.
/// returns true on success.
bool ted_new_file(Ted *ted, const char *filename);
/// returns the index of an available buffer, or -1 if none are available 
i32 ted_new_buffer(Ted *ted);
/// Returns the index of an available node, or -1 if none are available 
i32 ted_new_node(Ted *ted);
/// Opposite of ted_new_buffer
/// Make sure you set active_buffer to something else if you delete it!
void ted_delete_buffer(Ted *ted, u16 index);
/// save all changes to all buffers with unsaved changes.
bool ted_save_all(Ted *ted);
/// sets the active buffer to this buffer, and updates active_node, etc. accordingly
/// you can pass NULL to buffer to make it so no buffer is active.
void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer);
/// switch to this node
void ted_node_switch(Ted *ted, Node *node);
/// load ted.cfg files
void ted_load_configs(Ted *ted, bool reloading);
/// handle a key press
void ted_press_key(Ted *ted, SDL_Keycode keycode, SDL_Keymod modifier);
/// get the buffer and buffer position where the mouse is.
/// returns false if the mouse is not in a buffer.
Status ted_get_mouse_buffer_pos(Ted *ted, TextBuffer **pbuffer, BufferPos *ppos);
/// make the cursor red for a bit to indicate an error (e.g. no autocompletions)
void ted_flash_error_cursor(Ted *ted);
/// go to `path` at line `line` and index `index`, opening a new buffer if necessary.
/// if `is_lsp` is set to true, `index` is interpreted as a UTF-16 offset rather than a UTF-32 offset.
void ted_go_to_position(Ted *ted, const char *path, u32 line, u32 index, bool is_lsp);
/// go to this LSP document position, opening a new buffer containing the file if necessary.
void ted_go_to_lsp_document_position(Ted *ted, LSP *lsp, LSPDocumentPosition position);
/// cancel this LSP request. also zeroes *request
/// if *request is zeroed, this does nothing.
void ted_cancel_lsp_request(Ted *ted, LSPServerRequestID *request);
/// how tall is a line buffer?
float ted_line_buffer_height(Ted *ted);
/// check for orphaned nodes and node cycles
void ted_check_for_node_problems(Ted *ted);
/// convert LSPWindowMessageType to MessageType
MessageType ted_message_type_from_lsp(LSPWindowMessageType type);
/// get colors to use for message box
void ted_color_settings_for_message_type(MessageType type, ColorSetting *bg_color, ColorSetting *border_color);

// === ui.c ===
/// move selector cursor up by `n` entries
void selector_up(Ted *ted, Selector *s, i64 n);
/// move selector cursor down by `n` entries
void selector_down(Ted *ted, Selector *s, i64 n);
/// sort entries alphabetically
void selector_sort_entries_by_name(Selector *s);
/// returns a null-terminated UTF-8 string of the entry selected, or NULL if none was.
/// also, sel->cursor will be set to the index of the entry, even if the mouse was used.
/// you should call free() on the return value.
char *selector_update(Ted *ted, Selector *s);
/// NOTE: also renders the line buffer
void selector_render(Ted *ted, Selector *s);
void file_selector_free(FileSelector *fs);
/// returns the name of the selected file, or NULL if none was selected.
/// the returned pointer should be freed.
char *file_selector_update(Ted *ted, FileSelector *fs);
void file_selector_render(Ted *ted, FileSelector *fs);
vec2 button_get_size(Ted *ted, const char *text);
void button_render(Ted *ted, Rect button, const char *text, u32 color);
/// returns true if the button was clicked on.
bool button_update(Ted *ted, Rect button);
/// returns selected option, or POPUP_NONE if none was selected
PopupOption popup_update(Ted *ted, u32 options);
void popup_render(Ted *ted, u32 options, const char *title, const char *body);
vec2 checkbox_frame(Ted *ted, bool *value, const char *label, vec2 pos);


#ifdef __cplusplus
} // extern "C"
#endif

#endif
