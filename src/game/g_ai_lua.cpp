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
#include "g_ai.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_move.h"
#include "g_utils.h"
#include "g_vis.h"
extern "C" {
#include <lauxlib.h>
}

#define POS3_METATABLE	"pos3" /**< Pos3 Lua Metatable name. */
#define ACTOR_METATABLE	"actor" /**< Actor Lua Metable name. */
#define AI_METATABLE	"ai" /**< AI Lua Metable name. */

static lua_State* ailState;
/**
 * Provides an api like luaL_dostring for buffers.
 */
#define luaL_dobuffer(L, b, n, s) \
	(luaL_loadbuffer(L, b, n, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define AIL_invalidparameter(n)	\
	gi.DPrintf("AIL: Invalid parameter #%d in '%s'.\n", n, __func__)

/** @brief vis check types */
typedef enum {
	AILVT_ALL,		/* Don't do vis checks (god's view) */
	AILVT_SIGHT,	/* Standard vis check */
	AILVT_TEAM,		/* Team vis check */
	AILVT_DIST		/* Check only vis distance */
} ailVisType_t;

/** @brief target sorting criteria (lowest first) */
typedef enum {
	AILSC_DIST,		/* Sort by line distance */
	AILSC_PATH,		/* Sort by pathing cost */
	AILSC_HP		/* Sort by HP */
} ailSortCritType_t;

/** @brief Shooting position types */
typedef enum {
	AILSP_FAST,		/* Fastest to get to */
	AILSP_NEAR,		/* Nearest to target */
	AILSP_FAR		/* Farthest from target (within weapon's range) */
} ailShootPosType_t;

typedef enum {
	AILPW_RAND,		/* Wander randomly */
	AILPW_CW,		/* Move clockwise */
	AILPW_CCW		/* Move counter-clockwise */
} ailWanderPosType;
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
static int AIL_toTeamInt (const char* team, const int param)
{
	int teamInt = TEAM_DEFAULT;
	if (!gi.GetConstIntFromNamespace("luaaiteam", team, &teamInt))
		AIL_invalidparameter(param);
	return teamInt;
}

/**
 * @brief Return visibility mode int representation from the string representation in the lua stack
 * @param L The lua state to use
 * @param index Index on the lua stack where the string representation is stored
 * @return The integer representation of the visibility mode
 */
static ailVisType_t AIL_toVisInt (lua_State* L, const int index)
{
	int visInt = AILVT_ALL;
	if (lua_isstring(L, index)) {
		const char* s = lua_tostring(L, index);
		if (!gi.GetConstIntFromNamespace("luaaivis", s, &visInt))
			AIL_invalidparameter(index);
	} else
		AIL_invalidparameter(index);
	return static_cast<ailVisType_t> (visInt);
}

/**
 * @brief Return sort type int representation from the string representation in the lua stack
 * @param L The lua state to use
 * @param index Index on the lua stack where the string representation is stored
 * @return The integer representation of the sort type
 */
static ailSortCritType_t AIL_toSortInt (lua_State* L, const int index)
{
	int sortInt = AILSC_DIST;
	if (lua_isstring(L, index)) {
		const char* s = lua_tostring(L, index);
		if (!gi.GetConstIntFromNamespace("luaaisort", s, &sortInt))
			AIL_invalidparameter(index);
	} else
		AIL_invalidparameter(index);
	return static_cast<ailSortCritType_t> (sortInt);
}

/**
 * @brief Return distance type int representation from the string representation in the lua stack
 * @param L The lua state to use
 * @param index Index on the lua stack where the string representation is stored
 * @return The integer representation of the distance type
 */
static ailSortCritType_t AIL_toDistInt (lua_State* L, const int index)
{
	int distInt = AILSC_DIST;
	if (lua_isstring(L, index)) {
		const char* s = lua_tostring(L, index);
		if (!gi.GetConstIntFromNamespace("luaaidist", s, &distInt))
			AIL_invalidparameter(index);
	} else
		AIL_invalidparameter(index);
	return static_cast<ailSortCritType_t> (distInt);
}

/**
 * @brief Return shooting position type int representation from the string representation in the lua stack
 * @param L The lua state to use
 * @param index Index on the lua stack where the string representation is stored
 * @return The integer representation of the shooting position type
 */
static ailShootPosType_t AIL_toShotPInt (lua_State* L, const int index)
{
	int spInt = AILSP_FAST;
	if (lua_isstring(L, index)) {
		const char* s = lua_tostring(L, index);
		if (!gi.GetConstIntFromNamespace("luaaishot", s, &spInt))
			AIL_invalidparameter(index);
	} else
		AIL_invalidparameter(index);
	return static_cast<ailShootPosType_t> (spInt);
}

/**
 * @brief Return wander position type int representation from the string representation in the lua stack
 * @param L The lua state to use
 * @param index Index on the lua stack where the string representation is stored
 * @return The integer representation of the wander position type
 */
static ailWanderPosType AIL_toWanderPInt (lua_State* L, const int index)
{
	int wpInt = AILPW_RAND;
	if (lua_isstring(L, index)) {
		const char* s = lua_tostring(L, index);
		if (!gi.GetConstIntFromNamespace("luaaiwander", s, &wpInt))
			AIL_invalidparameter(index);
	} else
		AIL_invalidparameter(index);
	return static_cast<ailWanderPosType> (wpInt);
}

/**
 * @brief Wrapper around edict.
 */
typedef struct aiActor_s {
	Actor* actor; /**< Actual actor. */
} aiActor_t;


/* Table sorting */
template<typename T>
struct AilSortTable {
	T data;
	float sortLookup;
};

template<typename T>
bool operator< (AilSortTable<T> i, AilSortTable<T> j) {
		return (i.sortLookup < j.sortLookup);
}

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
static int actorL_team(lua_State* L);
static int actorL_throwgrenade(lua_State* L);
static int actorL_TU(lua_State* L);
static int actorL_HP(lua_State* L);
static int actorL_morale(lua_State* L);
static int actorL_isinjured(lua_State* L);
static int actorL_isarmed(lua_State* L);
static int actorL_isdead(lua_State* L);
static int actorL_isvalidtarget(lua_State* L);
/** Lua Actor metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg actorL_methods[] = {
	{"__tostring", actorL_tostring},
	{"pos", actorL_pos},
	{"shoot", actorL_shoot},
	{"team", actorL_team},
	{"throwgrenade", actorL_throwgrenade},
	{"TU", actorL_TU},
	{"HP", actorL_HP},
	{"morale", actorL_morale},
	{"isinjured", actorL_isinjured},
	{"isarmed", actorL_isarmed},
	{"isdead", actorL_isdead},
	{"isvalidtarget", actorL_isvalidtarget},
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
static int pos3L_distance(lua_State* L);
/** Lua Pos3 metatable methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg pos3L_methods[] = {
	{"__tostring", pos3L_tostring},
	{"goto", pos3L_goto},
	{"face", pos3L_face},
	{"distance", pos3L_distance},
	{nullptr, nullptr}
};


/**
 * General AI bindings.
 */
static int AIL_print(lua_State* L);
static int AIL_squad(lua_State* L);
static int AIL_select(lua_State* L);
static int AIL_see(lua_State* L);
static int AIL_crouch(lua_State* L);
static int AIL_reactionfire(lua_State* L);
static int AIL_roundsleft(lua_State* L);
static int AIL_canreload(lua_State* L);
static int AIL_reload(lua_State* L);
static int AIL_positionshoot(lua_State* L);
static int AIL_positionhide(lua_State* L);
static int AIL_positionherd(lua_State* L);
static int AIL_positionapproach(lua_State* L);
static int AIL_grabweapon(lua_State* L);
static int AIL_missiontargets(lua_State* L);
static int AIL_waypoints(lua_State* L);
static int AIL_positionmission(lua_State* L);
static int AIL_positionwander(lua_State* L);
static int AIL_findweapons(lua_State* L);
static int AIL_isfighter(lua_State* L);
static int AIL_setwaypoint(lua_State* L);
static int AIL_difficulty(lua_State* L);
static int AIL_positionflee(lua_State* L);
static int AIL_weapontype(lua_State* L);
static int AIL_actor(lua_State* L);
static int AIL_tusforshooting(lua_State* L);
static int AIL_class(lua_State* L);
static int AIL_hideneeded(lua_State* L);

/** Lua AI module methods.
 * http://www.lua.org/manual/5.1/manual.html#lua_CFunction
 */
static const luaL_reg AIL_methods[] = {
	{"print", AIL_print},
	{"squad", AIL_squad},
	{"select", AIL_select},
	{"see", AIL_see},
	{"crouch", AIL_crouch},
	{"reactionfire", AIL_reactionfire},
	{"roundsleft", AIL_roundsleft},
	{"canreload", AIL_canreload},
	{"reload", AIL_reload},
	{"positionshoot", AIL_positionshoot},
	{"positionhide", AIL_positionhide},
	{"positionherd", AIL_positionherd},
	{"positionapproach", AIL_positionapproach},
	{"grabweapon", AIL_grabweapon},
	{"missiontargets", AIL_missiontargets},
	{"waypoints", AIL_waypoints},
	{"positionmission", AIL_positionmission},
	{"positionwander", AIL_positionwander},
	{"findweapons", AIL_findweapons},
	{"isfighter", AIL_isfighter},
	{"setwaypoint", AIL_setwaypoint},
	{"difficulty", AIL_difficulty},
	{"positionflee", AIL_positionflee},
	{"weapontype", AIL_weapontype},
	{"actor", AIL_actor},
	{"tusforshooting", AIL_tusforshooting},
	{"class", AIL_class},
	{"hideneeded", AIL_hideneeded},
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

		tu = std::min(static_cast<int>(lua_tonumber(L, 2)), tu);
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

			float dist;
			if (!AI_FighterCheckShoot(AIL_ent, target->actor, fd, &dist))
				continue;

			if (!AI_CheckLineOfFire(AIL_ent, target->actor, fd, shots))
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

	bool shot = false;
	while (bestShots > 0) {
		if (G_IsDead(target->actor))
			break;
		bestShots--;
		shot = G_ClientShoot(*AIL_player, AIL_ent, target->actor->pos, bestType, bestFd, nullptr, true, 0) || shot;
	}

	/* Success? */
	lua_pushboolean(L, shot);
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
 * @brief Throws a grenade to the actor.
 */
static int actorL_throwgrenade(lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* target = lua_toactor(L, 1);
	assert(target != nullptr);

	/* Min number of enemies to use grenade */
	int minNum = 0;
	if (lua_gettop(L) > 1) {
		if (!lua_isnumber(L, 2)) { /* Must be a number. */
			AIL_invalidparameter(2);
			lua_pushboolean(L, 0);
			return 1;
		}
		minNum = static_cast<int>(lua_tonumber(L, 2));
	}

	/* Number of TU to spend */
	int tus = AIL_ent->getUsableTUs();
	if (lua_gettop(L) > 2) {
		if (!lua_isnumber(L, 3)) { /* Must be a number. */
			AIL_invalidparameter(3);
			lua_pushboolean(L, 0);
			return 1;
		}
		tus = std::min(static_cast<int>(lua_tonumber(L, 3)), tus);
	}

	/* Check that we have a free hand */
	containerIndex_t hand = CID_RIGHT;
	const Item* right = AIL_ent->getRightHandItem();
	if (right)
		hand = right->isHeldTwoHanded() || AIL_ent->getLeftHandItem() ? CID_MAX : CID_LEFT;
	if (hand >= CID_MAX) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Check if we have a grenade */
	Item* grenade = nullptr;
	const invDef_t* fromCont = AI_SearchGrenade(AIL_ent, &grenade);
	if (!fromCont || !grenade) {
		lua_pushboolean(L, 0);
		return 1;
	}
	/* Now check if we can use it */
	const fireDef_t* fdArray = grenade->getFiredefs();
	const int invMoveCost = fromCont->out + INVDEF(hand)->in;
	const shoot_types_t shotType = hand == CID_RIGHT ? ST_RIGHT : ST_LEFT;
	float dist = 0.0f;
	const fireDef_t* bestFd = nullptr;
	for (fireDefIndex_t fdIdx = 0; fdIdx < grenade->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
		const fireDef_t* fd = &fdArray[fdIdx];
		const int time = invMoveCost + G_ActorGetModifiedTimeForFiredef(AIL_ent, fd, false);
		/* Enough TU? */
		if (time > tus)
			continue;
		/* In range? */
		if (!AI_FighterCheckShoot(AIL_ent, target->actor, fd, &dist))
			continue;
		/* LOF? */
		if (!AI_CheckLineOfFire(AIL_ent, target->actor, fd, 1))
			continue;

		/* Select the first usable firemode */
		bestFd = fd;
		break;
	}
	if (!bestFd) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Finally check if we want to use it now */
	if (bestFd->splrad > 0) {
		Actor* check = nullptr;
		int n = 0;
		while ((check = G_EdictsGetNextLivingActor(check))) {
			/* check for distance */
			dist = VectorDist(target->actor->origin, check->origin);
			dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0;
			if (dist > bestFd->splrad)
				continue;

			if (!AI_IsHostile(AIL_ent, target->actor)) {
				lua_pushboolean(L, 0);
				return 1;
			}
			++n;
		}
		/* Check there's large enough group of targets */
		if (n < minNum) {
			lua_pushboolean(L, 0);
			return 1;
		}
	}

	/* Try to move the grenade to the free hand */
	if(!G_ActorInvMove(AIL_ent, fromCont, grenade, INVDEF(hand), NONE, NONE, true)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	/* All right use it! */
	const bool result = G_ClientShoot(*AIL_player, AIL_ent, target->actor->pos, shotType, bestFd->fdIdx, nullptr, true, 0);

	lua_pushboolean(L, result);
	return 1;
}

/**
 * @brief Gets the number of usable TU the actor has left.
 */
static int actorL_TU (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	lua_pushnumber(L, actor->actor->getUsableTUs());
	return 1;
}

/**
 * @brief Gets the number of HP the actor has left.
 */
static int actorL_HP (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	lua_pushnumber(L, actor->actor->HP);
	return 1;
}

/**
 * @brief Gets the current morale of the actor onto the stack.
 */
static int actorL_morale (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	const char* morStat = "normal";
	if (actor->actor->isPanicked())
		morStat = "panic";
	else if (actor->actor->isInsane())
		morStat = "insane";
	else if (actor->actor->isRaged())
		morStat = "rage";
	else if (actor->actor->getMorale() <= mor_brave->integer)
		morStat = "cower";

	lua_pushstring(L, morStat);
	return 1;
}

/**
* @brief Checks to see if the actor is seriously injured
*/
static int actorL_isinjured (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	lua_pushboolean(L, G_IsActorWounded(actor->actor, true)
			|| actor->actor->HP <= actor->actor->chr.maxHP * 0.5);
	return 1;
}

/**
 * @brief Check if actor has weapons
 */
static int actorL_isarmed (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	lua_pushboolean(L, actor->actor->getRightHandItem() ? 1 : 0);
	lua_pushboolean(L, actor->actor->getLeftHandItem() ? 1 : 0);
	return 2;
}

/**
 * @brief Check if the actor is dead.
 */
static int actorL_isdead (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* actor = lua_toactor(L, 1);
	assert(actor != nullptr);

	lua_pushboolean(L, actor->actor->isDead());
	return 1;
}

/**
 * @brief Check if the actor is a valid target to attack.
 */
static int actorL_isvalidtarget (lua_State* L)
{
	assert(lua_isactor(L, 1));

	const aiActor_t* target = lua_toactor(L, 1);
	assert(target != nullptr);
	lua_pushboolean(L, AI_IsHostile(AIL_ent, target->actor));
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
	/* do the move */
	for (;;) {
		if (AIL_ent->isDead())
			break;
		G_ClientMove(*AIL_player, 0, AIL_ent, *pos);
		if (AIL_ent->isSamePosAs(*pos))
			break;
		const pos_t length = G_ActorMoveLength(AIL_ent, level.pathingMap, *pos, false);
		if (length > AIL_ent->getUsableTUs() || length >= ROUTING_NOT_REACHABLE)
			break;
	}

	lua_pushboolean(L, AIL_ent->isSamePosAs(*pos));
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
 * @brief Return the distance the position an the AI actor
 */
static int pos3L_distance (lua_State* L)
{
	assert(lua_ispos3(L, 1));

	pos3_t* pos = lua_topos3(L, 1);
	assert(pos != nullptr);

	ailSortCritType_t distType = AILSC_DIST;
	if (lua_gettop(L) > 1)
		distType = AIL_toDistInt(L, 2);

	switch (distType) {
	case AILSC_PATH:
		/* Find a path to the target pos */
		if (!G_FindPath(0, AIL_ent, AIL_ent->pos, *pos, AIL_ent->isCrouched(), ROUTING_NOT_REACHABLE - 1)) {
			lua_pushnumber(L, ROUTING_NOT_REACHABLE);
			return 1;
		}
		lua_pushnumber(L, G_ActorMoveLength(AIL_ent, level.pathingMap, *pos, false));
		return 1;
	case AILSC_DIST:
	default:
		vec3_t to;
		PosToVec(*pos, to);
		lua_pushnumber(L, VectorDist(AIL_ent->origin, to));
		return 1;
	}
}

/**
 *    A I L
 */
/*
 * General functions.
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
		if (luaL_callmeta(L, -1, "__tostring")) {
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

/*
 * Player functions.
 */

/**
 * @brief Returns a table with the actors in the current player's team
 */
static int AIL_squad (lua_State* L)
{
	if (g_ailua->integer < 2) {
		gi.DPrintf("Problem while running lua: attempt to get the player's team while not in team mode.");
		lua_pushnil(L);
		return 1;
	}

	/* New Lua table. */
	lua_newtable(L);

	int i = 1; /* LUA indexes starting from one */
	Actor* check = nullptr;
	while ((check = G_EdictsGetNextActor(check))) {
		if (check->getPlayerNum() != AIL_player->getNum())
			continue;
		lua_pushnumber(L, i++); /* index */
		aiActor_t target;
		target.actor = check;
		lua_pushactor(L, &target); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of actors. */
}

/**
 * @brief Select an specific AI actor.
 */
static int AIL_select (lua_State* L)
{
	if (g_ailua->integer < 2) {
		gi.DPrintf("Problem while running lua: attempt to select the active AI actor while not in team mode.");
		lua_pushnil(L);
	} else if (lua_gettop(L) > 0 && lua_isactor(L, 1)) {
		aiActor_t* target = lua_toactor(L, 1);
		if (target->actor->getPlayerNum() == AIL_player->getNum())
			AIL_ent = target->actor;
		lua_pushboolean(L, AIL_ent == target->actor);
	} else {
		AIL_invalidparameter(1);
		lua_pushboolean(L, false);
	}
	return 1;
}

/*
 * Actor functions
 */

/**
 * @brief Returns what the actor can see.
 */
static int AIL_see (lua_State* L)
{
	/* Defaults. */
	int team = TEAM_ALL;
	ailVisType_t vision = AILVT_ALL;
	ailSortCritType_t sortCrit = AILSC_DIST;
	bool invTeam = false;

	/* Handle parameters. */
	if ((lua_gettop(L) > 0)) {
		/* Get what to "see" with. */
		vision = AIL_toVisInt(L, 1);

		/* We now check for different teams. */
		if ((lua_gettop(L) > 1)) {
			if (lua_isstring(L, 2)) {
				const char* s = lua_tostring(L, 2);
				if (s[0] == '-' || s[0] == '~') {
					invTeam = true;
					++s;
				}
				team = AIL_toTeamInt(s, 2);
				/* Trying to see no one? */
				if (team == TEAM_ALL && invTeam)
					AIL_invalidparameter(2);
			} else
				AIL_invalidparameter(2);
		}

		/* Sorting criteria */
		if ((lua_gettop(L) > 2)) {
			sortCrit = AIL_toSortInt(L, 3);
		}
	}

	int n = 0;
	Actor* check = nullptr;
	AilSortTable<Actor*> sortTable[MAX_EDICTS];
	/* Get visible things. */
	const int visDist = G_VisCheckDist(AIL_ent);
	/* We are about to check the team view, update it accordingly */
	if (vision == AILVT_TEAM)
		G_CheckVisTeamAll(AIL_ent->getTeam(), VT_NOFRUSTUM, AIL_ent);
	while ((check = G_EdictsGetNextLivingActor(check))) {
		if (AIL_ent == check)
			continue;
		const float distance = VectorDistSqr(AIL_ent->pos, check->pos);
		/* Check for team match if needed. */
		if ((team == TEAM_ALL || (check->getTeam() == team ? !invTeam : invTeam))
				&& (vision == AILVT_ALL
				|| (vision == AILVT_SIGHT && G_Vis(AIL_ent->getTeam(), AIL_ent, check, VT_NOFRUSTUM))
				|| (vision == AILVT_TEAM && G_IsVisibleForTeam(check, AIL_ent->getTeam()))
				|| (vision == AILVT_DIST && distance <= visDist * visDist))) {
			switch (sortCrit) {
			case AILSC_PATH:
			{
				pos_t move = ROUTING_NOT_REACHABLE;
				if (G_FindPath(0, AIL_ent, AIL_ent->pos, check->pos, false, 0xFE))
					move = gi.MoveLength(level.pathingMap, check->pos, 0, false);
				sortTable[n].sortLookup = move;
			}
				break;
			case AILSC_HP:
				sortTable[n].sortLookup = check->HP;
				break;
			case AILSC_DIST:
			default:
				sortTable[n].sortLookup = VectorDistSqr(AIL_ent->pos, check->pos);
				break;
			}
			sortTable[n++].data = check;
		}
	}

	/* Sort by given criterion - lesser first. */
	std::sort(sortTable, sortTable + n);

	/* Now save it in a Lua table. */
	lua_newtable(L);
	for (int i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		aiActor_t target;
		target.actor = sortTable[i].data;
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
				G_ClientStateChange(*AIL_player, AIL_ent, STATE_CROUCHED, true);
		} else
			AIL_invalidparameter(1);
	}

	lua_pushboolean(L, AIL_ent->isCrouched());
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

		if (reactionState) {
			G_ClientStateChange(*AIL_player, AIL_ent, reactionState, true);
		} else {
			AIL_invalidparameter(1);
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
 * @brief Actor tries to grab a weapon from inventory
 */
static int AIL_grabweapon (lua_State* L)
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
	ailShootPosType_t posType = AILSP_FAST;
	if ((lua_gettop(L) > 1))
		posType = AIL_toShotPInt(L, 2);

	/* Number of TU to spend shooting, to make sure we have enough tus to actually fire. */
	int tus = actor->getUsableTUs();
	if (lua_gettop(L) > 2) {
		assert(lua_isnumber(L, 3)); /* Must be a number. */

		tus = std::min(static_cast<int>(lua_tonumber(L, 3)), tus);
	}

	/* Don't shoot units under our control */
	if (!AI_IsHostile(actor, target->actor)) {
		lua_pushboolean(L, 0);
		return 1;
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

	int fdTime = G_ActorGetModifiedTimeForFiredef(AIL_ent, fd, false);
	if (tus - fdTime <= 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Calculate move table. */
	G_MoveCalc(0, actor, actor->pos, tus);
	gi.MoveStore(level.pathingMap);

	/* set borders */
	const int rad = (tus + 1) / TU_MOVE_STRAIGHT;

	pos3_t oldPos;
	vec3_t oldOrigin;
	VectorCopy(actor->pos, oldPos);
	VectorCopy(actor->origin, oldOrigin);

	/* evaluate moving to every possible location in the search area,
	 * including combat considerations */
	float bestScore = 0.0f;
	pos3_t to, bestPos;
	VectorSet(bestPos, 0, 0, PATHFINDING_HEIGHT);
	AiAreaSearch searchArea(oldPos, rad);
	while (searchArea.getNext(to)) {
		actor->setOrigin(to);
		const pos_t tu = G_ActorMoveLength(actor, level.pathingMap, to, true);
		if (tu > tus || tu == ROUTING_NOT_REACHABLE)
			continue;
		if (!AI_CheckPosition(actor, actor->pos))
			continue;
		/* Can we see the target? */
		if (!G_IsVisibleForTeam(target->actor, actor->getTeam()) && G_ActorVis(actor, target->actor, true) < ACTOR_VIS_10)
			continue;

		bool hasLoF = false;
		int shotChecked = NONE;
		float dist;
		for (shoot_types_t shootType = ST_RIGHT; shootType < ST_NUM_SHOOT_TYPES; shootType++) {
			const Item* item = AI_GetItemForShootType(shootType, AIL_ent);
			if (item == nullptr)
				continue;

			const fireDef_t* fdArray = item->getFiredefs();
			if (fdArray == nullptr)
				continue;

			for (fireDefIndex_t fdIdx = 0; fdIdx < item->ammoDef()->numFiredefs[fdArray->weapFdsIdx]; fdIdx++) {
				fd = &fdArray[fdIdx];
				fdTime = G_ActorGetModifiedTimeForFiredef(AIL_ent, fd, false);
				/* how many shoots can this actor do */
				const int shots = (tus - tu) / fdTime;
				if (shots < 1)
					continue;
				if (!AI_FighterCheckShoot(actor, target->actor, fd, &dist))
					continue;
				/* gun-to-target line free? */
				const int shotFlags = fd->gravity | (fd->launched << 1) | (fd->rolled << 2);
				if (shotChecked != shotFlags) {
					shotChecked = shotFlags;
					if ((hasLoF = AI_CheckLineOfFire(actor, target->actor, fd, shots)))
						break;
				}
			}
			if (hasLoF)
				break;
		}
		if (!hasLoF)
			continue;
		float score;
		switch (posType) {
		case AILSP_NEAR:
			score = -dist;
			break;
		case AILSP_FAR:
			score = dist;
			break;
		case AILSP_FAST:
		default:
			score = -tu;
			break;
		}
		if (score > bestScore || bestPos[2] >= PATHFINDING_HEIGHT) {
			VectorCopy(to, bestPos);
			bestScore = score;
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
			bool invTeam = false;
			if (s[0] == '-' || s[0] == '~') {
				invTeam = true;
				++s;
			}
			const int team = AIL_toTeamInt(s, 1);
			if (team == TEAM_ALL)
				AIL_invalidparameter(1);
			else if (invTeam)
				hidingTeam = -team;
			else
				hidingTeam = team;
		} else {
			AIL_invalidparameter(1);
		}
	}

	int tus = AIL_ent->getUsableTUs();
	/* parse parameter */
	if (lua_gettop(L) > 1) {
		if (lua_isnumber(L, 2)) {
			tus = std::min(static_cast<int>(lua_tonumber(L, 2)), tus);
		} else {
			AIL_invalidparameter(2);
		}
	}

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
 * @brief Determine the position where actor is closest to the target and
 * with the target located between the actor and the nemy enemy
 * @note @c target (parameter is passed through the lua stack) The actor
 * to which AI tries to become closer
 * @note @c inverse (passed through the lua stack) Try to shield the target instead
 * of using it as shield
 */
static int AIL_positionherd (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_isactor(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const aiActor_t* target = lua_toactor(L, 1);

	int tus = AIL_ent->getUsableTUs();
	/* parse parameter */
	if (lua_gettop(L) > 1) {
		if (lua_isnumber(L, 2)) {
			tus = std::min(static_cast<int>(lua_tonumber(L, 2)), tus);
		} else {
			AIL_invalidparameter(2);
		}
	}

	bool inverse = false;
	if (lua_gettop(L) > 2) {
		if (lua_isboolean(L, 3))
			inverse = lua_toboolean(L, 3);
		else
			AIL_invalidparameter(3);
	}

	pos3_t save;
	VectorCopy(AIL_ent->pos, save);
	if (AI_FindHerdLocation(AIL_ent, AIL_ent->pos, target->actor->origin, tus, inverse)) {
		lua_pushpos3(L, &AIL_ent->pos);
	} else {
		lua_pushboolean(L, 0);
	}
	AIL_ent->setOrigin(save);
	return 1;
}

/**
 * @brief Approach to a target actor
 */
static int AIL_positionapproach (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_ispos3(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}

	const pos3_t* target = lua_topos3(L, 1);
	assert(target != nullptr);

	int tus = AIL_ent->getUsableTUs();
	if (lua_gettop(L) > 1) {
		if (lua_isnumber(L, 2))
			tus = std::min(static_cast<int>(lua_tonumber(L, 2)), tus);
		else
			AIL_invalidparameter(2);
	}

	bool hide = false;
	if (lua_gettop(L) > 2){
		if (lua_isboolean(L, 3))
			hide = lua_toboolean(L, 3);
		else
			AIL_invalidparameter(3);
	}

	/* Find a path to the target actor */
	const int maxTUs = ROUTING_NOT_REACHABLE - 1;
	pos3_t to;
	VectorCopy(*target, to);
	byte crouchingState = AIL_ent->isCrouched() ? 1 : 0;
	if (!G_FindPath(0, AIL_ent, AIL_ent->pos, to, crouchingState, maxTUs)) {
		/* Not found */
		lua_pushboolean(L, 0);
		return 1;
	}

	/* Find the farthest we can go with current TUs */
	int dvec;
	while ((dvec = gi.MoveNext(level.pathingMap, to, crouchingState)) != ROUTING_UNREACHABLE) {
		/* Note: here we skip the first position so we don't try to walk into the target */
		PosSubDV(to, crouchingState, dvec);
		if (hide && (G_TestVis(AI_GetHidingTeam(AIL_ent), AIL_ent, VT_PERISHCHK | VT_NOFRUSTUM) & VS_YES))
			continue;
		if (!AI_CheckPosition(AIL_ent, to))
			continue;
		const byte length =  G_ActorMoveLength(AIL_ent, level.pathingMap, to, false);
		if (length <= tus)
			break;
		/* We are going backwards to the origin. */
	}

	if (AIL_ent->isSamePosAs(to))
		lua_pushboolean(L, 0);
	else
		lua_pushpos3(L, &to);
	return 1;
}

/**
 * @brief Returns the positions of the available mission targets.
 */
static int AIL_missiontargets (lua_State* L)
{

	/* Defaults. */
	int team = TEAM_ALL;
	ailVisType_t vision = AILVT_ALL;
	ailSortCritType_t sortCrit = AILSC_DIST;
	bool invTeam = false;

	/* Handle parameters. */
	if ((lua_gettop(L) > 0)) {
		/* Get what to "see" with. */
		vision = AIL_toVisInt(L, 1);

		/* We now check for different teams. */
		if ((lua_gettop(L) > 1)) {
			if (lua_isstring(L, 2)) {
				const char* s = lua_tostring(L, 2);
				if (s[0] == '-' || s[0] == '~') {
					invTeam = true;
					++s;
				}
				team = AIL_toTeamInt(s, 2);
				/* Trying to see no one? */
				if (team == TEAM_ALL && invTeam)
					AIL_invalidparameter(2);
			} else
				AIL_invalidparameter(2);
		}

		/* Sorting criteria */
		if ((lua_gettop(L) > 2))
			sortCrit = AIL_toDistInt(L, 3);
	}

	int n = 0;
	AilSortTable<Edict*> sortTable[MAX_EDICTS];
	/* Get visible things. */
	const int visDist = G_VisCheckDist(AIL_ent);
	Edict* mission = nullptr;
	while ((mission = G_EdictsGetNextInUse(mission))) {
		if (mission->type != ET_MISSION)
			continue;
		const float distance = VectorDistSqr(AIL_ent->pos, mission->pos);
		/* Check for team match if needed. */
		if ((team == TEAM_ALL || (mission->getTeam() == team ? !invTeam : invTeam))
				&& (vision == AILVT_ALL
				|| (vision == AILVT_SIGHT && !G_TestLineWithEnts(AIL_ent->origin, mission->origin))
				|| (vision == AILVT_DIST && distance <= visDist * visDist))) {
			switch (sortCrit) {
			case AILSC_PATH:
			{
				pos_t move = ROUTING_NOT_REACHABLE;
				if (G_FindPath(0, AIL_ent, AIL_ent->pos, mission->pos, false, ROUTING_NOT_REACHABLE - 1))
					move = gi.MoveLength(level.pathingMap, mission->pos, 0, false);
				sortTable[n].sortLookup = move;
			}
				break;
			case AILSC_DIST:
			default:
				sortTable[n].sortLookup = VectorDistSqr(AIL_ent->pos, mission->pos);
				break;
			}
			sortTable[n++].data = mission;
		}
	}

	/* Sort by given criterion - lesser first. */
	std::sort(sortTable, sortTable + n);

	/* Now save it in a Lua table. */
	lua_newtable(L);
	for (int i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		lua_pushpos3(L, &sortTable[i].data->pos); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of positions. */
}

/**
 * @brief Return the positions of the next waypoints
 */
static int AIL_waypoints (lua_State* L)
{
	const float minEnemyDist = 160.0f;
	/* Min distance to waypoint */
	float minDist = 800.0f;
	if (lua_gettop(L) > 0) {
		if (lua_isnumber(L, 1))
			minDist = lua_tonumber(L, 1);
		else
			AIL_invalidparameter(1);
	}

	/* Sorting criteria */
	ailSortCritType_t sortCrit = AILSC_DIST;
	if ((lua_gettop(L) > 1))
		sortCrit = AIL_toDistInt(L, 2);

	int n = 0;
	AilSortTable<Edict*> sortTable[MAX_EDICTS];
	for (Edict* checkPoint = level.ai_waypointList; checkPoint != nullptr; checkPoint = checkPoint->groupChain) {
		if (checkPoint->inuse)
			continue;
		if (checkPoint->getTeam() != AIL_ent->getTeam())
			continue;
		/* Don't walk to enemy ambush */
		Actor* check = nullptr;
		bool ambush = false;
		while ((check = G_EdictsGetNextLivingActorOfTeam(check, TEAM_ALIEN))) {
			const float dist = VectorDist(AIL_ent->origin, check->origin);
			/* @todo add visibility check here? */
			if (dist < minEnemyDist) {
				ambush = true;
				break;
			}
		}
		if (ambush)
			continue;
		switch (sortCrit) {
		case AILSC_PATH:
			{
				pos_t move = ROUTING_NOT_REACHABLE;
				if (G_FindPath(0, AIL_ent, AIL_ent->pos, checkPoint->pos, false, ROUTING_NOT_REACHABLE - 1))
					move = gi.MoveLength(level.pathingMap, checkPoint->pos, 0, false);
				if (move < minDist * TU_MOVE_STRAIGHT)
					continue;
				if (checkPoint->count < AIL_ent->count) {
					sortTable[n].sortLookup = move;
					sortTable[++n].data = checkPoint;
				}
			}
			break;
		case AILSC_DIST:
		default:
			{
				const float dist = VectorDist(AIL_ent->origin, checkPoint->origin);
				if (dist < minDist * UNIT_SIZE)
					continue;
				if (checkPoint->count < AIL_ent->count) {
					sortTable[n].sortLookup = dist;
					sortTable[++n].data = checkPoint;
				}
			}
			break;
		}
	}

	/* Sort by distance */
	std::sort(sortTable, sortTable + n);

	/* Now save it in a Lua table. */
	lua_newtable(L);
	for (int i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		lua_pushpos3(L, &sortTable[i].data->pos); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of positions. */
}

/**
 * @brief Try to find a position nearby to the given position
 * @note Intended to make aliens defend mission targets
 */
static int AIL_positionmission (lua_State* L)
{
	/* check parameter */
	if (!(lua_gettop(L) && lua_ispos3(L, 1))) {
		AIL_invalidparameter(1);
		lua_pushboolean(L, 0);
		return 1;
	}
	const pos3_t* target = lua_topos3(L, 1);
	int tus = AIL_ent->getUsableTUs();
	if (lua_gettop(L) > 1) {
		if (lua_isnumber(L, 2))
			tus = lua_tonumber(L, 2);
		else
			AIL_invalidparameter(2);
	}
	G_MoveCalc(0, AIL_ent, AIL_ent->pos, tus);
	gi.MoveStore(level.pathingMap);

	pos3_t oldPos;
	VectorCopy(AIL_ent->pos, oldPos);
	if (AI_FindMissionLocation(AIL_ent, *target, tus))
		lua_pushpos3(L, &AIL_ent->pos);
	else
		lua_pushboolean(L, 0);

	AIL_ent->setOrigin(oldPos);
	return 1;
}

/**
 * @brief Return a new position to move to.
 * @note To make the AI wander or patrol around.
 */
static int AIL_positionwander (lua_State* L)
{
	/* Calculate move table. */
	G_MoveCalc(0, AIL_ent, AIL_ent->pos, AIL_ent->getUsableTUs());
	gi.MoveStore(level.pathingMap);

	/* Set defaults */
	int radius = (AIL_ent->getUsableTUs() + 1) / TU_MOVE_STRAIGHT;
	pos3_t center;
	VectorCopy(AIL_ent->pos, center);
	int method = AILPW_RAND;
	int tus = AIL_ent->getUsableTUs();

	/* Check parameters */
	if (lua_gettop(L) > 0)
			method = AIL_toWanderPInt(L, 1);
	if (lua_gettop(L) > 1) {
		if (lua_isnumber(L, 2))
			radius = lua_tonumber(L, 2);
		else
			AIL_invalidparameter(2);
	}
	if (lua_gettop(L) > 2) {
		if (lua_ispos3(L, 3))
			VectorCopy(*lua_topos3(L, 3), center);
		else
			AIL_invalidparameter(3);
	}

	if (lua_gettop(L) > 3) {
		if (lua_isnumber(L, 4))
			tus = std::min(static_cast<int>(lua_tonumber(L, 4)), tus);
		else
			AIL_invalidparameter(4);
	}

	vec3_t d;
	if (method > 0)
		VectorSubtract(AIL_ent->pos, center, d);
	const int cDir = method > 0 ? (VectorEmpty(d) ? AIL_ent->dir : AngleToDir(static_cast<int>(atan2(d[1], d[0]) * todeg))) : NONE;
	float bestScore = 0;
	pos3_t bestPos = {0, 0, PATHFINDING_HEIGHT};
	pos3_t pos;
	AiAreaSearch searchArea(center, radius);
	while (searchArea.getNext(pos)) {
		const pos_t move = G_ActorMoveLength(AIL_ent, level.pathingMap, pos, true);
		if (move >= ROUTING_NOT_REACHABLE || move > tus)
			continue;
		if (!AI_CheckPosition(AIL_ent, pos))
			continue;
		float score = 0.0f;
		switch (method) {
		case AILPW_RAND:
			score = rand();
			break;
		case AILPW_CW:
		case AILPW_CCW: {
			score = VectorDistSqr(center, pos);
			VectorSubtract(pos, center, d);
			int dir = AngleToDir(static_cast<int>(atan2(d[1], d[0]) * todeg));
			if (!(method == AILPW_CW && dir == dvright[cDir]) && !(method == AILPW_CCW && dir == dvleft[cDir]))
				for (int n = 1; n < 8; ++n) {
					dir = method == 1 ? dvleft[dir] : dvright[dir];
					score /= pow(n * 2, 2);
					if ((method == 1 && dir == dvright[cDir]) || (method == 2 && dir == dvleft[cDir]))
						break;
				}
		}
			break;
		}
		if (score > bestScore) {
			bestScore = score;
			VectorCopy(pos, bestPos);
		}
	}

	if (bestPos[2] >= PATHFINDING_HEIGHT) {
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushpos3(L, &bestPos);
	return 1;
}

/**
 * @brief Returns a table of the positions of nearby usable weapons on the floor
 */
static int AIL_findweapons (lua_State* L)
{
	bool full = false;
	if (lua_gettop(L) > 0) {
		if (lua_isboolean(L, 1))
			full = lua_toboolean(L, 1);
		else
			AIL_invalidparameter(1);
	}

	AilSortTable<Edict*> sortTable[MAX_EDICTS];
	int n = 0;
	Edict* check = nullptr;
	while ((check = G_EdictsGetNextInUse(check))) {
		if (check->type != ET_ITEM)
			continue;
		if(!AI_CheckPosition(AIL_ent, check->pos))
			continue;
		if (!G_FindPath(0, AIL_ent, AIL_ent->pos, check->pos, AIL_ent->isCrouched(), ROUTING_NOT_REACHABLE - 1))
			continue;
		const pos_t move = G_ActorMoveLength(AIL_ent, level.pathingMap, check->pos, false);
		if (full || move <= AIL_ent->getUsableTUs() - INVDEF(CID_FLOOR)->out - INVDEF(CID_RIGHT)->in) {
			for (const Item* item = check->getFloor(); item; item = item->getNext()) {
				/** @todo Check if can reload the weapon with carried ammo or from the floor itself? */
				if (item->isWeapon() && (item->getAmmoLeft() > 0 || item->def()->ammo <= 0)) {
							sortTable[n].data = check;
							sortTable[n++].sortLookup = move;
					break;
				}
			}
		}
	}

	/* Sort by distance */
	std::sort(sortTable, sortTable + n);

	/* Now save it in a Lua table. */
	lua_newtable(L);
	for (int i = 0; i < n; i++) {
		lua_pushnumber(L, i + 1); /* index, starts with 1 */
		lua_pushpos3(L, &sortTable[i].data->pos); /* value */
		lua_rawset(L, -3); /* store the value in the table */
	}
	return 1; /* Returns the table of positions. */
}

/**
 * @brief Whether the current AI actor is a fighter or not
 */
static int AIL_isfighter (lua_State* L)
{
	const bool result = AIL_ent->chr.teamDef->weapons || AIL_ent->chr.teamDef->onlyWeapon;
	lua_pushboolean(L, result);
	return 1;
}

/**
 * @brief Mark the current waypoint for a civ
 */
static int AIL_setwaypoint (lua_State* L)
{
	/* No waypoint, reset the count value to restart the search */
	if (lua_gettop(L) < 1 || lua_isnil(L, 1)) {
		AIL_ent->count = 100;
		lua_pushboolean(L, 1);
	} else if (lua_ispos3(L, 1)){
		pos3_t pos;
		/** @todo A better way to handle waypoints */
		Edict* waypoint = G_GetEdictFromPos(pos, ET_CIVILIANTARGET);
		if (waypoint != nullptr) {
			AIL_ent->count = waypoint->count;
			lua_pushboolean(L, 1);
		} else
			lua_pushboolean(L, 0);
	} else
		lua_pushboolean(L, 0);

	return 1;
}

/**
 * @brief Return the difficulty number (in case we want different AI for different ones)
 */
static int AIL_difficulty (lua_State* L)
{
	lua_pushnumber(L, g_difficulty->value);
	return 1;
}

/**
 * @brief Return a position to flee to.
 */
static int AIL_positionflee (lua_State* L)
{
	int tus = AIL_ent->getUsableTUs();
	if (lua_gettop(L)) {
		if (lua_isnumber(L, 1))
			tus = std::min(static_cast<int>(lua_tonumber(L, 1)), tus);
		else
			AIL_invalidparameter(1);
	}

	/* Calculate move table. */
	G_MoveCalc(0, AIL_ent, AIL_ent->pos, AIL_ent->getUsableTUs());
	pos3_t oldPos;
	VectorCopy(AIL_ent->pos, oldPos);

	const int radius = (tus + 1) / TU_MOVE_STRAIGHT;
	float bestScore = -1;
	pos3_t bestPos = {0, 0, PATHFINDING_HEIGHT};
	AiAreaSearch searchArea(AIL_ent->pos, radius);
	while (searchArea.getNext(AIL_ent->pos)) {
		const pos_t move = G_ActorMoveLength(AIL_ent, level.pathingMap, AIL_ent->pos, false);
		if (move >= ROUTING_NOT_REACHABLE || move > tus)
			continue;
		if (!AI_CheckPosition(AIL_ent, AIL_ent->pos))
			continue;
		float minDistFoe = -1.0f, minDistFriend = -1.0f;
		Actor* check = nullptr;
		while ((check = G_EdictsGetNextLivingActor(check))) {
			const float dist = VectorDist(AIL_ent->origin, check->origin);
			if (check->isSameTeamAs(AIL_ent)) {
				if (dist < minDistFriend || minDistFriend < 0.0f)
					minDistFriend = dist;
			} else if (AI_IsHostile(AIL_ent, check) || AIL_ent->isPanicked()) {
				if (dist < minDistFoe || minDistFoe < 0.0f)
					minDistFoe = dist;
			}
		}
		float score = minDistFoe - (minDistFriend / GRID_WIDTH);
		/* Try to hide */
		AIL_ent->calcOrigin();
		if (G_TestVis(AI_GetHidingTeam(AIL_ent), AIL_ent, VT_PERISHCHK | VT_NOFRUSTUM) & VS_YES)
			score /= UNIT_SIZE;
		if (score > bestScore) {
			bestScore = score;
			VectorCopy(AIL_ent->pos, bestPos);
		}
	}
	AIL_ent->setOrigin(oldPos);

	if (bestPos[2] == PATHFINDING_HEIGHT) {
		lua_pushboolean(L, 0);
	} else {
		lua_pushpos3(L, &bestPos);
	}

	return 1;
}

/**
 * @brief Returns the type of the weapons in the actors hands.
 */
static int AIL_weapontype (lua_State* L)
{
	const Item* right = AIL_ent->getRightHandItem();
	const Item* left = AIL_ent->getLeftHandItem();

	lua_pushstring(L, right ? right->def()->type : "none");
	lua_pushstring(L, left ? left->def()->type : "none");

	return 2;
}

/**
 * @brief Returns the currently moving AI actor
 */
static int AIL_actor (lua_State* L)
{
	aiActor_t actor = {AIL_ent};
	lua_pushactor(L, &actor);
	return 1;
}
/**
 * @brief Returns the min TUs the actor needs to fire.
 */
static int AIL_tusforshooting (lua_State* L)
{
	int bestTUs = 256;
	const Item* weapon = AIL_ent->getRightHandItem();
	if (weapon) {
		const fireDef_t* fd = weapon->getFastestFireDef();
		if (fd)
			bestTUs = fd->time;
	}
	weapon = AIL_ent->getLeftHandItem();
	if (weapon) {
		const fireDef_t* fd = weapon->getFastestFireDef();
		if (fd) {
			const int tus = weapon->getFastestFireDef()->time;
			if (tus < bestTUs)
				bestTUs = tus;
		}
	}

	lua_pushnumber(L, bestTUs);
	return 1;
}

/**
 * @brief Returns the AI actor's class.
 */
static int AIL_class (lua_State* L)
{
	lua_pushstring(L, AIL_ent->AI.subtype);
	return 1;
}

/**
 * @brief Check if the actor needs wants to hide.
 */
static int AIL_hideneeded (lua_State* L)
{
	lua_pushboolean(L, AI_HideNeeded(AIL_ent));
	return 1;
}

/**
 * @brief The think function for the ai controlled players
 * @param[in] player
 * @param[in] actor
 */
void AIL_ActorThink (Player& player, Actor* actor)
{
	/* Set the global player and edict */
	AIL_ent = actor;
	AIL_player = &player;

	/* Try to run the function. */
	lua_getglobal(ailState, actor->AI.type);
	if (lua_istable(ailState, -1)) {
		lua_getfield(ailState, -1, "think");
		if (lua_pcall(ailState, 0, 0, 0)) { /* error has occured */
			gi.DPrintf("Error while running Lua: %s\n",
				lua_isstring(ailState, -1) ? lua_tostring(ailState, -1) : "Unknown Error");
		}
	} else {
		gi.DPrintf("Error while running Lua: AI for %s not found!\n", actor->AI.type);
	}

	/* Cleanup */
	AIL_ent = nullptr;
	AIL_player = nullptr;
}

/**
 * @brief Return the AI type for the given team (the lua file the team actors should run)
 */
static const char* AIL_GetAIType (const int team)
{
	const char* type;
	switch (team) {
	case TEAM_ALIEN:
		type = "alien";
		break;
	case TEAM_CIVILIAN:
		type = "civilian";
		break;
	case TEAM_PHALANX:
	default:	/* Default to "soldier" AI for multiplayer teams */
		type = "soldier";
		break;
	}
	return type;
}

/**
 * @brief The team think function for the ai controlled players
 * @param[in] player
 */
bool AIL_TeamThink (Player& player)
{
	/* Set the global player */
	AIL_player = &player;
	AIL_ent = nullptr;

	bool thinkAgain = false;
	/* Try to run the function. */
	lua_getglobal(ailState, AIL_GetAIType(player.getTeam()));
	if (lua_istable(ailState, -1)) {
		lua_getfield(ailState, -1, "team_think");
		if (lua_pcall(ailState, 0, 1, 0)) { /* error has occured */
			gi.DPrintf("Error while running Lua: %s\n",
				lua_isstring(ailState, -1) ? lua_tostring(ailState, -1) : "Unknown Error");
		} else
			thinkAgain = lua_toboolean(ailState, -1);
	} else {
		gi.DPrintf("Error while running Lua: AI for %s not found!\n", AIL_toTeamString(player.getTeam()));
	}

	/* Cleanup */
	AIL_player = nullptr;
	AIL_ent = nullptr;
	return thinkAgain;
}

/**
 * Initializes a new ai lua state.
 */
static lua_State* AIL_InitLua () {
	/* Create the Lua state */
	lua_State* newState = luaL_newstate();

	/* Register metatables. */
	actorL_register(newState);
	pos3L_register(newState);

	/* Register libraries. */
	luaL_register(newState, AI_METATABLE, AIL_methods);

	return newState;
}
/**
 * @brief Initializes the lua AI for an actor.
 * @param[in] ent Pointer to actor to initialize AI for.
 * @param[in] type Type of AI (Lua file name without .lua).
 * @param[in] subtype Subtype of the AI.
 * @return 0 on success.
 */
int AIL_InitActor (Actor* actor)
{
	/* Prepare the AI */
	AI_t* AI = &actor->AI;
	Q_strncpyz(AI->type, AIL_GetAIType(actor->getTeam()), sizeof(AI->type));
	Q_strncpyz(AI->subtype, actor->chr.teamDef->id, sizeof(AI->subtype));

	/* Create the a new Lua state if needed */
	if (ailState == nullptr)
		ailState = AIL_InitLua();

	if (ailState == nullptr) {
		gi.DPrintf("Unable to create Lua state.\n");
		return -1;
	}

	/* Load the AI if needed */
	lua_getglobal(ailState, AI->type);
	if (!lua_istable(ailState, -1)) {
		char path[MAX_VAR];
		Com_sprintf(path, sizeof(path), "ai/%s.lua", AI->type);
		char* fbuf;
		const int size = gi.FS_LoadFile(path, (byte**) &fbuf);
		if (size == 0) {
			gi.DPrintf("Unable to load Lua file '%s'.\n", path);
			return -1;
		}
		if (luaL_dobuffer(ailState, fbuf, size, path)) {
			gi.DPrintf("Unable to parse Lua file '%s'\n", path);
			gi.DPrintf("%s\n", lua_isstring(ailState, -1) ? lua_tostring(ailState, -1) : "Unknown Error");
			gi.FS_FreeFile(fbuf);
			return -1;
		}
		lua_setglobal(ailState, AI->type);
		gi.FS_FreeFile(fbuf);
	} else {
		lua_pop(ailState, 1);
	}

	return 0;
}

void AIL_Init (void)
{
	gi.RegisterConstInt("luaaiteam::phalanx", TEAM_PHALANX);
	gi.RegisterConstInt("luaaiteam::civilian", TEAM_CIVILIAN);
	gi.RegisterConstInt("luaaiteam::alien", TEAM_ALIEN);
	gi.RegisterConstInt("luaaiteam::all", TEAM_ALL);

	gi.RegisterConstInt("luaaivis::all", AILVT_ALL);
	gi.RegisterConstInt("luaaivis::sight", AILVT_SIGHT);
	gi.RegisterConstInt("luaaivis::team", AILVT_TEAM);
	gi.RegisterConstInt("luaaivis::extra", AILVT_DIST);

	gi.RegisterConstInt("luaaisort::dist", AILSC_DIST);
	gi.RegisterConstInt("luaaisort::path", AILSC_PATH);
	gi.RegisterConstInt("luaaisort::HP", AILSC_HP);

	gi.RegisterConstInt("luaaidist::dist", AILSC_DIST);
	gi.RegisterConstInt("luaaidist::path", AILSC_PATH);

	gi.RegisterConstInt("luaaishot::fastest", AILSP_FAST);
	gi.RegisterConstInt("luaaishot::nearest", AILSP_NEAR);
	gi.RegisterConstInt("luaaishot::farthest", AILSP_FAR);

	gi.RegisterConstInt("luaaiwander::rand", AILPW_RAND);
	gi.RegisterConstInt("luaaiwander::CW", AILPW_CW);
	gi.RegisterConstInt("luaaiwander::CCW", AILPW_CCW);

	ailState = nullptr;
}

void AIL_Shutdown (void)
{
	gi.UnregisterConstVariable("luaaiteam::phalanx");
	gi.UnregisterConstVariable("luaaiteam::civilian");
	gi.UnregisterConstVariable("luaaiteam::alien");
	gi.UnregisterConstVariable("luaaiteam::all");

	gi.UnregisterConstVariable("luaaivis::all");
	gi.UnregisterConstVariable("luaaivis::sight");
	gi.UnregisterConstVariable("luaaivis::team");
	gi.UnregisterConstVariable("luaaivis::extra");

	gi.UnregisterConstVariable("luaaisort::dist");
	gi.UnregisterConstVariable("luaaisort::path");
	gi.UnregisterConstVariable("luaaisort::HP");

	gi.UnregisterConstVariable("luaaidist::dist");
	gi.UnregisterConstVariable("luaaidist::path");

	gi.UnregisterConstVariable("luaaishot::fastest");
	gi.UnregisterConstVariable("luaaishot::nearest");
	gi.UnregisterConstVariable("luaaishot::farthest");

	gi.UnregisterConstVariable("luaaiwander::rand");
	gi.UnregisterConstVariable("luaaiwander::CW");
	gi.UnregisterConstVariable("luaaiwander::CCW");
}

/**
 * @brief Closes the LUA AI.
 */
void AIL_Cleanup (void)
{
	lua_close(ailState);
	ailState = nullptr;
}
