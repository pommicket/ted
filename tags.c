void tags_free(TagsFile *f) {
	free(f->file_data);
	arr_free(f->tags);
	memset(f, 0, sizeof *f);
}

void tags_read(Ted *ted, TagsFile *f) {
	change_directory(ted->cwd);
	Settings const *settings = &ted->settings;
	char const *tags_filename = settings->tags_filename;
	FILE *file = fopen(tags_filename, "rb");
	tags_free(f);
	if (file) {
		fseek(file, 0, SEEK_END);
		size_t file_size = (size_t)ftell(file);
		fseek(file, 0, SEEK_SET);
		char *file_data = ted_calloc(ted, file_size + 1, 1);
		if (file_data) {
			f->file_data = file_data;
			fread(file_data, 1, file_size, file);
			char *p = file_data;
			while (1) {
				// each line in the file is of the format:
				// tag name\tfile name\taddress
				// or
				// tag name\tfile name\taddress;" additional information
				
				char *end = p + strcspn(p, "\n");
				bool eof = *end == '\0';
				char *name = p;
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
						Tag tag = {.name = name, .file = filename, .address = address};
						arr_add(f->tags, tag);
					}
				}
				if (eof) break; // end of file
				p = end + 1;
			}
		}
		f->last_modified = time_last_modified(tags_filename);
		fclose(file);
	} else {
		ted_seterr(ted, "tags file does not exist.");
	}
}

void tags_read_if_changed(Ted *ted, TagsFile *f) {
	Settings const *settings = &ted->settings;
	char const *tags_filename = settings->tags_filename;
	struct timespec modify_time = time_last_modified(tags_filename);
	if (!timespec_eq(modify_time, f->last_modified)) {
		tags_read(ted, f);
	}
}