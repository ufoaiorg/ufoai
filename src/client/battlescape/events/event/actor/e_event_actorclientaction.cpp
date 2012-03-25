/**
 * @file e_event_actorclientaction.c
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
#include "e_event_actorclientaction.h"
#include "../../../../ui/ui_main.h"

/**
 * @brief Reads the entity number for client interaction
 * @sa EV_CLIENT_ACTION
 * @sa Touch_DoorTrigger
 * @sa CL_ActorUse
 * @todo Hud should have a button that should be activated now
 */
void CL_ActorClientAction (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t* le;
	int number, actionEntityNumber;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number, &actionEntityNumber);

	/* get actor le */
	le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	/* set client action entity number */
	le->clientAction = LE_Get(actionEntityNumber);
	if (!le->clientAction)
		LE_NotFoundError(actionEntityNumber);

	UI_ExecuteConfunc("enable_clientaction");

	Com_DPrintf(DEBUG_CLIENT, "CL_ActorClientAction: Set entity number: %i (for actor with entnum %i)\n",
			actionEntityNumber, number);
}
