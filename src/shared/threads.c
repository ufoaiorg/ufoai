/**
 * @file threads.c
 * @brief Thread function wrappers
 * @note Manage a priority queue as a heap - the heap is implemented as an array.
 */

#include "threads.h"
#include "../common/common.h"
#include "../common/mem.h"

/**
 * @brief Lock the mutex
 * @return @c -1 on error, @c 0 on success
 */
int _TH_MutexLock (threads_mutex_t *mutex, const char *file, int line)
{
	if (mutex != NULL) {
		const int ret = SDL_LockMutex(mutex->mutex);
		if (ret == -1)
			Sys_Error("Error locking mutex '%s' from %s:%i", mutex->name, file, line);
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
			Sys_Error("Error unlocking mutex '%s' from %s:%i", mutex->name, file, line);
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
		Sys_Error("Could not create mutex");
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
