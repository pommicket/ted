#define SESSION_FILENAME "session.txt"
#define SESSION_VERSION "\x7fTED0001"

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
	if (buffer->filename && !buffer_is_untitled(buffer))
		write_cstr(fp, buffer->filename);
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
	if (!buffer_haserr(buffer)) {
		if (*filename) {
			if (!buffer_load_file(buffer, filename))
				buffer_new_file(buffer, TED_UNTITLED);
		} else {
			buffer_new_file(buffer, TED_UNTITLED);
		}
		buffer->scroll_x = read_double(fp);
		buffer->scroll_y = read_double(fp);
		buffer->view_only = read_bool(fp);
		buffer->cursor_pos = buffer_pos_read(buffer, fp);
		buffer->selection = read_bool(fp);
		if (buffer->selection)
			buffer->selection_pos = buffer_pos_read(buffer, fp);
	}
}

static void session_write_file(Ted *ted, FILE *fp) {
	fwrite(SESSION_VERSION, 1, sizeof SESSION_VERSION, fp);

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

static void session_write(Ted *ted) {
	Settings const *settings = &ted->settings;
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
			rename(filename1, filename2); // overwrite old session
		}
	}
}

static void session_read(Ted *ted) {
	Settings const *settings = &ted->settings;
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
