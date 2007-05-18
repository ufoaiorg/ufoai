/**
 * @file threads.c
 * @brief
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include "cmdlib.h"
#include "threads.h"

#define	MAX_THREADS	64

static int dispatch;
static int workcount;
static int oldf;
static qboolean pacifier;
static qboolean threaded;

/**
 * @brief
 * @sa ThreadWorkerFunction
 */
static int GetThreadWork (void)
{
	int	r;
	int	f;

	ThreadLock ();

	if (dispatch == workcount) {
		ThreadUnlock ();
		return -1;
	}

	f = 10 * dispatch / workcount;
	if (f != oldf) {
		oldf = f;
		if (pacifier) {
			fprintf(stdout, "%i...", f);
			fflush(stdout);
		}
	}

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
}


void (*workfunction) (unsigned int);

/**
 * @brief
 * @sa GetThreadWork
 */
static void ThreadWorkerFunction (unsigned int threadnum)
{
	int		work;

	while (1) {
		work = GetThreadWork ();
		if (work == -1)
			break;
/* 		Sys_Printf ("thread %i, work %i\n", threadnum, work); */
		workfunction(work);
	}
}

/**
 * @brief
 */
extern void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	if (numthreads == -1)
		ThreadSetDefault ();
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
}


/*
===================================================================
WIN32
===================================================================
*/
#ifdef _WIN32

#define	USED

#include <windows.h>

int numthreads = -1;
CRITICAL_SECTION crit;
static int enter;

/**
 * @brief
 */
void ThreadSetDefault (void)
{
	SYSTEM_INFO info;
/*	DWORD_PTR procAffinity = 0;*/
/*	HANDLE proc = GetCurrentProcess();*/

	if (numthreads == -1) {	/* not set manually */
		GetSystemInfo(&info);
		numthreads = info.dwNumberOfProcessors;
		if (numthreads < 1 || numthreads > 32) {
			numthreads = 1;
			/*SetProcessAffinityMask(proc, procAffinity);*/
		}
	}
/*	CloseHandle(proc);*/

	Sys_FPrintf(SYS_VRB, "%i threads\n", numthreads);
}


/**
 * @brief
 */
void ThreadLock (void)
{
	if (!threaded)
		return;
	EnterCriticalSection (&crit);
	if (enter)
		Error ("Recursive ThreadLock\n");
	enter = 1;
}

/**
 * @brief
 */
void ThreadUnlock (void)
{
	if (!threaded)
		return;
	if (!enter)
		Error ("ThreadUnlock without lock\n");
	enter = 0;
	LeaveCriticalSection (&crit);
}

/**
 * @brief
 */
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	DWORD	threadid[MAX_THREADS];
	HANDLE	threadhandle[MAX_THREADS];
	int		i;
	int		start, end;

	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = qtrue;

	/* run threads in parallel */
	InitializeCriticalSection (&crit);

	if (numthreads == 1) {	/* use same thread */
		func (0);
	} else {
		for (i = 0; i < numthreads; i++) {
			threadhandle[i] = CreateThread(
			   NULL,	/* LPSECURITY_ATTRIBUTES lpsa, */
			   (4096 * 1024),		/* DWORD cbStack, */
			   (LPTHREAD_START_ROUTINE)func,	/* LPTHREAD_START_ROUTINE lpStartAddr, */
			   (LPVOID)i,	/* LPVOID lpvThreadParm, */
			   0,			/*   DWORD fdwCreate, */
			   &threadid[i]);
		}

		for (i = 0; i < numthreads; i++)
			WaitForSingleObject (threadhandle[i], INFINITE);
	}
	DeleteCriticalSection (&crit);

	threaded = qfalse;
	end = I_FloatTime ();
	if (pacifier)
		Sys_Printf (" (%i)\n", end-start);
}


#endif

/*
===================================================================
OSF1
===================================================================
*/

#ifdef __osf__
#define	USED

int numthreads = 4;

/**
 * @brief
 */
void ThreadSetDefault (void)
{
	if (numthreads == -1) {	/* not set manually */
		numthreads = 4;
	}
}


#include <pthread.h>

pthread_mutex_t	*my_mutex;

/**
 * @brief
 */
void ThreadLock (void)
{
	if (my_mutex)
		pthread_mutex_lock (my_mutex);
}

/**
 * @brief
 */
void ThreadUnlock (void)
{
	if (my_mutex)
		pthread_mutex_unlock (my_mutex);
}

/**
 * @brief
 */
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	int		i;
	pthread_t	work_threads[MAX_THREADS];
	pthread_addr_t	status;
	pthread_attr_t	attrib;
	pthread_mutexattr_t	mattrib;
	int		start, end;

	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = qtrue;

	if (pacifier)
		setbuf (stdout, NULL);

	if (!my_mutex) {
		my_mutex = malloc (sizeof(*my_mutex));
		if (pthread_mutexattr_create (&mattrib) == -1)
			Error ("pthread_mutex_attr_create failed");
		if (pthread_mutexattr_setkind_np (&mattrib, MUTEX_FAST_NP) == -1)
			Error ("pthread_mutexattr_setkind_np failed");
		if (pthread_mutex_init (my_mutex, mattrib) == -1)
			Error ("pthread_mutex_init failed");
	}

	if (pthread_attr_create(&attrib) == -1)
		Error ("pthread_attr_create failed");
	if (pthread_attr_setstacksize(&attrib, 0x100000) == -1)
		Error ("pthread_attr_setstacksize failed");

	for (i = 0; i < numthreads; i++) {
  		if (pthread_create(&work_threads[i], attrib
		, (pthread_startroutine_t)func, (pthread_addr_t)i) == -1)
			Error ("pthread_create failed");
	}

	for (i = 0; i < numthreads; i++) {
		if (pthread_join (work_threads[i], &status) == -1)
			Error ("pthread_join failed");
	}

	threaded = qfalse;

	end = I_FloatTime();
	if (pacifier)
		Sys_Printf(" (%i)\n", end-start);
}


#endif

/*
===================================================================
IRIX
===================================================================
*/

#ifdef _MIPS_ISA
#define	USED

#include <task.h>
#include <abi_mutex.h>
#include <sys/types.h>
#include <sys/prctl.h>


int numthreads = -1;
abilock_t lck;

/**
 * @brief
 */
void ThreadSetDefault (void)
{
	if (numthreads == -1)
		numthreads = prctl(PR_MAXPPROCS);
	Sys_Printf ("%i threads\n", numthreads);
	usconfig (CONF_INITUSERS, numthreads);
}


/**
 * @brief
 */
void ThreadLock (void)
{
	spin_lock (&lck);
}

/**
 * @brief
 */
void ThreadUnlock (void)
{
	release_lock (&lck);
}

/**
 * @brief
 */
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	int		i;
	int		pid[MAX_THREADS];
	int		start, end;

	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = qtrue;

	if (pacifier)
		setbuf (stdout, NULL);

	init_lock (&lck);

	for (i = 0; i < numthreads - 1; i++) {
		pid[i] = sprocsp ( (void (*)(void *, size_t))func, PR_SALL, (void *)i
			, NULL, 0x100000);
/*		pid[i] = sprocsp ( (void (*)(void *, size_t))func, PR_SALL, (void *)i */
/*			, NULL, 0x80000); */
		if (pid[i] == -1) {
			perror ("sproc");
			Error ("sproc failed");
		}
	}

	func(i);

	for (i = 0; i < numthreads - 1; i++)
		wait (NULL);

	threaded = qfalse;

	end = I_FloatTime ();
	if (pacifier)
		Sys_Printf (" (%i)\n", end-start);
}


#endif


/*
=======================================================================
Linux pthreads
=======================================================================
*/

#if defined( __linux__ )
#define USED

/* Setting default Threads to 1 */
int		numthreads = 1;

/**
 * @brief
 */
void ThreadSetDefault (void)
{
	if (numthreads == -1) {	/* not set manually */
		/* default to one thread, only multi-thread when specifically told to */
		numthreads = 1;
	}

	if (numthreads > 1)
		Sys_Printf("threads: %d\n", numthreads);
}

#include <pthread.h>

typedef struct pt_mutex_s
{
	pthread_t       *owner;
	pthread_mutex_t a_mutex;
	pthread_cond_t  cond;
	unsigned int    lock;
} pt_mutex_t;

pt_mutex_t global_lock;

/**
 * @brief
 */
void ThreadLock(void)
{
	pt_mutex_t *pt_mutex = &global_lock;

	if(!threaded)
		return;

	pthread_mutex_lock(&pt_mutex->a_mutex);
	if(pthread_equal(pthread_self(), (pthread_t)&pt_mutex->owner))
		pt_mutex->lock++;
	else {
		if((!pt_mutex->owner) && (pt_mutex->lock == 0)) {
			pt_mutex->owner = (pthread_t *)pthread_self();
			pt_mutex->lock  = 1;
		} else {
			while(1) {
				pthread_cond_wait(&pt_mutex->cond, &pt_mutex->a_mutex);
				if((!pt_mutex->owner) && (pt_mutex->lock == 0)) {
					pt_mutex->owner = (pthread_t *)pthread_self();
					pt_mutex->lock  = 1;
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&pt_mutex->a_mutex);
}

/**
 * @brief
 */
void ThreadUnlock(void)
{
	pt_mutex_t *pt_mutex = &global_lock;

	if(!threaded)
		return;

	pthread_mutex_lock(&pt_mutex->a_mutex);
	pt_mutex->lock--;

	if(pt_mutex->lock == 0) {
		pt_mutex->owner = NULL;
		pthread_cond_signal(&pt_mutex->cond);
	}

	pthread_mutex_unlock(&pt_mutex->a_mutex);
}

/**
 * @brief
 */
void recursive_mutex_init(pthread_mutexattr_t attribs)
{
	pt_mutex_t *pt_mutex = &global_lock;

	pt_mutex->owner = NULL;
	if(pthread_mutex_init(&pt_mutex->a_mutex, &attribs) != 0)
		Error("pthread_mutex_init failed\n");
	if(pthread_cond_init(&pt_mutex->cond, NULL) != 0)
		Error("pthread_cond_init failed\n");

	pt_mutex->lock = 0;
}

/**
 * @brief
 */
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	pthread_mutexattr_t         mattrib;
	pthread_t work_threads[MAX_THREADS];

	int start, end;
	int status = 0;
	unsigned int i = 0;

	start     = I_FloatTime();
	pacifier  = showpacifier;

	dispatch  = 0;
	oldf      = -1;
	workcount = workcnt;

	if (numthreads == 1)
		func(0);
	else {
		threaded = qtrue;

		if (pacifier)
			setbuf(stdout, NULL);

		if (pthread_mutexattr_init(&mattrib) != 0)
			Error("pthread_mutexattr_init failed");
#if __GLIBC_MINOR__ == 1
		if (pthread_mutexattr_settype(&mattrib, PTHREAD_MUTEX_FAST_NP) != 0)
#else
		if (pthread_mutexattr_settype(&mattrib, PTHREAD_MUTEX_ADAPTIVE_NP) != 0)
#endif
			Error ("pthread_mutexattr_settype failed");
		recursive_mutex_init(mattrib);

		for (i = 0; i < numthreads; i++) {
			/* Default pthread attributes: joinable & non-realtime scheduling */
			/* on 64bit arch, gcc complains cast to pointer from integer of different sizes
			 * but it is OK to cast from 4-byte integer to 8-byte pointer */
			if (pthread_create(&work_threads[i], NULL, (void*)func, &i) != 0)
				Error("pthread_create failed");
		}
		for (i = 0; i < numthreads; i++) {
			if (pthread_join(work_threads[i], (void **)&status) != 0)
				Error("pthread_join failed");
		}
		pthread_mutexattr_destroy(&mattrib);
		threaded = qfalse;
	}

	end = I_FloatTime();
	if (pacifier)
		Sys_Printf (" (%i)\n", end-start);
}
#endif /* __linux__ */

/*
=======================================================================
SINGLE THREAD
=======================================================================
*/

#ifndef USED

int numthreads = 1;

/**
 * @brief
 */
void ThreadSetDefault (void)
{
	numthreads = 1;
}

/**
 * @brief
 */
void ThreadLock (void)
{
}

/**
 * @brief
 */
void ThreadUnlock (void)
{
}

/**
 * @brief
 */
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int))
{
	int start, end;

	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	start = I_FloatTime ();
#ifdef NeXT
	if (pacifier)
		setbuf (stdout, NULL);
#endif
	func(0);

	end = I_FloatTime ();
	if (pacifier)
		Sys_Printf (" (%i)\n", end-start);
}

#endif
