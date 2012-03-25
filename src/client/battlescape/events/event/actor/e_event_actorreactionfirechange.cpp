/**
 * @file e_event_actorreactionfirechange.c
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
#include "../../../cl_hud.h"
#include "../../../../ui/ui_main.h"
#include "e_event_actorreactionfirechange.h"

/**
 * @brief Network event function for reaction fire mode changes. Responsible for updating
 * the HUD with the information that were received from the server
 * @param self The event pointer
 * @param msg The network message to parse the event data from
 * @sa HUD_UpdateReactionFiremodes
 */
void CL_ActorReactionFireChange (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	actorHands_t hand;
	int entnum, fmIdx, odIdx;
	character_t *chr;
	const objDef_t *od;

	NET_ReadFormat(msg, self->formatString, &entnum, &fmIdx, &hand, &odIdx);

	le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	chr = CL_ActorGetChr(le);
	if (!chr)
		return;

	if (odIdx != NONE)
		od = INVSH_GetItemByIDX(odIdx);
	else
		od = NULL;

	CL_ActorSetRFMode(chr, hand, fmIdx, od);

	UI_ExecuteConfunc("reactionfire_updated");
}
