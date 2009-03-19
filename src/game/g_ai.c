/**
 * @file g_ai.c
 * @brief Artificial Intelligence.
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

/* uncomment this to try the new lua driven ai */
/*#define LUA_AI*/

#include "g_local.h"
#include "../common/filesys.h"

#include "lua/lauxlib.h"


#define		POS3_METATABLE		"pos3" /**< Pos3 Lua Metatable name. */
#define		ACTOR_METATABLE		"actor" /**< Actor Lua Metable name. */

/**
 * Provides an api like luaL_dostring for buffers.
 */
#define luaL_dobuffer(L, b, n, s) \
	(luaL_loadbuffer(L, b, n, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define AIL_invalidparameter(n)	\
	Com_Printf("AIL: Invalid parameter #%d in '%s'.\n", n, __func__)


typedef struct {
	pos3_t to;			/**< grid pos to walk to */
	pos3_t stop;		/**< grid pos to stop at (e.g. hiding spots) */
	byte mode;			/**< shoot_types_t */
	byte shots;			/**< how many shoots can this actor do - only set this if the target is an actor */
	edict_t *target;	/**< the target edict */
	const fireDef_t *fd;/**< the firemode to use for shooting */
	int z_align;		/**< the z-align for every shoot */
} aiAction_t;


/**
 * @brief Wrapper around edict.
 */
typedef struct aiActor_s {
	edict_t *ent; /**< Actual actor. */
} aiActor_t;


/*
 * Current AI Actor.
 */
static edict_t *AIL_ent = NULL; /**< Actor currently running the Lua AI. */
static player_t *AIL_player = NULL; /**< Player currently running the Lua AI. */


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
static const luaL_reg actorL_methods[] = {
	{"__tostring", actorL_tostring},
	{"pos", actorL_pos},
	{"shoot", actorL_shoot},
	{"face", actorL_face},
	{"team", actorL_team},
	{NULL, NULL}
}; /**< Lua Actor metatable methods. */


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
static const luaL_reg pos3L_methods[] = {
	{"__tostring", pos3L_tostring},
	{"goto", pos3L_goto},
	{"face", pos3L_face},
	{NULL, NULL}
}; /**< Lua Pos3 metatable methods. */


/**
 * General AI bindings.
 */
static int AIL_print(lua_State *L);
static int AIL_see(lua_State *L);
static int AIL_crouch(lua_State *L);
static int AIL_TU(lua_State *L);
static int AIL_reactionfire(lua_State *L);
static int AIL_roundsleft(lua_State *L);
static int AIL_canreload(lua_State *L);
static int AIL_reload(lua_State *L);
static int AIL_positionshoot(lua_State *L);
static int AIL_positionhide(lua_State *L);
static const luaL_reg AIL_methods[] = {
	{"print", AIL_print},
	{"see", AIL_see},
	{"crouch", AIL_crouch},
	{"TU", AIL_TU},
	{"reactionfire", AIL_reactionfire},
	{"roundsleft", AIL_roundsleft},
	{"canreload", AIL_canreload},
	{"reload", AIL_reload},
	{"positionshoot", AIL_positionshoot},
	{"positionhide", AIL_positionhide},
	{NULL, NULL}
}; /**< Lua AI module methods. */


/**
 * Prototypes.
 */
static void AI_TurnIntoDirection(edict_t *aiActor, pos3_t pos);


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
	if (lua_isactor(L,index)) {
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
	int fm, tu, shots;
	aiActor_t *target;
	const item_t *item;
	const fireDef_t *fdArray;
	objDef_t *od;

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

	/** @todo figure out this shot mode stuff out. */
	fm = 0;

	/* Figure out weapon to use. */
	if (IS_SHOT_RIGHT(fm) && RIGHT(AIL_ent)
			&& RIGHT(AIL_ent)->item.m
			&& RIGHT(AIL_ent)->item.t->weapon
			&& (!RIGHT(AIL_ent)->item.t->reload
				|| RIGHT(AIL_ent)->item.a > 0)) {
		item = &RIGHT(AIL_ent)->item;
	} else if (IS_SHOT_LEFT(fm) && LEFT(AIL_ent)
			&& LEFT(AIL_ent)->item.m
			&& LEFT(AIL_ent)->item.t->weapon
			&& (!LEFT(AIL_ent)->item.t->reload
				|| LEFT(AIL_ent)->item.a > 0)) {
		item = &LEFT(AIL_ent)->item;
	} else {
		/* Failure - no weapon. */
		lua_pushboolean(L, 0);
		return 1;
	}

	/** @todo Choose fire mode based on TU available - currently the first one is used. */
	fdArray = FIRESH_FiredefsIDXForWeapon(item);
	if (fdArray == NULL) {
		/* Failure - no weapon. */
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!item->m)
		od = item->t;
	else
		od = item->m;

	shots = tu / fdArray->time;

	while (shots > 0) {
		shots--;
		/** @todo actually handle fire modes */
		G_ClientShoot(AIL_player, AIL_ent->number, target->ent->pos,
				0, 0, NULL, qtrue, 0);
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
#if 0
	aiActor_t *target;

	assert(lua_isactor(L, 1));

	/** @todo Get the target */

	AI_TurnIntoDirection(AIL_ent, target->ent->pos);

	lua_pushboolean(L, 1);
#endif
	return 1;
}

/**
 * @brief Gets the actor's team.
 */
static int actorL_team (lua_State *L)
{
	const aiActor_t *target;

	assert(lua_isactor(L, 1));

	target = lua_toactor(L, 1);
	switch (target->ent->team) {
	case TEAM_PHALANX:
		lua_pushstring(L, "phalanx");
		break;
	case TEAM_CIVILIAN:
		lua_pushstring(L, "civilian");
		break;
	case TEAM_ALIEN:
		lua_pushstring(L, "alien");
		break;
	default:
		lua_pushstring(L, "unknown");
		break;
	}
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
	const int crouching_state = AIL_ent->state & STATE_CROUCHED ? 1 : 0;

	assert(lua_ispos3(L, 1));

	/* Calculate move table. */
	G_MoveCalc(0, AIL_ent->pos, AIL_ent->fieldSize, crouching_state, AIL_ent->TU);
	gi.MoveStore(gi.pathingMap);

	/* Move. */
	pos = lua_topos3(L, 1);
	G_ClientMove(AIL_player, AIL_ent->team, AIL_ent->number, *pos, qfalse, QUIET);

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
		Com_Printf("%s%s", (i > 1) ? "\t" : "", s);
		lua_pop(L, 1); /* Pop the value */
		if (meta) /* Meta creates an additional string. */
			lua_pop(L, 1);
	}

	Com_Printf("\n");
	return 0;
}

/**
 * @brief Returns what the actor can see.
 */
static int AIL_see (lua_State *L)
{
	int vision, team;
	int i, j, k, n, cur;
	edict_t *check;
	aiActor_t target;
	edict_t *sorted[MAX_EDICTS], *unsorted[MAX_EDICTS];
	float dist_lookup[MAX_EDICTS];
	const char *s;

	/* Defaults. */
	team = TEAM_NONE;
	vision = 0;

	/* Handle parameters. */
	if ((lua_gettop(L) > 0)) {
		/* Get what to "see" with. */
		if (lua_isstring(L, 1)) {
			s = lua_tostring(L, 1);
			/** @todo Properly implement at edict level, get rid of magic numbers.
			 * These are only "placeholders". */
			if (Q_strcmp(s, "all") == 0)
				vision = 0;
			else if (Q_strcmp(s, "sight") == 0)
				vision = 1;
			else if (Q_strcmp(s, "psionic") == 0)
				vision = 2;
			else if (Q_strcmp(s, "infrared") == 0)
				vision = 3;
			else
				AIL_invalidparameter(1);
		} else
			AIL_invalidparameter(1);

		/* We now check for different teams. */
		if ((lua_gettop(L) > 1)) {
			if (lua_isstring(L, 2)) {
				s = lua_tostring(L, 2);
				if (Q_strcmp(s, "all") == 0)
					team = TEAM_NONE;
				else if (Q_strcmp(s, "alien") == 0)
					team = TEAM_ALIEN;
				else if (Q_strcmp(s, "civilian") == 0)
					team = TEAM_CIVILIAN;
				else if (Q_strcmp(s, "phalanx") == 0)
					team = TEAM_PHALANX;
				else
					AIL_invalidparameter(2);
			} else
				AIL_invalidparameter(2);
		}
	}

	n = 0;
	/* Get visible things. */
	/** @todo check for what they can see instead of seeing all. */
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && G_IsLivingActor(check) && AIL_ent != check
		 && vision == 0 /* Vision checks. */
		 && (team == TEAM_NONE || check->team == team)) {/* Check for team match if needed. */
			dist_lookup[n] = VectorDistSqr(AIL_ent->pos, check->pos);
			unsorted[n++] = check;
		}

	/* Sort by distance - nearest first. */
	for (i = 0; i < n; i++) { /* Until we fill sorted */
		cur = -1;
		for (j = 0; j < n; j++) { /* Check for closest */
			/* Is shorter then current minimum? */
			if ((cur < 0) || (dist_lookup[j] < dist_lookup[cur])) {
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
			G_ClientStateChange(AIL_player, AIL_ent->number, STATE_CROUCHED,
				(state) ? qtrue : qfalse);
		} else
			AIL_invalidparameter(1);
	}

	lua_pushboolean(L, AIL_ent->state & STATE_CROUCHED);
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
 * @brief Sets the actor's reaction fire.
 */
static int AIL_reactionfire (lua_State *L)
{
	if (lua_gettop(L) > 0) {
		if (lua_isboolean(L, 1)) {
			const int state = lua_toboolean(L, 1);
			G_ClientStateChange(AIL_player, AIL_ent->number, STATE_REACTION_ONCE,
				(state) ? qtrue : qfalse);
		} else
			AIL_invalidparameter(1);
	}

	lua_pushboolean(L, AIL_ent->state & STATE_REACTION_ONCE);
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
	lua_pushboolean(L, G_ClientCanReload(AIL_player, AIL_ent->number, gi.csi->idRight));
	lua_pushboolean(L, G_ClientCanReload(AIL_player, AIL_ent->number, gi.csi->idLeft));
	return 2;
}

/**
 * @brief Actor reloads his weapons.
 */
static int AIL_reload (lua_State *L)
{
	shoot_types_t weap;

	if (lua_gettop(L) > 0) {
		if (lua_isstring(L, 1)) {
			const char *s = lua_tostring(L, 1);

			if (!Q_strcmp(s, "right"))
				weap = gi.csi->idRight;
			else if (!Q_strcmp(s, "left"))
				weap = gi.csi->idLeft;
			else
				return 0;
		} else {
			AIL_invalidparameter(1);
			return 0;
		}
	} else
		weap = gi.csi->idRight; /* Default to right hand. */

	G_ClientReload(AIL_player, AIL_ent->number, weap, QUIET);
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
	const int crouching_state = AIL_ent->state & STATE_CROUCHED ? 1 : 0;

	/* We need a target. */
	assert(lua_isactor(L, 1));
	target = lua_toactor(L, 1);

	/* Make things more simple. */
	ent = AIL_ent;
	dist = ent->TU;

	/* Calculate move table. */
	G_MoveCalc(0, ent->pos, ent->fieldSize, crouching_state, ent->TU);
	gi.MoveStore(gi.pathingMap);

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
				/* Can we see the target? */
				gi.GridPosToVec(gi.routingMap, ent->fieldSize, to, check);
				if (G_ActorVis(check, target->ent, qtrue) > 0.3) {
					const int tu = gi.MoveLength(gi.pathingMap, to,
							(ent->state & STATE_CROUCHED) ? 1 : 0, qtrue);

					/* Better spot (easier to get to). */
					if (tu < min_tu) {
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
 */
static int AIL_positionhide (lua_State *L)
{
	/** @todo Implement getting a good hiding position. */
	return 0;
}

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
#define HIDE_DIST			7

#ifndef LUA_AI
/**
 * @brief Check whether friendly units are in the line of fire when shooting
 * @param[in] ent AI that is trying to shoot
 * @param[in] target Shoot to this location
 * @param[in] spread
 */
static qboolean AI_CheckFF (const edict_t * ent, const vec3_t target, float spread)
{
	edict_t *check;
	vec3_t dtarget, dcheck, back;
	float cosSpread;
	int i;

	/* spread data */
	if (spread < 1.0)
		spread = 1.0;
	spread *= torad;
	cosSpread = cos(spread);
	VectorSubtract(target, ent->origin, dtarget);
	VectorNormalize(dtarget);
	VectorScale(dtarget, PLAYER_WIDTH / spread, back);

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && G_IsLivingActor(check) && ent != check && check->team == ent->team) {
			/* found ally */
			VectorSubtract(check->origin, ent->origin, dcheck);
			if (DotProduct(dtarget, dcheck) > 0.0) {
				/* ally in front of player */
				VectorAdd(dcheck, back, dcheck);
				VectorNormalize(dcheck);
				if (DotProduct(dtarget, dcheck) > cosSpread)
					return qtrue;
			}
		}

	/* no ally in danger */
	return qfalse;
}

/**
 * @brief Check whether the fighter should perform the shoot
 * @todo Check whether radius and power of fd are to to big for dist
 * @todo Check whether the alien will die when shooting
 */
static qboolean AI_FighterCheckShoot (const edict_t* ent, const edict_t* check, const fireDef_t* fd, float *dist)
{
	/* check range */
	*dist = VectorDist(ent->origin, check->origin);
	if (*dist > fd->range)
		return qfalse;
	/* don't shoot - we are to close */
	else if (*dist < fd->splrad)
		return qfalse;

	/* check FF */
	if (!(ent->state & STATE_INSANE) && AI_CheckFF(ent, check->origin, fd->spread[0]))
		return qfalse;

	return qtrue;
}
#endif /* LUA_AI */

/**
 * @brief Checks whether the AI controlled actor wants to use a door
 * @param[in] ent The AI controlled actor
 * @param[in] door The door edict
 * @returns true if the AI wants to use (open/close) that door, false otherwise
 * @note Don't start any new events in here, don't change the actor state
 * @sa Touch_DoorTrigger
 * @todo Finish implementation
 */
qboolean AI_CheckUsingDoor (const edict_t *ent, const edict_t *door)
{
	/* don't try to use the door in every case */
	if (frand() < 0.3)
		return qfalse;

	/* not in the view frustom - don't use the door while not seeing it */
	if (!G_FrustumVis(door, ent->origin))
		return qfalse;

	/* if the alien is trying to hide and the door is
	* still opened, close it */
	if (ent->hiding && door->moveinfo.state == STATE_OPENED)
		return qtrue;

	/* aliens and civilians need different handling */
	switch (ent->team) {
	case TEAM_ALIEN: {
		/* only use the door when there is no civilian or phalanx to kill */
		int i;
		const edict_t *check;

		/* see if there are enemies */
		for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
			if (!check->inuse)
				continue;
			/* don't check for aliens */
			if (check->team == ent->team)
				continue;
			/* if it's an actor and he's still living */
			if (G_IsLivingActor(check)) {
				/* check whether the origin of the enemy is inside the
				* AI actors view frustum */
				float actorVis;
				qboolean frustum = G_FrustumVis(check, ent->origin);
				if (!frustum)
					continue;
				/* check whether the enemy is close enough to change the state */
				if (VectorDist(check->origin, ent->origin) > MAX_SPOT_DIST)
					continue;
				actorVis = G_ActorVis(check->origin, ent, qtrue);
				/* there is a visible enemy, don't use that door */
				if (actorVis > ACTOR_VIS_0)
					return qfalse;
				}
			}
		}
		break;
	case TEAM_CIVILIAN:
		/* don't use any door if no alien is inside the viewing angle  - but
		 * try to hide behind the door when there is an alien */
		break;
	default:
		Com_Printf("Invalid team in AI_CheckUsingDoor: %i for ent type: %i\n",
			ent->team, ent->type);
		break;
	}
	return qtrue;
}

#ifndef LUA_AI
/**
 * @brief Checks whether it would be smart to change the state to STATE_CROUCHED
 * @param[in] ent The AI controlled actor to chech the state change for
 * @returns true if the actor should go into STATE_CROUCHED, false otherwise
 */
static qboolean AI_CheckCrouch (const edict_t *ent)
{
	int i;
	const edict_t *check;

	/* see if we are very well visible by an enemy */
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		if (!check->inuse)
			continue;
		/* don't check for civilians or aliens */
		if (check->team == ent->team || check->team == TEAM_CIVILIAN)
			continue;
		/* if it's an actor and he's still living */
		if (G_IsLivingActor(check)) {
			/* check whether the origin of the enemy is inside the
			 * AI actors view frustum */
			float actorVis;
			qboolean frustum = G_FrustumVis(check, ent->origin);
			if (!frustum)
				continue;
			/* check whether the enemy is close enough to change the state */
			if (VectorDist(check->origin, ent->origin) > MAX_SPOT_DIST)
				continue;
			actorVis = G_ActorVis(check->origin, ent, qtrue);
			if (actorVis >= 0.6)
				return qtrue;
		}
	}
	return qfalse;
}

/**
 * @sa AI_ActorThink
 * @todo fix firedef stuff
 * @todo fill z_align values
 */
static float AI_FighterCalcBestAction (edict_t * ent, pos3_t to, aiAction_t * aia)
{
	edict_t *check;
	int move, delta = 0, tu;
	int i, fm, shots, reaction_trap = 0;
	float dist, minDist;
	float bestActionPoints, dmg, maxDmg, best_time = -1, vis;
	const objDef_t *ad;
	int still_searching = 1;

	bestActionPoints = 0.0;
	memset(aia, 0, sizeof(*aia));
	move = gi.MoveLength(gi.pathingMap, to,
			(ent->state & STATE_CROUCHED) ? 1 : 0, qtrue);
	tu = ent->TU - move;

	/* test for time */
	if (tu < 0)
		return AI_ACTION_NOTHING_FOUND;

	/* see if we are very well visible by a reacting enemy */
	/** @todo this is worthless now; need to check all squares along our way! */
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && G_IsLivingActor(check) && ent != check
			 && (check->team != ent->team || ent->state & STATE_INSANE)
			 /* also check if we are in range of the weapon's primary mode */
			 && check->state & STATE_REACTION) {
			qboolean frustum = G_FrustumVis(check, ent->origin);
			const float actorVis = G_ActorVis(check->origin, ent, qtrue);
			if (actorVis > 0.6 && frustum
				&& (VectorDistSqr(check->origin, ent->origin)
					> MAX_SPOT_DIST * MAX_SPOT_DIST))
				reaction_trap++;
		}

	/** Don't waste TU's when in reaction fire trap
	 * @todo Learn to escape such traps if move == 2 || move == 3 */
	bestActionPoints -= move * reaction_trap * GUETE_REACTION_FEAR_FACTOR;

	/* set basic parameters */
	VectorCopy(to, ent->pos);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, to, ent->origin);

	/* shooting */
	maxDmg = 0.0;
	for (fm = 0; fm < ST_NUM_SHOOT_TYPES; fm++) {
		const item_t *item;
		int fdIdx;
		const fireDef_t *fdArray;

		/* optimization: reaction fire is automatic */
		if (IS_SHOT_REACTION(fm))
			continue;

		if (IS_SHOT_RIGHT(fm) && RIGHT(ent)
			&& RIGHT(ent)->item.m
			&& RIGHT(ent)->item.t->weapon
			&& (!RIGHT(ent)->item.t->reload
				|| RIGHT(ent)->item.a > 0)) {
			item = &RIGHT(ent)->item;
		} else if (IS_SHOT_LEFT(fm) && LEFT(ent)
			&& LEFT(ent)->item.m
			&& LEFT(ent)->item.t->weapon
			&& (!LEFT(ent)->item.t->reload
				|| LEFT(ent)->item.a > 0)) {
			item = &LEFT(ent)->item;
		} else {
			item = NULL;
			Com_DPrintf(DEBUG_GAME, "AI_FighterCalcBestAction: todo - grenade/knife toss from inventory using empty hand\n");
			/** @todo grenade/knife toss from inventory using empty hand */
			/** @todo evaluate possible items to retrieve and pick one, then evaluate an action against the nearby enemies or allies */
		}

		if (!item)
			continue;

		fdArray = FIRESH_FiredefsIDXForWeapon(item);
		if (fdArray == NULL)
			continue;

		/** @todo timed firedefs that bounce around should not be thrown/shooten about the hole distance */
		for (fdIdx = 0; fdIdx < item->t->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
			const fireDef_t *fd = &fdArray[fdIdx];

			const float nspread = SPREAD_NORM((fd->spread[0] + fd->spread[1]) * 0.5 +
				GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill));
			/* how many shoots can this actor do */
			shots = tu / fd->time;
			if (shots) {
				/* search best target */
				for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
					if (check->inuse && G_IsLivingActor(check) && ent != check
						&& (check->team != ent->team || ent->state & STATE_INSANE)) {

						/* don't shoot civilians in mp */
						if (check->team == TEAM_CIVILIAN && sv_maxclients->integer > 1 && !(ent->state & STATE_INSANE))
							continue;

						if (!AI_FighterCheckShoot(ent, check, fd, &dist))
							continue;

						/* check whether target is visible */
						vis = G_ActorVis(ent->origin, check, qtrue);
						if (vis == ACTOR_VIS_0)
							continue;

						/* calculate expected damage */
						dmg = vis * (fd->damage[0] + fd->spldmg[0]) * fd->shots * shots;
						if (nspread && dist > nspread)
							dmg *= nspread / dist;

						/* take into account armour */
						if (check->i.c[gi.csi->idArmour]) {
							ad = check->i.c[gi.csi->idArmour]->item.t;
							dmg *= 1.0 - ad->protection[ad->dmgtype] * 0.01;
						}

						if (dmg > check->HP
							&& check->state & STATE_REACTION)
							/* reaction shooters eradication bonus */
							dmg = check->HP + GUETE_KILL
								+ GUETE_REACTION_ERADICATION;
						else if (dmg > check->HP)
							/* standard kill bonus */
							dmg = check->HP + GUETE_KILL;

						/* ammo is limited and shooting gives away your position */
						if ((dmg < 25.0 && vis < 0.2) /* too hard to hit */
							|| (dmg < 10.0 && vis < 0.6) /* uber-armour */
							|| dmg < 0.1) /* at point blank hit even with a stick */
							continue;

						/* civilian malus */
						if (check->team == TEAM_CIVILIAN && !(ent->state & STATE_INSANE))
							dmg *= GUETE_CIV_FACTOR;

						/* add random effects */
						dmg += GUETE_RANDOM * frand();

						/* check if most damage can be done here */
						if (dmg > maxDmg) {
							maxDmg = dmg;
							best_time = fd->time * shots;
							aia->mode = fm;
							aia->shots = shots;
							aia->target = check;
							aia->fd = fd;
						}
					}

				if (!aia->target) {
					/* search best none human target */
					for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
						if (check->inuse && (check->flags & FL_DESTROYABLE)) {

							if (!AI_FighterCheckShoot(ent, check, fd, &dist))
								continue;

							/* don't take vis into account, don't multiply with amout of shots
							 * others (human victims) should be prefered, that's why we don't
							 * want a too high value here */
							maxDmg = (fd->damage[0] + fd->spldmg[0]);
							aia->mode = fm;
							aia->shots = shots;
							aia->target = check;
							aia->fd = fd;
							best_time = fd->time * shots;
							/* take the first best breakable or door and try to shoot it */
							break;
						}
				}
			}
		} /* firedefs */
	}
	/* add damage to bestActionPoints */
	if (aia->target) {
		bestActionPoints += maxDmg;
		assert(best_time > 0);
		tu -= best_time;
	}

	if (!(ent->state & STATE_RAGE)) {
		/* hide */
		/** @todo Only hide if the visible actors have long range weapons in their hands
		 * otherwise make it depended on the mood (or some skill) of the alien whether
		 * it tries to attack by trying to get as close as possible or to try to hide */
		if (!(G_TestVis(-ent->team, ent, VT_PERISH | VT_NOFRUSTUM) & VIS_YES)) {
			/* is a hiding spot */
			bestActionPoints += GUETE_HIDE + (aia->target ? GUETE_CLOSE_IN : 0);
		/** @todo What is this 2? */
		} else if (aia->target && tu >= 2) {
			byte minX, maxX, minY, maxY;
			/* reward short walking to shooting spot, when seen by enemies;
			 * @todo do this decently, only penalizing the visible part of walk
			 * and penalizing much more for reaction shooters around;
			 * now it may remove some tactical options from aliens,
			 * e.g. they may now choose only the closer doors;
			 * however it's still better than going three times around soldier
			 * and only then firing at him */
			bestActionPoints += GUETE_CLOSE_IN - move < 0 ? 0 : GUETE_CLOSE_IN - move;

			/* search hiding spot */
			G_MoveCalc(0, to, ent->fieldSize,
					(ent->state & STATE_CROUCHED) ? 1 : 0, HIDE_DIST);
			ent->pos[2] = to[2];
			minX = to[0] - HIDE_DIST > 0 ? to[0] - HIDE_DIST : 0;
			minY = to[1] - HIDE_DIST > 0 ? to[1] - HIDE_DIST : 0;
			maxX = to[0] + HIDE_DIST < 254 ? to[0] + HIDE_DIST : 254;
			maxY = to[1] + HIDE_DIST < 254 ? to[1] + HIDE_DIST : 254;

			for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
				for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
					/* time */
					delta = gi.MoveLength(gi.pathingMap, ent->pos,
							(ent->state & STATE_CROUCHED) ? 1 : 0, qfalse);
					if (delta > tu)
						continue;

					/* visibility */
					gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
					if (G_TestVis(-ent->team, ent, VT_PERISH | VT_NOFRUSTUM) & VIS_YES)
						continue;

					still_searching = 0;
					break;
				}
				if (!still_searching)
					break;
			}
		}

		if (still_searching) {
			/* nothing found */
			VectorCopy(to, ent->pos);
			gi.GridPosToVec(gi.routingMap, ent->fieldSize, to, ent->origin);
			/** @todo Try to crouch if no hiding spot was found - randomized */
		} else {
			/* found a hiding spot */
			VectorCopy(ent->pos, aia->stop);
			bestActionPoints += GUETE_HIDE;
			tu -= delta;
			/** @todo also add bonus for fleeing from reaction fire
			 * and a huge malus if more than 1 move under reaction */
		}
	}

	/* reward closing in */
	minDist = CLOSE_IN_DIST;
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && check->team != ent->team && G_IsLivingActor(ent)) {
			dist = VectorDist(ent->origin, check->origin);
			if (dist < minDist)
				minDist = dist;
		}
	bestActionPoints += GUETE_CLOSE_IN * (1.0 - minDist / CLOSE_IN_DIST);

	return bestActionPoints;
}

/**
 * @sa AI_ActorThink
 * @note Even civilians can use weapons if the teamdef allows this
 */
static float AI_CivilianCalcBestAction (edict_t * ent, pos3_t to, aiAction_t * aia)
{
	edict_t *check;
	int i, move, tu;
	float minDist, minDistCivilian, minDistFighter;
	float bestActionPoints;
	float reaction_trap = 0.0;
	float delta = 0.0;
	const int crouching_state = ent->state & STATE_CROUCHED ? 1 : 0;

	/* set basic parameters */
	bestActionPoints = 0.0;
	memset(aia, 0, sizeof(*aia));
	VectorCopy(to, ent->pos);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, to, ent->origin);

	move = gi.MoveLength(gi.pathingMap, to, crouching_state, qtrue);
	tu = ent->TU - move;

	/* test for time */
	if (tu < 0)
		return AI_ACTION_NOTHING_FOUND;

	/* check whether this civilian can use weapons */
	if (ent->chr.teamDef) {
		const teamDef_t* teamDef = ent->chr.teamDef;
		if (ent->state & ~STATE_PANIC && teamDef->weapons)
			return AI_FighterCalcBestAction(ent, to, aia);
	} else
		Com_Printf("AI_CivilianCalcBestAction: Error - civilian team with no teamdef\n");

	/* run away */
	minDist = minDistCivilian = minDistFighter = RUN_AWAY_DIST * UNIT_SIZE;

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		float dist;
		if (!check->inuse || ent == check)
			continue;
		if (!G_IsLivingActor(check))
			continue;
		dist = VectorDist(ent->origin, check->origin);
		assert(dist);
		switch (check->team) {
		case TEAM_ALIEN:
			if (dist < minDist)
				minDist = dist;
			break;
		case TEAM_CIVILIAN:
			if (dist < minDistCivilian)
				minDistCivilian = dist;
			break;
		case TEAM_PHALANX:
			if (dist < minDistFighter)
				minDistFighter = dist;
			break;
		}
	}

	minDist /= UNIT_SIZE;
	minDistCivilian /= UNIT_SIZE;
	minDistFighter /= UNIT_SIZE;

	if (minDist < 8.0) {
		/* very near an alien: run away fast */
		delta = 4.0 * minDist;
	} else if (minDist < 16.0) {
		/* near an alien: run away */
		delta = 24.0 + minDist;
	} else if (minDist < 24.0) {
		/* near an alien: run away slower */
		delta = 40.0 + (minDist - 16) / 4;
	} else {
		delta = 42.0;
	}
	/* near a civilian: join him (1/3) */
	if (minDistCivilian < 10.0)
		delta += (10.0 - minDistCivilian) / 3.0;
	/* near a fighter: join him (1/5) */
	if (minDistFighter < 15.0)
		delta += (15.0 - minDistFighter) / 5.0;
	/* don't go close to a fighter to let him move */
	if (minDistFighter < 2.0)
		delta /= 10.0;

	/* try to hide */
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		if (!check->inuse || ent == check)
			continue;
		if (!(check->team == TEAM_ALIEN || ent->state & STATE_INSANE))
			continue;
		if (G_IsLivingActor(check) && G_ActorVis(check->origin, ent, qtrue) > 0.25)
			reaction_trap += 25.0;
	}
	delta -= reaction_trap;
	bestActionPoints += delta;

	/* add laziness */
	if (ent->TU)
		bestActionPoints += GUETE_CIV_LAZINESS * tu / ent->TU;
	/* add random effects */
	bestActionPoints += GUETE_CIV_RANDOM * frand();

	return bestActionPoints;
}
#endif /* LUA_AI */

edict_t *ai_waypointList;

void G_AddToWayPointList (edict_t *ent)
{
	int i = 1;

	if (!ai_waypointList)
		ai_waypointList = ent;
	else {
		edict_t *e = ai_waypointList;
		while (e->groupChain) {
			e = e->groupChain;
			i++;
		}
		i++;
		e->groupChain = ent;
	}

	Com_DPrintf(DEBUG_GAME, "%i waypoints in this map\n", i);
}

#ifndef LUA_AI
/**
 * @brief Searches the map for mission edicts and try to get there
 * @sa AI_PrepBestAction
 * @note The routing table is still valid, so we can still use
 * gi.MoveLength for the given edict here
 */
static int AI_CheckForMissionTargets (player_t* player, edict_t *ent, aiAction_t *aia)
{
	int bestActionPoints = AI_ACTION_NOTHING_FOUND;
	const int crouching_state = ent->state & STATE_CROUCHED ? 1 : 0;

	/* reset any previous given action set */
	memset(aia, 0, sizeof(*aia));

	switch (ent->team) {
	case TEAM_CIVILIAN:
	{
		edict_t *checkPoint = NULL;
		int length;
		int i = 0;
		/* find waypoints in a closer distance - if civilians are not close enough, let them walk
		 * around until they came close */
		checkPoint = ai_waypointList;
		while (checkPoint != NULL) {
			if (!checkPoint->inuse)
				continue;

			/* the lower the count value - the nearer the final target */
			if (checkPoint->count < ent->count) {
				if (VectorDist(ent->origin, checkPoint->origin) <= WAYPOINT_CIV_DIST) {
					i++;
					Com_DPrintf(DEBUG_GAME, "civ found civtarget with %i\n", checkPoint->count);
					/* test for time and distance */
					length = ent->TU - gi.MoveLength(gi.pathingMap, checkPoint->pos, crouching_state, qtrue);
					bestActionPoints = GUETE_MISSION_TARGET + length;

					ent->count = checkPoint->count;
					VectorCopy(checkPoint->pos, aia->to);
				}
			}
			checkPoint = checkPoint->groupChain;
		}
		/* reset the count value for this civilian to restart the search */
		if (!i)
			ent->count = 100;
		break;
	}
	case TEAM_ALIEN:
	{
		edict_t *mission;
		int i;
		/* search for a mission edict */
		for (i = 0, mission = g_edicts; i < globals.num_edicts; i++, mission++)
			if (mission->inuse && mission->type == ET_MISSION) {
				VectorCopy(mission->pos, aia->to);
				aia->target = mission;
				if (player->pers.team == mission->team) {
					bestActionPoints = GUETE_MISSION_TARGET;
					break;
				} else
					/* try to prevent the phalanx from reaching their mission target */
					bestActionPoints = GUETE_MISSION_OPPONENT_TARGET;
			}

		break;
	}
	}

	return bestActionPoints;
}

#define AI_MAX_DIST	30
/**
 * @brief Attempts to find the best action for an alien. Moves the alien
 * into the starting position for that action and returns the action.
 * @param[in] player The AI player
 * @param[in] ent The AI actor
 */
static aiAction_t AI_PrepBestAction (player_t *player, edict_t * ent)
{
	aiAction_t aia, bestAia;
	pos3_t oldPos, to;
	vec3_t oldOrigin;
	int xl, yl, xh, yh;
	float bestActionPoints, best;
	const int crouching_state = ent->state & STATE_CROUCHED ? 1 : 0;

	/* calculate move table */
	G_MoveCalc(0, ent->pos, ent->fieldSize, crouching_state, MAX_ROUTE);
	Com_DPrintf(DEBUG_ENGINE, "AI_PrepBestAction: Called MoveMark.\n");
	gi.MoveStore(gi.pathingMap);

	/* set borders */
	xl = (int) ent->pos[0] - AI_MAX_DIST;
	if (xl < 0)
		xl = 0;
	yl = (int) ent->pos[1] - AI_MAX_DIST;
	if (yl < 0)
		yl = 0;
	xh = (int) ent->pos[0] + AI_MAX_DIST;
	if (xh > PATHFINDING_WIDTH)
		xl = PATHFINDING_WIDTH;
	yh = (int) ent->pos[1] + AI_MAX_DIST;
	if (yh > PATHFINDING_WIDTH)
		yh = PATHFINDING_WIDTH;

	/* search best action */
	best = AI_ACTION_NOTHING_FOUND;
	VectorCopy(ent->pos, oldPos);
	VectorCopy(ent->origin, oldOrigin);

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	for (to[2] = 0; to[2] < PATHFINDING_HEIGHT; to[2]++)
		for (to[1] = yl; to[1] < yh; to[1]++)
			for (to[0] = xl; to[0] < xh; to[0]++)
				if (gi.MoveLength(gi.pathingMap, to, crouching_state, qtrue) <= ent->TU) {
					if (ent->team == TEAM_CIVILIAN || ent->state & STATE_PANIC)
						bestActionPoints = AI_CivilianCalcBestAction(ent, to, &aia);
					else
						bestActionPoints = AI_FighterCalcBestAction(ent, to, &aia);

					if (bestActionPoints > best) {
						bestAia = aia;
						best = bestActionPoints;
					}
				}

	VectorCopy(oldPos, ent->pos);
	VectorCopy(oldOrigin, ent->origin);

	bestActionPoints = AI_CheckForMissionTargets(player, ent, &aia);
	if (bestActionPoints > best) {
		bestAia = aia;
		best = bestActionPoints;
	}

	/* nothing found to do */
	if (best == AI_ACTION_NOTHING_FOUND) {
		bestAia.target = NULL;
		return bestAia;
	}

	/* check if the actor is in crouched state and try to stand up before doing the move */
	if (ent->state & STATE_CROUCHED)
		G_ClientStateChange(player, ent->number, STATE_CROUCHED, qtrue);

	/* do the move */
	/** @todo Why 0 - and not ent->team?
	 * I think this has something to do with the vis check in G_BuildForbiddenList */
	G_ClientMove(player, 0, ent->number, bestAia.to, qfalse, QUIET);

	/* test for possible death during move. reset bestAia due to dead status */
	if (G_IsDead(ent))
		memset(&bestAia, 0, sizeof(bestAia));

	if (g_aidebug->integer)
		Com_DPrintf(DEBUG_GAME, "bestAIAction.to (%i %i %i) stop (%i %i %i)\n",
			(int)bestAia.to[0], (int)bestAia.to[1], (int)bestAia.to[2],
			(int)bestAia.stop[0], (int)bestAia.stop[1], (int)bestAia.stop[2]);

	return bestAia;
}
#endif /* !LUA_AI */

/**
 * @brief This function will turn the AI actor into the direction that is needed to walk
 * to the given location
 * @param[in] pos The position to set the direction for
 */
static void AI_TurnIntoDirection (edict_t *aiActor, pos3_t pos)
{
	int dv;
	const int crouching_state = aiActor->state & STATE_CROUCHED ? 1 : 0;

	G_MoveCalc(aiActor->team, pos, aiActor->fieldSize, crouching_state, MAX_ROUTE);

	dv = gi.MoveNext(gi.routingMap, aiActor->fieldSize, gi.pathingMap, pos, crouching_state);
	if (dv != ROUTING_UNREACHABLE) {
		const byte dir = getDVdir(dv);
		/* Only attempt to turn if the direction is not a vertical only action */
		if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS) {
			const int status = G_DoTurn(aiActor, dir & (CORE_DIRECTIONS - 1));
			if (status) {
				/* send the turn */
				gi.AddEvent(G_VisToPM(aiActor->visflags), EV_ACTOR_TURN);
				gi.WriteShort(aiActor->number);
				gi.WriteByte(aiActor->dir);
			}
		}
	}
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
void AI_ActorThink (player_t * player, edict_t * ent)
{
#ifdef LUA_AI
	lua_State *L;

	/* The Lua State we will work with. */
	L = ent->chr.AI.L;

	/* Set the global player and edict */
	AIL_ent = ent;
	AIL_player = player;

	/* Try to run the function. */
	lua_getglobal(L, "think");
	if (lua_pcall(L, 0, 0, 0)) { /* error has occured */
		Com_Printf("Error while running Lua: %s\n",
			lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unknown Error");
	}

	/* Cleanup */
	AIL_ent = NULL;
	AIL_player = NULL;
#else /* LUA_AI */
	aiAction_t bestAia;

	/* if a weapon can be reloaded we attempt to do so if TUs permit, otherwise drop it */
	if (!(ent->state & STATE_PANIC)) {
		if (RIGHT(ent) && RIGHT(ent)->item.t->reload && RIGHT(ent)->item.a == 0) {
			if (G_ClientCanReload(game.players + ent->pnum, ent->number, gi.csi->idRight)) {
				G_ClientReload(player, ent->number, ST_RIGHT_RELOAD, QUIET);
			} else {
				G_ClientInvMove(game.players + ent->pnum, ent->number, &gi.csi->ids[gi.csi->idRight], RIGHT(ent), &gi.csi->ids[gi.csi->idFloor], NONE, NONE, qtrue, QUIET);
			}
		}
		if (LEFT(ent) && LEFT(ent)->item.t->reload && LEFT(ent)->item.a == 0) {
			if (G_ClientCanReload(game.players + ent->pnum, ent->number, gi.csi->idLeft)) {
				G_ClientReload(player, ent->number, ST_LEFT_RELOAD, QUIET);
			} else {
				G_ClientInvMove(game.players + ent->pnum, ent->number, &gi.csi->ids[gi.csi->idLeft], LEFT(ent), &gi.csi->ids[gi.csi->idFloor], NONE, NONE, qtrue, QUIET);
			}
		}
	}

	/* if both hands are empty, attempt to get a weapon out of backpack if TUs permit */
	if (ent->chr.teamDef->weapons && !LEFT(ent) && !RIGHT(ent)) {
		G_ClientGetWeaponFromInventory(player, ent->number, QUIET);
		if (LEFT(ent) || RIGHT(ent))
			Com_DPrintf(DEBUG_GAME, "AI_ActorThink: Got weapon from inventory\n");
	}

	bestAia = AI_PrepBestAction(player, ent);

	/* shoot and hide */
	if (bestAia.target) {
		const int fdIdx = bestAia.fd ? bestAia.fd->fdIdx : 0;
		/* shoot until no shots are left or target died */
		while (bestAia.shots) {
			G_ClientShoot(player, ent->number, bestAia.target->pos, bestAia.mode, fdIdx, NULL, qtrue, bestAia.z_align);
			bestAia.shots--;
			/* dies by our own shot? */
			if (G_IsDead(ent))
				return;
			/* check for target's death */
			if (G_IsDead(bestAia.target)) {
				/* search another target now */
				bestAia = AI_PrepBestAction(player, ent);
				/* no other target found - so no need to hide */
				if (!bestAia.target)
					return;
			}
		}
		ent->hiding = qtrue;

		/* now hide */
		G_ClientMove(player, ent->team, ent->number, bestAia.stop, qfalse, QUIET);
		/* no shots left, but possible targets left - maybe they shoot back
		 * or maybe they are still close after hiding */

		/* decide whether the actor maybe wants to go crouched */
		if (AI_CheckCrouch(ent))
			G_ClientStateChange(player, ent->number, STATE_CROUCHED, qtrue);

		/* actor is still alive - try to turn into the appropriate direction to see the target
		 * actor once he sees the ai, too */
		AI_TurnIntoDirection(ent, bestAia.target->pos);

		/** @todo If possible targets that can shoot back (check their inventory for weapons, not for ammo)
		 * are close, go into reaction fire mode, too */

		ent->hiding = qfalse;
	}
#endif /* LUA_AI */
}


/**
 * @brief Every server frame one single actor is handled - always in the same order
 * @sa G_RunFrame
 */
void AI_Run (void)
{
	player_t *player;
	edict_t *ent;
	int i, j;

	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;

	/* set players to ai players and cycle over all of them */
	for (i = 0, player = game.players + game.sv_maxplayersperteam; i < game.sv_maxplayersperteam; i++, player++)
		if (player->inuse && player->pers.ai && level.activeTeam == player->pers.team) {
			/* find next actor to handle */
			if (!player->pers.last)
				ent = g_edicts;
			else
				ent = player->pers.last + 1;

			for (j = ent - g_edicts; j < globals.num_edicts; j++, ent++)
				if (ent->inuse && ent->TU && ent->team == player->pers.team && G_IsLivingActor(ent)) {
					AI_ActorThink(player, ent);
					player->pers.last = ent;
					return;
				}

			/* nothing left to do, request endround */
			if (j >= globals.num_edicts) {
				G_ClientEndRound(player, QUIET);
				player->pers.last = g_edicts + globals.num_edicts;
			}
			return;
		}
}

/**
 * @brief Initializes the AI.
 * @param[in] ent Pointer to actor to initialize AI for.
 * @param[in] type Type of AI (Lua file name without .lua).
 * @param[in] subtype Subtype of the AI.
 * @return 0 on success.
 */
static int AI_InitActor (edict_t * ent, char *type, char *subtype)
{
	AI_t *AI;
	int size;
	char path[MAX_VAR];
	char *fbuf;

	/* Prepare the AI */
	AI = &ent->chr.AI;
	Q_strncpyz(AI->type, type, sizeof(AI->type));
	Q_strncpyz(AI->subtype, subtype, sizeof(AI->type));

	/* Create the new Lua state */
	AI->L = luaL_newstate();
	if (AI->L == NULL) {
		Com_Printf("Unable to create Lua state.\n");
		return -1;
	}

	/* Register metatables. */
	actorL_register(AI->L);
	pos3L_register(AI->L);

	/* Register libraries. */
	luaL_register(AI->L, "ai", AIL_methods);

	/* Load the AI */
	Com_sprintf(path, sizeof(path), "ai/%s.lua", type);
	size = gi.FS_LoadFile(path, (byte **) &fbuf);
	if (size == 0) {
		Com_Printf("Unable to load Lua file '%s'.\n", path);
		return -1;
	}
	if (luaL_dobuffer(AI->L, fbuf, size, path)) {
		Com_Printf("Unable to parse Lua file '%s'\n", path);
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
static void AI_CleanupActor (edict_t * ent)
{
	AI_t *AI;
	AI = &ent->chr.AI;

	/* Cleanup. */
	if (AI->L != NULL) {
		lua_close(AI->L);
		AI->L = NULL;
	}
}

/**
 * @brief Purges all the AI from the entities.
 */
void AI_Cleanup (void)
{
	int i;
	edict_t *ent;

	for (i = 0; i < globals.num_edicts; i++) {
		ent = g_edicts+i;
		if (ent->inuse && ent->type == ET_ACTOR)
			AI_CleanupActor(ent);
	}
}


/**
 * @brief Initializes the Actor's stats.
 * First we get the templates, then we set the stats, then we tweak stats.
 * @param ent Actor to set stats.
 * @param[in] team Team to which actor belongs.
 */
static void AI_SetStats (edict_t * ent, int team)
{
	/*
	 * Set base stats.
	 */
	if (team != TEAM_CIVILIAN) {
		/** skills; @todo more power to Ortnoks, more mind to Tamans
		 * For aliens we give "MAX_EMPL" as a type, since the emplyoee-type is not used at all for them. */
		CHRSH_CharGenAbilitySkills(&ent->chr, team, MAX_EMPL, sv_maxclients->integer >= 2);
		/* Aliens get much more mind */
		ent->chr.score.skills[ABILITY_MIND] += 100;
		if (ent->chr.score.skills[ABILITY_MIND] >= MAX_SKILL)
			ent->chr.score.skills[ABILITY_MIND] = MAX_SKILL;
	} else if (team == TEAM_CIVILIAN) {
		CHRSH_CharGenAbilitySkills(&ent->chr, team, EMPL_SOLDIER, sv_maxclients->integer >= 2);
	}

	/*
	 * Set health, morale and stun.
	 */
	ent->chr.HP = GET_HP(ent->chr.score.skills[ABILITY_POWER]);
	ent->HP = ent->chr.HP;
	ent->chr.morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
	if (ent->chr.morale >= MAX_SKILL)
		ent->chr.morale = MAX_SKILL;
	ent->morale = ent->chr.morale;
	ent->STUN = 0;

	/*
	 * Stat post tweaks.
	 */
	if (team == TEAM_CIVILIAN) {
		ent->chr.HP /= 2; /* civilians get half health */
		ent->HP = ent->chr.HP;
		if (ent->chr.morale > 45) /* they can't get over 45 morale */
			ent->chr.morale = 45;
	}
}


/**
 * @brief Sets an Actor's ingame model and character values.
 * @param ent Actor to set model of.
 * @param[in] team Team to which actor belongs.
 */
static void AI_SetModelAndCharacterValues (edict_t * ent, int team)
{
	/* Set model. */
	const char *teamDefintion;
	if (team != TEAM_CIVILIAN) {
		if (gi.csi->numAlienTeams) {
			const int alienTeam = rand() % gi.csi->numAlienTeams;
			assert(gi.csi->alienTeams[alienTeam]);
			teamDefintion = gi.csi->alienTeams[alienTeam]->id;
		} else
			teamDefintion = gi.Cvar_String("ai_alien");
	} else if (team == TEAM_CIVILIAN) {
		/** @todo Maybe we have civilians with armour, too - police and so on */
		teamDefintion = gi.Cvar_String("ai_civilian");
	}
	ent->chr.skin = gi.GetCharacterValues(teamDefintion, &ent->chr);
	ent->body = gi.ModelIndex(CHRSH_CharGetBody(&ent->chr));
	ent->head = gi.ModelIndex(CHRSH_CharGetHead(&ent->chr));
	ent->skin = ent->chr.skin;
	if (!ent->chr.teamDef)
		gi.error("Could not set teamDef for character: '%s'", teamDefintion);
}


/**
 * @brief Sets the Actor's equipment.
 * @param ent Actor to give equipment to.
 * @param[in] team Team to which the Actor belongs.
 * @param[in] ed Equipment definition for the new Actor.
 */
static void AI_SetEquipment (edict_t * ent, int team, equipDef_t * ed)
{
	/* Pack equipment. */
	if (team != TEAM_CIVILIAN) { /** @todo Give civilians gear. */
		if (ent->chr.teamDef->weapons)
			INVSH_EquipActor(&ent->i, ed, &ent->chr);
		else if (ent->chr.teamDef)
			/* actor cannot handle equipment but no weapons */
			INVSH_EquipActorMelee(&ent->i, &ent->chr);
		else
			Com_Printf("AI_InitPlayer: actor with no equipment\n");
	}
}


/**
 * @brief Initializes the Actor.
 * @param[in] player Player to which this actor belongs.
 * @param ent Pointer to edict_t representing actor.
 * @param[in] ed Equipment definition for the new Actor.
 */
static void AI_InitPlayer (player_t * player, edict_t * ent, equipDef_t * ed)
{
	const int team = player->pers.team;

	/* Set Actor state. */
	ent->type = ET_ACTOR;
	ent->pnum = player->num;

	/* Set the model and chose alien race. */
	AI_SetModelAndCharacterValues(ent, team);

	/* Calculate stats. */
	AI_SetStats(ent, team);

	/* Give equipment. */
	AI_SetEquipment(ent, team, ed);

	/* More tweaks */
	if (team != TEAM_CIVILIAN) {
		/** Set default reaction mode.
		 * @sa cl_team:CL_GenerateCharacter This function sets the initial default value for (newly created) non-AI actors.
		 * @todo Make the AI change the value (and its state) if needed while playing! */
		ent->chr.reservedTus.reserveReaction = STATE_REACTION_ONCE;

		/** Set initial state of reaction fire to previously stored state for this actor.
		 * @sa g_client.c:G_ClientSpawn */
		Com_DPrintf(DEBUG_GAME, "AI_InitPlayer: Setting default reaction-mode to %i (%s - %s).\n",ent->chr.reservedTus.reserveReaction, player->pers.netname, ent->chr.name);
		/* no need to call G_SendStats for the AI - reaction fire is serverside only for the AI */
		G_ClientStateChange(player, ent->number, ent->chr.reservedTus.reserveReaction, qfalse);
	}

	/* initialize the AI now */
	if (team == TEAM_CIVILIAN)
		AI_InitActor(ent, "civilian", "default");
	else if (team == TEAM_ALIEN)
		AI_InitActor(ent, "alien", "default");
	else
		Com_Printf("AI_InitPlayer: unknown team AI\n");

	/* link the new actor entity */
	gi.LinkEdict(ent);
}

#define MAX_SPAWNPOINTS		64
static int spawnPoints[MAX_SPAWNPOINTS];


/**
 * @brief Spawn civilians and aliens
 * @param[in] player
 * @param[in] numSpawn
 * @sa AI_CreatePlayer
 */
static void G_SpawnAIPlayer (player_t * player, int numSpawn)
{
	edict_t *ent;
	equipDef_t *ed = &gi.csi->eds[0];
	int i, j, numPoints, team;
	char name[MAX_VAR];

	/* search spawn points */
	team = player->pers.team;
	numPoints = 0;
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->type == ET_ACTORSPAWN && ent->team == team)
			spawnPoints[numPoints++] = i;

	/* check spawn point number */
	if (numPoints < numSpawn) {
		Com_Printf("Not enough spawn points for team %i\n", team);
		numSpawn = numPoints;
	}

	/* prepare equipment */
	if (team != TEAM_CIVILIAN) {
		Q_strncpyz(name, gi.Cvar_String("ai_equipment"), sizeof(name));
		for (i = 0, ed = gi.csi->eds; i < gi.csi->numEDs; i++, ed++)
			if (!Q_strncmp(name, ed->name, MAX_VAR))
				break;
		if (i == gi.csi->numEDs)
			ed = &gi.csi->eds[0];
	}

	/* spawn players */
	for (j = 0; j < numSpawn; j++) {
		assert(numPoints > 0);
		/* select spawnpoint */
		while (ent->type != ET_ACTORSPAWN)
			ent = &g_edicts[spawnPoints[rand() % numPoints]];

		/* spawn */
		level.num_spawned[team]++;
		level.num_alive[team]++;
		/* initialize the new actor */
		AI_InitPlayer(player, ent, ed);
	}
	/* show visible actors */
	G_ClearVisFlags(team);
	G_CheckVis(NULL, qfalse);

	/* give time */
	G_GiveTimeUnits(team);
}


/**
 * @brief Spawn civilians and aliens
 * @param[in] team
 * @sa G_SpawnAIPlayer
 * @return player_t pointer
 * @note see cvars ai_numaliens, ai_numcivilians, ai_numactors
 */
player_t *AI_CreatePlayer (int team)
{
	player_t *p;
	int i;

	if (!sv_ai->integer) {
		Com_Printf("AI deactivated - set sv_ai cvar to 1 to activate it\n");
		return NULL;
	}

	/* set players to ai players and cycle over all of them */
	for (i = 0, p = game.players + game.sv_maxplayersperteam; i < game.sv_maxplayersperteam; i++, p++)
		if (!p->inuse) {
			memset(p, 0, sizeof(*p));
			p->inuse = qtrue;
			p->num = p - game.players;
			p->pers.team = team;
			p->pers.ai = qtrue;
			if (team == TEAM_CIVILIAN)
				G_SpawnAIPlayer(p, ai_numcivilians->integer);
			else if (sv_maxclients->integer == 1)
				G_SpawnAIPlayer(p, ai_numaliens->integer);
			else
				G_SpawnAIPlayer(p, ai_numactors->integer);
			Com_Printf("Created AI player (team %i)\n", team);
			return p;
		}

	/* nothing free */
	return NULL;
}
