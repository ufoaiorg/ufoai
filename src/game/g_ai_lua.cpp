/**
 * @file
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
Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_move.h"
#include "g_vis.h"
extern "C" {
#include <lauxlib.h>
}

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
static const char* AIL_toTeamString (const int team)
{
	const char* teamStr = gi.GetConstVariable("luaaiteam", team);
	if (teamStr == nullptr)
		AIL_invalidparameter(1);
	return teamStr;
}

/**
 * @brief Converts team string into int representation
 * @param team The team to convert (alien, phalanx, civilian, ...)
 * @return The integer representation of the given team string
 * @sa AIL_Init
 */
static int AIL_toTeamInt (const char* team)
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
	Actor* actor; /**< Actual actor. */
} aiActor_t;


/*
 * Current AI Actor.
 */
static Actor* AIL_ent; /**< Actor currently running the Lua AI. */
static Player* AIL_player; /**< Player currently running the Lua AI. */


/*
 * Actor metatable.
 */
/* Internal functions. */
static int actorL_register(lua_State* L);
static int lua_isactor(lua_State* L, int index);
static aiActor_t* lua_toactor(lua_State* L, int index);
static aiActor_t* lua_pushactor(lua_State* L, aiActor_t* actor);
/* Metatable functions. */
static int actorL_tostring(lua_State* L);
static int actorL_pos(lua_State* L);
static int actorL_shoot(lua_State* L);
static int actorL_face(lua_State* L);
static int actorL_team(lua_State* L);
/** Lua Actor metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg actorL_methods[] = {
	{"__tostring", actorL_tostring},
	{"pos", actorL_pos},
	{"shoot", actorL_shoot},
	{"face", actorL_face},
	{"team", actorL_team},
	{nullptr, nullptr}
};


/**
 * pos3 metatable.
 */
/* Internal functions. */
static int pos3L_register(lua_State* L);
static int lua_ispos3(lua_State* L, int index);
static pos3_t* lua_topos3(lua_State* L, int index);
static pos3_t* lua_pushpos3(lua_State* L, pos3_t* pos);
/* Metatable functions. */
static int pos3L_tostring(lua_State* L);
static int pos3L_goto(lua_State* L);
static int pos3L_face(lua_State* L);
/** Lua Pos3 metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg pos3L_methods[] = {
	{"__tostring", pos3L_tostring},
	{"goto", pos3L_goto},
	{"face", pos3L_face},
	{nullptr, nullptr}
};


/**
 * General AI bindings.
 */
static int AIL_print(lua_State* L);
static int AIL_see(lua_State* L);
static int AIL_crouch(lua_State* L);
static int AIL_isinjured(lua_State* L);
static int AIL_TU(lua_State* L);
static int AIL_HP(lua_State* L);
static int AIL_morale(lua_State* L);
static int AIL_reactionfire(lua_State* L);
static int AIL_roundsleft(lua_State* L);
static int AIL_canreload(lua_State* L);
static int AIL_reload(lua_State* L);
static int AIL_positionshoot(lua_State* L);
static int AIL_positionhide(lua_State* L);
static int AIL_positionherd(lua_State* L);
static int AIL_distance(lua_State* L);
static int AIL_positionapproach(lua_State* L);
static int AIL_isarmed(lua_State* L);
static int AIL_getweapon(lua_State* L);

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
	{"positionapproach", AIL_positionapproach},
	{"isarmed", AIL_isarmed},
	{"getweapon", AIL_getweapon},
	{nullptr, nullptr}
};


/**
 *    A C T O R L
 */

/**
 * @brief Registers the actor metatable in the lua_State.
 * @param[in,out] L State to register the metatable in.
 * @return 0 on success.
 */
static int actorL_register (lua_State* L)
{
	/* Create the metatable */
	luaL_newmetatable(L, ACTOR_METATABLE);

	/* Create the access table */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	/* Register the values */
	luaL_register(L, nullptr, actorL_methods);

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
static int lua_isactor (lua_State* L, int index)
{
	if (lua_getmetatable(L, index) == 0)
		return 0;
	lua_getfield(L, LUA_REGISTRYINDEX, ACTOR_METATABLE);

	int ret = 0;
	if (lua_rawequal(L, -1, -2))  /* does it have the correct metatable? */
		ret = 1;

	lua_pop(L, 2);  /* remove both metatables */
	return ret;
}

/**
 * @brief Returns the actor from the metatable at index.
 */
static aiActor_t* lua_toactor (lua_State* L, int index)
{
	if (lua_isactor(L, index)) {
		return (aiActor_t*) lua_touserdata(L, index);
	}
	luaL_typerror(L, index, ACTOR_METATABLE);
	return nullptr;
}

/**
 * @brief Pushes a actor as a metatable at the top of the stack.
 */
static aiActor_t* lua_pushactor (lua_State* L, aiActor_t* actor)
{
	aiActor_t* a = (aiActor_t*) lua_newuserdata(L, sizeof(aiActor_t));
	*a = *actor;
	luaL_getmetatable(L, ACTOR_METATABLE);
	lua_setmetatable(L, -2);
	return a;
}

/**
 * @brief Pushes the actor as a string.
 */
static int actorL_tostring (lua_State* L)
{
	char buf[MAX_VAR];

	assert(lua_isactor(L, 1));

	const aiActor_t* target = lua_toactor(L, 1);
	Com_sprintf(buf, sizeof(buf), "Actor( %s )", target->actor->chr.name);

	lua_pushstring(L, buf);
	return 1;
}

/**
 * @brief Gets the actors position.
 */
static int actorL_pos (lua_State* L)
{
	assert(lua_isactor(L, 1));

	const aiActor_t* target = lua_toactor(L, 1);
	lua_pushpos3(L, &target->actor->pos);
	return 1;
}

/**
 * @brief Shoots the actor.
 */
static int actorL_shoot (lua_State* L)
{
	assert(lua_isactor(L, 1));

	/* Target */
	const aiActor_t* target = lua_toactor(L, 1);

	/* Number of TU to spend shooting, fire mode will adjust to that. */
	int tu = AIL_ent->getUsableTUs();
	if (lua_gettop(L) > 1) {
		assert(lua_isnumber(L, 2)); /* Must be a number. */

		tu = (int) lua_tonumber(L, 2);
	}

	shoot_types_t bestType = NONE;
	fireDefIndex_t bestFd = NONE;
	int bestShots = 0;
	float bestDmg = 0.0f;
	for (shoot_types_t shootType = ST_RIGHT; shootType < ST_NUM_SHOOT_TYPES; shootType++) {
		const Item* item = AI_GetItemForShootType(shootType, AIL_ent);
		if (item == nullptr)
			continue;

		const fireDef_t* fdArray = item->getFiredefs();
		if (fdArray == nullptr)
			continue;

		for (fireDefIndex_t fdIdx = 0; fdIdx < item->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
			const fireDef_t* fd = &fdArray[fdIdx];
			const int time = G_ActorGetModifiedTimeForFiredef(AIL_ent, fd, false);
			/* how many shoots can this actor do */
			const int shots = tu / time;

			if (!shots)
				continue;

			/* Check if we can do the most damage here */
			float dmg = AI_CalcShotDamage(AIL_ent, target->actor, fd, shootType) * shots;
			if (dmg > bestDmg) {
				bestDmg = dmg;
				bestShots = shots;
				bestFd = fdIdx;
				bestType = shootType;
			}
		}
	}

	/* Failure - no weapon. */
	if (bestType == NONE) {
		lua_pushboolean(L, 0);
		return 1;
	}

	while (bestShots > 0) {
		bestShots--;
		G_ClientShoot(*AIL_player, AIL_ent, target->actor->pos, bestType, bestFd, nullptr, true, 0);
		if (G_IsDead(target->actor))
			break;
	}

	/* Success. */
	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Makes the actor face the position.
 */
static int actorL_face (lua_State* L)
{
	assert(lua_isactor(L, 1));

	/* Target */
	const aiActor_t* target = lua_toactor(L, 1);

	AI_TurnIntoDirection(AIL_ent, target->actor->pos);

	/* Success. */
	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Gets the actor's team.
 */
static int actorL_team (lua_State* L)
{
	assert(lua_isactor(L, 1));

	const aiActor_t* target = lua_toactor(L, 1);
	assert(target != nullptr);
	const char* team = AIL_toTeamString(target->actor->getTeam());
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
static int pos3L_register (lua_State* L)
{
	/* Create the metatable */
	luaL_newmetatable(L, POS3_METATABLE);

	/* Create the access table */
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	/* Register the values */
	luaL_register(L, nullptr, pos3L_methods);

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
static int lua_ispos3 (lua_State* L, int index)
{
	if (lua_getmetatable(L, index) == 0)
		return 0;
	lua_getfield(L, LUA_REGISTRYINDEX, POS3_METATABLE);

	int ret = 0;
	if (lua_rawequal(L, -1, -2))  /* does it have the correct metatable? */
		ret = 1;

	lua_pop(L, 2);  /* remove both metatables */
	return ret;
}

/**
 * @brief Returns the pos3 from the metatable at index.
 */
static pos3_t* lua_topos3 (lua_State* L, int index)
{
	if (lua_ispos3(L, index)) {
		return (pos3_t*) lua_touserdata(L, index);
	}
	luaL_typerror(L, index, POS3_METATABLE);
	return nullptr;
}

/**
 * @brief Pushes a pos3 as a metatable at the top of the stack.
 */
static pos3_t* lua_pushpos3 (lua_State* L, pos3_t* pos)
{
	pos3_t* p = (pos3_t*) lua_newuserdata(L, sizeof(pos3_t));
	memcpy(p, pos, sizeof(*p));
	luaL_getmetatable(L, POS3_METATABLE);
	lua_setmetatable(L, -2);
	return p;
}

/**
 * @brief Puts the pos3 information in a string.
 */
static int pos3L_tostring (lua_State* L)
{
	char buf[MAX_VAR];

	assert(lua_ispos3(L, 1));

	const pos3_t* p = lua_topos3(L, 1);
	Com_sprintf(buf, sizeof(buf), "Pos3( x=%d, y=%d, z=%d )", (*p)[0], (*p)[1], (*p)[2]);

	lua_pushstring(L, buf);
	return 1;
}

/**
 * @brief Makes the actor head to the position.
 */
static int pos3L_goto (lua_State* L)
{
	assert(lua_ispos3(L, 1));

	/* Calculate move table. */
	G_MoveCalc(0, AIL_ent, AIL_ent->pos, AIL_ent->getUsableTUs());
	gi.MoveStore(level.pathingMap);

	/* Move. */
	const pos3_t* pos = lua_topos3(L, 1);
	G_ClientMove(*AIL_player, 0, AIL_ent, *pos);

	lua_pushboolean(L, 1);
	return 1;
}

/**
 * @brief Makes the actor face the position.
 */
static int pos3L_face (lua_State* L)
{
	assert(lua_ispos3(L, 1));

	const pos3_t* pos = lua_topos3(L, 1);
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
static int AIL_print (lua_State* L)
{
	const int n = lua_gettop(L);  /* number of arguments */

	for (int i = 1; i <= n; i++) {
		const char* s;
		bool meta = false;

		lua_pushvalue(L, i);   /* value to print */
		if (luaL_callmeta(L, 1, "__tostring")) {
			s = lua_tostring(L, -1);
			meta = true;
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
static int AIL_see (lua_State* L)
{
	/* Defaults. */
	int team = TEAM_ALL;
	int vision = 0;
	int sortCrit = 0;

	/* Handle parameters. */
	if ((lua_gettop(L) > 0)) {
		/* Get what to "see" with. */
		if (lua_isstring(L, 1)) {
			const char* s = lua_tostring(L, 1);
			/** @todo Get rid of magic numbers. */
			if (Q_streq(s, "all"))
				vision = 0;
			else if (Q_streq(s, "sight"))
				vision = 1;
			else if (Q_streq(s, "team"))
				vision = 2;
			/** @todo Properly implement at edict level */
			else if (Q_streq(s, "infrared"))
				vision = 3;
			else
				AIL_invalidparameter(1);
		} else
			AIL_invalidparameter(1);

		/* We now check for different teams. */
		if ((lua_gettop(L) > 1)) {
			if (lua_isstring(L, 2)) {
				const char* s = lua_tostring(L, 2);
				team = AIL_toTeamInt(s);
			} else
				AIL_invalidparameter(2);
		}

		/* Sorting criteria */
		if ((lua_gettop(L) > 2)) {
			if (lua_isstring(L, 3)) {
				const char* s = lua_tostring(L, 3);
				/** @todo Get rid of magic numbers. */
				if (Q_streq(s, "dist"))
					sortCrit = 0;
				else if (Q_streq(s, "path"))
					sortCrit = 1;
				else if (Q_streq(s, "HP"))
					sortCrit = 2;
				else
					AIL_invalidparameter(1);
			} else
				AIL_invalidparameter(3);
		}
	}

	int n = 0;
	Actor* check = nullptr;
	Actor* unsorted[MAX_EDICTS];
	float distLookup[MAX_EDICTS];
	/* Get visible things. */
	const int visDist = G_VisCheckDist(AIL_ent);
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (AIL_ent == check)
			continue;
		const float distance = VectorDistSqr(AIL_ent->pos, check->pos);
		if ((team == TEAM_ALL || check->getTeam() == team) && (vision == 0 /* Check for team match if needed. */
				|| (vision == 1 && G_Vis(AIL_ent->getTeam(), AIL_ent, check, VT_NOFRUSTUM))
				|| (vision == 2 && G_IsVisibleForTeam(check, AIL_ent->getTeam()))
				|| (vision == 3 && distance <= visDist * visDist))) {
			switch (sortCrit) {
			case 1:
			{
				pos_t move = ROUTING_NOT_REACHABLE;
				if (G_FindPath(0, AIL_ent, AIL_ent->pos, check->pos, false, 0xFE))
					move = gi.MoveLength(level.pathingMap, check->pos, 0, false);
				distLookup[n] = move;
			}
				break;
			case 2:
				distLookup[n] = check->HP;
				break;
			default:
				distLookup[n] = VectorDistSqr(AIL_ent->pos, check->pos);
				break;
			}
			unsorted[n++] = check;
		}
	}

	Actor* sorted[MAX_EDICTS];
	/* Sort by given criterion - lesser first. */
	for (int i = 0; i < n; i++) { /* Until we fill sorted */
		int cur = -1;
		for (int j = 0; j < n; j++) { /* Check for minimum */
			/* Is shorter then current minimum? */
			if (cur < 0 || distLookup[j] < distLookup[cur]) {
				int k;
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
	for (int i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		aiActor_t target;
		target.actor = sorted[i];
		lua_pushactor(L, &target); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of actors. */
}

/**
 * @brief Requests a crouch state (with true/false) and returns current crouch state.
 */
static int AIL_crouch (lua_State* L)
{
	if (lua_gettop(L) > 0) {
		if (lua_isboolean(L, 1)) {
			const bool reqState = lua_toboolean(L, 1);
			const bool state = AIL_ent->isCrouched();
			if (reqState != state)
				G_ClientStateChange(*AIL_player, AIL_ent, STATE_CROUCHED, false);
		} else
			AIL_invalidparameter(1);
	}

	lua_pushboolean(L, AIL_ent->isCrouched());
	return 1;
}

/**
* @brief Checks to see if the actor is injured
*/
static int AIL_isinjured (lua_State* L)
{
	lua_pushboolean(L, AIL_ent->HP != AIL_ent->chr.maxHP);
	return 1;
}

/**
 * @brief Gets the number of TU the actor has left.
 */
static int AIL_TU (lua_State* L)
{
	lua_pushnumber(L, AIL_ent->getTus());
	return 1;
}

/**
 * @brief Gets the number of HP the actor has left.
 */
static int AIL_HP (lua_State* L)
{
	lua_pushnumber(L, AIL_ent->HP);
	return 1;
}

/**
 * @brief Gets the current morale of the actor onto the stack.
 */
static int AIL_morale (lua_State* L)
{
	lua_pushnumber(L, AIL_ent->getMorale());
	return 1;
}

/**
 * @brief Sets the actor's reaction fire mode.
 */
static int AIL_reactionfire (lua_State* L)
{
	if (lua_gettop(L) > 0) {
		int reactionState = 0;

		if (lua_isstring(L, 1)) {
			/* get reaction fire mode */
			const char* cmd = lua_tostring(L, 1);
			reactionState = Q_streq(cmd, "disable") ? ~STATE_REACTION : STATE_REACTION;
		}

		if (reactionState && lua_gettop(L) > 1 && lua_isboolean(L, 2)) {
			const int state = lua_toboolean(L, 2);
			G_ClientStateChange(*AIL_player, AIL_ent, reactionState,
				(state) ? true : false);
		} else {
			AIL_invalidparameter(reactionState ? 2 : 1);
		}
	}

	lua_pushboolean(L, AIL_ent->isReaction());
	return 1;
}

/**
 * @brief Checks to see how many rounds the actor has left.
 */
static int AIL_roundsleft (lua_State* L)
{
	/* Right hand */
	const Item* rightHand = AIL_ent->getRightHandItem();
	if (rightHand && (rightHand->def()->ammo < 1 || rightHand->getAmmoLeft() > 0))
		lua_pushnumber(L, rightHand->getAmmoLeft());
	else
		/* Currently unusable */
		lua_pushnil(L);

	/* Left hand */
	const Item* leftHand = AIL_ent->getLeftHandItem();
	if (leftHand && (leftHand->def()->ammo < 1 || leftHand->getAmmoLeft() > 0))
		lua_pushnumber(L, leftHand->getAmmoLeft());
	else
		lua_pushnil(L);
	return 2;
}

/**
 * @brief Checks to see if the actor can reload.
 */
static int AIL_canreload (lua_State* L)
{
	lua_pushboolean(L, G_ClientCanReload(AIL_ent, CID_RIGHT));
	lua_pushboolean(L, G_ClientCanReload(AIL_ent, CID_LEFT));
	return 2;
}

/**
 * @brief Actor reloads his weapons.
 */
static int AIL_reload (lua_State* L)
{
	containerIndex_t container = CID_RIGHT; /* Default to right hand. */

	if (lua_gettop(L) > 0) {
		if (lua_isstring(L, 1)) {
			const char* s = lua_tostring(L, 1);

			if (Q_streq(s, "right")) {
				container = CID_RIGHT;
			} else if (Q_streq(s, "left")) {
				container = CID_LEFT;
			} else {
				AIL_invalidparameter(1);
				return 0;
			}
		} else {
			AIL_invalidparameter(1);
			return 0;
		}
	}

	AI_TryToReloadWeapon(AIL_ent, container);
	return 0;
}

/**
 * @brief Check if actor has weapons
 */
static int AIL_isarmed (lua_State* L)
{
	lua_pushboolean(L, AIL_ent->getRightHandItem() ? 1 : 0);
	lua_pushboolean(L, AIL_ent->getLeftHandItem() ? 1 : 0);
	return 2;
}

/**
 * @brief Actor tries to grab a weapon from inventory
 */
static int AIL_getweapon (lua_State* L)
{
	lua_pushboolean(L, G_ClientGetWeaponFromInventory(AIL_ent));
	return 1;
}

/**
 * @brief Moves the actor into a position in which he can shoot his target.
 */
static int AIL_positionshoot (lua_State* L)
{
	/* We need a target. */
	assert(lua_isactor(L, 1));
	aiActor_t* target = lua_toactor(L, 1);

	/* Make things more simple. */
	Actor* actor = AIL_ent;

	/* Shooting strategy */
	int posType = 0;
	if ((lua_gettop(L) > 1)) {
		if (lua_isstring(L, 2)) {
			const char* s = lua_tostring(L, 2);
			/** @todo Get rid of magic numbers. */
			if (Q_streq(s, "fastest"))
				posType = 0;
			else if (Q_streq(s, "nearest"))
				posType = 1;
			else if (Q_streq(s, "farthest"))
				posType = 2;
			else
				AIL_invalidparameter(1);
		} else
			AIL_invalidparameter(3);
	}

	/* Number of TU to spend shooting, to make sure we have enough tus to actually fire. */
	int tus = actor->getUsableTUs();
	if (lua_gettop(L) > 2) {
		assert(lua_isnumber(L, 3)); /* Must be a number. */

		tus = static_cast<int>(lua_tonumber(L, 3));
	}

	shoot_types_t shootType = ST_RIGHT;
	const Item* item = AI_GetItemForShootType(shootType, AIL_ent);
	if (item == nullptr) {
		shootType = ST_LEFT;
		item = AI_GetItemForShootType(shootType, AIL_ent);
	}

	/* Check for weapon. */
	if (item == nullptr) {
		lua_pushboolean(L, 0);
		return 1;
	}
	const fireDef_t* fd = item->getFastestFireDef();
	if (fd == nullptr) {
		lua_pushboolean(L, 0);
		return 1;
	}

	tus -= fd->time;
	if (tus <= 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Calculate move table. */
	G_MoveCalc(0, actor, actor->pos, tus);
	gi.MoveStore(level.pathingMap);

	/* set borders */
	const int dist = (tus + 1) / TU_MOVE_STRAIGHT;
	const int xl = std::max(actor->pos[0] - dist, 0);
	const int yl = std::max(actor->pos[1] - dist, 0);
	const int xh = std::min(actor->pos[0] + dist, PATHFINDING_WIDTH);
	const int yh = std::min(actor->pos[1] + dist, PATHFINDING_WIDTH);

	pos3_t oldPos;
	vec3_t oldOrigin;
	VectorCopy(actor->pos, oldPos);
	VectorCopy(actor->origin, oldOrigin);

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	float bestScore = -12000.0f;
	pos3_t to, bestPos;
	VectorSet(bestPos,0, 0, PATHFINDING_HEIGHT);
	for (to[2] = 0; to[2] < PATHFINDING_HEIGHT; to[2]++) {
		for (to[1] = yl; to[1] < yh; to[1]++) {
			for (to[0] = xl; to[0] < xh; to[0]++) {
				actor->setOrigin(to);
				const pos_t tu = G_ActorMoveLength(actor, level.pathingMap, to, true);
				if (tu > tus || tu == ROUTING_NOT_REACHABLE)
					continue;
				/* Can we see the target? */
				if (!G_IsVisibleForTeam(target->actor, actor->getTeam()) && G_ActorVis(actor->origin, actor, target->actor, true) < ACTOR_VIS_10)
					continue;
				float dist;
				if (!AI_FighterCheckShoot(actor, target->actor, fd, &dist))
					continue;
				/* gun-to-target line free? */
				if (!AI_CheckLineOfFire(actor, target->actor, fd, 1))
					continue;
				float score;
				switch (posType) {
				case 1:
					score = -dist;
					break;
				case 2:
					score = dist;
					break;
				default:
					score = -tu;
					break;
				}
				if (score > bestScore) {
					VectorCopy(to, bestPos);
					bestScore = score;
				}
			}
		}
	}

	VectorCopy(oldPos, actor->pos);
	VectorCopy(oldOrigin, actor->origin);

	/* No position found in range. */
	if (bestPos[2] >= PATHFINDING_HEIGHT) {
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
static int AIL_positionhide (lua_State* L)
{
	int hidingTeam = AI_GetHidingTeam(AIL_ent);

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

	const int tus = AIL_ent->getUsableTUs();
	pos3_t save;
	VectorCopy(AIL_ent->pos, save);

	if (AI_FindHidingLocation(hidingTeam, AIL_ent, AIL_ent->pos, tus)) {
		/* Return the spot. */
		lua_pushpos3(L, &AIL_ent->pos);
	} else {
		lua_pushboolean(L, 0);
	}
	AIL_ent->setOrigin(save);
	return 1;
}

/**
 * @brief Determine the position where actor is more closer to the target and
 * locate behind the target from enemy
 * @note @c target (parameter is passed through the lua stack) The actor
 * to which AI tries to become closer
 */
static int AIL_positionherd (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}

	pos3_t save;
	VectorCopy(AIL_ent->pos, save);
	const aiActor_t* target = lua_toactor(L, 1);
	if (AI_FindHerdLocation(AIL_ent, AIL_ent->pos, target->actor->origin, AIL_ent->getUsableTUs())) {
		lua_pushpos3(L, &AIL_ent->pos);
	} else {
		lua_pushboolean(L, 0);
	}
	AIL_ent->setOrigin(save);
	return 1;
}

/**
 * @brief Returns distance between AI and target
 * @note @c target (passed trough the lua stack) The target to which the distance is calculated
 */
static int AIL_distance (lua_State* L)
{

	/* check parameter */
	assert(lua_gettop(L) && lua_isactor(L, 1));

	/* calculate distance */
	const aiActor_t* target = lua_toactor(L, 1);
	const vec_t dist = VectorDist(AIL_ent->origin, target->actor->origin);
	lua_pushnumber(L, dist);
	return 1;
}

/**
 * @brief Approach to a target actor
 */
static int AIL_positionapproach (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}

	const aiActor_t* target = lua_toactor(L, 1);
	assert(target != nullptr);

	/* Find a path to the target actor */
	const int maxTUs = ROUTING_NOT_REACHABLE - 1;
	pos3_t to;
	VectorCopy(target->actor->pos, to);
	byte crouchingState = AIL_ent->isCrouched() ? 1 : 0;
	if (!G_FindPath(0, AIL_ent, AIL_ent->pos, to, crouchingState, maxTUs)) {
		/* Not found */
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Find the farthest we can go with current TUs */
	const int tus = AIL_ent->getUsableTUs();
	int dvec;
	while ((dvec = gi.MoveNext(level.pathingMap, to, crouchingState)) != ROUTING_UNREACHABLE) {
		/* Note: here we skip the first position so we don't try to walk into the target */
		PosSubDV(to, crouchingState, dvec);
		const byte length2 =  G_ActorMoveLength(target->actor, level.pathingMap, to, false);
		if (length2 <= tus)
			break;
		/* We are going backwards to the origin. */
	}

	lua_pushpos3(L, &to);
	return 1;
}

/**
 * @brief The think function for the ai controlled aliens
 * @param[in] player
 * @param[in] actor
 */
void AIL_ActorThink (Player& player, Actor* actor)
{
	/* The Lua State we will work with. */
	lua_State* L = actor->AI.L;

	/* Set the global player and edict */
	AIL_ent = actor;
	AIL_player = &player;

	/* Try to run the function. */
	lua_getglobal(L, "think");
	if (lua_pcall(L, 0, 0, 0)) { /* error has occured */
		gi.DPrintf("Error while running Lua: %s\n",
			lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unknown Error");
	}

	/* Cleanup */
	AIL_ent = nullptr;
	AIL_player = nullptr;
}


/**
 * @brief Initializes the AI.
 * @param[in] ent Pointer to actor to initialize AI for.
 * @param[in] type Type of AI (Lua file name without .lua).
 * @param[in] subtype Subtype of the AI.
 * @return 0 on success.
 */
int AIL_InitActor (Edict* ent, const char* type, const char* subtype)
{
	/* Prepare the AI */
	AI_t* AI = &ent->AI;
	Q_strncpyz(AI->type, type, sizeof(AI->type));
	Q_strncpyz(AI->subtype, subtype, sizeof(AI->subtype));

	/* Create the new Lua state */
	AI->L = luaL_newstate();
	if (AI->L == nullptr) {
		gi.DPrintf("Unable to create Lua state.\n");
		return -1;
	}

	/* Register metatables. */
	actorL_register(AI->L);
	pos3L_register(AI->L);

	/* Register libraries. */
	luaL_register(AI->L, AI_METATABLE, AIL_methods);

	/* Load the AI */
	char path[MAX_VAR];
	Com_sprintf(path, sizeof(path), "ai/%s.lua", type);
	char* fbuf;
	const int size = gi.FS_LoadFile(path, (byte**) &fbuf);
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
static void AIL_CleanupActor (Edict* ent)
{
	AI_t* AI = &ent->AI;

	/* Cleanup. */
	if (AI->L != nullptr) {
		lua_close(AI->L);
		AI->L = nullptr;
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
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextActor(actor)))
		AIL_CleanupActor(actor);
}
