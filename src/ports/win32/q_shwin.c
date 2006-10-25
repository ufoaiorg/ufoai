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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>


int		hunkcount;

byte	*membase;
int		hunkmaxsize;
int		cursize;

#define	VIRTUAL_ALLOC

/**
 * @brief
 */
void *Hunk_Begin (int maxsize)
{
	/* reserve a huge chunk of memory, but don't commit any yet */
	cursize = 0;
	hunkmaxsize = maxsize;
#ifdef VIRTUAL_ALLOC
	membase = VirtualAlloc (NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);
#else
	membase = malloc (maxsize);
	memset (membase, 0, maxsize);
#endif
	if (!membase)
		Sys_Error ("VirtualAlloc reserve failed");
	return (void *)membase;
}

/**
 * @brief
 */
void *Hunk_Alloc (int size)
{
	void	*buf;

	/* round to cacheline */
	size = (size+31)&~31;

#ifdef VIRTUAL_ALLOC
	/* commit pages as needed */
/*	buf = VirtualAlloc (membase+cursize, size, MEM_COMMIT, PAGE_READWRITE); */
	buf = VirtualAlloc (membase, cursize+size, MEM_COMMIT, PAGE_READWRITE);
	if (!buf) {
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", buf);
	}
#endif
	cursize += size;
	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

	return (void *)(membase+cursize-size);
}

/**
 * @brief
 */
int Hunk_End (void)
{
	/* free the remaining unused virtual memory */
#if 0
	void	*buf;

	/* write protect it */
	buf = VirtualAlloc (membase, cursize, MEM_COMMIT, PAGE_READONLY);
	if (!buf)
		Sys_Error ("VirtualAlloc commit failed");
#endif

	hunkcount++;
/*	Com_Printf ("hunkcount: %i\n", hunkcount); */
	return cursize;
}

/**
 * @brief
 */
void Hunk_Free (void *base)
{
	if ( base )
#ifdef VIRTUAL_ALLOC
		VirtualFree (base, 0, MEM_RELEASE);
#else
		free (base);
#endif

	hunkcount--;
}

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
void Sys_Mkdir (char *path)
{
	_mkdir (path);
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static int findhandle;

/**
 * @brief
 */
static qboolean CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	if ( ( found & _A_RDONLY ) && ( canthave & SFF_RDONLY ) )
		return qfalse;
	if ( ( found & _A_HIDDEN ) && ( canthave & SFF_HIDDEN ) )
		return qfalse;
	if ( ( found & _A_SYSTEM ) && ( canthave & SFF_SYSTEM ) )
		return qfalse;
	if ( ( found & _A_SUBDIR ) && ( canthave & SFF_SUBDIR ) )
		return qfalse;
	if ( ( found & _A_ARCH ) && ( canthave & SFF_ARCH ) )
		return qfalse;

	if ( ( musthave & SFF_RDONLY ) && !( found & _A_RDONLY ) )
		return qfalse;
	if ( ( musthave & SFF_HIDDEN ) && !( found & _A_HIDDEN ) )
		return qfalse;
	if ( ( musthave & SFF_SYSTEM ) && !( found & _A_SYSTEM ) )
		return qfalse;
	if ( ( musthave & SFF_SUBDIR ) && !( found & _A_SUBDIR ) )
		return qfalse;
	if ( ( musthave & SFF_ARCH ) && !( found & _A_ARCH ) )
		return qfalse;

	return qtrue;
}

/**
 * @brief
 */
char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canthave )
{
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error ("Sys_BeginFind without close");
	findhandle = 0;

	COM_FilePath (path, findbase);
	findhandle = _findfirst (path, &findinfo);
	if (findhandle == -1)
		return NULL;
	if ( !CompareAttributes( findinfo.attrib, musthave, canthave ) )
		return NULL;
	Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

/**
 * @brief
 */
char *Sys_FindNext ( unsigned musthave, unsigned canthave )
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;
	if (_findnext (findhandle, &findinfo) == -1)
		return NULL;
	if ( !CompareAttributes( findinfo.attrib, musthave, canthave ) )
		return NULL;

	Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

/**
 * @brief
 */
void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose (findhandle);
	findhandle = 0;
}
