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

#if PROFILE
#define PROFILE_TIME(var) double var = time_get_seconds();
#else
/// get current time for profiling
#define PROFILE_TIME(var)
#endif


/// Minimum text size
#define TEXT_SIZE_MIN 6
/// Maximum text size
#define TEXT_SIZE_MAX 70
/// max number of LSPs running at once
#define TED_LSP_MAX 200
/// max number of macros
#define TED_MACRO_MAX 256
/// max number of nodes
#define TED_NODE_MAX 256
/// max number of buffers
#define TED_BUFFER_MAX 1024

/// Version string
#define TED_VERSION_FULL "ted v. " TED_VERSION

typedef struct {
	// if `string == NULL`, this is an integer argument
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

/// Reference-counted texture
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
	u32 colors[COLOR_COUNT];
	float cursor_blink_time_on, cursor_blink_time_off;
	float hover_time;
	float ctrl_scroll_adjust_text_size;
	float lsp_delay;
	u32 max_file_size;
	u32 max_file_size_view_only;
	u16 framerate_cap;
	u16 text_size_no_dpi;
	u16 text_size;
	u16 max_menu_width;
	u16 error_display_time;
	u16 lsp_port;
	bool auto_indent;
	bool auto_add_newline;
	bool remove_trailing_whitespace;
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
	bool show_diagnostics;
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
	RcStr *comment_start;
	/// string used to end comments
	RcStr *comment_end;
	/// Comma-separated list of file names which identify the project root
	RcStr *root_identifiers;
	/// LSP server command
	RcStr *lsp;
	/// LSP "configuration" JSON
	RcStr *lsp_configuration;
	/// Build command. If non-empty, this overrides running `cargo build` if `Cargo.toml` exists, etc.
	RcStr *build_command;
	/// Default build command for if `Cargo.toml`, `Makefile`, etc. do not exist.
	RcStr *build_default_command;
	/// Comma separated list of paths to font files.
	RcStr *font;
	/// Comma separated list of paths to bold font files.
	RcStr *font_bold;
	LanguageExtension *language_extensions;
	/// dynamic array, sorted by KEY_COMBO(modifier, key)
	KeyAction *key_actions;
};

typedef enum {
	CONFIG_TED_CFG = 1,
	CONFIG_EDITORCONFIG = 2,
} ConfigFormat;

typedef struct {
	/// path to config file
	RcStr *source;
	/// format of config file
	ConfigFormat format;
	/// is this from a root .editorconfig file?
	///
	/// (if so, we don't want to apply editorconfigs in higher-up directories)
	bool is_editorconfig_root;
	/// language this config applies to
	Language language;
	/// path regex this config applies to
	struct pcre2_real_code_8 *path;
	/// path regex string
	char *path_regex;
	/// settings which this config specifies
	Settings settings;
	/// which bytes of settings are actually set
	bool settings_set[sizeof (Settings)];
} Config;

typedef struct EditNotifyInfo {
	EditNotify fn;
	void *context;
	EditNotifyID id;
} EditNotifyInfo;

/// max tabs per node
#define TED_MAX_TABS 100

/// "find" menu result
typedef struct FindResult FindResult;

typedef struct BuildError BuildError;

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

/// data needed for formatting code
typedef struct Formatting Formatting;

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

typedef struct Definitions Definitions;

/// "highlight" information from LSP server
typedef struct Highlights Highlights;

typedef struct Macro Macro;

typedef struct LoadedFont LoadedFont;

typedef struct {
	vec2 pos;
	u8 times;
} MouseClick;

typedef struct {
	vec2 pos;
} MouseRelease;

typedef TextBuffer *TextBufferPtr;
typedef Node *NodePtr;

struct Ted {
	/// all running LSP servers
	LSP *lsps[TED_LSP_MAX + 1];
	/// current time (see time_get_seconds), as of the start of this frame
	double frame_time;
	/// current time as a human readable string (used for logs)
	char frame_time_string[64];
	
	Macro *macros;
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
	Config *all_configs;
	/// cwd where \ref default_settings was computed
	char default_settings_cwd[TED_PATH_MAX];
	/// settings to use when no buffer is open
	Settings default_settings;
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
	FileSelector *file_selector;
	Selector *command_selector;
	/// general-purpose line buffer for inputs -- used for menus
	TextBuffer *line_buffer;
	/// use for "find" term in find/find+replace
	TextBuffer *find_buffer;
	/// "replace" for find+replace
	TextBuffer *replace_buffer;
	/// buffer for build output (view only)
	TextBuffer *build_buffer;
	/// used for command selector
	TextBuffer *argument_buffer;
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
	Definitions *definitions;
	Highlights *highlights;
	Usages *usages;
	RenameSymbol *rename_symbol;
	Formatting *formatting;
	/// process ID
	int pid;
	
	FILE *log;
	
	/// dynamic array of build errors
	BuildError *build_errors;
	/// build error we are currently "on"
	u32 build_error;
	
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
	/// has the shell command been modified (if so, we block up/down)
	bool shell_command_modified;

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
	/// `nodes[0]` is always the "root node", if any buffers are open.
	Node **nodes;
	TextBuffer **buffers;
	char window_title[256];
	
	/// little box used to display errors and info.
	char message[512];
	/// time message box was opened
	double message_time;
	MessageType message_type;
	MessageType message_shown_type;
	char message_shown[512];
	
	
	u64 edit_notify_id;
	EditNotifyInfo *edit_notifys;
};

// === buffer.c ===
/// create a new empty buffer with no file name
TextBuffer *buffer_new(Ted *ted);
/// create a new empty line buffer
TextBuffer *line_buffer_new(Ted *ted);
/// free all resources used by the buffer and the pointer `buffer` itself
void buffer_free(TextBuffer *buffer);
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
/// Get the current selection as an LSPRange.
///
/// Returns `(LSPRange){0}` if nothing is selected.
LSPRange buffer_selection_as_lsp_range(TextBuffer *buffer);
/// indicate that config has been reloaded so we need to recompute buffer settings
void buffer_recompute_settings(TextBuffer *buffer);
/// Apply LSP TextEdit[] from response
void buffer_apply_lsp_text_edits(TextBuffer *buffer, const LSPResponse *response, const LSPTextEdit *edits, size_t n_edits);
/// Get the cursor position as an LSPDocumentPosition.
LSPDocumentPosition buffer_cursor_pos_as_lsp_document_position(TextBuffer *buffer);
/// highlight an \ref LSPRange in this buffer.
///
/// make sure to call \ref gl_geometry_draw after this
void buffer_highlight_lsp_range(TextBuffer *buffer, LSPRange range, ColorSetting color);
/// process a mouse click.
/// returns true if the event was consumed.
bool buffer_handle_click(Ted *ted, TextBuffer *buffer, vec2 click, u8 times);
/// next frame, scroll so that the cursor is in the center of the buffer's rectangle.
///
/// currently needed to avoid bad positioning when a buffer is created
/// and buffer_center_cursor is called immediately after
void buffer_center_cursor_next_frame(TextBuffer *buffer);
/// perform a series of checks to make sure the buffer doesn't have any invalid values
void buffer_check_valid(TextBuffer *buffer);
void buffer_publish_diagnostics(TextBuffer *buffer, const LSPRequest *request, LSPDiagnostic *diagnostics);

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
/// read a config file.
///
/// returns true on success. if the file at `path` does not exist, this returns false but doesn't show an error message.
///
/// if the config with this path has already been read, this does nothing.
bool config_read(Ted *ted, const char *path, ConfigFormat format);
void config_free_all(Ted *ted);
void config_merge_into(Settings *dest, const Config *src_cfg);
/// call this after all your calls to \ref config_merge_into
///
/// (this sorts key actions, etc.)
void settings_finalize(Ted *ted, Settings *settings);
bool config_applies_to(Config *cfg, const char *path, Language language);
/// higher-priority configs override lower-priority ones.
i32 config_priority(const Config *cfg);
void settings_free(Settings *settings);

// === find.c ===
void find_init(Ted *ted);
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
void definitions_quit(Ted *ted);

// === ide-document-link.c ===
void document_link_init(Ted *ted);
void document_link_quit(Ted *ted);
void document_link_frame(Ted *ted);
void document_link_process_lsp_response(Ted *ted, const LSPResponse *response);


// === ide-format.c ===
/// initialize formatting stuff
void format_init(Ted *ted);
void format_process_lsp_response(Ted *ted, const LSPResponse *response);
/// cancel last formatting request
void format_cancel_request(Ted *ted);
/// free formatting stuff
void format_quit(Ted *ted);

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
void macros_init(Ted *ted);
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
Node *node_new(Ted *ted);
void node_free(Node *node);
/// don't call this if `buffer` is in any other nodes!
///
/// \returns false if there are too many tabs
Status node_add_tab(Ted *ted, Node *node, TextBuffer *buffer);
/// cannot be called if `node` has already been initialized or contains tabs.
void node_init_split(Node *node, Node *child1, Node *child2, float split_pos, bool is_vertical);
void node_frame(Ted *ted, Node *node, Rect r);

// === syntax.c ===
/// register built-in languages, etc.
void syntax_init(void);
/// free up resources used by `syntax.c`
void syntax_quit(void);

// === tags.c ===
/// get all tags in the tags file as SymbolInfos.
SymbolInfo *tags_get_symbols(Ted *ted);

// === ted.c ===
/// update `ted->frame_time`
void ted_update_time(Ted *ted);
/// set ted's active buffer to something nice
void ted_reset_active_buffer(Ted *ted);
/// set ted's error message to the buffer's error.
void ted_error_from_buffer(Ted *ted, TextBuffer *buffer);
float ted_get_ui_scaling(Ted *ted);
/// Get LSP by ID. Returns NULL if there is no LSP with that ID.
LSP *ted_get_lsp_by_id(Ted *ted, LSPID id);
/// go to this LSP document position, opening a new buffer containing the file if necessary.
void ted_go_to_lsp_document_position(Ted *ted, LSP *lsp, LSPDocumentPosition position);
/// cancel this LSP request. also zeroes *request
/// if *request is zeroed, this does nothing.
void ted_cancel_lsp_request(Ted *ted, LSPServerRequestID *request);
/// convert LSPWindowMessageType to MessageType
MessageType ted_message_type_from_lsp(LSPWindowMessageType type);
/// delete buffer - does NOT remove it from the node tree
void ted_delete_buffer(Ted *ted, TextBuffer *buffer);
/// Returns a new buffer, or NULL on out of memory
TextBuffer *ted_new_buffer(Ted *ted);
/// Compute the settings for a file at the given path in the given language.
///
/// NOTE: this frees the previous settings stored in `*settings`. so make sure it's either zeroed or points to valid settings.
void ted_compute_settings(Ted *ted, const char *path, Language language, Settings *settings);
/// check for orphaned nodes and node cycles
void ted_check_for_node_problems(Ted *ted);
/// load ted configuration
void ted_load_configs(Ted *ted);
/// get colors to use for message box
void ted_color_settings_for_message_type(MessageType type, ColorSetting *bg_color, ColorSetting *border_color);
/// Load all the fonts ted will use, freeing any previous ones.
void ted_load_fonts(Ted *ted);
/// Free all of ted's fonts.
void ted_free_fonts(Ted *ted);
/// process textDocument/publishDiagnostics request
void ted_process_publish_diagnostics(Ted *ted, LSP *lsp, LSPRequest *request);

#endif // TED_INTERNAL_H_
