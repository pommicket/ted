// used to speed things up for debug build
// just exports everything in stb_truetype.h


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

#if !DEBUG
#define STBTT_STATIC
#define STBRP_STATIC
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION

no_warn_start
#include "lib/stb_rect_pack.h"
#include "lib/stb_truetype.h"
no_warn_end
