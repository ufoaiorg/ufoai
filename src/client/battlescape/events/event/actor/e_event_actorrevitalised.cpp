/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../../../cl_actor.h"
#include "../../../cl_hud.h"
#include "../../../cl_particle.h"
#include "e_event_actorrevitalised.h"

/**
 * @brief Revitalizes a stunned actor (all that is needed is the local entity state set).
 * @param[in] msg The netchannel message
 * @param[in] self Pointer to the event structure that is currently executed
 */
void CL_ActorRevitalised (const eventRegister_t* self, dbuffer* msg)
{
	int entnum, state;
	NET_ReadFormat(msg, self->formatString, &entnum, &state);

	/* get les */
	le_t* le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	if (!LE_IsStunned(le) && !LE_IsLivingActor(le))
		Com_Error(ERR_DROP, "CL_ActorRevitalised: Can't revitalise, LE is not a dead or stunned actor");

	LE_Lock(le);

	/* link any floor container into the actor temp floor container */
	le_t* floor = LE_Find(ET_ITEM, le->pos);
	if (floor)
		le->setFloor(floor);

	le->state = state;

	/* play animation */
	LE_SetThink(le, LET_StartIdle);

	/* Print some info. */
	if (le->team == cls.team) {
		const character_t* chr = CL_ActorGetChr(le);
		if (chr) {
			char tmpbuf[128];
			Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s was revitalised\n"), chr->name);
			HUD_DisplayMessage(tmpbuf);
		}
	} else {
		switch (le->team) {
		case (TEAM_CIVILIAN):
			HUD_DisplayMessage(_("A civilian was revitalised."));
			break;
		case (TEAM_ALIEN):
			HUD_DisplayMessage(_("An alien was revitalised."));
			break;
		case (TEAM_PHALANX):
			HUD_DisplayMessage(_("A soldier was revitalised."));
			break;
		default:
			HUD_DisplayMessage(va(_("A member of team %i was revitalised."), le->team));
			break;
		}
	}

	le->aabb.setMaxs(player_maxs);

	if (le->ptl) {
		CL_ParticleFree(le->ptl);
		le->ptl = nullptr;
	}

	/* add team members to the actor list */
	CL_ActorAddToTeamList(le);

	/* update pathing as we maybe not can walk onto this actor anymore */
	CL_ActorConditionalMoveCalc(selActor);
	LE_Unlock(le);
}
