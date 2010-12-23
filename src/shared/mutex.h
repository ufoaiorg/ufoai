#ifndef _SHARED_THREADS_H
#define _SHARED_THREADS_H

#include <SDL_thread.h>

/** @brief mutex wrapper */
typedef struct threads_mutex_s {
	int line;				/**< line it was last locked */
	const char *file;		/**< filename it was last locked */
	const char *name;		/**< name of the mutex */
	SDL_mutex *mutex;		/**< the real sdl mutex */
} threads_mutex_t;

#define TH_MutexUnlock(mutex)	_TH_MutexUnlock((mutex), __FILE__, __LINE__)
#define TH_MutexLock(mutex)		_TH_MutexLock((mutex), __FILE__, __LINE__)

int _TH_MutexUnlock(threads_mutex_t *mutex, const char *file, int line);
int _TH_MutexLock(threads_mutex_t *mutex, const char *file, int line);
threads_mutex_t *TH_MutexCreate(const char *name);
void TH_MutexDestroy(threads_mutex_t *mutex);

#endif
