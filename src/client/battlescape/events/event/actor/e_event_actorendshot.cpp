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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_actorendshot.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorEndShootTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	eventTiming->parsedShot = false;
	eventTiming->parsedDeath = false;

	const int eventTime = eventTiming->impactTime;
	eventTiming->impactTime = cl.time;
	if (eventTime > cl.time)
		return eventTime;

	return  eventTiming->nextTime;
}

/**
 * @brief Ends shooting with actor.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 * @sa CL_ActorStartShoot
 * @sa EV_ACTOR_END_SHOOT
 */
void CL_ActorEndShoot (const eventRegister_t* self, dbuffer* msg)
{
	int entnum;

	NET_ReadFormat(msg, self->formatString, &entnum);

	/* shooting actor */
	le_t* le = LE_Get(entnum);

	if (!le)
		return;

	LE_SetThink(le, LET_StartIdle);
}
