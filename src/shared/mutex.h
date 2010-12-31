#ifndef _SHARED_MUTEX_H
#define _SHARED_MUTEX_H

#include <SDL_thread.h>

#define TH_MutexUnlock(mutex)	_TH_MutexUnlock((mutex), __FILE__, __LINE__)
#define TH_MutexLock(mutex)		_TH_MutexLock((mutex), __FILE__, __LINE__)

struct threads_mutex_s;
typedef struct threads_mutex_s threads_mutex_t;

int _TH_MutexUnlock(threads_mutex_t *mutex, const char *file, int line);
int _TH_MutexLock(threads_mutex_t *mutex, const char *file, int line);
threads_mutex_t *TH_MutexCreate(const char *name);
void TH_MutexDestroy(threads_mutex_t *mutex);
int TH_MutexCondWait(threads_mutex_t *mutex, SDL_cond *condition);
int TH_MutexCondWaitTimeout (threads_mutex_t *mutex, SDL_cond *condition, int timeout);

#endif
