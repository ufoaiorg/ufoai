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
#include "../../../cl_parse.h"
#include "../../../cl_particle.h"
#include "../../../../renderer/r_mesh_anim.h"
#include "e_event_actordie.h"

/**
 * @brief Some events will be delayed if they are executed in the context of a dying actor. That's why we set
 * the @c parsedDeath value to @c true here.
 */
int CL_ActorDieTime (const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	eventTiming->parsedDeath = true;
	if (eventTiming->parsedShot && eventTiming->impactTime > cl.time)
		return eventTiming->impactTime;
	return eventTiming->nextTime;
}

/**
 * @brief Kills an actor (all that is needed is the local entity state set to STATE_DEAD).
 * @note Also changes the animation to a random death sequence and appends the dead animation
 * @param[in] msg The netchannel message
 * @param[in] self Pointer to the event structure that is currently executed
 */
void CL_ActorDie (const eventRegister_t* self, dbuffer* msg)
{
	int entnum, state, playerNum, attacker;

	NET_ReadFormat(msg, self->formatString, &entnum, &state, &playerNum, &attacker);

	/* get les */
	le_t* le = LE_Get(entnum);

	if (!le)
		LE_NotFoundError(entnum);

	if (!LE_IsLivingActor(le))
		Com_Error(ERR_DROP, "CL_ActorDie: Can't kill, LE is not an actor (type: %i)", le->type);

	if (!LE_IsStunned(le) && LE_IsDead(le))
		Com_Error(ERR_DROP, "CL_ActorDie: Can't kill, actor already dead");

	LE_Lock(le);

	/* set relevant vars */
	le->resetFloor();
	le->state = state;

	/* count spotted aliens */
	cl.numEnemiesSpotted = CL_CountVisibleEnemies();
	Cvar_SetValue("mn_numaliensspotted", cl.numEnemiesSpotted);

	/* play animation */
	LE_SetThink(le, nullptr);
	R_AnimChange(&le->as, le->model1, va("death%i", LE_GetAnimationIndexForDeath(le)));
	R_AnimAppend(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));

	/* Print some info about the death or stun. */
	if (le->team == cls.team) {
		if (playerNum != le->pnum) {
			const char* playerName = CL_PlayerGetName(playerNum);
			char tmpbuf[128];
			Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s lost a soldier\n"), playerName);
			HUD_DisplayMessage(tmpbuf);
		} else {
			const character_t* chr = CL_ActorGetChr(le);
			if (chr) {
				char tmpbuf[128];
				if (LE_IsStunned(le)) {
					Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s was stunned\n"), chr->name);
				} else if (!attacker) {
					Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s has died\n"), chr->name);
				} else {
					Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s was killed\n"), chr->name);
				}
				HUD_DisplayMessage(tmpbuf);
			}
		}
	} else {
		switch (le->team) {
		case (TEAM_CIVILIAN):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("A civilian was stunned."));
			else if (!attacker)
				HUD_DisplayMessage(_("A civilian has died."));
			else
				HUD_DisplayMessage(_("A civilian was killed."));
			break;
		case (TEAM_ALIEN):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("An alien was stunned."));
			else if (!attacker)
				HUD_DisplayMessage(_("An alien has died."));
			else
				HUD_DisplayMessage(_("An alien was killed."));
			break;
		case (TEAM_PHALANX):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("A soldier was stunned."));
			else if (!attacker)
				HUD_DisplayMessage(_("A soldier has died."));
			else
				HUD_DisplayMessage(_("A soldier was killed."));
			break;
		default:
			if (LE_IsStunned(le))
				HUD_DisplayMessage(va(_("A member of team %i was stunned."), le->team));
			else if (!attacker)
				HUD_DisplayMessage(va(_("A member of team %i has died."), le->team));
			else
				HUD_DisplayMessage(va(_("A member of team %i was killed."), le->team));
			break;
		}
	}

	/**
	 * @todo CHRSH_IsTeamDefRobot: spawn smoke particles for robots
	 */

	CL_ActorPlaySound(le, SND_DEATH);

	le->aabb.setMaxs(player_dead_maxs);
	if (!LE_IsStunned(le))
		le->contents = CONTENTS_DEADACTOR;
	if (le->ptl) {
		CL_ParticleFree(le->ptl);
		le->ptl = nullptr;
	}
	CL_ActorRemoveFromTeamList(le);

	/* update pathing as we maybe can walk onto the dead actor now */
	CL_ActorConditionalMoveCalc(selActor);
	LE_Unlock(le);
}
