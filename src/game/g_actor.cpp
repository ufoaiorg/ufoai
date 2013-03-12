/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_inventory.h"
#include "g_reaction.h"
#include "g_spawn.h"
#include "g_utils.h"
#include "g_vis.h"

/* however we manipulate TUs, this value must never be exceeded */
#define MAX_TU				(ROUTING_NOT_REACHABLE - 1)

/**
 * @brief Checks whether the given edict is a living actor
 * @param[in] ent The edict to perform the check for
 * @sa LE_IsLivingActor
 */
bool G_IsLivingActor (const Edict *ent)
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
void G_ActorUseDoor (Edict *actor, Edict *door)
{
	/* check whether it's part of an edict group but not the master */
	if (door->flags & FL_GROUPSLAVE)
		door = door->groupMaster;

	if (!G_ClientUseEdict(actor->getPlayer(), actor, door))
		return;

	/* end this loop here, for the AI this is a) not interesting,
	 * and b) could result in endless loops */
	if (G_IsAI(actor))
		return;

	Edict *closeActor = NULL;
	while ((closeActor = G_FindRadius(closeActor, door->origin, UNIT_SIZE * 3))) {
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
bool G_ActorIsInRescueZone (const Edict *actor)
{
	return actor->inRescueZone;
}

/**
 * @brief Set the rescue zone data
 * @param[out] actor The actor to set the rescue zone flag for
 * @param[in] inRescueZone @c true if the actor is in the rescue zone, @c false otherwise
 */
void G_ActorSetInRescueZone (Edict *actor, bool inRescueZone)
{
	if (inRescueZone == G_ActorIsInRescueZone(actor))
		return;

	if (inRescueZone)
		G_ClientPrintf(actor->getPlayer(), PRINT_HUD, _("Soldier entered the rescue zone."));
	else
		G_ClientPrintf(actor->getPlayer(), PRINT_HUD, _("Soldier left the rescue zone."));

	actor->inRescueZone = inRescueZone;
}

/**
 * @brief Handles the client actions (interaction with the world)
 * @param actor The actors' edict
 * @param ent The edict that can be used by the actor
 */
void G_ActorSetClientAction (Edict *actor, Edict *ent)
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
int G_ActorGetReservedTUs (const Edict *ent)
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
int G_ActorUsableTUs (const Edict *ent)
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
int G_ActorGetTUForReactionFire (const Edict *ent)
{
	const FiremodeSettings *fm = &ent->chr.RFmode;

	const invList_t *invlistWeapon = ent->getHand(fm->getHand());
	assert(invlistWeapon);
	assert(invlistWeapon->item.def());

	const fireDef_t *fd = FIRESH_FiredefForWeapon(&invlistWeapon->item);
	assert(fd);
	return G_ActorGetTimeForFiredef(ent, &fd[fm->getFmIdx()], true);
}

/**
 * @brief Reserves TUs for different actor actions
 * @param[in,out] ent The actor to reserve TUs for. Might not be @c NULL.
 * @param[in] resReaction TUs for reaction fire
 * @param[in] resShot TUs for shooting
 * @param[in] resCrouch TUs for going into crouch mode
 */
void G_ActorReserveTUs (Edict *ent, int resReaction, int resShot, int resCrouch)
{
	if (ent->TU >= resReaction + resShot + resCrouch) {
		ent->chr.reservedTus.reaction = resReaction;
		ent->chr.reservedTus.shot = resShot;
		ent->chr.reservedTus.crouch = resCrouch;
	}

	G_EventActorSendReservations(ent);
}

/**
 * @brief Turns an actor around
 * @param[in] ent the actor (edict) we are talking about
 * @param[in] dir the direction to turn the edict into, might be an action
 * @return Bitmask of visible (VIS_*) values
 * @sa G_CheckVisTeamAll
 */
int G_ActorDoTurn (Edict *ent, byte dir)
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
		status |= G_CheckVisTeamAll(ent->team, 0, ent);
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
void G_ActorSetMaxs (Edict *ent)
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
int G_ActorCalculateMaxTU (const Edict *ent)
{
	const int invWeight = ent->chr.inv.getWeight();
	const int currentMaxTU = GET_TU(ent->chr.score.skills[ABILITY_SPEED], GET_ENCUMBRANCE_PENALTY(invWeight,
			ent->chr.score.skills[ABILITY_POWER])) * G_ActorGetInjuryPenalty(ent, MODIFIER_TU);

	return std::min(currentMaxTU, MAX_TU);
}

/**
 * @brief Set time units for the given edict. Based on speed skills
 * @param ent The actor edict
 */
void G_ActorGiveTimeUnits (Edict *ent)
{
	const int tus = G_IsDazed(ent) ? 0 : G_ActorCalculateMaxTU(ent);
	G_ActorSetTU(ent, tus);
	G_RemoveDazed(ent);
}

void G_ActorSetTU (Edict *ent, int tus)
{
	if (tus > 0 && tus < ent->TU) {
		if (g_notu != NULL && g_notu->integer) {
			ent->TU = G_ActorCalculateMaxTU(ent);
			return;
		}
	}
	ent->TU = std::max(tus, 0);
}

void G_ActorUseTU (Edict *ent, int tus)
{
	G_ActorSetTU(ent, ent->TU - tus);
}

static bool G_ActorStun (Edict *ent, const Edict *attacker)
{
	/* already dead or stunned? */
	if (G_IsDead(ent))
		return false;

	/* no other state should be set here */
	ent->state = STATE_STUN;
	G_ActorSetMaxs(ent);
	ent->link = attacker;

	G_ActorModifyCounters(attacker, ent, -1, 0, 1);

	return true;
}

void G_ActorModifyCounters (const Edict *attacker, const Edict *victim, int deltaAlive, int deltaKills, int deltaStuns)
{
	const int spawned = level.num_spawned[victim->team];
	const int attackerTeam = (attacker != NULL ? attacker->team : MAX_TEAMS);
	byte *alive = level.num_alive;

	alive[victim->team] += deltaAlive;
	if (alive[victim->team] > spawned)
		gi.Error("alive counter out of sync");

	if (deltaStuns != 0) {
		byte *stuns = level.num_stuns[attackerTeam];
		stuns[victim->team] += deltaStuns;
		if (stuns[victim->team] > spawned)
			gi.Error("stuns counter out of sync");
	}

	if (deltaKills != 0) {
		byte *kills = level.num_kills[attackerTeam];
		kills[victim->team] += deltaKills;
		if (kills[victim->team] > spawned)
			gi.Error("kills counter out of sync");
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
void G_ActorGetEyeVector (const Edict *actor, vec3_t eye)
{
	VectorCopy(actor->origin, eye);
	if (G_IsCrouched(actor) || G_IsPanicked(actor))
		eye[2] += EYE_CROUCH;
	else
		eye[2] += EYE_STAND;
}

static void G_ActorRevitalise (Edict *ent)
{
	if (G_IsStunned(ent)) {
		G_RemoveStunned(ent);
		/** @todo have a look at the morale value of
		 * the ent and maybe get into rage or panic? */
		G_ActorModifyCounters(ent->link, ent, 1, 0, -1);
		G_GetFloorItems(ent);
	}
	G_ActorSetMaxs(ent);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent);

	/* calc new vis for this player */
	G_CheckVisTeamAll(ent->team, 0, ent);

	G_PrintStats("%s is revitalized.", ent->chr.name);
}

void G_ActorCheckRevitalise (Edict *ent)
{
	if (G_IsStunned(ent) && ent->STUN < ent->HP) {
		/* check that we could move after we stood up */
		Edict *otherActor = NULL;
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

static bool G_ActorDie (Edict *ent, const Edict *attacker)
{
	const bool stunned = G_IsStunned(ent);

	G_RemoveStunned(ent);

	if (G_IsDead(ent))
		return false;

	G_SetState(ent, 1 + rand() % MAX_DEATH);
	G_ActorSetMaxs(ent);

	if (stunned) {
		G_ActorModifyCounters(attacker, ent, 0, 1, 0);
		G_ActorModifyCounters(ent->link, ent, 0, 0, -1);
	} else {
		G_ActorModifyCounters(attacker, ent, -1, 1, 0);
	}

	return true;
}

/**
 * @brief Reports and handles death or stun of an actor. If the HP of an actor is zero the actor
 * will die, otherwise the actor will get stunned.
 * @param[in] ent Pointer to an entity being killed or stunned actor.
 * @param[in] attacker Pointer to attacker - it must be notified about state of victim.
 * @todo Discuss whether stunned actor should really drop everything to floor. Maybe
 * it should drop only what he has in hands? Stunned actor can wake later during mission.
 */
bool G_ActorDieOrStun (Edict *ent, Edict *attacker)
{
	bool state;

	if (ent->HP == 0)
		state = G_ActorDie(ent, attacker);
	else
		state = G_ActorStun(ent, attacker);

	/* no state change performed? */
	if (!state) {
		gi.DPrintf("G_ActorDieOrStun: State wasn't changed\n");
		return false;
	}

	if (!G_IsStunned(ent))
		ent->solid = SOLID_NOT;

	/* send death */
	G_EventActorDie(ent, attacker != NULL);

	/* handle inventory - drop everything but the armour to the floor */
	G_InventoryToFloor(ent);
	G_ClientStateChange(ent->getPlayer(), ent, ~STATE_REACTION, false);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent);

	/* check if the attacker appears/perishes, seen from other teams */
	if (attacker)
		G_CheckVis(attacker);

	/* calc new vis for this player */
	G_CheckVisTeamAll(ent->team, 0, attacker);

	/* unlink the floor container */
	ent->resetFloor();

	return true;
}

/**
 * @brief Get the content flags from where the actor is currently standing
 */
int G_ActorGetContentFlags (const vec3_t origin)
{
	vec3_t pointTrace;

	VectorCopy(origin, pointTrace);
	pointTrace[2] += PLAYER_MIN;

	return gi.PointContents(pointTrace);
}

/**
 * @brief Moves an item inside an inventory. Floors are handled special.
 * @param[in] ent The pointer to the selected/used edict/soldier.
 * @param[in] from The container (-id) the item should be moved from.
 * @param[in] fItem The item you want to move.
 * @param[in] to The container (-id) the item should be moved to.
 * @param[in] tx x position where you want the item to go in the destination container
 * @param[in] ty y position where you want the item to go in the destination container
 * @param[in] checkaction Set this to true if you want to check for TUs, otherwise false.
 * @sa event PA_INVMOVE
 * @sa AI_ActorThink
 */
bool G_ActorInvMove (Edict *ent, const invDef_t *from, invList_t *fItem, const invDef_t *to, int tx,
		int ty, bool checkaction)
{
	Edict *floor;
	bool newFloor;
	invList_t *ic, *tc;
	item_t item;
	playermask_t mask;
	inventory_action_t ia;
	invList_t fItemBackup, tItemBackup;
	int fx, fy;
	int originalTU, reservedTU = 0;
	Player &player = ent->getPlayer();

	assert(fItem);
	assert(fItem->item.def());

	/* Store the location/item of 'from' BEFORE actually moving items with moveInInventory. */
	fItemBackup = *fItem;

	/* Store the location of 'to' BEFORE actually moving items with moveInInventory
	 * so in case we swap ammo the client can be updated correctly */
	tc = ent->chr.inv.getItemAtPos(to, tx, ty);
	if (tc)
		tItemBackup = *tc;
	else
		tItemBackup = *fItem;

	/* Get first used bit in item. */
	fItem->getFirstShapePosition(&fx, &fy);
	fx += fItem->getX();
	fy += fItem->y;

	/* Check if action is possible */
	/* TUs are 1 here - but this is only a dummy - the real TU check is done in the inventory functions below */
	if (checkaction && !G_ActionCheckForCurrentTeam(&player, ent, 1))
		return false;

	if (!ent->chr.inv.canHoldItemWeight(from->id, to->id, fItem->item, ent->chr.score.skills[ABILITY_POWER])) {
		G_ClientPrintf(player, PRINT_HUD, _("This soldier can not carry anything else."));
		return false;
	}

	/* "get floor ready" - searching for existing floor-edict */
	floor = G_GetFloorItems(ent);
	if (to->isFloorDef() && !floor) {
		/* We are moving to the floor, but no existing edict for this floor-tile found -> create new one */
		floor = G_SpawnFloor(ent->pos);
		newFloor = true;
	} else if (from->isFloorDef() && !floor) {
		/* We are moving from the floor, but no existing edict for this floor-tile found -> this should never be the case. */
		gi.DPrintf("G_ClientInvMove: No source-floor found.\n");
		return false;
	} else {
		/* There already exists an edict for this floor-tile. */
		newFloor = false;
	}

	/* search for space */
	if (tx == NONE) {
		ic = ent->chr.inv.getItemAtPos(from, fItem->getX(), fItem->y);
		if (ic)
			ent->chr.inv.findSpace(to, &ic->item, &tx, &ty, fItem);
		if (tx == NONE)
			return false;
	}

	/** @todo what if we don't have enough TUs after subtracting the reserved ones? */
	/* Because moveInInventory don't know anything about character_t and it updates ent->TU,
	 * we need to save original ent->TU for the sake of checking TU reservations. */
	originalTU = ent->TU;
	reservedTU = G_ActorGetReservedTUs(ent);
	/* Temporary decrease ent->TU to make moveInInventory do what expected. */
	G_ActorUseTU(ent, reservedTU);
	/* Try to actually move the item and check the return value after restoring valid ent->TU. */
	ia = game.i.moveInInventory(&ent->chr.inv, from, fItem, to, tx, ty, checkaction ? &ent->TU : NULL, &ic);
	/* Now restore the original ent->TU and decrease it for TU used for inventory move. */
	G_ActorSetTU(ent, originalTU - (originalTU - reservedTU - ent->TU));

	switch (ia) {
	case IA_NONE:
		/* No action possible - abort */
		return false;
	case IA_NOTIME:
		G_ClientPrintf(player, PRINT_HUD, _("Can't perform action - not enough TUs!"));
		return false;
	case IA_NORELOAD:
		G_ClientPrintf(player, PRINT_HUD,
				_("Can't perform action - weapon already fully loaded with the same ammunition!"));
		return false;
	default:
		/* Continue below. */
		break;
	}

	/* successful inventory change; remove the item in clients */
	if (from->isFloorDef()) {
		/* We removed an item from the floor - check how the client
		 * needs to be updated. */
		assert(!newFloor);
		if (ent->getFloor()) {
			/* There is still something on the floor. */
			floor->setFloor(ent);
			/* Delay this if swapping ammo, otherwise the le will be removed in the client before we can add back
			 * the current ammo because removeNextFrame is set in LE_PlaceItem() if the floor le has no items */
			if (ia != IA_RELOAD_SWAP)
				G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		} else {
			/* Floor is empty, remove the edict (from server + client) if we are
			 * not moving to it. */
			if (!to->isFloorDef()) {
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
		if (to->isFloorDef())
			mask = G_VisToPM(floor->visflags);
		else
			mask = G_TeamToPM(ent->team);

		G_EventInventoryReload(to->isFloorDef() ? floor : ent, mask, &item, to, ic);

		if (ia == IA_RELOAD) {
			return true;
		} else { /* ia == IA_RELOAD_SWAP */
			item.ammoLeft = NONE_AMMO;
			item.ammo = NULL;
			item.setDef(tItemBackup.item.ammo);
			item.rotated = fItemBackup.item.rotated;
			item.amount = tItemBackup.item.amount;
			to = from;
			if (to->isFloorDef()) {
				/* moveInInventory placed the swapped ammo in an available space, check where it was placed
				 * so we can place it at the same place in the client, otherwise since fItem hasn't been removed
				 * this could end in a different place in the client - will cause an error if trying to use it again */
				ic = ent->chr.inv.findInContainer(to, &item);
				assert(ic);
				fItemBackup.item = item;
				fItemBackup.x = ic->getX();
				fItemBackup.y = ic->y;
			}
			tx = fItemBackup.getX();
			ty = fItemBackup.y;
		}
	}

	/* We moved an item to the floor - check how the client needs to be updated. */
	if (to->isFloorDef()) {
		/* we have to link the temp floor container to the new floor edict or add
		 * the item to an already existing floor edict - the floor container that
		 * is already linked might be from a different entity (this might happen
		 * in case of a throw by another actor) */
		floor->setFloor(ent);

		/* A new container was created for the floor. */
		if (newFloor) {
			/* Send item info to the clients */
			G_CheckVis(floor);
		} else {
			/* use the backup item to use the old amount values, because the clients have to use the same actions
			 * on the original amount. Otherwise they would end in a different amount of items as the server (+1) */
			G_EventInventoryAdd(floor, G_VisToPM(floor->visflags), 1);
			G_WriteItem(&fItemBackup.item, to, tx, ty);
			G_EventEnd();
			/* Couldn't remove it before because that would remove the le from the client and would cause battlescape to crash
			 * when trying to add back the swapped ammo above */
			if (ia == IA_RELOAD_SWAP)
				G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		}
	} else {
		G_EventInventoryAdd(ent, G_TeamToPM(ent->team), 1);
		G_WriteItem(&item, to, tx, ty);
		G_EventEnd();
	}

	G_ReactionFireSettingsUpdate(ent, ent->chr.RFmode.getFmIdx(), ent->chr.RFmode.getHand(), ent->chr.RFmode.getWeapon());

	/* Other players receive weapon info only. */
	mask = G_VisToPM(ent->visflags) & ~G_TeamToPM(ent->team);
	if (mask) {
		if (from->isRightDef() || from->isLeftDef()) {
			G_EventInventoryDelete(ent, mask, from, fx, fy);
		}
		if (to->isRightDef() || to->isLeftDef()) {
			G_EventInventoryAdd(ent, mask, 1);
			G_WriteItem(&item, to, tx, ty);
			G_EventEnd();
		}
	}

	return true;
}

/**
 * @brief Reload weapon with actor.
 * @param[in] ent Pointer to an actor reloading weapon.
 * @param[in] invDef Reloading weapon in right or left hand.
 * @sa AI_ActorThink
 */
void G_ActorReload (Edict *ent, const invDef_t *invDef)
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

	if (ent->getContainer(invDef->id)) {
		weapon = ent->getContainer(invDef->id)->item.def();
	} else if (invDef->isLeftDef() && ent->getRightHand()->item.isHeldTwoHanded()) {
		/* Check for two-handed weapon */
		invDef = INVDEF(gi.csi->idRight);
		weapon = ent->getRightHand()->item.def();
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
			for (ic = ent->getContainer(containerID); ic; ic = ic->next)
				if (ic->item.def()->isLoadableInWeapon(weapon)) {
					icFinal = ic;
					bestContainer = INVDEF(containerID);
					tu = bestContainer->out;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer)
		G_ActorInvMove(ent, bestContainer, icFinal, invDef, 0, 0, true);
}

int G_ActorGetTimeForFiredef (const Edict *const ent, const fireDef_t *const fd, const bool reaction)
{
	return fd->time * G_ActorGetInjuryPenalty(ent, reaction ? MODIFIER_REACTION : MODIFIER_SHOOTING);
}
