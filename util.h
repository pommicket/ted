/// \file
/// C utility functions

#ifndef UTIL_H_
#define UTIL_H_

#include "base.h"

/// like snprintf, but not screwed up on old versions of windows
#define str_printf(str, size, ...) (str)[(size) - 1] = '\0', snprintf((str), (size) - 1, __VA_ARGS__)
/// like snprintf, but the size is taken to be the length of the array str.
///                              first, check that str is actually an array
#define strbuf_printf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_printf(str, sizeof str, __VA_ARGS__)
#define str_catf(str, size, ...) str_printf((str) + strlen(str), (size) - strlen(str), __VA_ARGS__)
#define strbuf_catf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_catf(str, sizeof str, __VA_ARGS__)
#define strbuf_cpy(dst, src) str_cpy(dst, sizeof dst, src)
#define strbuf_cat(dst, src) str_cat(dst, sizeof dst, src)


#define PIf 3.14159265358979f

typedef struct {
	float x, y;
} vec2;
typedef struct {
	float x, y, z, w;
} vec4;
typedef struct {
	double x, y;
} dvec2;

typedef struct {
	vec2 pos, size;
} Rect;

/// UTF-32 string
typedef struct {
	char32_t *str;
	size_t len;
} String32;

/// reference-counted string
typedef struct RcStr RcStr;

/// allocate new ref-counted string
///
/// if `len == -1`, `s` is assumed to be null-terminated. otherwise,
/// at most `len` bytes are copied from `s`.
RcStr *rc_str_new(const char *s, i64 len);
/// increases the reference count of `str`.
///
/// does nothing if `str` is `NULL`.
void rc_str_incref(RcStr *str);
/// increases reference count of `str` (if non-NULL) and returns it.
RcStr *rc_str_copy(RcStr *str);
/// decrease reference count of `*str` and set `*str` to `NULL`.
///
/// this frees `*str` if the reference count hits 0.
///
/// does nothing if `*str` is NULL.
void rc_str_decref(RcStr **str);
/// get rc string with default value if `s == NULL`
const char *rc_str(RcStr *s, const char *value_if_null);
/// `isword` for 32-bit chars.
bool is32_word(char32_t c);
/// `isspace` for 32-bit chars.
bool is32_space(char32_t c);
/// `isalpha` for 32-bit chars.
bool is32_alpha(char32_t c);
/// `isalnum` for 32-bit chars.
bool is32_alnum(char32_t c);
/// `isdigit` for 32-bit chars.
bool is32_digit(char32_t c);
/// `isgraph` for 32-bit chars.
bool is32_graph(char32_t c);
/// cross-platform `isatty`
bool is_a_tty(FILE *out);
/// returns terminal escape sequence for italics, or `""` if `out` is not a TTY.
const char *term_italics(FILE *out);
/// returns terminal escape sequence for bold, or `""` if `out` is not a TTY.
const char *term_bold(FILE *out);
/// returns terminal escape sequence for yellow, or `""` if `out` is not a TTY.
const char *term_yellow(FILE *out);
/// returns terminal escape sequence to clear, or `""` if `out` is not a TTY.
const char *term_clear(FILE *out);
/// number of 1 bits in x.
u8 util_popcount(u64 x);
/// count leading zeroes. if x == 0, this always returns 32 (not undefined behavior).
u8 util_count_leading_zeroes32(u32 x);
/// is x a power of 2?
bool util_is_power_of_2(u64 x);
/// like memchr, but 32-bit.
char32_t *util_mem32chr(char32_t *s, char32_t c, size_t n);
/// like memchr, but 32-bit, and constant.
const char32_t *util_mem32chr_const(const char32_t *s, char32_t c, size_t n);
/// does `str` start with `prefix`?
bool str_has_prefix(const char *str, const char *prefix);
/// does `str` end with `suffix`?
bool str_has_suffix(const char *str, const char *suffix);
/// like \ref str_has_prefix, but for paths. "ab/cd" is a path-prefix of "ab/cd/ef", but not "ab/cde".
/// also handles the fact that \ and / are the same on windows
bool str_has_path_prefix(const char *path, const char *prefix);
/// are these two strings equal?
bool streq(const char *a, const char *b);
/// equivalent to the POSIX function strnlen
size_t strn_len(const char *src, size_t n);
/// equivalent to the POSIX function strndup
char *strn_dup(const char *src, size_t n);
/// equivalent to the POSIX function strdup
char *str_dup(const char *src);
/// a safer version of strncat. `dst_sz` includes a null-terminator.
void strn_cat(char *dst, size_t dst_sz, const char *src, size_t src_len);
/// a safer version of strcat. `dst_sz` includes a null-terminator.
void str_cat(char *dst, size_t dst_sz, const char *src);
/// a safer version of strncpy. `dst_sz` includes a null-terminator.
void strn_cpy(char *dst, size_t dst_sz, const char *src, size_t src_len);
/// a safer version of strcpy. `dst_sz` includes a null-terminator.
void str_cpy(char *dst, size_t dst_sz, const char *src);
/// trim whitespace from the start of a string
void str_trim_start(char *str);
/// trim whitespace from the end of a string
void str_trim_end(char *str);
/// trim whitespace from both sides of a string
void str_trim(char *str);
/// convert ASCII to lowercase
void str_ascii_to_lowercase(char *str);
/// count occurences of `c` in `s`
size_t str_count_char(const char *s, char c);
/// equivalent to GNU function asprintf (like sprintf, but allocates the string with malloc).
char *a_sprintf(const char *fmt, ...);
/// convert binary number to string. make sure `s` can hold at least 65 bytes!!
void str_binary_number(char s[65], u64 n);
/// print some bytes. useful for debugging.
void print_bytes(const u8 *bytes, size_t n);
/// like strstr, but case-insensitive
/// currently this uses a "naive" string searching algorithm so
/// it may be O(len(haystack) * len(needle)) for certain strings.
char *strstr_case_insensitive(const char *haystack, const char *needle);
/// like strcmp, but case-insensitive
int strcmp_case_insensitive(const char *a, const char *b);
/// like streq, but case-insensitive
bool streq_case_insensitive(const char *a, const char *b);
/// function to be passed into qsort for case insensitive sorting
int str_qsort_case_insensitive_cmp(const void *av, const void *bv);
/// the actual file name part of the path; get rid of the containing directory.
///
/// NOTE: the returned string is part of path, so you don't need to free it or anything.
const char *path_filename(const char *path);
/// is this an absolute path?
bool path_is_absolute(const char *path);
/// cuts `path` off at last path separator
void path_dirname(char *path);
/// assuming `dir` is an absolute path, returns the absolute path of `relpath`, relative to `dir`.
void path_full(const char *dir, const char *relpath, char *abspath, size_t abspath_size);
/// returns true if the paths are the same.
///
/// handles the fact that paths are case insensitive on windows and that `\\` is the same as `/`.
/// a symbolic link is considered different from the file it points to, as are two hard
/// links to the same file.
bool paths_eq(const char *path1, const char *path2);
/// copy file from src to dest
/// returns true on success
bool copy_file(const char *src, const char *dst);
/// like qsort, but with a context object which gets passed to the comparison function
void qsort_with_context(void *base, size_t nmemb, size_t size,
	int (*compar)(void *, const void *, const void *),
	void *arg);
/// map x from the interval [0, 1] to the interval [a, b]. does NOT clamp.
///
/// note that the order is different from the usual convention because i like this order better.
float lerpf(float x, float a, float b);
/// opposite of lerp; map x from the interval [a, b] to the interval [0, 1]. does NOT clamp.
float normf(float x, float a, float b);
/// clamp `x` to the range [a, b].
float clampf(float x, float a, float b);
/// clamp `x` to the range [a, b].
double clampd(double x, double a, double b);
/// clamp `x` to the range [a, b].
int clampi(int x, int a, int b);
/// clamp `x` to the range [a, b].
i16 clamp_i16(i16 x, i16 a, i16 b);
/// clamp `x` to the range [a, b].
u16 clamp_u16(u16 x, u16 a, u16 b);
/// clamp `x` to the range [a, b].
i32 clamp_i32(i32 x, i32 a, i32 b);
/// clamp `x` to the range [a, b].
u32 clamp_u32(u32 x, u32 a, u32 b);
/// number of digits in the decimal representation `x`
u8 ndigits_u64(u64 x);
/// linearly remap `x` from the interval [`from_a`, `from_b`] to the interval [`to_a`, `to_b`]
float remapf(float x, float from_a, float from_b, float to_a, float to_b);
/// minimum of `a` and `b`
float minf(float a, float b);
float maxf(float a, float b);
double maxd(double a, double b);
/// minimum of `a` and `b`
double mind(double a, double b);
/// minimum of `a` and `b`
u32 min_u32(u32 a, u32 b);
/// maximum of `a` and `b`
u32 max_u32(u32 a, u32 b);
/// set `a` to the minimum of `a` and `b` and `b` to the maximum.
void sort2_u32(u32 *a, u32 *b);
/// minimum of `a` and `b`
i32 min_i32(i32 a, i32 b);
/// maximum of `a` and `b`
i32 max_i32(i32 a, i32 b);
/// minimum of `a` and `b`
u64 min_u64(u64 a, u64 b);
/// maximum of `a` and `b`
u64 max_u64(u64 a, u64 b);
/// minimum of `a` and `b`
i64 min_i64(i64 a, i64 b);
/// maximum of `a` and `b`
i64 max_i64(i64 a, i64 b);
/// returns `a` modulo `b`. for `b > 0`, this will be between `0` and `b - 1`.
///
/// if `b <= 0`, the return value is unspecified.
i64 mod_i64(i64 a, i64 b);
/// returns `a` modulo `b`. for `b > 0`, this will be between `0` and `b - 1`.
///
/// if `b <= 0`, the return value is unspecified.
i32 mod_i32(i32 a, i32 b);
/// absolute value
i64 abs_i64(i64 x);
/// `-1` if `x < 0`, `0` if `x == 0`, and `1` if `x > 0`
i64 sgn_i64(i64 x);
/// `-1` if `x < 0`, `0` if `x == 0`, and `1` if `x > 0`
float sgnf(float x);
vec2 vec2_add(vec2 a, vec2 b);
vec2 vec2_add_const(vec2 a, float c);
vec2 vec2_sub(vec2 a, vec2 b);
vec2 vec2_scale(vec2 v, float s);
vec2 vec2_mul(vec2 a, vec2 b);
vec2 vec2_clamp(vec2 x, vec2 a, vec2 b);
float vec2_dot(vec2 a, vec2 b);
float vec2_norm(vec2 v);
vec2 vec2_lerp(float x, vec2 a, vec2 b);
vec2 vec2_rotate(vec2 v, float theta);
vec2 vec2_normalize(vec2 v);
float vec2_distance(vec2 a, vec2 b);
void vec2_print(vec2 v);
vec2 vec2_polar(float r, float theta);
bool rect_contains_point_v2(vec2 pos, vec2 size, vec2 point);
bool centered_rect_contains_point(vec2 center, vec2 size, vec2 point);
Rect rect(vec2 pos, vec2 size);
Rect rect_endpoints(vec2 e1, vec2 e2);
Rect rect4(float x1, float y1, float x2, float y2);
Rect rect_xywh(float x, float y, float w, float h);
Rect rect_centered(vec2 center, vec2 size);
vec2 rect_center(Rect r);
bool rect_contains_point(Rect r, vec2 point);
void rect_coords(Rect r, float *x1, float *y1, float *x2, float *y2);
float rect_x1(Rect r);
float rect_y1(Rect r);
float rect_x2(Rect r);
float rect_y2(Rect r);
float rect_xmid(Rect r);
float rect_ymid(Rect r);
void rect_print(Rect r);
float rects_intersect(Rect r1, Rect r2);
bool rect_clip_to_rect(Rect *clipped, Rect clipper);
/// removes `amount` from all sides of r
void rect_shrink(Rect *r, float amount);
/// removes `amount` from the left side of r
void rect_shrink_left(Rect *r, float amount);
/// removes `amount` from the top side of r
void rect_shrink_top(Rect *r, float amount);
/// removes `amount` from the right side of r
void rect_shrink_right(Rect *r, float amount);
/// removes `amount` from the bottom side of r
void rect_shrink_bottom(Rect *r, float amount);
/// adds `amount` to all sides of r
void rect_grow(Rect *r, float amount);
int timespec_cmp(struct timespec a, struct timespec b);
bool timespec_eq(struct timespec a, struct timespec b);
struct timespec timespec_max(struct timespec a, struct timespec b);
double timespec_to_seconds(struct timespec ts);
String32 str32(char32_t *str, size_t len);
String32 str32_substr(String32 s, size_t from, size_t len);
void str32_free(String32 *s);
String32 str32_from_utf8(const char *utf8);
/// convert UTF-32 to UTF-8.
///
/// returns false on invalid UTF-32 (and `out` will be truncated to just
/// the prefix of `s` that is valid UTF-32)
///
/// out buffer must be long enough to fit the UTF-8 representation of `s`!
/// (`s.len * 4 + 1` is a safe buffer size)
bool str32_to_utf8_cstr_in_place(String32 s, char *out);
/// returns a null-terminated UTF-8 string
///
/// the string returned should be free'd
/// this will return NULL on failure
char *str32_to_utf8_cstr(String32 s);
int str32_cmp_ascii(String32 s, const char *ascii);
bool str32_has_ascii_prefix(String32 s, const char *ascii);
size_t str32chr(String32 s, char32_t c);
size_t str32_count_char(String32 s, char32_t c);
size_t str32_remove_all_instances_of_char(String32 *s, char32_t c);
size_t str32_ascii_spn(String32 s, const char *charset);

#endif // UTIL_H_
