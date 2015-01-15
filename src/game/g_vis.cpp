/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_inventory.h"
#include "g_utils.h"
#include "g_vis.h"

/**
 * @brief Checks whether a point is "visible" from the edicts position
 * @sa FrustumVis
 */
bool G_FrustumVis (const Edict* from, const vec3_t point)
{
	if (G_IsActiveCamera(from)) {
		/* it's a 360 degree camera */
		if (from->camera.rotate)
			return true;
	}
	return FrustumVis(from->origin, from->dir, point);
}

/**
 * @brief tests the visibility between two points
 * @param[in] from The point to check visibility from
 * @param[in] to The point to check visibility to
 * @return true if the points are invisible from each other (trace hit something), false otherwise.
 */
static bool G_LineVis (const vec3_t from, const vec3_t to)
{
	return G_TestLineWithEnts(from, to);
}

/**
 * @brief calculate how much check is "visible" by @c ent
 * @param[in] ent The source edict of the check
 * @param[in] check The edict to check how good (or if at all) it is visible
 * @param[in] full Perform a full check in different directions. If this is
 * @c false the actor is fully visible if one vis check returned @c true. With
 * @c true this function can also return a value != 0.0 and != 1.0. Try to only
 * use @c true if you really need the full check. Full checks are of course
 * more expensive.
 * @return a value between 0.0 and 1.0 (one of the ACTOR_VIS_* constants) which
 * reflects the visibility from 0 to 100 percent
 * @note This call isn't cheap - try to do this only if you really need the
 * visibility check or the statement whether one particular actor see another
 * particular actor.
 * @sa CL_ActorVis
 */
float G_ActorVis (const Edict* ent, const Edict* check, bool full)
{
	const float distance = VectorDist(check->origin, ent->origin);
	vec3_t eyeEnt;
	G_ActorGetEyeVector(ent, eyeEnt);

	/* units that are very close are visible in the smoke */
	if (distance > UNIT_SIZE * 1.5f) {
		Edict* e = nullptr;

		while ((e = G_EdictsGetNextInUse(e))) {
			if (G_IsSmoke(e)) {
				if (RayIntersectAABB(eyeEnt, check->absBox.mins, e->absBox)
				 || RayIntersectAABB(eyeEnt, check->absBox.maxs, e->absBox)) {
					return ACTOR_VIS_0;
				}
			}
		}
	}

	vec3_t test, dir;
	float delta;
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
	dir[0] = eyeEnt[1] - check->origin[1];
	dir[1] = check->origin[0] - eyeEnt[0];
	dir[2] = 0;
	VectorNormalizeFast(dir);
	VectorMA(test, -7, dir, test);

	/* do 3 tests */
	int n = 0;
	for (int i = 0; i < 3; i++) {
		if (!G_LineVis(eyeEnt, test)) {
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

int G_VisCheckDist (const Edict* const ent)
{
	if (G_IsActiveCamera(ent))
		return MAX_SPOT_DIST_CAMERA;

	if (G_IsActor(ent))
		return MAX_SPOT_DIST * G_ActorGetInjuryPenalty(ent, MODIFIER_SIGHT);

	return MAX_SPOT_DIST;
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
bool G_Vis (const int team, const Edict* from, const Edict* check, const vischeckflags_t flags)
{
	/* if any of them isn't in use, then they're not visible */
	if (!from->inuse || !check->inuse)
		return false;

	/* only actors and 2x2 units can see anything */
	if (!G_IsLivingActor(from) && !G_IsActiveCamera(from))
		return false;

	/* living team members are always visible */
	if (team >= 0 && check->getTeam() == team && !G_IsDead(check))
		return true;

	/* standard team rules */
	if (team >= 0 && from->getTeam() != team)
		return false;

	/* inverse team rules */
	if (team < 0 && from->getTeam() == -team)
		return false;

	/* check for same pos */
	if (VectorCompare(from->pos, check->pos))
		return true;

	if (!G_IsVisibleOnBattlefield(check))
		return false;

	/* view distance check */
	const int spotDist = G_VisCheckDist(from);
	if (VectorDistSqr(from->origin, check->origin) > spotDist * spotDist)
		return false;

	/* view frustum check */
	if (!(flags & VT_NOFRUSTUM) && !G_FrustumVis(from, check->origin))
		return false;

	/* line trace check */
	switch (check->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		return G_ActorVis(from, check, false) > ACTOR_VIS_0;
	case ET_ITEM:
	case ET_CAMERA:
	case ET_PARTICLE: {
		/* get viewers eye height */
		vec3_t eye;
		G_ActorGetEyeVector(from, eye);
		return !G_LineVis(eye, check->origin);
	}
	default:
		return false;
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
 * other players view, you should use the @c VT_PERISHCHK bits
 * @note If the edict is already visible and flags doesn't contain the
 * bits of @c VT_PERISHCHK, no further checks are performed - only the
 * @c VS_YES bits are returned
 * @return VS_CHANGE is added to the bit mask if the edict flipped its visibility
 * (invisible to visible or vice versa) VS_YES means the edict is visible for the
 * given team
 */
int G_TestVis (const int team, Edict* check, const vischeckflags_t flags)
{
	/* store old flag */
	const int old = G_IsVisibleForTeam(check, team) ? VS_CHANGE : 0;

	if (g_aidebug->integer)
		return VS_YES | !old;

	if (!(flags & VT_PERISHCHK) && old)
		return VS_YES;

	/* test if check is visible */
	Edict* from = nullptr;
	while ((from = G_EdictsGetNextInUse(from)))
		if (G_Vis(team, from, check, flags))
			/* visible */
			return VS_YES | !old;

	/* just return the old state */
	return old;
}

static bool G_VisShouldStop (const Edict* ent)
{
	return G_IsLivingActor(ent) && !G_IsCivilian(ent);
}

/**
 * @param[in] team The team looking at the edict (or not)
 * @param[in] check The edict to check the visibility for
 * @param[in] visFlags The flags for the vis check
 * @param[in] playerMask The mask for the players to send the appear/perish events to. If this is @c 0 the events
 * are not sent - we only update the visflags of the edict
 * @param[in] ent The edict that was responsible for letting the check edict appear and is looking
 */
static int G_DoTestVis (const int team, Edict* check, const vischeckflags_t visFlags, playermask_t playerMask, const Edict* ent)
{
	int status = 0;
	const int vis = G_TestVis(team, check, visFlags);

	/* visibility has changed ... */
	if (vis & VS_CHANGE) {
		/* swap the vis mask for the given team */
		const bool appear = (vis & VS_YES) == VS_YES;
		if (playerMask == 0) {
			G_VisFlagsSwap(*check, G_TeamToVisMask(team));
		} else {
			G_AppearPerishEvent(playerMask, appear, *check, ent);
		}

		/* ... to visible */
		if (vis & VS_YES) {
			status |= VIS_APPEAR;
			if (G_VisShouldStop(check))
				status |= VIS_STOP;
		} else {
			status |= VIS_PERISH;
		}
	} else if (vis == 0 && (visFlags & VT_NEW)) {
		if (G_IsActor(check)) {
			G_EventActorAdd(playerMask, *check);
		}
	}
	return status;
}

/**
 * @brief Sets visible edict on player spawn
 * @sa G_ClientStartMatch
 * @sa G_CheckVisTeam
 * @sa G_AppearPerishEvent
 */
void G_CheckVisPlayer (Player& player, const vischeckflags_t visFlags)
{
	Edict* ent = nullptr;

	/* check visibility */
	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* check if he's visible */
		G_DoTestVis(player.getTeam(), ent, visFlags, G_PlayerToPM(player), nullptr);
	}
}

/**
 * @brief Checks whether an edict appear/perishes for a specific team - also
 * updates the visflags each edict carries
 * @sa G_TestVis
 * @param[in] team Team to check the vis for
 * @param[in] check The edict that you want to check (and which maybe will appear
 * or perish for the given team). If this is nullptr every edict will be checked.
 * @param visFlags Modifiers for the checks
 * @param[in] ent The edict that is (maybe) seeing other edicts
 * @return If an actor get visible who's no civilian VIS_STOP is added to the
 * bit mask, VIS_APPEAR means, that the actor is becoming visible for the queried
 * team, VIS_PERISH means that the actor (the edict) is getting invisible
 * @note the edict may not only be actors, but also items of course
 * @sa G_TeamToPM
 * @sa G_TestVis
 * @sa G_AppearPerishEvent
 * @sa G_CheckVisPlayer
 * @sa G_CheckVisTeamAll
 * @note If something appears, the needed information for those clients that are affected
 * are also send in @c G_AppearPerishEvent
 */
static int G_CheckVisTeam (const int team, Edict* check, const vischeckflags_t visFlags, const Edict* ent)
{
	int status = 0;

	/* check visibility */
	if (check->inuse) {
		/* check if he's visible */
		status |= G_DoTestVis(team, check, visFlags, G_TeamToPM(team), ent);
	}

	return status;
}

/**
 * @brief Do @c G_CheckVisTeam for all entities
 * ent is the one that is looking at the others
 */
int G_CheckVisTeamAll (const int team, const vischeckflags_t visFlags, const Edict* ent)
{
	Edict* chk = nullptr;
	int status = 0;

	while ((chk = G_EdictsGetNextInUse(chk))) {
		status |= G_CheckVisTeam(team, chk, visFlags, ent);
	}
	return status;
}

/**
 * @brief Make everything visible to anyone who can't already see it
 */
void G_VisMakeEverythingVisible (void)
{
	Edict* ent = nullptr;
	while ((ent = G_EdictsGetNextInUse(ent))) {
		const int playerMask = G_VisToPM(ent->visflags);
		G_AppearPerishEvent(~playerMask, true, *ent, nullptr);
		if (G_IsActor(ent))
			G_SendInventory(~G_TeamToPM(ent->getTeam()), *ent);
	}
}

/**
 * @brief Check if the edict appears/perishes for the other teams. If they appear
 * for other teams, the needed information for those clients are also send in
 * @c G_CheckVisTeam resp. @c G_AppearPerishEvent
 * @param[in] check The edict that is maybe seen by others. If nullptr, all edicts are checked
 * @param visFlags Modifiers for the checks
 * @sa G_CheckVisTeam
 */
void G_CheckVis (Edict* check, const vischeckflags_t visFlags)
{
	for (int team = 0; team < MAX_TEAMS; team++)
		if (level.num_alive[team]) {
			if (!check)	/* no special entity given, so check them all */
				G_CheckVisTeamAll(team, visFlags, nullptr);
			else
				G_CheckVisTeam(team, check, visFlags, nullptr);
		}
}

/**
 * @brief Reset the visflags for all edicts in the global list for the
 * given team - and only for the given team
 */
void G_VisFlagsClear (int team)
{
	Edict* ent = nullptr;
	const teammask_t teamMask = ~G_TeamToVisMask(team);
	while ((ent = G_EdictsGetNextInUse(ent))) {
		ent->visflags &= teamMask;
	}
}

void G_VisFlagsAdd (Edict& ent, teammask_t teamMask)
{
	ent.visflags |= teamMask;
}

void G_VisFlagsReset (Edict& ent)
{
	ent.visflags = 0;
}

void G_VisFlagsSwap (Edict& ent, teammask_t teamMask)
{
	ent.visflags ^= teamMask;
}
