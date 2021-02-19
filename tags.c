static int tag_try(FILE *fp, char const *tag) {
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

// returns true if the tag exists.
bool tag_goto(Ted *ted, char const *tag) {
	change_directory(ted->cwd);
	Settings const *settings = &ted->settings;
	char const *tags_filename = settings->tags_filename;
	FILE *file = fopen(tags_filename, "rb");
	if (!file) {
		ted_seterr(ted, "No tags file. Try running ctags.");
		return false;
	}
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
			char tag_enty[1024];
			if (fgets(tag_enty, sizeof tag_enty, file)) {
			
				// the tag is of the format:
				// tag name\tfile name\taddress
				// or
				// tag name\tfile name\taddress;" additional information
				
				char *name = tag_enty;
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
						char path[TED_PATH_MAX];
						ted_full_path(ted, filename, path, sizeof path);
						if (ted_open_file(ted, path)) {
							TextBuffer *buffer = ted->active_buffer;
							int line_number = atoi(address);
							if (line_number > 0) {
								// the tags file gives us a line number
								BufferPos pos = {.line = (u32)line_number, .index = 0};
								buffer_cursor_move_to_pos(buffer, pos);
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
							break;
						}
					}
				}
			}
		}
	}
	fclose(file);
	return success;
}
