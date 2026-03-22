// TODO:
//  - keep separate index for each project?
#include "ted-internal.h"

struct FileFinder {
	Selector *files;
	Selector *new_files;
	Process *index_process;
	unsigned short index_readpos;
	bool open_when_index_finishes;
	char index_readbuf[8192];
};


static void selector_add_file(Selector *selector, const char *file, size_t len) {
	SelectorEntry entry = {0};
	entry.color = COLOR_TEXT;
	char *s = strn_dup(file, len);
	entry.name = s;
	selector_add_entry(selector, &entry);
	free(s);
}

static void filefinder_render(Ted *ted) {
	Rect bounds = selection_menu_render_bg(ted);
	FileFinder *file_finder = ted->file_finder;
	Selector *sel = file_finder->files;
	if (sel) {
		selector_set_bounds(sel, bounds);
		selector_render(ted, sel);
	} else {
		menu_close(ted);
	}
}

static void filefinder_select_file(Ted *ted, const SelectorEntry *entry) {
	char *root = ted_get_root_dir(ted);
	char *path = path_full(root, entry->name);
	free(root);
	menu_close(ted);
	ted_open_file(ted, path);
}

static void filefinder_update(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	if (file_finder->files) {
		char *selected = selector_update(ted, file_finder->files);
		if (selected) {
			free(selected);
			SelectorEntry entry;
			if (selector_get_cursor_entry(file_finder->files, &entry)) {
				filefinder_select_file(ted, &entry);
			}
		}
	}
}

static void filefinder_open(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	if (!selector_entry_count(file_finder->files)) {
		menu_close(ted);
		if (file_finder->open_when_index_finishes) {
			// we've been around this loop once before
			file_finder->open_when_index_finishes = false;
			return;
		}
		file_finder->open_when_index_finishes = true;
		filefinder_index(ted);
		return;
	}
	ted_switch_to_buffer(ted, ted->line_buffer);
	buffer_select_all(ted->active_buffer);
	file_finder->open_when_index_finishes = false;
}

static bool filefinder_close(Ted *ted) {
	(void)ted;
	return true;
}

void filefinder_init(Ted *ted) {
	FileFinder *file_finder = ted->file_finder = ted_calloc(ted, 1, sizeof *ted->file_finder);
	file_finder->files = selector_new(NULL);
	file_finder->new_files = selector_new(NULL);
	MenuInfo info = {
		.open = filefinder_open,
		.close = filefinder_close,
		.render = filefinder_render,
		.update = filefinder_update,
	};
	strbuf_cpy(info.name, MENU_FILEFINDER);
	menu_register(ted, &info);

}

void filefinder_index(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	process_kill(&file_finder->index_process);
	char *wd = ted_get_root_dir(ted);
	ProcessSettings settings = {0};
	settings.working_directory = wd;
	file_finder->index_process = process_run_ex("git ls-files", &settings);
	const char *err = process_geterr(file_finder->index_process);
	if (err) {
		ted_error(ted, "Error starting index process: %s", err);
		process_kill(&file_finder->index_process);
	}
	free(wd);
}

void filefinder_free(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	process_kill(&file_finder->index_process);
	selector_free(file_finder->files);
	selector_free(file_finder->new_files);
	free(file_finder);
	ted->file_finder = NULL;
}

void filefinder_frame(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	if (file_finder->index_process) {
		int read_calls = 0;
		long long nbytes = 0;
		// read the file names from the index process
		while ((++read_calls) < 100 && (nbytes = process_read(file_finder->index_process,
			file_finder->index_readbuf + file_finder->index_readpos,
			sizeof file_finder->index_readbuf - file_finder->index_readpos)) > 0) {
			size_t i = 0;
			while (i < (size_t)nbytes) {
				char *newline = memchr(file_finder->index_readbuf + i, '\n', (size_t)nbytes - i);
				if (!newline) break;
				if (newline == file_finder->index_readbuf + i) continue; // empty line, for some reason
				size_t line_len = (size_t)(newline - (file_finder->index_readbuf + i));
				if (newline[-1] == '\r') --line_len;
				if (line_len == 0) continue; // empty line, for some reason
				selector_add_file(file_finder->new_files, file_finder->index_readbuf + i, line_len);
				i = (size_t)(newline + 1 - file_finder->index_readbuf);
			}
			if ((size_t)nbytes - i > sizeof file_finder->index_readbuf / 2) {
				// super long file name is clogging up the read buffer - just ignore it
				file_finder->index_readpos = 0;
				nbytes = 0;
				i = 0;
			}
			// slide over bytes to start of buffer
			memmove(file_finder->index_readbuf,
				file_finder->index_readbuf + i,
				(size_t)nbytes - i);
			file_finder->index_readpos = (unsigned short)((size_t)nbytes - i);
		}
		ProcessExitInfo info;
		int status = process_check_status(&file_finder->index_process, &info);
		if (nbytes == -2 || status == -1) {
			ted_error(ted, "Error reading file list: %s",
				file_finder->index_process ? process_geterr(file_finder->index_process) : info.message);
			process_kill(&file_finder->index_process);
			selector_free(file_finder->new_files);
		}
		if (nbytes == -1 || status == 1) {
			// EOF - set files = new_files
			process_kill(&file_finder->index_process);
			selector_free(file_finder->files);
			file_finder->files = file_finder->new_files;
			if (file_finder->open_when_index_finishes) {
				menu_open(ted, MENU_FILEFINDER);
			}
		}
	}
}
