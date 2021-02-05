#include "lib/glcorearb.h"

// macro trickery to avoid having to write everything twice
#define gl_for_each_proc(do)\
	do(CREATESHADER, CreateShader)\
	do(DELETESHADER, DeleteShader)\
	do(CREATEPROGRAM, CreateProgram)\
	do(SHADERSOURCE, ShaderSource)\
	do(GETSHADERIV, GetShaderiv)\
	do(GETSHADERINFOLOG, GetShaderInfoLog)\
	do(COMPILESHADER, CompileShader)\
	do(CREATEPROGRAM, CreateProgram)\
	do(DELETEPROGRAM, DeleteProgram)\
	do(ATTACHSHADER, AttachShader)\
	do(LINKPROGRAM, LinkProgram)\
	do(GETPROGRAMIV, GetProgramiv)\
	do(GETPROGRAMINFOLOG, GetProgramInfoLog)\
	do(USEPROGRAM, UseProgram)\
	do(GETATTRIBLOCATION, GetAttribLocation)\
	do(GETUNIFORMLOCATION, GetUniformLocation)\
	do(GENBUFFERS, GenBuffers)\
	do(DELETEBUFFERS, DeleteBuffers)\
	do(BINDBUFFER, BindBuffer)\
	do(BUFFERDATA, BufferData)\
	do(VERTEXATTRIBPOINTER, VertexAttribPointer)\
	do(ENABLEVERTEXATTRIBARRAY, EnableVertexAttribArray)\
	do(DISABLEVERTEXATTRIBARRAY, DisableVertexAttribArray)\
	do(GENVERTEXARRAYS, GenVertexArrays)\
	do(DELETEVERTEXARRAYS, DeleteVertexArrays)\
	do(BINDVERTEXARRAY, BindVertexArray)\
	do(UNIFORM1F, Uniform1f)\
	do(UNIFORM2F, Uniform2f)\
	do(UNIFORM3F, Uniform3f)\
	do(UNIFORM4F, Uniform4f)\
	do(UNIFORM1I, Uniform1i)\
	do(UNIFORM2I, Uniform2i)\
	do(UNIFORM3I, Uniform3i)\
	do(UNIFORM4I, Uniform4i)\
	do(UNIFORMMATRIX4FV, UniformMatrix4fv)\
	do(DEBUGMESSAGECALLBACK, DebugMessageCallback)\
	do(DEBUGMESSAGECONTROL, DebugMessageControl)\

#define gl_declare_proc(upper, lower) static PFNGL##upper##PROC gl##lower;
gl_for_each_proc(gl_declare_proc)
#undef gl_declare_proc

// set by main()
static int gl_version_major;
static int gl_version_minor;

static void gl_get_procs(void) {
	#define gl_get_proc(upper, lower) gl##lower = (PFNGL##upper##PROC)SDL_GL_GetProcAddress("gl" #lower);
	gl_for_each_proc(gl_get_proc)
	#undef gl_get_proc
}

// compile a GLSL shader
static GLuint gl_compile_shader(char const *code, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &code, NULL);
	glCompileShader(shader);
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		char log[1024] = {0};
		glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
		debug_println("Error compiling shader: %s", log);
		return 0;
	}
	return shader;
}

// link together GL shaders
static GLuint gl_link_program(GLuint *shaders, size_t count) {
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
			debug_println("Error linking shaders: %s", log);
			glDeleteProgram(program);
			return 0;
		}
	}
	return program;
}

static GLuint gl_compile_and_link_shaders(char const *vshader_code, char const *fshader_code) {
	GLuint shaders[2];
	shaders[0] = gl_compile_shader(vshader_code, GL_VERTEX_SHADER);
	shaders[1] = gl_compile_shader(fshader_code, GL_FRAGMENT_SHADER);
	GLuint program = gl_link_program(shaders, 2);
	if (shaders[0]) glDeleteShader(shaders[0]);
	if (shaders[1]) glDeleteShader(shaders[1]);
	if (program) {
		debug_print("Successfully linked program %u.\n", program);
	}
	return program;
}

static GLuint gl_attrib_loc(GLuint program, char const *attrib) {
	GLint loc = glGetAttribLocation(program, attrib);
	if (loc == -1) {
		debug_print("Couldn't find vertex attribute %s.\n", attrib);
		return 0;
	}
	return (GLuint)loc;
}

static GLint gl_uniform_loc(GLuint program, char const *uniform) {
	GLint loc = glGetUniformLocation(program, uniform);
	if (loc == -1) {
		debug_print("Couldn't find uniform: %s.\n", uniform);
		return -1;
	}
	return loc;
}

typedef struct {
	v2 pos;
	v4 color;
} GLSimpleVertex;
typedef struct {
	GLSimpleVertex v1, v2, v3;
} GLSimpleTriangle;

static GLSimpleTriangle *gl_geometry_triangles;
static GLuint gl_geometry_program;
static GLuint gl_geometry_v_pos;
static GLuint gl_geometry_v_color;
static GLint  gl_geometry_u_window_size;
static GLuint gl_geometry_vbo, gl_geometry_vao;

static void gl_geometry_init(void) {
	char const *vshader_code = "#version 110\n\
	attribute vec2 v_pos;\n\
	attribute vec4 v_color;\n\
	varying vec4 color;\n\
	uniform vec2 window_size;\n\
	void main() {\n\
		float x = v_pos.x / window_size.x * 2.0 - 1.0;\n\
		float y = 1.0 - v_pos.y / window_size.y * 2.0;\n\
		gl_Position = vec4(x, y, 0.0, 1.0);\n\
		color = v_color;\n\
	}\n\
	";
	char const *fshader_code = "#version 110\n\
	varying vec4 color;\n\
	void main() {\n\
		gl_FragColor = color;\n\
	}\n\
	";

	gl_geometry_program = gl_compile_and_link_shaders(vshader_code, fshader_code);
	gl_geometry_v_pos = gl_attrib_loc(gl_geometry_program, "v_pos");
	gl_geometry_v_color = gl_attrib_loc(gl_geometry_program, "v_color");
	gl_geometry_u_window_size = gl_uniform_loc(gl_geometry_program, "window_size");

	glGenBuffers(1, &gl_geometry_vbo);
	if (gl_version_major >= 3)
		glGenVertexArrays(1, &gl_geometry_vao);
}

static void gl_geometry_rect(Rect r, u32 color_rgba) {
	v4 color = rgba_u32_to_v4(color_rgba);

	v2 p1 = r.pos;
	v2 p2 = v2_add(r.pos, V2(0, r.size.y));
	v2 p3 = v2_add(r.pos, V2(r.size.x, r.size.y));
	v2 p4 = p3;
	v2 p5 = v2_add(r.pos, V2(r.size.x, 0));
	v2 p6 = p1;
	GLSimpleTriangle triangle = {
		{p1, color},
		{p2, color},
		{p3, color}
	};
	arr_add(gl_geometry_triangles, triangle);
	triangle.v1.pos = p4;
	triangle.v2.pos = p5;
	triangle.v3.pos = p6;
	arr_add(gl_geometry_triangles, triangle);
}

static void gl_geometry_draw(Ted *ted) {
	size_t ntriangles = arr_len(gl_geometry_triangles);
	
	if (gl_version_major >= 3)
		glBindVertexArray(gl_geometry_vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl_geometry_vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ntriangles * sizeof(GLSimpleTriangle)), gl_geometry_triangles, GL_STREAM_DRAW);
	glVertexAttribPointer(gl_geometry_v_pos,   2, GL_FLOAT, 0, sizeof(GLSimpleVertex), (void *)offsetof(GLSimpleVertex, pos));
	glEnableVertexAttribArray(gl_geometry_v_pos);
	glVertexAttribPointer(gl_geometry_v_color, 4, GL_FLOAT, 0, sizeof(GLSimpleVertex), (void *)offsetof(GLSimpleVertex, color));
	glEnableVertexAttribArray(gl_geometry_v_color);

	glUseProgram(gl_geometry_program);
	glUniform2f(gl_geometry_u_window_size, ted->window_width, ted->window_height);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(3 * ntriangles));
	
	arr_clear(gl_geometry_triangles);
}
