/// \file
/// basic types and macros.
///
/// this file is included almost everywhere.

#ifndef BASE_H_
#define BASE_H_

#ifndef DEBUG
#define NDEBUG 1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if __GNUC__
#define FALLTHROUGH __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif

#if _WIN32
#include <windows.h>
#include <shlobj.h>
#include <dbghelp.h>
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
// on windows, let the user use forwards slashes as well as backslashes
#define ALL_PATH_SEPARATORS "\\/"
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define ALL_PATH_SEPARATORS "/"
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <float.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#if __linux__ || _WIN32
#include <uchar.h>
#else
// OpenBSD has uchar.h but it doesn't seem to define char32_t ?
typedef uint32_t char32_t;
#endif

#if !__TINYC__ && __STDC_VERSION__ >= 201112
#define static_assert_if_possible(cond) _Static_assert(cond, "Static assertion failed");
#else
#define static_assert_if_possible(cond)
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// (for u8 and u16, you can use %u)
#define U32_FMT "%" PRIu32
#define U64_FMT "%" PRIu64
#define U8_MAX  0xff
#define U16_MAX 0xffff
#define U32_MAX 0xffffffff
#define U64_MAX 0xffffffffffffffff

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// (for i8 and i16, you can use %d)
#define I32_FMT "%" PRId32
#define I64_FMT "%" PRId64
#define I8_MIN ((i8)0x80)
#define I16_MIN ((i16)0x8000)
#define I32_MIN ((i32)0x80000000)
#define I64_MIN ((i64)0x8000000000000000)
#define I8_MAX  0x7f
#define I16_MAX 0x7fff
#define I32_MAX 0x7fffffff
#define I64_MAX 0x7fffffffffffffff

typedef unsigned int  uint;
typedef unsigned long ulong;

typedef long long llong;
typedef unsigned long long ullong;

/// allows 
/// ```
/// switch (c) {
/// case ANY_DIGIT:
///     ...
/// }
/// ```
#define ANY_DIGIT '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'

#ifdef __GNUC__
#define WarnUnusedResult __attribute__((warn_unused_result))
#else
#define WarnUnusedResult
#endif

#if __GNUC__
#define ATTRIBUTE_PRINTF(fmt_idx, arg_idx) __attribute__ ((format(printf, fmt_idx, arg_idx)))
#else
/// attribute for functions which are like `printf` (to give `-Wformat` warnings)
#define ATTRIBUTE_PRINTF(fmt_idx, arg_idx)
#endif
#if _MSC_VER > 1400
#define PRINTF_FORMAT_STRING _Printf_format_string_
#else
/// needed to give format warnings for MSVC for custom functions
#define PRINTF_FORMAT_STRING
#endif

/// this type is an alias for bool, except that it
/// produces a warning if it's not used.
/// false = error, true = success
#define Status bool WarnUnusedResult

#define arr_count(a) (sizeof (a) / sizeof *(a))

#ifdef __GNUC__
#define no_warn_start _Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wpedantic\"") \
	_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
	_Pragma("GCC diagnostic ignored \"-Wsign-compare\"") \
	_Pragma("GCC diagnostic ignored \"-Wconversion\"") \
	_Pragma("GCC diagnostic ignored \"-Wimplicit-fallthrough\"") \
	_Pragma("GCC diagnostic ignored \"-Wunused-function\"")

#define no_warn_end _Pragma("GCC diagnostic pop")
#else
#define no_warn_start
#define no_warn_end
#endif

#if _WIN32
static void print(const char *fmt, ...) {
	char buf[2048];
	buf[2047] = '\0';
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof buf - 1, fmt, args);
	va_end(args);
	OutputDebugStringA(buf);
}
#define eprint print
#else
#define print printf
#define eprint(...) fprintf(stderr, __VA_ARGS__)
#endif
#define println(...) print(__VA_ARGS__), print("\n")
#define eprintln(...) eprint(__VA_ARGS__), eprint("\n")

#if DEBUG
#define debug_print print
#define debug_println println
#else
#define debug_print(...)
#define debug_println(...)
#endif

#if PROFILE
#define PROFILE_TIME(var) double var = time_get_seconds();
#else
#define PROFILE_TIME(var)
#endif

#endif // BASE_H_
