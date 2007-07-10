/**
 * @file sys_unix.c
 * @brief Some generic *nix functions
 */

/*
Copyright (C) 1997-2001 UFO:AI team

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

#include "../../qcommon/qcommon.h"

static void *game_library;

/**
 * @brief
 */
const char *Sys_GetCurrentUser (void)
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
void Sys_Quit (void)
{
	CL_Shutdown();
	Qcommon_Shutdown();
	Sys_ConsoleInputShutdown();

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

/**
 * @brief
 */
void Sys_Sleep (int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	usleep(milliseconds * 1000);
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
 * @brief Returns the home environment variable
 * (which hold the path of the user's homedir)
 */
char *Sys_GetHomeDirectory (void)
{
	return getenv("HOME");
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
void Sys_AppActivate (void)
{
}

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
	void *(*GetGameAPI) (void *);

	char name[MAX_OSPATH];
	const char *path;

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	Com_Printf("------- Loading game.%s -------\n", SHARED_EXT);

	/* now run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath(path);
		if (!path) {
			Com_Printf("LoadLibrary failed (game."SHARED_EXT")\n");
			Com_DPrintf("%s\n", dlerror());
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
	Com_DPrintf("Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY|RTLD_GLOBAL);
	if (lib)
		return lib;

	/* then use s_libdir cvar or current dir */
	Com_sprintf(libName, sizeof(libName), "%s/%s_"CPUSTRING"."SHARED_EXT, libDir, name);
	Com_DPrintf("Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY);
	if (lib)
		return lib;

	/* and not both again but without CPUSTRING */
	/* system wide */
	Com_sprintf(libName, sizeof(libName), "%s.%s", name, SHARED_EXT);
	Com_DPrintf("Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY|RTLD_GLOBAL);
	if (lib)
		return lib;

	/* then use s_libdir cvar or current dir */
	Com_sprintf(libName, sizeof(libName), "%s/%s."SHARED_EXT, libDir, name);
	Com_DPrintf("Sys_LoadLibrary: try %s\n", libName);
	lib = dlopen(libName, flags|RTLD_LAZY);
	if (lib)
		return lib;

	Com_Printf("Could not load %s."SHARED_EXT" and %s_"CPUSTRING"."SHARED_EXT"\n", name, name);
	return NULL;
}

/**
 * @brief
 * @sa Sys_LoadLibrary
 */
void Sys_FreeLibrary (void *libHandle)
{
	if (!libHandle)
		Com_Error(ERR_DROP, "Sys_FreeLibrary: No valid handle given\n");
	if (dlclose(libHandle) != 0)
		Com_Error(ERR_DROP, "Sys_FreeLibrary: dlclose() failed - %s\n", dlerror());
}

/**
 * @brief
 * @sa Sys_LoadLibrary
 */
void *Sys_GetProcAddress (void *libHandle, const char *procName)
{
	if (!libHandle)
		Com_Error(ERR_DROP, "Sys_GetProcAddress: No valid libHandle given\n");
	return dlsym(libHandle, procName);
}
