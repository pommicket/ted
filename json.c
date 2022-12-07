// JSON parser for LSP
// provides FAST(ish) parsing but SLOW lookup
// this is especially fast for small objects

typedef struct {
	u32 pos;
	u32 len;
} JSONString;

typedef struct JSONValue JSONValue;

typedef struct {
	u32 len;
	// this is an index into the values array
	//   values[items..items+len] store the names
	//   values[items+len..items+2*len] store the values
	u32 items;
} JSONObject;

typedef struct {
	u32 len;
	// this is an index into the values array
	//    values[elements..elements+len] are the elements
	u32 elements;
} JSONArray;

enum {
	// note: json doesn't actually include undefined.
	// this is only for returning things from json_get etc.
	JSON_UNDEFINED,
	JSON_NULL,
	JSON_FALSE,
	JSON_TRUE,
	JSON_NUMBER,
	JSON_STRING,
	JSON_OBJECT,
	JSON_ARRAY
};

struct JSONValue {
	u8 type;
	union {
		double number;
		JSONString string;
		JSONArray array;
		JSONObject object;
	} val;
};


typedef struct {
	char error[64];
	bool is_text_copied; // if this is true, then json_free will call free on text
	const char *text;
	// root = values[0]
	JSONValue *values;
} JSON;

#define SKIP_WHITESPACE while (json_is_space(text[index])) ++index;


static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val);

// defining this instead of using isspace seems to be faster
// probably because isspace depends on the locale.
static inline bool json_is_space(char c) {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static void json_debug_print_value(const JSON *json, const JSONValue *value) {
	switch (value->type) {
	case JSON_UNDEFINED: printf("undefined"); break;
	case JSON_NULL: printf("null"); break;
	case JSON_FALSE: printf("false"); break;
	case JSON_TRUE: printf("true"); break;
	case JSON_NUMBER: printf("%g", value->val.number); break;
	case JSON_STRING: {
		const JSONString *string = &value->val.string;
		printf("\"%.*s\"",
			(int)string->len,
			json->text + string->pos);
		} break;
	case JSON_ARRAY: {
		const JSONArray *array = &value->val.array;
		printf("[");
		for (u32 i = 0; i < array->len; ++i) {
			json_debug_print_value(json, &json->values[array->elements + i]);
			printf(", ");
		}
		printf("]");
	} break;
	case JSON_OBJECT: {
		const JSONObject *obj = &value->val.object;
		printf("{");
		for (u32 i = 0; i < obj->len; ++i) {
			json_debug_print_value(json, &json->values[obj->items + i]);
			printf(": ");
			json_debug_print_value(json, &json->values[obj->items + obj->len + i]);
			printf(", ");
		}
		printf("}");
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

// NOTE: text must live as long as json!!!
static bool json_parse(JSON *json, const char *text) {
	memset(json, 0, sizeof *json);
	json->text = text;
	arr_reserve(json->values, strlen(text) / 8);
	arr_addp(json->values); // add root
	JSONValue val = {0};
	u32 index = 0;
	if (!json_parse_value(json, &index, &val)) {
		arr_free(json->values);
		return false;
	}
	SKIP_WHITESPACE;
	if (text[index]) {
		arr_free(json->values);
		strbuf_printf(json->error, "extra text after end of root object");
		return false;
	}
	json->values[0] = val;
	return true;
}

// like json_parse, but a copy of text is made, so you can free/overwrite it immediately.
static bool json_parse_copy(JSON *json, const char *text) {
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

static void json_free(JSON *json) {
	arr_free(json->values);
	memset(json, 0, sizeof *json);
	if (json->is_text_copied) {
		free((void*)json->text);
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

// returns undefined if the property `name` does not exist.
static JSONValue json_object_get(const JSON *json, const JSONObject *object, const char *name) {
	const JSONValue *items = &json->values[object->items];
	for (u32 i = 0; i < object->len; ++i) {
		const JSONValue *this_name = items++;
		if (this_name->type == JSON_STRING && json_streq(json, &this_name->val.string, name)) {
			return json->values[object->items + object->len + i];
		}
	}
	return (JSONValue){0};
}

// e.g. if json is  { "a" : { "b": 3 }}, then json_get(json, "a.b") = 3.
// returns undefined if there is no such property
static JSONValue json_get(const JSON *json, const char *path) {
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
		curr_value = json_object_get(json, &curr_value.val.object, segment);
		p += segment_len;
		if (*p == '.') ++p;
	}
	return curr_value;
}

// turn a json string into a null terminated string.
// this won't be nice if the json string includes \u0000 but that's rare.
// if buf_sz > string->len, the string will fit.
static void json_string_get(const JSON *json, const JSONString *string, char *buf, size_t buf_sz) {
	const char *text = json->text;
	if (buf_sz == 0) {
		assert(0);
		return;
	}
	char *buf_end = buf + buf_sz - 1;
	for (u32 i = string->pos, end = string->pos + string->len; i < end && buf < buf_end; ++i) {
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

static void json_debug_print(const JSON *json) {
	printf("%u values (capacity %u, text length %zu)\n",
		arr_len(json->values), arr_cap(json->values), strlen(json->text));
	json_debug_print_value(json, &json->values[0]);
}

// e.g. converts "Hello\nworld" to "Hello\\nworld"
// the return value is the # of bytes in the escaped string.
static size_t json_escape_to(char *out, size_t out_sz, const char *in) {
	(void)out;(void)out_sz;(void)in;
	// @TODO
	abort();
}

// e.g. converts "Hello\nworld" to "Hello\\nworld"
// the resulting string should be free'd.
static char *json_escape(const char *str) {
	size_t out_sz = 2 * strlen(str) + 1;
	char *out = calloc(1, out_sz);
	json_escape_to(out, out_sz, str);
	return out;
}

#undef SKIP_WHITESPACE
