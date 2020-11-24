#ifndef UTIL_C_
#define UTIL_C_

#include "base.h"

static uint util_popcount(u64 x) {
	// @TODO: portability
	return (uint)__builtin_popcountll(x);
}

static bool util_is_power_of_2(u64 x) {
	return util_popcount(x) == 1;
}

static void util_zero_memory(void *mem, size_t size) {
	extern void *memset(void *s, int c, size_t n);
	memset(mem, 0, size);
}

#endif
