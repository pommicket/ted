/* VARIOUS DATA STRUCTURES
- dynamic array
- string builder
- string hash table
*/


// functions in this file suffixed with _ are not meant to be used outside here, unless you
// know what you're doing

// IMPORTANT NOTE: If you are using this with structures containing `long double`s, do
//  #define ARR_LONG_DOUBLE
// before including this file
// ( otherwise the long doubles will not be aligned.
//   this does mean that arrays waste 8 bytes of memory.
//    which isnt important unless you're making a lot of arrays.)

#include <stddef.h>
typedef union {
	long num;
	void *ptr;
	void (*fnptr)(void);
#ifdef ARR_LONG_DOUBLE
	long
#endif
	double flt;
} ArrMaxAlign;
#if __STDC_VERSION__ < 199901L && !defined inline
#define inline
#endif


typedef struct {
	u32 len;
	u32 cap;
	ArrMaxAlign data[];
} ArrHeader;

// watch out! do not call this function if arr is NULL.
static inline ArrHeader *arr_hdr_(void *arr) {
	return (ArrHeader *)((char *)arr - offsetof(ArrHeader, data));
}

static inline u32 arr_len(const void *arr) {
	return arr ? arr_hdr_((void*)arr)->len : 0;
}

static inline u32 arr_cap(void *arr) {
	return arr ? arr_hdr_(arr)->cap : 0;
}

static inline unsigned arr_lenu(void *arr) {
	return (unsigned)arr_len(arr);
}

// grow array to fit one more member
static void *arr_grow1_(void *arr, size_t member_size) {
	if (arr) {
		ArrHeader *hdr = arr_hdr_(arr);
		if (hdr->len >= hdr->cap) {
			u32 new_capacity = hdr->cap * 2;
			ArrHeader *old_hdr = hdr;
			hdr = (ArrHeader *)realloc(old_hdr, sizeof(ArrHeader) + new_capacity * member_size);
			if (hdr) {
				hdr->cap = new_capacity;
			} else {
				free(old_hdr);
				return NULL;
			}
		}
		return hdr->data;
	} else {
		// create a new array
		u32 initial_capacity = 2; // allocate enough space for two members
		ArrHeader *ret = (ArrHeader *)calloc(1, sizeof(ArrHeader) + initial_capacity * member_size);
		if (ret) {
			ret->cap = initial_capacity;
			return ret->data;
		} else {
			return NULL;
		}
	}
}

static inline void *arr_add_ptr_(void **arr, size_t member_size) {
	u8 *ret;
	*arr = arr_grow1_(*arr, member_size);
	if (*arr) {
		ret = (u8 *)*arr + member_size * (arr_hdr_(*arr)->len++);
		memset(ret, 0, member_size);
	} else {
		ret = NULL;
	}
	return ret;
}

static void arr_reserve_(void **arr, size_t member_size, size_t n) {
	if (n >= U32_MAX-1) { 
		// too big; free arr.
		if (*arr) free(arr_hdr_(*arr));
		*arr = NULL;
	}

	if (!*arr) {
		// create a new array with capacity n+1
		// why n+1? i dont know i wrote this a while ago
		ArrHeader *hdr = calloc(1, sizeof(ArrHeader) + (n+1) * member_size);
		if (hdr) {
			hdr->cap = (u32)n+1;
			*arr = hdr->data;
		}
	} else {
		// increase capacity of array
		ArrHeader *hdr = arr_hdr_(*arr);
		u32 curr_cap = hdr->cap;
		if (n > curr_cap) {
			ArrHeader *old_hdr = hdr;
			while (n > curr_cap) {
				if (curr_cap < U32_MAX/2)
					curr_cap *= 2;
				else
					curr_cap = U32_MAX;
			}
			hdr = realloc(hdr, sizeof(ArrHeader) + curr_cap * member_size);
			if (hdr) {
				hdr->cap = curr_cap;
			} else {
				// growing failed
				free(old_hdr);
				*arr = NULL;
				return;
			}
		}
		*arr = hdr->data;
	}
}

static void arr_set_len_(void **arr, size_t member_size, size_t n) {
	arr_reserve_(arr, member_size, n);
	if (*arr) {
		ArrHeader *hdr = arr_hdr_(*arr);
		if (n > hdr->len) {
			// zero new elements
			memset((char *)hdr->data + hdr->len * member_size, 0, (n - hdr->len) * member_size);
		}
		hdr->len = (u32)n;
	}
}

static void *arr_remove_(void *arr, size_t member_size, size_t index) {
	ArrHeader *hdr = arr_hdr_(arr);
	assert(index < hdr->len);
	memmove((char *)arr + index * member_size, (char *)arr + (index+1) * member_size, (hdr->len - (index+1)) * member_size);
	if (--hdr->len == 0) {
		free(hdr);
		return NULL;
	} else {
		return arr;
	}
}

#ifdef __cplusplus
#define arr_cast_typeof(a) (decltype(a))
#elif defined __GNUC__
#define arr_cast_typeof(a) (__typeof__(a))
#else
#define arr_cast_typeof(a)
#endif

#define arr__join2(a,b) a##b
#define arr__join(a,b) arr__join2(a,b) // macro used internally
// if the array is not NULL, free it and set it to NULL
#define arr_free(a) do { if (a) { free(arr_hdr_(a)); (a) = NULL; } } while (0)
// a nice alias
#define arr_clear(a) arr_free(a)
// add an item to the array - if allocation fails, the array will be freed and set to NULL.
// (how this works: if we can successfully grow the array, increase the length and add the item.)
#define arr_add(a, x) do { if (((a) = arr_cast_typeof(a) arr_grow1_((a), sizeof *(a)))) ((a)[arr_hdr_(a)->len++] = (x)); } while (0)
// like arr_add, but instead of passing it the value, it returns a pointer to the value. returns NULL if allocation failed.
// the added item will be zero-initialized.
#define arr_addp(a) arr_cast_typeof(a) arr_add_ptr_((void **)&(a), sizeof *(a))
// set the length of `a` to `n`, increasing the capacity if necessary.
// the newly-added elements are zero-initialized.
#define arr_qsort(a, cmp) qsort((a), arr_len(a), sizeof *(a), (cmp))
#define arr_remove_last(a) do { assert(a); if (--arr_hdr_(a)->len == 0) arr_free(a); } while (0)
#define arr_remove(a, i) (void)((a) = arr_remove_((a), sizeof *(a), (i)))
#define arr_insert(a, i, x) do { u32 _index = (i); (a) = arr_cast_typeof(a) arr_grow1_((a), sizeof *(a)); \
	if (a) { memmove((a) + _index + 1, (a) + _index, (arr_len(a) - _index) * sizeof *(a));\
	(a)[_index] = x; \
	++arr_hdr_(a)->len; } } while (0)
#define arr_pop_last(a) ((a)[--arr_hdr_(a)->len])
#define arr_size_in_bytes(a) (arr_len(a) * sizeof *(a))
#define arr_lastp(a) ((a) ? &(a)[arr_len(a)-1] : NULL)
#define arr_foreach_ptr_end(a, type, var, end) type *end = (a) + arr_len(a); \
	for (type *var = (a); var != end; ++var)
// Iterate through each element of the array, setting var to a pointer to the element.
// You can't use this like, e.g.:
// if (something)
//   arr_foreach_ptr(a, int, i);
// You'll get an error. You will need to use braces because it expands to multiple statements.
// (we need to name the end pointer something unique, which is why there's that arr__join thing
// we can't just declare it inside the for loop, because type could be something like char *.)
#define arr_foreach_ptr(a, type, var) arr_foreach_ptr_end(a, type, var, arr__join(_foreach_end,__LINE__))

#define arr_reverse(a, type) do { \
	u64 _i, _len = arr_len(a); \
	for (_i = 0; 2*_i < _len; ++_i) { \
		type *_x = &(a)[_i]; \
		type *_y = &(a)[_len-1-_i]; \
		type _tmp; \
		_tmp = *_x; \
		*_x = *_y; \
		*_y = _tmp; \
	} \
	} while (0)

// Ensure that enough space is allocated for n elements.
#define arr_reserve(a, n) arr_reserve_((void **)&(a), sizeof *(a), (n)) 
// Similar to arr_reserve, but also sets the length of the array to n.
#define arr_set_len(a, n) arr_set_len_((void **)&(a), sizeof *(a), (n))

#if 0
static void arrcstr_append_strn_(char **a, char const *s, size_t s_len) {
	size_t curr_len = arr_len(*a);
	size_t new_len = curr_len + s_len;
	arr_reserve(*a, new_len + 1);
	arr_set_len(*a, new_len);
	memcpy(*a + curr_len, s, s_len);
	(*a)[curr_len + s_len] = '\0';
}

static void arrcstr_shrink_(char **a, u32 new_len) {
	ArrHeader *hdr = arr_hdr_(*a);
	assert(hdr->cap > new_len);
	hdr->len = new_len;
	(*a)[new_len] = '\0';
}

// append to a C-string array
#define arrcstr_append_str(a, s) arrcstr_append_strn_(&(a), (s), strlen(s))
// take at most n bytes from s
#define arrcstr_append_strn(a, s, n) arrcstr_append_strn_(&(a), (s), (n))
// make the string smaller
#define arrcstr_shrink(a, n) arrcstr_shrink_(&(a), (n))
#endif

#ifndef NDEBUG
static void arr_test(void) {
	u32 *arr = NULL;
	u32 i;
	assert(arr_len(arr) == 0);
	for (i = 0; i < 10000; ++i) {
		arr_add(arr, i*i);
	}
	assert(arr_len(arr) == 10000);
	arr_remove_last(arr);
	assert(arr_len(arr) == 9999);
	for (i = 0; i < arr_len(arr); ++i)
		assert(arr[i] == i*i);
	while (arr_len(arr))
		arr_remove_last(arr);
	assert(arr_len(arr) == 0);
}
#endif

typedef struct {
	// dynamic array, including a null byte.
	char *str;
} StrBuilder;

void str_builder_create(StrBuilder *builder) {
	memset(builder, 0, sizeof *builder);
	arr_add(builder->str, 0);
}

StrBuilder str_builder_new(void) {
	StrBuilder ret = {0};
	str_builder_create(&ret);
	return ret;
}

void str_builder_free(StrBuilder *builder) {
	arr_free(builder->str);
}

void str_builder_clear(StrBuilder *builder) {
	str_builder_free(builder);
	str_builder_create(builder);
}

void str_builder_append(StrBuilder *builder, const char *s) {
	assert(builder->str);
	
	size_t s_len = strlen(s);
	size_t prev_size = arr_len(builder->str);
	size_t prev_len = prev_size - 1; // null terminator
	// note: this zeroes the newly created elements, so we have a new null terminator
	arr_set_len(builder->str, prev_size + s_len);
	memcpy(builder->str + prev_len, s, s_len);
}

void str_builder_appendf(StrBuilder *builder, PRINTF_FORMAT_STRING const char *fmt, ...) ATTRIBUTE_PRINTF(2, 3);
void str_builder_appendf(StrBuilder *builder, const char *fmt, ...) {
	// idk if you can always just pass NULL to vsnprintf
	va_list args;
	char fakebuf[2] = {0};
	va_start(args, fmt);
	int ret = vsnprintf(fakebuf, 1, fmt, args);
	va_end(args);
	
	if (ret < 0) return; // bad format or something
	u32 n = (u32)ret;
	
	size_t prev_size = arr_len(builder->str);
	size_t prev_len = prev_size - 1; // null terminator
	arr_set_len(builder->str, prev_size + n);
	va_start(args, fmt);
	vsnprintf(builder->str + prev_len, n + 1, fmt, args);
	va_end(args);
}

// append n null bytes.
void str_builder_append_null(StrBuilder *builder, size_t n) {
	arr_set_len(builder->str, arr_len(builder->str) + n);
}

u32 str_builder_len(StrBuilder *builder) {
	assert(builder->str);
	return arr_len(builder->str) - 1;
}

char *str_builder_get_ptr(StrBuilder *builder, size_t index) {
	assert(index <= str_builder_len(builder));
	return &builder->str[index];
}

void str_builder_shrink(StrBuilder *builder, size_t new_len) {
	if (new_len > str_builder_len(builder)) {
		assert(0);
		return;
	}
	arr_set_len(builder->str, new_len + 1);
}

