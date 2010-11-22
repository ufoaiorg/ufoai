/**
 * @file e_event_doorclose.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

static void CL_DoorSlidingClose (le_t * le)
{
	vec3_t shiftDir;
	const int dir = (le->dir & 3) - 1;
	const qboolean reverse = le->dir & 4;

	VectorClear(shiftDir);
	shiftDir[dir] = reverse ? -le->slidingSpeed : le->slidingSpeed;

	/* this origin is only an offset to the absolute mins/maxs for rendering */
	VectorAdd(le->origin, shiftDir, le->origin);

	/* le->slidingSpeed; */
	le->think = NULL;
	CM_SetInlineModelOrientation(cl.mapTiles, le->inlineModelName, le->origin, le->angles);
	CL_RecalcRouting(le);
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
		const int dir = (le->dir & 3) - 1;
		le->angles[dir] -= DOOR_ROTATION_ANGLE;

		CM_SetInlineModelOrientation(cl.mapTiles, le->inlineModelName, le->origin, le->angles);
		CL_RecalcRouting(le);
	} else if (le->type == ET_DOOR_SLIDING) {
		LE_SetThink(le, CL_DoorSlidingClose);
		le->think(le);
	} else {
		Com_Error(ERR_DROP, "Invalid door entity found of type: %i", le->type);
	}
}
