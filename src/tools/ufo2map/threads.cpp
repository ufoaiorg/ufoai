/**
 * @file
 */

/*
Copyright(c) 1997-2001 Id Software, Inc.
Copyright(c) 2002 The Quakeforge Project.
Copyright(c) 2006 Quake2World.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "bsp.h"
#include "../../shared/thread.h"

#define	MAX_THREADS	8

threadstate_t threadstate;

/**
 * @brief Return an iteration of work, updating progress when appropriate.
 */
static int GetThreadWork (void)
{
	int	r;
	int	f;

	ThreadLock();

	if (threadstate.workindex == threadstate.workcount) {  /* done */
		ThreadUnlock();
		return -1;
	}

	/* update work fraction and output progress if desired */
	f = 10 * threadstate.workindex / threadstate.workcount;
	if (f != threadstate.workfrac) {
		threadstate.workfrac = f;
		if (threadstate.progress) {
			fprintf(stdout, "%i...", f);
			fflush(stdout);
		}
	} else if (threadstate.progress && threadstate.workindex % threadstate.worktick == 0) {
		fprintf(stdout, "%c\b", "-\\|/"[(threadstate.workindex / threadstate.worktick) & 3]);
		fflush(stdout);
	}

	/* assign the next work iteration */
	r = threadstate.workindex;
	threadstate.workindex++;

	ThreadUnlock();

	return r;
}


/** @brief Generic function pointer to actual work to be done */
static void (*WorkFunction) (unsigned int);


/**
 * @brief Shared work entry point by all threads.  Retrieve and perform
 * chunks of work iteratively until work is finished.
 */
static int ThreadWork (void* p)
{
	while (true) {
		int work = GetThreadWork();
		if (work == -1)
			break;
		WorkFunction(work);
	}

	return 0;
}


static SDL_mutex *lock = nullptr;

static void ThreadInit (void)
{
	if (lock != nullptr)
		Sys_Error("Mutex already created!");

	lock = SDL_CreateMutex();
}

static void ThreadRelease (void)
{
	SDL_DestroyMutex(lock);
	lock = nullptr;
}

/**
 * @brief Lock the shared data by the calling thread.
 */
void ThreadLock (void)
{
	if (threadstate.numthreads == 1) {
		/* do nothing */
	} else if (lock != nullptr && SDL_LockMutex(lock) != -1) {
		/* already locked */
	} else {
		Sys_Error("Couldn't lock mutex (%p)!", (void*)lock);
	}
}

/**
 * @brief Release the lock on the shared data.
 */
void ThreadUnlock (void)
{
	if (threadstate.numthreads == 1) {
		/* do nothing */
	} else if (lock != nullptr && SDL_UnlockMutex(lock) != -1) {
		/* already locked */
	} else {
		Sys_Error("Couldn't unlock mutex (%p)!", (void*)lock);
	}
}

static void RunThreads (void)
{
	SDL_Thread *threads[MAX_THREADS];
	int i;

	if (threadstate.numthreads == 1) {
		ThreadWork(nullptr);
		return;
	}

	ThreadInit();

	for (i = 0; i < threadstate.numthreads; i++)
		threads[i] = Com_CreateThread(ThreadWork, "CompileWorker");

	for (i = 0; i < threadstate.numthreads; i++)
		SDL_WaitThread(threads[i], nullptr);

	ThreadRelease();
}


/**
 * @brief Entry point for all thread work requests.
 */
void RunThreadsOn (void (*func)(unsigned int), int unsigned workcount, bool progress, const char* id)
{
	int start, end;

	if (threadstate.numthreads < 1)  /* ensure safe thread counts */
		threadstate.numthreads = 1;

	if (threadstate.numthreads > MAX_THREADS)
		threadstate.numthreads = MAX_THREADS;

	threadstate.workindex = 0;
	threadstate.workcount = workcount;
	threadstate.workfrac = -1;
	threadstate.progress = progress;
	threadstate.worktick = sqrt(workcount) + 1;

	WorkFunction = func;

	start = time(nullptr);

	if (threadstate.progress) {
		fprintf(stdout, "%10s: ", id);
		fflush(stdout);
	}

	RunThreads();

	end = time(nullptr);

	if (threadstate.progress) {
		Verb_Printf(VERB_NORMAL, " (time: %6is, #: %i)\n", end - start, workcount);
	}
}

/**
 * @brief Entry point for all thread work requests.
 */
void RunSingleThreadOn (void (*func)(unsigned int), int unsigned workcount, bool progress, const char* id)
{
	int saved_numthreads = threadstate.numthreads;

	threadstate.numthreads = 1;

	RunThreadsOn(func, workcount, progress, id);

	threadstate.numthreads = saved_numthreads;
}
