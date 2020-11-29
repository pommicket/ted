// Text buffers - These store the contents of a file.
// To make inserting characters faster, the file contents are split up into
// blocks of a few thousand bytes. Block boundaries are not necessarily
// on character boundaries, so one block might have part of a UTF-8 character
// with the next block having the rest.
#include "unicode.h"
#include "util.c"
#include "text.h"

// @TODO: make this bigger -- maybe 4000 (it's small now to test having a bunch of blocks)
#define TEXT_BLOCK_MAX_SIZE 400
// Once two adjacent blocks are at most this big, combine them into
// one bigger text block.
#define TEXT_BLOCK_COMBINE_SIZE (3*(TEXT_BLOCK_MAX_SIZE / 4))
// A good size for text blocks to be. When a file is loaded, there
// shouldn't be just full blocks, because they will all immediately split.
// Instead, we use blocks of this size.
#define TEXT_BLOCK_DEFAULT_SIZE (3*(TEXT_BLOCK_MAX_SIZE / 4))
typedef struct {
	u16 len;
	u8 *contents;
} TextBlock;

// a position in the buffer
typedef struct {
	u32 block;
	u32 index;
} BufferPos;

typedef struct {
	double scroll_x, scroll_y; // number of characters scrolled in the x/y direction
	Font *font;
	BufferPos cursor_pos;
	u8 tab_width;
	float x1, y1, x2, y2;
	u32 nblocks; // number of text blocks
	TextBlock *blocks;
} TextBuffer;

// Returns a new block added at index `where`,
// or NULL if there's not enough memory.
static TextBlock *buffer_add_block(TextBuffer *buffer, u32 where) {
	// make sure we can actually allocate enough memory for the contents of the block
	// before doing anything
	u8 *block_contents = calloc(1, TEXT_BLOCK_MAX_SIZE);
	if (!block_contents) {
		return NULL;
	}

	u32 nblocks_before = buffer->nblocks;
	u32 nblocks_after = buffer->nblocks + 1;
	if (util_is_power_of_2(nblocks_after)) {
		// whenever the number of blocks is a power of 2, we need to grow the blocks array.
		buffer->blocks = realloc(buffer->blocks, 2 * nblocks_after * sizeof *buffer->blocks);
		if (!buffer->blocks) {
			// out of memory
			free(block_contents);
			return NULL;
		}
	}
	if (where != nblocks_before) { // if we aren't just inserting the block at the end,
		// make space for the new block
		memmove(buffer->blocks + (where + 1), buffer->blocks + where, nblocks_before - where);
	}
	
	TextBlock *block = buffer->blocks + where;
	util_zero_memory(block, sizeof *buffer->blocks); // zero everything
	block->contents = block_contents;

	
	buffer->nblocks = nblocks_after;
	return block;
}

void buffer_create(TextBuffer *buffer, Font *font) {
	util_zero_memory(buffer, sizeof *buffer);
	buffer->font = font;
	buffer->tab_width = 4;
}

Status buffer_load_file(TextBuffer *buffer, FILE *fp) {
	char block_contents[TEXT_BLOCK_DEFAULT_SIZE];
	size_t bytes_read;
	// @TODO: @TODO: IMPORTANT! make this work if buffer already has a file
	
	// read file one block at a time
	do {
		bytes_read = fread(block_contents, 1, sizeof block_contents, fp);
		if (bytes_read > 0) {
			TextBlock *block = buffer_add_block(buffer, buffer->nblocks);
			if (!block) {
				return false;
			}
			memcpy(block->contents, block_contents, bytes_read);
			block->len = (u16)bytes_read;
		}
	} while (bytes_read == sizeof block_contents);
	if (ferror(fp))	
		return false;
	return true;
}

static void buffer_print_internal(TextBuffer *buffer, bool debug) {
	printf("\033[2J\033[;H"); // clear terminal screen
	TextBlock *blocks = buffer->blocks;
	u32 nblocks = buffer->nblocks;
	
	for (u32 i = 0; i < nblocks; ++i) {
		TextBlock *block = &blocks[i];
		fwrite(block->contents, 1, block->len, stdout);
		if (debug)
			printf("\n\n------\n\n"); // NOTE: could be bad with UTF-8
	}
	fflush(stdout);
}

// print the contents of a buffer to stdout
void buffer_print(TextBuffer *buffer) {
	buffer_print_internal(buffer, false);
}

// print the contents of a buffer to stdout, along with debugging information
void buffer_print_debug(TextBuffer *buffer) {
	buffer_print_internal(buffer, true);
}

// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void buffer_free(TextBuffer *buffer) {
	TextBlock *blocks = buffer->blocks;
	u32 nblocks = buffer->nblocks;
	for (u32 i = 0; i < nblocks; ++i) {
		free(blocks[i].contents);
	}
	free(blocks);
}

// advance line and col according to the character c
// either can be NULL
static void buffer_update_line_col(TextBuffer *buffer, u64 *line, u64 *col, int c) {
	if (line) {
		if (c == '\n') ++*line;
	}
	if (col) {
		switch (c) {
		case '\n':
			*col = 0;
			break;
		case '\r': break;
		case '\t':
			do
				++*col;
			while (*col % buffer->tab_width);
			break;
		default:
			++*col;
			break;
		}
	}
}

// returns the number of lines of text in the buffer into *lines (if not NULL),
// and the number of columns of text, i.e. the length of the longest line, into *cols (if not NULL)
static void buffer_text_dimensions(TextBuffer *buffer, u64 *lines, u64 *cols) {
	// @OPTIMIZE
	u64 line = 1;
	u64 maxcol = 0;
	u64 col = 0;
	for (u32 i = 0; i < buffer->nblocks; ++i) {
		TextBlock *block = &buffer->blocks[i];
		for (u8 *p = block->contents, *end = p + block->len; p != end; ++p) {
			buffer_update_line_col(buffer, &line, &col, *p);
			if (col > maxcol)
				maxcol = col;
		}
	}
	if (lines) *lines = line;
	if (cols) *cols = maxcol;
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
	u64 nlines, ncols;
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

// returns the line and column of the given buffer position.
// line/col can be NULL.
void buffer_pos_to_line_col(TextBuffer *buffer, BufferPos pos, u64 *line, u64 *col) {
	TextBlock *pos_block = buffer->blocks + pos.block;
	assert(pos.index < pos_block->len);
	u8 *pos_byte = pos_block->contents + pos.index;
	u64 l = 1, c = 0;
	for (TextBlock *block = buffer->blocks; block <= pos_block; ++block) {
		for (u8 *p = block->contents, *end = p + block->len; p != end; ++p) {
			if (p == pos_byte) {
				if (line) *line = l;
				if (col) *col = c;
				return;
			}
			buffer_update_line_col(buffer, &l, &c, *p);
		}
	}
}

// returns the position of the character at the given position in the buffer.
// x/y can be NULL.
void buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos, float *x, float *y) {
	u64 line, col;
	buffer_pos_to_line_col(buffer, pos, &line, &col);
	if (x) *x = (float)((double)col  - buffer->scroll_x) * text_font_char_width(buffer->font) + buffer->x1;
	if (y) *y = (float)((double)(line-1) - buffer->scroll_y) * text_font_char_height(buffer->font) + buffer->y1;
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
	mbstate_t mbstate = {0};
	u32 nblocks = buffer->nblocks;
	TextBlock *blocks = buffer->blocks;
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

	for (u32 block_idx = 0; block_idx < nblocks; ++block_idx) {
		TextBlock *block = &blocks[block_idx];
		{
			float v = 0.7f + 0.3f * (float)(block_idx % 2);
			glColor3f(v, v, 1);
		}
		u8 *p = block->contents, *end = p + block->len;
		while (p != end) {
			char32_t c;
			size_t n = mbrtoc32(&c, (char *)p, (size_t)(end - p), &mbstate);
			if (n == 0) {
				// null character
				c = UNICODE_BOX_CHARACTER;
				++p;
			} else if (n == (size_t)(-3)) {
				// no bytes consumed, but a character was produced
			} else if (n == (size_t)(-2)) {
				// incomplete character at end of block.
				c = 0;
				p = end;
			} else if (n == (size_t)(-1)) {
				// invalid UTF-8
				c = UNICODE_BOX_CHARACTER;
				++p;
			} else {
				p += n;
			}
			switch (c) {
			case L'\n':
				text_state.x = render_start_x;
				text_state.y += text_font_char_height(font);
				column = 0;
				break;
			case L'\r': break; // for CRLF line endings
			case L'\t':
				do {
					text_render_char(font, L' ', &text_state);
					++column;
				} while (column % buffer->tab_width);
				break;
			default:
				text_render_char(font, c, &text_state);
				++column;
				break;
			}
		}
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

static u8 buffer_byte_at_pos(TextBuffer *buffer, BufferPos p) {
	return buffer->blocks[p.block].contents[p.index];
}

// returns true if p could move left (i.e. if it's not the very first position in the file)
static bool buffer_pos_move_left(TextBuffer *buffer, BufferPos *p) {
	do {
		if (p->index == 0) {
			if (p->block == 0) 
				return false;
			--p->block;
		} else {
			--p->index;
		}
	} while (!unicode_is_start_of_code_point(buffer_byte_at_pos(buffer, *p)));
	return true;
}

static bool buffer_pos_move_right(TextBuffer *buffer, BufferPos *p) {
	do {
		TextBlock *block = buffer->blocks + p->block;
		if (p->index >= block->len-1) {
			if (p->block >= buffer->nblocks-1) 
				return false;
			++p->block;
		} else {
			++p->index;
		}
	} while (!unicode_is_start_of_code_point(buffer_byte_at_pos(buffer, *p)));
	return true;
}

bool buffer_cursor_move_left(TextBuffer *buffer) {
	return buffer_pos_move_left(buffer, &buffer->cursor_pos);
}

bool buffer_cursor_move_right(TextBuffer *buffer) {
	return buffer_pos_move_right(buffer, &buffer->cursor_pos);
}
