// handles ted's find and replace menu.

#include "ted-internal.h"
#include "pcre-inc.h"

#define FIND_MAX_GROUPS 50
struct FindResult {
	BufferPos start;
	BufferPos end;
};

static u32 find_compilation_flags(Ted *ted) {
	return (ted->find_case_sensitive ? 0 : PCRE2_CASELESS)
		| (ted->find_regex ? 0 : PCRE2_LITERAL);
}

static u32 find_replace_flags(Ted *ted) {
	return (ted->find_regex ? 0 : PCRE2_SUBSTITUTE_LITERAL);
}

TextBuffer *find_search_buffer(Ted *ted) {
	if (ted->active_buffer
		&& ted->active_buffer != ted->find_buffer
		&& ted->active_buffer != ted->replace_buffer) {
		return ted->active_buffer;
	}
	return ted->prev_active_buffer;
}



static void ted_error_from_pcre2_error(Ted *ted, int err) {
	char32_t buf[256] = {0};
	size_t len = (size_t)pcre2_get_error_message_32(err, buf, arr_count(buf) - 1);
	char *error_cstr = str32_to_utf8_cstr(str32(buf, len));
	if (error_cstr) {
		ted_error(ted, "Search error: %s.", error_cstr);
		free(error_cstr);
	}
}

static bool find_compile_pattern(Ted *ted) {
	TextBuffer *find_buffer = ted->find_buffer;
	String32 term = buffer_get_line(find_buffer, 0);
	if (term.len) {
		pcre2_match_data_32 *match_data = pcre2_match_data_create_32(FIND_MAX_GROUPS, NULL);
		if (match_data) {
			int error = 0;
			PCRE2_SIZE error_pos = 0;
			pcre2_code_32 *code = pcre2_compile_32(term.str, term.len, find_compilation_flags(ted), &error, &error_pos, NULL);
			if (code) {
				ted->find_code = code;
				ted->find_match_data = match_data;
				ted->find_invalid_pattern = false;
				return true;
			} else {
				ted->find_invalid_pattern = true;
			}
			pcre2_match_data_free_32(match_data);
		} else {
			ted_error(ted, "Out of memory.");
		}
	} else {
		ted->find_invalid_pattern = false;
	}
	return false;
}

static void find_free_pattern(Ted *ted) {
	if (ted->find_code) {
		pcre2_code_free_32(ted->find_code);
		ted->find_code = NULL;
	}
	if (ted->find_match_data) {
		pcre2_match_data_free_32(ted->find_match_data);
		ted->find_match_data = NULL;
	}
	arr_clear(ted->find_results);
}

float find_menu_height(Ted *ted) {
	Font *font = ted->font;
	float char_height = text_font_char_height(font);
	const Settings *settings = ted_active_settings(ted);
	const float padding = settings->padding;
	const float border_thickness = settings->border_thickness;
	const float line_buffer_height = ted_line_buffer_height(ted);

	return 3 * char_height + 4 * border_thickness + (padding + line_buffer_height) * ted->replace + 6 * padding;
}

// finds the next match in the buffer, returning false if there is no match this line.
// sets *match_start and *match_end (if not NULL) to the start and end of the match, respectively
// advances *pos to the end of the match or the start of the next line if there is no match.
// direction should be either +1 (forwards) or -1 (backwards)
static WarnUnusedResult bool find_match(Ted *ted, BufferPos *pos, u32 *match_start, u32 *match_end, int direction) {
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return false;
	String32 str = buffer_get_line(buffer, pos->line);
	PCRE2_SIZE *groups = pcre2_get_ovector_pointer_32(ted->find_match_data);

	u32 match_flags = PCRE2_NOTEMPTY;
	
	int ret;
	if (direction == +1)
		ret = pcre2_match_32(ted->find_code, str.str, str.len, pos->index, match_flags, ted->find_match_data, NULL);
	else {
		// unfortunately PCRE does not have a backwards option, so we need to do the search multiple times
		u32 last_pos = 0;
		ret = -1;
		while (1) {
			int next_ret = pcre2_match_32(ted->find_code, str.str, pos->index, last_pos, match_flags, ted->find_match_data, NULL);
			if (next_ret > 0) {
				ret = next_ret;
				last_pos = (u32)groups[1];
			} else break;
		}
	}
	if (ret > 0) {
		if (match_start) *match_start = (u32)groups[0];
		if (match_end)   *match_end   = (u32)groups[1];
		pos->index = (u32)groups[1];
		return true;
	} else {
		pos->line += (u32)((i64)buffer_line_count(buffer) + direction);
		pos->line %= buffer_line_count(buffer);
		if (direction == +1)
			pos->index = 0;
		else
			pos->index = (u32)buffer_get_line(buffer, pos->line).len;
		return false;
	}
}

static void find_search_line(Ted *ted, u32 line, FindResult **results) {
	u32 match_start=0, match_end=0;
	BufferPos pos = {.line = line, .index = 0};
	while (find_match(ted, &pos, &match_start, &match_end, +1)) {
		BufferPos match_start_pos = {.line = pos.line, .index = match_start};
		BufferPos match_end_pos = {.line = pos.line, .index = match_end};
		FindResult result = {match_start_pos, match_end_pos};
		arr_add(*results, result);
	}
}

void find_redo_search(Ted *ted) {
	u32 flags = find_compilation_flags(ted);
	ted->find_flags = flags;
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return;

	find_free_pattern(ted);

	if (find_compile_pattern(ted)) {
		BufferPos best_scroll_candidate = {U32_MAX, U32_MAX};
		BufferPos cursor_pos = buffer_cursor_pos(buffer);
		// find all matches
		arr_clear(ted->find_results);
		for (u32 line = 0, count = buffer_line_count(buffer); line < count; ++line) {
			find_search_line(ted, line, &ted->find_results);
		}
		
		arr_foreach_ptr(ted->find_results, FindResult, res) {
			if (best_scroll_candidate.line == U32_MAX 
			|| (buffer_pos_cmp(best_scroll_candidate, cursor_pos) < 0 && buffer_pos_cmp(res->start, cursor_pos) >= 0))
				best_scroll_candidate = res->start;
		}
		
		if (best_scroll_candidate.line != U32_MAX) // scroll to first match (if there is one)
			buffer_scroll_to_pos(buffer, best_scroll_candidate);
	}
}

// returns the index of the match we are "on", or U32_MAX for none.
static u32 find_match_idx(Ted *ted) {
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return U32_MAX;
	arr_foreach_ptr(ted->find_results, FindResult, result) {
		u32 index = (u32)(result - ted->find_results);
		BufferPos cur_pos = buffer_cursor_pos(buffer), sel_pos = {0};
		if (buffer_pos_eq(result->start, cur_pos))
			return index;
		if (buffer_selection_pos(buffer, &sel_pos)
			&& buffer_pos_eq(result->start, sel_pos)
			&& buffer_pos_eq(result->end, cur_pos))
			return index;
	}
	return U32_MAX;
}


static void find_next_in_direction(Ted *ted, int direction) {
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return;
	
	BufferPos cursor_pos = buffer_cursor_pos(buffer);
	BufferPos pos = cursor_pos;
	if (direction == -1) {
		// start from selection pos if there is one
		buffer_selection_pos(buffer, &pos);
	}
	
	u32 nlines = buffer_line_count(buffer);
	
	// we need to search the starting line twice, because we might start at a non-zero index
	for (size_t nsearches = 0; nsearches < nlines + 1; ++nsearches) {
		u32 match_start, match_end;
		if (find_match(ted, &pos, &match_start, &match_end, direction)) {
			if (nsearches == 0 && match_start == cursor_pos.index) {
				// if you click "next" and your cursor is on a match, it should go to the next
				// one, not the one you're on
			} else {
				BufferPos pos_start = {.line = pos.line, .index = match_start};
				BufferPos pos_end = {.line = pos.line, .index = match_end};
				buffer_cursor_move_to_pos(buffer, pos_start);
				buffer_select_to_pos(buffer, pos_end);
				break;
			}
		}
	}
}

// returns true if successful.
// this function zeroes but keeps around the old find result! make sure you call find_redo_search after calling this function
// one or more times!
static bool find_replace_match(Ted *ted, u32 match_idx) {
	if (!ted->find_code) return false;
	
	bool success = false;
	FindResult match = ted->find_results[match_idx];
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return false;
	if (!buffer_pos_valid(buffer, match.start) || !buffer_pos_valid(buffer, match.end))
		return false;
	assert(match.start.line == match.end.line);
	String32 line = buffer_get_line(buffer, match.start.line);
	String32 replacement = buffer_get_line(ted->replace_buffer, 0);
	// we are currently highlighting the find pattern, let's replace it
	
	// get size of buffer needed.
	PCRE2_SIZE output_size = 0;
	u32 flags = find_replace_flags(ted);
	char32_t *str = line.str + match.start.index;
	u32 len = match.end.index - match.start.index;
	
	int ret = pcre2_substitute_32(ted->find_code, str, len, 0,
		PCRE2_SUBSTITUTE_OVERFLOW_LENGTH|flags, ted->find_match_data, NULL, replacement.str,
		replacement.len, NULL, &output_size);
	char32_t *output_buffer = output_size
		? calloc(output_size, sizeof *output_buffer)
		: NULL;
	if (output_buffer || !output_size) {
		ret = pcre2_substitute_32(ted->find_code, str, len, 0,
			flags, ted->find_match_data, NULL, replacement.str,
			replacement.len, output_buffer, &output_size);
		if (ret > 0) {
			buffer_deselect(buffer); // stop selecting match
			buffer_delete_chars_at_pos(buffer, match.start, len);
			if (output_buffer)
				buffer_insert_text_at_pos(buffer, match.start, str32(output_buffer, output_size));
			
			i64 diff = (i64)output_size - len; // change in number of characters
			for (u32 i = match_idx; i < arr_len(ted->find_results); ++i) {
				FindResult *result = &ted->find_results[i];
				if (result->start.line == match.start.line) {
					// fix indices of find results on this line to the right of match.
					result->start.index = (u32)(result->start.index + diff);
					result->end.index = (u32)(result->end.index + diff);
				} else break;
			}
			success = true;
		} else if (ret < 0) {
			ted_error_from_pcre2_error(ted, ret);
		}
		free(output_buffer);
	} else {
		ted_error(ted, "Out of memory.");
	}
	return success;
}

void find_replace(Ted *ted) {
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return;
	u32 match_idx = find_match_idx(ted);
	if (match_idx != U32_MAX) {
		buffer_cursor_move_to_pos(buffer, ted->find_results[match_idx].start); // move to start of match
		find_replace_match(ted, match_idx);
		find_redo_search(ted);
	}
}

void find_next(Ted *ted) {
	if (ted->replace) {
		find_replace(ted);
	}
	find_next_in_direction(ted, +1);
}

void find_prev(Ted *ted) {
	find_next_in_direction(ted, -1);
}

void find_replace_all(Ted *ted) {
	TextBuffer *buffer = find_search_buffer(ted);
	if (!buffer) return;
	if (ted->replace) {
		u32 match_idx = find_match_idx(ted);
		if (match_idx == U32_MAX) {
			// if we're not on a match, go to the next one
			find_next(ted);
			match_idx = find_match_idx(ted);
		}
		if (match_idx != U32_MAX) {
			{
				FindResult *last_result = arr_lastp(ted->find_results);
				buffer_cursor_move_to_pos(buffer, last_result->start);
			}
			buffer_start_edit_chain(buffer);
			for (u32 i = match_idx; i < arr_len(ted->find_results); ++i) {
				if (!find_replace_match(ted, i))
					break;
			}
			buffer_end_edit_chain(buffer);
			find_redo_search(ted);
		}
	}
}

void find_menu_frame(Ted *ted, Rect menu_bounds) {
	Font *font = ted->font, *font_bold = ted->font_bold;
	const float char_height = text_font_char_height(font);

	const Settings *settings = ted_active_settings(ted);
	const u32 color_text = settings_color(settings, COLOR_TEXT);
	const float padding = settings->padding;
	const float border_thickness = settings->border_thickness;
	bool const replace = ted->replace;
	const float line_buffer_height = ted_line_buffer_height(ted);
	
	TextBuffer *buffer = find_search_buffer(ted),
		*find_buffer = ted->find_buffer, *replace_buffer = ted->replace_buffer;
	if (!buffer) return;
	
	u32 first_rendered_line = buffer_first_rendered_line(buffer);
	u32 last_rendered_line = buffer_last_rendered_line(buffer);
	

	gl_geometry_rect(menu_bounds, settings_color(settings, COLOR_MENU_BG));
	gl_geometry_rect_border(menu_bounds, border_thickness, settings_color(settings, COLOR_BORDER));
	rect_shrink(&menu_bounds, border_thickness);

	float x1, y1, x2, y2;
	rect_coords(menu_bounds, &x1, &y1, &x2, &y2);

	x1 += padding;
	y1 += padding;
	x2 -= padding;
	y2 -= padding;
	
	const char *prev_text = "Previous", *next_text = "Next";
	const char *replace_text = "Replace", *replace_find_text = "Replace+find", *replace_all_text = "Replace all";
	vec2 prev_size = button_get_size(ted, prev_text);
	vec2 next_size = button_get_size(ted, next_text);
	vec2 replace_size = button_get_size(ted, replace_text);
	vec2 replace_find_size = button_get_size(ted, replace_find_text);
	vec2 replace_all_size = button_get_size(ted, replace_all_text);
	
	float x = x1, y = y2 - prev_size.y;
	// compute positions of buttons
	Rect button_prev = rect((vec2){x, y}, prev_size);
	x += button_prev.size.x + padding;
	Rect button_next = rect((vec2){x, y}, next_size);
	x += button_next.size.x + padding;
	Rect button_replace = rect((vec2){x, y}, replace_size);
	x += button_replace.size.x + padding;
	Rect button_replace_find = rect((vec2){x, y}, replace_find_size);
	x += button_replace_find.size.x + padding;
	Rect button_replace_all = rect((vec2){x, y}, replace_all_size);
	x += button_replace_all.size.x + padding;
	
	
	if (button_update(ted, button_prev))
		find_next_in_direction(ted, -1);
	if (button_update(ted, button_next))
		find_next_in_direction(ted, +1);
	if (replace) {
		if (button_update(ted, button_replace_find))
			find_next(ted);
		if (button_update(ted, button_replace))
			find_replace(ted);
		if (button_update(ted, button_replace_all))
			find_replace_all(ted);
	}
	
	if (ted->find_flags != find_compilation_flags(ted))
		 find_redo_search(ted);
	arr_foreach_ptr(ted->find_results, FindResult, result) {
		// highlight matches
		BufferPos p1 = result->start, p2 = result->end;
		if (p2.line >= first_rendered_line && p1.line <= last_rendered_line) {
			vec2 pos1 = buffer_pos_to_pixels(buffer, p1);
			vec2 pos2 = buffer_pos_to_pixels(buffer, p2);
			pos2.y += char_height;
			Rect hl_rect = rect4(pos1.x, pos1.y, pos2.x, pos2.y);
			if (buffer_clip_rect(buffer, &hl_rect))
				gl_geometry_rect(hl_rect, settings_color(settings, COLOR_FIND_HL));
		}
	}
	
	const char *find_text = "Find...", *replace_with_text = "Replace with";
	float text_width = 0;
	text_get_size(font_bold, replace ? replace_with_text : find_text, &text_width, NULL);
	

	Rect find_buffer_bounds = rect4(x1 + text_width + padding, y1, x2 - padding, y1 + line_buffer_height);
	Rect replace_buffer_bounds = find_buffer_bounds;
	replace_buffer_bounds.pos.y += line_buffer_height + padding;

	
	button_render(ted, button_prev, prev_text, color_text);
	button_render(ted, button_next, next_text, color_text);
	if (replace) {
		button_render(ted, button_replace, replace_text, color_text);
		button_render(ted, button_replace_find, replace_find_text, color_text);
		button_render(ted, button_replace_all, replace_all_text, color_text);
	}
	
	{
		float w = 0, h = 0;
		char str[32];
		u32 match_idx = find_match_idx(ted);
		if (match_idx == U32_MAX) {
			strbuf_printf(str, "%" PRIu32 " matches", arr_len(ted->find_results));
		} else {
			strbuf_printf(str, "%" PRIu32 " of %" PRIu32, match_idx + 1, arr_len(ted->find_results));
		}
		text_get_size(font, str, &w, &h);
		text_utf8(font, str, x2 - w, rect_ymid(find_buffer_bounds) - h * 0.5f, color_text);
		x2 -= w;
		find_buffer_bounds.size.x -= w;
	}

	text_utf8(font_bold, find_text, x1, y1, color_text);
	y1 += line_buffer_height + padding;
	
	if (replace) {
		text_utf8(font_bold, replace_with_text, x1, y1, color_text);
		y1 += line_buffer_height + padding;
	}
	
	gl_geometry_draw();
	text_render(font_bold);
	
	x = x1;
	x += checkbox_frame(ted, &ted->find_case_sensitive, "Case sensitive", (vec2){x, y1}).x + 2*padding;
	x += checkbox_frame(ted, &ted->find_regex, "Regular expression", (vec2){x, y1}).x + 2*padding;

	buffer_render(find_buffer, find_buffer_bounds);
	if (replace) buffer_render(replace_buffer, replace_buffer_bounds);
	
	String32 term = buffer_get_line(find_buffer, 0);
	
	if (ted->find_invalid_pattern)
		gl_geometry_rect(find_buffer_bounds, settings_color(settings, COLOR_NO) & 0xFFFFFF3F); // invalid regex
	else if (term.len && !ted->find_results)
		gl_geometry_rect(find_buffer_bounds, settings_color(settings, COLOR_CANCEL) & 0xFFFFFF3F); // no matches
	gl_geometry_draw();

}

void find_open(Ted *ted, bool replace) {
	if (menu_is_any_open(ted)) return;
	if (ted->active_buffer == ted->build_buffer) return; 
	if (!ted->find)
		ted->prev_active_buffer = ted->active_buffer;
	ted_switch_to_buffer(ted, ted->find_buffer);
	buffer_select_all(ted->active_buffer);
	ted->find = true;
	ted->replace = replace;
	find_redo_search(ted);
}

void find_close(Ted *ted) {
	ted->find = false;
	ted_switch_to_buffer(ted, find_search_buffer(ted));
	find_free_pattern(ted);
}

// index of first result with at least this line number
static u32 find_first_result_with_line(Ted *ted, u32 line) {
	u32 lo = 0;
	u32 hi = arr_len(ted->find_results);
	if (hi == 0) {
		return 0;
	}
	// all find results come before this line
	if (ted->find_results[hi - 1].start.line < line)
		return hi;
	
	while (lo + 1 < hi) {
		u32 mid = (lo + hi) / 2;
		u32 mid_line = ted->find_results[mid].start.line;
		if (mid_line >= line && mid > 0 && ted->find_results[mid - 1].start.line < line)
			return mid;
		if (line > mid_line) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

// update search results for the given range of lines
static void find_research_lines(Ted *ted, u32 line0, u32 line1) {
	FindResult *new_results = NULL;
	for (u32 l = line0; l <= line1; ++l) {
		find_search_line(ted, l, &new_results);
	}
	u32 i0 = find_first_result_with_line(ted, line0);
	u32 i1 = find_first_result_with_line(ted, line1 + 1);
	i32 diff = (i32)arr_len(new_results) - (i32)(i1 - i0);
	if (diff < 0) {
		arr_remove_multiple(ted->find_results, i0, (u32)-diff);
	} else if (diff > 0) {
		arr_insert_multiple(ted->find_results, i0, (u32)diff);
	}
	memcpy(&ted->find_results[i0], new_results, arr_len(new_results) * sizeof *new_results);
}
		
static void find_edit_notify(void *context, TextBuffer *buffer, const EditInfo *info) {
	Ted *ted = context;
	if (!ted->find) {
		return;
	}
	if (buffer == find_search_buffer(ted)) {
		const u32 line = info->pos.line;
		
		if (info->chars_inserted) {
			const u32 newlines_inserted = info->end.line - info->pos.line;
			
			if (newlines_inserted) {
				// update line numbers for find results after insertion.
				arr_foreach_ptr(ted->find_results, FindResult, res) {
					if (res->start.line > line) {
						res->start.line += newlines_inserted;
						res->end.line += newlines_inserted;
					}
				}
			}
			
			find_research_lines(ted, line, line + newlines_inserted);
			
		} else if (info->chars_deleted) {
			const u32 newlines_deleted = info->end.line - info->pos.line;
			
			if (newlines_deleted) {
				// update line numbers for find results after deletion.
				arr_foreach_ptr(ted->find_results, FindResult, res) {
					if (res->start.line >= line + newlines_deleted) {
						res->start.line -= newlines_deleted;
						res->end.line -= newlines_deleted;
					}
				}
				
			}
			
			find_research_lines(ted, line, line);
		}
	} else if (buffer == ted->find_buffer) {
		find_redo_search(ted);
		buffer_scroll_to_cursor(buffer);	
	}
}

void find_init(Ted *ted) {
	ted_add_edit_notify(ted, find_edit_notify, ted);
}
