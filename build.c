// :build command
// also handles :shell.

#include "ted-internal.h"

struct BuildError {
	char *path;
	u32 line;
	u32 column;
	/// if this is 1, then column == UTF-32 index.
	/// if this is 4, for example, then column 4 in a line starting with a tab would
	/// be the character right after the tab.
	u8 columns_per_tab;
	/// which line in the build output corresponds to this error
	u32 build_output_line;
};

void build_stop(Ted *ted) {
	if (ted->building)
		process_kill(&ted->build_process);
	ted->building = false;
	ted->build_shown = false;
	arr_foreach_ptr(ted->build_errors, BuildError, err) {
		free(err->path);
	}
	arr_clear(ted->build_errors);
	arr_foreach_ptr(ted->build_queue, char *, cmd) {
		free(*cmd);
	}
	arr_clear(ted->build_queue);
	if (ted->active_buffer == ted->build_buffer) {
		ted_switch_to_buffer(ted, NULL);
		ted_reset_active_buffer(ted);
	}
}

void build_queue_start(Ted *ted) {
	build_stop(ted);
}

void build_queue_command(Ted *ted, const char *command) {
	char *copy = str_dup(command);
	if (copy)
		arr_add(ted->build_queue, copy);
}

// returns true if there are still commands left in the queue
static bool build_run_next_command_in_queue(Ted *ted) {
	if (!ted->build_queue)
		return false;
	assert(!ted->build_process);
	assert(*ted->build_dir);
	char *command = ted->build_queue[0];
	arr_remove(ted->build_queue, 0);
	if (ted_save_all(ted)) {
		ProcessSettings settings = {0};
		settings.working_directory = ted->build_dir;
		ted->build_process = process_run_ex(command, &settings);
		const char *error = process_geterr(ted->build_process);
		if (!error) {
			ted->building = true;
			ted->build_shown = true;
			TextBuffer *build_buffer = ted->build_buffer;
			char32_t text[] = {'$', ' '};
			buffer_insert_text_at_cursor(build_buffer, str32(text, 2));
			buffer_insert_utf8_at_cursor(build_buffer, command);
			buffer_insert_char_at_cursor(build_buffer, '\n');
			buffer_set_view_only(build_buffer, true);
			free(command);
			return true;
		} else {
			ted_error(ted, "Couldn't start build: %s", error);
			build_stop(ted);
			return false;
		}
	} else {
		build_stop(ted);
		return false;
	}
}

void build_setup_buffer(Ted *ted) {
	// new empty build output buffer
	TextBuffer *build_buffer = ted->build_buffer;
	buffer_new_file(build_buffer, NULL);
	buffer_set_undo_enabled(build_buffer, false); // don't need undo events for build output buffer
}

void build_queue_finish(Ted *ted) {
	build_setup_buffer(ted);
	build_run_next_command_in_queue(ted); // run the first command
}

void build_set_working_directory(Ted *ted, const char *dir) {
	assert(strlen(dir) < TED_PATH_MAX - 1);
	strbuf_cpy(ted->build_dir, dir);
}

void build_start_with_command(Ted *ted, const char *command) {
	build_queue_start(ted);
	build_queue_command(ted, command);
	build_queue_finish(ted);
}

void build_start(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const char *command = settings->build_command;
	
	{
		char *root = ted_get_root_dir(ted);
		build_set_working_directory(ted, root);
		free(root);
	}
	
	if (*command == 0) {
		command = settings->build_default_command;
		typedef struct {
			const char *filename;
			const char *command;
		} Assoc;
		
		Assoc associations[] = {
		#if _WIN32
			{"make.bat", "make.bat"},
		#endif
			{"Cargo.toml", "cargo build"},
			{"Makefile", "make -j16"},
			{"go.mod", "go build"},
		};
		for (size_t i = 0; i < arr_count(associations); ++i) {
			Assoc *assoc = &associations[i];
			char path[TED_PATH_MAX];
			path_full(ted->build_dir, assoc->filename, path, sizeof path);
			if (fs_file_exists(path)) {
				command = assoc->command;
				break;
			}
		}
	}
	if (*command)
		build_start_with_command(ted, command);
}

static void build_go_to_error(Ted *ted) {
	if (ted->build_error < arr_len(ted->build_errors)) {
		BuildError error = ted->build_errors[ted->build_error];
		// open the file where the error happened
		if (ted_open_file(ted, error.path)) {
			TextBuffer *buffer = ted->active_buffer;
			assert(buffer);
			
			u32 index = error.column;
			
			if (error.columns_per_tab > 1) {
				// get correct index
				String32 line = buffer_get_line(buffer, error.line);
				u32 column = 0;
				index = 0;
				while (column < error.column) {
					if (index >= line.len)
						break;
					if (line.str[index] == '\t') {
						column += error.columns_per_tab;
					} else {
						column += 1;
					}
					++index;
				}
			}
			
			
			BufferPos pos = {
				.line = error.line,
				.index = index,
			};
			
			
			// move cursor to error
			buffer_cursor_move_to_pos(buffer, pos);
			buffer_center_cursor(buffer);

			// move cursor to error in build output
			TextBuffer *build_buffer = ted->build_buffer;
			BufferPos error_pos = {.line = error.build_output_line, .index = 0};
			buffer_cursor_move_to_pos(build_buffer, error_pos);
			buffer_center_cursor(build_buffer);
		}
	}
}

void build_next_error(Ted *ted) {
	if (ted->build_errors) {
		ted->build_error += 1;
		ted->build_error %= arr_len(ted->build_errors);
		build_go_to_error(ted);
	}
}

void build_prev_error(Ted *ted) {
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
	for (number_len = 0;
		&s[number_len] < end
		&& s[number_len] >= '0' && s[number_len] <= '9';
		++number_len) {
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
	const char *allowed_ascii_symbols_in_path = "./\\-_:";
	return c > CHAR_MAX || is32_alnum((wint_t)c)
		|| strchr(allowed_ascii_symbols_in_path, (char)c);
}

void build_check_for_errors(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	
	TextBuffer *buffer = ted->build_buffer;
	arr_clear(ted->build_errors);
	for (u32 line_idx = 0; line_idx < buffer_line_count(buffer); ++line_idx) {
		String32 line = buffer_get_line(buffer, line_idx);
		if (line.len < 3) {
			continue;
		}
		bool is_error = true;
		// well, for a bit of time i thought rust was weird
		// and treated tabs as 4 columns
		// apparently its just a bug, which ive filed here
		// https://github.com/rust-lang/rust/issues/109537
		// we could treat ::: references as 4-columns-per-tab,
		// but then that would be wrong if the bug gets fixed.
		// all this is to say that columns_per_tab is currently always 1,
		// but might be useful at some point.
		u8 columns_per_tab = 1;
		char32_t *p = line.str, *end = p + line.len;
		
		{
			// rust errors look like:
			// "     --> file:line:column"
			// and can also include stuff like
			// "     ::: file:line:column"
			while (p != end && *p == ' ') {
				++p;
			}
			if (end - p >= 4) {
				String32 first4 = str32(p, 4);
				if (str32_cmp_ascii(first4, "::: ") == 0 || str32_cmp_ascii(first4, "--> ") == 0) {
					p += 4;
				}
			}
		}
	
		// check if we have something like main.c:5 or main.c(5)
		
		// get file name
		char32_t *filename_start = p;
		while (p != end) {
			if ((*p == ':' || *p == '(')
				&& p != line.str + 1) // don't catch "C:\thing\whatever.c" as "filename: C, line number: \thing\whatever.c"
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
					const char *pfilename = filename;
					path_full(ted->build_dir, pfilename, full_path, sizeof full_path);
					// if the file does not exist, try stripping ../
					// this can solve "file not found" problems if your build command involves
					// cd'ing to a directory inside build_dir
					while (fs_path_type(full_path) == FS_NON_EXISTENT
						&& (str_has_prefix(pfilename, "../")
						#if _WIN32
						|| str_has_prefix(pfilename, "..\\")
						#endif
						)) {
						pfilename += 3;
						path_full(ted->build_dir, pfilename, full_path, sizeof full_path);
					}
										
					BuildError error = {
						.path = str_dup(full_path),
						.line = (u32)line_number,
						.column = (u32)column_number,
						.columns_per_tab = columns_per_tab,
						.build_output_line = line_idx
					};
					arr_add(ted->build_errors, error);
					free(filename);
				}
			}
		}
	}
	
	if (settings->jump_to_build_error) {
		// go to the first error (if there is one)
		ted->build_error = 0;
		build_go_to_error(ted);
	}
}

void build_frame(Ted *ted, float x1, float y1, float x2, float y2) {
	TextBuffer *buffer = ted->build_buffer;
	assert(ted->build_shown);
	char buf[256];
	if (ted->building) {
		buffer_set_view_only(buffer, false); // disable view only temporarily so we can edit it
		bool any_text_inserted = false;
		while (1) {
			char incomplete[4];
			memcpy(incomplete, ted->build_incomplete_codepoint, sizeof incomplete);
			*ted->build_incomplete_codepoint = 0;

			i64 bytes_read = (i64)process_read(ted->build_process, buf + 3, sizeof buf - 3);
			if (bytes_read == -2) {
				ted_error(ted, "Error reading command output: %s.", process_geterr(ted->build_process));
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
						buffer_insert_char_at_pos(buffer, buffer_pos_end_of_file(buffer), c);
						p += ret;
					}
				}
			}
		}

		if (any_text_inserted) {
			// show bottom of output (only relevant if there are no build errors)
			buffer_cursor_move_to_end_of_file(buffer);
		}

		ProcessExitInfo info = {0};
		int status = process_check_status(&ted->build_process, &info);
		if (status == 0) {
			// hasn't exited yet
		} else {
			buffer_insert_utf8_at_cursor(buffer, info.message);
			buffer_insert_utf8_at_cursor(buffer, "\n");
			if (!build_run_next_command_in_queue(ted)) {
				ted->building = false;
				// done command queue; check for errors
				build_check_for_errors(ted);
			}
		}
		buffer_set_view_only(buffer, true);
	}
	buffer_render(buffer, rect4(x1, y1, x2, y2));
}

