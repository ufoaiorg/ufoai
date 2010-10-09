/**
 * @file test_ui.c
 * @brief Test cases for code below client/ui/ when the global UI "engine" is started
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
#include "test_shared.h"
#include "test_ui_level2.h"
#include "../client/ui/ui_nodes.h"
#include "../client/ui/ui_main.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteUILevel2 (void)
{
	TEST_Init();
	UI_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteUILevel2 (void)
{
	UI_Shutdown();
	TEST_Shutdown();
	return 0;
}

/**
 * @brief test behaviour name
 */
static void testKnownBehaviours (void)
{
	uiBehaviour_t *behaviour;

	behaviour = UI_GetNodeBehaviour("button");
	CU_ASSERT_FATAL(behaviour != NULL);
	CU_ASSERT_STRING_EQUAL(behaviour->name, "button");

	/* first one */
	behaviour = UI_GetNodeBehaviour("");
	CU_ASSERT(behaviour != NULL);

	/* last one */
	behaviour = UI_GetNodeBehaviour("zone");
	CU_ASSERT_FATAL(behaviour != NULL);
	CU_ASSERT_STRING_EQUAL(behaviour->name, "zone");

	/* unknown */
	behaviour = UI_GetNodeBehaviour("dahu");
	CU_ASSERT(behaviour == NULL);
}

int UFO_AddUILevel2Tests (void)
{
	/* add a suite to the registry */
	CU_pSuite UISuite = CU_add_suite("UILevel2Tests", UFO_InitSuiteUILevel2, UFO_CleanSuiteUILevel2);

	if (UISuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(UISuite, testKnownBehaviours) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
