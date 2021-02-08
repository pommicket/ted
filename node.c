static void node_switch_to_tab(Ted *ted, Node *node, u16 new_tab_index) {
	node->active_tab = new_tab_index;
	if (node == ted->active_node) {
		// switch active buffer
		assert(node->tabs);
		u16 buffer_idx = node->tabs[new_tab_index];
		ted->active_buffer = &ted->buffers[buffer_idx];
	}
}

// move n tabs to the right
static void node_tab_next(Ted *ted, Node *node, i64 n) {
	assert(node->tabs);
	u16 ntabs = (u16)arr_len(node->tabs);
	u16 tab_idx = (u16)mod_i64(node->active_tab + n, ntabs);
	node_switch_to_tab(ted, node, tab_idx);
}

static void node_tab_prev(Ted *ted, Node *node, i64 n) {
	node_tab_next(ted, node, -n);
}

static void node_tab_switch(Ted *ted, Node *node, i64 tab) {
	assert(node->tabs);
	if (tab < arr_len(node->tabs)) {
		node_switch_to_tab(ted, node, (u16)tab);
	}
}

static void node_free(Node *node) {
	arr_free(node->tabs);
	memset(node, 0, sizeof *node);
}

static void node_close(Ted *ted, Node *node) {
	// @TODO(split)

	// delete all associated buffers
	arr_foreach_ptr(node->tabs, u16, tab) {
		u16 buffer_index = *tab;
		ted_delete_buffer(ted, buffer_index);
	}

	node_free(node);

	u16 node_index = (u16)(node - ted->nodes);
	assert(node_index < TED_MAX_NODES);
	ted->nodes_used[node_index] = false;
	if (ted->active_node == node) {
		ted->active_node = NULL;
	}
}

// close tab, WITHOUT checking for unsaved changes!
// returns true if the node is still open
static bool node_tab_close(Ted *ted, Node *node, u16 index) {
	u16 ntabs = (u16)arr_len(node->tabs);
	assert(index < ntabs);
	if (ntabs == 1) {
		// only 1 tab left, just close the node
		node_close(ted, node);
		return false;
	} else {
		u16 buffer_index = node->tabs[index];
		// remove tab from array
		memmove(&node->tabs[index], &node->tabs[index + 1], (size_t)(ntabs - (index + 1)) * sizeof *node->tabs);
		arr_remove_last(node->tabs);
		ted_delete_buffer(ted, buffer_index);

		ntabs = (u16)arr_len(node->tabs); // update ntabs
		assert(ntabs);
		// make sure active tab is valid
		node->active_tab = clamp_u16(node->active_tab, 0, ntabs - 1);
		if (ted->active_node == node) {
			// fix active buffer if necessary
			ted->active_buffer = &ted->buffers[node->tabs[node->active_tab]];
		}
		return true;
	}
}

static void node_frame(Ted *ted, Node *node, Rect r) {
	if (node->tabs) {
		bool is_active = node == ted->active_node;
		Settings const *settings = &ted->settings;
		u32 const *colors = settings->colors;
		Font *font = ted->font;
		float const border_thickness = settings->border_thickness;
		
		float tab_bar_height = 20;
		
		Rect tab_bar_rect = r;
		tab_bar_rect.size.y = tab_bar_height;

		{ // tab bar
			u16 ntabs = (u16)arr_len(node->tabs);
			float tab_width = r.size.x / ntabs;
			if (!ted->menu) {
				for (u16 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
					v2 click = ted->mouse_clicks[SDL_BUTTON_LEFT][c];
					if (rect_contains_point(tab_bar_rect, click)) {
						u16 tab_index = (u16)((click.x - r.pos.x) / tab_width);
						node_switch_to_tab(ted, node, tab_index);
					}
				}
				for (u16 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_MIDDLE]; ++c) {
					v2 click = ted->mouse_clicks[SDL_BUTTON_MIDDLE][c];
					if (rect_contains_point(tab_bar_rect, click)) {
						u16 tab_index = (u16)((click.x - r.pos.x) / tab_width);
						u16 buffer_idx = node->tabs[tab_index];
						TextBuffer *buffer = &ted->buffers[buffer_idx];
						// close that tab
						if (buffer_unsaved_changes(buffer)) {
							// make sure unsaved changes dialog is opened
							ted_switch_to_buffer(ted, buffer_idx);
							command_execute(ted, CMD_TAB_CLOSE, 1);
						} else {
							if (!node_tab_close(ted, node, tab_index)) {
								return; // node closed
							}
						}
						ntabs = (u16)arr_len(node->tabs);
						tab_width = r.size.x / ntabs;
					}
				}
			}

			TextRenderState text_state = text_render_state_default;
			for (u16 i = 0; i < ntabs; ++i) {
				TextBuffer *buffer = &ted->buffers[node->tabs[i]];
				char tab_title[256];
				char const *path = buffer_get_filename(buffer);
				char const *filename = path_filename(path);
				Rect tab_rect = rect(V2(r.pos.x + tab_width * i, r.pos.y), V2(tab_width, tab_bar_height));
				
				// tab border
				gl_geometry_rect_border(tab_rect, border_thickness, colors[COLOR_BORDER]);
				if (i == node->active_tab) {
					// highlight active tab
					gl_geometry_rect(tab_rect, colors[is_active ? COLOR_ACTIVE_TAB_HL : COLOR_HL]);
				}
				
				// tab title
				{
					char const *surround = buffer_unsaved_changes(buffer) ? "*" : "";
					strbuf_printf(tab_title, "%s%s%s", surround, filename, surround);
				}
				text_state.max_x = rect_x2(tab_rect);
				rgba_u32_to_floats(colors[COLOR_TEXT], text_state.color);
				text_state.x = tab_rect.pos.x;
				text_state.y = tab_rect.pos.y;
				text_utf8_with_state(font, &text_state, tab_title);
				
			}
			gl_geometry_draw();
			text_render(font);
		}

		u16 buffer_index = node->tabs[node->active_tab];
		TextBuffer *buffer = &ted->buffers[buffer_index];
		assert(ted->buffers_used[buffer_index]);
		Rect buffer_rect = rect_translate(r, V2(0, tab_bar_height));
		buffer_rect.size.y -= tab_bar_height;
		buffer_render(buffer, buffer_rect);
	} else {

#if 0
	// @TODO: test this
		// this node is a split
		Node *a = &ted->nodes[node->split_a];
		Node *b = &ted->nodes[node->split_b];
		Rect r1 = r, r2 = r;
		if (node->vertical_split) {
			float split_pos = r.size.y * node->split_pos;
			r1.size.y = split_pos;
			r2.pos.y += split_pos;
			r2.size.y = r.size.y - split_pos;
		} else {
			float split_pos = r.size.x * node->split_pos;
			r1.size.x = split_pos;
			r2.pos.x += split_pos;
			r2.size.x = r.size.x - split_pos;
		}
		node_render(ted, a, r1);
		node_render(ted, b, r2);
#endif
	}
}

