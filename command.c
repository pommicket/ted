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
	Settings *settings = &ted->settings;


	switch (c) {
	case CMD_UNKNOWN:
	case CMD_COUNT:
		assert(0);
		break;
	case CMD_NOOP:
		break;
	
	case CMD_LEFT:
		if (buffer) buffer_cursor_move_left(buffer, argument);
		break;
	case CMD_RIGHT:
		if (buffer) buffer_cursor_move_right(buffer, argument);
		break;
	case CMD_UP:
		if (buffer) buffer_cursor_move_up(buffer, argument);
		break;
	case CMD_DOWN:
		if (buffer) buffer_cursor_move_down(buffer, argument);
		break;
	case CMD_SELECT_LEFT:
		if (buffer) buffer_select_left(buffer, argument);
		break;
	case CMD_SELECT_RIGHT:
		if (buffer) buffer_select_right(buffer, argument);
		break;
	case CMD_SELECT_UP:
		if (buffer) buffer_select_up(buffer, argument);
		break;
	case CMD_SELECT_DOWN:
		if (buffer) buffer_select_down(buffer, argument);
		break;
	case CMD_LEFT_WORD:
		if (buffer) buffer_cursor_move_left_words(buffer, argument);
		break;
	case CMD_RIGHT_WORD:
		if (buffer) buffer_cursor_move_right_words(buffer, argument);
		break;
	case CMD_SELECT_LEFT_WORD:
		if (buffer) buffer_select_left_words(buffer, argument);
		break;
	case CMD_SELECT_RIGHT_WORD:
		if (buffer) buffer_select_right_words(buffer, argument);
		break;
	case CMD_START_OF_LINE:
		if (buffer) buffer_cursor_move_to_start_of_line(buffer);
		break;
	case CMD_END_OF_LINE:
		if (buffer) buffer_cursor_move_to_end_of_line(buffer);
		break;
	case CMD_SELECT_START_OF_LINE:
		if (buffer) buffer_select_to_start_of_line(buffer);
		break;
	case CMD_SELECT_END_OF_LINE:
		if (buffer) buffer_select_to_end_of_line(buffer);
		break;
	case CMD_START_OF_FILE:
		if (buffer) buffer_cursor_move_to_start_of_file(buffer);
		break;
	case CMD_END_OF_FILE:
		if (buffer) buffer_cursor_move_to_end_of_file(buffer);
		break;
	case CMD_SELECT_START_OF_FILE:
		if (buffer) buffer_select_to_start_of_file(buffer);
		break;
	case CMD_SELECT_END_OF_FILE:
		if (buffer) buffer_select_to_end_of_file(buffer);
		break;
	case CMD_SELECT_ALL:
		if (buffer) buffer_select_all(buffer);
		break;

	case CMD_BACKSPACE:
		if (buffer) buffer_backspace_at_cursor(buffer, argument);
		break;
	case CMD_DELETE:
		if (buffer) buffer_delete_chars_at_cursor(buffer, argument);
		break;
	case CMD_BACKSPACE_WORD:
		if (buffer) buffer_backspace_words_at_cursor(buffer, argument);
		break;
	case CMD_DELETE_WORD:
		if (buffer) buffer_delete_words_at_cursor(buffer, argument);
		break;

	case CMD_PAGE_DOWN:
		if (buffer) buffer_page_down(buffer, argument);
		break;
	case CMD_PAGE_UP:
		if (buffer) buffer_page_up(buffer, argument);
		break;
	
	case CMD_OPEN:
		ted_menu_open(ted, MENU_OPEN);
		break;
	case CMD_SAVE:
		if (buffer) buffer_save(buffer);
		break;
	case CMD_UNDO:
		if (buffer) buffer_undo(buffer, argument);
		break;
	case CMD_REDO:
		if (buffer) buffer_redo(buffer, argument);
		break;
	
	case CMD_TEXT_SIZE_INCREASE: {
		i64 new_text_size = settings->text_size + argument;
		if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
			settings->text_size = (u16)new_text_size;
			ted_load_font(ted);
		}
	} break;
	case CMD_TEXT_SIZE_DECREASE: {
		i64 new_text_size = settings->text_size - argument;	
		if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
			settings->text_size = (u16)new_text_size;
			ted_load_font(ted);
		}
	} break;

	}

	if (buffer && buffer_haserr(buffer)) {
		strncpy(ted->error, buffer_geterr(buffer), sizeof ted->error - 1);
		buffer_clearerr(buffer);
	}
}
