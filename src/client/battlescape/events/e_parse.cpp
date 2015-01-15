/**
 * @file
 * @section eventformat Event format identifiers
 * id	| type		| length (bytes)
 *======================================
 * c	| char		| 1
 * b	| byte		| 1
 * s	| short		| 2
 * l	| long		| 4
 * p	| pos		| 6 (map boundaries - (-MAX_WORLD_WIDTH) - (MAX_WORLD_WIDTH))
 * g	| gpos		| 3
 * d	| dir		| 1
 * a	| angle		| 1
 * &	| string	| x
 * !	| do not read the next id | 1
 * *	| pascal string type - SIZE+DATA, SIZE can be read from va_arg
 *		| 2 + sizeof(DATA)
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

#include "../../client.h"
#include "e_parse.h"
#include "e_main.h"

#include "../cl_localentity.h"
#include "../../cgame/cl_game.h"

cvar_t* cl_log_battlescape_events;

typedef struct evTimes_s {
	event_t eType;					/**< event type to handle */
	dbuffer* msg;		/**< the parsed network channel data */
} evTimes_t;

/**********************************************************
 * General battlescape event functions
 **********************************************************/

/**
 * @sa CL_ExecuteBattlescapeEvent
 */
static void CL_LogEvent (const eventRegister_t* eventData)
{
	if (!cl_log_battlescape_events->integer)
		return;

	ScopedFile f;
	FS_OpenFile("events.log", &f, FILE_APPEND);
	if (!f)
		return;

	char tbuf[32];
	Com_MakeTimestamp(tbuf, sizeof(tbuf));

	FS_Printf(&f, "%s - %s: %10i %s\n", tbuf, CL_GetConfigString(CS_MAPTITLE), cl.time, eventData->name);
}

/**
 * @brief Adds the ability to block battlescape event execution until something other is
 * finished. E.g. camera movement
 * @param block @c true to block the execution of other events until you unblock the event
 * execution again, @c false to unblock the event execution.
 */
void CL_BlockBattlescapeEvents (bool block)
{
	if (block)
		Com_DPrintf(DEBUG_EVENTSYS, "block battlescape events\n");
	else
		Com_DPrintf(DEBUG_EVENTSYS, "unblock battlescape events\n");
	cl.eventsBlocked = block;
}

/**
 * @return if the client interrupts the event execution, this is @c true
 */
static bool CL_AreBattlescapeEventsBlocked (void)
{
	return cl.eventsBlocked;
}

/**
 * @brief Checks if a given battlescape event is ok to run now.
 *  Uses the check_func pointer in the event struct.
 * @param now The current time.
 * @param data The event to check.
 * @return true if it's ok to run or there is no check function, false otherwise.
 */
static bool CL_CheckBattlescapeEvent (int now, void* data)
{
	if (CL_AreBattlescapeEventsBlocked())
		return false;

	const evTimes_t* event = (evTimes_t*)data;
	const eventRegister_t* eventData = CL_GetEvent(event->eType);

	if (eventData->eventCheck == nullptr)
		return true;

	return eventData->eventCheck(eventData, event->msg);
}

/**
 * @brief If we delayed the battlescape events due to event locking (e.g. le is locked or camera is locked),
 * we have to advance the time for new events, too. Otherwise they might be scheduled before the just delayed
 * events.
 * @param[in] now
 * @param[in] data The userdata that is passed to the function callback. In our case this is the
 * @c eventTiming_t data
 * @param[in] delay The milliseconds that the battlescape events were delayed and that we have to use to
 * advance the @c eventTiming_t values.
 */
static void CL_NotifyBattlescapeEventDelay (int now, void* data, int delay)
{
	eventTiming_t* eventTiming = (eventTiming_t*)data;
	eventTiming->impactTime += delay;
	eventTiming->nextTime += delay;
	eventTiming->shootTime += delay;
	le_t* le = nullptr;
	while ((le = LE_GetNextInUse(le))) {
		if (LE_IsLivingActor(le)) {
			leStep_t* stepList = le->stepList;
			if (stepList == nullptr)
				continue;
			for (int i = 0; i < le->stepIndex; i++) {
				stepList = stepList->next;
			}
			stepList->lastMoveTime += delay;
		}
	}
}

/**
 * @brief Checks if a given battlescape event should get delayed.
 * @param now The current time.
 */
static bool CL_DelayBattlescapeEvent (int now, void* data)
{
	if (!CL_AreBattlescapeEventsBlocked())
		return false;
#ifdef PARANOID
	const evTimes_t* event = (evTimes_t*)data;
	const eventRegister_t* eventData = CL_GetEvent(event->eType);
	Com_DPrintf(DEBUG_EVENTSYS, "delay event %p type %s from %i\n", (const void*)event, eventData->name, now);
#endif
	return true;
}

/**
 * @sa CL_ScheduleEvent
 */
static void CL_ExecuteBattlescapeEvent (int now, void* data)
{
	evTimes_t* event = (evTimes_t*)data;
	const eventRegister_t* eventData = CL_GetEvent(event->eType);

	if (event->eType <= EV_START || cls.state == ca_active) {
		Com_DPrintf(DEBUG_EVENTSYS, "event(dispatching at %d): %s %p\n", now, eventData->name, (void*)event);

		CL_LogEvent(eventData);

		if (!eventData->eventCallback)
			Com_Error(ERR_DROP, "Event %i doesn't have a callback", event->eType);

		GAME_NotifyEvent(event->eType);
		eventData->eventCallback(eventData, event->msg);
	} else {
		Com_DPrintf(DEBUG_EVENTSYS, "event(not executed): %s %p\n", eventData->name, (void*)event);
	}

	delete event->msg;
	Mem_Free(event);
}

static void CL_FreeBattlescapeEvent (void* data)
{
	evTimes_t* event = (evTimes_t*)data;
	delete event->msg;
	Mem_Free(event);
}

/**
 * @return @c true to keep the event, @c false to remove it from the queue
 */
static bool CL_FilterBattlescapeEvents (int when, event_func* func, event_check_func* check, void* data)
{
	if (func == &CL_ExecuteBattlescapeEvent) {
		const evTimes_t* event = (const evTimes_t*)data;
		const eventRegister_t* e = CL_GetEvent(event->eType);
		Com_Printf("Remove pending event %s\n", e->name);
		return false;
	}
	return true;
}

int CL_ClearBattlescapeEvents (void)
{
	const int filtered = CL_FilterEventQueue(&CL_FilterBattlescapeEvents);
	CL_BlockBattlescapeEvents(false);
	return filtered;
}

/**
 * @brief Calculates the time the event should get executed. If two events return the same time,
 * they are going to be executed in the order the were parsed.
 * @param[in] eType The event type
 * @param[in,out] msg The message buffer that can be modified to get the event time
 * @param[in,out] eventTiming The delay data for the events
 * @return the time (in milliseconds) the event should be executed. This value is
 * used to sort the event chain to determine which event must be executed at first.
 * This value also ensures, that the events are executed in the correct order.
 * E.g. @c impactTime is used to delay some events in case the projectile needs
 * some time to reach its target.
 */
static int CL_GetEventTime (const event_t eType, dbuffer* msg, eventTiming_t* eventTiming)
{
	const eventRegister_t* eventData = CL_GetEvent(eType);

	/* get event time */
	if (eventTiming->nextTime < cl.time)
		eventTiming->nextTime = cl.time;
	if (eventTiming->impactTime < cl.time)
		eventTiming->impactTime = cl.time;

	int eventTime;
	if (!eventData->timeCallback)
		eventTime = eventTiming->nextTime;
	else
		eventTime = eventData->timeCallback(eventData, msg, eventTiming);

	Com_DPrintf(DEBUG_EVENTSYS, "%s => eventTime: %i, nextTime: %i, impactTime: %i, shootTime: %i, cl.time: %i\n",
			eventData->name, eventTime, eventTiming->nextTime, eventTiming->impactTime, eventTiming->shootTime, cl.time);

	return eventTime;
}

/**
 * @brief Called in case a svc_event was send via the network buffer
 * @sa CL_ParseServerMessage
 * @param[in] msg The client stream message buffer to read from
 */
event_t CL_ParseEvent (dbuffer* msg)
{
	static eventTiming_t eventTiming;
	int eType = NET_ReadByte(msg);
	if (eType == EV_NULL)
		return EV_NULL;

	bool now;
	/* check instantly flag */
	if (eType & EVENT_INSTANTLY) {
		now = true;
		eType &= ~EVENT_INSTANTLY;
	} else
		now = false;

	/* check if eType is valid */
	if (eType >= EV_NUM_EVENTS || eType < 0)
		Com_Error(ERR_DROP, "CL_ParseEvent: invalid event %i", eType);

	const eventRegister_t* eventData = CL_GetEvent((event_t)eType);
	if (!eventData->eventCallback)
		Com_Error(ERR_DROP, "CL_ParseEvent: no handling function for event %i", eType);

	if (eType == EV_RESET)
		OBJZERO(eventTiming);

	if (now) {
		/* log and call function */
		CL_LogEvent(eventData);
		Com_DPrintf(DEBUG_EVENTSYS, "event(now [%i]): %s\n", cl.time, eventData->name);
		GAME_NotifyEvent((event_t)eType);
		eventData->eventCallback(eventData, msg);
	} else {
		evTimes_t* const cur = Mem_PoolAllocType(evTimes_t, cl_genericPool);

		/* copy the buffer as first action, the event time functions can modify the buffer already */
		cur->msg = new dbuffer(*msg);
		cur->eType = (event_t)eType;

		/* timestamp (msec) that is used to determine when the event should be executed */
		const int when = CL_GetEventTime(cur->eType, msg, &eventTiming);
		ScheduleEventPtr e = Schedule_Event(when, &CL_ExecuteBattlescapeEvent, &CL_CheckBattlescapeEvent, &CL_FreeBattlescapeEvent, cur);
		e->delayFollowing = 50;
		e->notifyDelay = &CL_NotifyBattlescapeEventDelay;
		e->notifyDelayUserData = (void*)&eventTiming;
		e->delay = &CL_DelayBattlescapeEvent;

		Com_DPrintf(DEBUG_EVENTSYS, "event(at %d): %s %p\n", when, eventData->name, (void*)cur);
	}

	return (event_t)eType;
}
