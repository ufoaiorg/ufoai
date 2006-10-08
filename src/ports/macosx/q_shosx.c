/**
 * @file q_shosx.c
 * @brief shared functions for the plug-ins
 * Written by:	awe				[mailto:awe@fruitz-of-dojo.de]
 * 2001-2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de]
 */

/*

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

#pragma mark =Includes=

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "../unix/glob.h"
#include "../../qcommon/qcommon.h"

#pragma mark -

#pragma mark =Variables=

byte *membase;
int maxhunksize, curhunksize, curtime;

static char	gSysFindBase[MAX_OSPATH], gSysFindPath[MAX_OSPATH], gSysFindPattern[MAX_OSPATH];
static	DIR	*gSysFindDir;

#pragma mark -


/**
 * @brief
 */
char* strlwr (char *s)
{
	char* origs = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return origs;
}

/**
 * @brief
 */
void* Hunk_Begin (int theMaxSize)
{
	maxhunksize = theMaxSize + sizeof (int);
	curhunksize = 0;
	membase = malloc (maxhunksize);
	if (membase == NULL)
		Sys_Error ("unable to virtual allocate %d bytes", theMaxSize);

	*((int *) membase) = curhunksize;

	return (membase + sizeof(int));
}

/**
 * @brief
 */
void* Hunk_Alloc (int theSize)
{
	unsigned char	*myMemory;

	theSize = (theSize + 31) & ~31;
	if (curhunksize + theSize > maxhunksize)
		Sys_Error ("Hunk_Alloc overflow");
	myMemory = membase + sizeof(int) + curhunksize;
	curhunksize += theSize;

	return (myMemory);
}

/**
 * @brief
 */
void Hunk_Free (void *theBuffer)
{
	unsigned char 	*myMemory;

	if (theBuffer != NULL) {
		myMemory = ((unsigned char *) theBuffer) - sizeof (int);
		free (myMemory);
	}
}

/**
 * @brief
 */
int Hunk_End (void)
{
	return (curhunksize);
}

/**
 * @brief
 */
int Sys_Milliseconds (void)
{
	struct timeval	myTimeValue;
	struct timezone	myTimeZone;
	static int		myStartSeconds;

	gettimeofday (&myTimeValue, &myTimeZone);

	if (!myStartSeconds) {
		myStartSeconds = myTimeValue.tv_sec;
		return (myTimeValue.tv_usec / 1000);
	}

	curtime = (myTimeValue.tv_sec - myStartSeconds) * 1000 + myTimeValue.tv_usec / 1000;

	return (curtime);
}

/**
 * @brief
 */
void Sys_Mkdir (char *thePath)
{
	if (mkdir (thePath, 0777) != -1)
	return;

	if (errno != EEXIST)
		Sys_Error ("\"mkdir %s\" failed, reason: \"%s\".", thePath, strerror(errno));
}

/**
 * @brief
 */
static qboolean Sys_CompareAttributes (char *thePath, char *theName, unsigned theMustHave, unsigned theCantHave)
{
	struct stat myStat;
	char 	myFileName[MAX_OSPATH];

	/* . and .. never match */
	if (Q_strcmp (theName, ".") == 0 || Q_strcmp (theName, "..") == 0)
		return qfalse;

	Com_sprintf(myFileName, MAX_OSPATH, "%s/%s", thePath, theName);

	if (stat (myFileName, &myStat) == -1)
		return (qfalse);

	if ((myStat.st_mode & S_IFDIR) && (theCantHave & SFF_SUBDIR))
		return (qfalse);

	if ((theMustHave & SFF_SUBDIR) && !(myStat.st_mode & S_IFDIR))
		return (qfalse);

	return (qtrue);
}

/**
 * @brief
 */
char* Sys_FindFirst (char *thePath, unsigned theMustHave, unsigned theCantHave)
{
	struct dirent	*myDirEnt;
	char		*myPointer;

	if (gSysFindDir != NULL)
		Sys_Error ("Sys_BeginFind without close");

	Q_strncpyz (gSysFindBase, thePath, MAX_OSPATH);

	if ((myPointer = strrchr (gSysFindBase, '/')) != NULL) {
		*myPointer = 0;
		Q_strncpyz (gSysFindPattern, myPointer + 1, MAX_OSPATH);
	} else
		Q_strncpyz (gSysFindPattern, "*", MAX_OSPATH);

	if (Q_strcmp (gSysFindPattern, "*.*") == 0)
		Q_strncpyz (gSysFindPattern, "*", MAX_OSPATH);

	if ((gSysFindDir = opendir (gSysFindBase)) == NULL)
		return (NULL);

	while ((myDirEnt = readdir (gSysFindDir)) != NULL)
		if (!*gSysFindPattern || glob_match(gSysFindPattern, myDirEnt->d_name))
			if (Sys_CompareAttributes (gSysFindBase, myDirEnt->d_name, theMustHave, theCantHave)) {
				Com_sprintf (gSysFindPath, MAX_OSPATH, "%s/%s", gSysFindBase, myDirEnt->d_name);
				return (gSysFindPath);
			}

	return (NULL);
}

/**
 * @brief
 */
char* Sys_FindNext (unsigned theMustHave, unsigned theCantHave)
{
	struct dirent 	*myDirEnt;

	/* just security: */
	if (gSysFindDir == NULL)
		return (NULL);

	/* find next... */
	while ((myDirEnt = readdir (gSysFindDir)) != NULL)
		if (!*gSysFindPattern || glob_match (gSysFindPattern, myDirEnt->d_name))
			if (Sys_CompareAttributes (gSysFindBase, myDirEnt->d_name, theMustHave, theCantHave)) {
				Com_sprintf (gSysFindPath, MAX_OSPATH, "%s/%s", gSysFindBase, myDirEnt->d_name);
				return (gSysFindPath);
			}

	return (NULL);
}

/**
 * @brief
 */
void Sys_FindClose (void)
{
	if (gSysFindDir != NULL)
		closedir (gSysFindDir);
	gSysFindDir = NULL;
}
