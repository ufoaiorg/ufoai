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
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>

#include "../unix/glob.h"

#include "../../qcommon/qcommon.h"
#if defined(__FreeBSD__)
#include <machine/param.h>
#endif

/*=============================================================================== */

byte *membase;
int maxhunksize;
int curhunksize;

void *Hunk_Begin (int maxsize)
{
	/* reserve a huge chunk of memory, but don't commit any yet */
	maxhunksize = maxsize + sizeof(int);
	curhunksize = 0;
#if (defined __FreeBSD__)
	membase = mmap(0, maxhunksize, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANON, -1, 0);
#else
	membase = mmap(0, maxhunksize, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
	if (membase == NULL || membase == (byte *)-1)
		Sys_Error("unable to virtual allocate %d bytes", maxsize);

	*((int *)membase) = curhunksize;

	return membase + sizeof(int);
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	/* round to cacheline */
	size = (size+31)&~31;
	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");
	buf = membase + sizeof(int) + curhunksize;
	curhunksize += size;
	return buf;
}

int Hunk_End (void)
{
	byte *n;
#if defined(__FreeBSD__)
	size_t old_size = maxhunksize;
	size_t new_size = curhunksize + sizeof(int);
	void * unmap_base;
	size_t unmap_len;

	new_size = round_page(new_size);
	old_size = round_page(old_size);
	if (new_size > old_size)
		n = 0; /* error */
	else if (new_size < old_size) {
		unmap_base = (caddr_t)(membase + new_size);
		unmap_len = old_size - new_size;
		n = munmap(unmap_base, unmap_len) + membase;
	}
#endif
#if defined(__linux__)
	n = mremap(membase, maxhunksize, curhunksize + sizeof(int), 0);
#endif
	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);
	*((int *)membase) = curhunksize + sizeof(int);

	return curhunksize;
}

void Hunk_Free (void *base)
{
	byte *m;

	if (base) {
		m = ((byte *)base) - sizeof(int);
		if (munmap(m, *((int *)m)))
			Sys_Error("Hunk_Free: munmap failed (%d)", errno);
	}
}

/*=============================================================================== */


/*
================
Sys_Milliseconds
================
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

void Sys_Mkdir (char *path)
{
	mkdir (path, 0777);
}

char *strlwr (char *s)
{
	char* origs = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return origs;
}

/*============================================ */

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

static qboolean CompareAttributes(char *path, char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

	/* . and .. never match */
	if (Q_strcmp(name, ".") == 0 || Q_strcmp(name, "..") == 0)
		return qfalse;

	return qtrue;

	if (stat(fn, &st) == -1)
		return qfalse; /* shouldn't happen */

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return qfalse;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return qfalse;

	return qtrue;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

/*	COM_FilePath (path, findbase); */
	Q_strncpyz(findbase, path, MAX_OSPATH);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		Q_strncpyz(findpattern, p + 1, MAX_OSPATH);
	} else
		Q_strncpyz(findpattern, "*", MAX_OSPATH);

	if (Q_strcmp(findpattern, "*.*") == 0)
		Q_strncpyz(findpattern, "*", MAX_OSPATH);

	if ((fdir = opendir(findbase)) == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
/*			if (*findpattern) */
/*				printf("%s matched %s\n", findpattern, d->d_name); */
			if ( CompareAttributes(findbase, d->d_name, musthave, canhave) ) {
				Com_sprintf (findpath, MAX_OSPATH, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

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
				Com_sprintf (findpath, MAX_OSPATH, "%s/%s", findbase, d->d_name);
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

