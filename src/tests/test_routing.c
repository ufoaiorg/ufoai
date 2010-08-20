/**
 * @file test_routing.c
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

#include "CUnit/Basic.h"
#include "test_routing.h"
#include "test_shared.h"

#include "../common/common.h"
#include "../common/cmodel.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRouting (void)
{
	TEST_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRouting (void)
{
	TEST_Shutdown();
	return 0;
}

static mapData_t mapData;
static mapTiles_t mapTiles;

static void testConnection (void)
{
	/**
	 * @todo use a special testmap
	 * @todo implement the test
	 */
	CM_LoadMap("fueldump", qtrue, "", &mapData, &mapTiles);
	CM_LoadMap("fueldump", qtrue, "", &mapData, &mapTiles);
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
