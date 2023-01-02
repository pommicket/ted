#ifndef SETTINGS_H_
#define SETTINGS_H_

// NOTE: the actual Settings struct is stored in ted.h.
// This file is only included by config.c.

// all the "control" pointers here are relative to a NULL Settings object.
typedef struct {
	char const *name;
	const bool *control;
	bool per_language; // allow per-language control
} SettingBool;
typedef struct {
	char const *name;
	const u8 *control;
	u8 min, max;
	bool per_language;
} SettingU8;
typedef struct {
	char const *name;
	const float *control;
	float min, max;
	bool per_language;
} SettingFloat;
typedef struct {
	char const *name;
	const u16 *control;
	u16 min, max;
	bool per_language;
} SettingU16;
typedef struct {
	char const *name;
	const u32 *control;
	u32 min, max;
	bool per_language;
} SettingU32;
typedef struct {
	char const *name;
	const char *control;
	size_t buf_size;
	bool per_language;
} SettingString;

typedef enum {
	SETTING_BOOL = 1,
	SETTING_U8,
	SETTING_U16,
	SETTING_U32,
	SETTING_FLOAT,
	SETTING_STRING
} SettingType;
typedef struct {
	SettingType type;
	const char *name;
	bool per_language;
	union {
		SettingU8 _u8;
		SettingBool _bool;
		SettingU16 _u16;
		SettingU32 _u32;
		SettingFloat _float;
		SettingString _string;
	} u;
} OptionAny;

// core options
static Settings const settings_zero = {0};
static SettingBool const settings_bool[] = {
	{"auto-indent", &settings_zero.auto_indent, true},
	{"auto-add-newline", &settings_zero.auto_add_newline, true},
	{"auto-reload", &settings_zero.auto_reload, true},
	{"auto-reload-config", &settings_zero.auto_reload_config, false},
	{"syntax-highlighting", &settings_zero.syntax_highlighting, true},
	{"line-numbers", &settings_zero.line_numbers, true},
	{"restore-session", &settings_zero.restore_session, false},
	{"regenerate-tags-if-not-found", &settings_zero.regenerate_tags_if_not_found, true},
	{"indent-with-spaces", &settings_zero.indent_with_spaces, true},
	{"trigger-characters", &settings_zero.trigger_characters, true},
	{"identifier-trigger-characters", &settings_zero.identifier_trigger_characters, true},
	{"signature-help-enabled", &settings_zero.signature_help_enabled, true},
	{"lsp-enabled", &settings_zero.lsp_enabled, true},
	{"hover-enabled", &settings_zero.hover_enabled, true},
	{"vsync", &settings_zero.vsync, false},
	{"highlight-enabled", &settings_zero.highlight_enabled, true},
	{"highlight-auto", &settings_zero.highlight_auto, true},
};
static SettingU8 const settings_u8[] = {
	{"tab-width", &settings_zero.tab_width, 1, 100, true},
	{"cursor-width", &settings_zero.cursor_width, 1, 100, true},
	{"undo-save-time", &settings_zero.undo_save_time, 1, 200, true},
	{"border-thickness", &settings_zero.border_thickness, 1, 30, false},
	{"padding", &settings_zero.padding, 0, 100, false},
	{"scrolloff", &settings_zero.scrolloff, 1, 100, true},
	{"tags-max-depth", &settings_zero.tags_max_depth, 1, 100, false},
};
static SettingU16 const settings_u16[] = {
	{"text-size", &settings_zero.text_size, TEXT_SIZE_MIN, TEXT_SIZE_MAX, false},
	{"max-menu-width", &settings_zero.max_menu_width, 10, U16_MAX, false},
	{"error-display-time", &settings_zero.error_display_time, 0, U16_MAX, false},
	{"framerate-cap", &settings_zero.framerate_cap, 3, 1000, false},
};
static SettingU32 const settings_u32[] = {
	{"max-file-size", &settings_zero.max_file_size, 100, 2000000000, false},
	{"max-file-size-view-only", &settings_zero.max_file_size_view_only, 100, 2000000000, false},
};
static SettingFloat const settings_float[] = {
	{"cursor-blink-time-on", &settings_zero.cursor_blink_time_on, 0, 1000, true},
	{"cursor-blink-time-off", &settings_zero.cursor_blink_time_off, 0, 1000, true},
	{"hover-time", &settings_zero.hover_time, 0, INFINITY, true},
};
static SettingString const settings_string[] = {
	{"build-default-command", settings_zero.build_default_command, sizeof settings_zero.build_default_command, true},
	{"bg-shader", settings_zero.bg_shader_text, sizeof settings_zero.bg_shader_text, true},
	{"bg-texture", settings_zero.bg_shader_image, sizeof settings_zero.bg_shader_image, true},
	{"root-identifiers", settings_zero.root_identifiers, sizeof settings_zero.root_identifiers, true},
	{"lsp", settings_zero.lsp, sizeof settings_zero.lsp, true},
};

#endif // SETTINGS_H_
