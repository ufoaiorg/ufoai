#pragma once

#include <SDL_mutex.h>

/**
 * Ensures that a mutex is released once the scope is left
 */
class ScopedMutex {
private:
	SDL_mutex* _mutex;
public:
	ScopedMutex (SDL_mutex *mutex) :
			_mutex(mutex)
	{
		SDL_LockMutex(_mutex);
	}

	~ScopedMutex ()
	{
		SDL_UnlockMutex(_mutex);
	}
};
