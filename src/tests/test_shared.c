/**
 * @file test_shared.c
 * @brief Shared code for unittests
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "test_shared.h"
#include <stdio.h>
#include <stdarg.h>

void TEST_Shutdown (void)
{
	FS_Shutdown();
	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
	Com_Shutdown();
	Cbuf_Shutdown();

	com_aliasSysPool = NULL;
	com_cmdSysPool = NULL;
	com_cmodelSysPool = NULL;
	com_cvarSysPool = NULL;
	com_fileSysPool = NULL;
	com_genericPool = NULL;
}

void TEST_vPrintf (const char *fmt, va_list argptr)
{
	char text[1024];
	Q_vsnprintf(text, sizeof(text), fmt, argptr);

	fprintf(stderr, "%s", text);

	fflush(stderr);
}

static void Test_InitError (void)
{
	CU_FAIL_FATAL("Error during initialization");
}

static void Test_RunError (void)
{
	CU_FAIL_FATAL("There was a Com_Error or Com_Drop call during the execution of this test");
}

void TEST_Init (void)
{
	Com_SetExceptionCallback(Test_InitError);

	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	Mem_Init();
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	FS_InitFilesystem(qtrue);
	FS_AddGameDirectory("./unittest", qfalse);
	Swap_Init();

	memset(&csi, 0, sizeof(csi));

	Com_SetExceptionCallback(Test_RunError);
}
