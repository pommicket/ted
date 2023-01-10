// used for debug build to speed things up
// just exports everything in stb_image.h

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

#define STB_IMAGE_IMPLEMENTATION
#if __TINYC__
#define STBI_NO_SIMD
#endif
no_warn_start
#include "lib/stb_image.h"
no_warn_end
