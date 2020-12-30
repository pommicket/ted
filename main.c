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
#include "time.c"
#include "unicode.h"
#include "util.c"
#define MATH_GL
#include "math.c"
#include "text.h"
#include "string32.c"
#include "arr.c"
#include "buffer.c"

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
#else
int main(void) {
#endif
	setlocale(LC_ALL, ""); // allow unicode
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); // if this program is sent a SIGTERM/SIGINT, don't turn it into a quit event
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		die("%s", SDL_GetError());

	SDL_Window *window = SDL_CreateWindow("ted", SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if (!window)
		die("%s", SDL_GetError());
		
	{ // set icon
		SDL_Surface *icon = SDL_LoadBMP("assets/icon.bmp");
		SDL_SetWindowIcon(window, icon);
		SDL_FreeSurface(icon);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GLContext *glctx = SDL_GL_CreateContext(window);
	if (!glctx) {
		die("%s", SDL_GetError());
	}

	SDL_GL_SetSwapInterval(1); // vsync

	Font *font = text_font_load("assets/font.ttf", 16);
	if (!font) {
		die("Couldn't load font: %s", text_get_err());
	}
	
	bool quit = false;
	TextBuffer text_buffer;
	TextBuffer *buffer = &text_buffer;
	buffer_create(buffer, font);

	if (!buffer_load_file(buffer, "buffer.c"))
		die("Error loading file: %s", buffer_geterr(buffer));


	Uint32 time_at_last_frame = SDL_GetTicks();

	while (!quit) {
	#if DEBUG
		printf("\033[H\033[2J");
	#endif

		SDL_Event event;
		Uint8 const *keyboard_state = SDL_GetKeyboardState(NULL);
		bool ctrl = keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL];
		bool shift = keyboard_state[SDL_SCANCODE_LSHIFT] || keyboard_state[SDL_SCANCODE_RSHIFT];
		bool alt = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];

		while (SDL_PollEvent(&event)) {
			// @TODO: make a function to handle text buffer events
			switch (event.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEWHEEL: {
				// scroll with mouse wheel
				Sint32 dx = event.wheel.x, dy = -event.wheel.y;
				double scroll_speed = 2.5;
				buffer_scroll(buffer, dx * scroll_speed, dy * scroll_speed);
			} break;
			case SDL_MOUSEBUTTONDOWN:
				switch (event.button.button) {
				case SDL_BUTTON_LEFT: {
					BufferPos pos;
					if (buffer_pixels_to_pos(buffer, V2((float)event.button.x, (float)event.button.y), &pos))
						buffer_cursor_move_to_pos(buffer, pos);
				} break;
				}
				break;
			case SDL_KEYDOWN: {
				switch (event.key.keysym.sym) {
				case SDLK_PAGEUP:
					buffer_scroll(buffer, 0, -buffer_display_lines(buffer));
					break;
				case SDLK_PAGEDOWN:
					buffer_scroll(buffer, 0, +buffer_display_lines(buffer));
					break;
				case SDLK_RIGHT:
					if (!alt) {
						if (shift) {
							if (ctrl)
								buffer_select_right_words(buffer, 1);
							else
								buffer_select_right(buffer, 1);
						} else {
							if (ctrl)
								buffer_cursor_move_right_words(buffer, 1);
							else
								buffer_cursor_move_right(buffer, 1);
						}
					}
					break;
				case SDLK_LEFT:
					if (!alt) {
						if (shift) {
							if (ctrl)
								buffer_select_left_words(buffer, 1);
							else
								buffer_select_left(buffer, 1);
						} else {
							if (ctrl)
								buffer_cursor_move_left_words(buffer, 1);
							else
								buffer_cursor_move_left(buffer, 1);
						}
					}
					break;
				case SDLK_UP:
					if (!alt) {
						if (shift) {
							if (ctrl)
								buffer_select_up(buffer, 10);
							else
								buffer_select_up(buffer, 1);
						} else {
							if (ctrl)
								buffer_cursor_move_up(buffer, 10);
							else
								buffer_cursor_move_up(buffer, 1);
						}
					}
					break;
				case SDLK_DOWN:
					if (!alt) {
						if (shift) {
							if (ctrl)
								buffer_select_down(buffer, 10);
							else
								buffer_select_down(buffer, 1);
						} else {
							if (ctrl)
								buffer_cursor_move_down(buffer, 10);
							else
								buffer_cursor_move_down(buffer, 1);
						}
					}
					break;
				case SDLK_RETURN:
					buffer_insert_char_at_cursor(buffer, U'\n');
					break;
				case SDLK_TAB:
					buffer_insert_char_at_cursor(buffer, U'\t');
					break;
				case SDLK_DELETE:
					if (ctrl)
						buffer_delete_words_at_cursor(buffer, 1);
					else
						buffer_delete_chars_at_cursor(buffer, 1);
					break;
				case SDLK_BACKSPACE:
					if (ctrl)
						buffer_backspace_words_at_cursor(buffer, 1);
					else
						buffer_backspace_at_cursor(buffer, 1);
					break;
				case SDLK_s:
					if (ctrl) {
						if (!buffer_save(buffer)) {
							printf("Error saving: %s.", buffer_geterr(buffer));
						}
					}
					break;
				case SDLK_z:
					if (ctrl) {
						if (shift) {
							buffer_redo(buffer, 1);
						} else {
							buffer_undo(buffer, 1);
						}
					}
					break;
				case SDLK_HOME:
					if (ctrl) {
						if (shift) {
							buffer_select_to_start_of_file(buffer);
						} else {
							buffer_cursor_move_to_start_of_file(buffer);
						}
					} else {
						if (shift) {
							buffer_select_to_start_of_line(buffer);
						} else {
							buffer_cursor_move_to_start_of_line(buffer);
						}
					}
					break;
				case SDLK_END:
					if (ctrl) {
						if (shift) {
							buffer_select_to_end_of_file(buffer);
						} else {
							buffer_cursor_move_to_end_of_file(buffer);
						}
					} else {
						if (shift) {
							buffer_select_to_end_of_line(buffer);
						} else {
							buffer_cursor_move_to_end_of_line(buffer);
						}
					}
					break;
				}
			} break;
			case SDL_TEXTINPUT: {
				char *text = event.text.text;
				buffer_insert_utf8_at_cursor(buffer, text);
			} break;
			}
		}

		double frame_dt;
		{
			Uint32 time_this_frame = SDL_GetTicks();
			frame_dt = 0.001 * (time_this_frame - time_at_last_frame);
			time_at_last_frame = time_this_frame;
		}

		if (alt) {
			// alt + arrow keys to scroll
			double scroll_speed = 20.0;
			double scroll_amount_x = scroll_speed * frame_dt * 1.5; // characters are taller than they are wide
			double scroll_amount_y = scroll_speed * frame_dt;
			if (keyboard_state[SDL_SCANCODE_UP])
				buffer_scroll(buffer, 0, -scroll_amount_y);
			if (keyboard_state[SDL_SCANCODE_DOWN])
				buffer_scroll(buffer, 0, +scroll_amount_y);
			if (keyboard_state[SDL_SCANCODE_LEFT])
				buffer_scroll(buffer, -scroll_amount_x, 0);
			if (keyboard_state[SDL_SCANCODE_RIGHT])
				buffer_scroll(buffer, +scroll_amount_x, 0);
		}
			

		int window_width = 0, window_height = 0;
		SDL_GetWindowSize(window, &window_width, &window_height);
		float window_widthf = (float)window_width, window_heightf = (float)window_height;

		// set up GL
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, window_width, window_height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		// pixel coordinates; down is positive y
		glOrtho(0, window_width, window_height, 0, -1, +1);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		{
			float x1 = 50, y1 = 50, x2 = window_widthf-50, y2 = window_heightf-50;
			buffer_render(buffer, x1, y1, x2, y2);
			if (text_has_err()) {
				debug_println("Text error: %s\n", text_get_err());
				break;
			}
		}

	#if DEBUG
		//buffer_print_debug(buffer);
		buffer_check_valid(buffer);
		buffer_print_undo_history(buffer);
	#endif

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glctx);
	SDL_DestroyWindow(window);
	SDL_Quit();
	buffer_free(buffer);
	text_font_free(font);

	return 0;
}
