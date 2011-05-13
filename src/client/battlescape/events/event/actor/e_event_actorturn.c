/**
 * @file e_event_actorturn.c
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
#include "e_event_actorturn.h"

/**
 * @brief Turns actor.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 */
void CL_ActorDoTurn (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	int entnum, dir;

	NET_ReadFormat(msg, self->formatString, &entnum, &dir);

	/* get le */
	le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	if (!LE_IsActor(le))
		Com_Error(ERR_DROP, "Can't turn, LE doesn't exist or is not an actor (number: %i, type: %i)\n",
				entnum, le->type);

	if (LE_IsDead(le))
		Com_Error(ERR_DROP, "Can't turn, actor dead\n");

	le->angle = dir;
	le->angles[YAW] = directionAngles[le->angle];
}
