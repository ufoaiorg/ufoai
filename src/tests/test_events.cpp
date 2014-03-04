/**
 * @file
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include <string>

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
	const event_t events[] = {EV_RESET, EV_START, EV_ENDROUND, EV_ENDROUNDANNOUNCE};
	for (int i = 0; i < lengthof(events); i++) {
		dbuffer buf;
		NET_WriteByte(&buf, events[i]);
		CL_ParseEvent(&buf);
	}
	CU_ASSERT_EQUAL(CL_ClearBattlescapeEvents(), lengthof(events));
}

ScheduleEventPtr Dequeue_Event(int now);

static void testScheduler (void)
{
	std::string s_one("one");
	std::string s_two("two");
	std::string s_three("three");
	std::string s_four("four");
	std::string s_five("five");

	ScheduleEventPtr one = Schedule_Event(3, nullptr, nullptr, nullptr, static_cast<void*>(&s_one));
	ScheduleEventPtr two = Schedule_Event(3, nullptr, nullptr, nullptr, static_cast<void*>(&s_two));
	ScheduleEventPtr three = Schedule_Event(4, nullptr, nullptr, nullptr, static_cast<void*>(&s_three));
	ScheduleEventPtr four = Schedule_Event(4, nullptr, nullptr, nullptr, static_cast<void*>(&s_four));
	ScheduleEventPtr five = Schedule_Event(5, nullptr, nullptr, nullptr, static_cast<void*>(&s_five));

	CU_ASSERT_EQUAL(Dequeue_Event(1000), one);
	CU_ASSERT_EQUAL(Dequeue_Event(1000), two);
	CU_ASSERT_EQUAL(Dequeue_Event(1000), three);
	CU_ASSERT_EQUAL(Dequeue_Event(1000), four);
	CU_ASSERT_EQUAL(Dequeue_Event(1000), five);
	CU_ASSERT_FALSE(Dequeue_Event(1000));
}

static bool delayCheck (int now, void* data)
{
	static bool check = false;
	const bool ret = check;

	if (!check) {
		check = true;
	}

	return ret;
}

static void testSchedulerCheck (void)
{
	std::string s_one("one");
	std::string s_two("two");
	std::string s_three("three");
	std::string s_four("four");
	std::string s_five("five");

	ScheduleEventPtr three = Schedule_Event(4, nullptr, nullptr, nullptr, static_cast<void*>(&s_three));
	ScheduleEventPtr four = Schedule_Event(4, nullptr, nullptr, nullptr, static_cast<void*>(&s_four));
	ScheduleEventPtr five = Schedule_Event(4, nullptr, nullptr, nullptr, static_cast<void*>(&s_five));
	ScheduleEventPtr one = Schedule_Event(3, nullptr, delayCheck, nullptr, static_cast<void*>(&s_one));
	ScheduleEventPtr two = Schedule_Event(3, nullptr, nullptr, nullptr, static_cast<void*>(&s_two));

	ScheduleEventPtr e = Dequeue_Event(1);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(2);
	CU_ASSERT_FALSE(e);
	/* one is delayed via check function - so we get the 2nd event at the first dequeue here */
	e = Dequeue_Event(3);
	CU_ASSERT_EQUAL_FATAL(e, two);
	/* now we are ready for the 1st event */
	e = Dequeue_Event(5);
	CU_ASSERT_EQUAL_FATAL(e, one);
	/* the remaining events are in order */
	e = Dequeue_Event(5);
	CU_ASSERT_EQUAL_FATAL(e, three);
	e = Dequeue_Event(5);
	CU_ASSERT_EQUAL_FATAL(e, four);
	e = Dequeue_Event(5);
	CU_ASSERT_EQUAL_FATAL(e, five);
}

static bool delayCheckBlockedVal = false;
static bool delayCheckBlocked (int now, void* data)
{
	return delayCheckBlockedVal;
}

static void testBlocked (void)
{
	std::string s_one("one");
	std::string s_two("two");
	std::string s_three("three");
	std::string s_four("three");

	const int delay = 10;

	event_func* f_oneFour = (event_func*)0xCAFED00D;
	event_func* f_twoThree = (event_func*)0xB16B00B5;

	ScheduleEventPtr one = Schedule_Event(3, f_oneFour, delayCheckBlocked, nullptr, static_cast<void*>(&s_one));
	one->delayFollowing = delay;
	ScheduleEventPtr two = Schedule_Event(4 + delay, f_twoThree, nullptr, nullptr, static_cast<void*>(&s_two));
	two->delayFollowing = delay;
	ScheduleEventPtr three = Schedule_Event(5 + delay, f_twoThree, nullptr, nullptr, static_cast<void*>(&s_three));
	three->delayFollowing = delay;
	ScheduleEventPtr four = Schedule_Event(5, f_oneFour, delayCheckBlocked, nullptr, static_cast<void*>(&s_four));
	four->delayFollowing = delay;

	ScheduleEventPtr e = Dequeue_Event(1);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(2);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(3);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(5);
	CU_ASSERT_FALSE(e);

	delayCheckBlockedVal = true;

	e = Dequeue_Event(5);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(5 + delay);
	CU_ASSERT_EQUAL_FATAL(e, one);
	e = Dequeue_Event(4 + delay);
	CU_ASSERT_EQUAL_FATAL(e, two);
	e = Dequeue_Event(4 + delay);
	CU_ASSERT_FALSE(e);
	e = Dequeue_Event(5 + delay);
	CU_ASSERT_EQUAL_FATAL(e, three);
	e = Dequeue_Event(5 + delay);
	CU_ASSERT_EQUAL_FATAL(e, four);
}

int UFO_AddEventsTests (void)
{
	/* add a suite to the registry */
	CU_pSuite EventsSuite = CU_add_suite("EventsTests", UFO_InitSuiteEvents, UFO_CleanSuiteEvents);
	if (EventsSuite == nullptr)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(EventsSuite, testRange) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(EventsSuite, testEvents) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(EventsSuite, testScheduler) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(EventsSuite, testSchedulerCheck) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(EventsSuite, testBlocked) == nullptr)
		return CU_get_error();

	return CUE_SUCCESS;
}
