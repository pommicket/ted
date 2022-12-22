// clear build errors and stop
static void build_stop(Ted *ted) {
	if (ted->building)
		process_kill(&ted->build_process);
	ted->building = false;
	ted->build_shown = false;
	arr_foreach_ptr(ted->build_errors, BuildError, err) {
		free(err->filename);
	}
	arr_clear(ted->build_errors);
	arr_foreach_ptr(ted->build_queue, char *, cmd) {
		free(*cmd);
	}
	arr_clear(ted->build_queue);
	if (ted->active_buffer == &ted->build_buffer) {
		ted_switch_to_buffer(ted, NULL);
		ted_reset_active_buffer(ted);
	}
}

// call before adding anything to the build queue
static void build_queue_start(Ted *ted) {
	build_stop(ted);
}

// add a command to the build queue
static void build_queue_command(Ted *ted, char const *command) {
	char *copy = str_dup(command);
	if (copy)
		arr_add(ted->build_queue, copy);
}

// returns true if there are still commands left in the queue
static bool build_run_next_command_in_queue(Ted *ted) {
	if (!ted->build_queue)
		return false;
	assert(*ted->build_dir);
	change_directory(ted->build_dir);
	char *command = ted->build_queue[0];
	arr_remove(ted->build_queue, 0);
	if (ted_save_all(ted)) {
		if (process_run(&ted->build_process, command)) {
			ted->building = true;
			ted->build_shown = true;
			TextBuffer *build_buffer = &ted->build_buffer;
			char32_t text[] = {'$', ' '};
			buffer_insert_text_at_cursor(build_buffer, str32(text, 2));
			buffer_insert_utf8_at_cursor(build_buffer, command);
			buffer_insert_char_at_cursor(build_buffer, '\n');
			build_buffer->view_only = true;
			free(command);
			return true;
		} else {
			ted_seterr(ted, "Couldn't start build: %s", process_geterr(&ted->build_process));
			build_stop(ted);
			return false;
		}
	} else {
		build_stop(ted);
		return false;
	}
}

// make sure you set ted->build_dir before running this!
static void build_queue_finish(Ted *ted) {
	// new empty build output buffer
	TextBuffer *build_buffer = &ted->build_buffer;
	buffer_new_file(build_buffer, NULL);
	build_buffer->store_undo_events = false; // don't need undo events for build output buffer
	build_run_next_command_in_queue(ted); // run the first command
}

// make sure you set ted->build_dir before running this!
static void build_start_with_command(Ted *ted, char const *command) {
	build_queue_start(ted);
	build_queue_command(ted, command);
	build_queue_finish(ted);
}

static void build_start(Ted *ted) {
	bool cargo = false, make = false;
	
	strbuf_cpy(ted->build_dir, ted->cwd);
	Settings *settings = ted_active_settings(ted);
	
	char *command = settings->build_default_command;
	
	change_directory(ted->cwd);
#if _WIN32
	if (fs_file_exists("make.bat")) {
		command = "make.bat";
	} else
#endif
	// check if Cargo.toml exists in this or the parent/parent's parent directory
	if (fs_file_exists("Cargo.toml")) {
		cargo = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR "Cargo.toml")) {
		ted_path_full(ted, "..", ted->build_dir, sizeof ted->build_dir);
		cargo = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR ".." PATH_SEPARATOR_STR "Cargo.toml")) {
		ted_path_full(ted, "../..", ted->build_dir, sizeof ted->build_dir);
		cargo = true;
	} else 
	// Check if Makefile exists in this or the parent/parent's parent directory
	if (fs_file_exists("Makefile")) {
		make = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR "Makefile")) {
		ted_path_full(ted, "..", ted->build_dir, sizeof ted->build_dir);
		make = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR ".." PATH_SEPARATOR_STR "Makefile")) {
		ted_path_full(ted, "../..", ted->build_dir, sizeof ted->build_dir);
		make = true;
	}
	
	
	// @TODO(eventually): `go build`
	
	if (cargo) {
		command = "cargo build";
	} else if (make) {
		command = "make -j12";
	}
	
	build_start_with_command(ted, command);
}

static void build_go_to_error(Ted *ted) {
	if (ted->build_error < arr_len(ted->build_errors)) {
		BuildError error = ted->build_errors[ted->build_error];
		// open the file where the error happened
		if (ted_open_file(ted, error.filename)) {
			TextBuffer *buffer = ted->active_buffer;
			assert(buffer);
			// move cursor to error
			buffer_cursor_move_to_pos(buffer, error.pos);
			buffer->center_cursor_next_frame = true;

			// move cursor to error in build output
			TextBuffer *build_buffer = &ted->build_buffer;
			BufferPos error_pos = {.line = error.build_output_line, .index = 0};
			buffer_cursor_move_to_pos(build_buffer, error_pos);
			buffer_center_cursor(build_buffer);
		}
	}
}

static void build_next_error(Ted *ted) {
	if (ted->build_errors) {
		ted->build_error += 1;
		ted->build_error %= arr_len(ted->build_errors);
		build_go_to_error(ted);
	}
}

static void build_prev_error(Ted *ted) {
	if (ted->build_errors) {
		ted->build_error += arr_len(ted->build_errors) - 1;
		ted->build_error %= arr_len(ted->build_errors);
		build_go_to_error(ted);
	}
}

// returns -1 if *str..end is not an integer or a negative integer.
// Otherwise, advances *str past the number and returns it.
static int parse_nonnegative_integer(char32_t **str, char32_t *end) {
	char32_t *s = *str;
	int number_len;
	int n = 0;
	for (number_len = 0; s != end && s[number_len] >= '0' && s[number_len] <= '9'; ++number_len) {
		n *= 10;
		n += (int)s[number_len] - '0';
	}
	if (number_len == 0)
		return -1;
	*str += number_len;
	return n;
}

// could this character (reasonably) appear in a source file path?
static bool is_source_path(char32_t c) {
	char const *allowed_ascii_symbols_in_path = "./\\-_:";
	return c > CHAR_MAX || isalnum((char)c)
		|| strchr(allowed_ascii_symbols_in_path, (char)c);
}

static void build_frame(Ted *ted, float x1, float y1, float x2, float y2) {
	TextBuffer *buffer = &ted->build_buffer;
	Process *process = &ted->build_process;
	assert(ted->build_shown);
	char buf[256];
	if (ted->building) {
		buffer->view_only = false; // disable view only temporarily so we can edit it
		bool any_text_inserted = false;
		while (1) {
			char incomplete[4];
			memcpy(incomplete, ted->build_incomplete_codepoint, sizeof incomplete);
			*ted->build_incomplete_codepoint = 0;

			i64 bytes_read = (i64)process_read(process, buf + 3, sizeof buf - 3);
			if (bytes_read == -2) {
				ted_seterr(ted, "Error reading command output: %s.", process_geterr(process));
				build_stop(ted);
				break;
			} else if (bytes_read == -1) {
				// no data right now.
				break;
			} else if (bytes_read == 0) {
				// end of file
				break;
			} else {
				any_text_inserted = true;
				// got some data.
				char *p = buf + 3 - strlen(incomplete);
				char *end = buf + 3 + bytes_read;
				// start off data with incomplete code point from last time
				memcpy(p, incomplete, strlen(incomplete));
				while (p != end) {
					char32_t c = 0;
					size_t ret = unicode_utf8_to_utf32(&c, p, (size_t)(end - p));
					if (ret == (size_t)-1) {
						// invalid UTF-8. skip this byte.
						++p;
					} else if (ret == (size_t)-2) {
						// incomplete UTF-8
						size_t leftovers = (size_t)(end - p);
						assert(leftovers < 4);
						memcpy(ted->build_incomplete_codepoint, p, leftovers);
						ted->build_incomplete_codepoint[leftovers] = '\0';
						p = end;
					} else {
						if (ret == 0) ret = 1;
						// got a code point
						buffer_insert_char_at_pos(buffer, buffer_end_of_file(buffer), c);
						p += ret;
					}
				}
			}
		}

		if (any_text_inserted) {
			// show bottom of output (only relevant if there are no build errors)
			buffer->cursor_pos = buffer_end_of_file(buffer);
			buffer_scroll_to_cursor(buffer);
		}

		char message[64];
		int status = process_check_status(process, message, sizeof message);
		if (status == 0) {
			// hasn't exited yet
		} else {
			buffer_insert_utf8_at_cursor(buffer, message);
			buffer_insert_utf8_at_cursor(buffer, "\n");
			if (!build_run_next_command_in_queue(ted)) {
				ted->building = false;
				// done command queue; check for errors
				for (u32 line_idx = 0; line_idx < buffer->nlines; ++line_idx) {
					Line *line = &buffer->lines[line_idx];
					if (line->len < 3) {
						continue;
					}
					bool is_error = true;
					char32_t *p = line->str, *end = p + line->len;
					
					{
						// rust errors look like:
						// "     --> file:line:column"
						while (p != end && *p == ' ') {
							++p;
						}
						if (end - p >= 4 && p[0] == '-' && p[1] == '-' && p[2] == '>' && p[3] == ' ') {
							p += 4;
						}
					}
	
					// check if we have something like main.c:5 or main.c(5)
					
					// get file name
					char32_t *filename_start = p;
					while (p != end) {
						if ((*p == ':' || *p == '(')
							&& p != line->str + 1) // don't catch "C:\thing\whatever.c" as "filename: C, line number: \thing\whatever.c"
							break;
						if (!is_source_path(*p)) {
							is_error = false;
							break;
						}
						++p;
					}
					if (p == end) is_error = false;
					u32 filename_len = (u32)(p - filename_start);
					if (filename_len == 0) is_error = false;
	
					if (is_error) {
						++p; // move past : or (
						int line_number = parse_nonnegative_integer(&p, end);
						if (p != end && line_number > 0) {
							// it's an error
							line_number -= 1; // line numbers in output start from 1.
							int column_number = 0;
							// check if there's a column number
							if (*p == ':') {
								++p; // move past :
								int num = parse_nonnegative_integer(&p, end);
								if (num > 0) {
									column_number = num - 1; // column numbers in output start from 1
								}
							}
							char *filename = str32_to_utf8_cstr(str32(filename_start, filename_len));
							if (filename) {
								char full_path[TED_PATH_MAX];
								path_full(ted->build_dir, filename, full_path, sizeof full_path);
								BuildError error = {
									.filename = str_dup(full_path),
									.pos = {.line = (u32)line_number, .index = (u32)column_number},
									.build_output_line = line_idx
								};
								arr_add(ted->build_errors, error);
							}
						}
					}
				}
	
				// go to the first error (if there is one)
				ted->build_error = 0;
				build_go_to_error(ted);
			}
		}
		buffer->view_only = true;
	}
	buffer_render(buffer, rect4(x1, y1, x2, y2));
}

