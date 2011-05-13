/**
 * @file e_event_actoradd.c
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
#include "e_event_actoradd.h"
#include "../../../../../common/grid.h"

/**
 * @brief Adds an hidden actor to the list of le's
 * @sa CL_ActorAppear
 * @note Actor is invisible until CL_ActorAppear (EV_ACTOR_APPEAR) was triggered
 * @note EV_ACTOR_ADD
 * @sa G_SendInvisible
 */
void CL_ActorAdd (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	int entnum;
	int teamDefID;

	/* check if the actor is already visible */
	entnum = NET_ReadShort(msg);
	le = LE_Get(entnum);
	if (le) {
		Com_Printf("CL_ActorAdd: Actor with number %i already exists\n", entnum);
		return;
	}
	le = LE_Add(entnum);

	/* get the info */
	NET_ReadFormat(msg, self->formatString,
				&le->team, &teamDefID,
				&le->gender, &le->pnum, &le->pos,
				&le->state, &le->fieldSize);

	if (teamDefID < 0 || teamDefID > csi.numTeamDefs)
		Com_Printf("CL_ActorAdd: Invalid teamDef index\n");
	else
		le->teamDef = &csi.teamDef[teamDefID];

	/*Com_Printf("CL_ActorAdd: Add number: %i\n", entnum);*/

	le->type = ET_ACTORHIDDEN;

	Grid_PosToVec(cl.mapData->map, le->fieldSize, le->pos, le->origin);
	le->invis = qtrue;
}
