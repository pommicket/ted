// Text buffers - These store the contents of a file.
#include "unicode.h"
#include "util.c"
#include "text.h"
#include "string32.c"

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

typedef struct {
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	Font *font;
	BufferPos cursor_pos;
	u8 tab_width;
	bool out_of_mem; // set to true if an allocation fails
	float x1, y1, x2, y2;
	u32 nlines;
	u32 lines_capacity;
	Line *lines;
} TextBuffer;

void buffer_create(TextBuffer *buffer, Font *font) {
	util_zero_memory(buffer, sizeof *buffer);
	buffer->font = font;
	buffer->tab_width = 4;
}

// grow capacity of line to at least minimum_capacity
// returns true if allocation was succesful
static Status buffer_line_grow(TextBuffer *buffer, Line *line, u32 minimum_capacity) {
	while (line->capacity < minimum_capacity) {
		// double capacity of line
		u32 new_capacity = line->capacity == 0 ? 4 : line->capacity * 2;
		if (new_capacity < line->capacity) {
			// overflow. this could only happen with an extremely long line, so we
			// treat it as 
			buffer->out_of_mem = true;
			return false;
		}
		char32_t *new_str = realloc(line->str, new_capacity * sizeof *line->str);
		if (!new_str) {
			// allocation failed ):
			buffer->out_of_mem = true;
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
static Status buffer_lines_grow(TextBuffer *buffer, u32 minimum_capacity) {
	while (minimum_capacity >= buffer->lines_capacity) {
		// allocate more lines
		u32 new_capacity = buffer->lines_capacity * 2;
		Line *new_lines = realloc(buffer->lines, new_capacity * sizeof *buffer->lines);
		if (new_lines) {
			buffer->lines = new_lines;
			buffer->lines_capacity = new_capacity;
			// zero new lines
			memset(buffer->lines + buffer->nlines, 0, (new_capacity - buffer->nlines) * sizeof *buffer->lines);
		} else {
			buffer->out_of_mem = true;
			return false;
		}
	}
	return true;
}

static void buffer_line_append_char(TextBuffer *buffer, Line *line, char32_t c) {
	if (buffer_line_grow(buffer, line, line->len + 1))
		line->str[line->len++] = c;
}

// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void buffer_free(TextBuffer *buffer) {
	Line *lines = buffer->lines;
	u32 nlines = buffer->nlines;
	for (u32 i = 0; i < nlines; ++i) {
		free(lines[i].str);
	}
	free(lines);
	buffer->nlines = 0;
	buffer->lines = NULL;
}


// fp should be a binary file
Status buffer_load_file(TextBuffer *buffer, FILE *fp) {
	assert(fp);
	fseek(fp, 0, SEEK_END);
	size_t file_size = (size_t)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (file_size > 10L<<20) {
		// @EVENTUALLY: better handling
		printf("File too big.\n");
		return false;
	}

	u8 *file_contents = malloc(file_size);
	bool success = true;
	if (file_contents) {
		buffer->lines_capacity = 4;
		buffer->lines = calloc(buffer->lines_capacity, sizeof *buffer->lines); // initial lines
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
						// incomplete character at end of file or invalid UTF-8 respectively; just treat it as a byte
						c = *p;
						++p;
					} else {
						p += n;
					}
				}
				if (c == U'\n') {
					if (buffer_lines_grow(buffer, buffer->nlines + 1))
						++buffer->nlines;
				} else {
					u32 line_idx = buffer->nlines - 1;
					Line *line = &buffer->lines[line_idx];

					buffer_line_append_char(buffer, line, c);
				}
			}
		}
		free(file_contents);
	}
	if (ferror(fp)) success = false;
	if (!success) {
		buffer_free(buffer);
	}
	return success;

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

// returns true if p could move left (i.e. if it's not the very first position in the file)
bool buffer_pos_move_left(TextBuffer *buffer, BufferPos *p) {
	if (p->line >= buffer->nlines)
		*p = buffer_end_pos(buffer); // invalid position; move to end of buffer
	if (p->index == 0) {
		// first column; move to previous line
		if (p->line == 0)
			return false;
		--p->line;
		p->index = buffer->lines[p->line].len;
	} else {
		--p->index;
	}
	return true;
}

bool buffer_pos_move_right(TextBuffer *buffer, BufferPos *p) {
	if (p->line >= buffer->nlines)
		*p = buffer_end_pos(buffer); // invalid position; move to end of buffer
	Line *line = &buffer->lines[p->line];
	if (p->index >= line->len) {
		// last column; move to next line
		if (p->line >= buffer->nlines - 1) {
			// last line
			*p = buffer_end_pos(buffer);
			return false;
		} else {
			p->index = 0;
			++p->line;
		}
	} else {
		++p->index;
	}
	return true;
}

bool buffer_pos_move_up(TextBuffer *buffer, BufferPos *pos) {
	(void)buffer;
	if (pos->line == 0)
		return false;
	// moving up/down should preserve the column, not the index.
	// consider:
	// tab|hello world
	// tab|tab|more text
	// the character above the 'm' is the 'o', not the 'e'
	u32 column = buffer_index_to_column(buffer, pos->line, pos->index);
	--pos->line;
	pos->index = buffer_column_to_index(buffer, pos->line, column);
	u32 line_len = buffer->lines[pos->line].len;
	if (pos->index >= line_len) pos->index = line_len;
	return true;
}

bool buffer_pos_move_down(TextBuffer *buffer, BufferPos *pos) {
	if (pos->line >= buffer->nlines-1)
		return false;
	u32 column = buffer_index_to_column(buffer, pos->line, pos->index);
	++pos->line;
	pos->index = buffer_column_to_index(buffer, pos->line, column);
	u32 line_len = buffer->lines[pos->line].len;
	if (pos->index >= line_len) pos->index = line_len;
	return true;
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

// insert `number` empty lines starting at index `where`.
static void buffer_insert_lines(TextBuffer *buffer, u32 where, u32 number) {
	u32 old_nlines = buffer->nlines;
	u32 new_nlines = old_nlines + number;
	if (buffer_lines_grow(buffer, new_nlines)) {
		assert(where <= old_nlines);
		// make space for new lines
		memmove(buffer->lines + where + (new_nlines - old_nlines),
			buffer->lines + where,
			(old_nlines - where) * sizeof *buffer->lines);
		// zero new lines
		util_zero_memory(buffer->lines + where, number * sizeof *buffer->lines);
	}
}

void buffer_insert_text_at_cursor(TextBuffer *buffer, String32 str) {
	u32 cur_line_idx = buffer->cursor_pos.line;
	u32 cur_index = buffer->cursor_pos.index;
	Line *cur_line = &buffer->lines[cur_line_idx];

	// `text` could consist of multiple lines, e.g. U"line 1\nline 2",
	// so we need to go through them one by one
	u32 n_added_lines = (u32)str32_count_char(str, U'\n');
	buffer_insert_lines(buffer, cur_line_idx + 1, n_added_lines);
	if (n_added_lines) {
		// move any text past the cursor on this line to the last added line.
		Line *last_line = &buffer->lines[cur_line_idx + n_added_lines];
		u32 chars_moved = cur_line->len - cur_index;
		if (chars_moved) {
			if (buffer_line_grow(buffer, last_line, chars_moved)) {
				memcpy(last_line->str, cur_line->str + cur_index, chars_moved * sizeof(char32_t));
				cur_line->len  -= chars_moved;
				last_line->len += chars_moved;
			}
		}
	}


	while (str.len) {
		u32 text_line_len = (u32)str32chr(str, U'\n');
		u32 old_len = cur_line->len;
		u32 new_len = old_len + text_line_len;
		if (new_len > old_len) { // handles both overflow and empty text lines
			if (buffer_line_grow(buffer, cur_line, new_len)) {
				// make space for text
				memmove(cur_line->str + cur_index + (new_len - old_len),
					cur_line->str + cur_index,
					(old_len - cur_index) * sizeof(char32_t));
				// insert text
				memcpy(cur_line->str + cur_index, str.str, text_line_len * sizeof(char32_t));
				
				cur_line->len = new_len;
			}

			str.str += text_line_len;
			str.len -= text_line_len;
			cur_index += text_line_len;
		}
		if (str.len) {
			// we've got a newline.
			cur_line_idx += 1;
			cur_index = 0;
			++cur_line;
			++str.str;
			--str.len;
		}
	}

	buffer->cursor_pos.line = cur_line_idx;
	buffer->cursor_pos.index = cur_index;

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

bool buffer_cursor_move_left(TextBuffer *buffer) {
	if (buffer_pos_move_left(buffer, &buffer->cursor_pos)) {
		buffer_scroll_to_cursor(buffer);
		return true;
	}
	return false;
}

bool buffer_cursor_move_right(TextBuffer *buffer) {
	if (buffer_pos_move_right(buffer, &buffer->cursor_pos)) {
		buffer_scroll_to_cursor(buffer);
		return true;
	}
	return false;
}

bool buffer_cursor_move_up(TextBuffer *buffer) {
	if (buffer_pos_move_up(buffer, &buffer->cursor_pos)) {
		buffer_scroll_to_cursor(buffer);
		return true;
	}
	return false;
}

bool buffer_cursor_move_down(TextBuffer *buffer) {
	if (buffer_pos_move_down(buffer, &buffer->cursor_pos)) {
		buffer_scroll_to_cursor(buffer);
		return true;
	}
	return false;
}

