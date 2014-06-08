#pragma once

#include <SDL.h>

inline SDL_Thread *Com_CreateThread (int (*fn)(void *), const char *name, void *data = nullptr)
{
#if SDL_VERSION_ATLEAST(2,0,0)
		return SDL_CreateThread(fn, name, data);
#else
		return SDL_CreateThread(fn, data);
#endif
}
