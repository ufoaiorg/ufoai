/**
 * @file e_event_actorreservationchange.c
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h"
#include "e_event_actorreservationchange.h"

/**
 * @brief Network event function for TU reservation. Responsible for updating the HUD with the information
 * that were received from the server
 * @param self The event pointer
 * @param msg The network message to parse the event data from
 */
void CL_ActorReservationChange (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	int entnum, reaction, shot, crouch;
	character_t *chr;

	NET_ReadFormat(msg, self->formatString, &entnum, &reaction, &shot, &crouch);

	le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	chr = CL_ActorGetChr(le);
	if (!chr)
		return;

	chr->reservedTus.reaction = reaction;
	chr->reservedTus.shot = shot;
	chr->reservedTus.crouch = crouch;
}
