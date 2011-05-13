/**
 * @file e_event_doorclose.c
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
#include "e_event_doorclose.h"

static void LET_DoorSlidingClose (le_t * le)
{
	LET_SlideDoor(le, -le->slidingSpeed);
}

/**
 * @brief Callback for EV_DOOR_CLOSE event - rotates the inline model and recalc routing
 * @sa EV_DOOR_CLOSE
 * @sa G_ClientUseEdict
 * @sa Touch_DoorTrigger
 */
void CL_DoorClose (const eventRegister_t *self, struct dbuffer *msg)
{
	/* get local entity */
	int number;
	le_t *le;

	NET_ReadFormat(msg, self->formatString, &number);

	le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	if (le->type == ET_DOOR) {
		if (le->dir & DOOR_OPEN_REVERSE)
			le->angles[le->dir & 3] += DOOR_ROTATION_ANGLE;
		else
			le->angles[le->dir & 3] -= DOOR_ROTATION_ANGLE;

		CM_SetInlineModelOrientation(cl.mapTiles, le->inlineModelName, le->origin, le->angles);
		CL_RecalcRouting(le);
	} else if (le->type == ET_DOOR_SLIDING) {
		LE_SetThink(le, LET_DoorSlidingClose);
		le->think(le);
	} else {
		Com_Error(ERR_DROP, "Invalid door entity found of type: %i", le->type);
	}
}
