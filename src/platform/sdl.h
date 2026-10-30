#pragma once

// The application provides a plain main(); tell SDL not to wrap it. This must
// be defined before SDL headers are included.
#ifndef SDL_MAIN_HANDLED
#  define SDL_MAIN_HANDLED
#endif

// SDL2 ships as <SDL2/SDL.h> on most Linux packages and as <SDL.h> in vcpkg
// and the official Windows development archive. Accept either layout.
#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>)
#    include <SDL2/SDL.h>
#  else
#    include <SDL.h>
#  endif
#else
#  include <SDL.h>
#endif
