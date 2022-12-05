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
	u32 properties;
	u32 values;
} JSONObject;

typedef struct {
	u32 len;
	u32 values;
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
	const char *text;
	// root = values[0]
	JSONValue *values;
} JSON;

static bool json_parse_value(JSON *json, u32 *p_index, JSONValue *val) {
	const char *text = json->text;
	u32 index = *p_index;
	while (isspace(text[index])) ++index;
	switch (text[index]) {
	case '{':
		val->type = JSON_OBJECT;
		json_parse_object(json, &index, &val->val.object);
		break;
	case '[':
		val->type = JSON_ARRAY;
		json_parse_array(json, &index, &val->val.array);
		break;
	case '"':
		val->type = JSON_STRING;
		json_parse_string(json, &index, &val->val.string);
		break;
	case ANY_DIGIT:
		json_parse_number(json, &index, &val->val.number);
		break;
	case 'f':
		if (!str_is_prefix(&text[index], "false", 5) != 0) {
			
		}
	}
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
		// more text after end of object
		// @TODO error message
		return false;
	}
	json->values[0] = val;
}

static void send_message(LSP *lsp, const char *content) {
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
	send_message(lsp, init_request);
	char init_response[4096] = {0};
	process_read(&lsp->process, init_response, sizeof init_response);
	printf("%s\n",init_response);
}
