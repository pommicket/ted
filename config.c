// Read a configuration file.
// Config files are formatted as several sections, each containing key = value pairs.
// e.g.:
// [section1]
// thing1 = 33
// thing2 = 454
// [section2]
// asdf = 123

typedef enum {
	SECTION_NONE,
	SECTION_KEYBOARD
} Section;

void config_read(Ted *ted, char const *filename) {
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		u32 line_number = 1;
		int line_cap = 4096;
		char *line = ted_malloc(ted, (size_t)line_cap);
		if (line) {
			Section section = SECTION_NONE;

			while (fgets(line, line_cap, fp)) {
				char *newline = strchr(line, '\n');
				if (newline || feof(fp)) {
					if (newline) *newline = '\0';

					bool error = false;

					// ok, we've now read a line.
					switch (line[0]) {
					case '#': // comment
					case '\0': // blank line
						break;
					case '[': { // section header
					#define SECTION_HEADER_HELP "Section headers should look like this: [section-name]"
						char *closing = strchr(line, ']');
						if (!closing) {
							ted_seterr(ted, "%s:" U32_FMT ": Unmatched [. " SECTION_HEADER_HELP, filename, line_number);
							error = true;
						} else if (closing[1] != '\0') {
							ted_seterr(ted, "%s:" U32_FMT ": Text after section. " SECTION_HEADER_HELP, filename, line_number);
							error = true;
						} else {
							*closing = '\0';
							char *section_name = line + 1;
							if (streq(section_name, "keyboard")) {
								section = SECTION_KEYBOARD;
							} else {
								ted_seterr(ted, "%s:" U32_FMT ": Unrecognized section: [%s].", filename, line_number, section_name);
								error = true;
							}
						}
					} break;
					}

					if (error) break;

					++line_number;
				} else {
					ted_seterr(ted, "%s:" U32_FMT ": Line is too long.", filename, line_number);
					break;
				}
			}
		}
		free(line);
		if (ferror(fp))
			ted_seterr(ted, "Error reading %s.", filename);
		fclose(fp);
	} else {
		ted_seterr(ted, "Couldn't open file %s.", filename);
	}
}
