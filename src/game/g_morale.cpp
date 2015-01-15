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
static void G_MoralePanic (Actor* actor)
{
	G_ClientPrintf(actor->getPlayer(), PRINT_HUD, _("%s panics!"), actor->chr.name);
	G_PrintStats("%s panics (entnum %i).", actor->chr.name, actor->getIdNum());
	/* drop items in hands */
	if (actor->isInsane() && actor->chr.teamDef->weapons) {
		if (actor->getRightHandItem())
			G_ActorInvMove(actor, INVDEF(CID_RIGHT), actor->getRightHandItem(),
					INVDEF(CID_FLOOR), NONE, NONE, true);
		if (actor->getLeftHandItem())
			G_ActorInvMove(actor, INVDEF(CID_LEFT), actor->getLeftHandItem(),
					INVDEF(CID_FLOOR), NONE, NONE, true);
		G_ClientStateChange(actor->getPlayer(), actor, ~STATE_REACTION, false);
	}

	/* get up */
	actor->removeCrouched();
	G_ActorSetMaxs(actor);

	/* send panic */
	actor->setPanicked();
	G_EventSendState(G_VisToPM(actor->visflags), *actor);

	/* center view */
	G_EventCenterView(*actor);

	/* move around a bit, try to avoid opponents */
	AI_ActorRun(actor->getPlayer(), actor);

	/* kill TUs */
	G_ActorSetTU(actor, 0);
}

/**
 * @brief Stops the panic state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopPanic (Actor* actor)
{
	actor->removeInsane();
	if (actor->morale / mor_panic->value > m_panic_stop->value * frand()) {
		actor->removePanicked();
		G_PrintStats("%s is no longer panicked (entnum %i).", actor->chr.name, actor->getIdNum());
		G_EventSendState(G_VisToPM(actor->visflags), *actor);
	} else {
		G_MoralePanic(actor);
	}
}

/**
 * @sa G_MoralePanic
 * @sa G_MoraleStopPanic
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleRage (Actor* actor)
{
	actor->setRaged();
	if (!actor->isInsane()) {
		gi.BroadcastPrintf(PRINT_HUD, _("%s is on a rampage!"), actor->chr.name);
		G_PrintStats("%s is on a rampage (entnum %i).", actor->chr.name, actor->getIdNum());
	} else {
		gi.BroadcastPrintf(PRINT_HUD, _("%s is consumed by mad rage!"), actor->chr.name);
		G_PrintStats("%s is consumed by mad rage (entnum %i).", actor->chr.name, actor->getIdNum());
	}
	G_EventSendState(G_VisToPM(actor->visflags), *actor);
	G_ClientStateChange(actor->getPlayer(), actor, ~STATE_REACTION, false);

	AI_ActorRun(actor->getPlayer(), actor);
}

/**
 * @brief Stops the rage state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopPanic
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopRage (Actor* actor)
{
	 /* regains sanity */
	actor->removeInsane();
	if (actor->morale / mor_panic->value > m_rage_stop->value * frand()) {
		actor->removeRaged();
		G_EventSendState(G_VisToPM(actor->visflags), *actor);
		G_PrintStats("%s is no longer insane (entnum %i).", actor->chr.name, actor->getIdNum());
	} else {
		G_MoralePanic(actor);
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
	const bool enabled = G_IsMoraleEnabled(team);
	if (!enabled)
		return;

	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team)) != nullptr) {
		/* this only applies to ET_ACTOR but not to ET_ACTOR2x2 */
		if (actor->type != ET_ACTOR || CHRSH_IsTeamDefRobot(actor->chr.teamDef))
			continue;

		/* if panic, determine what kind of panic happens: */
		if (!actor->isPanicked() && !actor->isRaged()) {
			if (actor->morale <= mor_panic->integer) {
				const float ratio = (float) actor->morale / mor_panic->value;
				const bool sanity = ratio > (m_sanity->value * frand());
				if (!sanity)
					actor->setInsane();
				if (ratio > (m_rage->value * frand()))
					G_MoralePanic(actor);
				else
					G_MoraleRage(actor);
				/* if shaken, well .. be shaken; */
			} else if (actor->morale <= mor_shaken->integer) {
				/* shaken is later reset along with reaction fire */
				actor->setShaken();
				G_ClientStateChange(actor->getPlayer(), actor, STATE_REACTION, false);
				G_EventSendState(G_VisToPM(actor->visflags), *actor);
				G_ClientPrintf(actor->getPlayer(), PRINT_HUD, _("%s is currently shaken."),
						actor->chr.name);
				G_PrintStats("%s is shaken (entnum %i).", actor->chr.name, actor->getIdNum());
			}
		} else {
			if (actor->isPanicked())
				G_MoraleStopPanic(actor);
			else if (actor->isRaged())
				G_MoraleStopRage(actor);
		}

		G_ActorSetMaxs(actor);

		/* morale-regeneration, capped at max: */
		const int newMorale = actor->morale + MORALE_RANDOM(mor_regeneration->value);
		const int maxMorale = GET_MORALE(actor->chr.score.skills[ABILITY_MIND]);
		if (newMorale > maxMorale)
			actor->morale = maxMorale;
		else
			actor->morale = newMorale;

		/* send phys data and state: */
		G_SendStats(*actor);
	}
}
