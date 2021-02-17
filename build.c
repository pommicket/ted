// clear build errors.
static void build_clear(Ted *ted) {
	arr_foreach_ptr(ted->build_errors, BuildError, err) {
		free(err->filename);
	}
	arr_clear(ted->build_errors);
}

static void build_start(Ted *ted) {
	// get rid of any old build errors
	build_clear(ted);
	
	bool cargo = false;
	
	chdir(ted->cwd);
	
	if (fs_file_exists("Cargo.toml")) {
		cargo = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR "Cargo.toml")) {
		chdir("..");
		cargo = true;
	} else if (fs_file_exists(".." PATH_SEPARATOR_STR ".." PATH_SEPARATOR_STR "Cargo.toml")) {
		chdir(".." PATH_SEPARATOR_STR "..");
		cargo = true;
	}
		
#if __unix__
	char *program = "/bin/sh";
	char *argv[5] = {
		program, "-c", NULL, NULL, NULL
	};
	if (cargo) {
		argv[2] = "cargo build";
	} else {
		argv[2] = "make";
	}
#else
	#error "TODO"
#endif

	process_exec(&ted->build_process, program, argv);
	ted->building = true;
	ted->build_shown = true;
	buffer_new_file(&ted->build_buffer, NULL);
	ted->build_buffer.store_undo_events = false; // don't need undo events for build output buffer
	ted->build_buffer.view_only = true;
}

static void build_stop(Ted *ted) {
	if (ted->building)
		process_kill(&ted->build_process);
	ted->building = false;
	ted->build_shown = false;
	build_clear(ted);
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
			ted->building = false;

			// check for errors
			for (u32 line_idx = 0; line_idx < buffer->nlines; ++line_idx) {
				Line *line = &buffer->lines[line_idx];
				if (line->len < 3) {
					continue;
				}
				bool is_error = true;
				u32 i = 0;
				char32_t *str = line->str; u32 len = line->len;
				
				// we have something like main.c:5
				
				// get file name
				while (i < len) {
					if (str[i] == ':') break;
					if (!is32_alnum(str[i]) && str[i] != '/' && str[i] != '.' && str[i] != '\\') {
						is_error = false;
						break;
					}
					++i;
				}
				if (i >= len) is_error = false;

				if (is_error) {
					u32 filename_len = i;
					u32 line_number_len = 0;
					++i;
					while (i < len) {
						if (str[i] == ':') break;
						if (str[i] < '0' || str[i] > '9') {
							is_error = false;
							break;
						}
						++i;
						++line_number_len;
					}
					if (i >= len) is_error = false;
					if (line_number_len == 0) is_error = false;
					

					if (is_error) {
						char *line_number_str = str32_to_utf8_cstr(str32(str + filename_len + 1, line_number_len));
						if (line_number_str) {
							int line_number = atoi(line_number_str);
							free(line_number_str);

							if (line_number > 0) {
								// it's an error

								// check if there's a column number
								u32 column_len = 0;
								++i;
								while (i < len) {
									if (str[i] >= '0' && str[i] <= '9')
										++column_len;
									else
										break;
									++i;
								}
								int column = 0;
								if (column_len) {
									// file:line:column syntax
									char *column_str = str32_to_utf8_cstr(str32(str + filename_len + 1 + line_number_len + 1, column_len));
									if (column_str) {
										column = atoi(column_str);
										column -= 1;
										if (column < 0) column = 0;
										free(column_str);
									}
								}

								line_number -= 1; // line numbers in output start from 1.
								char *filename = str32_to_utf8_cstr(str32(str, filename_len));
								if (filename) {
									char full_path[TED_PATH_MAX];
									ted_full_path(ted, filename, full_path, sizeof full_path);
									BuildError error = {
										.filename = str_dup(full_path),
										.pos = {.line = (u32)line_number, .index = (u32)column},
										.build_output_line = line_idx
									};
									arr_add(ted->build_errors, error);
								}
							}
						}
					}
				}
			}

			// go to the first error (if there is one)
			ted->build_error = 0;
			build_go_to_error(ted);
		}
		buffer->view_only = true;
	}
	buffer_render(buffer, rect4(x1, y1, x2, y2));
}
