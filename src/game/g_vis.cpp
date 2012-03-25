/**
 * @file g_vis.c
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
 * @return true if the points are invisible from each other (trace hit something), false otherwise.
 */
static qboolean G_LineVis (const vec3_t from, const vec3_t to)
{
	return G_TestLineWithEnts(from, to);
}

/**
 * @brief calculate how much check is "visible" from @c from
 * @param[in] from The world coordinate to check from
 * @param[in] ent The source edict of the check
 * @param[in] check The edict to check how good (or if at all) it is visible
 * @param[in] full Perform a full check in different directions. If this is
 * @c false the actor is fully visible if one vis check returned @c true. With
 * @c true this function can also return a value != 0.0 and != 1.0. Try to only
 * use @c true if you really need the full check. Full checks are of course
 * more expensive.
 * @return a value between 0.0 and 1.0 which reflects the visibility from 0
 * to 100 percent
 * @note This call isn't cheap - try to do this only if you really need the
 * visibility check or the statement whether one particular actor see another
 * particular actor.
 * @sa CL_ActorVis
 */
float G_ActorVis (const vec3_t from, const edict_t *ent, const edict_t *check, qboolean full)
{
	vec3_t test, dir;
	float delta;
	int i, n;
	const float distance = VectorDist(check->origin, ent->origin);

	/* units that are very close are visible in the smoke */
	if (distance > UNIT_SIZE * 1.5f) {
		vec3_t eyeEnt;
		edict_t *e = NULL;

		G_ActorGetEyeVector(ent, eyeEnt);

		while ((e = G_EdictsGetNext(e))) {
			if (G_IsSmoke(e)) {
				if (RayIntersectAABB(eyeEnt, check->absmin, e->absmin, e->absmax)
				 || RayIntersectAABB(eyeEnt, check->absmax, e->absmin, e->absmax)) {
					return ACTOR_VIS_0;
				}
			}
		}
	}

	/* start on eye height */
	VectorCopy(check->origin, test);
	if (G_IsDead(check)) {
		test[2] += PLAYER_DEAD;
		delta = 0;
	} else if (G_IsCrouched(check)) {
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
	VectorNormalizeFast(dir);
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
 * @param[in] flags @c VT_NOFRUSTUM, ...
 */
qboolean G_Vis (const int team, const edict_t *from, const edict_t *check, int flags)
{
	vec3_t eye;

	/* if any of them isn't in use, then they're not visible */
	if (!from->inuse || !check->inuse)
		return qfalse;

	/* only actors and 2x2 units can see anything */
	if (!G_IsLivingActor(from))
		return qfalse;

	/* living team members are always visible */
	if (team >= 0 && check->team == team && !G_IsDead(check))
		return qtrue;

	/* standard team rules */
	if (team >= 0 && from->team != team)
		return qfalse;

	/* inverse team rules */
	if (team < 0 && (from->team == -team || G_IsCivilian(from) || check->team != -team))
		return qfalse;

	/* check for same pos */
	if (VectorCompare(from->pos, check->pos))
		return qtrue;

	if (!G_IsVisibleOnBattlefield(check))
		return qfalse;

	/* view distance check */
	if (VectorDistSqr(from->origin, check->origin) > MAX_SPOT_DIST * MAX_SPOT_DIST)
		return qfalse;

	/* view frustum check */
	if (!(flags & VT_NOFRUSTUM) && !G_FrustumVis(from, check->origin))
		return qfalse;

	/* get viewers eye height */
	G_ActorGetEyeVector(from, eye);

	/* line trace check */
	switch (check->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		return G_ActorVis(eye, from, check, qfalse) > ACTOR_VIS_0;
	case ET_ITEM:
	case ET_PARTICLE:
		return !G_LineVis(eye, check->origin);
	default:
		return qfalse;
	}
}

/**
 * @brief test if @c check is visible by team (or if visibility changed?)
 * @sa G_CheckVisTeam
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
int G_TestVis (const int team, edict_t * check, int flags)
{
	edict_t *from = NULL;
	/* store old flag */
	const int old = G_IsVisibleForTeam(check, team) ? VIS_CHANGE : 0;

	if (g_aidebug->integer)
		return VIS_YES | !old;

	if (!(flags & VT_PERISH) && old)
		return VIS_YES;

	/* test if check is visible */
	while ((from = G_EdictsGetNextInUse(from)))
		if (G_Vis(team, from, check, flags))
			/* visible */
			return VIS_YES | !old;

	/* just return the old state */
	return old;
}

static qboolean G_VisShouldStop (const edict_t *ent)
{
	return G_IsLivingActor(ent) && !G_IsCivilian(ent);
}

static int G_DoTestVis (const int team, edict_t * check, qboolean perish, int playerMask, const edict_t *ent)
{
	int status = 0;
	const int visFlags = perish ? VT_PERISH : 0;
	const int vis = G_TestVis(team, check, visFlags);

	/* visibility has changed ... */
	if (vis & VIS_CHANGE) {
		/* swap the vis mask for the given team */
		const qboolean appear = (vis & VIS_YES) == VIS_YES;
		if (playerMask == 0) {
			G_VisFlagsSwap(check, G_TeamToVisMask(team));
		} else {
			G_AppearPerishEvent(playerMask, appear, check, ent);
		}

		/* ... to visible */
		if (vis & VIS_YES) {
			status |= VIS_APPEAR;
			if (G_VisShouldStop(check))
				status |= VIS_STOP;
		} else
			status |= VIS_PERISH;
	}
	return status;
}

/**
 * @brief Sets visible edict on player spawn
 * @sa G_ClientStartMatch
 * @sa G_CheckVisTeam
 * @sa G_AppearPerishEvent
 */
int G_CheckVisPlayer (player_t* player, qboolean perish)
{
	int status = 0;
	edict_t* ent = NULL;

	/* check visibility */
	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* check if he's visible */
		status |= G_DoTestVis(player->pers.team, ent, perish, G_PlayerToPM(player), NULL);
	}

	return status;
}

/**
 * @brief Checks whether an edict appear/perishes for a specific team - also
 * updates the visflags each edict carries
 * @sa G_TestVis
 * @param[in] team Team to check the vis for
 * @param[in] check The edict that you want to check (and which maybe will appear
 * or perish for the given team). If this is NULL every edict will be checked.
 * @param[in] perish Also check whether the edict (the actor) is going to become
 * invisible for the given team
 * @param[in] ent The edict that is (maybe) seeing other edicts
 * @return If an actor get visible who's no civilian VIS_STOP is added to the
 * bit mask, VIS_YES means, he is visible, VIS_CHANGE means that the actor
 * flipped its visibility (invisible to visible or vice versa), VIS_PERISH means
 * that the actor (the edict) is getting invisible for the queried team
 * @note the edict may not only be actors, but also items of course
 * @sa G_TeamToPM
 * @sa G_TestVis
 * @sa G_AppearPerishEvent
 * @sa G_CheckVisPlayer
 * @sa G_CheckVisTeamAll
 * @note If something appears, the needed information for those clients that are affected
 * are also send in @c G_AppearPerishEvent
 */
int G_CheckVisTeam (const int team, edict_t *check, qboolean perish, const edict_t *ent)
{
	int status = 0;

	/* check visibility */
	if (check->inuse) {
		/* check if he's visible */
		status |= G_DoTestVis(team, check, perish, G_TeamToPM(team), ent);
	}

	return status;
}

/**
 * @brief Do @c G_CheckVisTeam for all entities
 */
int G_CheckVisTeamAll (const int team, qboolean perish, const edict_t *ent)
{
	edict_t *chk = NULL;
	int status = 0;

	while ((chk = G_EdictsGetNextInUse(chk))) {
		status |= G_CheckVisTeam(team, chk, perish, ent);
	}
	return status;
}

/**
 * @brief Make everything visible to anyone who can't already see it
 */
void G_VisMakeEverythingVisible (void)
{
	edict_t *ent = NULL;
	while ((ent = G_EdictsGetNextInUse(ent))) {
		const int playerMask = G_VisToPM(ent->visflags);
		G_AppearPerishEvent(~playerMask, qtrue, ent, NULL);
		if (G_IsActor(ent))
			G_SendInventory(~G_TeamToPM(ent->team), ent);
	}
}

/**
 * @brief Check if the edict appears/perishes for the other teams. If they appear
 * for other teams, the needed information for those clients are also send in
 * @c G_CheckVisTeam resp. @c G_AppearPerishEvent
 * @param[in] perish Also check for perishing events
 * @param[in] check The edict that is maybe seen by others. If NULL, all edicts are checked
 * @return Bitmask of VIS_* values
 * @sa G_CheckVisTeam
 */
int G_CheckVis (edict_t * check, qboolean perish)
{
	int team;
	int status;

	status = 0;
	for (team = 0; team < MAX_TEAMS; team++)
		if (level.num_alive[team]) {
			if (!check)	/* no special entity given, so check them all */
				status |= G_CheckVisTeamAll(team, perish, NULL);
			else
				status |= G_CheckVisTeam(team, check, perish, NULL);
		}

	return status;
}

/**
 * @brief Reset the visflags for all edicts in the global list for the
 * given team - and only for the given team
 */
void G_VisFlagsClear (int team)
{
	edict_t *ent = NULL;
	vismask_t mask;

	mask = ~G_TeamToVisMask(team);
	while ((ent = G_EdictsGetNextInUse(ent))) {
		ent->visflags &= mask;
	}
}

void G_VisFlagsAdd (edict_t *ent, vismask_t visMask)
{
	ent->visflags |= visMask;
}

void G_VisFlagsReset (edict_t *ent)
{
	ent->visflags = 0;
}

void G_VisFlagsSwap (edict_t *ent, vismask_t visMask)
{
	ent->visflags ^= visMask;
}
