/**
 * @file ufomodel.c
 * @brief Starting point for model tool
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

#include "../../shared/ufotypes.h"
#include "../../common/unzip.h"
#include "../../common/mem.h"
#include "../../common/filesys.h"
#include "../../common/common.h"
#include "../../shared/shared.h"
#include "../../shared/mathlib.h"
#include "../../shared/defines.h"
#include "../../shared/typedefs.h"
#include "../../shared/byte.h"

#define VERSION "0.1"

struct memPool_s *com_genericPool;
struct memPool_s *com_fileSysPool;

void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

static void PrecalcNormalsAndTangents (void)
{
	const char *filename;
	FS_BuildFileList("**.md2");
	while ((filename = FS_NextFileFromFileList("**.md2")) != NULL) {
		Com_Printf("%s\n", filename);
	}
	FS_NextFileFromFileList(NULL);
}

int main (int argc, const char **argv)
{
	Com_Printf("---- ufomodel "VERSION" ----\n");
	Com_Printf(BUILDSTRING);

	Swap_Init();
	Mem_Init();

	com_genericPool = Mem_CreatePool("ufomodel");
	com_fileSysPool = Mem_CreatePool("ufomodel filesys");

	FS_InitFilesystem(qfalse);

	PrecalcNormalsAndTangents();

	Mem_Shutdown();

	return 0;
}
