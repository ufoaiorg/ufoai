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

#include "../../../shared/byte.h"
#include "shared.h"

void FS_Init(const char *path);
const char* FS_GameDir(void);

char *COM_ExpandRelativePath(const char *path);	/* from cmd line */

void SafeOpenWrite(const char *filename, qFILE *f);
void SafeWrite(qFILE *f, void *buffer, int count);

int LoadFile(const char *filename, void **bufferptr);
void CloseFile(qFILE *f);
void FreeFile(void *buffer);
int TryLoadFile(const char *filename, void **bufferptr);
char *FS_EntitiesDefUfoPath(void);

qboolean AbortPrint(const verbosityLevel_t importance);
void Verb_Printf(const verbosityLevel_t importance, const char *format, ...) __attribute__((format(printf, 2, 3)));

#endif
