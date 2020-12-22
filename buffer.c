// Text buffers - These store the contents of a file.
#include <wctype.h>
#include "unicode.h"
#include "util.c"
#include "text.h"
#include "string32.c"
#include "arr.c"

// a position in the buffer
typedef struct {
	u32 line;
	u32 index; // index of character in line (not the same as column, since a tab is buffer->tab_size columns)
} BufferPos;

typedef struct {
	u32 len;
	u32 capacity;
	char32_t *str;
} Line;

typedef enum {
	BUFFER_EDIT_DELETE_TEXT, // text = deleted text, text_len = length of text deleted, pos = where text was deleted (first character)
	BUFFER_EDIT_INSERT_TEXT // text = NULL, text_len = length of text inserted, pos = where text was inserted (first character)
	// (we don't need to know what text was inserted for BUFFER_EDIT_INSERT_TEXT, since it'll be right there)
} BufferEditType;

typedef struct BufferEdit {
	BufferEditType type;
	BufferPos pos;
	size_t text_len;
	char32_t *text;
} BufferEdit;

typedef struct {
	char const *filename;
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	Font *font;
	BufferPos cursor_pos;
	u8 tab_width;
	bool store_undo_events; // set to false to disable undo events
	float x1, y1, x2, y2;
	u32 nlines;
	u32 lines_capacity;
	Line *lines;
	char error[128];
	BufferEdit *undo_history; // dynamic array of undo history
	BufferEdit *redo_history; // dynamic array of redo history
} TextBuffer;



// for debugging
#if DEBUG
static void buffer_pos_check_valid(TextBuffer *buffer, BufferPos p) {
	assert(p.line < buffer->nlines);
	assert(p.index <= buffer->lines[p.line].len);
}

// perform a series of checks to make sure the buffer doesn't have any invalid values
static void buffer_check_valid(TextBuffer *buffer) {
	assert(buffer->nlines);
	buffer_pos_check_valid(buffer, buffer->cursor_pos);
}
#else
static void buffer_check_valid(TextBuffer *buffer) {
	(void)buffer;
}
#endif

void buffer_create(TextBuffer *buffer, Font *font) {
	util_zero_memory(buffer, sizeof *buffer);
	buffer->font = font;
	buffer->tab_width = 4;
	buffer->store_undo_events = true;
}

// this is a macro so we get -Wformat warnings
#define buffer_seterr(buffer, ...) \
	snprintf(buffer->error, sizeof buffer->error - 1, __VA_ARGS__)

bool buffer_haserr(TextBuffer *buffer) {
	return buffer->error[0] != '\0';
}

// returns the buffer's last error
char const *buffer_geterr(TextBuffer *buffer) {
	return buffer->error;
}

// set the buffer's error to indicate that we're out of memory
static void buffer_out_of_mem(TextBuffer *buffer) {
	buffer_seterr(buffer, "Out of memory.");
}


static void buffer_edit_free(BufferEdit *edit) {
	free(edit->text);
}

static void buffer_clear_redo_history(TextBuffer *buffer) {
	arr_foreach_ptr(buffer->redo_history, BufferEdit, edit) {
		buffer_edit_free(edit);
	}
	arr_clear(buffer->redo_history);
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

// ensures that `p` refers to a valid position.
static void buffer_pos_validate(TextBuffer *buffer, BufferPos *p) {
	if (p->line >= buffer->nlines)
		p->line = buffer->nlines - 1;
	u32 line_len = buffer->lines[p->line].len;
	if (p->index > line_len)
		p->index = line_len;
}

static bool buffer_pos_valid(TextBuffer *buffer, BufferPos p) {
	return p.line < buffer->nlines && p.index <= buffer->lines[p.line].len;
}

// code point at position.
// returns 0 if the position is invalid. note that it can also return 0 for a valid position, if there's a null character there
char32_t buffer_char_at_pos(TextBuffer *buffer, BufferPos p) {
	if (p.line >= buffer->nlines)
		return 0; // invalid (line too large)
	
	Line *line = &buffer->lines[p.line];
	if (p.index < line->len) {
		return line->str[p.index];
	} else if (p.index > line->len) {
		// invalid (col too large)
		return 0;
	} else {
		return U'\n';
	}
}

BufferPos buffer_start_pos(TextBuffer *buffer) {
	(void)buffer;
	return (BufferPos){.line = 0, .index = 0};
}

BufferPos buffer_end_pos(TextBuffer *buffer) {
	return (BufferPos){.line = buffer->nlines - 1, .index = buffer->lines[buffer->nlines-1].len};
}


// get some number of characters of text from the given position in the buffer.
static Status buffer_get_text_at_pos(TextBuffer *buffer, BufferPos pos, char32_t *text, size_t nchars) {
	if (!buffer_pos_valid(buffer, pos)) {
		return false;
	}
	char32_t *p = text;
	size_t chars_left = nchars;
	Line *line = &buffer->lines[pos.line], *end = buffer->lines + buffer->nlines;
	u32 index = pos.index;
	while (chars_left) {
		u32 chars_from_this_line = line->len - index;
		if (chars_left <= chars_from_this_line) {
			memcpy(p, line->str, chars_left);
			chars_left = 0;
		} else {
			memcpy(p, line->str, chars_from_this_line);
			p += chars_from_this_line;
			*p++ = U'\n';
			chars_left -= chars_from_this_line+1;
		}
		
		index = 0;
		++line;
		if (chars_left && line == end) {
			// reached end of file before getting full text... this isn't good
			buffer_seterr(buffer, "Failed to fetch %zu characters at " U32_FMT ":" U32_FMT ".", nchars, pos.line+1, pos.index+1);
			return false;
		}
	}
	return true;
}

static BufferPos buffer_pos_advance(TextBuffer *buffer, BufferPos pos, size_t nchars) {
	buffer_pos_validate(buffer, &pos);
	size_t chars_left = nchars;
	Line *line = &buffer->lines[pos.line], *end = buffer->lines + buffer->nlines;
	u32 index = pos.index;
	while (chars_left && line != end) {
		u32 chars_from_this_line = line->len - index;
		if (chars_left <= chars_from_this_line) {
			index += chars_left;
			pos.index = index;
			pos.line = (u32)(line - buffer->lines);
			return pos;
		}
		chars_left -= chars_from_this_line+1; // +1 for newline
		index = 0;
		++line;
	}
	return buffer_end_pos(buffer);
}


// returns "p2 - p1", that is, the number of characters between p1 and p2.
static i64 buffer_pos_diff(TextBuffer *buffer, BufferPos p1, BufferPos p2) {
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
		chars_in_lines_in_between += line->len;
	}
	i64 total = chars_at_end_of_p1_line + chars_in_lines_in_between + chars_at_start_of_p2_line;
	return total * factor;
}

// functions for creating buffer edits:
// call these before you make an edit to create an undo event
static Status buffer_create_edit_delete_text(TextBuffer *buffer, BufferEdit *edit, BufferPos start, size_t len) {
	edit->text = calloc(1, len * sizeof *edit->text);
	if (edit->text) {
		edit->type = BUFFER_EDIT_DELETE_TEXT;
		edit->pos = start;
		edit->text_len = len;
		if (!buffer_get_text_at_pos(buffer, start, edit->text, len)) {
			free(edit->text);
			return false;
		}
		return true;
	} else {
		return false;
	}
}

static Status buffer_create_edit_insert_text(TextBuffer *buffer, BufferEdit *edit, BufferPos start, size_t len) {
	if (!buffer_pos_valid(buffer, start)) return false;
	edit->type = BUFFER_EDIT_INSERT_TEXT;
	edit->pos = start;
	edit->text_len = len;
	edit->text = NULL;
	return true;
}

static void buffer_edit_delete_text(TextBuffer *buffer, BufferPos start, size_t len) {
	if (buffer->store_undo_events) {
		BufferEdit edit = {0};
		if (buffer_create_edit_delete_text(buffer, &edit, start, len)) {
			buffer_append_edit(buffer, &edit);
		}
	}
}

static void buffer_edit_insert_text(TextBuffer *buffer, BufferPos start, size_t len) {
	if (buffer->store_undo_events) {
		BufferEdit *last_edit = buffer->undo_history;
		i64 where_in_last_edit = buffer_pos_diff(buffer, last_edit->pos, start);
		if (where_in_last_edit >= 0 && where_in_last_edit < (i64)last_edit->text_len) {
			// @TODO
		}

		BufferEdit edit = {0};
		if (buffer_create_edit_insert_text(buffer, &edit, start, len)) {
			buffer_append_edit(buffer, &edit);
		}
	}
}

// grow capacity of line to at least minimum_capacity
// returns true if allocation was succesful
static Status buffer_line_set_min_capacity(TextBuffer *buffer, Line *line, u32 minimum_capacity) {
	while (line->capacity < minimum_capacity) {
		// double capacity of line
		u32 new_capacity = line->capacity == 0 ? 4 : line->capacity * 2;
		if (new_capacity < line->capacity) {
			// this could only happen if line->capacity * 2 overflows.
			buffer_seterr(buffer, "Line %td is too large.", line - buffer->lines);
			return false;
		}
		char32_t *new_str = buffer_realloc(buffer, line->str, new_capacity * sizeof *line->str);
		if (!new_str) {
			// allocation failed ):
			return false;
		}
		// allocation successful
		line->str = new_str;
		line->capacity = new_capacity;
	}
	return true;
}

// grow capacity of buffer->lines array
// returns true if successful
static Status buffer_lines_set_min_capacity(TextBuffer *buffer, u32 minimum_capacity) {
	while (minimum_capacity >= buffer->lines_capacity) {
		// allocate more lines
		u32 new_capacity = buffer->lines_capacity * 2;
		Line *new_lines = buffer_realloc(buffer, buffer->lines, new_capacity * sizeof *buffer->lines);
		if (new_lines) {
			buffer->lines = new_lines;
			buffer->lines_capacity = new_capacity;
			// zero new lines
			memset(buffer->lines + buffer->nlines, 0, (new_capacity - buffer->nlines) * sizeof *buffer->lines);
		} else {
			return false;
		}
	}
	return true;
}

static void buffer_line_append_char(TextBuffer *buffer, Line *line, char32_t c) {
	if (buffer_line_set_min_capacity(buffer, line, line->len + 1))
		line->str[line->len++] = c;
}

static void buffer_line_free(Line *line) {
	free(line->str);
}

// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void buffer_free(TextBuffer *buffer) {

	Line *lines = buffer->lines;
	u32 nlines = buffer->nlines;
	for (u32 i = 0; i < nlines; ++i) {
		buffer_line_free(&lines[i]);
	}
	free(lines);

	arr_free(buffer->undo_history);
	arr_free(buffer->redo_history);

	// zero buffer, except for error
	char error[sizeof buffer->error];
	memcpy(error, buffer->error, sizeof error);
	memset(buffer, 0, sizeof *buffer);
	memcpy(buffer->error, error, sizeof error);
}


// filename must be around for at least as long as the buffer is.
Status buffer_load_file(TextBuffer *buffer, char const *filename) {
	buffer->filename = filename;
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		size_t file_size = (size_t)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (file_size > 10L<<20) {
			buffer_seterr(buffer, "File too big (size: %zu).", file_size);
			return false;
		}

		u8 *file_contents = buffer_calloc(buffer, 1, file_size);
		bool success = true;
		if (file_contents) {
			buffer->lines_capacity = 4;
			buffer->lines = buffer_calloc(buffer, buffer->lines_capacity, sizeof *buffer->lines); // initial lines
			if (buffer->lines) {
				buffer->nlines = 1;
				size_t bytes_read = fread(file_contents, 1, file_size, fp);
				if (bytes_read == file_size) {
					char32_t c = 0;
					mbstate_t mbstate = {0};
					for (u8 *p = file_contents, *end = p + file_size; p != end; ) {
						if (*p == '\r' && p != end-1 && p[1] == '\n') {
							// CRLF line endings
							p += 2;
							c = U'\n';
						} else {
							size_t n = mbrtoc32(&c, (char *)p, (size_t)(end - p), &mbstate);
							if (n == 0) {
								// null character
								c = 0;
								++p;
							} else if (n == (size_t)(-3)) {
								// no bytes consumed, but a character was produced
							} else if (n == (size_t)(-2) || n == (size_t)(-1)) {
								// incomplete character at end of file or invalid UTF-8 respectively; fail
								success = false;
								buffer_seterr(buffer, "Invalid UTF-8 (position: %td).", p - file_contents);
								break;
							} else {
								p += n;
							}
						}
						if (c == U'\n') {
							if (buffer_lines_set_min_capacity(buffer, buffer->nlines + 1))
								++buffer->nlines;
						} else {
							u32 line_idx = buffer->nlines - 1;
							Line *line = &buffer->lines[line_idx];

							buffer_line_append_char(buffer, line, c);
						}
					}
				}
			}
			free(file_contents);
		}
		if (ferror(fp)) {	
			buffer_seterr(buffer, "Error reading from file.");
			success = false;
		}
		if (fclose(fp) != 0) {
			buffer_seterr(buffer, "Error closing file.");
			success = false;
		}
		if (!success) {
			buffer_free(buffer);
		}
		return success;
	} else {
		buffer_seterr(buffer, "File %s does not exist.", filename);
		return false;
	}
}

Status buffer_save(TextBuffer *buffer) {
	FILE *out = fopen(buffer->filename, "wb");
	if (out) {
		bool success = true;
		for (Line *line = buffer->lines, *end = line + buffer->nlines; line != end; ++line) {
			mbstate_t state = {0};
			for (char32_t *p = line->str, *p_end = p + line->len; p != p_end; ++p) {
				char utf8[MB_LEN_MAX] = {0};
				size_t bytes = c32rtomb(utf8, *p, &state);
				fwrite(utf8, 1, bytes, out);
			}

			if (line != end-1) {
				putc('\n', out);
			}
		}
		if (ferror(out)) success = false;
		if (fclose(out) != 0) success = false;
		return success;
	} else {
		buffer_seterr(buffer, "Couldn't create file %s.", buffer->filename);
		return false;
	}
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
			putwchar((wchar_t)line->str[j]);
		}
	}
	fflush(stdout);
}

static u32 buffer_index_to_column(TextBuffer *buffer, u32 line, u32 index) {
	char32_t *str = buffer->lines[line].str;
	u32 col = 0;
	for (u32 i = 0; i < index; ++i) {
		switch (str[i]) {
		case U'\t':
			do
				++col;
			while (col % buffer->tab_width);
			break;
		default:
			++col;
			break;
		}
	}
	return col;
}

static u32 buffer_column_to_index(TextBuffer *buffer, u32 line, u32 column) {
	char32_t *str = buffer->lines[line].str;
	u32 len = buffer->lines[line].len;
	u32 col = 0;
	for (u32 i = 0; i < len; ++i) {
		switch (str[i]) {
		case U'\t':
			do {
				if (col == column)
					return i;
				++col;
			} while (col % buffer->tab_width);
			break;
		default:
			if (col == column)
				return i;
			++col;
			break;
		}
	}
	return len;
}

// returns the number of lines of text in the buffer into *lines (if not NULL),
// and the number of columns of text, i.e. the number of columns in the longest line, into *cols (if not NULL)
void buffer_text_dimensions(TextBuffer *buffer, u32 *lines, u32 *columns) {
	if (lines) {
		*lines = buffer->nlines;
	}
	if (columns) {
		// @OPTIMIZE
		u32 nlines = buffer->nlines;
		Line *line_arr = buffer->lines;
		u32 maxcol = 0;
		for (u32 i = 0; i < nlines; ++i) {
			Line *line = &line_arr[i];
			u32 cols = buffer_index_to_column(buffer, i, line->len);
			if (cols > maxcol)
				maxcol = cols;
		}
		*columns = maxcol;
	}
}


// returns the number of rows of text that can fit in the buffer, rounded down.
int buffer_display_rows(TextBuffer *buffer) {
	return (int)((buffer->y2 - buffer->y1) / text_font_char_height(buffer->font));
}

// returns the number of columns of text that can fit in the buffer, rounded down.
int buffer_display_cols(TextBuffer *buffer) {
	return (int)((buffer->x2 - buffer->x1) / text_font_char_width(buffer->font));
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
	double max_scroll_y = (double)nlines - buffer_display_rows(buffer);
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

// returns the position of the character at the given position in the buffer.
// x/y can be NULL.
void buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos, float *x, float *y) {
	u32 line = pos.line, index = pos.index;
	u32 col = buffer_index_to_column(buffer, line, index);
	// we need to convert the index to a column
	if (x) *x = (float)((double)col  - buffer->scroll_x) * text_font_char_width(buffer->font) + buffer->x1;
	if (y) *y = (float)((double)line - buffer->scroll_y) * text_font_char_height(buffer->font) + buffer->y1;
}


// clip the rectangle so it's all inside the buffer. returns true if there's any rectangle left.
static bool buffer_clip_rect(TextBuffer *buffer, float *x1, float *y1, float *x2, float *y2) {
	if (*x1 > buffer->x2 || *y1 > buffer->y2 || *x2 < buffer->x1 || *y2 < buffer->y1) {
		*x1 = *y1 = *x2 = *y2 = 0;
		return false;
	}
	if (*x1 < buffer->x1) *x1 = buffer->x1;
	if (*y1 < buffer->y1) *y1 = buffer->y1;
	if (*x2 > buffer->x2) *x2 = buffer->x2;
	if (*y2 > buffer->y2) *y2 = buffer->y2;
	return true;
}

// Render the text buffer in the given rectangle
// NOTE: also corrects scroll
void buffer_render(TextBuffer *buffer, float x1, float y1, float x2, float y2) {
	buffer->x1 = x1; buffer->y1 = y1; buffer->x2 = x2; buffer->y2 = y2;
	Font *font = buffer->font;
	u32 nlines = buffer->nlines;
	Line *lines = buffer->lines;
	float char_width = text_font_char_width(font),
		char_height = text_font_char_height(font);
	glColor3f(0.5f,0.5f,0.5f);
	glBegin(GL_LINE_STRIP);
	glVertex2f(x1,y1);
	glVertex2f(x1,y2);
	glVertex2f(x2,y2);
	glVertex2f(x2,y1);
	glVertex2f(x1-1,y1);
	glEnd();

	glColor3f(1,1,1);
	text_chars_begin(font);

	// what x coordinate to start rendering the text from
	float render_start_x = x1 - (float)buffer->scroll_x * char_width;

	u32 column = 0;

	TextRenderState text_state = {
		.x = render_start_x, .y = y1 + text_font_char_height(font),
		.min_x = x1, .min_y = y1,
		.max_x = x2, .max_y = y2
	};

	// @TODO: make this better (we should figure out where to start rendering, etc.)
	text_state.y -= (float)buffer->scroll_y * char_height;

	for (u32 line_idx = 0; line_idx < nlines; ++line_idx) {
		Line *line = &lines[line_idx];
		for (char32_t *p = line->str, *end = p + line->len; p != end; ++p) {
			char32_t c = *p;

			switch (c) {
			case U'\n': assert(0);
			case U'\r': break; // for CRLF line endings
			case U'\t':
				do {
					text_render_char(font, U' ', &text_state);
					++column;
				} while (column % buffer->tab_width);
				break;
			default:
				text_render_char(font, c, &text_state);
				++column;
				break;
			}
		}

		// next line
		text_state.x = render_start_x;
		text_state.y += text_font_char_height(font);
		column = 0;
	}
	text_chars_end(font);

	{ // render cursor
		float cur_x1, cur_y1;
		buffer_pos_to_pixels(buffer, buffer->cursor_pos, &cur_x1, &cur_y1);
		cur_y1 += 0.25f * char_height;
		float cur_x2 = cur_x1 + 1.0f, cur_y2 = cur_y1 + char_height;
		if (buffer_clip_rect(buffer, &cur_x1, &cur_y1, &cur_x2, &cur_y2)) {
			glColor3f(0,1,1);
			glBegin(GL_QUADS);
			glVertex2f(cur_x1,cur_y1);
			glVertex2f(cur_x2,cur_y1);
			glVertex2f(cur_x2,cur_y2);
			glVertex2f(cur_x1,cur_y2);
			glEnd();
		}
	}
}

// if the cursor is offscreen, this will scroll to make it onscreen.
static void buffer_scroll_to_cursor(TextBuffer *buffer) {
	i64 cursor_line = buffer->cursor_pos.line;
	i64 cursor_col  = buffer_index_to_column(buffer, (u32)cursor_line, buffer->cursor_pos.index);
	i64 display_lines = buffer_display_rows(buffer);
	i64 display_cols = buffer_display_cols(buffer);
	double scroll_x = buffer->scroll_x, scroll_y = buffer->scroll_y;
	i64 scroll_padding = 5;

	// scroll left if cursor is off screen in that direction
	double max_scroll_x = (double)(cursor_col - scroll_padding);
	scroll_x = util_mind(scroll_x, max_scroll_x);
	// scroll right
	double min_scroll_x = (double)(cursor_col - display_cols + scroll_padding);
	scroll_x = util_maxd(scroll_x, min_scroll_x);
	// scroll up
	double max_scroll_y = (double)(cursor_line - scroll_padding);
	scroll_y = util_mind(scroll_y, max_scroll_y);
	// scroll down
	double min_scroll_y = (double)(cursor_line - display_lines + scroll_padding);
	scroll_y = util_maxd(scroll_y, min_scroll_y);

	buffer->scroll_x = scroll_x;
	buffer->scroll_y = scroll_y;
	buffer_correct_scroll(buffer); // it's possible that min/max_scroll_x/y go too far
}

// move left (if `by` is negative) or right (if `by` is positive) by the specified amount.
// returns the signed number of characters successfully moved (it could be less in magnitude than `by` if the beginning of the file is reached)
i64 buffer_pos_move_horizontally(TextBuffer *buffer, BufferPos *p, i64 by) {
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
			*p = buffer_end_pos(buffer); // invalid position; move to end of buffer
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
i64 buffer_pos_move_vertically(TextBuffer *buffer, BufferPos *pos, i64 by) {
	buffer_pos_validate(buffer, pos);
	// moving up/down should preserve the column, not the index.
	// consider:
	// tab|hello world
	// tab|tab|more text
	// the character above the 'm' is the 'o', not the 'e'
	if (by < 0) {
		by = -by;
		u32 column = buffer_index_to_column(buffer, pos->line, pos->index);
		if (pos->line < by) {
			i64 ret = pos->line;
			pos->line = 0;
			return -ret;
		}
		pos->line -= (u32)by;
		pos->index = buffer_column_to_index(buffer, pos->line, column);
		u32 line_len = buffer->lines[pos->line].len;
		if (pos->index >= line_len) pos->index = line_len;
		return -by;
	} else if (by > 0) {
		u32 column = buffer_index_to_column(buffer, pos->line, pos->index);
		if (pos->line + by >= buffer->nlines) {
			i64 ret = buffer->nlines-1 - pos->line;
			pos->line = buffer->nlines-1;
			return ret;
		}
		pos->line += (u32)by;
		pos->index = buffer_column_to_index(buffer, pos->line, column);
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

i64 buffer_cursor_move_left(TextBuffer *buffer, i64 by) {
	i64 ret = buffer_pos_move_left(buffer, &buffer->cursor_pos, by);
	buffer_scroll_to_cursor(buffer);
	return ret;
}

i64 buffer_cursor_move_right(TextBuffer *buffer, i64 by) {
	i64 ret = buffer_pos_move_right(buffer, &buffer->cursor_pos, by);
	buffer_scroll_to_cursor(buffer);
	return ret;
}

i64 buffer_cursor_move_up(TextBuffer *buffer, i64 by) {
	i64 ret = buffer_pos_move_up(buffer, &buffer->cursor_pos, by);
	buffer_scroll_to_cursor(buffer);
	return ret;
}

i64 buffer_cursor_move_down(TextBuffer *buffer, i64 by) {
	i64 ret = buffer_pos_move_down(buffer, &buffer->cursor_pos, by);
	buffer_scroll_to_cursor(buffer);
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
			char32_t const *str = line->str;
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
				// move past any whitespace before the word
				while (index < line->len && iswspace(str[index]))
					++index;
				
				bool starting_alnum = iswalnum(str[index]) != 0;
				for (; index < line->len && !iswspace(str[index]); ++index) {
					bool this_alnum = iswalnum(str[index]) != 0;
					if (this_alnum != starting_alnum) {
						// either the position *was* on an alphanumeric character and now it's not
						// or it wasn't and now it is.
						break;
					}
				}
				
				// move past any whitespace after the word
				while (index < line->len && iswspace(str[index]))
					++index;
				pos->index = index;
			}
		}
		return nwords;
	} else if (nwords < 0) {
		nwords = -nwords;
		for (i64 i = 0; i < nwords; ++i) {
			Line *line = &buffer->lines[pos->line];
			u32 index = pos->index;
			char32_t const *str = line->str;
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

				while (index > 0 && iswspace(str[index]))
					--index;
				bool starting_alnum = iswalnum(str[index]) != 0;
				for (; index > 0 && !iswspace(str[index]); --index) {
					bool this_alnum = iswalnum(str[index]) != 0;
					if (this_alnum != starting_alnum) {
						break;
					}
				}
				if (iswspace(str[index]))
					++index;
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
	i64 ret = buffer_pos_move_left_words(buffer, &buffer->cursor_pos, nwords);
	buffer_scroll_to_cursor(buffer);
	return ret;
}

i64 buffer_cursor_move_right_words(TextBuffer *buffer, i64 nwords) {
	i64 ret = buffer_pos_move_right_words(buffer, &buffer->cursor_pos, nwords);
	buffer_scroll_to_cursor(buffer);
	return ret;
}

// insert `number` empty lines starting at index `where`.
static void buffer_insert_lines(TextBuffer *buffer, u32 where, u32 number) {
	u32 old_nlines = buffer->nlines;
	u32 new_nlines = old_nlines + number;
	if (buffer_lines_set_min_capacity(buffer, new_nlines)) {
		assert(where <= old_nlines);
		// make space for new lines
		memmove(buffer->lines + where + (new_nlines - old_nlines),
			buffer->lines + where,
			(old_nlines - where) * sizeof *buffer->lines);
		// zero new lines
		util_zero_memory(buffer->lines + where, number * sizeof *buffer->lines);
		buffer->nlines = new_nlines;
	}
}

// inserts the given text, returning the position of the end of the text
BufferPos buffer_insert_text_at_pos(TextBuffer *buffer, BufferPos pos, String32 str) {
	buffer_edit_insert_text(buffer, pos, str.len);

	u32 line_idx = pos.line;
	u32 index = pos.index;
	Line *line = &buffer->lines[line_idx];

	// `text` could consist of multiple lines, e.g. U"line 1\nline 2",
	// so we need to go through them one by one
	u32 n_added_lines = (u32)str32_count_char(str, U'\n');
	if (n_added_lines) {
		buffer_insert_lines(buffer, line_idx + 1, n_added_lines);
		// move any text past the cursor on this line to the last added line.
		Line *last_line = &buffer->lines[line_idx + n_added_lines];
		u32 chars_moved = line->len - index;
		if (chars_moved) {
			if (buffer_line_set_min_capacity(buffer, last_line, chars_moved)) {
				memcpy(last_line->str, line->str + index, chars_moved * sizeof(char32_t));
				line->len  -= chars_moved;
				last_line->len += chars_moved;
			}
		}
	}


	while (str.len) {
		u32 text_line_len = (u32)str32chr(str, U'\n');
		u32 old_len = line->len;
		u32 new_len = old_len + text_line_len;
		if (new_len > old_len) { // handles both overflow and empty text lines
			if (buffer_line_set_min_capacity(buffer, line, new_len)) {
				// make space for text
				memmove(line->str + index + (new_len - old_len),
					line->str + index,
					(old_len - index) * sizeof(char32_t));
				// insert text
				memcpy(line->str + index, str.str, text_line_len * sizeof(char32_t));
				
				line->len = new_len;
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
		}
	}

	BufferPos b = {.line = line_idx, .index = index};
	return b;
}
	
void buffer_insert_text_at_cursor(TextBuffer *buffer, String32 str) {
	buffer->cursor_pos = buffer_insert_text_at_pos(buffer, buffer->cursor_pos, str);
	buffer_scroll_to_cursor(buffer);
}

void buffer_insert_char_at_cursor(TextBuffer *buffer, char32_t c) {
	String32 s = {1, &c};
	buffer_insert_text_at_cursor(buffer, s);
}

void buffer_insert_utf8_at_cursor(TextBuffer *buffer, char const *utf8) {
	String32 s32 = str32_from_utf8(utf8);
	if (s32.str) {
		buffer_insert_text_at_cursor(buffer, s32);
		str32_free(&s32);
	}
}

static void buffer_shorten_line(Line *line, u32 new_len) {
	assert(line->len >= new_len);
	line->len = new_len; // @OPTIMIZE(memory): decrease line capacity
}

// decrease the number of lines in the buffer.
// DOES NOT DO ANYTHING TO THE LINES REMOVED! YOU NEED TO FREE THEM YOURSELF!
static void buffer_shorten(TextBuffer *buffer, u32 new_nlines) {
	buffer->nlines = new_nlines; // @OPTIMIZE(memory): decrease lines capacity
}

// delete `nlines` lines starting from index `first_line_idx`
void buffer_delete_lines(TextBuffer *buffer, u32 first_line_idx, u32 nlines) {
	assert(first_line_idx < buffer->nlines);
	assert(first_line_idx+nlines <= buffer->nlines);
	Line *first_line = &buffer->lines[first_line_idx];
	Line *end = first_line + nlines;

	for (Line *l = first_line; l != end; ++l) {
		buffer_line_free(l);
	}
	memmove(first_line, end, (size_t)(buffer->lines + buffer->nlines - end) * sizeof(Line));
}

void buffer_delete_chars_at_pos(TextBuffer *buffer, BufferPos pos, i64 nchars) {
	assert(nchars >= 0);
	if (nchars <= 0) return;

	buffer_edit_delete_text(buffer, pos, (size_t)nchars);

	u32 line_idx = pos.line;
	u32 index = pos.index;
	Line *line = &buffer->lines[line_idx], *lines_end = &buffer->lines[buffer->nlines];
	if (nchars + index > line->len) {
		// delete rest of line
		buffer_shorten_line(line, index);
		nchars -= line->len - index + 1; // +1 for the newline that got deleted
		Line *last_line; // last line in lines deleted
		for (last_line = line + 1; last_line < lines_end && nchars > last_line->len; ++last_line) {
			nchars -= last_line->len+1;
		}
		if (last_line == lines_end) {
			// delete everything to the end of the file
			for (u32 idx = line_idx + 1; idx < buffer->nlines; ++idx) {
				buffer_line_free(&buffer->lines[idx]);
			}
			buffer_shorten(buffer, line_idx + 1);
		} else {
			// join last_line to line.
			u32 last_line_chars_left = (u32)(last_line->len - nchars);
			if (buffer_line_set_min_capacity(buffer, line, line->len + last_line_chars_left)) {
				memcpy(line->str + line->len, last_line->str, last_line_chars_left * sizeof(char32_t));
				line->len += last_line_chars_left;
			}
			// remove all lines between line + 1 and last_line (inclusive).
			buffer_delete_lines(buffer, line_idx + 1, (u32)(last_line - line));

			u32 lines_removed = (u32)(last_line - line);
			buffer_shorten(buffer, buffer->nlines - lines_removed);
		}
	} else {
		// just delete characters from this line
		memmove(line->str + index, line->str + index + nchars, (size_t)(line->len - (nchars + index)) * sizeof(char32_t));
		line->len -= (u32)nchars;
	}
}

// Delete characters between the given buffer positions.
void buffer_delete_chars_between(TextBuffer *buffer, BufferPos p1, BufferPos p2) {
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
}

void buffer_delete_chars_at_cursor(TextBuffer *buffer, i64 nchars) {
	buffer_delete_chars_at_pos(buffer, buffer->cursor_pos, nchars);
}

i64 buffer_backspace_at_pos(TextBuffer *buffer, BufferPos *pos, i64 ntimes) {
	i64 n = buffer_pos_move_left(buffer, pos, ntimes);
	buffer_delete_chars_at_pos(buffer, *pos, n);
	return n;
}

// returns number of characters backspaced
i64 buffer_backspace_at_cursor(TextBuffer *buffer, i64 ntimes) {
	return buffer_backspace_at_pos(buffer, &buffer->cursor_pos, ntimes);
}

void buffer_delete_words_at_pos(TextBuffer *buffer, BufferPos pos, i64 nwords) {
	BufferPos pos2 = pos;
	buffer_pos_move_right_words(buffer, &pos2, nwords);
	buffer_delete_chars_between(buffer, pos, pos2);
}

void buffer_delete_words_at_cursor(TextBuffer *buffer, i64 nwords) {
	buffer_delete_words_at_pos(buffer, buffer->cursor_pos, nwords);
}

void buffer_backspace_words_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	BufferPos pos2 = *pos;
	buffer_pos_move_left_words(buffer, pos, nwords);
	buffer_delete_chars_between(buffer, pos2, *pos);
}

void buffer_backspace_words_at_cursor(TextBuffer *buffer, i64 nwords) {
	buffer_backspace_words_at_pos(buffer, &buffer->cursor_pos, nwords);
}

// returns the inverse edit
static Status buffer_undo_edit(TextBuffer *buffer, BufferEdit const *edit, BufferEdit *inverse) {
	bool success = false;
	bool prev_store_undo_events = buffer->store_undo_events;
	// temporarily disable saving of undo events so we don't add the inverse edit
	// to the undo history
	buffer->store_undo_events = false;

	switch (edit->type) {
	case BUFFER_EDIT_INSERT_TEXT:
		// the inverse edit is the deletion of this text
		if (buffer_create_edit_delete_text(buffer, inverse, edit->pos, edit->text_len)) {
			// delete the text
			buffer_delete_chars_at_pos(buffer, edit->pos, (i64)edit->text_len);
			
			success = true;
		}
		break;
	case BUFFER_EDIT_DELETE_TEXT: {
		// the inverse edit is the insertion of this text
		if (buffer_create_edit_insert_text(buffer, inverse, edit->pos, edit->text_len)) {
			// insert the text
			String32 str = {edit->text_len, edit->text};
			buffer_insert_text_at_pos(buffer, edit->pos, str);

			success = true;
		}
	} break;
	}
	buffer->store_undo_events = prev_store_undo_events;
	return success;
}

void buffer_undo(TextBuffer *buffer, i64 ntimes) {
	for (i64 i = 0; i < ntimes; ++i) {
		BufferEdit *edit = arr_lastp(buffer->undo_history);
		if (edit) {
			BufferEdit inverse = {0};
			if (buffer_undo_edit(buffer, edit, &inverse)) {
				buffer->cursor_pos = edit->pos; // put cursor where edit happened
				buffer_scroll_to_cursor(buffer);

				buffer_append_redo(buffer, &inverse);
				buffer_edit_free(edit);
				arr_remove_last(buffer->undo_history);
			}
		}
	}
}

void buffer_redo(TextBuffer *buffer, i64 ntimes) {
	for (i64 i = 0; i < ntimes; ++i) {
		BufferEdit *edit = arr_lastp(buffer->redo_history);
		if (edit) {
			BufferEdit inverse = {0};
			if (buffer_undo_edit(buffer, edit, &inverse)) {
				buffer->cursor_pos = edit->pos; // put cursor where edit happened
				buffer_scroll_to_cursor(buffer);
				
				// NOTE: we can't just use buffer_append_edit, because that clears the redo history
				arr_add(buffer->undo_history, inverse);
				if (!buffer->undo_history) buffer_out_of_mem(buffer);

				buffer_edit_free(edit);
				arr_remove_last(buffer->redo_history);
			}
		}
	}
}
