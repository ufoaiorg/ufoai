/**
 * @file e_event_actordooraction.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../../../../cl_le.h"
#include "e_event_actordooraction.h"

/**
 * @brief Reads the door entity number for client interaction
 * @sa EV_DOOR_ACTION
 * @sa Touch_DoorTrigger
 * @sa CL_ActorUseDoor
 */
void CL_ActorDoorAction (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t* le;
	int number, doornumber;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number, &doornumber);

	/* get actor le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorDoorAction: Could not get le %i\n", number);
		return;
	}
	/* set door number */
	le->clientAction = doornumber;
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoorAction: Set door number: %i (for actor with entnum %i)\n", doornumber, number);
}
