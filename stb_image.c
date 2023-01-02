// used for debug build to speed things up
// just exports everything in stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#if __TINYC__
#define STBI_NO_SIMD
#endif
#include "lib/stb_image.h"
