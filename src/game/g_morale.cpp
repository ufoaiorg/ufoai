/**
 * @file g_morale.c
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

#include "g_local.h"


/**
 * @sa G_MoraleStopPanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoralePanic (edict_t * ent, qboolean sanity)
{
	G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("%s panics!\n"), ent->chr.name);

	/* drop items in hands */
	if (!sanity && ent->chr.teamDef->weapons) {
		if (RIGHT(ent))
			G_ActorInvMove(ent, INVDEF(gi.csi->idRight), RIGHT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, qtrue);
		if (LEFT(ent))
			G_ActorInvMove(ent, INVDEF(gi.csi->idLeft), LEFT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, qtrue);
	}

	/* get up */
	G_RemoveCrouched(ent);
	G_ActorSetMaxs(ent);

	/* send panic */
	G_SetPanic(ent);
	G_EventSendState(G_VisToPM(ent->visflags), ent);

	/* center view */
	G_EventCenterView(ent);

	/* move around a bit, try to avoid opponents */
	AI_ActorThink(G_PLAYER_FROM_ENT(ent), ent);

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
static void G_MoraleStopPanic (edict_t * ent)
{
	if (ent->morale / mor_panic->value > m_panic_stop->value * frand())
		G_RemovePanic(ent);
	else
		G_MoralePanic(ent, qtrue);
}

/**
 * @sa G_MoralePanic
 * @sa G_MoraleStopPanic
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleRage (edict_t * ent, qboolean sanity)
{
	if (sanity)
		G_SetRage(ent);
	else
		G_SetInsane(ent);
	G_EventSendState(G_VisToPM(ent->visflags), ent);

	if (sanity)
		gi.BroadcastPrintf(PRINT_HUD, _("%s is on a rampage.\n"), ent->chr.name);
	else
		gi.BroadcastPrintf(PRINT_HUD, _("%s is consumed by mad rage!\n"), ent->chr.name);
	AI_ActorThink(G_PLAYER_FROM_ENT(ent), ent);
}

/**
 * @brief Stops the rage state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopPanic
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopRage (edict_t * ent)
{
	if (ent->morale / mor_panic->value > m_rage_stop->value * frand()) {
		G_RemoveInsane(ent);
		G_EventSendState(G_VisToPM(ent->visflags), ent);
	} else
		G_MoralePanic(ent, qtrue); /* regains sanity */
}

/**
 * @brief Checks whether the morale handling is activated for this game. This is always
 * the case in singleplayer matches, and might be disabled for multiplayer matches.
 * @return @c true if the morale is activated for this game.
 */
static qboolean G_IsMoraleEnabled (void)
{
	if (sv_maxclients->integer == 1)
		return qtrue;

	if (sv_maxclients->integer >= 2 && sv_enablemorale->integer == 1)
		return qtrue;

	return qfalse;
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
	edict_t *ent = NULL;
	int newMorale;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* this only applies to ET_ACTOR but not to ET_ACTOR2x2 */
		if (ent->type == ET_ACTOR && ent->team == team && !G_IsDead(ent)) {
			/* civilians have a 1:1 chance to randomly run away in multiplayer */
			if (sv_maxclients->integer >= 2 && level.activeTeam == TEAM_CIVILIAN && 0.5 > frand())
				G_MoralePanic(ent, qfalse);
			/* multiplayer needs enabled sv_enablemorale */
			/* singleplayer has this in every case */
			if (G_IsMoraleEnabled()) {
				/* if panic, determine what kind of panic happens: */
				if (ent->morale <= mor_panic->value && !G_IsPaniced(ent) && !G_IsRaged(ent)) {
					qboolean sanity;
					if ((float) ent->morale / mor_panic->value > (m_sanity->value * frand()))
						sanity = qtrue;
					else
						sanity = qfalse;
					if ((float) ent->morale / mor_panic->value > (m_rage->value * frand()))
						G_MoralePanic(ent, sanity);
					else
						G_MoraleRage(ent, sanity);
					/* if shaken, well .. be shaken; */
				} else if (ent->morale <= mor_shaken->value && !G_IsPaniced(ent)
						&& !G_IsRaged(ent)) {
					/* shaken is later reset along with reaction fire */
					G_SetShaken(ent);
					G_SetState(ent, STATE_REACTION);
					G_EventSendState(G_VisToPM(ent->visflags), ent);
					G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("%s is currently shaken.\n"),
							ent->chr.name);
				} else {
					if (G_IsPaniced(ent))
						G_MoraleStopPanic(ent);
					else if (G_IsRaged(ent))
						G_MoraleStopRage(ent);
				}
			}

			G_ActorSetMaxs(ent);

			/* morale-regeneration, capped at max: */
			newMorale = ent->morale + MORALE_RANDOM(mor_regeneration->value);
			if (newMorale > GET_MORALE(ent->chr.score.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
			else
				ent->morale = newMorale;

			/* send phys data and state: */
			G_SendStats(ent);
			gi.EndEvents();
		}
	}
}
