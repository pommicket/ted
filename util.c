#if _WIN32
#include <intrin.h>
#include <direct.h>
#elif __unix__
#include <unistd.h>
#else
#error "Unrecognized operating system."
#endif

static u8 util_popcount(u64 x) {
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

static u8 util_count_leading_zeroes32(u32 x) {
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

static bool util_is_power_of_2(u64 x) {
	return util_popcount(x) == 1;
}

// for finding a character in a char32 string
static char32_t *util_mem32chr(char32_t *s, char32_t c, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (s[i] == c) {
			return &s[i];
		}
	}
	return NULL;
}

static char32_t const *util_mem32chr_const(char32_t const *s, char32_t c, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (s[i] == c) {
			return &s[i];
		}
	}
	return NULL;
}

static bool str_has_prefix(char const *str, char const *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static bool streq(char const *a, char const *b) {
	return strcmp(a, b) == 0;
}

// duplicates at most n characters from src
static char *strn_dup(char const *src, size_t n) {
	size_t len = strlen(src);
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
static char *str_dup(char const *src) {
	return strn_dup(src, SIZE_MAX);
}

// like snprintf, but not screwed up on windows
#define str_printf(str, size, ...) (str)[(size) - 1] = '\0', snprintf((str), (size) - 1, __VA_ARGS__)
// like snprintf, but the size is taken to be the length of the array str.
//                              first, check that str is actually an array
#define strbuf_printf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_printf(str, sizeof str, __VA_ARGS__)
#define str_catf(str, size, ...) str_printf((str) + strlen(str), (size) - strlen(str), __VA_ARGS__)
#define strbuf_catf(str, ...) assert(sizeof str != 4 && sizeof str != 8), \
	str_catf(str, sizeof str, __VA_ARGS__)

// on 16-bit systems, this is 16383. on 32/64-bit systems, this is 1073741823
// it is unusual to have a string that long.
#define STRLEN_SAFE_MAX (UINT_MAX >> 2)

// safer version of strncat. dst_sz includes a null terminator.
static void strn_cat(char *dst, size_t dst_sz, char const *src, size_t src_len) {
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
static void str_cat(char *dst, size_t dst_sz, char const *src) {
	strn_cat(dst, dst_sz, src, strlen(src));
}

// safer version of strncpy. dst_sz includes a null terminator.
static void strn_cpy(char *dst, size_t dst_sz, char const *src, size_t src_len) {
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
static void str_cpy(char *dst, size_t dst_sz, char const *src) {
	strn_cpy(dst, dst_sz, src, SIZE_MAX);
}

#define strbuf_cpy(dst, src) str_cpy(dst, sizeof dst, src)
#define strbuf_cat(dst, src) str_cat(dst, sizeof dst, src)

// advances str to the start of the next UTF8 character
static void utf8_next_char_const(char const **str) {
	if (**str) {
		do {
			++*str;
		} while (((u8)(**str) & 0xC0) == 0x80); // while we are on a continuation byte
	}
}

/* 
returns the first instance of needle in haystack, where both are UTF-8 strings, ignoring the case of the characters,
or NULL if the haystack does not contain needle
WARNING: O(strlen(haystack) * strlen(needle))
*/
static char *stristr(char const *haystack, char const *needle) {
	size_t needle_bytes = strlen(needle), haystack_bytes = strlen(haystack);

	if (needle_bytes > haystack_bytes) return NULL;
	
	char const *haystack_end = haystack + haystack_bytes;
	char const *needle_end = needle + needle_bytes;

	for (char const *haystack_start = haystack; haystack_start + needle_bytes <= haystack_end; utf8_next_char_const(&haystack_start)) {
		char const *p = haystack_start, *q = needle;
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

static void print_bytes(u8 *bytes, size_t n) {
	u8 *b, *end;
	for (b = bytes, end = bytes + n; b != end; ++b)
		printf("%x ", *b);
	printf("\n");
}

/*
does this predicate hold for all the characters of s. predicate is int (*)(int) instead
of bool (*)(char) so that you can pass isprint, etc. to it.
*/
static bool str_satisfies(char const *s, int (*predicate)(int)) {
	char const *p;
	for (p = s; *p; ++p)
		if (!predicate(*p))
			return false;
	return true;
}


static int strcmp_case_insensitive(char const *a, char const *b) {
#if _WIN32
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

// function to be passed into qsort for case insensitive sorting
static int str_qsort_case_insensitive_cmp(const void *av, const void *bv) {
	char const *const *a = av, *const *b = bv;
	return strcmp_case_insensitive(*a, *b);
}

static void *qsort_ctx_arg;
static int (*qsort_ctx_cmp)(void *, const void *, const void *);
static int qsort_with_context_cmp(const void *a, const void *b) {
	return qsort_ctx_cmp(qsort_ctx_arg, a, b);
}

static void qsort_with_context(void *base, size_t nmemb, size_t size, int (*compar)(void *, const void *, const void *), void *arg) {
	// just use global variables. hopefully we don't try to run this in something multithreaded!
	qsort_ctx_arg = arg;
	qsort_ctx_cmp = compar;
	qsort(base, nmemb, size, qsort_with_context_cmp);
}

// the actual file name part of the path; get rid of the containing directory.
// NOTE: the returned string is part of path, so you don't need to free it or anything.
static char const *path_filename(char const *path) {
	char const *last_path_sep = strrchr(path, PATH_SEPARATOR);
	if (last_path_sep)
		return last_path_sep + 1;
	// (a relative path with no path separators)
	return path;
}

static bool path_is_absolute(char const *path) {
	return path[0] == PATH_SEPARATOR
	#if _WIN32
		|| path[1] == ':'
	#endif
		;
}

// assuming `dir` is an absolute path, returns the absolute path of `relpath`, relative to `dir`.
static void path_full(char const *dir, char const *relpath, char *abspath, size_t abspath_size) {
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
		char const *component_end = relpath + component_len;

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

// returns true if the paths are the same.
// handles the fact that paths are case insensitive on windows.
// treats links as different from the files they point to.
static bool paths_eq(char const *path1, char const *path2) {
#if _WIN32
	return _stricmp(path1, path2) == 0;
#else
	return streq(path1, path2);
#endif
}

static void change_directory(char const *path) {
#if _WIN32
	_chdir(path);
#else
	chdir(path);
#endif
}

// returns true on success
static bool copy_file(char const *src, char const *dst) {
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
