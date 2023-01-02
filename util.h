#ifndef UTIL_H_
#define UTIL_H_


// like snprintf, but not screwed up on windows
#define str_printf(str, size, ...) (str)[(size) - 1] = '\0', snprintf((str), (size) - 1, __VA_ARGS__)
// like snprintf, but the size is taken to be the length of the array str.
//                              first, check that str is actually an array
#define strbuf_printf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_printf(str, sizeof str, __VA_ARGS__)
#define str_catf(str, size, ...) str_printf((str) + strlen(str), (size) - strlen(str), __VA_ARGS__)
#define strbuf_catf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_catf(str, sizeof str, __VA_ARGS__)
#define strbuf_cpy(dst, src) str_cpy(dst, sizeof dst, src)
#define strbuf_cat(dst, src) str_cat(dst, sizeof dst, src)


#define PIf 3.14159265358979f
#define HALF_PIf 1.5707963267948966f
#define TAUf 6.283185307179586f
#define SQRT2f 1.4142135623730951f
#define HALF_SQRT2f 0.7071067811865476f
#define SQRT3f 1.7320508075688772f
#define HALF_SQRT3f 0.8660254037844386f


typedef struct {
	float x, y;
} v2;
typedef struct {
	float x, y, z;
} v3;
typedef struct {
	float x, y, z, w;
} v4;
typedef struct {
	double x, y;
} v2d;
typedef struct {
	int x, y;
} v2i;

// matrices are column-major, because that's what they are in OpenGL 
typedef struct {
	float e[16];
} m4;

static const m4 m4_identity = {{
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
}};

typedef struct {
	v2 pos, size;
} Rect;


// ctype functions for 32-bit chars.
bool is_word(char32_t c);
bool is_digit(char32_t c);
bool is_space(char32_t c);
bool is_a_tty(FILE *out);
// terminal colors. if `out` is a TTY, these will return the appropriate escape sequences.
// if `out` is not a TTY, these will return "".
const char *term_italics(FILE *out);
const char *term_bold(FILE *out);
const char *term_yellow(FILE *out);
const char *term_clear(FILE *out);
// number of 1 bits in x.
u8 util_popcount(u64 x);
// count leading zeroes. if x == 0, this always returns 32 (not undefined behavior).
u8 util_count_leading_zeroes32(u32 x);
// is x a power of 2?
bool util_is_power_of_2(u64 x);
// like memchr, but 32-bit.
char32_t *util_mem32chr(char32_t *s, char32_t c, size_t n);
// like memchr, but 32-bit, and constant.
const char32_t *util_mem32chr_const(const char32_t *s, char32_t c, size_t n);
// does `str` have this prefix?
bool str_has_prefix(const char *str, const char *prefix);
// like str_has_prefix, but for paths. "ab/cd" is a path-prefix of "ab/cd/ef", but not "ab/cde".
bool str_has_path_prefix(const char *path, const char *prefix);
// are these two strings equal?
bool streq(const char *a, const char *b);
// equivalent to the POSIX function strnlen
size_t strn_len(const char *src, size_t n);
// equivalent to the POSIX function strndup
char *strn_dup(const char *src, size_t n);
// equivalent to the POSIX function strdup
char *str_dup(const char *src);
// a safer version of strncat. `dst_sz` includes a null-terminator.
void strn_cat(char *dst, size_t dst_sz, const char *src, size_t src_len);
// a safer version of strcat. `dst_sz` includes a null-terminator.
void str_cat(char *dst, size_t dst_sz, const char *src);
// a safer version of strncpy. `dst_sz` includes a null-terminator.
void strn_cpy(char *dst, size_t dst_sz, const char *src, size_t src_len);
// a safer version of strcpy. `dst_sz` includes a null-terminator.
void str_cpy(char *dst, size_t dst_sz, const char *src);
// equivalent to GNU function asprintf (like sprintf, but allocates the string with malloc).
char *a_sprintf(const char *fmt, ...);
// print some bytes. useful for debugging.
void print_bytes(const u8 *bytes, size_t n);
// like strstr, but case-insensitive
// currently this uses a "naive" string searching algorithm so
// it may be O(len(haystack) * len(needle)) for certain strings.
char *strstr_case_insensitive(const char *haystack, const char *needle);
// like strcmp, but case-insensitive
int strcmp_case_insensitive(const char *a, const char *b);
// function to be passed into qsort for case insensitive sorting
int str_qsort_case_insensitive_cmp(const void *av, const void *bv);
// the actual file name part of the path; get rid of the containing directory.
// NOTE: the returned string is part of path, so you don't need to free it or anything.
const char *path_filename(const char *path);
// is this an absolute path?
bool path_is_absolute(const char *path);
// assuming `dir` is an absolute path, returns the absolute path of `relpath`, relative to `dir`.
void path_full(const char *dir, const char *relpath, char *abspath, size_t abspath_size);
// returns true if the paths are the same.
// handles the fact that paths are case insensitive on windows and that \ is the same as /.
// a symbolic link is considered different from the file it points to, as are two hard
// links to the same file.
bool paths_eq(const char *path1, const char *path2);
// equivalent to POSIX function chdir.
void change_directory(const char *path);
// copy file from src to dest
// returns true on success
bool copy_file(const char *src, const char *dst);
// like qsort, but with a context object which gets passed to the comparison function
void qsort_with_context(void *base, size_t nmemb, size_t size,
	int (*compar)(void *, const void *, const void *),
	void *arg);
float degrees(float r);
float radians(float r);
float lerpf(float x, float a, float b);
float normf(float x, float a, float b);
float clampf(float x, float a, float b);
int clampi(int x, int a, int b);
i16 clamp_i16(i16 x, i16 a, i16 b);
u16 clamp_u16(u16 x, u16 a, u16 b);
i32 clamp_i32(i32 x, i32 a, i32 b);
u32 clamp_u32(u32 x, u32 a, u32 b);
u8 ndigits_u64(u64 x);
float remapf(float x, float from_a, float from_b, float to_a, float to_b);
float minf(float a, float b);
float maxf(float a, float b);
double maxd(double a, double b);
double mind(double a, double b);
u32 min_u32(u32 a, u32 b);
u32 max_u32(u32 a, u32 b);
void sort2_u32(u32 *a, u32 *b);
i32 min_i32(i32 a, i32 b);
i32 max_i32(i32 a, i32 b);
u64 min_u64(u64 a, u64 b);
u64 max_u64(u64 a, u64 b);
i64 min_i64(i64 a, i64 b);
i64 max_i64(i64 a, i64 b);
i64 mod_i64(i64 a, i64 b);
i32 mod_i32(i32 a, i32 b);
i64 abs_i64(i64 x);
i64 sgn_i64(i64 x);
float sgnf(float x);
float smoothstepf(float x);
float randf(void);
float rand_gauss(void);
u32 rand_u32(void);
float rand_uniform(float from, float to);
float sigmoidf(float x);
i32 ceildivi32(i32 x, i32 y);
v2 V2(float x, float y);
v2 v2_add(v2 a, v2 b);
v2 v2_add_const(v2 a, float c);
v2 v2_sub(v2 a, v2 b);
v2 v2_scale(v2 v, float s);
v2 v2_mul(v2 a, v2 b);
v2 v2_clamp(v2 x, v2 a, v2 b);
float v2_dot(v2 a, v2 b);
float v2_len(v2 v);
v2 v2_lerp(float x, v2 a, v2 b);
v2 v2_rotate(v2 v, float theta);
v2 v2_normalize(v2 v);
float v2_dist(v2 a, v2 b);
float v2_dist_squared(v2 a, v2 b);
void v2_print(v2 v);
v2 v2_rand_unit(void);
v2 v2_polar(float r, float theta);
v3 V3(float x, float y, float z);
v3 v3_from_v2(v2 v);
v3 v3_add(v3 a, v3 b);
v3 v3_sub(v3 a, v3 b);
v3 v3_scale(v3 v, float s);
v3 v3_lerp(float x, v3 a, v3 b);
float v3_dot(v3 u, v3 v);
v3 v3_cross(v3 u, v3 v);
float v3_len(v3 v);
float v3_dist(v3 a, v3 b);
float v3_dist_squared(v3 a, v3 b);
v3 v3_normalize(v3 v);
v2 v3_xy(v3 v);
v3 v3_on_sphere(float yaw, float pitch);
void v3_print(v3 v);
v3 v3_rand(void);
v3 v3_rand_unit(void);
v4 V4(float x, float y, float z, float w);
v4 v4_add(v4 a, v4 b);
v4 v4_sub(v4 a, v4 b);
v4 v4_scale(v4 v, float s);
v4 v4_scale_xyz(v4 v, float s);
v4 v4_lerp(float x, v4 a, v4 b);
float v4_dot(v4 u, v4 v);
v4 v4_mul(v4 u, v4 v);
float v4_len(v4 v);
v4 v4_normalize(v4 v);
v3 v4_xyz(v4 v);
v4 v4_rand(void);
void v4_print(v4 v);
v2d V2D(double x, double y);
void m4_print(m4 m);
m4 m4_yaw(float yaw);
m4 m4_pitch(float pitch);
m4 m4_translate(v3 t);
v3 m4_mul_v3(m4 m, v3 v);
m4 m4_perspective(float fov, float aspect, float z_near, float z_far);
m4 m4_ortho(float left, float right, float bottom, float top, float near_, float far_);
m4 m4_mul(m4 a, m4 b);
m4 m4_inv(m4 mat);
v2i V2I(int x, int y);
void rgba_u32_to_floats(u32 rgba, float floats[4]);
v4 rgba_u32_to_v4(u32 rgba);
u32 rgba_v4_to_u32(v4 color);
float rgba_brightness(u32 color);
bool rect_contains_point_v2(v2 pos, v2 size, v2 point);
bool centered_rect_contains_point(v2 center, v2 size, v2 point);
Rect rect(v2 pos, v2 size);
Rect rect_endpoints(v2 e1, v2 e2);
Rect rect4(float x1, float y1, float x2, float y2);
Rect rect_xywh(float x, float y, float w, float h);
Rect rect_centered(v2 center, v2 size);
v2 rect_center(Rect r);
bool rect_contains_point(Rect r, v2 point);
Rect rect_translate(Rect r, v2 by);
void rect_coords(Rect r, float *x1, float *y1, float *x2, float *y2);
void rect_print(Rect r);
float rects_intersect(Rect r1, Rect r2);
bool rect_clip_to_rect(Rect *clipped, Rect clipper);
Rect rect_shrink(Rect r, float amount);
Rect rect_grow(Rect r, float amount);
v4 color_rgba_to_hsva(v4 rgba);
v4 color_hsva_to_rgba(v4 hsva);
u32 color_interpolate(float x, u32 color1, u32 color2);

#endif // UTIL_H_
