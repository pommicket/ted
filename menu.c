static void menu_close(Ted *ted) {
	ted_switch_to_buffer(ted, ted->prev_active_buffer);
	TextBuffer *buffer = ted->active_buffer;
	ted->prev_active_buffer = NULL;
	if (buffer) {
		buffer->scroll_x = ted->prev_active_buffer_scroll.x;
		buffer->scroll_y = ted->prev_active_buffer_scroll.y;
	}
	switch (ted->menu) {
	case MENU_NONE: assert(0); break;
	case MENU_OPEN:
	case MENU_SAVE_AS:
		file_selector_free(&ted->file_selector);
		buffer_clear(&ted->line_buffer);
		break;
	case MENU_WARN_UNSAVED:
		ted->warn_unsaved = 0;
		*ted->warn_unsaved_names = 0;
		break;
	case MENU_ASK_RELOAD:
		*ted->ask_reload = 0;
		break;
	case MENU_GOTO_DEFINITION:
		tag_selector_close(ted);
		break;
	case MENU_GOTO_LINE:
		buffer_clear(&ted->line_buffer);
		break;
	case MENU_COMMAND_SELECTOR: {
		Selector *selector = &ted->command_selector;
		buffer_clear(&ted->line_buffer);
		buffer_clear(&ted->argument_buffer);
		free(selector->entries); selector->entries = NULL; selector->n_entries = 0;
	} break;
	case MENU_SHELL:
		buffer_clear(&ted->line_buffer);
		break;
	}
	ted->menu = MENU_NONE;
	ted->selector_open = NULL;
}

static void menu_open(Ted *ted, Menu menu) {
	if (ted->menu)
		menu_close(ted);
	if (ted->find) find_close(ted);
	ted->autocomplete = false;
	ted->menu = menu;
	TextBuffer *prev_buf = ted->prev_active_buffer = ted->active_buffer;
	if (prev_buf)
		ted->prev_active_buffer_scroll = V2D(prev_buf->scroll_x, prev_buf->scroll_y);
	
	ted_switch_to_buffer(ted, NULL);
	*ted->warn_overwrite = 0; // clear warn_overwrite
	buffer_clear(&ted->line_buffer);
	switch (menu) {
	case MENU_NONE: assert(0); break;
	case MENU_OPEN:
		ted_switch_to_buffer(ted, &ted->line_buffer);
		ted->file_selector.create_menu = false;
		break;
	case MENU_SAVE_AS:
		ted_switch_to_buffer(ted, &ted->line_buffer);
		ted->file_selector.create_menu = true;
		break;
	case MENU_WARN_UNSAVED:
		assert(ted->warn_unsaved);
		assert(*ted->warn_unsaved_names);
		break;
	case MENU_ASK_RELOAD:
		assert(*ted->ask_reload);
		break;
	case MENU_GOTO_DEFINITION:
		tag_selector_open(ted);
		break;
	case MENU_GOTO_LINE:
		ted_switch_to_buffer(ted, &ted->line_buffer);
		break;
	case MENU_COMMAND_SELECTOR: {
		ted_switch_to_buffer(ted, &ted->line_buffer);
		buffer_insert_char_at_cursor(&ted->argument_buffer, '1');
		Selector *selector = &ted->command_selector;
		selector->enable_cursor = true;
		selector->cursor = 0;
	} break;
	case MENU_SHELL:
		ted_switch_to_buffer(ted, &ted->line_buffer);
		break;
	}
}

static void menu_escape(Ted *ted) {
	if (*ted->warn_overwrite) {
		// just close "are you sure you want to overwrite?"
		*ted->warn_overwrite = 0;
		ted_switch_to_buffer(ted, &ted->line_buffer);
	} else {
		menu_close(ted);
	}
}

static float menu_get_width(Ted *ted) {
	Settings *settings = &ted->settings;
	return minf(settings->max_menu_width, ted->window_width - 2.0f * settings->padding);
}

// returns the rectangle of the screen coordinates of the menu
static Rect menu_rect(Ted *ted) {
	Settings *settings = &ted->settings;
	float window_width = ted->window_width, window_height = ted->window_height;
	float padding = settings->padding;
	float menu_width = menu_get_width(ted);
	return rect(
		V2(window_width * 0.5f - 0.5f * menu_width, padding),
		V2(menu_width, window_height - 2 * padding)
	);
}

static void menu_update(Ted *ted) {
	Menu menu = ted->menu;
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	TextBuffer *line_buffer = &ted->line_buffer;

	assert(menu);
	switch (menu) {
	case MENU_NONE: break;
	case MENU_SAVE_AS: {
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
				ted_switch_to_buffer(ted, &ted->line_buffer);
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
	} break;
	case MENU_OPEN: {
		char *selected_file = file_selector_update(ted, &ted->file_selector);
		if (selected_file) {
			// open that file!
			menu_close(ted);
			ted_open_file(ted, selected_file);
			free(selected_file);
		}
	} break;
	case MENU_ASK_RELOAD: {
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
			if (buffer)
				buffer->last_write_time = time_last_modified(buffer->filename);
			break;
		case POPUP_CANCEL: assert(0); break;
		}
	} break;
	case MENU_WARN_UNSAVED:
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
		break;
	case MENU_GOTO_DEFINITION: {
		char *chosen_tag = tag_selector_update(ted);
		if (chosen_tag) {
			menu_close(ted);
			tag_goto(ted, chosen_tag);
			free(chosen_tag);
		}
	} break;
	case MENU_GOTO_LINE: {
		char *contents = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
		char *end;
		long line_number = strtol(contents, &end, 0);
		TextBuffer *buffer = ted->prev_active_buffer;
		if (line_number > 0 && *end == '\0' && line_number <= (long)buffer->nlines) {
			BufferPos pos = {(u32)line_number - 1, 0};
			
			if (line_buffer->line_buffer_submitted) {
				// let's go there!
				menu_close(ted);
				buffer_cursor_move_to_pos(buffer, pos);
				buffer_center_cursor(buffer);
			} else {
				// scroll to the line
				buffer_scroll_center_pos(buffer, pos);
			}
		}
		line_buffer->line_buffer_submitted = false;
		free(contents);
	} break;
	case MENU_COMMAND_SELECTOR: {
		Selector *selector = &ted->command_selector;
		SelectorEntry *entries = selector->entries = calloc(arr_count(command_names), sizeof *selector->entries);
		char *search_term = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
		if (entries) {
			SelectorEntry *entry = entries;
			for (size_t i = 0; i < arr_count(command_names); ++i) {
				char const *name = command_names[i].name;
				if (command_names[i].cmd != CMD_UNKNOWN && stristr(name, search_term)) {
					entry->name = name;
					entry->color = colors[COLOR_TEXT];
					++entry;
				}
			}
			selector->n_entries = (u32)(entry - entries);
		}

		char *chosen_command = selector_update(ted, &ted->command_selector);
		if (chosen_command) {
			Command c = command_from_str(chosen_command);
			if (c != CMD_UNKNOWN) {
				char *argument = str32_to_utf8_cstr(buffer_get_line(&ted->argument_buffer, 0)), *endp = NULL;
				long long arg = strtoll(argument, &endp, 0);

				if (*endp == '\0') {
					menu_close(ted);
					command_execute(ted, c, arg);
				}

				free(argument);
			}

			free(chosen_command);
		}
		free(search_term);
	} break;
	case MENU_SHELL:
		if (line_buffer->line_buffer_submitted) {
			char *command = str32_to_utf8_cstr(buffer_get_line(line_buffer, 0));
			menu_close(ted);
			strbuf_cpy(ted->build_dir, ted->cwd);
			build_start_with_command(ted, command);
			free(command);
		}
		break;
	}
}

static void menu_render(Ted *ted) {
	Menu menu = ted->menu;
	assert(menu);
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	float const window_width = ted->window_width, window_height = ted->window_height;
	Font *font_bold = ted->font_bold, *font = ted->font;
	float const char_height = text_font_char_height(font);
	float const char_height_bold = text_font_char_height(font_bold);
	float const line_buffer_height = ted_line_buffer_height(ted);
	
	// render backdrop
	gl_geometry_rect(rect(V2(0, 0), V2(window_width, window_height)), colors[COLOR_MENU_BACKDROP]);
	gl_geometry_draw();

	if (*ted->warn_overwrite) {
		char const *path = ted->warn_overwrite;
		char const *filename = path_filename(path);
		char title[64] = {0}, body[1024] = {0};
		strbuf_printf(title, "Overwrite %s?", filename);
		strbuf_printf(body, "Are you sure you want to overwrite %s?", path);
		popup_render(ted, POPUP_YES_NO_CANCEL, title, body);
		return;
	}

	
	float padding = settings->padding;
	Rect bounds = menu_rect(ted);
	float x1, y1, x2, y2;
	rect_coords(bounds, &x1, &y1, &x2, &y2);

	if (menu == MENU_OPEN || menu == MENU_SAVE_AS || menu == MENU_GOTO_DEFINITION || menu == MENU_COMMAND_SELECTOR) {
		// menu rectangle & border
		gl_geometry_rect(bounds, colors[COLOR_MENU_BG]);
		gl_geometry_rect_border(bounds, settings->border_thickness, colors[COLOR_BORDER]);
		gl_geometry_draw();
		
		x1 += padding;
		y1 += padding;
		x2 -= padding;
		y2 -= padding;
	}
		

	switch (menu) {
	case MENU_NONE: assert(0); break;
	case MENU_WARN_UNSAVED: {
		char title[64] = {0}, body[1024] = {0};
		strbuf_printf(title, "Save changes?");
		strbuf_printf(body, "Do you want to save your changes to %s?", ted->warn_unsaved_names);
		popup_render(ted, POPUP_YES_NO_CANCEL, title, body);
	} break;
	case MENU_ASK_RELOAD: {
		char title[64] = {0}, body[1024] = {0};
		strbuf_printf(title, "Reload %s?", ted->ask_reload);
		strbuf_printf(body, "%s has been changed by another program. Do you want to reload it?", ted->ask_reload);
		popup_render(ted, POPUP_YES_NO, title, body);
	} break;
	case MENU_OPEN:
	case MENU_SAVE_AS: {

		if (menu == MENU_OPEN) {
			text_utf8(font_bold, "Open...", x1, y1, colors[COLOR_TEXT]);
		} else if (menu == MENU_SAVE_AS) {
			text_utf8(font_bold, "Save as...", x1, y1, colors[COLOR_TEXT]);
		}
		text_render(font_bold);
			
		y1 += char_height_bold * 0.75f + padding;

		FileSelector *fs = &ted->file_selector;
		fs->bounds = rect4(x1, y1, x2, y2);
		file_selector_render(ted, fs);
	} break;
	case MENU_GOTO_DEFINITION: {
		tag_selector_render(ted, rect4(x1, y1, x2, y2));
	} break;
	case MENU_GOTO_LINE: {
		float menu_height = char_height + 2 * padding;
		Rect r = rect(V2(padding, window_height - menu_height - padding), V2(window_width - 2 * padding, menu_height));
		gl_geometry_rect(r, colors[COLOR_MENU_BG]);
		gl_geometry_rect_border(r, settings->border_thickness, colors[COLOR_BORDER]);
		char const *text = "Go to line...";
		v2 text_size = text_get_size_v2(font_bold, text);
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
		buffer_render(&ted->line_buffer, rect4(x1, y1, x2, y2));
	} break;
	case MENU_COMMAND_SELECTOR: {
		// argument field
		char const *text = "Argument";
		text_utf8(font_bold, text, x1, y1, colors[COLOR_TEXT]);
		float x = x1 + text_get_size_v2(font_bold, text).x + padding;
		buffer_render(&ted->argument_buffer, rect4(x, y1, x2, y1 + line_buffer_height));

		y1 += line_buffer_height + padding;

		Selector *selector = &ted->command_selector;
		selector->bounds = rect4(x1, y1, x2, y2);
		selector_render(ted, selector);

		text_render(font_bold);
	} break;
	case MENU_SHELL: {		
		bounds.size.y = line_buffer_height + 2 * padding;
		rect_coords(bounds, &x1, &y1, &x2, &y2);
		
		gl_geometry_rect(bounds, colors[COLOR_MENU_BG]);
		gl_geometry_rect_border(bounds, settings->border_thickness, colors[COLOR_BORDER]);
		gl_geometry_draw();
		
		x1 += padding;
		y1 += padding;
		x2 -= padding;
		y2 -= padding;
		
		char const *text = "Run";
		text_utf8(font_bold, text, x1, y1, colors[COLOR_TEXT]);
		x1 += text_get_size_v2(font_bold, text).x + padding;
		text_render(font_bold);
		
		buffer_render(&ted->line_buffer, rect4(x1, y1, x2, y2));
	} break;
	}
}
