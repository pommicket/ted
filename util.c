#if _WIN32
#include <intrin.h>
#include <direct.h>
#elif __unix__
#include <unistd.h>
#else
#error "Unrecognized operating system."
#endif

#include "base.h"
#include "util.h"
#include "unicode.h"

// on 16-bit systems, this is 16383. on 32/64-bit systems, this is 1073741823
// it is unusual to have a string that long.
#define STRLEN_SAFE_MAX (UINT_MAX >> 2)

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
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

// e.g. "/usr/share/bla" has the path prefix "/usr/share" but not "/usr/sha"
bool str_has_path_prefix(const char *path, const char *prefix) {
	size_t prefix_len = strlen(prefix);
	if (strncmp(path, prefix, prefix_len) != 0)
		return false;
	return path[prefix_len] == '\0' || strchr(ALL_PATH_SEPARATORS, path[prefix_len]);
}

bool streq(const char *a, const char *b) {
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

// safer version of strcpy. dst_sz includes a null terminator.
void str_cpy(char *dst, size_t dst_sz, const char *src) {
	strn_cpy(dst, dst_sz, src, SIZE_MAX);
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
	const char *last_path_sep = strrchr(path, PATH_SEPARATOR);
	if (last_path_sep)
		return last_path_sep + 1;
	// (a relative path with no path separators)
	return path;
}

bool path_is_absolute(const char *path) {
	return path[0] == PATH_SEPARATOR
	#if _WIN32
		|| path[1] == ':'
	#endif
		;
}

void path_full(const char *dir, const char *relpath, char *abspath, size_t abspath_size) {
	assert(abspath_size);
	assert(dir[0]);
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
			assert(lastsep);
			if (lastsep == abspath)
				lastsep[1] = '\0';
			else
				lastsep[0] = '\0';
		} else {
			if (len == 0 || abspath[len - 1] != PATH_SEPARATOR)
				str_cat(abspath, abspath_size, PATH_SEPARATOR_STR);
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

void change_directory(const char *path) {
#if _WIN32
	_chdir(path);
#else
	chdir(path);
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

float degrees(float r) {
	return r * (180.0f / PIf);
}
float radians(float r) {
	return r * (PIf / 180.f);
}

// map x from the interval [0, 1] to the interval [a, b]. does NOT clamp.
float lerpf(float x, float a, float b) {
	return x * (b-a) + a;
}

// opposite of lerp; map x from the interval [a, b] to the interval [0, 1]. does NOT clamp.
float normf(float x, float a, float b) {
	return (x-a) / (b-a);
}

float clampf(float x, float a, float b) {
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
	i64 ret = a % b;
	if (ret < 0) ret += b;
	return ret;
}
i32 mod_i32(i32 a, i32 b) {
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

float smoothstepf(float x) {
	if (x <= 0) return 0;
	if (x >= 1) return 1;
	return x * x * (3 - 2 * x);
}

float randf(void) {
	return (float)rand() / (float)((ulong)RAND_MAX + 1);
}

float rand_gauss(void) {
	// https://en.wikipedia.org/wiki/Normal_distribution#Generating_values_from_normal_distribution
	float U, V;
	do
		U = randf(), V = randf();
	while (U == 0 || V == 0);
	return sqrtf(-2 * logf(U)) * cosf(TAUf * V);
}

u32 rand_u32(void) {
	return ((u32)rand() & 0xfff)
		| ((u32)rand() & 0xfff) << 12
		| ((u32)rand() & 0xff) << 24;
}

float rand_uniform(float from, float to) {
	return lerpf(randf(), from, to);
}

float sigmoidf(float x) {
	return 1.0f / (1.0f + expf(-x));
}

// returns ⌈x/y⌉ (x/y rounded up)
i32 ceildivi32(i32 x, i32 y) {
	if (y < 0) {
		// negating both operands doesn't change the answer
		x = -x;
		y = -y;
	}
	if (x < 0) {
		// truncation is the same as ceiling for negative numbers
		return x / y;
	} else {
		return (x + (y-1)) / y;
	}
}

v2 V2(float x, float y) {
	v2 v;
	v.x = x;
	v.y = y;
	return v;
}

v2 v2_add(v2 a, v2 b) {
	return V2(a.x + b.x, a.y + b.y);
}

v2 v2_add_const(v2 a, float c) {
	return V2(a.x + c, a.y + c);
}

v2 v2_sub(v2 a, v2 b) {
	return V2(a.x - b.x, a.y - b.y);
}

v2 v2_scale(v2 v, float s) {
	return V2(v.x * s, v.y * s);
}

v2 v2_mul(v2 a, v2 b) {
	return V2(a.x * b.x, a.y * b.y);
}

v2 v2_clamp(v2 x, v2 a, v2 b) {
	return V2(clampf(x.x, a.x, b.x), clampf(x.y, a.y, b.y));
}

float v2_dot(v2 a, v2 b) {
	return a.x * b.x + a.y * b.y;
}

float v2_len(v2 v) {
	return sqrtf(v2_dot(v, v));
}

v2 v2_lerp(float x, v2 a, v2 b) {
	return V2(lerpf(x, a.x, b.x), lerpf(x, a.y, b.y));
}

// rotate v theta radians counterclockwise
v2 v2_rotate(v2 v, float theta) {
	float c = cosf(theta), s = sinf(theta);
	return V2(
		c * v.x - s * v.y,
		s * v.x + c * v.y
	);
}

v2 v2_normalize(v2 v) {
	float len = v2_len(v);
	float mul = len == 0.0f ? 1.0f : 1.0f/len;
	return v2_scale(v, mul);
}

float v2_dist(v2 a, v2 b) {
	return v2_len(v2_sub(a, b));
}

float v2_dist_squared(v2 a, v2 b) {
	v2 diff = v2_sub(a, b);
	return v2_dot(diff, diff);
}

void v2_print(v2 v) {
	printf("(%f, %f)\n", v.x, v.y);
}

v2 v2_rand_unit(void) {
	float theta = rand_uniform(0, TAUf);
	return V2(cosf(theta), sinf(theta));
}

v2 v2_polar(float r, float theta) {
	return V2(r * cosf(theta), r * sinf(theta));
}

v3 V3(float x, float y, float z) {
	v3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

v3 v3_from_v2(v2 v) {
	return V3(v.x, v.y, 0);
}

v3 v3_add(v3 a, v3 b) {
	return V3(a.x + b.x, a.y + b.y, a.z + b.z);
}

v3 v3_sub(v3 a, v3 b) {
	return V3(a.x - b.x, a.y - b.y, a.z - b.z);
}

v3 v3_scale(v3 v, float s) {
	return V3(v.x * s, v.y * s, v.z * s);
}

v3 v3_lerp(float x, v3 a, v3 b) {
	return V3(lerpf(x, a.x, b.x), lerpf(x, a.y, b.y), lerpf(x, a.z, b.z));
}

float v3_dot(v3 u, v3 v) {
	return u.x*v.x + u.y*v.y + u.z*v.z;
}

v3 v3_cross(v3 u, v3 v) {
	v3 prod = V3(u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x);
	return prod;
}

float v3_len(v3 v) {
	return sqrtf(v3_dot(v, v));
}

float v3_dist(v3 a, v3 b) {
	return v3_len(v3_sub(a, b));
}

float v3_dist_squared(v3 a, v3 b) {
	v3 diff = v3_sub(a, b);
	return v3_dot(diff, diff);
}

v3 v3_normalize(v3 v) {
	float len = v3_len(v);
	float mul = len == 0.0f ? 1.0f : 1.0f/len;
	return v3_scale(v, mul);
}

v2 v3_xy(v3 v) {
	return V2(v.x, v.y);
}

// a point on a unit sphere 
v3 v3_on_sphere(float yaw, float pitch) {
	return V3(cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch));
}

void v3_print(v3 v) {
	printf("(%f, %f, %f)\n", v.x, v.y, v.z);
}

v3 v3_rand(void) {
	return V3(randf(), randf(), randf());
}

v3 v3_rand_unit(void) {
	/*
		monte carlo method
		keep generating random points in cube of radius 1 (width 2) centered at origin,
		until you get a point in the unit sphere, then extend it to find the point lying
		on the sphere.
	*/
	while (1) {
		v3 v = V3(rand_uniform(-1.0f, +1.0f), rand_uniform(-1.0f, +1.0f), rand_uniform(-1.0f, +1.0f));
		float dist_squared_to_origin = v3_dot(v, v);
		if (dist_squared_to_origin <= 1 && dist_squared_to_origin != 0.0f) {
			return v3_scale(v, 1.0f / sqrtf(dist_squared_to_origin));
		}
	}
	return V3(0, 0, 0);
}

v4 V4(float x, float y, float z, float w) {
	v4 v;
	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;
	return v;
}

v4 v4_add(v4 a, v4 b) {
	return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

v4 v4_sub(v4 a, v4 b) {
	return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

v4 v4_scale(v4 v, float s) {
	return V4(v.x * s, v.y * s, v.z * s, v.w * s);
}

v4 v4_scale_xyz(v4 v, float s) {
	return V4(v.x * s, v.y * s, v.z * s, v.w);
}

v4 v4_lerp(float x, v4 a, v4 b) {
	return V4(lerpf(x, a.x, b.x), lerpf(x, a.y, b.y), lerpf(x, a.z, b.z), lerpf(x, a.w, b.w));
}

float v4_dot(v4 u, v4 v) {
	return u.x*v.x + u.y*v.y + u.z*v.z + u.w*v.w;
}

// create a new vector by multiplying the respective components of u and v 
v4 v4_mul(v4 u, v4 v) {
	return V4(u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w);
}

float v4_len(v4 v) {
	return sqrtf(v4_dot(v, v));
}

v4 v4_normalize(v4 v) {
	float len = v4_len(v);
	float mul = len == 0.0f ? 1.0f : 1.0f/len;
	return v4_scale(v, mul);
}

v3 v4_xyz(v4 v) {
	return V3(v.x, v.y, v.z);
}

v4 v4_rand(void) {
	return V4(randf(), randf(), randf(), randf());
}

void v4_print(v4 v) {
	printf("(%f, %f, %f, %f)\n", v.x, v.y, v.z, v.w);
}


v2d V2D(double x, double y) {
	v2d v;
	v.x = x;
	v.y = y;
	return v;
}

void m4_print(m4 m) {
	int i;
	for (i = 0; i < 4; ++i)
		printf("[ %f %f %f %f ]\n", m.e[i], m.e[i+4], m.e[i+8], m.e[i+12]);
	printf("\n");
}

m4 M4(
	float a, float b, float c, float d,
	float e, float f, float g, float h,
	float i, float j, float k, float l,
	float m, float n, float o, float p) {
	m4 ret;
	float *x = ret.e;
	x[0] = a; x[4] = b; x[ 8] = c; x[12] = d;
	x[1] = e; x[5] = f; x[ 9] = g; x[13] = h;
	x[2] = i; x[6] = j; x[10] = k; x[14] = l;
	x[3] = m; x[7] = n; x[11] = o; x[15] = p;
	return ret;
}

// see https://en.wikipedia.org/wiki/Rotation_matrix#General_rotations 
m4 m4_yaw(float yaw) {
	float c = cosf(yaw), s = sinf(yaw);
	return M4(
		c, 0, -s, 0,
		0, 1,  0, 0,
		s, 0,  c, 0,
		0, 0,  0, 1
	);
}

m4 m4_pitch(float pitch) {
	float c = cosf(pitch), s = sinf(pitch);
	return M4(
		1, 0,  0, 0,
		0, c, -s, 0,
		0, s,  c, 0,
		0, 0,  0, 1
	);
}

// https://en.wikipedia.org/wiki/Translation_(geometry) 
m4 m4_translate(v3 t) {
	return M4(
		1, 0, 0, t.x,
		0, 1, 0, t.y,
		0, 0, 1, t.z,
		0, 0, 0, 1
	);
}

// multiply m by [v.x, v.y, v.z, 1] 
v3 m4_mul_v3(m4 m, v3 v) {
	return v3_add(v3_scale(V3(m.e[0], m.e[1], m.e[2]), v.x), v3_add(v3_scale(V3(m.e[4], m.e[5], m.e[6]), v.y),
		v3_add(v3_scale(V3(m.e[8], m.e[9], m.e[10]), v.z), V3(m.e[12], m.e[13], m.e[14]))));
}

/*
4x4 perspective matrix.
fov - field of view in radians, aspect - width:height aspect ratio, z_near/z_far - clipping planes
math stolen from gluPerspective (https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml)
*/
m4 m4_perspective(float fov, float aspect, float z_near, float z_far) {
	float f = 1.0f / tanf(fov / 2.0f);
	return M4(
		f/aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (z_far+z_near) / (z_near-z_far), (2.0f*z_far*z_near) / (z_near-z_far),
		0, 0, -1, 0
	);
}

// windows.h defines near and far, so let's not use those 
m4 m4_ortho(float left, float right, float bottom, float top, float near_, float far_) {
	float tx = -(right + left)/(right - left);
	float ty = -(top + bottom)/(top - bottom);
	float tz = -(far_ + near_)/(far_ - near_);
	return M4(
		2.0f / (right - left), 0, 0, tx,
		0, 2.0f / (top - bottom), 0, ty,
		0, 0, -2.0f / (far_ - near_), tz,
		0, 0, 0, 1
	);
}


m4 m4_mul(m4 a, m4 b) {
	m4 prod = {0};
	int i, j;
	float *x = prod.e;
	for (i = 0; i < 4; ++i) {
		for (j = 0; j < 4; ++j, ++x) {
			float *as = &a.e[j];
			float *bs = &b.e[4*i];
			*x = as[0]*bs[0] + as[4]*bs[1] + as[8]*bs[2] + as[12]*bs[3];
		}
	}
	return prod;
}

m4 m4_inv(m4 mat) {
	m4 ret;
	float *inv = ret.e;
	float *m = mat.e;

	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0) {
		memset(inv, 0, sizeof *inv);
	} else {
		det = 1 / det;

		for (int i = 0; i < 16; i++)
			inv[i] *= det;
	}

	return ret;
}

v2i V2I(int x, int y) {
	v2i v;
	v.x = x;
	v.y = y;
	return v;
}

void rgba_u32_to_floats(u32 rgba, float floats[4]) {
	floats[0] = (float)((rgba >> 24) & 0xFF) / 255.f;
	floats[1] = (float)((rgba >> 16) & 0xFF) / 255.f;
	floats[2] = (float)((rgba >>  8) & 0xFF) / 255.f;
	floats[3] = (float)((rgba >>  0) & 0xFF) / 255.f;
}

v4 rgba_u32_to_v4(u32 rgba) {
	float c[4];
	rgba_u32_to_floats(rgba, c);
	return V4(c[0], c[1], c[2], c[3]);
}

u32 rgba_v4_to_u32(v4 color) {
	return (u32)(color.x * 255) << 24
		| (u32)(color.y * 255) << 16
		| (u32)(color.z * 255) << 8
		| (u32)(color.w * 255);
}

// returns average of red green and blue components of color
float rgba_brightness(u32 color) {
	u8 r = (u8)(color >> 24), g = (u8)(color >> 16), b = (u8)(color >> 8);
	return ((float)r+(float)g+(float)b) * (1.0f / 3);
}

bool rect_contains_point_v2(v2 pos, v2 size, v2 point) {
	float x1 = pos.x, y1 = pos.y, x2 = pos.x + size.x, y2 = pos.y + size.y,
		x = point.x, y = point.y;
	return x >= x1 && x < x2 && y >= y1 && y < y2;
}

bool centered_rect_contains_point(v2 center, v2 size, v2 point) {
	return rect_contains_point_v2(v2_sub(center, v2_scale(size, 0.5f)), size, point);
}

Rect rect(v2 pos, v2 size) {
	Rect r;
	r.pos = pos;
	r.size = size;
	return r;
}

Rect rect_endpoints(v2 e1, v2 e2) {
	Rect r;
	r.pos = e1;
	r.size = v2_sub(e2, e1);
	return r;
}

Rect rect4(float x1, float y1, float x2, float y2) {
	assert(x2 >= x1);
	assert(y2 >= y1);
	return rect(V2(x1,y1), V2(x2-x1, y2-y1));
}

Rect rect_xywh(float x, float y, float w, float h) {
	assert(w >= 0);
	assert(h >= 0);
	return rect(V2(x, y), V2(w, h));
}

Rect rect_centered(v2 center, v2 size) {
	Rect r;
	r.pos = v2_sub(center, v2_scale(size, 0.5f));
	r.size = size;
	return r;
}

v2 rect_center(Rect r) {
	return v2_add(r.pos, v2_scale(r.size, 0.5f));
}

bool rect_contains_point(Rect r, v2 point) {
	return rect_contains_point_v2(r.pos, r.size, point);
}

Rect rect_translate(Rect r, v2 by) {
	return rect(v2_add(r.pos, by), r.size);
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
	v2 start_pos = clipped->pos;
	clipped->pos.x = maxf(clipped->pos.x, clipper.pos.x);
	clipped->pos.y = maxf(clipped->pos.y, clipper.pos.y);
	clipped->size = v2_add(clipped->size, v2_sub(start_pos, clipped->pos));

	clipped->size.x = clampf(clipped->size.x, 0, clipper.pos.x + clipper.size.x - clipped->pos.x);
	clipped->size.y = clampf(clipped->size.y, 0, clipper.pos.y + clipper.size.y - clipped->pos.y);
	return clipped->size.x > 0 && clipped->size.y > 0;
}

// removes `amount` from all sides of r
Rect rect_shrink(Rect r, float amount) {
	r.pos.x += amount;
	r.pos.y += amount;
	r.size.x -= 2 * amount;
	r.size.y -= 2 * amount;
	r.size.x = maxf(r.size.x, 0);
	r.size.y = maxf(r.size.y, 0);
	return r;
}

// adds `amount` to all sides of r
Rect rect_grow(Rect r, float amount) {
	r.pos.x -= amount;
	r.pos.y -= amount;
	r.size.x += 2 * amount;
	r.size.y += 2 * amount;
	return r;
}

v4 color_rgba_to_hsva(v4 rgba) {
	float R = rgba.x, G = rgba.y, B = rgba.z, A = rgba.w;
	float M = maxf(R, maxf(G, B));
	float m = minf(R, minf(G, B));
	float C = M - m;
	float H = 0;
	if (C == 0)
		H = 0;
	else if (M == R)
		H = fmodf((G - B) / C, 6);
	else if (M == G)
		H = (B - R) / C + 2;
	else if (M == B)
		H = (R - G) / C + 4;
	H *= 60;
	float V = M;
	float S = V == 0 ? 0 : C / V;
	return V4(H, S, V, A);
}

v4 color_hsva_to_rgba(v4 hsva) {
	float H = hsva.x, S = hsva.y, V = hsva.z, A = hsva.w;
	H /= 60;
	float C = S * V;
	float X = C * (1 - fabsf(fmodf(H, 2) - 1));
	float R, G, B;
	if (H <= 1)
		R=C, G=X, B=0;
	else if (H <= 2)
		R=X, G=C, B=0;
	else if (H <= 3)
		R=0, G=C, B=X;
	else if (H <= 4)
		R=0, G=X, B=C;
	else if (H <= 5)
		R=X, G=0, B=C;
	else
		R=C, G=0, B=X;
	
	float m = V-C;
	R += m;
	G += m;
	B += m;
	return V4(R, G, B, A);
}

u32 color_interpolate(float x, u32 color1, u32 color2) {
	x = x * x * (3 - 2*x); // hermite interpolation

	v4 c1 = rgba_u32_to_v4(color1), c2 = rgba_u32_to_v4(color2);
	// to make it interpolate more nicely, convert to hsv, interpolate in that space, then convert back
	c1 = color_rgba_to_hsva(c1);
	c2 = color_rgba_to_hsva(c2);
	// v_1/2 named differently to avoid shadowing
	float h1 = c1.x, s1 = c1.y, v_1 = c1.z, a1 = c1.w;
	float h2 = c2.x, s2 = c2.y, v_2 = c2.z, a2 = c2.w;
	
	float s_out = lerpf(x, s1, s2);
	float v_out = lerpf(x, v_1, v_2);
	float a_out = lerpf(x, a1, a2);
	
	float h_out;
	// because hue is on a circle, we need to make sure we take the shorter route around the circle
	if (fabsf(h1 - h2) < 180) {
		h_out = lerpf(x, h1, h2);
	} else if (h1 > h2) {
		h_out = lerpf(x, h1, h2 + 360);
	} else {
		h_out = lerpf(x, h1 + 360, h2);
	}
	h_out = fmodf(h_out, 360);
	
	v4 c_out = V4(h_out, s_out, v_out, a_out);
	c_out = color_hsva_to_rgba(c_out);
	return rgba_v4_to_u32(c_out);
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

// returns a null-terminated UTF-8 string
// the string returned should be free'd
// this will return NULL on failure
char *str32_to_utf8_cstr(String32 s) {
	char *utf8 = calloc(4 * s.len + 1, 1); // each codepoint takes up at most 4 bytes in UTF-8, + we need a terminating null byte
	if (utf8) {
		char *p = utf8;
		for (size_t i = 0; i < s.len; ++i) {
			size_t bytes = unicode_utf32_to_utf8(p, s.str[i]);
			if (bytes == (size_t)-1) {
				// invalid UTF-32 code point
				free(utf8);
				return NULL;
			} else {
				p += bytes;
			}
		}
		*p = '\0';
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
