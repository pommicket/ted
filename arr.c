#ifndef ARR_C_
#define ARR_C_
/*
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
For more information, please refer to <http://unlicense.org/>
*/

// functions in this file suffixed with _ are not meant to be used outside here, unless you
// know what you're doing

// IMPORTANT NOTE: If you are using this with structures containing `long double`s, do
//  #define ARR_LONG_DOUBLE
// before including this file

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

static inline u32 arr_len(void *arr) {
	return arr ? arr_hdr_(arr)->len : 0;
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
			memset((char *)hdr->data + hdr->len, 0, (n - hdr->len) * member_size);
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

#endif // ARR_C_
