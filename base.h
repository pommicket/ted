#ifndef BASE_H_
#define BASE_H_

#ifndef DEBUG
#define NDEBUG 1
#endif

#if _WIN32
#include <windows.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned int  uint;
typedef unsigned long ulong;

#ifdef __GNUC__
#define WarnUnusedResult __attribute__((warn_unused_result))
#else
#define WarnUnusedResult
#endif

#define Status bool WarnUnusedResult // false = error, true = success


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

#if DEBUG
#if __unix__
#define debug_println(...) printf(__VA_ARGS__), printf("\n")
#else // __unix__
static void debug_println(char const *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf_s(buf, sizeof buf, fmt, args);
	va_end(args);
	OutputDebugStringA(buf);
	OutputDebugStringA("\n");
}
#endif // __unix__
#else // DEBUG
#define debug_println(...)
#endif

#endif
