/**
 * @file e_time.c
 * @brief Battlescape event timing code
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
#include "../cl_localentity.h"
#include "e_main.h"
#include "e_time.h"

/**
 * @brief Calculates the time the event should get executed. If two events return the same time,
 * they are going to be executed in the order the were parsed.
 * @param[in] eType The event type
 * @param[in,out] msg The message buffer that can be modified to get the event time
 * @param[in,out] eventTiming The delay data for the events
 * @return the time the event should be executed. This value is used to sort the
 * event chain to determine which event must be executed at first. This
 * value also ensures, that the events are executed in the correct
 * order. E.g. @c impactTime is used to delay some events in case the
 * projectile needs some time to reach its target.
 */
int CL_GetEventTime (const event_t eType, struct dbuffer *msg, eventTiming_t *eventTiming)
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
