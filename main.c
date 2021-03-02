// @TODO:
// - command selector
// - :open-config
// - test on BSD

// - completion

#include "base.h"
no_warn_start
#if _WIN32
#include <SDL.h>
#else
#if DEBUG || __TINYC__ // speed up compile time on debug, also tcc doesn't have immintrin.h
#define SDL_DISABLE_IMMINTRIN_H
#endif
#include <SDL2/SDL.h>
#endif
no_warn_end
#include <locale.h>
#include <wctype.h>
#if __linux__
#include <execinfo.h>
#endif
#if _WIN32
#include <shellapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "shell32.lib")
#endif
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 32
#include "pcre2.h"

#include "unicode.h"
#include "util.c"
#if _WIN32
#include "filesystem-win.c"
#elif __unix__
#include "filesystem-posix.c"
#else
#error "Unrecognized operating system."
#endif
#include "arr.c"

#include "math.c"
#if _WIN32
#include "process-win.c"
#elif __unix__
#include "process-posix.c"
#else
#error "Unrecognized operating system."
#endif

#include "io.c"

#include "text.h"
#include "command.h"
#include "colors.h"
#include "time.c"
#include "ted.h"
#include "gl.c"
#include "text.c"

#include "string32.c"
#include "syntax.c"
bool tag_goto(Ted *ted, char const *tag);
#include "buffer.c"
#include "ted.c"
#include "ui.c"
#include "find.c"
#include "node.c"
#include "tags.c"
#include "menu.c"
#include "build.c"
#include "command.c"
#include "config.c"
#include "session.c"

#if PROFILE
#define PROFILE_TIME(var) double var = time_get_seconds();
#else
#define PROFILE_TIME(var)
#endif

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

static Rect error_box_rect(Ted *ted) {
	Font *font = ted->font;
	Settings const *settings = &ted->settings;
	float padding = settings->padding;
	float window_width = ted->window_width, window_height = ted->window_height;
	float char_height = text_font_char_height(font);
	return rect_centered(V2(window_width * 0.5f, window_height * 0.9f),
			V2(menu_get_width(ted), 3 * char_height + 2 * padding));
}

#if DEBUG
static void APIENTRY gl_message_callback(GLenum source, GLenum type, unsigned int id, GLenum severity, 
	GLsizei length, const char *message, const void *userParam) {
	(void)source; (void)type; (void)id; (void)length; (void)userParam;
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
	debug_println("Message from OpenGL: %s.", message);
}
#endif

#define CRASH_CRASH_MESSAGE "ted crashed while trying to handle a crash! yikes! ):"
#define CRASH_MESSAGE "ted has crashed ):  Please send %s/log.txt to pommicket""@gmail.com if you want this fixed.", ted->local_data_dir
#define CRASH_STARTUP_MESSAGE "ted crashed when starting up ):"

static Ted *error_signal_handler_ted;
static bool signal_being_handled; // prevent infinite signal recursion
#if __unix__
static void error_signal_handler(int signum, siginfo_t *info, void *context) {
	(void)context;
	if (signal_being_handled)
		die(CRASH_CRASH_MESSAGE);
	signal_being_handled = true;
	Ted *ted = error_signal_handler_ted;
	if (ted) {
		FILE *log = ted->log;
		if (log) {
			fprintf(log, "Signal %d: %s\n", signum, strsignal(signum));
			fprintf(log, "errno = %d\n",  info->si_errno);
			fprintf(log, "code = %d\n", info->si_code);
			fprintf(log, "address = 0x%llx\n", (unsigned long long)info->si_addr);
		#if __linux__
			fprintf(log, "utime = %lu\n", (unsigned long)info->si_utime);
			fprintf(log, "stime = %lu\n", (unsigned long)info->si_stime);
			fprintf(log, "address lsb = %d\n", info->si_addr_lsb);
			fprintf(log, "lower bound = 0x%llx\n", (unsigned long long)info->si_lower);
			fprintf(log, "lower bound = 0x%llx\n", (unsigned long long)info->si_upper);
			fprintf(log, "syscall address = 0x%llx\n", (unsigned long long)info->si_call_addr);
			fprintf(log, "syscall = 0x%d\n", info->si_syscall);

			fprintf(log, "Backtrace:\n");
			void *addresses[300] = {0};
			int n_addresses = backtrace(addresses, (int)arr_count(addresses));
			for (int i = 0; i < n_addresses; ++i) {
				fprintf(log, " 0x%llx\n", (unsigned long long)addresses[i]);
			}
		#endif
			fclose(log);
		}

		die(CRASH_MESSAGE);
	} else {
		die(CRASH_STARTUP_MESSAGE);
	}
}
#elif _WIN32
static char const *windows_exception_to_str(DWORD exception_code) {
	switch (exception_code) {
	case EXCEPTION_ACCESS_VIOLATION: return "Access violation";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array out of bounds";
	case EXCEPTION_BREAKPOINT: return "Breakpoint";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "Misaligned read or write";
	case EXCEPTION_FLT_DENORMAL_OPERAND: return "Floating-point denormal operand";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Floating-point division by zero";
	case EXCEPTION_FLT_INEXACT_RESULT: return "Floating-point inexact result";
	case EXCEPTION_FLT_INVALID_OPERATION: return "Floating-point invalid operation";
	case EXCEPTION_FLT_OVERFLOW: return "Floating-point overflow";
	case EXCEPTION_FLT_STACK_CHECK: return "Floating-point stack over/underflow";
	case EXCEPTION_FLT_UNDERFLOW: return "Floating-point underflow";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal instruction";
	case EXCEPTION_IN_PAGE_ERROR: return "Page not present";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer divide by zero";
	case EXCEPTION_INT_OVERFLOW: return "Integer overflow";
	case EXCEPTION_INVALID_DISPOSITION: return "Invalid disposition";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Continue after non-continuable exception";
	case EXCEPTION_PRIV_INSTRUCTION: return "Private instruction executed";
	case EXCEPTION_SINGLE_STEP: return "Single step";
	case EXCEPTION_STACK_OVERFLOW: return "Stack overflow";
	}
	return "Unknown exception";
}


static LONG WINAPI error_signal_handler(EXCEPTION_POINTERS *info) {
	if (signal_being_handled)
		die(CRASH_CRASH_MESSAGE);
	signal_being_handled = true;
	Ted *ted = error_signal_handler_ted;
	if (ted) {
		FILE *log = ted->log;
		if (log) {
			DWORD exception_code = info->ExceptionRecord->ExceptionCode;
			fprintf(log, "Exception 0x%lx: %s.\n", (unsigned long)exception_code, windows_exception_to_str(exception_code));
			fprintf(log, "Address: 0x%llx.\n", (unsigned long long)info->ExceptionRecord->ExceptionAddress);
			fprintf(log, "Info0: 0x%llx.\n", (unsigned long long)info->ExceptionRecord->ExceptionInformation[0]);
			fprintf(log, "Info1: 0x%llx.\n", (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
			fprintf(log, "Info2: 0x%llx.\n", (unsigned long long)info->ExceptionRecord->ExceptionInformation[2]);
	#if _M_AMD64
			CONTEXT *context = info->ContextRecord;
			if (exception_code == EXCEPTION_STACK_OVERFLOW) {
				// don't backtrace; just output current address
				fprintf(log, "Instruction: 0x%llx\n", (unsigned long long)context->Rip);
			} else {
				fprintf(log, "Backtrace:\n");
				HANDLE process = GetCurrentProcess(), thread = GetCurrentThread(); 
				// backtrace
				// this here was very helpful: https://gist.github.com/jvranish/4441299
				if (SymInitialize(process, NULL, true)) {
					STACKFRAME frame = {0};
					frame.AddrPC.Offset = context->Rip;
					frame.AddrStack.Offset = context->Rsp;
					frame.AddrFrame.Offset = context->Rbp;
					frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
					while (StackWalk(IMAGE_FILE_MACHINE_AMD64, process, thread,
						&frame, context, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL)) {
						fprintf(log, "0x%llx\n", (unsigned long long)frame.AddrPC.Offset);
					}
					SymCleanup(process);
				}
			}
	#endif
			fclose(log);
		}
		die(CRASH_MESSAGE);
	} else {
		die(CRASH_STARTUP_MESSAGE);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void ted_update_window_dimensions(Ted *ted) {
	int w = 0, h = 0;
	SDL_GetWindowSize(ted->window, &w, &h);
	gl_window_width = ted->window_width = (float)w;
	gl_window_height = ted->window_height = (float)h;
}

// returns true if the buffer "used" this event
static bool handle_buffer_click(Ted *ted, TextBuffer *buffer, v2 click, u8 times) {
	BufferPos buffer_pos;
	if (buffer_pixels_to_pos(buffer, click, &buffer_pos)) {
		// user clicked on buffer
		if (!ted->menu) {
			ted_switch_to_buffer(ted, buffer);
		}
		if (buffer == ted->active_buffer) {
			switch (ted->key_modifier) {
			case KEY_MODIFIER_SHIFT:
				// select to position
				buffer_select_to_pos(buffer, buffer_pos);
				break;
			case KEY_MODIFIER_CTRL: {
				buffer_cursor_move_to_pos(buffer, buffer_pos);
				String32 word = buffer_word_at_cursor(buffer);
				if (word.len) {
					char *tag = str32_to_utf8_cstr(word);
					if (tag) {
						tag_goto(buffer->ted, tag);
						free(tag);
					}
				}
			} break;
			case 0:
				buffer_cursor_move_to_pos(buffer, buffer_pos);
				switch ((times - 1) % 3) {
				case 0: break; // single-click
				case 1: // double-click: select word
					buffer_select_word(buffer);
					break;
				case 2: // triple-click: select line
					buffer_select_line(buffer);
					break;
				}
				ted->drag_buffer = buffer;
				break;
			}
			return true;
		}
	}
	return false;
}

#if _WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow) {
	(void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
	int argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char** argv = malloc(argc * sizeof *argv);
	if (!argv) {
		die("Out of memory.");
	}
    for (int i = 0; i < argc; i++) {
        LPWSTR wide_arg = wide_argv[i];
        int len = (int)wcslen(wide_arg);
        argv[i] = malloc(len + 1);
		if (!argv[i]) die("Out of memory.");
        for (int j = 0; j <= len; j++)
            argv[i][j] = (char)wide_arg[j];
    }
    LocalFree(wide_argv);
#else
int main(int argc, char **argv) {
#endif
	
#if __unix__
	{
		struct sigaction act = {0};
		act.sa_sigaction = error_signal_handler;
		act.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &act, NULL);
		sigaction(SIGFPE, &act, NULL);
		sigaction(SIGABRT, &act, NULL);
		sigaction(SIGILL, &act, NULL);
		sigaction(SIGPIPE, &act, NULL);
	}
#elif _WIN32
	SetUnhandledExceptionFilter(error_signal_handler);
#else
	#error "Unrecognized operating system."
#endif
	
	setlocale(LC_ALL, ""); // allow unicode

	// read command-line arguments
	char const *starting_filename = NULL;
	switch (argc) {
	case 0: case 1: break;
	case 2:
		// essentially, replace / with \ on windows.
		for (char *p = argv[1]; *p; ++p)
			if (strchr(ALL_PATH_SEPARATORS, *p))
				*p = PATH_SEPARATOR;
		starting_filename = argv[1];
		break;
	default:	
		fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	

	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); // if this program is sent a SIGTERM/SIGINT, don't turn it into a quit event
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		die("%s", SDL_GetError());

	Ted *ted = calloc(1, sizeof *ted);
	if (!ted) {
		die("Not enough memory available to run ted.");
	}
	
	// make sure signal handler has access to ted.
	error_signal_handler_ted = ted;

	{ // get local data directory
#if _WIN32
		wchar_t *appdata = NULL;
		KNOWNFOLDERID id = FOLDERID_LocalAppData;
		if (SHGetKnownFolderPath(&id, 0, NULL, &appdata) == S_OK) {
			strbuf_printf(ted->local_data_dir, "%ls" PATH_SEPARATOR_STR "ted", appdata);
			CoTaskMemFree(appdata);
		}
		id = FOLDERID_Profile;
		wchar_t *home = NULL;
		if (SHGetKnownFolderPath(&id, 0, NULL, &home) == S_OK) {
			strbuf_printf(ted->home, "%ls", home);
			CoTaskMemFree(home);
		}
		
		// on Windows, the global data directory is just the directory where the executable is.
		char executable_path[TED_PATH_MAX] = {0};
		if (GetModuleFileNameA(NULL, executable_path, sizeof executable_path) > 0) {
			char *last_backslash = strrchr(executable_path, '\\');
			if (last_backslash) {
				*last_backslash = '\0';
				strbuf_cpy(ted->global_data_dir, executable_path);
			}
		}
#else
		char *home = getenv("HOME");
		strbuf_printf(ted->home, "%s", home);
		strbuf_printf(ted->local_data_dir, "%s/.local/share/ted", home);
		strbuf_printf(ted->global_data_dir, "/usr/share/ted");
#endif

		if (fs_path_type(ted->local_data_dir) == FS_NON_EXISTENT)
			fs_mkdir(ted->local_data_dir);
		
	}

	FILE *log = NULL;
	{
		// open log file
		char log_filename[TED_PATH_MAX];
		strbuf_printf(log_filename, "%s/log.txt", ted->local_data_dir);
		log = fopen(log_filename, "w");
	}
	ted->log = log;

	{ // get current working directory
		fs_get_cwd(ted->cwd, sizeof ted->cwd);
	}

	{ // check if this is the installed version of ted (as opposed to just running it from the directory with the source)
	#if _WIN32
		// never search cwd; we'll search the executable directory anyways
	#else
		char executable_path[TED_PATH_MAX] = {0};
		char const *cwd = ted->cwd;
		ssize_t len = readlink("/proc/self/exe", executable_path, sizeof executable_path - 1);
		if (len == -1) {
			// some posix systems don't have /proc/self/exe. oh well.
		} else {
			executable_path[len] = '\0';
			char *last_slash = strrchr(executable_path, '/');
			if (last_slash) {
				*last_slash = '\0';
				ted->search_cwd = streq(cwd, executable_path);
			}
		}
	#endif
	}
	#if TED_FORCE_SEARCH_CWD
	// override whether or not we are in the executable's directory
	// (for testing on Unix systems without /proc)
	ted->search_cwd = true;
	#endif

	Settings *settings = &ted->settings;
	char config_err[sizeof ted->error] = {0};
	
	{
		// copy global config to local config
		char local_config_filename[TED_PATH_MAX];
		strbuf_printf(local_config_filename, "%s" PATH_SEPARATOR_STR "ted.cfg", ted->local_data_dir);
		char global_config_filename[TED_PATH_MAX];
		strbuf_printf(global_config_filename, "%s" PATH_SEPARATOR_STR "ted.cfg", ted->global_data_dir);
		if (!fs_file_exists(local_config_filename)) {
			if (fs_file_exists(global_config_filename)) {
				if (!copy_file(global_config_filename, local_config_filename)) {
					die("Couldn't copy config %s to %s.", global_config_filename, local_config_filename);
				}
			} else {
				die("ted's backup config file, %s, does not exist. Try reinstalling ted?", global_config_filename);
			}
		}
		config_read(ted, local_config_filename);
		if (ted_haserr(ted)) {
			// if there's an error in the local config, read the global config to make sure everything's ok
			config_read(ted, global_config_filename);
			strcpy(config_err, ted->error);
			ted_clearerr(ted); // clear the error so later things (e.g. loading font) don't detect an error
		}
	}
	

	SDL_Window *window = SDL_CreateWindow("ted", SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if (!window)
		die("%s", SDL_GetError());

	ted->window = window;
		
	{ // set icon
		char icon_filename[TED_PATH_MAX];
		if (ted_get_file(ted, "assets/icon.bmp", icon_filename, sizeof icon_filename)) {
			SDL_Surface *icon = SDL_LoadBMP(icon_filename);
			SDL_SetWindowIcon(window, icon);
			SDL_FreeSurface(icon);
		} // if we can't find the icon file, it's no big deal
	}

	gl_version_major = 4;
	gl_version_minor = 3;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
#if DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
	SDL_GLContext *glctx = SDL_GL_CreateContext(window);
	if (!glctx) {
		debug_println("Couldn't get GL 4.3 context. Falling back to 2.0.");
		gl_version_major = 2;
		gl_version_minor = 0;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		glctx = SDL_GL_CreateContext(window);
		if (!glctx)
			die("%s", SDL_GetError());
	}
	gl_get_procs();

#if DEBUG
	if (gl_version_major * 100 + gl_version_minor >= 403) {
		GLint flags = 0;
		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
			// set up debug message callback
			glDebugMessageCallback(gl_message_callback, NULL);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		}
	}
#endif
	
	gl_geometry_init();
	text_init();
	
	SDL_GL_SetSwapInterval(1); // vsync
	ted_load_fonts(ted);
	if (ted_haserr(ted))
		die("Error loading font: %s", ted_geterr(ted));
	{
		TextBuffer *lbuffer = &ted->line_buffer;
		line_buffer_create(lbuffer, ted);
		if (buffer_haserr(lbuffer))
			die("Error creating line buffer: %s", buffer_geterr(lbuffer));
	}
	line_buffer_create(&ted->find_buffer, ted);
	line_buffer_create(&ted->replace_buffer, ted);
	buffer_create(&ted->build_buffer, ted);

	{
		if (starting_filename) {
			if (fs_file_exists(starting_filename)) {
				if (!ted_open_file(ted, starting_filename))
					ted_seterr(ted, "Couldn't load file: %s", ted_geterr(ted));
			} else {
				if (!ted_new_file(ted, starting_filename))
					ted_seterr(ted, "Couldn't create file: %s", ted_geterr(ted));
			}
		} else {
			session_read(ted);
		}
	}


	u32 *colors = settings->colors; (void)colors;

	ted->cursor_ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	ted->cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	ted->cursor_resize_h = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	ted->cursor_resize_v = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	ted->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	ted->cursor_move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);



	Uint32 time_at_last_frame = SDL_GetTicks();
	
	strbuf_cpy(ted->error, config_err);
	
	while (!ted->quit) {
	#if DEBUG
		//printf("\033[H\033[2J");
	#endif
	#if PROFILE
		double frame_start = time_get_seconds();
	#endif

		SDL_Event event;
		Uint8 const *keyboard_state = SDL_GetKeyboardState(NULL);
		
		{ // get mouse position
			int mouse_x = 0, mouse_y = 0;
			ted->mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
			ted->mouse_pos = V2((float)mouse_x, (float)mouse_y);
		}
		bool ctrl_down = keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL];
		bool shift_down = keyboard_state[SDL_SCANCODE_LSHIFT] || keyboard_state[SDL_SCANCODE_RSHIFT];
		bool alt_down = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];

		memset(ted->nmouse_clicks, 0, sizeof ted->nmouse_clicks);
		memset(ted->nmouse_releases, 0, sizeof ted->nmouse_releases);
		ted->scroll_total_x = ted->scroll_total_y = 0;

		ted_update_window_dimensions(ted);
		u32 key_modifier = (u32)ctrl_down << KEY_MODIFIER_CTRL_BIT
				| (u32)shift_down << KEY_MODIFIER_SHIFT_BIT
				| (u32)alt_down << KEY_MODIFIER_ALT_BIT;
		ted->key_modifier = key_modifier;
			
		while (SDL_PollEvent(&event)) {
			TextBuffer *buffer = ted->active_buffer;
			
			switch (event.type) {
			case SDL_QUIT:
				command_execute(ted, CMD_QUIT, 1);
				break;
			case SDL_MOUSEWHEEL: {
				// scroll with mouse wheel
				Sint32 dx = event.wheel.x, dy = -event.wheel.y;
				ted->scroll_total_x += dx;
				ted->scroll_total_y += dy;
			} break;
			case SDL_MOUSEBUTTONDOWN: {
				Uint32 button = event.button.button;
				u8 times = event.button.clicks; // number of clicks
				float x = (float)event.button.x, y = (float)event.button.y;
				if (button < arr_count(ted->nmouse_clicks) 
					&& ted->nmouse_clicks[button] < arr_count(ted->mouse_clicks[button])) {
					v2 pos = V2(x, y);
					bool add = true;
					if (*ted->error_shown) {
						if (rect_contains_point(error_box_rect(ted), pos)) {
							// clicked on error
							if (button == SDL_BUTTON_LEFT) {
								// dismiss error
								*ted->error_shown = '\0';
							}
							// don't let anyone else use this event
							add = false;
						}
					}
					
					if (add) {						
						// handle mouse click
						// we need to do this here, and not in buffer_render, because ctrl+click (go to definition)
						// could switch to a different buffer.
						for (u32 i = 0; i < TED_MAX_NODES; ++i) {
							if (ted->nodes_used[i]) {
								Node *node = &ted->nodes[i];
								if (node->tabs) {
									buffer = &ted->buffers[node->tabs[node->active_tab]];
									if (handle_buffer_click(ted, buffer, pos, times)) {
										add = false;
										break;
									}
								}
							}
						}
						if (ted->find) {
							add = add && !handle_buffer_click(ted, &ted->find_buffer, pos, times);
							if (ted->replace)
								add = add && !handle_buffer_click(ted, &ted->replace_buffer, pos, times);
						}
						if (add) {
							ted->mouse_clicks[button][ted->nmouse_clicks[button]] = pos;
							ted->mouse_click_times[button][ted->nmouse_clicks[button]] = times;
							++ted->nmouse_clicks[button];
						}
					}
				}
			} break;
			case SDL_MOUSEBUTTONUP: {
				Uint8 button = event.button.button;
				v2 pos = V2((float)event.button.x, (float)event.button.y);
				if (ted->nmouse_releases[button] < arr_count(ted->mouse_releases[button])) {
					ted->mouse_releases[button][ted->nmouse_releases[button]++] = pos;
				}
			} break;
			case SDL_MOUSEMOTION: {
				float x = (float)event.motion.x, y = (float)event.motion.y;
				if (ted->drag_buffer != ted->active_buffer)
					ted->drag_buffer = NULL;
				if (ted->drag_buffer) {
					BufferPos pos = {0};
					// drag to select
					// we don't check the return value here, because it's okay to drag off the screen.
					buffer_pixels_to_pos(ted->drag_buffer, V2(x, y), &pos);
					buffer_select_to_pos(ted->drag_buffer, pos);
				}
			} break;
			case SDL_KEYDOWN: {
				SDL_Scancode scancode = event.key.keysym.scancode;
				SDL_Keymod modifier = event.key.keysym.mod;
				u32 key_combo = (u32)scancode << 3 |
					(u32)((modifier & (KMOD_LCTRL|KMOD_RCTRL)) != 0) << KEY_MODIFIER_CTRL_BIT |
					(u32)((modifier & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0) << KEY_MODIFIER_SHIFT_BIT |
					(u32)((modifier & (KMOD_LALT|KMOD_RALT)) != 0) << KEY_MODIFIER_ALT_BIT;
				if (key_combo < KEY_COMBO_COUNT) {
					KeyAction *action = &ted->key_actions[key_combo];
					if (action->command) {
						command_execute(ted, action->command, action->argument);
					}
				}
			} break;
			case SDL_TEXTINPUT: {
				char *text = event.text.text;
				if (buffer
					&& (key_modifier & ~KEY_MODIFIER_SHIFT) == 0) // unfortunately, some key combinations like ctrl+minus still register as a "-" text input event
					buffer_insert_utf8_at_cursor(buffer, text);
			} break;
			}
		}
		{
			int mx = 0, my = 0;
			ted->mouse_state = SDL_GetMouseState(&mx, &my);
			ted->mouse_pos = V2((float)mx, (float)my);
		}
		// default to arrow cursor
		ted->cursor = ted->cursor_arrow;
		if (!(ted->mouse_state & SDL_BUTTON_LMASK)) {
			// originally this was done on SDL_MOUSEBUTTONUP events but for some reason
			// I was getting a bunch of those even when I was holding down the mouse.
			// This makes it much smoother.
			ted->drag_buffer = NULL;
		}

		{ // ted->cwd should be the directory containing the last active buffer
			TextBuffer *buffer = ted->active_buffer;
			if (buffer) {
				char const *buffer_path = buffer_get_filename(buffer);
				if (buffer_path && !buffer_is_untitled(buffer)) {
					assert(*buffer_path);
					char *last_sep = strrchr(buffer_path, PATH_SEPARATOR);
					if (last_sep) {
						size_t dirname_len = (size_t)(last_sep - buffer_path);
						if (dirname_len == 0) dirname_len = 1; // make sure "/x" sets our cwd to "/", not ""
						// set cwd to buffer's directory
						memcpy(ted->cwd, buffer_path, dirname_len);
						ted->cwd[dirname_len] = 0;
					}
				}
			}
		}
		
		// check if active buffer should be reloaded
		{
			TextBuffer *active_buffer = ted->active_buffer;
			if (active_buffer && buffer_externally_changed(active_buffer)) {
				if (settings->auto_reload)
					buffer_reload(active_buffer);
				else {
					strbuf_cpy(ted->ask_reload, buffer_get_filename(active_buffer));
					menu_open(ted, MENU_ASK_RELOAD);
				}
			}
		}

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
		
		if (ted->menu) {
			menu_update(ted);
		}
		
		ted_update_window_dimensions(ted);
		float window_width = ted->window_width, window_height = ted->window_height;

		// set up GL
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, (GLsizei)window_width, (GLsizei)window_height);
		{ // clear (background)
			float bg_color[4];
			rgba_u32_to_floats(settings->colors[COLOR_BG], bg_color);
			glClearColor(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
		}
		glClear(GL_COLOR_BUFFER_BIT);
		
		Font *font = ted->font;

		// default window titlel
		strcpy(ted->window_title, "ted");


		if (ted->nodes_used[0]) {
			float const padding = settings->padding;
			float x1 = padding, y = window_height-padding, x2 = window_width-padding;
			Node *node = &ted->nodes[0];
			if (ted->find) {
				float y2 = y;
				y -= find_menu_height(ted);
				find_menu_frame(ted, rect4(x1, y, x2, y2));
				y -= padding;
			}
			if (ted->build_shown) {
				float y2 = y;
				y -= 0.3f * ted->window_height;
				build_frame(ted, x1, y, x2, y2);
				y -= padding;
			}

			float y1 = padding;
			node_frame(ted, node, rect4(x1, y1, x2, y));
		} else {
			text_utf8_anchored(font, "Press Ctrl+O to open a file or Ctrl+N to create a new one.",
				window_width * 0.5f, window_height * 0.5f, colors[COLOR_TEXT_SECONDARY], ANCHOR_MIDDLE);
			text_render(font);
		}

		// stop dragging tab if mouse was released
		if (ted->nmouse_releases[SDL_BUTTON_LEFT])
			ted->dragging_tab_node = NULL;

		if (ted->menu) {
			menu_render(ted);
		}

		if (text_has_err()) {
			ted_seterr(ted, "Couldn't render text: %s", text_get_err());
		}
		for (u16 i = 0; i < TED_MAX_BUFFERS; ++i) {
			TextBuffer *buffer = &ted->buffers[i];
			if (buffer_haserr(buffer)) {
				ted_seterr_to_buferr(ted, buffer);
				buffer_clearerr(buffer);
			}
		}

		// check if there's a new error
		if (ted_haserr(ted)) {
			ted->error_time = time_get_seconds();
			str_cpy(ted->error_shown, sizeof ted->error_shown, ted->error);

			{ // output error to log file
				char tstr[256];
				time_t t = time(NULL);
				struct tm *tm = localtime(&t);
				strftime(tstr, sizeof tstr, "%Y-%m-%d %H:%M:%S", tm);
				if (log) {
					fprintf(log, "[ERROR %s] %s\n", tstr, ted->error);
					fflush(log);
				}
			}
			ted_clearerr(ted);
		}

		// error box
		if (*ted->error_shown) {
			double t = time_get_seconds();
			double time_passed = t - ted->error_time;
			if (time_passed > settings->error_display_time) {
				// stop showing error
				*ted->error_shown = '\0';
			} else {
				Rect r = error_box_rect(ted);
				float padding = settings->padding;

				gl_geometry_rect(r, colors[COLOR_ERROR_BG]);
				gl_geometry_rect_border(r, settings->border_thickness, colors[COLOR_ERROR_BORDER]);

				float text_x1 = rect_x1(r) + padding, text_x2 = rect_x2(r) - padding;
				float text_y1 = rect_y1(r) + padding;

				// (make sure text wraps)
				TextRenderState text_state = text_render_state_default;
				text_state.min_x = text_x1;
				text_state.max_x = text_x2;
				text_state.x = text_x1;
				text_state.y = text_y1;
				text_state.wrap = true;
				rgba_u32_to_floats(colors[COLOR_ERROR_TEXT], text_state.color);
				text_utf8_with_state(font, &text_state, ted->error_shown);
				gl_geometry_draw();
				text_render(font);
			}
		}

	#if DEBUG
		for (u16 i = 0; i < TED_MAX_BUFFERS; ++i)
			if (ted->buffers_used[i])
				buffer_check_valid(&ted->buffers[i]);
		buffer_check_valid(&ted->line_buffer);
	#endif
	
		glFinish();
		
	#if PROFILE
		double frame_end_noswap = time_get_seconds();
		{
			print("Frame (noswap): %.1f ms\n", (frame_end_noswap - frame_start) * 1000);
		}
	#endif
		
		if (ted->dragging_tab_node)
			ted->cursor = ted->cursor_move;

		SDL_SetWindowTitle(window, ted->window_title);
		SDL_SetCursor(ted->cursor);
	
		SDL_GL_SwapWindow(window);
		PROFILE_TIME(frame_end);

		assert(glGetError() == 0);

	#if PROFILE
		{
			print("Frame: %.1f ms\n", (frame_end - frame_start) * 1000);
		}
	#endif

	}

	session_write(ted);

	SDL_FreeCursor(ted->cursor_arrow);
	SDL_FreeCursor(ted->cursor_ibeam);
	SDL_FreeCursor(ted->cursor_resize_h);
	SDL_FreeCursor(ted->cursor_resize_v);
	SDL_FreeCursor(ted->cursor_hand);
	SDL_FreeCursor(ted->cursor_move);
	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);
	SDL_Quit();
	build_stop(ted);
	if (ted->menu)
		menu_close(ted); // free any memory used by the current menu
	if (log) fclose(log);
	find_close(ted);
	tag_selector_close(ted);
	for (u16 i = 0; i < TED_MAX_BUFFERS; ++i)
		if (ted->buffers_used[i])
			buffer_free(&ted->buffers[i]);
	for (u16 i = 0; i < TED_MAX_NODES; ++i)
		if (ted->nodes_used[i])
			node_free(&ted->nodes[i]);
	buffer_free(&ted->line_buffer);
	buffer_free(&ted->find_buffer);
	buffer_free(&ted->replace_buffer);
	buffer_free(&ted->build_buffer);
	text_font_free(ted->font);
	text_font_free(ted->font_bold);
	settings_free(&ted->settings);
	free(ted);
#if _WIN32
	for (int i = 0; i < argc; ++i)
		free(argv[i]);
	free(argv);
#endif

	return 0;
}
