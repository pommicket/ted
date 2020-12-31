typedef struct {
	u32 line_number; // config line number where this was set
	Command command; // this will be 0 (COMMAND_UNKNOWN) if there's no action for the key
	i64 argument;
} KeyAction;

typedef struct {
	u32 colors[COLOR_COUNT];
} Settings;

#define SCANCODE_COUNT 0x120 // SDL scancodes should be less than this value.
// a "key combo" is some subset of {control, shift, alt} + some key.
#define KEY_COMBO_COUNT (SCANCODE_COUNT << 3)
#define KEY_MODIFIER_CTRL_BIT 0
#define KEY_MODIFIER_SHIFT_BIT 1
#define KEY_MODIFIER_ALT_BIT 2
#define KEY_MODIFIER_CTRL (1<<KEY_MODIFIER_CTRL_BIT)
#define KEY_MODIFIER_SHIFT (1<<KEY_MODIFIER_SHIFT_BIT)
#define KEY_MODIFIER_ALT (1<<KEY_MODIFIER_ALT_BIT)
// ctrl+alt+c is encoded as SDL_SCANCODE_C << 3 | KEY_MODIFIER_CTRL | KEY_MODIFIER_ALT

typedef struct {
	TextBuffer *active_buffer;
	KeyAction key_actions[KEY_COMBO_COUNT];
	char error[256];
} Ted;

// this is a macro so we get -Wformat warnings
#define ted_seterr(buffer, ...) \
	snprintf(ted->error, sizeof ted->error - 1, __VA_ARGS__)

bool ted_haserr(Ted *ted) {
	return ted->error[0] != '\0';
}

char const *ted_geterr(Ted *ted) {
	return ted->error;
}

static void ted_out_of_mem(Ted *ted) {
	ted_seterr(ted, "Out of memory.");
}

static void *ted_malloc(Ted *ted, size_t size) {
	void *ret = malloc(size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

static void *ted_calloc(Ted *ted, size_t n, size_t size) {
	void *ret = calloc(n, size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

static void *ted_realloc(Ted *ted, void *p, size_t new_size) {
	void *ret = realloc(p, new_size);
	if (!ret) ted_out_of_mem(ted);
	return ret;
}

