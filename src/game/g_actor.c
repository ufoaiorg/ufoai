/**
 * @file g_actor.c
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
 * @brief Checks whether the given edict is a living actor
 * @param[in] ent The edict to perform the check for
 * @sa LE_IsLivingActor
 */
qboolean G_IsLivingActor (const edict_t *ent)
{
	return G_IsActor(ent) && !G_IsDead(ent);
}

/**
 * @brief Turns an actor around
 * @param[in] ent the actor (edict) we are talking about
 * @param[in] dir the direction to turn the edict into, might be an action
 * @return Bitmask of visible (VIS_*) values
 * @sa G_CheckVisTeam
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

	/* prepare rotation */
	if (angleDiv > 0) {
		rot = dvleft;
		num = (angleDiv + 22.5) / 45.0;
	} else {
		rot = dvright;
		num = (-angleDiv + 22.5) / 45.0;
	}

	/* do rotation and vis checks */
	status = 0;

	/* check every angle in the rotation whether something becomes visible */
	for (i = 0; i < num; i++) {
		ent->dir = rot[ent->dir];
		assert(ent->dir < CORE_DIRECTIONS);
		status |= G_CheckVisTeam(ent->team, NULL, qfalse, ent);
	}

	if (status & VIS_STOP) {
		/* send the turn */
		G_EventActorTurn(ent);
	}

	return status;
}

/**
 * @brief Report and handle death of an actor
 */
void G_ActorDie (edict_t * ent, int state, edict_t *attacker)
{
	assert(ent);

	Com_DPrintf(DEBUG_GAME, "G_ActorDie: kill actor on team %i\n", ent->team);
	switch (state) {
	case STATE_DEAD:
		ent->state |= (1 + rand() % MAX_DEATH);
		break;
	case STATE_STUN:
		/**< @todo Is there a reason this is reset? We _may_ need that in the future somehow.
		 * @sa CL_ActorDie */
		ent->STUN = 0;
		ent->state = state;
		break;
	default:
		Com_DPrintf(DEBUG_GAME, "G_ActorDie: unknown state %i\n", state);
		break;
	}
	VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD);
	gi.LinkEdict(ent);
	level.num_alive[ent->team]--;
	/* send death */
	G_EventActorDie(ent, attacker);

	/* handle inventory - drop everything to floor edict (but not the armour) */
	G_InventoryToFloor(ent);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent, qtrue);

	/* check if the attacker appears/perishes, seen from other teams */
	if (attacker)
		G_CheckVis(attacker, qtrue);

	/* calc new vis for this player */
	G_CheckVisTeam(ent->team, NULL, qfalse, attacker);
}

/**
 * @brief Moves an item inside an inventory. Floors are handled special.
 * @param[in] entNum The edict number of the selected/used edict/soldier.
 * @param[in] from The container (-id) the item should be moved from.
 * @param[in] fItem The item you want to move.
 * @param[in] to The container (-id) the item should be moved to.
 * @param[in] tx x position where you want the item to go in the destination container
 * @param[in] ty y position where you want the item to go in the destination container
 * @param[in] checkaction Set this to qtrue if you want to check for TUs, otherwise qfalse.
 * @param[in] quiet Set this to qfalse to prevent message-flooding.
 * @sa event PA_INVMOVE
 * @sa AI_ActorThink
 */
void G_ActorInvMove (int entNum, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx,
		int ty, qboolean checkaction, qboolean quiet)
{
	player_t *player;
	edict_t *ent, *floor;
	qboolean newFloor;
	invList_t *ic;
	item_t item;
	int mask;
	inventory_action_t ia;
	int msglevel;
	invList_t fItemBackup;
	const invList_t* fItemBackupPtr;
	int fx, fy;

	ent = g_edicts + entNum;
	player = G_PLAYER_FROM_ENT(ent);
	msglevel = quiet ? PRINT_NONE : PRINT_HUD;

	assert(fItem);
	assert(fItem->item.t);

	/* Store the location/item of 'from' BEFORE actually moving items with Com_MoveInInventory. */
	fItemBackup = *fItem;
	fItemBackupPtr = fItem;

	/* Get first used bit in item. */
	Com_GetFirstShapePosition(fItem, &fx, &fy);
	fx += fItem->x;
	fy += fItem->y;

	/* Check if action is possible */
	/* TUs are 1 here - but this is only a dummy - the real TU check is done in the inventory functions below */
	if (checkaction && !G_ActionCheck(player, ent, 1, quiet))
		return;

	/* "get floor ready" - searching for existing floor-edict */
	floor = G_GetFloorItems(ent);
	if (INV_IsFloorDef(to) && !floor) {
		/* We are moving to the floor, but no existing edict for this floor-tile found -> create new one */
		floor = G_SpawnFloor(ent->pos);
		newFloor = qtrue;
	} else if (INV_IsFloorDef(from) && !floor) {
		/* We are moving from the floor, but no existing edict for this floor-tile found -> this should never be the case. */
		gi.dprintf("G_ClientInvMove: No source-floor found.\n");
		return;
	} else {
		/* There already exists an edict for this floor-tile. */
		newFloor = qfalse;
	}

	/* search for space */
	if (tx == NONE) {
		if (ty != NONE)
			gi.dprintf("G_ClientInvMove: Error: ty != NONE, it is %i.\n", ty);

		ic = Com_SearchInInventory(&ent->i, from, fItem->x, fItem->y);
		if (ic)
			Com_FindSpace(&ent->i, &ic->item, to, &tx, &ty, fItem);
	}
	if (tx == NONE) {
		if (ty != NONE)
			gi.error("G_ClientInvMove: Error: ty != NONE, it is %i.\n", ty);
		return;
	}

	/* Try to actually move the item and check the return value */
	ia = Com_MoveInInventory(&ent->i, from, fItem, to, tx, ty, checkaction ? &ent->TU : NULL, &ic);
	switch (ia) {
	case IA_NONE:
		/* No action possible - abort */
		return;
	case IA_NOTIME:
		G_PlayerPrintf(player, msglevel, _("Can't perform action - not enough TUs!\n"));
		return;
	case IA_NORELOAD:
		G_PlayerPrintf(player, msglevel,
				_("Can't perform action - weapon already fully loaded with the same ammunition!\n"));
		return;
	default:
		/* Continue below. */
		assert(ic);
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
			G_EventInventoryDelete(floor, G_VisToPM(floor->visflags), from, fx, fy);
		} else {
			/* Floor is empty, remove the edict (from server + client) if we are
			 * not moving to it. */
			if (!INV_IsFloorDef(to)) {
				G_EventPerish(floor);
				G_FreeEdict(floor);
			}
		}
	} else {
		G_EventInventoryDelete(ent, G_TeamToPM(ent->team), from, fx, fy);
	}

	/* send tu's */
	G_SendStats(ent);

	item = ic->item;

	if (ia == IA_RELOAD || ia == IA_RELOAD_SWAP) {
		/* reload */
		if (INV_IsFloorDef(to)) {
			assert(!newFloor);
			assert(FLOOR(floor) == FLOOR(ent));
			mask = G_VisToPM(floor->visflags);
		} else {
			mask = G_TeamToPM(ent->team);
		}

		G_EventInventoryReload(INV_IsFloorDef(to) ? floor : ent, mask, &item, to, ic);

		if (ia == IA_RELOAD) {
			gi.EndEvents();
			return;
		} else { /* ia == IA_RELOAD_SWAP */
			if (!fItemBackupPtr) {
				gi.dprintf("G_ClientInvMove: Didn't find invList of the item you're moving\n");
				gi.EndEvents();
				return;
			}
			item = fItemBackup.item;
			to = from;
			tx = fItemBackup.x;
			ty = fItemBackup.y;
		}
	}

	/* add it */
	if (INV_IsFloorDef(to)) {
		/* We moved an item to the floor - check how the client needs to be updated. */
		if (newFloor) {
			/* A new container was created for the floor. */
			assert(FLOOR(ent));
			/* we have to link the temp floor container to the new floor edict */
			FLOOR(floor) = FLOOR(ent);
			/* Send item info to the clients */
			G_CheckVis(floor, qtrue);
		} else {
			/* Add the item to an already existing floor edict - the floor container that
			 * is already linked might be from a different entity */
			FLOOR(floor) = FLOOR(ent);
			G_EventInventoryAdd(floor, G_VisToPM(floor->visflags), 1);
			assert(item.t);
			G_WriteItem(item, to, tx, ty);
		}
	} else {
		G_EventInventoryAdd(floor, G_TeamToPM(ent->team), 1);
		assert(item.t);
		G_WriteItem(item, to, tx, ty);
	}

	/* Update reaction firemode when something is moved from/to a hand. */
	if (INV_IsRightDef(from) || INV_IsRightDef(to)) {
		G_EventReactionFireHandChange(ent, ACTOR_HAND_RIGHT);
	} else if (INV_IsLeftDef(from) || INV_IsLeftDef(to)) {
		G_EventReactionFireHandChange(ent, ACTOR_HAND_LEFT);
	}

	/* Other players receive weapon info only. */
	mask = G_VisToPM(ent->visflags) & ~G_TeamToPM(ent->team);
	if (mask) {
		if (INV_IsRightDef(from) || INV_IsLeftDef(from)) {
			G_EventInventoryDelete(ent, mask, from, fx, fy);
		}
		if (INV_IsRightDef(to) || INV_IsLeftDef(to)) {
			G_EventInventoryAdd(ent, mask, 1);
			assert(item.t);
			G_WriteItem(item, to, tx, ty);
		}
	}
	gi.EndEvents();
}

/**
 * @brief Reload weapon with actor.
 * @sa AI_ActorThink
 */
void G_ActorReload (int entnum, shoot_types_t st, qboolean quiet)
{
	edict_t *ent;
	invList_t *ic;
	invList_t *icFinal;
	invDef_t * hand;
	objDef_t *weapon;
	int tu;
	int containerID;
	invDef_t *bestContainer;

	ent = g_edicts + entnum;

	/* search for clips and select the one that is available easily */
	icFinal = NULL;
	tu = 100;
	hand = (st == ST_RIGHT_RELOAD) ? INVDEF(gi.csi->idRight) : INVDEF(gi.csi->idLeft);
	bestContainer = NULL;

	if (CONTAINER(ent, hand->id)) {
		weapon = CONTAINER(ent, hand->id)->item.t;
	/** @todo What if there is no item in the right hand - this would segfault then */
	} else if (INV_IsLeftDef(hand) && RIGHT(ent)->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = INVDEF(gi.csi->idRight);
		weapon = CONTAINER(ent, hand->id)->item.t;
	} else
		return;

	assert(weapon);

	/* LordHavoc: Check if item is researched when in singleplayer? I don't think this is really a
	 * cheat issue as in singleplayer there is no way to inject fake client commands in the virtual
	 * network buffer, and in multiplayer everything is researched */

	for (containerID = 0; containerID < gi.csi->numIDs; containerID++) {
		if (INVDEF(containerID)->out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = ent->i.c[containerID]; ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)) {
					icFinal = ic;
					tu = INVDEF(containerID)->out;
					bestContainer = INVDEF(containerID);
					break;
				}
		}
	}

	/* send request */
	if (bestContainer)
		G_ActorInvMove(entnum, bestContainer, icFinal, hand, 0, 0, qtrue, quiet);
}
