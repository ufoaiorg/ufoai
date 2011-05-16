/**
 * @file test_ui.c
 * @brief Test cases for code below client/ui/
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "test_ui.h"
#include "../client/ui/ui_nodes.h"
#include "../client/ui/ui_timer.h"

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
	uiNode_t *dummyNode = (uiNode_t *) 0x1;
	timerCallback_t dummyCallback = (timerCallback_t) 0x1;

	uiTimer_t *a, *b, *c;
	a = UI_AllocTimer(dummyNode, 10, dummyCallback);
	b = UI_AllocTimer(dummyNode, 20, dummyCallback);
	c = UI_AllocTimer(dummyNode, 30, dummyCallback);
	CU_ASSERT(UI_PrivateGetFirstTimer() == NULL);

	UI_TimerStart(b);
	CU_ASSERT(UI_PrivateGetFirstTimer() == b);

	UI_TimerStart(a);
	CU_ASSERT(UI_PrivateGetFirstTimer() == a);

	UI_TimerStart(c);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next->next == c);

	UI_TimerStop(a);
	UI_TimerStop(b);
	CU_ASSERT(a->owner != NULL);
	CU_ASSERT(UI_PrivateGetFirstTimer() == c);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next == NULL);

	UI_TimerStart(a);
	CU_ASSERT(UI_PrivateGetFirstTimer() == a);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next == c);

	UI_PrivateInsertTimerInActiveList(a->next, b);
	CU_ASSERT(UI_PrivateGetFirstTimer() == a);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next == b);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next->next == c);

	UI_TimerRelease(b);
	CU_ASSERT(UI_PrivateGetFirstTimer() == a);
	CU_ASSERT(UI_PrivateGetFirstTimer()->next == c);

	UI_TimerRelease(a);
	CU_ASSERT(UI_PrivateGetFirstTimer() == c);

	UI_TimerRelease(c);
	CU_ASSERT(UI_PrivateGetFirstTimer() == NULL);
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
