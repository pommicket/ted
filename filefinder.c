// TODO:
//  - keep separate index for each project?
#include "ted-internal.h"

#include <stdatomic.h>

typedef struct BufferList BufferList;
struct BufferList {
	BufferList *next;
	size_t bytes_used;
	char buffer[8192];
};

typedef struct {
	const char **files;
	BufferList *buffers;
} FileList;

struct FileFinder {
	FileList files;
	FileList new_files;
	Selector *selector;
	Process *index_process;
	char *prev_search_term;
	unsigned short index_readpos;
	SDL_Thread *filter_thread;
	bool selector_entries_dirty;
	bool open_when_index_finishes;
	size_t filter_progress;
	size_t filter_count;
	const char *filter_results[500];
	char index_readbuf[8192];
};

static void *buffer_list_malloc(BufferList **buf, size_t n) {
	BufferList *head = *buf;
	if (n == 0) return NULL;
	if (n >= sizeof head->buffer) {
		// could allocate a special large buffer here, but for this use case it doesn't matter
	#if DEBUG
		die("buffer_list_malloc called with too big n");
	#endif
		return NULL;
	}
	size_t bytes_left = head ? sizeof head->buffer - head->bytes_used : 0;
	if (bytes_left >= n) {
		void *ret = head->buffer + head->bytes_used;
		head->bytes_used += n;
		return ret;
	} else {
		BufferList *new_head = malloc(sizeof *new_head);
		if (!new_head) {
			#if DEBUG
			die("out of memory");
			#endif
			return NULL;
		}
		new_head->next = head;
		void *ret = new_head->buffer;
		new_head->bytes_used = n;
		*buf = new_head;
		return ret;
	}
}

static void buffer_list_free(BufferList *list) {
	if (!list) return;
	buffer_list_free(list->next);
	free(list);
}

static void file_list_free(FileList *list) {
	buffer_list_free(list->buffers);
	arr_free(list->files);
	memset(list, 0, sizeof *list);
}

static void file_list_add(FileList *list, const char *file, size_t len) {
	char *name = buffer_list_malloc(&list->buffers, len + 1);
	if (!name) return;
	memcpy(name, file, len);
	name[len] = 0;
	arr_add(list->files, name);
}


static void file_list_sort(FileList *list) {
	arr_qsort(list->files, str_qsort_case_insensitive_cmp);
}

static void filefinder_render(Ted *ted) {
	Rect bounds = selection_menu_render_bg(ted);
	FileFinder *file_finder = ted->file_finder;
	Selector *sel = file_finder->selector;
	if (sel) {
		selector_set_bounds(sel, bounds);
		selector_render(ted, sel);
	} else {
		menu_close(ted);
	}
}

static void filefinder_select_file(Ted *ted, const SelectorEntry *entry) {
	FileFinder *file_finder = ted->file_finder;
	char *root = ted_get_root_dir(ted);
	char *path = path_full(root, file_finder->filter_results[entry->userdata]);
	free(root);
	menu_close(ted);
	ted_open_file(ted, path);
}

static void filefinder_update(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	char *search_term = buffer_contents_utf8_alloc(ted->line_buffer);
	if (!search_term) return;
	if (file_finder->selector_entries_dirty || !streq(search_term, file_finder->prev_search_term)) {
		// start new filter
		file_finder->filter_progress = 0;
		file_finder->filter_count = 0;
		free(file_finder->prev_search_term);
		file_finder->prev_search_term = search_term;
		file_finder->selector_entries_dirty = false;
		search_term = NULL; // prevent it from being freed
	}
	free(search_term);
	if (file_finder->filter_progress < arr_len(file_finder->files.files)) {
		double start_time = time_get_seconds();
		for (;
			file_finder->filter_progress < arr_len(file_finder->files.files);
			file_finder->filter_progress++) {
			if (file_finder->filter_count >= arr_count(file_finder->filter_results)) {
				// we've got enough results; finish up now.
				file_finder->filter_progress = arr_len(file_finder->files.files);
				break;
			}
			const char *file = file_finder->files.files[file_finder->filter_progress];
			if (strstr_case_insensitive(file, file_finder->prev_search_term)) {
				file_finder->filter_results[file_finder->filter_count++] = file;
			}
			if (file_finder->filter_progress % 1024 == 0
				&& time_get_seconds() - start_time >= 0.01) {
				// only spend 10ms per frame on filtering
				break;
			}
		}
		if (file_finder->filter_progress >= arr_len(file_finder->files.files)) {
			selector_clear_entries(file_finder->selector);
			for (size_t i = 0; i < file_finder->filter_count; i++) {
				SelectorEntry entry = {0};
				const char *path = file_finder->filter_results[i];
				char *dirname = strdup(path);
				if (strchr(dirname, PATH_SEPARATOR)) {
					path_dirname(dirname);
					if (*dirname && dirname[strlen(dirname)-1] == PATH_SEPARATOR)
						dirname[strlen(dirname)-1] = 0;
				} else {
					*dirname = 0;
				}
				entry.detail = dirname;
				entry.name = path_filename(path);
				entry.userdata = i;
				selector_add_entry(file_finder->selector, &entry);
				free(dirname);
			}
			selector_home(ted, file_finder->selector);
		}
	}
	char *selected = selector_update(ted, file_finder->selector);
	if (selected) {
		free(selected);
		SelectorEntry entry;
		if (selector_get_cursor_entry(file_finder->selector, &entry)) {
			filefinder_select_file(ted, &entry);
		}
	}
}

static void filefinder_open(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	if (!file_finder->files.files) {
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
	selector_home(ted, file_finder->selector);
	file_finder->open_when_index_finishes = false;
}

static bool filefinder_close(Ted *ted) {
	(void)ted;
	return true;
}

void filefinder_init(Ted *ted) {
	FileFinder *file_finder = ted->file_finder = ted_calloc(ted, 1, sizeof *ted->file_finder);
	file_finder->selector = selector_new(NULL);
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
	selector_free(file_finder->selector);
	file_list_free(&file_finder->files);
	file_list_free(&file_finder->new_files);
	free(file_finder->prev_search_term);
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
				if (newline == file_finder->index_readbuf + i) {
					// empty line, for some reason
					i++;
					continue;
				}
				size_t line_len = (size_t)(newline - (file_finder->index_readbuf + i));
				if (newline[-1] == '\r') --line_len;
				if (line_len == 0) {
					i++; // empty line, for some reason
					continue;
				}
				file_list_add(&file_finder->new_files, file_finder->index_readbuf + i, line_len);
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
			file_list_free(&file_finder->new_files);
		}
		if (nbytes == -1 || status == 1) {
			// EOF - set files = new_files
			process_kill(&file_finder->index_process);
			file_list_free(&file_finder->files);
			file_finder->files = file_finder->new_files;
			memset(&file_finder->new_files, 0, sizeof file_finder->new_files);
			file_list_sort(&file_finder->files);
			selector_clear_entries(file_finder->selector);
			file_finder->selector_entries_dirty = true;
			if (file_finder->open_when_index_finishes) {
				menu_open(ted, MENU_FILEFINDER);
			}
		}
	}
}
