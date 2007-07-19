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
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "../unix/glob.h"

#include "../../qcommon/qcommon.h"

/*=============================================================================== */

/**
 * @brief
 */
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;

	return curtime;
}

/**
 * @brief
 */
void Sys_Mkdir (const char *path)
{
	mkdir(path, 0777);
}

/*============================================ */

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

/**
 * @brief
 */
static qboolean CompareAttributes (const char *path, char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

	/* . and .. never match */
	if (Q_strcmp(name, ".") == 0 || Q_strcmp(name, "..") == 0)
		return qfalse;

	Com_sprintf(fn, MAX_OSPATH, "%s/%s", path, name);
	if (stat(fn, &st) == -1)
		return qfalse; /* shouldn't happen */

	if ((st.st_mode & S_IFDIR) && (canthave & SFF_SUBDIR))
		return qfalse;

	if ((musthave & SFF_SUBDIR) && !(st.st_mode & S_IFDIR))
		return qfalse;

	return qtrue;
}

/**
 * @brief
 */
char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

/*	COM_FilePath (path, findbase); */
	Q_strncpyz(findbase, path, MAX_OSPATH);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		Q_strncpyz(findpattern, MAX_OSPATH, p + 1);
	} else
		Q_strncpyz(findpattern, MAX_OSPATH, "*");

	if (Q_strcmp(findpattern, "*.*") == 0)
		Q_strncpyz(findpattern, MAX_OSPATH, "*");

	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
/*			if (*findpattern) */
/*				printf("%s matched %s\n", findpattern, d->d_name); */
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				Com_sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

/**
 * @brief
 */
char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
/*			if (*findpattern) */
/*				printf("%s matched %s\n", findpattern, d->d_name); */
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				Com_sprintf(findpath, MAX_OSPATH, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

/**
 * @brief
 */
void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}

/**
 * @brief Breakpoint for debugger sessions
 */
void Sys_DebugBreak (void)
{
#if defined DEBUG
	__asm ("int $3");
#endif
}
