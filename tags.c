#include "ted.h"
#include "pcre-inc.h"

static const char *tags_filename(Ted *ted, bool error_if_does_not_exist) {
	change_directory(ted->cwd);
	const char *filename = "tags";
	ted_path_full(ted, ".", ted->tags_dir, sizeof ted->tags_dir);
	if (!fs_file_exists(filename)) {
		filename = "../tags";
		ted_path_full(ted, "..", ted->tags_dir, sizeof ted->tags_dir);
		if (!fs_file_exists(filename)) {
			filename = "../../tags";
			ted_path_full(ted, "../..", ted->tags_dir, sizeof ted->tags_dir);
			if (!fs_file_exists(filename)) {
				if (error_if_does_not_exist)
					ted_seterr(ted, "No tags file. Try running ctags.");
				filename = NULL;
			}
		}
	}
	return filename;
}

// is this a file we can generate tags for?
static bool is_source_file(const char *filename) {
	const char *dot = strchr(filename, '.');
	const char *const extensions[] = {
		"py", "c", "h", "cpp", "hpp", "cc", "hh", "cxx", "hxx", "C", "H",
		"rb", "rs", "go", "lua", "s", "asm", "js", "pl", "cs", "sh", "java", "php"
	};
	if (!dot) return false;
	for (size_t i = 0; i < arr_count(extensions); ++i) {
		if (streq(dot + 1, extensions[i])) {
			return true;
		}
	}	
	return false;
}


static void tags_generate_at_dir(Ted *ted, bool run_in_build_window, const char *dir, int depth) {
	const Settings *settings = ted_active_settings(ted);
	if (depth >= settings->tags_max_depth) {
		return;
	}
	FsDirectoryEntry **entries = fs_list_directory(dir);
	if (entries) {
		char command[2048]; // 2048 is the limit on Windows XP, apparently
		
	#if __unix__
		// ctags.emacs's sorting depends on the locale 
		// (ctags-universal doesn't)
		const char *cmd_prefix = "LC_ALL=C ctags --append";
	#else
		const char *cmd_prefix = "ctags --append";
	#endif
		bool any_files = false;
		strcpy(command, cmd_prefix);
		for (int i = 0; entries[i]; ++i) {
			FsDirectoryEntry *entry = entries[i];
			char path[TED_PATH_MAX];
			path_full(dir, entry->name, path, sizeof path);
			if (entry->name[0] != '.') { // ignore hidden directories and . and ..
				switch (entry->type) {
				case FS_FILE: {
					if (is_source_file(entry->name)) {
						size_t cmdlen = strlen(command), pathlen = strlen(path);
						any_files = true;
						// make sure command doesn't get too long
						if (cmdlen + pathlen + 5 >= sizeof command) {
							if (run_in_build_window)
								build_queue_command(ted, command);
							else
								system(command);
							strbuf_printf(command, "%s %s", cmd_prefix, path);
						} else {
							command[cmdlen++] = ' ';
							memcpy(command + cmdlen, path, pathlen+1);
						}
					}
				} break;
				case FS_DIRECTORY:
					tags_generate_at_dir(ted, run_in_build_window, path, depth+1);
					break;
				default: break;
				}
			}
		}
		if (any_files) {
			if (run_in_build_window)
				build_queue_command(ted, command);
			else
				system(command);
		}
	}
}

// generate/re-generate tags.
void tags_generate(Ted *ted, bool run_in_build_window) {
	const char *filename = tags_filename(ted, false);
	if (!filename) {
		strcpy(ted->tags_dir, ted->cwd);
	}
	change_directory(ted->tags_dir);
	strcpy(ted->build_dir, ted->tags_dir);
	remove("tags"); // delete old tags file
	if (run_in_build_window) build_queue_start(ted);
	tags_generate_at_dir(ted, run_in_build_window, ted->tags_dir, 0);
	if (run_in_build_window) build_queue_finish(ted);
	change_directory(ted->cwd);
}

static int tag_try(FILE *fp, const char *tag) {
	if (ftell(fp) != 0) {
		while (1) {
			int c = getc(fp);
			if (c == EOF || c == '\n')
				break;
		}
	}
	
	size_t tag_len = strlen(tag);
	if (!feof(fp)) {
		char line[256];
		long pos = ftell(fp);
		if (fgets(line, sizeof line, fp)) {
			fseek(fp, pos, SEEK_SET);
			size_t len = strcspn(line, "\t");
			if (tag_len > len)
				len = tag_len;
			return strncmp(tag, line, len);
		}
	}
	return -1;
}

size_t tags_beginning_with(Ted *ted, const char *prefix, char **out, size_t out_size) {
	assert(out_size);
	const char *tags_name = tags_filename(ted, true);
	if (!tags_name) return 0;
	FILE *file = fopen(tags_name, "rb");
	if (!file) return 0;
	
	fseek(file, 0, SEEK_END);
	size_t file_size = (size_t)ftell(file);
	// binary search for prefix in file
	size_t lo = 0;
	size_t hi = file_size;
	size_t mid = 0;
	bool exact = false;
	while (lo < hi) {
		mid = (lo + hi) / 2;
		fseek(file, (long)mid, SEEK_SET);
		int cmp = tag_try(file, prefix);
		if (cmp > 0) {
			lo = mid + 1;
		} else if (cmp < 0) {
			hi = mid;
		} else {
			exact = true;
			break;
		}
	}
	char line[1024];
	fseek(file, (long)mid, SEEK_SET);
	if (!exact && mid > 0)
		fgets(line, sizeof line, file); // go to next line
	
	size_t nmatches = 0;
	size_t prefix_len = strlen(prefix);
	bool done = false;
	char prev_match[1024];
	
	while (!done && fgets(line, sizeof line, file)) {
		switch (strncmp(line, prefix, prefix_len)) {
		case 0: {
			char *tag = strn_dup(line, strcspn(line, "\t"));
			if (nmatches == 0 || !streq(tag, prev_match)) { // don't include duplicate tags
				strbuf_cpy(prev_match, tag);
				if (out) out[nmatches] = tag;
				else free(tag);
				++nmatches;
			} else {
				free(tag);
			}
			if (nmatches >= out_size) done = true;
		} break;
		case +1:
			done = true;
			break;
		}
	}
	fclose(file);
	return nmatches;
}

// returns true if the tag exists.
bool tag_goto(Ted *ted, const char *tag) {
	bool already_regenerated_tags;
	already_regenerated_tags = false;
top:;
	const Settings *settings = ted_active_settings(ted);
	
	const char *tags_name = tags_filename(ted, true);
	if (!tags_name) return false;
	FILE *file = fopen(tags_name, "rb");
	if (!file) return false;
	
	fseek(file, 0, SEEK_END);
	size_t file_size = (size_t)ftell(file);
	// binary search for tag in file
	size_t lo = 0;
	size_t hi = file_size;
	bool success = false;
	
	while (lo < hi) {
		size_t mid = (lo + hi)/2;
		fseek(file, (long)mid, SEEK_SET);
		int cmp = tag_try(file, tag);
		if (cmp > 0) {
			lo = mid+1;
		} else if (cmp < 0) {
			hi = mid;
		} else {
			// we found it!
			char tag_entry[1024];
			if (fgets(tag_entry, sizeof tag_entry, file)) {
			
				// the tag is of the format:
				// tag name\tfile name\taddress
				// or
				// tag name\tfile name\taddress;" additional information
				
				char *name = tag_entry;
				char *name_end = strchr(name, '\t');
				if (name_end) {
					*name_end = '\0';
					char *filename = name_end + 1;
					char *filename_end = strchr(filename, '\t');
					if (filename_end) {
						*filename_end = '\0';
						char *address = filename_end + 1;
						char *address_end = address;
						int backslashes = 0;
						while (1) {
							bool is_end = false;
							switch (*address_end) {
							case '\n':
							case '\r':
								is_end = true;
								break;
							case '\\':
								++backslashes;
								break;
							case '/':
								if (address_end != address && backslashes % 2 == 0)
									is_end = true;
								break;
							}
							if (is_end) break;
							if (*address_end != '\\') backslashes = 0;
							++address_end;
						}
						*address_end = '\0';
						if (address_end - address > 2 && address_end[-2] == ';' && address_end[-1] == '"') {
							address_end[-2] = '\0';
						}
						assert(streq(name, tag));
						char path[TED_PATH_MAX], full_path[TED_PATH_MAX];
						path_full(ted->tags_dir, filename, path, sizeof path);
						ted_path_full(ted, path, full_path, sizeof full_path);
						if (ted_open_file(ted, full_path)) {
							TextBuffer *buffer = ted->active_buffer;
							int line_number = atoi(address);
							if (line_number > 0) {
								// the tags file gives us a (1-indexed) line number
								BufferPos pos = {.line = (u32)line_number - 1, .index = 0};
								buffer_cursor_move_to_pos(buffer, pos);
								buffer->center_cursor_next_frame = true;
								success = true;
							} else if (address[0] == '/') {
								// the tags file gives us a pattern to look for
								char *pattern = address + 1;
								// the patterns seem to be always literal (not regex-y), except for ^ and $
								bool start_anchored = false, end_anchored = false;
								if (*pattern == '^') {
									start_anchored = true;
									++pattern;
								}
								char *dollar = strchr(pattern, '$');
								if (dollar) {
									end_anchored = true;
									*dollar = '\0';
								}
								
								String32 pattern32 = str32_from_utf8(pattern);
								u32 options = PCRE2_LITERAL;
								if (start_anchored) options |= PCRE2_ANCHORED;
								if (end_anchored) options |= PCRE2_ENDANCHORED;
								int error_code;
								PCRE2_SIZE error_offset;
								pcre2_code *code = pcre2_compile(pattern32.str, pattern32.len,
									options, &error_code, &error_offset, NULL);
								if (code) {
									pcre2_match_data *match_data = pcre2_match_data_create(10, NULL);
									if (match_data) {
										for (u32 line_idx = 0; line_idx < buffer->nlines; ++line_idx) {
											Line *line = &buffer->lines[line_idx];
											int n = pcre2_match(code, line->str, line->len, 0, PCRE2_NOTEMPTY,
												match_data, NULL);
											if (n == 1) {
												// found it!
												PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
												PCRE2_SIZE index = ovector[0];
												BufferPos pos = {line_idx, (u32)index};
												buffer_cursor_move_to_pos(buffer, pos);
												buffer->center_cursor_next_frame = true;
												success = true;
												break;
											}
										}
										pcre2_match_data_free(match_data);
									}
									pcre2_code_free(code);
								}
								str32_free(&pattern32);
							} else {
								ted_seterr(ted, "Unrecognized tag address: %s", address);
							}
						}
						break;
					}
				}
			}
		}
	}
	if (!success) {
		if (settings->regenerate_tags_if_not_found && !already_regenerated_tags) {
			tags_generate(ted, false);
			already_regenerated_tags = true;
			goto top;
		} else {
			ted_seterr(ted, "No such tag: %s", tag);
		}
	}
	fclose(file);
	return success;
}

SymbolInfo *tags_get_symbols(Ted *ted) {
	// read tags file and extract tag names
	const char *filename = tags_filename(ted, true);
	if (!filename) return NULL;
	FILE *file = fopen(filename, "rb");
	if (!file) return NULL;
	
	SymbolInfo *infos = NULL;
	u32 color = ted_color(ted, COLOR_TEXT);
	if (file) {
		char line[1024];
		while (fgets(line, sizeof line, file)) {
			if (line[0] != '!') { // tag metadata is formatted as tag names beginning with !	
				size_t len = strcspn(line, "\t");
				SymbolInfo *info = arr_addp(infos);
				info->name = strn_dup(line, len);
				info->color = color;
			}
		}
		fclose(file);
	}
	return infos;
}
