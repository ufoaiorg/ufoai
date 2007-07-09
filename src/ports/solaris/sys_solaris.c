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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/file.h>

#include <dlfcn.h>

#include "../../qcommon/qcommon.h"
#include "../linux/rw_linux.h"

extern cvar_t *nostdout;

unsigned sys_frame_time;

uid_t saved_euid;

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

/**
 * @brief
 */
void Sys_Quit (void)
{
	CL_Shutdown();
	Qcommon_Shutdown();
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

/**
 * @brief
 */
void Sys_Init (void)
{
	Cvar_Get("sys_os", "solaris", CVAR_SERVERINFO, NULL);
#if USE_X86_ASM
/*	Sys_SetFPCW(); */
#endif
}

/**
 * @brief
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	/* change stdin to non blocking */
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	string[sizeof(string)-1] = 0;
	fprintf(stderr, "Error: %s\n", string);

	CL_Shutdown ();
	Qcommon_Shutdown ();
	exit (0);
}

void floating_point_exception_handler (int whatever)
{
/*	Sys_Warn("floating point exception\n"); */
	signal(SIGFPE, floating_point_exception_handler);
}

/**
 * @brief
 */
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	if (KBD_Update_fp)
		KBD_Update_fp();
#endif
	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}

/**
 * @brief
 */
char *Sys_GetClipboardData (void)
{
	return NULL;
}

/**
 * @brief
 */
int main (int argc, char **argv)
{
	int 	time, oldtime, newtime;

	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get("nostdout", "0", 0, NULL);

	if (!nostdout->integer) {
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
	}

	oldtime = Sys_Milliseconds ();
	while (1) {
		/* find time spent rendering last frame */
		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		} while (time < 1);
		Qcommon_Frame(time);
		oldtime = newtime;
    }
}

