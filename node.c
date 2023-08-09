// deals with ted's split-screen feature

#include "ted-internal.h"

static void node_switch_to_tab(Ted *ted, Node *node, u16 new_tab_index) {
	if (new_tab_index >= arr_len(node->tabs))
		return;
	
	node->active_tab = new_tab_index;
	if (node == ted->active_node) {
		// switch active buffer
		assert(node->tabs);
		TextBuffer *buffer = node->tabs[new_tab_index];
		ted_switch_to_buffer(ted, buffer);
	}
}

// move n tabs to the right
void node_tab_next(Ted *ted, Node *node, i32 n) {
	u16 ntabs = (u16)arr_len(node->tabs);
	if (!ntabs) return;
	u16 tab_idx = (u16)mod_i64((i64)node->active_tab + n, ntabs);
	node_switch_to_tab(ted, node, tab_idx);
}

void node_tab_prev(Ted *ted, Node *node, i32 n) {
	node_tab_next(ted, node, -n);
}

void node_tab_switch(Ted *ted, Node *node, i32 tab) {
	assert(node->tabs);
	if (tab >= 0 && tab < (i32)arr_len(node->tabs)) {
		node_switch_to_tab(ted, node, (u16)tab);
	}
}

void node_tabs_swap(Node *node, i32 tab1i, i32 tab2i) {
	if (tab1i < 0 || tab1i >= (i32)arr_len(node->tabs) || tab2i < 0 || tab2i >= (i32)arr_len(node->tabs))
		return;
	
	u16 tab1 = (u16)tab1i, tab2 = (u16)tab2i;
	if (node->active_tab == tab1) node->active_tab = tab2;
	else if (node->active_tab == tab2) node->active_tab = tab1;
	TextBuffer *temp = node->tabs[tab1];
	node->tabs[tab1] = node->tabs[tab2];
	node->tabs[tab2] = temp;
}

void node_free(Node *node) {
	arr_free(node->tabs);
	memset(node, 0, sizeof *node);
	free(node);
}

Node *node_parent(Ted *ted, Node *child) {
	if (!child)
		return NULL;
	arr_foreach_ptr(ted->nodes, NodePtr, pnode) {
		Node *node = *pnode;
		if (!node->tabs) {
			if (node->split_a == child || node->split_b == child)
				return node;
		}
	}
	return NULL;
}

// the root has depth 0, and a child node has 1 more than its parent's depth.
static u8 node_depth(Ted *ted, Node *node) {
	u8 depth = 0;
	while (node) {
		node = node_parent(ted, node);
		++depth;
	}
	return depth;
}

void node_join(Ted *ted, Node *node) {
	Node *parent = node_parent(ted, node);
	if (parent) {
		Node *a = parent->split_a;
		Node *b = parent->split_b;
		if (a->tabs && b->tabs) {
			if (ted->active_node == a || ted->active_node == b) {
				ted->active_node = parent;
			}
			arr_foreach_ptr(a->tabs, TextBufferPtr, tab) {
				arr_add(parent->tabs, *tab);
			}
			arr_foreach_ptr(b->tabs, TextBufferPtr, tab) {
				arr_add(parent->tabs, *tab);
			}
			if (!parent->tabs) {
				ted_out_of_mem(ted);
				return;
			}
			
			parent->split_a = NULL;
			parent->split_b = NULL;
			if (node == a) {
				parent->active_tab = a->active_tab;
			} else {
				parent->active_tab = (u16)arr_len(a->tabs) + b->active_tab;
			}
			node_free(a);
			node_free(b);
			arr_remove_item(ted->nodes, a);
			arr_remove_item(ted->nodes, b);
			
		}
	}
}

void node_close(Ted *ted, Node *node) {
	ted->dragging_tab_node = NULL;
	ted->resizing_split = NULL;
	if (!node) {
		return;
	}
	
	
	Node *parent = node_parent(ted, node);
	bool was_active = ted->active_node == node;

	// delete all associated buffers
	arr_foreach_ptr(node->tabs, TextBufferPtr, tab) {
		TextBuffer *buffer = *tab;
		ted_delete_buffer(ted, buffer);
	}

	node_free(node);

	if (!parent) {
		// no parent; this must be the root node
		ted->active_node = NULL;
	} else {
		// turn parent from split node into tab node
		if (parent->tabs) {
			assert(0); // this node's parent should be a split node
			return;
		}
		Node *other_side;
		if (node == parent->split_a) {
			other_side = parent->split_b;
		} else {
			assert(node == parent->split_b);
			other_side = parent->split_a;
		}
		// replace parent with other side of split
		*parent = *other_side;
		free(other_side);
		arr_remove_item(ted->nodes, other_side);
		if (was_active) {
			Node *new_active_node = parent;
			// make sure we don't set the active node to a split
			while (!new_active_node->tabs)
				new_active_node = new_active_node->split_a;
			ted_node_switch(ted, new_active_node);
		}
	}
	
	arr_remove_item(ted->nodes, node);
}


bool node_tab_close(Ted *ted, Node *node, i32 index) {
	ted->dragging_tab_node = NULL;

	u16 ntabs = (u16)arr_len(node->tabs);
	
	if (index < 0 || index >= ntabs) {
		return false;
	}
	
	if (ntabs == 1) {
		// only 1 tab left, just close the node
		node_close(ted, node);
		return false;
	} else {
		bool was_active = ted->active_node == node; // ted->active_node will be set to NULL when the active buffer is deleted.
		TextBuffer *buffer = node->tabs[index];
		// remove tab from array
		arr_remove(node->tabs, (size_t)index);
		ted_delete_buffer(ted, buffer);

		ntabs = (u16)arr_len(node->tabs); // update ntabs
		assert(ntabs);
		// fix active_tab
		if (index < node->active_tab)
			--node->active_tab;
		node->active_tab = clamp_u16(node->active_tab, 0, ntabs - 1);
		if (was_active) {
			// fix active buffer if necessary
			ted_switch_to_buffer(ted, node->tabs[node->active_tab]);
		}
		return true;
	}
}

void node_frame(Ted *ted, Node *node, Rect r) {
	const Settings *settings = ted_active_settings(ted);
	if (node->tabs) {
		bool is_active = node == ted->active_node;
		const u32 *colors = settings->colors;
		Font *font = ted->font;
		const float border_thickness = settings->border_thickness;
		const float char_height = text_font_char_height(font);
		float tab_bar_height = char_height + 2 * border_thickness;
		
		Rect tab_bar_rect = r;
		tab_bar_rect.size.y = tab_bar_height;

		{ // tab bar
			u16 ntabs = (u16)arr_len(node->tabs);
			float tab_width = r.size.x / ntabs;
			if (!menu_is_any_open(ted)) {
				arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_LEFT], MouseClick, click) {
					if (rect_contains_point(tab_bar_rect, click->pos)) {
						// click on tab to switch to it
						u16 tab_index = (u16)((click->pos.x - r.pos.x) / tab_width);
						if (tab_index < arr_len(node->tabs)) {
							ted->active_node = node;
							node_switch_to_tab(ted, node, tab_index);
							ted->dragging_tab_node = node;
							ted->dragging_tab_idx = tab_index;
							ted->dragging_tab_origin = click->pos;
						}
					}
				}
				if (ted->dragging_tab_node) {
					// check if user dropped tab here
					arr_foreach_ptr(ted->mouse_releases[SDL_BUTTON_LEFT], MouseRelease, release) {
						if (rect_contains_point(tab_bar_rect, release->pos)) {
							u16 tab_index = (u16)roundf((release->pos.x - r.pos.x) / tab_width);
							if (tab_index <= arr_len(node->tabs)) {
								Node *drag_node = ted->dragging_tab_node;
								u16 drag_index = ted->dragging_tab_idx;
								TextBuffer *tab = drag_node->tabs[drag_index];

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
									node_close(ted, drag_node);
								} else {
									// make sure active tab is valid
									drag_node->active_tab = clamp_u16(drag_node->active_tab, 0, (u16)arr_len(drag_node->tabs) - 1);
								}

								ted->dragging_tab_node = NULL; // stop dragging
								// switch to this buffer
								ted_switch_to_buffer(ted, tab);
							}
						}
					}
				}
				arr_foreach_ptr(ted->mouse_clicks[SDL_BUTTON_MIDDLE], MouseClick, click) {
					// middle-click to close tab
					if (rect_contains_point(tab_bar_rect, click->pos)) {
						u16 tab_index = (u16)((click->pos.x - r.pos.x) / tab_width);
						if (tab_index < arr_len(node->tabs)) {
							TextBuffer *buffer = node->tabs[tab_index];
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
				TextBuffer *buffer = node->tabs[i];
				char tab_title[256];
				const char *filename = buffer_display_filename(buffer);
				Rect tab_rect = rect_xywh(r.pos.x + tab_width * i, r.pos.y, tab_width, tab_bar_height);
				
				if (i > 0) {
					// make sure tab borders overlap (i.e. don't double the border thickness between tabs)
					tab_rect.pos.x  -= border_thickness;
					tab_rect.size.x += border_thickness;
				}

				if (node == ted->dragging_tab_node && i == ted->dragging_tab_idx) {
					// make tab follow mouse
					tab_rect.pos = vec2_add(tab_rect.pos, vec2_sub(ted->mouse_pos, ted->dragging_tab_origin));
				}
				
				// tab border
				gl_geometry_rect_border(tab_rect, border_thickness, colors[COLOR_BORDER]);
				rect_shrink(&tab_rect, border_thickness);
				
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
				text_state_break_kerning(&text_state);
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

		TextBuffer *buffer = node->tabs[node->active_tab];
		Rect buffer_rect = r;
		buffer_rect.pos.y += tab_bar_height;
		
		// make sure buffer border and tab border overlap
		buffer_rect.pos.y  -= border_thickness;
		buffer_rect.size.y += border_thickness;
		
		buffer_rect.size.y -= tab_bar_height;
		buffer_render(buffer, buffer_rect);
	} else {
		float padding = settings->padding;
		// this node is a split
		Node *a = node->split_a, *b = node->split_b;
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
			r_between = rect_xywh(r.pos.x, r.pos.y + split_pos - padding, r.size.x, 2 * padding);
		} else {
			float split_pos = r.size.x * node->split_pos;
			r1.size.x = split_pos - padding;
			r2.pos.x += split_pos + padding;
			r2.size.x = r.size.x - split_pos - padding;
			r_between = rect_xywh(r.pos.x + split_pos - padding, r.pos.y, 2 * padding, r.size.y);
		}
		if (rect_contains_point(r_between, ted->mouse_pos)) {
			ted->cursor = resize_cursor;
		}
		if (ted_clicked_in_rect(ted, r_between)) 
			ted->resizing_split = node;
		
		node_frame(ted, a, r1);
		node_frame(ted, b, r2);
	}
}

void node_split(Ted *ted, Node *node, bool vertical) {
	if (node_depth(ted, node) >= 4) return; // prevent splitting too deep

	if (arr_len(node->tabs) > 1) { // need at least 2 tabs to split
		Node *left = ted_new_node(ted);
		Node *right = ted_new_node(ted);
		if (left && right) {
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
			node->split_a = left;
			node->split_b = right;
			node->split_vertical = vertical;
			node->split_pos = 0.5f;
			if (node == ted->active_node)
				ted_node_switch(ted, right);
		}
	}
}

void node_split_switch(Ted *ted) {
	Node *parent = node_parent(ted, ted->active_node);
	if (!parent) return;
	if (parent->split_a == ted->active_node) {
		ted_node_switch(ted, parent->split_b);
	} else {
		ted_node_switch(ted, parent->split_a);
	}
}

void node_split_swap(Ted *ted) {
	assert(ted->active_node);
	Node *parent = node_parent(ted, ted->active_node);
	if (!parent) return;
	Node *temp = parent->split_a;
	parent->split_a = parent->split_b;
	parent->split_b = temp;
}
