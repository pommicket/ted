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
/// used to mark switch cases that can fallthrough.
#define FALLTHROUGH
#endif

#if _WIN32
#include <windows.h>
#include <shlobj.h>
#include <dbghelp.h>
#define PATH_SEPARATOR '\\'
#define ALL_PATH_SEPARATORS "\\/"
#else
/// the default path separator for this OS
#define PATH_SEPARATOR '/'
/// a string containing all possible path separators for this OS
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
/// UTF-32 character
///
/// OpenBSD has `uchar.h` but it doesn't seem to define `char32_t` ? (as of writing)
typedef uint32_t char32_t;
#endif

#if !__TINYC__ && __STDC_VERSION__ >= 201112
#define static_assert_if_possible(cond) _Static_assert(cond, "Static assertion failed");
#else
/// perform static assertion if it's available.
#define static_assert_if_possible(cond)
#endif

/// 8-bit unsigned integer
typedef uint8_t  u8;
/// 16-bit unsigned integer
typedef uint16_t u16;
/// 32-bit unsigned integer
typedef uint32_t u32;
/// 64-bit unsigned integer
typedef uint64_t u64;

/// maximum value of \ref u8
#define U8_MAX  0xff
/// maximum value of \ref u16
#define U16_MAX 0xffff
/// maximum value of \ref u32
#define U32_MAX 0xffffffff
/// maximum value of \ref u64
#define U64_MAX 0xffffffffffffffff

/// 8-bit signed integer
typedef int8_t  i8;
/// 16-bit signed integer
typedef int16_t i16;
/// 32-bit signed integer
typedef int32_t i32;
/// 64-bit signed integer
typedef int64_t i64;

/// minimum value of \ref i8
#define I8_MIN ((i8)0x80)
/// minimum value of \ref i16
#define I16_MIN ((i16)0x8000)
/// minimum value of \ref i32
#define I32_MIN ((i32)0x80000000)
/// minimum value of \ref i64
#define I64_MIN ((i64)0x8000000000000000)
/// maximum value of \ref i8
#define I8_MAX  0x7f
/// maximum value of \ref i16
#define I16_MAX 0x7fff
/// maximum value of \ref i32
#define I32_MAX 0x7fffffff
/// maximum value of \ref i64
#define I64_MAX 0x7fffffffffffffff

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
/// add warn-if-unused attribute if it's available.
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

/// this type is an alias for `bool`, except that it
/// produces a warning if it's not used.
///
/// `false` = error, `true` = success
#define Status bool WarnUnusedResult

/// number of elements in static array
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
///  disable compiler warnings temporarily
#define no_warn_start
///  reenable compiler warnings
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
/// print to `stdout`, or debugger output on Windows
#define print printf
/// print to `stderr`, or debugger output on Windows
#define eprint(...) fprintf(stderr, __VA_ARGS__)
#endif
/// like \ref print, but adds a newline
#define println(...) print(__VA_ARGS__), print("\n")
/// like \ref eprint, but adds a newline
#define eprintln(...) eprint(__VA_ARGS__), eprint("\n")

#if DEBUG
#define debug_print print
#define debug_println println
#else
/// like \ref print, but only enabled in debug mode
#define debug_print(...)
/// like \ref println, but only enabled in debug mode
#define debug_println(...)
#endif

#endif // BASE_H_
