/**
 * @file g_ai.c
 * @brief Artificial Intelligence.
 *
 * @par
 * You can find the reference lua manual at http://www.lua.org/manual/5.1/
 *
 * @par -1 and -2 are pseudo indexes, they count backwards:
 * @li -1 is top
 * @li 1 is bottom
 * @li -2 is under the top
 * @li etc...
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
#include "g_ai.h"
#include "lua/lauxlib.h"

#define POS3_METATABLE	"pos3" /**< Pos3 Lua Metatable name. */
#define ACTOR_METATABLE	"actor" /**< Actor Lua Metable name. */
#define AI_METATABLE	"ai" /**< AI Lua Metable name. */

/**
 * Provides an api like luaL_dostring for buffers.
 */
#define luaL_dobuffer(L, b, n, s) \
	(luaL_loadbuffer(L, b, n, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define AIL_invalidparameter(n)	\
	gi.DPrintf("AIL: Invalid parameter #%d in '%s'.\n", n, __func__)

/*
 * Helper functions
 */

/**
 * @brief Converts integer team representation into string
 * @param team The team to convert to the string representation
 * @return The team string
 * @sa AIL_Init
 */
static const char *AIL_toTeamString (const int team)
{
	const char *teamStr = gi.GetConstVariable("luaaiteam", team);
	if (teamStr == NULL)
		AIL_invalidparameter(1);
	return teamStr;
}

/**
 * @brief Converts team string into int representation
 * @param team The team to convert (alien, phalanx, civilian, ...)
 * @return The integer representation of the given team string
 * @sa AIL_Init
 */
static int AIL_toTeamInt (const char *team)
{
	int teamInt = TEAM_DEFAULT;
	if (!gi.GetConstIntFromNamespace("luaaiteam", team, &teamInt))
		AIL_invalidparameter(1);
	return teamInt;
}

/**
 * @brief Wrapper around edict.
 */
typedef struct aiActor_s {
	edict_t *ent; /**< Actual actor. */
} aiActor_t;


/*
 * Current AI Actor.
 */
static edict_t *AIL_ent; /**< Actor currently running the Lua AI. */
static player_t *AIL_player; /**< Player currently running the Lua AI. */


/*
 * Actor metatable.
 */
/* Internal functions. */
static int actorL_register(lua_State *L);
static int lua_isactor(lua_State *L, int index);
static aiActor_t* lua_toactor(lua_State *L, int index);
static aiActor_t* lua_pushactor(lua_State *L, aiActor_t *actor);
/* Metatable functions. */
static int actorL_tostring(lua_State *L);
static int actorL_pos(lua_State *L);
static int actorL_shoot(lua_State *L);
static int actorL_face(lua_State *L);
static int actorL_team(lua_State *L);
/** Lua Actor metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg actorL_methods[] = {
	{"__tostring", actorL_tostring},
	{"pos", actorL_pos},
	{"shoot", actorL_shoot},
	{"face", actorL_face},
	{"team", actorL_team},
	{NULL, NULL}
};


/**
 * pos3 metatable.
 */
/* Internal functions. */
static int pos3L_register(lua_State *L);
static int lua_ispos3(lua_State *L, int index);
static pos3_t* lua_topos3(lua_State *L, int index);
static pos3_t* lua_pushpos3(lua_State *L, pos3_t *pos);
/* Metatable functions. */
static int pos3L_tostring(lua_State *L);
static int pos3L_goto(lua_State *L);
static int pos3L_face(lua_State *L);
/** Lua Pos3 metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg pos3L_methods[] = {
	{"__tostring", pos3L_tostring},
	{"goto", pos3L_goto},
	{"face", pos3L_face},
	{NULL, NULL}
};


/**
 * General AI bindings.
 */
static int AIL_print(lua_State *L);
static int AIL_see(lua_State *L);
static int AIL_crouch(lua_State *L);
static int AIL_isinjured(lua_State *L);
static int AIL_TU(lua_State *L);
static int AIL_HP(lua_State *L);
static int AIL_morale(lua_State *L);
static int AIL_reactionfire(lua_State *L);
static int AIL_roundsleft(lua_State *L);
static int AIL_canreload(lua_State *L);
static int AIL_reload(lua_State *L);
static int AIL_positionshoot(lua_State *L);
static int AIL_positionhide(lua_State *L);
static int AIL_positionherd(lua_State *L);
static int AIL_distance(lua_State *L);
/** Lua AI module methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg AIL_methods[] = {
	{"print", AIL_print},
	{"see", AIL_see},
	{"crouch", AIL_crouch},
	{"isinjured", AIL_isinjured},
	{"TU", AIL_TU},
	{"HP", AIL_HP},
	{"morale", AIL_morale},
	{"reactionfire", AIL_reactionfire},
	{"roundsleft", AIL_roundsleft},
	{"canreload", AIL_canreload},
	{"reload", AIL_reload},
	{"positionshoot", AIL_positionshoot},
	{"positionhide", AIL_positionhide},
	{"positionherd", AIL_positionherd},
	{"distance", AIL_distance},
	{NULL, NULL}
};


/**
 *    A C T O R L
 */

/**
 * @brief Registers the actor metatable in the lua_State.
 * @param[in,out] L State to register the metatable in.
 * @return 0 on success.
 */
static int actorL_register (lua_State *L)
{
	/* Create the metatable */
	luaL_newmetatable(L, ACTOR_METATABLE);

	/* Create the access table */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	/* Register the values */
	luaL_register(L, NULL, actorL_methods);

	/* Clean up stack. */
	lua_pop(L, 1);

	return 0; /* No error */
}

/**
 * @brief Checks to see if there is a actor metatable at index in the lua_State.
 * @param[in,out] L Lua state to check.
 * @param[in] index Index to check for a actor metatable.
 * @return 1 if index has a actor metatable otherwise returns 0.
 */
static int lua_isactor (lua_State *L, int index)
{
	int ret;

	if (lua_getmetatable(L, index) == 0)
		return 0;
	lua_getfield(L, LUA_REGISTRYINDEX, ACTOR_METATABLE);

	ret = 0;
	if (lua_rawequal(L, -1, -2))  /* does it have the correct metatable? */
		ret = 1;

	lua_pop(L, 2);  /* remove both metatables */
	return ret;
}

/**
 * @brief Returns the actor from the metatable at index.
 */
static aiActor_t* lua_toactor (lua_State *L, int index)
{
	if (lua_isactor(L, index)) {
		return (aiActor_t*) lua_touserdata(L, index);
	}
	luaL_typerror(L, index, ACTOR_METATABLE);
	return NULL;
}

/**
 * @brief Pushes a actor as a metatable at the top of the stack.
 */
static aiActor_t* lua_pushactor (lua_State *L, aiActor_t *actor)
{
	aiActor_t *a;
	a = (aiActor_t*) lua_newuserdata(L, sizeof(*a));
	*a = *actor;
	luaL_getmetatable(L, ACTOR_METATABLE);
	lua_setmetatable(L, -2);
	return a;
}

/**
 * @brief Pushes the actor as a string.
 */
static int actorL_tostring (lua_State *L)
{
	aiActor_t *target;
	char buf[MAX_VAR];

	assert(lua_isactor(L, 1));

	target = lua_toactor(L, 1);
	Com_sprintf(buf, sizeof(buf), "Actor( %s )", target->ent->chr.name);

	lua_pushstring(L, buf);
	return 1;
}

/**
 * @brief Gets the actors position.
 */
static int actorL_pos (lua_State *L)
{
	aiActor_t *target;

	assert(lua_isactor(L, 1));

	target = lua_toactor(L, 1);
	lua_pushpos3(L, &target->ent->pos);
	return 1;
}

/**
 * @brief Shoots the actor.
 */
static int actorL_shoot (lua_State *L)
{
	int tu, shots;
	shoot_types_t shootType;
	aiActor_t *target;
	const item_t *item;
	const fireDef_t *fdArray;

	assert(lua_isactor(L, 1));

	/* Target */
	target = lua_toactor(L, 1);

	/* Number of TU to spend shooting, fire mode will adjust to that. */
	if (lua_gettop(L) > 1) {
		assert(lua_isnumber(L, 2)); /* Must be a number. */

		tu = (int) lua_tonumber(L, 2);
	} else {
		tu = AIL_ent->TU;
	}

	shootType = ST_RIGHT;
	item = AI_GetItemForShootType(shootType, AIL_ent);
	if (item == NULL) {
		shootType = ST_LEFT;
		item = AI_GetItemForShootType(shootType, AIL_ent);
	}

	/* Failure - no weapon. */
	if (item == NULL) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/** @todo Choose fire mode based on TU available - currently the first one is used. */
	fdArray = FIRESH_FiredefForWeapon(item);
	if (fdArray == NULL) {
		/* Failure - no weapon. */
		lua_pushboolean(L, 0);
		return 1;
	}

	shots = tu / fdArray->time;

	while (shots > 0) {
		shots--;
		/** @todo actually handle fire modes */
		G_ClientShoot(AIL_player, AIL_ent, target->ent->pos,
				shootType, 0, NULL, qtrue, 0);
	}

	/* Success. */
	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Makes the actor face the position.
 */
static int actorL_face (lua_State *L)
{
	aiActor_t *target;

	assert(lua_isactor(L, 1));

	/* Target */
	target = lua_toactor(L, 1);

	AI_TurnIntoDirection(AIL_ent, target->ent->pos);

	/* Success. */
	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Gets the actor's team.
 */
static int actorL_team (lua_State *L)
{
	const aiActor_t *target;
	const char *team;

	assert(lua_isactor(L, 1));

	target = lua_toactor(L, 1);
	assert(target != NULL);
	team = AIL_toTeamString(target->ent->team);
	lua_pushstring(L, team);
	return 1;
}


/**
 *   P O S 3 L
 */

/**
 * @brief Registers the pos3 metatable in the lua_State.
 * @param[in] L State to register the metatable in.
 * @return 0 on success.
 */
static int pos3L_register (lua_State *L)
{
	/* Create the metatable */
	luaL_newmetatable(L, POS3_METATABLE);

	/* Create the access table */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	/* Register the values */
	luaL_register(L, NULL, pos3L_methods);

	/* Clean up the stack. */
	lua_pop(L, 1);

	return 0; /* No error */
}

/**
 * @brief Checks to see if there is a pos3 metatable at index in the lua_State.
 * @param[in] L Lua state to check.
 * @param[in] index Index to check for a pos3 metatable.
 * @return 1 if index has a pos3 metatable otherwise returns 0.
 */
static int lua_ispos3 (lua_State *L, int index)
{
	int ret;

	if (lua_getmetatable(L, index) == 0)
		return 0;
	lua_getfield(L, LUA_REGISTRYINDEX, POS3_METATABLE);

	ret = 0;
	if (lua_rawequal(L, -1, -2))  /* does it have the correct metatable? */
		ret = 1;

	lua_pop(L, 2);  /* remove both metatables */
	return ret;
}

/**
 * @brief Returns the pos3 from the metatable at index.
 */
static pos3_t* lua_topos3 (lua_State *L, int index)
{
	if (lua_ispos3(L, index)) {
		return (pos3_t*) lua_touserdata(L, index);
	}
	luaL_typerror(L, index, POS3_METATABLE);
	return NULL;
}

/**
 * @brief Pushes a pos3 as a metatable at the top of the stack.
 */
static pos3_t* lua_pushpos3 (lua_State *L, pos3_t *pos)
{
	pos3_t *p;
	p = (pos3_t*) lua_newuserdata(L, sizeof(*p));
	memcpy(p, pos, sizeof(*p));
	luaL_getmetatable(L, POS3_METATABLE);
	lua_setmetatable(L, -2);
	return p;
}

/**
 * @brief Puts the pos3 information in a string.
 */
static int pos3L_tostring (lua_State *L)
{
	pos3_t *p;
	char buf[MAX_VAR];

	assert(lua_ispos3(L, 1));

	p = lua_topos3(L, 1);
	Com_sprintf(buf, sizeof(buf), "Pos3( x=%d, y=%d, z=%d )", (*p)[0], (*p)[1], (*p)[2]);

	lua_pushstring(L, buf);
	return 1;
}

/**
 * @brief Makes the actor head to the position.
 */
static int pos3L_goto (lua_State *L)
{
	pos3_t *pos;
	const byte crouchingState = G_IsCrouched(AIL_ent) ? 1 : 0;

	assert(lua_ispos3(L, 1));

	/* Calculate move table. */
	G_MoveCalc(0, AIL_ent, AIL_ent->pos, crouchingState, AIL_ent->TU);
	gi.MoveStore(level.pathingMap);

	/* Move. */
	pos = lua_topos3(L, 1);
	G_ClientMove(AIL_player, 0, AIL_ent, *pos);

	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Makes the actor face the position.
 */
static int pos3L_face (lua_State *L)
{
	pos3_t *pos;

	assert(lua_ispos3(L, 1));

	pos = lua_topos3(L, 1);
	AI_TurnIntoDirection(AIL_ent, *pos);

	lua_pushboolean(L, 1);
	return 1;
}

/**
 *    A I L
 */
/**
 * @brief Works more or less like Lua's builtin print.
 */
static int AIL_print (lua_State *L)
{
	int i;
	const int n = lua_gettop(L);  /* number of arguments */

	for (i = 1; i <= n; i++) {
		const char *s;
		qboolean meta = qfalse;

		lua_pushvalue(L, i);   /* value to print */
		if (luaL_callmeta(L, 1, "__tostring")) {
			s = lua_tostring(L, -1);
			meta = qtrue;
		} else {
			switch (lua_type(L, -1)) {
			case LUA_TNUMBER:
			case LUA_TSTRING:
				s = lua_tostring(L, -1);
				break;
			case LUA_TBOOLEAN:
				s = lua_toboolean(L, -1) ? "true" : "false";
				break;
			case LUA_TNIL:
				s = "nil";
				break;

			default:
				s = "unknown lua type";
				break;
			}
		}
		gi.DPrintf("%s%s", (i > 1) ? "\t" : "", s);
		lua_pop(L, 1); /* Pop the value */
		if (meta) /* Meta creates an additional string. */
			lua_pop(L, 1);
	}

	gi.DPrintf("\n");
	return 0;
}

/**
 * @brief Returns what the actor can see.
 */
static int AIL_see (lua_State *L)
{
	int vision, team;
	int i, j, k, n, cur;
	edict_t *check = NULL;
	aiActor_t target;
	edict_t *sorted[MAX_EDICTS], *unsorted[MAX_EDICTS];
	float distLookup[MAX_EDICTS];

	/* Defaults. */
	team = TEAM_ALL;
	vision = 0;

	/* Handle parameters. */
	if ((lua_gettop(L) > 0)) {
		/* Get what to "see" with. */
		if (lua_isstring(L, 1)) {
			const char *s = lua_tostring(L, 1);
			/** @todo Properly implement at edict level, get rid of magic numbers.
			 * These are only "placeholders". */
			if (Q_streq(s, "all"))
				vision = 0;
			else if (Q_streq(s, "sight"))
				vision = 1;
			else if (Q_streq(s, "psionic"))
				vision = 2;
			else if (Q_streq(s, "infrared"))
				vision = 3;
			else
				AIL_invalidparameter(1);
		} else
			AIL_invalidparameter(1);

		/* We now check for different teams. */
		if ((lua_gettop(L) > 1)) {
			if (lua_isstring(L, 2)) {
				const char *s = lua_tostring(L, 2);
				team = AIL_toTeamInt(s);
			} else
				AIL_invalidparameter(2);
		}
	}

	n = 0;
	/* Get visible things. */
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (AIL_ent == check)
			continue;
		if (vision == 0 && (team == TEAM_ALL || check->team == team) /* Check for team match if needed. */
		 && G_Vis(AIL_ent->team, AIL_ent, check, VT_NOFRUSTUM)) {
			distLookup[n] = VectorDistSqr(AIL_ent->pos, check->pos);
			unsorted[n++] = check;
		}
	}

	/* Sort by distance - nearest first. */
	for (i = 0; i < n; i++) { /* Until we fill sorted */
		cur = -1;
		for (j = 0; j < n; j++) { /* Check for closest */
			/* Is shorter then current minimum? */
			if (cur < 0 || distLookup[j] < distLookup[cur]) {
				/* Check if not already in sorted. */
				for (k = 0; k < i; k++)
					if (sorted[k] == unsorted[j])
						break;

				/* Not already sorted and is new minimum. */
				if (k == i)
					cur = j;
			}
		}

		sorted[i] = unsorted[cur];
	}

	/* Now save it in a Lua table. */
	lua_newtable(L);
	for (i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		target.ent = sorted[i];
		lua_pushactor(L, &target); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of actors. */
}

/**
 * @brief Toggles crouch state with true/false and returns current crouch state.
 */
static int AIL_crouch (lua_State *L)
{
	if (lua_gettop(L) > 0) {
		if (lua_isboolean(L, 1)) {
			const int state = lua_toboolean(L, 1);
			G_ClientStateChange(AIL_player, AIL_ent, STATE_CROUCHED,
				(state) ? qtrue : qfalse);
		} else
			AIL_invalidparameter(1);
	}

	lua_pushboolean(L, G_IsCrouched(AIL_ent));
	return 1;
}

/**
* @brief Checks to see if the actor is injured
*/
static int AIL_isinjured (lua_State *L)
{
	lua_pushboolean(L, AIL_ent->HP != AIL_ent->chr.maxHP);
	return 1;
}

/**
 * @brief Gets the number of TU the actor has left.
 */
static int AIL_TU (lua_State *L)
{
	lua_pushnumber(L, AIL_ent->TU);
	return 1;
}

/**
 * @brief Gets the number of HP the actor has left.
 */
static int AIL_HP (lua_State *L)
{
	lua_pushnumber(L, AIL_ent->HP);
	return 1;
}

/**
 * @brief Gets the current morale of the actor onto the stack.
 */
static int AIL_morale (lua_State *L)
{
	lua_pushnumber(L, AIL_ent->morale);
	return 1;
}

/**
 * @brief Sets the actor's reaction fire mode.
 */
static int AIL_reactionfire (lua_State *L)
{
	int reactionState = 0;
	if (lua_gettop(L) > 0) {

		if (lua_isstring(L, 1)) {
			/* get reaction fire mode */
			const char* cmd = lua_tostring(L, 1);
			reactionState = Q_streq(cmd, "disable") ? ~STATE_REACTION : STATE_REACTION;
		}

		if (reactionState && lua_gettop(L) > 1 && lua_isboolean(L, 2)) {
			const int state = lua_toboolean(L, 2);
			G_ClientStateChange(AIL_player, AIL_ent, reactionState,
				(state) ? qtrue : qfalse);
		} else {
			AIL_invalidparameter(reactionState ? 2 : 1);
		}
	}

	lua_pushboolean(L, G_IsReaction(AIL_ent));
	return 1;
}

/**
 * @brief Checks to see how many rounds the actor has left.
 */
static int AIL_roundsleft (lua_State *L)
{
	/* Right hand */
	if (RIGHT(AIL_ent) && RIGHT(AIL_ent)->item.t->reload)
		lua_pushnumber(L, RIGHT(AIL_ent)->item.a);
	else
		lua_pushnil(L);

	/* Left hand */
	if (LEFT(AIL_ent) && LEFT(AIL_ent)->item.t->reload)
		lua_pushnumber(L, LEFT(AIL_ent)->item.a);
	else
		lua_pushnil(L);
	return 2;
}

/**
 * @brief Checks to see if the actor can reload.
 */
static int AIL_canreload (lua_State *L)
{
	lua_pushboolean(L, G_ClientCanReload(AIL_ent, gi.csi->idRight));
	lua_pushboolean(L, G_ClientCanReload(AIL_ent, gi.csi->idLeft));
	return 2;
}

/**
 * @brief Actor reloads his weapons.
 */
static int AIL_reload (lua_State *L)
{
	containerIndex_t container;

	if (lua_gettop(L) > 0) {
		if (lua_isstring(L, 1)) {
			const char *s = lua_tostring(L, 1);

			if (Q_streq(s, "right"))
				container = gi.csi->idRight;
			else if (Q_streq(s, "left"))
				container = gi.csi->idLeft;
			else
				return 0;
		} else {
			AIL_invalidparameter(1);
			return 0;
		}
	} else
		container = gi.csi->idRight; /* Default to right hand. */

	G_ActorReload(AIL_ent, INVDEF(container));
	return 0;
}

/**
 * @brief Moves the actor into a position in which he can shoot his target.
 */
static int AIL_positionshoot (lua_State *L)
{
	pos3_t to, bestPos;
	vec3_t check;
	edict_t *ent;
	int dist;
	int xl, yl, xh, yh;
	int min_tu;
	aiActor_t *target;
	const byte crouchingState = G_IsCrouched(AIL_ent) ? 1 : 0;

	/* We need a target. */
	assert(lua_isactor(L, 1));
	target = lua_toactor(L, 1);

	/* Make things more simple. */
	ent = AIL_ent;
	dist = ent->TU;

	/* Calculate move table. */
	G_MoveCalc(0, ent, ent->pos, crouchingState, ent->TU);
	gi.MoveStore(level.pathingMap);

	/* set borders */
	xl = (int) ent->pos[0] - dist;
	if (xl < 0)
		xl = 0;
	yl = (int) ent->pos[1] - dist;
	if (yl < 0)
		yl = 0;
	xh = (int) ent->pos[0] + dist;
	if (xh > PATHFINDING_WIDTH)
		xl = PATHFINDING_WIDTH;
	yh = (int) ent->pos[1] + dist;
	if (yh > PATHFINDING_WIDTH)
		yh = PATHFINDING_WIDTH;

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	min_tu = INT_MAX;
	for (to[2] = 0; to[2] < PATHFINDING_HEIGHT; to[2]++)
		for (to[1] = yl; to[1] < yh; to[1]++)
			for (to[0] = xl; to[0] < xh; to[0]++) {
				pos_t tu;
				/* Can we see the target? */
				gi.GridPosToVec(gi.routingMap, ent->fieldSize, to, check);
				tu = gi.MoveLength(level.pathingMap, to, G_IsCrouched(ent) ? 1 : 0, qtrue);
				if (tu > ent->TU || tu == ROUTING_NOT_REACHABLE)
					continue;
				/* Better spot (easier to get to). */
				if (tu < min_tu) {
					if (G_ActorVis(check, ent, target->ent, qtrue) > 0.3) {
						VectorCopy(to, bestPos);
						min_tu = tu;
					}
				}
			}

	/* No position found in range. */
	if (min_tu > ent->TU) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Return the spot. */
	lua_pushpos3(L, &bestPos);
	return 1;
}

/**
 * @brief Moves the actor into a position in which he can hide.
 * @note @c team (parameter is passed through the lua stack) means that the AI tries to find
 * a hide position from the @c team members, if parameter is empty - from any enemy
 */
static int AIL_positionhide (lua_State *L)
{
	pos3_t save;
	int tus = AIL_ent->TU;
	int hidingTeam;

	VectorCopy(AIL_ent->pos, save);

	hidingTeam = AI_GetHidingTeam(AIL_ent);

	/* parse parameter */
	if (lua_gettop(L)) {
		if (lua_isstring(L, 1)) {
			const char* s = lua_tostring(L, 1);
			hidingTeam = AIL_toTeamInt(s);
			if (hidingTeam == TEAM_ALL)
				AIL_invalidparameter(1);
		} else {
			AIL_invalidparameter(1);
		}
	}

	if (AI_FindHidingLocation(hidingTeam, AIL_ent, AIL_ent->pos, &tus)) {
		/* Return the spot. */
		lua_pushpos3(L, &AIL_ent->pos);
	} else {
		lua_pushboolean(L, 0);
	}
	G_EdictSetOrigin(AIL_ent, save);
	return 1;
}

/**
 * @brief Determine the position where actor is more closer to the target and
 * locate behind the target from enemy
 * @note @c target (parameter is passed through the lua stack) The actor
 * to which AI tries to become closer
 */
static int AIL_positionherd (lua_State *L)
{
	pos3_t save;
	aiActor_t* target;

	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}

	VectorCopy(AIL_ent->pos, save);
	target = lua_toactor(L, 1);
	if (AI_FindHerdLocation(AIL_ent, AIL_ent->pos, target->ent->origin, AIL_ent->TU)) {
		lua_pushpos3(L, &AIL_ent->pos);
	} else {
		lua_pushboolean(L, 0);
	}
	G_EdictSetOrigin(AIL_ent, save);
	return 1;
}

/**
 * @brief Returns distance between AI and target
 * @note @c target (passed trough the lua stack) The target to which the distance is calculated
 */
static int AIL_distance (lua_State *L)
{
	vec_t dist;
	aiActor_t* target;

	/* check parameter */
	assert(lua_gettop(L) && lua_isactor(L, 1));

	/* calculate distance */
	target = lua_toactor(L, 1);
	dist = VectorDist(AIL_ent->origin, target->ent->origin);
	lua_pushnumber(L, dist);
	return 1;
}

/**
 * @brief The think function for the ai controlled aliens
 * @param[in] player
 * @param[in] ent
 * @sa AI_FighterCalcBestAction
 * @sa AI_CivilianCalcBestAction
 * @sa G_ClientMove
 * @sa G_ClientShoot
 */
void AIL_ActorThink (player_t * player, edict_t * ent)
{
	lua_State *L;

	/* The Lua State we will work with. */
	L = ent->AI.L;

	/* Set the global player and edict */
	AIL_ent = ent;
	AIL_player = player;

	/* Try to run the function. */
	lua_getglobal(L, "think");
	if (lua_pcall(L, 0, 0, 0)) { /* error has occured */
		gi.DPrintf("Error while running Lua: %s\n",
			lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unknown Error");
	}

	/* Cleanup */
	AIL_ent = NULL;
	AIL_player = NULL;
}


/**
 * @brief Initializes the AI.
 * @param[in] ent Pointer to actor to initialize AI for.
 * @param[in] type Type of AI (Lua file name without .lua).
 * @param[in] subtype Subtype of the AI.
 * @return 0 on success.
 */
int AIL_InitActor (edict_t * ent, const char *type, const char *subtype)
{
	AI_t *AI;
	int size;
	char path[MAX_VAR];
	char *fbuf;

	/* Prepare the AI */
	AI = &ent->AI;
	Q_strncpyz(AI->type, type, sizeof(AI->type));
	Q_strncpyz(AI->subtype, subtype, sizeof(AI->type));

	/* Create the new Lua state */
	AI->L = luaL_newstate();
	if (AI->L == NULL) {
		gi.DPrintf("Unable to create Lua state.\n");
		return -1;
	}

	/* Register metatables. */
	actorL_register(AI->L);
	pos3L_register(AI->L);

	/* Register libraries. */
	luaL_register(AI->L, AI_METATABLE, AIL_methods);

	/* Load the AI */
	Com_sprintf(path, sizeof(path), "ai/%s.lua", type);
	size = gi.FS_LoadFile(path, (byte **) &fbuf);
	if (size == 0) {
		gi.DPrintf("Unable to load Lua file '%s'.\n", path);
		return -1;
	}
	if (luaL_dobuffer(AI->L, fbuf, size, path)) {
		gi.DPrintf("Unable to parse Lua file '%s'\n", path);
		gi.FS_FreeFile(fbuf);
		return -1;
	}
	gi.FS_FreeFile(fbuf);

	return 0;
}

/**
 * @brief Cleans up the AI part of the actor.
 * @param[in] ent Pointer to actor to cleanup AI.
 */
static void AIL_CleanupActor (edict_t * ent)
{
	AI_t *AI = &ent->AI;

	/* Cleanup. */
	if (AI->L != NULL) {
		lua_close(AI->L);
		AI->L = NULL;
	}
}

void AIL_Init (void)
{
	gi.RegisterConstInt("luaaiteam::phalanx", TEAM_PHALANX);
	gi.RegisterConstInt("luaaiteam::civilian", TEAM_CIVILIAN);
	gi.RegisterConstInt("luaaiteam::alien", TEAM_ALIEN);
	gi.RegisterConstInt("luaaiteam::all", TEAM_ALL);
}

void AIL_Shutdown (void)
{
	gi.UnregisterConstVariable("luaaiteam::phalanx");
	gi.UnregisterConstVariable("luaaiteam::civilian");
	gi.UnregisterConstVariable("luaaiteam::alien");
	gi.UnregisterConstVariable("luaaiteam::all");
}

/**
 * @brief Purges all the AI from the entities.
 */
void AIL_Cleanup (void)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextActor(ent)))
		AIL_CleanupActor(ent);
}
