/* Debian multiarch ships only the x86_64 SDL configuration header; the i386
 * runtime library is ABI-compatible apart from the pointer size. */
#include "/usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h"
#undef SIZEOF_VOIDP
#define SIZEOF_VOIDP 4
