#pragma once

#include <SDL.h>

inline SDL_Thread *Com_CreateThread (int (*fn)(void *), const char *name, void *data = nullptr)
{
		return SDL_CreateThread(fn, name, data);
}
