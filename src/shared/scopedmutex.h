#pragma once

#include <SDL_mutex.h>

/**
 * Ensures that a mutex is released once the scope is left
 */
class ScopedMutex {
public:
	explicit ScopedMutex (SDL_mutex *mutex) :
			_mutex(mutex)
	{
		SDL_LockMutex(_mutex);
	}

	~ScopedMutex ()
	{
		SDL_UnlockMutex(_mutex);
	}

	ScopedMutex (const ScopedMutex&) = delete;
	ScopedMutex& operator= (const ScopedMutex&) = delete;

private:
	SDL_mutex* _mutex;
};
