#include "base.h"
#include "util.h"
#include "unicode.h"

#if _WIN32
#include <intrin.h>
#include <direct.h>
#include <io.h>
#elif __unix__
#include <unistd.h>
#else
#error "Unrecognized operating system."
#endif
#include <wctype.h>
#include <ctype.h>

// on 16-bit systems, this is 16383. on 32/64-bit systems, this is 1073741823
// it is unusual to have a string that long.
#define STRLEN_SAFE_MAX (UINT_MAX >> 2)
struct RcStr {
	u32 ref_count;
	char str[];
};

RcStr *rc_str_new(const char *s, i64 len) {
	if (len < 0) {
		len = (i64)strlen(s);
	}
	RcStr *rc = calloc(1, sizeof(RcStr) + (size_t)len + 1);
	assert(rc);
	memcpy(rc->str, s, (size_t)len);
	rc->ref_count = 1;
	return rc;
}

void rc_str_incref(RcStr *str) {
	if (str)
		str->ref_count += 1;
}


RcStr *rc_str_copy(RcStr *str) {
	rc_str_incref(str);
	return str;
}

void rc_str_decref(RcStr **pstr) {
	RcStr *const str = *pstr;
	if (!str) return;
	str->ref_count -= 1;
	if (str->ref_count == 0) {
		*pstr = NULL;
		free(str);
	}
}

const char *rc_str(RcStr *s, const char *default_value) {
	if (!s) return default_value;
	assert(s->ref_count > 0);
	return s->str;
}

size_t rc_str_len(RcStr *s) {
	return strlen(rc_str(s, ""));
}

bool is32_word(char32_t c) {
	return c > WCHAR_MAX || c == '_' || iswalnum((wint_t)c);
}

bool is32_space(char32_t c) {
	return c <= WINT_MAX && iswspace((wint_t)c);
}

bool is32_alpha(char32_t c) {
	return c <= WINT_MAX && iswalpha((wint_t)c);
}

bool is32_alnum(char32_t c) {
	return c <= WINT_MAX && iswalnum((wint_t)c);
}

bool is32_digit(char32_t c) {
	return c <= WINT_MAX && iswdigit((wint_t)c);
}

bool is32_graph(char32_t c) {
	return c <= WINT_MAX && iswgraph((wint_t)c);
}

bool is_a_tty(FILE *out) {
	#if _WIN32
	int fd = _fileno(out);
	return fd >= 0 && _isatty(fd);
	#else
	int fd = fileno(out);
	return fd >= 0 && isatty(fd);
	#endif
}

const char *term_italics(FILE *out) {
	return is_a_tty(out) ? "\x1b[3m" : "";
}

const char *term_bold(FILE *out) {
	return is_a_tty(out) ? "\x1b[1m" : "";
}

const char *term_yellow(FILE *out) {
	return is_a_tty(out) ? "\x1b[93m" : "";
}

const char *term_clear(FILE *out) {
	return is_a_tty(out) ? "\x1b[0m" : "";
}


u8 util_popcount(u64 x) {
#ifdef __GNUC__
	return (u8)__builtin_popcountll(x);
#else
	u8 count = 0;
	while (x) {
		x &= x-1;
		++count;
	}
	return count;
#endif
}

u8 util_count_leading_zeroes32(u32 x) {
	if (x == 0) return 32; // GCC's __builtin_clz is undefined for x = 0
#if __GNUC__ && UINT_MAX == 4294967295
	return (u8)__builtin_clz(x);
#elif _WIN32 && UINT_MAX == 4294967295
	return (u8)__lzcnt(x);
#else
	u8 count = 0;
	for (int i = 31; i >= 0; --i) {
		if (x & ((u32)1<<i)) {
			break;
		}
		++count;
	}
	return count;
#endif
}

bool util_is_power_of_2(u64 x) {
	return util_popcount(x) == 1;
}

// for finding a character in a char32 string
char32_t *util_mem32chr(char32_t *s, char32_t c, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (s[i] == c) {
			return &s[i];
		}
	}
	return NULL;
}

const char32_t *util_mem32chr_const(const char32_t *s, char32_t c, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (s[i] == c) {
			return &s[i];
		}
	}
	return NULL;
}

bool str_has_prefix(const char *str, const char *prefix) {
	size_t prefix_len = strlen(prefix);
	size_t str_len = strlen(str);
	if (str_len < prefix_len)
		return false;
	return memcmp(str, prefix, prefix_len) == 0;
}

bool str_has_suffix(const char *str, const char *suffix) {
	size_t suffix_len = strlen(suffix);
	size_t str_len = strlen(str);
	if (str_len < suffix_len)
		return false;
	return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

bool str_has_path_prefix(const char *path, const char *prefix) {
	size_t prefix_len = strlen(prefix);
	for (size_t i = 0; i < prefix_len; ++i) {
		if (strchr(ALL_PATH_SEPARATORS, path[i]) && strchr(ALL_PATH_SEPARATORS, prefix[i]))
			continue; // treat all path separators as the same
		if (prefix[i] != path[i])
			return false;
	}
	return path[prefix_len] == '\0' || strchr(ALL_PATH_SEPARATORS, path[prefix_len]);
}

bool streq(const char *a, const char *b) {
	assert(a);
	assert(b);
	return strcmp(a, b) == 0;
}

size_t strn_len(const char *src, size_t n) {
	const char *p = src;
	// in C99 and C++11/14, calling memchr with a size larger than
	// the size of the object is undefined behaviour.
	// i don't think there's any way of doing this (efficiently) with standard C functions.
	for (size_t i = 0 ; i < n; ++i, ++p)
		if (*p == '\0')
			break;
	return (size_t)(p - src);
}

// duplicates at most n characters from src
char *strn_dup(const char *src, size_t n) {
	if (!src) return NULL;
	size_t len = strn_len(src, n);
	if (n > len)
		n = len;
	char *ret = malloc(n + 1);
	if (ret) {
		memcpy(ret, src, n);
		ret[n] = 0;
	}
	return ret;
}

// duplicates a null-terminated string. the returned string should be passed to free()
char *str_dup(const char *src) {
	return strn_dup(src, SIZE_MAX);
}

// safer version of strncat. dst_sz includes a null terminator.
void strn_cat(char *dst, size_t dst_sz, const char *src, size_t src_len) {
	size_t dst_len = strlen(dst);

	// make sure dst_len + src_len + 1 doesn't overflow
	if (dst_len > STRLEN_SAFE_MAX || src_len > STRLEN_SAFE_MAX) {
		assert(0);
		return;
	}

	if (dst_len >= dst_sz) {
		// dst doesn't actually contain a null-terminated string!
		assert(0);
		return;
	}

	if (dst_len + src_len + 1 > dst_sz) {
		// number of bytes left in dst, not including null terminator
		size_t n = dst_sz - dst_len - 1;
		memcpy(dst + dst_len, src, n);
		dst[dst_sz - 1] = 0; // dst_len + n == dst_sz - 1
	} else {
		memcpy(dst + dst_len, src, src_len);
		dst[dst_len + src_len] = 0;
	}
}

// safer version of strcat. dst_sz includes a null terminator.
void str_cat(char *dst, size_t dst_sz, const char *src) {
	strn_cat(dst, dst_sz, src, strlen(src));
}

// safer version of strncpy. dst_sz includes a null terminator.
void strn_cpy(char *dst, size_t dst_sz, const char *src, size_t src_len) {
	size_t n = src_len; // number of bytes to copy
	for (size_t i = 0; i < n; ++i) {
		if (src[i] == '\0') {
			n = i;
			break;
		}
	}
	
	if (dst_sz == 0) {
		assert(0);
		return;
	}

	if (dst_sz-1 < n)
		n = dst_sz-1;
	memcpy(dst, src, n);
	dst[n] = 0;
}

void str_cpy(char *dst, size_t dst_sz, const char *src) {
	strn_cpy(dst, dst_sz, src, SIZE_MAX);
}

void str_trim_start(char *str) {
	size_t n = strspn(str, "\r\v\t\n\f ");
	size_t len = strlen(str);
	memmove(str, str + n, len - n);
	str[len - n] = '\0';
}

void str_trim_end(char *str) {
	size_t i = strlen(str);
	while (i > 0 && isspace(str[i - 1])) {
		str[i - 1] = '\0';
		--i;
	}
}


void str_trim(char *str) {
	str_trim_end(str);
	str_trim_start(str);
}

void str_ascii_to_lowercase(char *str) {
	for (char *p = str; *p; p++) {
		if (*p > 0 && *p < 127)
			*p = (char)tolower(*p);
	}
}

size_t str_count_char(const char *s, char c) {
	const char *p = s;
	size_t count = 0;
	while (1) {
		p = strchr(p, c);
		if (p) {
			++count;
			++p;
		} else {
			break;
		}
	}
	return count;
}

char *a_sprintf(PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
char *a_sprintf(const char *fmt, ...) {
	// idk if you can always just pass NULL to vsnprintf
	va_list args;
	char fakebuf[2] = {0};
	va_start(args, fmt);
	int ret = vsnprintf(fakebuf, 1, fmt, args);
	va_end(args);
	
	if (ret < 0) return NULL; // bad format or something
	u32 n = (u32)ret;
	char *str = calloc(1, n + 1);
	va_start(args, fmt);
	vsnprintf(str, n + 1, fmt, args);
	va_end(args);
	return str;
}

void str_binary_number(char s[65], u64 n) {
	if (n == 0) {
		strcpy(s, "0");
		return;
	}
	
	u64 digits = 0;
	u64 m = n;
	while (m) {
		m >>= 1;
		digits += 1;
	}
	
	m = n;
	s[digits] = '\0';
	char *p = s + digits - 1;
	while (m) {
		*p-- = (m & 1) + '0';
		m >>= 1;
	}
}


// advances str to the start of the next UTF8 character
static void utf8_next_char_const(const char **str) {
	if (**str) {
		do {
			++*str;
		} while (((u8)(**str) & 0xC0) == 0x80); // while we are on a continuation byte
	}
}

char *strstr_case_insensitive(const char *haystack, const char *needle) {
	size_t needle_bytes = strlen(needle), haystack_bytes = strlen(haystack);

	if (needle_bytes > haystack_bytes) return NULL;
	
	const char *haystack_end = haystack + haystack_bytes;
	const char *needle_end = needle + needle_bytes;

	for (const char *haystack_start = haystack; haystack_start + needle_bytes <= haystack_end; utf8_next_char_const(&haystack_start)) {
		const char *p = haystack_start, *q = needle;
		bool match = true;

		// check if p matches q
		while (q < needle_end) {
			char32_t pchar = 0, qchar = 0;
			size_t bytes_p = unicode_utf8_to_utf32(&pchar, p, (size_t)(haystack_end - p));
			size_t bytes_q = unicode_utf8_to_utf32(&qchar, q, (size_t)(needle_end - q));
			if (bytes_p >= (size_t)-2 || bytes_q >= (size_t)-2) return NULL; // invalid UTF-8
			bool same = pchar == qchar;
			if (pchar < WINT_MAX && qchar < WINT_MAX) // on Windows, there is no way of finding the lower-case version of a codepoint outside the BMP. ):
				same = towlower((wint_t)pchar) == towlower((wint_t)qchar);
			if (!same) match = false;
			p += bytes_p;
			q += bytes_q;
		}
		if (match)
			return (char *)haystack_start;
	}
	return NULL;
}

void print_bytes(const u8 *bytes, size_t n) {
	const u8 *b, *end;
	for (b = bytes, end = bytes + n; b != end; ++b)
		printf("%02x ", *b);
	printf("\n");
}

int strcmp_case_insensitive(const char *a, const char *b) {
#if _WIN32
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

bool streq_case_insensitive(const char *a, const char *b) {
	return strcmp_case_insensitive(a, b) == 0;
}

int str_qsort_case_insensitive_cmp(const void *av, const void *bv) {
	const char *const *a = av, *const *b = bv;
	return strcmp_case_insensitive(*a, *b);
}

#if _WIN32
void qsort_with_context(void *base, size_t nmemb, size_t size,
	int (*compar)(void *, const void *, const void *),
	void *arg) {
	qsort_s(base, nmemb, size, compar, arg);
}
#else
typedef struct {
	int (*compar)(void *, const void *, const void *);
	void *context;
} QSortWithContext;
int qsort_with_context_cmp(const void *a, const void *b, void *context) {
	QSortWithContext *c = context;
	return c->compar(c->context, a, b);
}
void qsort_with_context(void *base, size_t nmemb, size_t size,
	int (*compar)(void *, const void *, const void *),
	void *arg) {
	QSortWithContext ctx = {
		.compar = compar,
		.context = arg
	};
	qsort_r(base, nmemb, size, qsort_with_context_cmp, &ctx);
}
#endif

const char *path_filename(const char *path) {
	for (int i = (int)strlen(path) - 1; i >= 0; --i) {
		if (strchr(ALL_PATH_SEPARATORS, path[i]))
			return &path[i+1];
	}
	// (a relative path with no path separators)
	return path;
}

bool path_is_absolute(const char *path) {
	return strchr(ALL_PATH_SEPARATORS, path[0]) != NULL
	#if _WIN32
		|| path[1] == ':'
	#endif
		;
}

void path_dirname(char *path) {
	if (!*path) {
		assert(0); // invalid path
		return;
	}
	for (size_t i = strlen(path) - 1; i > 0; --i) {
		if (strchr(ALL_PATH_SEPARATORS, path[i])) {
			if (strcspn(path, ALL_PATH_SEPARATORS) == i) {
				// only one path separator
				path[i+1] = '\0';
				return;
			}
			path[i] = '\0';
			return;
		}
	}
	if (strchr(ALL_PATH_SEPARATORS, path[0])) {
		path[1] = '\0';
		return;
	}
	assert(0); // invalid path (no path separator)
}

void path_full(const char *dir, const char *relpath, char *abspath, size_t abspath_size) {
	assert(abspath_size);
	assert(dir[0]);
	assert(path_is_absolute(dir));
	abspath[0] = '\0';
	
	if (path_is_absolute(relpath)) {
		if (strchr(ALL_PATH_SEPARATORS, relpath[0])) {
			// make sure that on windows, if dir's drive is C: the absolute path of \a is c:\a
			strn_cat(abspath, abspath_size, dir, strcspn(dir, ALL_PATH_SEPARATORS));
		} else {
			// copy drive component (e.g. set abspath to "C:")
			size_t drive_len = strcspn(relpath, ALL_PATH_SEPARATORS);
			strn_cat(abspath, abspath_size, relpath, drive_len);
			relpath += drive_len;
			if (*relpath) ++relpath; // move past separator
		}
	} else {
		str_cpy(abspath, abspath_size, dir);
	}
	
	while (*relpath) {
		size_t component_len = strcspn(relpath, ALL_PATH_SEPARATORS);
		const char *component_end = relpath + component_len;

		size_t len = strlen(abspath);
		if (component_len == 1 && relpath[0] == '.') {
			// ., do nothing
		} else if (component_len == 2 && relpath[0] == '.' && relpath[1] == '.') {
			// ..
			char *lastsep = strrchr(abspath, PATH_SEPARATOR);
			if (lastsep) {
				if (lastsep == abspath)
					lastsep[1] = '\0'; // e.g.  /abc
				else
					lastsep[0] = '\0';
			} else {
				// e.g. if abspath is currently C:
				// (do nothing)
			}
		} else {
			if (len == 0 || abspath[len - 1] != PATH_SEPARATOR)
				str_catf(abspath, abspath_size, "%c", PATH_SEPARATOR);
			strn_cat(abspath, abspath_size, relpath, component_len);
		}
		if (*component_end == 0)
			break;
		else
			relpath = component_end + 1;
	}
}

bool paths_eq(const char *path1, const char *path2) {
#if __unix__
	return streq(path1, path2);
#else
	char fixed_path1[8192];
	char fixed_path2[8192];
	strbuf_cpy(fixed_path1, path1);
	strbuf_cpy(fixed_path2, path2);
	for (size_t i = 0; fixed_path1[i]; ++i)
		if (fixed_path1[i] == '/')
			fixed_path1[i] = '\\';
	for (size_t i = 0; fixed_path2[i]; ++i)
		if (fixed_path2[i] == '/')
			fixed_path2[i] = '\\';
	return _stricmp(fixed_path1, fixed_path2) == 0;
#endif
}

bool copy_file(const char *src, const char *dst) {
	bool success = false;
	FILE *src_file = fopen(src, "rb");
	if (src_file) {
		FILE *dst_file = fopen(dst, "wb");
		if (dst_file) {
			char buf[1024];
			while (1) {
				size_t count = fread(buf, 1, sizeof buf, src_file);
				fwrite(buf, 1, count, dst_file);
				if (count < sizeof buf) break;
			}
			success = !ferror(src_file) && !ferror(dst_file);
			fclose(dst_file);
		}
		fclose(src_file);
	}
	return success;
}


#ifdef MATH_GL
#undef MATH_GL
#define MATH_GL 1
#endif

float lerpf(float x, float a, float b) {
	return x * (b-a) + a;
}

float normf(float x, float a, float b) {
	return (x-a) / (b-a);
}

float clampf(float x, float a, float b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

double clampd(double x, double a, double b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

int clampi(int x, int a, int b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

i16 clamp_i16(i16 x, i16 a, i16 b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

u16 clamp_u16(u16 x, u16 a, u16 b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

i32 clamp_i32(i32 x, i32 a, i32 b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

u32 clamp_u32(u32 x, u32 a, u32 b) {
	if (x < a) return a;
	if (x > b) return b;
	return x;
}

u8 ndigits_u64(u64 x) {
	u8 ndigits = 1;
	while (x > 9) {
		x /= 10;
		++ndigits;
	}
	return ndigits;
}

// remap x from the interval [from_a, from_b] to the interval [to_a, to_b], NOT clamping if x is outside the "from" interval.
float remapf(float x, float from_a, float from_b, float to_a, float to_b) {
	float pos = (x - from_a) / (from_b - from_a);
	return lerpf(pos, to_a, to_b);
}

float minf(float a, float b) {
	return a < b ? a : b;
}

float maxf(float a, float b) {
	return a > b ? a : b;
}

double maxd(double a, double b) {
	return a > b ? a : b;
}

double mind(double a, double b) {
	return a < b ? a : b;
}

u32 min_u32(u32 a, u32 b) {
	return a < b ? a : b;
}

u32 max_u32(u32 a, u32 b) {
	return a > b ? a : b;
}

// set *a to the minimum of *a and *b, and *b to the maximum
void sort2_u32(u32 *a, u32 *b) {
	u32 x = *a, y = *b;
	if (x > y) {
		*a = y;
		*b = x;
	}
}

i32 min_i32(i32 a, i32 b) {
	return a < b ? a : b;
}

i32 max_i32(i32 a, i32 b) {
	return a > b ? a : b;
}

u64 min_u64(u64 a, u64 b) {
	return a < b ? a : b;
}

u64 max_u64(u64 a, u64 b) {
	return a > b ? a : b;
}

i64 min_i64(i64 a, i64 b) {
	return a < b ? a : b;
}

i64 max_i64(i64 a, i64 b) {
	return a > b ? a : b;
}

i64 mod_i64(i64 a, i64 b) {
	assert(b > 0);
	i64 ret = a % b;
	if (ret < 0) ret += b;
	return ret;
}
i32 mod_i32(i32 a, i32 b) {
	assert(b > 0);
	i32 ret = a % b;
	if (ret < 0) ret += b;
	return ret;
}

i64 abs_i64(i64 x) {
	return x < 0 ? -x : +x;
}

i64 sgn_i64(i64 x) {
	if (x < 0) return -1;
	if (x > 0) return +1;
	return 0;
}

float sgnf(float x) {
	if (x < 0) return -1;
	if (x > 0) return +1;
	return 0;
}

vec2 vec2_add(vec2 a, vec2 b) {
	return (vec2){a.x + b.x, a.y + b.y};
}

vec2 vec2_add_const(vec2 a, float c) {
	return (vec2){a.x + c, a.y + c};
}

vec2 vec2_sub(vec2 a, vec2 b) {
	return (vec2){a.x - b.x, a.y - b.y};
}

vec2 vec2_scale(vec2 v, float s) {
	return (vec2){v.x * s, v.y * s};
}

vec2 vec2_mul(vec2 a, vec2 b) {
	return (vec2){a.x * b.x, a.y * b.y};
}

vec2 vec2_clamp(vec2 x, vec2 a, vec2 b) {
	return (vec2){clampf(x.x, a.x, b.x), clampf(x.y, a.y, b.y)};
}

float vec2_dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

float vec2_norm(vec2 v) {
	return sqrtf(vec2_dot(v, v));
}

vec2 vec2_lerp(float x, vec2 a, vec2 b) {
	return (vec2){lerpf(x, a.x, b.x), lerpf(x, a.y, b.y)};
}

// rotate v theta radians counterclockwise
vec2 vec2_rotate(vec2 v, float theta) {
	float c = cosf(theta), s = sinf(theta);
	return (vec2){
		c * v.x - s * v.y,
		s * v.x + c * v.y
	};
}

vec2 vec2_normalize(vec2 v) {
	float len = vec2_norm(v);
	float mul = len == 0.0f ? 1.0f : 1.0f/len;
	return vec2_scale(v, mul);
}

float vec2_distance(vec2 a, vec2 b) {
	return vec2_norm(vec2_sub(a, b));
}

void vec2_print(vec2 v) {
	printf("(%f, %f)\n", v.x, v.y);
}

vec2 vec2_polar(float r, float theta) {
	return (vec2){r * cosf(theta), r * sinf(theta)};
}

bool rect_contains_point_v2(vec2 pos, vec2 size, vec2 point) {
	float x1 = pos.x, y1 = pos.y, x2 = pos.x + size.x, y2 = pos.y + size.y,
		x = point.x, y = point.y;
	return x >= x1 && x < x2 && y >= y1 && y < y2;
}

bool centered_rect_contains_point(vec2 center, vec2 size, vec2 point) {
	return rect_contains_point_v2(vec2_sub(center, vec2_scale(size, 0.5f)), size, point);
}

Rect rect(vec2 pos, vec2 size) {
	Rect r;
	r.pos = pos;
	r.size = size;
	return r;
}

Rect rect_endpoints(vec2 e1, vec2 e2) {
	Rect r;
	r.pos = e1;
	r.size = vec2_sub(e2, e1);
	return r;
}

Rect rect4(float x1, float y1, float x2, float y2) {
	assert(x2 >= x1);
	assert(y2 >= y1);
	return rect_xywh(x1, y1, x2-x1, y2-y1);
}

Rect rect_xywh(float x, float y, float w, float h) {
	assert(w >= 0);
	assert(h >= 0);
	return rect((vec2){x, y}, (vec2){w, h});
}

Rect rect_centered(vec2 center, vec2 size) {
	Rect r;
	r.pos = vec2_sub(center, vec2_scale(size, 0.5f));
	r.size = size;
	return r;
}

vec2 rect_center(Rect r) {
	return vec2_add(r.pos, vec2_scale(r.size, 0.5f));
}

bool rect_contains_point(Rect r, vec2 point) {
	return rect_contains_point_v2(r.pos, r.size, point);
}

float rect_x1(Rect r) { return r.pos.x; }
float rect_y1(Rect r) { return r.pos.y; }
float rect_x2(Rect r) { return r.pos.x + r.size.x; }
float rect_y2(Rect r) { return r.pos.y + r.size.y; }
float rect_xmid(Rect r) { return r.pos.x + r.size.x * 0.5f; }
float rect_ymid(Rect r) { return r.pos.y + r.size.y * 0.5f; }

void rect_coords(Rect r, float *x1, float *y1, float *x2, float *y2) {
	*x1 = r.pos.x;
	*y1 = r.pos.y;
	*x2 = r.pos.x + r.size.x;
	*y2 = r.pos.y + r.size.y;
}

void rect_print(Rect r) {
	printf("Position: (%f, %f), Size: (%f, %f)\n", r.pos.x, r.pos.y, r.size.x, r.size.y);
}


float rects_intersect(Rect r1, Rect r2) {
	if (r1.pos.x >= r2.pos.x + r2.size.x) return false; // r1 is to the right of r2
	if (r2.pos.x >= r1.pos.x + r1.size.x) return false; // r2 is to the right of r1
	if (r1.pos.y >= r2.pos.y + r2.size.y) return false; // r1 is above r2
	if (r2.pos.y >= r1.pos.y + r1.size.y) return false; // r2 is above r1
	return true;
}

// returns whether or not there is any of the clipped rectangle left
bool rect_clip_to_rect(Rect *clipped, Rect clipper) {
	vec2 start_pos = clipped->pos;
	clipped->pos.x = maxf(clipped->pos.x, clipper.pos.x);
	clipped->pos.y = maxf(clipped->pos.y, clipper.pos.y);
	clipped->size = vec2_add(clipped->size, vec2_sub(start_pos, clipped->pos));

	clipped->size.x = clampf(clipped->size.x, 0, clipper.pos.x + clipper.size.x - clipped->pos.x);
	clipped->size.y = clampf(clipped->size.y, 0, clipper.pos.y + clipper.size.y - clipped->pos.y);
	return clipped->size.x > 0 && clipped->size.y > 0;
}

void rect_shrink(Rect *r, float amount) {
	r->pos.x += amount;
	r->pos.y += amount;
	r->size.x -= 2 * amount;
	r->size.y -= 2 * amount;
	r->size.x = maxf(r->size.x, 0);
	r->size.y = maxf(r->size.y, 0);
}

void rect_shrink_left(Rect *r, float amount) {
	r->pos.x += amount;
	r->size.x -= amount;
	r->size.x = maxf(r->size.x, 0);
}

void rect_shrink_top(Rect *r, float amount) {
	r->pos.y += amount;
	r->size.y -= amount;
	r->size.y = maxf(r->size.y, 0);
}

void rect_shrink_right(Rect *r, float amount) {
	r->size.x -= amount;
	r->size.x = maxf(r->size.x, 0);
}

void rect_shrink_bottom(Rect *r, float amount) {
	r->size.y -= amount;
	r->size.y = maxf(r->size.y, 0);
}

void rect_grow(Rect *r, float amount) {
	r->pos.x -= amount;
	r->pos.y -= amount;
	r->size.x += 2 * amount;
	r->size.y += 2 * amount;
}

int timespec_cmp(struct timespec a, struct timespec b) {
	if (a.tv_sec  > b.tv_sec)  return 1;
	if (a.tv_sec  < b.tv_sec)  return -1;
	if (a.tv_nsec > b.tv_nsec) return 1;
	if (a.tv_nsec < b.tv_nsec) return -1;
	return 0;
}

bool timespec_eq(struct timespec a, struct timespec b) {
	return timespec_cmp(a, b) == 0;
}

struct timespec timespec_max(struct timespec a, struct timespec b) {
	return timespec_cmp(a, b) < 0 ? b : a;
}

double timespec_to_seconds(struct timespec ts) {
	return (double)ts.tv_sec
		+ (double)ts.tv_nsec * 1e-9;
}


String32 str32(char32_t *str, size_t len) {
	String32 s = {str, len};
	return s;
}

String32 str32_substr(String32 s, size_t from, size_t len) {
	return str32(s.str + from, len);
}

// frees string and sets it to ""
void str32_free(String32 *s) {
	free(s->str);
	s->str = NULL;
	s->len = 0;
}

// the string returned should be str32_free'd.
// this will return an empty string if the allocation failed or the string is invalid UTF-8
String32 str32_from_utf8(const char *utf8) {
	String32 string = {NULL, 0};
	size_t len = strlen(utf8);
	if (len) {
		// the wide string uses at most as many "characters" (elements?) as the UTF-8 string
		char32_t *widestr = calloc(len, sizeof *widestr);
		if (widestr) {
			char32_t *wide_p = widestr;
			const char *utf8_p = utf8;
			const char *utf8_end = utf8_p + len;
			while (utf8_p < utf8_end) {
				char32_t c = 0;
				size_t n = unicode_utf8_to_utf32(&c, utf8_p, (size_t)(utf8_end - utf8_p));
				if (n == 0 // null character. this shouldn't happen.
					|| n >= (size_t)(-2) // invalid UTF-8
					) {
					free(widestr);
					widestr = wide_p = NULL;
					break;
				} else {
					// n bytes consumed
					*wide_p++ = c;
					utf8_p += n;
				}
			}
			string.str = widestr;
			string.len = (size_t)(wide_p - widestr);
		}
	}
	return string;
}


bool str32_to_utf8_cstr_in_place(String32 s, char *out) {
	char *p = out;
	for (size_t i = 0; i < s.len; ++i) {
		size_t bytes = unicode_utf32_to_utf8(p, s.str[i]);
		if (bytes == (size_t)-1) {
			// invalid UTF-32 code point
			*p = '\0';
			return false;
		} else {
			p += bytes;
		}
	}
	*p = '\0';
	return true;
}

char *str32_to_utf8_cstr(String32 s) {
	char *utf8 = calloc(4 * s.len + 1, 1); // each codepoint takes up at most 4 bytes in UTF-8, + we need a terminating null byte
	if (utf8) {
		str32_to_utf8_cstr_in_place(s, utf8);
	}
	return utf8;
}

// compare s to the ASCII string `ascii`
int str32_cmp_ascii(String32 s, const char *ascii) {
	for (size_t i = 0; i < s.len; ++i) {
		assert((char32_t)ascii[i] < 128);
		if ((char32_t)ascii[i] == '\0')
			return -1; // ascii is a prefix of s
		if (s.str[i] > (char32_t)ascii[i])
			return +1;
		if (s.str[i] < (char32_t)ascii[i])
			return -1;
	}
	if (ascii[s.len]) {
		// s is a prefix of ascii
		return +1;
	}
	return 0;
}

// check if s starts with the ASCII string `ascii`
bool str32_has_ascii_prefix(String32 s, const char *ascii) {
	for (size_t i = 0; i < s.len; ++i) {
		assert((char32_t)ascii[i] < 128);
		if ((char32_t)ascii[i] == '\0')
			return true; // ascii is a prefix of s
		if (s.str[i] > (char32_t)ascii[i])
			return false;
		if (s.str[i] < (char32_t)ascii[i])
			return false;
	}
	if (ascii[s.len]) {
		// s is a prefix of ascii
		return false;
	}
	// s is the same as ascii
	return true;
}

// returns the index of the given character in the string, or the length of the string if it's not found.
size_t str32chr(String32 s, char32_t c) {
	for (size_t i = 0; i < s.len; ++i) {
		if (s.str[i] == c)
			return i;
	}
	return s.len;
}

// returns number of instances of c in s
size_t str32_count_char(String32 s, char32_t c) {
	size_t total = 0;
	for (size_t i = 0; i < s.len; ++i) {
		total += s.str[i] == c;
	}
	return total;
}

// returns number of characters deleted from s
size_t str32_remove_all_instances_of_char(String32 *s, char32_t c) {
	char32_t *str = s->str;
	size_t ndeleted = 0;
	size_t len = s->len;
	size_t out = 0;
	for (size_t in = 0; in < len; ++in) {
		if (str[in] == c) {
			++ndeleted;
		} else {
			str[out++] = str[in];
		}
	}
	s->len = out;
	return ndeleted;
}

// returns the length of the longest prefix of `s` containing only
// ASCII characters in the C-string `charset`.
size_t str32_ascii_spn(String32 s, const char *charset) {
	for (u32 i = 0; i < s.len; ++i) {
		if (s.str[i] >= 128)
			return i; // non-ASCII character in s, so that can't be in charset.
		bool found = false;
		for (const char *p = charset; *p; ++p) {
			assert((char32_t)*p < 128);
			if ((char32_t)*p == s.str[i]) {
				found = true;
				break;
			}
		}
		if (!found) return i;
	}
	return s.len;
}
