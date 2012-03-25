/**
 * @file g_reaction.c
 * @brief Reaction fire code
 * Reaction fire is involved in the following situations:
 * 1. G_ReactionFireOnMovement()
 *		calls	G_ReactionFireCheckExecution()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFireSearchTarget() (equiv. to RFT_Update)
 * 2. G_ClientShoot()
 *		calls	G_ReactionFirePreShot()
 *				calls	G_ReactionFireSearchTarget()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFirePostShot()
 *				calls	G_ReactionFireCheckExecution()
 *						calls	G_ReactionFireTryToShoot()
 * 3. G_ClientEndRound()
 *		calls	G_ReactionFireOnEndTurn()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFireReset()
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

#define MAX_RF_TARGETS 10
#define MAX_RF_DATA 50

typedef struct reactionFireTarget
{
	edict_t const *target;
	int triggerTUs;		/* the amount of TUS of the target(!) at which the reaction takes place */
} reactionFireTarget_t;

typedef struct reactionFireTargets
{
	int entnum;
	int count;
	reactionFireTarget_t targets[MAX_RF_TARGETS];
} reactionFireTargets_t;

static reactionFireTargets_t rfData[MAX_RF_DATA];

/**
 * @brief Initialize the reaction fire table for all entities.
 */
void G_ReactionFireTargetsInit (void)
{
	int i;

	for (i = 0; i < MAX_RF_DATA; i++) {
		rfData[i].entnum = -1;
		rfData[i].count = 0;
	}
}

/**
 * @brief Create a table of reaction fire targets for the given edict.
 * @param[in] shooter The reaction firing actor
 */
void G_ReactionFireTargetsCreate (const edict_t *shooter)
{
	int i;

	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == shooter->number)
			gi.Error("Entity already has rfData");
	}
	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == -1) {
			rfData[i].entnum = shooter->number;
			break;
		}
	}
#if 1
	if (i == MAX_RF_DATA)
		gi.Error("Not enough rfData");
#endif
}

/**
 * @brief Add a reaction fire target for the given shooter.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @param[in] tusForShot The TUs needed for the shot
 */
static void G_ReactionFireTargetsAdd (const edict_t *shooter, const edict_t *target, const int tusForShot)
{
	int i;
	reactionFireTargets_t *rfts = NULL;

	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == shooter->number)
			rfts = &rfData[i];
	}

	assert(rfts);
	assert(target);

	for (i = 0; i < rfts->count; i++) {
		if (rfts->targets[i].target == target)	/* found it ? */
			return;								/* shooter already knows that target */
	}
	assert(i < MAX_RF_TARGETS);
	rfts->targets[i].target = target;
	rfts->targets[i].triggerTUs = target->TU - tusForShot;
	rfts->count++;
}

/**
 * @brief Remove a reaction fire target for the given shooter.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 */
static void G_ReactionFireTargetsRemove (edict_t *shooter, const edict_t *target)
{
	int i;
	reactionFireTargets_t *rfts = NULL;

	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == shooter->number)
			rfts = &rfData[i];
	}

	assert(rfts);
	assert(target);

	for (i = 0; i < rfts->count; i++) {
		if (rfts->targets[i].target == target) {	/* found it ? */
			if (i != rfts->count - 1) {				/* not the last one ? */
				rfts->targets[i].target = rfts->targets[rfts->count - 1].target;
				rfts->targets[i].triggerTUs = rfts->targets[rfts->count - 1].triggerTUs;
			}
			rfts->count--;
		}
	}
	/* support the legacy RF code */
	shooter->reactionTarget = NULL;
}

/**
 * @brief Check if the given shooter is ready to reaction fire at the given target.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @param[in] tusTarget The TUs the target will need for the shot, 0 for just moving
 */
static qboolean G_ReactionFireTargetsExpired (const edict_t *shooter, const edict_t *target, const int tusTarget)
{
	int i;
	reactionFireTargets_t *rfts = NULL;

	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == shooter->number) {
			rfts = &rfData[i];
			break;
		}
	}

	if (!rfts)
		return qfalse;	/* the shooter doesn't aim at anything */

	assert(target);

	for (i = 0; i < rfts->count; i++) {
		if (rfts->targets[i].target == target)	/* found it ? */
			return rfts->targets[i].triggerTUs >= target->TU - tusTarget;
	}

	return qfalse;	/* the shooter doesn't aim at this target */
}


/**
 * @brief Increase the triggertime for the next RF shot (after a reaction fire at the given target).
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @param[in] tusShot The TUs the shooter will need for the next shot
 */
static void G_ReactionFireTargetsAdvance (const edict_t *shooter, const edict_t *target, const int tusShot)
{
	int i;
	reactionFireTargets_t *rfts = NULL;

	for (i = 0; i < MAX_RF_DATA; i++) {
		if (rfData[i].entnum == shooter->number) {
			rfts = &rfData[i];
			break;
		}
	}

	assert(rfts);
	assert(target);

	for (i = 0; i < rfts->count; i++) {
		if (rfts->targets[i].target == target)	/* found it ? */
			rfts->targets[i].triggerTUs -= tusShot;
	}
}


/**
 * @brief Get the weapon firing TUs of the item in the right hand of the edict.
 * @return -1 if no firedef was found for the item or the reaction fire mode is not activated for the right hand.
 * @todo why only right hand?
 * @param[in] ent The reaction firing actor
 * @param[in] target The target to check reaction fire for (e.g. check whether the weapon that was marked for
 * using in reaction fire situations can handle the distance between the shooter and the target)
 * @param[in] invList The items that are checked for reaction fire
 * @note This does 'not' return the weapon (lowest TU costs, highest damage, highest accuracy) but the first weapon that
 * would fit for reaction fire.
 */
static int G_ReactionFireGetTUsForItem (const edict_t *ent, const edict_t *target, const invList_t *invList)
{
	if (invList && invList->item.m && invList->item.t->weapon
	 && (!invList->item.t->reload || invList->item.a > 0)) {
		const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&invList->item);
		const chrFiremodeSettings_t *fmSetting;
		if (fdArray == NULL)
			return -1;

		fmSetting = &ent->chr.RFmode;
		if (fmSetting->hand == ACTOR_HAND_RIGHT && fmSetting->fmIdx >= 0
		 && fmSetting->fmIdx < MAX_FIREDEFS_PER_WEAPON) { /* If a RIGHT-hand firemode is selected and sane. */
			const fireDefIndex_t fmIdx = fmSetting->fmIdx;
			const int reactionFire = G_PLAYER_FROM_ENT(ent)->reactionLeftover;
			const fireDef_t *fd = &fdArray[fmIdx];
			const int tus = fd->time + reactionFire;

			if (tus <= ent->TU && fd->range > VectorDist(ent->origin, target->origin)) {
				return tus;
			}
		}
	}

	return -1;
}

/**
 * @brief Checks whether the actor has a reaction fire enabled weapon in one of his hands.
 * @param[in] ent The actor to check the weapons for
 * @return @c NULL if no actor has not reaction fire enabled weapons, the fire definition otherwise.
 */
static qboolean G_ActorHasReactionFireEnabledWeapon (const edict_t *ent)
{
	const objDef_t *weapon = INVSH_HasReactionFireEnabledWeapon(RIGHT(ent));
	if (weapon)
		return qtrue;
	return INVSH_HasReactionFireEnabledWeapon(LEFT(ent)) != NULL;
}

/**
 * @brief Checks if the currently selected firemode is usable with the defined weapon.
 * @param[in] actor The actor to check the firemode for.
 */
static qboolean G_ActorHasWorkingFireModeSet (const edict_t *actor)
{
	const fireDef_t *fd;
	const chrFiremodeSettings_t *fmSettings = &actor->chr.RFmode;
	const invList_t* invList;

	if (!SANE_FIREMODE(fmSettings))
		return qfalse;

	invList = ACTOR_GET_INV(actor, fmSettings->hand);
	if (!invList)
		return qfalse;
	fd = FIRESH_FiredefForWeapon(&invList->item);
	if (fd == NULL)
		return qfalse;

	if (fd->obj->weapons[fd->weapFdsIdx] == fmSettings->weapon && fmSettings->fmIdx
			< fd->obj->numFiredefs[fd->weapFdsIdx]) {
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Updates the reaction fire settings in case something was moved into a hand or from a hand
 * that would make the current settings invalid
 * @param[in,out] ent The actor edict to check the settings for
 * @param[in] fmIdx The fire mode index that should be used for reaction fire
 * @param[in] hand The hand that should be used for reaction fire
 * @param[in] od The object/weapon for the reaction fire
 */
void G_ReactionFireUpdate (edict_t *ent, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t *od)
{
	chrFiremodeSettings_t *fm = &ent->chr.RFmode;
	fm->fmIdx = fmIdx;
	fm->hand = hand;
	fm->weapon = od;

	if (!G_ActorHasWorkingFireModeSet(ent)) {
		/* Disable reaction fire if no valid firemode was found. */
		G_ClientStateChange(G_PLAYER_FROM_ENT(ent), ent, ~STATE_REACTION, qtrue);
		return;
	}

	G_EventReactionFireChange(ent);

	/* If reaction fire is active, update the reserved TUs */
	if (G_IsReaction(ent)) {
		G_ReactionFireSettingsReserveTUs(ent);
	}
}

/**
 * @brief Checks whether an actor has enough TUs left to activate reaction fire.
 * @param[in] ent The actors edict to check for TUs for
 * @return @c true if the given actor has enough TUs left to activate reaction fire, @c false otherwise.
 */
static qboolean G_ActorHasEnoughTUsReactionFire (const edict_t *ent)
{
	const int TUs = G_ActorGetTUForReactionFire(ent);
	const chrReservations_t *res = &ent->chr.reservedTus;
	return ent->TU - TUs >= res->shot + res->crouch;
}

/**
 * @param ent The actor to set the reaction fire for
 * @return @c true if the needed settings could have been made or settings are
 * already valid, @c false otherwise.
 */
static qboolean G_ReactionFireSetDefault (edict_t *ent)
{
	const objDef_t *weapon;
	const invList_t *invList;
	actorHands_t hand = ACTOR_HAND_RIGHT;

	if (G_ActorHasWorkingFireModeSet(ent))
		return qtrue;

	invList = ACTOR_GET_INV(ent, hand);
	if (!invList) {
		hand = ACTOR_HAND_LEFT;
		invList = ACTOR_GET_INV(ent, hand);
	}

	weapon = INVSH_HasReactionFireEnabledWeapon(invList);
	if (!weapon)
		return qfalse;

	ent->chr.RFmode.fmIdx = 0;
	ent->chr.RFmode.hand = hand;
	ent->chr.RFmode.weapon = weapon;

	if (!G_IsAI(ent))
		G_EventReactionFireChange(ent);

	return qtrue;
}

/**
 * @brief Checks whether the actor is allowed to activate reaction fire and will informs the player about
 * the reason if this would not work.
 * @param[in] ent The actor to check
 * @return @c true if the actor is allowed to activate it, @c false otherwise
 */
static qboolean G_ReactionFireCanBeEnabled (const edict_t *ent)
{
	/* check ent is a suitable shooter */
	if (!ent->inuse || !G_IsLivingActor(ent))
		return qfalse;

	if (G_MatchIsRunning() && ent->team != level.activeTeam)
		return qfalse;

	/* actor may not carry weapons at all - so no further checking is needed */
	if (!ent->chr.teamDef->weapons)
		return qfalse;

	if (!G_ActorHasReactionFireEnabledWeapon(ent)) {
		G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("No reaction fire enabled weapon.\n"));
		return qfalse;
	}

	if (!G_ActorHasWorkingFireModeSet(ent)) {
		G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("No fire mode selected for reaction fire.\n"));
		return qfalse;
	}

	if (!G_ActorHasEnoughTUsReactionFire(ent)) {
		G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("Not enough TUs left for activating reaction fire.\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Set the reaction fire TU reservation for an actor
 * @param[in,out] ent The actor edict to set the TUs for
 * @return @c true if TUs for reaction fire were reserved, @c false if the reservation was set
 * back to @c 0
 */
qboolean G_ReactionFireSettingsReserveTUs (edict_t *ent)
{
	if (G_ReactionFireSetDefault(ent) && G_ReactionFireCanBeEnabled(ent)) {
		const int TUs = G_ActorGetTUForReactionFire(ent);
		/* Enable requested reaction fire. */
		G_ActorReserveTUs(ent, TUs, ent->chr.reservedTus.shot, ent->chr.reservedTus.crouch);
		return qtrue;
	}

	G_ActorReserveTUs(ent, 0, ent->chr.reservedTus.shot, ent->chr.reservedTus.crouch);
	return qfalse;
}

/**
 * @brief Check whether ent can reaction fire at target, i.e. that it can see it and neither is dead etc.
 * @param[in] ent The entity that might be firing
 * @param[in] target The entity that might be fired at
 * @return @c true if 'ent' can actually fire at 'target', @c false otherwise
 */
static qboolean G_ReactionFireIsPossible (const edict_t *ent, const edict_t *target)
{
	float actorVis;
	qboolean frustum;

	/* an entity can't reaction fire at itself */
	if (ent == target)
		return qfalse;

	/* Don't react in your own turn */
	if (ent->team == level.activeTeam)
		return qfalse;

	/* ent can't use RF if is in STATE_DAZED (flashbang impact) */
	if (G_IsDazed(ent))
		return qfalse;

	if (G_IsDead(target))
		return qfalse;

	/* check ent has reaction fire enabled */
	if (!G_IsShaken(ent) && !G_IsReaction(ent))
		return qfalse;

	/* check ent has weapon in RF hand */
	/* @todo Should this situation even happen when G_IsReaction(ent) is true? */
	if (!ACTOR_GET_INV(ent, ent->chr.RFmode.hand)) {
		/* print character info if this happens, for now */
		gi.DPrintf("Reaction fire enabled but no weapon for hand (name=%s,hand=%i,fmIdx=%i)\n",
				ent->chr.name, ent->chr.RFmode.hand, ent->chr.RFmode.fmIdx);
		return qfalse;
	}

	if (!G_IsVisibleForTeam(target, ent->team))
		return qfalse;

	/* If reaction fire is triggered by a friendly unit
	 * and the shooter is still sane, don't shoot;
	 * well, if the shooter isn't sane anymore... */
	if (G_IsCivilian(target) || target->team == ent->team)
		if (!G_IsShaken(ent) || (float) ent->morale / mor_shaken->value > frand())
			return qfalse;

	/* check in range and visible */
	if (VectorDistSqr(ent->origin, target->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST)
		return qfalse;

	frustum = G_FrustumVis(ent, target->origin);
	if (!frustum)
		return qfalse;

	actorVis = G_ActorVis(ent->origin, ent, target, qtrue);
	if (actorVis <= 0.2)
		return qfalse;

	/* okay do it then */
	return qtrue;
}

/**
 * @brief Check whether 'target' has just triggered any new reaction fire
 * @param[in] target The entity triggering fire
 */
static void G_ReactionFireTargetsUpdateAll (const edict_t *target)
{
	edict_t *shooter = NULL;

	/* check all possible shooters */
	while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
		/* check whether reaction fire is possible (friend/foe, LoS */
		if (G_ReactionFireIsPossible(shooter, target)) {
			const int TUs = G_ReactionFireGetTUsForItem(shooter, target, RIGHT(shooter));
			if (TUs < 0)
				continue;	/* no suitable weapon */
			G_ReactionFireTargetsAdd(shooter, target, TUs);
		} else {
			G_ReactionFireTargetsRemove(shooter, target);
		}
	}
}

/**
 * @brief Check whether 'target' has just triggered any new reaction fire
 * @param[in] target The entity triggering fire
 * @sa G_CanReactionFire
 * @sa G_GetFiringTUs
 */
static void G_ReactionFireSearchTarget (const edict_t *target)
{
	edict_t *ent = NULL;

	/* check all possible shooters */
	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		int tus;

		/* not if ent has reaction target already */
		if (ent->reactionTarget)
			continue;

		/* check whether reaction fire is possible */
		if (!G_ReactionFireIsPossible(ent, target))
			continue;

		/* see how quickly ent can fire (if it can fire at all) */
		tus = G_ReactionFireGetTUsForItem(ent, target, RIGHT(ent));
		if (tus < 0)
			continue;

		/* queue a reaction fire to take place */
		ent->reactionTarget = target;
		/* An enemy entering the line of fire of a soldier on reaction
		 * fire should have the opportunity to spend time equal to the
		 * sum of these values. */
		ent->reactionTUs = max(0, target->TU - (tus / 4.0));
		ent->reactionNoDraw = qfalse;
	}
}

/**
 * @brief Perform the reaction fire shot
 * @param[in] player The player this action belongs to (the human player or the ai)
 * @param[in] shooter The actor that is trying to shoot
 * @param[in] at Position to fire on.
 * @param[in] type What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon (objDef.fd[][x])  .
 * @return qtrue if everything went ok (i.e. the shot(s) where fired ok), otherwise qfalse.
 * @sa G_ClientShoot
 */
static qboolean G_ReactionFireShoot (const player_t *player, edict_t *shooter, const pos3_t at, shoot_types_t type, fireDefIndex_t firemode)
{
	const int minhit = 30;
	shot_mock_t mock;
	int ff, i;
	/* this is the max amount of friendly units that were hit during the mock calculation */
	int maxff;

	if (G_IsInsane(shooter))
		maxff = 100;
	else if (G_IsRaged(shooter))
		maxff = 60;
	else if (G_IsPaniced(shooter))
		maxff = 30;
	else if (G_IsShaken(shooter))
		maxff = 15;
	else
		maxff = 5;

	/* calculate the mock values - e.g. how many friendly units we would hit
	 * when opening the reaction fire */
	OBJZERO(mock);
	for (i = 0; i < 100; i++)
		if (!G_ClientShoot(player, shooter, at, type, firemode, &mock, qfalse, 0))
			break;

	ff = mock.friendCount + (G_IsAlien(shooter) ? 0 : mock.civilian);
	if (ff <= maxff && mock.enemyCount >= minhit)
		return G_ClientShoot(player, shooter, at, type, firemode, NULL, qfalse, 0);

	return qfalse;
}

/**
 * @brief Resolve the reaction fire for an entity, this checks that the entity can fire and then takes the shot
 * @param[in] shooter The entity to resolve reaction fire for
 * @param[in] target The victim of the reaction fire
 * @return true if the entity fired (or would have fired if mock), false otherwise
 */
static qboolean G_ReactionFireTryToShoot (edict_t *shooter, const edict_t *target)
{
	qboolean tookShot;

	/* check for valid target */
	assert(target);

	/* shooter can't take a reaction shot if it's not possible - and check that
	 * the target is still alive */
	if (!G_ReactionFireIsPossible(shooter, target)) {
		G_ReactionFireTargetsRemove(shooter, target);
		return qfalse;
	}

	/* take the shot */
	tookShot = G_ReactionFireShoot(G_PLAYER_FROM_ENT(shooter), shooter, target->pos, ST_RIGHT_REACTION, shooter->chr.RFmode.fmIdx);

	if (tookShot) {
		/* clear any shakenness */
		G_RemoveShaken(shooter);

		/* check whether further reaction fire is possible */
		if (G_ReactionFireIsPossible(shooter, target)){
			/* see how quickly shooter can fire (if it can fire at all) */
			const int tus = G_ReactionFireGetTUsForItem(shooter, target, RIGHT(shooter));
			if (tus >= 0) {
				/* An enemy getting reaction shot gets more time before
				 * reaction fire is repeated. */
				shooter->reactionTUs = max(0, target->TU - tus);
			}
		}
	}

	return tookShot;
}

/**
 * @brief Check all entities to see whether target has caused reaction fire to resolve.
 * @param[in] target The entity that might be resolving reaction fire
 * @returns whether any entity fired (or would fire) upon target
 * @sa G_ReactionFireOnMovement
 * @sa G_ReactionFirePostShot
 */
static qboolean G_ReactionFireCheckExecution (const edict_t *target)
{
	edict_t *shooter = NULL;
	qboolean fired = qfalse;

	/* check all possible shooters */
	while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
		const int tus = G_ReactionFireGetTUsForItem(shooter, target, RIGHT(shooter));
		if (tus > 1 && g_reactionnew->integer) {
			if (G_ReactionFireTargetsExpired(shooter, target, 0)) {
				shooter->reactionTarget = target;
				if (G_ReactionFireTryToShoot(shooter, target)) {
					G_ReactionFireTargetsAdvance(shooter, target, tus);
					fired |= qtrue;
				}
			}
		} else {
			if (shooter->reactionTarget) {
				const int reactionTargetTU = shooter->reactionTarget->TU;
				const int reactionTU = shooter->reactionTUs;
				const qboolean timeout = reactionTargetTU < reactionTU;
				/* check whether target has changed (i.e. the player is making a move with a
				 * different entity) or whether target is out of time. */
				if (shooter->reactionTarget != target || timeout)
					fired |= G_ReactionFireTryToShoot(shooter, target);
			}
		}
	}
	return fired;
}

/**
 * @brief Called when 'target' moves, possibly triggering or resolving reaction fire
 * @param[in] target The target entity
 * @return true If any shots were (or would be) taken
 * @sa G_ClientMove
 */
qboolean G_ReactionFireOnMovement (edict_t *target)
{
	/* Check to see whether this resolves any reaction fire */
	const qboolean fired = G_ReactionFireCheckExecution(target);

	/* Check to see whether this triggers any reaction fire */
	G_ReactionFireSearchTarget(target);

	G_ReactionFireTargetsUpdateAll(target);

	return fired;
}

/**
 * @brief Called when 'target' is about to shoot, this forces a 'draw' to decide who gets the first shot
 * @param[in] target The entity about to shoot
 * @param[in] fdTime The TU of the shot
 * @sa G_ClientShoot
 */
void G_ReactionFirePreShot (const edict_t *target, const int fdTime)
{
	edict_t *shooter = NULL;
	qboolean repeat = qtrue;

	/* Check to see whether this triggers any reaction fire */
	G_ReactionFireSearchTarget(target);

	G_ReactionFireTargetsUpdateAll(target);

	/* if any reaction fire occurs, we have to loop through all entities again to allow
	 * multiple (fast) RF snap shots before a (slow) aimed shot from the target occurs (only if g_reactionnew is set to 1). */
	while (repeat) {
		repeat = qfalse;
		/* check all ents to see who wins and who loses a draw */
		while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
			int entTUs = G_ReactionFireGetTUsForItem(shooter, target, RIGHT(shooter));
			if (entTUs > 1 && g_reactionnew->integer) {
				if (G_ReactionFireTargetsExpired(shooter, target, fdTime)) {
					shooter->reactionTarget = target;
					if (G_ReactionFireTryToShoot(shooter, target)) {
						repeat = qtrue;
						G_ReactionFireTargetsAdvance(shooter, target, fdTime);
					}
				}
			} else {
				if (!shooter->reactionTarget)
					continue;

				/* check this shooter hasn't already lost the draw */
				if (shooter->reactionNoDraw)
					continue;

				/* can't reaction fire if no TUs to fire */
				if (entTUs < 0) {
					shooter->reactionTarget = NULL;
					continue;
				}

				/* see who won */
				if (entTUs >= fdTime) {
					/* target wins, so delay shooter */
					/* shooter can't lose the TU battle again */
					shooter->reactionNoDraw = qtrue;
				} else {
					/* shooter wins so take the shot */
					G_ReactionFireTryToShoot(shooter, shooter->reactionTarget);
				}
			}
		}
	}
}

/**
 * @brief Called after 'target' has fired, this might trigger more reaction fire or resolve outstanding reaction fire (because target is out of time)
 * @param[in] target The entity that has just fired
 * @sa G_ClientShoot
 */
void G_ReactionFirePostShot (edict_t *target)
{
	/* Check to see whether this resolves any reaction fire */
	G_ReactionFireCheckExecution(target);
}

/**
 * @brief Called at the end of turn, all outstanding reaction fire is resolved
 * @sa G_ClientEndRound
 */
void G_ReactionFireOnEndTurn (void)
{
	edict_t *ent = NULL;

	/* resolve all outstanding reaction firing if possible */
	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (!ent->reactionTarget)
			continue;
		G_ReactionFireTargetsRemove(ent, ent->reactionTarget);
		ent->reactionTarget = NULL;
		/* we explicitly do nothing at end of turn */
		/* G_ReactionFireTryToShoot(ent, ent->reactionTarget); */
	}
}

/**
 * @brief Guess! Reset all "shaken" states on end of turn?
 * @note Normally called on end of turn.
 * @sa G_ClientStateChange
 * @param[in] team Index of team to loop through.
 */
void G_ReactionFireReset (int team)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, team))) {
		/** @todo why do we send the state here and why do we change the "shaken"
		 * state? - see G_MoraleBehaviour */
		G_RemoveShaken(ent);
		ent->reactionTarget = NULL;
		ent->reactionTUs = 0;
		ent->reactionNoDraw = qfalse;

		G_EventActorStateChange(G_TeamToPM(ent->team), ent);
	}
}
