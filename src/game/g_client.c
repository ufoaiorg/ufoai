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

static chrScoreMission_t scoreMission[MAX_EDICTS];
static int scoreMissionNum = 0;

/**
 * @brief Checks whether the given edict is a living actor
 * @param[in] ent The edict to perform the check for
 * @sa LE_IsLivingActor
 */
qboolean G_IsLivingActor (const edict_t *ent)
{
	assert(ent);
	return ((ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2) && !G_IsDead(ent));
}

/**
 * @brief Generates the player bit mask for a given team
 * @param[in] team The team to create the player bit mask for
 * @note E.g. multiplayer team play can have more than one human player on the
 * same team.
 */
unsigned int G_TeamToPM (int team)
{
	const player_t *p;
	unsigned int player_mask, i;

	player_mask = 0;

	/* don't handle the ai players, here */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++)
		if (p->inuse && team == p->pers.team)
			player_mask |= (1 << i);

	return player_mask;
}

/**
 * @brief Converts vis mask to player mask
 * @param[in] vis_mask The visibility bit mask (contains the team numbers) that
 * is converted to a player mask
 * @return Returns a playermask for all the teams of the connected players that
 * are marked in the given @c vis_mask.
 */
unsigned int G_VisToPM (unsigned int vis_mask)
{
	const player_t *p;
	unsigned int player_mask, i;

	player_mask = 0;

	/* don't handle the ai players, here */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++)
		if (p->inuse && (vis_mask & (1 << p->pers.team)))
			player_mask |= (1 << i);

	return player_mask;
}

/**
 * @brief Send stats to network buffer
 * @sa CL_ActorStats
 */
void G_SendStats (edict_t * ent)
{
	/* extra sanity checks */
	ent->TU = max(ent->TU, 0);
	ent->HP = max(ent->HP, 0);
	ent->STUN = min(ent->STUN, 255);
	ent->morale = max(ent->morale, 0);

	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);
}

/**
 * @brief Write an item to the network buffer
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
static void G_WriteItem (item_t item, const invDef_t *container, int x, int y)
{
	assert(item.t);
	gi.WriteFormat("sbsbbbb", item.t->idx, item.a, item.m ? item.m->idx : NONE, container->id, x, y, item.rotated);
}

/**
 * @brief Read item from the network buffer
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
static void G_ReadItem (item_t *item, invDef_t **container, int *x, int *y)
{
	int t, m;
	int containerID;

	gi.ReadFormat("sbsbbbb", &t, &item->a, &m, &containerID, x, y, &item->rotated);

	assert(t != NONE);
	item->t = &gi.csi->ods[t];

	item->m = NULL;
	if (m != NONE)
		item->m = &gi.csi->ods[m];

	if (containerID >= 0 && containerID < gi.csi->numIDs)
		*container = &gi.csi->ids[containerID];
	else
		*container = NULL;
}

/**
 * @brief Write player stats to network buffer
 * @sa G_SendStats
 */
static void G_SendPlayerStats (player_t * player)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2) && ent->team == player->pers.team)
			G_SendStats(ent);
}

/**
 * Send messages to human players
 * @param player A player (AI players are ignored here)
 */
void G_PlayerPrintf (const player_t * player, int printlevel, const char *fmt, ...)
{
	va_list ap;

	/* there is no client for an AI controlled player on the server where we
	 * could send the message to */
	if (player->pers.ai)
		return;

	va_start(ap, fmt);
	gi.PlayerPrintf(player, printlevel, fmt, ap);
	va_end(ap);
}

/**
 * @brief Regenerate the "STUN" value of each (partly) stunned team member.
 * @note The values are @b not sent via network. This is done in
 * @c G_GiveTimeUnits. It @b has to be called afterwards.
 * Fully stunned team members are not considered here (yet) - they remain
 * fully stunned (i.e. on the floor).
 * @param[in] team The index of the team to update the values for.
 * @sa G_GiveTimeUnits
 * @todo Make this depend on the character-attributes. http://lists.cifrid.net/pipermail/ufoai/2008-February/000346.html
 */
static void G_UpdateStunState (int team)
{
	edict_t *ent;
	int i;
	const int regen = 1;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == team) {
			if (ent->STUN > 0 && (ent->state & ~STATE_STUN)) {
				if (regen > ent->STUN)
					ent->STUN = 0;
				else
					ent->STUN -= regen;
			}
		}
}

/**
 * @brief Network function to update the time units (TUs) for each team-member.
 * @param[in] team The index of the team to update the values for.
 * @sa G_SendStats
 */
void G_GiveTimeUnits (int team)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == team) {
			ent->state &= ~STATE_DAZED;
			ent->TU = GET_TU(ent->chr.score.skills[ABILITY_SPEED]);
			G_SendStats(ent);
		}
}

static void G_SendState (unsigned int player_mask, edict_t * ent)
{
	gi.AddEvent(player_mask & G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);

	gi.AddEvent(player_mask & ~G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state & STATE_PUBLIC);
}

/**
 * Send a particle spawn event to the client
 * @param player_mask The clients that should see the particle
 * @param ent The particle to spawn
 */
static void G_SendParticle (unsigned int player_mask, edict_t *ent)
{
	gi.AddEvent(player_mask, EV_SPAWN_PARTICLE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->spawnflags);
	gi.WriteString(ent->particle);
}

/**
 * @sa G_EndGame
 * @sa G_AppearPerishEvent
 * @sa CL_InvAdd
 */
void G_SendInventory (unsigned int player_mask, edict_t * ent)
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
	if (nr == 0 && ent->type != ET_ITEM)
		return;

	gi.AddEvent(player_mask, EV_INV_ADD);

	gi.WriteShort(ent->number);

	/* size of inventory */
	gi.WriteShort(nr * INV_INVENTORY_BYTES);
	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next) {
			/* send a single item */
			G_WriteItem(ic->item, &gi.csi->ids[j], ic->x, ic->y);
		}
}

static void G_EdictAppear (unsigned int player_mask, edict_t *ent)
{
	gi.AddEvent(player_mask, EV_ENT_APPEAR);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->type);
	gi.WriteGPos(ent->pos);
}

/**
 * @brief Send the appear or perish event the the affected clients
 * @param[in] player_mask These are the affected players or clients
 * In case of e.g. teamplay there can be more than one client affected - thus
 * this is a player mask
 * @param[in] appear Is this event about an appearing actor (or a perishing one)
 * @param[in] check The edict we are talking about
 * @param[in] ent The edict that was responsible for letting the check edict appear or perish
 * @sa CL_ActorAppear
 */
void G_AppearPerishEvent (unsigned int player_mask, int appear, edict_t *check, edict_t *ent)
{
	/* test for pointless player mask */
	if (!player_mask)
		return;

	if (appear) {
		/* appear */
		switch (check->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* parsed in CL_ActorAppear */
			gi.AddEvent(player_mask, EV_ACTOR_APPEAR);
			gi.WriteShort(check->number);
			if (ent != NULL)
				gi.WriteShort(ent->number);
			else
				gi.WriteShort(-1);

			gi.WriteByte(check->team);
			gi.WriteByte(check->chr.teamDef ? check->chr.teamDef->idx : NONE);
			gi.WriteByte(check->chr.gender);
			gi.WriteByte(check->pnum);
			gi.WriteGPos(check->pos);
			gi.WriteByte(check->dir);
			if (RIGHT(check)) {
				gi.WriteShort(RIGHT(check)->item.t->idx);
			} else {
				gi.WriteShort(NONE);
			}

			if (LEFT(check)) {
				gi.WriteShort(LEFT(check)->item.t->idx);
			} else {
				gi.WriteShort(NONE);
			}

			gi.WriteShort(check->body);
			gi.WriteShort(check->head);
			gi.WriteByte(check->chr.skin);
			gi.WriteShort(check->state & STATE_PUBLIC);
			gi.WriteByte(check->fieldSize);
			gi.WriteByte(GET_TU(check->chr.score.skills[ABILITY_SPEED]));
			gi.WriteByte(min(MAX_SKILL, GET_MORALE(check->chr.score.skills[ABILITY_MIND])));
			gi.WriteShort(check->chr.maxHP);

			if (player_mask & G_TeamToPM(check->team)) {
				gi.AddEvent(player_mask & G_TeamToPM(check->team), EV_ACTOR_STATECHANGE);
				gi.WriteShort(check->number);
				gi.WriteShort(check->state);
			}
			G_SendInventory(G_TeamToPM(check->team) & player_mask, check);
			break;

		case ET_ITEM:
			G_EdictAppear(player_mask, check);
			G_SendInventory(player_mask, check);
			break;

		case ET_PARTICLE:
			G_EdictAppear(player_mask, check);
			G_SendParticle(player_mask, check);
			break;
		}
	} else if (check->type == ET_ACTOR || check->type == ET_ACTOR2x2 || check->type == ET_ITEM || check->type
			== ET_PARTICLE) {
		/* disappear */
		gi.AddEvent(player_mask, EV_ENT_PERISH);
		gi.WriteShort(check->number);
	}
}

/**
 * @brief Checks whether a point is "visible" from the edicts position
 * @sa FrustumVis
 */
qboolean G_FrustumVis (const edict_t *from, const vec3_t point)
{
	return FrustumVis(from->origin, from->dir, point);
}

/**
 * @brief tests the visibility between two points
 * @param[in] from The point to check visibility from
 * @param[in] to The point to check visibility to
 * @return true if the points are visible from each other, false otherwise.
 */
static qboolean G_LineVis (const vec3_t from, const vec3_t to)
{
#if 0 /* this version is more accurate and includes entity tests */
	trace_t tr;
	tr = gi.trace(from, NULL, NULL, to, NULL, MASK_SOLID);
	return (tr.fraction < 1.0);
#elif 0 /* this version is much faster but has no entity test*/
	return gi.TestLine(from, to, TL_FLAG_NONE);
#else /* a compromise- but still checks for entities that may obstruct view */
	const char *entList[MAX_EDICTS];
	/* generate entity list */
	G_GenerateEntList(entList);
	/* test for visibility */
	return gi.TestLineWithEnt(from, to, TL_FLAG_NONE, entList);
#endif
}

/**
 * @brief calculate how much check is "visible" from @c from
 * @return a value between 0.0 and 1.0 which reflects the visibility from 0
 * to 100 percent
 */
float G_ActorVis (const vec3_t from, const edict_t *check, qboolean full)
{
	vec3_t test, dir;
	float delta;
	int i, n;

	/* start on eye height */
	VectorCopy(check->origin, test);
	if (G_IsDead(check)) {
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
				return ACTOR_VIS_100;
		}

		/* look further down or stop */
		if (!delta) {
			if (n > 0)
				return ACTOR_VIS_100;
			else
				return ACTOR_VIS_0;
		}
		VectorMA(test, 7, dir, test);
		test[2] -= delta;
	}

	/* return factor */
	switch (n) {
	case 0:
		return ACTOR_VIS_0;
	case 1:
		return ACTOR_VIS_10;
	case 2:
		return ACTOR_VIS_50;
	default:
		return ACTOR_VIS_100;
	}
}

/**
 * @brief test if check is visible by from
 * @param[in] team Living team members are always visible. If this is a negative
 * number we inverse the team rules (see comments included). In combination with VT_NOFRUSTUM
 * we can check whether there is any edict (that is no in our team) that can see @c check
 * @param[in] from is from team @c team and must be a living actor
 * @param[in] check The edict we want to get the visibility for
 * @param[in] flags VT_NOFRUSTUM, ...
 */
float G_Vis (int team, const edict_t * from, const edict_t * check, int flags)
{
	vec3_t eye;

	/* if any of them isn't in use, then they're not visible */
	if (!from->inuse || !check->inuse)
		return ACTOR_VIS_0;

	/* only actors and 2x2 units can see anything */
	if (!G_IsLivingActor(from))
		return ACTOR_VIS_0;

	/* living team members are always visible */
	if (team >= 0 && check->team == team && !G_IsDead(check))
		return ACTOR_VIS_100;

	/* standard team rules */
	if (team >= 0 && from->team != team)
		return ACTOR_VIS_0;

	/* inverse team rules */
	if (team < 0 && (from->team == -team || from->team == TEAM_CIVILIAN || check->team != -team))
		return ACTOR_VIS_0;

	/* check for same pos */
	if (VectorCompare(from->pos, check->pos))
		return ACTOR_VIS_100;

	/* view distance check */
	if (VectorDistSqr(from->origin, check->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST)
		return ACTOR_VIS_0;

	/* view frustum check */
	if (!(flags & VT_NOFRUSTUM) && !G_FrustumVis(from, check->origin))
		return ACTOR_VIS_0;

	if (!(check->type == ET_ACTOR || check->type ==  ET_ACTOR2x2 || check->type == ET_ITEM || check->type == ET_PARTICLE))
		return ACTOR_VIS_0;

	/* get viewers eye height */
	VectorCopy(from->origin, eye);
	if (from->state & (STATE_CROUCHED | STATE_PANIC))
		eye[2] += EYE_CROUCH;
	else
		eye[2] += EYE_STAND;

	/* line trace check */
	switch (check->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		return G_ActorVis(eye, check, qfalse);
	case ET_ITEM:
	case ET_PARTICLE:
		return !G_LineVis(eye, check->origin);
	default:
		return ACTOR_VIS_0;
	}
}

/**
 * @brief test if check is visible by team (or if visibility changed?)
 * @sa G_CheckVisTeam
 * @sa AI_FighterCalcBestAction
 * @param[in] team the team the edict may become visible for or perish from
 * their view
 * @param[in] check the edict we are talking about - which may become visible
 * or perish
 * @param[in] flags if you want to check whether the edict may also perish from
 * other players view, you should use the @c VT_PERISH bits
 * @note If the edict is already visible and flags doesn't contain the
 * bits of @c VT_PERISH, no further checks are performed - only the
 * @c VIS_YES bits are returned
 */
int G_TestVis (int team, edict_t * check, int flags)
{
	int i;
	edict_t *from;
	/* store old flag */
	const int old = (check->visflags & (1 << team)) ? 1 : 0;

	if (g_aidebug->integer)
		return VIS_YES | !old;

	if (!(flags & VT_PERISH) && old)
		return VIS_YES;

	/* test if check is visible */
	for (i = 0, from = g_edicts; i < globals.num_edicts; i++, from++)
		if (G_Vis(team, from, check, flags))
			/* visible */
			return VIS_YES | !old;

	/* just return the old state */
	return old;
}

/**
 * @brief This function sends all the actors to the client that are not visible
 * initially - this is needed because an actor can e.g. produce sounds that are
 * send over the net. And the client can only handle them if he knows the
 * @c le_t (local entity) already
 * @note Call this for the first @c G_CheckVis call for every new
 * actor or player
 * @sa G_CheckVis
 * @sa CL_ActorAdd
 */
void G_SendInvisible (player_t* player)
{
	const int team = player->pers.team;

	assert(team != TEAM_NO_ACTIVE);
	if (level.num_alive[team]) {
		int i;
		edict_t* ent;
		/* check visibility */
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
			if (ent->inuse && ent->team != team && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)) {
				/* not visible for this team - so add the le only */
				if (!(ent->visflags & (1 << team))) {
					/* parsed in CL_ActorAdd */
					gi.AddEvent(G_PlayerToPM(player), EV_ACTOR_ADD);
					gi.WriteShort(ent->number);
					gi.WriteByte(ent->team);
					gi.WriteByte(ent->chr.teamDef ? ent->chr.teamDef->idx : NONE);
					gi.WriteByte(ent->chr.gender);
					gi.WriteByte(ent->pnum);
					gi.WriteGPos(ent->pos);
					gi.WriteShort(ent->state & STATE_PUBLIC);
					gi.WriteByte(ent->fieldSize);
				}
			}
		}
	}
}

/**
 * @brief Sets visible edict on player spawn
 * @sa G_ClientSpawn
 * @sa G_CheckVisTeam
 * @sa G_AppearPerishEvent
 */
static int G_CheckVisPlayer (player_t* player, qboolean perish)
{
	int i;
	int status = 0;
	edict_t* ent;

	/* check visibility */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse) {
			/* check if he's visible */
			const int vis = G_TestVis(player->pers.team, ent, perish);

			if (vis & VIS_CHANGE) {
				ent->visflags ^= (1 << player->pers.team);
				G_AppearPerishEvent(G_PlayerToPM(player), vis & VIS_YES, ent, NULL);

				if (vis & VIS_YES) {
					status |= VIS_APPEAR;
					if (G_IsLivingActor(ent) && ent->team != TEAM_CIVILIAN)
						status |= VIS_STOP;
				} else
					status |= VIS_PERISH;
			}
		}

	return status;
}

/**
 * @brief Checks whether an edict appear/perishes for a specific team - also
 * updates the visflags each edict carries
 * @sa G_TestVis
 * @param[in] team Team to check the vis for
 * @param[in] check The edict that you wanna check (and which maybe will appear
 * or perish for the given team). If this is NULL every edict will be checked.
 * If check is a NULL pointer - all edicts in g_edicts are checked
 * @param[in] perish Also check whether the edict (the actor) is going to become
 * invisible for the given team
 * @return If an actor get visible who's no civilian VIS_STOP is added to the
 * bit mask, VIS_YES means, he is visible, VIS_CHANGE means that the actor
 * flipped its visibility (invisible to visible or vice versa), VIS_PERISH means
 * that the actor (the edict) is getting invisible for the queried team
 * @note the edict may not only be actors, but also items of course
 * @sa G_TeamToPM
 * @sa G_TestVis
 * @sa G_AppearPerishEvent
 * @sa G_CheckVisPlayer
 */
int G_CheckVisTeam (int team, edict_t * check, qboolean perish, edict_t *ent)
{
	int i, end;
	int status = 0;

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
			const int vis = G_TestVis(team, check, perish);

			/* visiblity has changed ... */
			if (vis & VIS_CHANGE) {
				/* swap the vis mask for the given team */
				check->visflags ^= (1 << team);
				G_AppearPerishEvent(G_TeamToPM(team), vis & VIS_YES, check, ent);

				/* ... to visible - if this is no civilian, stop the movement */
				if (vis & VIS_YES) {
					status |= VIS_APPEAR;
					if (G_IsLivingActor(check) && check->team != TEAM_CIVILIAN)
						status |= VIS_STOP;
				} else
					status |= VIS_PERISH;
			}
		}

	return status;
}

/**
 * @brief Check if the edict appears/perishes for the other teams
 * @sa G_CheckVisTeam
 * @param[in] perish Also check for perishing events
 * @param[in] check The edict we are talking about
 * @return Bitmask of VIS_* values
 * @sa G_CheckVisTeam
 */
int G_CheckVis (edict_t * check, qboolean perish)
{
	int team;
	int status;

	status = 0;
	for (team = 0; team < MAX_TEAMS; team++)
		if (level.num_alive[team])
			status |= G_CheckVisTeam(team, check, perish, NULL);

	return status;
}

/**
 * @brief Reset the visflags for all edict in the global list (g_edicts) for the
 * given team - and only for the given team
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
 * @brief Turns an actor around
 * @param[in] ent the actor (edict) we are talking about
 * @param[in] dir the direction to turn the edict into, might be an action
 * @return Bitmask of visible (VIS_*) values
 * @sa G_CheckVisTeam
 */
int G_DoTurn (edict_t * ent, byte dir)
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

	if (status) {
		/* send the turn */
		gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
		gi.WriteShort(ent->number);
		gi.WriteByte(ent->dir);
	}

	return status;
}

/**
 * @brief Returns the current active team to the server
 */
int G_GetActiveTeam (void)
{
	return level.activeTeam;
}

/**
 * @brief Checks whether the the requested action is possible
 * @param[in] player Which player (human player) is trying to do the action
 * @param[in] ent Which of his units is trying to do the action.
 * @param[in] TU The time units to check against the ones ent has.
 * @param[in] quiet Don't print the console message if quiet is true.
 * the action with
 */
qboolean G_ActionCheck (player_t *player, edict_t *ent, int TU, qboolean quiet)
{
	int msglevel;

	/* don't check for a player - but maybe a server action */
	if (!player)
		return qtrue;

	/* a generic tester if an action could be possible */
	if (level.activeTeam != player->pers.team) {
		G_PlayerPrintf(player, PRINT_HUD, _("Can't perform action - this isn't your round!\n"));
		return qfalse;
	}

	msglevel = quiet ? PRINT_NONE : PRINT_HUD;

	if (!ent || !ent->inuse) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - object not present!\n"));
		return qfalse;
	}

	if (ent->type != ET_ACTOR && ent->type != ET_ACTOR2x2) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - not an actor!\n"));
		return qfalse;
	}

	if (G_IsStunned(ent)) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - actor is stunned!\n"));
		return qfalse;
	}

	if (G_IsDead(ent)) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - actor is dead!\n"));
		return qfalse;
	}

	if (ent->team != player->pers.team) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - not on same team!\n"));
		return qfalse;
	}

	if (ent->pnum != player->num) {
		G_PlayerPrintf(player, msglevel, _("Can't perform action - no control over allied actors!\n"));
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
	/* make sure that the item is always on a field that even the smallest actor can reach */
	floor->fieldSize = ACTOR_SIZE_NORMAL;
	VectorCopy(pos, floor->pos);
	floor->pos[2] = gi.GridFall(gi.routingMap, floor->fieldSize, floor->pos);
	gi.GridPosToVec(gi.routingMap, floor->fieldSize, floor->pos, floor->origin);
	return floor;
}

/**
 * @sa G_GetFloorItems
 */
static edict_t *G_GetFloorItemsFromPos (pos3_t pos)
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
 * @param[in] ent Pointer to an entity being an actor.
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

/**
 * @brief Moves an item inside an inventory. Floors are handled special.
 * @param[in] player The player the edict/soldier belongs to.
 * @param[in] num The edict number of the selected/used edict/soldier.
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
void G_ClientInvMove (player_t * player, int num, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx,
		int ty, qboolean checkaction, qboolean quiet)
{
	edict_t *ent, *floor;
	invList_t *ic;
	qboolean newFloor;
	item_t item;
	int mask;
	inventory_action_t ia;
	int msglevel;
	invList_t fItemBackup;
	const invList_t* fItemBackupPtr;
	int fx, fy;

	ent = g_edicts + num;
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

	/* "get floor ready" - searching for existing floor-edict*/
	floor = G_GetFloorItems(ent); /* Also sets FLOOR(ent) to correct value. */
	if (to->id == gi.csi->idFloor && !floor) {
		/* We are moving to the floor, but no existing edict for this floor-tile found -> create new one */
		floor = G_SpawnFloor(ent->pos);
		newFloor = qtrue;
	} else if (from->id == gi.csi->idFloor && !floor) {
		/* We are moving from the floor, but no existing edict for this floor-tile found -> thi should never be the case. */
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
	ia = Com_MoveInInventory(&ent->i, from, fItem, to, tx, ty, &ent->TU, &ic);
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

	assert(gi.csi->idFloor >= 0 && gi.csi->idFloor < MAX_CONTAINERS);

	/* successful inventory change; remove the item in clients */
	if (from->id == gi.csi->idFloor) {
		/* We removed an item from the floor - check how the client
		 * needs to be updated. */
		assert(!newFloor);
		if (FLOOR(ent)) {
			/* There is still something on the floor. */
			/** @todo @b why do we do this here exactly? Shouldn't they be the
			 * same already at this point? */
			FLOOR(floor) = FLOOR(ent);
			/* Tell the client to remove the item from the container */
			gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_DEL);
			gi.WriteShort(floor->number);
			gi.WriteByte(from->id);
			gi.WriteByte(fx);
			gi.WriteByte(fy);
		} else {
			/* Floor is empty, remove the edict (from server + client) if we are
			 * not moving to it. */
			if (to->id != gi.csi->idFloor) {
				gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
				gi.WriteShort(floor->number);
				G_FreeEdict(floor);
			}
		}
	} else {
		/* Tell the client to remove the item from the container */
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_DEL);
		gi.WriteShort(num);
		gi.WriteByte(from->id);
		gi.WriteByte(fx);
		gi.WriteByte(fy);
	}

	/* send tu's */
	G_SendStats(ent);

	item = ic->item;

	if (ia == IA_RELOAD || ia == IA_RELOAD_SWAP) {
		/* reload */
		if (to->id == gi.csi->idFloor) {
			assert(!newFloor);
			assert(FLOOR(floor) == FLOOR(ent));
			mask = G_VisToPM(floor->visflags);
		} else {
			mask = G_TeamToPM(ent->team);
		}

		/* send ammo message to all --- it's fun to hear that sound */
		gi.AddEvent(PM_ALL, EV_INV_RELOAD);
		gi.WriteShort(to->id == gi.csi->idFloor ? floor->number : num);
		gi.WriteByte(item.t->ammo);
		gi.WriteByte(item.m->idx);
		gi.WriteByte(to->id);
		gi.WriteByte(ic->x);
		gi.WriteByte(ic->y);

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
	if (to->id == gi.csi->idFloor) {
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
			 * is already linked might be from a differnet entity */
			FLOOR(floor) = FLOOR(ent);
			/* Tell the client to add the item to the container. */
			gi.AddEvent(G_VisToPM(floor->visflags), EV_INV_ADD);
			gi.WriteShort(floor->number);
			gi.WriteShort(INV_INVENTORY_BYTES);
			G_WriteItem(item, to, tx, ty);
		}
	} else {
		/* Tell the client to add the item to the container. */
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_ADD);
		gi.WriteShort(num);
		gi.WriteShort(INV_INVENTORY_BYTES);
		G_WriteItem(item, to, tx, ty);
	}

	/* Update reaction firemode when something is moved from/to a hand. */
	if (from->id == gi.csi->idRight || to->id == gi.csi->idRight) {
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
		gi.WriteShort(num);
		gi.WriteShort(0); /**< hand = right */
	} else if (from->id == gi.csi->idLeft || to->id == gi.csi->idLeft) {
		gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
		gi.WriteShort(num);
		gi.WriteShort(1); /**< hand = left */
	}

	/* Other players receive weapon info only. */
	mask = G_VisToPM(ent->visflags) & ~G_TeamToPM(ent->team);
	if (mask) {
		if (from->id == gi.csi->idRight || from->id == gi.csi->idLeft) {
			gi.AddEvent(mask, EV_INV_DEL);
			gi.WriteShort(num);
			gi.WriteByte(from->id);
			gi.WriteByte(fx);
			gi.WriteByte(fy);
		}
		if (to->id == gi.csi->idRight || to->id == gi.csi->idLeft) {
			gi.AddEvent(mask, EV_INV_ADD);
			gi.WriteShort(num);
			gi.WriteShort(INV_INVENTORY_BYTES);
			G_WriteItem(item, to, tx, ty);
		}
	}
	gi.EndEvents();
}

/** @brief Move items to adjacent locations if the containers on the current
 * floor edict are full */
/* #define ADJACENT */

/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 * @param[in] ent Pointer to an edict_t being an actor.
 * @sa G_ActorDie
 */
static void G_InventoryToFloor (edict_t * ent)
{
	invList_t *ic, *next;
	int k;
	edict_t *floor;
#ifdef ADJACENT
	edict_t *floorAdjacent;
	int i;
#endif

	/* check for items */
	for (k = 0; k < gi.csi->numIDs; k++)
		if (k != gi.csi->idArmour && ent->i.c[k])
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

		/* skip csi->idArmour, we will collect armours using idArmour container,
		 * not idFloor */
		if (k == gi.csi->idArmour)
			continue;

		/* now cycle through all items for the container of the character (or the entity) */
		for (ic = ent->i.c[k]; ic; ic = next) {
			int x, y;
#ifdef ADJACENT
			vec2_t oldPos; /* if we have to place it to adjacent  */
#endif
			/* Save the next inv-list before it gets overwritten below.
			 * Do not put this in the "for" statement,
			 * unless you want an endless loop. ;) */
			next = ic->next;
			/* find the coordinates for the current item on floor */
			Com_FindSpace(&floor->i, &ic->item, &gi.csi->ids[gi.csi->idFloor], &x, &y, ic);
			if (x == NONE) {
				assert(y == NONE);
				/* Run out of space on the floor or the item is armour
				 * destroy the offending item if no adjacent places are free */
				/* store pos for later restoring the original value */
#ifdef ADJACENT
				Vector2Copy(ent->pos, oldPos);
				for (i = 0; i < DIRECTIONS; i++) {
					/** @todo Check whether movement is possible here - otherwise don't use this field */
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

					Com_FindSpace(&floorAdjacent->i, &ic->item, &gi.csi->ids[gi.csi->idFloor], &x, &y, ic);
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
				Com_RemoveFromInventory(&ent->i, &gi.csi->ids[k], ic);
			} else {
				ic->x = x;
				ic->y = y;
				ic->next = FLOOR(floor);
				FLOOR(floor) = ic;
			}
		}

		/* destroy link */
		ent->i.c[k] = NULL;
	}

	FLOOR(ent) = FLOOR(floor);

	/* send item info to the clients */
	G_CheckVis(floor, qtrue);
#ifdef ADJACENT
	if (floorAdjacent)
		G_CheckVis(floorAdjacent, qtrue);
#endif
}

pos_t *fb_list[MAX_FORBIDDENLIST];
int fb_length;

/**
 * @brief Build the forbidden list for the pathfinding (server side).
 * @param[in] team The team number if the list should be calculated from the eyes of that team. Use 0 to ignore team.
 * @note The forbidden list (fb_list and fb_length) is a
 * list of entity positions that are occupied by an entity.
 * This list is checked everytime an actor wants to walk there.
 * @sa G_MoveCalc
 * @sa Grid_CheckForbidden
 * @sa CL_BuildForbiddenList <- shares quite some code
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void G_BuildForbiddenList (int team)
{
	edict_t *ent;
	int vis_mask;
	int i;

	fb_length = 0;

	/* team visibility */
	if (team)
		vis_mask = 1 << team;
	else
		vis_mask = PM_ALL;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->inuse)
			continue;
		/* Dead 2x2 unit will stop walking, too. */
		/**
		 * @todo Just a note for the future.
		 * If we get any that does not block the map when dead this is the place to look.
		 */
		if (((ent->type == ET_ACTOR && !G_IsDead(ent)) || ent->type == ET_ACTOR2x2) && (ent->visflags & vis_mask)) {
			fb_list[fb_length++] = ent->pos;
			fb_list[fb_length++] = (byte*) &ent->fieldSize;
		} else if (ent->type == ET_SOLID) {
			fb_list[fb_length++] = ent->pos;
			/** @todo get fieldsize from bounding box (ent->mins, ent->maxs) */
			fb_list[fb_length++] = (byte*) &ent->fieldSize;
		}
	}

	if (fb_length > MAX_FORBIDDENLIST)
		gi.error("G_BuildForbiddenList: list too long\n");
}

/**
 * @brief Precalculates a move table for a given team and a given starting position.
 * This will calculate a routing table for all reachable fields with the given distance
 * from the given spot with the given actorsize
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] actorSize
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] crouchingState The crouching state of the actor. 0=stand, 1=crouch
 * @param[in] distance The distance to calculate the move for.
 * @sa G_BuildForbiddenList
 */
void G_MoveCalc (int team, pos3_t from, int actorSize, byte crouchingState, int distance)
{
	G_BuildForbiddenList(team);
	gi.MoveCalc(gi.routingMap, actorSize, gi.pathingMap, from, crouchingState, distance, fb_list, fb_length);
}

#define ACTOR_SPEED_NORMAL 100
#define ACTOR_SPEED_CROUCHED (ACTOR_SPEED_NORMAL / 2)

/**
 * @brief Generates the client events that are send over the netchannel to move an actor
 * @param[in] player Player who is moving an actor
 * @param[in] visTeam
 * @param[in] num Edict index to move
 * @param[in] to The grid position to walk to
 * @param[in] stopOnVisStop qfalse means that VIS_STOP is ignored
 * @param[in] quiet Don't print the console message (G_ActionCheck) if quiet is true.
 * @sa CL_ActorStartMove
 * @sa PA_MOVE
 */
void G_ClientMove (player_t * player, int visTeam, int num, pos3_t to, qboolean stopOnVisStop, qboolean quiet)
{
	edict_t *ent;
	int status, initTU;
	byte dvtab[MAX_DVTAB];
	byte olddvtab[MAX_DVTAB];
	int dv, dir;
	byte numdv, length;
	pos3_t pos;
	float div, truediv, tu;
	int contentFlags;
	vec3_t pointTrace;
	byte* stepAmount = NULL;
	qboolean triggers = qfalse;
	edict_t* clientAction;
	int oldState;
	qboolean autoCrouchRequired = qfalse;
	byte crouchingState;

	ent = g_edicts + num;
	crouchingState = ent->state & STATE_CROUCHED ? 1 : 0;
	oldState = 0;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_MOVE_STRAIGHT, quiet))
		return;

	/* calculate move table */
	G_MoveCalc(visTeam, ent->pos, ent->fieldSize, crouchingState, MAX_ROUTE);
	length = gi.MoveLength(gi.pathingMap, to, crouchingState, qfalse);

	/* length of ROUTING_NOT_REACHABLE means not reachable */
	if (length && length < ROUTING_NOT_REACHABLE) {
		/* Autostand: check if the actor is crouched and player wants autostanding...*/
		if ((ent->state & STATE_CROUCHED) && player->autostand) {
			/* ...and if this is a long walk... */
			if ((float) (2 * TU_CROUCH) < (float) length * (TU_CROUCH_MOVING_FACTOR - 1.0f)) {
				/* ...make them stand first. If the player really wants them to walk a long
				 * way crouched, he can move the actor in several stages.
				 * Uses the threshold at which standing, moving and crouching again takes
				 * fewer TU than just crawling while crouched. */
				G_ClientStateChange(player, num, STATE_CROUCHED, qtrue); /* change to stand state */
				autoCrouchRequired = qtrue;
			}
		}

		/* slower if crouched */
		if (ent->state & STATE_CROUCHED)
			ent->speed = ACTOR_SPEED_CROUCHED;
		else
			ent->speed = ACTOR_SPEED_NORMAL;
		ent->speed *= g_actorspeed->value;
		/* this let the footstep sounds play even over network */
		ent->think = G_PhysicsStep;
		ent->nextthink = level.time;

		/* assemble dv-encoded move data */
		VectorCopy(to, pos);
		numdv = 0;
		tu = 0;
		initTU = ent->TU;

		while ((dv = gi.MoveNext(gi.routingMap, ent->fieldSize, gi.pathingMap, pos, crouchingState))
				!= ROUTING_UNREACHABLE) {
			const int oldZ = pos[2];
			/* dv indicates the direction traveled to get to the new cell and the original cell height. */
			/* dv = (dir << 3) | z */
			if (numdv >= MAX_DVTAB) {
				gi.GridDumpDVTable(gi.pathingMap);
				for (numdv = 0; numdv < MAX_DVTAB; numdv++)
					gi.dprintf(" %i", olddvtab[numdv]);
				gi.error("G_ClientMove: numdv == %i (%i %i %i) ", numdv, to[0], to[1], to[2]);
			}
			olddvtab[numdv] = dv;
			PosSubDV(pos, crouchingState, dv); /* We are going backwards to the origin. */
			dvtab[numdv++] = NewDVZ(dv, oldZ); /* Replace the z portion of the DV value so we can get back to where we were. */
		}

		if (VectorCompare(pos, ent->pos)) {
			/* everything ok, found valid route */

			/* no floor inventory at this point */
			FLOOR(ent) = NULL;

			while (numdv > 0) {
				/* get next dv */
				numdv--;
				dv = dvtab[numdv];
				dir = getDVdir(dv); /**< This is the direction */

				/* turn around first */
				status = G_DoTurn(ent, dir);
				if (stopOnVisStop && (status & VIS_STOP))
					break;

				/* decrease TUs */
				/* moveDiagonal = !((dvtab[numdv] & (DIRECTIONS - 1)) < 4); */
				div = gi.TUsUsed(dir);
				truediv = div;
				if (ent->state & STATE_CROUCHED && dir < CORE_DIRECTIONS)
					div *= TU_CROUCH_MOVING_FACTOR;
				if ((int) (tu + div) > ent->TU)
					break;
				tu += div;

				/* move */
				crouchingState = 0; /* This is now a flag to indicate a change in crouching */
				PosAddDV(ent->pos, crouchingState, dv);
				if (crouchingState == 0) { /* No change in crouch */
					gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
					VectorCopy(ent->origin, pointTrace);
					pointTrace[2] += PLAYER_MIN;

					contentFlags = gi.PointContents(pointTrace);

					/* link it at new position - this must be done for every edict
					 * movement - to let the server know about it. */
					gi.LinkEdict(ent);

					/* Only the PHALANX team has these stats right now. */
					if (ent->chr.scoreMission) {
						if (ent->state & STATE_CROUCHED)
							ent->chr.scoreMission->movedCrouched += truediv;
						else
							ent->chr.scoreMission->movedNormal += truediv;
					}

					/* write move header if not yet done */
					if (gi.GetEvent() != EV_ACTOR_MOVE) {
						gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
						gi.WriteShort(num);
						/* stepAmount is a pointer to a location in the netchannel
						 * the value of this pointer depends on how far the actor walks
						 * and this might be influenced at a later stage inside this
						 * loop. That's why we can modify the value of this byte
						 * if e.g. a VIS_STOP occurred and no more steps should be made.
						 * But keep in mind, that no other events might be between
						 * this event and its successful end - otherwise the
						 * stepAmount pointer would no longer be valid and you would
						 * modify data in the new event. */
						stepAmount = gi.WriteDummyByte(0);
						/* Add three more dummy bytes.  These will be the final actor position. */
						gi.WriteDummyByte(0); /* x */
						gi.WriteDummyByte(0); /* y */
						gi.WriteDummyByte(0); /* z */
					}

					/* the moveinfo stuff is used inside the G_PhysicsStep think function */
					if (ent->moveinfo.steps >= MAX_DVTAB) {
						ent->moveinfo.steps = 0;
						ent->moveinfo.currentStep = 0;
					}
					ent->moveinfo.contentFlags[ent->moveinfo.steps] = contentFlags;
					ent->moveinfo.visflags[ent->moveinfo.steps] = ent->visflags;
					ent->moveinfo.steps++;

					/* store steps in netchannel */
					(*stepAmount)++;
					/* store the position too */
					*(stepAmount + 1) = ent->pos[0];
					*(stepAmount + 2) = ent->pos[1];
					*(stepAmount + 3) = ent->pos[2];

					/* write move header and always one step after another - because the next step
					 * might already be the last one due to some stop event */
					gi.WriteByte(dv);
					gi.WriteShort(ent->speed);
					gi.WriteShort(contentFlags);

					/* check if player appears/perishes, seen from other teams */
					G_CheckVis(ent, qtrue);

					/* check for anything appearing, seen by "the moving one" */
					status = G_CheckVisTeam(ent->team, NULL, qfalse, ent);

					/* Set ent->TU because the reaction code relies on ent->TU being accurate. */
					ent->TU = max(0, initTU - (int) tu);

					clientAction = ent->client_action;
					oldState = ent->state;
					/* check triggers at new position but only if no actor appeared */
					if (G_TouchTriggers(ent)) {
						triggers = qtrue;
						if (!clientAction)
							status |= VIS_STOP;
					}
					/* state has changed - maybe we walked on a trigger_hurt */
					if (oldState != ent->state)
						status |= VIS_STOP;
				} else if (crouchingState == 1) { /* Actor is standing */
					G_ClientStateChange(player, num, STATE_CROUCHED, qtrue);
				} else { /* Actor is crouching */
					G_ClientStateChange(player, num, STATE_CROUCHED, qfalse);
				}

				/* check for reaction fire */
				if (G_ReactToMove(ent, qtrue)) {
					if (G_ReactToMove(ent, qfalse))
						status |= VIS_STOP;

					autoCrouchRequired = qfalse;
					/** @todo if the attacker is invisible let the target turn in the shooting direction
					 * of the attacker (@see G_Turn) */
					/*G_DoTurn(ent->reactionTarget, dir);*/
				}

				/* Restore ent->TU because the movement code relies on it not being modified! */
				ent->TU = initTU;

				/* check for death */
				if (oldState != ent->state && !(ent->state & STATE_DAZED)) {
					/** @todo Handle dazed via trigger_hurt */
					/* maybe this was due to rf - then the G_ActorDie was already called */
					if (ent->maxs[2] != PLAYER_DEAD)
						G_ActorDie(ent, ent->HP == 0 ? STATE_DEAD : STATE_STUN, NULL);
					return;
				}

				if (stopOnVisStop && (status & VIS_STOP))
					break;
			}

			/* now we can send other events again - the EV_ACTOR_MOVE event has ended */

			/* submit the TUs / round down */
			if (g_notu != NULL && !g_notu->integer)
				ent->TU = max(0, initTU - (int) tu);

			G_SendStats(ent);

			/* only if triggers are touched - there was a client
			 * action set and there were steps made */
			if (!triggers && ent->client_action) {
				/* no triggers, no client action */
				ent->client_action = NULL;
				gi.AddEvent(G_TeamToPM(ent->team), EV_RESET_CLIENT_ACTION);
				gi.WriteShort(ent->number);
			}

			/* end the move */
			G_GetFloorItems(ent);
			gi.EndEvents();
		}

		if (autoCrouchRequired)
			/* toggle back to crouched state */
			G_ClientStateChange(player, num, STATE_CROUCHED, qtrue);
	}
}

/**
 * @brief Sends the actual actor turn event over the netchannel
 */
static void G_ClientTurn (player_t * player, int num, byte dv)
{
	const int dir = getDVdir(dv);
	edict_t *ent = g_edicts + num;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_TURN, NOISY))
		return;

	/* check if we're already facing that direction */
	if (ent->dir == dir)
		return;

	/* do the turn */
	G_DoTurn(ent, dir);
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
 * @brief Changes the state of a player/soldier.
 * @param[in,out] player The player who controlls the actor
 * @param[in] num The index of the edict in the global g_edicts array
 * @param[in] reqState The bit-map of the requested state change
 * @param[in] checkaction only activate the events - network stuff is handled in the calling function
 * don't even use the G_ActionCheck function
 * @note Use checkaction true only for e.g. spawning values
 */
void G_ClientStateChange (player_t * player, int num, int reqState, qboolean checkaction)
{
	edict_t *ent;

	ent = g_edicts + num;

	/* Check if any action is possible. */
	if (checkaction && !G_ActionCheck(player, ent, 0, NOISY))
		return;

	if (!reqState)
		return;

	switch (reqState) {
	case STATE_CROUCHED: /* Toggle between crouch/stand. */
		/* Check if player has enough TUs (TU_CROUCH TUs for crouch/uncrouch). */
		if (!checkaction || G_ActionCheck(player, ent, TU_CROUCH, NOISY)) {
			ent->state ^= STATE_CROUCHED;
			ent->TU -= TU_CROUCH;
			/* Link it. */
			if (ent->state & STATE_CROUCHED)
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH);
			else
				VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
			gi.LinkEdict(ent);
		}
		break;
	case ~STATE_REACTION: /* Request to turn off reaction fire. */
		if ((ent->state & STATE_REACTION_MANY) || (ent->state & STATE_REACTION_ONCE)) {
			if (ent->state & STATE_SHAKEN) {
				G_PlayerPrintf(player, PRINT_CONSOLE, _("Currently shaken, won't let their guard down.\n"));
			} else {
				/* Turn off reaction fire. */
				ent->state &= ~STATE_REACTION;

				if (player->pers.ai && checkaction)
					gi.error("AI reaction fire is server side only");
			}
		}
		break;
	case STATE_REACTION_MANY: /* Request to turn on multi-reaction fire mode. */
		/* Disable reaction fire. */
		ent->state &= ~STATE_REACTION;

		if (player->pers.ai && checkaction)
			gi.error("AI reaction fire is server side only");

		/* Enable multi reaction fire. */
		ent->state |= STATE_REACTION_MANY;
		break;
	case STATE_REACTION_ONCE: /* Request to turn on single-reaction fire mode. */
		/* Disable reaction fire. */
		ent->state &= ~STATE_REACTION;

		if (player->pers.ai && checkaction)
			gi.error("AI reaction fire is server side only");

		/* Turn on single reaction fire. */
		ent->state |= STATE_REACTION_ONCE;
		break;
	default:
		gi.dprintf("G_ClientStateChange: unknown request %i, ignoring\n", reqState);
		return;
	}

	/* Only activate the events - network stuff is handled in the calling function */
	if (!checkaction)
		return;

	/* Send the state change. */
	G_SendState(G_VisToPM(ent->visflags), ent);

	/* Check if the player appears/perishes, seen from other teams. */
	G_CheckVis(ent, qtrue);

	/* Calc new vis for this player. */
	G_CheckVisTeam(ent->team, NULL, qfalse, ent);

	/* Send the new TUs. */
	G_SendStats(ent);

	/* End the event. */
	gi.EndEvents();
}

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
			G_ClientInvMove(G_PLAYER_FROM_ENT(ent), ent->number, &gi.csi->ids[gi.csi->idRight], RIGHT(ent),
					&gi.csi->ids[gi.csi->idFloor], NONE, NONE, qtrue, quiet);
		if (LEFT(ent))
			G_ClientInvMove(G_PLAYER_FROM_ENT(ent), ent->number, &gi.csi->ids[gi.csi->idLeft], LEFT(ent),
					&gi.csi->ids[gi.csi->idFloor], NONE, NONE, qtrue, quiet);
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
static void G_MoraleBehaviour (int team, qboolean quiet)
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

/**
 * @brief Reload weapon with actor.
 * @sa AI_ActorThink
 */
void G_ClientReload (player_t *player, int entnum, shoot_types_t st, qboolean quiet)
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
	hand = st == ST_RIGHT_RELOAD ? &gi.csi->ids[gi.csi->idRight] : &gi.csi->ids[gi.csi->idLeft];
	bestContainer = NULL;

	if (ent->i.c[hand->id]) {
		weapon = ent->i.c[hand->id]->item.t;
	} else if (hand->id == gi.csi->idLeft && ent->i.c[gi.csi->idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = &gi.csi->ids[gi.csi->idRight];
		weapon = ent->i.c[hand->id]->item.t;
	} else
		return;

	assert(weapon);

	/* LordHavoc: Check if item is researched when in singleplayer? I don't think this is really a
	 * cheat issue as in singleplayer there is no way to inject fake client commands in the virtual
	 * network buffer, and in multiplayer everything is researched */

	for (containerID = 0; containerID < gi.csi->numIDs; containerID++) {
		if (gi.csi->ids[containerID].out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = ent->i.c[containerID]; ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)) {
					icFinal = ic;
					tu = gi.csi->ids[containerID].out;
					bestContainer = &gi.csi->ids[containerID];
					break;
				}
		}
	}

	/* send request */
	if (bestContainer)
		G_ClientInvMove(player, entnum, bestContainer, icFinal, hand, 0, 0, qtrue, quiet);
}

/**
 * @brief Returns true if actor can reload weapon
 * @sa AI_ActorThink
 */
qboolean G_ClientCanReload (player_t *player, int entnum, shoot_types_t st)
{
	edict_t *ent;
	invList_t *ic;
	int hand;
	objDef_t *weapon;
	int container;

	ent = g_edicts + entnum;

	/* search for clips and select the one that is available easily */
	hand = st == ST_RIGHT_RELOAD ? gi.csi->idRight : gi.csi->idLeft;

	if (ent->i.c[hand]) {
		weapon = ent->i.c[hand]->item.t;
	} else if (hand == gi.csi->idLeft && ent->i.c[gi.csi->idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = gi.csi->idRight;
		weapon = ent->i.c[hand]->item.t;
	} else
		return qfalse;

	assert(weapon);

	for (container = 0; container < gi.csi->numIDs; container++)
		for (ic = ent->i.c[container]; ic; ic = ic->next)
			if (INVSH_LoadableInWeapon(ic->item.t, weapon))
				return qtrue;
	return qfalse;
}

/**
 * @brief Retrieve or collect weapon from any linked container for the actor
 * @note This function will also collect items from floor containers when the actor
 * is standing on a given point.
 * @sa AI_ActorThink
 */
void G_ClientGetWeaponFromInventory (player_t *player, int entnum, qboolean quiet)
{
	edict_t *ent;
	invList_t *ic;
	invList_t *icFinal;
	invDef_t *hand;
	int tu;
	int container;
	invDef_t *bestContainer;

	ent = g_edicts + entnum;
	/* e.g. bloodspiders are not allowed to carry or collect weapons */
	if (!ent->chr.teamDef->weapons)
		return;

	/* search for weapons and select the one that is available easily */
	tu = 100;
	hand = &gi.csi->ids[gi.csi->idRight];
	bestContainer = NULL;
	icFinal = NULL;

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (gi.csi->ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = ent->i.c[container]; ic; ic = ic->next) {
				assert(ic->item.t);
				if (ic->item.t->weapon && (ic->item.a > 0 || !ic->item.t->reload)) {
					icFinal = ic;
					tu = gi.csi->ids[container].out;
					bestContainer = &gi.csi->ids[container];
					break;
				}
			}
		}
	}

	/* send request */
	if (bestContainer)
		G_ClientInvMove(player, entnum, bestContainer, icFinal, hand, 0, 0, qtrue, quiet);
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
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_DIE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);

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
 * @brief This function 'uses' the edict. E.g. it opens the door when the player wants it to open
 * @sa PA_USE_DOOR
 * @param[in] player The player is trying to activate the door
 * @param[in,out] actor The actor the player is using to activate the entity
 * @param[in,out] edict The entity that is to be used
 * @todo Do we have to change the trigger position here, too? I don't think this is really needed.
 * @sa CL_ActorUseDoor
 * @sa G_UseEdict
 */
qboolean G_ClientUseEdict (player_t *player, edict_t *actor, edict_t *edict)
{
	/* check whether the actor has sufficient TUs to 'use' this edicts */
	if (!G_ActionCheck(player, actor, edict->TU, qfalse))
		return qfalse;

	if (!G_UseEdict(edict))
		return qfalse;

	/* using a group of edicts only costs TUs once (for the master) */
	actor->TU -= edict->TU;
	/* send the new TUs */
	G_SendStats(actor);

	gi.EndEvents();

	return qtrue;
}

/**
 * @brief The client sent us a message that he did something. We now execute the related fucntion(s) adn notify him if neccessary.
 * @param[in] player The player to execute the action for (the actor belongs to this player)
 * @note a client action will also send the server side edict number to determine the actor
 */
int G_ClientAction (player_t * player)
{
	player_action_t action;
	int num;
	pos3_t pos;
	int i;
	int firemode;
	int from, fx, fy, to, tx, ty;
	int hand, fdIdx, objIdx;
	int resType, resState, resValue;
	edict_t *ent;

	/* read the header */
	action = gi.ReadByte();
	num = gi.ReadShort();

	if (num < 0 || num >= MAX_EDICTS) {
		gi.dprintf("Invalid edict num %i\n", num);
		return action;
	}

	switch (action) {
	case PA_NULL:
		/* do nothing on a null action */
		break;

	case PA_TURN:
		gi.ReadFormat(pa_format[PA_TURN], &i);
		G_ClientTurn(player, num, (byte) i);
		break;

	case PA_MOVE:
		gi.ReadFormat(pa_format[PA_MOVE], &pos);
		G_ClientMove(player, player->pers.team, num, pos, qtrue, NOISY);
		break;

	case PA_STATE:
		gi.ReadFormat(pa_format[PA_STATE], &i);
		G_ClientStateChange(player, num, i, qtrue);
		break;

	case PA_SHOOT:
		gi.ReadFormat(pa_format[PA_SHOOT], &pos, &i, &firemode, &from);
		(void) G_ClientShoot(player, num, pos, i, firemode, NULL, qtrue, from);
		break;

	case PA_INVMOVE:
		gi.ReadFormat(pa_format[PA_INVMOVE], &from, &fx, &fy, &to, &tx, &ty);

		ent = g_edicts + num;

		if (from < 0 || from >= gi.csi->numIDs || to < 0 || to >= gi.csi->numIDs) {
			gi.dprintf("G_ClientAction: PA_INVMOVE Container index out of range. (from: %i, to: %i)\n", from, to);
		} else {
			invDef_t *fromPtr = &gi.csi->ids[from];
			invDef_t *toPtr = &gi.csi->ids[to];
			invList_t *fromItem = Com_SearchInInventory(&ent->i, fromPtr, fx, fy);
			assert(fromItem);
			G_ClientInvMove(player, num, fromPtr, fromItem, toPtr, tx, ty, qtrue, NOISY);
		}
		break;

	case PA_USE_DOOR: {
		edict_t *door;
		edict_t *actor = (g_edicts + num);

		/* read the door the client wants to open */
		gi.ReadFormat(pa_format[PA_USE_DOOR], &i);

		/* get the door edict */
		door = g_edicts + i;

		if (actor->client_action == door) {
			/* check whether it's part of an edict group but not the master */
			if (door->flags & FL_GROUPSLAVE)
				door = door->groupMaster;

			G_ClientUseEdict(player, actor, door);
		} else
			Com_DPrintf(DEBUG_GAME, "client_action and ent differ: %i - %i\n", actor->client_action->number,
					door->number);
	}
		break;

	case PA_REACT_SELECT:
		gi.ReadFormat(pa_format[PA_REACT_SELECT], &hand, &fdIdx, &objIdx);
		Com_DPrintf(DEBUG_GAME, "G_ClientAction: entnum:%i hand:%i fd:%i obj:%i\n", num, hand, fdIdx, objIdx);
		ent = g_edicts + num;
		ent->chr.RFmode.hand = hand;
		ent->chr.RFmode.fmIdx = fdIdx;
		ent->chr.RFmode.weapon = &gi.csi->ods[objIdx];
		break;

	case PA_RESERVE_STATE:
		gi.ReadFormat(pa_format[PA_RESERVE_STATE], &resType, &resState, &resValue);

		if (resValue < 0)
			gi.error("G_ClientAction: No sane value received for resValue!  (resType=%i resState=%i resValue=%i)\n",
					resType, resState, resValue);

		ent = g_edicts + num;
		switch (resType) {
		case RES_REACTION:
			ent->chr.reservedTus.reserveReaction = resState;
			ent->chr.reservedTus.reaction = resValue;
			break;
		default:
			gi.error("G_ClientAction: Unknown reservation type (on the server-side)!\n");
		}
		break;

	default:
		gi.error("G_ClientAction: Unknown action!\n");
	}
	return action;
}

/**
 * @brief Sets the teamnum var for this match
 * @param[in] player Pointer to connected player
 * @todo Check whether there are enough free spawnpoints in all cases
 */
static void G_GetTeam (player_t * player)
{
	player_t *p;
	int i, j;
	int playersInGame = 0;

	/* number of currently connected players (no ai players) */
	for (j = 0, p = game.players; j < game.sv_maxplayersperteam; j++, p++)
		if (p->inuse)
			playersInGame++;

	/* player has already a team */
	if (player->pers.team > 0) {
		Com_DPrintf(DEBUG_GAME, "You are already on team %i\n", player->pers.team);
		return;
	}

	/* randomly assign a teamnumber in deathmatch games */
	if (playersInGame <= 1 && sv_maxclients->integer > 1 && !sv_teamplay->integer) {
		int spawnCheck[MAX_TEAMS];
		int spawnSpots = 0;
		int randomSpot;
		/* skip civilian teams */
		for (i = TEAM_PHALANX; i < MAX_TEAMS; i++) {
			spawnCheck[i] = 0;
			/* check whether there are spawnpoints for this team */
			if (level.num_spawnpoints[i])
				spawnCheck[spawnSpots++] = i;
		}
		/* we need at least 2 different team spawnpoints for multiplayer */
		if (spawnSpots <= 1) {
			Com_DPrintf(DEBUG_GAME, "G_GetTeam: Not enough spawn spots in map!\n");
			player->pers.team = TEAM_NO_ACTIVE;
			return;
		}
		/* assign random valid team number */
		randomSpot = (int) (frand() * (spawnSpots - 1) + 0.5);
		player->pers.team = spawnCheck[randomSpot];
		gi.dprintf("You have been randomly assigned to team %i\n", player->pers.team);
		return;
	}

	/* find a team */
	if (sv_maxclients->integer == 1)
		player->pers.team = TEAM_PHALANX;
	else if (sv_teamplay->integer) {
		/* set the team specified in the userinfo */
		gi.dprintf("Get a team for teamplay for %s\n", player->pers.netname);
		i = atoi(Info_ValueForKey(player->pers.userinfo, "cl_teamnum"));
		/* civilians are at team zero */
		if (i > TEAM_CIVILIAN && sv_maxteams->integer >= i) {
			player->pers.team = i;
			gi.BroadcastPrintf(PRINT_CHAT, "serverconsole: %s has chosen team %i\n", player->pers.netname, i);
		} else {
			gi.dprintf("Team %i is not valid - choose a team between 1 and %i\n", i, sv_maxteams->integer);
			player->pers.team = TEAM_DEFAULT;
		}
	} else {
		qboolean teamAvailable;
		/* search team */
		gi.dprintf("Getting a multiplayer team for %s\n", player->pers.netname);
		for (i = TEAM_CIVILIAN + 1; i < MAX_TEAMS; i++) {
			if (level.num_spawnpoints[i]) {
				teamAvailable = qtrue;
				/* check if team is in use (only human controlled players) */
				for (j = 0, p = game.players; j < game.sv_maxplayersperteam; j++, p++)
					if (p->inuse && p->pers.team == i) {
						Com_DPrintf(DEBUG_GAME, "Team %i is already in use\n", i);
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
			for (j = 0, p = game.players + game.sv_maxplayersperteam; j < game.sv_maxplayersperteam; j++, p++)
				if (p->inuse && p->pers.team == i) {
					gi.BroadcastPrintf(PRINT_CONSOLE, "Removing ai player...");
					p->inuse = qfalse;
					break;
				}
			Com_DPrintf(DEBUG_GAME, "Assigning %s to Team %i\n", player->pers.netname, i);
			player->pers.team = i;
		} else {
			gi.dprintf("No free team - disconnecting '%s'\n", player->pers.netname);
			G_ClientDisconnect(player);
		}
	}
}

/**
 * @brief Returns the assigned team number of the player
 */
int G_ClientGetTeamNum (const player_t * player)
{
	assert(player);
	return player->pers.team;
}

/**
 * @brief Returns the preferred team number for the player
 */
int G_ClientGetTeamNumPref (const player_t * player)
{
	assert(player);
	return atoi(Info_ValueForKey(player->pers.userinfo, "cl_teamnum"));
}

/**
 * @brief Tests if all teams are connected for a multiplayer match and if so,
 * randomly assigns the first turn to a team.
 * @sa SVCmd_StartGame_f
 */
static void G_ClientTeamAssign (const player_t * player)
{
	int i, j, teamCount = 1;
	int playerCount = 0;
	int knownTeams[MAX_TEAMS];
	player_t *p;
	knownTeams[0] = player->pers.team;

	/* return with no action if activeTeam already assigned or if in single-player mode */
	if (G_GameRunning() || sv_maxclients->integer == 1)
		return;

	/* count number of currently connected unique teams and players (human controlled players only) */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++) {
		if (p->inuse && p->pers.team > 0) {
			playerCount++;
			for (j = 0; j < teamCount; j++) {
				if (p->pers.team == knownTeams[j])
					break;
			}
			if (j == teamCount)
				knownTeams[teamCount++] = p->pers.team;
		}
	}

	Com_DPrintf(DEBUG_GAME, "G_ClientTeamAssign: Players in game: %i, Unique teams in game: %i\n", playerCount,
			teamCount);

	/* if all teams/players have joined the game, randomly assign which team gets the first turn */
	if ((sv_teamplay->integer && teamCount >= sv_maxteams->integer) || playerCount >= sv_maxclients->integer) {
		char buffer[MAX_VAR] = "";
		G_PrintStats(va("Starting new game: %s with %i teams", level.mapname, teamCount));
		level.activeTeam = knownTeams[(int) (frand() * (teamCount - 1) + 0.5)];
		for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++) {
			if (p->inuse) {
				if (p->pers.team == level.activeTeam) {
					Q_strcat(buffer, p->pers.netname, sizeof(buffer));
					Q_strcat(buffer, " ", sizeof(buffer));
				} else
					/* all the others are set to waiting */
					p->ready = qtrue;
				if (p->pers.team)
					G_PrintStats(va("Team %i: %s", p->pers.team, p->pers.netname));
			}
		}
		G_PrintStats(va("Team %i got the first round", level.activeTeam));
		gi.BroadcastPrintf(PRINT_CONSOLE, _("Team %i (%s) will get the first turn.\n"), level.activeTeam, buffer);
	}
}

/**
 * @brief Find valid actor spawn fields for this player.
 * @note Already used spawn-point are not found because ent->type is changed in G_ClientTeamInfo.
 * @param[in] player The player to spawn the actors for.
 * @param[in] spawnType The type of spawn-point so search for (ET_ACTORSPAWN or ET_ACTOR2x2SPAWN)
 * @return A pointer to a found spawn point or NULL if nothing was found or on error.
 */
static edict_t *G_ClientGetFreeSpawnPoint (const player_t * player, int spawnType)
{
	int i;
	edict_t *ent;

	/* Abort for non-spawnpoints */
	assert(spawnType == ET_ACTORSPAWN || spawnType == ET_ACTOR2x2SPAWN);

	if (level.randomSpawn) {
		edict_t *list[MAX_EDICTS];
		int count = 0;
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->type == spawnType && player->pers.team == ent->team)
				list[count++] = ent;

		if (count)
			return list[rand() % count];
	} else {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->type == spawnType && player->pers.team == ent->team)
				return ent;
	}

	return NULL;
}

/**
 * @brief Call this if you want to skip some actor netchannel data
 * @note The fieldsize is not skipped
 * @sa G_ClientTeamInfo
 */
static void G_ClientSkipActorInfo (void)
{
	int k, j;

	gi.ReadShort(); /* ucn */
	for (k = 0; k < 4; k++)
		gi.ReadString(); /* name, path, body, head */
	gi.ReadByte(); /* skin */

	gi.ReadShort(); /* HP */
	gi.ReadShort(); /* maxHP */
	gi.ReadByte(); /* teamDef->idx */
	gi.ReadByte(); /* gender */
	gi.ReadByte(); /* STUN */
	gi.ReadByte(); /* morale */

	/** Scores @sa inv_shared.h:chrScoreGlobal_t */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.ReadLong(); /* score.experience */
	for (k = 0; k < SKILL_NUM_TYPES; k++)
		gi.ReadByte(); /* score.skills */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.ReadByte(); /* score.initialSkills */
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.ReadShort(); /* score.kills */
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.ReadShort(); /* score.stuns */
	gi.ReadShort(); /* score.assigned missions */

	gi.ReadShort(); /* reservedTus.reserveReaction */

	/* skip inventory */
	j = gi.ReadShort();
	for (k = 0; k < j; k++)
		gi.ReadByte(); /* inventory */
}

/**
 * @brief The client lets the server spawn the actors for a given player by sending their information (models, inventory, etc..) over the network.
 * @param[in] player The player to spawn the actors for.
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa clc_teaminfo
 * @sa G_EndGame
 */
void G_ClientTeamInfo (player_t * player)
{
	int i, k;
	int x, y;
	item_t item;
	const int length = gi.ReadByte(); /* Get the actor amount that the client sent. */

	/* find a team */
	G_GetTeam(player);

	for (i = 0; i < length; i++) {
		/* Search for a spawn point for each entry the client sent
		 * Don't allow spawning of soldiers if:
		 * + the player is not in a valid team
		 * +++ Multiplayer
		 * + the game is already running (activeTeam != -1)
		 * + the sv_maxsoldiersperplayer limit is hit (e.g. the assembled team is bigger than the allowed number of soldiers)
		 * + the team already hit the max allowed amount of soldiers */
		if (player->pers.team != TEAM_NO_ACTIVE && (sv_maxclients->integer == 1 || (!G_GameRunning() && i
				< sv_maxsoldiersperplayer->integer && level.num_spawned[player->pers.team]
				< sv_maxsoldiersperteam->integer))) {
			/* Here the client tells the server the information for the spawned actor(s). */
			edict_t *ent;

			/* Receive fieldsize and get matching spawnpoint. */
			const int dummyFieldSize = gi.ReadByte();
			switch (dummyFieldSize) {
			case ACTOR_SIZE_NORMAL:
				/* Find valid actor spawn fields for this player. */
				ent = G_ClientGetFreeSpawnPoint(player, ET_ACTORSPAWN);
				if (ent) {
					ent->type = ET_ACTOR;
				}
				break;
			case ACTOR_SIZE_2x2:
				/* Find valid actor spawn fields for this player. */
				ent = G_ClientGetFreeSpawnPoint(player, ET_ACTOR2x2SPAWN);
				if (ent) {
					ent->type = ET_ACTOR2x2;
					ent->morale = 100;
				}
				break;
			default:
				gi.error("G_ClientTeamInfo: unknown fieldSize for actor edict (actorSize: %i, actor num: %i)\n",
						dummyFieldSize, i);
			}

			if (!ent) {
				Com_DPrintf(DEBUG_GAME,
						"G_ClientTeamInfo: Could not spawn actor because no useable spawn-point is available (%i)\n",
						dummyFieldSize);
				G_ClientSkipActorInfo();
				continue;
			}

			level.num_alive[ent->team]++;
			level.num_spawned[ent->team]++;
			ent->pnum = player->num;

			ent->chr.fieldSize = dummyFieldSize;
			ent->fieldSize = ent->chr.fieldSize;

			Com_DPrintf(DEBUG_GAME, "Player: %i - team %i - size: %i\n", player->num, ent->team, ent->fieldSize);

			gi.LinkEdict(ent);

			/* model */
			ent->chr.ucn = gi.ReadShort();
			Q_strncpyz(ent->chr.name, gi.ReadString(), sizeof(ent->chr.name));
			Q_strncpyz(ent->chr.path, gi.ReadString(), sizeof(ent->chr.path));
			Q_strncpyz(ent->chr.body, gi.ReadString(), sizeof(ent->chr.body));
			Q_strncpyz(ent->chr.head, gi.ReadString(), sizeof(ent->chr.head));
			ent->chr.skin = gi.ReadByte();

			Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: name: %s, path: %s, body: %s, head: %s, skin: %i\n",
					ent->chr.name, ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.skin);

			ent->chr.HP = gi.ReadShort();
			ent->chr.minHP = ent->chr.HP;
			ent->chr.maxHP = gi.ReadShort();
			ent->chr.teamDef = &gi.csi->teamDef[gi.ReadByte()];

			ent->chr.gender = gi.ReadByte();
			ent->chr.STUN = gi.ReadByte();
			ent->chr.morale = gi.ReadByte();

			/** Scores @sa G_ClientSkipActorInfo */
			for (k = 0; k < SKILL_NUM_TYPES + 1; k++) /* new attributes */
				ent->chr.score.experience[k] = gi.ReadLong();
			for (k = 0; k < SKILL_NUM_TYPES; k++) /* new attributes */
				ent->chr.score.skills[k] = gi.ReadByte();
			for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
				ent->chr.score.initialSkills[k] = gi.ReadByte();
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				ent->chr.score.kills[k] = gi.ReadShort();
			for (k = 0; k < KILLED_NUM_TYPES; k++)
				ent->chr.score.stuns[k] = gi.ReadShort();
			ent->chr.score.assignedMissions = gi.ReadShort();

			/* Read user-defined reaction-state. */
			ent->chr.reservedTus.reserveReaction = gi.ReadShort();

			/* Mission Scores */
			memset(&scoreMission[scoreMissionNum], 0, sizeof(scoreMission[scoreMissionNum]));
			ent->chr.scoreMission = &scoreMission[scoreMissionNum];
			scoreMissionNum++;

			/* inventory */
			{
				int nr = gi.ReadShort() / INV_INVENTORY_BYTES;

				for (; nr-- > 0;) {
					invDef_t *container;
					G_ReadItem(&item, &container, &x, &y);
					Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: t=%i:a=%i:m=%i (x=%i:y=%i)\n", (item.t ? item.t->idx
							: NONE), item.a, (item.m ? item.m->idx : NONE), x, y);

					if (container) {
						Com_AddToInventory(&ent->i, item, container, x, y, 1);
						Com_DPrintf(DEBUG_GAME, "G_ClientTeamInfo: (container: %i - idArmour: %i) <- Added %s.\n",
								container->id, gi.csi->idArmour, ent->i.c[container->id]->item.t->id);
					}
				}
			}

			/* set models */
			/** @todo is this copy needed? - wouldn't it be enough to use the inventory from character_t? */
			ent->chr.inv = ent->i;
			ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
			ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));

			/* set initial vital statistics */
			ent->HP = ent->chr.HP;
			ent->morale = ent->chr.morale;

			/** @todo for now, heal fully upon entering mission */
			ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);

			ent->reaction_minhit = 30; /** @todo allow later changes from GUI */
		} else {
			/* just do nothing with the info */
			gi.ReadByte(); /* fieldSize */
			G_ClientSkipActorInfo();
		}
	}
	G_ClientTeamAssign(player);
}

/**
 * @brief Counts the still living actors for a player
 */
static int G_PlayerSoldiersCount (const player_t* player)
{
	int i, cnt = 0;
	edict_t *ent;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->pnum == player->num)
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
		gi.BroadcastPrintf(PRINT_HUD, _("4 minutes left until forced round end\n"));
		return;
	case 180:
		gi.BroadcastPrintf(PRINT_HUD, _("3 minutes left until forced round end\n"));
		return;
	case 120:
		gi.BroadcastPrintf(PRINT_HUD, _("2 minutes left until forced round end\n"));
		return;
	case 60:
		gi.BroadcastPrintf(PRINT_HUD, _("1 minute left until forced round end\n"));
		return;
	case 30:
		gi.BroadcastPrintf(PRINT_HUD, _("30 seconds left until forced round end\n"));
		return;
	case 15:
		gi.BroadcastPrintf(PRINT_HUD, _("15 seconds left until forced round end\n"));
		return;
	}

	/* active team still has time left */
	if (level.time < level.roundstartTime + sv_roundtimelimit->integer)
		return;

	gi.BroadcastPrintf(PRINT_HUD, _("Current active team hit the max round time\n"));

	/* set all team members to ready (only human players) */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam) {
			G_ClientEndRound(p, NOISY);
			level.nextEndRound = level.framenum;
		}

	level.roundstartTime = level.time;
}

/**
 * @sa G_PlayerSoldiersCount
 */
void G_ClientEndRound (player_t * player, qboolean quiet)
{
	player_t *p;
	int i, lastTeam;

	/* inactive players can't end their inactive round :) */
	if (level.activeTeam != player->pers.team)
		return;

	/* check for "team oszillation" */
	if (level.framenum < level.nextEndRound)
		return;
	level.nextEndRound = level.framenum + 20;

	/* only use this for teamplay matches like coopX or 2on2 and above
	 * also skip this for ai players, this is only called when all ai actors
	 * have finished their 'thinking' */
	if (!player->pers.ai && sv_teamplay->integer) {
		/* check if all team members are ready */
		if (!player->ready) {
			player->ready = qtrue;
			/* don't send this for ai controlled teams */
			if (!player->pers.ai) {
				gi.AddEvent(PM_ALL, EV_ENDROUNDANNOUNCE | EVENT_INSTANTLY);
				gi.WriteByte(player->num);
				gi.WriteByte(player->pers.team);
				gi.EndEvents();
			}
		}
		for (i = 0, p = game.players; i < game.sv_maxplayersperteam * 2; i++, p++)
			if (p->inuse && p->pers.team == level.activeTeam && !p->ready && G_PlayerSoldiersCount(p) > 0)
				return;
	} else {
		player->ready = qtrue;
	}

	/* clear any remaining reaction fire */
	G_ReactToEndTurn();

	/* let all the invisible players perish now */
	G_CheckVisTeam(level.activeTeam, NULL, qtrue, NULL);

	lastTeam = player->pers.team;
	level.activeTeam = TEAM_NO_ACTIVE;

	/* Get the next active team. */
	p = NULL;
	while (level.activeTeam == TEAM_NO_ACTIVE) {
		/* search next team */
		int nextTeam = TEAM_NO_ACTIVE;

		for (i = 1; i < MAX_TEAMS; i++) {
			const int team = (lastTeam + i) % MAX_TEAMS;
			if (level.num_alive[team]) {
				nextTeam = team;
				break;
			}
		}

		/* search corresponding player (even ai players) */
		for (i = 0, p = game.players; i < game.sv_maxplayersperteam * 2; i++, p++)
			if (p->inuse && p->pers.team == nextTeam) {
				/* found next player */
				level.activeTeam = nextTeam;
				break;
			}

		if (level.activeTeam == TEAM_NO_ACTIVE)
			gi.error("Could not find any living player on team %i", nextTeam);

		lastTeam = nextTeam;
	}
	assert(level.activeTeam != TEAM_NO_ACTIVE);
	level.actualRound++;

	/* communicate next player in row to clients */
	gi.AddEvent(PM_ALL, EV_ENDROUND);
	gi.WriteByte(level.activeTeam);

	/* store the round start time to be able to abort the round after a give time */
	level.roundstartTime = level.time;

	/* Update the state of stuned team-members. The actual statistics are sent below! */
	G_UpdateStunState(level.activeTeam);

	/* Give the actors of the now active team their TUs. */
	G_GiveTimeUnits(level.activeTeam);

	/* apply morale behaviour, reset reaction fire */
	G_ResetReactionFire(level.activeTeam);
	if (mor_panic->integer)
		G_MoraleBehaviour(level.activeTeam, quiet);

	/* start ai */
	p->pers.last = NULL;

	/* finish off events */
	gi.EndEvents();

	/* reset ready flag (even ai players) */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam * 2; i++, p++)
		if (p->inuse && p->pers.team == level.activeTeam)
			p->ready = qfalse;
}

/**
 * @brief Send brush models for entities like func_breakable and func_door and triggers
 * with their mins and maxs bounding boxes to the client and let him know about them.
 * There are also entities that are announced here, but fully handled clientside - like
 * func_rotating.
 * @sa CL_AddBrushModel
 * @sa EV_ADD_BRUSH_MODEL
 * @param[in] team The team the edicts are send to
 */
static void G_ClientSendEdictsAndBrushModels (const player_t *player)
{
	int i;
	edict_t *ent;

	/* make SOLID_BSP edicts visible to the client */
	for (i = 1, ent = g_edicts + 1; i < globals.num_edicts; ent++, i++) {
		if (!ent->inuse)
			continue;

		/* brush models that have a type - not the world - keep in
		 * mind that there are several world edicts in the list in case of
		 * a map assembly */
		switch (ent->solid) {
		case SOLID_BSP:
			/* skip the world(s) in case of map assembly */
			if (ent->type) {
				gi.AddEvent(G_PlayerToPM(player), EV_ADD_BRUSH_MODEL);
				gi.WriteShort(ent->type);
				gi.WriteShort(ent->number);
				gi.WriteShort(ent->modelindex);
				/* strip the higher bits - only send levelflags */
				gi.WriteByte(ent->spawnflags & 0xFF);
				gi.WritePos(ent->origin);
				gi.WritePos(ent->angles);
				gi.WriteShort(ent->speed);
				gi.WriteByte(ent->angle);
				ent->visflags |= ~ent->visflags;
			}
			break;

			/* send trigger entities to the client to display them (needs mins, maxs set) */
		case SOLID_TRIGGER:
			if (sv_send_edicts->integer) {
				gi.AddEvent(G_PlayerToPM(player), EV_ADD_EDICT);
				gi.WriteShort(ent->type);
				gi.WriteShort(ent->number);
				gi.WritePos(ent->mins);
				gi.WritePos(ent->maxs);
			}
			break;

		case SOLID_NOT:
		case SOLID_BBOX:
			break;
		}
	}
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

	/** @todo This should be a client side error */
	if (!G_PlayerToPM(player)) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s tried to join - but server is full\n", player->pers.netname);
		return;
	}

	player->began = qtrue;

	level.numplayers++;
	gi.ConfigString(CS_PLAYERCOUNT, va("%i", level.numplayers));

	/* spawn camera (starts client rendering) */
	gi.AddEvent(G_PlayerToPM(player), EV_START | EVENT_INSTANTLY);
	gi.WriteByte(sv_teamplay->integer);

	/* ensure that the start event is send */
	gi.EndEvents();

	/* set the net name */
	gi.ConfigString(CS_PLAYERNAMES + player->num, player->pers.netname);

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has joined team %i\n", player->pers.netname, player->pers.team);
}

/**
 * @brief Sets the team, init the TU and sends the player stats.
 * @return Returns qtrue if player spawns, otherwise qfalse.
 * @sa G_SendPlayerStats
 * @sa G_GetTeam
 * @sa G_GiveTimeUnits
 * @sa G_ClientBegin
 * @sa CL_Reset
 */
qboolean G_ClientSpawn (player_t * player)
{
	edict_t *ent;
	int i;

	if (player->spawned) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already spawned.\n", player->pers.netname);
		G_ClientDisconnect(player);
		return qfalse;
	}

	/** @todo Check player->pers.team here */
	if (!G_GameRunning()) {
		/* activate round if in single-player */
		if (sv_maxclients->integer == 1) {
			level.activeTeam = player->pers.team;
		} else {
			/* return since not all multiplayer teams have joined */
			/* (G_ClientTeamAssign sets level.activeTeam once all teams have joined) */
			return qfalse;
		}
	}

	player->spawned = qtrue;

	/* do all the init events here... */
	/* reset the data */
	gi.AddEvent(G_PlayerToPM(player), EV_RESET | EVENT_INSTANTLY);
	gi.WriteByte(player->pers.team);
	gi.WriteByte(level.activeTeam);

	/* show visible actors and add invisible actor */
	G_ClearVisFlags(player->pers.team);
	G_CheckVisPlayer(player, qfalse);
	G_SendInvisible(player);

	/** Set initial state of reaction fire to previously stored state for all team-members.
	 * This (and the second loop below) defines the default reaction-mode.
	 * @sa CL_GenerateCharacter for the initial default value.
	 * @sa G_SpawnAIPlayer */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->team == player->pers.team && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)) {
			Com_DPrintf(DEBUG_GAME, "G_ClientSpawn: Setting default reaction-mode to %i (%s - %s).\n",
					ent->chr.reservedTus.reserveReaction, player->pers.netname, ent->chr.name);
			G_ClientStateChange(player, i, ent->chr.reservedTus.reserveReaction, qfalse);
		}

	/* submit stats */
	G_SendPlayerStats(player);

	/* send things like doors and breakables */
	G_ClientSendEdictsAndBrushModels(player);

	/* give time units */
	G_GiveTimeUnits(player->pers.team);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->team == player->pers.team && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)) {
			gi.AddEvent(player->pers.team, EV_ACTOR_STATECHANGE);
			gi.WriteShort(ent->number);
			gi.WriteShort(ent->state);
		}

	gi.AddEvent(G_PlayerToPM(player), EV_START_DONE);
	/* ensure that the last event is send, too */
	gi.EndEvents();

	/* inform all clients */
	gi.BroadcastPrintf(PRINT_CONSOLE, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team);
	return qtrue;
}

/**
 * @brief called whenever the player updates a userinfo variable.
 * @note The game can override any of the settings in place (forcing skins or names, etc) before copying it off.
 */
void G_ClientUserinfoChanged (player_t * player, char *userinfo)
{
	const char *s;

	/* check for malformed or illegal info strings */
	if (!Info_Validate(userinfo))
		Q_strncpyz(userinfo, "\\cl_name\\badinfo", sizeof(userinfo));

	/* set name */
	s = Info_ValueForKey(userinfo, "cl_name");
	Q_strncpyz(player->pers.netname, s, sizeof(player->pers.netname));

	Q_strncpyz(player->pers.userinfo, userinfo, sizeof(player->pers.userinfo));

	s = Info_ValueForKey(userinfo, "cl_autostand");
	player->autostand = atoi(s);

	/* send the updated config string */
	gi.ConfigString(CS_PLAYERNAMES + player->num, player->pers.netname);
}

/**
 * @sa G_ClientDisconnect
 * @sa CL_ConnectionlessPacket
 * @todo Check if the teamnum preference has already reached maxsoldiers
 * and reject connection if so
 */
qboolean G_ClientConnect (player_t * player, char *userinfo)
{
	const char *value;

	/* check to see if they are on the banned IP list */
	value = Info_ValueForKey(userinfo, "ip");
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", REJ_BANNED);
		return qfalse;
	}

	value = Info_ValueForKey(userinfo, "password");
	if (*password->string && strcmp(password->string, "none") && strcmp(password->string, value)) {
		Info_SetValueForKey(userinfo, "rejmsg", REJ_PASSWORD_REQUIRED_OR_INCORRECT);
		return qfalse;
	}

	/* fix for fast reconnects after a disconnect */
	if (player->inuse) {
		gi.BroadcastPrintf(PRINT_CONSOLE, "%s already in use.\n", player->pers.netname);
		G_ClientDisconnect(player);
	}

	/* reset persistent data */
	memset(&player->pers, 0, sizeof(player->pers));
	G_ClientUserinfoChanged(player, userinfo);

	gi.BroadcastPrintf(PRINT_CHAT, "%s is connecting...\n", player->pers.netname);
	return qtrue;
}

/**
 * @sa G_ClientConnect
 */
void G_ClientDisconnect (player_t * player)
{
	/* only if the player already sent his began */
	if (player->began) {
		level.numplayers--;
		gi.ConfigString(CS_PLAYERCOUNT, va("%i", level.numplayers));

		if (level.activeTeam == player->pers.team)
			G_ClientEndRound(player, NOISY);

		/* if no more players are connected - stop the server */
		if (!level.numplayers)
			level.intermissionTime = level.time + 10.0f;
	}

	player->began = qfalse;
	player->spawned = qfalse;
	player->ready = qfalse;

	gi.BroadcastPrintf(PRINT_CHAT, "%s disconnected.\n", player->pers.netname);
}

/**
 * @brief Called after every player has joined
 */
void G_ResetClientData (void)
{
	scoreMissionNum = 0;
	memset(scoreMission, 0, sizeof(scoreMission));
}
