#ifndef UTIL_C_
#define UTIL_C_

#include "base.h"

static u32 util_popcount(u64 x) {
#ifdef __GNUC__
	return (u32)__builtin_popcountll(x);
#else
	u32 count = 0;
	while (x) {
		x &= x-1;
		++count;
	}
	return count;
#endif
}

static bool util_is_power_of_2(u64 x) {
	return util_popcount(x) == 1;
}

static void util_zero_memory(void *mem, size_t size) {
	extern void *memset(void *s, int c, size_t n);
	memset(mem, 0, size);
}

static double util_maxd(double a, double b) {
	return a > b ? a : b;
}

static double util_mind(double a, double b) {
	return a < b ? a : b;
}

#endif
