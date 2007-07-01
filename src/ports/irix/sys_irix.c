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
#include <mntent.h>

#include <dlfcn.h>

#include "../../qcommon/qcommon.h"

#include "../linux/rw_linux.h"

extern cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

void Sys_Init (void)
{
	Cvar_Get("sys_os", "irix", CVAR_SERVERINFO, NULL);
#if USE_X86_ASM
/*	Sys_SetFPCW(); */
#endif
}

void Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	/* change stdin to non blocking */
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start (argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	string[sizeof(string)-1] = 0;

	fprintf(stderr, "Error: %s\n", string);

	CL_Shutdown ();
	Qcommon_Shutdown ();
	exit (1);
}

/**
 * @brief
 * @returns -1 if not present
 */
int	Sys_FileTime (char *path)
{
	struct	stat	buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}

/**
 * @brief
 */
void floating_point_exception_handler (int whatever)
{
	signal(SIGFPE, floating_point_exception_handler);
}

/*****************************************************************************/

static void *game_library;

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (game_library)
		Sys_FreeLibrary(game_library);
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
#ifndef REF_HARD_LINKED
	void	*(*GetGameAPI) (void *);

	char	name[MAX_OSPATH];
	char	curpath[MAX_OSPATH];
	const char	*path;

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	getcwd(curpath, sizeof(curpath));

	Com_Printf("------- Loading game.so -------");

	/* now run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath(path);
		if (!path)
			return NULL;		/* couldn't find one anywhere */
		Com_sprintf(name, sizeof(name), "%s/game.so", path);
		Com_Printf("Trying to load library (%s)\n",name);
		game_library = dlopen(name, RTLD_NOW );
		if (game_library) {
			Com_DPrintf("LoadLibrary (%s)\n",name);
			break;
		}
	}

	GetGameAPI = (void *)dlsym(game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
#else
	return (void *)GetGameAPI (parms);
#endif
}

/**
 * @brief
 */
void Sys_AppActivate (void)
{
}

/**
 * @brief
 */
void Sys_SendKeyEvents (void)
{
	if (KBD_Update_fp)
		KBD_Update_fp();

	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}

/**
 * @brief
 */
char *Sys_GetCurrentUser (void)
{
	struct passwd *p;

	if ((p = getpwuid(getuid())) == NULL) {
		return "player";
	}
	return p->pw_name;
}

/**
 * @brief
 */
char *Sys_Cwd (void)
{
	static char cwd[MAX_OSPATH];

	if (getcwd(cwd, sizeof(cwd) - 1) == NULL)
		return NULL;
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/**
 * @brief
 */
void Sys_NormPath (char* path)
{
}

/**
 * @brief
 */
void Sys_OSPath (char* path)
{
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
	int time, oldtime, newtime;
	float timescale = 1.0;

	/* go back to real user for config loads */
	saved_euid = geteuid();
	seteuid(getuid());

	Sys_ConsoleInputInit();
	Qcommon_Init(argc, argv);

/*	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY); */

	nostdout = Cvar_Get("nostdout", "0", 0, NULL);
	if (!nostdout->integer) {
/*		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY); */
	}

	newtime = Sys_Milliseconds();
	for (;;) {
		/* find time spent rendering last frame */
		oldtime = newtime;
		newtime = Sys_Milliseconds();
		time = timescale * (newtime - oldtime);
		timescale = Qcommon_Frame(time);
	}
	return 0;
}

