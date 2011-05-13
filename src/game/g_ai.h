/**
 * @file g_ai.h
 * @brief Artificial Intelligence structures.
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

#ifndef G_AI_H
#define G_AI_H

typedef struct {
	pos3_t to;			/**< grid pos to walk to */
	pos3_t stop;		/**< grid pos to stop at (e.g. hiding spots) */
	shoot_types_t shootType;	/**< the shoot type */
	byte shots;			/**< how many shoots can this actor do - only set this if the target is an actor */
	edict_t *target;	/**< the target edict */
	const fireDef_t *fd;/**< the firemode to use for shooting */
	int z_align;		/**< the z-align for every shoot */
} aiAction_t;

#define GUETE_HIDE			60
#define GUETE_CLOSE_IN		20
#define GUETE_KILL			30
#define GUETE_RANDOM		10
#define GUETE_REACTION_ERADICATION 30
#define GUETE_REACTION_FEAR_FACTOR 20
#define GUETE_CIV_FACTOR	0.25

#define GUETE_CIV_RANDOM	10
#define GUETE_RUN_AWAY		50
#define GUETE_CIV_LAZINESS	5
#define RUN_AWAY_DIST		160
#define WAYPOINT_CIV_DIST	768

#define GUETE_MISSION_OPPONENT_TARGET	50
#define GUETE_MISSION_TARGET	60

#define AI_ACTION_NOTHING_FOUND -10000.0

#define CLOSE_IN_DIST		1200.0
#define SPREAD_FACTOR		8.0
#define	SPREAD_NORM(x)		(x > 0 ? SPREAD_FACTOR/(x*torad) : 0)
/** @brief distance for (ai) hiding in grid tiles */
#define HIDE_DIST			7
#define HERD_DIST			7

/*
 * Shared functions (between C AI and LUA AI)
 */
void AI_TurnIntoDirection(edict_t *ent, const pos3_t pos);
qboolean AI_FindHidingLocation(int team, edict_t *ent, const pos3_t from, int *tuLeft);
qboolean AI_FindHerdLocation(edict_t *ent, const pos3_t from, const vec3_t target, int tu);
int AI_GetHidingTeam(const edict_t *ent);
const item_t *AI_GetItemForShootType(shoot_types_t shootType, const edict_t *ent);

/*
 * LUA functions
 */
void AIL_ActorThink(player_t * player, edict_t * ent);
int AIL_InitActor(edict_t * ent, const char *type, const char *subtype);
void AIL_Cleanup(void);
void AIL_Init(void);
void AIL_Shutdown(void);

#endif
