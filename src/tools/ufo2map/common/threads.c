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

int dispatch;
int workcount;
int oldf;
qboolean pacifier;
qboolean threaded;

/**
 * @brief
 */
int	GetThreadWork (void)
{
	int	r;
	int	f;

	ThreadLock ();

	if (dispatch == workcount) {
		ThreadUnlock ();
		return -1;
	}

	f = 10*dispatch / workcount;
	if (f != oldf) {
		oldf = f;
		if (pacifier) {
			fprintf (stdout, "%i...", f);
			fflush(stdout);
		}
	}

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
}


void (*workfunction) (int);

/**
 * @brief
 */
void ThreadWorkerFunction (int threadnum)
{
	int		work;

	while (1) {
		work = GetThreadWork ();
		if (work == -1)
			break;
/* 		printf ("thread %i, work %i\n", threadnum, work); */
		workfunction(work);
	}
}

/**
 * @brief
 */
void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(int))
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

	if (numthreads == -1) {	/* not set manually */
		GetSystemInfo (&info);
		numthreads = info.dwNumberOfProcessors;
		if (numthreads < 1 || numthreads > 32)
			numthreads = 1;
	}

	qprintf ("%i threads\n", numthreads);
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
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
{
	int		threadid[MAX_THREADS];
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
		for (i=0 ; i<numthreads ; i++) {
			threadhandle[i] = CreateThread(
			   NULL,	/* LPSECURITY_ATTRIBUTES lpsa, */
			   0,		/* DWORD cbStack, */
			   (LPTHREAD_START_ROUTINE)func,	/* LPTHREAD_START_ROUTINE lpStartAddr, */
			   (LPVOID)i,	/* LPVOID lpvThreadParm, */
			   0,			/*   DWORD fdwCreate, */
			   &threadid[i]);
		}

		for (i=0 ; i<numthreads ; i++)
			WaitForSingleObject (threadhandle[i], INFINITE);
	}
	DeleteCriticalSection (&crit);

	threaded = qfalse;
	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
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
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
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

	if (pthread_attr_create (&attrib) == -1)
		Error ("pthread_attr_create failed");
	if (pthread_attr_setstacksize (&attrib, 0x100000) == -1)
		Error ("pthread_attr_setstacksize failed");

	for (i=0 ; i<numthreads ; i++) {
  		if (pthread_create(&work_threads[i], attrib
		, (pthread_startroutine_t)func, (pthread_addr_t)i) == -1)
			Error ("pthread_create failed");
	}

	for (i=0 ; i<numthreads ; i++) {
		if (pthread_join (work_threads[i], &status) == -1)
			Error ("pthread_join failed");
	}

	threaded = qfalse;

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
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
	printf ("%i threads\n", numthreads);
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
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
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

	for (i=0 ; i<numthreads-1 ; i++) {
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

	for (i=0 ; i<numthreads-1 ; i++)
		wait (NULL);

	threaded = qfalse;

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
}


#endif

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
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int))
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
		printf (" (%i)\n", end-start);
}

#endif
