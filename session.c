#include "ted-internal.h"

#define SESSION_FILENAME "session.txt"
#define SESSION_VERSION "\x7fTED0003"

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


static void session_write_node(Ted *ted, FILE *fp, Node *node) {
	bool is_split = node_child1(node) != NULL;
	write_bool(fp, is_split);
	if (is_split) {
		write_float(fp, node_split_pos(node));
		write_bool(fp, node_split_is_vertical(node));
		Node *child1 = node_child1(node), *child2 = node_child2(node);
		write_u16(fp, (u16)arr_index_of(ted->nodes, child1));
		write_u16(fp, (u16)arr_index_of(ted->nodes, child2));
	} else {
		write_u16(fp, (u16)node_active_tab(node)); // active tab
		u16 ntabs = (u16)node_tab_count(node);
		write_u16(fp, ntabs);
		for (u16 i = 0; i < ntabs; ++i) {
			TextBuffer *tab = node_get_tab(node, i);
			write_u16(fp, (u16)arr_index_of(ted->buffers, tab));
		}
	}
}

static Status session_read_node(Ted *ted, FILE *fp, Node *node) {
	bool is_split = read_bool(fp);
	if (is_split) {
		float split_pos = clampf(read_float(fp), 0, 1);
		bool vertical = read_bool(fp);
		u16 split_a_index = read_u16(fp), split_b_index = read_u16(fp);
		if (split_a_index == split_b_index || split_a_index >= arr_len(ted->nodes)
			|| split_b_index >= arr_len(ted->nodes)) {
			return false;
		}
		Node *child1 = ted->nodes[split_a_index];
		Node *child2 = ted->nodes[split_b_index];
		if (child1 == node || child2 == node)
			return false;
		node_init_split(node, child1, child2, split_pos, vertical);
	} else {
		u16 active_tab = read_u16(fp);
		u16 ntabs = clamp_u16(read_u16(fp), 0, TED_MAX_TABS);
		if (active_tab >= ntabs)
			active_tab = 0;
		for (u16 i = 0; i < ntabs; ++i) {
			u16 buf_idx = read_u16(fp);
			if (buf_idx >= arr_len(ted->buffers)) return false;
			if (!node_add_tab(ted, node, ted->buffers[buf_idx]))
				return false;
		}
		node_tab_switch(ted, node, active_tab);
	}
	return true;
}

static void session_write_buffer(FILE *fp, TextBuffer *buffer) {
	// some info about the buffer that should be restored
	if (buffer_is_named_file(buffer))
		write_cstr(fp, buffer_get_path(buffer));
	else
		write_char(fp, 0);
	write_double(fp, buffer_get_scroll_columns(buffer));
	write_double(fp, buffer_get_scroll_lines(buffer));
	write_bool(fp, buffer_is_view_only(buffer));
	buffer_pos_write(buffer_cursor_pos(buffer), fp);
	BufferPos sel_pos = {0};
	bool has_selection = buffer_selection_pos(buffer, &sel_pos);
	write_bool(fp, has_selection);
	if (has_selection)
		buffer_pos_write(sel_pos, fp);
}

static bool session_read_buffer(Ted *ted, FILE *fp) {
	TextBuffer *buffer = ted_new_buffer(ted);
	char filename[TED_PATH_MAX] = {0};
	read_cstr(fp, filename, sizeof filename);
	if (!buffer_has_error(buffer)) {
		if (*filename) {
			if (!buffer_load_file(buffer, filename))
				buffer_new_file(buffer, NULL);
		} else {
			buffer_new_file(buffer, NULL);
		}
		double scroll_x = read_double(fp);
		double scroll_y = read_double(fp);
		buffer_set_view_only(buffer, read_bool(fp));
		BufferPos cursor_pos = buffer_pos_read(buffer, fp);
		bool has_selection = read_bool(fp);
		if (has_selection) {
			buffer_cursor_move_to_pos(buffer, buffer_pos_read(buffer, fp));
			buffer_select_to_pos(buffer, cursor_pos);
		} else {
			buffer_cursor_move_to_pos(buffer, cursor_pos);
		}
		buffer_scroll_to_pos(buffer, buffer_pos_start_of_file(buffer));
		buffer_scroll(buffer, scroll_x, scroll_y);
	}
	return true;
}

static void session_write_file(Ted *ted, FILE *fp) {
	fwrite(SESSION_VERSION, 1, sizeof SESSION_VERSION, fp);

	write_cstr(fp, ted->cwd);

	write_u16(fp, (u16)arr_index_of(ted->nodes, ted->active_node)); // active node idx
	write_u16(fp, (u16)arr_index_of(ted->buffers, ted->active_buffer)); // active buffer idx

	write_u16(fp, (u16)arr_len(ted->buffers));
	arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
		TextBuffer *buffer = *pbuffer;
		session_write_buffer(fp, buffer);
	}
	
	write_u16(fp, (u16)arr_len(ted->nodes));
	arr_foreach_ptr(ted->nodes, NodePtr, pnode) {
		session_write_node(ted, fp, *pnode);
	}
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

	u16 nbuffers = read_u16(fp);
	for (u16 i = 0; i < nbuffers; ++i) {
		if (!session_read_buffer(ted, fp)) {
			arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
				buffer_free(*pbuffer);
			}
			arr_clear(ted->buffers);
			return;
		}
	}
	
	u16 nnodes = read_u16(fp);
	for (u16 i = 0; i < nnodes; i++) {
		node_new(ted);
	}
	for (u16 i = 0; i < nnodes; i++) {
		if (!session_read_node(ted, fp, ted->nodes[i])) {
			arr_foreach_ptr(ted->buffers, TextBufferPtr, pbuffer) {
				buffer_free(*pbuffer);
			}
			arr_clear(ted->buffers);
			arr_foreach_ptr(ted->nodes, NodePtr, pnode) {
				node_free(*pnode);
			}
			arr_clear(ted->nodes);
			return;
		}
	}

	if (active_node_idx == U16_MAX) {
		ted->active_node = NULL;
	} else {
		if (active_node_idx >= arr_len(ted->nodes))
			active_node_idx = 0;
		ted->active_node = ted->nodes[active_node_idx];
	}

	if (active_buffer_idx == U16_MAX) {
		ted->active_buffer = NULL;
	} else if (active_buffer_idx < arr_len(ted->buffers)) {
		ted->active_buffer = ted->buffers[active_buffer_idx];
	}

	if (arr_len(ted->buffers) && !ted->active_buffer) {
		// set active buffer to something
		ted->active_buffer = ted->buffers[0];
	}
	ted_check_for_node_problems(ted);
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
