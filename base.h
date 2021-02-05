#ifndef BASE_H_
#define BASE_H_

#ifndef DEBUG
#define NDEBUG 1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if _WIN32
#include <windows.h>
#include <shlobj.h>
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
#include <stddef.h>
#include <stdarg.h>
#include <float.h>
#include <limits.h>
#include <assert.h>
#include <uchar.h>

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
#define I8_MAX  0x7f
#define I16_MAX 0x7fff
#define I32_MAX 0x7fffffff
#define I64_MAX 0x7fffffffffffffff

typedef unsigned int  uint;
typedef unsigned long ulong;

typedef long long llong;
typedef unsigned long long ullong;

// allows 
// switch (c) {
//     case ANY_DIGIT:
//        ...
// }
#define ANY_DIGIT '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'

#if __clang__
#define ENUM_U8 typedef enum : u8
#define ENUM_U8_END(name) name
#else
#define ENUM_U8 enum
#define ENUM_U8_END(name) ; typedef u8 name
#endif

#if __clang__
#define ENUM_U16 typedef enum : u16
#define ENUM_U16_END(name) name
#else
#define ENUM_U16 enum
#define ENUM_U16_END(name) ; typedef u16 name
#endif

#ifdef __GNUC__
#define WarnUnusedResult __attribute__((warn_unused_result))
#else
#define WarnUnusedResult
#endif

#define Status bool WarnUnusedResult // false = error, true = success

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
static void print(char const *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf_s(buf, sizeof buf, fmt, args);
	va_end(args);
	OutputDebugStringA(buf);
}
#else
#define print printf
#endif
#define println(...) print(__VA_ARGS__), print("\n")

#if DEBUG
#define debug_print print
#define debug_println println
#else
#define debug_print(...)
#define debug_println(...)
#endif

#endif
