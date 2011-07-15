/**
 * @file e_event_actorstats.c
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
#include "e_event_actorstats.h"

/**
 * @brief Parses the actor stats that comes from the netchannel
 * @sa CL_ParseEvent
 * @sa G_SendStats
 */
void CL_ActorStats (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	int entnum, oldTUs = 0;

	entnum = NET_ReadShort(msg);
	le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	switch (le->type) {
	case ET_ACTORHIDDEN:
	case ET_ACTOR:
	case ET_ACTOR2x2:
		break;
	default:
		Com_Printf("CL_ActorStats: LE (%i) not an actor (type: %i)\n", entnum, le->type);
		return;
	}

	if (le->selected)
		oldTUs = le->TU;

	NET_ReadFormat(msg, self->formatString, &le->TU, &le->HP, &le->STUN, &le->morale);

	if (le->TU > le->maxTU)
		le->maxTU = le->TU;
	if (le->HP > le->maxHP)
		le->maxHP = le->HP;
	if (le->morale > le->maxMorale)
		le->maxMorale = le->morale;

	/* if selActor's timeunits have changed, update movelength */
	if (le->TU != oldTUs && le->selected)
		CL_ActorResetMoveLength(le);
}
