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

// get the string corresponding to this argument; returns NULL if it's not a string argument
char const *arg_get_string(Ted *ted, i64 argument) {
	if (argument < 0) return NULL;
	if (argument & ARG_STRING) {
		argument -= ARG_STRING;
		if (argument < ted->nstrings)
			return ted->strings[argument];
	}
	return NULL;
}

void command_execute(Ted *ted, Command c, i64 argument) {
	TextBuffer *buffer = ted->active_buffer;
	Node *node = ted->active_node;
	Settings *settings = ted_active_settings(ted);


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
		else if (ted->menu == MENU_SHELL && buffer == &ted->line_buffer)
			menu_shell_up(ted);
		else if (buffer) buffer_cursor_move_up(buffer, argument);
		break;
	case CMD_DOWN:
		if (ted->selector_open) selector_down(ted, ted->selector_open, argument);
		else if (ted->menu == MENU_SHELL && buffer == &ted->line_buffer)
			menu_shell_down(ted);
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
			ted_switch_to_buffer(ted, &ted->replace_buffer);
			buffer_select_all(buffer);
		} else if (ted->menu == MENU_COMMAND_SELECTOR && buffer == &ted->argument_buffer) {
			buffer = &ted->line_buffer;
			ted_switch_to_buffer(ted, buffer);
			buffer_select_all(buffer);
		} else if (ted->autocomplete) {
			autocomplete_select_cursor_completion(ted);
		} else if (buffer) {
			if (buffer->selection)
				buffer_indent_selection(buffer);
			else
				buffer_insert_tab_at_cursor(buffer);
		}
		break;
	case CMD_BACKTAB:
		if (ted->replace && buffer == &ted->replace_buffer) {
			ted_switch_to_buffer(ted, &ted->find_buffer);
			buffer_select_all(buffer);
		} else if (ted->menu == MENU_COMMAND_SELECTOR && buffer == &ted->line_buffer) {
			buffer = &ted->argument_buffer;
			ted_switch_to_buffer(ted, buffer);
			buffer_select_all(buffer);
		} else if (buffer) {
			if (buffer->selection)
				buffer_dedent_selection(buffer);
			else
				buffer_dedent_cursor_line(buffer);
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
			} else if (buffer) {
				buffer_newline(buffer);
			}
		} else if (buffer) {
			buffer_newline(buffer);
		}
		break;
	case CMD_COMMENT_SELECTION:
		if (buffer) buffer_toggle_comment_selection(buffer);
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
	case CMD_SELECT_PAGE_DOWN:
		if (buffer) buffer_select_page_down(buffer, argument);
		break;
	case CMD_SELECT_PAGE_UP:
		if (buffer) buffer_select_page_up(buffer, argument);
		break;
	
	case CMD_OPEN:
		menu_open(ted, MENU_OPEN);
		break;
	case CMD_NEW:
		ted_new_file(ted, NULL);
		break;
	case CMD_SAVE:
		if (buffer) {
			if (buffer_is_untitled(buffer)) {
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
	case CMD_RELOAD_ALL:
		ted_reload_all(ted);
		break;
	case CMD_QUIT:
		// pass argument of 2 to override dialog
		if (argument == 2 || ted->warn_unsaved == CMD_QUIT) {
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
	
	case CMD_SET_LANGUAGE:
		if (buffer && !buffer->is_line_buffer) {
			if (argument < 0 || argument >= LANG_COUNT)
				buffer->manual_language = -1;
			else
				buffer->manual_language = (i16)(argument + 1);
		}
		break;
	case CMD_AUTOCOMPLETE:
		if (ted->autocomplete)
			++ted->autocomplete_cursor;
		else
			autocomplete_open(ted);
		break;
	case CMD_AUTOCOMPLETE_BACK:
		if (ted->autocomplete)
			--ted->autocomplete_cursor;
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
	case CMD_OPEN_CONFIG: {
		char local_config_filename[TED_PATH_MAX];
		strbuf_printf(local_config_filename, "%s" PATH_SEPARATOR_STR TED_CFG, ted->local_data_dir);
		ted_open_file(ted, local_config_filename);
	} break;
	case CMD_COMMAND_SELECTOR:
		menu_open(ted, MENU_COMMAND_SELECTOR);
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
		} else if (ted->find) {
			find_close(ted);
		} else if (node) {
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
		} else if (ted->build_shown) {
			build_stop(ted);
		} else if (ted->nodes_used[0]) {
			// there are nodes open, but no active node.
			// do nothing.
		} else {
			// no nodes open
			command_execute(ted, CMD_QUIT, 1);
			return;
		}
	} break;
	case CMD_TAB_NEXT:
		if (node) node_tab_next(ted, node, argument);
		break;
	case CMD_TAB_PREV:
		if (node) node_tab_prev(ted, node, argument);
		break;
	case CMD_TAB_SWITCH:
		if (node) node_tab_switch(ted, node, argument);
		break;
	case CMD_TAB_MOVE_LEFT: {
		u16 active_tab = node->active_tab;
		if (active_tab > 0)
			node_tabs_swap(node, active_tab, active_tab - 1);
	} break;
	case CMD_TAB_MOVE_RIGHT: {
		u16 active_tab = node->active_tab;
		if ((uint)active_tab + 1 < arr_len(node->tabs))
			node_tabs_swap(node, active_tab, active_tab + 1);
	} break;
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
		} else if (ted->autocomplete) {
			ted->autocomplete = false;
		} else if (ted->menu) {
			menu_escape(ted);
		} else {
			if (ted->find) {
				find_close(ted);
			}
			if (ted->build_shown) {
				build_stop(ted);
			}
			if (buffer) {
				buffer_disable_selection(buffer);
			}
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
	case CMD_SHELL: {
		char const *str = arg_get_string(ted, argument);
		if (str) {
			strbuf_cpy(ted->build_dir, ted->cwd);
			build_start_with_command(ted, str);
		} else {
			menu_open(ted, MENU_SHELL);
		}
	} break;
	case CMD_GENERATE_TAGS:
		tags_generate(ted, true);
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
			node_split(ted, node, c == CMD_SPLIT_VERTICAL);
		}
		break;
	case CMD_SPLIT_JOIN:
		if (node) node_join(ted, node);
		break;
	case CMD_SPLIT_SWITCH:
		if (node) node_split_switch(ted);
		break;
	case CMD_SPLIT_SWAP:
		if (node) node_split_swap(ted);
		break;
	}
}
