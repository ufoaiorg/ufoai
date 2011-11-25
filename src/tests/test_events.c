/**
 * @file test_events.c
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

#include "test_events.h"
#include "test_shared.h"
#include "../common/common.h"
#include "../client/battlescape/events/e_parse.h"
#include "../client/cl_shared.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteEvents (void)
{
	TEST_Init();

	cl_genericPool = Mem_CreatePool("Client: Generic");

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteEvents (void)
{
	TEST_Shutdown();
	return 0;
}

static void testRange (void)
{
	CU_ASSERT_TRUE(EV_NUM_EVENTS < EVENT_INSTANTLY);
}

static void testEvents (void)
{
	int i;
	const event_t events[] = {EV_RESET, EV_START, EV_ENDROUND, EV_ENDROUNDANNOUNCE};
	for (i = 0; i < lengthof(events); i++) {
		struct dbuffer* buf = new_dbuffer();
		NET_WriteByte(buf, events[i]);
		CL_ParseEvent(buf);
	}
	CU_ASSERT_EQUAL(CL_ClearBattlescapeEvents(), lengthof(events));
}

int UFO_AddEventsTests (void)
{
	/* add a suite to the registry */
	CU_pSuite EventsSuite = CU_add_suite("EventsTests", UFO_InitSuiteEvents, UFO_CleanSuiteEvents);
	if (EventsSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(EventsSuite, testRange) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(EventsSuite, testEvents) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
