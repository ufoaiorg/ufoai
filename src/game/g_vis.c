/**
 * @file g_vis.c
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

#define VIS_APPEAR	1
#define VIS_PERISH	2

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
 * @param[in] full Perform a full check in different directions. If this is
 * @c false the actor is fully visible if one vis check returned @c true. With
 * @c true this function can also return a value != 0.0 and != 1.0.
 * @param[in] from The world coordinate to check from
 * @param[in] check The edict to check how good (or if at all) it is visible
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
 * @param[in] flags @c VT_NOFRUSTUM, ...
 */
float G_Vis (int team, const edict_t *from, const edict_t *check, int flags)
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
	if (team < 0 && (from->team == -team || G_IsCivilian(from) || check->team != -team))
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

	if (!G_IsVisibleOnBattlefield(check))
		return ACTOR_VIS_0;

	/* get viewers eye height */
	VectorCopy(from->origin, eye);
	if (G_IsCrouched(from) || G_IsPaniced(from))
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
	const int old = G_IsVisibleForTeam(check, team) ? 1 : 0;

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

static qboolean G_VisShouldStop (const edict_t *ent)
{
	return G_IsLivingActor(ent) && !G_IsCivilian(ent);
}

/**
 * @brief Sets visible edict on player spawn
 * @sa G_ClientSpawn
 * @sa G_CheckVisTeam
 * @sa G_AppearPerishEvent
 */
int G_CheckVisPlayer (player_t* player, qboolean perish)
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
					if (G_VisShouldStop(ent))
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
 * @param[in] check The edict that you want to check (and which maybe will appear
 * or perish for the given team). If this is NULL every edict will be checked.
 * If check is a NULL pointer - all edicts in g_edicts are checked
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
				check->visflags ^= G_TeamToVisMask(team);
				G_AppearPerishEvent(G_TeamToPM(team), vis & VIS_YES, check, ent);

				/* ... to visible */
				if (vis & VIS_YES) {
					status |= VIS_APPEAR;
					if (G_VisShouldStop(check))
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
 * @param[in] check The edict that is maybe seen by others
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

	mask = ~G_TeamToVisMask(team);
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse)
			ent->visflags &= mask;
}
