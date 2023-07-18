#include "ted.h"

#define SESSION_FILENAME "session.txt"
#define SESSION_VERSION "\x7fTED0002"

static void write_u8(FILE *fp, u8 x) {
	putc(x, fp);
}

static void write_u16(FILE *fp, u16 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_u32(FILE *fp, u32 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_u64(FILE *fp, u64 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i8(FILE *fp, i8 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i16(FILE *fp, i16 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i32(FILE *fp, i32 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_i64(FILE *fp, i64 x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_float(FILE *fp, float x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_double(FILE *fp, double x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_char(FILE *fp, char x) {
	fwrite(&x, sizeof x, 1, fp);
}

static void write_bool(FILE *fp, bool x) {
	putc(x, fp);
}

static u8 read_u8(FILE *fp) {
	return (u8)getc(fp);
}

static u16 read_u16(FILE *fp) {
	u16 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static u32 read_u32(FILE *fp) {
	u32 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static u64 read_u64(FILE *fp) {
	u64 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i8 read_i8(FILE *fp) {
	i8 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i16 read_i16(FILE *fp) {
	i16 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i32 read_i32(FILE *fp) {
	i32 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static i64 read_i64(FILE *fp) {
	i64 x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static float read_float(FILE *fp) {
	float x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static double read_double(FILE *fp) {
	double x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static char read_char(FILE *fp) {
	char x = 0;
	fread(&x, sizeof x, 1, fp);
	return x;
}

static bool read_bool(FILE *fp) {
	return getc(fp) != 0;
}

static void write_cstr(FILE *fp, const char *cstr) {
	fwrite(cstr, 1, strlen(cstr) + 1, fp);
}

static void read_cstr(FILE *fp, char *out, size_t out_sz) {
	char *p = out, *end = out + out_sz;
	while (1) {
		if (p >= end - 1) {
			*p = '\0';
			break;
		}
		int c = getc(fp);
		if (c == 0 || c == EOF) {
			*p = '\0';
			break;
		}
		*p++ = (char)c;
	}
}


// write a buffer position to a file
static void buffer_pos_write(BufferPos pos, FILE *fp) {
	write_u32(fp, pos.line);
	write_u32(fp, pos.index);
}

// read a buffer position from a file, and validate it
static BufferPos buffer_pos_read(TextBuffer *buffer, FILE *fp) {
	BufferPos pos = {0};
	pos.line = read_u32(fp);
	pos.index = read_u32(fp);
	buffer_pos_validate(buffer, &pos);
	return pos;
}


static void session_write_node(Ted *ted, FILE *fp, u16 node_idx) {
	Node *node = &ted->nodes[node_idx];
	write_u16(fp, node_idx);
	bool is_split = !node->tabs;
	write_bool(fp, is_split);
	if (is_split) {
		write_float(fp, node->split_pos);
		write_bool(fp, node->split_vertical);
		write_u16(fp, node->split_a);
		write_u16(fp, node->split_b);
	} else {
		write_u16(fp, node->active_tab); // active tab
		write_u16(fp, (u16)arr_len(node->tabs)); // ntabs
		arr_foreach_ptr(node->tabs, u16, tab) {
			write_u16(fp, *tab);
		}
	}
}

static void session_read_node(Ted *ted, FILE *fp) {
	u16 node_idx = read_u16(fp);
	if (node_idx >= TED_MAX_NODES) {
		debug_println("WARNING: Invalid node index (see %s:%d)!\n", __FILE__, __LINE__);
		return;
	}
	ted->nodes_used[node_idx] = true;
	Node *node = &ted->nodes[node_idx];
	bool is_split = read_bool(fp);
	if (is_split) {
		node->split_pos = clampf(read_float(fp), 0, 1);
		node->split_vertical = read_bool(fp);
		node->split_a = clamp_u16(read_u16(fp), 0, TED_MAX_NODES);
		node->split_b = clamp_u16(read_u16(fp), 0, TED_MAX_NODES);
	} else {
		node->active_tab = read_u16(fp);
		u16 ntabs = clamp_u16(read_u16(fp), 0, TED_MAX_TABS);
		if (node->active_tab >= ntabs)
			node->active_tab = 0;
		for (u16 i = 0; i < ntabs; ++i) {
			u16 buf_idx = read_u16(fp);
			if (buf_idx >= TED_MAX_BUFFERS) continue;
			arr_add(node->tabs, buf_idx);
		}
	}
}

static void session_write_buffer(Ted *ted, FILE *fp, u16 buffer_idx) {
	write_u16(fp, buffer_idx);
	TextBuffer *buffer = &ted->buffers[buffer_idx];
	// some info about the buffer that should be restored
	if (buffer_is_named_file(buffer))
		write_cstr(fp, buffer->path);
	else
		write_char(fp, 0);
	write_double(fp, buffer->scroll_x);
	write_double(fp, buffer->scroll_y);
	write_bool(fp, buffer->view_only);
	buffer_pos_write(buffer->cursor_pos, fp);
	write_bool(fp, buffer->selection);
	if (buffer->selection)
		buffer_pos_write(buffer->selection_pos, fp);
}

static void session_read_buffer(Ted *ted, FILE *fp) {
	u16 buffer_idx = read_u16(fp);
	if (buffer_idx >= TED_MAX_BUFFERS) {
		debug_println("WARNING: Invalid buffer index (see %s:%d)!\n", __FILE__, __LINE__);
		return;
	}
	TextBuffer *buffer = &ted->buffers[buffer_idx];
	ted->buffers_used[buffer_idx] = true;
	char filename[TED_PATH_MAX] = {0};
	read_cstr(fp, filename, sizeof filename);
	buffer_create(buffer, ted);
	if (!buffer_has_error(buffer)) {
		if (*filename) {
			if (!buffer_load_file(buffer, filename))
				buffer_new_file(buffer, NULL);
		} else {
			buffer_new_file(buffer, NULL);
		}
		buffer->scroll_x = read_double(fp);
		buffer->scroll_y = read_double(fp);
		buffer->view_only = read_bool(fp);
		buffer->cursor_pos = buffer_pos_read(buffer, fp);
		buffer->selection = read_bool(fp);
		if (buffer->selection)
			buffer->selection_pos = buffer_pos_read(buffer, fp);
		buffer_pos_validate(buffer, &buffer->cursor_pos);
		buffer_pos_validate(buffer, &buffer->selection_pos);
		if (buffer->selection && buffer_pos_eq(buffer->cursor_pos, buffer->selection_pos)) {
			// this could happen if the file was changed on disk
			buffer->selection = false;
		}
	}
}

static void session_write_file(Ted *ted, FILE *fp) {
	fwrite(SESSION_VERSION, 1, sizeof SESSION_VERSION, fp);

	write_cstr(fp, ted->cwd);

	write_u16(fp, ted->active_node ? (u16)(ted->active_node - ted->nodes) : U16_MAX); // active node idx
	write_u16(fp, ted->active_buffer ? (u16)(ted->active_buffer - ted->buffers) : U16_MAX); // active buffer idx

	u16 nnodes = 0;
	for (u16 i = 0; i < TED_MAX_NODES; ++i)
		nnodes += ted->nodes_used[i];
	write_u16(fp, nnodes);
	for (u16 i = 0; i < TED_MAX_NODES; ++i)
		if (ted->nodes_used[i])
			session_write_node(ted, fp, i);

	u16 nbuffers = 0;
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i)
		nbuffers += ted->buffers_used[i];
	write_u16(fp, nbuffers);
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i)
		if (ted->buffers_used[i])
			session_write_buffer(ted, fp, i);
}

static void session_read_file(Ted *ted, FILE *fp) {
	char version[sizeof SESSION_VERSION] = {0};
	fread(version, 1, sizeof version, fp);
	if (memcmp(version, SESSION_VERSION, sizeof version) != 0) {
		debug_println("WARNING: Session file has wrong version (see %s:%d)!\n", __FILE__, __LINE__);
		return; // wrong version
	}

	read_cstr(fp, ted->cwd, sizeof ted->cwd);

	u16 active_node_idx = read_u16(fp);
	u16 active_buffer_idx = read_u16(fp);

	u16 nnodes = clamp_u16(read_u16(fp), 0, TED_MAX_NODES);
	for (u16 i = 0; i < nnodes; ++i) {
		session_read_node(ted, fp);
	}

	u16 nbuffers = clamp_u16(read_u16(fp), 0, TED_MAX_BUFFERS);
	for (u16 i = 0; i < nbuffers; ++i) {
		session_read_buffer(ted, fp);
	}

	if (active_node_idx == U16_MAX) {
		ted->active_node = NULL;
	} else {
		active_node_idx = clamp_u16(active_node_idx, 0, TED_MAX_NODES);
		if (ted->nodes_used[active_node_idx])
			ted->active_node = &ted->nodes[active_node_idx];
	}

	if (active_buffer_idx == U16_MAX) {
		ted->active_buffer = NULL;
	} else {
		active_buffer_idx = clamp_u16(active_buffer_idx, 0, TED_MAX_BUFFERS);
		if (ted->buffers_used[active_buffer_idx])
			ted->active_buffer = &ted->buffers[active_buffer_idx];
	}

	if (nbuffers && !ted->active_buffer) {
		// set active buffer to something
		for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
			if (ted->buffers_used[i]) {
				ted_switch_to_buffer(ted, &ted->buffers[i]);
				break;
			}
		}
	}
}

void session_write(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	if (!settings->restore_session)
		return;
	// first we write to a prefixed file so in case something goes wrong we still have the old session.
	char filename1[TED_PATH_MAX], filename2[TED_PATH_MAX];
	strbuf_printf(filename1, "%s/_" SESSION_FILENAME, ted->local_data_dir);
	strbuf_printf(filename2, "%s/"  SESSION_FILENAME, ted->local_data_dir);
	FILE *fp = fopen(filename1, "wb");
	if (fp) {
		session_write_file(ted, fp);

		bool success = !ferror(fp);
		success &= fclose(fp) == 0;
		if (success) {
			remove(filename2);
			os_rename_overwrite(filename1, filename2); // overwrite old session
		}
	}
}

void session_read(Ted *ted) {
	const Settings *settings = ted_active_settings(ted);
	if (settings->restore_session) {
		char filename[TED_PATH_MAX];
		strbuf_printf(filename, "%s/" SESSION_FILENAME, ted->local_data_dir);
		FILE *fp = fopen(filename, "rb");
		if (fp) {
			session_read_file(ted, fp);
			fclose(fp);
		}
	}
}
