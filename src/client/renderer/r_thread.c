/**
 * @file r_thread.c
 */

/*
Copyright(c) 1997-2001 Id Software, Inc.
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

#include "r_local.h"
#include "../../ports/system.h"

#include <unistd.h>
#include "../../shared/threads.h"

renderer_threadstate_t r_threadstate;

#define THREAD_SLEEP_INTERVAL 100

int R_RunThread (void *p)
{
	Com_DPrintf(DEBUG_RENDERER, "R_RunThread: %d\n", SDL_ThreadID());

	while (r_threads->integer) {
		if (!refdef.ready) {
			Sys_Sleep(THREAD_SLEEP_INTERVAL);
			continue;
		}

		/* the renderer is up, so busy-wait for it */
		while (r_threadstate.state != THREAD_BSP)
			Sys_Sleep(0);

		R_SetupFrustum();

		/* draw brushes on current worldlevel */
		R_GetLevelSurfaceLists();

		/** @todo - update per-model dynamic light list sorting here */

		r_threadstate.state = THREAD_RENDERER;
	}

	return 0;
}

/**
 * @sa R_InitThreads
 */
void R_ShutdownThreads (void)
{
	if (r_threadstate.thread)
		SDL_KillThread(r_threadstate.thread);

	r_threadstate.thread = NULL;
}

/**
 * @sa R_ShutdownThreads
 */
void R_InitThreads (void)
{
	r_threadstate.thread = SDL_CreateThread(R_RunThread, NULL);
}
