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

#endif // UTIL_H_
