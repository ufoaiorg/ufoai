/**
 * @file e_event_entappear.c
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
#include "e_event_entappear.h"
#include "../../../../../common/grid.h"

int CL_EntAppearTime (const struct eventRegister_s *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	if (eventTiming->parsedDeath) { /* drop items after death (caused by impact) */
		return eventTiming->impactTime + 400;
	} else if (eventTiming->impactTime > cl.time) { /* item thrown on the ground */
		return eventTiming->impactTime + 75;
	}

	return eventTiming->nextTime;
}

/**
 * @brief Let an entity appear - like an item on the ground that just got visible
 * @sa EV_ENT_APPEAR
 * @sa CL_EntPerish
 * @sa CL_AddEdict
 */
void CL_EntAppear (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t	*le;
	int		entnum;
	entity_type_t type;
	pos3_t	pos;

	NET_ReadFormat(msg, self->formatString, &entnum, &type, &pos);

	/* check if the ent is already visible */
	le = LE_Get(entnum);
	if (!le) {
		le = LE_Add(entnum);
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_EntAppear: Entity appearing already visible... overwriting the old one\n");
		le->inuse = qtrue;
	}

	le->type = type;

	/* the default is invisible - another event will follow which spawns not
	 * only the le, but also the particle. The visibility is set there, too */
	if (le->type == ET_PARTICLE)
		le->invis = !cl_leshowinvis->integer;

	VectorCopy(pos, le->pos);
	Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, le->origin);
}
