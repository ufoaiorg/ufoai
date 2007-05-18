/**
 * @file sys_osx.m
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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include <pwd.h>
#include <dirent.h>
#include <libgen.h> /* dirname */

#include <dlfcn.h>
#include "../../qcommon/qcommon.h"
#include "../linux/rw_linux.h"
#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>

extern cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;

static void *game_library;

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
 * @return NULL if getcwd failed
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
 * @brief This resolves any symlinks to the binary. It's disabled for debug
 * builds because there are situations where you are likely to want
 * to symlink to binaries and /not/ have the links resolved.
 * This way you can make a link in /usr/bin and the data-files are still
 * found in e.g. /usr/local/games/ufoai
 */
char *Sys_BinName (const char *arg0)
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
		Com_sprintf(dst, MAX_OSPATH, Sys_Cwd());
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
	return getenv("HOME");
}

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

/**
 * @brief
 */
int Sys_FileLength (const char *path)
{
	struct stat st;

	if (stat (path, &st) || (st.st_mode & S_IFDIR))
		return -1;

	return st.st_size;
}

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
	Cvar_Get("sys_os", "macosx", CVAR_SERVERINFO, NULL);
}

/**
 * @brief
 * @sa Sys_DisableTray
 */
void Sys_EnableTray (void)
{
}

/**
 * @brief
 * @sa Sys_EnableTray
 */
void Sys_DisableTray (void)
{
}

/**
 * @brief
 */
void Sys_Minimize (void)
{
}

/**
 * @brief
 */
void Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	/* change stdin to non blocking */
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	Qcommon_Shutdown();

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	string[sizeof(string)-1] = 0;

	fprintf(stderr, "Error: %s\n", string);
#if defined DEBUG
	Sys_DebugBreak();				/* break execution before game shutdown */
#endif
	exit(1);
}

/**
 * @brief
 * @return -1 if not present
 */
int Sys_FileTime (const char *path)
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

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (game_library)
		dlclose(game_library);
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
	void	*(*GetGameAPI) (void *);

	char	name[MAX_OSPATH];
	const char	*path;

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	Com_Printf("------- Loading game.so -------\n");

	/* now run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath(path);
		if (!path)
			return NULL;		/* couldn't find one anywhere */
		Com_sprintf(name, sizeof(name), "%s/game.%s", path, SHARED_EXT);
		game_library = dlopen(name, RTLD_LAZY);
		if (game_library) {
			Com_Printf("LoadLibrary (%s)\n", name);
			break;
		} else {
			Com_Printf("LoadLibrary failed (%s)\n", name);
			Com_Printf("%s\n", dlerror());
		}
	}

	GetGameAPI = (void *)dlsym(game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
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
char *Sys_GetClipboardData (void)
{
#if 0 /* this should be in the renderer lib */
	Window sowner;
	Atom type, property;
	unsigned long len, bytes_left, tmp;
	unsigned char *data;
	int format, result;
	char *ret = NULL;

	sowner = XGetSelectionOwner(dpy, XA_PRIMARY);

	if (sowner != None) {
		property = XInternAtom(dpy, "GETCLIPBOARDDATA_PROP", False);

		XConvertSelection(dpy, XA_PRIMARY, XA_STRING, property, win, myxtime);
		/* myxtime is time of last X event */

		XFlush(dpy);

		XGetWindowProperty(dpy, win, property, 0, 0, False, AnyPropertyType,
							&type, &format, &len, &bytes_left, &data);

		if (bytes_left > 0) {
			result = XGetWindowProperty(dpy, win, property, 0, bytes_left, True, AnyPropertyType,
							&type, &format, &len, &tmp, &data);

			if (result == Success)
				ret = strdup((char*)data);
			XFree(data);
		}
	}
	return ret;
#else
	return NULL;
#endif
}

/**
 * @brief
 */
static void InitCocoa (void)
{
	void* cocoa_lib;
	/* @todo: Don't hardcode the path - let configure decide */
	cocoa_lib = dlopen("/System/Library/Frameworks/Cocoa.framework/Cocoa", RTLD_LAZY);
	void (*nsappload)(void);
	if (!cocoa_lib)
		Sys_Error("InitCocoa: Could not load cocoa framework\n");
	nsappload = (void(*)()) dlsym(cocoa_lib, "NSApplicationLoad");
	nsappload();
}

int lenstr(const char *text)
{
	int count = -1;				/* Character counter */

	while(text[++count] != '\0');		/* Search for a null */

	return(count);				/* Return the position of the NULL-1 */
}


unsigned char CheckForFinderCall(char **argv, int argc)
{
	unsigned char change = 0;


	if(argc < 2) {
		/* No change needed, finder would set argc to 2 */
		change = 0;
	} else {
		/* now check if first param is a cvar or a finder path */
		if (argv[0][0] == '+') {
			/* parameter is a cvar, don't change directory */
			change = 0;
		} else {
			/*change directory is needed */
			/* finder gives as second argument -psxxxx process ID */
			if(argv[1][0] == '-') {
				change = 1;
			}
		}

	}
	return change;
}

void FixWorkingDirectory (char **argv)
{
	char newPath[255];

	printf("Path Length : %d \n",lenstr(*argv));
	if(lenstr(*argv) > 255) {
		printf("Path is too long. Please copy Bundle to a shorter path location \n");
	} else {
		/* unfortunately the finder gives the path inclusive the executeable */
		/* so we only go to length - 3, to remove "ufo" from the path */
		printf("Changing wd to path %s \n",argv[0]);
		strncpy(newPath,argv[0],(strlen(argv[0])-3));
		printf("%s = neuer Pfad \n",newPath);
		chdir(newPath);
	}
}

/**
* @brief The entry point for OSX server and client.
 *
 * Inits the the program and calls Qcommon in an infinite loop.
 * FIXME: While this works, infinite loops are bad; one should not rely on exit() call; the program should be designed to fall through the bottom.
 * FIXME: Why is there a sleep statement?
 */

int main (int argc, char **argv)
{
	/* create Autorelease Pool, to avoid Error Messages under MacOSX */
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	int time, oldtime, newtime;
	float timescale = 1.0;

	InitCocoa();

	if(CheckForFinderCall(argv,argc)) {
		printf("---> Fixing Working Directory, depending on Finder Call ! \n");
		FixWorkingDirectory(argv);
	}
	Qcommon_Init(argc, argv);
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get("nostdout", "0", 0, NULL);
	if (!nostdout->value) {
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
	}

	newtime = Sys_Milliseconds();

	for (;;) {
		/* find time spent rendering last frame */
		oldtime = newtime;
		newtime = Sys_Milliseconds();
		time = timescale * (newtime - oldtime);
		timescale = Qcommon_Frame(time);
	}

	/* Free the Release Pool resources */
	[pool release];
	return 0;
}

void gSysIsDeactivated (void)
{

}

void gSysIsMinimized(void)
{

}
