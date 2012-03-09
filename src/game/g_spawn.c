/**
 * @file g_spawn.c
 * @brief Brings new objects into the world.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "g_local.h"
#include "../shared/parse.h"

static spawn_temp_t st;

static void SP_light(edict_t *ent);
static void SP_dummy(edict_t *ent);
static void SP_player_start(edict_t *ent);
static void SP_human_start(edict_t *ent);
static void SP_alien_start(edict_t *ent);
static void SP_civilian_start(edict_t *ent);
static void SP_worldspawn(edict_t *ent);
static void SP_2x2_start(edict_t *ent);
static void SP_civilian_target(edict_t *ent);
static void SP_misc_model(edict_t *ent);
static void SP_misc_item(edict_t *ent);
static void SP_misc_mission(edict_t *ent);
static void SP_misc_mission_aliens(edict_t *ent);
static void SP_misc_message(edict_t *ent);
static void SP_misc_smoke(edict_t *ent);
static void SP_misc_fire(edict_t *ent);

typedef struct {
	const char *name;
	void (*spawn) (edict_t * ent);
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

	{NULL, NULL}
};

typedef enum {
	F_INT,
	F_FLOAT,
	F_LSTRING,					/* string on disk, pointer in memory, TAG_LEVEL */
	F_VECTOR,
	F_IGNORE
} fieldtype_t;

typedef struct {
	const char *name;
	size_t ofs;
	fieldtype_t type;
	int flags;
} field_t;

static const field_t fields[] = {
	{"classname", offsetof(edict_t, classname), F_LSTRING, 0},
	{"model", offsetof(edict_t, model), F_LSTRING, 0},
	{"spawnflags", offsetof(edict_t, spawnflags), F_INT, 0},
	{"speed", offsetof(edict_t, speed), F_INT, 0},
	{"dir", offsetof(edict_t, dir), F_INT, 0},
	{"target", offsetof(edict_t, target), F_LSTRING, 0},
	{"targetname", offsetof(edict_t, targetname), F_LSTRING, 0},
	{"item", offsetof(edict_t, item), F_LSTRING, 0},
	{"noise", offsetof(edict_t, noise), F_LSTRING, 0},
	{"particle", offsetof(edict_t, particle), F_LSTRING, 0},
	{"nextmap", offsetof(edict_t, nextmap), F_LSTRING, 0},
	{"frame", offsetof(edict_t, frame), F_INT, 0},
	{"team", offsetof(edict_t, team), F_INT, 0},
	{"group", offsetof(edict_t, group), F_LSTRING, 0},
	{"size", offsetof(edict_t, fieldSize), F_INT, 0},
	{"count", offsetof(edict_t, count), F_INT, 0},
	{"time", offsetof(edict_t, time), F_INT, 0},
	{"health", offsetof(edict_t, HP), F_INT, 0},
	{"radius", offsetof(edict_t, radius), F_INT, 0},
	{"sounds", offsetof(edict_t, sounds), F_INT, 0},
	{"material", offsetof(edict_t, material), F_INT, 0},
	{"light", 0, F_IGNORE, 0},
	/** @todo This (maxteams) should also be handled server side - currently this is
	 * only done client side */
	{"maxteams", 0, F_IGNORE, 0},
	{"maxlevel", 0, F_IGNORE, 0},
	{"dmg", offsetof(edict_t, dmg), F_INT, 0},
	{"origin", offsetof(edict_t, origin), F_VECTOR, 0},
	{"angles", offsetof(edict_t, angles), F_VECTOR, 0},
	{"angle", offsetof(edict_t, angle), F_FLOAT, 0},
	{"message", offsetof(edict_t, message), F_LSTRING, 0},

	{"randomspawn", offsetof(spawn_temp_t, randomSpawn), F_INT, FFL_SPAWNTEMP},
	{"noequipment", offsetof(spawn_temp_t, noEquipment), F_INT, FFL_SPAWNTEMP},

	{0, 0, 0, 0}
};

/**
 * @brief Finds the spawn function for the entity and calls it
 */
static void ED_CallSpawn (edict_t * ent)
{
	const spawn_t *s;

	if (!ent->classname)
		return;

	/* check normal spawn functions */
	for (s = spawns; s->name; s++) {
		/* found it */
		if (Q_streq(s->name, ent->classname)) {
			s->spawn(ent);
			return;
		}
	}

	ent->inuse = qfalse;
}

/**
 * @brief Allocated memory for the given string in the level context (TAG_LEVEL)
 * @note This memory is automatically freed when we close or change the level
 * @param[in] string The string to copy
 */
static char *ED_NewString (const char *string)
{
	char *newb, *new_p;
	int i;
	const size_t l = strlen(string) + 1;

	newb = (char*)G_TagMalloc(l, TAG_LEVEL);
	new_p = newb;

	for (i = 0; i < l; i++) {
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
static void ED_ParseField (const char *key, const char *value, edict_t * ent)
{
	const field_t *f;
	byte *b;
	vec3_t vec;

	for (f = fields; f->name; f++) {
		if (!(f->flags & FFL_NOSPAWN) && !Q_strcasecmp(f->name, key)) {
			/* found it */
			if (f->flags & FFL_SPAWNTEMP)
				b = (byte *) & st;
			else
				b = (byte *) ent;

			switch (f->type) {
			case F_LSTRING:
				*(char **) (b + f->ofs) = ED_NewString(value);
				break;
			case F_VECTOR:
				sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *) (b + f->ofs))[0] = vec[0];
				((float *) (b + f->ofs))[1] = vec[1];
				((float *) (b + f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *) (b + f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *) (b + f->ofs) = atof(value);
				break;
			case F_IGNORE:
				break;
			}
			return;
		}
	}
}

/**
 * @brief Parses an edict out of the given string, returning the new position
 * @param[in] data The string to parse from
 * @param[in] ent should be a properly initialized empty edict.
 */
static const char *ED_ParseEdict (const char *data, edict_t * ent)
{
	qboolean init;
	char keyname[MAX_VAR];

	init = qfalse;
	OBJZERO(st);

	/* go through all the dictionary pairs */
	while (1) {
		/* parse key */
		const char *c = Com_Parse(&data);
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

		init = qtrue;

		/* keynames with a leading underscore are used for utility comments,
		 * and are immediately discarded by ufo */
		if (keyname[0] == '_')
			continue;

		ED_ParseField(keyname, c, ent);
	}

	if (!init)
		OBJZERO(*ent);

	return data;
}

/**
 * @brief Chain together all entities with a matching team field.
 * All but the first will have the FL_GROUPSLAVE flag set.
 * All but the last will have the groupchain field set to the next one
 */
static void G_FindEdictGroups (void)
{
	edict_t *ent = G_EdictsGetFirst(); /* the first edict is always a world edict that can be skipped */

	while ((ent = G_EdictsGetNextInUse(ent))) {
		edict_t *ent2, *chain;

		if (!ent->group)
			continue;
		if (ent->flags & FL_GROUPSLAVE)
			continue;
		chain = ent;
		ent->groupMaster = ent;
		ent2 = ent;			/* search only the remainder of the entities */
		while ((ent2 = G_EdictsGetNextInUse(ent2))) {
			if (!ent2->group)
				continue;
			if (ent2->flags & FL_GROUPSLAVE)
				continue;
			if (Q_streq(ent->group, ent2->group)) {
				chain->groupChain = ent2;
				ent2->groupMaster = ent;
				chain = ent2;
				ent2->flags |= FL_GROUPSLAVE;
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
void G_SpawnEntities (const char *mapname, qboolean day, const char *entities)
{
	int entnum;

	G_FreeTags(TAG_LEVEL);

	OBJZERO(level);
	level.pathingMap = (pathing_t *)G_TagMalloc(sizeof(*level.pathingMap), TAG_LEVEL);

	G_EdictsReset();

	/* initialize reactionFire data */
	G_ReactionFireTargetsInit();

	Q_strncpyz(level.mapname, mapname, sizeof(level.mapname));
	level.day = day;

	G_ResetClientData();

	level.activeTeam = TEAM_NO_ACTIVE;
	level.actualRound = 1;
	level.hurtAliens = sv_hurtaliens->integer;
	ai_waypointList = NULL;

	/* parse ents */
	entnum = 0;
	while (1) {
		edict_t *ent;
		/* parse the opening brace */
		const char *token = Com_Parse(&entities);
		if (!entities)
			break;
		if (token[0] != '{')
			gi.Error("ED_LoadFromFile: found %s when expecting {", token);

		ent = G_Spawn();

		entities = ED_ParseEdict(entities, ent);

		ent->mapNum = entnum++;

		/* Set the position of the entity */
		VecToPos(ent->origin, ent->pos);

		/* Call this entity's specific initializer (sets ent->type) */
		ED_CallSpawn(ent);

		/* if this entity is an bbox (e.g. actor), then center its origin based on its position */
		if (ent->solid == SOLID_BBOX)
			G_EdictCalcOrigin(ent);
	}

	/* spawn ai players, if needed */
	if (level.num_spawnpoints[TEAM_CIVILIAN]) {
		if (AI_CreatePlayer(TEAM_CIVILIAN) == NULL)
			gi.DPrintf("Could not create civilian\n");
	}

	if ((sv_maxclients->integer == 1 || ai_numactors->integer) && level.num_spawnpoints[TEAM_ALIEN]) {
		if (AI_CreatePlayer(TEAM_ALIEN) == NULL)
			gi.DPrintf("Could not create alien\n");
	}

	Com_Printf("Used inventory slots after ai spawn: %i\n", game.i.GetUsedSlots(&game.i));

	G_FindEdictGroups();
}

/**
 * @sa G_Spawn
 */
static inline void G_InitEdict (edict_t * ent)
{
	ent->inuse = qtrue;
	ent->classname = "noclass";
	ent->number = G_EdictsGetNumber(ent);
	ent->fieldSize = ACTOR_SIZE_NORMAL;
}

/**
 * @brief Either finds a free edict, or allocates a new one.
 * @note Try to avoid reusing an entity that was recently freed, because it
 * can cause the player to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 * @sa G_InitEdict
 * @sa G_FreeEdict
 */
edict_t *G_Spawn (void)
{
	edict_t *ent = G_EdictsGetNewEdict();

	if (!ent)
		gi.Error("G_Spawn: no free edicts");

	G_InitEdict(ent);
	return ent;
}

static void Think_SmokeAndFire (edict_t *self)
{
	if (self->time + self->count <= level.actualRound) {
		G_EventEdictPerish(G_VisToPM(self->particleLink->visflags), self->particleLink);
		G_FreeEdict(self->particleLink);
		G_FreeEdict(self);
	}
}

static void G_SpawnSmoke (const vec3_t vec, const char *particle, int rounds)
{
	pos3_t pos;
	edict_t *ent;

	VecToPos(vec, pos);

	ent = G_GetEdictFromPos(pos, ET_SMOKE);
	if (ent == NULL) {
		pos_t z = gi.GridFall(gi.routingMap, ACTOR_SIZE_NORMAL, pos);
		if (z != pos[2])
			return;

		ent = G_Spawn();
		VectorCopy(pos, ent->pos);
		G_EdictCalcOrigin(ent);
		ent->spawnflags = G_GetLevelFlagsFromPos(pos);
		ent->particle = particle;
		SP_misc_smoke(ent);
	}

	ent->count = rounds;
}

/**
 * @brief Spawns a smoke field that is available for some rounds
 * @param[in] vec The position in the world that is the center of the smoke field
 */
void G_SpawnSmokeField (const vec3_t vec, const char *particle, int rounds)
{
	int i;

	G_SpawnSmoke(vec, particle, rounds);

	for (i = 0; i < DIRECTIONS; i++) {
		vec3_t end;
		trace_t tr;

		VectorSet(end, vec[0] + dvecs[i][0] * UNIT_SIZE, vec[1] + dvecs[i][1] * UNIT_SIZE, vec[2]);

		tr = G_Trace(vec, end, NULL, MASK_SMOKE_AND_FIRE);
		/* trace didn't reach the target - something was hit before */
		if (tr.fraction < 1.0 || (tr.contentFlags & CONTENTS_WATER)) {
			continue;
		}
		G_SpawnSmoke(end, particle, rounds);
	}
}

static void G_SpawnFire (const vec3_t vec, const char *particle, int rounds, int damage)
{
	pos3_t pos;
	edict_t *ent;

	VecToPos(vec, pos);

	ent = G_GetEdictFromPos(pos, ET_FIRE);
	if (ent == NULL) {
		pos_t z = gi.GridFall(gi.routingMap, ACTOR_SIZE_NORMAL, pos);
		if (z != pos[2])
			return;

		ent = G_Spawn();
		VectorCopy(pos, ent->pos);
		VectorCopy(vec, ent->origin);
		ent->dmg = damage;
		ent->particle = particle;
		ent->spawnflags = G_GetLevelFlagsFromPos(pos);
		SP_misc_fire(ent);
	}

	ent->count = rounds;
}

void G_SpawnFireField (const vec3_t vec, const char *particle, int rounds, int damage)
{
	int i;

	G_SpawnFire(vec, particle, rounds, damage);

	for (i = 0; i < DIRECTIONS; i++) {
		vec3_t end;
		trace_t tr;

		VectorSet(end, vec[0] + dvecs[i][0] * UNIT_SIZE, vec[1] + dvecs[i][1] * UNIT_SIZE, vec[2]);

		tr = G_Trace(vec, end, NULL, MASK_SMOKE_AND_FIRE);
		/* trace didn't reach the target - something was hit before */
		if (tr.fraction < 1.0 || (tr.contentFlags & CONTENTS_WATER)) {
			continue;
		}

		G_SpawnFire(end, particle, rounds, damage);
	}
}

/**
 * @brief Spawns a new entity at the floor
 * @note This is e.g. used to place dropped weapons/items at the floor
 */
edict_t *G_SpawnFloor (const pos3_t pos)
{
	edict_t *floor;

	floor = G_Spawn();
	floor->classname = "item";
	floor->type = ET_ITEM;
	/* make sure that the item is always on a field that even the smallest actor can reach */
	floor->fieldSize = ACTOR_SIZE_NORMAL;
	VectorCopy(pos, floor->pos);
	floor->pos[2] = gi.GridFall(gi.routingMap, floor->fieldSize, floor->pos);
	G_EdictCalcOrigin(floor);
	return floor;
}

/**
 * This is only for particles that are spawned during a match - not for map particles.
 * @return A particle edict
 */
edict_t *G_SpawnParticle (const vec3_t origin, int spawnflags, const char *particle)
{
	edict_t *ent = G_Spawn();
	ent->classname = "particle";
	ent->type = ET_PARTICLE;
	VectorCopy(origin, ent->origin);

	/* Set the position of the entity */
	VecToPos(ent->origin, ent->pos);

	ent->particle = particle;
	ent->spawnflags = spawnflags;

	G_CheckVis(ent, qtrue);

	return ent;
}

/**
 * @brief Spawn a 1x1 unit.
 */
static void G_ActorSpawn (edict_t *ent)
{
	/* set properties */
	level.num_spawnpoints[ent->team]++;
	ent->classname = "actor";
	ent->type = ET_ACTORSPAWN;
	ent->fieldSize = ACTOR_SIZE_NORMAL;

	/* Fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;

	ent->pos[2] = gi.GridFall(gi.routingMap, ent->fieldSize, ent->pos);
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		gi.DPrintf("G_ActorSpawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);

	G_EdictCalcOrigin(ent);

	/* link it for collision detection */
	ent->dir = AngleToDir(ent->angle);
	assert(ent->dir < CORE_DIRECTIONS);
	ent->solid = SOLID_BBOX;

	/* Set bounding box. Maybe this is already set in one of the spawn functions? */
	if (ent->maxs[0] == 0)
		VectorSet(ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND);
	if (ent->mins[0] == 0)
		VectorSet(ent->mins, -PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN);
}

/**
 * @brief Spawn a singleplayer 2x2 unit.
 */
static void G_Actor2x2Spawn (edict_t *ent)
{
	/* set properties */
	level.num_2x2spawnpoints[ent->team]++;
	ent->classname = "ugv";
	ent->type = ET_ACTOR2x2SPAWN;
	ent->fieldSize = ACTOR_SIZE_2x2;

	/* Fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;
	ent->pos[2] = gi.GridFall(gi.routingMap, ent->fieldSize, ent->pos);
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		gi.DPrintf("G_Actor2x2Spawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);
	G_EdictCalcOrigin(ent);

	/* link it for collision detection */
	ent->dir = AngleToDir(ent->angle);
	assert(ent->dir < CORE_DIRECTIONS);
	ent->solid = SOLID_BBOX;

	/* Set bounding box. Maybe this is already set in one of the spawn functions? */
	if (ent->maxs[0] == 0)
		VectorSet(ent->maxs, PLAYER2x2_WIDTH, PLAYER2x2_WIDTH, PLAYER_STAND);
	if (ent->mins[0] == 0)
		VectorSet(ent->mins, -PLAYER2x2_WIDTH, -PLAYER2x2_WIDTH, PLAYER_MIN);
}

/**
 * @brief light (0 1 0) (-8 -8 -8) (8 8 8)
 */
static void SP_light (edict_t *ent)
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
static void SP_player_start (edict_t *ent)
{
	/* only used in multi player */
	if (sv_maxclients->integer == 1) {
		G_FreeEdict(ent);
		return;
	}

	/** @todo Wrong place here */
	/* maybe there are already the max soldiers allowed per team connected */
	if (sv_maxsoldiersperteam->integer > level.num_spawnpoints[ent->team]) {
		ent->STUN = 0;
		ent->HP = MAX_HP;
		G_ActorSpawn(ent);
	} else
		G_FreeEdict(ent);
}

/**
 * @brief info_human_start (1 0 0) (-16 -16 -24) (16 16 32)
 * Starting point for a single player human.
 */
static void SP_human_start (edict_t *ent)
{
	/* only used in single player */
	if (sv_maxclients->integer > 1) {
		G_FreeEdict(ent);
		return;
	}
	ent->team = TEAM_PHALANX;
	ent->STUN = 0;
	ent->HP = MAX_HP;
	G_ActorSpawn(ent);
}


/**
 * @brief info_2x2_start (1 1 0) (-32 -32 -24) (32 32 32)
 * Starting point for a 2x2 unit.
 */
static void SP_2x2_start (edict_t *ent)
{
	/* no 2x2 unit in multiplayer */
	if (sv_maxclients->integer > 1) {
		G_FreeEdict(ent);
		return;
	}
	/* set stats */
	ent->STUN = 0;
	ent->HP = MAX_HP;

	if (!ent->team)
		ent->team = TEAM_PHALANX;

	/* these units are bigger */
	VectorSet(ent->maxs, PLAYER_WIDTH * 2, PLAYER_WIDTH * 2, PLAYER_STAND);
	VectorSet(ent->mins, -(PLAYER_WIDTH * 2), -(PLAYER_WIDTH * 2), PLAYER_MIN);

	/* spawn singleplayer 2x2 unit */
	G_Actor2x2Spawn(ent);
}

/**
 * @brief info_alien_start (1 0 0) (-16 -16 -24) (16 16 32)
 * Starting point for a single player alien.
 */
static void SP_alien_start (edict_t *ent)
{
	/* deactivateable in multiplayer */
	if (sv_maxclients->integer > 1 && !ai_numactors->integer) {
		G_FreeEdict(ent);
		return;
	}
	ent->team = TEAM_ALIEN;
	/* set stats */
	ent->STUN = 0;
	ent->HP = MAX_HP;
	/* hurt aliens in ufo crash missions (5%: almost dead, 15%: wounded, 30%: stunned)  */
	if (level.hurtAliens) {
		const float random = frand();
		if (random <= .05f) {
			ent->STUN = 50;
			ent->HP = 5;
		} else if (random <= .15f) {
			ent->STUN = 30;
			ent->HP /= 2;
		} else if (random <= .3f) {
			ent->STUN = 75;
		}
	}

	G_ActorSpawn(ent);
}


/**
 * @brief info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
 * Starting point for a civilian.
 */
static void SP_civilian_start (edict_t *ent)
{
	/* deactivateable in multiplayer */
	if (sv_maxclients->integer > 1 && !ai_numcivilians->integer) {
		G_FreeEdict(ent);
		return;
	}
	ent->team = TEAM_CIVILIAN;
	/* set stats */
	ent->STUN = 99;	/** @todo Does anybody know _why_ this is set to 99? */
	ent->HP = MAX_HP;
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
static void SP_civilian_target (edict_t *ent)
{
	/* target point for which team */
	ent->team = TEAM_CIVILIAN;
	ent->classname = "civtarget";
	ent->type = ET_CIVILIANTARGET;
	ent->fieldSize = ACTOR_SIZE_NORMAL; /* to let the grid fall function work */

	/* add the edict to the list of known waypoints */
	G_AddToWayPointList(ent);

	/* fall to ground */
	if (ent->pos[2] >= PATHFINDING_HEIGHT)
		ent->pos[2] = PATHFINDING_HEIGHT - 1;
	ent->pos[2] = gi.GridFall(gi.routingMap, ent->fieldSize, ent->pos);
	G_EdictCalcOrigin(ent);
}

/**
 * @brief Initializes the human/phalanx mission entity
 */
static void SP_misc_mission (edict_t *ent)
{
	edict_t *other;

	ent->classname = "misc_mission";
	ent->type = ET_MISSION;

	/* maybe this was set to something else for multiplayer */
	if (!ent->team)
		ent->team = TEAM_PHALANX;

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
	VectorSet(ent->absmax, ent->radius, ent->radius, PLAYER_STAND);
	VectorSet(ent->absmin, -ent->radius, -ent->radius, PLAYER_MIN);

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = G_MissionTouch;
	if (ent->target)
		ent->use = G_MissionUse;
	ent->child = other;

	gi.LinkEdict(ent);
}

/**
 * @brief Initializes the alien mission entity
 */
static void SP_misc_mission_aliens (edict_t *ent)
{
	edict_t *other;

	ent->classname = "mission";
	ent->type = ET_MISSION;
	ent->team = TEAM_ALIEN;
	ent->solid = SOLID_BBOX;

	/* think function values */
	ent->think = G_MissionThink;
	ent->nextthink = 1;

	VectorSet(ent->absmax, PLAYER_WIDTH * 3, PLAYER_WIDTH * 3, PLAYER_STAND);
	VectorSet(ent->absmin, -(PLAYER_WIDTH * 3), -(PLAYER_WIDTH * 3), PLAYER_MIN);

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = G_MissionTouch;
	ent->child = other;

	gi.LinkEdict(ent);
}

/**
 * @note This is only working for one z-level. But our models should be
 * split for each level anyway.
 * @param ent The edict to fill the forbidden list for
 */
static void G_BuildForbiddenListForEntity (edict_t *ent)
{
	pos3_t mins, maxs, origin;
	vec3_t center, shiftedMins, shiftedMaxs;
	int xDelta, yDelta, size, i, j;

	VectorAdd(ent->absmin, ent->origin, shiftedMins);
	VectorAdd(ent->absmax, ent->origin, shiftedMaxs);

	VectorCenterFromMinsMaxs(shiftedMins, shiftedMaxs, center);
	VecToPos(shiftedMins, mins);
	VecToPos(shiftedMaxs, maxs);
	VecToPos(center, origin);

	xDelta = max(1, maxs[0] - mins[0]);
	yDelta = max(1, maxs[1] - mins[1]);

	size = xDelta * yDelta;
	ent->forbiddenListPos = (pos3_t *)G_TagMalloc(size * sizeof(pos3_t), TAG_LEVEL);
	ent->forbiddenListSize = size;

	for (i = 0; i < xDelta; i++) {
		for (j = 0; j < yDelta; j++) {
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
static void SP_misc_model (edict_t *ent)
{
	if (ent->spawnflags & MISC_MODEL_SOLID) {
		if (ent->model && ent->model[0] != '\0') {
			vec3_t modelMins, modelMaxs;
			if (gi.LoadModelMinsMaxs(ent->model, ent->frame, modelMins, modelMaxs)) {
				ent->classname = "model";
				VectorCopy(modelMaxs, ent->absmax);
				VectorCopy(modelMins, ent->absmin);
				ent->type = ET_SOLID;
				ent->solid = SOLID_BBOX;
				/** @todo is fieldsize and forbidden list update really needed here? */
				ent->fieldSize = ACTOR_SIZE_NORMAL;
				G_BuildForbiddenListForEntity(ent);
				gi.LinkEdict(ent);
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
static void SP_misc_item (edict_t *ent)
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

static qboolean Message_Use (edict_t *self, edict_t *activator)
{
	if (!activator || !G_IsActor(activator)) {
		return qfalse;
	} else {
		player_t *player = G_PLAYER_FROM_ENT(activator);
		const char *msg = self->message;
		/* remove gettext marker */
		if (msg[0] == '_')
			msg++;
		G_ClientPrintf(player, PRINT_HUD, "%s", msg);

		if (self->spawnflags & 1)
			G_FreeEdict(self);

		return qfalse;
	}
}

static void G_SpawnField (edict_t *ent, const char *classname, entity_type_t type, solid_t solid)
{
	vec3_t particleOrigin;

	ent->classname = classname;
	ent->type = type;
	ent->fieldSize = ACTOR_SIZE_NORMAL;
	ent->solid = solid;
	VectorSet(ent->maxs, UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2);
	VectorSet(ent->mins, -UNIT_SIZE / 2, -UNIT_SIZE / 2, -UNIT_HEIGHT / 2);
	G_EdictCalcOrigin(ent);
	ent->think = Think_SmokeAndFire;
	ent->nextthink = 1;
	ent->time = level.actualRound;

	gi.LinkEdict(ent);

	VectorCopy(ent->origin, particleOrigin);
	particleOrigin[2] -= GROUND_DELTA;
	ent->particleLink = G_SpawnParticle(particleOrigin, ent->spawnflags, ent->particle);
}

static void SP_misc_smoke (edict_t *ent)
{
	G_SpawnField(ent, "smoke", ET_SMOKE, SOLID_NOT);
}

static void SP_misc_fire (edict_t *ent)
{
	G_SpawnField(ent, "fire", ET_FIRE, SOLID_BBOX);
	ent->touch = Touch_HurtTrigger;
}

static void SP_misc_message (edict_t *ent)
{
	if (!ent->message) {
		G_FreeEdict(ent);
		return;
	}

	if (ent->message[0] != '_')
		gi.DPrintf("No translation marker for misc_message set\n");
	ent->use = Message_Use;
	ent->classname = "misc_message";
	ent->type = ET_MESSAGE;
	ent->solid = SOLID_NOT;
}

/**
 * @brief a dummy to get rid of local entities
 */
static void SP_dummy (edict_t *ent)
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
static void SP_worldspawn (edict_t *ent)
{
	ent->solid = SOLID_BSP;
	/* since the world doesn't use G_Spawn() */
	ent->inuse = qtrue;
	ent->classname = "worldspawn";

	level.noEquipment = st.noEquipment;
	level.randomSpawn = st.randomSpawn;

	gi.ConfigString(CS_MAXCLIENTS, "%i", sv_maxclients->integer);

	/* only used in multi player */
	if (sv_maxclients->integer >= 2) {
		gi.ConfigString(CS_MAXSOLDIERSPERTEAM, "%i", sv_maxsoldiersperteam->integer);
		gi.ConfigString(CS_MAXSOLDIERSPERPLAYER, "%i", sv_maxsoldiersperplayer->integer);
		gi.ConfigString(CS_ENABLEMORALE, "%i", sv_enablemorale->integer);
		gi.ConfigString(CS_MAXTEAMS, "%s", sv_maxteams->string);
	}
}
