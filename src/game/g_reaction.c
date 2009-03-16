/**
 * @file g_reaction.c
 * @brief Reaction fire code
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

static int G_GetFiringTUsForItem (edict_t *ent, edict_t *target, int *fire_hand_type, int *firemode, invList_t *invList)
{
	/* Fire the weapon in the right hand if everything is ok. */
	if (invList && invList->item.m && invList->item.t->weapon
	 && (!invList->item.t->reload || invList->item.a > 0)) {
		const fireDef_t *fdArray = FIRESH_FiredefsIDXForWeapon(&invList->item);
		if (fdArray == NULL)
			return -1;

		if (ent->chr.RFmode.hand == 0 && ent->chr.RFmode.fmIdx >= 0
		 && ent->chr.RFmode.fmIdx < MAX_FIREDEFS_PER_WEAPON) { /* If a RIGHT-hand firemode is selected and sane. */
			*firemode = ent->chr.RFmode.fmIdx; /* Get selected (if any) firemode for the weapon in the right hand. */

			if (fdArray[*firemode].time + sv_reaction_leftover->integer <= ent->TU
			 && fdArray[*firemode].range > VectorDist(ent->origin, target->origin)) {
				if (fire_hand_type)
					*fire_hand_type = ST_RIGHT_REACTION;

				Com_DPrintf(DEBUG_GAME, "G_GetFiringTUs: entnumber:%i firemode:%i entteam:%i\n",
					ent->number, *firemode, ent->team);
				return fdArray[*firemode].time + sv_reaction_leftover->integer;
			}
		}
	}

	return -1;
}

/**
 * @brief Get the number of TUs that ent needs to fire at target, also optionally return the firing hand. Used for reaction fire.
 * @param[in] ent The shooter entity.
 * @param[in] target The target entity.
 * @param[out] fire_hand_type If not NULL then this stores the hand (combined with the 'reaction' info) that the shooter will fire with.
 * @param[out] firemode The firemode that will be used for the shot.
 * @returns The number of TUs required to fire or -1 if firing is not possible
 * @sa G_CheckRFTrigger
 */
static int G_GetFiringTUs (edict_t *ent, edict_t *target, int *fire_hand_type, int *firemode)
{
	int TUs;
	int tmp = -2;

	/* The caller doesn't use this parameter, use a temporary one instead. */
	if (!firemode)
		firemode = &tmp;

	TUs = G_GetFiringTUsForItem(ent, target, fire_hand_type, firemode, RIGHT(ent));
	if (TUs != -1)
		return TUs;
	TUs = G_GetFiringTUsForItem(ent, target, fire_hand_type, firemode, LEFT(ent));
	if (TUs != -1)
		return TUs;

	return -1;
}

/**
 * @brief Check whether ent can reaction fire at target, i.e. that it can see it and neither is dead etc.
 * @param[in] ent The entity that might be firing
 * @param[in] target The entity that might be fired at
 * @param[out] reason If not null then it prints the reason that reaction fire wasn't possible
 * @returns Whether 'ent' can actually fire at 'target'
 */
static qboolean G_CanReactionFire (edict_t *ent, edict_t *target, char **reason)
{
	float actorVis;
	qboolean frustum;

	/* an entity can't reaction fire at itself */
	if (ent == target) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "Cannot fire on self";
		return qfalse;
#endif
	}

	/* check ent is a suitable shooter */
	if (!ent->inuse || !G_IsLivingActor(ent)) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "Shooter is not ent, is non-actor or is dead";
#endif
		return qfalse;
	}

	/* check ent has reaction fire enabled */
	if (!(ent->state & STATE_SHAKEN) && !(ent->state & STATE_REACTION)) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "Shooter does not have reaction fire enabled";
#endif
		return qfalse;
	}

	/* check in range and visible */
	if (VectorDistSqr(ent->origin, target->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "Target is out of range";
#endif
		return qfalse;
	}

	actorVis = G_ActorVis(ent->origin, target, qtrue);
	frustum = G_FrustumVis(ent, target->origin);
	if (actorVis <= 0.2 || !frustum) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "Target is not visible";
#endif
		return qfalse;
	}

	/* If reaction fire is triggered by a friendly unit
	 * and the shooter is still sane, don't shoot;
	 * well, if the shooter isn't sane anymore... */
	if (target->team == TEAM_CIVILIAN || target->team == ent->team)
		if (!(ent->state & STATE_SHAKEN) || (float) ent->morale / mor_shaken->value > frand()) {
#ifdef DEBUG_REACTION
			if (reason)
				*reason = "Shooter will not fire on friendly";
#endif
			return qfalse;
		}

	/* Don't react in your own turn, trust your commander. Can't use
	 * level.activeTeam, because this function touches it recursively. */
	if (ent->team == turnTeam) {
#ifdef DEBUG_REACTION
		if (reason)
			*reason = "It's the shooter's turn";
#endif
		return qfalse;
	}

	/* okay do it then */
	return qtrue;
}

/**
 * @brief Check whether 'target' has just triggered any new reaction fire
 * @param[in] target The entity triggering fire
 * @returns qtrue if some entity initiated firing
 * @sa G_CanReactionFire
 * @sa G_GetFiringTUs
 */
static qboolean G_CheckRFTrigger (edict_t *target)
{
	edict_t *ent;
	int i, tus;
	qboolean queued = qfalse;
	int firemode = -3;

	/* check all possible shooters */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		/* not if ent has reaction target already */
		if (ent->reactionTarget)
			continue;

		/* check whether reaction fire is possible */
		if (!G_CanReactionFire(ent, target, NULL))
			continue;

		/* see how quickly ent can fire (if it can fire at all) */
		tus = G_GetFiringTUs(ent, target, NULL, &firemode);
		if (tus < 0)
			continue;

		/* Check if a firemode has been set by the client. */
		if (firemode < 0)
			continue;

		/* queue a reaction fire to take place */
		ent->reactionTarget = target;
		target->reactionAttacker = ent;
		ent->reactionTUs = max(0, target->TU - (tus / 4.0));
		ent->reactionNoDraw = qfalse;
		queued = qtrue;

		/** @todo generate an 'interrupt'? */
#ifdef DEBUG_REACTION
		Com_Printf("Entity %s begins reaction fire on %s\n", ent->chr.name, target->chr.name);
#endif
	}
	return queued;
}

/**
 * @brief
 * @todo This seems to be the function that is called for reaction fire isn't it?
 * @param[in] player The player this action belongs to (the human player or the ai)
 * @param[in] shooter The actor that is trying to shoot
 * @param[in] at Position to fire on.
 * @param[in] type What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon (objDef.fd[][x])  .
 * @return qtrue if everthing went ok (i.e. the shot(s) where fired ok), otherwise qfalse.
 * @sa G_ClientShoot
 */
static qboolean G_FireWithJudgementCall (player_t *player, edict_t *shooter, pos3_t at, int type, int firemode)
{
	const int minhit = shooter->reaction_minhit;
	shot_mock_t mock;
	int ff, i, maxff;

	if (shooter->state & STATE_INSANE)
		maxff = 100;
	else if (shooter->state & STATE_RAGE)
		maxff = 60;
	else if (shooter->state & STATE_PANIC)
		maxff = 30;
	else if (shooter->state & STATE_SHAKEN)
		maxff = 15;
	else
		maxff = 5;

	memset(&mock, 0, sizeof(mock));
	for (i = 0; i < 100; i++)
		G_ClientShoot(player, shooter->number, at, type, firemode, &mock, qfalse, 0);

	Com_DPrintf(DEBUG_GAME, "G_FireWithJudgementCall: Hit: %d/%d FF+Civ: %d+%d=%d/%d Self: %d.\n",
		mock.enemy, minhit, mock.friend, mock.civilian, mock.friend + mock.civilian, maxff, mock.self);

	ff = mock.friend + (shooter->team == TEAM_ALIEN ? 0 : mock.civilian);
	if (ff <= maxff && mock.enemy >= minhit)
		return G_ClientShoot(player, shooter->number, at, type, firemode, NULL, qfalse, 0);
	else
		return qfalse;
}

/**
 * @brief Resolve the reaction fire for an entity, this checks that the entity can fire and then takes the shot
 * @param[in] ent The entity to resolve reaction fire for
 * @param[in] mock If true then don't actually fire
 * @return true if the entity fired (or would have fired if mock), false otherwise
 */
static qboolean G_ResolveRF (edict_t *ent, qboolean mock)
{
	player_t *player;
	int tus, fire_hand_type, team;
	int firemode = -1;
	qboolean tookShot;
	char *reason = NULL;

	/* check whether this ent has a reaction fire queued */
	if (!ent->reactionTarget) {
#ifdef DEBUG_REACTION
		if (!mock)
			Com_Printf("Not resolving reaction fire for %s because ent has no target (which shouldn't happen)\n", ent->chr.name);
#endif
		return qfalse;
	}

	/* ent can't use RF if is in STATE_DAZED (flashbang impact) */
	if (ent->state & STATE_DAZED) {
#ifdef DEBUG
		Com_Printf("This entity is in STATE_DAZED, will not use reaction fire.\n");
#endif
		return qfalse;
	}

	/* ent can't take a reaction shot if it's not possible */
	if (!G_CanReactionFire(ent, ent->reactionTarget, &reason)) {
		ent->reactionTarget = NULL;
#ifdef DEBUG_REACTION
		if (!mock)
			Com_Printf("Not resolving reaction fire for %s because '%s'\n", ent->chr.name, reason);
#endif
		return qfalse;
	}

	/* check the target is still alive */
	if (G_IsDead(ent->reactionTarget)) {
		ent->reactionTarget = NULL;
#ifdef DEBUG_REACTION
		if (!mock)
			Com_Printf("Not resolving reaction fire for %s because target is dead\n", ent->chr.name);
#endif
		return qfalse;
	}

	/* check ent can fire (necessary? covered by G_CanReactionFire?) */
	tus = G_GetFiringTUs(ent, ent->reactionTarget, &fire_hand_type, &firemode);
	if (tus < 0) {
#ifdef DEBUG_REACTION
		if (!mock)
			Com_Printf("Cancelling resolution because %s cannot fire\n", ent->chr.name);
#endif
		return qfalse;
	}

	/* Check if a firemode has been set by the client. */
	if (firemode < 0) {
		if (!mock)
			Com_DPrintf(DEBUG_GAME, "G_ResolveRF: Cancelling resolution because %s has not set a reaction-firemode (%i).\n", ent->chr.name, firemode);
		return qfalse;
	}

	/* Get player. */
	player = game.players + ent->pnum;
	if (!player) {
#ifdef DEBUG_REACTION
		if (!mock)
			Com_Printf("Cancelling resolution because %s has no player\n", ent->chr.name);
#endif
		return qfalse;
	}

	/* take the shot */
	if (mock)
		/* if just pretending then this is far enough */
		return qtrue;

	/* Change active team for this shot only. */
	team = level.activeTeam;
	level.activeTeam = ent->team;

	/* take the shot */
	Com_DPrintf(DEBUG_GAME, "G_ResolveRF: reaction shot: fd:%i\n", firemode);
	tookShot = G_FireWithJudgementCall(player, ent, ent->reactionTarget->pos, fire_hand_type, firemode);

	/* Revert active team. */
	level.activeTeam = team;

	/* clear any shakenness */
	if (tookShot) {
		ent->state &= ~STATE_SHAKEN;
		/* Save the fact that the ent has fired. */
		ent->reactionFired += 1;
	} else {
#ifdef DEBUG_REACTION
		Com_Printf("Cancelling resolution because %s judged it unwise to fire\n", ent->chr.name);
#endif
	}
	return tookShot;
}

/**
 * @brief Check all entities to see whether target has caused reaction fire to resolve.
 * @param[in] target The entity that might be resolving reaction fire
 * @param[in] mock If true then don't actually fire
 * @returns whether any entity fired (or would fire) upon target
 * @sa G_ReactToMove
 * @sa G_ReactToPostFire
 */
static qboolean G_CheckRFResolution (edict_t *target, qboolean mock)
{
	edict_t *ent;
	int i;
	qboolean fired = qfalse, shoot = qfalse;

	/* check all possible shooters */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->reactionTarget)
			continue;

		shoot = qfalse;

		/* check whether target has changed (i.e. the player is making a move with a different entity) */
		if (ent->reactionTarget != target) {
#ifdef DEBUG_REACTION
			if (!mock)
				Com_Printf("Resolving reaction fire for %s because target has changed\n", ent->chr.name);
#endif
			shoot = qtrue;
		}

		/* check whether target is out of time */
		if (!shoot  && ent->reactionTarget->TU < ent->reactionTUs) {
#ifdef DEBUG_REACTION
			if (!mock)
				Com_Printf("Resolving reaction fire for %s because target is out of time\n", ent->chr.name);
#endif
			shoot = qtrue;
		}

		/* okay do it */
		if (shoot)
			fired |= G_ResolveRF(ent, mock);
	}
	return fired;
}

/**
 * @brief Called when 'target' moves, possibly triggering or resolving reaction fire
 * @param[in] target The target entity
 * @param[in] mock If true then don't actually fire just say whether someone would
 * @returns true If any shots were (or would be) taken
 * @sa G_ClientMove
 */
qboolean G_ReactToMove (edict_t *target, qboolean mock)
{
	/* Check to see whether this resolves any reaction fire */
	const qboolean fired = G_CheckRFResolution(target, mock);

	/* Check to see whether this triggers any reaction fire */
	G_CheckRFTrigger(target);

	return fired;
}

/**
 * @brief Called when 'target' is about to fire, this forces a 'draw' to decide who gets to fire first
 * @param[in] target The entity about to fire
 * @sa G_ClientShoot
 */
void G_ReactToPreFire (edict_t *target)
{
	edict_t *ent;
	int i, entTUs, targTUs;
	int firemode = -4;

	/* check all ents to see who wins and who loses a draw */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->reactionTarget)
			continue;
		if (ent->reactionTarget != target) {
			/* if the entity has changed then resolve the reaction fire */
			G_ResolveRF(ent, qfalse);
			continue;
		}
		/* check this ent hasn't already lost the draw */
		if (ent->reactionNoDraw)
			continue;

		/* draw!! */
		entTUs = G_GetFiringTUs(ent, target, NULL, &firemode);
		targTUs = G_GetFiringTUs(target, ent, NULL, NULL);
		if ((entTUs < 0) || (firemode < 0)) {
			/* can't reaction fire if no TUs to fire */
			ent->reactionTarget = NULL;
			continue;
		}

		/* see who won */
		if (entTUs >= targTUs) {
			/* target wins, so delay ent */
			ent->reactionTUs = max(0, target->TU - (entTUs - targTUs)); /* target gets the difference in TUs */
			ent->reactionNoDraw = qtrue; 								/* so ent can't lose the TU battle again */
#ifdef DEBUG_REACTION
			Com_Printf("Entity %s was out-drawn\n", ent->chr.name);
#endif
		} else {
			/* ent wins so take the shot */
#ifdef DEBUG_REACTION
			Com_Printf("Entity %s won the draw\n", ent->chr.name);
#endif
			G_ResolveRF(ent, qfalse);
		}
	}
}

/**
 * @brief Called after 'target' has fired, this might trigger more reaction fire or resolve outstanding reaction fire (because target is out of time)
 * @param[in] target The entity that has just fired
 * @sa G_ClientShoot
 */
void G_ReactToPostFire (edict_t *target)
{
	/* same as movement, but never mocked */
	G_ReactToMove(target, qfalse);
}

/**
 * @brief Called at the end of turn, all outstanding reaction fire is resolved
 * @sa G_ClientEndRound
 */
void G_ReactToEndTurn (void)
{
	edict_t *ent;
	int i;

	/* resolve all outstanding reaction firing if possible */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->reactionTarget)
			continue;
#ifdef DEBUG_REACTION
		Com_Printf("Resolving reaction fire for %s because the other player ended their turn\n", ent->chr.name);
#endif
		G_ResolveRF(ent, qfalse);
		ent->reactionTarget = NULL;
		ent->reactionFired = 0;
	}
}

/**
 * @brief Guess! Reset all "shaken" states on end of turn?
 * @note Normally called on end of turn.
 * @todo Comment on the AddEvent code.
 * @sa G_ClientStateChange
 * @param[in] team Index of team to loop through.
 */
void G_ResetReactionFire (int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == team) {
			/** @todo why do we send the state here and why do we change the "shaken" state? */
			ent->state &= ~STATE_SHAKEN;
			gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->state);
		}
}
