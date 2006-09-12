/**
 * @file g_client.c
 * @brief Main part of the game logic.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

/* Timeunits for the various actions. */
#define TU_REACTION		7
#define TU_CROUCH		1
#define TU_TURN		1

/*
 * 0: Stores the used TUs for Reaction fire for each edict.
 * 1: Stores if the edict has fired in rection.
*/
static int TU_REACTIONS[MAX_EDICTS][2];

/**
 * @brief Generates the player mask
 */
int G_TeamToPM(int team)
{
	player_t *player;
	int player_mask, i;

	player_mask = 0;

	for (i = 0, player = game.players; i < game.maxplayers; i++, player++)
		if (player->inuse && team == player->pers.team)
			player_mask |= (1 << i);

	return player_mask;
}


/**
 * @brief Converts vis mask to player mask
 */
int G_VisToPM(int vis_mask)
{
	player_t *player;
	int player_mask, i;

	player_mask = 0;

	for (i = 0, player = game.players; i < game.maxplayers; i++, player++)
		if (player->inuse && (vis_mask & (1 << player->pers.team)))
			player_mask |= (1 << i);

	return player_mask;
}


/**
 * @brief Send stats to network buffer
 */
void G_SendStats(edict_t * ent)
{
	/* extra sanity check for ent->TU */
	if (ent->TU < 0)
		ent->TU = 0;

	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteByte(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->AP);
	gi.WriteByte(ent->morale);
}


/**
 * @brief Write an item to the network buffer
 */
void G_WriteItem(item_t item, int container, int x, int y)
{
	assert (item.t != NONE);

	gi.WriteFormat("bbbbbb",
				   item.t, item.a, item.m, container, x, y);
}

/**
 * @brief Read item from the network buffer
 */
void G_ReadItem(item_t * item, int * container, int * x, int * y)
{
	gi.ReadFormat("bbbbbb",
				   &item->t, &item->a, &item->m, container, x, y);
}


/**
 * @brief Write player stats to network buffer
 * @sa G_SendStats
 */
void G_SendPlayerStats(player_t * player)
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
void G_GiveTimeUnits(int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && !(ent->state & STATE_DEAD) && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == team) {
			ent->TU = GET_TU(ent->chr.skills[ABILITY_SPEED]);
			G_SendStats(ent);
		}
}


/**
 * @brief Stores the TUs for reaction fire that are used (if any).
 * @note Normally called on end of turn.
 * @todo Comment on the AddEvent code.
 * @sa G_ClientStateChange
 * @param[in] team Index of team to loop through.
 */
void G_ResetReactionFire(int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && !(ent->state & STATE_DEAD) && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == team) {
			TU_REACTIONS[ent->number][1]  = 0;	/* reset 'RF used' flag */
			if ( ent->state & STATE_REACTION ) {
				if (ent->TU >= TU_REACTION ) {
					/* Enough TUs for reaction fire available. */
					ent->TU -= TU_REACTION;
					TU_REACTIONS[ent->number][0] = TU_REACTION;	/* Save the used TUs for possible later re-adding. */
				} else if (ent->TU > 0) {
					/* Not enough TUs for reaction fire available. */
					TU_REACTIONS[ent->number][0] = ent->TU;	/* Save the used TUs for possible later re-adding. */
					ent->TU = 0;
				} else {
					/* No TUs at all available. */
					TU_REACTIONS[ent->number][0] = -1;
				}
			} else {
				TU_REACTIONS[ent->number][0] = 0;	/* Reset saved TUs. */
			}
			ent->state &= ~STATE_SHAKEN;
			gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->state);
		}
}


/**
 * @brief
 */
void G_SendState(int player_mask, edict_t * ent)
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
 */
void G_SendInventory(int player_mask, edict_t * ent)
{
	invList_t *ic;
	unsigned short nr = 0;
	int j;

	/* test for pointless player mask */
	if (!player_mask)
		return;

	gi.AddEvent(player_mask, EV_INV_ADD);

	gi.WriteShort(ent->number);

	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next)
			nr++;

	/* size of inventory */
	gi.WriteShort(nr*6);
	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next) {
			/* send a single item */
			G_WriteItem(ic->item, j, ic->x, ic->y);
		}
}


/**
 * @brief
 */
void G_AppearPerishEvent(int player_mask, int appear, edict_t * check)
{
	if (appear) {
		/* appear */
		switch (check->type) {
		case ET_ACTOR:
		case ET_UGV:
			/* parsed in CL_ActorAppear */
			gi.AddEvent(player_mask, EV_ACTOR_APPEAR);
			gi.WriteShort(check->number);
			gi.WriteByte(check->team);
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
 * @brief Checks whether a point is visible from the edicts position
 */
qboolean G_FrustomVis(edict_t * from, vec3_t point)
{
	/* view frustom check */
	vec3_t delta;
	byte dv;

	delta[0] = point[0] - from->origin[0];
	delta[1] = point[1] - from->origin[1];
	delta[2] = 0;
	VectorNormalize(delta);
	dv = from->dir & 7;

	/* test 120 frustom (cos 60 = 0.5) */
	if ((delta[0] * dvecsn[dv][0] + delta[1] * dvecsn[dv][1]) < 0.5)
		return qfalse;
	else
		return qtrue;
}


/**
 * @brief
 */
qboolean G_TeamPointVis(int team, vec3_t point)
{
	edict_t *from;
	vec3_t eye;
	int i;

	/* test if check is visible */
	for (i = 0, from = g_edicts; i < globals.num_edicts; i++, from++)
		if (from->inuse && (from->type == ET_ACTOR || from->type == ET_UGV) && !(from->state & STATE_DEAD) && from->team == team && G_FrustomVis(from, point)) {
			/* get viewers eye height */
			VectorCopy(from->origin, eye);
			if (from->state & (STATE_CROUCHED | STATE_PANIC))
				eye[2] += EYE_CROUCH;
			else
				eye[2] += EYE_STAND;

			/* line of sight */
			if (!gi.TestLine(eye, point))
				return qtrue;
		}

	/* not visible */
	return qfalse;
}


/**
 * @brief
 */
float G_ActorVis(vec3_t from, edict_t * check, qboolean full)
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
		if (!gi.TestLine(from, test)) {
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
 * @brief
 */
float G_Vis(int team, edict_t * from, edict_t * check, int flags)
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
		return !gi.TestLine(eye, check->origin);
	default:
		return 0.0;
	}
}


/**
 * @brief
 */
int G_TestVis(int team, edict_t * check, int flags)
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
 */
int G_CheckVisTeam(int team, edict_t * check, qboolean perish)
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
 */
int G_CheckVis(edict_t * check, qboolean perish)
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
void G_ClearVisFlags(int team)
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
int G_DoTurn(edict_t * ent, byte toDV)
{
	float angleDiv;
	const byte *rot;
	int i, num;
	int status;

	assert(ent->dir <= 7);
#ifdef DEBUG
	if (ent->dir > 7)
		return 0;	/* never reached. need for code analyst. */
#endif

	/* return if no rotation needs to be done */
	if ((ent->dir) == (toDV & 7))
		return 0;

	/* calculate angle difference */
	angleDiv = dangle[toDV & 7] - dangle[ent->dir];
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
 * @brief
 * @todo: Integrate into hud - don´t use cprintf
 */
qboolean G_ActionCheck(player_t * player, edict_t * ent, int TU)
{
	/* a generic tester if an action could be possible */
	if (level.activeTeam != player->pers.team) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - this isn't your round!\n"));
		return qfalse;
	}

	if (!ent || !ent->inuse) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - object not present!\n"));
		return qfalse;
	}

	if (ent->type != ET_ACTOR && ent->type != ET_UGV) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not an actor!\n"));
		return qfalse;
	}

	if (ent->state & STATE_STUN) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - actor is stunned!\n"));
		return qfalse;
	}

	if (ent->state & STATE_DEAD) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - actor is dead!\n"));
		return qfalse;
	}

	if (ent->team != player->pers.team) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not on same team!\n"));
		return qfalse;
	}

	if (ent->pnum != player->num) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - no control over allied actors!\n"));
		return qfalse;
	}

	if (TU > ent->TU) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not enough TUs!\n"));
		return qfalse;
	}

	/* could be possible */
	return qtrue;
}


/**
 * @brief
 */
edict_t *G_SpawnFloor(pos3_t pos)
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
 */
edict_t *G_GetFloorItems(edict_t * ent)
{
	edict_t *floor;

	for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
		if (!floor->inuse || floor->type != ET_ITEM)
			continue;
		if (!VectorCompare(ent->pos, floor->pos))
			continue;

		/* found items */
		FLOOR(ent) = FLOOR(floor);
		return floor;
	}

	/* no items on ground found */
	FLOOR(ent) = NULL;
	return NULL;
}


/**
 * @brief
 * @sa event PA_INVMOVE
 * @sa AI_ActorThink
 */
void G_ClientInvMove(player_t * player, int num, int from, int fx, int fy, int to, int tx, int ty, qboolean checkaction)
{
	edict_t *ent, *floor;
	invList_t *ic;
	qboolean newFloor;
	item_t item;
	int mask;
	int ia;

	ent = g_edicts + num;

	/* check if action is possible */
	if (checkaction && !G_ActionCheck(player, ent, 1))
		return;

	/* "get floor ready" */
	floor = G_GetFloorItems(ent);
	if (to == gi.csi->idFloor && !floor) {
		floor = G_SpawnFloor(ent->pos);
		newFloor = qtrue;
	} else {
		newFloor = qfalse;
	}

	/* search for space */
	if (tx == NONE) {
		assert (ty == NONE);
		ic = Com_SearchInInventory(&ent->i, from, fx, fy);
		if (ic)
			Com_FindSpace(&ent->i, ic->item.t, to, &tx, &ty);
	}
	if (tx == NONE) {
		assert (ty == NONE);
		return;
	}

	if ( ( ia = Com_MoveInInventory(&ent->i, from, fx, fy, to, tx, ty, &ent->TU, &ic) ) != 0 ) {
		switch (ia) {
		case IA_NOTIME:
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not enough TUs!\n"));
			return;
		case IA_NORELOAD:
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - weapon already fully loaded with the same ammunition!\n")); /* TODO: "or not researched" */
			return;
		}

		/* FIXME: This is impossible - if not we should check MAX_INVDEFS*/
		assert((gi.csi->idFloor >= 0) && (gi.csi->idFloor < MAX_CONTAINERS));
#ifdef DEBUG
		if ((gi.csi->idFloor < 0) || (gi.csi->idFloor >= MAX_CONTAINERS))
			return;	/* never reached. need for code analyst. */
#endif

		/* successful inventory change; remove the item in clients */
		if (from == gi.csi->idFloor) {
			assert (!newFloor);
			if ( FLOOR(ent) ) { /* floor not totally emptied */
				FLOOR(floor) = FLOOR(ent);
				gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_DEL);
				gi.WriteShort(floor->number);
				gi.WriteByte(from);
				gi.WriteByte(fx);
				gi.WriteByte(fy);
			} else {
				gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
				gi.WriteShort(floor->number);
				G_FreeEdict(floor);
			}
		} else {
			gi.AddEvent(G_TeamToPM(ent->team), EV_INV_DEL);
			gi.WriteShort(num);
			gi.WriteByte(from);
			gi.WriteByte(fx);
			gi.WriteByte(fy);
		}

		/* send tu's */
		G_SendStats(ent);

		item = ic->item;

		if (ia == IA_RELOAD || ia == IA_RELOAD_SWAP ) {
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
		}

		if (ia == IA_RELOAD) {
			gi.EndEvents();
			return;
		} else if (ia == IA_RELOAD_SWAP ) {
			to = from;
			tx = fx;
			ty = fy;
			item = Com_SearchInInventory(&ent->i, from, fx, fy)->item;
		}

		/* add it */
		if (to == gi.csi->idFloor) {
			if (newFloor) {
				assert (FLOOR(ent));
				FLOOR(floor) = FLOOR(ent);
				/* send item info to the clients */
				G_CheckVis(floor, qtrue);
			} else {
				/* add the item; update floor, because we add at beginning */
				FLOOR(floor) = FLOOR(ent);
				gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_ADD);
				gi.WriteShort(floor->number);
				gi.WriteShort(6);
				G_WriteItem(item, to, tx, ty);
			}
		} else {
			gi.AddEvent(G_TeamToPM(ent->team), EV_INV_ADD);
			gi.WriteShort(num);
			gi.WriteShort(6);
			G_WriteItem(item, to, tx, ty);
		}

		/* other players receive weapon info only */
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
}


/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 */
void G_InventoryToFloor(edict_t * ent)
{
	invList_t *ic, *next;
	edict_t *floor;
	int k;

	/* check for items */
	for (k = 0; k < gi.csi->numIDs; k++)
		if (ent->i.c[k])
			break;

	if (k >= gi.csi->numIDs)
		return;

	/* find the floor */
	floor = G_GetFloorItems(ent);
	if (!floor) {
		floor = G_SpawnFloor(ent->pos);
	} else {
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
		/* now cycle through all items for the container of the character (or the entity) */
		for (ic = ent->i.c[k]; ic; ic = next) {
			int x, y;

			/* Save the next inv-list before it gets overwritten below.
			   Do not put this in the "for" statement,
			   unless you want an endless loop. ;) */
			next = ic->next;
			/* find the coordinates for the current item on floor */
			Com_FindSpace(&floor->i, ic->item.t, gi.csi->idFloor, &x, &y);
			if (x == NONE) {
				assert (y == NONE);
				/* Run out of space on the floor or the item is armor
				   --- destroy the offending item.
				   TODO: for items other than armor we should really
				   just spill into adjacent locations */
				if (Q_strncmp(gi.csi->ods[ic->item.t].type, "armor", MAX_VAR))
					gi.dprintf("G_InventoryToFloor: Warning: could not drop item to floor: %s\n", gi.csi->ods[ic->item.t].kurz);
				if (!Com_RemoveFromInventory(&ent->i, k, ic->x, ic->y))
					gi.dprintf("G_InventoryToFloor: Error: could not remove item: %s\n", gi.csi->ods[ic->item.t].kurz);
			} else {
				ic->x = x;
				ic->y = y;
				ic->next = FLOOR(floor);
				FLOOR(floor) = ic;
#ifdef PARANOID
				gi.dprintf("G_InventoryToFloor: item to floor: %s\n", gi.csi->ods[ic->item.t].kurz);
#endif
			}
		}
		/* destroy link */
		ent->i.c[k] = NULL;
	}

	FLOOR(ent) = FLOOR(floor);

	/* send item info to the clients */
	G_CheckVis(floor, qtrue);
}


byte *fb_list[MAX_FB_LIST];
int fb_length;

/**
 * @brief
 */
void G_BuildForbiddenList(int team)
{
	edict_t *ent;
	int vis_mask;
	int i;

	fb_length = 0;
	if (team)
		vis_mask = 1 << team;
	else
		vis_mask = 0xFFFFFFFF;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) && (ent->visflags & vis_mask)) {
			/* TODO: Implement fieldSize */
			switch(ent->fieldSize) {
			default:
				fb_list[fb_length++] = ent->pos;
			}
		}

	if (fb_length > MAX_FB_LIST)
		gi.error("G_BuildForbiddenList: list too long\n");
}

/**
 * @brief
 */
void G_MoveCalc(int team, pos3_t from, int distance)
{
	G_BuildForbiddenList(team);
	gi.MoveCalc(gi.map, from, distance, fb_list, fb_length);
}


/**
 * @brief
 */
static qboolean G_CheckMoveBlock(pos3_t from, int dv)
{
	edict_t *ent;
	pos3_t pos;
	int i;

	/* get target position */
	VectorCopy(from, pos);
	PosAddDV(pos, dv);

	/* search for blocker */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) && VectorCompare(pos, ent->pos))
			return qtrue;

	/* nothing found */
	return qfalse;
}


/**
 * @brief
 * @sa CL_ActorStartMove
 */
void G_ClientMove(player_t * player, int visTeam, int num, pos3_t to, qboolean stop)
{
	edict_t *ent;
	int length, status;
	byte dvtab[32];
	byte dv, numdv, steps;
	pos3_t pos;
	float div, tu;

	ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, 2))
		return;

	/* calculate move table */
	G_MoveCalc(visTeam, ent->pos, MAX_ROUTE);
	length = gi.MoveLength(gi.map, to, qfalse);

	if (length && length < 0xFF) {
		/* start move */
		gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_START_MOVE);
		gi.WriteShort(ent->number);

		/* assemble dv-encoded move data */
		VectorCopy(to, pos);
		numdv = 0;
		tu = 0;

		while ((dv = gi.MoveNext(gi.map, pos)) < 0xFF) {
			/* store the inverted dv */
			/* (invert by flipping the first bit and add the old height) */
			dvtab[numdv++] = ((dv ^ 1) & 7) | (pos[2] << 3);
			PosAddDV(pos, dv);
		}

		if (VectorCompare(pos, ent->pos)) {
			/* everything ok, found valid route */
			/* create movement related events */
			steps = 0;
			FLOOR(ent) = NULL;

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
				if (status)
					steps = 0;

				/* check for "blockers" */
				if (G_CheckMoveBlock(ent->pos, dvtab[numdv]))
					break;

				/* decrease TUs */
				div = ((dvtab[numdv] & 7) < 4) ? 2 : 3;
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
					gi.WriteNewSave(1);
				}

				/* write step */
				steps++;
				gi.WriteByte(dvtab[numdv]);
				gi.WriteToSave(steps);

				/*gi.dprintf( "move: (pm: %i)\n", ent->visflags>>1 ); */

				/* check if the player appears/perishes, seen from other teams */
				status = G_CheckVis(ent, qtrue);
				if (status)
					steps = 0;

				/* check for anything appearing, seen by "the moving one" */
				status = G_CheckVisTeam(ent->team, NULL, qfalse);
				if (status)
					steps = 0;

				/* check for reaction fire */
				if (G_ReactionFire(ent))
					steps = 0;

				/* check for death */
				if (ent->state & STATE_DEAD)
					return;

				if (stop && (status & VIS_STOP))
					break;
			}

			/* submit the TUs / round down */
			ent->TU -= (int) tu;
			G_SendStats(ent);

			/* end the move */
			G_GetFloorItems(ent);
			gi.EndEvents();
		}
	}
}


/**
 * @brief
 */
static void G_ClientTurn(player_t * player, int num, int dv)
{
	edict_t *ent;

	ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_TURN))
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
 * @param[in,out] player TODO Writeme
 * @param[in] num TODO Writeme
 * @param[in] newState The bit-map of the new states.
 */
static void G_ClientStateChange(player_t * player, int num, int newState)
{
	edict_t *ent;
	int changeState;

	ent = g_edicts + num;

	/* Check if any action is possible. */
	if (!G_ActionCheck(player, ent, 0))
		return;

	changeState = ent->state ^ newState;
	if (!changeState)
		return;

	if (changeState & STATE_CROUCHED)
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (G_ActionCheck(player, ent, TU_CROUCH)) {
			ent->state ^= STATE_CROUCHED;
			ent->TU -= TU_CROUCH;
			/* Link it. */
			if (ent->state & STATE_CROUCHED)
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
			else
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
			gi.linkentity(ent);
		}

	if (changeState & STATE_REACTION) {
		if (ent->state & STATE_REACTION_MANY) {
			if (ent->state & STATE_SHAKEN)
				gi.cprintf(player, PRINT_HIGH, _("Currently shaken, won't let it's guard down.\n"));
			else {
				/* Turn off reaction fire and give the soldier back his TUs if it used some. */
				ent->state &= ~STATE_REACTION;

				if ( TU_REACTIONS[ent->number][0] > 0) {
					/* TUs where used for activation. */
					ent->TU += TU_REACTIONS[ent->number][0];
				} else if ( TU_REACTIONS[ent->number][0] < 0) {
					/* No TUs where used for activation. */
					/* Don't give TUs back because none where used up (reaction fire was already active from previous turn) */
				} else {
					/* TU_REACTIONS[ent->number] == 0) */
					/* This should never be the case.  */
					Com_DPrintf("G_ClientStateChange: 0 value saved for reaction while reaction is activated.\n");
				}
			}
		} else if (ent->state & STATE_REACTION_ONCE) {
			ent->state &= ~STATE_REACTION;
			ent->state |= STATE_REACTION_MANY;
		} else if (G_ActionCheck(player, ent, TU_REACTION)) {
			/* Turn on reaction fire and save the used TUs to the list. */
			ent->state |= STATE_REACTION_ONCE;

			if ( TU_REACTIONS[ent->number][0] > 0) {
				/* TUs where saved for this turn (either the full TU_REACTION or some remaining TUs from the shot. This was done either in the last turn or this one. */
				ent->TU -= TU_REACTIONS[ent->number][0];
			} else if ( TU_REACTIONS[ent->number][0] == 0) {
				/* Reaction fire was not triggered in the last turn. */
				ent->TU -= TU_REACTION;
				TU_REACTIONS[ent->number][0] = TU_REACTION;
			}  else {
				/* TU_REACTIONS[ent->number][0] < 0 */
				/* Reaction fire was triggered in the last turn,
				   and has used 0 TU from this one.
				   Can be activated without TU-loss. */
			}
		}
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


typedef enum {
	ML_WOUND,
	ML_DEATH
} morale_modifiers;

#define MORALE_RANDOM( mod )	( (mod) * (1.0 + 0.3*crand()) )

/**
 * @brief Applies morale changes to actors around a wounded or killed actor
 * @note only called when mor_panic is not zero
 */
static void G_Morale(int type, edict_t * victim, edict_t * attacker, int param)
{
	edict_t *ent;
	int i, newMorale;
	float mod;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not ET_UGV */
		if (ent->inuse && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->team != TEAM_CIVILIAN) {
			switch (type) {
			case ML_WOUND:
			case ML_DEATH:
				/* morale damage is damage dependant */
				mod = mob_wound->value * param;
				/* death hurts morale even more than just damage */
				if (type == ML_DEATH)
					mod += mob_death->value;
				/* seeing how someone gets shot increases the morale change */
				if (ent == victim || (G_ActorVis(ent->origin, victim, qfalse) && G_FrustomVis(ent, victim->origin)))
					mod *= mof_watching->value;
				if (ent->team == attacker->team) {
					/* teamkills are considered to be bad form, but won't cause an increased morale boost for the enemy */
					/* morale boost isn't equal to morale loss (it's lower, but morale gets regenerated) */
					if (victim->team == attacker->team)
						mod *= mof_teamkill->value;
					else
						mod *= mof_enemy->value;
				}
				/* seeing a civi die is more "acceptable" */
				if (victim->team == TEAM_CIVILIAN)
					mod *= mof_civilian->value;
				/* if an ally (or in singleplayermode, as human, a civilian) got shot, lower the morale, don't heighten it. */
				if (victim->team == ent->team || (victim->team == TEAM_CIVILIAN && ent->team != TEAM_ALIEN && (int) sv_maxclients->value == 1))
					mod *= -1;
				/* if you stand near to the attacker or the victim, the morale change is higher. */
				mod *= mor_default->value + pow(0.5, VectorDist(ent->origin, victim->origin) / mor_distance->value)
					* mor_victim->value + pow(0.5, VectorDist(ent->origin, attacker->origin) / mor_distance->value)
					* mor_attacker->value;
				/* morale damage is dependant on the number of living allies */
				mod *= (1 - mon_teamfactor->value)
					+ mon_teamfactor->value * (level.num_spawned[victim->team] + 1)
					/ (level.num_alive[victim->team] + 1);
				/* being hit isn't fun */
				if (ent == victim)
					mod *= mor_pain->value;
				break;
			default:
				Com_Printf("Undefined morale modifier type %i\n", type);
				mod = 0;
			}
			/* clamp new morale */
			/*+0.9 to allow weapons like flamethrowers to inflict panic (typecast rounding) */
			newMorale = ent->morale + (int) (MORALE_RANDOM(mod) + 0.9);
			if (newMorale > GET_MORALE(ent->chr.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);
			else if (newMorale < 0)
				ent->morale = 0;
			else
				ent->morale = newMorale;

			/* send phys data */
			G_SendStats(ent);
		}
}

/**
 * @brief
 */
static void G_MoralePanic(edict_t * ent, qboolean sanity)
{
	gi.cprintf(game.players + ent->pnum, PRINT_HIGH, _("%s panics!\n"), ent->chr.name);

	/* drop items in hands */
	if (!sanity) {
		if (RIGHT(ent))
			G_ClientInvMove(game.players + ent->pnum, ent->number, gi.csi->idRight, 0, 0, gi.csi->idFloor, NONE, NONE, qtrue);
		if (LEFT(ent))
			G_ClientInvMove(game.players + ent->pnum, ent->number, gi.csi->idLeft, 0, 0, gi.csi->idFloor, NONE, NONE, qtrue);
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
 */
static void G_MoraleStopPanic(edict_t * ent)
{
	if (((ent->morale) / mor_panic->value) > (m_panic_stop->value * frand()))
		ent->state &= ~STATE_PANIC;
	else
		G_MoralePanic(ent, qtrue);
}

/**
 * @brief
 */
static void G_MoraleRage(edict_t * ent, qboolean sanity)
{
	if (sanity)
		ent->state |= STATE_RAGE;
	else
		ent->state |= STATE_INSANE;
	G_SendState(G_VisToPM(ent->visflags), ent);

	if (sanity)
		gi.bprintf(PRINT_HIGH, _("%s is on a rampage.\n"), ent->chr.name);
	else
		gi.bprintf(PRINT_HIGH, _("%s is in mad rage!\n"), ent->chr.name);
	AI_ActorThink(game.players + ent->pnum, ent);
}

/**
 * @brief Stops the rage state of an actor
 * @note This is only called when cvar mor_panic is not zero
 */
static void G_MoraleStopRage(edict_t * ent)
{
	if (((ent->morale) / mor_panic->value) > (m_rage_stop->value * frand())) {
		ent->state &= ~STATE_INSANE;
		G_SendState(G_VisToPM(ent->visflags), ent);
	} else
		G_MoralePanic(ent, qtrue);	/*regains sanity */
}

/**
 * @brief Applies morale behaviour on actors
 * @note only called when mor_panic is not zero
 */
static void G_MoraleBehaviour(int team)
{
	edict_t *ent;
	int i, newMorale;
	qboolean sanity;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not to ET_UGV */
		if (ent->inuse && ent->type == ET_ACTOR && ent->team == team && !(ent->state & STATE_DEAD)) {
			/* civilians have a 1:1 chance to randomly run away, will be changed: */
			if (level.activeTeam == TEAM_CIVILIAN && 0.5 > frand())
				G_MoralePanic(ent, qfalse);
			/* multiplayer needs enabled sv_enablemorale */
			/* singleplayer has this in every case */
			if (((int) sv_maxclients->value >= 2 && (int) sv_enablemorale->value == 1)
				|| (int) sv_maxclients->value == 1) {
				/* if panic, determine what kind of panic happens: */
				if (ent->morale <= mor_panic->value && !(ent->state & STATE_PANIC) && !(ent->state & STATE_RAGE)) {
					if ((float) ent->morale / mor_panic->value > (m_sanity->value * frand()))
						sanity = qtrue;
					else
						sanity = qfalse;
					if ((float) ent->morale / mor_panic->value > (m_rage->value * frand()))
						G_MoralePanic(ent, sanity);
					else
						G_MoraleRage(ent, sanity);
					/* if shaken, well .. be shaken; */
				} else if (ent->morale <= mor_shaken->value && !(ent->state & STATE_PANIC) && !(ent->state & STATE_RAGE)) {
					ent->TU -= TU_REACTION;
					/* shaken is later reset along with reaction fire */
					ent->state |= STATE_SHAKEN | STATE_REACTION_MANY;
					G_SendState(G_VisToPM(ent->visflags), ent);
					gi.cprintf(game.players + ent->pnum, PRINT_HIGH, _("%s is currently shaken.\n"), ent->chr.name);
				} else {
					if (ent->state & STATE_PANIC)
						G_MoraleStopPanic(ent);
					else if (ent->state & STATE_RAGE)
						G_MoraleStopRage(ent);
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
 * @brief
 */
static void G_Damage(edict_t * ent, int dmgtype, int damage, edict_t * attacker)
{
	int stun = dmgtype == gi.csi->damStun;

	assert (ent);
	assert (ent->type == ET_ACTOR
			|| ent->type == ET_UGV
			|| ent->type == ET_BREAKABLE);

	/* breakables are immune to stun damage */
	if (stun && ent->type == ET_BREAKABLE)
		return;

	/* breakables */
	if (ent->type == ET_BREAKABLE) {
		if (damage >= ent->HP) {
			gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE);
			gi.WriteShort(ent->mapNum);
			if (ent->particle && Q_strcmp(ent->particle, "null")) {
				/* TODO: Spawn the breaking particle here */
				gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
				gi.WriteShort(ent->spawnflags);
				gi.WriteGPos(ent->pos);
				gi.WriteShort((int)strlen(ent->particle));
				gi.WriteString(ent->particle);
			}
			gi.unlinkentity(ent);
			ent->inuse = qfalse;
			G_RecalcRouting(ent);
			G_FreeEdict(ent);
		} else
			ent->HP -= damage;
		return;
	}

	/* actors don't die again */
	if (ent->state & STATE_DEAD)
		return;

	/* apply difficulty settings */
	if (sv_maxclients->value == 1) {
		if (attacker->team == TEAM_ALIEN && ent->team < TEAM_ALIEN)
			damage *= pow(1.3, difficulty->value);
		else if (attacker->team < TEAM_ALIEN && ent->team == TEAM_ALIEN)
			damage *= pow(1.3, -difficulty->value);
	}

	/* apply armor effects */
	if (damage > 0 && ent->i.c[gi.csi->idArmor]) {
		objDef_t *ad;
		int totalDamage;

		ad = &gi.csi->ods[ent->i.c[gi.csi->idArmor]->item.t];

		totalDamage = damage;

		if (ad->protection[dmgtype]) {
			if (ad->protection[dmgtype] > 0)
				damage *= 1.0 - ad->protection[dmgtype] * ent->AP * 0.0001;
			else
				damage *= 1.0 - ad->protection[dmgtype] * 0.01;
		}

		if (ad->hardness[dmgtype]) {
			int armorDamage;

			armorDamage = (totalDamage - damage) / ad->hardness[dmgtype];
			ent->AP = armorDamage < ent->AP ? ent->AP - armorDamage : 0;
		}
	}

	assert((attacker->team >= 0) && (attacker->team < MAX_TEAMS));
	assert((ent->team >= 0) && (ent->team < MAX_TEAMS));
#ifdef DEBUG
	if ((attacker->team < 0) || (attacker->team >= MAX_TEAMS))
		return;	/* never reached. need for code analyst. */
	if ((ent->team < 0) || (ent->team >= MAX_TEAMS))
		return;	/* never reached. need for code analyst. */
#endif

	if (g_nodamage != NULL && !g_nodamage->value)
	{
		/* hit */
		if (stun)
			ent->STUN += damage;
		else
			ent->HP -= damage > ent->HP ? ent->HP : damage;
	}

	/* check death/knockouth */
	if (ent->HP <= 0 || ent->HP <= ent->STUN) {
		G_ActorDie(ent, ent->HP <= 0 ? STATE_DEAD : STATE_STUN);

		/* apply morale changes */
		if (mor_panic->value)
			G_Morale(ML_DEATH, ent, attacker, damage);

		/* count also the stunned ones as killed ones */
		level.num_kills[attacker->team][ent->team]++;

		/* count score */
		if (ent->team == TEAM_CIVILIAN)
			attacker->chr.kills[KILLED_CIVILIANS]++;
		else if (attacker->team == ent->team)
			attacker->chr.kills[KILLED_TEAM]++;
		else
			attacker->chr.kills[KILLED_ALIENS]++;

		/* check for win conditions */
		G_CheckEndGame();
	} else {
		if (damage > 0 && mor_panic->value) {
			G_Morale(ML_WOUND, ent, attacker, damage);
		}
		else { /* medikit, etc. */
			if (ent->HP > GET_HP(ent->chr.skills[ABILITY_POWER]))
				ent->HP = GET_HP(ent->chr.skills[ABILITY_POWER]);
		}
		G_SendStats(ent);
	}
}


/**
 * @brief
 */
void G_SplashDamage(edict_t * ent, fireDef_t * fd, vec3_t impact)
{
	edict_t *check;
	vec3_t center;
	float dist;
	int damage;
	int i;

	assert (fd->splrad);

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		/* check basic info */
		if (!check->inuse)
			continue;

		if (check->type == ET_ACTOR || check->type == ET_UGV)
			VectorCopy(check->origin, center);
		else if (check->type == ET_BREAKABLE) {
			VectorAdd(check->absmin, check->absmax, center);
			VectorScale(center, 0.5, center);
		} else
			continue;

		/* check for distance */
		dist = VectorDist(impact, center);
		dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0;
		if (dist > fd->splrad)
			continue;

		/* FIXME: don't make aliens in back visible */
		if (fd->irgoggles && (check->type == ET_ACTOR || check->type == ET_UGV)) {
			G_AppearPerishEvent(~G_VisToPM(check->visflags), 1, check);
			check->visflags |= ~check->visflags;
			continue;
		}

		/* check for walls */
		if ((check->type == ET_ACTOR || check->type == ET_UGV) && !G_ActorVis(impact, check, qfalse))
			continue;

		/* do damage */
		damage = (fd->spldmg[0] + fd->spldmg[1] * crand()) * (1.0 - dist / fd->splrad);
		G_Damage(check, fd->dmgtype, damage, ent);
	}
}

#define GRENADE_DT			0.1
#define GRENADE_STOPSPEED	60.0
/**
 * @brief
 */
void G_ShootGrenade(player_t * player, edict_t * ent, fireDef_t * fd, int type, vec3_t from, pos3_t at, int mask, item_t * weapon)
{
	vec3_t last, target, temp;
	vec3_t startV, curV, oldPos, newPos;
	vec3_t angles;
	float dt, time, speed;
	float acc;
	trace_t tr;
	int bounce;
/*	int i; */
	byte flags;

	/* get positional data */
	VectorCopy(from, last);
	gi.GridPosToVec(gi.map, at, target);
	target[2] -= 9;

	/* calculate parabola */
	dt = gi.GrenadeTarget(last, target, startV); /* TODO: improve trow down */
	if (!dt) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - impossible throw!\n"));
		return;
	}

	/* cap start speed */
	speed = VectorLength(startV);
	if (speed > fd->range)
		speed = fd->range;

	/* add random effects and get new dir */
	acc = GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.skills[fd->weaponSkill] : 0);

	VecToAngles(startV, angles);
	angles[PITCH] += crand() * fd->spread[0] * acc;
	angles[YAW] += crand() * fd->spread[1] * acc;
	AngleVectors(angles, startV, NULL, NULL);
	VectorScale(startV, speed, startV);

	/* move */
	VectorCopy(last, oldPos);
	VectorCopy(startV, curV);
	time = 0;
	dt = 0;
	bounce = 0;
	flags = SF_BOUNCING;
	while (qtrue) {
		/* kinematics */
		VectorMA(oldPos, GRENADE_DT, curV, newPos);
		newPos[2] -= 0.5 * GRAVITY * GRENADE_DT * GRENADE_DT;
		curV[2] -= GRAVITY * GRENADE_DT;

		/* trace */
		tr = gi.trace(oldPos, NULL, NULL, newPos, ent, MASK_SHOT);
		if (tr.fraction < 1.0 || time + dt > 4.0) {
			/* advance time */
			dt += tr.fraction * GRENADE_DT;
			time += dt;
			bounce++;

			if (tr.fraction < 1.0)
				VectorCopy(tr.endpos, newPos);

#if 0
/* please debug, currently it causes double sounds */
			/* calculate additional visibility */
			for (i = 0; i < MAX_TEAMS; i++)
				if (G_TeamPointVis(i, newPos))
					mask |= 1 << i;
#endif

			if
				/* enough bouncing around */
				(VectorLength(curV) < GRENADE_STOPSPEED || time > 4.0 || bounce > fd->bounce
				 /* or we have sensors that tell us enemy is near */
				 || (!fd->delay && tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))) {

				/* explode */
				gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
				gi.WriteShort(dt * 1000);
				gi.WriteByte(type);
				if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))
					gi.WriteByte(flags | SF_BODY);
				else
					gi.WriteByte(flags | SF_IMPACT);
				gi.WritePos(last);
				gi.WritePos(startV);

				tr.endpos[2] += 10;

				/* check if this is a stone, ammor clip or grenade */
				if (fd->splrad) {
					G_SplashDamage(ent, fd, tr.endpos);
				} else {
					/* spawn the stone on the floor */
					if (fd->ammo && !fd->splrad && gi.csi->ods[weapon->t].thrown) {
						pos3_t drop;
						edict_t *floor, *actor;

						VecToPos(tr.endpos, drop);

						for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
							if (floor->inuse
								&& floor->type == ET_ITEM
								&& VectorCompare(drop, floor->pos))
								break;
						}

						if (floor == &g_edicts[globals.num_edicts]) {
							floor = G_SpawnFloor(drop);

							for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
								if ( actor->inuse
									 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
									 && VectorCompare(drop, actor->pos) )
									FLOOR(actor) = FLOOR(floor);
						} else {
							gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
							gi.WriteShort(floor->number);
							floor->visflags = 0;
						}
						Com_TryAddToInventory(&floor->i, *weapon, gi.csi->idFloor);

						/* send item info to the clients */
						G_CheckVis(floor, qtrue);
					}
				}
				return;
			}
			/* send */
			gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
			gi.WriteShort(dt * 1000);
			gi.WriteByte(type);
			gi.WriteByte(flags);
			gi.WritePos(last);
			gi.WritePos(startV);
			flags |= SF_BOUNCED;

			/* bounce */
			VectorScale(curV, fd->bounceFac, curV);
			VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, curV), temp);
			VectorAdd(temp, curV, startV);
			VectorAdd(temp, startV, curV);

			/* prepare next move */
			VectorCopy(tr.endpos, last);
			VectorCopy(tr.endpos, oldPos);
			VectorCopy(curV, startV);
			dt = 0;
		} else {
			dt += GRENADE_DT;
			VectorCopy(newPos, oldPos);
		}
	}
}


/**
 * @brief Fires straight shots.
 * @param[in] ent The attacker.
 * @param[in] fd ?? TODO
 * @param[in] type ?? TODO
 * @param[in] from Location of the gun muzzle.
 * @param[in] at Grid coordinate of the target.
 * @param[in] mask ?? TODO Visibility bit-mask of the others?
 */
void G_ShootSingle(edict_t * ent, fireDef_t * fd, int type, vec3_t from, pos3_t at, int mask, item_t * weapon)
{
	vec3_t dir;	/* Direction from the location of the gun muzzle ("from") to the target ("at") */
	vec3_t angles;	/* ?? TODO The random dir-modifier ?? */
	vec3_t cur_loc;	/* The current location of the projectile. */
	vec3_t impact;	/* The location of the target (-center?) */
	vec3_t temp;
	trace_t tr;	/* ?? TODO */
	float acc;	/* Accuracy modifier for the angle of the shot. */
	float range;	/* ?? TODO */
	int bounce;	/* ?? TODO */
	int damage;	/* The damage to be dealt to the target. */
	byte flags;	/* ?? TODO */
/*	int i; */

	/* Calc direction of the shot. */
	gi.GridPosToVec(gi.map, at, impact);	/* Get the position of the targetted grid-cell. ('impact' is used only temporary here)*/
	VectorCopy(from, cur_loc);		/* Set current location of the projectile to the starting (muzzle) location. */
	VectorSubtract(impact, cur_loc, dir);	/* Calculate the vector from current location to the target. */
	VectorNormalize(dir);			/* Normalize the vector i.e. make length 1.0 */
	VectorMA(cur_loc, 8, dir, cur_loc);	/* ?? TODO: Probably places the starting-location a bit away (cur_loc+8*dir) from the attacker-model/grid. Might need some change to reflect 2x2 units. Also might need a check if the distace is bigger than the one to the impact location.*/
	VecToAngles(dir, angles);		/* Get the angles of the direction vector. */

	/* Get accuracy value for this attacker. */
	acc = GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.skills[fd->weaponSkill] : 0);

	/* Modify the angles with the accuracy modifier as a randomizer-range. If the attacker is crouched this modifier is included as well.  */
	if ((ent->state & STATE_CROUCHED) && fd->crouch) {
		angles[PITCH] += crand() * fd->spread[0] * fd->crouch * acc;
		angles[YAW] += crand() * fd->spread[1] * fd->crouch * acc;
	} else {
		angles[PITCH] += crand() * fd->spread[0] * acc;
		angles[YAW] += crand() * fd->spread[1] * acc;
	}
	/* Convert changed angles into new direction. */
	AngleVectors(angles, dir, NULL, NULL);

	/* shoot and bounce */
	range = fd->range;
	bounce = 0;
	flags = 0;
	do {
		/* Calc 'impact' vector that is located at the end of the range
		   defined by the fireDef_t. This is not really the impact location,
		   but rather the 'endofrange' location, see below for another use.*/
		VectorMA(cur_loc, range, dir, impact);

		/* Do the trace from current position of the projectile
		   to the end_of_range location.*/
		tr = gi.trace(cur_loc, NULL, NULL, impact, ent, MASK_SHOT);
		/* _Now_ we copy the correct impact location. */
		VectorCopy(tr.endpos, impact);

		/* set flags */
		if (tr.fraction < 1.0) {
			if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV)
				/* check if we differenciate between body and wall */
				&& !fd->delay)
				flags |= SF_BODY;
			else if (bounce < fd->bounce)
				flags |= SF_BOUNCING;
			else
				flags |= SF_IMPACT;
		}

#if 0
/* please debug, currently it causes double sounds */
		/* calculate additional visibility */
		for (i = 0; i < MAX_TEAMS; i++)
			if (G_TeamPointVis(i, impact))
				mask |= 1 << i;

		/* victims see shots */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV))
			mask |= 1 << tr.ent->team;
#endif

		/* send shot */
		gi.AddEvent(G_VisToPM(mask), EV_ACTOR_SHOOT);
		gi.WriteShort(ent->number);
		gi.WriteByte(type);
		gi.WriteByte(flags);
		gi.WritePos(cur_loc);
		gi.WritePos(impact);
		gi.WriteDir(tr.plane.normal);

		/* send shot sound to the others */
		gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
		gi.WriteByte(qfalse);
		gi.WriteByte(type);

		/* do splash damage */
		if (tr.fraction < 1.0 && fd->splrad && !fd-> bounce) {
			VectorMA(impact, 8, tr.plane.normal, impact);
			G_SplashDamage(ent, fd, impact);
		}

		/* do damage */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_UGV || tr.ent->type == ET_BREAKABLE)) {
			damage = fd->damage[0] + fd->damage[1] * crand();
			G_Damage(tr.ent, fd->dmgtype, damage, ent);
			break;
		}

		/* bounce is checked here to see if the rubber rocket hit walls enought times to wear out*/
		bounce++;
		if (bounce > fd->bounce || tr.fraction >= 1.0)
			break;

		range -= tr.fraction * range;
		VectorCopy(impact, cur_loc);
		VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, dir), temp);
		VectorAdd(temp, dir, dir);
		VectorAdd(temp, dir, dir);
		flags |= SF_BOUNCED;
	}
	while (1);

	/* spawn the knife on the floor */
	if (fd->ammo && !fd->splrad && gi.csi->ods[weapon->t].thrown) {
		pos3_t drop;
		edict_t *floor, *actor;

		if (VectorCompare(ent->pos, at)) { /* throw under his own feet */
			VectorCopy(at, drop);
		} else {
			impact[2] -= 20; /* a hack: no-gravity items are flying high */
			VecToPos(impact, drop);
		}

		for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
			if (floor->inuse
				&& floor->type == ET_ITEM
				&& VectorCompare(drop, floor->pos))
				break;
		}

		if (floor == &g_edicts[globals.num_edicts]) {
			floor = G_SpawnFloor(drop);

			for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
				if ( actor->inuse
					 && (actor->type == ET_ACTOR || actor->type == ET_UGV)
					 && VectorCompare(drop, actor->pos) )
					FLOOR(actor) = FLOOR(floor);
		} else {
			gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
			gi.WriteShort(floor->number);
			floor->visflags = 0;
		}
		Com_TryAddToInventory(&floor->i, *weapon, gi.csi->idFloor);

		/* send item info to the clients */
		G_CheckVis(floor, qtrue);
	}
}

/**
 * @brief
 */
qboolean G_ClientShoot(player_t * player, int num, pos3_t at, int type)
{
	fireDef_t *fd;
	edict_t *ent;
	item_t *weapon;
	vec3_t dir, center, target, shotOrigin;
	int i, ammo, wi, reaction_leftover, shots;
	int container, mask;

	ent = g_edicts + num;

	/* test shoot type */
	if (type >= ST_NUM_SHOOT_TYPES)
		gi.error("G_ClientShoot: unknown shoot type %i.\n", type);

	if (IS_SHOT_RIGHT(type)) {
		if (!RIGHT(ent))
			return qfalse;
		weapon = &RIGHT(ent)->item;
		container = gi.csi->idRight;
	} else {
		if (!LEFT(ent))
			return qfalse;
		weapon = &LEFT(ent)->item;
		container = gi.csi->idLeft;
	}

	if (weapon->m == NONE) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - object not activable!\n"));
		return qfalse; /* TODO: do G_ShootGrenade with that ammo clip */
	}

	fd = &gi.csi->ods[weapon->m].fd[SHOT_FD_PRIO(type)];
	wi = weapon->m | (SHOT_FD_PRIO(type) << 7/*move to byte end*/);
	ammo = weapon->a;
	reaction_leftover = IS_SHOT_REACTION(type) ? sv_reaction_leftover->value : 0;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, fd->time + reaction_leftover))
		return qfalse;

	if (!ammo && fd->ammo && !gi.csi->ods[weapon->t].thrown) {
		gi.cprintf(player, PRINT_HIGH, _("Can't perform action - no ammo!\n"));
		return qfalse;
	}

	/* fire shots */
	shots = fd->shots;
	if (fd->ammo && !gi.csi->ods[weapon->t].thrown) {
		if (ammo < fd->ammo) {
			shots = fd->shots * ammo / fd->ammo;
			ammo = 0;
		}
		ammo -= fd->ammo;
		if (shots < 1) {
			gi.cprintf(player, PRINT_HIGH, _("Can't perform action - not enough ammo!\n"));
			return qfalse;
		}
	}

	/* rotate the player */
	VectorSubtract(at, ent->pos, dir);
	ent->dir = AngleToDV((int) (atan2(dir[1], dir[0]) * 180 / M_PI));

	G_CheckVisTeam(ent->team, NULL, qfalse);

	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
	gi.WriteShort(num);
	gi.WriteByte(ent->dir);

	/* calculate visibility */
	gi.GridPosToVec(gi.map, at, target);
	VectorSubtract(target, ent->origin, dir);
	VectorMA(ent->origin, 0.5, dir, center);
	mask = 0;
	for (i = 0; i < MAX_TEAMS; i++)
		if (ent->visflags & (1 << i) || G_TeamPointVis(i, target) || G_TeamPointVis(i, center))
			mask |= 1 << i;

	/* start shoot */
	gi.AddEvent(G_VisToPM(mask), EV_ACTOR_START_SHOOT);
	gi.WriteShort(ent->number);
	gi.WriteByte(wi);
	gi.WriteGPos(ent->pos);
	gi.WriteGPos(at);

	/* send shot sound to the others */
	gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
	gi.WriteByte(qtrue);
	gi.WriteByte(wi);

	/* ammo... */
	if (fd->ammo) {
		if ( ammo > 0
			 || !gi.csi->ods[weapon->t].thrown ) {
			gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_AMMO);
			gi.WriteShort(num);
			gi.WriteByte(ammo);
			gi.WriteByte(weapon->m);
			weapon->a = ammo;
			if (IS_SHOT_RIGHT(type))
				gi.WriteByte(gi.csi->idRight);
			else
				gi.WriteByte(gi.csi->idLeft);
		} else { /* delete the knife or the rifle without ammo */
			gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_DEL);
			gi.WriteShort(num);
			gi.WriteByte(container);
			assert (gi.csi->ids[container].single);
			Com_EmptyContainer(&ent->i, container);
		}
		/* x and y value */
		gi.WriteByte(0);
		gi.WriteByte(0);
	}

	/* get weapon position */
	gi.GridPosToVec(gi.map, ent->pos, shotOrigin);
	/* adjust height: */
	shotOrigin[2] += fd->shotOrg[1];
	/* adjust horizontal: */
	if (fd->shotOrg[0] != 0) {
		float x, y, length;

		/* get "right" and "left" of a unit(rotate dir 90 on the x-y plane): */
		x = dir[1];
		y = -dir[0];
		length = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		/* assign adjustments: */
		shotOrigin[0] += x * fd->shotOrg[0] / length;
		shotOrigin[1] += y * fd->shotOrg[0] / length;
	}

	/* fire all shots */
	for (i = 0; i < shots; i++)
		if (fd->gravity)
			G_ShootGrenade(player, ent, fd, wi, shotOrigin, at, mask, weapon);
		else
			G_ShootSingle(ent, fd, wi, shotOrigin, at, mask, weapon);

	/* send TUs */
	if (ent->inuse) {
		ent->TU -= fd->time;
		G_SendStats(ent);
	}

	/* end events */
	gi.EndEvents();

	/* check for Reaction fire against the shooter */
	G_ReactionFire(ent);

	return qtrue;
}

/**
 * @brief Reload weapon with actor.
 * @param[in] hand
 * @sa AI_ActorThink
 */
void G_ClientReload(player_t *player, int entnum, shoot_types_t st)
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
			   && gi.csi->ods[ent->i.c[gi.csi->idRight]->item.t].twohanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	}
	else
		return;

	/* LordHavoc: Check if item is researched when in singleplayer? I don't think this is really a cheat issue as in singleplayer there is no way to inject fake client commands in the virtual network buffer, and in multiplayer everything is researched */

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (gi.csi->ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point */
			/* searching other containers if it would take longer */
			/* to retrieve the ammo from them than the one */
			/* we've already found. */
			for (ic = ent->i.c[container]; ic; ic = ic->next)
				if ( gi.csi->ods[ic->item.t].link == weapon ) {
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
		G_ClientInvMove(player, entnum, bestContainer, x, y, hand, 0, 0, qtrue);
}

/**
 * @brief Returns true if actor can reload weapon
 * @param[in] hand
 * @sa AI_ActorThink
 */
qboolean G_ClientCanReload(player_t *player, int entnum, shoot_types_t st)
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
			   && gi.csi->ods[ent->i.c[gi.csi->idRight]->item.t].twohanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	}
	else
		return qfalse;

	for (container = 0; container < gi.csi->numIDs; container++)
		for (ic = ent->i.c[container]; ic; ic = ic->next)
			if ( gi.csi->ods[ic->item.t].link == weapon )
				return qtrue;
	return qfalse;
}

/**
 * @brief Retrieve weapon from backpack for actor
 * @sa AI_ActorThink
 */
void G_ClientGetWeaponFromInventory(player_t *player, int entnum)
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
				if ( gi.csi->ods[ic->item.t].weapon && (ic->item.a > 0 || !gi.csi->ods[ic->item.t].reload)) {
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
		G_ClientInvMove(player, entnum, bestContainer, x, y, hand, 0, 0, qtrue);
}

/**
 * @brief Report and handle death of an actor
 */
void G_ActorDie(edict_t * ent, int state)
{
	assert(ent);
#ifdef DEBUG
	if (!ent)
		return;	/* never reached. need for code analyst. */
#endif

	gi.dprintf("G_ActorDie: kill actor on team %i\n", ent->team);
	switch (state) {
	case STATE_DEAD:
		ent->HP = 0;
		ent->state |= (1 + rand() % 3);
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

	/* handle inventory */
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
void G_KillTeam(void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.argc() == 2)
		teamToKill = atoi(gi.argv(1));

	gi.dprintf("G_KillTeam: kill team %i\n", teamToKill);

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
 * @brief	Checks if an edict has reaction fire enabled and sees the target. Then fire at him.
 * @param[in,out] target The edict that will be fired on.
 */
qboolean G_ReactionFire(edict_t * target)
{
	qboolean fired, frustom;
	player_t *player;
	edict_t *ent;
	int i, team;
	float actorVis;

	fired = qfalse;
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if ( ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && !(ent->state & STATE_DEAD) &&
				(ent->state & STATE_SHAKEN || ent->state & STATE_REACTION_MANY || (ent->state & STATE_REACTION_ONCE && !TU_REACTIONS[ent->number][1]))) {
			actorVis = G_ActorVis(ent->origin, target, qtrue);
			frustom = G_FrustomVis(ent, target->origin);
			if (actorVis > 0.4 && frustom) {
				if (target->team == TEAM_CIVILIAN || target->team == ent->team)
					if (!(ent->state & STATE_SHAKEN) || (float) ent->morale / mor_shaken->value > frand())
						continue;
				/* If reaction fire is triggered by a friendly unit
				   and the shooter is still sane, don't shoot;
				   well, if the shooter isn't sane anymore... */

				/* Get player. */
				player = game.players + ent->pnum;
				if (!player)
					continue;

				/* Change active team for this shot only. */
				team = level.activeTeam;
				level.activeTeam = ent->team;

				/* Fire the first weapon in hands if everything is ok. */
				if ( RIGHT(ent)
					 && (RIGHT(ent)->item.m != NONE)
					 && gi.csi->ods[RIGHT(ent)->item.t].weapon
					 && (!gi.csi->ods[RIGHT(ent)->item.t].reload
						 || RIGHT(ent)->item.a > 0)
					 && gi.csi->ods[RIGHT(ent)->item.m].fd[0].range > VectorDist(ent->origin, target->origin) ) {
					fired = G_ClientShoot(player, ent->number, target->pos, ST_RIGHT_PRIMARY_REACTION);
				} else if ( LEFT(ent)
							&& (LEFT(ent)->item.m != NONE)
							&& gi.csi->ods[LEFT(ent)->item.t].weapon
							&& (!gi.csi->ods[LEFT(ent)->item.t].reload
								|| LEFT(ent)->item.a > 0)
							&& gi.csi->ods[LEFT(ent)->item.m].fd[0].range > VectorDist(ent->origin, target->origin) ) {
					fired = G_ClientShoot(player, ent->number, target->pos, ST_LEFT_PRIMARY_REACTION);
				}

				/* Revert active team. */
				level.activeTeam = team;

				if ( fired ) {
					ent->state &= ~STATE_SHAKEN;
					TU_REACTIONS[ent->number][1]  += 1;	/* Save the fact that the ent has fired. */
				}

				/* Check for death of the target. */
				if (target->state & STATE_DEAD)
					return fired;
			}
		}

	return fired;
}


/**
 * @brief
 */
void G_ClientAction(player_t * player)
{
	int action;
	int num;
	pos3_t pos;
	int i;
	int from, fx, fy, to, tx, ty;

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
		G_ClientMove(player, player->pers.team, num, pos, qtrue);
		break;

	case PA_STATE:
		gi.ReadFormat(pa_format[PA_STATE], &i);
		G_ClientStateChange(player, num, i);
		break;

	case PA_SHOOT:
		gi.ReadFormat(pa_format[PA_SHOOT], &pos, &i);
		(void)G_ClientShoot(player, num, pos, i);
		break;

	case PA_INVMOVE:
		gi.ReadFormat(pa_format[PA_INVMOVE], &from, &fx, &fy, &to, &tx, &ty);
		G_ClientInvMove(player, num, from, fx, fy, to, tx, ty, qtrue);
		break;

	default:
		gi.error("G_ClientAction: Unknown action!\n");
		break;
	}
}


/**
 * @brief Sets the teanum var for this match
 * @param[in] player Pointer to connected player
 * @TODO: Check whether there are enough free spawnpoints
 */
void G_GetTeam(player_t * player)
{
	player_t *p;
	int i, j;

	/* player has already a team */
	if (player->pers.team) {
		gi.dprintf("You are already on team %i\n", player->pers.team);
		return;
	}

	/* find a team */
	if (sv_maxclients->value == 1)
		player->pers.team = TEAM_PHALANX;
	else if (atoi(Info_ValueForKey(player->pers.userinfo, "spectator"))) {
		/* TODO: spectors get an in game menu to select the team */
		player->pers.spectator = qtrue;
	} else if (sv_teamplay->value) {
		/* set the team specified in the userinfo */
		gi.dprintf("Get a team for teamplay\n");
		i = atoi(Info_ValueForKey(player->pers.userinfo, "teamnum"));
		/* civilians are at team zero */
		if (i > 0)
			player->pers.team = i;
		else
			player->pers.team = 1;
	} else {
		/* search team */
		gi.dprintf("Get a team for multiplayer\n");
		for (i = 1; i < MAX_TEAMS; i++)
			if (level.num_spawnpoints[i]) {
				/* check if team is in use */
				/* FIXME: If someone left the game and rejoins he should get his "old" team back */
				/*        maybe we could identify such a situation */
				for (j = 0, p = game.players; j < game.maxplayers; j++, p++)
					if (p->inuse && p->pers.team == i) {
						Com_Printf("Team %i is already in use\n", i);
						/* team already in use */
						break;
					}
				/* TODO: disconnect the player or convert to spectator */
				if (j >= game.maxplayers) {
					player->pers.spectator = qtrue;
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
			player->pers.team = i;
		} else
			player->pers.team = -1;
	}
}

/**
 * @brief
 * @sa CL_SendTeamInfo
 * @TODO: Check size (fieldSize here)
 */
void G_ClientTeamInfo(player_t * player)
{
	edict_t *ent;
	int i, j, k, length;
	int container, x, y;
	item_t item;
	byte count[MAX_OBJDEFS];

	/* find a team */
	G_GetTeam(player);
	length = gi.ReadByte();

	/* find actors */
	for (j = 0, ent = g_edicts; j < globals.num_edicts; j++, ent++)
		if ((ent->type == ET_ACTORSPAWN || ent->type == ET_UGVSPAWN)
			&& player->pers.team == ent->team)
			break;

	memset(count, 0, sizeof(count));
	for (i = 0; i < length; i++) {
		if (j < globals.num_edicts) {
			/* here the actors actually spawn */
			level.num_alive[ent->team]++;
			level.num_spawned[ent->team]++;
			ent->pnum = player->num;
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
			Q_strncpyz(ent->chr.name, gi.ReadString(), MAX_VAR);
			Q_strncpyz(ent->chr.path, gi.ReadString(), MAX_VAR);
			Q_strncpyz(ent->chr.body, gi.ReadString(), MAX_VAR);
			Q_strncpyz(ent->chr.head, gi.ReadString(), MAX_VAR);
			ent->chr.skin = gi.ReadByte();
#if 0
			gi.dprintf("G_ClientTeamInfo: name: %s\n", ent->chr.name);
			gi.dprintf("G_ClientTeamInfo: path: %s\n", ent->chr.path);
			gi.dprintf("G_ClientTeamInfo: body: %s\n", ent->chr.body);
			gi.dprintf("G_ClientTeamInfo: head: %s\n", ent->chr.head);
			gi.dprintf("G_ClientTeamInfo: skin: %i\n", ent->chr.skin);
#endif

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
					/* gi.dprintf("G_ClientTeamInfo: add %s to inventory (container %i - idArmor: %i)\n", gi.csi->ods[ent->i.c[container]->item.t].kurz, container, gi.csi->idArmor); */
				}
			}

			/* set models */
			ent->chr.inv = &ent->i;
			ent->body = gi.modelindex(Com_CharGetBody(&ent->chr));
			ent->head = gi.modelindex(Com_CharGetHead(&ent->chr));
			ent->skin = ent->chr.skin;

			/* set initial vital statistics */
			ent->HP = GET_HP(ent->chr.skills[ABILITY_POWER]);
			ent->AP = ent->i.c[gi.csi->idArmor] ? 100 : 0;
			ent->STUN = 0;
			if (ent->type == ET_ACTOR)
				ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);
		} else {
			/* just do nothing with the info */
			gi.ReadByte(); /* fieldSize */
			gi.ReadShort(); /* ucn */
			for (k = 0; k < 4; k++)
				gi.ReadString(); /* name, path, body, head */
			gi.ReadByte(); /* skin */
			for (k = 0; k < SKILL_NUM_TYPES; k++)
				gi.ReadByte(); /* skills */
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				gi.ReadShort(); /* kills */
			gi.ReadShort(); /* assigned missions */
			j = gi.ReadShort();
			/* TODO: skip j bytes instead of reading and ignoring */
			for (k = 0; k < j; k++)
				gi.ReadByte(); /* inventory */
		}

		/* find actors */
		for (; j < globals.num_edicts; j++, ent++)
			if ((ent->type == ET_ACTORSPAWN || ent->type == ET_UGVSPAWN)
			&& player->pers.team == ent->team)
				break;
	}
}


/**
 * @brief
 * @TODO: Check if we are in multiplayer and there are other teams
 */
void G_ClientEndRound(player_t * player)
{
	player_t *p;
	int i, lastTeam, nextTeam;

	/* inactive players can't end their inactive round :) */
	if (level.activeTeam != player->pers.team)
		return;

	/* check for "team oszillation" */
	if (level.framenum < level.nextEndRound)
		return;
	level.nextEndRound = level.framenum + 20;

	/* check if all team members are ready */
	player->ready = qtrue;
	for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam && !p->ready)
			return;

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
			if (i >= MAX_TEAMS)
				i = 0;
			if (((level.num_alive[i] || (level.num_spawnpoints[i] && !level.num_spawned[i]))
				 && i != lastTeam)) {
				nextTeam = i;
				break;
			}
		}
		if (nextTeam == -1) {
/*			gi.bprintf( PRINT_HIGH, "Can't change round - no living actors left.\n" ); */
			level.activeTeam = lastTeam;
			gi.EndEvents();
			return;
		}

		/* search corresponding player */
		for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
			if (p->inuse && p->pers.team == nextTeam) {
				/* found next player */
				level.activeTeam = nextTeam;
				break;
			}

		if (level.activeTeam == -1 && sv_ai->value && ai_autojoin->value) {
			/* no corresponding player found - create ai player */
			p = AI_CreatePlayer(nextTeam);
			if (p)
				level.activeTeam = nextTeam;
		}

		lastTeam = nextTeam;
	}

	/* communicate next player in row to clients */
	gi.AddEvent(PM_ALL, EV_ENDROUND);
	gi.WriteByte(level.activeTeam);

	/* apply morale behaviour, reset reaction fire */
	G_ResetReactionFire(level.activeTeam);
	if (mor_panic->value)
		G_MoraleBehaviour(level.activeTeam);

	/* start ai */
	p->pers.last = NULL;

	/* finish off events */
	gi.EndEvents();

	/* reset ready flag */
	for (i = 0, p = game.players; i < game.maxplayers * 2; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam)
			p->ready = qfalse;
}


/**
 * @brief
 */
void G_ClientBegin(player_t * player)
{
	/* this doesn't belong here, but it works */
	if (!level.routed) {
		level.routed = qtrue;
		G_CompleteRecalcRouting();
	}

	/* get a team */
	G_GetTeam(player);

	/* activate his round if he's first to join */
	if (level.activeTeam == -1)
		level.activeTeam = player->pers.team;

	level.numplayers++;

	/* do all the init events here... */
	/* reset the data */
	gi.AddEvent(P_MASK(player), EV_RESET | INSTANTLY);
	gi.WriteByte(player->pers.team);
	gi.WriteByte(level.activeTeam);

	/* show visible actors and submit stats */
	G_ClearVisFlags(player->pers.team);
	G_CheckVis(NULL, qfalse);
	G_SendPlayerStats(player);

	/* give time units */
	G_GiveTimeUnits(player->pers.team);

	/* spawn camera (starts client rendering) */
	gi.AddEvent(P_MASK(player), EV_START);

	/* send events */
	gi.EndEvents();

	/* inform all clients */
	gi.bprintf(PRINT_HIGH, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team);
}


/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged(player_t * player, char *userinfo)
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
}


/**
 * @brief
 */
qboolean G_ClientConnect(player_t * player, char *userinfo)
{
	char *value;

	/* check to see if they are on the banned IP list */
	value = Info_ValueForKey(userinfo, "ip");
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return qfalse;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->inuse)
		G_ClientDisconnect(player);

	/* reset persistant data */
	memset(&player->pers, 0, sizeof(client_persistant_t));
	Q_strncpyz(player->pers.userinfo, userinfo, sizeof(player->pers.userinfo));

	gi.bprintf(PRINT_HIGH, "%s is connecting...\n", Info_ValueForKey(userinfo, "name"));
	return qtrue;
}

/**
 * @brief
 */
void G_ClientDisconnect(player_t * player)
{
	level.numplayers--;

	if (level.activeTeam == player->pers.team)
		G_ClientEndRound(player);

	gi.bprintf(PRINT_HIGH, "%s disconnected.\n", player->pers.netname);
}
