#ifndef _SHARED_MUTEX_H
#define _SHARED_MUTEX_H

#include <SDL_thread.h>

struct threads_mutex_s;
typedef struct threads_mutex_s threads_mutex_t;

int TH_MutexUnlock(threads_mutex_t *mutex);
int TH_MutexLock(threads_mutex_t *mutex);
threads_mutex_t *TH_MutexCreate(const char *name);
void TH_MutexDestroy(threads_mutex_t *mutex);
int TH_MutexCondWait(threads_mutex_t *mutex, SDL_cond *condition);
int TH_MutexCondWaitTimeout (threads_mutex_t *mutex, SDL_cond *condition, int timeout);

#endif
