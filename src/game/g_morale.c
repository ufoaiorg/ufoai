/**
 * @file g_morale.c
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

#include "g_local.h"


/**
 * @sa G_MoraleStopPanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoralePanic (edict_t * ent, qboolean sanity, qboolean quiet)
{
	G_PlayerPrintf(G_PLAYER_FROM_ENT(ent), PRINT_CONSOLE, _("%s panics!\n"), ent->chr.name);

	/* drop items in hands */
	if (!sanity && ent->chr.teamDef->weapons) {
		if (RIGHT(ent))
			G_ActorInvMove(ent->number, INVDEF(gi.csi->idRight), RIGHT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, qtrue, quiet);
		if (LEFT(ent))
			G_ActorInvMove(ent->number, INVDEF(gi.csi->idLeft), LEFT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, qtrue, quiet);
	}

	/* get up */
	ent->state &= ~STATE_CROUCHED;
	VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);

	/* send panic */
	ent->state |= STATE_PANIC;
	G_SendState(G_VisToPM(ent->visflags), ent);

	/* center view */
	gi.AddEvent(G_VisToPM(ent->visflags), EV_CENTERVIEW);
	gi.WriteGPos(ent->pos);

	/* move around a bit, try to avoid opponents */
	AI_ActorThink(G_PLAYER_FROM_ENT(ent), ent);

	/* kill TUs */
	ent->TU = 0;
}

/**
 * @brief Stops the panic state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopPanic (edict_t * ent, qboolean quiet)
{
	if (ent->morale / mor_panic->value > m_panic_stop->value * frand())
		ent->state &= ~STATE_PANIC;
	else
		G_MoralePanic(ent, qtrue, quiet);
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
		ent->state |= STATE_RAGE;
	else
		ent->state |= STATE_INSANE;
	G_SendState(G_VisToPM(ent->visflags), ent);

	if (sanity)
		gi.BroadcastPrintf(PRINT_CONSOLE, _("%s is on a rampage.\n"), ent->chr.name);
	else
		gi.BroadcastPrintf(PRINT_CONSOLE, _("%s is consumed by mad rage!\n"), ent->chr.name);
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
static void G_MoraleStopRage (edict_t * ent, qboolean quiet)
{
	if (ent->morale / mor_panic->value > m_rage_stop->value * frand()) {
		ent->state &= ~STATE_INSANE;
		G_SendState(G_VisToPM(ent->visflags), ent);
	} else
		G_MoralePanic(ent, qtrue, quiet); /*regains sanity */
}

/**
 * @brief Applies morale behaviour on actors
 * @note only called when mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleStopPanic
 */
void G_MoraleBehaviour (int team, qboolean quiet)
{
	edict_t *ent;
	int i, newMorale;
	qboolean sanity;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not to ET_ACTOR2x2 */
		if (ent->inuse && ent->type == ET_ACTOR && ent->team == team && !G_IsDead(ent)) {
			/* civilians have a 1:1 chance to randomly run away in multiplayer */
			if (sv_maxclients->integer >= 2 && level.activeTeam == TEAM_CIVILIAN && 0.5 > frand())
				G_MoralePanic(ent, qfalse, quiet);
			/* multiplayer needs enabled sv_enablemorale */
			/* singleplayer has this in every case */
			if ((sv_maxclients->integer >= 2 && sv_enablemorale->integer == 1) || sv_maxclients->integer == 1) {
				/* if panic, determine what kind of panic happens: */
				if (ent->morale <= mor_panic->value && !(ent->state & STATE_PANIC) && !(ent->state & STATE_RAGE)) {
					if ((float) ent->morale / mor_panic->value > (m_sanity->value * frand()))
						sanity = qtrue;
					else
						sanity = qfalse;
					if ((float) ent->morale / mor_panic->value > (m_rage->value * frand()))
						G_MoralePanic(ent, sanity, quiet);
					else
						G_MoraleRage(ent, sanity);
					/* if shaken, well .. be shaken; */
				} else if (ent->morale <= mor_shaken->value && !(ent->state & STATE_PANIC)
						&& !(ent->state & STATE_RAGE)) {
					/* shaken is later reset along with reaction fire */
					ent->state |= STATE_SHAKEN | STATE_REACTION_MANY;
					G_SendState(G_VisToPM(ent->visflags), ent);
					G_PlayerPrintf(G_PLAYER_FROM_ENT(ent), PRINT_CONSOLE, _("%s is currently shaken.\n"),
							ent->chr.name);
				} else {
					if (ent->state & STATE_PANIC)
						G_MoraleStopPanic(ent, quiet);
					else if (ent->state & STATE_RAGE)
						G_MoraleStopRage(ent, quiet);
				}
			}
			/* set correct bounding box */
			if (ent->state & (STATE_CROUCHED | STATE_PANIC))
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
			else
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);

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
