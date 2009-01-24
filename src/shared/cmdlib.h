/**
 * @file cmdlib.h
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

#ifndef __CMDLIB__
#define __CMDLIB__

#include "byte.h"
#include "typedefs.h"

/** @brief verbosity levels for use in calls to Verb_Printf
 *  and on the command line -v <int> */
typedef enum {
	VERB_SILENT_EXCEPT_ERROR,
	VERB_CHECK, /**< print nothing, except check/fix messages */
	VERB_MAPNAME, /**< print mapname only. in check/fix, only do this if a problem is found */
	VERB_LESS,
	VERB_NORMAL,
	VERB_EXTRA,
	VERB_DUMP, /**< the highest verbosity level */
	VERB_NUM /**< the number of verbosity levels */
} verbosityLevel_t;

void FS_Init(const char *path);
const char* FS_GameDir(void);

void Com_Printf(const char *msg, ...) __attribute__((format(printf, 1, 2)));

char *COM_ExpandRelativePath(const char *path);	/* from cmd line */

void SafeOpenWrite(const char *filename, qFILE *f);
void SafeWrite(qFILE *f, void *buffer, int count);

int LoadFile(const char *filename, void **bufferptr);
void CloseFile(qFILE *f);
void FreeFile(void *buffer);
int TryLoadFile(const char *filename, void **bufferptr);

qboolean AbortPrint(const verbosityLevel_t importance);
void Verb_Printf(const verbosityLevel_t importance, const char *format, ...) __attribute__((format(printf, 2, 3)));

#endif
