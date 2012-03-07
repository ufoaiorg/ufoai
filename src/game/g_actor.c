/**
 * @file g_actor.c
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
 * @brief Checks whether the given edict is a living actor
 * @param[in] ent The edict to perform the check for
 * @sa LE_IsLivingActor
 */
qboolean G_IsLivingActor (const edict_t *ent)
{
	return G_IsActor(ent) && (G_IsStunned(ent) || !G_IsDead(ent));
}

/**
 * @brief Make the actor use (as in open/close) a door edict
 * @note Will also check whether the door is still reachable (this might have
 * changed due to the rotation) after the usage
 * @param actor The actor that is using the door
 * @param door The door that should be opened/closed
 */
void G_ActorUseDoor (edict_t *actor, edict_t *door)
{
	edict_t *closeActor = NULL;

	if (!G_ClientUseEdict(G_PLAYER_FROM_ENT(actor), actor, door))
		return;

	/* end this loop here, for the AI this is a) not interesting,
	 * and b) could result in endless loops */
	if (G_IsAI(actor))
		return;

	while ((closeActor = G_FindRadius(closeActor, door->origin, UNIT_SIZE * 3, ET_NULL))) {
		/* check whether the door is still reachable (this might have
		 * changed due to the rotation) or whether an actor can reach it now */
		G_TouchTriggers(closeActor);
	}
}

/**
 * @brief Checks whether the given actor is currently standing in a rescue zone
 * @param[in] actor The actor to check
 * @return @c true if the actor is standing in a rescue zone, @c false otherwise.
 */
qboolean G_ActorIsInRescueZone (const edict_t* actor)
{
	return actor->inRescueZone;
}

/**
 * @brief Set the rescue zone data
 * @param[out] actor The actor to set the rescue zone flag for
 * @param[in] inRescueZone @c true if the actor is in the rescue zone, @c false otherwise
 */
void G_ActorSetInRescueZone (edict_t* actor, qboolean inRescueZone)
{
	if (inRescueZone == G_ActorIsInRescueZone(actor))
		return;

	if (inRescueZone)
		G_ClientPrintf(G_PLAYER_FROM_ENT(actor), PRINT_HUD, _("Soldier entered the rescue zone\n"));
	else
		G_ClientPrintf(G_PLAYER_FROM_ENT(actor), PRINT_HUD, _("Soldier left the rescue zone\n"));

	actor->inRescueZone = inRescueZone;
}

/**
 * @brief Handles the client actions (interaction with the world)
 * @param actor The actors' edict
 * @param ent The edict that can be used by the actor
 */
void G_ActorSetClientAction (edict_t *actor, edict_t *ent)
{
	if (actor->clientAction == ent)
		return;

	assert(ent == NULL || (ent->flags & FL_CLIENTACTION));
	actor->clientAction = ent;
	if (ent == NULL) {
		G_EventResetClientAction(actor);
	} else {
		G_EventSetClientAction(actor);
	}
}

/**
 * @brief Calculates the amount of all currently reserved TUs
 * @param ent The actor to calculate the reserved TUs for
 * @return The amount of reserved TUs for the given actor edict
 */
int G_ActorGetReservedTUs (const edict_t *ent)
{
	const chrReservations_t *res = &ent->chr.reservedTus;
	return res->reaction + res->shot + res->crouch;
}

/**
 * @brief Calculates the amount of usable TUs. This is without the reserved TUs.
 * @param[in] ent The actor to calculate the amount of usable TUs for. If @c ent is @c NULL, we
 * return zero here
 * @return The amount of usable TUs for the given actor edict
 */
int G_ActorUsableTUs (const edict_t *ent)
{
	if (!ent)
		return 0;

	return ent->TU - G_ActorGetReservedTUs(ent);
}

/**
 * @brief Calculates the amount of TUs that are needed for the current selected reaction fire mode.
 * @note It's assumed that there is a sane fire mode selected for reaction fire
 * @param[in] ent The actors edict
 * @return The amount of TUs that are needed for the current selected reaction fire mode.
 */
int G_ActorGetTUForReactionFire (const edict_t *ent)
{
	const invList_t *invlistWeapon;
	const chrFiremodeSettings_t *fm = &ent->chr.RFmode;
	const fireDef_t *fd;

	invlistWeapon = ACTOR_GET_INV(ent, fm->hand);
	assert(invlistWeapon);
	assert(invlistWeapon->item.t);

	fd = FIRESH_FiredefForWeapon(&invlistWeapon->item);
	assert(fd);
	return fd[fm->fmIdx].time;
}

/**
 * @brief Reserves TUs for different actor actions
 * @param[in,out] ent The actor to reserve TUs for. Might not be @c NULL.
 * @param[in] resReaction TUs for reaction fire
 * @param[in] resShot TUs for shooting
 * @param[in] resCrouch TUs for going into crouch mode
 */
void G_ActorReserveTUs (edict_t *ent, int resReaction, int resShot, int resCrouch)
{
	if (ent->TU >= resReaction + resShot + resCrouch) {
		ent->chr.reservedTus.reaction = resReaction;
		ent->chr.reservedTus.shot = resShot;
		ent->chr.reservedTus.crouch = resCrouch;
	}

	G_EventActorSendReservations(ent);
}

/**
 * @brief Searches an actor by a unique character number
 * @param[in] ucn The unique character number
 * @param[in] team The team to get the actor with the ucn from
 * @return The actor edict if found, otherwise @c NULL
 */
edict_t *G_ActorGetByUCN (const int ucn, const int team)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextActor(ent)))
		if (team == ent->team && ent->chr.ucn == ucn)
			return ent;

	return NULL;
}

/**
 * @brief Turns an actor around
 * @param[in] ent the actor (edict) we are talking about
 * @param[in] dir the direction to turn the edict into, might be an action
 * @return Bitmask of visible (VIS_*) values
 * @sa G_CheckVisTeamAll
 */
int G_ActorDoTurn (edict_t * ent, byte dir)
{
	float angleDiv;
	const byte *rot;
	int i, num;
	int status;

	assert(ent->dir < CORE_DIRECTIONS);
	assert(dir < PATHFINDING_DIRECTIONS);

	/*
	 * If dir is at least CORE_DIRECTIONS but less than FLYING_DIRECTIONS,
	 * then the direction is an action.
	 */
	/** @todo If performing an action, ensure the actor is facing the direction
	 *  needed to perform the action if needed (climbing ladders).
	 */
	if (dir >= CORE_DIRECTIONS && dir < FLYING_DIRECTIONS)
		return 0;

	/* Clamp dir between 0 and CORE_DIRECTIONS - 1. */
	dir &= (CORE_DIRECTIONS - 1);
	assert(dir < CORE_DIRECTIONS);

	/* Return if no rotation needs to be done. */
	if (ent->dir == dir)
		return 0;

	/* calculate angle difference */
	angleDiv = directionAngles[dir] - directionAngles[ent->dir];
	if (angleDiv > 180.0)
		angleDiv -= 360.0;
	if (angleDiv < -180.0)
		angleDiv += 360.0;

	/* prepare rotation - decide whether the actor turns around the left
	 * shoulder or the right - this is needed the get the rotation vector
	 * that is used below to check in each of the rotation steps
	 * (1/8, 22.5 degree) whether something became visible while turning */
	if (angleDiv > 0) {
		const int angleStep = (360.0 / CORE_DIRECTIONS);
		rot = dvleft;
		num = (angleDiv + angleStep / 2) / angleStep;
	} else {
		const int angleStep = (360.0 / CORE_DIRECTIONS);
		rot = dvright;
		num = (-angleDiv + angleStep / 2) / angleStep;
	}

	/* do rotation and vis checks */
	status = 0;

	/* check every angle (1/8 steps - on the way to the end direction) in the rotation
	 * whether something becomes visible and stop before reaching the final direction
	 * if this happened */
	for (i = 0; i < num; i++) {
		ent->dir = rot[ent->dir];
		assert(ent->dir < CORE_DIRECTIONS);
		status |= G_CheckVisTeamAll(ent->team, qfalse, ent);
	}

	if (status & VIS_STOP) {
		/* send the turn */
		G_EventActorTurn(ent);
	}

	return status;
}

/**
 * @brief Sets correct bounding box for actor (state dependent).
 * @param[in] ent Pointer to entity for which bounding box is being set.
 * @note Also re-links the actor edict - because the server must know about the
 * changed bounding box for tracing to work.
 */
void G_ActorSetMaxs (edict_t* ent)
{
	if (G_IsCrouched(ent))
		VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
	else if (G_IsDead(ent) && !CHRSH_IsTeamDefRobot(ent->chr.teamDef))
		VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD);
	else
		VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);

	/* Link it. */
	gi.LinkEdict(ent);
}

/**
 * @brief Calculates the TU penalty when the given actor is wearing armour
 * @param[in] ent The actor to calculate the TU penalty for
 * @return The amount of TU that should be used as penalty, @c 0 if the actor does not wear any armour
 * @note The armour weight only adds penalty if its weight is big enough.
 */
int G_ActorGetArmourTUPenalty (const edict_t *ent)
{
	const invList_t *invList = ARMOUR(ent);
	const objDef_t *armour;
	int penalty;
	int weightToCausePenalty = 100;
	int actorPower;
	float factorPower;

	if (ARMOUR(ent) == NULL)
		return 0;

	armour = invList->item.t;
	if (armour->weight >= weightToCausePenalty)
		penalty = (armour->weight - weightToCausePenalty - 1) / 10;
	else
		penalty = 0;

	actorPower = ent->chr.score.skills[ABILITY_POWER] * 10 / MAX_SKILL;
	if (actorPower <= 2)
		factorPower = 2.0F;
	else if (actorPower <= 5)
		factorPower = 1.0F;
	else if (actorPower <= 7)
		factorPower = 0.5F;
	else
		factorPower = 0.25F;

	penalty *= factorPower;

	return penalty;
}

int G_ActorCalculateMaxTU (const edict_t *ent)
{
	const int currentMaxHP = (MIN_TU + (ent->chr.score.skills[ABILITY_SPEED]) * 20 / MAX_SKILL) - G_ActorGetArmourTUPenalty(ent);
	return min(currentMaxHP, 255);
}

static inline int G_ActorGetTU (const edict_t *ent)
{
	if (G_IsDazed(ent))
		return 0;

	return G_ActorCalculateMaxTU(ent);
}

/**
 * @brief Set time units for the given edict. Based on speed skills
 * @param ent The actor edict
 */
void G_ActorGiveTimeUnits (edict_t *ent)
{
	const int tus = G_ActorGetTU(ent);
	G_ActorSetTU(ent, tus);
	G_RemoveDazed(ent);
}

void G_ActorSetTU (edict_t *ent, int tus)
{
	ent->TU = max(tus, 0);
}

void G_ActorUseTU (edict_t *ent, int tus)
{
	G_ActorSetTU(ent, ent->TU - tus);
}

static qboolean G_ActorStun (edict_t * ent, const edict_t *attacker)
{
	/* already dead or stunned? */
	if (G_IsDead(ent))
		return qfalse;

	/* no other state should be set here */
	ent->state = STATE_STUN;
	ent->link = attacker;

	G_ActorModifyCounters(attacker, ent, -1, 0, 1);

	return qtrue;
}

void G_ActorModifyCounters (const edict_t *attacker, const edict_t *victim, int deltaAlive, int deltaKills, int deltaStuns)
{
	const int spawned = level.num_spawned[victim->team];
	byte *alive = level.num_alive;

	alive[victim->team] += deltaAlive;
	if (alive[victim->team] > spawned)
		gi.Error("alive counter out of sync");

	if (attacker != NULL) {
		if (deltaStuns != 0) {
			byte *stuns = level.num_stuns[attacker->team];
			stuns[victim->team] += deltaStuns;
			if (stuns[victim->team] > spawned)
				gi.Error("stuns counter out of sync");
		}

		if (deltaKills != 0) {
			byte *kills = level.num_kills[attacker->team];
			kills[victim->team] += deltaKills;
			if (kills[victim->team] > spawned)
				gi.Error("kills counter out of sync");
		}
	}
#if 0
	{
		int i;

		Com_Printf("num_spawned: %i\n", spawned);

		if (attacker)
		for (i = 0; i < MAX_TEAMS; i++) {
			int j;

			if (i == victim->team) {
				Com_Printf("^2num_alive (team %i): %i\n", i, level.num_alive[i]);
			} else {
				Com_Printf("num_alive (team %i): %i\n", i, level.num_alive[i]);
			}

			for (j = 0; j < MAX_TEAMS; j++) {
				if (j == victim->team) {
					Com_Printf("^2num_kills (team %i killed  %i): %i\n", i, j, level.num_kills[i][j]);
					Com_Printf("^2num_stuns (team %i stunned %i): %i\n", i, j, level.num_stuns[i][j]);
				} else if (i == attacker->team) {
					Com_Printf("^3num_kills (team %i killed  %i): %i\n", i, j, level.num_kills[i][j]);
					Com_Printf("^3num_stuns (team %i stunned %i): %i\n", i, j, level.num_stuns[i][j]);
				} else {
					Com_Printf("num_kills (team %i killed  %i): %i\n", i, j, level.num_kills[i][j]);
					Com_Printf("num_stuns (team %i stunned %i): %i\n", i, j, level.num_stuns[i][j]);
				}
			}
		}
	}
#endif
}

/**
 * @brief Fills a vector with the eye position of a given actor
 * @param[in] actor The actor edict to get the eye position in the world from
 * @param[out] eye The eye vector world position
 */
void G_ActorGetEyeVector (const edict_t *actor, vec3_t eye)
{
	VectorCopy(actor->origin, eye);
	if (G_IsCrouched(actor) || G_IsPaniced(actor))
		eye[2] += EYE_CROUCH;
	else
		eye[2] += EYE_STAND;
}

static void G_ActorRevitalise (edict_t *ent)
{
	if (G_IsStunned(ent)) {
		G_RemoveStunned(ent);
		ent->solid = SOLID_BBOX;
		/** @todo have a look at the morale value of
		 * the ent and maybe get into rage or panic? */
		G_ActorModifyCounters(ent->link, ent, 1, 0, -1);
		G_GetFloorItems(ent);
	}
	G_ActorSetMaxs(ent);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent, qtrue);

	/* calc new vis for this player */
	G_CheckVisTeamAll(ent->team, qfalse, ent);
}

void G_ActorCheckRevitalise (edict_t *ent)
{
	if (G_IsStunned(ent) && ent->STUN < ent->HP) {
		/* check that we could move after we stood up */
		edict_t *otherActor = NULL;
		while ((otherActor = G_EdictsGetNextInUse(otherActor))) {
			if (!VectorCompare(ent->pos, otherActor->pos))
				continue;
			if (G_IsBlockingMovementActor(otherActor))
				return;
		}

		G_ActorRevitalise(ent);
		G_EventActorRevitalise(ent);
		G_SendStats(ent);
	}
}

static qboolean G_ActorDie (edict_t * ent, const edict_t *attacker)
{
	const qboolean stunned = G_IsStunned(ent);

	G_RemoveStunned(ent);

	if (G_IsDead(ent))
		return qfalse;

	G_SetState(ent, 1 + rand() % MAX_DEATH);
	G_ActorSetMaxs(ent);

	if (stunned)
		G_ActorModifyCounters(attacker, ent, 0, 1, -1);
	else
		G_ActorModifyCounters(attacker, ent, -1, 1, 0);

	return qtrue;
}

/**
 * @brief Reports and handles death or stun of an actor. If the HP of an actor is zero the actor
 * will die, otherwise the actor will get stunned.
 * @param[in] ent Pointer to an entity being killed or stunned actor.
 * @param[in] attacker Pointer to attacker - it must be notified about state of victim.
 * @todo Discuss whether stunned actor should really drop everything to floor. Maybe
 * it should drop only what he has in hands? Stunned actor can wake later during mission.
 */
qboolean G_ActorDieOrStun (edict_t * ent, edict_t *attacker)
{
	qboolean state;

	if (ent->HP == 0)
		state = G_ActorDie(ent, attacker);
	else
		state = G_ActorStun(ent, attacker);

	/* no state change performed? */
	if (!state) {
		Com_Printf("State wasn't changed\n");
		return qfalse;
	}

	ent->solid = SOLID_NOT;

	/* send death */
	G_EventActorDie(ent);

	/* handle inventory - drop everything but the armour to the floor */
	G_InventoryToFloor(ent);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent, qtrue);

	/* check if the attacker appears/perishes, seen from other teams */
	if (attacker)
		G_CheckVis(attacker, qtrue);

	/* calc new vis for this player */
	G_CheckVisTeamAll(ent->team, qfalse, attacker);

	/* unlink the floor container */
	FLOOR(ent) = NULL;

	return qtrue;
}

/**
 * @brief Moves an item inside an inventory. Floors are handled special.
 * @param[in] ent The pointer to the selected/used edict/soldier.
 * @param[in] from The container (-id) the item should be moved from.
 * @param[in] fItem The item you want to move.
 * @param[in] to The container (-id) the item should be moved to.
 * @param[in] tx x position where you want the item to go in the destination container
 * @param[in] ty y position where you want the item to go in the destination container
 * @param[in] checkaction Set this to qtrue if you want to check for TUs, otherwise qfalse.
 * @sa event PA_INVMOVE
 * @sa AI_ActorThink
 */
qboolean G_ActorInvMove (edict_t *ent, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx,
		int ty, qboolean checkaction)
{
	player_t *player;
	edict_t *floor;
	qboolean newFloor;
	invList_t *ic, *tc;
	item_t item;
	int mask;
	inventory_action_t ia;
	invList_t fItemBackup, tItemBackup;
	int fx, fy;
	int originalTU, reservedTU = 0;

	player = G_PLAYER_FROM_ENT(ent);

	assert(fItem);
	assert(fItem->item.t);

	/* Store the location/item of 'from' BEFORE actually moving items with I_MoveInInventory. */
	fItemBackup = *fItem;

	/* Store the location of 'to' BEFORE actually moving items with I_MoveInInventory
	 * so in case we swap ammo the client can be updated correctly */
	tc = INVSH_SearchInInventory(&ent->chr.i, to, tx, ty);
	if (tc)
		tItemBackup = *tc;
	else
		tItemBackup = *fItem;

	/* Get first used bit in item. */
	INVSH_GetFirstShapePosition(fItem, &fx, &fy);
	fx += fItem->x;
	fy += fItem->y;

	/* Check if action is possible */
	/* TUs are 1 here - but this is only a dummy - the real TU check is done in the inventory functions below */
	if (checkaction && !G_ActionCheckForCurrentTeam(player, ent, 1))
		return qfalse;

	/* "get floor ready" - searching for existing floor-edict */
	floor = G_GetFloorItems(ent);
	if (INV_IsFloorDef(to) && !floor) {
		/* We are moving to the floor, but no existing edict for this floor-tile found -> create new one */
		floor = G_SpawnFloor(ent->pos);
		newFloor = qtrue;
	} else if (INV_IsFloorDef(from) && !floor) {
		/* We are moving from the floor, but no existing edict for this floor-tile found -> this should never be the case. */
		gi.DPrintf("G_ClientInvMove: No source-floor found.\n");
		return qfalse;
	} else {
		/* There already exists an edict for this floor-tile. */
		newFloor = qfalse;
	}

	/* search for space */
	if (tx == NONE) {
		ic = INVSH_SearchInInventory(&ent->chr.i, from, fItem->x, fItem->y);
		if (ic)
			INVSH_FindSpace(&ent->chr.i, &ic->item, to, &tx, &ty, fItem);
		if (tx == NONE)
			return qfalse;
	}

	/** @todo what if we don't have enough TUs after subtracting the reserved ones? */
	/* Because I_MoveInInventory don't know anything about character_t and it updates ent->TU,
	 * we need to save original ent->TU for the sake of checking TU reservations. */
	originalTU = ent->TU;
	reservedTU = G_ActorGetReservedTUs(ent);
	/* Temporary decrease ent->TU to make I_MoveInInventory do what expected. */
	G_ActorUseTU(ent, reservedTU);
	/* Try to actually move the item and check the return value after restoring valid ent->TU. */
	ia = game.i.MoveInInventory(&game.i, &ent->chr.i, from, fItem, to, tx, ty, checkaction ? &ent->TU : NULL, &ic);
	/* Now restore the original ent->TU and decrease it for TU used for inventory move. */
	G_ActorSetTU(ent, originalTU - (originalTU - reservedTU - ent->TU));

	switch (ia) {
	case IA_NONE:
		/* No action possible - abort */
		return qfalse;
	case IA_NOTIME:
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not enough TUs!\n"));
		return qfalse;
	case IA_NORELOAD:
		G_ClientPrintf(player, PRINT_HUD,
				_("Can't perform action - weapon already fully loaded with the same ammunition!\n"));
		return qfalse;
	default:
		/* Continue below. */
		break;
	}

	/* successful inventory change; remove the item in clients */
	if (INV_IsFloorDef(from)) {
		/* We removed an item from the floor - check how the client
		 * needs to be updated. */
		assert(!newFloor);
		if (FLOOR(ent)) {
			/* There is still something on the floor. */
			FLOOR(floor) = FLOOR(ent);
			/* Delay this if swapping ammo, otherwise the le will be removed in the client before we can add back
			 * the current ammo because removeNextFrame is set in LE_PlaceItem() if the floor le has no items */
			if (ia != IA_RELOAD_SWAP)
				G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		} else {
			/* Floor is empty, remove the edict (from server + client) if we are
			 * not moving to it. */
			if (!INV_IsFloorDef(to)) {
				G_EventPerish(floor);
				G_FreeEdict(floor);
			} else
				G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		}
	} else {
		G_EventInventoryDelete(ent, G_TeamToPM(ent->team), from, fx, fy);
	}

	/* send tu's */
	G_SendStats(ent);

	assert(ic);
	item = ic->item;

	if (ia == IA_RELOAD || ia == IA_RELOAD_SWAP) {
		/* reload */
		if (INV_IsFloorDef(to))
			mask = G_VisToPM(floor->visflags);
		else
			mask = G_TeamToPM(ent->team);

		G_EventInventoryReload(INV_IsFloorDef(to) ? floor : ent, mask, &item, to, ic);

		if (ia == IA_RELOAD) {
			return qtrue;
		} else { /* ia == IA_RELOAD_SWAP */
			item.a = NONE_AMMO;
			item.m = NULL;
			item.t = tItemBackup.item.m;
			item.rotated = fItemBackup.item.rotated;
			item.amount = tItemBackup.item.amount;
			to = from;
			if (INV_IsFloorDef(to)) {
				/* I_MoveInInventory placed the swapped ammo in an available space, check where it was placed
				 * so we can place it at the same place in the client, otherwise since fItem hasn't been removed
				 * this could end in a different place in the client - will cause an error if trying to use it again */
				ic = INVSH_FindInInventory(&ent->chr.i, to, &item);
				assert(ic);
				fItemBackup.item = item;
				fItemBackup.x = ic->x;
				fItemBackup.y = ic->y;
			}
			tx = fItemBackup.x;
			ty = fItemBackup.y;
		}
	}

	/* We moved an item to the floor - check how the client needs to be updated. */
	if (INV_IsFloorDef(to)) {
		/* we have to link the temp floor container to the new floor edict or add
		 * the item to an already existing floor edict - the floor container that
		 * is already linked might be from a different entity (this might happen
		 * in case of a throw by another actor) */
		FLOOR(floor) = FLOOR(ent);

		/* A new container was created for the floor. */
		if (newFloor) {
			/* Send item info to the clients */
			G_CheckVis(floor, qtrue);
		} else {
			/* use the backup item to use the old amount values, because the clients have to use the same actions
			 * on the original amount. Otherwise they would end in a different amount of items as the server (+1) */
			G_EventInventoryAdd(floor, G_VisToPM(floor->visflags), 1);
			G_WriteItem(&fItemBackup.item, to, tx, ty);
			gi.EndEvents();
			/* Couldn't remove it before because that would remove the le from the client and would cause battlescape to crash
			 * when trying to add back the swapped ammo above */
			if (ia == IA_RELOAD_SWAP)
				G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		}
	} else {
		G_EventInventoryAdd(ent, G_TeamToPM(ent->team), 1);
		G_WriteItem(&item, to, tx, ty);
		gi.EndEvents();
	}

	G_ReactionFireUpdate(ent, ent->chr.RFmode.fmIdx, ent->chr.RFmode.hand, ent->chr.RFmode.weapon);

	/* Other players receive weapon info only. */
	mask = G_VisToPM(ent->visflags) & ~G_TeamToPM(ent->team);
	if (mask) {
		if (INV_IsRightDef(from) || INV_IsLeftDef(from)) {
			G_EventInventoryDelete(ent, mask, from, fx, fy);
		}
		if (INV_IsRightDef(to) || INV_IsLeftDef(to)) {
			G_EventInventoryAdd(ent, mask, 1);
			G_WriteItem(&item, to, tx, ty);
			gi.EndEvents();
		}
	}

	return qtrue;
}

/**
 * @brief Reload weapon with actor.
 * @param[in] ent Pointer to an actor reloading weapon.
 * @param[in] invDef Reloading weapon in right or left hand.
 * @sa AI_ActorThink
 */
void G_ActorReload (edict_t* ent, const invDef_t *invDef)
{
	invList_t *ic;
	invList_t *icFinal;
	const objDef_t *weapon;
	int tu;
	containerIndex_t containerID;
	const invDef_t *bestContainer;

	/* search for clips and select the one that is available easily */
	icFinal = NULL;
	tu = 100;
	bestContainer = NULL;

	if (CONTAINER(ent, invDef->id)) {
		weapon = CONTAINER(ent, invDef->id)->item.t;
	} else if (INV_IsLeftDef(invDef) && RIGHT(ent)->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		invDef = INVDEF(gi.csi->idRight);
		weapon = CONTAINER(ent, invDef->id)->item.t;
	} else
		return;

	assert(weapon);

	/* LordHavoc: Check if item is researched when in singleplayer? I don't think this is really a
	 * cheat issue as in singleplayer there is no way to inject fake client commands in the virtual
	 * network buffer, and in multiplayer everything is researched */

	/* also try the temp containers */
	for (containerID = 0; containerID < gi.csi->numIDs; containerID++) {
		if (INVDEF(containerID)->out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = CONTAINER(ent, containerID); ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)) {
					icFinal = ic;
					bestContainer = INVDEF(containerID);
					tu = bestContainer->out;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer)
		G_ActorInvMove(ent, bestContainer, icFinal, invDef, 0, 0, qtrue);
}
