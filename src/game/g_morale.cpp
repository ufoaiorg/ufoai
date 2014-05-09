/**
 * @file
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

#include "g_local.h"
#include "g_actor.h"
#include "g_ai.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_utils.h"

/**
 * @sa G_MoraleStopPanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoralePanic (Edict* ent)
{
	G_ClientPrintf(ent->getPlayer(), PRINT_HUD, _("%s panics!"), ent->chr.name);
	G_PrintStats("%s panics (entnum %i).", ent->chr.name, ent->getIdNum());
	/* drop items in hands */
	if (G_IsInsane(ent) && ent->chr.teamDef->weapons) {
		if (ent->getRightHandItem())
			G_ActorInvMove(ent, INVDEF(CID_RIGHT), ent->getRightHandItem(),
					INVDEF(CID_FLOOR), NONE, NONE, true);
		if (ent->getLeftHandItem())
			G_ActorInvMove(ent, INVDEF(CID_LEFT), ent->getLeftHandItem(),
					INVDEF(CID_FLOOR), NONE, NONE, true);
		G_ClientStateChange(ent->getPlayer(), ent, ~STATE_REACTION, false);
	}

	/* get up */
	G_RemoveCrouched(ent);
	G_ActorSetMaxs(ent);

	/* send panic */
	G_SetPanic(ent);
	G_EventSendState(G_VisToPM(ent->visflags), *ent);

	/* center view */
	G_EventCenterView(*ent);

	/* move around a bit, try to avoid opponents */
	AI_ActorThink(ent->getPlayer(), ent);

	/* kill TUs */
	G_ActorSetTU(ent, 0);
}

/**
 * @brief Stops the panic state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopPanic (Edict* ent)
{
	G_RemoveInsane(ent);
	if (ent->morale / mor_panic->value > m_panic_stop->value * frand()) {
		G_RemovePanic(ent);
		G_PrintStats("%s is no longer panicked (entnum %i).", ent->chr.name, ent->getIdNum());
		G_EventSendState(G_VisToPM(ent->visflags), *ent);
	} else {
		G_MoralePanic(ent);
	}
}

/**
 * @sa G_MoralePanic
 * @sa G_MoraleStopPanic
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleRage (Edict* ent)
{
	G_SetRage(ent);
	if (!G_IsInsane(ent)) {
		gi.BroadcastPrintf(PRINT_HUD, _("%s is on a rampage!"), ent->chr.name);
		G_PrintStats("%s is on a rampage (entnum %i).", ent->chr.name, ent->getIdNum());
	} else {
		gi.BroadcastPrintf(PRINT_HUD, _("%s is consumed by mad rage!"), ent->chr.name);
		G_PrintStats("%s is consumed by mad rage (entnum %i).", ent->chr.name, ent->getIdNum());
	}
	G_EventSendState(G_VisToPM(ent->visflags), *ent);
	G_ClientStateChange(ent->getPlayer(), ent, ~STATE_REACTION, false);

	AI_ActorThink(ent->getPlayer(), ent);
}

/**
 * @brief Stops the rage state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopPanic
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopRage (Edict* ent)
{
	 /* regains sanity */
	G_RemoveInsane(ent);
	if (ent->morale / mor_panic->value > m_rage_stop->value * frand()) {
		G_RemoveRage(ent);
		G_EventSendState(G_VisToPM(ent->visflags), *ent);
		G_PrintStats("%s is no longer insane (entnum %i).", ent->chr.name, ent->getIdNum());
	} else {
		G_MoralePanic(ent);
	}
}

/**
 * @brief Checks whether the morale handling is activated for this game. This is always
 * the case in singleplayer matches, and might be disabled for multiplayer matches.
 * @return @c true if the morale is activated for this game.
 */
static bool G_IsMoraleEnabled (int team)
{
	if (G_IsSinglePlayer())
		return true;

	/* multiplayer */
	if (team == TEAM_CIVILIAN || sv_enablemorale->integer == 1)
		return true;

	return false;
}

/**
 * @brief Applies morale behaviour on actors
 * @note only called when mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleStopPanic
 */
void G_MoraleBehaviour (int team)
{
	bool enabled = G_IsMoraleEnabled(team);
	if (!enabled)
		return;

	Actor* ent = nullptr;
	while ((ent = G_EdictsGetNextLivingActorOfTeam2(ent, team)) != nullptr) {
		/* this only applies to ET_ACTOR but not to ET_ACTOR2x2 */
		if (ent->type != ET_ACTOR || CHRSH_IsTeamDefRobot(ent->chr.teamDef))
			continue;

		/* if panic, determine what kind of panic happens: */
		if (!G_IsPanicked(ent) && !G_IsRaged(ent)) {
			if (ent->morale <= mor_panic->integer) {
				const float ratio = (float) ent->morale / mor_panic->value;
				const bool sanity = ratio > (m_sanity->value * frand());
				if (!sanity)
					G_SetInsane(ent);
				if (ratio > (m_rage->value * frand()))
					G_MoralePanic(ent);
				else
					G_MoraleRage(ent);
				/* if shaken, well .. be shaken; */
			} else if (ent->morale <= mor_shaken->integer) {
				/* shaken is later reset along with reaction fire */
				G_SetShaken(ent);
				G_ClientStateChange(ent->getPlayer(), ent, STATE_REACTION, false);
				G_EventSendState(G_VisToPM(ent->visflags), *ent);
				G_ClientPrintf(ent->getPlayer(), PRINT_HUD, _("%s is currently shaken."),
						ent->chr.name);
				G_PrintStats("%s is shaken (entnum %i).", ent->chr.name, ent->getIdNum());
			}
		} else {
			if (G_IsPanicked(ent))
				G_MoraleStopPanic(ent);
			else if (G_IsRaged(ent))
				G_MoraleStopRage(ent);
		}

		G_ActorSetMaxs(ent);

		/* morale-regeneration, capped at max: */
		int newMorale = ent->morale + MORALE_RANDOM(mor_regeneration->value);
		const int maxMorale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
		if (newMorale > maxMorale)
			ent->morale = maxMorale;
		else
			ent->morale = newMorale;

		/* send phys data and state: */
		G_SendStats(*ent);
	}
}
