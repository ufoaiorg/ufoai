/**
 * @file e_event_invcheckhands.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../../../../cl_le.h"
#include "../../../../cl_actor.h"
#include "../../../cl_hud.h"
#include "e_event_invcheckhands.h"

/**
 * @brief The client changed something in his hand-containers. This function updates the reactionfire info.
 * @param[in] msg The netchannel message
 */
void CL_InvCheckHands (const eventRegister_t *self, struct dbuffer *msg)
{
	int entnum;
	le_t *le;
	int actorIdx = -1;
	int hand = -1;		/**< 0=right, 1=left -1=undef*/

	NET_ReadFormat(msg, self->formatString, &entnum, &hand);
	if (entnum < 0 || hand < 0)
		Com_Error(ERR_DROP, "CL_InvCheckHands: entnum or hand not sent/received correctly. (number: %i)\n", entnum);

	le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	actorIdx = CL_GetActorNumber(le);
	if (actorIdx == -1)
		Com_Error(ERR_DROP, "CL_InvCheckHands: Could not get local entity actor id via CL_GetActorNumber: team=%i(%s) type=%i inuse=%i\n",
			le->team, le->teamDef ? le->teamDef->name : "No team", le->type, le->inuse);

	/* No need to continue if stored firemode settings are still usable. */
	if (!CL_WorkingFiremode(le, qtrue)) {
		/* Firemode for reaction not sane and/or not usable. */
		/* Update the changed hand with default firemode. */
		if (hand == ACTOR_HAND_RIGHT)
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_RIGHT, -1);
		else
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_LEFT, -1);

		HUD_HideFiremodes();
	}
}
