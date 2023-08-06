// Text buffers - These store the contents of a file.
// NOTE: All text editing should be done through the two functions
// buffer_insert_text_at_pos and buffer_delete_chars_at_pos

#include "ted-internal.h"

#include <sys/stat.h>

#define BUFFER_UNTITLED "Untitled" // what to call untitled buffers

struct Line {
	SyntaxState syntax;
	u32 len;
	char32_t *str;
};

// This refers to replacing prev_len characters (found in prev_text) at pos with new_len characters
struct BufferEdit {
	bool chain; // should this + the next edit be treated as one?
	BufferPos pos;
	u32 new_len;
	u32 prev_len;
	char32_t *prev_text;
	double time; // time at start of edit (i.e. the time just before the edit), in seconds since epoch
};

// this is a macro so we get -Wformat warnings
#define buffer_error(buffer, ...) \
	snprintf(buffer->error, sizeof buffer->error - 1, __VA_ARGS__)

bool buffer_has_error(TextBuffer *buffer) {
	return buffer->error[0] != '\0';
}

// returns the buffer's last error
const char *buffer_get_error(TextBuffer *buffer) {
	return buffer->error;
}

void buffer_clear_error(TextBuffer *buffer) {
	*buffer->error = '\0';
}

// set the buffer's error to indicate that we're out of memory
static void buffer_out_of_mem(TextBuffer *buffer) {
	buffer_error(buffer, "Out of memory.");
}


static void buffer_edit_free(BufferEdit *edit) {
	free(edit->prev_text);
}

static void buffer_clear_redo_history(TextBuffer *buffer) {
	arr_foreach_ptr(buffer->redo_history, BufferEdit, edit) {
		buffer_edit_free(edit);
	}
	arr_clear(buffer->redo_history);
	// if the write pos is in the redo history,
	if (buffer->undo_history_write_pos > arr_len(buffer->undo_history))
		buffer->undo_history_write_pos = U32_MAX; // get rid of it
}

static void buffer_clear_undo_history(TextBuffer *buffer) {
	arr_foreach_ptr(buffer->undo_history, BufferEdit, edit) {
		buffer_edit_free(edit);
	}
	arr_clear(buffer->undo_history);
	buffer->undo_history_write_pos = U32_MAX;
}

void buffer_clear_undo_redo(TextBuffer *buffer) {
	buffer_clear_undo_history(buffer);
	buffer_clear_redo_history(buffer);
}

bool buffer_empty(TextBuffer *buffer) {
	return buffer->nlines == 1 && buffer->lines[0].len == 0;
}

bool buffer_is_named_file(TextBuffer *buffer) {
	return buffer->path != NULL;
}


bool buffer_is_view_only(TextBuffer *buffer) {
	return buffer->view_only;
}

void buffer_set_view_only(TextBuffer *buffer, bool view_only) {
	buffer->view_only = view_only;
}

const char *buffer_get_path(TextBuffer *buffer) {
	return buffer->path;
}

const char *buffer_display_filename(TextBuffer *buffer) {
	return buffer->path ? path_filename(buffer->path) : BUFFER_UNTITLED;
}

// add this edit to the undo history
static void buffer_append_edit(TextBuffer *buffer, BufferEdit const *edit) {
	// whenever an edit is made, clear the redo history
	buffer_clear_redo_history(buffer);
	
	arr_add(buffer->undo_history, *edit);
	if (!buffer->undo_history) buffer_out_of_mem(buffer);
}

// add this edit to the redo history
static void buffer_append_redo(TextBuffer *buffer, BufferEdit const *edit) {
	arr_add(buffer->redo_history, *edit);
	if (!buffer->redo_history) buffer_out_of_mem(buffer);
}

static void *buffer_malloc(TextBuffer *buffer, size_t size) {
	void *ret = malloc(size);
	if (!ret) buffer_out_of_mem(buffer);
	return ret;
}

static void *buffer_calloc(TextBuffer *buffer, size_t n, size_t size) {
	void *ret = calloc(n, size);
	if (!ret) buffer_out_of_mem(buffer);
	return ret;
}

static void *buffer_realloc(TextBuffer *buffer, void *p, size_t new_size) {
	void *ret = realloc(p, new_size);
	if (!ret) buffer_out_of_mem(buffer);
	return ret;
}

static char *buffer_strdup(TextBuffer *buffer, const char *src) {
	char *dup = str_dup(src);
	if (!dup) buffer_out_of_mem(buffer);
	return dup;
}

void buffer_create(TextBuffer *buffer, Ted *ted) {
	memset(buffer, 0, sizeof *buffer);
	buffer->store_undo_events = true;
	buffer->ted = ted;
	buffer->settings_idx = -1;
}

void line_buffer_create(TextBuffer *buffer, Ted *ted) {
	buffer_create(buffer, ted);
	buffer->is_line_buffer = true;
	if ((buffer->lines = buffer_calloc(buffer, 1, sizeof *buffer->lines))) {
		buffer->nlines = 1;
		buffer->lines_capacity = 1;
	}
}

void buffer_pos_validate(TextBuffer *buffer, BufferPos *p) {
	if (p->line >= buffer->nlines)
		p->line = buffer->nlines - 1;
	u32 line_len = buffer->lines[p->line].len;
	if (p->index > line_len)
		p->index = line_len;
}

// validate the cursor and selection positions
static void buffer_validate_cursor(TextBuffer *buffer) {
	buffer_pos_validate(buffer, &buffer->cursor_pos);
	if (buffer->selection)
		buffer_pos_validate(buffer, &buffer->selection_pos);
}

// ensure *line points to a line in buffer.
static void buffer_validate_line(TextBuffer *buffer, u32 *line) {
	if (*line >= buffer->nlines)
		*line = buffer->nlines - 1;
}

// update *pos, given that nchars characters were deleted at del_pos.
static void buffer_pos_handle_deleted_chars(BufferPos *pos, BufferPos del_pos, u32 nchars) {
	if (pos->line != del_pos.line) return;
	
	if (pos->index >= del_pos.index + nchars) {
		pos->index -= nchars;
	} else if (pos->index >= del_pos.index) {
		pos->index = del_pos.index;
	}
}
static void buffer_pos_handle_inserted_chars(BufferPos *pos, BufferPos ins_pos, u32 nchars) {
	if (pos->line != ins_pos.line) return;
	
	if (pos->index >= ins_pos.index) {
		pos->index += nchars;
	}
}

bool buffer_pos_valid(TextBuffer *buffer, BufferPos p) {
	return p.line < buffer->nlines && p.index <= buffer->lines[p.line].len;
}

// are there any unsaved changes?
bool buffer_unsaved_changes(TextBuffer *buffer) {
	if (!buffer->path && buffer_empty(buffer))
		return false; // don't worry about empty untitled buffers
	return arr_len(buffer->undo_history) != buffer->undo_history_write_pos;
}

char32_t buffer_char_at_pos(TextBuffer *buffer, BufferPos pos) {
	if (!buffer_pos_valid(buffer, pos))
		return 0;
	Line *line = &buffer->lines[pos.line];
	if (pos.index >= line->len)
		return 0;
	return line->str[pos.index];
}

char32_t buffer_char_before_pos(TextBuffer *buffer, BufferPos pos) {
	if (!buffer_pos_valid(buffer, pos))
		return 0;
	if (pos.index == 0) return 0;
	return buffer->lines[pos.line].str[pos.index - 1];
}

char32_t buffer_char_before_cursor(TextBuffer *buffer) {
	return buffer_char_before_pos(buffer, buffer->cursor_pos);
}

char32_t buffer_char_at_cursor(TextBuffer *buffer) {
	return buffer_char_at_pos(buffer, buffer->cursor_pos);
}

BufferPos buffer_pos_start_of_file(TextBuffer *buffer) {
	(void)buffer;
	return (BufferPos){.line = 0, .index = 0};
}

BufferPos buffer_pos_end_of_file(TextBuffer *buffer) {
	return (BufferPos){.line = buffer->nlines - 1, .index = buffer->lines[buffer->nlines-1].len};
}

Font *buffer_font(TextBuffer *buffer) {
	return buffer->ted->font;
}

// what programming language is this?
Language buffer_language(TextBuffer *buffer) {
	if (!buffer->path)
		return LANG_TEXT;
	
	if (buffer->manual_language != LANG_NONE)
		return (Language)buffer->manual_language;
	const Settings *settings = buffer->ted->default_settings; // important we don't use buffer_settings here since that would cause infinite recursion!
	const char *filename = path_filename(buffer->path);

	int match_score = 0;
	Language match = LANG_TEXT;
	arr_foreach_ptr(settings->language_extensions, LanguageExtension, ext) {
		if (str_has_suffix(filename, ext->extension)) {
			int score = (int)strlen(ext->extension);
			if (score > match_score) {
				// found a better match!
				match_score = score;
				match = ext->language;
			}
		}
	}
	
	return match;
}

// set path = NULL to default to buffer->path
static void buffer_send_lsp_did_close(TextBuffer *buffer, LSP *lsp, const char *path) {
	if (path && !path_is_absolute(path)) {
		assert(0);
		return;
	}
	LSPRequest did_close = {.type = LSP_REQUEST_DID_CLOSE};
	did_close.data.close = (LSPRequestDidClose){
		.document = lsp_document_id(lsp, path ? path : buffer->path)
	};
	lsp_send_request(lsp, &did_close);
	buffer->lsp_opened_in = 0;
}

// buffer_contents must either be NULL or allocated with malloc or similar
//   - don't free it after calling this function.
// if buffer_contents = NULL, fetches the current buffer contents.
static void buffer_send_lsp_did_open(TextBuffer *buffer, LSP *lsp, char *buffer_contents) {
	if (!buffer_contents)
		buffer_contents = buffer_contents_utf8_alloc(buffer);
	LSPRequest request = {.type = LSP_REQUEST_DID_OPEN};
	LSPRequestDidOpen *open = &request.data.open;
	open->file_contents = buffer_contents;
	open->document = lsp_document_id(lsp, buffer->path);
	open->language = buffer_language(buffer);
	lsp_send_request(lsp, &request);
	buffer->lsp_opened_in = lsp->id;
}

LSP *buffer_lsp(TextBuffer *buffer) {
	if (!buffer)
		return NULL;
	if (!buffer_is_named_file(buffer))
		return NULL;
	if (buffer->view_only)
		return NULL; // we don't really want to start up an LSP in /usr/include
	if (buffer->ted->frame_time - buffer->last_lsp_check < 1.0) {
		return ted_get_lsp_by_id(buffer->ted, buffer->lsp_opened_in);
	}
	
	LSP *true_lsp = ted_get_lsp(buffer->ted, buffer->path, buffer_language(buffer));
	LSP *curr_lsp = ted_get_lsp_by_id(buffer->ted, buffer->lsp_opened_in);
	if (true_lsp != curr_lsp) {
		if (curr_lsp)
			buffer_send_lsp_did_close(buffer, curr_lsp, NULL);
		if (true_lsp)
			buffer_send_lsp_did_open(buffer, true_lsp, NULL);
	}
	buffer->last_lsp_check = buffer->ted->frame_time;
	return true_lsp;
}



Settings *buffer_settings(TextBuffer *buffer) {
	Ted *ted = buffer->ted;
	if (buffer->settings_idx >= 0 && buffer->settings_idx < (i32)arr_len(ted->all_settings))
		return &ted->all_settings[buffer->settings_idx];
	
	Settings *settings = ted_get_settings(ted, buffer->path, buffer_language(buffer));
	buffer->settings_idx = (i32)(settings - ted->all_settings);
	assert(buffer->settings_idx >= 0 && buffer->settings_idx < (i32)arr_len(ted->all_settings));
	return settings;
}

u8 buffer_tab_width(TextBuffer *buffer) {
	return buffer_settings(buffer)->tab_width;
}

bool buffer_indent_with_spaces(TextBuffer *buffer) {
	return buffer_settings(buffer)->indent_with_spaces;
}

u32 buffer_get_num_lines(TextBuffer *buffer) {
	return buffer->nlines;
}

String32 buffer_get_line(TextBuffer *buffer, u32 line_number) {
	if (line_number >= buffer->nlines) {
		return str32(NULL, 0);
	}
	Line *line = &buffer->lines[line_number];
	return (String32) {
		.str = line->str, .len = line->len
	};
}

char *buffer_get_line_utf8(TextBuffer *buffer, u32 line_number) {
	return str32_to_utf8_cstr(buffer_get_line(buffer, line_number));
}

// Returns a simple checksum of the buffer.
// This is only used for testing, and shouldn't be relied on.
static u64 buffer_checksum(TextBuffer *buffer) {
	u64 sum = 0x40fdd49b58ee4b15; // some random prime number
	for (Line *line = buffer->lines, *end = line + buffer->nlines; line != end; ++line) {
		for (char32_t *p = line->str, *p_end = p + line->len; p != p_end; ++p) {
			sum += *p;
			sum *= 0xf033ae1b58e6562f; // another random prime number
			sum += 0x6fcc63c3d38a2bb9; // another random prime number
		}
	}
	return sum;
}

size_t buffer_get_text_at_pos(TextBuffer *buffer, BufferPos pos, char32_t *text, size_t nchars) {
	if (!buffer_pos_valid(buffer, pos)) {
		return 0; // invalid position. no chars for you!
	}
	char32_t *p = text;
	size_t chars_left = nchars;
	Line *line = &buffer->lines[pos.line], *end = buffer->lines + buffer->nlines;
	u32 index = pos.index;
	while (chars_left) {
		u32 chars_from_this_line = line->len - index;
		if (chars_left <= chars_from_this_line) {
			if (p) memcpy(p, line->str + index, chars_left * sizeof *p);
			chars_left = 0;
		} else {
			if (p) {
				memcpy(p, line->str + index, chars_from_this_line * sizeof *p);
				p += chars_from_this_line;
				*p++ = '\n';
			}
			chars_left -= chars_from_this_line+1;
		}
		
		index = 0;
		++line;
		if (chars_left && line == end) {
			// reached end of file before getting full text
			break;
		}
	}
	return nchars - chars_left;
}

String32 buffer_get_str32_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars) {
	String32 s32 = {0};
	size_t len = buffer_get_text_at_pos(buffer, pos, NULL, nchars);
	if (len) {
		char32_t *str = buffer_calloc(buffer, len, sizeof *str);
		if (str) {
			buffer_get_text_at_pos(buffer, pos, str, nchars);
			s32.str = str;
			s32.len = len;
		}
	}
	return s32;
}

char *buffer_get_utf8_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars) {
	String32 s32 = buffer_get_str32_text_at_pos(buffer, pos, nchars);
	char *ret = str32_to_utf8_cstr(s32);
	if (!ret) buffer_out_of_mem(buffer);
	str32_free(&s32);
	return ret;
}

size_t buffer_contents_utf8(TextBuffer *buffer, char *out) {
	char *p = out, x[4];
	size_t size = 0;
	for (Line *line = buffer->lines, *end = line + buffer->nlines; line != end; ++line) {
		char32_t *str = line->str;
		for (u32 i = 0, len = line->len; i < len; ++i) {
			size_t bytes = unicode_utf32_to_utf8(p ? p : x, str[i]);
			if (p) p += bytes;
			size += bytes;
		}
		if (line != end - 1) {
			// newline
			if (p) *p++ = '\n';
			size += 1;
		}
	}
	if (p) *p = '\0';
	size += 1;
	return size;
}

char *buffer_contents_utf8_alloc(TextBuffer *buffer) {
	size_t size = buffer_contents_utf8(buffer, NULL);
	char *s = calloc(1, size);
	buffer_contents_utf8(buffer, s);
	return s;
}


static BufferPos buffer_pos_advance(TextBuffer *buffer, BufferPos pos, size_t nchars) {
	buffer_pos_validate(buffer, &pos);
	size_t chars_left = nchars;
	Line *line = &buffer->lines[pos.line], *end = buffer->lines + buffer->nlines;
	u32 index = pos.index;
	while (line != end) {
		u32 chars_from_this_line = line->len - index;
		if (chars_left <= chars_from_this_line) {
			index += (u32)chars_left;
			pos.index = index;
			pos.line = (u32)(line - buffer->lines);
			return pos;
		}
		chars_left -= chars_from_this_line+1; // +1 for newline
		index = 0;
		++line;
	}
	return buffer_pos_end_of_file(buffer);
}


i64 buffer_pos_diff(TextBuffer *buffer, BufferPos p1, BufferPos p2) {
	assert(buffer_pos_valid(buffer, p1));
	assert(buffer_pos_valid(buffer, p2));

	if (p1.line == p2.line) {
		// p1 and p2 are in the same line
		return (i64)p2.index - p1.index;
	}
	i64 factor = 1;
	if (p1.line > p2.line) {
		// switch positions so p2 has the later line
		BufferPos tmp = p1;
		p1 = p2;
		p2 = tmp;
		factor = -1;
	}

	assert(p2.line > p1.line);
	i64 chars_at_end_of_p1_line = buffer->lines[p1.line].len - p1.index + 1; // + 1 for newline
	i64 chars_at_start_of_p2_line = p2.index;
	i64 chars_in_lines_in_between = 0;
	// now we need to add up the lengths of the lines between p1 and p2
	for (Line *line = buffer->lines + (p1.line + 1), *end = buffer->lines + p2.line; line != end; ++line) {
		chars_in_lines_in_between += line->len + 1; // +1 for newline
	}
	i64 total = chars_at_end_of_p1_line + chars_in_lines_in_between + chars_at_start_of_p2_line;
	return total * factor;
}

int buffer_pos_cmp(BufferPos p1, BufferPos p2) {
	if (p1.line < p2.line) {
		return -1;
	} else if (p1.line > p2.line) {
		return +1;
	} else {
		if (p1.index < p2.index) {
			return -1;
		} else if (p1.index > p2.index) {
			return +1;
		} else {
			return 0;
		}
	}
}

bool buffer_pos_eq(BufferPos p1, BufferPos p2) {
	return p1.line == p2.line && p1.index == p2.index;
}

static BufferPos buffer_pos_min(BufferPos p1, BufferPos p2) {
	return buffer_pos_cmp(p1, p2) < 0 ? p1 : p2;
}

static BufferPos buffer_pos_max(BufferPos p1, BufferPos p2) {
	return buffer_pos_cmp(p1, p2) > 0 ? p1 : p2;
}

static void buffer_pos_print(BufferPos p) {
	printf("[" U32_FMT ":" U32_FMT "]", p.line, p.index);
}

// for debugging
#if !NDEBUG
static void buffer_pos_check_valid(TextBuffer *buffer, BufferPos p) {
	assert(p.line < buffer->nlines);
	assert(p.index <= buffer->lines[p.line].len);
}

static bool buffer_line_valid(Line *line) {
	if (line->len && !line->str)
		return false;
	return true;
}

void buffer_check_valid(TextBuffer *buffer) {
	assert(buffer->nlines);
	buffer_pos_check_valid(buffer, buffer->cursor_pos);
	if (buffer->selection) {
		buffer_pos_check_valid(buffer, buffer->selection_pos);
		// you shouldn't be able to select nothing
		assert(!buffer_pos_eq(buffer->cursor_pos, buffer->selection_pos));
	}
	for (u32 i = 0; i < buffer->nlines; ++i) {
		Line *line = &buffer->lines[i];
		assert(buffer_line_valid(line));
	}
}
#else
static void buffer_pos_check_valid(TextBuffer *buffer, BufferPos p) {
	(void)buffer; (void)p;
}

void buffer_check_valid(TextBuffer *buffer) {
	(void)buffer;
}
#endif

static Status buffer_edit_create(TextBuffer *buffer, BufferEdit *edit, BufferPos start, u32 prev_len, u32 new_len) {
	edit->time = buffer->ted->frame_time;
	if (prev_len == 0)
		edit->prev_text = NULL; // if there's no previous text, don't allocate anything
	else
		edit->prev_text = calloc(1, prev_len * sizeof *edit->prev_text);
	if (prev_len == 0 || edit->prev_text) {
		edit->pos = start;
		edit->prev_len = prev_len;
		edit->new_len = new_len;
		if (prev_len) {
			size_t chars_gotten = buffer_get_text_at_pos(buffer, start, edit->prev_text, prev_len);
			edit->prev_len = (u32)chars_gotten; // update the previous length, in case it went past the end of the file
		}
		return true;
	} else {
		return false;
	}
}


static void buffer_edit_print(BufferEdit *edit) {
	buffer_pos_print(edit->pos);
	printf(" (" U32_FMT " chars): ", edit->prev_len);
	for (size_t i = 0; i < edit->prev_len; ++i) {
		char32_t c = edit->prev_text[i];
		if (c == '\n')
			printf("\\n");
		else
			printf("%lc", (wint_t)c);
	}
	printf(" => " U32_FMT " chars.\n", edit->new_len);
}

static void buffer_print_undo_history(TextBuffer *buffer) {
	printf("-----------------\n");
	arr_foreach_ptr(buffer->undo_history, BufferEdit, e)
		buffer_edit_print(e);
}

// add this edit to the undo history
// call this before actually changing buffer
static void buffer_edit(TextBuffer *buffer, BufferPos start, u32 prev_len, u32 new_len) {
	BufferEdit edit = {0};
	if (buffer_edit_create(buffer, &edit, start, prev_len, new_len)) {
		edit.chain = buffer->chaining_edits;
		if (buffer->will_chain_edits) buffer->chaining_edits = true;
		buffer_append_edit(buffer, &edit);
	}
}

// change the capacity of edit->prev_text
static Status buffer_edit_resize_prev_text(TextBuffer *buffer, BufferEdit *edit, u32 new_capacity) {
	assert(edit->prev_len <= new_capacity);
	if (new_capacity == 0) {
		free(edit->prev_text);
		edit->prev_text = NULL;
	} else {
		char32_t *new_text = buffer_realloc(buffer, edit->prev_text, new_capacity * sizeof *new_text);
		if (new_text) {
			edit->prev_text = new_text;
		} else {
			return false;
		}
	}
	return true;
}

// does this edit actually make a difference to the buffer?
static bool buffer_edit_does_anything(TextBuffer *buffer, BufferEdit *edit) {
	if (edit->prev_len == edit->new_len) {
		// @TODO(optimization): compare directly to the buffer contents,
		// rather than extracting them temporarily into new_text.
		char32_t *new_text = buffer_calloc(buffer, edit->new_len, sizeof *new_text);
		if (new_text) {
			size_t len = buffer_get_text_at_pos(buffer, edit->pos, new_text, edit->new_len);
			assert(len == edit->new_len);
			int cmp = memcmp(edit->prev_text, new_text, len * sizeof *new_text);
			free(new_text);
			return cmp != 0;
		} else {
			return false;
		}
	} else {
		return true;
	}
}

// has enough time passed since the last edit that we should create a new one?
//
// is_deletion should be set to true if the edit involves any deletion.
static bool buffer_edit_split(TextBuffer *buffer, bool is_deletion) {
	BufferEdit *last_edit = arr_lastp(buffer->undo_history);
	if (!last_edit) return true;
	if (buffer->will_chain_edits) return true;
	if (buffer->chaining_edits) return false;
	double curr_time = buffer->ted->frame_time;
	double undo_time_cutoff = buffer_settings(buffer)->undo_save_time; // only keep around edits for this long (in seconds).
	return last_edit->time <= buffer->last_write_time // last edit happened before buffer write (we need to split this so that undo_history_write_pos works)
		|| curr_time - last_edit->time > undo_time_cutoff
		|| (curr_time != last_edit->time && (// if the last edit didn't take place on the same frame,
			(last_edit->prev_len && !is_deletion) || // last edit deleted text but this edit inserts text
			(last_edit->new_len && is_deletion) // last edit inserted text and this one deletes text
		));
}

// removes the last edit in the undo history if it doesn't do anything
static void buffer_remove_last_edit_if_empty(TextBuffer *buffer) {
	if (buffer->store_undo_events) {
		BufferEdit *last_edit = arr_lastp(buffer->undo_history);
		if (last_edit && !buffer_edit_does_anything(buffer, last_edit)) {
			buffer_edit_free(last_edit);
			arr_remove_last(buffer->undo_history);
		}
	}
}

u32 buffer_line_len(TextBuffer *buffer, u32 line_number) {
	if (line_number >= buffer->nlines)
		return 0;
	return buffer->lines[line_number].len;
}

// returns true if allocation was succesful
static Status buffer_line_set_len(TextBuffer *buffer, Line *line, u32 new_len) {
	if (new_len >= 8) {
		u32 curr_capacity = (u32)1 << (32 - util_count_leading_zeroes32(line->len));
		assert(curr_capacity > line->len);

		if (new_len >= curr_capacity) {
			u8 leading_zeroes = util_count_leading_zeroes32(new_len);
			if (leading_zeroes == 0) {
				// this line is too big
				return false;
			} else {
				u32 new_capacity = (u32)1 << (32 - leading_zeroes);
				assert(new_capacity > new_len);
				char32_t *new_str = buffer_realloc(buffer, line->str, new_capacity * sizeof *line->str);
				if (!new_str) {
					// allocation failed ):
					return false;
				}
				// allocation successful
				line->str = new_str;
			}
		}
	} else if (!line->str) {
		// start by allocating 8 code points
		line->str = buffer_malloc(buffer, 8 * sizeof *line->str);
		if (!line->str) {
			// ):
			return false;
		}
	}
	line->len = new_len;
	assert(line->str);
	return true;
}

// grow capacity of lines array
// returns true if successful
static Status buffer_lines_set_min_capacity(TextBuffer *buffer, Line **lines, u32 *lines_capacity, u32 minimum_capacity) {
	while (minimum_capacity >= *lines_capacity) {
		// allocate more lines
		u32 new_capacity = *lines_capacity * 2;
		Line *new_lines = buffer_realloc(buffer, *lines, new_capacity * sizeof(Line));
		if (new_lines) {
			*lines = new_lines;
			// zero new lines
			memset(new_lines + *lines_capacity, 0, (new_capacity - *lines_capacity) * sizeof(Line));
			*lines_capacity = new_capacity;
		} else {
			return false;
		}
	}
	return true;
}

static void buffer_line_append_char(TextBuffer *buffer, Line *line, char32_t c) {
	if (c == '\r') return;
	if (buffer_line_set_len(buffer, line, line->len + 1))
		line->str[line->len-1] = c;
}

static void buffer_line_free(Line *line) {
	free(line->str);
}

// Free a buffer. Once a buffer is freed, you can call buffer_create on it again.
// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void buffer_free(TextBuffer *buffer) {
	if (!buffer->ted->quit) { // don't send didClose on quit (calling buffer_lsp would actually create a LSP if this is called after destroying all the LSPs which isnt good)
		
		LSP *lsp = buffer_lsp(buffer);
		if (lsp) {
			buffer_send_lsp_did_close(buffer, lsp, NULL);	
		}
	}
	
	Line *lines = buffer->lines;
	u32 nlines = buffer->nlines;
	for (u32 i = 0; i < nlines; ++i) {
		buffer_line_free(&lines[i]);
	}
	free(lines);
	free(buffer->path);

	arr_foreach_ptr(buffer->undo_history, BufferEdit, edit)
		buffer_edit_free(edit);
	arr_foreach_ptr(buffer->redo_history, BufferEdit, edit)
		buffer_edit_free(edit);

	arr_free(buffer->undo_history);
	arr_free(buffer->redo_history);
	memset(buffer, 0, sizeof *buffer);
}

// clear contents, undo history, etc. of a buffer
void buffer_clear(TextBuffer *buffer) {
	bool is_line_buffer = buffer->is_line_buffer;
	Ted *ted = buffer->ted;
	char error[sizeof buffer->error];
	memcpy(error, buffer->error, sizeof error);
	buffer_free(buffer);
	if (is_line_buffer) {
		line_buffer_create(buffer, ted);
	} else {
		buffer_create(buffer, ted);
	}
	memcpy(buffer->error, error, sizeof error);
}


// print the contents of a buffer to stdout
static void buffer_print(TextBuffer const *buffer) {
	printf("\033[2J\033[;H"); // clear terminal screen
	Line *lines = buffer->lines;
	u32 nlines = buffer->nlines;
	
	for (u32 i = 0; i < nlines; ++i) {
		Line *line = &lines[i];
		for (u32 j = 0; j < line->len; ++j) {
			// on windows, this will print characters outside the Basic Multilingual Plane incorrectly
			// but this function is just for debugging anyways
			print("%lc", (wchar_t)line->str[j]);
		}
		print("\n");
	}
	fflush(stdout);
}

static void buffer_render_char(TextBuffer *buffer, Font *font, TextRenderState *state, char32_t c) {
	switch (c) {
	case '\n': assert(0); break;
	case '\r': break; // for CRLF line endings
	case '\t': {
		u16 tab_width = buffer_tab_width(buffer);
		double x = state->x;
		double space_size = text_font_char_width(font, ' ');
		double tab_width_px = tab_width * space_size;
		double tabs = x / tab_width_px;
		double tab_stop = (1.0 + floor(tabs)) * tab_width_px;
		if (tab_stop - x < space_size * 0.5) {
			// tab shouldn't be less than half a space
			tab_stop += tab_width_px;
		}
		state->x = tab_stop;
	} break;
	default:
		text_char_with_state(font, state, c);
		break;
	}
}

// convert line character index to offset in pixels
static double buffer_index_to_xoff(TextBuffer *buffer, u32 line_number, u32 index) {
	if (line_number >= buffer->nlines) {
		assert(0);
		return 0;
	}
	Line *line = &buffer->lines[line_number];
	char32_t *str = line->str;
	if (index > line->len)
		index = line->len;
	Font *font = buffer_font(buffer);
	TextRenderState state = text_render_state_default;
	state.render = false;
	for (u32 i = 0; i < index; ++i) {
		buffer_render_char(buffer, font, &state, str[i]);
	}
	return state.x;
}

// convert line x offset in pixels to character index
static u32 buffer_xoff_to_index(TextBuffer *buffer, u32 line_number, double xoff) {
	if (line_number >= buffer->nlines) {
		assert(0);
		return 0;
	}
	if (xoff <= 0) {
		return 0;
	}
	Line *line = &buffer->lines[line_number];
	char32_t *str = line->str;
	Font *font = buffer_font(buffer);
	TextRenderState state = text_render_state_default;
	state.render = false;
	for (u32 i = 0; i < line->len; ++i) {
		double x0 = state.x;
		buffer_render_char(buffer, font, &state, str[i]);
		double x1 = state.x;
		if (x1 > xoff) {
			if (x1 - xoff > xoff - x0)
				return i;
			else
				return i + 1;
		}
	}
	return line->len;
}


void buffer_text_dimensions(TextBuffer *buffer, u32 *lines, u32 *columns) {
	if (lines) {
		*lines = buffer->nlines;
	}
	if (columns) {
		double longest_line = 0;
		// which line on screen is the longest?
		for (u32 l = buffer->first_line_on_screen; l <= buffer->last_line_on_screen && l < buffer->nlines; ++l) {
			Line *line = &buffer->lines[l];
			longest_line = maxd(longest_line, buffer_index_to_xoff(buffer, l, line->len));
		}
		*columns = (u32)(longest_line / text_font_char_width(buffer_font(buffer), ' '));
	}
}


float buffer_display_lines(TextBuffer *buffer) {
	return (buffer->y2 - buffer->y1) / text_font_char_height(buffer_font(buffer));
}

float buffer_display_cols(TextBuffer *buffer) {
	return (buffer->x2 - buffer->x1) / text_font_char_width(buffer_font(buffer), ' ');
}

// make sure we don't scroll too far
static void buffer_correct_scroll(TextBuffer *buffer) {
	if (buffer->scroll_x < 0)
		buffer->scroll_x = 0;
	if (buffer->scroll_y < 0)
		buffer->scroll_y = 0;
	u32 nlines, ncols;
	buffer_text_dimensions(buffer, &nlines, &ncols);
	double max_scroll_x = (double)ncols  - buffer_display_cols(buffer);
	max_scroll_x += 2; // allow "overscroll" (makes it so you can see the cursor when it's on the right side of the screen)
	double max_scroll_y = (double)nlines - buffer_display_lines(buffer);
	if (max_scroll_x <= 0) {
		buffer->scroll_x = 0;
	} else if (buffer->scroll_x > max_scroll_x) {
		buffer->scroll_x = max_scroll_x;
	}

	if (max_scroll_y <= 0) {
		buffer->scroll_y = 0;
	} else if (buffer->scroll_y > max_scroll_y) {
		buffer->scroll_y = max_scroll_y;
	}
}

void buffer_scroll(TextBuffer *buffer, double dx, double dy) {
	buffer->scroll_x += dx;
	buffer->scroll_y += dy;
	buffer_correct_scroll(buffer);
}

vec2 buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos) {
	buffer_pos_validate(buffer, &pos);
	u32 line = pos.line, index = pos.index;
	double xoff = buffer_index_to_xoff(buffer, line, index);
	Font *font = buffer_font(buffer);
	float x = (float)((double)xoff - buffer->scroll_x * text_font_char_width(font, ' ')) + buffer->x1;
	float y = (float)((double)line - buffer->scroll_y) * text_font_char_height(font) + buffer->y1;
	return Vec2(x, y);
}

bool buffer_pixels_to_pos(TextBuffer *buffer, vec2 pixel_coords, BufferPos *pos) {
	bool ret = true;
	double x = pixel_coords.x, y = pixel_coords.y;
	Font *font = buffer_font(buffer);
	pos->line = pos->index = 0;

	x -= buffer->x1;
	y -= buffer->y1;
	
	double buffer_width = buffer->x2 - buffer->x1;
	double buffer_height = buffer->y2 - buffer->y1;
	
	if (x < 0 || y < 0 || x >= buffer_width || y >= buffer_height)
		ret = false;
	
	x = clampd(x, 0, buffer_width);
	y = clampd(y, 0, buffer_height);
	
	double xoff = x + buffer->scroll_x * text_font_char_width(font, ' ');
	
	u32 line = (u32)floor(y / text_font_char_height(font) + buffer->scroll_y);
	if (line >= buffer->nlines) line = buffer->nlines - 1;
	u32 index = buffer_xoff_to_index(buffer, line, xoff);
	pos->line = line;
	pos->index = index;
	
	return ret;
}

bool buffer_clip_rect(TextBuffer *buffer, Rect *r) {
	float x1, y1, x2, y2;
	rect_coords(*r, &x1, &y1, &x2, &y2);
	if (x1 > buffer->x2 || y1 > buffer->y2 || x2 < buffer->x1 || y2 < buffer->y1) {
		r->pos = r->size = Vec2(0, 0);
		return false;
	}
	if (x1 < buffer->x1) x1 = buffer->x1;
	if (y1 < buffer->y1) y1 = buffer->y1;
	if (x2 > buffer->x2) x2 = buffer->x2;
	if (y2 > buffer->y2) y2 = buffer->y2;
	*r = rect4(x1, y1, x2, y2);
	return true;
}


void buffer_scroll_to_pos(TextBuffer *buffer, BufferPos pos) {
	const Settings *settings = buffer_settings(buffer);
	Font *font = buffer_font(buffer);
	double line = pos.line;
	double space_width = text_font_char_width(font, ' ');
	double char_height = text_font_char_height(font);
	double col = buffer_index_to_xoff(buffer, pos.line, pos.index) / space_width;
	double display_lines = (buffer->y2 - buffer->y1) / char_height;
	double display_cols = (buffer->x2 - buffer->x1) / space_width;
	double scroll_x = buffer->scroll_x, scroll_y = buffer->scroll_y;
	double scrolloff = settings->scrolloff;
	
	// for very small buffers, the scrolloff might need to be reduced.
	scrolloff = mind(scrolloff, display_lines * 0.5);

	// scroll left if pos is off screen in that direction
	double max_scroll_x = col - scrolloff;
	scroll_x = mind(scroll_x, max_scroll_x);
	// scroll right
	double min_scroll_x = col - display_cols + scrolloff;
	scroll_x = maxd(scroll_x, min_scroll_x);
	// scroll up
	double max_scroll_y = line - scrolloff;
	scroll_y = mind(scroll_y, max_scroll_y);
	// scroll down
	double min_scroll_y = line - display_lines + scrolloff;
	scroll_y = maxd(scroll_y, min_scroll_y);

	buffer->scroll_x = scroll_x;
	buffer->scroll_y = scroll_y;
	buffer_correct_scroll(buffer); // it's possible that min/max_scroll_x/y go too far
}

void buffer_scroll_center_pos(TextBuffer *buffer, BufferPos pos) {
	double line = pos.line;
	Font *font = buffer_font(buffer);
	float space_width = text_font_char_width(font, ' ');
	float char_height = text_font_char_height(font);
	double xoff = buffer_index_to_xoff(buffer, pos.line, pos.index);
	buffer->scroll_x = (xoff - (buffer->x1 - buffer->x1) * 0.5) / space_width;
	buffer->scroll_y = line - (buffer->y2 - buffer->y1) / char_height * 0.5;
	buffer_correct_scroll(buffer);
}

// if the cursor is offscreen, this will scroll to make it onscreen.
void buffer_scroll_to_cursor(TextBuffer *buffer) {
	buffer_scroll_to_pos(buffer, buffer->cursor_pos);
}

// scroll so that the cursor is in the center of the screen
void buffer_center_cursor(TextBuffer *buffer) {
	double cursor_line = buffer->cursor_pos.line;
	double cursor_col  = buffer_index_to_xoff(buffer, (u32)cursor_line, buffer->cursor_pos.index)
		/ text_font_char_width(buffer_font(buffer), ' ');
	double display_lines = buffer_display_lines(buffer);
	double display_cols = buffer_display_cols(buffer);
	
	buffer->scroll_x = cursor_col - display_cols * 0.5;
	buffer->scroll_y = cursor_line - display_lines * 0.5;

	buffer_correct_scroll(buffer);
}

// move left (if `by` is negative) or right (if `by` is positive) by the specified amount.
// returns the signed number of characters successfully moved (it could be less in magnitude than `by` if the beginning of the file is reached)
static i64 buffer_pos_move_horizontally(TextBuffer *buffer, BufferPos *p, i64 by) {
	buffer_pos_validate(buffer, p);
	if (by < 0) {
		by = -by;
		i64 by_start = by;
		
		while (by > 0) {
			if (by <= p->index) {
				// no need to go to the previous line
				p->index -= (u32)by;
				by = 0;
			} else {
				by -= p->index;
				p->index = 0;
				if (p->line == 0) {
					// beginning of file reached
					return -(by_start - by);
				}
				--by; // count newline as a character
				// previous line
				--p->line;
				p->index = buffer->lines[p->line].len;
			}
		}
		return -by_start;
	} else if (by > 0) {
		i64 by_start = by;
		if (p->line >= buffer->nlines)
			*p = buffer_pos_end_of_file(buffer); // invalid position; move to end of buffer
		Line *line = &buffer->lines[p->line];
		while (by > 0) {
			if (by <= line->len - p->index) {
				p->index += (u32)by;
				by = 0;
			} else {
				by -= line->len - p->index;
				p->index = line->len;
				if (p->line >= buffer->nlines - 1) {
					// end of file reached
					return by_start - by;
				}
				--by;
				++p->line;
				p->index = 0;
			}
		}
		return by_start;
	}
	return 0;
}

// same as buffer_pos_move_horizontally, but for up and down.
static i64 buffer_pos_move_vertically(TextBuffer *buffer, BufferPos *pos, i64 by) {
	buffer_pos_validate(buffer, pos);
	// moving up/down should preserve the x offset, not the index.
	// consider:
	// tab|hello world
	// tab|tab|more text
	// the character above the 'm' is the 'o', not the 'e'
	if (by < 0) {
		by = -by;
		double xoff = buffer_index_to_xoff(buffer, pos->line, pos->index);
		if (pos->line < by) {
			i64 ret = pos->line;
			pos->line = 0;
			return -ret;
		}
		pos->line -= (u32)by;
		pos->index = buffer_xoff_to_index(buffer, pos->line, xoff);
		u32 line_len = buffer->lines[pos->line].len;
		if (pos->index >= line_len) pos->index = line_len;
		return -by;
	} else if (by > 0) {
		double xoff = buffer_index_to_xoff(buffer, pos->line, pos->index);
		if (pos->line + by >= buffer->nlines) {
			i64 ret = buffer->nlines-1 - pos->line;
			pos->line = buffer->nlines-1;
			return ret;
		}
		pos->line += (u32)by;
		pos->index = buffer_xoff_to_index(buffer, pos->line, xoff);
		u32 line_len = buffer->lines[pos->line].len;
		if (pos->index >= line_len) pos->index = line_len;
		return by;
	}
	return 0;
}

i64 buffer_pos_move_left(TextBuffer *buffer, BufferPos *pos, i64 by) {
	return -buffer_pos_move_horizontally(buffer, pos, -by);
}

i64 buffer_pos_move_right(TextBuffer *buffer, BufferPos *pos, i64 by) {
	return +buffer_pos_move_horizontally(buffer, pos, +by);
}

i64 buffer_pos_move_up(TextBuffer *buffer, BufferPos *pos, i64 by) {
	return -buffer_pos_move_vertically(buffer, pos, -by);
}

i64 buffer_pos_move_down(TextBuffer *buffer, BufferPos *pos, i64 by) {
	return +buffer_pos_move_vertically(buffer, pos, +by);
}

static bool buffer_line_is_blank(Line *line) {
	for (u32 i = 0; i < line->len; ++i)
		if (!is32_space(line->str[i]))
			return false;
	return true;
}

void buffer_cursor_move_to_pos(TextBuffer *buffer, BufferPos pos) {
	buffer_pos_validate(buffer, &pos);
	if (buffer_pos_eq(buffer->cursor_pos, pos)) {
		return;
	}
	
	if (labs((long)buffer->cursor_pos.line - (long)pos.line) > 20) {
		// if this is a big jump, update the previous cursor pos
		buffer->prev_cursor_pos = buffer->cursor_pos;
	}
	
	buffer->cursor_pos = pos;
	buffer->selection = false;
	buffer_scroll_to_cursor(buffer);
	signature_help_retrigger(buffer->ted);
}

void buffer_cursor_move_to_prev_pos(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer->prev_cursor_pos);
}

i64 buffer_cursor_move_left(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = 0;
	// if use is selecting something, then moves left, the cursor should move to the left of the selection
	if (buffer->selection) {
		if (buffer_pos_cmp(buffer->selection_pos, buffer->cursor_pos) < 0) {
			ret = buffer_pos_diff(buffer, buffer->selection_pos, buffer->cursor_pos);
			buffer_cursor_move_to_pos(buffer, buffer->selection_pos);
		}
		buffer->selection = false;
	} else {
		ret = buffer_pos_move_left(buffer, &cur_pos, by);
		buffer_cursor_move_to_pos(buffer, cur_pos);
	}
	return ret;
}

i64 buffer_cursor_move_right(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = 0;
	if (buffer->selection) {
		if (buffer_pos_cmp(buffer->selection_pos, buffer->cursor_pos) > 0) {
			ret = buffer_pos_diff(buffer, buffer->cursor_pos, buffer->selection_pos);
			buffer_cursor_move_to_pos(buffer, buffer->selection_pos);
		}
		buffer->selection = false;
	} else {
		ret = buffer_pos_move_right(buffer, &cur_pos, by);
		buffer_cursor_move_to_pos(buffer, cur_pos);
	}
	return ret;
}

i64 buffer_cursor_move_up(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	if (buffer->selection && buffer_pos_cmp(buffer->selection_pos, buffer->cursor_pos) < 0)
		cur_pos = buffer->selection_pos; // go to one line above the selection pos (instead of the cursor pos) if it's before the cursor
	i64 ret = buffer_pos_move_up(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_cursor_move_down(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	if (buffer->selection && buffer_pos_cmp(buffer->selection_pos, buffer->cursor_pos) > 0)
		cur_pos = buffer->selection_pos;
	i64 ret = buffer_pos_move_down(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_pos_move_up_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by) {
	if (by == 0) return 0;
	if (by < 0) return buffer_pos_move_down_blank_lines(buffer, pos, -by);
	
	
	buffer_pos_validate(buffer, pos);
	
	u32 line = pos->line;
	
	// skip blank lines at start
	while (line > 0 && buffer_line_is_blank(&buffer->lines[line]))
		--line;
	
	i64 i;
	for (i = 0; i < by; ++i) {
		while (1) {
			if (line == 0) {
				goto end;
			} else if (buffer_line_is_blank(&buffer->lines[line])) {
				// move to the top blank line in this group
				while (line > 0 && buffer_line_is_blank(&buffer->lines[line-1]))
					--line;
				break;
			} else {
				--line;
			}
		}
	}
	end:
	pos->line = line;
	pos->index = 0;
	return i;
}

i64 buffer_pos_move_down_blank_lines(TextBuffer *buffer, BufferPos *pos, i64 by) {
	if (by == 0) return 0;
	if (by < 0) return buffer_pos_move_up_blank_lines(buffer, pos, -by);
	
	buffer_pos_validate(buffer, pos);
	
	u32 line = pos->line;
	// skip blank lines at start
	while (line + 1 < buffer->nlines && buffer_line_is_blank(&buffer->lines[line]))
		++line;
	
	i64 i;
	for (i = 0; i < by; ++i) {
		while (1) {
			if (line + 1 >= buffer->nlines) {
				goto end;
			} else if (buffer_line_is_blank(&buffer->lines[line])) {
				// move to the bottom blank line in this group
				while (line + 1 < buffer->nlines && buffer_line_is_blank(&buffer->lines[line+1]))
					++line;
				break;
			} else {
				++line;
			}
		}
	}
	end:
	pos->line = line;
	pos->index = 0;
	return i;
}

i64 buffer_cursor_move_up_blank_lines(TextBuffer *buffer, i64 by) {
	BufferPos cursor = buffer->cursor_pos;
	i64 ret = buffer_pos_move_up_blank_lines(buffer, &cursor, by);
	buffer_cursor_move_to_pos(buffer, cursor);
	return ret;
}

i64 buffer_cursor_move_down_blank_lines(TextBuffer *buffer, i64 by) {
	BufferPos cursor = buffer->cursor_pos;
	i64 ret = buffer_pos_move_down_blank_lines(buffer, &cursor, by);
	buffer_cursor_move_to_pos(buffer, cursor);
	return ret;
}

// move left / right by the specified number of words
// returns the number of words successfully moved forward
i64 buffer_pos_move_words(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	buffer_pos_validate(buffer, pos);
	if (nwords > 0) {
		for (i64 i = 0; i < nwords; ++i) { // move forward one word `nwords` times
			Line *line = &buffer->lines[pos->line];
			u32 index = pos->index;
			const char32_t *str = line->str;
			if (index == line->len) {
				if (pos->line >= buffer->nlines - 1) {
					// end of file reached
					return i;
				} else {
					// end of line reached; move to next line
					++pos->line;
					pos->index = 0;
				}
			} else {
				bool starting_isword = is32_word(str[index]) != 0;
				for (; index < line->len; ++index) {
					bool this_isword = is32_word(str[index]) != 0;
					if (this_isword != starting_isword) {
						// either the position *was* on an alphanumeric character and now it's not
						// or it wasn't and now it is.
						break;
					}
				}
				
				pos->index = index;
			}
		}
		return nwords;
	} else if (nwords < 0) {
		nwords = -nwords;
		for (i64 i = 0; i < nwords; ++i) {
			Line *line = &buffer->lines[pos->line];
			u32 index = pos->index;
			const char32_t *str = line->str;
			if (index == 0) {
				if (pos->line == 0) {
					// start of file reached
					return i;
				} else {
					// start of line reached; move to previous line
					--pos->line;
					pos->index = buffer->lines[pos->line].len;
				}
			} else {
				--index;
				if (index > 0) {
					bool starting_isword = is32_word(str[index]) != 0;
					while (true) {
						bool this_isword = is32_word(str[index]) != 0;
						if (this_isword != starting_isword) {
							++index;
							break;
						}
						if (index == 0) break;
						--index;
					}
				}
				pos->index = index;
			}
		}
	}
	return 0;
}

i64 buffer_pos_move_left_words(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	return -buffer_pos_move_words(buffer, pos, -nwords);
}

i64 buffer_pos_move_right_words(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	return +buffer_pos_move_words(buffer, pos, +nwords);
}

i64 buffer_cursor_move_left_words(TextBuffer *buffer, i64 nwords) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_left_words(buffer, &cur_pos, nwords);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_cursor_move_right_words(TextBuffer *buffer, i64 nwords) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_right_words(buffer, &cur_pos, nwords);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

void buffer_word_span_at_pos(TextBuffer *buffer, BufferPos pos, u32 *word_start, u32 *word_end) {
	buffer_pos_validate(buffer, &pos);
	Line *line = &buffer->lines[pos.line];
	char32_t *str = line->str;
	i64 start, end;
	for (start = pos.index; start > 0; --start) {
		if (!is32_word(str[start - 1]))
			break;
	}
	for (end = pos.index; end < line->len; ++end) {
		if (!is32_word(str[end]))
			break;
	}
	*word_start = (u32)start;
	*word_end = (u32)end;
}

String32 buffer_word_at_pos(TextBuffer *buffer, BufferPos pos) {
	buffer_pos_validate(buffer, &pos);
	Line *line = &buffer->lines[pos.line];
	u32 word_start=0, word_end=0;
	buffer_word_span_at_pos(buffer, pos, &word_start, &word_end);
	u32 len = (u32)(word_end - word_start);
	if (len == 0)
		return str32(NULL, 0);
	else
		return str32(line->str + word_start, len);
}

String32 buffer_word_at_cursor(TextBuffer *buffer) {
	return buffer_word_at_pos(buffer, buffer->cursor_pos);
}

char *buffer_word_at_cursor_utf8(TextBuffer *buffer) {
	return str32_to_utf8_cstr(buffer_word_at_cursor(buffer));
}

// Returns the position corresponding to the start of the given line.
BufferPos buffer_pos_start_of_line(TextBuffer *buffer, u32 line) {
	(void)buffer;
	return (BufferPos){
		.line = line,
		.index = 0
	};
}

BufferPos buffer_pos_end_of_line(TextBuffer *buffer, u32 line) {
	return (BufferPos){
		.line = line,
		.index = buffer->lines[line].len
	};
}

void buffer_cursor_move_to_start_of_line(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_pos_start_of_line(buffer, buffer->cursor_pos.line));
}

void buffer_cursor_move_to_end_of_line(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_pos_end_of_line(buffer, buffer->cursor_pos.line));
}

void buffer_cursor_move_to_start_of_file(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_pos_start_of_file(buffer));
}

void buffer_cursor_move_to_end_of_file(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_pos_end_of_file(buffer));
}


static void buffer_lines_modified(TextBuffer *buffer, u32 first_line, u32 last_line) {
	assert(last_line >= first_line);
	buffer->modified = true;
	if (first_line < buffer->frame_earliest_line_modified)
		buffer->frame_earliest_line_modified = first_line;
	if (last_line > buffer->frame_latest_line_modified)
		buffer->frame_latest_line_modified = last_line;
}

// insert `number` empty lines starting at index `where`.
static Status buffer_insert_lines(TextBuffer *buffer, u32 where, u32 number) {
	assert(!buffer->is_line_buffer);

	u32 old_nlines = buffer->nlines;
	u32 new_nlines = old_nlines + number;
	if (buffer_lines_set_min_capacity(buffer, &buffer->lines, &buffer->lines_capacity, new_nlines)) {
		assert(where <= old_nlines);
		// make space for new lines
		memmove(buffer->lines + where + (new_nlines - old_nlines),
			buffer->lines + where,
			(old_nlines - where) * sizeof *buffer->lines);
		// zero new lines
		memset(buffer->lines + where, 0, number * sizeof *buffer->lines);
		buffer->nlines = new_nlines;
		return true;
	}
	return false;
}

LSPDocumentID buffer_lsp_document_id(TextBuffer *buffer) {
	LSP *lsp = buffer_lsp(buffer);
	return lsp ? lsp_document_id(lsp, buffer->path) : 0;
}

// LSP uses UTF-16 indices because Microsoft fucking loves UTF-16 and won't let it die
LSPPosition buffer_pos_to_lsp_position(TextBuffer *buffer, BufferPos pos) {
	LSPPosition lsp_pos = {
		.line = pos.line
	};
	buffer_pos_validate(buffer, &pos);
	const Line *line = &buffer->lines[pos.line];
	const char32_t *str = line->str;
	for (uint32_t i = 0; i < pos.index; ++i) {
		if (str[i] < 0x10000)
			lsp_pos.character += 1; // this codepoint needs 1 UTF-16 word
		else
			lsp_pos.character += 2; // this codepoint needs 2 UTF-16 words
	}
	return lsp_pos;
}

LSPDocumentPosition buffer_pos_to_lsp_document_position(TextBuffer *buffer, BufferPos pos) {
	LSPDocumentPosition docpos = {
		.document = buffer_lsp_document_id(buffer),
		.pos = buffer_pos_to_lsp_position(buffer, pos)
	};
	return docpos;
}

BufferPos buffer_pos_from_lsp(TextBuffer *buffer, LSPPosition lsp_pos) {
	if (lsp_pos.line >= buffer->nlines) {
		return buffer_pos_end_of_file(buffer);
	}
	const Line *line = &buffer->lines[lsp_pos.line];
	const char32_t *str = line->str;
	u32 character = 0;
	for (u32 i = 0; i < line->len; ++i) {
		if (character >= lsp_pos.character)
			return (BufferPos){.line = lsp_pos.line, .index = i};
		if (str[i] < 0x10000)
			character += 1;
		else
			character += 2;
	}

	return buffer_pos_end_of_line(buffer, lsp_pos.line);
}

LSPPosition buffer_cursor_pos_as_lsp_position(TextBuffer *buffer) {
	return buffer_pos_to_lsp_position(buffer, buffer->cursor_pos);
}

LSPDocumentPosition buffer_cursor_pos_as_lsp_document_position(TextBuffer *buffer) {
	return buffer_pos_to_lsp_document_position(buffer, buffer->cursor_pos);
}

static void buffer_send_lsp_did_change(LSP *lsp, TextBuffer *buffer, BufferPos pos,
	u32 nchars_deleted, String32 new_text) {
	if (!buffer_is_named_file(buffer))
		return; // this isn't a named buffer so we can't send a didChange request.
	LSPDocumentChangeEvent event = {0};
	if (new_text.len > 0)
		event.text = str32_to_utf8_cstr(new_text);
	event.range.start = buffer_pos_to_lsp_position(buffer, pos);
	BufferPos pos_end = buffer_pos_advance(buffer, pos, nchars_deleted);
	event.range.end = buffer_pos_to_lsp_position(buffer, pos_end);
	lsp_document_changed(lsp, buffer->path, event);
}

BufferPos buffer_insert_text_at_pos(TextBuffer *buffer, BufferPos pos, String32 str) {
	buffer_pos_validate(buffer, &pos);

	if (buffer->view_only)
		return pos;
	if (str.len > U32_MAX) {
		buffer_error(buffer, "Inserting too much text (length: %zu).", str.len);
		return (BufferPos){0};
	}
	for (u32 i = 0; i < str.len; ++i) {
		char32_t c = str.str[i];
		if (c == 0 || c >= UNICODE_CODE_POINTS || (c >= 0xD800 && c <= 0xDFFF)) {
			buffer_error(buffer, "Inserting null character or bad unicode.");
			return (BufferPos){0};
		}
	}
	if (str.len == 0) {
		// no text to insert
		return pos;
	}
	
	// create a copy of str. we need to do this to remove carriage returns and newlines in the case of line buffers
	char32_t str_copy[256];
	char32_t *str_alloc = NULL;
	if (str.len > arr_count(str_copy)) {
		str_alloc = buffer_calloc(buffer, str.len, sizeof *str_alloc);
		memcpy(str_alloc, str.str, str.len * sizeof *str.str);
		str.str = str_alloc;
	} else {
		// most insertions are small, so it's better to do this
		memcpy(str_copy, str.str, str.len * sizeof *str.str);
		str.str = str_copy;
	}
	
	
	if (buffer->is_line_buffer) {
		// remove all the newlines from str.
		str32_remove_all_instances_of_char(&str, '\n');
	}
	str32_remove_all_instances_of_char(&str, '\r');
	
	if (autocomplete_is_open(buffer->ted)) {
		// close completions if a non-word character is typed
		bool close_completions = false;
		for (u32 i = 0; i < str.len; ++i) {
			if (!is32_word(str.str[i])) {
				close_completions = true;
				break;
			}
		}
		if (close_completions)
			autocomplete_close(buffer->ted);
	}


	LSP *lsp = buffer_lsp(buffer);
	if (lsp)
		buffer_send_lsp_did_change(lsp, buffer, pos, 0, str);

	if (buffer->store_undo_events) {
		BufferEdit *last_edit = arr_lastp(buffer->undo_history);
		i64 where_in_last_edit = last_edit ? buffer_pos_diff(buffer, last_edit->pos, pos) : -1;
		// create a new edit, rather than adding to the old one if:
		bool create_new_edit = where_in_last_edit < 0 || where_in_last_edit > last_edit->new_len // insertion is happening outside the previous edit,
			|| buffer_edit_split(buffer, false); // or enough time has elapsed/etc to warrant a new one.

		if (create_new_edit) {
			// create a new edit for this insertion
			buffer_edit(buffer, pos, 0, (u32)str.len);
		} else {
			// merge this edit into the previous one.
			last_edit->new_len += (u32)str.len;
		}

	}

	u32 line_idx = pos.line;
	u32 index = pos.index;
	Line *line = &buffer->lines[line_idx];

	// `text` could consist of multiple lines, e.g. U"line 1\nline 2",

	u32 n_added_lines = (u32)str32_count_char(str, '\n');
	if (n_added_lines) {
		// allocate space for the new lines
		if (buffer_insert_lines(buffer, line_idx + 1, n_added_lines)) {
			line = &buffer->lines[line_idx]; // fix pointer
			// move any text past the cursor on this line to the last added line.
			Line *last_line = &buffer->lines[line_idx + n_added_lines];
			u32 chars_moved = line->len - index;
			if (chars_moved) {
				if (buffer_line_set_len(buffer, last_line, chars_moved)) {
					memcpy(last_line->str, line->str + index, chars_moved * sizeof(char32_t));
					line->len -= chars_moved;
				}
			}
		}
	}


	// insert the actual text from each line in str
	while (1) {
		u32 text_line_len = (u32)str32chr(str, '\n');
		u32 old_len = line->len;
		u32 new_len = old_len + text_line_len;
		if (new_len > old_len) { // handles both overflow and empty text lines
			if (buffer_line_set_len(buffer, line, new_len)) {
				buffer_pos_handle_inserted_chars(&buffer->cursor_pos,    (BufferPos){line_idx, index}, text_line_len);
				buffer_pos_handle_inserted_chars(&buffer->selection_pos, (BufferPos){line_idx, index}, text_line_len);
				// make space for text
				memmove(line->str + index + (new_len - old_len),
					line->str + index,
					(old_len - index) * sizeof(char32_t));
				// insert text
				memcpy(line->str + index, str.str, text_line_len * sizeof(char32_t));
			}

			str.str += text_line_len;
			str.len -= text_line_len;
			index += text_line_len;
		}
		if (str.len) {
			// we've got a newline.
			line_idx += 1;
			index = 0;
			++line;
			++str.str;
			--str.len;
		} else {
			break;
		}
	}

	// We need to put this after the end so the emptiness-checking is done after the edit is made.
	buffer_remove_last_edit_if_empty(buffer);

	buffer_lines_modified(buffer, pos.line, line_idx);

	BufferPos b = {.line = line_idx, .index = index};
	free(str_alloc);
	
	signature_help_retrigger(buffer->ted);
	
	return b;
}

void buffer_insert_char_at_pos(TextBuffer *buffer, BufferPos pos, char32_t c) {
	String32 s = {&c, 1};
	buffer_insert_text_at_pos(buffer, pos, s);
}

// Select (or add to selection) everything between the cursor and pos, and move the cursor to pos
void buffer_select_to_pos(TextBuffer *buffer, BufferPos pos) {
	if (!buffer->selection)
		buffer->selection_pos = buffer->cursor_pos;
	buffer_cursor_move_to_pos(buffer, pos);
	buffer->selection = !buffer_pos_eq(buffer->cursor_pos, buffer->selection_pos); // disable selection if cursor_pos = selection_pos.
}

void buffer_select_left(TextBuffer *buffer, i64 nchars) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_left(buffer, &cpos, nchars);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_right(TextBuffer *buffer, i64 nchars) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_right(buffer, &cpos, nchars);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_down(TextBuffer *buffer, i64 nchars) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_down(buffer, &cpos, nchars);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_up(TextBuffer *buffer, i64 nchars) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_up(buffer, &cpos, nchars);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_down_blank_lines(TextBuffer *buffer, i64 by) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_down_blank_lines(buffer, &cpos, by);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_up_blank_lines(TextBuffer *buffer, i64 by) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_up_blank_lines(buffer, &cpos, by);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_left_words(TextBuffer *buffer, i64 nwords) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_left_words(buffer, &cpos, nwords);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_right_words(TextBuffer *buffer, i64 nwords) {
	BufferPos cpos = buffer->cursor_pos;
	buffer_pos_move_right_words(buffer, &cpos, nwords);
	buffer_select_to_pos(buffer, cpos);
}

void buffer_select_to_start_of_line(TextBuffer *buffer) {
	buffer_select_to_pos(buffer, buffer_pos_start_of_line(buffer, buffer->cursor_pos.line));
}

void buffer_select_to_end_of_line(TextBuffer *buffer) {
	buffer_select_to_pos(buffer, buffer_pos_end_of_line(buffer, buffer->cursor_pos.line));
}

void buffer_select_to_start_of_file(TextBuffer *buffer) {
	buffer_select_to_pos(buffer, buffer_pos_start_of_file(buffer));
}

void buffer_select_to_end_of_file(TextBuffer *buffer) {
	buffer_select_to_pos(buffer, buffer_pos_end_of_file(buffer));
}

void buffer_select_word(TextBuffer *buffer) {
	BufferPos start_pos = buffer->cursor_pos, end_pos = buffer->cursor_pos;
	if (start_pos.index > 0)
		buffer_pos_move_left_words(buffer, &start_pos, 1);
	if (end_pos.index < buffer->lines[end_pos.line].len)
		buffer_pos_move_right_words(buffer, &end_pos, 1);
	
	buffer_cursor_move_to_pos(buffer, end_pos);
	buffer_select_to_pos(buffer, start_pos);
}

void buffer_select_line(TextBuffer *buffer) {
	u32 line = buffer->cursor_pos.line;
	if (line == buffer->nlines - 1)
		buffer_cursor_move_to_pos(buffer, buffer_pos_end_of_line(buffer, line));
	else
		buffer_cursor_move_to_pos(buffer, buffer_pos_start_of_line(buffer, line + 1));
	buffer_select_to_pos(buffer, buffer_pos_start_of_line(buffer, line));
}

void buffer_select_all(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_pos_start_of_file(buffer));
	buffer_select_to_pos(buffer, buffer_pos_end_of_file(buffer));
}

void buffer_deselect(TextBuffer *buffer) {
	if (buffer->selection) {
		buffer->cursor_pos = buffer->selection_pos;
		buffer->selection = false;
	}
}

void buffer_page_up(TextBuffer *buffer, i64 npages) {
	buffer_scroll(buffer, 0, (double)-npages * buffer_display_lines(buffer));
}

void buffer_page_down(TextBuffer *buffer, i64 npages) {
	buffer_scroll(buffer, 0, (double)+npages * buffer_display_lines(buffer));
}

void buffer_select_page_up(TextBuffer *buffer, i64 npages) {
	buffer_select_up(buffer, npages * (i64)buffer_display_lines(buffer));
}

void buffer_select_page_down(TextBuffer *buffer, i64 npages) {
	buffer_select_down(buffer, npages * (i64)buffer_display_lines(buffer));
}

static void buffer_shorten_line(Line *line, u32 new_len) {
	assert(line->len >= new_len);
	line->len = new_len; // @TODO(optimization,memory): decrease line capacity
}

// returns -1 if c is not a digit of base
static int base_digit(char c, int base) {
	int value = -1;
	if (c >= '0' && c <= '9') {
		value = c - '0';
	} else if (c >= 'a' && c <= 'f') {
		value = c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		value = c - 'A' + 10;
	}
	return value >= base ? -1 : value;
}

// e.g. returns "0x1b"  for num = "0x1a", by = 1
// turns out it's surprisingly difficult to handle all cases
static char *change_number(const char *num, i64 by) {
	/*
	we break up a number like "-0x00ff17u" into 4 parts:
	- negative        = true   -- sign
	- num[..start]    = "0x00" -- base and leading zeroes
	- num[start..end] = "ff17" -- main number
	- num[end..]      = "u"    -- suffix
	*/
	
	if (!isdigit(*num) && *num != '-') {
		return NULL;
	}
	
	bool negative = *num == '-';
	if (negative) ++num;
	
	int start = 0;
	int base = 10;
	if (num[0] == '0') {
		switch (num[1]) {
		case '\0':
			start = 0;
			break;
		case 'x':
		case 'X':
			start = 2;
			base = 16;
			break;
		case 'o':
		case 'O':
			start = 2;
			base = 8;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
			start = 1;
			base = 8;
			break;
		case 'b':
		case 'B':
			start = 2;
			base = 2;
			break;
		default:
			return NULL;
		}
	}
	
	// find end of number
	int end;
	for (end = start + 1; base_digit(num[end], base) != -1; ++end);
	
	int leading_zeroes = 0;
	while (num[start] == '0' && start + 1 < end) {
		++leading_zeroes;
		++start;
	}
	
	if (base_digit(num[end], 16) != -1) {
		// we're probably wrong about the base. let's not do anything.
		return NULL;
	}
	
	if (num[end] != '\0'
		&& !isalnum(num[end]) // number suffixes e.g. 0xffu
		) {
		// probably not a number.
		// at least not one we understand.
		return NULL;
	}
	
	bool uppercase = false;
	for (int i = 0; i < end; ++i)
		if (isupper(num[i]))
			uppercase = true;
	
	char numcpy[128] = {0};
	strn_cpy(numcpy, sizeof numcpy, &num[start], (size_t)(end - start));
	long long number = strtoll(numcpy, NULL, base);
	if (number == LLONG_MIN || number == LLONG_MAX)
		return NULL;
	if (negative) number = -number;
	// overflow checks
	if (by > 0 && number > LLONG_MAX - by)
		return NULL;
	if (by < 0 && number <= LLONG_MIN - by)
		return NULL;
	number += by;
	negative = number < 0;
	if (negative) number = -number;
	
	char new_number[128] = {0};
	switch (base) {
	case 2:
		// aaa sprintf doesnt have binary yet
		str_binary_number(new_number, (u64)number);
		break;
	case 8: sprintf(new_number, "%llo", number); break;
	case 10: sprintf(new_number, "%lld", number); break;
	case 16:
		if (uppercase)
			sprintf(new_number, "%llX", number);
		else
			sprintf(new_number, "%llx", number);
		break;
	}
	
	int digit_diff = (int)strlen(new_number) - (end - start);
	char extra_leading_zeroes[128] = {0};
	if (digit_diff > 0) {
		// e.g. 0x000ff should be incremented to 0x00100
		start -= min_i32(digit_diff, leading_zeroes);
	} else if (digit_diff < 0) {
		if (leading_zeroes) {
			// e.g. 0x00100 should be decremented to 0x000ff
			for (int i = 0; i < -digit_diff && i < (int)sizeof extra_leading_zeroes; ++i) {
				extra_leading_zeroes[i] = '0';
			}
		}
	}
	
	// show the parts of the new number:
	//printf("%s %.*s %s %s %s\n",negative ? "-" : "", start, num, extra_leading_zeroes, new_number, &num[end]);
	return a_sprintf("%s%.*s%s%s%s", negative ? "-" : "", start, num, extra_leading_zeroes, new_number, &num[end]);
	
}

bool buffer_change_number_at_pos(TextBuffer *buffer, BufferPos *ppos, i64 by) {
	bool ret = false;
	BufferPos pos = *ppos;
	
	// move to start of number
	if (is32_alnum(buffer_char_before_pos(buffer, pos))) {
		buffer_pos_move_left_words(buffer, &pos, 1);
	}
	char32_t c = buffer_char_at_pos(buffer, pos);
	if (c >= 127 || !isdigit((char)c)) {
		if (c != '-') {
			// not a number
			return ret;
		}
	}
	
	BufferPos end = pos;
	if (c == '-') {
		buffer_pos_move_right(buffer, &end, 1);
	}
	buffer_pos_move_right_words(buffer, &end, 1);
	if (buffer_char_at_pos(buffer, end) == '.') {
		// floating-point number. dont try to increment it
		return ret;
	}
	
	if (buffer_char_before_pos(buffer, pos) == '-') {
		// include negative sign
		buffer_pos_move_left(buffer, &pos, 1);
	}
	
	if (buffer_char_before_pos(buffer, pos) == '.') {
		// floating-point number. dont try to increment it
		return ret;
	}
	
	u32 nchars = (u32)buffer_pos_diff(buffer, pos, end);
	char *word = buffer_get_utf8_text_at_pos(buffer, pos, nchars);
	char *newnum = change_number(word, by);
	if (newnum) {
		buffer_delete_chars_between(buffer, pos, end);
		buffer_insert_utf8_at_pos(buffer, pos, newnum);
		free(newnum);
		*ppos = pos;
		ret = true;
	}
	free(word);
	return ret;
}

void buffer_change_number_at_cursor(TextBuffer *buffer, i64 by) {
	buffer_start_edit_chain(buffer);
	buffer_change_number_at_pos(buffer, &buffer->cursor_pos, by);
	buffer_end_edit_chain(buffer);
}

// decrease the number of lines in the buffer.
// DOES NOT DO ANYTHING TO THE LINES REMOVED! YOU NEED TO FREE THEM YOURSELF!
static void buffer_shorten(TextBuffer *buffer, u32 new_nlines) {
	buffer->nlines = new_nlines; // @TODO(optimization,memory): decrease lines capacity
}

// delete `nlines` lines starting from index `first_line_idx`
static void buffer_delete_lines(TextBuffer *buffer, u32 first_line_idx, u32 nlines) {
	assert(first_line_idx < buffer->nlines);
	assert(first_line_idx+nlines <= buffer->nlines);
	Line *first_line = &buffer->lines[first_line_idx];
	Line *end = first_line + nlines;

	for (Line *l = first_line; l != end; ++l) {
		buffer_line_free(l);
	}
	memmove(first_line, end, (size_t)(buffer->lines + buffer->nlines - end) * sizeof(Line));
}

void buffer_delete_chars_at_pos(TextBuffer *buffer, BufferPos pos, i64 nchars_) {
	if (buffer->view_only) return;
	if (nchars_ < 0) {
		buffer_error(buffer, "Deleting negative characters (specifically, " I64_FMT ").", nchars_);
		return;
	}
	if (nchars_ <= 0) return;

	if (nchars_ > U32_MAX) nchars_ = U32_MAX;
	u32 nchars = (u32)nchars_;

	buffer_pos_validate(buffer, &pos);

	// Correct nchars in case it goes past the end of the file.
	// Why do we need to correct it?
	// When generating undo events, we allocate nchars characters of memory (see buffer_edit below).
	// Not doing this might also cause other bugs, best to keep it here just in case.
	nchars = (u32)buffer_get_text_at_pos(buffer, pos, NULL, nchars);
	
	if (autocomplete_is_open(buffer->ted)) {
		// close completions if a non-word character is deleted
		bool close_completions = false;
		if (nchars > 256) {
			// this is a massive deletion
			// even if it's all word characters, let's close the completion menu anyways
			close_completions = true;
		} else {
			char32_t text[256];
			size_t n = buffer_get_text_at_pos(buffer, pos, text, nchars);
			(void)n;
			assert(n == nchars);
			for (u32 i = 0; i < nchars; ++i) {
				if (!is32_word(text[i])) {
					close_completions = true;
					break;
				}
			}
		}
		if (close_completions)
			autocomplete_close(buffer->ted);
	}


	LSP *lsp = buffer_lsp(buffer);
	if (lsp)
		buffer_send_lsp_did_change(lsp, buffer, pos, nchars, (String32){0});

	if (buffer->store_undo_events) {
		// we need to make sure the undo history keeps track of the edit.
		// we will either combine it with the previous BufferEdit, or create a new
		// one with just this deletion.
		
		BufferEdit *last_edit = arr_lastp(buffer->undo_history);
		BufferPos edit_start = {0}, edit_end = {0};
		if (last_edit) {
			edit_start = last_edit->pos;
			edit_end = buffer_pos_advance(buffer, edit_start, last_edit->new_len);
		}
		BufferPos del_start = pos, del_end = buffer_pos_advance(buffer, del_start, nchars);

		bool create_new_edit = 
			!last_edit || // if there is no previous edit to combine it with
			buffer_pos_cmp(del_end, edit_start) < 0 || // or if delete does not overlap last_edit
			buffer_pos_cmp(del_start, edit_end) > 0 ||
			buffer_edit_split(buffer, true); // or if enough time has passed to warrant a new edit

		if (create_new_edit) {
			// create a new edit
			buffer_edit(buffer, pos, nchars, 0);
		} else {
			if (buffer_pos_cmp(del_start, edit_start) < 0) {
				// if we delete characters before the last edit, add them onto the start of prev_text.
				i64 chars_before_edit = buffer_pos_diff(buffer, del_start, edit_start);
				assert(chars_before_edit > 0);
				u32 updated_prev_len = (u32)(chars_before_edit + last_edit->prev_len);
				if (buffer_edit_resize_prev_text(buffer, last_edit, updated_prev_len)) {
					// make space
					memmove(last_edit->prev_text + chars_before_edit, last_edit->prev_text, last_edit->prev_len * sizeof(char32_t));
					// prepend these chracters to the edit's text
					buffer_get_text_at_pos(buffer, del_start, last_edit->prev_text, (size_t)chars_before_edit);

					last_edit->prev_len = updated_prev_len;
				}
				// move edit position back, because we started deleting from an earlier point
				last_edit->pos = del_start;
			}
			if (buffer_pos_cmp(del_end, edit_end) > 0) {
				// if we delete characters after the last edit, add them onto the end of prev_text.
				i64 chars_after_edit = buffer_pos_diff(buffer, edit_end, del_end);
				assert(chars_after_edit > 0);
				u32 updated_prev_len = (u32)(chars_after_edit + last_edit->prev_len);
				if (buffer_edit_resize_prev_text(buffer, last_edit, updated_prev_len)) {
					// append these characters to the edit's text
					buffer_get_text_at_pos(buffer, edit_end, last_edit->prev_text + last_edit->prev_len, (size_t)chars_after_edit);
					last_edit->prev_len = updated_prev_len;
				}
			}
			
			// we might have deleted text inside the edit.
			i64 new_text_del_start = buffer_pos_diff(buffer, edit_start, del_start);
			if (new_text_del_start < 0) new_text_del_start = 0;
			i64 new_text_del_end = buffer_pos_diff(buffer, edit_start, del_end);
			if (new_text_del_end > last_edit->new_len) new_text_del_end = last_edit->new_len;
			if (new_text_del_end > new_text_del_start) {
				// shrink length to get rid of that text
				last_edit->new_len -= (u32)(new_text_del_end - new_text_del_start);
			}
		}

	}

	u32 line_idx = pos.line;
	u32 index = pos.index;
	Line *line = &buffer->lines[line_idx], *lines_end = &buffer->lines[buffer->nlines];
	if (nchars + index > line->len) {
		// delete rest of line
		nchars -= line->len - index + 1; // +1 for the newline that got deleted
		buffer_shorten_line(line, index);

		Line *last_line; // last line in lines deleted
		for (last_line = line + 1; last_line < lines_end && nchars > last_line->len; ++last_line) {
			nchars -= last_line->len+1;
		}
		if (last_line == lines_end) {
			assert(nchars == 0); // we already shortened nchars to go no further than the end of the file
			// delete everything to the end of the file
			for (u32 idx = line_idx + 1; idx < buffer->nlines; ++idx) {
				buffer_line_free(&buffer->lines[idx]);
			}
			buffer_shorten(buffer, line_idx + 1);
		} else {
			// join last_line[nchars:] to line.
			u32 last_line_chars_left = (u32)(last_line->len - nchars);
			u32 old_len = line->len;
			if (buffer_line_set_len(buffer, line, old_len + last_line_chars_left)) {
				memcpy(line->str + old_len, last_line->str + nchars, last_line_chars_left * sizeof(char32_t));
			}
			// remove all lines between line + 1 and last_line (inclusive).
			buffer_delete_lines(buffer, line_idx + 1, (u32)(last_line - line));

			u32 lines_removed = (u32)(last_line - line);
			buffer_shorten(buffer, buffer->nlines - lines_removed);
		}
	} else {
		// just delete characters from this line
		buffer_pos_handle_deleted_chars(&buffer->cursor_pos, pos, nchars);
		buffer_pos_handle_deleted_chars(&buffer->selection_pos, pos, nchars);
		memmove(line->str + index, line->str + index + nchars, (size_t)(line->len - (nchars + index)) * sizeof(char32_t));
		line->len -= nchars;
	}

	buffer_remove_last_edit_if_empty(buffer);
	
	// cursor position could have been invalidated by this edit
	buffer_validate_cursor(buffer);

	buffer_lines_modified(buffer, line_idx, line_idx);
	signature_help_retrigger(buffer->ted);
}

// Delete characters between the given buffer positions. Returns number of characters deleted.
i64 buffer_delete_chars_between(TextBuffer *buffer, BufferPos p1, BufferPos p2) {
	buffer_pos_validate(buffer, &p1);
	buffer_pos_validate(buffer, &p2);
	i64 nchars = buffer_pos_diff(buffer, p1, p2);
	if (nchars < 0) {
		// swap positions if p1 comes after p2
		nchars = -nchars;
		BufferPos tmp = p1;
		p1 = p2;
		p2 = tmp;
	}
	buffer_delete_chars_at_pos(buffer, p1, nchars);
	return nchars;
}

// Delete the current buffer selection. Returns the number of characters deleted.
i64 buffer_delete_selection(TextBuffer *buffer) {
	i64 ret = 0;
	if (buffer->selection) {
		ret = buffer_delete_chars_between(buffer, buffer->selection_pos, buffer->cursor_pos);
		buffer_cursor_move_to_pos(buffer, buffer_pos_min(buffer->selection_pos, buffer->cursor_pos)); // move cursor to whichever endpoint comes first
		buffer->selection = false;
	}
	return ret;
}

void buffer_insert_text_at_cursor(TextBuffer *buffer, String32 str) {
	buffer_delete_selection(buffer); // delete any selected text
	BufferPos endpos = buffer_insert_text_at_pos(buffer, buffer->cursor_pos, str);
	buffer_cursor_move_to_pos(buffer, endpos);
	if (str.len) {
		// this actually isn't handled by the move above since buffer->cursor_pos
		// should be moved to endpos anyways. might as well keep that there though
		buffer_scroll_to_cursor(buffer);
	}
}

void buffer_insert_char_at_cursor(TextBuffer *buffer, char32_t c) {
	String32 s = {&c, 1};
	buffer_insert_text_at_cursor(buffer, s);
}


void buffer_insert_utf8_at_pos(TextBuffer *buffer, BufferPos pos, const char *utf8) {
	String32 s32 = str32_from_utf8(utf8);
	if (s32.str) {
		buffer_insert_text_at_pos(buffer, pos, s32);
		str32_free(&s32);
	}
}

void buffer_insert_utf8_at_cursor(TextBuffer *buffer, const char *utf8) {
	String32 s32 = str32_from_utf8(utf8);
	if (s32.str) {
		buffer_insert_text_at_cursor(buffer, s32);
		str32_free(&s32);
	}
}

void buffer_insert_tab_at_cursor(TextBuffer *buffer) {
	if (buffer_indent_with_spaces(buffer)) {
		u16 tab_width = buffer_tab_width(buffer);
		for (int i = 0; i < tab_width; ++i)
			buffer_insert_char_at_cursor(buffer, ' ');
	} else {
		buffer_insert_char_at_cursor(buffer, '\t');
	}
}

// insert newline at cursor and auto-indent
void buffer_newline(TextBuffer *buffer) {
	if (buffer->is_line_buffer) {
		buffer->line_buffer_submitted = true;
		return;
	}
	const Settings *settings = buffer_settings(buffer);
	BufferPos cursor_pos = buffer->cursor_pos;
	String32 line = buffer_get_line(buffer, cursor_pos.line);
	u32 whitespace_len;
	for (whitespace_len = 0; whitespace_len < line.len; ++whitespace_len) {
		if (line.str[whitespace_len] != ' ' && line.str[whitespace_len] != '\t')
			break; // found end of indentation
	}
	if (settings->auto_indent) {
		// newline + auto-indent
		 // @TODO(optimize): don't allocate on heap if whitespace_len is small
		char32_t *text = buffer_calloc(buffer, whitespace_len + 1, sizeof *text);
		if (text) {
			text[0] = '\n';
			memcpy(&text[1], line.str, whitespace_len * sizeof *text);
			buffer_insert_text_at_cursor(buffer, str32(text, whitespace_len + 1));
			free(text);
		}
	} else {
		// just newline
		buffer_insert_char_at_cursor(buffer, '\n');
	}
}

void buffer_delete_chars_at_cursor(TextBuffer *buffer, i64 nchars) {
	if (buffer->selection) {
		buffer_delete_selection(buffer);
	} else {
		BufferPos cursor_pos = buffer->cursor_pos;
		u16 tab_width = buffer_tab_width(buffer);
		bool delete_soft_tab = false;
		if (buffer_indent_with_spaces(buffer)
			&& cursor_pos.index + tab_width <= buffer_line_len(buffer, cursor_pos.line)
			&& cursor_pos.index % tab_width == 0) {
			delete_soft_tab = true;
			// check that all characters deleted + all characters before cursor are ' '
			for (u32 i = 0; i < cursor_pos.index + tab_width; ++i) {
				BufferPos p = {.line = cursor_pos.line, .index = i };
				if (buffer_char_at_pos(buffer, p) != ' ')
					delete_soft_tab = false;
			}
		}
		
		if (delete_soft_tab)
			nchars = tab_width;
		buffer_delete_chars_at_pos(buffer, cursor_pos, nchars);
		
	}
	buffer_scroll_to_cursor(buffer);
}

i64 buffer_backspace_at_pos(TextBuffer *buffer, BufferPos *pos, i64 ntimes) {
	i64 n = buffer_pos_move_left(buffer, pos, ntimes);
	buffer_delete_chars_at_pos(buffer, *pos, n);
	return n;
}

i64 buffer_backspace_at_cursor(TextBuffer *buffer, i64 ntimes) {
	i64 ret=0;
	if (buffer->selection) {
		ret = buffer_delete_selection(buffer);
	} else {
		BufferPos cursor_pos = buffer->cursor_pos;
		// check whether to delete the "soft tab" if indent-with-spaces is enabled
		u16 tab_width = buffer_tab_width(buffer);
		bool delete_soft_tab = false;
		if (buffer_indent_with_spaces(buffer) && cursor_pos.index > 0
			&& cursor_pos.index % tab_width == 0) {
			delete_soft_tab = true;
			// check that all characters before cursor are ' '
			for (u32 i = 0; i + 1 < cursor_pos.index; ++i) {
				BufferPos p = {.line = cursor_pos.line, .index = i };
				if (buffer_char_at_pos(buffer, p) != ' ')
					delete_soft_tab = false;
			}
		}
		if (delete_soft_tab)
			ntimes = tab_width;
		ret = buffer_backspace_at_pos(buffer, &cursor_pos, ntimes);
		buffer_cursor_move_to_pos(buffer, cursor_pos);
	}
	buffer_scroll_to_cursor(buffer);
	return ret;
}

void buffer_delete_words_at_pos(TextBuffer *buffer, BufferPos pos, i64 nwords) {
	BufferPos pos2 = pos;
	buffer_pos_move_right_words(buffer, &pos2, nwords);
	buffer_delete_chars_between(buffer, pos, pos2);
}

void buffer_delete_words_at_cursor(TextBuffer *buffer, i64 nwords) {
	if (buffer->selection)
		buffer_delete_selection(buffer);
	else
		buffer_delete_words_at_pos(buffer, buffer->cursor_pos, nwords);
	buffer_scroll_to_cursor(buffer);
}

void buffer_backspace_words_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	BufferPos pos2 = *pos;
	buffer_pos_move_left_words(buffer, pos, nwords);
	buffer_delete_chars_between(buffer, pos2, *pos);
}

void buffer_backspace_words_at_cursor(TextBuffer *buffer, i64 nwords) {
	if (buffer->selection) {
		buffer_delete_selection(buffer);
	} else {
		BufferPos cursor_pos = buffer->cursor_pos;
		buffer_backspace_words_at_pos(buffer, &cursor_pos, nwords);
		buffer_cursor_move_to_pos(buffer, cursor_pos);
	}
	buffer_scroll_to_cursor(buffer);
}

// puts the inverse edit into `inverse`
static Status buffer_undo_edit(TextBuffer *buffer, BufferEdit const *edit, BufferEdit *inverse) {
	bool success = false;
	bool prev_store_undo_events = buffer->store_undo_events;
	// temporarily disable saving of undo events so we don't add the inverse edit
	// to the undo history
	buffer->store_undo_events = false;

	// create inverse edit
	if (buffer_edit_create(buffer, inverse, edit->pos, edit->new_len, edit->prev_len)) {
		buffer_delete_chars_at_pos(buffer, edit->pos, (i64)edit->new_len);
		String32 str = {edit->prev_text, edit->prev_len};
		buffer_insert_text_at_pos(buffer, edit->pos, str);
		success = true;
	}

	buffer->store_undo_events = prev_store_undo_events;
	return success;
}

static void buffer_cursor_to_edit(TextBuffer *buffer, BufferEdit *edit) {
	buffer->selection = false;
	buffer_cursor_move_to_pos(buffer,
		buffer_pos_advance(buffer, edit->pos, edit->prev_len));
}

// a <-b <-c
// c <-b <-a

void buffer_undo(TextBuffer *buffer, i64 ntimes) {
	bool chain_next = false;
	for (i64 i = 0; i < ntimes; ++i) {
		BufferEdit *edit = arr_lastp(buffer->undo_history);
		if (edit) {
			bool chain = edit->chain;
			BufferEdit inverse = {0};
			if (buffer_undo_edit(buffer, edit, &inverse)) {
				inverse.chain = chain_next;
				if (i == ntimes - 1) {
					// if we're on the last undo, put cursor where edit is
					buffer_cursor_to_edit(buffer, edit);
				}

				buffer_append_redo(buffer, &inverse);
				buffer_edit_free(edit);
				arr_remove_last(buffer->undo_history);
			}
			if (chain) --i;
			chain_next = chain;
		} else break;
	}
}

void buffer_redo(TextBuffer *buffer, i64 ntimes) {
	bool chain_next = false;
	for (i64 i = 0; i < ntimes; ++i) {
		BufferEdit *edit = arr_lastp(buffer->redo_history);
		if (edit) {
			bool chain = edit->chain;
			BufferEdit inverse = {0};
			if (buffer_undo_edit(buffer, edit, &inverse)) {
				inverse.chain = chain_next;
				if (i == ntimes - 1)
					buffer_cursor_to_edit(buffer, edit);
				
				// NOTE: we can't just use buffer_append_edit, because that clears the redo history
				arr_add(buffer->undo_history, inverse);
				if (!buffer->undo_history) buffer_out_of_mem(buffer);

				buffer_edit_free(edit);
				arr_remove_last(buffer->redo_history);
			}
			if (chain) --i;
			chain_next = chain;
		} else break;
	}
}

// if you do:
//  buffer_start_edit_chain(buffer)
//  buffer_insert_text_at_pos(buffer, some position, "text1")
//  buffer_insert_text_at_pos(buffer, another position, "text2")
//  buffer_end_edit_chain(buffer)
// pressing ctrl+z will undo both the insertion of text1 and text2.
void buffer_start_edit_chain(TextBuffer *buffer) {
	assert(!buffer->chaining_edits);
	assert(!buffer->will_chain_edits);
	buffer->will_chain_edits = true;
}

void buffer_end_edit_chain(TextBuffer *buffer) {
	buffer->chaining_edits = buffer->will_chain_edits = false;
}

static void buffer_copy_or_cut(TextBuffer *buffer, bool cut) {
	if (buffer->selection) {
		BufferPos pos1 = buffer_pos_min(buffer->selection_pos, buffer->cursor_pos);
		BufferPos pos2 = buffer_pos_max(buffer->selection_pos, buffer->cursor_pos);
		i64 selection_len = buffer_pos_diff(buffer, pos1, pos2);
		char *text = buffer_get_utf8_text_at_pos(buffer, pos1, (size_t)selection_len);
		if (text) {
			int err = SDL_SetClipboardText(text);
			free(text);
			if (err < 0) {
				buffer_error(buffer, "Couldn't set clipboard contents: %s", SDL_GetError());
			} else {
				// text copied successfully
				if (cut) {
					buffer_delete_selection(buffer);
				}
			}
		}
	}
}

void buffer_copy(TextBuffer *buffer) {
	buffer_copy_or_cut(buffer, false);
}

void buffer_cut(TextBuffer *buffer) {
	buffer_copy_or_cut(buffer, true);
}

void buffer_paste(TextBuffer *buffer) {
	if (SDL_HasClipboardText()) {
		char *text = SDL_GetClipboardText();
		if (text) {
			String32 str = str32_from_utf8(text);
			if (str.len) {
				buffer_start_edit_chain(buffer);
				buffer_insert_text_at_cursor(buffer, str);
				buffer_end_edit_chain(buffer);
				str32_free(&str);
			}
			SDL_free(text);
		}
	}
}

// if an error occurs, buffer is left untouched (except for the error field) and the function returns false.
Status buffer_load_file(TextBuffer *buffer, const char *path) {
	if (!path || !path_is_absolute(path)) {
		buffer_error(buffer, "Loaded path '%s' is not an absolute path.", path);
		return false;
	}
	
	// it's important we do this first, since someone might write to the file while we're reading it,
	// and we want to detect that in buffer_externally_changed
	double modified_time = timespec_to_seconds(time_last_modified(path));
	
	FILE *fp = fopen(path, "rb");
	bool success = true;
	Line *lines = NULL;
	u32 nlines = 0, lines_capacity = 0;
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long file_pos = ftell(fp);
		size_t file_size = (size_t)file_pos;
		fseek(fp, 0, SEEK_SET);
		const Settings *default_settings = buffer->ted->default_settings;
		u32 max_file_size_editable = default_settings->max_file_size;
		u32 max_file_size_view_only = default_settings->max_file_size_view_only;
		
		if (file_pos == -1 || file_pos == LONG_MAX) {
			buffer_error(buffer, "Couldn't get file position. There is something wrong with the file '%s'.", path);
			success = false;
		} else if (file_size > max_file_size_editable && file_size > max_file_size_view_only) {
			buffer_error(buffer, "File too big (size: %zu).", file_size);
			success = false;
		} else {
			u8 *file_contents = buffer_calloc(buffer, 1, file_size + 2);
			lines_capacity = 4;
			lines = buffer_calloc(buffer, lines_capacity, sizeof *buffer->lines); // initial lines
			nlines = 1;
			size_t bytes_read = fread(file_contents, 1, file_size, fp);
			if (bytes_read == file_size) {
				// append a newline if there's no newline
				if (file_contents[file_size - 1] != '\n') {
					file_contents[file_size] = '\n';
					++file_size;
				}
				
				char32_t c = 0;
				for (u8 *p = file_contents, *end = p + file_size; p != end; ) {
					if (*p == '\r' && p != end-1 && p[1] == '\n') {
						// CRLF line endings
						p += 2;
						c = '\n';
					} else {
						size_t n = unicode_utf8_to_utf32(&c, (char *)p, (size_t)(end - p));
						if (n == 0) {
							buffer_error(buffer, "Null character in file (position: %td).", p - file_contents);
							success = false;
							break;
						} else if (n >= (size_t)(-2)) {
							// invalid UTF-8
							buffer_error(buffer, "Invalid UTF-8 (position: %td).", p - file_contents);
							success = false;
							break;
						} else {
							p += n;
						}
					}
					
					if (c == '\n') {
						if (buffer_lines_set_min_capacity(buffer, &lines, &lines_capacity, nlines + 1))
							++nlines;
					} else {
						u32 line_idx = nlines - 1;
						Line *line = &lines[line_idx];
						buffer_line_append_char(buffer, line, c);
					}
				}
			} else {
				buffer_error(buffer, "Error reading from file.");
				success = false;
			}
			if (!success) {
				// something went wrong; we need to free all the memory we used
				for (u32 i = 0; i < nlines; ++i)
					buffer_line_free(&lines[i]);
				free(lines);
			}
				
			if (success) {
				char *path_copy = buffer_strdup(buffer, path);
				if (!path_copy) success = false;
				#if _WIN32
				// only use \ as a path separator
				for (char *p = path_copy; *p; ++p)
					if (*p == '/')
						*p = '\\';
				#endif
				if (success) {
					// everything is good
					buffer_clear(buffer);
					buffer->settings_idx = -1;
					buffer->lines = lines;
					buffer->nlines = nlines;
					buffer->frame_earliest_line_modified = 0;
					buffer->frame_latest_line_modified = nlines - 1;
					buffer->lines_capacity = lines_capacity;
					buffer->path = path_copy;
					buffer->last_write_time = modified_time;
					if (!(fs_path_permission(path) & FS_PERMISSION_WRITE)) {
						// can't write to this file; make the buffer view only.
						buffer->view_only = true;
					}
					
					if (file_size > max_file_size_editable) {
						// file very large; open in view-only mode.
						buffer->view_only = true;
					}
					
					// this will send a didOpen request if needed
					buffer_lsp(buffer);
				}
				
			}
			
			free(file_contents);
		}
		fclose(fp);
	} else {
		buffer_error(buffer, "Couldn't open file %s: %s.", path, strerror(errno));
		success = false;
	}
	return success;
}

void buffer_reload(TextBuffer *buffer) {
	if (buffer_is_named_file(buffer)) {
		BufferPos cursor_pos = buffer->cursor_pos;
		float x1 = buffer->x1, y1 = buffer->y1, x2 = buffer->x2, y2 = buffer->y2;
		double scroll_x = buffer->scroll_x; double scroll_y = buffer->scroll_y;
		char *path = str_dup(buffer->path);
		if (buffer_load_file(buffer, path)) {
			buffer->x1 = x1; buffer->y1 = y1; buffer->x2 = x2; buffer->y2 = y2;
			buffer->cursor_pos = cursor_pos;
			buffer->scroll_x = scroll_x;
			buffer->scroll_y = scroll_y;
			buffer_validate_cursor(buffer);
			buffer_correct_scroll(buffer);
		}
		free(path);
	}
}

bool buffer_externally_changed(TextBuffer *buffer) {
	if (!buffer_is_named_file(buffer))
		return false;
	return buffer->last_write_time != timespec_to_seconds(time_last_modified(buffer->path));
}

void buffer_new_file(TextBuffer *buffer, const char *path) {
	if (path && !path_is_absolute(path)) {
		buffer_error(buffer, "Cannot create %s: path is not absolute", path);
		return;
	}
	
	buffer_clear(buffer);

	if (path)
		buffer->path = buffer_strdup(buffer, path);
	buffer->lines_capacity = 4;
	buffer->lines = buffer_calloc(buffer, buffer->lines_capacity, sizeof *buffer->lines);
	buffer->nlines = 1;
}

static bool buffer_write_to_file(TextBuffer *buffer, const char *path) {
	const Settings *settings = buffer_settings(buffer);
	FILE *out = fopen(path, "wb");
	if (!out) {
		buffer_error(buffer, "Couldn't open file %s for writing: %s.", path, strerror(errno));
		return false;
	}
	if (settings->auto_add_newline) {
		Line *last_line = &buffer->lines[buffer->nlines - 1];
		if (last_line->len) {
			// if the last line isn't empty, add a newline to the end of the file
			char32_t c = '\n';
			String32 s = {&c, 1};
			buffer_insert_text_at_pos(buffer, buffer_pos_end_of_file(buffer), s);
		}
	}
	
	bool success = true;
	
	for (u32 i = 0; i < buffer->nlines; ++i) {
		Line *line = &buffer->lines[i];
		for (char32_t *p = line->str, *p_end = p + line->len; p != p_end; ++p) {
			char utf8[4] = {0};
			size_t bytes = unicode_utf32_to_utf8(utf8, *p);
			if (bytes != (size_t)-1) {
				if (fwrite(utf8, 1, bytes, out) != bytes) {
					buffer_error(buffer, "Couldn't write to %s.", path);
					success = false;
				}
			}
		}

		if (i != buffer->nlines-1) {
		#if _WIN32
			if (settings->crlf_windows)
				putc('\r', out);
		#endif
			putc('\n', out);
		}
	}
	
	if (ferror(out)) {
		if (!buffer_has_error(buffer))
			buffer_error(buffer, "Couldn't write to %s.", path);
		success = false;
	}
	if (fclose(out) != 0) {
		if (!buffer_has_error(buffer))
			buffer_error(buffer, "Couldn't close file %s.", path);
		success = false;
	}
	
	return success;
}

bool buffer_save(TextBuffer *buffer) {
	const Settings *settings = buffer_settings(buffer);
	
	if (!buffer_is_named_file(buffer)) {
		// user tried to save line buffer. whatever
		return true;
	}
	if (buffer->view_only) {
		buffer_error(buffer, "Can't save view-only file.");
		return false;
	}
	
	char backup_path[TED_PATH_MAX+10];
	*backup_path = '\0';
	
	if (settings->save_backup) {
		strbuf_printf(backup_path, "%s.ted-bk", buffer->path);
		if (strlen(backup_path) != strlen(buffer->path) + 7) {
			buffer_error(buffer, "File name too long.");
			return false;
		}
		
	}
	
	bool success = true;
	if (*backup_path)
		success &= buffer_write_to_file(buffer, backup_path);
	if (success)
		success &= buffer_write_to_file(buffer, buffer->path);
	if (success && *backup_path)
		remove(backup_path);
	buffer->last_write_time = timespec_to_seconds(time_last_modified(buffer->path));
	if (success) {
		buffer->undo_history_write_pos = arr_len(buffer->undo_history);
		if (buffer->path && str_has_suffix(path_filename(buffer->path), "ted.cfg")
			&& buffer_settings(buffer)->auto_reload_config) {
			ted_reload_configs(buffer->ted);
		}
	}
	return success;
}

bool buffer_save_as(TextBuffer *buffer, const char *new_path) {
	if (!path_is_absolute(new_path)) {
		assert(0);
		buffer_error(buffer, "New path %s is not absolute.", new_path);
		return false;
	}
	
	LSP *lsp = buffer_lsp(buffer);
	char *prev_path = buffer->path;
	buffer->path = buffer_strdup(buffer, new_path);
	buffer->settings_idx = -1; // we might have new settings
	
	if (buffer->path && buffer_save(buffer)) {
		buffer->view_only = false;
		// ensure whole file is re-highlighted when saving with a different
		//  file extension
		buffer->frame_earliest_line_modified = 0;
		buffer->frame_latest_line_modified = buffer->nlines - 1;
		if (lsp)
			buffer_send_lsp_did_close(buffer, lsp, prev_path);
		buffer->last_lsp_check = -INFINITY;
		// we'll send a didOpen the next time buffer_lsp is called.
		free(prev_path);
		return true;
	} else {
		free(buffer->path);
		buffer->path = prev_path;
		return false;
	}
}

u32 buffer_first_rendered_line(TextBuffer *buffer) {
	return (u32)buffer->scroll_y;
}

u32 buffer_last_rendered_line(TextBuffer *buffer) {
	u32 line = buffer_first_rendered_line(buffer) + (u32)buffer_display_lines(buffer) + 1;
	return clamp_u32(line, 0, buffer->nlines);
}

void buffer_goto_word_at_cursor(TextBuffer *buffer, GotoType type) {
	char *word = buffer_word_at_cursor_utf8(buffer);
	if (*word) {
		LSPDocumentPosition pos = buffer_pos_to_lsp_document_position(buffer, buffer->cursor_pos);
		definition_goto(buffer->ted, buffer_lsp(buffer), word, pos, type);
	}
	free(word);
}

// returns true if the buffer "used" this event
bool buffer_handle_click(Ted *ted, TextBuffer *buffer, vec2 click, u8 times) {
	BufferPos buffer_pos;
	if (autocomplete_is_open(ted)) {
		if (autocomplete_box_contains_point(ted, click))
			return false; // don't look at clicks in the autocomplete menu
		else
			autocomplete_close(ted); // close autocomplete menu if user clicks outside of it
	}
	if (buffer_pixels_to_pos(buffer, click, &buffer_pos)) {
		// user clicked on buffer
		if (!ted->menu || buffer->is_line_buffer) {
			ted_switch_to_buffer(ted, buffer);
		}
		if (buffer == ted->active_buffer) {
			switch (ted_get_key_modifier(ted)) {
			case KEY_MODIFIER_SHIFT:
				// select to position
				buffer_select_to_pos(buffer, buffer_pos);
				break;
			case KEY_MODIFIER_CTRL:
			case KEY_MODIFIER_CTRL | KEY_MODIFIER_SHIFT:
			case KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT:
				if (!buffer->is_line_buffer) {
					// go to definition/declaration
					buffer_cursor_move_to_pos(buffer, buffer_pos);
					GotoType type = GOTO_DEFINITION;
					if (ted_is_shift_down(ted))
						type = GOTO_DECLARATION;
					else if (ted_is_alt_down(ted))
						type = GOTO_TYPE_DEFINITION;
					buffer_goto_word_at_cursor(buffer, type);
				}
				break;
			case 0:
				buffer_cursor_move_to_pos(buffer, buffer_pos);
				switch ((times - 1) % 3) {
				case 0: break; // single-click
				case 1: // double-click: select word
					buffer_select_word(buffer);
					break;
				case 2: // triple-click: select line
					buffer_select_line(buffer);
					break;
				}
				ted->drag_buffer = buffer;
				break;
			}
			return true;
		}
	}
	return false;
}

char32_t buffer_pos_move_to_matching_bracket(TextBuffer *buffer, BufferPos *pos) {
	Language language = buffer_language(buffer);
	char32_t bracket_char = buffer_char_at_pos(buffer, *pos);
	char32_t matching_char = syntax_matching_bracket(language, bracket_char);
	if (bracket_char && matching_char) {
		int direction = syntax_is_opening_bracket(language, bracket_char) ? +1 : -1;
		int depth = 1;
		bool found_bracket = false;
		while (buffer_pos_move_right(buffer, pos, direction)) {
			char32_t c = buffer_char_at_pos(buffer, *pos);
			if (c == bracket_char) depth += 1;
			else if (c == matching_char) depth -= 1;
			if (depth == 0) {
				found_bracket = true;
				break;
			}
		}
		if (found_bracket)
			return matching_char;
	}
	return 0;
}

bool buffer_cursor_move_to_matching_bracket(TextBuffer *buffer) {
	// it's more natural here to consider the character to the left of the cursor
	BufferPos pos = buffer->cursor_pos;
	if (pos.index == 0) return false;
	buffer_pos_move_left(buffer, &pos, 1);
	if (buffer_pos_move_to_matching_bracket(buffer, &pos)) {
		buffer_pos_move_right(buffer, &pos, 1);
		buffer_cursor_move_to_pos(buffer, pos);
		return true;
	}
	return false;
}

// Render the text buffer in the given rectangle
void buffer_render(TextBuffer *buffer, Rect r) {
	const Settings *settings = buffer_settings(buffer);
	
	buffer_lsp(buffer); // this will send didOpen/didClose if the buffer's LSP changed
	
	if (r.size.x < 1 || r.size.y < 1) {
		// rectangle less than 1 pixel
		// set x1,y1,x2,y2 to an size 0 rectangle
		buffer->x1 = buffer->x2 = r.pos.x;
		buffer->y1 = buffer->y2 = r.pos.y;
		return;
	}
	
	float x1, y1, x2, y2;
	rect_coords(r, &x1, &y1, &x2, &y2);
	// Correct the scroll, because the window size might have changed
	buffer_correct_scroll(buffer);
	
	Font *font = buffer_font(buffer);
	u32 nlines = buffer->nlines;
	Line *lines = buffer->lines;
	float char_height = text_font_char_height(font);

	Ted *ted = buffer->ted;
	const u32 *colors = settings->colors;
	const float padding = settings->padding;
	const float border_thickness = settings->border_thickness;
	
	u32 start_line = buffer_first_rendered_line(buffer); // line to start rendering from
	
	
	u32 border_color = colors[COLOR_BORDER]; // color of border around buffer
	// bounding box around buffer
	gl_geometry_rect_border(rect4(x1, y1, x2, y2), border_thickness, border_color);
	
	x1 += border_thickness;
	y1 += border_thickness;
	x2 -= border_thickness;
	y2 -= border_thickness;
	
	
	float render_start_y = y1 - (float)(buffer->scroll_y - start_line) * char_height; // where the 1st line is rendered


	// line numbering
	if (!buffer->is_line_buffer && settings->line_numbers) {
		float max_digit_width = 0;
		for (char32_t digit = '0'; digit <= '9'; ++digit) {
			max_digit_width = maxf(max_digit_width, text_font_char_width(font, digit));
		}
		
		float line_number_width = ndigits_u64(buffer->nlines) * max_digit_width + padding;

		TextRenderState text_state = text_render_state_default;
		text_state.min_x = x1;
		text_state.max_x = x2;
		text_state.min_y = y1;
		text_state.max_y = y2;

		float y = render_start_y;
		u32 cursor_line = buffer->cursor_pos.line;
		for (u32 line = start_line; line < nlines; ++line) {
			char str[32] = {0};
			strbuf_printf(str, U32_FMT, line + 1); // convert line number to string
			float x = x1 + line_number_width - text_get_size_vec2(font, str).x; // right justify
			// set color
			rgba_u32_to_floats(colors[line == cursor_line ? COLOR_CURSOR_LINE_NUMBER : COLOR_LINE_NUMBERS],
				text_state.color);
			text_state.x = x; text_state.y = y;
			text_state_break_kerning(&text_state);
			text_utf8_with_state(font, &text_state, str);
			y += char_height;
			if (y > y2) break;
		}

		x1 += line_number_width;
		x1 += 2; // a little bit of padding
		// line separating line numbers from text
		gl_geometry_rect(rect(Vec2(x1, y1), Vec2(border_thickness, y2 - y1)), colors[COLOR_LINE_NUMBERS_SEPARATOR]);
		x1 += border_thickness;
	}

	if (x2 < x1) x2 = x1;
	if (y2 < y1) y2 = y1;
	buffer->x1 = x1; buffer->y1 = y1; buffer->x2 = x2; buffer->y2 = y2;
	if (x1 == x2 || y1 == y2) return;

	if (buffer->is_line_buffer) {
		// handle clicks
		// this is only done for line buffers, so that ctrl+click works properly (and happens in one frame).
		arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], MouseClick, click) {
			buffer_handle_click(ted, buffer, click->pos, click->times);
		}
	}

	// change cursor to ibeam when it's hovering over the buffer
	if ((!ted->menu || buffer == &ted->line_buffer) && rect_contains_point(rect4(x1, y1, x2, y2), ted->mouse_pos)) {
		ted->cursor = ted->cursor_ibeam;
	}

	
	if (buffer->center_cursor_next_frame) {
		buffer_center_cursor(buffer);
		buffer->center_cursor_next_frame = false;
	}

	if (rect_contains_point(rect4(x1, y1, x2, y2), ted->mouse_pos) && !ted->menu) {
		// scroll with mouse wheel
		double scroll_speed = 2.5;
		buffer_scroll(buffer, ted->scroll_total_x * scroll_speed, ted->scroll_total_y * scroll_speed);
	}

	// get screen coordinates of cursor
	vec2 cursor_display_pos = buffer_pos_to_pixels(buffer, buffer->cursor_pos);
	// the rectangle that the cursor is rendered as
	Rect cursor_rect = rect(cursor_display_pos, Vec2(settings->cursor_width, char_height));

	if (!buffer->is_line_buffer) { // highlight line cursor is on
		Rect hl_rect = rect(Vec2(x1, cursor_display_pos.y), Vec2(x2-x1-1, char_height));
		buffer_clip_rect(buffer, &hl_rect);
		gl_geometry_rect(hl_rect, colors[COLOR_CURSOR_LINE_BG]);
	}

	
	// what x coordinate to start rendering the text from
	double render_start_x = x1 - (double)buffer->scroll_x * text_font_char_width(font, ' ');

	if (buffer->selection) { // draw selection
		BufferPos sel_start = {0}, sel_end = {0};
		int cmp = buffer_pos_cmp(buffer->cursor_pos, buffer->selection_pos);
		if (cmp < 0) {
			// cursor_pos comes first
			sel_start = buffer->cursor_pos;
			sel_end   = buffer->selection_pos;
		} else if (cmp > 0) {
			// selection_pos comes first
			sel_end   = buffer->cursor_pos;
			sel_start = buffer->selection_pos;
		} else assert(0);

		for (u32 line_idx = max_u32(sel_start.line, start_line); line_idx <= sel_end.line; ++line_idx) {
			Line *line = &buffer->lines[line_idx];
			u32 index1 = line_idx == sel_start.line ? sel_start.index : 0;
			u32 index2 = line_idx == sel_end.line ? sel_end.index : line->len;
			assert(index2 >= index1);

			// highlight everything from index1 to index2
			double highlight_width = buffer_index_to_xoff(buffer, line_idx, index2)
				- buffer_index_to_xoff(buffer, line_idx, index1);
			if (line_idx != sel_end.line) {
				highlight_width += text_font_char_width(font, ' '); // highlight the newline (otherwise empty higlighted lines wouldn't be highlighted at all).
			}

			if (highlight_width > 0) {
				BufferPos p1 = {.line = line_idx, .index = index1};
				vec2 hl_p1 = buffer_pos_to_pixels(buffer, p1);
				Rect hl_rect = rect(
					hl_p1,
					Vec2((float)highlight_width, char_height)
				);
				buffer_clip_rect(buffer, &hl_rect);
				gl_geometry_rect(hl_rect, colors[buffer->view_only ? COLOR_VIEW_ONLY_SELECTION_BG : COLOR_SELECTION_BG]);
			}
			index1 = 0;
		}
	}
	gl_geometry_draw();

	Language language = buffer_language(buffer);
	// dynamic array of character types, to be filled by syntax_highlight
	SyntaxCharType *char_types = NULL;
	bool syntax_highlighting = language && language != LANG_TEXT && settings->syntax_highlighting;

	if (buffer->frame_latest_line_modified >= buffer->frame_earliest_line_modified
		&& syntax_highlighting) {
		// update syntax cache
		if (buffer->frame_latest_line_modified >= buffer->nlines)
			buffer->frame_latest_line_modified = buffer->nlines - 1;
		Line *earliest = &buffer->lines[buffer->frame_earliest_line_modified];
		Line *latest = &buffer->lines[buffer->frame_latest_line_modified];
		Line *buffer_last_line = &buffer->lines[buffer->nlines - 1];
		Line *start = earliest == buffer->lines ? earliest : earliest - 1;

		for (Line *line = start; line != buffer_last_line; ++line) {
			SyntaxState syntax = line->syntax;
			syntax_highlight(&syntax, language, line->str, line->len, NULL);
			if (line > latest && line[1].syntax == syntax) {
				// no further necessary changes to the cache
				break;
			} else {
				line[1].syntax = syntax;
			}
		}
	}
	buffer->frame_earliest_line_modified = U32_MAX;
	buffer->frame_latest_line_modified = 0;


	TextRenderState text_state = text_render_state_default;
	text_state.x = 0;
	// we need to start at x = 0 to be consistent with
	// buffer_index_to_xoff and the like (because tabs are difficult).
	text_state.x_render_offset = (float)render_start_x;
	text_state.y = render_start_y;
	text_state.min_x = x1;
	text_state.min_y = y1;
	text_state.max_x = x2;
	text_state.max_y = y2;
	if (!syntax_highlighting)
		rgba_u32_to_floats(colors[COLOR_TEXT], text_state.color);

	buffer->first_line_on_screen = start_line;
	buffer->last_line_on_screen = 0;
	for (u32 line_idx = start_line; line_idx < nlines; ++line_idx) {
		Line *line = &lines[line_idx];
		if (arr_len(char_types) < line->len) {
			arr_set_len(char_types, line->len);
		}
		if (syntax_highlighting) {
			SyntaxState syntax_state = line->syntax;
			syntax_highlight(&syntax_state, language, line->str, line->len, char_types);
		}
		for (u32 i = 0; i < line->len; ++i) {
			char32_t c = line->str[i];
			if (syntax_highlighting) {
				SyntaxCharType type = char_types[i];
				ColorSetting color = syntax_char_type_to_color_setting(type);
				rgba_u32_to_floats(colors[color], text_state.color);
			}
			buffer_render_char(buffer, font, &text_state, c);
		}

		// next line
		text_state_break_kerning(&text_state);
		text_state.x = 0;
		if (text_state.y > text_state.max_y) {
			buffer->last_line_on_screen = line_idx;
			// made it to the bottom of the buffer view.
			break;
		}
		text_state.y += text_font_char_height(font);
	}
	if (buffer->last_line_on_screen == 0) buffer->last_line_on_screen = nlines - 1;
	
	arr_free(char_types);

	text_render(font);

	if (ted->active_buffer == buffer) {
		
		// highlight matching brackets
		BufferPos pos = buffer->cursor_pos;
		if (pos.index > 0) {
			// it's more natural to consider the bracket to the left of the cursor
			buffer_pos_move_left(buffer, &pos, 1);
			char32_t c = buffer_pos_move_to_matching_bracket(buffer, &pos);
			if (c) {
				vec2 gl_pos = buffer_pos_to_pixels(buffer, pos);
				Rect hl_rect = rect(gl_pos, Vec2(text_font_char_width(font, c), char_height));
				if (buffer_clip_rect(buffer, &hl_rect)) {
					gl_geometry_rect(hl_rect, colors[COLOR_MATCHING_BRACKET_HL]);
				}
			}
		}
		
		// render cursor
		float time_on = settings->cursor_blink_time_on;
		float time_off = settings->cursor_blink_time_off;
		double error_animation_duration = 1.0;
		double error_animation_dt = ted->frame_time - ted->cursor_error_time;
		bool error_animation = ted->cursor_error_time > 0 && error_animation_dt < error_animation_duration;
		
		bool is_on = true;
		if (!error_animation && time_off > 0) {
			double absolute_time = ted->frame_time;
			float period = time_on + time_off;
			// time in period
			double t = fmod(absolute_time, period);
			is_on = t < time_on; // are we in the "on" part of the period?
		}
		
		if (is_on) {
			if (buffer_clip_rect(buffer, &cursor_rect)) {
				// draw cursor
				u32 color = colors[COLOR_CURSOR];
				if (buffer->view_only)
					color = colors[COLOR_VIEW_ONLY_CURSOR];
				if (error_animation) {
					color = color_interpolate(maxf(0, 2 * ((float)(error_animation_dt / error_animation_duration) - 0.5f)), colors[COLOR_CURSOR_ERROR], colors[COLOR_CURSOR]);
				}
				
				gl_geometry_rect(cursor_rect, color);
			}
		}
		gl_geometry_draw();
	}

}

void buffer_indent_lines(TextBuffer *buffer, u32 first_line, u32 last_line) {
	assert(first_line <= last_line);
	
	buffer_start_edit_chain(buffer);
	for (u32 l = first_line; l <= last_line; ++l) {
		BufferPos pos = {.line = l, .index = 0};
		if (buffer_indent_with_spaces(buffer)) {
			u16 tab_width = buffer_tab_width(buffer);
			for (int i = 0; i < tab_width; ++i)
				buffer_insert_char_at_pos(buffer, pos, ' ');
		} else {
			buffer_insert_char_at_pos(buffer, pos, '\t');
		}
	}
	buffer_end_edit_chain(buffer);
}

void buffer_dedent_lines(TextBuffer *buffer, u32 first_line, u32 last_line) {
	assert(first_line <= last_line);
	buffer_validate_line(buffer, &first_line);
	buffer_validate_line(buffer, &last_line);
	
	buffer_start_edit_chain(buffer);
	const u8 tab_width = buffer_tab_width(buffer);
	
	for (u32 line_idx = first_line; line_idx <= last_line; ++line_idx) {
		Line *line = &buffer->lines[line_idx];
		if (line->len) {
			u32 chars_to_delete = 0;
			if (line->str[0] == '\t') {
				chars_to_delete = 1;
			} else {
				u32 i;
				for (i = 0; i < line->len && i < tab_width; ++i) {
					char32_t c = line->str[i];
					if (c == '\t' || !is32_space(c))
						break;
				}
				chars_to_delete = i;
			}
			if (chars_to_delete) {
				BufferPos pos = {.line = line_idx, .index = 0};
				buffer_delete_chars_at_pos(buffer, pos, chars_to_delete);
			}
		}
	}
	buffer_end_edit_chain(buffer);
}


void buffer_indent_selection(TextBuffer *buffer) {
	if (!buffer->selection) return;
	u32 l1 = buffer->cursor_pos.line;
	u32 l2 = buffer->selection_pos.line;
	sort2_u32(&l1, &l2); // ensure l1 <= l2
	buffer_indent_lines(buffer, l1, l2);
}

void buffer_dedent_selection(TextBuffer *buffer) {
	if (!buffer->selection) return;
	u32 l1 = buffer->cursor_pos.line;
	u32 l2 = buffer->selection_pos.line;
	sort2_u32(&l1, &l2); // ensure l1 <= l2
	buffer_dedent_lines(buffer, l1, l2);
}

void buffer_indent_cursor_line(TextBuffer *buffer) {
	u32 line = buffer->cursor_pos.line;
	buffer_indent_lines(buffer, line, line);
}
void buffer_dedent_cursor_line(TextBuffer *buffer) {
	u32 line = buffer->cursor_pos.line;
	buffer_dedent_lines(buffer, line, line);
}


void buffer_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line) {
	Settings *settings = buffer_settings(buffer);
	const char *start = settings->comment_start, *end = settings->comment_end;
	if (!start[0] && !end[0])
		return;
	String32 start32 = str32_from_utf8(start), end32 = str32_from_utf8(end);
	
	buffer_start_edit_chain(buffer);
	
	for (u32 line_idx = first_line; line_idx <= last_line; ++line_idx) {
		// insert comment start
		if (start32.len) {
			BufferPos sol = buffer_pos_start_of_line(buffer, line_idx);
			buffer_insert_text_at_pos(buffer, sol, start32);
		}
		// insert comment end
		if (end32.len) {
			BufferPos eol = buffer_pos_end_of_line(buffer, line_idx);
			buffer_insert_text_at_pos(buffer, eol, end32);
		}
	}
	
	str32_free(&start32);
	str32_free(&end32);
	
	buffer_end_edit_chain(buffer);
}

static bool buffer_line_starts_with_ascii(TextBuffer *buffer, u32 line_idx, const char *prefix) {
	buffer_validate_line(buffer, &line_idx);
	Line *line = &buffer->lines[line_idx];
	size_t prefix_len = strlen(prefix);
	if (line->len < prefix_len)
		return false;
	for (size_t i = 0; i < prefix_len; ++i) {
		assert(prefix[i] > 0 && prefix[i] <= 127); // check if actually ASCII
		if ((char32_t)prefix[i] != line->str[i])
			return false;
	}
	return true;
}
static bool buffer_line_ends_with_ascii(TextBuffer *buffer, u32 line_idx, const char *suffix) {
	buffer_validate_line(buffer, &line_idx);
	Line *line = &buffer->lines[line_idx];
	size_t suffix_len = strlen(suffix), line_len = line->len;
	if (line_len < suffix_len)
		return false;
	for (size_t i = 0; i < suffix_len; ++i) {
		assert(suffix[i] > 0 && suffix[i] <= 127); // check if actually ASCII
		if ((char32_t)suffix[i] != line->str[line_len-suffix_len+i])
			return false;
	}
	return true;
}

void buffer_uncomment_lines(TextBuffer *buffer, u32 first_line, u32 last_line) {
	Settings *settings = buffer_settings(buffer);
	const char *start = settings->comment_start, *end = settings->comment_end;
	if (!start[0] && !end[0])
		return;
	u32 start_len = (u32)strlen(start), end_len = (u32)strlen(end);
	buffer_start_edit_chain(buffer);
	for (u32 line_idx = first_line; line_idx <= last_line; ++line_idx) {
		// make sure line is actually commented
		if (buffer_line_starts_with_ascii(buffer, line_idx, start)
			&& buffer_line_ends_with_ascii(buffer, line_idx, end)) {
			// we should do the end first, because start and end might be overlapping,
			// and it would cause an underflow if we deleted the start first.
			BufferPos end_pos = buffer_pos_end_of_line(buffer, line_idx);
			end_pos.index -= end_len;
			buffer_delete_chars_at_pos(buffer, end_pos, end_len);
			
			BufferPos start_pos = buffer_pos_start_of_line(buffer, line_idx);
			buffer_delete_chars_at_pos(buffer, start_pos, start_len);
		}
	}
	buffer_end_edit_chain(buffer);
}

void buffer_toggle_comment_lines(TextBuffer *buffer, u32 first_line, u32 last_line) {
	Settings *settings = buffer_settings(buffer);
	const char *start = settings->comment_start, *end = settings->comment_end;
	if (!start[0] && !end[0])
		return;
	// if first line is a comment, uncomment lines, otherwise, comment lines
	if (buffer_line_starts_with_ascii(buffer, first_line, start)
		&& buffer_line_ends_with_ascii(buffer, first_line, end))
		buffer_uncomment_lines(buffer, first_line, last_line);
	else
		buffer_comment_lines(buffer, first_line, last_line);
}

void buffer_toggle_comment_selection(TextBuffer *buffer) {
	u32 l1, l2;
	if (buffer->selection) {
		l1 = buffer->cursor_pos.line;
		l2 = buffer->selection_pos.line;
		sort2_u32(&l1, &l2); // ensure l1 <= l2
	} else {
		l1 = l2 = buffer->cursor_pos.line;
	}
	buffer_toggle_comment_lines(buffer, l1, l2);
}


void buffer_highlight_lsp_range(TextBuffer *buffer, LSPRange range, ColorSetting color) {
	Font *font = buffer_font(buffer);
	const u32 *colors = buffer_settings(buffer)->colors;
	const float char_height = text_font_char_height(font);
	BufferPos range_start = buffer_pos_from_lsp(buffer, range.start);
	BufferPos range_end = buffer_pos_from_lsp(buffer, range.end);
	// draw the highlight
	if (range_start.line == range_end.line) {
		vec2 a = buffer_pos_to_pixels(buffer, range_start);
		vec2 b = buffer_pos_to_pixels(buffer, range_end);
		b.y += char_height;
		Rect r = rect_endpoints(a, b); buffer_clip_rect(buffer, &r);
		gl_geometry_rect(r, colors[color]);
	} else if (range_end.line - range_start.line < 1000) { // prevent gigantic highlights from slowing things down
		// multiple lines.
		vec2 a = buffer_pos_to_pixels(buffer, range_start);
		vec2 b = buffer_pos_to_pixels(buffer, buffer_pos_end_of_line(buffer, range_start.line));
		b.y += char_height;
		Rect r1 = rect_endpoints(a, b); buffer_clip_rect(buffer, &r1);
		gl_geometry_rect(r1, colors[COLOR_HOVER_HL]);
		
		for (u32 line = range_start.line + 1; line < range_end.line; ++line) {
			// these lines are fully contained in the range.
			BufferPos start = buffer_pos_start_of_line(buffer, line);
			BufferPos end = buffer_pos_end_of_line(buffer, line);
			a = buffer_pos_to_pixels(buffer, start);
			b = buffer_pos_to_pixels(buffer, end);
			b.y += char_height;
			Rect r = rect_endpoints(a, b); buffer_clip_rect(buffer, &r);
			gl_geometry_rect(r, colors[COLOR_HOVER_HL]);
		}
		
		// last line
		a = buffer_pos_to_pixels(buffer, buffer_pos_start_of_line(buffer, range_end.line));
		b = buffer_pos_to_pixels(buffer, range_end);
		b.y += char_height;
		Rect r2 = rect_endpoints(a, b); buffer_clip_rect(buffer, &r2);
		gl_geometry_rect(r2, colors[COLOR_HOVER_HL]);
	}
}
