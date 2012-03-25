/**
 * @file mutex.c
 * @brief SDL mutex function wrappers
 */

#include "mutex.h"
#include "../common/common.h"

/** @brief mutex wrapper */
struct threads_mutex_s {
	int line;				/**< line it was last locked */
	const char *file;		/**< filename it was last locked */
	const char *name;		/**< name of the mutex */
	SDL_mutex *mutex;		/**< the real sdl mutex */
};

/**
 * @brief Lock the mutex
 * @return @c -1 on error, @c 0 on success
 */
int _TH_MutexLock (threads_mutex_t *mutex, const char *file, int line)
{
	if (mutex != NULL) {
		const int ret = SDL_LockMutex(mutex->mutex);
		if (ret == -1)
			Sys_Error("Error locking mutex '%s' from %s:%i (%s)", mutex->name, file, line, SDL_GetError());
		mutex->file = file;
		mutex->line = line;
		return ret;
	}
	return -1;
}

/**
 * @brief Unlock the mutex
 * @return @c -1 on error, @c 0 on success
 */
int _TH_MutexUnlock (threads_mutex_t *mutex, const char *file, int line)
{
	if (mutex != NULL) {
		const int ret = SDL_UnlockMutex(mutex->mutex);
		if (ret == -1)
			Sys_Error("Error unlocking mutex '%s' from %s:%i (%s)", mutex->name, file, line, SDL_GetError());
		mutex->file = NULL;
		mutex->line = -1;
		return ret;
	}
	return -1;
}

/**
 * @brief Creates unlocked mutex
 * @param name The name of the mutex
 */
threads_mutex_t *TH_MutexCreate (const char *name)
{
	threads_mutex_t *mutex = (threads_mutex_t *)malloc(sizeof(*mutex));
	mutex->mutex = SDL_CreateMutex();
	if (mutex->mutex == NULL)
		Sys_Error("Could not create mutex (%s)", SDL_GetError());
	mutex->name = name;
	return mutex;
}

void TH_MutexDestroy (threads_mutex_t *mutex)
{
	if (mutex) {
		SDL_DestroyMutex(mutex->mutex);
		free(mutex);
	}
}

int TH_MutexCondWait (threads_mutex_t *mutex, SDL_cond *condition)
{
	if (mutex == NULL)
		return -1;
	return SDL_CondWait(condition, mutex->mutex);
}

int TH_MutexCondWaitTimeout (threads_mutex_t *mutex, SDL_cond *condition, int timeout)
{
	if (mutex == NULL)
		return -1;
	return SDL_CondWaitTimeout(condition, mutex->mutex, timeout);
}
