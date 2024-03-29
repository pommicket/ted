// JSON parser for LSP
// provides FAST(ish) parsing but SLOW lookup for large objects

#define LSP_INTERNAL 1
#include "lsp.h"
#include "util.h"
#include "unicode.h"

#define SKIP_WHITESPACE while (json_is_space(text[index])) ++index;

const char *json_type_to_str(JSONValueType type) {
	switch (type) {
	case JSON_UNDEFINED:
		return "undefined";
	case JSON_NULL:
		return "null";
	case JSON_STRING:
		return "string";
	case JSON_NUMBER:
		return "number";
	case JSON_FALSE:
		return "false";
	case JSON_TRUE:
		return "true";
	case JSON_ARRAY:
		return "array";
	case JSON_OBJECT:
		return "object";
	}
	assert(0);
	return "???";
}

static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val);
void json_debug_print_value(const JSON *json, JSONValue value);

// defining this instead of using isspace seems to be faster
// probably because isspace depends on the locale.
static bool json_is_space(char c) {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void json_debug_print_array(const JSON *json, JSONArray array) {
	printf("[");
	for (u32 i = 0; i < array.len; ++i) {
		json_debug_print_value(json, json->values[array.elements + i]);
		printf(", ");
	}
	printf("]");
}

void json_debug_print_object(const JSON *json, JSONObject obj) {
	printf("{");
	for (u32 i = 0; i < obj.len; ++i) {
		json_debug_print_value(json, json->values[obj.items + i]);
		printf(": ");
		json_debug_print_value(json, json->values[obj.items + obj.len + i]);
		printf(", ");
	}
	printf("}");
}

void json_debug_print_string(const JSON *json, JSONString string) {
	printf("\"%.*s\"",
		(int)string.len,
		json->text + string.pos);
}

void json_debug_print_value(const JSON *json, JSONValue value) {
	switch (value.type) {
	case JSON_UNDEFINED: printf("undefined"); break;
	case JSON_NULL: printf("null"); break;
	case JSON_FALSE: printf("false"); break;
	case JSON_TRUE: printf("true"); break;
	case JSON_NUMBER: printf("%g", value.val.number); break;
	case JSON_STRING: {
		json_debug_print_string(json, value.val.string);
		} break;
	case JSON_ARRAY: {
		json_debug_print_array(json, value.val.array);
		} break;
	case JSON_OBJECT: {
		json_debug_print_object(json, value.val.object);
		} break;
	}
}

// count number of comma-separated values until
// closing ] or }
static u32 json_count(JSON *json, u32 index) {
	int bracket_depth = 0;
	int brace_depth = 0;
	u32 count = 1;
	const char *text = json->text;
	SKIP_WHITESPACE;
	// special case: empty object/array
	if (text[index] == '}' || text[index] == ']')
		return 0;
	
// 	int mark[5000] = {0};
	for (; ; ++index) {
		switch (text[index]) {
		case '\0':
			return 0; // bad no closing bracket
		case '[':
			++bracket_depth;
			break;
		case ']':
			--bracket_depth;
			if (bracket_depth < 0)
				return count;
			break;
		case '{':
			++brace_depth;
// 			mark[index] = 1;
			break;
		case '}':
			--brace_depth;
// 			mark[index] = 1;
			if (brace_depth < 0){
			// useful visualization for debugging
// 			for (int i = 0; text[i]; ++i ){
// 				switch (mark[i]){
// 				case 1: printf("\x1b[91m"); break;
// 				case 2: printf("\x1b[92m"); break;
// 				}
// 				printf("%c",text[i]);
// 				if (mark[i]) printf("\x1b[0m");
// 			}
// 			printf("\n");
			
				return count;
			}
			break;
		case ',':
			if (bracket_depth == 0 && brace_depth == 0)
				++count;
			break;
		case '"': {
			++index; // skip opening "
			int escaped = 0;
			for (; ; ++index) {
// 				mark[index] = 2;
				switch (text[index]) {
				case '\0': return 0; // bad no closing quote
				case '\\': escaped = !escaped; break;
				case '"':
					if (!escaped)
						goto done;
					escaped = false;
					break;
				default:
					escaped = false;
					break;
				}
			}
			done:;
			} break;
		}
	}
}

static bool json_parse_object(JSON *json, u32 *p_index, JSONObject *object) {
	u32 index = *p_index;
	const char *text = json->text;
	++index; // go past {
	u32 count = json_count(json, index);
	object->len = count;
	object->items = arr_len(json->values);
	arr_set_len(json->values, arr_len(json->values) + 2 * count);
	
	for (u32 i = 0; i < count; ++i) {
		if (i > 0) {
			if (text[index] != ',') {
				strbuf_printf(json->error, "stuff after value in object");
				return false;
			}
			++index;
		}
		SKIP_WHITESPACE;
		JSONValue name = {0}, value = {0};
		if (!json_parse_value(json, &index, &name))
			return false;
		if (name.type != JSON_STRING) {
			strbuf_printf(json->error, "object key is not a string");
			return false;
		}
		SKIP_WHITESPACE;
		if (text[index] != ':') {
			strbuf_printf(json->error, "stuff after name in object");
			return false;
		}
		++index; // skip :
		SKIP_WHITESPACE;
		if (!json_parse_value(json, &index, &value))
			return false;
		SKIP_WHITESPACE;
		json->values[object->items + i] = name;
		json->values[object->items + count + i] = value;
	}
	
	if (text[index] != '}') {
		strbuf_printf(json->error, "mismatched brackets or quotes.");
		return false;
	}
	++index; // skip }
	*p_index = index;
	return true;
}

static bool json_parse_array(JSON *json, u32 *p_index, JSONArray *array) {
	u32 index = *p_index;
	const char *text = json->text;
	++index; // go past [
	u32 count = json_count(json, index);
	array->len = count;
	array->elements = arr_len(json->values);
	
	arr_set_len(json->values, arr_len(json->values) + count);
	
	SKIP_WHITESPACE;
	
	for (u32 i = 0; i < count; ++i) {
		if (i > 0) {
			if (text[index] != ',') {
				strbuf_printf(json->error, "stuff after element in array");
				return false;
			}
			++index;
		}
		SKIP_WHITESPACE;
		JSONValue element = {0};
		if (!json_parse_value(json, &index, &element))
			return false;
		SKIP_WHITESPACE;
		json->values[array->elements + i] = element;
	}
	
	if (text[index] != ']') {
		strbuf_printf(json->error, "mismatched brackets or quotes.");
		return false;
	}
	++index; // skip ]
	*p_index = index;
	return true;
}

static bool json_parse_string(JSON *json, u32 *p_index, JSONString *string) {
	u32 index = *p_index;
	++index; // skip opening "
	string->pos = index;
	const char *text = json->text;
	bool escaped = false;
	for (; ; ++index) {
		switch (text[index]) {
		case '"':
			if (!escaped)
				goto done;
			escaped = false;
			break;
		case '\\':
			escaped = !escaped;
			break;
		case '\0':
			strbuf_printf(json->error, "string literal goes to end of JSON");
			return false;
		default:
			escaped = false;
			break;
		}
	}
	done:
	string->len = index - string->pos;
	++index; // skip closing "
	*p_index = index;
	return true;
}

static bool json_parse_number(JSON *json, u32 *p_index, double *number) {
	char *endp = NULL;
	const char *text = json->text;
	u32 index = *p_index;
	*number = strtod(text + index, &endp);
	if (endp == text + index) {
		strbuf_printf(json->error, "bad number");
		return false;
	}
	index = (u32)(endp - text);
	*p_index = index;
	return true;
}

static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val) {
	const char *text = json->text;
	u32 index = *p_index;
	SKIP_WHITESPACE;
	switch (text[index]) {
	case '{':
		val->type = JSON_OBJECT;
		if (!json_parse_object(json, &index, &val->val.object))
			return false;
		break;
	case '[':
		val->type = JSON_ARRAY;
		if (!json_parse_array(json, &index, &val->val.array))
			return false;
		break;
	case '"':
		val->type = JSON_STRING;
		if (!json_parse_string(json, &index, &val->val.string))
			return false;
		break;
	case ANY_DIGIT:
	case '-':
	case '+':
		val->type = JSON_NUMBER;
		if (!json_parse_number(json, &index, &val->val.number))
			return false;
		break;
	case 'f':
		val->type = JSON_FALSE;
		if (!str_has_prefix(&text[index], "false"))
			return false;
		index += 5;
		break;
	case 't':
		val->type = JSON_TRUE;
		if (!str_has_prefix(&text[index], "true"))
			return false;
		index += 4;
		break;
	case 'n':
		val->type = JSON_NULL;
		if (!str_has_prefix(&text[index], "null"))
			return false;
		index += 4;
		break;
	default:
		strbuf_printf(json->error, "bad value");
		return false;
	}
	*p_index = index;
	return true;
}

void json_free(JSON *json) {
	arr_free(json->values);
	// important we don't zero json here because we want to preserve json->error.
	if (json->is_text_copied) {
		free((void*)json->text);
	}
	json->text = NULL;
}

bool json_parse(JSON *json, const char *text) {
	memset(json, 0, sizeof *json);
	json->text = text;
	arr_reserve(json->values, strlen(text) / 8);
	arr_addp(json->values); // add root
	JSONValue val = {0};
	u32 index = 0;
	if (!json_parse_value(json, &index, &val)) {
		json_free(json);
		return false;
	}
	SKIP_WHITESPACE;
	if (text[index]) {
		json_free(json);
		strbuf_printf(json->error, "extra text after end of root object");
		return false;
	}
	json->values[0] = val;
	return true;
}

bool json_parse_copy(JSON *json, const char *text) {
	bool success = json_parse(json, str_dup(text));
	if (success) {
		json->is_text_copied = true;
		return true;
	} else {
		free((void*)json->text);
		json->text = NULL;
		return false;
	}
}

static bool json_streq(const JSON *json, const JSONString *string, const char *name) {
	const char *p = &json->text[string->pos];
	const char *end = p + string->len;
	for (; p < end; ++p, ++name) {
		if (*name != *p)
			return false;
	}
	return *name == '\0';
}

JSONValue json_object_get(const JSON *json, JSONObject object, const char *name) {
	const JSONValue *items = &json->values[object.items];
	for (u32 i = 0; i < object.len; ++i) {
		const JSONValue *this_name = items++;
		assert(this_name->type == JSON_STRING);
		if (json_streq(json, &this_name->val.string, name)) {
			return json->values[object.items + object.len + i];
		}
	}
	return (JSONValue){0};
}

JSONValue json_array_get(const JSON *json, JSONArray array, u64 i) {
	if (i < array.len) {
		return json->values[array.elements + i];
	}
	return (JSONValue){0};
}

// returns the `i`th key in `object`.
JSONValue json_object_key(const JSON *json, JSONObject object, u64 i) {
	if (i < object.len)
		return json->values[object.items + i];
	return (JSONValue){0};
}

// returns the `i`th value in `object`.
JSONValue json_object_value(const JSON *json, JSONObject object, u64 i) {
	if (i < object.len)
		return json->values[object.items + object.len + i];
	return (JSONValue){0};
}

// returns NaN if `x` is not a number (ha ha).
double json_force_number(JSONValue x) {
	if (x.type == JSON_NUMBER) {
		return x.val.number;
	} else {
		return NAN;
	}
}

double json_object_get_number(const JSON *json, JSONObject object, const char *name) {
	return json_force_number(json_object_get(json, object, name));
}

double json_array_get_number(const JSON *json, JSONArray array, size_t i) {
	return json_force_number(json_array_get(json, array, i));
}

bool json_force_bool(JSONValue x, bool default_value) {
	if (x.type == JSON_TRUE) return true;
	if (x.type == JSON_FALSE) return false;
	return default_value;
}

bool json_object_get_bool(const JSON *json, JSONObject object, const char *name, bool default_value) {
	return json_force_bool(json_object_get(json, object, name), default_value);
}

bool json_array_get_bool(const JSON *json, JSONArray array, size_t i, bool default_value) {
	return json_force_bool(json_array_get(json, array, i), default_value);
}

// returns (JSONString){0} (which is interpreted as an empty string) if `x` is not a string
JSONString json_force_string(JSONValue x) {
	if (x.type == JSON_STRING) {
		return x.val.string;
	} else {
		return (JSONString){0};
	}
}

JSONString json_object_get_string(const JSON *json, JSONObject object, const char *name) {
	return json_force_string(json_object_get(json, object, name));
}

JSONString json_array_get_string(const JSON *json, JSONArray array, size_t i) {
	return json_force_string(json_array_get(json, array, i));
}

// returns (JSONObject){0} (which is interpreted as an empty object) if `x` is not an object
JSONObject json_force_object(JSONValue x) {
	if (x.type == JSON_OBJECT) {
		return x.val.object;
	} else {
		return (JSONObject){0};
	}
}

JSONObject json_object_get_object(const JSON *json, JSONObject object, const char *name) {
	return json_force_object(json_object_get(json, object, name));
}

JSONObject json_array_get_object(const JSON *json, JSONArray array, size_t i) {
	return json_force_object(json_array_get(json, array, i));
}

// returns (JSONArray){0} (which is interpreted as an empty array) if `x` is not an array
JSONArray json_force_array(JSONValue x) {
	if (x.type == JSON_ARRAY) {
		return x.val.array;
	} else {
		return (JSONArray){0};
	}
}

JSONArray json_object_get_array(const JSON *json, JSONObject object, const char *name) {
	return json_force_array(json_object_get(json, object, name));
}

JSONArray json_array_get_array(const JSON *json, JSONArray array, size_t i) {
	return json_force_array(json_array_get(json, array, i));
}

JSONValue json_root(const JSON *json) {
	return json->values[0];
}

// e.g. if json is  { "a" : { "b": 3 }}, then json_get(json, "a.b") = 3.
// returns undefined if there is no such property
JSONValue json_get(const JSON *json, const char *path) {
	char segment[128];
	const char *p = path;
	if (!json->values) {
		return (JSONValue){0};
	}
	JSONValue curr_value = json->values[0];
	while (*p) {
		size_t segment_len = strcspn(p, ".");
		strn_cpy(segment, sizeof segment, p, segment_len);
		if (curr_value.type != JSON_OBJECT) {
			return (JSONValue){0};
		}
		curr_value = json_object_get(json, curr_value.val.object, segment);
		p += segment_len;
		if (*p == '.') ++p;
	}
	return curr_value;
}

// equivalent to json_get(json, path).type != JSON_UNDEFINED, but more readable
bool json_has(const JSON *json, const char *path) {
	JSONValue value = json_get(json, path);
	return value.type != JSON_UNDEFINED;
}

// turn a json string into a null terminated string.
// this won't be nice if the json string includes \u0000 but that's rare.
// if buf_sz > string->len, the string will fit.
void json_string_get(const JSON *json, JSONString string, char *buf, size_t buf_sz) {
	const char *text = json->text;
	if (buf_sz == 0) {
		assert(0);
		return;
	}
	char *buf_end = buf + buf_sz - 1;
	for (u32 i = string.pos, end = string.pos + string.len; i < end && buf < buf_end; ++i) {
		if (text[i] != '\\') {
			*buf++ = text[i];
		} else {
			++i;
			if (i >= end) break;
			// escape sequence
			switch (text[i]) {
			case 'n': *buf++ = '\n'; break;
			case 'r': *buf++ = '\r'; break;
			case 'b': *buf++ = '\b'; break;
			case 't': *buf++ = '\t'; break;
			case 'f': *buf++ = '\f'; break;
			case '\\': *buf++ = '\\'; break;
			case '/': *buf++ = '/'; break;
			case '"': *buf++ = '"'; break;
			case 'u': {
				if ((buf_end - buf) < 4 || i + 5 > end)
					goto brk;
				++i;
				
				char hex[5] = {0};
				hex[0] = text[i++];
				hex[1] = text[i++];
				hex[2] = text[i++];
				hex[3] = text[i++];
				unsigned code_point=0;
				sscanf(hex, "%04x", &code_point);
				// technically this won't deal with people writing out UTF-16 surrogate halves
				// using \u. i dont care.
				size_t n = unicode_utf32_to_utf8(buf, code_point);
				if (n <= 4) buf += n;
				} break;
			}
		}
	}
	brk:
	*buf = '\0';
}

char *json_string_get_alloc(const JSON *json, JSONString string) {
	u32 n = string.len + 1;
	if (n == 0) --n; // extreme edge case
	char *buf = calloc(1, n);
	json_string_get(json, string, buf, n);
	return buf;
}


#if __unix__
static void json_test_time_large(const char *filename) {
	struct timespec start={0},end={0};
	FILE *fp = fopen(filename,"rb");
	if (!fp) {
		perror(filename);
		return;
	}
	
	fseek(fp,0,SEEK_END);
	size_t sz = (size_t)ftell(fp);
	char *buf = calloc(1,sz+1);
	rewind(fp);
	fread(buf, 1, sz, fp);
	fclose(fp);
	for (int trial = 0; trial < 5; ++trial) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		JSON json={0};
		bool success = json_parse(&json, buf);
		if (!success) {
			printf("FAIL: %s\n",json.error);
			return;
		}
		
		json_free(&json);
		clock_gettime(CLOCK_MONOTONIC, &end);
		
		
		
		printf("time: %.1fms\n",
			((double)end.tv_sec*1e3+(double)end.tv_nsec*1e-6)
			-((double)start.tv_sec*1e3+(double)start.tv_nsec*1e-6));
	}
		
}
static void json_test_time_small(void) {
	struct timespec start={0},end={0};
	int trials = 50000000;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int trial = 0; trial < trials; ++trial) {
		JSON json={0};
		bool success = json_parse(&json, "{\"hello\":\"there\"}");
		if (!success) {
			printf("FAIL: %s\n",json.error);
			return;
		}
		
		json_free(&json);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("time per trial: %.1fns\n",
		(((double)end.tv_sec*1e9+(double)end.tv_nsec)
		-((double)start.tv_sec*1e9+(double)start.tv_nsec))
		/ trials);
		
}
#endif

void json_debug_print(const JSON *json) {
	printf("%u values (capacity %u, text length %zu)\n",
		arr_len(json->values), arr_cap(json->values), strlen(json->text));
	json_debug_print_value(json, json->values[0]);
	printf("\n");
}

// e.g. converts "Hello\nworld" to "Hello\\nworld"
// if out_sz is at least 2 * strlen(str) + 1, the string will fit.
// returns the number of bytes actually written, not including the null terminator.
size_t json_escape_to(char *out, size_t out_sz, const char *in) {
	char *start = out;
	char *end = out + out_sz;
	assert(out_sz);
	
	--end; // leave room for null terminator
	
	for (; *in; ++in) {
		if (out + 1 > end) {
			break;
		}
		char esc = '\0';
		switch (*in) {
		case '\0': goto brk;
		case '\n':
			esc = 'n';
			goto escape;
		case '\\':
			esc = '\\';
			goto escape;
		case '"':
			esc = '"';
			goto escape;
		case '\t':
			esc = 't';
			goto escape;
		case '\r':
			esc = 'r';
			goto escape;
		case '\f':
			esc = 'f';
			goto escape;
		case '\b':
			esc = 'b';
			goto escape;
		escape:
			if (out + 2 > end)
				goto brk;
			*out++ = '\\';
			*out++ = esc;
			break;
		default:
			*out = *in;
			++out;
			break;
		}
	}
	brk:
	*out = '\0';
	return (size_t)(out - start);
}

// e.g. converts "Hello\nworld" to "Hello\\nworld"
// the resulting string should be free'd.
char *json_escape(const char *str) {
	size_t out_sz = 2 * strlen(str) + 1;
	char *out = calloc(1, out_sz);
	json_escape_to(out, out_sz, str);
	return out;
}

#undef SKIP_WHITESPACE
