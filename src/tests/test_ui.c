/**
 * @file test_ui.c
 * @brief Test cases for code below client/menu/
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
#include "test_ui.h"
#include "../client/menu/m_nodes.h"
#include "../client/menu/m_timer.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteUI (void)
{
	TEST_Init();
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteUI (void)
{
	TEST_Shutdown();
	return 0;
}

/**
 * @brief unittest around timer data structure.
 * It not test timer execution.
 */
static void testTimerDataStructure (void)
{
	menuNode_t *dummyNode = (menuNode_t *) 0x1;
	timerCallback_t dummyCallback = (timerCallback_t) 0x1;

	menuTimer_t *a, *b, *c;
	a = MN_AllocTimer(dummyNode, 10, dummyCallback);
	b = MN_AllocTimer(dummyNode, 20, dummyCallback);
	c = MN_AllocTimer(dummyNode, 30, dummyCallback);
	CU_ASSERT(MN_GetFirstTimer() == NULL);

	MN_TimerStart(b);
	CU_ASSERT(MN_GetFirstTimer() == b);

	MN_TimerStart(a);
	CU_ASSERT(MN_GetFirstTimer() == a);

	MN_TimerStart(c);
	CU_ASSERT(MN_GetFirstTimer()->next->next == c);

	MN_TimerStop(a);
	MN_TimerStop(b);
	CU_ASSERT(a->owner != NULL);
	CU_ASSERT(MN_GetFirstTimer() == c);
	CU_ASSERT(MN_GetFirstTimer()->next == NULL);

	MN_TimerStart(a);
	CU_ASSERT(MN_GetFirstTimer() == a);
	CU_ASSERT(MN_GetFirstTimer()->next == c);

	MN_PrivateInsertTimerInActiveList(a->next, b);
	CU_ASSERT(MN_GetFirstTimer() == a);
	CU_ASSERT(MN_GetFirstTimer()->next == b);
	CU_ASSERT(MN_GetFirstTimer()->next->next == c);

	MN_TimerRelease(b);
	CU_ASSERT(MN_GetFirstTimer() == a);
	CU_ASSERT(MN_GetFirstTimer()->next == c);

	MN_TimerRelease(a);
	CU_ASSERT(MN_GetFirstTimer() == c);

	MN_TimerRelease(c);
	CU_ASSERT(MN_GetFirstTimer() == NULL);
	CU_ASSERT(c->owner == NULL);
}

int UFO_AddUITests (void)
{
	/* add a suite to the registry */
	CU_pSuite UISuite = CU_add_suite("UITests", UFO_InitSuiteUI, UFO_CleanSuiteUI);

	if (UISuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(UISuite, testTimerDataStructure) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
