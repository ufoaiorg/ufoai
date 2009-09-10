/**
 * @file test_routing.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "CUnit/Basic.h"
#include "test_routing.h"

#include "../common/common.h"
#include "../common/routing.h"
#include "../common/tracing.h"
#include "../common/cmodel.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRouting (void)
{
	unsigned int checksum;

	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	Mem_Init();
	Cmd_Init();
	Cvar_Init();
	FS_InitFilesystem(qtrue);
	Swap_Init();

	/**
	 * @todo use a special testmap
	 */
	CM_LoadMap("fueldump", qtrue, "", &checksum);
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRouting (void)
{
	FS_Shutdown();
	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
	return 0;
}

static void testConnection (void)
{
	/**
	 * @todo implement the test
	 */
}

int UFO_AddRoutingTests (void)
{
	/* add a suite to the registry */
	CU_pSuite routingSuite = CU_add_suite("RoutingTests", UFO_InitSuiteRouting, UFO_CleanSuiteRouting);
	if (routingSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(routingSuite, testConnection) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
