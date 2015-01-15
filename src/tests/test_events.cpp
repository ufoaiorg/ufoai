/**
 * @file
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
#include "../common/common.h"
#include "../client/battlescape/events/e_parse.h"
#include "../client/cl_shared.h"
#include <string>

class EventsTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		cl_genericPool = Mem_CreatePool("Client: Generic");
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

TEST_F(EventsTest, Range)
{
	ASSERT_TRUE(EV_NUM_EVENTS < EVENT_INSTANTLY);
}

TEST_F(EventsTest, Events)
{
	const event_t events[] = {EV_RESET, EV_START, EV_ENDROUND, EV_ENDROUNDANNOUNCE};
	for (int i = 0; i < lengthof(events); i++) {
		dbuffer buf;
		NET_WriteByte(&buf, events[i]);
		CL_ParseEvent(&buf);
	}
	ASSERT_EQ(CL_ClearBattlescapeEvents(), lengthof(events));
}

ScheduleEventPtr Dequeue_Event(int now);

TEST_F(EventsTest, Scheduler)
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

	ASSERT_EQ(Dequeue_Event(1000), one);
	ASSERT_EQ(Dequeue_Event(1000), two);
	ASSERT_EQ(Dequeue_Event(1000), three);
	ASSERT_EQ(Dequeue_Event(1000), four);
	ASSERT_EQ(Dequeue_Event(1000), five);
	ASSERT_FALSE(Dequeue_Event(1000));
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

TEST_F(EventsTest, SchedulerCheck)
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
	ASSERT_FALSE(e);
	e = Dequeue_Event(2);
	ASSERT_FALSE(e);
	/* one is delayed via check function - so we get the 2nd event at the first dequeue here */
	e = Dequeue_Event(3);
	ASSERT_EQ(e, two);
	/* now we are ready for the 1st event */
	e = Dequeue_Event(5);
	ASSERT_EQ(e, one);
	/* the remaining events are in order */
	e = Dequeue_Event(5);
	ASSERT_EQ(e, three);
	e = Dequeue_Event(5);
	ASSERT_EQ(e, four);
	e = Dequeue_Event(5);
	ASSERT_EQ(e, five);
}

static bool delayCheckBlockedVal = false;
static bool delayCheckBlocked (int now, void* data)
{
	return delayCheckBlockedVal;
}

TEST_F(EventsTest, Blocked)
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
	ASSERT_FALSE(e);
	e = Dequeue_Event(2);
	ASSERT_FALSE(e);
	e = Dequeue_Event(3);
	ASSERT_FALSE(e);
	e = Dequeue_Event(5);
	ASSERT_FALSE(e);

	delayCheckBlockedVal = true;

	e = Dequeue_Event(5);
	ASSERT_FALSE(e);
	e = Dequeue_Event(5 + delay);
	ASSERT_EQ(e, one);
	e = Dequeue_Event(4 + delay);
	ASSERT_EQ(e, two);
	e = Dequeue_Event(4 + delay);
	ASSERT_FALSE(e);
	e = Dequeue_Event(5 + delay);
	ASSERT_EQ(e, three);
	e = Dequeue_Event(5 + delay);
	ASSERT_EQ(e, four);
}
