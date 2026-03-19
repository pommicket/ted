// support for ctags go-to-definition and completion

#include "ted-internal.h"
#include "pcre-inc.h"

static bool get_tags_dir(Ted *ted, bool error_if_does_not_exist) {
	free(ted->tags_dir);
	ted->tags_dir = str_dup(ted->cwd);
	while (1) {
		char *path = path_full(ted->tags_dir, "tags");
		bool exists = fs_file_exists(path);
		free(path);
		if (exists)
			return true;
		path = path_full(ted->tags_dir, "..");
		if (streq(path, ted->tags_dir))
			break;
		free(ted->tags_dir);
		ted->tags_dir = path;
	}
	if (error_if_does_not_exist)
		ted_flash_error_cursor(ted);
	return false;
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

static void run_tags_command(Ted *ted, const char *command, bool run_in_build_window) {
	if (run_in_build_window) {
		build_queue_command(ted, command);
	} else {
		if (system(command) != 0) {
			// maybe would be better to ted_error here. but that might be annoying
			ted_log(ted, "Failed to generate tags: %s", strerror(errno));
		}
	}
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
			char *path = path_full(dir, entry->name);
			if (entry->name[0] != '.') { // ignore hidden directories and . and ..
				switch (entry->type) {
				case FS_FILE: {
					if (is_source_file(entry->name)) {
						size_t cmdlen = strlen(command), pathlen = strlen(path);
						any_files = true;
						// make sure command doesn't get too long
						if (cmdlen + pathlen + 5 >= sizeof command) {
							run_tags_command(ted, command, run_in_build_window);
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
			free(path);
		}
		if (any_files) {
			run_tags_command(ted, command, run_in_build_window);
		}
	}
}

// generate/re-generate tags.
void tags_generate(Ted *ted, bool run_in_build_window) {
	if (!get_tags_dir(ted, false)) {
		free(ted->tags_dir);
		ted->tags_dir = ted_get_root_dir(ted);
	}
	build_set_working_directory(ted, ted->tags_dir);
	
	{
		char *path = path_full(ted->tags_dir, "tags");
		remove(path); // delete old tags file
		free(path);
	}
	
	if (run_in_build_window) build_queue_start(ted);
	tags_generate_at_dir(ted, run_in_build_window, ted->tags_dir, 0);
	if (run_in_build_window) build_queue_finish(ted);
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

static FILE *open_tags_file(Ted *ted, bool error_if_tags_does_not_exist) {
	if (!get_tags_dir(ted, error_if_tags_does_not_exist))
		return NULL;
	char *tags_name = path_full(ted->tags_dir, "tags");
	if (!tags_name) return NULL;
	FILE *file = fopen(tags_name, "rb");
	free(tags_name); tags_name = NULL;
	return file;
}

size_t tags_beginning_with(Ted *ted, const char *prefix, char **out, size_t out_size, bool error_if_tags_does_not_exist) {
	assert(out_size);
	FILE *file = open_tags_file(ted, error_if_tags_does_not_exist);
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
	if (!exact && mid > 0) {
		// go to next line (consistent with tag_try, so we don't start reading halfway through a line)
		if (!fgets(line, sizeof line, file)) {
			// probably means file changed halfway through, but whatever, just return no match.
			return 0;
		}
	}
	
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

static bool goto_tag_address(Ted *ted, const char *address) {
	TextBuffer *buffer = ted_active_buffer(ted);
	int line_number = atoi(address);
	if (line_number > 0) {
		// the tags file gives us a (1-indexed) line number
		BufferPos pos = {.line = (u32)line_number - 1, .index = 0};
		buffer_cursor_move_to_pos(buffer, pos);
		buffer_center_cursor_next_frame(buffer);
		return true;
	} else if (address[0] == '/') {
		// the tags file gives us a pattern to look for
		const char *in = address + 1;
		
		// the patterns seem to be always literal (not regex-y), except for ^ and $
		// first, we do some preprocessing to remove backslashes and check for ^ and $.
		bool start_anchored = false, end_anchored = false;
		char *pattern = calloc(1, strlen(in) + 1);
		{
			char *out = pattern;
			if (*in == '^') {
				start_anchored = true;
				++in;
			}
			while (*in) {
				if (*in == '\\' && in[1]) {
					 *out++ = in[1];
					 in += 2;
				} else
				// NOTE: ctags-universal doesn't escape $ when it's not at the end of the pattern
				if (*in == '$' && in[1] == 0) {
					end_anchored = true;
					break;
				} else {
					*out++ = *in++;
				}
			}
		}
		
		// now we search
		String32 pattern32 = str32_from_utf8(pattern);
		u32 options = PCRE2_LITERAL;
		if (start_anchored) options |= PCRE2_ANCHORED;
		if (end_anchored) options |= PCRE2_ENDANCHORED;
		int error_code;
		bool success = false;
		PCRE2_SIZE error_offset;
		pcre2_code_32 *code = pcre2_compile_32(pattern32.str, pattern32.len,
			options, &error_code, &error_offset, NULL);
		if (code) {
			pcre2_match_data_32 *match_data = pcre2_match_data_create_32(10, NULL);
			if (match_data) {
				for (u32 line_idx = 0, line_count = buffer_line_count(buffer); line_idx < line_count; ++line_idx) {
					String32 line = buffer_get_line(buffer, line_idx);
					int n = pcre2_match_32(code, line.str, line.len, 0, PCRE2_NOTEMPTY,
						match_data, NULL);
					if (n == 1) {
						// found it!
						PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_32(match_data);
						PCRE2_SIZE index = ovector[0];
						BufferPos pos = {line_idx, (u32)index};
						buffer_cursor_move_to_pos(buffer, pos);
						buffer_center_cursor_next_frame(buffer);
						success = true;
						break;
					}
				}
				pcre2_match_data_free_32(match_data);
			}
			pcre2_code_free_32(code);
		}
		str32_free(&pattern32);
		free(pattern);
		return success;
	} else {
		ted_error(ted, "Unrecognized tag address: %s", address);
		return false;
	}
}

// returns true if the tag exists.
bool tag_goto(Ted *ted, const char *tag) {
	bool already_regenerated_tags = false;
top:;
	const Settings *settings = ted_active_settings(ted);
	FILE *file = open_tags_file(ted, true);
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
						// some addresses randomly end with ;" I think. not entirely sure why this needs to be here.
						if (address_end - address > 2 && address_end[-2] == ';' && address_end[-1] == '"') {
							address_end[-2] = '\0';
						}
						assert(streq(name, tag));
						char *path = path_full(ted->tags_dir, filename);
						char *full_path = ted_path_full(ted, path);
						success = ted_open_file(ted, full_path);
						free(path); path = NULL;
						free(full_path); full_path = NULL;
						success = success && goto_tag_address(ted, address);
						if (!success)
							goto failure;
					}
					break;
				}
			}
		}
	}
	if (!success) {
	failure:
		if (settings->regenerate_tags_if_not_found && !already_regenerated_tags) {
			tags_generate(ted, false);
			already_regenerated_tags = true;
			goto top;
		} else {
			ted_error(ted, "No such tag: %s", tag);
		}
	}
	fclose(file);
	return success;
}

SymbolInfo *tags_get_symbols(Ted *ted) {
	// read tags file and extract tag names
	FILE *file = open_tags_file(ted, true);
	if (!file) return NULL;
	
	SymbolInfo *infos = NULL;
	if (file) {
		char line[1024];
		while (fgets(line, sizeof line, file)) {
			if (line[0] != '!') { // tag metadata is formatted as tag names beginning with !
				size_t len = strcspn(line, "\t");
				SymbolInfo *info = arr_addp(infos);
				info->name = strn_dup(line, len);
				info->color = COLOR_TEXT;
			}
		}
		fclose(file);
	}
	return infos;
}
