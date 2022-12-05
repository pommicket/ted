typedef struct {
	Process process;
} LSP;

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
	const char *text;
	// root = values[0]
	JSONValue *values;
} JSON;

static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val);

// count number of comma-separated values until
// closing ] or }
static u32 json_count(JSON *json, u32 index) {
	int bracket_depth = 0;
	int brace_depth = 0;
	u32 count = 1;
	const char *text = json->text;
	while (isspace(text[index])) ++index;
	// special case: empty object/array
	if (text[index] == '}' || text[index] == ']')
		return 0;
	
	int mark[5000] = {0};
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
			mark[index] = true;
			break;
		case '}':
			--brace_depth;
			mark[index] = true;
			if (brace_depth < 0){
			
			for (int i = 0; text[i]; ++i ){
				switch (mark[i]){
				case 1: printf("\x1b[91m"); break;
				case 2: printf("\x1b[92m"); break;
				}
				printf("%c",text[i]);
				if (mark[i]) printf("\x1b[0m");
			}
			printf("\n");
			
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
				mark[index] = 2;
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
	printf("%u\n",count);
	exit(0);
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
		JSONValue name = {0}, value = {0};
		if (!json_parse_value(json, &index, &name))
			return false;
		while (isspace(text[index])) ++index;
		if (text[index] != ':') {
			strbuf_printf(json->error, "stuff after name in object");
			return false;
		}
		while (isspace(text[index])) ++index;
		if (!json_parse_value(json, &index, &value))
			return false;
		while (isspace(text[index])) ++index;
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
	(void)json;(void)p_index;(void)array;
	abort();
}

static bool json_parse_string(JSON *json, u32 *p_index, JSONString *string) {
	(void)json;(void)p_index;(void)string;
	abort();
}

static bool json_parse_number(JSON *json, u32 *p_index, double *number) {
	(void)json;(void)p_index;(void)number;
	abort();
}

static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val) {
	const char *text = json->text;
	u32 index = *p_index;
	while (isspace(text[index])) ++index;
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
	}
	*p_index = index;
	return true;
}

// NOTE: text must live as long as json!!!
static bool json_parse(JSON *json, const char *text) {
	memset(json, 0, sizeof *json);
	json->text = text;
	// @TODO: is this a good capacity?
	arr_reserve(json->values, strlen(text) / 8);
	arr_addp(json->values); // add root
	JSONValue val = {0};
	u32 index = 0;
	if (!json_parse_value(json, &index, &val))
		return false;
	
	while (isspace(text[index])) ++index;
	if (text[index]) {
		strbuf_printf(json->error, "extra text after end of root object");
		return false;
	}
	json->values[0] = val;
	return true;
}


static void json_debug_print_value(const JSON *json, u32 idx) {
	JSONValue *value = &json->values[idx];
	switch (value->type) {
	case JSON_NULL: printf("null"); break;
	case JSON_FALSE: printf("false"); break;
	case JSON_TRUE: printf("true"); break;
	case JSON_NUMBER: printf("%f", value->val.number); break;
	case JSON_STRING: {
		JSONString *string = &value->val.string;
		printf("\"%.*s\"",
			(int)string->len,
			json->text + string->pos);
		} break;
	case JSON_ARRAY: {
		JSONArray *array = &value->val.array;
		printf("[");
		for (u32 i = 0; i < array->len; ++i) {
			json_debug_print_value(json, array->elements + i);
			printf(",");
		}
		printf("]");
	} break;
	case JSON_OBJECT: {
		JSONObject *obj = &value->val.object;
		printf("{");
		for (u32 i = 0; i < obj->len; ++i) {
			json_debug_print_value(json, obj->items + i);
			printf(": ");
			json_debug_print_value(json, obj->items + obj->len + i);
			printf(",");
		}
		printf("}");
		} break;
	}
}

static void json_debug_print(const JSON *json) {
	json_debug_print_value(json, 0);
}

static void send_request(LSP *lsp, const char *content) {
	char header[128];
	size_t content_size = strlen(content);
	strbuf_printf(header, "Content-Length: %zu\r\n\r\n", content_size); 
	process_write(&lsp->process, header, strlen(header));
	process_write(&lsp->process, content, content_size);
}

void lsp_create(LSP *lsp, const char *analyzer_command) {
	ProcessSettings settings = {
		.stdin_blocking = true,
		.stdout_blocking = true
	};
	process_run_ex(&lsp->process, analyzer_command, &settings);
	char init_request[1024];
	strbuf_printf(init_request,
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
			"\"processId\":%d,"
			"\"capabilities\":{}"
		"}}",
		process_get_id());
	send_request(lsp, init_request);
	// @TODO: recv_response
	char response_text[4096] = {0};
	process_read(&lsp->process, response_text, sizeof response_text);
	char *rnrn = strstr(response_text, "\r\n\r\n");
	if (!rnrn) {
		//@TODO delete me
		printf("no analyzer ):\n");
		exit(0);
	}
	JSON  json = {0};
	printf("%s\n",rnrn+4);
	if (!json_parse(&json, rnrn + 4))
		printf("fail : %s\n",json.error);
	json_debug_print(&json);
	
}
