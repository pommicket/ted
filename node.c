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

static void node_frame(Ted *ted, Node *node, Rect r) {
	if (node->tabs) {
		bool is_active = node == ted->active_node;
		Settings const *settings = &ted->settings;
		u32 const *colors = settings->colors;
		Font *font = ted->font;
		
		u16 buffer_index = node->tabs[node->active_tab];
		float tab_bar_height = 20;
		
		Rect tab_bar_rect = r;
		tab_bar_rect.size.y = tab_bar_height;

		{ // tab bar
			u16 ntabs = (u16)arr_len(node->tabs);
			float tab_width = r.size.x / ntabs;
			for (u16 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
				v2 click = ted->mouse_clicks[SDL_BUTTON_LEFT][c];
				if (rect_contains_point(tab_bar_rect, click)) {
					u16 tab_index = (u16)((click.x - r.pos.x) / tab_width);
					node_switch_to_tab(ted, node, tab_index);
				}
			}
			for (u16 i = 0; i < ntabs; ++i) {
				TextBuffer *buffer = &ted->buffers[node->tabs[i]];
				char tab_title[256];
				char const *path = buffer_get_filename(buffer);
				char const *filename = path_filename(path);
				Rect tab_rect = rect(V2(r.pos.x + tab_width * i, r.pos.y), V2(tab_width, tab_bar_height));
				glBegin(GL_QUADS);
				gl_color_rgba(colors[COLOR_BORDER]);

				// tab border
				rect_render_border(tab_rect, 1);
				if (i == node->active_tab) {
					// highlight active tab
					gl_color_rgba(colors[is_active ? COLOR_ACTIVE_TAB_HL : COLOR_HL]);
					rect_render(tab_rect);
				}
				glEnd();

				// tab title
				{
					char const *surround = buffer_unsaved_changes(buffer) ? "*" : "";
					strbuf_printf(tab_title, "%s%s%s", surround, filename, surround);
				}
				gl_color_rgba(colors[COLOR_TEXT]);
				TextRenderState text_state = text_render_state_default;
				text_state.max_x = rect_x2(tab_rect);
				text_render_with_state(font, &text_state, tab_title, tab_rect.pos.x, tab_rect.pos.y);
				
			}
		}

		TextBuffer *buffer = &ted->buffers[buffer_index];
		Rect buffer_rect = rect_translate(r, V2(0, tab_bar_height));
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

