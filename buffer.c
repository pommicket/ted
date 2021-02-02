// Text buffers - These store the contents of a file.
// NOTE: All text editing should be done through the two functions
// buffer_insert_text_at_pos and buffer_delete_chars_at_pos

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

void buffer_clearerr(TextBuffer *buffer) {
	*buffer->error = '\0';
}

// set the buffer's error to indicate that we're out of memory
static void buffer_out_of_mem(TextBuffer *buffer) {
	buffer_seterr(buffer, "Out of memory.");
}


static void buffer_edit_free(BufferEdit *edit) {
	free(edit->prev_text);
}

static void buffer_clear_redo_history(TextBuffer *buffer) {
	arr_foreach_ptr(buffer->redo_history, BufferEdit, edit) {
		buffer_edit_free(edit);
	}
	arr_clear(buffer->redo_history);
}

static void buffer_clear_undo_history(TextBuffer *buffer) {
	arr_foreach_ptr(buffer->undo_history, BufferEdit, edit) {
		buffer_edit_free(edit);
	}
	arr_clear(buffer->undo_history);
}


bool buffer_empty(TextBuffer *buffer) {
	return buffer->nlines == 1 && buffer->lines[0].len == 0;
}

char const *buffer_get_filename(TextBuffer *buffer) {
	return buffer->filename;
}

bool buffer_is_untitled(TextBuffer *buffer) {
	return streq(buffer->filename, TED_UNTITLED);
}

// clear all undo and redo events
void buffer_clear_undo_redo(TextBuffer *buffer) {
	buffer_clear_undo_history(buffer);
	buffer_clear_redo_history(buffer);
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

static char *buffer_strdup(TextBuffer *buffer, char const *src) {
	char *dup = str_dup(src);
	if (!dup) buffer_out_of_mem(buffer);
	return dup;
}

void buffer_create(TextBuffer *buffer, Ted *ted) {
	memset(buffer, 0, sizeof *buffer);
	buffer->store_undo_events = true;
	buffer->ted = ted;
}


void line_buffer_create(TextBuffer *buffer, Ted *ted) {
	buffer_create(buffer, ted);
	buffer->is_line_buffer = true;
	if ((buffer->lines = buffer_calloc(buffer, 1, sizeof *buffer->lines))) {
		buffer->nlines = 1;
		buffer->lines_capacity = 1;
	}
}

// ensures that `p` refers to a valid position.
static void buffer_pos_validate(TextBuffer *buffer, BufferPos *p) {
	if (p->line >= buffer->nlines)
		p->line = buffer->nlines - 1;
	u32 line_len = buffer->lines[p->line].len;
	if (p->index > line_len)
		p->index = line_len;
}

static void buffer_validate_cursor(TextBuffer *buffer) {
	buffer_pos_validate(buffer, &buffer->cursor_pos);
}

static bool buffer_pos_valid(TextBuffer *buffer, BufferPos p) {
	return p.line < buffer->nlines && p.index <= buffer->lines[p.line].len;
}

// are there any unsaved changes?
bool buffer_unsaved_changes(TextBuffer *buffer) {
	if (buffer_is_untitled(buffer) && buffer_empty(buffer))
		return false; // don't worry about empty untitled buffers
	return buffer->modified;
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
		return '\n';
	}
}

BufferPos buffer_start_of_file(TextBuffer *buffer) {
	(void)buffer;
	return (BufferPos){.line = 0, .index = 0};
}

BufferPos buffer_end_of_file(TextBuffer *buffer) {
	return (BufferPos){.line = buffer->nlines - 1, .index = buffer->lines[buffer->nlines-1].len};
}

// Get the font used for this buffer.
static inline Font *buffer_font(TextBuffer *buffer) {
	return buffer->ted->font;
}

// Get the settings used for this buffer.
static inline Settings const *buffer_settings(TextBuffer *buffer) {
	return &buffer->ted->settings;
}

// what programming language is this?
Language buffer_language(TextBuffer *buffer) {
	Settings const *settings = buffer_settings(buffer);
	char const *filename = buffer->filename;
	if (!filename)
		return LANG_NONE;
	size_t filename_len = strlen(filename);

	for (u16 l = 0; l < LANG_COUNT; ++l) {
		char const *extensions = settings->language_extensions[l];
		if (extensions) {
			// extensions is a string with commas separating each extension.
			size_t len = 0;
			for (char const *p = extensions; *p; p += len) {
				if (*p == ',') ++p; // move past comma
				len = strcspn(p, ",");
				if (filename_len >= len && strncmp(&filename[filename_len - len], p, len) == 0) {
					// found a match!
					return (Language)l;
				}
			}
		}
	}
	// no extensions matched
	return LANG_NONE;
}

// NOTE: this string will be invalidated when the line is edited!!!
// only use it briefly!!
static String32 buffer_get_line(TextBuffer *buffer, u32 line_number) {
	Line *line = &buffer->lines[line_number];
	return (String32) {
		.str = line->str, .len = line->len
	};
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

// Get some number of characters of text from the given position in the buffer.
// Returns the number of characters gotten.
// You can pass NULL for text if you just want to know how many characters *could* be accessed before the
// end of the file.
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

// returns a UTF-32 string of at most `nchars` code points from `buffer` starting at `pos`
// the string should be str32_free'd.
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

// see buffer_get_str32_text_at_pos. returns NULL on failure (out of memory)
// the returned string should be free'd
char *buffer_get_utf8_text_at_pos(TextBuffer *buffer, BufferPos pos, size_t nchars) {
	String32 s32 = buffer_get_str32_text_at_pos(buffer, pos, nchars);
	char *ret = str32_to_utf8_cstr(s32);
	if (!ret) buffer_out_of_mem(buffer);
	str32_free(&s32);
	return ret;
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
	return buffer_end_of_file(buffer);
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
		chars_in_lines_in_between += line->len + 1; // +1 for newline
	}
	i64 total = chars_at_end_of_p1_line + chars_in_lines_in_between + chars_at_start_of_p2_line;
	return total * factor;
}

// returns:
// -1 if p1 comes before p2
// +1 if p1 comes after p2
// 0  if p1 = p2
// faster than buffer_pos_diff (constant time)
static int buffer_pos_cmp(BufferPos p1, BufferPos p2) {
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

static bool buffer_pos_eq(BufferPos p1, BufferPos p2) {
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

static Status buffer_edit_create(TextBuffer *buffer, BufferEdit *edit, BufferPos start, u32 prev_len, u32 new_len) {
	edit->time = time_get_seconds();
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
		// @OPTIMIZE: compare directly to the buffer contents, rather than extracting them temporarily
		// into new_text.
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
static bool buffer_edit_split(TextBuffer *buffer) {
	double curr_time = time_get_seconds();
	double undo_time_cutoff = buffer_settings(buffer)->undo_save_time; // only keep around edits for this long (in seconds).
	BufferEdit *last_edit = arr_lastp(buffer->undo_history);
	if (!last_edit) return true;
	return curr_time - last_edit->time > undo_time_cutoff;
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
	if (buffer_line_set_min_capacity(buffer, line, line->len + 1))
		line->str[line->len++] = c;
}

static void buffer_line_free(Line *line) {
	free(line->str);
}

// Free a buffer. Once a buffer is freed, you can call buffer_create on it again.
// Does not free the pointer `buffer` (buffer might not have even been allocated with malloc)
void buffer_free(TextBuffer *buffer) {
	Line *lines = buffer->lines;
	u32 nlines = buffer->nlines;
	for (u32 i = 0; i < nlines; ++i) {
		buffer_line_free(&lines[i]);
	}
	free(lines);
	free(buffer->filename);

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

// if an error occurs, buffer is left untouched (except for the error field) and the function returns false.
Status buffer_load_file(TextBuffer *buffer, char const *filename) {
	FILE *fp = fopen(filename, "rb");
	bool success = true;
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long file_pos = ftell(fp);
		size_t file_size = (size_t)file_pos;
		fseek(fp, 0, SEEK_SET);
		if (file_pos == -1 || file_pos == LONG_MAX) {
			buffer_seterr(buffer, "Couldn't get file position. There is something wrong with the file '%s'.", filename);
			success = false;
		} else if (file_size > 10L<<20) {
			buffer_seterr(buffer, "File too big (size: %zu).", file_size);
			success = false;
		} else {
			u8 *file_contents = buffer_calloc(buffer, 1, file_size);
			if (file_contents) {
				u32 lines_capacity = 4;
				Line *lines = buffer_calloc(buffer, lines_capacity, sizeof *buffer->lines); // initial lines
				if (lines) {
					u32 nlines = 1;
					size_t bytes_read = fread(file_contents, 1, file_size, fp);
					if (bytes_read == file_size) {
						char32_t c = 0;
						for (u8 *p = file_contents, *end = p + file_size; p != end; ) {
							if (*p == '\r' && p != end-1 && p[1] == '\n') {
								// CRLF line endings
								p += 2;
								c = '\n';
							} else {
								size_t n = unicode_utf8_to_utf32(&c, (char *)p, (size_t)(end - p));
								if (n == 0) {
									// null character
									c = 0;
									++p;
								} else if (n == (size_t)(-1)) {
									// invalid UTF-8
									success = false;
									buffer_seterr(buffer, "Invalid UTF-8 (position: %td).", p - file_contents);
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
						if (success) {
							char *filename_copy = buffer_strdup(buffer, filename);
							if (!filename_copy) success = false;
							if (success) {
								// everything is good
								buffer_clear(buffer);
								buffer->lines = lines;
								buffer->nlines = nlines;
								buffer->frame_earliest_line_modified = 0;
								buffer->frame_latest_line_modified = nlines - 1;
								buffer->lines_capacity = lines_capacity;
								buffer->filename = filename_copy;
							}
						}
					} else {
						success = false;
					}
					if (!success) {
						// something went wrong; we need to free all the memory we used
						for (u32 i = 0; i < nlines; ++i)
							buffer_line_free(&lines[i]);
						free(lines);
					}
				}
				free(file_contents);
			}
			if (ferror(fp)) {	
				buffer_seterr(buffer, "Error reading from file.");
				success = false;
			}
		}
		if (fclose(fp) != 0) {
			buffer_seterr(buffer, "Error closing file.");
			success = false;
		}
	} else {
		buffer_seterr(buffer, "Couldn't open file %s: %s.", filename, strerror(errno));
		success = false;
	}
	return success;
}

void buffer_new_file(TextBuffer *buffer, char const *filename) {
	buffer_clear(buffer);

	buffer->filename = buffer_strdup(buffer, filename);
	buffer->lines_capacity = 4;
	buffer->lines = buffer_calloc(buffer, buffer->lines_capacity, sizeof *buffer->lines);
	buffer->nlines = 1;
}

// Save the buffer to its current filename. This will rewrite the entire file, regardless of
// whether there are any unsaved changes.
bool buffer_save(TextBuffer *buffer) {
	Settings const *settings = buffer_settings(buffer);
	if (!buffer->is_line_buffer && buffer->filename) {
		FILE *out = fopen(buffer->filename, "wb");
		if (out) {
			for (Line *line = buffer->lines, *end = line + buffer->nlines; line != end; ++line) {
				for (char32_t *p = line->str, *p_end = p + line->len; p != p_end; ++p) {
					char utf8[4] = {0};
					size_t bytes = unicode_utf32_to_utf8(utf8, *p);
					if (bytes != (size_t)-1) {
						if (fwrite(utf8, 1, bytes, out) != bytes) {
							buffer_seterr(buffer, "Couldn't write to %s.", buffer->filename);
						}
					}
				}

				if (line != end-1) {
					putc('\n', out);
				} else {
					if (settings->auto_add_newline) {
						if (line->len) {
							// if the last line isn't empty, add a newline.
							putc('\n', out);
						}
					}
				}
			}
			if (ferror(out)) {
				if (!buffer_haserr(buffer))
					buffer_seterr(buffer, "Couldn't write to %s.", buffer->filename);
			}
			if (fclose(out) != 0) {
				if (!buffer_haserr(buffer))
					buffer_seterr(buffer, "Couldn't close file %s.", buffer->filename);
			}
			bool success = !buffer_haserr(buffer);
			if (success) {
				buffer->modified = false;
			}
			return success;
		} else {
			buffer_seterr(buffer, "Couldn't open file %s for writing: %s.", buffer->filename, strerror(errno));
			return false;
		}
	} else {
		// user tried to save line buffer. whatever
		return true;
	}
}

// save, but with a different file name
bool buffer_save_as(TextBuffer *buffer, char const *new_filename) {
	char *prev_filename = buffer->filename;
	if ((buffer->filename = buffer_strdup(buffer, new_filename))) {
		if (buffer_save(buffer)) {
			free(prev_filename);
			return true;
		} else {
			free(buffer->filename);
			buffer->filename = prev_filename;
			return false;
		}
	} else {
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
	uint tab_width = buffer_settings(buffer)->tab_width;
	for (u32 i = 0; i < index; ++i) {
		switch (str[i]) {
		case '\t': {
			do
				++col;
			while (col % tab_width);
		} break;
		default:
			++col;
			break;
		}
	}
	return col;
}

static u32 buffer_column_to_index(TextBuffer *buffer, u32 line, u32 column) {
	if (line >= buffer->nlines) {
		assert(0);
		return 0;
	}
	char32_t *str = buffer->lines[line].str;
	u32 len = buffer->lines[line].len;
	u32 col = 0;
	uint tab_width = buffer_settings(buffer)->tab_width;
	for (u32 i = 0; i < len; ++i) {
		switch (str[i]) {
		case '\t': {
			do {
				if (col == column)
					return i;
				++col;
			} while (col % tab_width);
		} break;
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
// and the number of columns of text, i.e. the number of columns in the longest line displayed, into *cols (if not NULL)
void buffer_text_dimensions(TextBuffer *buffer, u32 *lines, u32 *columns) {
	if (lines) {
		*lines = buffer->nlines;
	}
	if (columns) {
		*columns = buffer->longest_line_on_screen;
	}
}


// returns the number of rows of text that can fit in the buffer, rounded down.
int buffer_display_lines(TextBuffer *buffer) {
	return (int)((buffer->y2 - buffer->y1) / text_font_char_height(buffer_font(buffer)));
}

// returns the number of columns of text that can fit in the buffer, rounded down.
int buffer_display_cols(TextBuffer *buffer) {
	return (int)((buffer->x2 - buffer->x1) / text_font_char_width(buffer_font(buffer)));
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

void buffer_page_up(TextBuffer *buffer, i64 npages) {
	buffer_scroll(buffer, 0, (double)(-npages * buffer_display_lines(buffer)));
}

void buffer_page_down(TextBuffer *buffer, i64 npages) {
	buffer_scroll(buffer, 0, (double)(+npages * buffer_display_lines(buffer)));
}

// returns the position of the character at the given position in the buffer.
v2 buffer_pos_to_pixels(TextBuffer *buffer, BufferPos pos) {
	u32 line = pos.line, index = pos.index;
	// we need to convert the index to a column
	u32 col = buffer_index_to_column(buffer, line, index);
	Font *font = buffer_font(buffer);
	float x = (float)((double)col  - buffer->scroll_x) * text_font_char_width(font) + buffer->x1;
	float y = (float)((double)line - buffer->scroll_y + 0.2f /* nudge */) * text_font_char_height(font) + buffer->y1;
	return V2(x, y);
}

// convert pixel coordinates to a position in the buffer, selecting the closest character.
// returns false if the position is not inside the buffer, but still sets *pos to the closest character.
bool buffer_pixels_to_pos(TextBuffer *buffer, v2 pixel_coords, BufferPos *pos) {
	bool ret = true;
	float x = pixel_coords.x, y = pixel_coords.y;
	Font *font = buffer_font(buffer);
	pos->line = pos->index = 0;

	x -= buffer->x1;
	y -= buffer->y1;
	x /= text_font_char_width(font);
	y /= text_font_char_height(font);
	double display_col = (double)x;
	if (display_col < 0) {
		display_col = 0;
		ret = false;
	}
	int display_cols = buffer_display_cols(buffer), display_lines = buffer_display_lines(buffer);
	if (display_col >= display_cols) {
		display_col = display_cols - 1;
		ret = false;
	}
	double display_line = (double)y;
	if (display_line < 0) {
		display_line = 0;
		ret = false;
	}
	if (display_line >= display_lines) {
		display_line = display_lines - 1;
		ret = false;
	}
	
	u32 line = (u32)floor(display_line + buffer->scroll_y);
	if (line >= buffer->nlines) line = buffer->nlines - 1;
	u32 column = (u32)round(display_col + buffer->scroll_x);
	u32 index = buffer_column_to_index(buffer, line, column);
	pos->line = line;
	pos->index = index;
	
	return ret;
}

// clip the rectangle so it's all inside the buffer. returns true if there's any rectangle left.
static bool buffer_clip_rect(TextBuffer *buffer, Rect *r) {
	float x1, y1, x2, y2;
	rect_coords(*r, &x1, &y1, &x2, &y2);
	if (x1 > buffer->x2 || y1 > buffer->y2 || x2 < buffer->x1 || y2 < buffer->y1) {
		r->pos = r->size = V2(0, 0);
		return false;
	}
	if (x1 < buffer->x1) x1 = buffer->x1;
	if (y1 < buffer->y1) y1 = buffer->y1;
	if (x2 > buffer->x2) x2 = buffer->x2;
	if (y2 > buffer->y2) y2 = buffer->y2;
	*r = rect4(x1, y1, x2, y2);
	return true;
}


// if the cursor is offscreen, this will scroll to make it onscreen.
static void buffer_scroll_to_cursor(TextBuffer *buffer) {
	Settings const *settings = buffer_settings(buffer);
	i64 cursor_line = buffer->cursor_pos.line;
	i64 cursor_col  = buffer_index_to_column(buffer, (u32)cursor_line, buffer->cursor_pos.index);
	i64 display_lines = buffer_display_lines(buffer);
	i64 display_cols = buffer_display_cols(buffer);
	double scroll_x = buffer->scroll_x, scroll_y = buffer->scroll_y;
	i64 scrolloff = settings->scrolloff;

	// scroll left if cursor is off screen in that direction
	double max_scroll_x = (double)(cursor_col - scrolloff);
	scroll_x = mind(scroll_x, max_scroll_x);
	// scroll right
	double min_scroll_x = (double)(cursor_col - display_cols + scrolloff);
	scroll_x = maxd(scroll_x, min_scroll_x);
	// scroll up
	double max_scroll_y = (double)(cursor_line - scrolloff);
	scroll_y = mind(scroll_y, max_scroll_y);
	// scroll down
	double min_scroll_y = (double)(cursor_line - display_lines + scrolloff);
	scroll_y = maxd(scroll_y, min_scroll_y);

	buffer->scroll_x = scroll_x;
	buffer->scroll_y = scroll_y;
	buffer_correct_scroll(buffer); // it's possible that min/max_scroll_x/y go too far
}

// scroll so that the cursor is in the center of the screen
void buffer_center_cursor(TextBuffer *buffer) {
	i64 cursor_line = buffer->cursor_pos.line;
	i64 cursor_col  = buffer_index_to_column(buffer, (u32)cursor_line, buffer->cursor_pos.index);
	i64 display_lines = buffer_display_lines(buffer);
	i64 display_cols = buffer_display_cols(buffer);
	
	buffer->scroll_x = (double)(cursor_col - display_cols / 2);
	buffer->scroll_y = (double)(cursor_line - display_lines / 2);

	buffer_correct_scroll(buffer);
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
			*p = buffer_end_of_file(buffer); // invalid position; move to end of buffer
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

void buffer_cursor_move_to_pos(TextBuffer *buffer, BufferPos pos) {
	buffer_pos_validate(buffer, &pos);
	buffer->cursor_pos = pos;
	buffer->selection = false;
	buffer_scroll_to_cursor(buffer);
}

i64 buffer_cursor_move_left(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_left(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_cursor_move_right(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_right(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_cursor_move_up(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_up(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

i64 buffer_cursor_move_down(TextBuffer *buffer, i64 by) {
	BufferPos cur_pos = buffer->cursor_pos;
	i64 ret = buffer_pos_move_down(buffer, &cur_pos, by);
	buffer_cursor_move_to_pos(buffer, cur_pos);
	return ret;
}

// Is this character a "word" character?
// This determines how buffer_pos_move_words (i.e. ctrl+left/right) works
static bool is_word(char32_t c) {
	return c > WCHAR_MAX || c == '_' || iswalnum((wint_t)c);
}

static bool is_space(char32_t c) {
	return c > WCHAR_MAX || iswspace((wint_t)c);
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
				while (index < line->len && is_space(str[index]))
					++index;
				
				bool starting_isword = is_word(str[index]) != 0;
				for (; index < line->len && !is_space(str[index]); ++index) {
					bool this_isword = is_word(str[index]) != 0;
					if (this_isword != starting_isword) {
						// either the position *was* on an alphanumeric character and now it's not
						// or it wasn't and now it is.
						break;
					}
				}
				
				// move past any whitespace after the word
				while (index < line->len && is_space(str[index]))
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

				while (index > 0 && is_space(str[index])) // skip whitespace after word
					--index;
				if (index > 0) {
					bool starting_isword = is_word(str[index]) != 0;
					while (true) {
						bool this_isword = is_word(str[index]) != 0;
						if (is_space(str[index]) || this_isword != starting_isword) {
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
	buffer_cursor_move_to_pos(buffer, buffer_start_of_file(buffer));
}

void buffer_cursor_move_to_end_of_file(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_end_of_file(buffer));
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

// inserts the given text, returning the position of the end of the text
BufferPos buffer_insert_text_at_pos(TextBuffer *buffer, BufferPos pos, String32 str) {
	if (str.len > U32_MAX) {
		buffer_seterr(buffer, "Inserting too much text (length: %zu).", str.len);
		BufferPos ret = {0,0};
		return ret;
	}

	if (buffer->is_line_buffer) {
		// remove all the newlines from str.
		str32_remove_all_instances_of_char(&str, '\n');
	}

	if (str.len == 0) {
		// no text to insert
		return pos;
	}

	if (buffer->store_undo_events) {
		BufferEdit *last_edit = arr_lastp(buffer->undo_history);
		i64 where_in_last_edit = last_edit ? buffer_pos_diff(buffer, last_edit->pos, pos) : -1;
		// create a new edit, rather than adding to the old one if:
		bool create_new_edit = where_in_last_edit < 0 || where_in_last_edit > last_edit->new_len // insertion is happening outside the previous edit,
			|| buffer_edit_split(buffer); // or enough time has elapsed to warrant a new one.

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
	// so we need to go through them one by one
	u32 n_added_lines = (u32)str32_count_char(str, '\n');
	if (n_added_lines) {
		if (buffer_insert_lines(buffer, line_idx + 1, n_added_lines)) {
			line = &buffer->lines[line_idx]; // fix pointer
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
	}


	while (str.len) {
		u32 text_line_len = (u32)str32chr(str, '\n');
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

	// We need to put this after the end so the emptiness-checking is done after the edit is made.
	buffer_remove_last_edit_if_empty(buffer);

	buffer_lines_modified(buffer, pos.line, line_idx);

	BufferPos b = {.line = line_idx, .index = index};
	return b;
}

// Select (or add to selection) everything between the cursor and pos, and move the cursor to pos
void buffer_select_to_pos(TextBuffer *buffer, BufferPos pos) {
	if (!buffer->selection)
		buffer->selection_pos = buffer->cursor_pos;
	buffer_cursor_move_to_pos(buffer, pos);
	buffer->selection = !buffer_pos_eq(buffer->cursor_pos, buffer->selection_pos); // disable selection if cursor_pos = selection_pos.
}

// Like shift+left in most editors, move cursor nchars chars to the left, selecting everything in between
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
	buffer_select_to_pos(buffer, buffer_start_of_file(buffer));
}

void buffer_select_to_end_of_file(TextBuffer *buffer) {
	buffer_select_to_pos(buffer, buffer_end_of_file(buffer));
}

// select the word the cursor is inside of
void buffer_select_word(TextBuffer *buffer) {
	BufferPos start_pos = buffer->cursor_pos, end_pos = buffer->cursor_pos;
	buffer_pos_move_left_words(buffer, &start_pos, 1);
	buffer_pos_move_right_words(buffer, &end_pos, 1);
	buffer_cursor_move_to_pos(buffer, end_pos);
	buffer_select_to_pos(buffer, start_pos);
}

// select the line the cursor is currently on
void buffer_select_line(TextBuffer *buffer) {
	u32 line = buffer->cursor_pos.line;
	if (line == buffer->nlines - 1)
		buffer_cursor_move_to_pos(buffer, buffer_pos_end_of_line(buffer, line));
	else
		buffer_cursor_move_to_pos(buffer, buffer_pos_start_of_line(buffer, line + 1));
	buffer_select_to_pos(buffer, buffer_pos_start_of_line(buffer, line));
}

void buffer_select_all(TextBuffer *buffer) {
	buffer_cursor_move_to_pos(buffer, buffer_start_of_file(buffer));
	buffer_select_to_pos(buffer, buffer_end_of_file(buffer));
}

// stop selecting
void buffer_disable_selection(TextBuffer *buffer) {
	if (buffer->selection) {
		buffer->cursor_pos = buffer->selection_pos;
		buffer->selection = false;
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
	if (nchars_ < 0) {
		buffer_seterr(buffer, "Deleting negative characters (specifically, " I64_FMT ").", nchars_);
		return;
	}
	if (nchars_ <= 0) return;
	if (nchars_ > U32_MAX) nchars_ = U32_MAX;
	u32 nchars = (u32)nchars_;

	// Correct nchars in case it goes past the end of the file.
	// Why do we need to correct it?
	// When generating undo events, we allocate nchars characters of memory (see buffer_edit below).
	// Not doing this might also cause other bugs, best to keep it here just in case.
	nchars = (u32)buffer_get_text_at_pos(buffer, pos, NULL, nchars);

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
			buffer_edit_split(buffer); // or if enough time has passed to warrant a new edit

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
			// join last_line to line.
			u32 last_line_chars_left = (u32)(last_line->len - nchars);
			if (buffer_line_set_min_capacity(buffer, line, line->len + last_line_chars_left)) {
				memcpy(line->str + line->len, last_line->str + nchars, last_line_chars_left * sizeof(char32_t));
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
		line->len -= nchars;
	}

	buffer_remove_last_edit_if_empty(buffer);
	
	// cursor position could have been invalidated by this edit
	buffer_validate_cursor(buffer);

	buffer_lines_modified(buffer, line_idx, line_idx);
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
}

void buffer_insert_char_at_cursor(TextBuffer *buffer, char32_t c) {
	String32 s = {&c, 1};
	buffer_insert_text_at_cursor(buffer, s);
}

void buffer_insert_utf8_at_cursor(TextBuffer *buffer, char const *utf8) {
	String32 s32 = str32_from_utf8(utf8);
	if (s32.str) {
		buffer_insert_text_at_cursor(buffer, s32);
		str32_free(&s32);
	}
}

// insert newline at cursor and auto-indent
void buffer_newline(TextBuffer *buffer) {
	Settings const *settings = buffer_settings(buffer);
	BufferPos cursor_pos = buffer->cursor_pos;
	String32 line = buffer_get_line(buffer, cursor_pos.line);
	u32 whitespace_len;
	for (whitespace_len = 0; whitespace_len < line.len; ++whitespace_len) {
		if (line.str[whitespace_len] != ' ' && line.str[whitespace_len] != '\t')
			break; // found end of indentation
	}
	if (settings->auto_indent) {
		// newline + auto-indent
		char32_t *text = buffer_calloc(buffer, whitespace_len + 1, sizeof *text); // @OPTIMIZE: don't allocate on heap if whitespace_len is small
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
	if (buffer->selection)
		buffer_delete_selection(buffer);
	else
		buffer_delete_chars_at_pos(buffer, buffer->cursor_pos, nchars);
}

i64 buffer_backspace_at_pos(TextBuffer *buffer, BufferPos *pos, i64 ntimes) {
	i64 n = buffer_pos_move_left(buffer, pos, ntimes);
	buffer_delete_chars_at_pos(buffer, *pos, n);
	return n;
}

// returns number of characters backspaced
i64 buffer_backspace_at_cursor(TextBuffer *buffer, i64 ntimes) {
	if (buffer->selection)
		return buffer_delete_selection(buffer);
	else
		return buffer_backspace_at_pos(buffer, &buffer->cursor_pos, ntimes);
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
}

void buffer_backspace_words_at_pos(TextBuffer *buffer, BufferPos *pos, i64 nwords) {
	BufferPos pos2 = *pos;
	buffer_pos_move_left_words(buffer, pos, nwords);
	buffer_delete_chars_between(buffer, pos2, *pos);
}

void buffer_backspace_words_at_cursor(TextBuffer *buffer, i64 nwords) {
	if (buffer->selection)
		buffer_delete_selection(buffer);
	else
		buffer_backspace_words_at_pos(buffer, &buffer->cursor_pos, nwords);
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
	buffer_center_cursor(buffer); // whenever we undo an edit, put the cursor in the center, to make it clear where the undo happened
}

void buffer_undo(TextBuffer *buffer, i64 ntimes) {
	for (i64 i = 0; i < ntimes; ++i) {
		BufferEdit *edit = arr_lastp(buffer->undo_history);
		if (edit) {
			BufferEdit inverse = {0};
			if (buffer_undo_edit(buffer, edit, &inverse)) {
				if (i == ntimes - 1) {
					// if we're on the last undo, put cursor where edit is
					buffer_cursor_to_edit(buffer, edit);
				}

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
				if (i == ntimes - 1)
					buffer_cursor_to_edit(buffer, edit);
				
				// NOTE: we can't just use buffer_append_edit, because that clears the redo history
				arr_add(buffer->undo_history, inverse);
				if (!buffer->undo_history) buffer_out_of_mem(buffer);

				buffer_edit_free(edit);
				arr_remove_last(buffer->redo_history);
			}
		}
	}
}

void buffer_copy_or_cut(TextBuffer *buffer, bool cut) {
	if (buffer->selection) {
		BufferPos pos1 = buffer_pos_min(buffer->selection_pos, buffer->cursor_pos);
		BufferPos pos2 = buffer_pos_max(buffer->selection_pos, buffer->cursor_pos);
		i64 selection_len = buffer_pos_diff(buffer, pos1, pos2);
		char *text = buffer_get_utf8_text_at_pos(buffer, pos1, (size_t)selection_len);
		if (text) {
			int err = SDL_SetClipboardText(text);
			free(text);
			if (err < 0) {
				buffer_seterr(buffer, "Couldn't get clipboard contents: %s", SDL_GetError());
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
				buffer_insert_text_at_cursor(buffer, str);
				str32_free(&str);
			}
			SDL_free(text);
		}
	}
}

// for debugging
#if DEBUG
static void buffer_pos_check_valid(TextBuffer *buffer, BufferPos p) {
	assert(p.line < buffer->nlines);
	assert(p.index <= buffer->lines[p.line].len);
}

// perform a series of checks to make sure the buffer doesn't have any invalid values
void buffer_check_valid(TextBuffer *buffer) {
	assert(buffer->nlines);
	buffer_pos_check_valid(buffer, buffer->cursor_pos);
	if (buffer->selection) {
		buffer_pos_check_valid(buffer, buffer->selection_pos);
		// you shouldn't be able to select nothing
		assert(!buffer_pos_eq(buffer->cursor_pos, buffer->selection_pos));
	}
}
#else
void buffer_check_valid(TextBuffer *buffer) {
	(void)buffer;
}
#endif

// Render the text buffer in the given rectangle
void buffer_render(TextBuffer *buffer, Rect r) {
	float x1, y1, x2, y2;
	rect_coords(r, &x1, &y1, &x2, &y2);
	// Correct the scroll, because the window size might have changed
	buffer_correct_scroll(buffer);

	Font *font = buffer_font(buffer);
	u32 nlines = buffer->nlines;
	Line *lines = buffer->lines;
	float char_width = text_font_char_width(font),
		char_height = text_font_char_height(font);

	Ted *ted = buffer->ted;
	Settings const *settings = buffer_settings(buffer);
	u32 const *colors = settings->colors;

	float border_thickness = settings->border_thickness;

	// get screen coordinates of cursor
	v2 cursor_display_pos = buffer_pos_to_pixels(buffer, buffer->cursor_pos);
	// the rectangle that the cursor is rendered as
	Rect cursor_rect = rect(cursor_display_pos, V2(settings->cursor_width, char_height));

	u32 border_color = colors[COLOR_BORDER]; // color of border around buffer

	// bounding box around buffer
	glBegin(GL_QUADS);
	gl_color_rgba(border_color);
	rect_render_border(rect4(x1, y1, x2, y2), border_thickness);
	glEnd();
	x1 += border_thickness * 0.5f;
	y1 += border_thickness * 0.5f;
	x2 -= border_thickness * 0.5f;
	y2 -= border_thickness * 0.5f;

	TextRenderState text_state = {
		.x = 0, .y = 0,
		.min_x = x1, .min_y = y1,
		.max_x = x2, .max_y = y2,
		.render = true
	};

	buffer->x1 = x1; buffer->y1 = y1; buffer->x2 = x2; buffer->y2 = y2;

	
	// highlight line cursor is on
	{
		gl_color_rgba(colors[COLOR_CURSOR_LINE_BG]);
		glBegin(GL_QUADS);
		Rect hl_rect = rect(V2(x1, cursor_display_pos.y), V2(x2-x1-1, char_height));
		buffer_clip_rect(buffer, &hl_rect);
		rect_render(hl_rect);
		glEnd();
	}


	// what x coordinate to start rendering the text from
	float render_start_x = x1 - (float)buffer->scroll_x * char_width;
	u32 column = 0;

	u32 start_line = (u32)buffer->scroll_y; // line to start rendering from

	if (buffer->selection) { // draw selection
		glBegin(GL_QUADS);
		gl_color_rgba(colors[COLOR_SELECTION_BG]);
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
			u32 n_columns_highlighted = buffer_index_to_column(buffer, line_idx, index2)
				- buffer_index_to_column(buffer, line_idx, index1);
			if (line_idx != sel_end.line) {
				++n_columns_highlighted; // highlight the newline (otherwise empty higlighted lines wouldn't be highlighted at all).
			}

			if (n_columns_highlighted) {
				BufferPos p1 = {.line = line_idx, .index = index1};
				v2 hl_p1 = buffer_pos_to_pixels(buffer, p1);
				Rect hl_rect = rect(
					hl_p1,
					V2((float)n_columns_highlighted * char_width, char_height)
				);
				buffer_clip_rect(buffer, &hl_rect);
				rect_render(hl_rect);
			}
			index1 = 0;
		}

		glEnd();
	}
	

	text_chars_begin(font);

	text_state = (TextRenderState){
		.x = render_start_x, .y = y1 + 0.25f * text_font_char_height(font),
		.min_x = x1, .min_y = y1,
		.max_x = x2, .max_y = y2,
		.render = true
	};

	// sel_pos >= scrolloff
	// sel - scroll >= scrolloff
	// scroll <= sel - scrolloff
	text_state.y -= (float)(buffer->scroll_y - start_line) * char_height;

	Language language = buffer_language(buffer);
	// dynamic array of character types, to be filled by syntax_highlight
	SyntaxCharType *char_types = NULL;
	bool syntax_highlighting = language && settings->syntax_highlighting;
	if (!syntax_highlighting) {
		gl_color_rgba(colors[COLOR_TEXT]);
	}

	if (buffer->frame_latest_line_modified >= buffer->frame_earliest_line_modified
		&& syntax_highlighting) {
		// update syntax cache
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

	buffer->longest_line_on_screen = 0;
	
	for (u32 line_idx = start_line; line_idx < nlines; ++line_idx) {
		Line *line = &lines[line_idx];
		buffer->longest_line_on_screen = max_u32(buffer->longest_line_on_screen, line->len);
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
				gl_color_rgba(colors[syntax_char_type_to_color(type)]);
			}
			switch (c) {
			case '\n': assert(0); break;
			case '\r': break; // for CRLF line endings
			case '\t': {
				uint tab_width = settings->tab_width;
				do {
					text_render_char(font, &text_state, ' ');
					++column;
				} while (column % tab_width);
			} break;
			default:
				text_render_char(font, &text_state, c);
				++column;
				break;
			}
		}

		// next line
		text_state.x = render_start_x;
		if (text_state.y > text_state.max_y) {
			// made it to the bottom of the buffer view.
			break;
		}
		text_state.y += text_font_char_height(font);
		column = 0;
	}
	
	arr_free(char_types);

	

	text_chars_end(font);

	if (buffer == ted->active_buffer) {
		// render cursor
		float time_on = settings->cursor_blink_time_on;
		float time_off = settings->cursor_blink_time_off;
		bool is_on = true;
		if (time_off > 0) {
			double absolute_time = time_get_seconds();
			float period = time_on + time_off;
			// time in period
			double t = fmod(absolute_time, period);
			is_on = t < time_on; // are we in the "on" part of the period?
		}
		if (is_on) {
			if (buffer_clip_rect(buffer, &cursor_rect)) {
				gl_color_rgba(colors[COLOR_CURSOR]);
				glBegin(GL_QUADS);
				rect_render(cursor_rect);
				glEnd();
			}
		}
	}
}
