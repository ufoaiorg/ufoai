/**
 * @file e_parse.c
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

#include "../../client.h"
#include "e_parse.h"
#include "e_main.h"

#include "../cl_localentity.h"
#include "../../cgame/cl_game.h"

cvar_t *cl_log_battlescape_events;

typedef struct evTimes_s {
	event_t eType;					/**< event type to handle */
	struct dbuffer *msg;		/**< the parsed network channel data */
} evTimes_t;

/**********************************************************
 * General battlescape event functions
 **********************************************************/

/**
 * @sa CL_ExecuteBattlescapeEvent
 */
static void CL_LogEvent (const eventRegister_t *eventData)
{
	qFILE f;

	if (!cl_log_battlescape_events->integer)
		return;

	FS_OpenFile("events.log", &f, FILE_APPEND);
	if (!f.f)
		return;
	else {
		char tbuf[32];

		Com_MakeTimestamp(tbuf, sizeof(tbuf));

		FS_Printf(&f, "%s - %s: %10i %s\n", tbuf, CL_GetConfigString(CS_MAPTITLE), cl.time, eventData->name);
		FS_CloseFile(&f);
	}
}

/**
 * @brief Adds the ability to block battlescape event execution until something other is
 * finished. E.g. camera movement
 * @param block @c true to block the execution of other events until you unblock the event
 * execution again, @c false to unblock the event execution.
 */
void CL_BlockBattlescapeEvents (qboolean block)
{
	cl.eventsBlocked = block;
}

/**
 * @return if the client interrupts the event execution, this is @c true
 */
static qboolean CL_AreBattlescapeEventsBlocked (void)
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
static qboolean CL_CheckBattlescapeEvent (int now, void *data)
{
	if (CL_AreBattlescapeEventsBlocked()) {
		return qfalse;
	} else {
		const evTimes_t *event = (evTimes_t *)data;
		const eventRegister_t *eventData = CL_GetEvent(event->eType);

		if (eventData->eventCheck == NULL)
			return qtrue;

		return eventData->eventCheck(eventData, event->msg);
	}
}

/**
 * @sa CL_ScheduleEvent
 */
static void CL_ExecuteBattlescapeEvent (int now, void *data)
{
	evTimes_t *event = (evTimes_t *)data;
	const eventRegister_t *eventData = CL_GetEvent(event->eType);

	if (event->eType <= EV_START || cls.state == ca_active) {
		Com_DPrintf(DEBUG_EVENTSYS, "event(dispatching at %d): %s %p\n", now, eventData->name, (void*)event);

		CL_LogEvent(eventData);

		if (!eventData->eventCallback)
			Com_Error(ERR_DROP, "Event %i doesn't have a callback", event->eType);

		GAME_NofityEvent(event->eType);
		eventData->eventCallback(eventData, event->msg);
	} else {
		Com_DPrintf(DEBUG_EVENTSYS, "event(not executed): %s %p\n", eventData->name, (void*)event);
	}

	free_dbuffer(event->msg);
	Mem_Free(event);
}

static void CL_FreeBattlescapeEvent (void *data)
{
	evTimes_t *event = (evTimes_t *)data;
	free_dbuffer(event->msg);
	Mem_Free(event);
}

/**
 * @return @c true to keep the event, @c false to remove it from the queue
 */
static qboolean CL_FilterBattlescapeEvents (int when, event_func *func, event_check_func *check, void *data)
{
	if (func == &CL_ExecuteBattlescapeEvent) {
		const evTimes_t *event = (const evTimes_t *)data;
		Com_Printf("Remove pending event %i\n", event->eType);
		return qfalse;
	}
	return qtrue;
}

int CL_ClearBattlescapeEvents (void)
{
	int filtered = CL_FilterEventQueue(&CL_FilterBattlescapeEvents);
	CL_BlockBattlescapeEvents(qfalse);
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
static int CL_GetEventTime (const event_t eType, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	const eventRegister_t *eventData = CL_GetEvent(eType);
	int eventTime;

	/* get event time */
	if (eventTiming->nextTime < cl.time)
		eventTiming->nextTime = cl.time;
	if (eventTiming->impactTime < cl.time)
		eventTiming->impactTime = cl.time;

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
void CL_ParseEvent (struct dbuffer *msg)
{
	static eventTiming_t eventTiming;
	qboolean now;
	const eventRegister_t *eventData;
	event_t eType = NET_ReadByte(msg);
	if (eType == EV_NULL)
		return;

	/* check instantly flag */
	if (eType & EVENT_INSTANTLY) {
		now = qtrue;
		eType &= ~EVENT_INSTANTLY;
	} else
		now = qfalse;

	/* check if eType is valid */
	if (eType >= EV_NUM_EVENTS)
		Com_Error(ERR_DROP, "CL_ParseEvent: invalid event %i", eType);

	eventData = CL_GetEvent(eType);
	if (!eventData->eventCallback)
		Com_Error(ERR_DROP, "CL_ParseEvent: no handling function for event %i", eType);

	if (eType == EV_RESET)
		OBJZERO(eventTiming);

	if (now) {
		/* log and call function */
		CL_LogEvent(eventData);
		Com_DPrintf(DEBUG_EVENTSYS, "event(now [%i]): %s\n", cl.time, eventData->name);
		GAME_NofityEvent(eType);
		eventData->eventCallback(eventData, msg);
	} else {
		evTimes_t *cur = (evTimes_t *)Mem_PoolAlloc(sizeof(*cur), cl_genericPool, 0);
		int when;

		/* copy the buffer as first action, the event time functions can modify the buffer already */
		cur->msg = dbuffer_dup(msg);
		cur->eType = eType;

		/* timestamp (msec) that is used to determine when the event should be executed */
		when = CL_GetEventTime(cur->eType, msg, &eventTiming);
		Schedule_Event(when, &CL_ExecuteBattlescapeEvent, &CL_CheckBattlescapeEvent, &CL_FreeBattlescapeEvent, cur);

		Com_DPrintf(DEBUG_EVENTSYS, "event(at %d): %s %p\n", when, eventData->name, (void*)cur);
	}
}
