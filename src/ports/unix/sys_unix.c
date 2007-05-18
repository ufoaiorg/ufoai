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

#include "../../qcommon/qcommon.h"

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
int Sys_FileLength (const char *path)
{
	struct stat st;

	if (stat (path, &st) || (st.st_mode & S_IFDIR))
		return -1;

	return st.st_size;
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
		dlclose(game_library);
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
