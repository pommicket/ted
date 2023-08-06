// various functions for dealing with OpenGL.
// also houses all of the basic rendering functions ted uses.

#include "ted-internal.h"
#include "lib/glcorearb.h"

float gl_window_width, gl_window_height;
int gl_version_major, gl_version_minor;


#if DEBUG
unsigned char *stbi_load(const char *filename, int *x, int *y, int *comp, int req_comp);
#else
#define STB_IMAGE_STATIC
no_warn_start
#include "stb_image.c"
no_warn_end
#endif


#define gl_define_proc(upper, lower) PFNGL##upper##PROC gl##lower;
gl_for_each_proc(gl_define_proc)
#undef gl_define_proc

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

GlRcTexture *gl_rc_texture_new(GLuint texture) {
	GlRcTexture *t = calloc(1, sizeof *t);
	t->ref_count = 1;
	t->texture = texture;
	return t;
}

void gl_rc_texture_incref(GlRcTexture *t) {
	if (!t) return;
	++t->ref_count;
}

void gl_rc_texture_decref(GlRcTexture **pt) {
	GlRcTexture *t = *pt;
	if (!t) return;
	if (--t->ref_count == 0) {
		glDeleteTextures(1, &t->texture);
		free(t);
	}
	*pt = NULL;
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
GLuint gl_link_program(char error_buf[256], GLuint *shaders, size_t count) {
	GLuint program = glCreateProgram();
	if (program) {
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
	}
	return program;
}

GLuint gl_compile_and_link_shaders(char error_buf[256], const char *vshader_code, const char *fshader_code) {
	GLuint shaders[2];
	shaders[0] = gl_compile_shader(error_buf, vshader_code, GL_VERTEX_SHADER);
	shaders[1] = gl_compile_shader(error_buf, fshader_code, GL_FRAGMENT_SHADER);
	GLuint program = gl_link_program(error_buf, shaders, 2);
	if (shaders[0]) glDeleteShader(shaders[0]);
	if (shaders[1]) glDeleteShader(shaders[1]);
	if (program) {
		debug_print("Successfully linked program %u.\n", program);
	}
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

void gl_geometry_init(void) {
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
	vec4 color = rgba_u32_to_vec4(color_rgba);

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

GLuint gl_load_texture_from_image(const char *path) {
	GLuint texture = 0;
	int w=0, h=0, n=0;
	unsigned char *data = stbi_load(path, &w, &h, &n, 4);
	if (data && w > 0 && h > 0) {
		size_t bytes_per_row = (size_t)w * 4;
		unsigned char *tempbuf = calloc(1, bytes_per_row);
		// flip image vertically
		for (int y = 0; y < h/2; ++y) {
			int y2 = h-1-y;
			unsigned char *row1 = &data[y*w*4];
			unsigned char *row2 = &data[y2*w*4];
			memcpy(tempbuf, row1, bytes_per_row);
			memcpy(row1, row2, bytes_per_row);
			memcpy(row2, tempbuf, bytes_per_row);
		}
		free(tempbuf);
		
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		free(data);
	}
	return texture;
}
