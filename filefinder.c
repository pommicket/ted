#include "ted-internal.h"
#include <wctype.h>

typedef struct BufferList BufferList;
struct BufferList {
	BufferList *next;
	size_t bytes_used;
	char buffer[8192];
};

typedef struct {
	const char **files;
	// file name offsets for quick lookup
	// (files[i] + name_offsets[i] == path_filename(files[i]))
	u32 *name_offsets;
	// hashtable[i] = dynamic array of indices into FileList.files which have
	//                a 4-character substring that hashes to i (mod arr_count(hashtable))
	u32 *hashtable[1024];
	BufferList *buffers;
} FileList;

// every "project" (i.e. root directory, per ted_get_root_dir) has its own list of files.
typedef struct {
	char *root;
	FileList files;
	FileList new_files;
	Process *process;
	unsigned short readpos;
	char readbuf[8192];
} Project;

struct FileFinder {
	// probably we should clean up old projects at some point. but who knows what's the
	// best way of doing that...
	Project *projects;
	Selector *selector;
	char *prev_search_term;
	bool selector_entries_dirty;
	bool open_when_index_finishes;
	u32 filter_progress;
	u32 filter_count;
	i32 filter_bucket;
	long prev_project;
	const char *filter_results[500];
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
	arr_free(list->name_offsets);
	for (size_t i = 0; i < arr_count(list->hashtable); i++) {
		arr_free(list->hashtable[i]);
	}
	memset(list, 0, sizeof *list);
}

static void file_list_add(FileList *list, const char *file, u32 len) {
	char *path = buffer_list_malloc(&list->buffers, len + 1);
	if (!path) return;
	memcpy(path, file, len);
	path[len] = 0;
	u32 file_id = arr_len(list->files);
	arr_add(list->files, path);
	const char *filename = path_filename(path);
	u32 filename_len = (u32)(path + len - filename);
	arr_add(list->name_offsets, len - filename_len);
	u32 hash = 0;
	// add to hash table
	u32 c, hist[4] = {0};
	size_t n;
	// rolling hash of lowercase filename
	while ((n = unicode_utf8_to_utf32(&c, filename, 4)) <= 4) {
		if (c == 0) break;
		filename += n;
		// 3709206359 = 1000000007^3 mod 2^32
		hash -= hist[0] * 3709206359u;
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = hist[3];
		hist[3] = c =
		#if WCHAR_MAX < UNICODE_CODE_POINTS
			c > WCHAR_MAX ? c :
		#endif
			towlower(c);
		hash *= 1000000007u;
		hash += c;
		if (hist[0])
			arr_add(list->hashtable[hash % arr_count(list->hashtable)], file_id);
	}
}

static long filefinder_get_project_index(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	bool is_identified;
	char *root = ted_get_root_dir_ex(ted, &is_identified);
	if (!is_identified) {
		free(root);
		return -1;
	}
	arr_foreach_ptr(file_finder->projects, Project, p) {
		if (streq(p->root, root)) {
			free(root);
			return (long)(p - file_finder->projects);
		}
	}
	Project *project = arr_addp(file_finder->projects);
	project->root = root;
	return arr_len(file_finder->projects) - 1;
}

static Project *filefinder_get_project(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	long index = filefinder_get_project_index(ted);
	return index == -1 ? NULL : &file_finder->projects[index];
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
	const char *relpath = entry->userdata;
	char *root = ted_get_root_dir(ted);
	char *path = path_full(root, relpath);
	free(root);
	menu_close(ted);
	ted_open_file(ted, path);
	free(path);
}

static u32 file_list_len(FileList *list) {
	return arr_len(list->files);
}

static void filefinder_update(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	char *search_term = buffer_contents_utf8_alloc(ted->line_buffer);
	if (!search_term) return;
	bool match_vs_whole_file_path = search_term
		&& strcspn(search_term, ALL_PATH_SEPARATORS) != strlen(search_term);
	if (file_finder->selector_entries_dirty ||
		!file_finder->prev_search_term ||
		!streq(search_term, file_finder->prev_search_term)) {
		// start new filter
		file_finder->filter_progress = 0;
		file_finder->filter_count = 0;
		file_finder->filter_bucket = -1;
		free(file_finder->prev_search_term);
		file_finder->prev_search_term = search_term;
		file_finder->selector_entries_dirty = false;
		search_term = NULL; // prevent it from being freed
	}
	free(search_term);
	search_term = file_finder->prev_search_term;
	Project *project = filefinder_get_project(ted);
	if (!project) return;
	FileList *files = &project->files;
	if (match_vs_whole_file_path) {
		// can't use hash table
		file_finder->filter_bucket = -1;
	} else if (file_finder->filter_bucket < 0) {
		// figure out which hash table bucket to filter through
		u32 hash = 0, c, hist[4] = {0};
		size_t n;
		// rolling hash of lowercase search term
		const char *p = search_term;
		i32 best_candidate = -1;
		u32 best_entries = U32_MAX;
		while ((n = unicode_utf8_to_utf32(&c, p, 4)) <= 4) {
			if (c == 0) break;
			p += n;
			hash -= hist[0] * 3709206359u;
			hist[0] = hist[1];
			hist[1] = hist[2];
			hist[2] = hist[3];
			hist[3] = c =
			#if WCHAR_MAX < UNICODE_CODE_POINTS
				c > WCHAR_MAX ? c :
			#endif
				towlower(c);
			hash *= 1000000007u;
			hash += c;
			if (hist[0]) {
				u32 candidate = hash % arr_count(files->hashtable);
				u32 entries = arr_len(files->hashtable[candidate]);
				// search in matching hash table bucket with fewest entries
				if (entries < best_entries) {
					best_candidate = (i32)candidate;
					best_entries = entries;
				}
			}
		}
		if (best_entries == 0) {
			// no matching files
			selector_clear_entries(file_finder->selector);
		}
		file_finder->filter_bucket = best_candidate;
	}
	// hash table bucket to search, or -1 to search all files
	const i32 filter_bucket = file_finder->filter_bucket;
	u32 bucket_len = filter_bucket < 0 ? file_list_len(files) : arr_len(files->hashtable[filter_bucket]);
	if (file_finder->filter_progress < bucket_len) {
		double start_time = time_get_seconds();
		u32 i = file_finder->filter_progress;
		u32 filter_count = file_finder->filter_count;
		for (; i < bucket_len; i++) {
			u32 file_id = filter_bucket < 0 ? i : files->hashtable[filter_bucket][i];
			const char *path = files->files[file_id];
			const char *search_target = match_vs_whole_file_path ? path : path + files->name_offsets[file_id];
			if (strstr_case_insensitive(search_target, search_term)) {
				file_finder->filter_results[filter_count++] = path;
				if (filter_count >= arr_count(file_finder->filter_results)) {
					// we've got enough results; finish up now.
					i = bucket_len;
					break;
				}
			}
			if (i % 1024 == 0 && time_get_seconds() - start_time >= 0.01) {
				// only spend 10ms per frame on filtering
				break;
			}
		}
		file_finder->filter_count = filter_count;
		file_finder->filter_progress = i;
		if (file_finder->filter_progress >= bucket_len) {
			// finished filtering - update selector
			selector_clear_entries(file_finder->selector);
			for (i = 0; i < file_finder->filter_count; i++) {
				SelectorEntry entry = {0};
				const char *path = file_finder->filter_results[i];
				char *dirname = strdup(path);
				if (strcspn(dirname, ALL_PATH_SEPARATORS) != strlen(dirname)) {
					path_dirname(dirname);
					if (*dirname && strchr(ALL_PATH_SEPARATORS, dirname[strlen(dirname)-1]))
						dirname[strlen(dirname)-1] = 0;
				} else {
					*dirname = 0;
				}
				entry.detail = dirname;
				entry.name = path_filename(path);
				entry.userdata = (void *)path;
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
	Project *project = filefinder_get_project(ted);
	if (!project) {
		menu_close(ted);
		ted_error(ted, "Not sure what project root is. Make sure that there's a file in the root of your project matching root-identifiers in ted.cfg");
		return;
	}
	if (!project->files.files) {
		menu_close(ted);
		if (file_finder->open_when_index_finishes) {
			// we've been around this loop once before
			file_finder->open_when_index_finishes = false;
			return;
		}
		filefinder_index(ted, false);
		file_finder->open_when_index_finishes = true;
		return;
	}
	ted_switch_to_buffer(ted, ted->line_buffer);
	buffer_select_all(ted->active_buffer);
	selector_home(ted, file_finder->selector);
	file_finder->open_when_index_finishes = false;
}

static bool filefinder_close(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	file_finder->filter_progress = 0;
	file_finder->filter_count = 0;
	free(file_finder->prev_search_term);
	file_finder->prev_search_term = NULL;
	return true;
}


static int filefinder_selector_cmp(Selector *s, const SelectorEntry *a_entry, const SelectorEntry *b_entry) {
	FileFinder *file_finder = selector_get_userdata(s);
	const char *a_path = a_entry->userdata, *b_path = b_entry->userdata;
	const char *a_name = a_entry->name;
	const char *b_name = b_entry->name;
	const char *search_term = file_finder->prev_search_term;
	if (search_term && *search_term) {
		static bool (*const comparators[])(const char *, const char *) = {
			// first exact matches of search term
			streq,
			// then case-insensitive matches of search term
			streq_case_insensitive,
			// then exact extensions of search term
			str_has_prefix,
			// then case-insensitive extensions of search term
			str_has_prefix_case_insensitive
		};

		// first match against file name
		for (size_t i = 0; i < arr_count(comparators); i++) {
			int a_value = comparators[i](a_name, search_term);
			int b_value = comparators[i](b_name, search_term);
			if (a_value != b_value)
				return b_value - a_value;
		}
		// then match against full path
		for (size_t i = 0; i < arr_count(comparators); i++) {
			int a_value = comparators[i](a_path, search_term);
			int b_value = comparators[i](b_path, search_term);
			if (a_value != b_value)
				return b_value - a_value;
		}
	}
	// first compare by filenames
	int cmp = strcmp_case_insensitive(a_name, b_name);
	if (cmp) return cmp;
	// then compare paths directly, I guess
	cmp = strcmp_case_insensitive(a_path, b_path);
	if (cmp) return cmp;
	return strcmp(a_path, b_path);
}

void filefinder_init(Ted *ted) {
	FileFinder *file_finder = ted->file_finder = ted_calloc(ted, 1, sizeof *ted->file_finder);
	if (!file_finder) die("out of memory");
	file_finder->selector = selector_new(filefinder_selector_cmp);
	if (!file_finder->selector) die("out of memory");
	selector_set_filtering_disabled(file_finder->selector, true);
	selector_set_userdata(file_finder->selector, file_finder);
	MenuInfo info = {
		.open = filefinder_open,
		.close = filefinder_close,
		.render = filefinder_render,
		.update = filefinder_update,
	};
	file_finder->filter_bucket = -1;
	strbuf_cpy(info.name, MENU_FILEFINDER);
	menu_register(ted, &info);

}

void filefinder_index(Ted *ted, bool show_message) {
	FileFinder *file_finder = ted->file_finder;
	file_finder->open_when_index_finishes = false;
	Settings *settings = ted_active_settings(ted);
	Project *project = filefinder_get_project(ted);
	process_kill(&project->process);
	const char *command = rc_str(settings->filefinder_command, "");
	if (!*command) {
		return;
	}
	ProcessSettings process_settings = {0};
	process_settings.working_directory = project->root;
	project->process = process_run_ex(command, &process_settings);
	if (show_message) {
		ted_info(ted, "Indexing %s...", project->root);
	}
	const char *err = process_geterr(project->process);
	if (err) {
		ted_error(ted, "Error starting index process: %s", err);
		process_kill(&project->process);
	}
}

static void filefinder_project_free(Project *p) {
	free(p->root);
	process_kill(&p->process);
	file_list_free(&p->files);
	file_list_free(&p->new_files);
}

void filefinder_reset(Ted *ted) {
	if (menu_is_open(ted, MENU_FILEFINDER)) {
		menu_close(ted);
	}
	FileFinder *file_finder = ted->file_finder;
	arr_foreach_ptr(file_finder->projects, Project, p) {
		filefinder_project_free(p);
	}
	arr_free(file_finder->projects);
}

void filefinder_quit(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	selector_free(file_finder->selector);
	free(file_finder->prev_search_term);
	arr_foreach_ptr(file_finder->projects, Project, p) {
		filefinder_project_free(p);
	}
	arr_free(file_finder->projects);
	free(file_finder);
	ted->file_finder = NULL;
}

void filefinder_frame(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	Project *project = filefinder_get_project(ted);
	if (project && project->process) {
		int read_calls = 0;
		long long nbytes = 0;
		// read the file names from the index process
		while ((++read_calls) < 100 && (nbytes = process_read(project->process,
			project->readbuf + project->readpos,
			sizeof project->readbuf - project->readpos)) > 0) {
			size_t i = 0;
			while (i < (size_t)nbytes) {
				char *newline = memchr(project->readbuf + i, '\n', (size_t)nbytes - i);
				if (!newline) break;
				if (newline == project->readbuf + i) {
					// empty line, for some reason
					i++;
					continue;
				}
				size_t line_len = (size_t)(newline - (project->readbuf + i));
				if (newline[-1] == '\r') --line_len;
				if (line_len == 0) {
					i++; // empty line, for some reason
					continue;
				}
				file_list_add(&project->new_files, project->readbuf + i, (u32)line_len);
				i = (size_t)(newline + 1 - project->readbuf);
			}
			if ((size_t)nbytes - i > sizeof project->readbuf / 2) {
				// super long file name is clogging up the read buffer - just ignore it
				project->readpos = 0;
				nbytes = 0;
				i = 0;
			}
			// slide over bytes to start of buffer
			memmove(project->readbuf,
				project->readbuf + i,
				(size_t)nbytes - i);
			project->readpos = (unsigned short)((size_t)nbytes - i);
		}
		ProcessExitInfo info;
		int status = process_check_status(&project->process, &info);
		if (nbytes == -2 || status == -1) {
			ted_error(ted, "Error reading file list: %s",
				project->process ? process_geterr(project->process) : info.message);
			process_kill(&project->process);
			file_list_free(&project->new_files);
		}
		if (nbytes == -1 || status == 1) {
			// EOF - set files = new_files
			process_kill(&project->process);
			file_list_free(&project->files);
			project->files = project->new_files;
			memset(&project->new_files, 0, sizeof project->new_files);
			selector_clear_entries(file_finder->selector);
			file_finder->selector_entries_dirty = true;
			if (ted_message_type(ted) == MESSAGE_INFO) {
				// clear "Indexing project..." message box
				ted_clear_message(ted);
			}
			if (file_finder->open_when_index_finishes) {
				menu_open(ted, MENU_FILEFINDER);
			}
		}
	}
}

void filefinder_stop_index(Ted *ted) {
	FileFinder *file_finder = ted->file_finder;
	arr_foreach_ptr(file_finder->projects, Project, project) {
		if (project->process) {
			if (ted_message_type(ted) == MESSAGE_INFO) {
				// clear "Indexing project..." message box
				ted_clear_message(ted);
			}
			process_kill(&project->process);
			file_list_free(&project->new_files);
		}
	}
	file_finder->open_when_index_finishes = false;
}
