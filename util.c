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

static bool streq(char const *a, char const *b) {
	return strcmp(a, b) == 0;
}
