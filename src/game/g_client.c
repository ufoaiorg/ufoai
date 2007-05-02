/**
 * @file g_client.c
 * @brief Main part of the game logic.
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

#define VIS_APPEAR	1
#define VIS_PERISH	2

int turnTeam;	/* Defined in g_local.h Stores level.activeTeam while G_CanReactionFire() is abusing it. */

int reactionFiremode[MAX_EDICTS][RF_MAX]; /* Defined in g_local.h See there for full info. */

static qboolean sentAppearPerishEvent;

/**
 * @brief Generates the player mask
 */
int G_TeamToPM (int team)
{
	player_t *player;
	int player_mask, i;

	player_mask = 0;

	/* don't handle the ai players, here */
	for (i = 0, player = game.players; i < game.maxplayers; i++, player++)
		if (player->inuse && team == player->pers.team)
			player_mask |= (1 << i);

	return player_mask;
}


/**
 * @brief Converts vis mask to player mask
 */
int G_VisToPM (int vis_mask)
{
	player_t *player;
	int player_mask, i;

	player_mask = 0;

	/* don't handle the ai players, here */
	for (i = 0, player = game.players; i < game.maxplayers; i++, player++)
		if (player->inuse && (vis_mask & (1 << player->pers.team)))
			player_mask |= (1 << i);

	return player_mask;
}


/**
 * @brief Send stats to network buffer
 */
void G_SendStats (edict_t * ent)
{
	/* extra sanity checks */
	ent->TU = max(ent->TU, 0);
	ent->HP = max(ent->HP, 0);
	ent->AP = max(ent->AP, 0);
	ent->STUN = min(ent->STUN, 255);
	ent->morale = max(ent->morale, 0);

	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->AP);
	gi.WriteByte(ent->morale);
}


/**
 * @brief Write an item to the network buffer
 * @sa CL_ReceiveItem
 * @sa CL_SendItem
 */
void G_WriteItem (item_t item, int container, int x, int y)
{
	assert (item.t != NONE);
	gi.WriteFormat("bbbbbb", item.t, item.a, item.m, container, x, y);
}

/**
 * @brief Read item from the network buffer
 * @sa CL_ReceiveItem
 * @sa CL_SendItem
 */
void G_ReadItem (item_t * item, int * container, int * x, int * y)
{
	gi.ReadFormat("bbbbbb", &item->t, &item->a, &item->m, container, x, y);
}


/**
 * @brief Write player stats to network buffer
 * @sa G_SendStats
 */
void G_SendPlayerStats (player_t * player)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == player->pers.team)
			G_SendStats(ent);
}


/**
 * @brief Network function to update the time units
 * @sa G_SendStats
 */
void G_GiveTimeUnits (int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && !(ent->state & STATE_DEAD) && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == team) {
			ent->state &= ~STATE_DAZED;
			ent->TU = GET_TU(ent->chr.skills[ABILITY_SPEED]);
			G_SendStats(ent);
		}
}


/**
 * @brief
 */
void G_SendState (int player_mask, edict_t * ent)
{
	gi.AddEvent(player_mask & G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);

	gi.AddEvent(player_mask & ~G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state & STATE_PUBLIC);
}


/**
 * @brief
 * @sa G_EndGame
 * @sa G_AppearPerishEvent
 * @sa CL_InvAdd
 */
void G_SendInventory (int player_mask, edict_t * ent)
{
	invList_t *ic;
	unsigned short nr = 0;
	int j;

	/* test for pointless player mask */
	if (!player_mask)
		return;

	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next)
			nr++;

	/* return if no inventory items to send */
	if (nr == 0 && ent->type != ET_ITEM) {
		return;
	}

	gi.AddEvent(player_mask, EV_INV_ADD);

	gi.WriteShort(ent->number);

	/* size of inventory */
	gi.WriteShort(nr * 6);
	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next) {
			/* send a single item */
			G_WriteItem(ic->item, j, ic->x, ic->y);
		}
}


/**
 * @brief
 * @sa CL_ActorAppear
 */
extern void G_AppearPerishEvent (int player_mask, int appear, edict_t * check)
{
	int maxMorale;

	sentAppearPerishEvent = qtrue;

	if (appear) {
		/* appear */
		switch (check->type) {
		case ET_ACTOR:
		case ET_UGV:
			/* parsed in CL_ActorAppear */
			gi.AddEvent(player_mask, EV_ACTOR_APPEAR);
			gi.WriteShort(check->number);
			gi.WriteByte(check->team);
			gi.WriteByte(check->chr.teamDesc);
			gi.WriteByte(check->pnum);
			gi.WriteGPos(check->pos);
			gi.WriteByte(check->dir);
			if (RIGHT(check)) {
				gi.WriteByte(RIGHT(check)->item.t);
			} else {
				gi.WriteByte(NONE);
			}

			if (LEFT(check)) {
				gi.WriteByte(LEFT(check)->item.t);
			} else {
				gi.WriteByte(NONE);
			}

			gi.WriteShort(check->body);
			gi.WriteShort(check->head);
			gi.WriteByte(check->skin);
			gi.WriteShort(check->state & STATE_PUBLIC);
			gi.WriteByte(check->fieldSize);
			gi.WriteByte(GET_TU(check->chr.skills[ABILITY_SPEED]));
			maxMorale = GET_MORALE(check->chr.skills[ABILITY_MIND]);
			if (maxMorale >= MAX_SKILL)
				maxMorale = MAX_SKILL;
			gi.WriteByte(maxMorale);
			gi.WriteShort(GET_HP(check->chr.skills[ABILITY_POWER]));

			if (player_mask & G_TeamToPM(check->team)) {
				gi.AddEvent(player_mask & G_TeamToPM(check->team), EV_ACTOR_STATECHANGE);
				gi.WriteShort(check->number);
				gi.WriteShort(check->state);
			}
			G_SendInventory(G_TeamToPM(check->team) & player_mask, check);
			break;

		case ET_ITEM:
			gi.AddEvent(player_mask, EV_ENT_APPEAR);
			gi.WriteShort(check->number);
			gi.WriteByte(ET_ITEM);
			gi.WriteGPos(check->pos);
			G_SendInventory(player_mask, check);
			break;
		}
	} else if (check->type == ET_ACTOR || check->type == ET_UGV || check->type == ET_ITEM) {
		/* disappear */
		gi.AddEvent(player_mask, EV_ENT_PERISH);
		gi.WriteShort(check->number);
	}
}


/**
 * @brief Checks whether a point is "visible" from the edicts position
 */
extern qboolean G_FrustomVis (edict_t * from, vec3_t point)
{
	/* view frustom check */
	vec3_t delta;
	byte dv;

	delta[0] = point[0] - from->origin[0];
	delta[1] = point[1] - from->origin[1];
	delta[2] = 0;
	VectorNormalize(delta);
	dv = from->dir & (DIRECTIONS-1);

	/* test 120 frustom (cos 60 = 0.5) */
	if ((delta[0] * dvecsn[dv][0] + delta[1] * dvecsn[dv][1]) < 0.5)
		return qfalse;
	else
		return qtrue;
}


/**
 * @brief tests the visibility between two points
 * @param[in] from  The point to check visibility from
 * @param[to] to    The point to check visibility to
 * @return true if the points are visible from each other, false otherwise.
 */
static qboolean G_LineVis (vec3_t from, vec3_t to)
{
#if 0 /* this version is more accurate the other version is much faster */
	/* FIXME: this version is not working with func_breakable */
	trace_t tr;
	tr = gi.trace(from, NULL, NULL, to, NULL, MASK_SOLID);
	return (tr.fraction >= 1.0);
#else
	return gi.TestLine(from, to);
#endif
}

/**
 * @brief calculate how much check is "visible" from from
 */
extern float G_ActorVis (vec3_t from, edict_t * check, qboolean full)
{
	vec3_t test, dir;
	float delta;
	int i, n;

	/* start on eye height */
	VectorCopy(check->origin, test);
	if (check->state & STATE_DEAD) {
		test[2] += PLAYER_DEAD;
		delta = 0;
	} else if (check->state & (STATE_CROUCHED | STATE_PANIC)) {
		test[2] += PLAYER_CROUCH - 2;
		delta = (PLAYER_CROUCH - PLAYER_MIN) / 2 - 2;
	} else {
		test[2] += PLAYER_STAND;
		delta = (PLAYER_STAND - PLAYER_MIN) / 2 - 2;
	}

	/* side shifting -> better checks */
	dir[0] = from[1] - check->origin[1];
	dir[1] = check->origin[0] - from[0];
	dir[2] = 0;
	VectorNormalize(dir);
	VectorMA(test, -7, dir, test);

	/* do 3 tests */
	n = 0;
	for (i = 0; i < 3; i++) {
		if (!G_LineVis(from, test)) {
			if (full)
				n++;
			else
				return 1.0;
		}

		/* look further down or stop */
		if (!delta) {
			if (n > 0)
				return 1.0;
			else
				return 0.0;
		}
		VectorMA(test, 7, dir, test);
		test[2] -= delta;
	}

	/* return factor */
	switch (n) {
	case 0:
		return 0.0;
	case 1:
		return 0.1;
	case 2:
		return 0.5;
	default:
		return 1.0;
	}
}


/**
 * @brief test if check is visible by from
 * from is from team team
 */
float G_Vis (int team, edict_t * from, edict_t * check, int flags)
{
	vec3_t eye;

	/* if any of them isn't in use, then they're not visible */
	if (!from->inuse || !check->inuse)
		return 0.0;

	/* only actors and ugvs can see anything */
	if ((from->type != ET_ACTOR && from->type != ET_UGV) || (from->state & STATE_DEAD))
		return 0.0;

	/* living team members are always visible */
	if (team >= 0 && check->team == team && !(check->state & STATE_DEAD))
		return 1.0;

	/* standard team rules */
	if (team >= 0 && from->team != team)
		return 0.0;

	/* inverse team rules */
	if (team < 0 && (from->team == -team || from->team == TEAM_CIVILIAN || check->team != -team))
		return 0.0;

	/* check for same pos */
	if (VectorCompare(from->pos, check->pos))
		return 1.0;

	/* view distance check */
	if (VectorDistSqr(from->origin, check->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST)
		return 0.0;

	/* view frustom check */
	if (!(flags & VT_NOFRUSTOM) && !G_FrustomVis(from, check->origin))
		return 0.0;

	/* get viewers eye height */
	VectorCopy(from->origin, eye);
	if (from->state & (STATE_CROUCHED | STATE_PANIC))
		eye[2] += EYE_CROUCH;
	else
		eye[2] += EYE_STAND;

	/* line trace check */
	switch (check->type) {
	case ET_ACTOR:
	case ET_UGV:
		return G_ActorVis(eye, check, qfalse);
	case ET_ITEM:
		return !G_LineVis(eye, check->origin);
	default:
		return 0.0;
	}
}


/**
 * @brief test if check is visible by team (or if visibility changed?)
 * @sa G_CheckVisTeam
 * @sa AI_FighterCalcGuete
 */
int G_TestVis (int team, edict_t * check, int flags)
{
	int i, old;
	edict_t *from;

	/* store old flag */
	old = (check->visflags & (1 << team)) ? 1 : 0;
	if (!(flags & VT_PERISH) && old)
		return VIS_YES;

	/* test if check is visible */
	for (i = 0, from = g_edicts; i < globals.num_edicts; i++, from++)
		if (G_Vis(team, from, check, flags))
			/* visible */
			return VIS_YES | !old;

	/* not visible */
	return old;
}


/**
 * @brief
 * @sa G_TestVis
 */
int G_CheckVisTeam (int team, edict_t * check, qboolean perish)
{
	int vis, i, end;
	int status;

	status = 0;

	/* decide whether to check all entities or a specific one */
	if (!check) {
		check = g_edicts;
		end = globals.num_edicts;
	} else
		end = 1;


	/* check visibility */
	for (i = 0; i < end; i++, check++)
		if (check->inuse) {
			/* check if he's visible */
			vis = G_TestVis(team, check, perish);

			if (vis & VIS_CHANGE) {
				check->visflags ^= (1 << team);
				G_AppearPerishEvent(G_TeamToPM(team), vis & VIS_YES, check);

				if (vis & VIS_YES) {
					status |= VIS_APPEAR;
					if ((check->type == ET_ACTOR || check->type == ET_UGV) && !(check->state & STATE_DEAD) && check->team != TEAM_CIVILIAN)
						status |= VIS_STOP;
				} else
					status |= VIS_PERISH;
			}
		}

	return status;
}


/**
 * @brief
 * @sa G_CheckVisTeam
 */
int G_CheckVis (edict_t * check, qboolean perish)
{
	int team;
	int status;

	status = 0;
	for (team = 0; team < MAX_TEAMS; team++)
		if (level.num_alive[team])
			status |= G_CheckVisTeam(team, check, perish);

	return status;
}


/**
 * @brief
 */
void G_ClearVisFlags (int team)
{
	edict_t *ent;
	int i, mask;

	mask = ~(1 << team);
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse)
			ent->visflags &= mask;
}


/**
 * @brief
 */
int G_DoTurn (edict_t * ent, byte toDV)
{
	float angleDiv;
	const byte *rot;
	int i, num;
	int status;

	assert(ent->dir < DIRECTIONS);
#ifdef DEBUG
	if (ent->dir >= DIRECTIONS)
		return 0;	/* never reached. need for code analyst. */
#endif

	toDV &= (DIRECTIONS-1);

	/* return if no rotation needs to be done */
	if ((ent->dir) == (toDV))
		return 0;

	/* calculate angle difference */
	angleDiv = dangle[toDV] - dangle[ent->dir];
	if (angleDiv > 180.0)
		angleDiv -= 360.0;
	if (angleDiv < -180.0)
		angleDiv += 360.0;

	/* prepare rotation */
	if (angleDiv > 0) {
		rot = dvleft;
		num = (+angleDiv + 22.5) / 45.0;
	} else {
		rot = dvright;
		num = (-angleDiv + 22.5) / 45.0;
	}

	/* do rotation and vis checks */
	status = 0;

	for (i = 0; i < num; i++) {
		ent->dir = rot[ent->dir];

		status |= G_CheckVisTeam(ent->team, NULL, qfalse);
/*		if ( stop && (status & VIS_STOP) ) */
		/* stop turning if a new living player appears */
/*			break; */
	}

	return status;
}


/**
 * @brief Checks whether the the requested action is possible
 * @param[in] quiet Don't print the console message if quiet is true
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent And which of his soldiers (or entities) he is trying to do
 * the action with
 * @todo: Integrate into hud - don't use cprintf
 */
qboolean G_ActionCheck (player_t * player, edict_t * ent, int TU, qboolean quiet)
{
	int msglevel;

	/* a generic tester if an action could be possible */
	if (level.activeTeam != player->pers.team) {
		gi.cprintf(player, PRINT_HUD, _("Can't perform action - this isn't your round!\n"));
		return qfalse;
	}

	msglevel = quiet ? PRINT_NONE : PRINT_HUD;

	if (!ent || !ent->inuse) {
		gi.cprintf(player, msglevel, _("Can't perform action - object not present!\n"));
		return qfalse;
	}

	if (ent->type != ET_ACTOR && ent->type != ET_UGV) {
		gi.cprintf(player, msglevel, _("Can't perform action - not an actor!\n"));
		return qfalse;
	}

	if (ent->state & STATE_STUN) {
		gi.cprintf(player, msglevel, _("Can't perform action - actor is stunned!\n"));
		return qfalse;
	}

	if (ent->state & STATE_DEAD) {
		gi.cprintf(player, msglevel, _("Can't perform action - actor is dead!\n"));
		return qfalse;
	}

	if (ent->team != player->pers.team) {
		gi.cprintf(player, msglevel, _("Can't perform action - not on same team!\n"));
		return qfalse;
	}

	if (ent->pnum != player->num) {
		gi.cprintf(player, msglevel, _("Can't perform action - no control over allied actors!\n"));
		return qfalse;
	}

	if (TU > ent->TU) {
		return qfalse;
	}

	/* could be possible */
	return qtrue;
}


/**
 * @brief Spawns a new entity at the floor
 * @note This is e.g. used to place dropped weapons/items at the floor
 */
edict_t *G_SpawnFloor (pos3_t pos)
{
	edict_t *floor;

	floor = G_Spawn();
	floor->classname = "item";
	floor->type = ET_ITEM;
	VectorCopy(pos, floor->pos);
	gi.GridPosToVec(gi.map, floor->pos, floor->origin);
	floor->origin[2] -= 24;
	return floor;
}

/**
 * @brief
 * @sa G_GetFloorItems
 */
edict_t *G_GetFloorItemsFromPos (pos3_t pos)
{
	edict_t *floor;

	for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
		if (!floor->inuse || floor->type != ET_ITEM)
			continue;
		if (!VectorCompare(pos, floor->pos))
			continue;

		return floor;
	}
	/* nothing found at this pos */
	return NULL;
}

/**
 * @brief Prepares a list of items on the floor at given entity position.
 * @param[in] *ent Pointer to an entity being an actor.
 * @return pointer to edict_t being a floor (with items).
 * @note This function is somehow broken - it returns NULL in some cases of items on the floor.
 */
edict_t *G_GetFloorItems (edict_t * ent)
{
	edict_t *floor = G_GetFloorItemsFromPos(ent->pos);
	/* found items */
	if (floor) {
		FLOOR(ent) = FLOOR(floor);
		return floor;
	}

	/* no items on ground found */
	FLOOR(ent) = NULL;
	return NULL;
}

#ifdef DEBUG
/**
 * @brief Debug functions that prints the content of a floor (container) at a given position in the map.
 * @param[in] Position of the floor in the map.
 */
static void G_PrintFloorToConsole (pos3_t pos)
{
	edict_t *floor = G_GetFloorItemsFromPos(pos);
	Com_DPrintf("G_PrintFloorToConsole: Printing containers from floor at %i,%i,%i.\n", pos[0], pos[1], pos[2]);
	if (floor) {
		INV_PrintToConsole(&floor->i);
	} else {
		Com_DPrintf("G_PrintFloorToConsole: No Floor items found.\n");
	}
}
#endif

/**
 * @brief Moves an item inside an inventory. Floors are handled special.
 * @input[in] player The player the edict/soldier belongs to.
 * @input[in] num The edict number of the selected/used edict/soldier.
 * @input[in] from The container (-id) the item should be moved from.
 * @input[in] fx
 * @input[in] fy
 * @input[in] to The container (-id) the item should be moved to.
 * @input[in] tx
 * @input[in] ty
 * @input[in] checkaction Set this to qtrue if you want to check for TUs, otherwise qfalse.
 * @input[in] quiet Set this to qfalse to prevent message-flooding.
 * @sa event PA_INVMOVE
 * @sa AI_ActorThink
 */
void G_ClientInvMove (player_t * player, int num, int from, int fx, int fy, int to, int tx, int ty, qboolean checkaction, qboolean quiet)
{
	edict_t *ent, *floor;
	invList_t *ic;
	qboolean newFloor;
	item_t item;
	int mask;
	inventory_action_t ia;
	int msglevel;

	ent = g_edicts + num;
	msglevel = quiet ? PRINT_NONE : PRINT_HIGH;

	/* check if action is possible */
	if (checkaction && !G_ActionCheck(player, ent, 1, quiet))
		return;

	/* "get floor ready" - searching for existing floor-edict*/
	floor = G_GetFloorItems(ent); /* Also sets FLOOR(ent) to correct value. */
	if (to == gi.csi->idFloor && !floor) {
		/* We are moving to the floor, but no existing edict for this floor-tile found -> create new one */
		floor = G_SpawnFloor(ent->pos);
		newFloor = qtrue;
	} else if (from == gi.csi->idFloor && !floor) {
		/* We are moving from the floor, but no existing edict for this floor-tile found -> thi should never be the case. */
		Com_Printf("G_ClientInvMove: No source-floor found.\n");
		return;
	} else {
		/* There already exists an edict for this floor-tile. */
		newFloor = qfalse;
	}

	/* search for space */
	if (tx == NONE) {
/*		assert (ty == NONE); */
		if (ty != NONE)
			Com_Printf("G_ClientInvMove: Error: ty != NONE, it is %i.\n", ty);

		ic = Com_SearchInInventory(&ent->i, from, fx, fy);
		if (ic)
			Com_FindSpace(&ent->i, ic->item.t, to, &tx, &ty);
	}
	if (tx == NONE) {
/*		assert (ty == NONE); */
		if (ty != NONE)
			Com_Printf("G_ClientInvMove: Error: ty != NONE, it is %i.\n", ty);
		return;
	}

	/* Try to actually move the item and check the return value */
	ia = Com_MoveInInventory(&ent->i, from, fx, fy, to, tx, ty, &ent->TU, &ic);
	switch (ia) {
	case IA_NONE:
		/* No action possible - abort */
		return;
	case IA_NOTIME:
		gi.cprintf(player, msglevel, _("Can't perform action - not enough TUs!\n"));
		return;
	case IA_NORELOAD:
		gi.cprintf(player, msglevel, _("Can't perform action - weapon already fully loaded with the same ammunition!\n")); /* @todo: "or not researched" */
		return;
	default:
		/* Continue below. */
		break;
	}

	/* FIXME: This is impossible - if not we should check MAX_INVDEFS*/
	assert((gi.csi->idFloor >= 0) && (gi.csi->idFloor < MAX_CONTAINERS));
#ifdef DEBUG
	if ((gi.csi->idFloor < 0) || (gi.csi->idFloor >= MAX_CONTAINERS))
		return;	/* never reached. need for code analyst. */
#endif

	/* successful inventory change; remove the item in clients */
	if (from == gi.csi->idFloor) {
		/* We removed an item from a floor - check how the client needs to be updated. */
		assert (!newFloor);
		if (FLOOR(ent)) {
			/* There is still something on the floor. */
			FLOOR(floor) = FLOOR(ent); /* @todo: _why_ do we do this here exactly? Shouldn't they be the same already at this point? */
			/* Tell the client to remove the item from the container */
			gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_DEL);
			gi.WriteShort(floor->number);
			gi.WriteByte(from);
			gi.WriteByte(fx);
			gi.WriteByte(fy);
		} else {
			/* Floor is empty, remove the edict (from server+client) if we are not moving to it. */
			if (to != gi.csi->idFloor) {
				gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
				gi.WriteShort(floor->number);
				G_FreeEdict(floor);
			}
		}
	} else {
		/* Tell the client to remove the item from the container */
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_DEL);
		gi.WriteShort(num);
		gi.WriteByte(from);
		gi.WriteByte(fx);
		gi.WriteByte(fy);
	}

	/* send tu's */
	G_SendStats(ent);

	item = ic->item;

	if (ia == IA_RELOAD || ia == IA_RELOAD_SWAP) {
		/* reload */
		if (to == gi.csi->idFloor) {
			assert (!newFloor);
			assert (FLOOR(floor) == FLOOR(ent));
			mask = G_VisToPM(floor->visflags);
		} else {
			mask = G_TeamToPM(ent->team);
		}

		/* send ammo message to all --- it's fun to hear that sound */
		gi.AddEvent(PM_ALL, EV_INV_RELOAD);
		/* is this condition below needed? or is 'num' enough?
		   probably needed so that red rifle on the floor changes color,
		   but this needs testing. */
		gi.WriteShort(to == gi.csi->idFloor ? floor->number : num);
		gi.WriteByte(gi.csi->ods[item.t].ammo);
		gi.WriteByte(item.m);
		gi.WriteByte(to);
		gi.WriteByte(ic->x);
		gi.WriteByte(ic->y);

		if (ia == IA_RELOAD) {
			gi.EndEvents();
			return;
		} else { /* ia == IA_RELOAD_SWAP */
			to = from;
			tx = fx;
			ty = fy;
			item = Com_SearchInInventory(&ent->i, from, fx, fy)->item;
		}
	}


	/* add it */
	if (to == gi.csi->idFloor) {
		/* We moved an item to the floor - check how the client needs to be updated. */
		if (newFloor) {
			/* A new container was created for the floor. */
			assert(FLOOR(ent));
			FLOOR(floor) = FLOOR(ent); /* @todo: _why_ do we do this here exactly? Shouldn't they be the same already at this point? */
			/* Send item info to the clients */
			G_CheckVis(floor, qtrue);
		} else {
			/* Add the item; update floor, because we add at beginning */
			FLOOR(floor) = FLOOR(ent); /* @todo: _why_ do we do this here exactly? Shouldn't they be the same already at this point? */
			/* Tell the client to add the item to the container. */
			gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_ADD);
			gi.WriteShort(floor->number);
			gi.WriteShort(6);
			G_WriteItem(item, to, tx, ty);
		}
	} else {
		/* Tell the client to add the item to the container. */
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_ADD);
		gi.WriteShort(num);
		gi.WriteShort(6);
		G_WriteItem(item, to, tx, ty);
	}

	/* Update reaction firemode when something is moved from/to a hand. */
	if ((from == gi.csi->idRight) || (to == gi.csi->idRight)) {
		Com_DPrintf("G_ClientInvMove: Something moved in/out of Right hand.\n");
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
		gi.WriteShort(num);
		gi.WriteShort(0);	/**< hand=right */
	} else if ((from == gi.csi->idLeft) || (to == gi.csi->idLeft)) {
		Com_DPrintf("G_ClientInvMove:  Something moved in/out of Left hand.\n");
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
		gi.WriteShort(num);
		gi.WriteShort(1);	/**< hand=left */
	}

	/* Other players receive weapon info only. */
	mask = G_VisToPM(ent->visflags) & ~G_TeamToPM(ent->team);
	if (mask) {
		if (from == gi.csi->idRight || from == gi.csi->idLeft) {
			gi.AddEvent(mask, EV_INV_DEL);
			gi.WriteShort(num);
			gi.WriteByte(from);
			gi.WriteByte(fx);
			gi.WriteByte(fy);
		}
		if (to == gi.csi->idRight || to == gi.csi->idLeft) {
			gi.AddEvent(mask, EV_INV_ADD);
			gi.WriteShort(num);
			gi.WriteShort(6);
			G_WriteItem(item, to, tx, ty);
		}
	}
	gi.EndEvents();
}


/*#define ADJACENT*/
/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 * @param[in] *ent Pointer to an edict_t being an actor.
 * @sa G_ActorDie
 */
void G_InventoryToFloor (edict_t * ent)
{
	invList_t *ic, *next;
	int k;
	edict_t *floor;
#ifdef ADJACENT
	edict_t *floorAdjacent = NULL;
	int i;
#endif

	/* check for items */
	for (k = 0; k < gi.csi->numIDs; k++)
		if (ent->i.c[k])
			break;

	/* edict is not carrying any items */
	if (k >= gi.csi->numIDs)
		return;

	/* find the floor */
	floor = G_GetFloorItems(ent);
	if (!floor) {
		floor = G_SpawnFloor(ent->pos);
	} else {
		/* destroy this edict (send this event to all clients that see the edict) */
		gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
		gi.WriteShort(floor->number);
		floor->visflags = 0;
	}

	/* drop items */
	/* cycle through all containers */
	for (k = 0; k < gi.csi->numIDs; k++) {
		/* skip floor - we want to drop to floor */
		if (k == gi.csi->idFloor)
			continue;
		/* skip csi->idArmor, we will collect armours using idArmor container,
		   not idFloor */
		if (k == gi.csi->idArmor) {
			if (ent->i.c[gi.csi->idArmor])
				Com_DPrintf("G_InventoryToFloor()... this actor has armour: %s\n", gi.csi->ods[ent->i.c[gi.csi->idArmor]->item.t].name);
			continue;
		}
		/* now cycle through all items for the container of the character (or the entity) */
		for (ic = ent->i.c[k]; ic; ic = next) {
			int x, y;
#ifdef ADJACENT
			vec2_t oldPos; /**< if we have to place it to adjacent  */
#endif
			/* Save the next inv-list before it gets overwritten below.
			   Do not put this in the "for" statement,
			   unless you want an endless loop. ;) */
			next = ic->next;
			/* find the coordinates for the current item on floor */
			Com_FindSpace(&floor->i, ic->item.t, gi.csi->idFloor, &x, &y);
			if (x == NONE) {
				assert (y == NONE);
				/* Run out of space on the floor or the item is armor
				   destroy the offending item if no adjacent places are free */
				/* store pos for later restoring the original value */
#ifdef ADJACENT
				Vector2Copy(ent->pos, oldPos);
				for (i = 0; i < DIRECTIONS; i++) {
					/* @todo: Check whether movement is possible here - otherwise don't use this field */
					/* extend pos with the direction vectors */
					Vector2Set(ent->pos, ent->pos[0] + dvecs[i][0], ent->pos[0] + dvecs[i][1]);
					/* now try to get a floor entity for that new location */
					floorAdjacent = G_GetFloorItems(ent);
					if (!floorAdjacent) {
						floorAdjacent = G_SpawnFloor(ent->pos);
					} else {
						/* destroy this edict (send this event to all clients that see the edict) */
						gi.AddEvent(G_VisToPM(floorAdjacent->visflags), EV_ENT_PERISH);
						gi.WriteShort(floorAdjacent->number);
						floorAdjacent->visflags = 0;
					}
					Com_FindSpace(&floorAdjacent->i, ic->item.t, gi.csi->idFloor, &x, &y);
					if (x != NONE) {
						ic->x = x;
						ic->y = y;
						ic->next = FLOOR(floorAdjacent);
						FLOOR(floorAdjacent) = ic;
						break;
					}
					/* restore original pos */
					Vector2Copy(oldPos, ent->pos);
				}
				/* added to adjacent pos? */
				if (i < DIRECTIONS) {
					/* restore original pos - if no free space, this was done
					 * already in the for loop */
					Vector2Copy(oldPos, ent->pos);
					continue;
				}
#endif
				if (Q_strncmp(gi.csi->ods[ic->item.t].type, "armor", MAX_VAR))
					gi.dprintf("G_InventoryToFloor: Warning: could not drop item to floor: %s\n", gi.csi->ods[ic->item.t].id);
				if (!Com_RemoveFromInventory(&ent->i, k, ic->x, ic->y))
					gi.dprintf("G_InventoryToFloor: Error: could not remove item: %s\n", gi.csi->ods[ic->item.t].id);
			} else {
				ic->x = x;
				ic->y = y;
				ic->next = FLOOR(floor);
				FLOOR(floor) = ic;
#ifdef PARANOID
				Com_DPrintf("G_InventoryToFloor: item to floor: %s\n", gi.csi->ods[ic->item.t].id);
#endif
			}
		}

		/* destroy link */
		ent->i.c[k] = NULL;
	}

	FLOOR(ent) = FLOOR(floor);

	if (ent->i.c[gi.csi->idArmor])
		Com_DPrintf("At the end of G_InventoryToFloor()... this actor has armor in idArmor container: %s\n", gi.csi->ods[ent->i.c[gi.csi->idArmor]->item.t].name);
	else
		Com_DPrintf("At the end of G_InventoryToFloor()... this actor has NOT armor in idArmor container\n");

	/* send item info to the clients */
	G_CheckVis(floor, qtrue);
#ifdef ADJACENT
	if (floorAdjacent)
		G_CheckVis(floorAdjacent, qtrue);
#endif
}


byte *fb_list[MAX_EDICTS];
int fb_length;

/**
 * @brief Build the forbidden list for the pathfinding.
 * @param[in] team The team number if the list should be calculated from the eyes of that team. Use 0 to ignore team.
 * @note The forbidden list (fb_list and fb_length) is a
 * list of entity positions that are occupied by an entity.
 * This list is checked everytime an actor wants to walk there.
 * @sa G_MoveCalc
 * @sa cl_actor.c:CL_BuildForbiddenList <- shares quite some code
 */
void G_BuildForbiddenList (int team)
{
	edict_t *ent = NULL;
	int vis_mask;
	int i;

	fb_length = 0;

	/* team visibility */
	if (team)
		vis_mask = 1 << team;
	else
		vis_mask = 0xFFFFFFFF;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->inuse)
			continue;
		if ((ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) && (ent->visflags & vis_mask))
			fb_list[fb_length++] = ent->pos;
	}

	if (fb_length > MAX_EDICTS)
		gi.error("G_BuildForbiddenList: list too long\n");
}

/**
 * @brief @todo: writeme
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] distance The distance to calculate the move for.
 * @sa G_BuildForbiddenList
 */
void G_MoveCalc (int team, pos3_t from, int distance)
{
	G_BuildForbiddenList(team);
	gi.MoveCalc(gi.map, from, distance, fb_list, fb_length);
}


/**
 * @brief Check whether there is already an edict on the field where the actor
 * is trying to get with the next move
 * @param[in] dv direction to walk to
 * @param[in] from starting point to walk from
 */
static qboolean G_CheckMoveBlock (pos3_t from, int dv)
{
	edict_t *ent;
	pos3_t pos;
	int i;

	/* get target position */
	VectorCopy(from, pos);
	/* check the next field in the given direction */
	PosAddDV(pos, dv);

	/* search for blocker */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) && VectorCompare(pos, ent->pos))
			return qtrue;

	/* nothing found */
	return qfalse;
}

#define MAX_DVTAB 32
/**
 * @brief
 * @sa CL_ActorStartMove
 */
void G_ClientMove (player_t * player, int visTeam, int num, pos3_t to, qboolean stop, qboolean quiet)
{
	edict_t *ent;
	int length, status, initTU;
	byte dvtab[MAX_DVTAB];
	byte dv, numdv, steps;
	pos3_t pos;
	float div, tu;

	ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, 2, quiet))
		return;

	/* calculate move table */
	G_MoveCalc(visTeam, ent->pos, MAX_ROUTE);
	length = gi.MoveLength(gi.map, to, qfalse);

	/* length of 0xFF means not reachable */
	if (length && length < 0xFF) {
		/* start move */
		gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_START_MOVE);
		gi.WriteShort(ent->number);

		/* assemble dv-encoded move data */
		VectorCopy(to, pos);
		numdv = 0;
		tu = 0;
		initTU = ent->TU;

		while ((dv = gi.MoveNext(gi.map, pos)) < 0xFF) {
			/* store the inverted dv */
			/* (invert by flipping the first bit and add the old height) */
			dvtab[numdv++] = ((dv ^ 1) & (DIRECTIONS - 1)) | (pos[2] << 3);
			PosAddDV(pos, dv);
			assert(numdv < MAX_DVTAB);
		}

		if (VectorCompare(pos, ent->pos)) {
			/* everything ok, found valid route */
			/* create movement related events */
			steps = 0;
			sentAppearPerishEvent = qfalse;

			FLOOR(ent) = NULL;

			/* BEWARE: do not print anything (even with DPrintf)
			   in functions called in this loop
			   without setting 'steps = 0' afterwards;
			   also do not send events except G_AppearPerishEvent
			   without manually setting 'steps = 0' */
			while (numdv > 0) {
				/* get next dv */
				numdv--;

				/* turn around first */
				status = G_DoTurn(ent, dvtab[numdv]);
				if (status) {
					/* send the turn */
					gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
					gi.WriteShort(ent->number);
					gi.WriteByte(ent->dir);
				}
				if (stop && (status & VIS_STOP))
					break;
				if (status || sentAppearPerishEvent) {
					steps = 0;
					sentAppearPerishEvent = qfalse;
				}

				/* check for "blockers" */
				if (G_CheckMoveBlock(ent->pos, dvtab[numdv]))
					break;

				/* decrease TUs */
				div = ((dvtab[numdv] & (DIRECTIONS - 1)) < 4) ? 2 : 3;
				if (ent->state & STATE_CROUCHED)
					div *= 1.5;
				if ((int) (tu + div) > ent->TU)
					break;
				tu += div;

				/* move */
				PosAddDV(ent->pos, dvtab[numdv]);
				gi.GridPosToVec(gi.map, ent->pos, ent->origin);

				/* link it at new position */
				gi.linkentity(ent);

				/* write move header if not yet done */
				if (!steps) {
					gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
					gi.WriteShort(num);
					gi.WriteNewSave(0);
				}

				/* write step */
				steps++;
				gi.WriteByte(dvtab[numdv]);
				gi.WriteToSave(steps);

				/* check if player appears/perishes, seen from other teams */
				status = G_CheckVis(ent, qtrue);

				/* check for anything appearing, seen by "the moving one" */
				status = G_CheckVisTeam(ent->team, NULL, qfalse);

				/* Set ent->TU because the reaction code relies on ent->TU being accurate. */
				ent->TU = max(0, initTU - (int) tu);

				/* check for reaction fire */
				if (G_ReactToMove(ent, qtrue)){
					if (G_ReactToMove(ent, qfalse))
						status |= VIS_STOP;
					steps = 0;
					sentAppearPerishEvent = qfalse;
				}

				/* Restore ent->TU because the movement code relies on it not being modified! */
				ent->TU = max(0, initTU);

				/* check for death */
				if (ent->state & STATE_DEAD)
					return;

				if (stop && (status & VIS_STOP))
					break;

				if (sentAppearPerishEvent) {
					steps = 0;
					sentAppearPerishEvent = qfalse;
				}
			}

			/* submit the TUs / round down */
			ent->TU = max(0, initTU - (int) tu);
			G_SendStats(ent);

			/* end the move */
			G_GetFloorItems(ent);
			gi.EndEvents();
		}
	}
}


/**
 * @brief Sends the actual actor turn event over the netchannel
 */
static void G_ClientTurn (player_t * player, int num, int dv)
{
	edict_t *ent;

	ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_TURN, NOISY))
		return;

	/* check if we're already facing that direction */
	if (ent->dir == dv)
		return;

	/* start move */
	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_START_MOVE);
	gi.WriteShort(ent->number);

	/* do the turn */
	G_DoTurn(ent, (byte) dv);
	ent->TU -= TU_TURN;

	/* send the turn */
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->dir);

	/* send the new TUs */
	G_SendStats(ent);

	/* end the event */
	gi.EndEvents();
}


/**
 * @brief Changes the thate of a player/soldier.
 * @param[in,out] player @todo Writeme
 * @param[in] num @todo Writeme
 * @param[in] reqState The bit-map of the requested state change
 */
static void G_ClientStateChange (player_t * player, int num, int reqState)
{
	edict_t *ent;

	ent = g_edicts + num;

	/* Check if any action is possible. */
	if (!G_ActionCheck(player, ent, 0, NOISY))
		return;

	if (!reqState)
		return;

	switch (reqState) {
	case STATE_CROUCHED: /* toggle between crouch/stand */
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (G_ActionCheck(player, ent, TU_CROUCH, NOISY)) {
			ent->state ^= STATE_CROUCHED;
			ent->TU -= TU_CROUCH;
			/* Link it. */
			if (ent->state & STATE_CROUCHED)
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
			else
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
			gi.linkentity(ent);
		}
		break;
	case ~STATE_REACTION: /* request to turn off reaction fire */
		if ((ent->state & STATE_REACTION_MANY) || (ent->state & STATE_REACTION_ONCE)){
			if (ent->state & STATE_SHAKEN)
				gi.cprintf(player, PRINT_HIGH, _("Currently shaken, won't let their guard down.\n"));
			else {
				/* Turn off reaction fire and give the soldier back his TUs if it used some. */
				ent->state &= ~STATE_REACTION;

				if (reactionTUs[ent->number][REACT_TUS] > 0) {
					/* TUs where used for activation. */
					ent->TU += reactionTUs[ent->number][0];
				} else if (reactionTUs[ent->number][REACT_TUS] < 0) {
					/* No TUs where used for activation. */
					/* Don't give TUs back because none where used up (reaction fire was already active from previous turn) */
				} else {
					/* reactionTUs[ent->number][REACT_TUS] == 0) */
					/* This should never be the case.  */
					Com_DPrintf("G_ClientStateChange: 0 value saved for reaction while reaction is activated.\n");
				}
			}
		}
		break;
	case STATE_REACTION_MANY: /* request to turn on multi-reaction fire mode */
		/* We can assume that the previous mode is STATE_REACTION_ONCE here.
		 * If the gui behaviour ever changes we need to check for the previosu state in a better way here.
		 */
		if ((ent->TU >= (TU_REACTION_MULTI - reactionTUs[ent->number][REACT_TUS]))
			&& (ent->state & STATE_REACTION_ONCE)) {
			/* We have enough TUs for switching to multi-rf and the previous state is STATE_REACTION_ONCE */

			/* Disable single reaction fire and remove saved TUs. */
 			ent->state &= ~STATE_REACTION;
			if (reactionTUs[ent->number][REACT_TUS] > 0) {
				ent->TU += reactionTUs[ent->number][REACT_TUS];
				reactionTUs[ent->number][REACT_TUS] = 0;
			}

			/* enable multi reaction fire */
 			ent->state |= STATE_REACTION_MANY;
			ent->TU -= TU_REACTION_MULTI;
			reactionTUs[ent->number][REACT_TUS] = TU_REACTION_MULTI;
		} else {
			/* @todo: this is a workaround for now.
			the multi-rf button was clicked (i.e. single-rf is activated right now), but there are not enough TUs left for it.
			so the only sane action if the button is clicked is to disable everything in order to make it work at all.
			@todo: dsiplay correct "disable" button and directly call this function with "disable rf" parameters
			*/
			G_ClientStateChange (player, num, ~STATE_REACTION); /**< Turn off RF */
		}
		break;
	case STATE_REACTION_ONCE: /* request to turn on single-reaction fire mode */
		if (G_ActionCheck(player, ent, TU_REACTION_SINGLE, NOISY)) {
			/* Turn on reaction fire and save the used TUs to the list. */
			ent->state |= STATE_REACTION_ONCE;

#if 0 /* @todo: do we really want to enable this? i don't think we can get soemthing useable out of it .. the whole "save TUs until nex round" needs a cleanup. */
			if (reactionTUs[ent->number][REACT_TUS] > 0) {
				/* TUs where saved for this turn (either the full TU_REACTION or some remaining TUs from the shot. This was done either in the last turn or this one. */
				ent->TU -= reactionTUs[ent->number][REACT_TUS];
			} else

			if (reactionTUs[ent->number][REACT_TUS] == 0) {
#endif
				/* Reaction fire was not triggered in the last turn. */
				ent->TU -= TU_REACTION_SINGLE;
				reactionTUs[ent->number][REACT_TUS] = TU_REACTION_SINGLE;
#if 0	/* @todo: Same here */
			}  else {
				/* reactionTUs[ent->number][REACT_TUS] < 0 */
				/* Reaction fire was triggered in the last turn,
				   and has used 0 TU from this one.
				   Can be activated without TU-loss. */
			}
#endif
		}
		break;
	default:
		Com_Printf("G_ClientStateChange: unknown request %i, ignoring\n", reqState);
		return;
	}

	/* Send the state change. */
	G_SendState(G_VisToPM(ent->visflags), ent);

	/* Check if the player appears/perishes, seen from other teams. */
	G_CheckVis(ent, qtrue);

	/* Calc new vis for this player. */
	G_CheckVisTeam(ent->team, NULL, qfalse);

	/* Send the new TUs. */
	G_SendStats(ent);

	/* End the event. */
	gi.EndEvents();
}

/**
 * @brief
 * @sa G_MoraleStopPanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoralePanic (edict_t * ent, qboolean sanity, qboolean quiet)
{
	gi.cprintf(game.players + ent->pnum, PRINT_HIGH, _("%s panics!\n"), ent->chr.name);

	/* drop items in hands */
	if (!sanity) {
		if (RIGHT(ent))
			G_ClientInvMove(game.players + ent->pnum, ent->number, gi.csi->idRight, 0, 0, gi.csi->idFloor, NONE, NONE, qtrue, quiet);
		if (LEFT(ent))
			G_ClientInvMove(game.players + ent->pnum, ent->number, gi.csi->idLeft, 0, 0, gi.csi->idFloor, NONE, NONE, qtrue, quiet);
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
	AI_ActorThink(game.players + ent->pnum, ent);

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
	if (((ent->morale) / mor_panic->value) > (m_panic_stop->value * frand()))
		ent->state &= ~STATE_PANIC;
	else
		G_MoralePanic(ent, qtrue, quiet);
}

/**
 * @brief
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
		gi.bprintf(PRINT_HIGH, _("%s is on a rampage.\n"), ent->chr.name);
	else
		gi.bprintf(PRINT_HIGH, _("%s is consumed by mad rage!\n"), ent->chr.name);
	AI_ActorThink(game.players + ent->pnum, ent);
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
	if (((ent->morale) / mor_panic->value) > (m_rage_stop->value * frand())) {
		ent->state &= ~STATE_INSANE;
		G_SendState(G_VisToPM(ent->visflags), ent);
	} else
		G_MoralePanic(ent, qtrue, quiet);	/*regains sanity */
}

/**
 * @brief Applies morale behaviour on actors
 * @note only called when mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleStopPanic
 */
static void G_MoraleBehaviour (int team, qboolean quiet)
{
	edict_t *ent;
	int i, newMorale;
	qboolean sanity;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not to ET_UGV */
		if (ent->inuse && ent->type == ET_ACTOR && ent->team == team && !(ent->state & STATE_DEAD)) {
			/* civilians have a 1:1 chance to randomly run away in multiplayer */
			if (sv_maxclients->integer >= 2 && level.activeTeam == TEAM_CIVILIAN && 0.5 > frand())
				G_MoralePanic(ent, qfalse, quiet);
			/* multiplayer needs enabled sv_enablemorale */
			/* singleplayer has this in every case */
			if ((sv_maxclients->integer >= 2 && sv_enablemorale->integer == 1)
				|| sv_maxclients->integer == 1) {
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
				} else if (ent->morale <= mor_shaken->value && !(ent->state & STATE_PANIC) && !(ent->state & STATE_RAGE)) {
					ent->TU -= TU_REACTION_SINGLE; /* @todo: Comment-me: Why do we modify reaction stuff here? */
					/* shaken is later reset along with reaction fire */
					ent->state |= STATE_SHAKEN | STATE_REACTION_MANY;
					G_SendState(G_VisToPM(ent->visflags), ent);
					gi.cprintf(game.players + ent->pnum, PRINT_HIGH, _("%s is currently shaken.\n"), ent->chr.name);
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

			/* moraleregeneration, capped at max: */
			newMorale = ent->morale + MORALE_RANDOM(mor_regeneration->value);
			if (newMorale > GET_MORALE(ent->chr.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);
			else
				ent->morale = newMorale;

			/* send phys data and state: */
			G_SendStats(ent);
			gi.EndEvents();
		}
}


/**
 * @brief Reload weapon with actor.
 * @param[in] hand
 * @sa AI_ActorThink
 */
void G_ClientReload (player_t *player, int entnum, shoot_types_t st, qboolean quiet)
{
	edict_t *ent;
	invList_t *ic;
	int hand;
	int weapon, x, y, tu;
	int container, bestContainer;

	ent = g_edicts + entnum;

	/* search for clips and select the one that is available easily */
	x = 0;
	y = 0;
	tu = 100;
	bestContainer = NONE;
	hand = st == ST_RIGHT_RELOAD ? gi.csi->idRight : gi.csi->idLeft;

	if (ent->i.c[hand]) {
		weapon = ent->i.c[hand]->item.t;
	} else if (hand == gi.csi->idLeft
			&& gi.csi->ods[ent->i.c[gi.csi->idRight]->item.t].holdtwohanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	}
	else
		return;

	/* LordHavoc: Check if item is researched when in singleplayer? I don't think this is really a
	 * cheat issue as in singleplayer there is no way to inject fake client commands in the virtual
	 * network buffer, and in multiplayer everything is researched */

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (gi.csi->ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point */
			/* searching other containers if it would take longer */
			/* to retrieve the ammo from them than the one */
			/* we've already found. */
			for (ic = ent->i.c[container]; ic; ic = ic->next)
				if (INV_LoadableInWeapon(&gi.csi->ods[ic->item.t], weapon)) {
					x = ic->x;
					y = ic->y;
					tu = gi.csi->ids[container].out;
					bestContainer = container;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer != NONE)
		G_ClientInvMove(player, entnum, bestContainer, x, y, hand, 0, 0, qtrue, quiet);
}

/**
 * @brief Returns true if actor can reload weapon
 * @param[in] hand
 * @sa AI_ActorThink
 */
qboolean G_ClientCanReload (player_t *player, int entnum, shoot_types_t st)
{
	edict_t *ent;
	invList_t *ic;
	int hand, weapon;
	int container;

	ent = g_edicts + entnum;

	/* search for clips and select the one that is available easily */
	hand = st == ST_RIGHT_RELOAD ? gi.csi->idRight : gi.csi->idLeft;

	if (ent->i.c[hand]) {
		weapon = ent->i.c[hand]->item.t;
	} else if (hand == gi.csi->idLeft
			   && gi.csi->ods[ent->i.c[gi.csi->idRight]->item.t].holdtwohanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	} else
		return qfalse;

	for (container = 0; container < gi.csi->numIDs; container++)
		for (ic = ent->i.c[container]; ic; ic = ic->next)
			if ( INV_LoadableInWeapon(&gi.csi->ods[ic->item.t], weapon) )
				return qtrue;
	return qfalse;
}

/**
 * @brief Retrieve weapon from backpack for actor
 * @sa AI_ActorThink
 */
void G_ClientGetWeaponFromInventory (player_t *player, int entnum, qboolean quiet)
{
	edict_t *ent;
	invList_t *ic;
	int hand;
	int x, y, tu;
	int container, bestContainer;

	ent = g_edicts + entnum;

	/* search for weapons and select the one that is available easily */
	x = 0;
	y = 0;
	tu = 100;
	bestContainer = NONE;
	hand = gi.csi->idRight;

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (gi.csi->ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point */
			/* searching other containers if it would take longer */
			/* to retrieve the ammo from them than the one */
			/* we've already found. */
			for (ic = ent->i.c[container]; ic; ic = ic->next)
				if (gi.csi->ods[ic->item.t].weapon && (ic->item.a > 0 || !gi.csi->ods[ic->item.t].reload)) {
					x = ic->x;
					y = ic->y;
					tu = gi.csi->ids[container].out;
					bestContainer = container;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer != NONE)
		G_ClientInvMove(player, entnum, bestContainer, x, y, hand, 0, 0, qtrue, quiet);
}

/**
 * @brief Report and handle death of an actor
 */
void G_ActorDie (edict_t * ent, int state)
{
	assert(ent);
#ifdef DEBUG
	if (!ent)
		return;	/* never reached. need for code analyst. */
#endif

	Com_DPrintf("G_ActorDie: kill actor on team %i\n", ent->team);
	switch (state) {
	case STATE_DEAD:
		ent->state |= (1 + rand() % MAX_DEATH);
		break;
	case STATE_STUN:
		ent->STUN = 0;
		ent->state = state;
		break;
	default:
		gi.dprintf("G_ActorDie: unknown state %i\n", state);
		break;
	}
	VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD);
	gi.linkentity(ent);
	level.num_alive[ent->team]--;
	/* send death */
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_DIE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);

	/* handle inventory - drop everything to floor edict (but not the armor) */
	G_InventoryToFloor(ent);

	/* check if the player appears/perishes, seen from other teams */
	G_CheckVis(ent, qtrue);

	/* calc new vis for this player */
	G_CheckVisTeam(ent->team, NULL, qfalse);
}

/**
 * @brief This function does not add statistical values. Because there is no attacker.
 * The same counts for morale states - they are not affected.
 * @note: This is a debug function to let a hole team die
 */
void G_KillTeam (void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.argc() == 2)
		teamToKill = atoi(gi.argv(1));

	Com_DPrintf("G_KillTeam: kill team %i\n", teamToKill);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD)) {
			if (teamToKill >= 0 && ent->team != teamToKill)
				continue;

			/* die */
			G_ActorDie(ent, STATE_DEAD);
		}

	/* check for win conditions */
	G_CheckEndGame();
}



/**
 * @brief The client sent us a message that he did something. We now execute the related fucntion(s) adn notify him if neccessary.
 * @param[in] player The player that sent us the message (@todo: is this correct?)
 */
void G_ClientAction (player_t * player)
{
	int action;
	int num;
	pos3_t pos;
	int i;
	int firemode;
	int from, fx, fy, to, tx, ty;
	int hand, fd_idx;

	/* read the header */
	action = gi.ReadByte();
	num = gi.ReadShort();
	switch (action) {
	case PA_NULL:
		/* do nothing on a null action */
		break;

	case PA_TURN:
		gi.ReadFormat(pa_format[PA_TURN], &i);
		G_ClientTurn(player, num, i);
		break;

	case PA_MOVE:
		gi.ReadFormat(pa_format[PA_MOVE], &pos);
		G_ClientMove(player, player->pers.team, num, pos, qtrue, NOISY);
		break;

	case PA_STATE:
		gi.ReadFormat(pa_format[PA_STATE], &i);
		G_ClientStateChange(player, num, i);
		break;

	case PA_SHOOT:
		gi.ReadFormat(pa_format[PA_SHOOT], &pos, &i, &firemode);
		(void)G_ClientShoot(player, num, pos, i, firemode, NULL, qtrue);
		break;

	case PA_INVMOVE:
		gi.ReadFormat(pa_format[PA_INVMOVE], &from, &fx, &fy, &to, &tx, &ty);
		G_ClientInvMove(player, num, from, fx, fy, to, tx, ty, qtrue, NOISY);
		break;

	case PA_REACT_SELECT:
		hand = -1;
		fd_idx = -1;
		gi.ReadFormat(pa_format[PA_REACT_SELECT], &hand, &fd_idx);
		Com_DPrintf("G_ClientAction: entnum:%i hand:%i fd:%i\n", num, hand, fd_idx);
		/* @todo: Add check for correct player here (player==g_edicts[num]->team ???) */
		reactionFiremode[num][RF_HAND] = hand;
		reactionFiremode[num][RF_FM] = fd_idx;
		break;

	default:
		gi.error("G_ClientAction: Unknown action!\n");
		break;
	}
}


/**
 * @brief Sets the teanum var for this match
 * @param[in] player Pointer to connected player
 * @todo: Check whether there are enough free spawnpoints in all cases
 */
void G_GetTeam (player_t * player)
{
	player_t *p;
	int i, j;
	int playersInGame = 0;

	/* number of currently connected players (no ai players) */
	for (j = 0, p = game.players; j < game.maxplayers; j++, p++)
		if (p->inuse && !p->pers.spectator)
			playersInGame++;

	/* player has already a team */
	if (player->pers.team) {
		gi.dprintf("You are already on team %i\n", player->pers.team);
		return;
	}

	/* randomly assign a teamnumber in deathmatch games */
	if (playersInGame <= 1 && sv_maxclients->integer > 1 && !sv_teamplay->integer) {
		int spawnCheck[MAX_TEAMS];
		int spawnSpots = 0;
		int randomSpot = -1;
		for (i = 1; i < MAX_TEAMS; i++)
			if (level.num_spawnpoints[i])
				spawnCheck[spawnSpots++] = i;
		if (spawnSpots <= 1) {
			gi.dprintf("G_GetTeam: Not enough spawn spots in map!\n");
			player->pers.team = -1;
			return;
		}
		/* assign random valid team number */
		randomSpot = (int)(frand() * (spawnSpots - 1) + 0.5);
		player->pers.team = spawnCheck[randomSpot];
		gi.dprintf("You have been randomly assigned to team %i\n", player->pers.team);
		return;
	}

	/* find a team */
	if (sv_maxclients->integer == 1)
		player->pers.team = TEAM_PHALANX;
	else if (atoi(Info_ValueForKey(player->pers.userinfo, "spectator"))) {
		/* @todo: spectators get in a game menu to select the team */
		player->pers.spectator = qtrue;
	} else if (sv_teamplay->integer) {
		/* set the team specified in the userinfo */
		gi.dprintf("Get a team for teamplay for %s\n", player->pers.netname);
		i = atoi(Info_ValueForKey(player->pers.userinfo, "teamnum"));
		/* civilians are at team zero */
		if (i > 0 && sv_maxteams->integer >= i) {
			player->pers.team = i;
			gi.bprintf(PRINT_CHAT, "serverconsole: %s has chosen team %i\n", player->pers.netname, i);
		} else {
			gi.dprintf("Team %i is not valid - choose a team between 1 and %i\n", i, sv_maxteams->integer);
			player->pers.team = DEFAULT_TEAMNUM;
		}
	} else {
		qboolean teamAvailable;
		/* search team */
		gi.dprintf("Getting a multiplayer team for %s\n", player->pers.netname);
		for (i = 1; i < MAX_TEAMS; i++) {
			if (level.num_spawnpoints[i]) {
				teamAvailable = qtrue;
				/* check if team is in use (only human controlled players) */
				/* FIXME: If someone left the game and rejoins he should get his "old" team back */
				/*        maybe we could identify such a situation */
				for (j = 0, p = game.players; j < game.maxplayers; j++, p++)
					if (p->inuse && p->pers.team == i) {
						Com_DPrintf("Team %i is already in use\n", i);
						/* team already in use */
						teamAvailable = qfalse;
						break;
					}
				if (teamAvailable)
					break;
			}
		}

		/* set the team */
		if (i < MAX_TEAMS) {
			/* remove ai player */
			for (j = 0, p = game.players + game.maxplayers; j < game.maxplayers; j++, p++)
				if (p->inuse && p->pers.team == i) {
					gi.bprintf(PRINT_HIGH, "Removing ai player...");
					p->inuse = qfalse;
					break;
				}
			Com_DPrintf("Assigning %s to Team %i\n", player->pers.netname, i);
			player->pers.team = i;
		} else {
			/* @todo: disconnect the player or convert to spectator */
			player->pers.spectator = qtrue;
			player->pers.team = -1;
		}
	}
}

/**
 * @brief Returns the assigned team number of the player
 */
int G_ClientGetTeamNum (player_t * player)
{
	assert(player);
	return player->pers.team;
}

/**
 * @brief Returns the preferred team number for the player
 */
int G_ClientGetTeamNumPref (player_t * player)
{
	assert(player);
	return atoi(Info_ValueForKey(player->pers.userinfo, "teamnum"));
}

/**
 * @brief Tests if all teams are connected for a multiplayer match and if so,
 * randomly assigns the first turn to a team.
 * @sa SVCmd_StartGame_f
 */
void G_ClientTeamAssign (player_t * player)
{
	int i, j, teamCount = 1;
	int playerCount = 0;
	int knownTeams[MAX_TEAMS];
	player_t *p;
	knownTeams[0] = player->pers.team;

	/* return with no action if activeTeam already assigned or if in single-player mode */
	if (level.activeTeam != -1 || sv_maxclients->integer == 1)
		return;

	/* count number of currently connected unique teams and players (human controlled players only) */
	for (i = 0, p = game.players; i < game.maxplayers; i++, p++) {
		if (p->inuse && !p->pers.spectator && p->pers.team > 0) {
			playerCount++;
			for (j = 0; j < teamCount; j++) {
				if (p->pers.team == knownTeams[j])
					break;
			}
			if (j == teamCount)
				knownTeams[teamCount++] = p->pers.team;
		}
	}
	Com_DPrintf("G_ClientTeamAssign: Players in game: %i, Unique teams in game: %i\n", playerCount, teamCount);

	/* if all teams/players have joined the game, randomly assign which team gets the first turn */
	if ((!sv_teamplay->integer && teamCount >= sv_maxteams->integer) ||
		(sv_teamplay->integer && playerCount >= sv_maxclients->integer)) {
		char buffer[MAX_VAR] = "";
		level.activeTeam = knownTeams[(int)(frand() * (teamCount - 1) + 0.5)];
		turnTeam = level.activeTeam;
		for (i = 0, p = game.players; i < game.maxplayers; i++, p++) {
			if (p->inuse && !p->pers.spectator && p->pers.team == level.activeTeam) {
				Q_strcat(buffer, p->pers.netname, sizeof(buffer));
				Q_strcat(buffer, " ", sizeof(buffer));
			}
		}
		gi.bprintf(PRINT_HIGH, _("Team %i ( %s) will get the first turn.\n"), turnTeam, buffer);
	}
}

/**
 * @brief
 * @sa CL_SendTeamInfo
 * @todo: Check size (fieldSize here)
 */
void G_ClientTeamInfo (player_t * player)
{
	edict_t *ent;
	int i, j, k, length;
	int container, x, y;
	item_t item;

	/* find a team */
	G_GetTeam(player);
	length = gi.ReadByte();

	/* find actors */
	for (j = 0, ent = g_edicts; j < globals.num_edicts; j++, ent++)
		if ((ent->type == ET_ACTORSPAWN || ent->type == ET_UGVSPAWN)
			&& player->pers.team == ent->team)
			break;

	for (i = 0; i < length; i++) {
		if (j < globals.num_edicts && i < maxsoldiersperplayer->integer) {
			/* here the actors actually spawn */
			level.num_alive[ent->team]++;
			level.num_spawned[ent->team]++;
			ent->pnum = player->num;
			Com_DPrintf("Team %i - player: %i\n", ent->team, player->num);
			ent->chr.fieldSize = gi.ReadByte();
			ent->fieldSize = ent->chr.fieldSize;
			switch (ent->fieldSize) {
			case ACTOR_SIZE_NORMAL:
				ent->type = ET_ACTOR;
				break;
			case ACTOR_SIZE_UGV:
				ent->type = ET_UGV;
				ent->morale = 100;
				break;
			default:
				gi.dprintf("G_ClientTeamInfo: unknown edict fieldSize (%i)\n", ent->fieldSize);
				break;
			}
			gi.linkentity(ent);

			/* model */
			ent->chr.ucn = gi.ReadShort();
			Q_strncpyz(ent->chr.name, gi.ReadString(), sizeof(ent->chr.name));
			Q_strncpyz(ent->chr.path, gi.ReadString(), sizeof(ent->chr.path));
			Q_strncpyz(ent->chr.body, gi.ReadString(), sizeof(ent->chr.body));
			Q_strncpyz(ent->chr.head, gi.ReadString(), sizeof(ent->chr.head));
			ent->chr.skin = gi.ReadByte();
#if 0
			gi.dprintf("G_ClientTeamInfo: name: %s\n", ent->chr.name);
			gi.dprintf("G_ClientTeamInfo: path: %s\n", ent->chr.path);
			gi.dprintf("G_ClientTeamInfo: body: %s\n", ent->chr.body);
			gi.dprintf("G_ClientTeamInfo: head: %s\n", ent->chr.head);
			gi.dprintf("G_ClientTeamInfo: skin: %i\n", ent->chr.skin);
#endif

			ent->chr.HP = gi.ReadShort();
			ent->chr.maxHP = gi.ReadShort();
			ent->chr.STUN = gi.ReadByte();
			ent->chr.AP = gi.ReadByte();
			ent->chr.morale = gi.ReadByte();

			/* new attributes */
			for (k = 0; k < SKILL_NUM_TYPES; k++)
				ent->chr.skills[k] = gi.ReadByte();

			/* scores */
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				ent->chr.kills[k] = gi.ReadShort();
			ent->chr.assigned_missions = gi.ReadShort();

			/* inventory */
			{
				int nr = gi.ReadShort() / 6;

				for (; nr-- > 0;) {
					G_ReadItem (&item, &container, &x, &y);
					/* gi.dprintf("G_ClientTeamInfo: t=%i:a=%i:m=%i (x=%i:y=%i)\n", item.t, item.a, item.m, x, y); */

					Com_AddToInventory(&ent->i, item, container, x, y);
					/* gi.dprintf("G_ClientTeamInfo: add %s to inventory (container %i - idArmor: %i)\n", gi.csi->ods[ent->i.c[container]->item.t].id, container, gi.csi->idArmor); */
				}
			}

			/* set models */
			ent->chr.inv = &ent->i;
			ent->body = gi.modelindex(Com_CharGetBody(&ent->chr));
			ent->head = gi.modelindex(Com_CharGetHead(&ent->chr));
			ent->skin = ent->chr.skin;

			/* set initial vital statistics */
			ent->HP = ent->chr.HP;
			ent->morale = ent->chr.morale;

			/* FIXME: for now, heal fully upon entering mission */
			ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);

			ent->reaction_minhit = 30; /* @todo: allow later changes from GUI */
		} else {
			/* just do nothing with the info */
			gi.ReadByte(); /* fieldSize */
			gi.ReadShort(); /* ucn */
			for (k = 0; k < 4; k++)
				gi.ReadString(); /* name, path, body, head */
			gi.ReadByte(); /* skin */
			gi.ReadShort(); /* HP */
			gi.ReadShort(); /* maxHP */
			gi.ReadByte(); /* STUN */
			gi.ReadByte(); /* AP */
			gi.ReadByte(); /* morale */
			for (k = 0; k < SKILL_NUM_TYPES; k++)
				gi.ReadByte(); /* skills */
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				gi.ReadShort(); /* kills */
			gi.ReadShort(); /* assigned missions */
			j = gi.ReadShort();
			/* @todo: skip j bytes instead of reading and ignoring */
			for (k = 0; k < j; k++)
				gi.ReadByte(); /* inventory */
		}

		/* find actors */
		for (; j < globals.num_edicts; j++, ent++)
			if ((ent->type == ET_ACTORSPAWN || ent->type == ET_UGVSPAWN)
			&& player->pers.team == ent->team)
				break;
	}
	G_ClientTeamAssign(player);
}

/**
 * @brief Counts the still living actors for a player
 */
static int G_PlayerSoldiersCount (player_t* player)
{
	int i, cnt = 0;
	edict_t *ent;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && !(ent->state & STATE_DEAD) && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->pnum == player->num)
			cnt++;

	return cnt;
}

/**
 * @brief Check whether a forced round end should be executed
 */
void G_ForceEndRound (void)
{
	player_t *p;
	int i, diff;

	/* check for roundlimits in multiplayer only */
	if (!sv_roundtimelimit->integer || sv_maxclients->integer == 1)
		return;

	if (level.time != ceil(level.time))
		return;

	diff = level.roundstartTime + sv_roundtimelimit->integer - level.time;
	switch (diff) {
	case 240:
		gi.bprintf(PRINT_HUD, _("4 minutes left until forced round end\n"));
		return;
	case 180:
		gi.bprintf(PRINT_HUD, _("3 minutes left until forced round end\n"));
		return;
	case 120:
		gi.bprintf(PRINT_HUD, _("2 minutes left until forced round end\n"));
		return;
	case 60:
		gi.bprintf(PRINT_HUD, _("1 minute left until forced round end\n"));
		return;
	case 30:
		gi.bprintf(PRINT_HUD, _("30 seconds left until forced round end\n"));
		return;
	case 15:
		gi.bprintf(PRINT_HUD, _("15 seconds left until forced round end\n"));
		return;
	}

	/* active team still has time left */
	if (level.time < level.roundstartTime + sv_roundtimelimit->integer)
		return;

	gi.bprintf(PRINT_HUD, _("Current active team hit the max round time\n"));

	/* set all team members to ready (only human players) */
	for (i = 0, p = game.players; i < game.maxplayers; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam) {
			G_ClientEndRound(p, NOISY);
			level.nextEndRound = level.framenum;
		}

	level.roundstartTime = level.time;
}

/**
 * @brief
 * @sa G_PlayerSoldiersCount
 */
void G_ClientEndRound (player_t * player, qboolean quiet)
{
	player_t *p;
	qboolean sanity = qfalse;
	int i, lastTeam, nextTeam;

	/* inactive players can't end their inactive round :) */
	if (level.activeTeam != player->pers.team)
		return;

	/* check for "team oszillation" */
	if (level.framenum < level.nextEndRound)
		return;
	level.nextEndRound = level.framenum + 20;

	/* check if all team members are ready (even ai players) */
	player->ready = qtrue;
	for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam
			&& !p->ready && G_PlayerSoldiersCount(p) > 0)
			return;

	/* clear any remaining reaction fire */
	G_ReactToEndTurn();

	/* give his actors their TUs */
	G_GiveTimeUnits(level.activeTeam);

	/* let all the invisible players perish now */
	G_CheckVisTeam(level.activeTeam, NULL, qtrue);

	lastTeam = player->pers.team;
	level.activeTeam = -1;

	p = NULL;
	while (level.activeTeam == -1) {
		/* search next team */
		nextTeam = -1;

		for (i = lastTeam + 1; i != lastTeam; i++) {
			if (i >= MAX_TEAMS) {
				if (!sanity)
					sanity = qtrue;
				else {
					Com_Printf("Not enough spawn positions in this map\n");
					break;
				}
				i = 0;
			}

			if (((level.num_alive[i] || (level.num_spawnpoints[i] && !level.num_spawned[i]))
				 && i != lastTeam)) {
				nextTeam = i;
				break;
			}
		}

		if (nextTeam == -1) {
/*			gi.bprintf(PRINT_HIGH, "Can't change round - no living actors left.\n"); */
			level.activeTeam = lastTeam;
			gi.EndEvents();
			return;
		}

		/* search corresponding player (even ai players) */
		for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
			if (p->inuse && p->pers.team == nextTeam) {
				/* found next player */
				level.activeTeam = nextTeam;
				break;
			}

		if (level.activeTeam == -1 && sv_ai->integer && ai_autojoin->integer) {
			/* no corresponding player found - create ai player */
			p = AI_CreatePlayer(nextTeam);
			if (p)
				level.activeTeam = nextTeam;
		}

		lastTeam = nextTeam;
	}
	turnTeam = level.activeTeam;

	/* communicate next player in row to clients */
	gi.AddEvent(PM_ALL, EV_ENDROUND);
	gi.WriteByte(level.activeTeam);

	/* store the round start time to be able to abort the round after a give time */
	level.roundstartTime = level.time;

	/* apply morale behaviour, reset reaction fire */
	G_ResetReactionFire(level.activeTeam);
	if (mor_panic->value)
		G_MoraleBehaviour(level.activeTeam, quiet);

	/* start ai */
	p->pers.last = NULL;

	/* finish off events */
	gi.EndEvents();

	/* reset ready flag (even ai players) */
	for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam)
			p->ready = qfalse;
}

/**
 * @brief
 * @sa CL_EntEdict
 */
static void G_SendVisibleEdicts (void)
{
	int i;
	edict_t *ent;
	qboolean end = qfalse;

	/* make every edict visible thats not an actor or an ugv */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++) {
		/* don't add actors here */
		if (!ent->inuse)
			continue;
		if (ent->type == ET_BREAKABLE || ent->type == ET_DOOR) {
			gi.AddEvent(~G_VisToPM(ent->visflags), EV_ENT_EDICT);
			gi.WriteShort(ent->type);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->modelindex);
			ent->visflags |= ~ent->visflags;
			end = qtrue;
		}
	}

	if (end)
		gi.EndEvents();
}

/**
 * @brief This functions starts the client
 * @sa G_ClientSpawn
 * @sa CL_StartGame
 */
void G_ClientBegin (player_t* player)
{
	/* this doesn't belong here, but it works */
	if (!level.routed) {
		level.routed = qtrue;
		G_CompleteRecalcRouting();
	}

	/* FIXME: This should be a client side error */
	if (!P_MASK(player)) {
		gi.bprintf(PRINT_HIGH, "%s tried to join - but server is full\n", player->pers.netname);
		return;
	}

	level.numplayers++;

	/*Com_Printf("G_ClientBegin: player: %i - pnum: %i , game.maxplayers: %i	\n", P_MASK(player), player->num, game.maxplayers);*/
	/* spawn camera (starts client rendering) */
	gi.AddEvent(P_MASK(player), EV_START | INSTANTLY);
	gi.WriteByte(sv_teamplay->integer);

	/* send events */
	gi.EndEvents();

	/* set the net name */
	gi.configstring(CS_PLAYERNAMES + player->num, player->pers.netname);

	/* inform all clients */
	gi.bprintf(PRINT_HIGH, "%s has joined team %i\n", player->pers.netname, player->pers.team);
}

/**
 * @brief Sets the team, init the TU and sends the player stats. Returns
 * true if player spawns.
 * @sa G_SendPlayerStats
 * @sa G_GetTeam
 * @sa G_GiveTimeUnits
 * @sa G_ClientBegin
 * @sa CL_Reset
 */
qboolean G_ClientSpawn (player_t * player)
{
	/* @todo: Check player->pers.team here */
	if (level.activeTeam == -1) {
		/* activate round if in single-player */
		if (sv_maxclients->integer == 1) {
			level.activeTeam = player->pers.team;
			turnTeam = level.activeTeam;
		} else {
			/* return since not all multiplayer teams have joined */
			/* (G_ClientTeamAssign sets level.activeTeam once all teams have joined) */
			return qfalse;
		}
	}

	/* do all the init events here... */
	/* reset the data */
	gi.AddEvent(P_MASK(player), EV_RESET | INSTANTLY);
	gi.WriteByte(player->pers.team);
	gi.WriteByte(level.activeTeam);

	/* show visible actors and submit stats */
	G_ClearVisFlags(player->pers.team);
	G_CheckVis(NULL, qfalse);
	G_SendPlayerStats(player);

	/* send things like doors and breakables */
	G_SendVisibleEdicts();

	/* give time units */
	G_GiveTimeUnits(player->pers.team);

	/* send events */
	gi.EndEvents();

	/* inform all clients */
	gi.bprintf(PRINT_HIGH, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team);
	return qtrue;
}


/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged (player_t * player, char *userinfo)
{
	char *s;

	/* check for malformed or illegal info strings */
	if (!Info_Validate(userinfo))
		Q_strncpyz(userinfo, "\\name\\badinfo", sizeof(userinfo));

	/* set name */
	s = Info_ValueForKey(userinfo, "name");
	Q_strncpyz(player->pers.netname, s, sizeof(player->pers.netname));

	/* set spectator */
	s = Info_ValueForKey(userinfo, "spectator");
	if (*s == '1')
		player->pers.spectator = qtrue;
	else
		player->pers.spectator = qfalse;

	Q_strncpyz(player->pers.userinfo, userinfo, sizeof(player->pers.userinfo));

	/* send the updated config string */
	gi.configstring(CS_PLAYERNAMES + player->num, player->pers.netname);
}


/**
 * @brief
 */
qboolean G_ClientConnect (player_t * player, char *userinfo)
{
	char *value;

	/* check to see if they are on the banned IP list */
	value = Info_ValueForKey(userinfo, "ip");
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return qfalse;
	}

	value = Info_ValueForKey(userinfo, "password");
	if (*password->string && Q_strcmp(password->string, "none")
	 && Q_strcmp(password->string, value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
		return qfalse;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->inuse) {
		gi.bprintf(PRINT_HIGH, "%s already in use.\n", player->pers.netname);
		G_ClientDisconnect(player);
	}

	/* reset persistant data */
	memset(&player->pers, 0, sizeof(client_persistant_t));
	G_ClientUserinfoChanged(player, userinfo);

	gi.bprintf(PRINT_HIGH, "%s is connecting...\n", Info_ValueForKey(userinfo, "name"));
	return qtrue;
}

/**
 * @brief
 */
void G_ClientDisconnect (player_t * player)
{
	level.numplayers--;

	if (level.activeTeam == player->pers.team)
		G_ClientEndRound(player, NOISY);

	gi.bprintf(PRINT_HIGH, "%s disconnected.\n", player->pers.netname);
}
