/**
 * @file q_shwin.c
 * @brief Windows system functions
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

#include "../../qcommon/qcommon.h"
#include "winquake.h"
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#if defined DEBUG && defined _MSC_VER >= 1300
#include <intrin.h>
#endif

int	curtime;
/**
 * @brief
 */
int Sys_Milliseconds (void)
{
	static int		base;
	static qboolean	initialized = qfalse;

	if (!initialized) {	/* let base retain 16 bits of effectively random data */
		base = timeGetTime() & 0xffff0000;
		initialized = qtrue;
	}
	curtime = timeGetTime() - base;

	return curtime;
}

/**
 * @brief
 */
void Sys_Mkdir (const char *path)
{
	_mkdir(path);
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static int findhandle;

/**
 * @brief
 */
static qboolean CompareAttributes (unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return qfalse;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return qfalse;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return qfalse;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return qfalse;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return qfalse;

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return qfalse;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return qfalse;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return qfalse;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return qfalse;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return qfalse;

	return qtrue;
}

/**
 * @brief
 * @sa Sys_FindNext
 * @sa Sys_FindClose
 */
char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error("Sys_BeginFind without close");
	findhandle = 0;

	COM_FilePath (path, findbase);
	findhandle = _findfirst (path, &findinfo);
	while (findhandle != -1) {
		/* found one that matched */
		if (strcmp(findinfo.name, ".") && strcmp(findinfo.name, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		/* doesn't match - try the next one */
		} else if (_findnext(findhandle, &findinfo) == -1) {
			/* ok, no further entries here - leave the while loop */
			_findclose(findhandle);
			findhandle = -1;
		}
	}

	/* none found */
	return NULL;
}

/**
 * @brief
 * @sa Sys_FindFirst
 * @sa Sys_FindClose
 */
char *Sys_FindNext (unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	/* until we found the next entry */
	while (_findnext (findhandle, &findinfo) != -1) {
		if (strcmp(findinfo.name, ".") && strcmp(findinfo.name, "..") &&
			CompareAttributes(findinfo.attrib, musthave, canthave)) {
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}

	/* none found */
	return NULL;
}

/**
 * @brief Closes the find handle
 * @note Call this before calling a new Sys_FindFirst
 * @sa Sys_FindNext
 * @sa Sys_FindFirst
 */
void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose(findhandle);
	findhandle = 0;
}

/**
 * @brief Breakpoint for debugger sessions
 */
void Sys_DebugBreak (void)
{
#if defined DEBUG
# if _MSC_VER >= 1300
	__debugbreak();	/* break execution before game shutdown */
# else
	/*DebugBreak();*/
# endif
#endif
}
