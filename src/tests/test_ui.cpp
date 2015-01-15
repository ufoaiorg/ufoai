/**
 * @file
 * @brief Test cases for code below client/ui/
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../client/ui/ui_nodes.h"
#include "../client/ui/ui_timer.h"

class UITest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

/**
 * @brief unittest around timer data structure.
 * It not test timer execution.
 */
TEST_F(UITest, TimerDataStructure)
{
	uiNode_t* dummyNode = (uiNode_t*) 0x1;
	timerCallback_t dummyCallback = (timerCallback_t) 0x1;

	uiTimer_t* a, *b, *c;
	a = UI_AllocTimer(dummyNode, 10, dummyCallback);
	b = UI_AllocTimer(dummyNode, 20, dummyCallback);
	c = UI_AllocTimer(dummyNode, 30, dummyCallback);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == nullptr);

	UI_TimerStart(b);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == b);

	UI_TimerStart(a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == a);

	UI_TimerStart(c);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next->next == c);

	UI_TimerStop(a);
	UI_TimerStop(b);
	ASSERT_TRUE(a->owner != nullptr);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == c);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next == nullptr);

	UI_TimerStart(a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next == c);

	UI_PrivateInsertTimerInActiveList(a->next, b);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next == b);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next->next == c);

	UI_TimerRelease(b);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer()->next == c);

	UI_TimerRelease(a);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == c);

	UI_TimerRelease(c);
	ASSERT_TRUE(UI_PrivateGetFirstTimer() == nullptr);
	ASSERT_TRUE(c->owner == nullptr);
}
