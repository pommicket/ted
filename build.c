static void build_start(Ted *ted) {
#if __unix__
	char *program = "/bin/sh";
	char *argv[] = {
		program, "-c", "make", NULL
	};
#else
	#error "TODO"
#endif

	process_exec(&ted->build_process, program, argv);
	ted->building = true;
	ted->build_shown = true;
	buffer_new_file(&ted->build_buffer, NULL);
	ted->build_buffer.store_undo_events = false;
	ted->build_buffer.view_only = true;
}

static void build_stop(Ted *ted) {
	if (ted->building)
		process_kill(&ted->build_process);
	ted->building = false;
	ted->build_shown = false;
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
			memcpy(ted->build_incomplete_codepoint, incomplete, sizeof incomplete);
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
		}
		buffer->view_only = true;
	}
	buffer_render(buffer, rect4(x1, y1, x2, y2));
}
