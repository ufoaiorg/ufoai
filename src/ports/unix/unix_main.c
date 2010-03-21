/**
 * @file unix_main.c
 * @brief Some generic *nix functions
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>

#include "../../common/common.h"
#include "unix_glob.h"
#include "unix_curses.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

const char *Sys_GetCurrentUser (void)
{
	static char s_userName[MAX_VAR];
	struct passwd *p;

	if ((p = getpwuid(getuid())) == NULL)
		s_userName[0] = '\0';
	else {
		strncpy(s_userName, p->pw_name, sizeof(s_userName));
		s_userName[sizeof(s_userName) - 1] = '\0';
	}
	return s_userName;
}

/**
 * @return NULL if getcwd failed
 */
char *Sys_Cwd (void)
{
	static char cwd[MAX_OSPATH];

	if (getcwd(cwd, sizeof(cwd) - 1) == NULL)
		return NULL;
	cwd[MAX_OSPATH - 1] = 0;

	return cwd;
}

/**
 * @brief Errors out of the game.
 * @note The error message should not have a newline - it's added inside of this function
 * @note This function does never return
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	Sys_Backtrace();

	/* change stdin to non blocking */
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

#ifdef COMPILE_MAP
	Mem_Shutdown();
#endif

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	fprintf(stderr, "Error: %s\n", string);

	exit(1);
}

/**
 * @brief
 * @sa Qcommon_Shutdown
 */
void Sys_Quit (void)
{
#ifdef COMPILE_UFO
	CL_Shutdown();
	Qcommon_Shutdown();
	Sys_ConsoleShutdown();
#elif COMPILE_MAP
	Mem_Shutdown();
#endif

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

void Sys_Sleep (int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	usleep(milliseconds * 1000);
}

#ifdef COMPILE_UFO
const char *Sys_SetLocale (const char *localeID)
{
	const char *locale;

# ifndef __sun
	unsetenv("LANGUAGE");
# endif /* __sun */
# ifdef __APPLE__
	if (localeID[0] != '\0') {
		if (Sys_Setenv("LANGUAGE", localeID) != 0)
			Com_Printf("...setenv for LANGUAGE failed: %s\n", localeID);
		if (Sys_Setenv("LC_ALL", localeID) != 0)
			Com_Printf("...setenv for LC_ALL failed: %s\n", localeID);
	}
# endif /* __APPLE__ */

	/* set to system default */
	setlocale(LC_ALL, "C");
	locale = setlocale(LC_MESSAGES, localeID);
	if (!locale) {
		Com_DPrintf(DEBUG_CLIENT, "...could not set to language: %s\n", localeID);
		locale = setlocale(LC_MESSAGES, "");
		if (!locale) {
			Com_DPrintf(DEBUG_CLIENT, "...could not set to system language\n");
		}
		return NULL;
	} else {
		Com_Printf("...using language: %s\n", locale);
	}

	return locale;
}

const char *Sys_GetLocale (void)
{
	/* Calling with NULL param should return current system settings. */
	const char *currentLocale = setlocale(LC_MESSAGES, NULL);
	if (currentLocale != NULL && currentLocale[0] != '\0')
		return currentLocale;
	else
		return "C";
}
#endif

/**
 * @brief set/unset environment variables (empty value removes it)
 */
int Sys_Setenv (const char *name, const char *value)
{
	if (value && value[0] != '\0')
		return setenv(name, value, 1);
	else
		return unsetenv(name);
}

/**
 * @brief Returns the home environment variable
 * (which hold the path of the user's homedir)
 */
char *Sys_GetHomeDirectory (void)
{
	return getenv("HOME");
}

void Sys_NormPath (char* path)
{
}

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

static qboolean CompareAttributes (const char *path, const char *name, unsigned musthave, unsigned canthave)
{
	struct stat st;
	char fn[MAX_OSPATH];

	/* . and .. never match */
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return qfalse;

	Com_sprintf(fn, sizeof(fn), "%s/%s", path, name);
	if (stat(fn, &st) == -1) {
		Com_Printf("CompareAttributes: Warning, stat failed: %s\n", name);
		return qfalse; /* shouldn't happen */
	}

	if ((st.st_mode & S_IFDIR) && (canthave & SFF_SUBDIR))
		return qfalse;

	if ((musthave & SFF_SUBDIR) && !(st.st_mode & S_IFDIR))
		return qfalse;

	return qtrue;
}

/**
 * @brief Opens the directory and returns the first file that matches our searchrules
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 */
char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error("Sys_BeginFind without close");

	Q_strncpyz(findbase, path, sizeof(findbase));

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		Q_strncpyz(findpattern, p + 1, sizeof(findpattern));
	} else
		Q_strncpyz(findpattern, "*", sizeof(findpattern));

	if (strcmp(findpattern, "*.*") == 0)
		Q_strncpyz(findpattern, "*", sizeof(findpattern));

	if ((fdir = opendir(findbase)) == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

/**
 * @brief Returns the next file of the already opened directory (Sys_FindFirst) that matches our search mask
 * @sa Sys_FindClose
 * @sa Sys_FindFirst
 * @sa static var findpattern
 */
char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}

#ifdef COMPILE_UFO
static void *game_library;

void Sys_UnloadGame (void)
{
#ifndef GAME_HARD_LINKED
	if (game_library)
		Sys_FreeLibrary(game_library);
#endif
	game_library = NULL;
}


/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
#ifndef HARD_LINKED_GAME
	void *(*GetGameAPI) (void *);

	char name[MAX_OSPATH];
	const char *path;
#endif

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

#ifndef HARD_LINKED_GAME
	Com_Printf("------- Loading game.%s -------\n", SHARED_EXT);

	/* now run through the search paths */
	path = NULL;
	while (!game_library) {
		path = FS_NextPath(path);
		if (!path) {
			Com_Printf("LoadLibrary failed (game."SHARED_EXT")\n");
			Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());
			return NULL;		/* couldn't find one anywhere */
		}
		Com_sprintf(name, sizeof(name), "%s/game_"CPUSTRING".%s", path, SHARED_EXT);
		game_library = dlopen(name, RTLD_LAZY);
		if (game_library) {
			Com_Printf("LoadLibrary (%s)\n", name);
			break;
		} else {
			Com_sprintf(name, sizeof(name), "%s/game.%s", path, SHARED_EXT);
			game_library = dlopen(name, RTLD_LAZY);
			if (game_library) {
				Com_Printf("LoadLibrary (%s)\n", name);
				break;
			}
			Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());
		}
	}

	GetGameAPI = (void *)dlsym(game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame();
		return NULL;
	}
#endif

	return GetGameAPI(parms);
}

/**
 * @sa Sys_FreeLibrary
 * @sa Sys_GetProcAddress
 */
void *Sys_LoadLibrary (const char *name, int flags)
{
	void *lib;
	cvar_t *s_libdir;
	char libName[MAX_OSPATH];
	char libDir[MAX_OSPATH];

	s_libdir = Cvar_Get("s_libdir", "", CVAR_ARCHIVE, "Library dir for graphic, sound and game libraries");

	/* try path given via cvar */
	if (*s_libdir->string) {
		Com_Printf("...also try library search path: '%s'\n", s_libdir->string);
		Q_strncpyz(libDir, s_libdir->string, sizeof(libDir));
	} else {
		Q_strncpyz(libDir, ".", sizeof(libDir));
	}

	/* first try system wide */
	Com_sprintf(libName, sizeof(libName), "%s_"CPUSTRING"."SHARED_EXT, name);
	Com_DPrintf(DEBUG_SYSTEM, "Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY|RTLD_GLOBAL);
	if (lib)
		return lib;
	else
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());

	/* then use s_libdir cvar or current dir */
	Com_sprintf(libName, sizeof(libName), "%s/%s_"CPUSTRING"."SHARED_EXT, libDir, name);
	Com_DPrintf(DEBUG_SYSTEM, "Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY);
	if (lib)
		return lib;
	else
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());

	/* and now both again but without CPUSTRING */
	/* system wide */
	Com_sprintf(libName, sizeof(libName), "%s.%s", name, SHARED_EXT);
	Com_DPrintf(DEBUG_SYSTEM, "Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY|RTLD_GLOBAL);
	if (lib)
		return lib;
	else
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());

	/* then use s_libdir cvar or current dir */
	Com_sprintf(libName, sizeof(libName), "%s/%s."SHARED_EXT, libDir, name);
	Com_DPrintf(DEBUG_SYSTEM, "Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY);
	if (lib)
		return lib;
	else
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());

#ifdef LIBDIR
	/* then use s_libdir cvar or current dir */
	Com_sprintf(libName, sizeof(libName), "%s/%s."SHARED_EXT, LIBDIR, name);
	Com_DPrintf(DEBUG_SYSTEM, "Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY);
	if (lib)
		return lib;
	else
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", dlerror());
#endif

	Com_Printf("Could not load %s."SHARED_EXT" and %s_"CPUSTRING"."SHARED_EXT"\n", name, name);
	return NULL;
}

/**
 * @sa Sys_LoadLibrary
 */
void Sys_FreeLibrary (void *libHandle)
{
	if (!libHandle)
		Sys_Error("Sys_FreeLibrary: No valid handle given");
	if (dlclose(libHandle) != 0)
		Sys_Error("Sys_FreeLibrary: dlclose() failed - %s", dlerror());
}

/**
 * @sa Sys_LoadLibrary
 */
void *Sys_GetProcAddress (void *libHandle, const char *procName)
{
	if (!libHandle)
		Sys_Error("Sys_GetProcAddress: No valid libHandle given");
	return dlsym(libHandle, procName);
}


void Sys_SetAffinityAndPriority (void)
{
	if (sys_affinity->modified) {
		sys_affinity->modified = qfalse;
	}

	if (sys_priority->modified) {
		sys_priority->modified = qfalse;
	}
}
#endif

int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase = 0;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000;
	}

	return (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;
}

void Sys_Mkdir (const char *thePath)
{
	if (mkdir(thePath, 0777) != -1)
		return;

	if (errno != EEXIST)
		Com_Printf("\"mkdir %s\" failed, reason: \"%s\".", thePath, strerror(errno));
}

/**
 * @brief On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace (void)
{
#ifdef HAVE_EXECINFO_H
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	const int i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
	backtrace_symbols_fd(symbols, i, STDERR_FILENO);
#endif
}

#if USE_SIGNALS
/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
static void Sys_Signal (int s)
{
	switch (s) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		Com_Printf("Received signal %d, quitting..\n", s);
		Sys_Quit();
		break;
#ifdef HAVE_CURSES
	case SIGWINCH:
		Curses_Resize();
		break;
#endif
	default:
		Sys_Backtrace();
		Sys_Error("Received signal %d.\n", s);
		break;
	}
}
#endif

void Sys_InitSignals (void)
{
#if USE_SIGNALS
#ifdef HAVE_CURSES
	signal(SIGWINCH, Sys_Signal);
#endif
	signal(SIGHUP, Sys_Signal);
	signal(SIGINT, Sys_Signal);
	signal(SIGQUIT, Sys_Signal);
	signal(SIGILL, Sys_Signal);
	signal(SIGABRT, Sys_Signal);
	signal(SIGFPE, Sys_Signal);
	signal(SIGSEGV, Sys_Signal);
	signal(SIGTERM, Sys_Signal);
#endif
}
