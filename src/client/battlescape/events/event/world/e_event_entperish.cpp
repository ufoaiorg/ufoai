/**
 * @file e_event_entperish.c
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
#include "../../../cl_particle.h"
#include "e_event_entperish.h"

/**
 * @brief Called whenever an entity disappears from view
 * @sa CL_EntAppear
 */
void CL_EntPerish (const eventRegister_t *self, struct dbuffer *msg)
{
	int		entnum;
	int		type;
	le_t	*le, *actor;

	NET_ReadFormat(msg, self->formatString, &entnum, &type);

	le = LE_Get(entnum);

	if (!le)
		LE_NotFoundWithTypeError(entnum, type);

	switch (le->type) {
	case ET_ITEM:
		cls.i.EmptyContainer(&cls.i, &le->i, INVDEF(csi.idFloor));

		/* search owners (there can be many, some of them dead) */
		actor = NULL;
		while ((actor = LE_GetNextInUse(actor))) {
			if ((actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
			 && VectorCompare(actor->pos, le->pos)) {
				Com_DPrintf(DEBUG_CLIENT, "CL_EntPerish: le of type ET_ITEM hidden\n");
				FLOOR(actor) = NULL;
			}
		}
		break;
	case ET_ACTOR:
	case ET_ACTOR2x2:
		cls.i.DestroyInventory(&cls.i, &le->i);
		break;
#ifdef DEBUG
	case ET_ACTORHIDDEN:
		Com_DPrintf(DEBUG_CLIENT, "CL_EntPerish: It should not happen that we perish a hidden actor\n");
		return;
#endif
	case ET_PARTICLE:
		CL_ParticleFree(le->ptl);
		le->ptl = NULL;
		break;
	case ET_BREAKABLE:
	case ET_DOOR:
	case ET_DOOR_SLIDING:
		break;
	default:
		break;
	}

	le->invis = qtrue;
	/* decrease the count of spotted aliens (also stunned) */
	cl.numEnemiesSpotted = CL_CountVisibleEnemies();
}
