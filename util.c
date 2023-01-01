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

// on 16-bit systems, this is 16383. on 32/64-bit systems, this is 1073741823
// it is unusual to have a string that long.
#define STRLEN_SAFE_MAX (UINT_MAX >> 2)

bool is_word(char32_t c) {
	return c > WCHAR_MAX || c == '_' || iswalnum((wint_t)c);
}

bool is_digit(char32_t c) {
	return c < WCHAR_MAX && iswdigit((wint_t)c);
}

bool is_space(char32_t c) {
	return c < WCHAR_MAX && iswspace((wint_t)c);
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


