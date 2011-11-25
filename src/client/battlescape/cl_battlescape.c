/**
 * @file cl_battlescape.c
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

#include "../client.h"
#include "cl_battlescape.h"
#include "cl_actor.h"

clientBattleScape_t cl;

/**
 * @brief Searches a local entity at the given position.
 * @param[in] pos The grid position to search a local entity at
 * @param[in] includingStunned Also search for stunned actors if @c true.
 * @param[in] actor The current selected actor
 */
le_t* CL_BattlescapeSearchAtGridPos (const pos3_t pos, qboolean includingStunned, const le_t *actor)
{
	le_t *le;
	le_t *nonActor = NULL;

	/* search for an actor on this field */
	le = NULL;
	while ((le = LE_GetNextInUse(le))) {
		if (actor != NULL && le == actor->clientAction) {
			/* if the actor has a client action assigned and we click onto the actor,
			 * we will trigger this client action */
			if (VectorCompare(actor->pos, pos))
				nonActor = le;
		} else if (le != actor && LE_IsLivingAndVisibleActor(le) && (includingStunned || !LE_IsStunned(le)))
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
				if (VectorCompare(le->pos, pos))
					return le;
				break;
			case ACTOR_SIZE_2x2: {
				pos3_t actor2x2[3];

				VectorSet(actor2x2[0], le->pos[0] + 1, le->pos[1],     le->pos[2]);
				VectorSet(actor2x2[1], le->pos[0],     le->pos[1] + 1, le->pos[2]);
				VectorSet(actor2x2[2], le->pos[0] + 1, le->pos[1] + 1, le->pos[2]);
				if (VectorCompare(le->pos, pos)
				|| VectorCompare(actor2x2[0], pos)
				|| VectorCompare(actor2x2[1], pos)
				|| VectorCompare(actor2x2[2], pos))
					return le;
				break;
			}
			default:
				Com_Error(ERR_DROP, "Grid_MoveCalc: unknown actor-size: %i!", le->fieldSize);
		}
	}

	return nonActor;
}

/**
 * @brief Check whether we already have actors spawned on the battlefield
 * @sa CL_OnBattlescape
 * @return @c true when we are in battlefield and have soldiers spawned (game is running)
 */
qboolean CL_BattlescapeRunning (void)
{
	return cl.spawned;
}

/**
 * @brief Check whether we are in a tactical mission as server or as client. But this
 * only means that we are able to render the map - not that the game is running (the
 * team can still be missing - see @c CL_BattlescapeRunning)
 * @note handles multiplayer and singleplayer
 * @sa CL_BattlescapeRunning
 * @return true when we are in battlefield
 */
qboolean CL_OnBattlescape (void)
{
	/* server_state is set to zero (ss_dead) on every battlefield shutdown */
	if (Com_ServerState())
		return qtrue; /* server */

	/* client */
	if (cls.state >= ca_connected)
		return qtrue;

	return qfalse;
}


#define LOOKUP_EPSILON 0.0001f

/**
 * @brief table for lookup_erf
 * lookup[]= {erf(0), erf(0.1), ...}
 */
static const float lookup[30]= {
	0.0f,    0.1125f, 0.2227f, 0.3286f, 0.4284f, 0.5205f, 0.6039f,
	0.6778f, 0.7421f, 0.7969f, 0.8427f, 0.8802f, 0.9103f, 0.9340f,
	0.9523f, 0.9661f, 0.9763f, 0.9838f, 0.9891f, 0.9928f, 0.9953f,
	0.9970f, 0.9981f, 0.9989f, 0.9993f, 0.9996f, 0.9998f, 0.9999f,
	0.9999f, 1.0000f
};

/**
 * @brief table for lookup_erf
 * lookup[]= {10*(erf(0.1)-erf(0.0)), 10*(erf(0.2)-erf(0.1)), ...}
 */
static const float lookupdiff[30]= {
	1.1246f, 1.1024f, 1.0592f, 0.9977f, 0.9211f, 0.8336f, 0.7395f,
	0.6430f, 0.5481f, 0.4579f, 0.3750f, 0.3011f, 0.2369f, 0.1828f,
	0.1382f, 0.1024f, 0.0744f, 0.0530f, 0.0370f, 0.0253f, 0.0170f,
	0.0112f, 0.0072f, 0.0045f, 0.0028f, 0.0017f, 0.0010f, 0.0006f,
	0.0003f, 0.0002f
};

/**
 * @brief calculate approximate erf, the error function
 * http://en.wikipedia.org/wiki/Error_function
 * uses lookup table and linear interpolation
 * approximation good to around 0.001.
 * easily good enough for the job.
 * @param[in] z the number to calculate the erf of.
 * @return for positive arg, returns approximate erf. for -ve arg returns 0.0f.
 */
static float CL_LookupErrorFunction (float z)
{
	float ifloat;
	int iint;

	/* erf(-z)=-erf(z), but erf of -ve number should never be used here
	 * so return 0 here */
	if (z < LOOKUP_EPSILON)
		return 0.0f;
	if (z > 2.8f)
		return 1.0f;
	ifloat = floor(z * 10.0f);
	iint = (int)ifloat;
	assert(iint < 30);
	return lookup[iint] + (z - ifloat / 10.0f) * lookupdiff[iint];
}

static inline qboolean CL_TestLine (const vec3_t start, const vec3_t stop, const int levelmask)
{
	return TR_TestLine(cl.mapTiles, start, stop, levelmask);
}

/**
 * @brief Calculates chance to hit if the actor has a fire mode activated.
 * @param[in] actor The local entity of the actor to calculate the hit probability for.
 * @todo The hit probability should work somewhat differently for splash damage weapons.
 * Since splash damage weapons can deal damage even when they don't directly hit an actor,
 * the hit probability should be defined as the predicted percentage of the maximum splash
 * damage of the firemode, assuming the projectile explodes at the desired location. This
 * means that a percentage should be displayed for EVERY actor in the predicted blast
 * radius. This will likely require specialized code.
 */
int CL_GetHitProbability (const le_t* actor)
{
	vec3_t shooter, target;
	float distance, pseudosin, width, height, acc, perpX, perpY, hitchance,
		stdevupdown, stdevleftright, crouch, commonfactor;
	int distx, disty, n;
	le_t *le;
	const character_t *chr;
	pos3_t toPos;

	assert(actor);
	assert(actor->fd);

	if (IS_MODE_FIRE_PENDING(actor->actorMode))
		VectorCopy(actor->mousePendPos, toPos);
	else
		VectorCopy(mousePos, toPos);

	/** @todo use LE_FindRadius */
	le = LE_GetFromPos(toPos);
	if (!le)
		return 0;

	/* or suicide attempted */
	if (le->selected && !FIRESH_IsMedikit(le->fd))
		return 0;

	VectorCopy(actor->origin, shooter);
	VectorCopy(le->origin, target);

	/* Calculate HitZone: */
	distx = fabs(shooter[0] - target[0]);
	disty = fabs(shooter[1] - target[1]);
	distance = sqrt(distx * distx + disty * disty);
	if (distx > disty)
		pseudosin = distance / distx;
	else
		pseudosin = distance / disty;
	width = 2 * PLAYER_WIDTH * pseudosin;
	height = LE_IsCrouched(le) ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT;

	chr = CL_ActorGetChr(actor);
	if (!chr)
		Com_Error(ERR_DROP, "No character given for local entity");

	acc = GET_ACC(chr->score.skills[ABILITY_ACCURACY],
			actor->fd->weaponSkill ? chr->score.skills[actor->fd->weaponSkill] : 0.0);

	crouch = (LE_IsCrouched(actor) && actor->fd->crouch) ? actor->fd->crouch : 1.0;

	commonfactor = crouch * torad * distance * GET_INJURY_MULT(chr->score.skills[ABILITY_MIND], actor->HP, actor->maxHP);
	stdevupdown = (actor->fd->spread[0] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	stdevleftright = (actor->fd->spread[1] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	hitchance = (stdevupdown > LOOKUP_EPSILON ? CL_LookupErrorFunction(height * 0.3536f / stdevupdown) : 1.0f)
			  * (stdevleftright > LOOKUP_EPSILON ? CL_LookupErrorFunction(width * 0.3536f / stdevleftright) : 1.0f);
	/* 0.3536=sqrt(2)/4 */

	/* Calculate cover: */
	n = 0;
	height = height / 18;
	width = width / 18;
	target[2] -= UNIT_HEIGHT / 2;
	target[2] += height * 9;
	perpX = disty / distance * width;
	perpY = 0 - distx / distance * width;

	target[0] += perpX;
	perpX *= 2;
	target[1] += perpY;
	perpY *= 2;
	target[2] += 6 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 6 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] += 4 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[2] += 4 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX * 3;
	target[1] -= perpY * 3;
	target[2] -= 12 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 6 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] -= 4 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 10 * height;
	if (!CL_TestLine(shooter, target, TL_FLAG_NONE))
		n++;

	return 100 * (hitchance * (0.125) * n);
}

static const float mapZBorder = -(UNIT_HEIGHT * 5);
/**
 * @brief Checks whether give position is still inside the map borders
 * @param[in] position The position to check (world coordinate)
 * @param[in] delta The delta from the map boundaries. Positive values to make
 * the position being earlier out of the map, negative values to let the position
 * be later out of the map
 * @return @c true if the given position is out of the map boundaries, @c false
 * otherwise.
 */
qboolean CL_OutsideMap (const vec3_t position, const float delta)
{
	if (position[0] < cl.mapData->mapMin[0] - delta || position[0] > cl.mapData->mapMax[0] + delta)
		return qtrue;

	if (position[1] < cl.mapData->mapMin[1] - delta || position[1] > cl.mapData->mapMax[1] + delta)
		return qtrue;

	/* if a le is deeper than 5 levels below the latest walkable level (0) then
	 * we can assume that it is outside the world
	 * This is needed because some maps (e.g. the dam map) has unwalkable levels
	 * that just exists for detail reasons */
	if (position[2] < mapZBorder)
		return qtrue;

	/* still inside the map borders */
	return qfalse;
}

/**
 * @brief Counts visible enemies on the battlescape
 * @return The amount of visible enemies (from all the other teams)
 */
int CL_CountVisibleEnemies (void)
{
	le_t *le;
	int count;

	count = 0;
	le = NULL;
	while ((le = LE_GetNextInUse(le))) {
		if (LE_IsLivingAndVisibleActor(le) && le->team != cls.team && le->team != TEAM_CIVILIAN)
			count++;
	}

	return count;
}

#ifdef DEBUG
/** @todo this does not belong here */
#include "../../common/routing.h"

/**
 * @brief Dumps contents of the entire client map to console for inspection.
 * @sa CL_InitLocal
 */
void Grid_DumpWholeClientMap_f (void)
{
	int i;

	for (i = 0; i < ACTOR_MAX_SIZE; i++)
		RT_DumpWholeMap(cl.mapTiles, &(cl.mapData->map[i]));
}

/**
 * @brief Dumps contents of the entire client routing table to CSV file.
 * @sa CL_InitLocal
 */
void Grid_DumpClientRoutes_f (void)
{
	ipos3_t wpMins, wpMaxs;
	VecToPos(cl.mapData->mapMin, wpMins);
	VecToPos(cl.mapData->mapMax, wpMaxs);
	RT_WriteCSVFiles(cl.mapData->map, "ufoaiclient", wpMins, wpMaxs);
}
#endif

char *CL_GetConfigString (int index)
{
	if (!Com_CheckConfigStringIndex(index))
		Com_Error(ERR_DROP, "invalid access to configstring array with index: %i", index);

	return cl.configstrings[index];
}

int CL_GetConfigStringInteger (int index)
{
	return atoi(CL_GetConfigString(index));
}

char *CL_SetConfigString (int index, struct dbuffer *msg)
{
	if (!Com_CheckConfigStringIndex(index))
		Com_Error(ERR_DROP, "invalid access to configstring array with index: %i", index);

	/* change the string in cl
	 * there may be overflows in i==CS_TILES - but thats ok
	 * see definition of configstrings and MAX_TILESTRINGS */
	if (index == CS_TILES || index == CS_POSITIONS)
		NET_ReadString(msg, cl.configstrings[index], MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	else
		NET_ReadString(msg, cl.configstrings[index], sizeof(cl.configstrings[index]));

	return cl.configstrings[index];
}
