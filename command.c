// the main highlight here is command_execute, which
// determines what to do when a command is executed.

#include "ted-internal.h"

typedef struct {
	const char *name;
	Command cmd;
} CommandName;
static CommandName command_names[] = {
	{"unknown", CMD_UNKNOWN},
	{"noop", CMD_NOOP},
	{"left", CMD_LEFT},
	{"right", CMD_RIGHT},
	{"up", CMD_UP},
	{"down", CMD_DOWN},
	{"select-left", CMD_SELECT_LEFT},
	{"select-right", CMD_SELECT_RIGHT},
	{"select-up", CMD_SELECT_UP},
	{"select-down", CMD_SELECT_DOWN},
	{"left-word", CMD_LEFT_WORD},
	{"right-word", CMD_RIGHT_WORD},
	{"up-blank-line", CMD_UP_BLANK_LINE},
	{"down-blank-line", CMD_DOWN_BLANK_LINE},
	{"select-left-word", CMD_SELECT_LEFT_WORD},
	{"select-right-word", CMD_SELECT_RIGHT_WORD},
	{"start-of-line", CMD_START_OF_LINE},
	{"end-of-line", CMD_END_OF_LINE},
	{"select-start-of-line", CMD_SELECT_START_OF_LINE},
	{"select-end-of-line", CMD_SELECT_END_OF_LINE},
	{"start-of-file", CMD_START_OF_FILE},
	{"end-of-file", CMD_END_OF_FILE},
	{"select-start-of-file", CMD_SELECT_START_OF_FILE},
	{"select-end-of-file", CMD_SELECT_END_OF_FILE},
	{"select-page-up", CMD_SELECT_PAGE_UP},
	{"select-page-down", CMD_SELECT_PAGE_DOWN},
	{"select-all", CMD_SELECT_ALL},
	{"select-up-blank-line", CMD_SELECT_UP_BLANK_LINE},
	{"select-down-blank-line", CMD_SELECT_DOWN_BLANK_LINE},
	{"matching-bracket", CMD_MATCHING_BRACKET},
	{"clear-selection", CMD_CLEAR_SELECTION},
	{"page-up", CMD_PAGE_UP},
	{"page-down", CMD_PAGE_DOWN},
	{"previous-position", CMD_PREVIOUS_POSITION},
	{"tab", CMD_TAB},
	{"backtab", CMD_BACKTAB},
	{"insert-text", CMD_INSERT_TEXT},
	{"newline", CMD_NEWLINE},
	{"newline-back", CMD_NEWLINE_BACK},
	{"comment-selection", CMD_COMMENT_SELECTION},
	{"backspace", CMD_BACKSPACE},
	{"delete", CMD_DELETE},
	{"backspace-word", CMD_BACKSPACE_WORD},
	{"delete-word", CMD_DELETE_WORD},
	{"open", CMD_OPEN},
	{"new", CMD_NEW},
	{"save", CMD_SAVE},
	{"save-as", CMD_SAVE_AS},
	{"save-all", CMD_SAVE_ALL},
	{"reload-all", CMD_RELOAD_ALL},
	{"quit", CMD_QUIT},
	{"set-language", CMD_SET_LANGUAGE},
	{"command-selector", CMD_COMMAND_SELECTOR},
	{"open-config", CMD_OPEN_CONFIG},
	{"undo", CMD_UNDO},
	{"redo", CMD_REDO},
	{"copy", CMD_COPY},
	{"cut", CMD_CUT},
	{"paste", CMD_PASTE},
	{"autocomplete", CMD_AUTOCOMPLETE},
	{"autocomplete-back", CMD_AUTOCOMPLETE_BACK},
	{"find-usages", CMD_FIND_USAGES},
	{"copy-path", CMD_COPY_PATH},
	{"goto-definition", CMD_GOTO_DEFINITION},
	{"goto-definition-at-cursor", CMD_GOTO_DEFINITION_AT_CURSOR},
	{"goto-declaration-at-cursor", CMD_GOTO_DECLARATION_AT_CURSOR},
	{"goto-type-definition-at-cursor", CMD_GOTO_TYPE_DEFINITION_AT_CURSOR},
	{"lsp-reset", CMD_LSP_RESET},
	{"find", CMD_FIND},
	{"find-replace", CMD_FIND_REPLACE},
	{"tab-close", CMD_TAB_CLOSE},
	{"tab-switch", CMD_TAB_SWITCH},
	{"tab-next", CMD_TAB_NEXT},
	{"tab-prev", CMD_TAB_PREV},
	{"tab-move-left", CMD_TAB_MOVE_LEFT},
	{"tab-move-right", CMD_TAB_MOVE_RIGHT},
	{"increase-text-size", CMD_TEXT_SIZE_INCREASE},
	{"decrease-text-size", CMD_TEXT_SIZE_DECREASE},
	{"view-only", CMD_VIEW_ONLY},
	{"build", CMD_BUILD},
	{"build-prev-error", CMD_BUILD_PREV_ERROR},
	{"build-next-error", CMD_BUILD_NEXT_ERROR},
	{"shell", CMD_SHELL},
	{"generate-tags", CMD_GENERATE_TAGS},
	{"goto-line", CMD_GOTO_LINE},
	{"split-horizontal", CMD_SPLIT_HORIZONTAL},
	{"split-vertical", CMD_SPLIT_VERTICAL},
	{"split-join", CMD_SPLIT_JOIN},
	{"split-switch", CMD_SPLIT_SWITCH},
	{"split-swap", CMD_SPLIT_SWAP},
	{"escape", CMD_ESCAPE},
	{"macro-record", CMD_MACRO_RECORD},
	{"macro-stop", CMD_MACRO_STOP},
	{"macro-execute", CMD_MACRO_EXECUTE},
	{"increment-number", CMD_INCREMENT_NUMBER},
	{"decrement-number", CMD_DECREMENT_NUMBER},
	{"rename-symbol", CMD_RENAME_SYMBOL},
};

static_assert_if_possible(arr_count(command_names) == CMD_COUNT)

int command_name_cmp(const void *av, const void *bv) {
	const CommandName *a = av, *b = bv;
	return strcmp(a->name, b->name);
}

void command_init(void) {
	qsort(command_names, arr_count(command_names), sizeof *command_names, command_name_cmp);
}

Command command_from_str(const char *str) {
	int lo = 0;
	int hi = CMD_COUNT;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		int cmp = strcmp(command_names[mid].name, str);
		if (cmp < 0) {
			lo = mid + 1;
		} else if (cmp > 0) {
			hi = mid;
		} else {
			return command_names[mid].cmd;
		}
	}
	return CMD_UNKNOWN;
}

const char *command_to_str(Command c) {
	// NOTE: this probably won't need to be optimized.
	for (int i = 0; i < CMD_COUNT; ++i) {
		if (command_names[i].cmd == c)
			return command_names[i].name;
	}
	return "???";
}

void command_execute(Ted *ted, Command c, i64 argument) {
	const CommandArgument arg = {
		.number = argument,
		.string = NULL,
	};
	const CommandContext ctx = {0};
	command_execute_ex(ted, c, &arg, &ctx);
}

void command_execute_string_argument(Ted *ted, Command c, const char *string) {
	assert(string);
	const CommandArgument arg = {
		.number = 0,
		.string = string,
	};
	const CommandContext ctx = {0};
	command_execute_ex(ted, c, &arg, &ctx);
}

void command_execute_ex(Ted *ted, Command c, const CommandArgument *full_argument, const CommandContext *context) {
	TextBuffer *buffer = ted->active_buffer;
	Node *node = ted->active_node;
	Settings *settings = ted_active_settings(ted);
	if (ted->recording_macro)
		macro_add(ted, c, full_argument);
	i64 argument = full_argument->number;
	const char *argument_str = full_argument->string;
	/*
	it's important that when we're playing back a macro,
	we only execute commands specifically from the macro.
	for example, suppose the user opens the find menu and searches for "apple".
	this might generate the macro:
	     open_find_menu()
	     insert_text("apple")
	     newline()
	     find_next("apple") // (generated by find.c)
	if we ran these commands as-is, we'd end up searching for "apple" twice!
	*/
	if (ted->executing_macro && !context->running_macro)
		return;
	
	switch (c) {
	case CMD_UNKNOWN:
	case CMD_COUNT:
		assert(0);
		break;
	case CMD_NOOP:
		break;
	
	case CMD_LEFT:
		if (buffer) buffer_cursor_move_left(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_RIGHT:
		if (buffer) buffer_cursor_move_right(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_UP:
		if (ted->selector_open) selector_up(ted, ted->selector_open, argument);
		else if (menu_is_open(ted, MENU_SHELL) && buffer == ted->line_buffer)
			menu_shell_up(ted);
		else if (buffer) buffer_cursor_move_up(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_DOWN:
		if (ted->selector_open) selector_down(ted, ted->selector_open, argument);
		else if (menu_is_open(ted, MENU_SHELL) && buffer == ted->line_buffer)
			menu_shell_down(ted);
		else if (buffer) buffer_cursor_move_down(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_UP_BLANK_LINE:
		if (buffer) buffer_cursor_move_up_blank_lines(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_DOWN_BLANK_LINE:
		if (buffer) buffer_cursor_move_down_blank_lines(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_LEFT:
		if (buffer) buffer_select_left(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_CLEAR_SELECTION:
		if (buffer) buffer_deselect(buffer);
		break;
	case CMD_SELECT_RIGHT:
		if (buffer) buffer_select_right(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_UP:
		if (buffer) buffer_select_up(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_DOWN:
		if (buffer) buffer_select_down(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_LEFT_WORD:
		if (buffer) buffer_cursor_move_left_words(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_RIGHT_WORD:
		if (buffer) buffer_cursor_move_right_words(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_LEFT_WORD:
		if (buffer) buffer_select_left_words(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_RIGHT_WORD:
		if (buffer) buffer_select_right_words(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_START_OF_LINE:
		if (buffer) buffer_cursor_move_to_start_of_line(buffer);
		autocomplete_close(ted);
		break;
	case CMD_END_OF_LINE:
		if (buffer) buffer_cursor_move_to_end_of_line(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_START_OF_LINE:
		if (buffer) buffer_select_to_start_of_line(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_END_OF_LINE:
		if (buffer) buffer_select_to_end_of_line(buffer);
		autocomplete_close(ted);
		break;
	case CMD_START_OF_FILE:
		if (buffer) buffer_cursor_move_to_start_of_file(buffer);
		autocomplete_close(ted);
		break;
	case CMD_END_OF_FILE:
		if (buffer) buffer_cursor_move_to_end_of_file(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_START_OF_FILE:
		if (buffer) buffer_select_to_start_of_file(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_END_OF_FILE:
		if (buffer) buffer_select_to_end_of_file(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_ALL:
		if (buffer) buffer_select_all(buffer);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_UP_BLANK_LINE:
		if (buffer) buffer_select_up_blank_lines(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_SELECT_DOWN_BLANK_LINE:
		if (buffer) buffer_select_down_blank_lines(buffer, argument);
		autocomplete_close(ted);
		break;
	case CMD_PREVIOUS_POSITION:
		if (buffer)
			buffer_cursor_move_to_prev_pos(buffer);
		break;
	case CMD_MATCHING_BRACKET:
		if (buffer)
			buffer_cursor_move_to_matching_bracket(buffer);
		break;
	case CMD_INSERT_TEXT: {
		const char *str = argument_str;
		if (str && buffer) {
			buffer_insert_utf8_at_cursor(buffer, str);
		}
		}
		break;
	case CMD_COPY_PATH:
		if (buffer && buffer_is_named_file(buffer)) {
			SDL_SetClipboardText(buffer_get_path(buffer));
		} else {
			SDL_SetClipboardText(ted->cwd);
		}
		break;
	case CMD_TAB:
		if (ted->replace && buffer == ted->find_buffer) {
			ted_switch_to_buffer(ted, ted->replace_buffer);
			buffer_select_all(buffer);
		} else if (menu_is_open(ted, MENU_COMMAND_SELECTOR) && buffer == ted->argument_buffer) {
			buffer = ted->line_buffer;
			ted_switch_to_buffer(ted, buffer);
			buffer_select_all(buffer);
		} else if (autocomplete_is_open(ted) || autocomplete_has_phantom(ted)) {
			autocomplete_select_completion(ted);
		} else if (buffer) {
			if (buffer_has_selection(buffer))
				buffer_indent_selection(buffer);
			else
				buffer_insert_tab_at_cursor(buffer);
		}
		break;
	case CMD_BACKTAB:
		if (ted->replace && buffer == ted->replace_buffer) {
			ted_switch_to_buffer(ted, ted->find_buffer);
			buffer_select_all(buffer);
		} else if (menu_is_open(ted, MENU_COMMAND_SELECTOR) && buffer == ted->line_buffer) {
			buffer = ted->argument_buffer;
			ted_switch_to_buffer(ted, buffer);
			buffer_select_all(buffer);
		} else if (buffer) {
			if (buffer_has_selection(buffer))
				buffer_dedent_selection(buffer);
			else
				buffer_dedent_cursor_line(buffer);
		}
		break;
	case CMD_NEWLINE:
	case CMD_NEWLINE_BACK:
		if (ted->find) {
			if (buffer == ted->find_buffer || buffer == ted->replace_buffer) {
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
	case CMD_INCREMENT_NUMBER:
		if (buffer) buffer_change_number_at_cursor(buffer, argument);
		break;
	case CMD_DECREMENT_NUMBER:
		if (buffer) buffer_change_number_at_cursor(buffer, -argument);
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
		ted->last_save_time = ted->frame_time;
		if (buffer) {
			if (!buffer_is_named_file(buffer)) {
				command_execute(ted, CMD_SAVE_AS, 1);
				return;
			}
			buffer_save(buffer);
		}
		break;
	case CMD_SAVE_AS:
		ted->last_save_time = ted->frame_time;
		if (buffer && !buffer_is_line_buffer(buffer)) {
			menu_open(ted, MENU_SAVE_AS);
		}
		break;
	case CMD_SAVE_ALL:
		ted->last_save_time = ted->frame_time;
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
			bool first = true;
			
			arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
				buffer = *pbuffer;
				if (buffer_unsaved_changes(buffer)) {
					const char *path = buffer_display_filename(buffer);
					strbuf_catf(ted->warn_unsaved_names, "%s%s", first ? "" : ", ", path);
					first = false;
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
		if (buffer && !buffer_is_line_buffer(buffer)) {
			if (argument <= 0 || argument > LANG_USER_MAX || !language_is_valid((Language)argument))
				buffer_set_manual_language(buffer, 0);
			else
				buffer_set_manual_language(buffer, (u32)argument);
		}
		break;
	case CMD_AUTOCOMPLETE:
		if (autocomplete_is_open(ted))
			autocomplete_next(ted);
		else
			autocomplete_open(ted, TRIGGER_INVOKED);
		break;
	case CMD_AUTOCOMPLETE_BACK:
		if (autocomplete_is_open(ted))
			autocomplete_prev(ted);
		break;	
	case CMD_GOTO_DEFINITION:
		menu_open(ted, MENU_GOTO_DEFINITION);
		break;
	case CMD_GOTO_DEFINITION_AT_CURSOR: {
		if (buffer && buffer_is_named_file(buffer)) {
			buffer_goto_word_at_cursor(buffer, GOTO_DEFINITION);
		}
		} break;
	case CMD_GOTO_DECLARATION_AT_CURSOR: {
		if (buffer && buffer_is_named_file(buffer)) {
			buffer_goto_word_at_cursor(buffer, GOTO_DECLARATION);
		}
		} break;
	case CMD_GOTO_TYPE_DEFINITION_AT_CURSOR: {
		if (buffer && buffer_is_named_file(buffer)) {
			buffer_goto_word_at_cursor(buffer, GOTO_TYPE_DEFINITION);
		}
		} break;
	case CMD_LSP_RESET:
		for (int i = 0; i < TED_LSP_MAX; ++i) {
			LSP *lsp = ted->lsps[i];
			if (lsp) {
				lsp_free(lsp);
				ted->lsps[i] = NULL;
			}
		}
		break;
	case CMD_FIND_USAGES:
		usages_find(ted);
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
		strbuf_printf(local_config_filename, "%s%c" TED_CFG, ted->local_data_dir, PATH_SEPARATOR);
		ted_open_file(ted, local_config_filename);
	} break;
	case CMD_COMMAND_SELECTOR:
		menu_open(ted, MENU_COMMAND_SELECTOR);
		break;

	case CMD_TEXT_SIZE_INCREASE:
		if (argument != 0) {
			i64 new_text_size = settings->text_size + argument;
			if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
				settings->text_size = (u16)new_text_size;
				ted_change_text_size(ted, (float)new_text_size);
			}
		}
		break;
	case CMD_TEXT_SIZE_DECREASE:
		if (argument != 0) {
			i64 new_text_size = settings->text_size - argument;	
			if (new_text_size >= TEXT_SIZE_MIN && new_text_size <= TEXT_SIZE_MAX) {
				settings->text_size = (u16)new_text_size;
				ted_change_text_size(ted, (float)new_text_size);
			}
		}
		break;

	case CMD_VIEW_ONLY:
		if (buffer) buffer_set_view_only(buffer, !buffer_is_view_only(buffer));
		break;

	case CMD_TAB_CLOSE: {
		if (menu_is_any_open(ted)) {
			menu_close(ted);
		} else if (ted->find) {
			find_close(ted);
		} else if (node) {
			u16 tab_idx = node->active_tab;
			buffer = node->tabs[tab_idx];
			// (an argument of 2 overrides the unsaved changes dialog)
			if (argument != 2 && buffer_unsaved_changes(buffer)) {
				// there are unsaved changes!
				ted->warn_unsaved = CMD_TAB_CLOSE;
				strbuf_printf(ted->warn_unsaved_names, "%s", buffer_display_filename(buffer));
				menu_open(ted, MENU_WARN_UNSAVED);
			} else {
				node_tab_close(ted, node, node->active_tab);
			}
		} else if (ted->build_shown) {
			build_stop(ted);
		} else if (arr_len(ted->nodes)) {
			// there are nodes open, but no active node.
			// do nothing.
		} else {
			// no nodes open
			command_execute(ted, CMD_QUIT, 1);
			return;
		}
	} break;
	case CMD_TAB_NEXT:
		if (node && argument > I32_MIN && argument < I32_MAX)
			node_tab_next(ted, node, (i32)argument);
		break;
	case CMD_TAB_PREV:
		if (node && argument > I32_MIN && argument < I32_MAX)
			node_tab_prev(ted, node, (i32)argument);
		break;
	case CMD_TAB_SWITCH:
		if (node && argument > I32_MIN && argument < I32_MAX)
			node_tab_switch(ted, node, (i32)argument);
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
		definition_cancel_lookup(ted);
		usages_cancel_lookup(ted);
		if (*ted->message_shown) {
			// dismiss message box
			*ted->message_shown = '\0';
		} else if (autocomplete_is_open(ted)) {
			autocomplete_close(ted);
		} else if (menu_is_any_open(ted)) {
			menu_escape(ted);
		} else {
			if (ted->find) {
				find_close(ted);
			}
			if (ted->build_shown) {
				build_stop(ted);
			}
			if (buffer) {
				buffer_deselect(buffer);
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
		const char *str = argument_str;
		if (str) {
			build_set_working_directory(ted, ted->cwd);
			build_start_with_command(ted, str);
		} else {
			menu_open(ted, MENU_SHELL);
		}
	} break;
	case CMD_GENERATE_TAGS:
		tags_generate(ted, true);
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
	case CMD_MACRO_RECORD:
		macro_start_recording(ted, (u32)argument);
		break;
	case CMD_MACRO_STOP:
		macro_stop_recording(ted);
		break;
	case CMD_MACRO_EXECUTE:
		macro_execute(ted, (u32)argument);
		break;
	case CMD_RENAME_SYMBOL:
		if (buffer && buffer_lsp(buffer))
			menu_open(ted, MENU_RENAME_SYMBOL);
		break;
	}
}
