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
	Node *node = ted->active_node;
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
		if (ted->selector_open) selector_up(ted, ted->selector_open, argument);
		else if (buffer) buffer_cursor_move_up(buffer, argument);
		break;
	case CMD_DOWN:
		if (ted->selector_open) selector_down(ted, ted->selector_open, argument);
		else if (buffer) buffer_cursor_move_down(buffer, argument);
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

	case CMD_TAB:
		if (ted->replace && buffer == &ted->find_buffer) {
			buffer = ted->active_buffer = &ted->replace_buffer;
			buffer_select_all(buffer);
		} else if (buffer) {
			if (buffer->selection)
				buffer_indent_selection(buffer);
			else
				buffer_insert_char_at_cursor(buffer, '\t');
		}
		break;
	case CMD_BACKTAB:
		if (ted->replace && buffer == &ted->replace_buffer) {
			buffer = ted->active_buffer = &ted->find_buffer;
			buffer_select_all(buffer);
		} else if (buffer) {
			if (buffer->selection)
				buffer_dedent_selection(buffer);
		}
		break;
	case CMD_NEWLINE:
	case CMD_NEWLINE_BACK:
		if (ted->find) {
			if (buffer == &ted->find_buffer || buffer == &ted->replace_buffer) {
				if (c == CMD_NEWLINE)
					find_next(ted);
				else
					find_prev(ted);
			}
		} else if (buffer) {
			buffer_newline(buffer);
		}
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
		menu_open(ted, MENU_OPEN);
		break;
	case CMD_NEW:
		ted_new_file(ted);
		break;
	case CMD_SAVE:
		if (buffer) {
			if (buffer->filename && streq(buffer->filename, TED_UNTITLED)) {
				// don't worry, this won't catch files called "Untitled"; buffer->filename is the full path.
				command_execute(ted, CMD_SAVE_AS, 1);
				return;
			}
			buffer_save(buffer);
		}
		break;
	case CMD_SAVE_AS:
		if (buffer && !buffer->is_line_buffer) {
			menu_open(ted, MENU_SAVE_AS);
		}
		break;
	case CMD_SAVE_ALL:
		ted_save_all(ted);
		break;
	case CMD_QUIT:
		// pass argument of 2 to override dialog
		if (argument == 2) {
			ted->quit = true;
		} else {
			*ted->warn_unsaved_names = 0;
			bool *buffers_used = ted->buffers_used;
			bool first = true;
			for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
				if (buffers_used[i]) {
					buffer = &ted->buffers[i];
					if (buffer_unsaved_changes(buffer)) {
						strbuf_catf(ted->warn_unsaved_names, "%s%s", first ? "" : ", ", path_filename(buffer->filename));
						first = false;
					}
				}
			}
			if (*ted->warn_unsaved_names) {
				ted->warn_unsaved = CMD_QUIT;
				menu_open(ted, MENU_WARN_UNSAVED);
			} else {
				// no unsaved changes
				ted->quit = true;
			}
		}
		break;
	case CMD_UNDO:
		if (buffer) buffer_undo(buffer, argument);
		break;
	case CMD_REDO:
		if (buffer) buffer_redo(buffer, argument);
		break;
	case CMD_COPY:
		if (buffer) buffer_copy(buffer);
		break;
	case CMD_CUT:
		if (buffer) buffer_cut(buffer);
		break;
	case CMD_PASTE:
		if (buffer) buffer_paste(buffer);
		break;
	
	case CMD_TEXT_SIZE_INCREASE: {
		i64 new_text_size = settings->text_size + argument;
		if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
			settings->text_size = (u16)new_text_size;
			ted_load_fonts(ted);
		}
	} break;
	case CMD_TEXT_SIZE_DECREASE: {
		i64 new_text_size = settings->text_size - argument;	
		if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
			settings->text_size = (u16)new_text_size;
			ted_load_fonts(ted);
		}
	} break;

	case CMD_VIEW_ONLY:
		if (buffer) buffer->view_only = !buffer->view_only;
		break;

	case CMD_TAB_CLOSE: {
		if (ted->menu) {
			menu_close(ted);
		} else {
			if (node) {
				u16 tab_idx = node->active_tab;
				buffer = &ted->buffers[node->tabs[tab_idx]];
				// (an argument of 2 overrides the unsaved changes dialog)
				if (argument != 2 && buffer_unsaved_changes(buffer)) {
					// there are unsaved changes!
					ted->warn_unsaved = CMD_TAB_CLOSE;
					strbuf_printf(ted->warn_unsaved_names, "%s", path_filename(buffer->filename));
					menu_open(ted, MENU_WARN_UNSAVED);
				} else {
					node_tab_close(ted, node, node->active_tab);
				}
			} else {
				command_execute(ted, CMD_QUIT, 1);
				return;
			}
		}
	} break;
	case CMD_TAB_NEXT:
		if (ted->active_node) node_tab_next(ted, ted->active_node, argument);
		break;
	case CMD_TAB_PREV:
		if (ted->active_node) node_tab_prev(ted, ted->active_node, argument);
		break;
	case CMD_TAB_SWITCH:
		if (ted->active_node) node_tab_switch(ted, ted->active_node, argument);
		break;
	
	case CMD_FIND:
		if (buffer)
			find_open(ted, false);
		break;
	case CMD_FIND_REPLACE:
		if (buffer)
			find_open(ted, true);
		break;
	
	case CMD_ESCAPE:
		if (*ted->error_shown) {
			// dismiss error box
			*ted->error_shown = '\0';
		} else if (ted->menu) {
			menu_escape(ted);
		} else if (ted->find) {
			find_close(ted);
		} else if (ted->build_shown) {
			build_stop(ted);
		} else if (buffer) {
			buffer_disable_selection(buffer);
		}
		break;
	
	case CMD_BUILD:
		build_start(ted);
		break;
	case CMD_BUILD_NEXT_ERROR:
		build_next_error(ted);
		break;
	case CMD_BUILD_PREV_ERROR:
		build_prev_error(ted);
		break;
	case CMD_GOTO_DEFINITION:
		menu_open(ted, MENU_GOTO_DEFINITION);
		break;
	case CMD_GOTO_LINE:
		menu_open(ted, MENU_GOTO_LINE);
		break;

	case CMD_SPLIT_HORIZONTAL:
	case CMD_SPLIT_VERTICAL:
		if (node) {
			if (arr_len(node->tabs) > 1) { // need at least 2 tabs to split
				i32 left_idx = ted_new_node(ted);
				i32 right_idx = ted_new_node(ted);
				if (left_idx >= 0 && right_idx >= 0) {
					Node *left = &ted->nodes[left_idx];
					Node *right = &ted->nodes[right_idx];
					u16 active_tab = node->active_tab;
					// put active tab on the right
					arr_add(right->tabs, node->tabs[active_tab]);
					for (u32 i = 0; i < arr_len(node->tabs); ++i) {
						if (i != active_tab) {
							// put all other tabs on the left
							arr_add(left->tabs, node->tabs[i]);
						}
					}

					arr_clear(node->tabs);
					node->split_a = (u16)left_idx;
					node->split_b = (u16)right_idx;
					node->split_vertical = c == CMD_SPLIT_VERTICAL;
					node->split_pos = 0.5f;
					ted->active_node = &ted->nodes[right_idx];
				}
			}
		}
		break;
	}
}
