static void node_switch_to_tab(Ted *ted, Node *node, u16 new_tab_index) {
	node->active_tab = new_tab_index;
	if (node == ted->active_node) {
		// switch active buffer
		assert(node->tabs);
		u16 buffer_idx = node->tabs[new_tab_index];
		ted_switch_to_buffer(ted, &ted->buffers[buffer_idx]);
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

// swap the position of two tabs
static void node_tabs_swap(Node *node, u16 tab1, u16 tab2) {
	assert(tab1 < arr_len(node->tabs) && tab2 < arr_len(node->tabs));
	if (node->active_tab == tab1) node->active_tab = tab2;
	else if (node->active_tab == tab2) node->active_tab = tab1;
	u16 tmp = node->tabs[tab1];
	node->tabs[tab1] = node->tabs[tab2];
	node->tabs[tab2] = tmp;
}

static void node_free(Node *node) {
	arr_free(node->tabs);
	memset(node, 0, sizeof *node);
}

// returns index of parent in ted->nodes, or -1 if this is the root node.
static i32 node_parent(Ted *ted, u16 node_idx) {
	bool *nodes_used = ted->nodes_used;
	assert(node_idx < TED_MAX_NODES && nodes_used[node_idx]);
	for (u16 i = 0; i < TED_MAX_NODES; ++i) {
		if (nodes_used[i]) {
			Node *node = &ted->nodes[i];
			if (!node->tabs) {
				if (node->split_a == node_idx || node->split_b == node_idx)
					return i;
			}
		}
	}
	return -1;
}

// the root has depth 0, and a child node has 1 more than its parent's depth.
static u8 node_depth(Ted *ted, u16 node_idx) {
	u8 depth = 0;
	while (1) {
		i32 parent = node_parent(ted, node_idx);
		if (parent < 0) {
			break;
		} else {
			node_idx = (u16)parent;
			depth += 1;
		}
	}
	return depth;
}

// join this node with its sibling
static void node_join(Ted *ted, Node *node) {
	i32 parent_idx = node_parent(ted, (u16)(node - ted->nodes));
	if (parent_idx >= 0) {
		Node *parent = &ted->nodes[parent_idx];
		Node *a = &ted->nodes[parent->split_a];
		Node *b = &ted->nodes[parent->split_b];
		if (a->tabs && b->tabs) {
			if (ted->active_node == a || ted->active_node == b) {
				ted->active_node = parent;
			}
			arr_foreach_ptr(a->tabs, u16, tab) {
				arr_add(parent->tabs, *tab);
			}
			arr_foreach_ptr(b->tabs, u16, tab) {
				arr_add(parent->tabs, *tab);
			}
			if (parent->tabs) {
				if (node == a) {
					parent->active_tab = a->active_tab;
				} else {
					parent->active_tab = (u16)arr_len(a->tabs) + b->active_tab;
				}
				node_free(a);
				node_free(b);
				ted->nodes_used[parent->split_a] = false;
				ted->nodes_used[parent->split_b] = false;
			}
		}
	}
}

static void node_close(Ted *ted, u16 node_idx) {
	ted->dragging_tab_node = NULL;
	ted->resizing_split = NULL;
	
	assert(node_idx < TED_MAX_NODES);
	assert(ted->nodes_used[node_idx]);
	i32 parent_idx = node_parent(ted, node_idx);
	ted->nodes_used[node_idx] = false;

	Node *node = &ted->nodes[node_idx];
	bool was_active = ted->active_node == node;

	// delete all associated buffers
	arr_foreach_ptr(node->tabs, u16, tab) {
		u16 buffer_index = *tab;
		ted_delete_buffer(ted, buffer_index);
	}

	node_free(node);

	if (parent_idx < 0) {
		// no parent; this must be the root node
		ted->active_node = NULL;
	} else {
		// turn parent from split node into tab node
		Node *parent = &ted->nodes[parent_idx];
		assert(!parent->tabs);
		u16 other_side;
		if (node_idx == parent->split_a) {
			other_side = parent->split_b;
		} else {
			assert(node_idx == parent->split_b);
			other_side = parent->split_a;
		}
		// replace parent with other side of split
		*parent = ted->nodes[other_side];

		ted->nodes_used[other_side] = false;
		if (was_active) {
			Node *new_active_node = parent;
			// make sure we don't set the active node to a split
			while (!new_active_node->tabs)
				new_active_node = &ted->nodes[new_active_node->split_a];
			ted_node_switch(ted, new_active_node);
		}
	}
}


// close tab, WITHOUT checking for unsaved changes!
// returns true if the node is still open
static bool node_tab_close(Ted *ted, Node *node, u16 index) {
	ted->dragging_tab_node = NULL;

	u16 ntabs = (u16)arr_len(node->tabs);
	assert(index < ntabs);
	if (ntabs == 1) {
		// only 1 tab left, just close the node
		node_close(ted, (u16)(node - ted->nodes));
		return false;
	} else {
		bool was_active = ted->active_node == node; // ted->active_node will be set to NULL when the active buffer is deleted.
		u16 buffer_index = node->tabs[index];
		// remove tab from array
		arr_remove(node->tabs, index);
		ted_delete_buffer(ted, buffer_index);

		ntabs = (u16)arr_len(node->tabs); // update ntabs
		assert(ntabs);
		// fix active_tab
		if (index < node->active_tab)
			--node->active_tab;
		node->active_tab = clamp_u16(node->active_tab, 0, ntabs - 1);
		if (was_active) {
			// fix active buffer if necessary
			ted_switch_to_buffer(ted, &ted->buffers[node->tabs[node->active_tab]]);
		}
		return true;
	}
}

static void node_frame(Ted *ted, Node *node, Rect r) {
	Settings const *settings = &ted->settings;
	if (node->tabs) {
		bool is_active = node == ted->active_node;
		u32 const *colors = settings->colors;
		Font *font = ted->font;
		float const border_thickness = settings->border_thickness;
		float const char_height = text_font_char_height(font);
		float tab_bar_height = char_height + 2 * border_thickness;
		
		Rect tab_bar_rect = r;
		tab_bar_rect.size.y = tab_bar_height;

		{ // tab bar
			u16 ntabs = (u16)arr_len(node->tabs);
			float tab_width = r.size.x / ntabs;
			if (!ted->menu) {
				for (u16 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++c) {
					v2 click = ted->mouse_clicks[SDL_BUTTON_LEFT][c];
					if (rect_contains_point(tab_bar_rect, click)) {
						// click on tab to switch to it
						u16 tab_index = (u16)((click.x - r.pos.x) / tab_width);
						if (tab_index < arr_len(node->tabs)) {
							ted->active_node = node;
							node_switch_to_tab(ted, node, tab_index);
							ted->dragging_tab_node = node;
							ted->dragging_tab_idx = tab_index;
							ted->dragging_tab_origin = click;
						}
					}
				}
				if (ted->dragging_tab_node) {
					// check if user dropped tab here
					for (u16 c = 0; c < ted->nmouse_releases[SDL_BUTTON_LEFT]; ++c) {
						v2 release = ted->mouse_releases[SDL_BUTTON_LEFT][c];
						if (rect_contains_point(tab_bar_rect, release)) {
							u16 tab_index = (u16)roundf((release.x - r.pos.x) / tab_width);
							if (tab_index <= arr_len(node->tabs)) {
								Node *drag_node = ted->dragging_tab_node;
								u16 drag_index = ted->dragging_tab_idx;
								u16 tab = drag_node->tabs[drag_index];

								// remove the old tab
								arr_remove(drag_node->tabs, drag_index);
								if (node == drag_node) {
									// fix index if we move tab from one place to another in the same node
									if (tab_index > drag_index)
										--tab_index;
								}
								// insert the tab here
								arr_insert(node->tabs, tab_index, tab);
								if (arr_len(drag_node->tabs) == 0) {
									// removed the last tab from a node; close it
									node_close(ted, (u16)(drag_node - ted->nodes));
								} else {
									// make sure active tab is valid
									drag_node->active_tab = clamp_u16(drag_node->active_tab, 0, (u16)arr_len(drag_node->tabs) - 1);
								}

								ted->dragging_tab_node = NULL; // stop dragging
								// switch to this buffer
								ted_switch_to_buffer(ted, &ted->buffers[tab]);
							}
						}
					}
				}
				for (u16 c = 0; c < ted->nmouse_clicks[SDL_BUTTON_MIDDLE]; ++c) {
					// middle-click to close tab
					v2 click = ted->mouse_clicks[SDL_BUTTON_MIDDLE][c];
					if (rect_contains_point(tab_bar_rect, click)) {
						u16 tab_index = (u16)((click.x - r.pos.x) / tab_width);
						if (tab_index < arr_len(node->tabs)) {
							u16 buffer_idx = node->tabs[tab_index];
							TextBuffer *buffer = &ted->buffers[buffer_idx];
							// close that tab
							if (buffer_unsaved_changes(buffer)) {
								// make sure unsaved changes dialog is opened
								ted_switch_to_buffer(ted, buffer);
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
			}

			TextRenderState text_state = text_render_state_default;
			for (u16 i = 0; i < ntabs; ++i) {
				TextBuffer *buffer = &ted->buffers[node->tabs[i]];
				char tab_title[256];
				char const *path = buffer_get_filename(buffer);
				char const *filename = path ? path_filename(path) : TED_UNTITLED;
				Rect tab_rect = rect(V2(r.pos.x + tab_width * i, r.pos.y), V2(tab_width, tab_bar_height));
				
				if (i > 0) {
					// make sure tab borders overlap (i.e. don't double the border thickness between tabs)
					tab_rect.pos.x  -= border_thickness;
					tab_rect.size.x += border_thickness;
				}

				if (node == ted->dragging_tab_node && i == ted->dragging_tab_idx) {
					// make tab follow mouse
					tab_rect.pos = v2_add(tab_rect.pos, v2_sub(ted->mouse_pos, ted->dragging_tab_origin));
				}
				
				// tab border
				gl_geometry_rect_border(tab_rect, border_thickness, colors[COLOR_BORDER]);
				tab_rect = rect_shrink(tab_rect, border_thickness);
				
				// tab title
				{
					if (buffer_unsaved_changes(buffer))
						strbuf_printf(tab_title, "*%s*", filename);
					else if (buffer->view_only)
						strbuf_printf(tab_title, "VIEW %s", filename);
					else
						strbuf_printf(tab_title, "%s", filename);
				}
				text_state.max_x = rect_x2(tab_rect);
				rgba_u32_to_floats(colors[COLOR_TEXT], text_state.color);
				text_state.x = tab_rect.pos.x;
				text_state.y = tab_rect.pos.y;
				text_utf8_with_state(font, &text_state, tab_title);

				if (i == node->active_tab) {
					// highlight active tab
					gl_geometry_rect(tab_rect, colors[is_active ? COLOR_ACTIVE_TAB_HL : COLOR_SELECTED_TAB_HL]);
					// set window title to active tab's title
					strbuf_printf(ted->window_title, "ted %s", tab_title);
				}
				
			}
			gl_geometry_draw();
			text_render(font);
		}

		u16 buffer_index = node->tabs[node->active_tab];
		TextBuffer *buffer = &ted->buffers[buffer_index];
		assert(ted->buffers_used[buffer_index]);
		Rect buffer_rect = rect_translate(r, V2(0, tab_bar_height));
		
		// make sure buffer border and tab border overlap
		buffer_rect.pos.y  -= border_thickness;
		buffer_rect.size.y += border_thickness;
		
		buffer_rect.size.y -= tab_bar_height;
		buffer_render(buffer, buffer_rect);
	} else {
		float padding = settings->padding;
		// this node is a split
		Node *a = &ted->nodes[node->split_a];
		Node *b = &ted->nodes[node->split_b];
		Rect r1 = r, r2 = r;
		SDL_Cursor *resize_cursor = node->split_vertical ? ted->cursor_resize_v : ted->cursor_resize_h;
		if (node == ted->resizing_split) {
			if (!(ted->mouse_state & SDL_BUTTON_LMASK)) {
				// let go of mouse
				ted->resizing_split = NULL;
			} else {
				// resize the split
				float mouse_coord = node->split_vertical ? ted->mouse_pos.y : ted->mouse_pos.x;
				float rect_coord1 = (node->split_vertical ? rect_y1 : rect_x1)(r);
				float rect_coord2 = (node->split_vertical ? rect_y2 : rect_x2)(r);
				// make sure the split doesn't make one of the sides too small
				float min_split = 50.0f / (node->split_vertical ? r.size.y : r.size.x);
				node->split_pos = clampf(normf(mouse_coord, rect_coord1, rect_coord2), min_split, 1-min_split);
			}
		}
		Rect r_between; // rectangle of space between r1 and r2
		if (node->split_vertical) {
			float split_pos = r.size.y * node->split_pos;
			r1.size.y = split_pos - padding;
			r2.pos.y += split_pos + padding;
			r2.size.y = r.size.y - split_pos - padding;
			r_between = rect(V2(r.pos.x, r.pos.y + split_pos - padding), V2(r.size.x, 2 * padding));
		} else {
			float split_pos = r.size.x * node->split_pos;
			r1.size.x = split_pos - padding;
			r2.pos.x += split_pos + padding;
			r2.size.x = r.size.x - split_pos - padding;
			r_between = rect(V2(r.pos.x + split_pos - padding, r.pos.y), V2(2 * padding, r.size.y));
		}
		if (rect_contains_point(r_between, ted->mouse_pos)) {
			ted->cursor = resize_cursor;
		}
		for (u32 i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
			if (rect_contains_point(r_between, ted->mouse_clicks[SDL_BUTTON_LEFT][i])) {
				ted->resizing_split = node;
			}
		}
		
		node_frame(ted, a, r1);
		node_frame(ted, b, r2);
	}
}

static void node_split(Ted *ted, Node *node, bool vertical) {
	if (node_depth(ted, (u16)(node - ted->nodes)) >= 4) return; // prevent splitting too deep

	if (arr_len(node->tabs) > 1) { // need at least 2 tabs to split
		i32 left_idx = ted_new_node(ted);
		i32 right_idx = ted_new_node(ted);
		if (left_idx >= 0 && right_idx >= 0) {
			Node *left = &ted->nodes[left_idx];
			Node *right = &ted->nodes[right_idx];
			u16 active_tab = node->active_tab;
			// put active tab on the right
			arr_add(right->tabs, node->tabs[active_tab]);
			for (u32 i = 0; i < arr_len(node->tabs); ++i) {
				if (i != active_tab) {
					// put all other tabs on the left
					arr_add(left->tabs, node->tabs[i]);
				}
			}

			arr_clear(node->tabs);
			node->split_a = (u16)left_idx;
			node->split_b = (u16)right_idx;
			node->split_vertical = vertical;
			node->split_pos = 0.5f;
			if (node == ted->active_node)
				ted_node_switch(ted, &ted->nodes[right_idx]);
		}
	}
}

static void node_split_switch(Ted *ted) {
	assert(ted->active_node);
	u16 active_node_idx = (u16)(ted->active_node - ted->nodes);
	i32 parent_idx = node_parent(ted, active_node_idx);
	if (parent_idx < 0) return;
	Node *parent = &ted->nodes[parent_idx];
	if (parent) {
		if (parent->split_a == active_node_idx) {
			ted_node_switch(ted, &ted->nodes[parent->split_b]);
		} else {
			ted_node_switch(ted, &ted->nodes[parent->split_a]);
		}
	}
}

static void node_split_swap(Ted *ted) {
	assert(ted->active_node);
	u16 active_node_idx = (u16)(ted->active_node - ted->nodes);
	i32 parent_idx = node_parent(ted, active_node_idx);
	if (parent_idx < 0) return;
	Node *parent = &ted->nodes[parent_idx];
	u16 temp = parent->split_a;
	parent->split_a = parent->split_b;
	parent->split_b = temp;
}
