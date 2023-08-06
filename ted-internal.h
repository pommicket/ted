/// \file
///
/// this file is included by (almost) all of ted's source files.
/// it may change arbitrarily so plugin authors should not include it!

#ifndef TED_INTERNAL_H_
#define TED_INTERNAL_H_

#include "ted.h"
#include "lsp.h"
#include "os.h"
#include "unicode.h"
#include "ds.h"
#include "sdl-inc.h"
#include "lib/glcorearb.h"

/// Minimum text size
#define TEXT_SIZE_MIN 6
/// Maximum text size
#define TEXT_SIZE_MAX 70
/// max number of LSPs running at once
#define TED_LSP_MAX 200
/// max number of macros
#define TED_MACRO_MAX 256

/// Version string
#define TED_VERSION_FULL "ted v. " TED_VERSION

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
	char title[32];
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
typedef struct FindResult FindResult;

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


/// "document link" information (LSP)
typedef struct DocumentLinks DocumentLinks;

/// information for symbol rename (LSP)
typedef struct RenameSymbol RenameSymbol;

/// "hover" information from LSP server
typedef struct Hover Hover;

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
typedef struct Highlights Highlights;

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
	MenuInfo *all_menus;
	/// index of currently open menu, or 0 if no menu is open
	u32 menu_open_idx;
	void *menu_context;
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
	DocumentLinks *document_links;
	Hover *hover;
	Definitions definitions;
	Highlights *highlights;
	Usages *usages;
	RenameSymbol *rename_symbol;
	
	FILE *log;
	
	/// dynamic array of build errors
	BuildError *build_errors;
	/// build error we are currently "on"
	u32 build_error;
	
	/// used by menus to keep track of the scroll position so we can return to it.
	dvec2 prev_active_buffer_scroll;
	
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

// === buffer.c ===
/// create a new empty buffer with no file name
void buffer_create(TextBuffer *buffer, Ted *ted);
/// create a new empty line buffer
void line_buffer_create(TextBuffer *buffer, Ted *ted);
/// Does this buffer have an error?
bool buffer_has_error(TextBuffer *buffer);
/// get buffer error
const char *buffer_get_error(TextBuffer *buffer);
/// clear buffer error
void buffer_clear_error(TextBuffer *buffer);
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
/// highlight an \ref LSPRange in this buffer.
///
/// make sure to call \ref gl_geometry_draw after this
void buffer_highlight_lsp_range(TextBuffer *buffer, LSPRange range, ColorSetting color);
/// process a mouse click.
/// returns true if the event was consumed.
bool buffer_handle_click(Ted *ted, TextBuffer *buffer, vec2 click, u8 times);

// === build.c ===
void build_frame(Ted *ted, float x1, float y1, float x2, float y2);

// === colors.c ====
void color_init(void);
/// which color setting should be used for the given symbol kind.
/// this is the color used in the autocomplete selector, for example.
ColorSetting color_for_symbol_kind(SymbolKind kind);

// === command.c ===
void command_init(void);
void command_execute_ex(Ted *ted, Command c, const CommandArgument *argument, const CommandContext *context);

// === config.c ===
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

// === find.c ===
/// height of the find/find+replace menu in pixels
float find_menu_height(Ted *ted);
void find_menu_frame(Ted *ted, Rect menu_bounds);

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

// === ide-autocomplete.c ===
void autocomplete_init(Ted *ted);
void autocomplete_quit(Ted *ted);
void autocomplete_frame(Ted *ted);
void autocomplete_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-definitions.c ===
void definitions_init(Ted *ted);
/// go to the definition of `name`.
/// if `lsp` is NULL, tags will be used.
/// Note: the document position is required for LSP requests because of overloading (where the name
/// alone isn't sufficient)
void definition_goto(Ted *ted, LSP *lsp, const char *name, LSPDocumentPosition pos, GotoType type);
void definitions_process_lsp_response(Ted *ted, LSP *lsp, const LSPResponse *response);
void definitions_frame(Ted *ted);

// === ide-document-link.c ===
void document_link_init(Ted *ted);
void document_link_quit(Ted *ted);
void document_link_frame(Ted *ted);
void document_link_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-highlights.c ===
void highlights_init(Ted *ted);
void highlights_quit(Ted *ted);
void highlights_frame(Ted *ted);
void highlights_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-hover.c ===
void hover_init(Ted *ted);
void hover_frame(Ted *ted, double dt);
void hover_process_lsp_response(Ted *ted, const LSPResponse *response);
void hover_quit(Ted *ted);

// === ide-rename-symbol.c ===
void rename_symbol_init(Ted *ted);
void rename_symbol_quit(Ted *ted);
void rename_symbol_frame(Ted *ted);
void rename_symbol_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-signature-help.c ===
void signature_help_init(Ted *ted);
void signature_help_quit(Ted *ted);
void signature_help_frame(Ted *ted);
void signature_help_process_lsp_response(Ted *ted, const LSPResponse *response);

// === ide-usages.c ===
void usages_init(Ted *ted);
void usages_process_lsp_response(Ted *ted, const LSPResponse *response);
void usages_frame(Ted *ted);
void usages_quit(Ted *ted);

// === macro.c ===
void macro_add(Ted *ted, Command command, const CommandArgument *argument);
void macros_free(Ted *ted);

// === menu.c ===
void menu_init(Ted *ted);
void menu_quit(Ted *ted);
void menu_update(Ted *ted);
void menu_render(Ted *ted);
/// move to next/previous command
void menu_shell_move(Ted *ted, int direction);
/// move to previous command
void menu_shell_up(Ted *ted);
/// move to next command
void menu_shell_down(Ted *ted);
Rect selection_menu_render_bg(Ted *ted);

// === node.c ===
void node_free(Node *node);
void node_frame(Ted *ted, Node *node, Rect r);

// === syntax.c ===
/// free up resources used by `syntax.c`
void syntax_quit(void);

// === tags.c ===
/// get all tags in the tags file as SymbolInfos.
SymbolInfo *tags_get_symbols(Ted *ted);

// === ted.c ===
/// Get LSP by ID. Returns NULL if there is no LSP with that ID.
LSP *ted_get_lsp_by_id(Ted *ted, LSPID id);
/// go to this LSP document position, opening a new buffer containing the file if necessary.
void ted_go_to_lsp_document_position(Ted *ted, LSP *lsp, LSPDocumentPosition position);
/// cancel this LSP request. also zeroes *request
/// if *request is zeroed, this does nothing.
void ted_cancel_lsp_request(Ted *ted, LSPServerRequestID *request);
/// convert LSPWindowMessageType to MessageType
MessageType ted_message_type_from_lsp(LSPWindowMessageType type);
/// Returns the index of an available node, or -1 if none are available 
i32 ted_new_node(Ted *ted);
/// check for orphaned nodes and node cycles
void ted_check_for_node_problems(Ted *ted);
/// load ted configuration
void ted_load_configs(Ted *ted);
/// get colors to use for message box
void ted_color_settings_for_message_type(MessageType type, ColorSetting *bg_color, ColorSetting *border_color);

#endif // TED_INTERNAL_H_
