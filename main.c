/*
@TODO:
- LSPResponse is_error member (and make sure ide-*.c handles it)
- some way of showing that we're currently loading the definition location (different cursor color?)
- more LSP stuff:
     - go to definition using LSP
     - find usages
     - refactoring?
- test full unicode position handling
- check if there are any other non-optional/nice-to-have-support-for server-to-client requests
- better non-error window/showMessage(Request)
- document lsp.h and lsp.c.
- maximum queue size for requests/responses just in case?
   - idea: configurable timeout
   -  what to do if initialize request takes a long time?
- delete old sent requests? but make sure requests that just take a long time are okay.
    (if the server never sends a response)
- check that tags still works
- TESTING: make rust-analyzer-slow (waits 10s before sending response)
- run everything through valgrind ideally with leak checking
- grep -i -n TODO *.[ch]
- when searching files/definitions, sort by length? or put exact matches at the top? 
--- LSP MERGE ---
- improve structure of ted source code to make LSP completions better
      (make every c file a valid translation unit)
- CSS highlighting
- styles ([color] sections)
- more documentation generally (development.md or something?)
- rename buffer->filename to buffer->path
    - make buffer->path NULL for untitled buffers & fix resulting mess
- rust-analyzer bug reports:
    - bad json can give "Unexpected error: client exited without proper shutdown sequence"
FUTURE FEATURES:
- comment-start + comment-end settings
- robust find (results shouldn't move around when you type things)
- multiple files with command line arguments
- :set-build-command
- add numlock as a key modifier? (but make sure "Ctrl+S" handles both "No NumLock+Ctrl+S" and "NumLock+Ctrl+S")
- better undo chaining (dechain on backspace?)
- allow multiple fonts (fonts directory?)
- regenerate tags for completion too if there are no results
- config variables
- bind key to multiple commands
- plugins?
- keyboard macros
    -  ctrl+9/0 to inc/dec number would be useful here
    - with macros we can really test performance of buffer_insert_text_at_pos, etc. (which should ideally be fast)
- real typescript highlighting (?)
*/

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
#include <pcre2.h>

#if DEBUG
extern unsigned char *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
#else
#define STB_IMAGE_STATIC
no_warn_start
#include "stb_image.c"
no_warn_end
#endif

#include "unicode.h"
#include "ds.c"
#include "util.c"

#if _WIN32
#include "filesystem-win.c"
#elif __unix__
#include "filesystem-posix.c"
#else
#error "Unrecognized operating system."
#endif

#include "math.c"
#if _WIN32
#include "process-win.c"
#elif __unix__
#include "process-posix.c"
#else
#error "Unrecognized operating system."
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

#include "io.c"

#include "text.h"
#include "command.h"
#include "colors.h"
#include "time.c"
#include "lsp.h"
#include "ted.h"
#include "gl.c"
#include "text.c"

#include "string32.c"
#include "colors.c"
#include "syntax.c"
bool tag_goto(Ted *ted, char const *tag);
#include "buffer.c"
#include "ted.c"
#include "ui.c"
#include "find.c"
#include "node.c"
#include "build.c"
#include "tags.c"
#include "menu.c"
#include "ide-autocomplete.c"
#include "ide-signature-help.c"
#include "ide-hover.c"
#include "ide-definitions.c"
#include "command.c"
#include "config.c"
#include "session.c"
#include "lsp.c"

#if PROFILE
#define PROFILE_TIME(var) double var = time_get_seconds();
#else
#define PROFILE_TIME(var)
#endif

static Rect error_box_rect(Ted *ted) {
	Font *font = ted->font;
	Settings const *settings = ted_active_settings(ted);
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

#if _WIN32
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow) {
	(void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
	int argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char** argv = calloc(argc + 1, sizeof *argv);
	if (!argv) {
		die("Out of memory.");
	}
    for (int i = 0; i < argc; i++) {
        LPWSTR wide_arg = wide_argv[i];
        int len = (int)wcslen(wide_arg);
        int bufsz = len * 4 + 8;
        argv[i] = calloc((size_t)bufsz, 1);
	if (!argv[i]) die("Out of memory.");
	WideCharToMultiByte(CP_UTF8, 0, wide_arg, len, argv[i], bufsz - 1, NULL, NULL);
    }
    LocalFree(wide_argv);
#else
int main(int argc, char **argv) {
#endif
	PROFILE_TIME(init_start)
	PROFILE_TIME(basic_init_start)
	
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
	
	#if _WIN32
	setlocale(LC_ALL, ".65001");
	#else
	setlocale(LC_ALL, "C.UTF-8");
	#endif
	
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
	
	PROFILE_TIME(basic_init_end)
	
	PROFILE_TIME(sdl_start)
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); // if this program is sent a SIGTERM/SIGINT, don't turn it into a quit event
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		die("%s", SDL_GetError());
	PROFILE_TIME(sdl_end)
	
	PROFILE_TIME(misc_start)

	Ted *ted = calloc(1, sizeof *ted);
	if (!ted) {
		die("Not enough memory available to run ted.");
	}
	ted->last_save_time = -1e50;
	
	// make sure signal handler has access to ted.
	error_signal_handler_ted = ted;

	fs_get_cwd(ted->start_cwd, sizeof ted->start_cwd);
	{ // get local and global data directory
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
		WCHAR executable_wide_path[TED_PATH_MAX] = {0};
		char executable_path[TED_PATH_MAX] = {0};
		if (GetModuleFileNameW(NULL, executable_wide_path, sizeof executable_wide_path - 1) > 0) {
			WideCharToMultiByte(CP_UTF8, 0, executable_wide_path, -1, executable_path, sizeof executable_path, NULL, NULL);
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
				ted->search_start_cwd = streq(cwd, executable_path);
			}
		}
	#endif
	}
	#if TED_FORCE_SEARCH_CWD
	// override whether or not we are in the executable's directory
	// (for testing on Unix systems without /proc)
	ted->search_cwd = true;
	#endif


	char config_err[sizeof ted->error] = {0};
	
	PROFILE_TIME(misc_end)
	
	PROFILE_TIME(window_start)
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
	
	PROFILE_TIME(window_end)
	PROFILE_TIME(gl_start)
	
	SDL_GLContext *glctx = NULL;
	{ // get OpenGL context
		int gl_versions[][2] = {
			{4,3},
			{3,0},
			{2,0},
			{0,0},
		};
		for (int i = 0; gl_versions[i][0]; ++i) {
			gl_version_major = gl_versions[i][0];
			gl_version_minor = gl_versions[i][1];
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
		#if DEBUG
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		#endif
			glctx = SDL_GL_CreateContext(window);
			if (glctx) {
				break;
			} else {
				debug_println("Couldn't get GL %d.%d context. Falling back to %d.%d.",
					gl_versions[i][0], gl_versions[i][1], gl_versions[i+1][0], gl_versions[i+1][1]);
			}
		}
		
		if (!glctx)
			die("%s", SDL_GetError());
		gl_get_procs();
	}
	
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
	PROFILE_TIME(gl_end)
	
	
	PROFILE_TIME(configs_start)
	ted_load_configs(ted, false);
	if (ted_haserr(ted)) {
		strcpy(config_err, ted->error);
		ted_clearerr(ted); // clear the error so later things (e.g. loading font) don't detect an error
	}
	PROFILE_TIME(configs_end)
	
	PROFILE_TIME(fonts_start)
	ted_load_fonts(ted);
	PROFILE_TIME(fonts_end)
	
	PROFILE_TIME(create_start)
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
	line_buffer_create(&ted->argument_buffer, ted);
	buffer_create(&ted->build_buffer, ted);

	{
		if (starting_filename) {
			if (fs_file_exists(starting_filename)) {
				if (!ted_open_file(ted, starting_filename)) {
					char err[512] = {0};
					sprintf(err, "%.500s", ted_geterr(ted)); // -Wrestrict (rightly) complains without this intermediate step
					ted_seterr(ted, "Couldn't load file: %s", err);
				}
			} else {
				if (!ted_new_file(ted, starting_filename)) {
					char err[512] = {0};
					sprintf(err, "%.500s", ted_geterr(ted));
					ted_seterr(ted, "Couldn't create file: %s", err);
				}
			}
		} else {
			session_read(ted);
		}
	}



	ted->cursor_ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	ted->cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	ted->cursor_resize_h = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	ted->cursor_resize_v = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	ted->cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	ted->cursor_move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
	
	PROFILE_TIME(create_end)

	PROFILE_TIME(get_ready_start)

	Uint32 time_at_last_frame = SDL_GetTicks();
	
	strbuf_cpy(ted->error, config_err);

	SDL_GL_SetSwapInterval(1); // vsync
	
	PROFILE_TIME(get_ready_end)
	
	PROFILE_TIME(init_end)
	
#if PROFILE
	print("Initialization: %.1fms\n", 1000 * (init_end - init_start));
	print(" - Basic init: %.1fms\n", 1000 * (basic_init_end - basic_init_start));
	print(" - SDL: %.1fms\n", 1000 * (sdl_end - sdl_start));
	print(" - SDL window: %.1fms\n", 1000 * (window_end - window_start));
	print(" - Create: %.1fms\n", 1000 * (create_end - create_start));
	print(" - OpenGL: %.1fms\n", 1000 * (gl_end - gl_start));
	print(" - misc: %.1fms\n", 1000 * (misc_end - misc_start));
	print(" - Loading fonts: %.1fms\n", 1000 * (fonts_end - fonts_start));
	print(" - Read configs: %.1fms\n", 1000 * (configs_end - configs_start));
	print(" - Get ready: %.1fms\n", 1000 * (get_ready_end - get_ready_start));
#endif

	{
		// clear event queue
		// this is probably only a problem for me, but
		//  some events that the WM should have consumed
		//  are going to ted
		SDL_Event event;
		while (SDL_PollEvent(&event));
	}
	
	double start_time = time_get_seconds();
	
	while (!ted->quit) {
		double frame_start = time_get_seconds();
		ted->frame_time = time_get();

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
				Autocomplete *ac = &ted->autocomplete;
				if (ac->open && rect_contains_point(ac->rect, ted->mouse_pos)) {
					autocomplete_scroll(ted, dy);
				} else {
					ted->scroll_total_x += dx;
					ted->scroll_total_y += dy;
				}
			} break;
			case SDL_MOUSEBUTTONDOWN: {
				Uint32 button = event.button.button;
				u8 times = event.button.clicks; // number of clicks
				float x = (float)event.button.x, y = (float)event.button.y;
				
				if (button == SDL_BUTTON_X1) {
					ted_press_key(ted, SCANCODE_MOUSE_X1, key_modifier);
				} else if (button == SDL_BUTTON_X2) {
					ted_press_key(ted, SCANCODE_MOUSE_X2, key_modifier);
				}
				
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
						// line buffer click handling, IS done in buffer_render (yes this is less than ideal)
						if (!ted->menu) {
							for (u32 i = 0; i < TED_MAX_NODES; ++i) {
								if (ted->nodes_used[i]) {
									Node *node = &ted->nodes[i];
									if (node->tabs) {
										buffer = &ted->buffers[node->tabs[node->active_tab]];
										if (buffer_handle_click(ted, buffer, pos, times)) {
											add = false;
											break;
										}
									}
								}
							}
							if (ted->build_shown)
								if (buffer_handle_click(ted, &ted->build_buffer, pos, times)) // handle build buffer clicks
									add = false;
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
				if (button < arr_count(ted->nmouse_releases)) {
					v2 pos = V2((float)event.button.x, (float)event.button.y);
					if (ted->nmouse_releases[button] < arr_count(ted->mouse_releases[button])) {
						ted->mouse_releases[button][ted->nmouse_releases[button]++] = pos;
					}
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
				ted_press_key(ted, scancode, modifier);
			} break;
			case SDL_TEXTINPUT: {
				char *text = event.text.text;
				if (buffer
					// unfortunately, some key combinations like ctrl+minus still register as a "-" text input event
					&& (key_modifier & ~KEY_MODIFIER_SHIFT) == 0) { 
					// insert the text
					buffer_insert_utf8_at_cursor(buffer, text);
					// check for trigger character
					LSP *lsp = buffer_lsp(buffer);
					Settings *settings = buffer_settings(buffer);
					if (lsp && settings->trigger_characters) {
						u32 last_code_point = (u32)strlen(text) - 1;
						while (last_code_point > 0 &&
							unicode_is_continuation_byte((u8)text[last_code_point]))
							--last_code_point;
						char32_t last_char = 0;
						unicode_utf8_to_utf32(&last_char, &text[last_code_point],
							strlen(text) - last_code_point);
						arr_foreach_ptr(lsp->completion_trigger_chars, char32_t, c) {
							if (*c == last_char) {
								autocomplete_open(ted, last_char);
								break;
							}
						}
						// NOTE: we are not checking for signature help trigger
						// characters because currently we ask for signature
						// help any time a character is inserted.
						
						if (settings->identifier_trigger_characters && is_word(last_char)
							&& !is_digit(last_char))
							autocomplete_open(ted, last_char);
					}
					 
				}
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
				if (buffer_settings(active_buffer)->auto_reload)
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
		
		{
			LSP *lsp = ted_get_active_lsp(ted);
			if (lsp) {
				LSPMessage message = {0};
				while (lsp_next_message(lsp, &message)) {
					switch (message.type) {
					case LSP_REQUEST: {
						LSPRequest *r = &message.u.request;
						switch (r->type) {
						case LSP_REQUEST_SHOW_MESSAGE: {
							LSPRequestMessage *m = &r->data.message;
							// @TODO: multiple messages
							ted_seterr(ted, "%s", m->message);
							} break;
						case LSP_REQUEST_LOG_MESSAGE: {
							LSPRequestMessage *m = &r->data.message;
							// @TODO: actual logging
							printf("%s\n", m->message);
							} break;
						default: break;
						}
						} break;
					case LSP_RESPONSE: {
						LSPResponse *r = &message.u.response;
						autocomplete_process_lsp_response(ted, r);
						signature_help_process_lsp_response(ted, r);
						hover_process_lsp_response(ted, r);
						definitions_process_lsp_response(ted, lsp, r);
						} break;
					}
					lsp_message_free(&message);
				}
			}
		}
		
		ted_update_window_dimensions(ted);
		float window_width = ted->window_width, window_height = ted->window_height;

		// set up GL
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, (GLsizei)window_width, (GLsizei)window_height);
		{ // clear (background)
			float bg_color[4];
			rgba_u32_to_floats(ted_color(ted, COLOR_BG), bg_color);
			glClearColor(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
		}
		glClear(GL_COLOR_BUFFER_BIT);
		
		{
			// background shader
			Settings *s = ted_active_settings(ted);
			if (s->bg_shader) {
				GLuint shader = s->bg_shader->shader;
				GLuint buffer = s->bg_shader->buffer;
				GLuint array = s->bg_shader->array;
				
				glUseProgram(shader);
				if (array) glBindVertexArray(array);
				double t = time_get_seconds();
				glUniform1f(glGetUniformLocation(shader, "t_time"), (float)fmod(t - start_time, 3600));
				glUniform2f(glGetUniformLocation(shader, "t_aspect"), (float)window_width / (float)window_height, 1);
				glUniform1f(glGetUniformLocation(shader, "t_save_time"), (float)(t - ted->last_save_time));
				if (s->bg_texture) {
					GLuint texture = s->bg_texture->texture;
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, texture);
					glUniform1i(glGetUniformLocation(shader, "t_texture"), 0);
				} else {
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, 0);
				}
				glBindBuffer(GL_ARRAY_BUFFER, buffer);
				if (!array) {
					GLuint v_pos = (GLuint)glGetAttribLocation(shader, "v_pos");
					glVertexAttribPointer(v_pos, 2, GL_FLOAT, 0, 2 * sizeof(float), 0);
					glEnableVertexAttribArray(v_pos);
				}
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}
		
		Font *font = ted->font;

		// default window title
		strcpy(ted->window_title, "ted");
		
		{
			float const padding = ted_active_settings(ted)->padding;
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
				y -= ted->build_output_height * ted->window_height;
				if (ted->build_output_height == 0) {
					// hasn't been initialized yet
					ted->build_output_height = 0.25f;
				}
				if (ted->resizing_build_output) {
					if (ted->mouse_state & SDL_BUTTON_LMASK) {
						// resize it
						ted->build_output_height = clampf((y2 - ted->mouse_pos.y) / ted->window_height, 0.05f, 0.8f);
					} else {
						// stop resizing build output
						ted->resizing_build_output = false;
					}
					ted->cursor = ted->cursor_resize_v;
				} else {
					Rect gap = rect4(x1, y - padding, x2, y);
					for (uint i = 0; i < ted->nmouse_clicks[SDL_BUTTON_LEFT]; ++i) {
						if (rect_contains_point(gap, ted->mouse_clicks[SDL_BUTTON_LEFT][i])) {
							// start resizing build output
							ted->resizing_build_output = true;
						}
					}
					if (rect_contains_point(gap, ted->mouse_pos)) {
						ted->cursor = ted->cursor_resize_v;
					}
				}
				build_frame(ted, x1, y, x2, y2);
				y -= padding;
			}

			if (ted->nodes_used[0]) {
				float y1 = padding;
				node_frame(ted, node, rect4(x1, y1, x2, y));
				autocomplete_frame(ted);
				signature_help_frame(ted);
				hover_frame(ted, frame_dt);
			} else {
				autocomplete_close(ted);
				text_utf8_anchored(font, "Press Ctrl+O to open a file or Ctrl+N to create a new one.",
					window_width * 0.5f, window_height * 0.5f, ted_color(ted, COLOR_TEXT_SECONDARY), ANCHOR_MIDDLE);
				text_render(font);
			}
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
		for (int i = 0; ted->lsps[i]; ++i) {
			LSP *lsp = ted->lsps[i];
			if (lsp_get_error(lsp, NULL, 0, false)) {
				lsp_get_error(lsp, ted->error, sizeof ted->error, true);
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
			Settings *settings = ted_active_settings(ted);
			if (time_passed > settings->error_display_time) {
				// stop showing error
				*ted->error_shown = '\0';
			} else {
				Rect r = error_box_rect(ted);
				float padding = settings->padding;

				gl_geometry_rect(r, ted_color(ted, COLOR_ERROR_BG));
				gl_geometry_rect_border(r, settings->border_thickness, ted_color(ted, COLOR_ERROR_BORDER));

				float text_x1 = rect_x1(r) + padding, text_x2 = rect_x2(r) - padding;
				float text_y1 = rect_y1(r) + padding;

				// (make sure text wraps)
				TextRenderState text_state = text_render_state_default;
				text_state.min_x = text_x1;
				text_state.max_x = text_x2;
				text_state.x = text_x1;
				text_state.y = text_y1;
				text_state.wrap = true;
				rgba_u32_to_floats(ted_color(ted, COLOR_ERROR_TEXT), text_state.color);
				text_utf8_with_state(font, &text_state, ted->error_shown);
				gl_geometry_draw();
				text_render(font);
			}
		}

	#if !NDEBUG
		for (u16 i = 0; i < TED_MAX_BUFFERS; ++i)
			if (ted->buffers_used[i])
				buffer_check_valid(&ted->buffers[i]);
		buffer_check_valid(&ted->line_buffer);
	#endif
	
		double frame_end_noswap = time_get_seconds();
	#if PROFILE
		{
			print("Frame (noswap): %.1f ms\n", (frame_end_noswap - frame_start) * 1000);
		}
	#endif
		
		if (ted->dragging_tab_node)
			ted->cursor = ted->cursor_move;

		SDL_SetWindowTitle(window, ted->window_title);
		if (ted->cursor) {
			SDL_SetCursor(ted->cursor);
			SDL_ShowCursor(SDL_ENABLE);
		} else {
			SDL_ShowCursor(SDL_DISABLE);
		}
		
		// annoyingly, SDL_GL_SwapWindow seems to be a busy loop on my laptop for some reason...
		// enforce a framerate of 60. this isn't ideal but SDL_GetDisplayMode is *extremely slow* (250ms), so we don't really have a choice.
		int refresh_rate = 60;
		if (refresh_rate) {
			i32 ms_wait = 1000 / refresh_rate - (i32)((frame_end_noswap - frame_start) * 1000);
			if (ms_wait > 0) {
				ms_wait -= 1; // give swap an extra ms to make sure it's actually vsynced
				SDL_Delay((u32)ms_wait);
			}
		}
		SDL_GL_SwapWindow(window);
		PROFILE_TIME(frame_end)

		assert(glGetError() == 0);

	#if PROFILE
		{
			print("Frame: %.1f ms\n", (frame_end - frame_start) * 1000);
		}
	#endif

	}
	
	if (ted->find)
		find_close(ted);
	build_stop(ted);
	if (ted->menu)
		menu_close(ted);
	hover_close(ted);
	autocomplete_close(ted);
	session_write(ted);
	
	for (int i = 0; i < TED_LSP_MAX; ++i) {
		if (!ted->lsps[i]) break;
		lsp_free(ted->lsps[i]);
		ted->lsps[i] = NULL;
	}
	arr_foreach_ptr(ted->shell_history, char *, cmd) {
		free(*cmd);
	}
	arr_free(ted->shell_history);

	SDL_FreeCursor(ted->cursor_arrow);
	SDL_FreeCursor(ted->cursor_ibeam);
	SDL_FreeCursor(ted->cursor_resize_h);
	SDL_FreeCursor(ted->cursor_resize_v);
	SDL_FreeCursor(ted->cursor_hand);
	SDL_FreeCursor(ted->cursor_move);
	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);
	SDL_Quit();
	if (log) fclose(log);
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
	buffer_free(&ted->argument_buffer);
	text_font_free(ted->font);
	text_font_free(ted->font_bold);
	config_free(ted);
	free(ted);
#if _WIN32
	for (int i = 0; i < argc; ++i)
		free(argv[i]);
	free(argv);
#endif

	return 0;
}
