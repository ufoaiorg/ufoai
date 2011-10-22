/**
 * @file e_event_actorresetclientaction.c
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
#include "e_event_actorresetclientaction.h"
#include "../../../../ui/ui_main.h"

/**
 * @brief When no trigger is touched, the client actions are reset
 * @sa EV_RESET_CLIENT_ACTION
 * @sa G_ClientMove
 */
void CL_ActorResetClientAction (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t* le;
	int number;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number);

	/* get actor le */
	le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	/* reset client action entity */
	le->clientAction = NULL;
	UI_ExecuteConfunc("disable_clientaction");
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorResetClientAction: Reset client action for actor with entnum %i\n", number);
}
