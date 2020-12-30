Command command_from_str(char const *str) {
	// @OPTIMIZE: sort command_names, do a binary search
	for (int i = 0; i < CMD_COUNT; ++i) {
		if (streq(command_names[i].name, str))
			return command_names[i].cmd;
	}
	return CMD_UNKNOWN;
}

// Returns string representation of command
char const *command_to_str(Command c) {
	// NOTE: this probably won't need to be optimized.
	for (int i = 0; i < CMD_COUNT; ++i) {
		if (command_names[i].cmd == c)
			return command_names[i].name;
	}
	return "???";
}

void command_execute(Ted *ted, Command c, i64 argument) {
	TextBuffer *buffer = ted->active_buffer;
	switch (c) {
	case CMD_UNKNOWN:
	case CMD_COUNT:
		assert(0);
		break;
	case CMD_NOOP:
		break;
	
	case CMD_LEFT:
		buffer_cursor_move_left(buffer, argument);
		break;
	case CMD_RIGHT:
		buffer_cursor_move_right(buffer, argument);
		break;
	case CMD_UP:
		buffer_cursor_move_up(buffer, argument);
		break;
	case CMD_DOWN:
		buffer_cursor_move_down(buffer, argument);
		break;
	case CMD_SELECT_LEFT:
		buffer_select_left(buffer, argument);
		break;
	case CMD_SELECT_RIGHT:
		buffer_select_right(buffer, argument);
		break;
	case CMD_SELECT_UP:
		buffer_select_up(buffer, argument);
		break;
	case CMD_SELECT_DOWN:
		buffer_select_down(buffer, argument);
		break;
	case CMD_LEFT_WORD:
		buffer_cursor_move_left_words(buffer, argument);
		break;
	case CMD_RIGHT_WORD:
		buffer_cursor_move_right_words(buffer, argument);
		break;
	case CMD_SELECT_LEFT_WORD:
		buffer_select_left_words(buffer, argument);
		break;
	case CMD_SELECT_RIGHT_WORD:
		buffer_select_right_words(buffer, argument);
		break;
	case CMD_START_OF_LINE:
		buffer_cursor_move_to_start_of_line(buffer);
		break;
	case CMD_END_OF_LINE:
		buffer_cursor_move_to_end_of_line(buffer);
		break;
	case CMD_SELECT_START_OF_LINE:
		buffer_select_to_start_of_line(buffer);
		break;
	case CMD_SELECT_END_OF_LINE:
		buffer_select_to_end_of_line(buffer);
		break;
	case CMD_START_OF_FILE:
		buffer_cursor_move_to_start_of_file(buffer);
		break;
	case CMD_END_OF_FILE:
		buffer_cursor_move_to_end_of_file(buffer);
		break;
	case CMD_SELECT_START_OF_FILE:
		buffer_select_to_start_of_file(buffer);
		break;
	case CMD_SELECT_END_OF_FILE:
		buffer_select_to_end_of_file(buffer);
		break;

	case CMD_BACKSPACE:
		buffer_backspace_at_cursor(buffer, argument);
		break;
	case CMD_DELETE:
		buffer_delete_chars_at_cursor(buffer, argument);
		break;
	case CMD_BACKSPACE_WORDS:
		buffer_backspace_words_at_cursor(buffer, argument);
		break;
	case CMD_DELETE_WORDS:
		buffer_delete_words_at_cursor(buffer, argument);
		break;
	
	case CMD_SAVE:
		buffer_save(buffer);
		break;
	}
}
