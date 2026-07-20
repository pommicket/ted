// various functions for dealing with OpenGL.
// also houses all of the basic rendering functions ted uses.

#include "ted-internal.h"

float gl_window_width, gl_window_height;
int gl_version_major, gl_version_minor;

static SDL_GLContext glctx = NULL;

#define gl_define_proc(upper, lower) PFNGL##upper##PROC gl##lower;
gl_for_each_proc(gl_define_proc)
#undef gl_define_proc

#if DEBUG
static void APIENTRY gl_message_callback(GLenum source, GLenum type, unsigned int id, GLenum severity,
	GLsizei length, const char *message, const void *userParam) {
	(void)source; (void)type; (void)id; (void)length; (void)userParam;
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
	debug_println("Message from OpenGL: %s.", message);
}
#endif

GlRcSAB *gl_rc_sab_new(GLuint shader, GLuint array, GLuint buffer) {
	GlRcSAB *s = calloc(1, sizeof *s);
	s->ref_count = 1;
	s->shader = shader;
	s->array = array;
	s->buffer = buffer;
	return s;
}

void gl_rc_sab_incref(GlRcSAB *s) {
	if (!s) return;
	++s->ref_count;
}

void gl_rc_sab_decref(GlRcSAB **ps) {
	GlRcSAB *s = *ps;
	if (!s) return;
	if (--s->ref_count == 0) {
		debug_println("Delete program %u", s->shader);
		glDeleteProgram(s->shader);
		glDeleteBuffers(1, &s->buffer);
		glDeleteVertexArrays(1, &s->array);
		free(s);
	}
	*ps = NULL;
}

void gl_get_procs(void) {
	#define gl_get_proc(upper, lower) gl##lower = (PFNGL##upper##PROC)SDL_GL_GetProcAddress("gl" #lower);
#if __GNUC__ && !__clang__
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wpedantic"
#endif
	gl_for_each_proc(gl_get_proc)
#if __GNUC__ && !__clang__
	#pragma GCC diagnostic pop
#endif
	#undef gl_get_proc
}

static int glsl_version(void) {
	int v = gl_version_major * 100 + gl_version_minor * 10;
	switch (v) {
	case 200: return 110;
	case 210: return 120;
	case 300: return 130;
	case 310: return 140;
	}
	// not going any later than GLSL 150 since
	// then we don't have gl_FragColor
	return 150;
}

// compile a GLSL shader
GLuint gl_compile_shader(char error_buf[256], const char *code, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);
	char header[128];
	int glsl = glsl_version();
	strbuf_printf(header, "#version %u\n\
#define IN %s\n\
#define OUT %s\n\
#line 1\n", glsl, glsl >= 130 ? "in" : "varying", glsl >= 130 ? "out" : "varying");
	const char *sources[2] = {
		header,
		code
	};
	glShaderSource(shader, 2, sources, NULL);
	glCompileShader(shader);
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		char log[1024] = {0};
		glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
		if (error_buf) {
			str_printf(error_buf, 256, "Error compiling shader: %s", log);
		} else {
			debug_println("Error compiling shader: %s", log);
		}
		return 0;
	}
	return shader;
}

// link together GL shaders
GLuint gl_link_program(char error_buf[256], const GLuint *shaders, size_t count) {
	GLuint program = glCreateProgram();
	if (!program) return 0;
	for (size_t i = 0; i < count; ++i) {
		if (!shaders[i]) {
			glDeleteProgram(program);
			return 0;
		}
		glAttachShader(program, shaders[i]);
	}
	glLinkProgram(program);
	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		char log[1024] = {0};
		glGetProgramInfoLog(program, sizeof log - 1, NULL, log);
		if (error_buf) {
			str_printf(error_buf, 256, "Error linking shaders: %s", log);
		} else {
			debug_println("Error linking shaders: %s", log);
		}
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint gl_compile_and_link_shaders(char error_buf[256], const char *vshader_code, const char *fshader_code) {
	GLuint shaders[2];
	shaders[0] = gl_compile_shader(error_buf, vshader_code, GL_VERTEX_SHADER);
	shaders[1] = gl_compile_shader(error_buf, fshader_code, GL_FRAGMENT_SHADER);
	GLuint program = gl_link_program(error_buf, shaders, 2);
	if (program) {
		glDetachShader(program, shaders[0]);
		glDetachShader(program, shaders[1]);
	}
	if (shaders[0]) glDeleteShader(shaders[0]);
	if (shaders[1]) glDeleteShader(shaders[1]);
	return program;
}

GLuint gl_attrib_location(GLuint program, const char *attrib) {
	GLint loc = glGetAttribLocation(program, attrib);
	if (loc == -1) {
		debug_print("Couldn't find vertex attribute %s.\n", attrib);
		return 0;
	}
	return (GLuint)loc;
}

GLint gl_uniform_location(GLuint program, const char *uniform) {
	GLint loc = glGetUniformLocation(program, uniform);
	if (loc == -1) {
		debug_print("Couldn't find uniform: %s.\n", uniform);
		return -1;
	}
	return loc;
}

typedef struct {
	vec2 pos;
	vec4 color;
} GLSimpleVertex;
typedef struct {
	GLSimpleVertex vert1, vert2, vert3;
} GLSimpleTriangle;

static GLSimpleTriangle *gl_geometry_triangles;
static GLuint gl_geometry_program;
static GLuint gl_geometry_v_pos;
static GLuint gl_geometry_v_color;
static GLint gl_geometry_u_window_size;
static GLuint gl_geometry_vbo, gl_geometry_vao;

static void geometry_init(void) {
	const char *vshader_code = "attribute vec2 v_pos;\n\
	attribute vec4 v_color;\n\
	uniform vec2 u_window_size;\n\
	OUT vec4 color;\n\
	void main() {\n\
		vec2 p = v_pos * (2.0 / u_window_size);\n\
		gl_Position = vec4(p.x - 1.0, 1.0 - p.y, 0.0, 1.0);\n\
		color = v_color;\n\
	}\n\
	";
	const char *fshader_code = "IN vec4 color;\n\
	void main() {\n\
		gl_FragColor = color;\n\
	}\n\
	";

	gl_geometry_program = gl_compile_and_link_shaders(NULL, vshader_code, fshader_code);
	gl_geometry_v_pos = gl_attrib_location(gl_geometry_program, "v_pos");
	gl_geometry_v_color = gl_attrib_location(gl_geometry_program, "v_color");
	gl_geometry_u_window_size = gl_uniform_location(gl_geometry_program, "u_window_size");

	glGenBuffers(1, &gl_geometry_vbo);
	if (gl_version_major >= 3)
		glGenVertexArrays(1, &gl_geometry_vao);
}

void gl_geometry_rect(Rect r, u32 color_rgba) {
	if (r.size.x <= 0 || r.size.y <= 0)
		return;
	vec4 color = color_u32_to_vec4(color_rgba);

	vec2 p1 = {rect_x1(r), rect_y1(r)};
	vec2 p2 = {rect_x1(r), rect_y2(r)};
	vec2 p3 = {rect_x2(r), rect_y2(r)};
	vec2 p4 = {rect_x2(r), rect_y1(r)};

	GLSimpleTriangle triangle = {
		{p1, color},
		{p2, color},
		{p3, color}
	};
	arr_add(gl_geometry_triangles, triangle);
	triangle.vert1.pos = p3;
	triangle.vert2.pos = p4;
	triangle.vert3.pos = p1;
	arr_add(gl_geometry_triangles, triangle);
}

void gl_geometry_rect_border(Rect r, float border_thickness, u32 color) {
	float x1 = r.pos.x, y1 = r.pos.y, x2 = x1 + r.size.x, y2 = y1 + r.size.y;
	
	// make sure rectangle isn't too small
	x2 = maxf(x2, x1 + border_thickness);
	y2 = maxf(y2, y1 + border_thickness);

	gl_geometry_rect(rect4(x1+border_thickness, y1, x2, y1+border_thickness), color);
	gl_geometry_rect(rect4(x1, y2-border_thickness, x2-border_thickness, y2), color);
	gl_geometry_rect(rect4(x1, y1, x1+border_thickness, y2), color);
	gl_geometry_rect(rect4(x2-border_thickness, y1+border_thickness, x2, y2), color);
}

void gl_geometry_draw(void) {
	size_t ntriangles = arr_len(gl_geometry_triangles);
	
	if (ntriangles == 0) return;

	if (gl_version_major >= 3)
		glBindVertexArray(gl_geometry_vao);

	glBindBuffer(GL_ARRAY_BUFFER, gl_geometry_vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ntriangles * sizeof(GLSimpleTriangle)), gl_geometry_triangles, GL_STREAM_DRAW);
	glVertexAttribPointer(gl_geometry_v_pos,   2, GL_FLOAT, 0, sizeof(GLSimpleVertex), (void *)offsetof(GLSimpleVertex, pos));
	glEnableVertexAttribArray(gl_geometry_v_pos);
	glVertexAttribPointer(gl_geometry_v_color, 4, GL_FLOAT, 0, sizeof(GLSimpleVertex), (void *)offsetof(GLSimpleVertex, color));
	glEnableVertexAttribArray(gl_geometry_v_color);

	glUseProgram(gl_geometry_program);
	glUniform2f(gl_geometry_u_window_size, gl_window_width, gl_window_height);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(3 * ntriangles));
	
	arr_clear(gl_geometry_triangles);
}

void gl_init(SDL_Window *window) {
	// get OpenGL context
	const int gl_versions[][2] = {
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
	geometry_init();
}

void gl_quit(void) {
	if (gl_geometry_program) {
		glDeleteProgram(gl_geometry_program);
	}
	if (glDeleteBuffers) {
		glDeleteBuffers(1, &gl_geometry_vbo);
		glDeleteVertexArrays(1, &gl_geometry_vao);
	}
	if (glctx) {
		SDL_GL_DestroyContext(glctx);
	}
	// Set all OpenGL function pointers to NULL, so we know if we
	// accidentally call one after quitting.
	#define gl_set_proc_to_null(upper, lower) gl##lower = NULL;
	gl_for_each_proc(gl_set_proc_to_null)
	#undef gl_set_proc_to_null
}
