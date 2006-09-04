/**
 * @file sys_linux.c
 * @brief main function and system functions
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
#if defined(__FreeBSD__)
#else
#include <mntent.h>
#endif
#include <pwd.h>
#include <dirent.h>
#include <libgen.h> /* dirname */

#include <dlfcn.h>

#include "../../qcommon/qcommon.h"

#include "rw_linux.h"

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = qtrue;

static void *game_library;

/**
 * @brief
 */
char *Sys_GetCurrentUser( void )
{
	struct passwd *p;

	if ( (p = getpwuid( getuid() )) == NULL ) {
		return "player";
	}
	return p->pw_name;
}

/**
 * @brief
 */
char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/**
 * @brief
 */
void Sys_NormPath(char* path)
{
}

/**
 * @brief This resolves any symlinks to the binary. It's disabled for debug
 * builds because there are situations where you are likely to want
 * to symlink to binaries and /not/ have the links resolved.
 * This way you can make a link in /usr/bin and the data-files are still
 * found in e.g. /usr/local/games/ufoai
 */
char *Sys_BinName( const char *arg0 )
{
#ifndef DEBUG
	int	n;
	char	src[MAX_OSPATH];
	char	dir[MAX_OSPATH];
	qboolean	links = qfalse;
#endif

	static char	dst[MAX_OSPATH];
	Com_sprintf(dst, MAX_OSPATH, (char*)arg0);

#ifndef DEBUG
	while((n = readlink(dst, src, MAX_OSPATH)) >= 0) {
		src[n] = '\0';
		Com_sprintf(dir, MAX_OSPATH, dirname(dst));
		Com_sprintf(dst, MAX_OSPATH, dir);
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, src, MAX_OSPATH);
		links = qtrue;
	}

	if (links) {
		Com_sprintf( dst, MAX_OSPATH, Sys_Cwd());
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, dir, MAX_OSPATH);
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, src, MAX_OSPATH);
	}
#endif
	return dst;
}

/**
 * @brief
 */
char *Sys_GetHomeDirectory (void)
{
	return getenv ( "HOME" );
}


/* ======================================================================= */
/* General routines */
/* ======================================================================= */

/**
 * @brief
 */
void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
		return;

	fputs(string, stdout);
}

/**
 * @brief
 */
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	unsigned char		*p;

	va_start (argptr,fmt);
	vsnprintf (text,1024,fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	if (nostdout && nostdout->value)
		return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= SCHAR_MAX;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
}

/**
 * @brief
 */
void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

/**
 * @brief
 */
void Sys_Init(void)
{
	Cvar_Get("sys_os", "linux", CVAR_SERVERINFO);
#if id386
/*	Sys_SetFPCW(); */
#endif
}

/**
 * @brief
 */
void Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	/* change stdin to non blocking */
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr,error);
	vsnprintf (string,1024,error,argptr);
	va_end (argptr);

	fprintf(stderr, "Error: %s\n", string);
	exit (1);
}

/**
 * @brief
 */
void Sys_Warn (char *warning, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr,warning);
	vsnprintf (string,1024,warning,argptr);
	va_end (argptr);
	fprintf(stderr, "Warning: %s", string);
}

/**
 * @brief
 * @return -1 if not present
 */
int Sys_FileTime (char *path)
{
	struct	stat	buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}

/**
 * @brief
 */
void floating_point_exception_handler(int whatever)
{
/*	Sys_Warn("floating point exception\n"); */
	signal(SIGFPE, floating_point_exception_handler);
}

/**
 * @brief
 */
char *Sys_ConsoleInput(void)
{
	static char text[256];
	int     len;
	fd_set	fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { /* eof! */
		stdin_active = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    /* rip off the /n and terminate */

	return text;
}

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (game_library)
		dlclose (game_library);
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
	void	*(*GetGameAPI) (void *);

	char	name[MAX_OSPATH];
	char	curpath[MAX_OSPATH];
	char	*path;

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	getcwd(curpath, sizeof(curpath));

	Com_Printf("------- Loading game.so -------\n");

	/* now run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath (path);
		if (!path)
			return NULL;		/* couldn't find one anywhere */
		Com_sprintf (name, MAX_OSPATH, "%s/%s/game.so", curpath, path);
		game_library = dlopen (name, RTLD_LAZY );
		if (game_library) {
			Com_Printf ("LoadLibrary (%s)\n", name);
			break;
		} else {
			Com_Printf ("LoadLibrary failed (%s)\n", name);
			Com_Printf("%s\n", dlerror());
		}
	}

	GetGameAPI = (void *)dlsym (game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame ();
		return NULL;
	}

	return GetGameAPI (parms);
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
char *Sys_GetClipboardData(void)
{
	return NULL;
}

/**
 * @brief
 */
int main (int argc, char **argv)
{
	int 	time, oldtime, newtime;

	/* go back to real user for config loads */
	saved_euid = geteuid();
	seteuid(getuid());

	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get("nostdout", "0", 0);
	if (!nostdout->value) {
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
	}

	oldtime = Sys_Milliseconds ();
	while (1) {
		if (dedicated && dedicated->value)
			usleep(1);
		/* find time spent rendering last frame */
		do {
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
		Qcommon_Frame (time);
		oldtime = newtime;
	}
}

