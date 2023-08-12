// deals with all of ted's menus ("open" menu, "save as" menu, etc.)

#include "ted-internal.h"

bool menu_is_open(Ted *ted, const char *menu_name) {
	if (!menu_is_any_open(ted))
		return false;
	return streq(ted->all_menus[ted->menu_open_idx].name, menu_name);
}

bool menu_is_any_open(Ted *ted) {
	return ted->menu_open_idx > 0;
}

void *menu_get_context(Ted *ted) {
	return ted->menu_context;
}

void menu_close(Ted *ted) {
	if (!menu_is_any_open(ted))
		return;
	const MenuInfo *menu = &ted->all_menus[ted->menu_open_idx];
	if (menu->close) {
		if (!menu->close(ted))
			return;
	}
	
	ted_switch_to_buffer(ted, ted->prev_active_buffer);
	ted->prev_active_buffer = NULL;
	ted->menu_open_idx = 0;
	ted->menu_context = NULL;
	ted->selector_open = NULL;
}

void menu_open_with_context(Ted *ted, const char *menu_name, void *context) {
	if (menu_is_open(ted, menu_name))
		return;
	u32 menu_idx = U32_MAX;
	if (*menu_name) {
		for (u32 i = 0; i < arr_len(ted->all_menus); ++i) {
			if (streq(ted->all_menus[i].name, menu_name)) {
				menu_idx = i;
				break;
			}
		}
	}
	if (menu_idx == U32_MAX) {
		ted_error(ted, "No such menu: %s", menu_name);
		return;
	}
	
	if (menu_is_any_open(ted))
		menu_close(ted);
	if (ted->find) find_close(ted);
	autocomplete_close(ted);
		
	const MenuInfo *info = &ted->all_menus[menu_idx];
	ted->menu_open_idx = menu_idx;
	ted->menu_context = context;
	ted->prev_active_buffer = ted->active_buffer;
	
	ted_switch_to_buffer(ted, NULL);
	*ted->warn_overwrite = 0; // clear warn_overwrite
	buffer_clear(ted->line_buffer);
	if (info->open) info->open(ted);
}

void menu_open(Ted *ted, const char *menu_name) {
	menu_open_with_context(ted, menu_name, NULL);
}

void menu_escape(Ted *ted) {
	if (!menu_is_any_open(ted)) return;
	
	if (*ted->warn_overwrite) {
		// just close "are you sure you want to overwrite?"
		*ted->warn_overwrite = 0;
		ted_switch_to_buffer(ted, ted->line_buffer);
	} else {
		menu_close(ted);
	}
}

void menu_update(Ted *ted) {
	if (!menu_is_any_open(ted)) return;
	
	const MenuInfo *info = &ted->all_menus[ted->menu_open_idx];
	
	if (info->update) info->update(ted);
}

Rect selection_menu_render_bg(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const float menu_width = ted_get_menu_width(ted);
	const float padding = settings->padding;
	const u32 *colors = settings->colors;
	const float window_width = ted->window_width, window_height = ted->window_height;
	Rect bounds = rect_xywh(
		window_width * 0.5f - 0.5f * menu_width, padding,
		menu_width, window_height - 2 * padding
	);
	
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	// menu rectangle & border
	gl_geometry_rect(bounds, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(bounds, settings->border_thickness, colors[COLOR_BORDER]);
	gl_geometry_draw();
	
	x1 += padding;
	y1 += padding;
	x2 -= padding;
	y2 -= padding;
	return rect4(x1, y1, x2, y2);
}

void menu_render(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	const float window_width = ted->window_width, window_height = ted->window_height;
	const MenuInfo *info = &ted->all_menus[ted->menu_open_idx];
	// render backdrop
	gl_geometry_rect(rect_xywh(0, 0, window_width, window_height), colors[COLOR_MENU_BACKDROP]);
	gl_geometry_draw();

	if (info->render)
		info->render(ted);
}

static void menu_edit_notify(void *context, TextBuffer *buffer, const EditInfo *info) {
	(void)info;
	
	Ted *ted = context;
	if (buffer == ted->line_buffer && menu_is_open(ted, MENU_SHELL)) {
		ted->shell_command_modified = true;
	}
	
}

void menu_shell_move(Ted *ted, int direction) {
	TextBuffer *line_buffer = ted->line_buffer;
	i64 pos = ted->shell_history_pos;
	pos += direction;
	if (pos >= 0 && pos <= arr_len(ted->shell_history)) {
		ted->shell_history_pos = (u32)pos;
		buffer_clear(line_buffer);
		if (pos == arr_len(ted->shell_history)) {
			// bottom of history; just clear line buffer
		} else {
			buffer_set_undo_enabled(line_buffer, false);
			buffer_insert_utf8_at_cursor(line_buffer, ted->shell_history[pos]);
			buffer_set_undo_enabled(line_buffer, true);
			ted->shell_command_modified = true;
		}
		// line_buffer->x/y1/2 are wrong (all 0), because of buffer_clear
		buffer_center_cursor_next_frame(line_buffer);
	}
}

void menu_shell_up(Ted *ted) {
	menu_shell_move(ted, -1);
}
void menu_shell_down(Ted *ted) {
	menu_shell_move(ted, +1);
}

static void open_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
	ted->file_selector.create_menu = false;
}

static void open_menu_update(Ted *ted) {
	char *selected_file = file_selector_update(ted, &ted->file_selector);
	if (selected_file) {
		// open that file!
		menu_close(ted);
		ted_open_file(ted, selected_file);
		free(selected_file);
	}
}

static void open_menu_render(Ted *ted) {
	FileSelector *fs = &ted->file_selector;
	strbuf_cpy(fs->title, "Open...");
	fs->bounds = selection_menu_render_bg(ted);
	file_selector_render(ted, fs);
}

static bool open_menu_close(Ted *ted) {
	file_selector_free(&ted->file_selector);
	buffer_clear(ted->line_buffer);
	return true;
}

static void save_as_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
	ted->file_selector.create_menu = true;
}

static void save_as_menu_update(Ted *ted) {
	if (*ted->warn_overwrite) {
		switch (popup_update(ted, POPUP_YES_NO_CANCEL)) {
		case POPUP_NONE:
			// no option selected
			break;
		case POPUP_YES: {
			// overwrite it!
			TextBuffer *buffer = ted->prev_active_buffer;
			if (buffer) {
				buffer_save_as(buffer, ted->warn_overwrite);
			}
			menu_close(ted);
		} break;
		case POPUP_NO:
			// back to the file selector
			*ted->warn_overwrite = '\0';
			ted_switch_to_buffer(ted, ted->line_buffer);
			break;
		case POPUP_CANCEL:
			// close "save as" menu
			menu_close(ted);
			break;
		}
	} else {
		char *selected_file = file_selector_update(ted, &ted->file_selector);
		if (selected_file) {
			TextBuffer *buffer = ted->prev_active_buffer;
			if (buffer) {
				if (fs_path_type(selected_file) != FS_NON_EXISTENT) {
					// file already exists! warn about overwriting it.
					strbuf_cpy(ted->warn_overwrite, selected_file);
					ted_switch_to_buffer(ted, NULL);
				} else {
					// create the new file.
					buffer_save_as(buffer, selected_file);
					menu_close(ted);
				}
			}
			free(selected_file);
		}
	}
}

static void save_as_menu_render(Ted *ted) {
	if (*ted->warn_overwrite) {
		const char *path = ted->warn_overwrite;
		const char *filename = path_filename(path);
		char title[64] = {0}, body[1024] = {0};
		strbuf_printf(title, "Overwrite %s?", filename);
		strbuf_printf(body, "Are you sure you want to overwrite %s?", path);
		popup_render(ted, POPUP_YES_NO_CANCEL, title, body);
		return;
	}

	FileSelector *fs = &ted->file_selector;
	strbuf_cpy(fs->title, "Save as...");
	fs->bounds = selection_menu_render_bg(ted);
	file_selector_render(ted, fs);
}

static bool save_as_menu_close(Ted *ted) {
	file_selector_free(&ted->file_selector);
	buffer_clear(ted->line_buffer);
	return true;
}

static void warn_unsaved_menu_update(Ted *ted) {
	assert(ted->warn_unsaved);
	assert(*ted->warn_unsaved_names);
	switch (popup_update(ted, POPUP_YES_NO_CANCEL)) {
	case POPUP_NONE: break;
	case POPUP_YES:
		// save changes
		switch (ted->warn_unsaved) {
		case CMD_TAB_CLOSE: {
			menu_close(ted);
			TextBuffer *buffer = ted->active_buffer;
			command_execute(ted, CMD_SAVE, 1); 
			if (!buffer_unsaved_changes(buffer)) {
				command_execute(ted, CMD_TAB_CLOSE, 1);
			}
		} break;
		case CMD_QUIT:
			menu_close(ted);
			if (ted_save_all(ted)) {
				command_execute(ted, CMD_QUIT, 1);
			}
			break;
		default:
			assert(0);
			break;
		}
		break;
	case POPUP_NO: {
		// pass in an argument of 2 to override dialog
		Command cmd = ted->warn_unsaved;
		menu_close(ted);
		command_execute(ted, cmd, 2);
	} break;
	case POPUP_CANCEL:
		menu_close(ted);
		break;
	}
}

static void warn_unsaved_menu_render(Ted *ted) {
	char title[64] = {0}, body[1024] = {0};
	strbuf_printf(title, "Save changes?");
	strbuf_printf(body, "Do you want to save your changes to %s?", ted->warn_unsaved_names);
	popup_render(ted, POPUP_YES_NO_CANCEL, title, body);
}

static bool warn_unsaved_menu_close(Ted *ted) {
	ted->warn_unsaved = 0;
	*ted->warn_unsaved_names = 0;
	return true;
}

static void ask_reload_menu_update(Ted *ted) {
	assert(*ted->ask_reload);
	TextBuffer *buffer = ted->prev_active_buffer;
	switch (popup_update(ted, POPUP_YES_NO)) {
	case POPUP_NONE: break;
	case POPUP_YES:
		menu_close(ted);
		if (buffer)
			buffer_reload(buffer);
		break;
	case POPUP_NO:
		menu_close(ted);
		break;
	case POPUP_CANCEL: assert(0); break;
	}
}

static void ask_reload_menu_render(Ted *ted) {
	char title[64] = {0}, body[1024] = {0};
	strbuf_printf(title, "Reload %s?", ted->ask_reload);
	strbuf_printf(body, "%s has been changed by another program. Do you want to reload it?", ted->ask_reload);
	popup_render(ted, POPUP_YES_NO, title, body);
}

static bool ask_reload_menu_close(Ted *ted) {
	*ted->ask_reload = 0;
	return true;
}

static void command_selector_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
	buffer_insert_char_at_cursor(ted->argument_buffer, '1');
	Selector *selector = &ted->command_selector;
	selector->enable_cursor = true;
	selector->cursor = 0;
}

static void command_selector_update(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const u32 *colors = settings->colors;
	TextBuffer *line_buffer = ted->line_buffer;
	Selector *selector = &ted->command_selector;
	SelectorEntry *entries = selector->entries = calloc(CMD_COUNT, sizeof *selector->entries);
	char *search_term = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
	if (entries) {
		SelectorEntry *entry = entries;
		for (Command c = 0; c < CMD_COUNT; ++c) {
			const char *name = command_to_str(c);
			if (c != CMD_UNKNOWN && *name && strstr_case_insensitive(name, search_term)) {
				entry->name = name;
				entry->color = colors[COLOR_TEXT];
				++entry;
			}
		}
		selector->n_entries = (u32)(entry - entries);
		selector_sort_entries_by_name(selector);
	}

	char *chosen_command = selector_update(ted, &ted->command_selector);
	if (chosen_command) {
		Command c = command_from_str(chosen_command);
		if (c != CMD_UNKNOWN) {
			char *argument = str32_to_utf8_cstr(buffer_get_line(ted->argument_buffer, 0)), *endp = NULL;
			long long arg = 1;
			bool execute = true;
			if (*argument) {
				strtoll(argument, &endp, 0);
				if (*endp != '\0')
					execute = false;
			}
			
			if (execute) {
				menu_close(ted);
				command_execute(ted, c, arg);
			}

			free(argument);
		}

		free(chosen_command);
	}
	free(search_term);
}

static void command_selector_render(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const float padding = settings->padding;
	const u32 *colors = settings->colors;
	const float line_buffer_height = ted_line_buffer_height(ted);
	Font *font_bold = ted->font_bold;
	
	Rect r = selection_menu_render_bg(ted);
	
	float x1=0, y1=0, x2=0, y2=0;
	rect_coords(r, &x1, &y1, &x2, &y2);
	
	// argument field
	const char *text = "Argument";
	text_utf8(font_bold, text, x1, y1, colors[COLOR_TEXT]);
	float x = x1 + text_get_size_vec2(font_bold, text).x + padding;
	buffer_render(ted->argument_buffer, rect4(x, y1, x2, y1 + line_buffer_height));

	y1 += line_buffer_height + padding;

	Selector *selector = &ted->command_selector;
	selector->bounds = rect4(x1, y1, x2, y2);
	selector_render(ted, selector);

	text_render(font_bold);
}

static bool command_selector_close(Ted *ted) {
	Selector *selector = &ted->command_selector;
	buffer_clear(ted->line_buffer);
	buffer_clear(ted->argument_buffer);
	free(selector->entries); selector->entries = NULL; selector->n_entries = 0;
	return true;
}

static void goto_line_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
}

static void goto_line_menu_update(Ted *ted) {
	TextBuffer *line_buffer = ted->line_buffer;
	
	char *contents = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
	char *end;
	long line_number = strtol(contents, &end, 0);
	TextBuffer *buffer = ted->prev_active_buffer;
	if (*contents != '\0' && *end == '\0') {
		if (line_number < 1) line_number = 1;
		if (line_number > (long)buffer_line_count(buffer)) line_number = (long)buffer_line_count(buffer); 
		BufferPos pos = {(u32)line_number - 1, 0};
		
		if (line_buffer_is_submitted(line_buffer)) {
			// let's go there!
			menu_close(ted);
			buffer_cursor_move_to_pos(buffer, pos);
			buffer_center_cursor(buffer);
		} else {
			// scroll to the line
			buffer_scroll_center_pos(buffer, pos);
		}
	}
	line_buffer_clear_submitted(line_buffer);
	free(contents);
}

static void goto_line_menu_render(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	const float padding = settings->padding;
	const u32 *colors = settings->colors;
	const float window_width = ted->window_width, window_height = ted->window_height;
	Font *font_bold = ted->font_bold;
	
	float menu_height = ted_line_buffer_height(ted) + 2 * padding;
	Rect r = rect_xywh(padding, window_height - menu_height - padding, window_width - 2 * padding, menu_height);
	gl_geometry_rect(r, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(r, settings->border_thickness, colors[COLOR_BORDER]);
	const char *text = "Go to line...";
	vec2 text_size = text_get_size_vec2(font_bold, text);
	float x1=0, y1=0, x2=0, y2=0;
	rect_coords(r, &x1, &y1, &x2, &y2);
	x1 += padding;
	y1 += padding;
	x2 -= padding;
	y2 -= padding;
	// render "Go to line" text
	text_utf8(font_bold, text, x1, 0.5f * (y1 + y2 - text_size.y), colors[COLOR_TEXT]);
	x1 += text_size.x + padding;
	gl_geometry_draw();
	text_render(font_bold);
	
	// line buffer
	buffer_render(ted->line_buffer, rect4(x1, y1, x2, y2));
}

static bool goto_line_menu_close(Ted *ted) {
	buffer_clear(ted->line_buffer);
	return true;
}

static void shell_menu_open(Ted *ted) {
	ted_switch_to_buffer(ted, ted->line_buffer);
	ted->shell_history_pos = arr_len(ted->shell_history);
	ted->shell_command_modified = false;
}

static void shell_menu_update(Ted *ted) {
	TextBuffer *line_buffer = ted->line_buffer;
	if (line_buffer_is_submitted(line_buffer)) {
		char *command = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
		if (ted->shell_history_pos == arr_len(ted->shell_history) || ted->shell_command_modified) {
			arr_add(ted->shell_history, command);
		}
		menu_close(ted);
		strbuf_cpy(ted->build_dir, ted->cwd);
		build_start_with_command(ted, command);
	}
}

static void shell_menu_render(Ted *ted) {
	const float line_buffer_height = ted_line_buffer_height(ted);
	const Settings *settings = ted_active_settings(ted);
	const float padding = settings->padding;
	const u32 *colors = settings->colors;
	const float width = ted_get_menu_width(ted);
	const float height = line_buffer_height + 2 * padding;
	Rect bounds = {
		.pos = {(ted->window_width - width) / 2, padding},
		.size = {width, height},
	};
	gl_geometry_rect(bounds, colors[COLOR_MENU_BG]);
	gl_geometry_rect_border(bounds, settings->border_thickness, colors[COLOR_BORDER]);
	gl_geometry_draw();
	rect_shrink(&bounds, padding);
	const char *text = "Run";
	text_utf8(ted->font_bold, text, bounds.pos.x, bounds.pos.y, colors[COLOR_TEXT]);
	rect_shrink_left(&bounds, text_get_size_vec2(ted->font_bold, text).x + padding);
	text_render(ted->font_bold);
	buffer_render(ted->line_buffer, bounds);
}

static bool shell_menu_close(Ted *ted) {
	buffer_clear(ted->line_buffer);
	return true;
}

void menu_register(Ted *ted, const MenuInfo *infop) {
	MenuInfo info = *infop;
	if (!*info.name) {
		ted_error(ted, "menu has no name");
		return;
	}
	info.name[sizeof info.name - 1] = '\0';
	arr_add(ted->all_menus, info);
}

void menu_init(Ted *ted) {
	// dummy 0 entry so that nothing has index 0.
	arr_add(ted->all_menus, (MenuInfo){0});
	
	ted_add_edit_notify(ted, menu_edit_notify, ted);
	
	MenuInfo save_as_menu = {
		.open = save_as_menu_open,
		.update = save_as_menu_update,
		.render = save_as_menu_render,
		.close = save_as_menu_close,
	};
	strbuf_cpy(save_as_menu.name, MENU_SAVE_AS);
	menu_register(ted, &save_as_menu);
	
	MenuInfo open_menu = {
		.open = open_menu_open,
		.update = open_menu_update,
		.render = open_menu_render,
		.close = open_menu_close,
	};
	strbuf_cpy(open_menu.name, MENU_OPEN);
	menu_register(ted, &open_menu);
	
	MenuInfo warn_unsaved_menu = {
		.open = NULL,
		.update = warn_unsaved_menu_update,
		.render = warn_unsaved_menu_render,
		.close = warn_unsaved_menu_close,
	};
	strbuf_cpy(warn_unsaved_menu.name, MENU_WARN_UNSAVED);
	menu_register(ted, &warn_unsaved_menu);
	
	MenuInfo ask_reload_menu = {
		.open = NULL,
		.update = ask_reload_menu_update,
		.render = ask_reload_menu_render,
		.close = ask_reload_menu_close,
	};
	strbuf_cpy(ask_reload_menu.name, MENU_ASK_RELOAD);
	menu_register(ted, &ask_reload_menu);
	
	MenuInfo command_selector_menu = {
		.open = command_selector_open,
		.update = command_selector_update,
		.render = command_selector_render,
		.close = command_selector_close,
	};
	strbuf_cpy(command_selector_menu.name, MENU_COMMAND_SELECTOR);
	menu_register(ted, &command_selector_menu);
	
	MenuInfo goto_line_menu = {
		.open = goto_line_menu_open,
		.update = goto_line_menu_update,
		.render = goto_line_menu_render,
		.close = goto_line_menu_close,
	};
	strbuf_cpy(goto_line_menu.name, MENU_GOTO_LINE);
	menu_register(ted, &goto_line_menu);
	
	MenuInfo shell_menu = {
		.open = shell_menu_open,
		.update = shell_menu_update,
		.render = shell_menu_render,
		.close = shell_menu_close,
	};
	strbuf_cpy(shell_menu.name, MENU_SHELL);
	menu_register(ted, &shell_menu);
}

void menu_quit(Ted *ted) {
	arr_clear(ted->all_menus);
}
