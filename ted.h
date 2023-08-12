/// \file
/// the main header file for ted.
///
/// this contains almost all of the function declarations.


/// \mainpage ted doxygen documentation
/// 
/// See "files" above. You probably want to look at \ref ted.h.

#ifndef TED_H_
#define TED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#include "util.h"
#include "text.h"
#include "colors.h"
#include "command.h"

/// Version number
#define TED_VERSION "2.4.3"
/// Maximum path size ted handles.
#define TED_PATH_MAX 1024
/// Config filename
#define TED_CFG "ted.cfg"


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

/// enum used with \ref autocomplete_open
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

/// "Open file"
#define MENU_OPEN "ted-open-file"
/// "Save file as"
#define MENU_SAVE_AS "ted-save-as"
/// "X has unsaved changes"
#define MENU_WARN_UNSAVED "ted-warn-unsaved"
/// "X has been changed by another program"
#define MENU_ASK_RELOAD "ted-ask-reload"
/// "Go to definition of..."
#define MENU_GOTO_DEFINITION "ted-goto-defn"
/// "Go to line"
#define MENU_GOTO_LINE "ted-goto-line"
/// "Command palette"
#define MENU_COMMAND_SELECTOR "ted-cmd-sel"
/// "Run a shell command"
#define MENU_SHELL "ted-shell"
/// "Rename symbol"
#define MENU_RENAME_SYMBOL "ted-rename-sym"

/// Information about a programming language
///
/// Used for dynamic language registration (\ref syntax_register_language).
/// Please zero all the fields of the struct which you aren't using.
///
/// The fields `id` and `name` MUST NOT be 0, or `ted` will reject your language.
typedef struct {
	/// Language ID number. For user-defined languages, this must be `>= LANG_USER_MIN` and `< LANG_USER_MAX`.
	///
	/// To avoid conflict, try picking a unique number.
	Language id;
	/// Unique name for the language
	char name[30];
	/// LSP identifier given by the specification https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
	char lsp_identifier[32];
	/// function used for syntax highlighting
	SyntaxHighlightFunction highlighter;
	/// reserved for future use
	char reserved[128];
} LanguageInfo;

/// information about a menu
///
/// used for dynamic menu registration (see \ref menu_register)
typedef struct {
	/// identifier used to open the menu.
	///
	/// try to pick a unique name.
	char name[24];
	/// if non-NULL this will be called right after the menu is opened
	void (*open)(Ted *ted);
	/// if non-NULL this will be called every frame
	/// before anything is rendered to the screen.
	void (*update)(Ted *ted);
	/// if non-NULL this will be called every frame
	/// after buffers, etc. have been rendered to the screen
	/// (when the menu should be rendered)
	void (*render)(Ted *ted);
	/// if non-NULL this will be called right before the menu is closed.
	/// if it returns false, the menu will not be closed.
	bool (*close)(Ted *ted);
	/// should be zeroed -- reserved for future use.
	char reserved[128];
} MenuInfo;

/// information about an edit provided to \ref EditNotify.
///
/// NOTE: more members may be added in the future (this does not affect backwards compatibility)
typedef struct {
	/// position where the edit took place
	BufferPos pos;
	/// number of characters (unicode codepoints, including newlines) deleted
	///
	/// if this is non-zero, \ref chars_inserted will be zero.
	u32 chars_deleted;
	/// number of characters (unicode codepoints, including newlines) inserted
	///
	/// if this is non-zero, \ref chars_deleted will be zero.
	u32 chars_inserted;
	/// number of newlines deleted
	u32 newlines_deleted;
	/// number of newlines inserted
	u32 newlines_inserted;
} EditInfo;

/// this type of callback is called right after an edit is made to the buffer.
///
/// you will be given the number of characters deleted at the position, the number
/// of characters inserted after the position, and the context pointer
/// which was passed to \ref ted_add_edit_notify.
typedef void (*EditNotify)(void *context, TextBuffer *buffer, const EditInfo *info);

/// ID number of an \ref EditNotify callback.
///
/// Can be used to remove the callback with \ref ted_remove_edit_notify.
typedef u64 EditNotifyID;

// === buffer.c ===
/// Returns `true` if the buffer is in view-only mode.
bool buffer_is_view_only(TextBuffer *buffer);
/// Set whether the buffer should be in view only mode.
void buffer_set_view_only(TextBuffer *buffer, bool view_only);
/// amount scrolled horizontally, in terms of the width of a space character
double buffer_get_scroll_columns(TextBuffer *buffer);
///n number of lines scrolled vertically
double buffer_get_scroll_lines(TextBuffer *buffer);
/// get last time buffer was written to, in the format of \ref time_get_seconds
double buffer_last_write_time(TextBuffer *buffer);
/// get position of the cursor
BufferPos buffer_cursor_pos(TextBuffer *buffer);
/// returns true if anything is selected
bool buffer_has_selection(TextBuffer *buffer);
/// get position of non-cursor end of selection.
///
/// `pos` is allowed to be NULL.
/// returns false if nothing is selected.
bool buffer_selection_pos(TextBuffer *buffer, BufferPos *pos);
/// Get path to buffer's file, or `NULL` if the buffer is unnamed.
///
/// This string can be freed if the buffer is saved under a different name or closed, so don't keep it around for long.
const char *buffer_get_path(TextBuffer *buffer);
/// clear undo and redo history
void buffer_clear_undo_redo(TextBuffer *buffer);
/// set whether undo history should be kept
void buffer_set_undo_enabled(TextBuffer *buffer, bool enabled);
/// set manual language override for buffer.
///
/// passing `language = 0` goes back to automatic language detection.
void buffer_set_manual_language(TextBuffer *buffer, u32 language);
/// first line which will appear on screen
u32 buffer_first_line_on_screen(TextBuffer *buffer);
/// last line which will appear on screen
u32 buffer_last_line_on_screen(TextBuffer *buffer);
/// get rectangle buffer is rendered to
Rect buffer_rect(TextBuffer *buffer);
/// is this buffer empty?
bool buffer_empty(TextBuffer *buffer);
/// returns the buffer's filename (not full path), or "Untitled" if this buffer is untitled.
const char *buffer_display_filename(TextBuffer *buffer);
/// does this buffer contained a named file (i.e. not a line buffer, not the build buffer, not untitled)
bool buffer_is_named_file(TextBuffer *buffer);
/// does this buffer have unsaved changes?
bool buffer_unsaved_changes(TextBuffer *buffer);
/// is this a line buffer?
bool buffer_is_line_buffer(TextBuffer *buffer);
/// has this line buffer been submitted?
///
/// returns false if `buffer` is not a line buffer.
bool line_buffer_is_submitted(TextBuffer *buffer);
/// clear submission status of line buffer.
void line_buffer_clear_submitted(TextBuffer *buffer);
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
struct LSP *buffer_lsp(TextBuffer *buffer);
/// Get the settings used for this buffer.
Settings *buffer_settings(TextBuffer *buffer);
/// Get tab width for this buffer
u8 buffer_tab_width(TextBuffer *buffer);
/// Get whether or not to indent with spaces for this buffer.
bool buffer_indent_with_spaces(TextBuffer *buffer);
/// returns the number of lines in the buffer.
u32 buffer_line_count(TextBuffer *buffer);
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
/// clear contents, undo history, etc. of a buffer
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
/// Put text at a position.
///
/// Returns the position of the end of the text, or (BufferPos){0} on error.
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
/// Start a new "edit chain".
///
/// Undoing once after an edit chain will undo everything in the chain.
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
/// Save, but with a different path.
///
/// `new_path` must be an absolute path.
bool buffer_save_as(TextBuffer *buffer, const char *new_path);
/// index of first line that will be displayed on screen
u32 buffer_first_rendered_line(TextBuffer *buffer);
/// index of last line that will be displayed on screen
u32 buffer_last_rendered_line(TextBuffer *buffer);
/// go to the definition/declaration/etc of the word at the cursor.
void buffer_goto_word_at_cursor(TextBuffer *buffer, GotoType type);
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
/// returns "`p2 - p1`", that is, the number of characters between `p1` and `p2`,
/// but negative if `p1` comes after `p2`.
i64 buffer_pos_diff(TextBuffer *buffer, BufferPos p1, BufferPos p2);

// === build.c ===
/// clear build errors and stop
void build_stop(Ted *ted);
/// call before adding anything to the build queue
void build_queue_start(Ted *ted);
/// add a command to the build queue. call \ref build_queue_start before this.
void build_queue_command(Ted *ted, const char *command);
/// call this after calling \ref build_queue_start, \ref build_queue_command.
/// make sure you set `ted->build_dir` before running this!
void build_queue_finish(Ted *ted);
/// set up the build output buffer.
void build_setup_buffer(Ted *ted);
/// run a single command in the build window.
/// make sure you set `ted->build_dir` before running this!
void build_start_with_command(Ted *ted, const char *command);
/// figure out which build command to run, and run it.
void build_start(Ted *ted);
/// go to next build error
void build_next_error(Ted *ted);
/// go to previous build error
void build_prev_error(Ted *ted);
/// find build errors in build buffer.
void build_check_for_errors(Ted *ted);

// === colors.c ===
/// parse color setting
ColorSetting color_setting_from_str(const char *str);
/// get string corresponding to color setting
const char *color_setting_to_str(ColorSetting s);
Status color_from_str(const char *str, u32 *color);
/// perform alpha blending with `bg` and `fg`.
u32 color_blend(u32 bg, u32 fg);
/// multiply color's alpha value by `opacity`.
u32 color_apply_opacity(u32 color, float opacity);

// === command.c ===
/// parse command
Command command_from_str(const char *str);
/// get string representation of command
const char *command_to_str(Command c);
/// execute command with integer argument
void command_execute(Ted *ted, Command c, i64 argument);
/// execute command with string argument
void command_execute_string_argument(Ted *ted, Command c, const char *string);

// === config.c ===
/// returns the best guess for the root directory of the project containing `path`
/// (which should be an absolute path).
/// the return value should be freed.
char *settings_get_root_dir(Settings *settings, const char *path);

// === find.c ===
/// which buffer will be searched?
TextBuffer *find_search_buffer(Ted *ted);
/// discard find results and perform search again.
void find_redo_search(Ted *ted);
/// replace the match we are currently highlighting, or do nothing if there is no highlighted match
void find_replace(Ted *ted);
/// go to next find result
void find_next(Ted *ted);
/// go to previous find result
void find_prev(Ted *ted);
/// replace all matches
void find_replace_all(Ted *ted);
/// open the find/find+replace menu.
void find_open(Ted *ted, bool replace);
/// close the find/find+replace menu.
void find_close(Ted *ted);

// === gl.c ===
/// create and compile a shader
u32 gl_compile_shader(char error_buf[256], const char *code, u32 shader_type);
/// create new shader program from shaders
u32 gl_link_program(char error_buf[256], u32 *shaders, size_t count);
/// create a shader program from vertex shader and fragment shader source
u32 gl_compile_and_link_shaders(char error_buf[256], const char *vshader_code, const char *fshader_code);
/// get vertex attribute location
///
/// prints a debug message if `attrib` is not found
u32 gl_attrib_location(u32 program, const char *attrib);
/// get shader uniform location
///
/// prints a debug message if `uniform` is not found
i32 gl_uniform_location(u32 program, const char *uniform);
/// queue a filled rectangle with the given color.
void gl_geometry_rect(Rect r, u32 color_rgba);
/// queue the border of a rectangle with the given color.
void gl_geometry_rect_border(Rect r, float border_thickness, u32 color);
/// draw all queued geometry
void gl_geometry_draw(void);
/// create an OpenGL texture object from an image file.
u32 gl_load_texture_from_image(const char *path);

// === ide-autocomplete.c ===
/// is the autocomplete box open?
bool autocomplete_is_open(Ted *ted);
/// is there a phantom completion being displayed?
bool autocomplete_has_phantom(Ted *ted);
/// is this point in the autocomplete box?
bool autocomplete_box_contains_point(Ted *ted, vec2 point);
/// open autocomplete
///
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

// === ide-definitions.c ===
/// cancel the last go-to-definition / find symbols request.
void definition_cancel_lookup(Ted *ted);

// === ide-document-link.c ===
/// get document link at this position in the active buffer.
///
/// this will always return `NULL` if the document link activation key isn't presed.
/// the returned pointer won't be freed immediately, but could be on the next frame,
/// so don't keep it around long.
const char *document_link_at_buffer_pos(Ted *ted, BufferPos pos);

// === ide-highlights.c ===

// === ide-hover.c ===
/// called for example whenever the mouse moves to reset the timer before hover info is displayed
void hover_reset_timer(Ted *ted);

// === ide-rename-symbol.c ===
/// renname symbol at cursor of `buffer` to `new_name`
void rename_symbol_at_cursor(Ted *ted, TextBuffer *buffer, const char *new_name);

// === ide-signature-help.c ===
/// figure out new signature help
void signature_help_retrigger(Ted *ted);
/// open signature help.
///
/// `trigger` should either be the trigger character (e.g. ',')
/// or one of the TRIGGER_* constants.
void signature_help_open(Ted *ted, uint32_t trigger);
/// is the signature help window open?
bool signature_help_is_open(Ted *ted);
/// close the signature help window
void signature_help_close(Ted *ted);

// === ide-usages.c ===
/// cancel the last "find usages" request
void usages_cancel_lookup(Ted *ted);
/// find usages for word under the cursor in the active buffer.
void usages_find(Ted *ted);

// === macro.c ===
/// start recording macro with index
void macro_start_recording(Ted *ted, u32 index);
/// stop recording macro
void macro_stop_recording(Ted *ted);
/// execute macro with index
void macro_execute(Ted *ted, u32 index);

// === menu.c ===
/// register a new menu
void menu_register(Ted *ted, const MenuInfo *infop);
/// close the currently opened menu.
void menu_close(Ted *ted);
/// open menu by name (with `NULL` context pointer).
void menu_open(Ted *ted, const char *menu_name);
/// open menu with context pointer which will be passed to the menu callback.
void menu_open_with_context(Ted *ted, const char *menu_name, void *context);
/// get the `context` value passed to the last \ref menu_open_with_context,
/// or `NULL` if no menu is open.
void *menu_get_context(Ted *ted);
/// is this menu open?
bool menu_is_open(Ted *ted, const char *menu_name);
/// is any menu open?
bool menu_is_any_open(Ted *ted);
/// process a :escape command (by default this happens when the escape key is pressed)
void menu_escape(Ted *ted);
/// get rectangle which menu takes up
Rect menu_rect(Ted *ted);

// === node.c ===
/// go to the `n`th next tab (e.g. `n=1` goes to the next tab)
/// going past the end of the tabs will "wrap around" to the first one.
///
/// if `node` is a split, nothing happens.
void node_tab_next(Ted *ted, Node *node, i32 n);
/// go to the `n`th previous tab (e.g. `n=1` goes to the previous tab)
/// going before the first tab will "wrap around" to the last one.
///
/// if `node` is a split, nothing happens.
void node_tab_prev(Ted *ted, Node *node, i32 n);
/// switch to a specific tab.
///
/// if `tab` is out of range or `node` is a split, nothing happens.
void node_tab_switch(Ted *ted, Node *node, i32 tab);
/// swap the position of two tabs
///
/// if `node` is a split or either index is out of range, nothing happens.
void node_tabs_swap(Node *node, i32 tab1, i32 tab2);
/* TODO
/// get two children of split node.
///
/// if `node` isn't a split, returns false and sets `*child1` and `*child2` to `NULL`.
bool node_children(Node *node, Node **child1, Node **child2);
/// returns true if this node is a vertical split
bool node_split_is_vertical(Node *node);
/// get number of tabs in node
u32 node_tab_count(Ted *ted, Node *node);
/// get buffer in tab at index of node.
///
/// returns `NULL` if `tab` is out of range.
TextBuffer *node_tab_get(Ted *ted, Node *node, u32 tab);
*/
/// returns parent node, or NULL if this is the root node.
Node *node_parent(Ted *ted, Node *node);
/// join this node with its sibling
void node_join(Ted *ted, Node *node);
/// close a node, WITHOUT checking for unsaved changes
///
/// does nothing if `node` is `NULL`.
void node_close(Ted *ted, Node *node);
/// close tab, WITHOUT checking for unsaved changes!
/// returns true if the node is still open
///
/// does nothing and returns false if node_idx is out of range
bool node_tab_close(Ted *ted, Node *node, i32 index);
/// make a split
void node_split(Ted *ted, Node *node, bool vertical);
/// switch to the other side of the current split.
void node_split_switch(Ted *ted);
/// swap the two sides of the current split.
void node_split_swap(Ted *ted);

// === session.c ===
/// store ted session
void session_write(Ted *ted);
/// load ted session
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
/// returns ')' for '(', '[' for ']', etc., or 0 if c is not a bracket
char32_t syntax_matching_bracket(Language lang, char32_t c);
/// returns true for opening brackets, false for closing brackets/non-brackets
bool syntax_is_opening_bracket(Language lang, char32_t c);
/// This is the main syntax highlighting function. It will determine which colors to use for each character.
/// Rather than returning colors, it returns a character type (e.g. comment) which can be converted to a color.
/// To highlight multiple lines, start out with a zeroed SyntaxState, and pass a pointer to it each time.
/// You can set char_types to NULL if you just want to advance the state, and don't care about the character types.
void syntax_highlight(SyntaxState *state, Language lang, const char32_t *line, u32 line_len, SyntaxCharType *char_types);

// === tags.c ===
void tags_generate(Ted *ted, bool run_in_build_window);
/// find all tags beginning with the given prefix, returning them into `*out`, writing at most out_size entries.
/// you may pass NULL for `out`, in which case just the number of matching tags is returned
/// (still maxing out at `out_size`).
/// each element in `out` should be freed when you're done with them.
size_t tags_beginning_with(Ted *ted, const char *prefix, char **out, size_t out_size, bool error_if_tags_does_not_exist);
/// go to the definition of the given tag
bool tag_goto(Ted *ted, const char *tag);

// === ted.c ===
/// for fatal errors
void die(PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
/// returns the current active buffer, or NULL if no buffer is active.
TextBuffer *ted_get_active_buffer(Ted *ted);
/// if a menu is open, returns the buffer that was open before the menu was opened.
///
/// returns NULL if no menu is open or no buffer was open before the menu was opened.
TextBuffer *ted_get_active_buffer_behind_menu(Ted *ted);
/// get width of ted window
float ted_window_width(Ted *ted);
/// get height of ted window
float ted_window_height(Ted *ted);
/// set title of ted window
void ted_set_window_title(Ted *ted, const char *title);
/// returns `true` if the given SDL key code is down
bool ted_is_key_down(Ted *ted, i32 key);
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
/// get width of menu (e.g. "open file" menu) in pixels
float ted_get_menu_width(Ted *ted);
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
/// Returns the buffer containing the file at absolute path `path`, or NULL if there is none.
TextBuffer *ted_get_buffer_with_file(Ted *ted, const char *path);
/// close this buffer, discarding unsaved changes.
void ted_close_buffer(Ted *ted, TextBuffer *buffer);
/// close buffer with this absolute path, discarding unsaved changes.
///
/// returns true if the buffer was actually present.
bool ted_close_buffer_with_file(Ted *ted, const char *path);
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
/// Get LSP which should be used for the given path and language.
/// If no running LSP server would cover the path and language, a new one is
/// started if possible.
/// Returns NULL on failure (e.g. there is no LSP server
/// specified for the given path and language).
struct LSP *ted_get_lsp(Ted *ted, const char *path, Language language);
/// Get the LSP of the active buffer/ted->cwd.
/// Returns NULL if there is no such server.
struct LSP *ted_active_lsp(Ted *ted);
/// get the value of the given color setting, according to `ted_active_settings(ted)`.
u32 ted_active_color(Ted *ted, ColorSetting color);
/// open the given file, or switch to it if it's already open.
/// returns true on success.
bool ted_open_file(Ted *ted, const char *filename);
/// create a new buffer for the file `filename`, or open it if it's already open.
/// if `filename` is NULL, this creates an untitled buffer.
/// returns true on success.
bool ted_new_file(Ted *ted, const char *filename);
/// save all changes to all buffers with unsaved changes.
bool ted_save_all(Ted *ted);
/// sets the active buffer to this buffer, and updates active_node, etc. accordingly
/// you can pass NULL to buffer to make it so no buffer is active.
void ted_switch_to_buffer(Ted *ted, TextBuffer *buffer);
/// switch to this node
void ted_node_switch(Ted *ted, Node *node);
/// reload ted configuration
void ted_reload_configs(Ted *ted);
/// handle a key press
void ted_press_key(Ted *ted, i32 keycode, u32 modifier);
/// get the buffer and buffer position where the mouse is.
/// returns false if the mouse is not in a buffer.
Status ted_get_mouse_buffer_pos(Ted *ted, TextBuffer **pbuffer, BufferPos *ppos);
/// make the cursor red for a bit to indicate an error (e.g. no autocompletions)
void ted_flash_error_cursor(Ted *ted);
/// how tall is a line buffer?
float ted_line_buffer_height(Ted *ted);
/// add a \ref EditNotify callback which will be called whenever `buffer` is edited.
///
/// returns an ID which can be used with \ref ted_remove_edit_notify
EditNotifyID ted_add_edit_notify(Ted *ted, EditNotify notify, void *context);
/// remove edit notify callback. if `id` is zero or invalid, no action is taken.
void ted_remove_edit_notify(Ted *ted, EditNotifyID id);

// === ui.c ===
/// move selector cursor up by `n` entries
void selector_up(Ted *ted, Selector *s, i64 n);
/// move selector cursor down by `n` entries
void selector_down(Ted *ted, Selector *s, i64 n);
/// sort entries alphabetically
void selector_sort_entries_by_name(Selector *s);
/// returns a null-terminated UTF-8 string of the entry selected, or NULL if none was.
///
/// also, sel->cursor will be set to the index of the entry, even if the mouse was used.
/// you should call free() on the return value.
char *selector_update(Ted *ted, Selector *s);
/// render selector
///
/// NOTE: also renders the line buffer
void selector_render(Ted *ted, Selector *s);
/// free resources uesd by file selecor
void file_selector_free(FileSelector *fs);
/// returns the name of the selected file, or NULL if none was selected.
///
/// the returned pointer should be freed.
char *file_selector_update(Ted *ted, FileSelector *fs);
/// render file selector
void file_selector_render(Ted *ted, FileSelector *fs);
/// get a good size of button for this text
vec2 button_get_size(Ted *ted, const char *text);
/// render button
void button_render(Ted *ted, Rect button, const char *text, u32 color);
/// returns true if the button was clicked on.
bool button_update(Ted *ted, Rect button);
/// returns selected option, or POPUP_NONE if none was selected
PopupOption popup_update(Ted *ted, u32 options);
/// render popup menu.
///
/// `options` should be a bitwise or of the POPUP_* constants.
void popup_render(Ted *ted, u32 options, const char *title, const char *body);
/// update and render checkbox
vec2 checkbox_frame(Ted *ted, bool *value, const char *label, vec2 pos);


#ifdef __cplusplus
} // extern "C"
#endif

#endif
