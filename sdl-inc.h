// this file includes SDL.h.
// you should include this rather than including SDL.h directly.

#ifndef SDL_INC_H_
#define SDL_INC_H_


#if DEBUG || __TINYC__ // speed up compile time on debug, also tcc doesn't have immintrin.h
#define SDL_DISABLE_IMMINTRIN_H
#endif

no_warn_start
#if _WIN32
	#include <SDL.h>
#else
	#include <SDL2/SDL.h>
#endif
no_warn_end

#endif // SDL_INC_H_
