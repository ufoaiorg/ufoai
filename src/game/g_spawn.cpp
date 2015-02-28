/**
 * @file
 * @brief Brings new objects into the world.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_spawn.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "g_spawn.h"
#include "g_ai.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_func.h"
#include "g_inventory.h"
#include "g_mission.h"
#include "g_reaction.h"
#include "g_trigger.h"
#include "g_utils.h"
#include "g_vis.h"
#include "../shared/parse.h"
#include "../shared/keyvaluepair.h"

/* fields are needed for spawning from the entity string */
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2

#define G_ValidMessage(ent)		((ent)->message && ((ent)->message[0] == '_' || strstr((ent)->message, "*msgid:") != nullptr))

/**
 * @brief this is only used to hold entity field values that can be set from
 * the editor, but aren't actually present in Edict during gameplay
 */
typedef struct spawn_temp_s {
	/* world vars */
	int noRandomSpawn;	/**< spawn the actors on random spawnpoints */
	int noEquipment;	/**< spawn the actors with no equipment - must be collected in the map */
} spawn_temp_t;

static spawn_temp_t spawnTemp;

static void SP_light(Edict* ent);
static void SP_dummy(Edict* ent);
static void SP_player_start(Edict* ent);
static void SP_human_start(Edict* ent);
static void SP_alien_start(Edict* ent);
static void SP_civilian_start(Edict* ent);
static void SP_worldspawn(Edict* ent);
static void SP_2x2_start(Edict* ent);
static void SP_civilian_target(Edict* ent);
static void SP_misc_model(Edict* ent);
static void SP_misc_item(Edict* ent);
static void SP_misc_mission(Edict* ent);
static void SP_misc_mission_aliens(Edict* ent);
static void SP_misc_message(Edict* ent);
static void SP_misc_smoke(Edict* ent);
static void SP_misc_fire(Edict* ent);
static void SP_misc_camera(Edict* ent);
static void SP_misc_smokestun(Edict* ent);

typedef struct spawn_s {
	const char* name;
	void (*spawn) (Edict* ent);
} spawn_t;

static const spawn_t spawns[] = {
	{"worldspawn", SP_worldspawn},
	{"light", SP_light},
	{"misc_item", SP_misc_item},
	{"misc_sound", SP_dummy},
	{"misc_model", SP_misc_model},
	{"misc_particle", SP_dummy},
	{"misc_mission", SP_misc_mission},
	{"misc_mission_aliens", SP_misc_mission_aliens},
	{"info_player_start", SP_player_start},
	{"info_human_start", SP_human_start},
	{"info_alien_start", SP_alien_start},
	{"info_civilian_start", SP_civilian_start},
	{"info_civilian_target", SP_civilian_target},
	{"info_2x2_start", SP_2x2_start},
	{"info_null", SP_dummy},
	{"func_breakable", SP_func_breakable},
	{"func_door", SP_func_door},
	{"func_door_sliding", SP_func_door_sliding},
	{"func_rotating", SP_func_rotating},
	{"trigger_nextmap", SP_trigger_nextmap},
	{"trigger_hurt", SP_trigger_hurt},
	{"trigger_touch", SP_trigger_touch},
	{"trigger_rescue", SP_trigger_rescue},
	{"misc_message", SP_misc_message},
	{"misc_smoke", SP_misc_smoke},
	{"misc_fire", SP_misc_fire},
	{"misc_smokestun", SP_misc_smokestun},
	{"misc_camera", SP_misc_camera},

	{nullptr, nullptr}
};

/**
 * @brief Finds the spawn function for the entity and calls it
 */
static void ED_CallSpawn (Edict* ent)
{
	if (!ent->classname)
		return;

	/* check normal spawn functions */
	for (const spawn_t* s = spawns; s->name; s++) {
		/* found it */
		if (Q_streq(s->name, ent->classname)) {
			s->spawn(ent);
			return;
		}
	}

	ent->inuse = false;
}

/**
 * @brief Allocated memory for the given string in the level context (TAG_LEVEL)
 * @note This memory is automatically freed when we close or change the level
 * @param[in] string The string to copy
 */
static char* ED_NewString (const char* string)
{
	const size_t l = strlen(string) + 1;
	char* newb = (char*)G_TagMalloc(l, TAG_LEVEL);
	char* new_p = newb;

	for (int i = 0; i < l; i++) {
		/* check for special chars and convert them */
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
	}

	return newb;
}

/**
 * @brief Takes a key/value pair and sets the binary values in an edict
 */
static void ED_ParseField (const char* key, const char* value, Edict* ent)
{
	KeyValuePair kvp(key, value);

	if (kvp.isKey("classname"))
		ent->classname = ED_NewString(value);
	else if (kvp.isKey("model"))
		ent->model = ED_NewString(value);
	else if (kvp.isKey("spawnflags"))
		ent->spawnflags = kvp.asInt();
	else if (kvp.isKey("speed"))
		ent->speed = kvp.asInt();
	else if (kvp.isKey("dir"))
		ent->dir = kvp.asInt();
	else if (kvp.isKey("active"))
		ent->active = kvp.asBool();
	else if (kvp.isKey("target"))
		ent->target = ED_NewString(value);
	else if (kvp.isKey("targetname"))
		ent->targetname = ED_NewString(value);
	else if (kvp.isKey("item"))
		ent->item = ED_NewString(value);
	else if (kvp.isKey("noise"))
		ent->noise = ED_NewString(value);
	else if (kvp.isKey("particle"))
		ent->particle = ED_NewString(value);
	else if (kvp.isKey("nextmap"))
		ent->nextmap = ED_NewString(value);
	else if (kvp.isKey("frame"))
		ent->frame = kvp.asInt();
	else if (kvp.isKey("team"))
		ent->setTeam(kvp.asInt());
	else if (kvp.isKey("group"))
		ent->group = ED_NewString(value);
	else if (kvp.isKey("size"))
		ent->fieldSize = kvp.asInt();
	else if (kvp.isKey("count"))
		ent->count = kvp.asInt();
	else if (kvp.isKey("time"))
		ent->time = kvp.asInt();
	else if (kvp.isKey("health"))
		ent->HP = kvp.asInt();
	else if (kvp.isKey("radius"))
		ent->radius = kvp.asInt();
	else if (kvp.isKey("sounds"))
		ent->sounds = kvp.asInt();
	else if (kvp.isKey("material"))
		ent->material = static_cast<edictMaterial_t>(kvp.asInt()); // enum !!
	else if (kvp.isKey("light"))
		; // ignore
	/** @todo This (maxteams) should also be handled server side - currently this is
	 * only done client side */
	else if (kvp.isKey("maxteams"))
		; // ignore
	else if (kvp.isKey("maxlevel"))
		; // ignore
	else if (kvp.isKey("dmg"))
		ent->dmg = kvp.asInt();
	else if (kvp.isKey("origin"))
		kvp.asVec3(ent->origin);
	else if (kvp.isKey("angles"))
		kvp.asVec3(ent->angles);
	else if (kvp.isKey("angle"))
		ent->angle = kvp.asFloat();
	else if (kvp.isKey("message"))
		ent->message = ED_NewString(value);

	else if (kvp.isKey("norandomspawn"))
		spawnTemp.noRandomSpawn = kvp.asInt();
	else if (kvp.isKey("noequipment"))
		spawnTemp.noEquipment = kvp.asInt();
}

/**
 * @brief Parses an edict out of the given string, returning the new position
 * @param[in] data The string to parse from
 * @param[in] ent should be a properly initialized empty edict.
 */
static const char* ED_ParseEdict (const char* data, Edict* ent)
{
	char keyname[MAX_VAR];

	bool reset = true;
	OBJZERO(spawnTemp);

	/* go through all the dictionary pairs */
	while (1) {
		/* parse key */
		const char* c = Com_Parse(&data);
		if (c[0] == '}')
			break;
		if (!data)
			gi.Error("ED_ParseEntity: EOF without closing brace");

		Q_strncpyz(keyname, c, sizeof(keyname));

		/* parse value */
		c = Com_Parse(&data);
		if (!data)
			gi.Error("ED_ParseEntity: EOF without closing brace");

		if (c[0] == '}')
			gi.Error("ED_ParseEntity: closing brace without data");

		reset = false;

		/* keynames with a leading underscore are used for utility comments,
		 * and are immediately discarded by ufo */
		if (keyname[0] == '_')
			continue;

		ED_ParseField(keyname, c, ent);
	}

	if (reset)
		ent->nativeReset();

	return data;
}

/**
 * @brief Chain together all entities with a matching team field.
 * All but the first will have the FL_GROUPSLAVE flag set.
 * All but the last will have the groupchain field set to the next one
 */
static void G_FindEdictGroups (void)
{
	Edict* ent = G_EdictsGetFirst(); /* the first edict is always a world edict that can be skipped */

	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* no group at all */
		if (!ent->group)
			continue;
		/* already marked as slave in another group */
		if (ent->flags & FL_GROUPSLAVE)
			continue;
		Edict* chain = ent;
		ent->groupMaster = ent;
		Edict* groupMember = ent;
		/* search only the remainder of the entities */
		while ((groupMember = G_EdictsGetNextInUse(groupMember))) {
			/* no group at all */
			if (!groupMember->group)
				continue;
			/* already marked as slave in another group */
			if (groupMember->flags & FL_GROUPSLAVE)
				continue;
			/* same group as the master? */
			if (Q_streq(ent->group, groupMember->group)) {
				chain->groupChain = groupMember;
				groupMember->groupMaster = ent;
				chain = groupMember;
				groupMember->flags |= FL_GROUPSLAVE;
			}
		}
	}
}

/**
 * @brief Creates a server's entity / program execution context
 * by parsing textual entity definitions out of an ent file.
 * @sa CM_EntityString
 * @sa SV_SpawnServer
 */
void G_SpawnEntities (const char* mapname, bool day, const char* entities)
{
	G_FreeTags(TAG_LEVEL);

	OBJZERO(level);
	level.pathingMap = (pathing_t*)G_TagMalloc(sizeof(*level.pathingMap), TAG_LEVEL);

	G_EdictsInit();

	/* initialize reactionFire data */
	G_ReactionFireTargetsInit();

	Q_strncpyz(level.mapname, mapname, sizeof(level.mapname));
	level.day = day;

	G_ResetClientData();

	level.activeTeam = TEAM_NO_ACTIVE;
	level.actualRound = 1;
	level.hurtAliens = sv_hurtaliens->integer;

	/* parse ents */
	int entnum = 0;
	while (1) {
		/* parse the opening brace */
		const char* token = Com_Parse(&entities);
		if (!entities)
			break;
		if (token[0] != '{')
			gi.Error("ED_LoadFromFile: found %s when expecting {", token);

		Edict* ent = G_Spawn();

		entities = ED_ParseEdict(entities, ent);

		ent->mapNum = entnum++;

		/* Set the position of the entity */
		VecToPos(ent->origin, ent->pos);

		/* Call this entity's specific initializer (sets ent->type) */
		ED_CallSpawn(ent);

		/* if this entity is an bbox (e.g. actor), then center its origin based on its position */
		if (ent->solid == SOLID_BBOX)
			ent->calcOrigin();
	}

	/* spawn ai players, if needed */
	if (level.num_spawnpoints[TEAM_CIVILIAN]) {
		if (AI_CreatePlayer(TEAM_CIVILIAN) == nullptr)
			gi.DPrintf("Could not create civilian\n");
	}

	if ((G_IsSinglePlayer() || ai_multiplayeraliens->integer) && level.num_spawnpoints[TEAM_ALIEN]) {
		if (AI_CreatePlayer(TEAM_ALIEN) == nullptr)
			gi.DPrintf("Could not create alien\n");
	}

	Com_Printf("Used inventory slots after ai spawn: %i\n", game.invi.GetUsedSlots());

	G_FindEdictGroups();
}

/**
 * @brief Either finds a free edict, or allocates a new one.
 * @note Try to avoid reusing an entity that was recently freed, because it
 * can cause the player to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 * @sa G_FreeEdict
 */
Edict* G_Spawn (const char* classname)
{
	Edict* ent = G_EdictsGetNewEdict();

	if (!ent)
		gi.Error("G_Spawn: no free edicts");

	ent->inuse = true;
	ent->number = G_EdictsGetNumber(ent);
	if (classname)
		ent->classname = classname;
	else
		ent->classname = "noclass";
	ent->fieldSize = ACTOR_SIZE_NORMAL;
	ent->setActive();		/* only used by camera */
	return ent;
}

static void Think_SmokeAndFire (Edict* self)
{
	const int endRound = self->time + self->count;
	const int spawnIndex = (self->getTeam() + level.teamOfs) % MAX_TEAMS;
	const int currentIndex = (level.activeTeam + level.teamOfs) % MAX_TEAMS;
	if (endRound < level.actualRound || (endRound == level.actualRound && spawnIndex <= currentIndex)) {
		const bool checkVis = self->type == ET_SMOKE;
		G_EventEdictPerish(G_VisToPM(self->particleLink->visflags), *self->particleLink);
		G_FreeEdict(self->particleLink);
		G_FreeEdict(self);
		if (checkVis)
			G_CheckVis(nullptr);
	}
}

static void G_SpawnFieldPart (const entity_type_t fieldtype, const vec3_t vec, const char* particle, int rounds, int damage)
{
	pos3_t pos;
	VecToPos(vec, pos);		/* calculate grid position */

	Edict* ent = G_GetEdictFromPos(pos, fieldtype);
	if (ent == nullptr) {
		pos_t z = gi.GridFall(ACTOR_SIZE_NORMAL, pos);
		if (z != pos[2])
			return;

		ent = G_Spawn();
		VectorCopy(pos, ent->pos);
		ent->calcOrigin();		/* although vec is supposed to be the origin, calc origin from pos. That's safer. */
		ent->dmg = damage;
		ent->particle = particle;
		ent->spawnflags = G_GetLevelFlagsFromPos(pos);
		switch (fieldtype) {
		case ET_SMOKE:
			SP_misc_smoke(ent);
			break;
		case ET_FIRE:
			SP_misc_fire(ent);
			break;
		case ET_SMOKESTUN:
			SP_misc_smokestun(ent);
			break;
		default:
			break;
		}
	}

	ent->count = rounds;
}

/**
 * @brief Spawns a field that is available for some rounds
 * @param[in] fieldtype The kind of field eg. smoke, fire, ...
 * @param[in] vec The position in the world that is the center of the field
 * @param[in] particle The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] rounds The number of rounds the particle will last
 * @todo Does '2 rounds' mean: created in player's turn, last through the aliens turn, vanish before the 2nd player's turn ??
 * @param[in] damage The damage dealt by the field
 * @param[in] radius The max distance of a cell from the center to get a particle
 */
static void G_SpawnFieldGroup (const entity_type_t fieldtype, const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius)
{
	G_SpawnFieldPart(fieldtype, vec, particle, rounds, damage);

	/* for all cells in a square of +/- radius */
	for (vec_t x = vec[0] - radius; x <= vec[0] + radius; x += UNIT_SIZE) {
		for (vec_t y = vec[1] - radius; y <= vec[1] + radius; y += UNIT_SIZE) {
			vec3_t end;
			VectorSet(end, x, y, vec[2]);

			/* cut off the edges of the square to resemble a circle */
			if (VectorDist(end, vec) > radius)
				continue;
			if (!gi.isOnMap(vec))
				continue;
			const trace_t tr = G_Trace(Line(vec, end), nullptr, MASK_SMOKE_AND_FIRE);
			/* trace didn't reach the target - something was hit before */
			if (tr.fraction < 1.0 || (tr.contentFlags & CONTENTS_WATER)) {
				continue;
			}
			G_SpawnFieldPart(fieldtype, end, particle, rounds, damage);
		}
	}
}

/**
 * @brief Spawns a smoke field that is available for some rounds
 * @param[in] vec The position in the world that is the center of the smoke field
 * @param[in] particle The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] rounds The number of rounds the particle will last
 * @todo Does '2 rounds' mean: created in player's turn, last through the aliens turn, vanish before the 2nd player's turn ??
 * @param[in] damage The amount of damage this field will cause when touched by an actor.
 * @param[in] radius The max distance of a cell from the center to get a particle
 */
void G_SpawnSmokeField (const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius)
{
	G_SpawnFieldGroup(ET_SMOKE, vec, particle, rounds, damage, radius);
}

void G_SpawnFireField (const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius)
{
	G_SpawnFieldGroup(ET_FIRE, vec, particle, rounds, damage, radius);
}

void G_SpawnStunSmokeField (const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius)
{
	G_SpawnFieldGroup(ET_SMOKESTUN, vec, particle, rounds, damage, radius);
}

/**
 * @brief Spawns a new entity at the floor
 * @note This is e.g. used to place dropped weapons/items at the floor
 */
Edict* G_SpawnFloor (const pos3_t pos)
{
	Edict* floorItem = G_Spawn("item");
	floorItem->type = ET_ITEM;
	/* make sure that the item is always on a field that even the smallest actor can reach */
	floorItem->fieldSize = ACTOR_SIZE_NORMAL;
	VectorCopy(pos, floorItem->pos);
	floorItem->pos[2] = gi.GridFall(floorItem->fieldSize, floorItem->pos);
	floorItem->calcOrigin();
	return floorItem;
}

/**
 * This is only for particles that are spawned during a match - not for map particles.
 * @return A particle edict
 */
Edict* G_SpawnParticle (const vec3_t origin, int spawnflags, const char* particle)
{
	Edict* ent = G_Spawn("particle");
	ent->type = ET_PARTICLE;
	VectorCopy(origin, ent->origin);

	/* Set the position of the entity */
	VecToPos(ent->origin, ent->pos);

	ent->particle = particle;
	ent->spawnflags = spawnflags;

	G_CheckVis(ent);

	return ent;
}

/**
 * @brief Spawn point for a 1x1 unit.
 */
static void G_ActorSpawn (Edict* ent)
{
	/* set properties */
	level.num_spawnpoints[ent->getTeam()]++;
	ent->classname = "actor";
	ent->type = ET_ACTORSPAWN;
	ent->fieldSize = ACTOR_SIZE_NORMAL;

	/* Fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;

	ent->pos[2] = gi.GridFall(ent->fieldSize, ent->pos);
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		gi.DPrintf("G_ActorSpawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);

	ent->calcOrigin();

	/* link it for collision detection */
	ent->dir = AngleToDir(ent->angle);
	assert(ent->dir < CORE_DIRECTIONS);
	ent->solid = SOLID_BBOX;

	/* Set bounding box. Maybe this is already set in one of the spawn functions? */
	if (ent->entBox.getMaxX() == 0)
		ent->entBox.setMaxs(PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
	if (ent->entBox.getMinX() == 0)
		ent->entBox.setMins(-PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN);
}

/**
 * @brief Spawn a singleplayer 2x2 unit.
 */
static void G_Actor2x2Spawn (Edict* ent)
{
	/* set properties */
	level.num_2x2spawnpoints[ent->getTeam()]++;
	ent->classname = "ugv";
	ent->type = ET_ACTOR2x2SPAWN;
	ent->fieldSize = ACTOR_SIZE_2x2;

	/* Spawning has already calculated the pos from the origin ( = center of the cell). Perfect for normal size actors.
	 * For 2x2 actors, the origin(of the info_ box) is in the middle of the four cells. Using VecToPos on that origin
	 * results in the upper right cell being the pos of the actor. But we want the lower left cell to be the pos of the
	 * 2x2 actor because routing and pathfinding rely on that. So compensate for that. */
	ent->pos[0]--;
	ent->pos[1]--;

	/* Fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;
	ent->pos[2] = gi.GridFall(ent->fieldSize, ent->pos);
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		gi.DPrintf("G_Actor2x2Spawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);
	ent->calcOrigin();

	/* link it for collision detection */
	ent->dir = AngleToDir(ent->angle);
	assert(ent->dir < CORE_DIRECTIONS);
	ent->solid = SOLID_BBOX;

	/* Set bounding box. Maybe this is already set in one of the spawn functions? */
	if (ent->entBox.getMaxX() == 0)
		ent->entBox.setMaxs(PLAYER2x2_WIDTH, PLAYER2x2_WIDTH, PLAYER_STAND);
	if (ent->entBox.getMinX() == 0)
		ent->entBox.setMins(-PLAYER2x2_WIDTH, -PLAYER2x2_WIDTH, PLAYER_MIN);
}

/**
 * @brief light (0 1 0) (-8 -8 -8) (8 8 8)
 */
static void SP_light (Edict* ent)
{
	/* lights aren't client-server communicated items */
	/* they are completely client side */
	G_FreeEdict(ent);
}

/**
 * @brief info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
 * Starting point for a player.
 * "team"	the number of the team for this player starting point
 * "0" is reserved for civilians and critters (use info_civilian_start instead)
 */
static void SP_player_start (Edict* ent)
{
	/* only used in multi player */
	if (G_IsSinglePlayer()) {
		G_FreeEdict(ent);
		return;
	}

	/** @todo Wrong place here */
	/* maybe there are already the max soldiers allowed per team connected */
	if (sv_maxsoldiersperteam->integer > level.num_spawnpoints[ent->getTeam()]) {
		G_ActorSpawn(ent);
	} else
		G_FreeEdict(ent);
}

/**
 * @brief info_human_start (1 0 0) (-16 -16 -24) (16 16 32)
 * Starting point for a single player human.
 */
static void SP_human_start (Edict* ent)
{
	/* only used in single player */
	if (G_IsMultiPlayer()) {
		G_FreeEdict(ent);
		return;
	}

	ent->setTeam(TEAM_PHALANX);
	G_ActorSpawn(ent);
}


/**
 * @brief info_2x2_start (1 1 0) (-32 -32 -24) (32 32 32)
 * Starting point for a 2x2 unit.
 */
static void SP_2x2_start (Edict* ent)
{
	/* no 2x2 unit in multiplayer */
	if (G_IsMultiPlayer()) {
		G_FreeEdict(ent);
		return;
	}

	if (!ent->getTeam())
		ent->setTeam(TEAM_PHALANX);

	/* these units are bigger */
	ent->entBox.setMaxs(PLAYER_WIDTH * 2, PLAYER_WIDTH * 2, PLAYER_STAND);
	ent->entBox.setMins(-(PLAYER_WIDTH * 2), -(PLAYER_WIDTH * 2), PLAYER_MIN);

	/* spawn singleplayer 2x2 unit */
	G_Actor2x2Spawn(ent);
}

/**
 * @brief info_alien_start (1 0 0) (-16 -16 -24) (16 16 32)
 * Starting point for a single player alien.
 */
static void SP_alien_start (Edict* ent)
{
	/* deactivateable in multiplayer */
	if (G_IsMultiPlayer() && !ai_multiplayeraliens->integer) {
		G_FreeEdict(ent);
		return;
	}
	ent->setTeam(TEAM_ALIEN);

	G_ActorSpawn(ent);
}


/**
 * @brief info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
 * Starting point for a civilian.
 */
static void SP_civilian_start (Edict* ent)
{
	/* deactivateable in multiplayer */
	if (G_IsMultiPlayer() && !ai_numcivilians->integer) {
		G_FreeEdict(ent);
		return;
	}
	ent->setTeam(TEAM_CIVILIAN);
	ent->count = 100; /* current waypoint */
	G_ActorSpawn(ent);
}

/**
 * @brief info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
 * Way point for a civilian.
 * @sa SP_civilian_start
 * @todo These waypoints should be placeable by the human player (e.g. spawn a special particle on the waypoint)
 * to direct the civilians to a special location
 */
static void SP_civilian_target (Edict* ent)
{
	/* target point for which team */
	ent->setTeam(TEAM_CIVILIAN);
	ent->classname = "civtarget";
	ent->type = ET_CIVILIANTARGET;
	ent->fieldSize = ACTOR_SIZE_NORMAL; /* to let the grid fall function work */

	/* add the edict to the list of known waypoints */
	G_AddToWayPointList(ent);

	/* fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;
	ent->pos[2] = gi.GridFall(ent->fieldSize, ent->pos);
	ent->calcOrigin();
}

/**
 * @brief Initializes the human/phalanx mission entity
 */
static void SP_misc_mission (Edict* ent)
{
	ent->classname = "misc_mission";
	ent->type = ET_MISSION;

	/* maybe this was set to something else for multiplayer */
	if (!ent->getTeam())
		ent->setTeam(TEAM_PHALANX);

	ent->solid = SOLID_BBOX;

	if (ent->HP) {
		ent->flags |= FL_DESTROYABLE;
		ent->destroy = G_MissionDestroy;
	}

	if (!ent->HP && !ent->time && !ent->target) {
		G_FreeEdict(ent);
		gi.DPrintf("misc_mission given with no objective\n");
		return;
	}

	/* think function values */
	ent->think = G_MissionThink;
	ent->nextthink = 1;

	if (ent->radius <= GRID_WIDTH) {
		ent->radius = GRID_WIDTH * 3;
	}
	ent->absBox.setMaxs(ent->radius, ent->radius, PLAYER_STAND);
	ent->absBox.setMins(-ent->radius, -ent->radius, PLAYER_MIN);

	if (G_ValidMessage(ent))
		G_MissionAddVictoryMessage(ent->message);

	/* spawn the trigger entity */
	Edict* other = G_TriggerSpawn(ent);
	other->setTouch(G_MissionTouch);
	if (ent->target)
		ent->use = G_MissionUse;
	ent->setChild(other);

	gi.LinkEdict(ent);
}

/**
 * @brief Initializes the alien mission entity
 */
static void SP_misc_mission_aliens (Edict* ent)
{
	ent->classname = "mission";
	ent->type = ET_MISSION;
	ent->setTeam(TEAM_ALIEN);
	ent->solid = SOLID_BBOX;

	/* think function values */
	ent->think = G_MissionThink;
	ent->nextthink = 1;

	ent->absBox.setMaxs(PLAYER_WIDTH * 3, PLAYER_WIDTH * 3, PLAYER_STAND);
	ent->absBox.setMins(-(PLAYER_WIDTH * 3), -(PLAYER_WIDTH * 3), PLAYER_MIN);

	/* spawn the trigger entity */
	Edict* other = G_TriggerSpawn(ent);
	other->setTouch(G_MissionTouch);
	ent->setChild(other);

	gi.LinkEdict(ent);
}

/**
 * @brief Create a list of the grid positions the edict occupies and store it with the edict
 * @note This is only working for one z-level. But our models should be
 * split for each level anyway.
 * @param ent The edict to fill the forbidden list for
 */
static void G_BuildForbiddenListForEntity (Edict* ent)
{
	vec3_t center;
	AABB shiftedBox(ent->absBox);
	shiftedBox.getCenter(center);

	pos3_t mins, maxs, origin;
	VecToPos(shiftedBox.mins, mins);
	VecToPos(shiftedBox.maxs, maxs);
	VecToPos(center, origin);

	const int xDelta = std::max(1, maxs[0] - mins[0]);
	const int yDelta = std::max(1, maxs[1] - mins[1]);

	int size = xDelta * yDelta;
	ent->forbiddenListPos = (pos3_t*)G_TagMalloc(size * sizeof(pos3_t), TAG_LEVEL);
	ent->forbiddenListSize = size;

	for (int i = 0; i < xDelta; i++) {
		for (int j = 0; j < yDelta; j++) {
			const pos_t x = mins[0] + i;
			const pos_t y = mins[1] + j;
			const pos_t z = origin[2];
			VectorSet(ent->forbiddenListPos[i], x, y, z);
		}
	}
}

#define MISC_MODEL_SOLID (1 << 8)
/**
 * @brief Spawns a misc_model if there is a solid state
 */
static void SP_misc_model (Edict* ent)
{
	if (ent->spawnflags & MISC_MODEL_SOLID) {
		if (ent->model && ent->model[0] != '\0') {
			AABB modelAabb;
			if (gi.LoadModelAABB(ent->model, ent->frame, modelAabb)) {
				ent->classname = "model";
				ent->entBox.set(modelAabb);
				ent->type = ET_SOLID;
				ent->solid = SOLID_BBOX;
				/** @todo is fieldsize and forbidden list update really needed here? */
				ent->fieldSize = ACTOR_SIZE_NORMAL;
				gi.LinkEdict(ent);
				G_BuildForbiddenListForEntity(ent);
			} else {
				gi.DPrintf("Could not get mins/maxs for model '%s'\n", ent->model);
				G_FreeEdict(ent);
			}
		} else {
			gi.DPrintf("server_solid misc_model with no model given\n");
			G_FreeEdict(ent);
		}
	} else {
		/* handled client side */
		G_FreeEdict(ent);
	}
}

/**
 * @brief Spawns an item to the ground container
 */
static void SP_misc_item (Edict* ent)
{
	if (!ent->item) {
		gi.DPrintf("No item defined in misc_item\n");
		G_FreeEdict(ent);
		return;
	}

	G_AddItemToFloor(ent->pos, ent->item);

	/* now we can free the original edict */
	G_FreeEdict(ent);
}

static bool Message_Use (Edict* self, Edict* activator)
{
	if (!activator || !G_IsActor(activator)) {
		return false;
	} else {
		Player& player = activator->getPlayer();
		const char* msg = self->message;
		/* remove gettext marker */
		if (msg[0] == '_')
			msg++;
		G_ClientPrintf(player, PRINT_HUD, "%s", msg);

		if (self->spawnflags & 1)
			G_FreeEdict(self);

		return false;
	}
}

static void G_SpawnField (Edict* ent, const char* classname, entity_type_t type, solid_t solid)
{
	ent->classname = classname;
	ent->type = type;
	ent->fieldSize = ACTOR_SIZE_NORMAL;
	ent->solid = solid;
	ent->entBox.setMaxs(UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2);
	ent->entBox.setMins(-UNIT_SIZE / 2, -UNIT_SIZE / 2, -UNIT_HEIGHT / 2);
	ent->calcOrigin();
	ent->think = Think_SmokeAndFire;
	ent->nextthink = 1;
	ent->time = level.actualRound;
	ent->setTeam(level.activeTeam);

	gi.LinkEdict(ent);

	vec3_t particleOrigin;
	VectorCopy(ent->origin, particleOrigin);
	particleOrigin[2] -= GROUND_DELTA;
	ent->particleLink = G_SpawnParticle(particleOrigin, ent->spawnflags, ent->particle);
}

static void SP_misc_smoke (Edict* ent)
{
	G_SpawnField(ent, "smoke", ET_SMOKE, SOLID_TRIGGER);
	ent->dmgtype = gi.csi->damStunGas;
	ent->setTouch(Touch_HurtTrigger);
	G_CheckVis(nullptr);
}

static void SP_misc_fire (Edict* ent)
{
	G_SpawnField(ent, "fire", ET_FIRE, SOLID_TRIGGER);
	ent->dmgtype = gi.csi->damIncendiary;
	ent->setTouch(Touch_HurtTrigger);
}

static void SP_misc_smokestun (Edict* ent)
{
	G_SpawnField(ent, "stunsmoke", ET_SMOKESTUN, SOLID_TRIGGER);
	ent->dmgtype = gi.csi->damStunGas;
	ent->setTouch(Touch_HurtTrigger);
}

static void SP_misc_message (Edict* ent)
{
	if (!ent->message) {
		G_FreeEdict(ent);
		return;
	}

	if (!G_ValidMessage(ent))
		gi.DPrintf("No translation marker for misc_message set\n");
	ent->use = Message_Use;
	ent->classname = "misc_message";
	ent->type = ET_MESSAGE;
	ent->solid = SOLID_NOT;
}

#define CAMERA_ROTATE (1 << 8)

static void SP_misc_camera (Edict* ent)
{
	/* only used in single player */
	if (G_IsMultiPlayer()) {
		G_FreeEdict(ent);
		return;
	}

	const bool rotate = ent->spawnflags & CAMERA_ROTATE;
	G_InitCamera(ent, CAMERA_STATIONARY, ent->angle, rotate);
}

/**
 * @brief a dummy to get rid of local entities
 */
static void SP_dummy (Edict* ent)
{
	/* particles aren't client-server communicated items
	 * they are completely client side */
	G_FreeEdict(ent);
}

/**
 * @brief Spawns the world entity
 *
 * Only used for the world.
 * "sounds"	music cd track number
 * "maxlevel"	max. level to use in the map
 * "maxteams"	max team amount for multiplayergames for the current map
 */
static void SP_worldspawn (Edict* ent)
{
	ent->solid = SOLID_BSP;
	/* since the world doesn't use G_Spawn() */
	ent->inuse = true;
	ent->classname = "worldspawn";

	level.noEquipment = spawnTemp.noEquipment;
	level.noRandomSpawn = spawnTemp.noRandomSpawn;

	gi.ConfigString(CS_MAXCLIENTS, "%i", sv_maxclients->integer);

	/* only used in multi player */
	if (G_IsMultiPlayer()) {
		gi.ConfigString(CS_MAXSOLDIERSPERTEAM, "%i", sv_maxsoldiersperteam->integer);
		gi.ConfigString(CS_MAXSOLDIERSPERPLAYER, "%i", sv_maxsoldiersperplayer->integer);
		gi.ConfigString(CS_ENABLEMORALE, "%i", sv_enablemorale->integer);
		gi.ConfigString(CS_MAXTEAMS, "%s", sv_maxteams->string);
	}
}
