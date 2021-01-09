// @TODO:
// - Windows installation
#include "base.h"
no_warn_start
#if _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
no_warn_end
#include <GL/gl.h>
#include <locale.h>
#include <wctype.h>
#if _WIN32
#include <shellapi.h>
#endif

#define TED_PATH_MAX 256

#include "text.h"
#include "util.c"
#define MATH_GL
#include "math.c"
#if _WIN32
#include "filesystem-win.c"
#elif __unix__
#include "filesystem-posix.c"
#else
#error "Unrecognized operating system."
#endif

#include "unicode.h"
#include "command.h"
#include "colors.h"
#include "ted.h"
#include "time.c"
#include "string32.c"
#include "arr.c"
#include "buffer.c"
#include "ted-base.c"
#include "command.c"
#include "config.c"
#include "menu.c"

static void die(char const *fmt, ...) {
	char buf[256] = {0};
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof buf - 1, fmt, args);
	va_end(args);

	// show a message box, and if that fails, print it
	if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", buf, NULL) < 0) {
		debug_println("%s\n", buf);
	}

	exit(EXIT_FAILURE);
}

#if _WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow) {
	int argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char** argv = malloc(argc * sizeof *argv);
	if (!argv) {
		die("Out of memory.");
	}
    for (int i = 0; i < argc; i++) {
        LPWSTR wide_arg = wide_args[i];
        int len = wcslen(wide_arg);
        argv[i] = malloc(len + 1);
		if (!argv[i]) die("Out of memory.");
        for (int j = 0; j <= len; j++)
            argv[i][j] = (char)wide_arg[j];
    }
    LocalFree(wide_args);
#else
int main(int argc, char **argv) {
#endif
	setlocale(LC_ALL, ""); // allow unicode

	{ // check if this is the installed version of ted (as opposed to just running it from the directory with the source)
		char executable_path[TED_PATH_MAX] = {0};
	#if _WIN32
		// @TODO(windows): GetModuleFileNameW
	#else
		ssize_t len = readlink("/proc/self/exe", executable_path, sizeof executable_path - 1);
		if (len == -1) {
			// some posix systems don't have /proc/self/exe. oh well.
		} else {
			executable_path[len] = '\0';
			ted_search_cwd = !str_is_prefix(executable_path, "/usr");
		}
	#endif
	}

	Ted *ted = calloc(1, sizeof *ted);
	if (!ted) {
		die("Not enough memory available to run ted.");
	}

	Settings *settings = &ted->settings;

	{
		// read global configuration file first to establish defaults
		char global_config_filename[TED_PATH_MAX];
		strbuf_printf(global_config_filename, "%s/ted.cfg", ted_global_data_dir);
		if (fs_file_exists(global_config_filename))
			config_read(ted, global_config_filename);
	}
	{
		// read local configuration file
		char config_filename[TED_PATH_MAX];
		if (ted_get_file("ted.cfg", config_filename, sizeof config_filename))
			config_read(ted, config_filename);
		else
			ted_seterr(ted, "Couldn't find config file (ted.cfg), not even the backup one that should have come with ted.");
	}
	if (ted_haserr(ted)) {
		die("Error reading config: %s", ted_geterr(ted));
	}

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); // if this program is sent a SIGTERM/SIGINT, don't turn it into a quit event
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		die("%s", SDL_GetError());

	SDL_Window *window = SDL_CreateWindow("ted", SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if (!window)
		die("%s", SDL_GetError());
		
	{ // set icon
		char icon_filename[TED_PATH_MAX];
		if (ted_get_file("assets/icon.bmp", icon_filename, sizeof icon_filename)) {
			SDL_Surface *icon = SDL_LoadBMP(icon_filename);
			SDL_SetWindowIcon(window, icon);
			SDL_FreeSurface(icon);
		} // if we can't find the icon file, it's no big deal
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GLContext *glctx = SDL_GL_CreateContext(window);
	if (!glctx) {
		die("%s", SDL_GetError());
	}

	SDL_GL_SetSwapInterval(1); // vsync

	ted_load_font(ted);
	if (ted_haserr(ted))
		die("Error loadng font: %s", ted_geterr(ted));
	
	
	TextBuffer text_buffer;
	{
		TextBuffer *buffer = &text_buffer;
		buffer_create(buffer, ted);
		ted->active_buffer = buffer;

		char const *starting_filename = "Untitled";
		
		switch (argc) {
		case 0: case 1: break;
		case 2:
			starting_filename = argv[1];
			break;
		default:	
			die("Usage: %s [filename]", argv[0]);
			break;
		}

		if (fs_file_exists(starting_filename)) {
			buffer_load_file(buffer, starting_filename);
			if (buffer_haserr(buffer))
				die("Error loading file: %s", buffer_geterr(buffer));
		} else {
			buffer_new_file(buffer, starting_filename);
			if (buffer_haserr(buffer))
				die("Error creating file: %s", buffer_geterr(buffer));
		}
	}


	u32 *colors = settings->colors; (void)colors;

	Uint32 time_at_last_frame = SDL_GetTicks();

	bool quit = false;
	bool ctrl_down = false;
	bool shift_down = false;
	bool alt_down = false;
	while (!quit) {
	#if DEBUG
		//printf("\033[H\033[2J");
	#endif

		{
			int window_width_int = 0, window_height_int = 0;
			SDL_GetWindowSize(window, &window_width_int, &window_height_int);
			ted->window_width = (float)window_width_int;
			ted->window_height = (float)window_height_int;
		}
		float window_width = ted->window_width, window_height = ted->window_height;

		SDL_Event event;
		Uint8 const *keyboard_state = SDL_GetKeyboardState(NULL);

		while (SDL_PollEvent(&event)) {
			TextBuffer *buffer = ted->active_buffer;
			u32 key_modifier = (u32)ctrl_down << KEY_MODIFIER_CTRL_BIT
				| (u32)shift_down << KEY_MODIFIER_SHIFT_BIT
				| (u32)alt_down << KEY_MODIFIER_ALT_BIT;
			// @TODO: make a function to handle text buffer events
			switch (event.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEWHEEL: {
				// scroll with mouse wheel
				Sint32 dx = event.wheel.x, dy = -event.wheel.y;
				double scroll_speed = 2.5;
				if (ted->active_buffer)
					buffer_scroll(ted->active_buffer, dx * scroll_speed, dy * scroll_speed);
			} break;
			case SDL_MOUSEBUTTONDOWN:
				switch (event.button.button) {
				case SDL_BUTTON_LEFT: {
					if (buffer) {
						BufferPos pos = {0};
						if (buffer_pixels_to_pos(buffer, V2((float)event.button.x, (float)event.button.y), &pos)) {
							if (key_modifier == KEY_MODIFIER_SHIFT) {
								buffer_select_to_pos(buffer, pos);
							} else if (key_modifier == 0) {
								buffer_cursor_move_to_pos(buffer, pos);
								switch ((event.button.clicks - 1) % 3) {
								case 0: break; // single-click
								case 1: // double-click: select word
									buffer_select_word(buffer);
									break;
								case 2: // triple-click: select line
									buffer_select_line(buffer);
									break;
								}
							}
						}
					}
				} break;
				}
				break;
			case SDL_MOUSEMOTION:
				if (event.motion.state == SDL_BUTTON_LMASK) {
					if (buffer) {
						BufferPos pos = {0};
						if (buffer_pixels_to_pos(buffer, V2((float)event.button.x, (float)event.button.y), &pos)) {
							buffer_select_to_pos(buffer, pos);
						}
					}
				}
				break;
			case SDL_KEYDOWN: {
				SDL_Scancode scancode = event.key.keysym.scancode;
				switch (scancode) {
				case SDL_SCANCODE_LCTRL:
				case SDL_SCANCODE_RCTRL:
					ctrl_down = true; break;
				case SDL_SCANCODE_LSHIFT:
				case SDL_SCANCODE_RSHIFT:
					shift_down = true; break;
				case SDL_SCANCODE_LALT:
				case SDL_SCANCODE_RALT:
					alt_down = true; break;
				default: break;
				}
				SDL_Keymod modifier = event.key.keysym.mod;
				u32 key_combo = (u32)scancode << 3 |
					(u32)((modifier & (KMOD_LCTRL|KMOD_RCTRL)) != 0) << KEY_MODIFIER_CTRL_BIT |
					(u32)((modifier & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0) << KEY_MODIFIER_SHIFT_BIT |
					(u32)((modifier & (KMOD_LALT|KMOD_RALT)) != 0) << KEY_MODIFIER_ALT_BIT;
				if (key_combo < KEY_COMBO_COUNT) {
					KeyAction *action = &ted->key_actions[key_combo];
					if (action->command) {
						command_execute(ted, action->command, action->argument);
					} else if (buffer) switch (event.key.keysym.sym) {
						case SDLK_RETURN:
							buffer_insert_char_at_cursor(buffer, U'\n');
							break;
						case SDLK_TAB:
							buffer_insert_char_at_cursor(buffer, U'\t');
							break;
					}
				}
			} break;
			case SDL_KEYUP: {
				SDL_Scancode scancode = event.key.keysym.scancode;
				switch (scancode) {
				case SDL_SCANCODE_LCTRL:
				case SDL_SCANCODE_RCTRL:
					ctrl_down = false; break;
				case SDL_SCANCODE_LSHIFT:
				case SDL_SCANCODE_RSHIFT:
					shift_down = false; break;
				case SDL_SCANCODE_LALT:
				case SDL_SCANCODE_RALT:
					alt_down = false; break;
				default: break;
				}
			} break;
			case SDL_TEXTINPUT: {
				char *text = event.text.text;
				if (buffer
					&& key_modifier == 0) // unfortunately, some key combinations like ctrl+minus still register as a "-" text input event
					buffer_insert_utf8_at_cursor(buffer, text);
			} break;
			}

			if (ted_haserr(ted)) {
				die("%s", ted_geterr(ted));
			}
		}

		u32 key_modifier = (u32)ctrl_down << KEY_MODIFIER_CTRL_BIT
			| (u32)shift_down << KEY_MODIFIER_SHIFT_BIT
			| (u32)alt_down << KEY_MODIFIER_ALT_BIT;

		double frame_dt;
		{
			Uint32 time_this_frame = SDL_GetTicks();
			frame_dt = 0.001 * (time_this_frame - time_at_last_frame);
			time_at_last_frame = time_this_frame;
		}

		TextBuffer *active_buffer = ted->active_buffer;
		if (active_buffer && key_modifier == KEY_MODIFIER_ALT) {
			// alt + arrow keys to scroll
			double scroll_speed = 20.0;
			double scroll_amount_x = scroll_speed * frame_dt * 1.5; // characters are taller than they are wide
			double scroll_amount_y = scroll_speed * frame_dt;
			if (keyboard_state[SDL_SCANCODE_UP])
				buffer_scroll(active_buffer, 0, -scroll_amount_y);
			if (keyboard_state[SDL_SCANCODE_DOWN])
				buffer_scroll(active_buffer, 0, +scroll_amount_y);
			if (keyboard_state[SDL_SCANCODE_LEFT])
				buffer_scroll(active_buffer, -scroll_amount_x, 0);
			if (keyboard_state[SDL_SCANCODE_RIGHT])
				buffer_scroll(active_buffer, +scroll_amount_x, 0);
		}
			


		// set up GL
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, (GLsizei)window_width, (GLsizei)window_height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		// pixel coordinates; down is positive y
		glOrtho(0, window_width, window_height, 0, -1, +1);
		{ // clear (background)
			float bg_color[4];
			rgba_u32_to_floats(settings->colors[COLOR_BG], bg_color);
			glClearColor(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
		}
		glClear(GL_COLOR_BUFFER_BIT);

		{
			float x1 = 50, y1 = 50, x2 = window_width-50, y2 = window_height-50;
			buffer_render(&text_buffer, x1, y1, x2, y2);
			if (text_has_err()) {
				die("Text error: %s\n", text_get_err());
				break;
			}
		}

		Menu menu = ted->menu;
		if (menu) {
			menu_render(ted, menu);
		}

	#if DEBUG
		buffer_check_valid(&text_buffer);
	#endif

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);
	SDL_Quit();
	buffer_free(&text_buffer);
	text_font_free(ted->font);
	free(ted);
#if _WIN32
	for (int i = 0; i < argc; ++i)
		free(argv[i]);
	free(argv);
#endif

	return 0;
}
