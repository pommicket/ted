static void menu_open(Ted *ted, Menu menu) {
	if (ted->find) find_close(ted);
	ted->menu = menu;
	ted->prev_active_buffer = ted->active_buffer;
	ted->active_buffer = NULL;
	*ted->warn_overwrite = 0; // clear warn_overwrite
	switch (menu) {
	case MENU_NONE: assert(0); break;
	case MENU_OPEN:
		ted->active_buffer = &ted->line_buffer;
		break;
	case MENU_SAVE_AS:
		ted->active_buffer = &ted->line_buffer;
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
	}
}

static void menu_close(Ted *ted) {
	ted->active_buffer = ted->prev_active_buffer;
	ted->prev_active_buffer = NULL;
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
	}
	ted->menu = MENU_NONE;
	ted->selector_open = NULL;
}

static void menu_escape(Ted *ted) {
	if (*ted->warn_overwrite) {
		// just close "are you sure you want to overwrite?"
		*ted->warn_overwrite = 0;
		ted->active_buffer = &ted->line_buffer;
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
				ted->active_buffer = &ted->line_buffer;
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
						ted->active_buffer = NULL;
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
	}
}

static void menu_render(Ted *ted) {
	Menu menu = ted->menu;
	assert(menu);
	Settings const *settings = &ted->settings;
	u32 const *colors = settings->colors;
	float window_width = ted->window_width, window_height = ted->window_height;
	Font *font_bold = ted->font_bold;
	float char_height_bold = text_font_char_height(font_bold);

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
	Rect rect = menu_rect(ted);
	float menu_x1, menu_y1, menu_x2, menu_y2;
	rect_coords(rect, &menu_x1, &menu_y1, &menu_x2, &menu_y2);

	if (menu == MENU_OPEN || menu == MENU_SAVE_AS || menu == MENU_GOTO_DEFINITION) {
		// menu rectangle & border
		gl_geometry_rect(rect, colors[COLOR_MENU_BG]);
		gl_geometry_rect_border(rect, settings->border_thickness, colors[COLOR_BORDER]);
		gl_geometry_draw();
		
		menu_x1 += padding;
		menu_y1 += padding;
		menu_x2 -= padding;
		menu_y2 -= padding;
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
			text_utf8(font_bold, "Open...", menu_x1, menu_y1, colors[COLOR_TEXT]);
		} else if (menu == MENU_SAVE_AS) {
			text_utf8(font_bold, "Save as...", menu_x1, menu_y1, colors[COLOR_TEXT]);
		}
		text_render(font_bold);
			
		menu_y1 += char_height_bold * 0.75f + padding;

		FileSelector *fs = &ted->file_selector;
		fs->bounds = rect4(menu_x1, menu_y1, menu_x2, menu_y2);
		file_selector_render(ted, fs);
	} break;
	case MENU_GOTO_DEFINITION: {
		tag_selector_render(ted, rect4(menu_x1, menu_y1, menu_x2, menu_y2));
	} break;
	}
}
