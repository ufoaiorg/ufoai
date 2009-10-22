/**
 * @file g_spawn.c
 * @brief Brings new objects into the world.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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
	{"func_rotating", SP_func_rotating},
	{"trigger_hurt", SP_trigger_hurt},
	{"trigger_touch", SP_trigger_touch},

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
	{"target", offsetof(edict_t, target), F_LSTRING, 0},
	{"targetname", offsetof(edict_t, targetname), F_LSTRING, 0},
	{"item", offsetof(edict_t, item), F_LSTRING, 0},
	{"noise", offsetof(edict_t, noise), F_LSTRING, 0},
	{"particle", offsetof(edict_t, particle), F_LSTRING, 0},
	{"frame", offsetof(edict_t, frame), F_INT, 0},
	{"team", offsetof(edict_t, team), F_INT, 0},
	{"group", offsetof(edict_t, group), F_LSTRING, 0},
	{"size", offsetof(edict_t, fieldSize), F_INT, 0},
	{"count", offsetof(edict_t, count), F_INT, 0},
	{"time", offsetof(edict_t, time), F_INT, 0},
	{"health", offsetof(edict_t, HP), F_INT, 0},
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

	/* need for item field in edict struct, FFL_SPAWNTEMP item will be skipped on saves */
	{"nextmap", offsetof(spawn_temp_t, nextmap), F_LSTRING, FFL_SPAWNTEMP},
	{"randomspawn", offsetof(spawn_temp_t, randomSpawn), F_INT, FFL_SPAWNTEMP},

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
		if (!strcmp(s->name, ent->classname)) {
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

	newb = G_TagMalloc(l, TAG_LEVEL);
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
 * @param[in] ent should be a properly initialized empty edict.
 */
static const char *ED_ParseEdict (const char *data, edict_t * ent)
{
	qboolean init;
	char keyname[256];

	init = qfalse;
	memset(&st, 0, sizeof(st));

	/* go through all the dictionary pairs */
	while (1) {
		/* parse key */
		const char *c = Com_Parse(&data);
		if (c[0] == '}')
			break;
		if (!data)
			gi.error("ED_ParseEntity: EOF without closing brace");

		Q_strncpyz(keyname, c, sizeof(keyname));

		/* parse value */
		c = Com_Parse(&data);
		if (!data)
			gi.error("ED_ParseEntity: EOF without closing brace");

		if (c[0] == '}')
			gi.error("ED_ParseEntity: closing brace without data");

		init = qtrue;

		/* keynames with a leading underscore are used for utility comments,
		 * and are immediately discarded by ufo */
		if (keyname[0] == '_')
			continue;

		ED_ParseField(keyname, c, ent);
	}

	if (!init)
		memset(ent, 0, sizeof(*ent));

	return data;
}

/**
 * @brief Chain together all entities with a matching team field.
 * All but the first will have the FL_GROUPSLAVE flag set.
 * All but the last will have the groupchain field set to the next one
 */
static void G_FindEdictGroups (void)
{
	edict_t *e, *e2, *chain;
	int i, j;
	int c, c2;

	c = 0;
	c2 = 0;
	for (i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++) {
		if (!e->inuse)
			continue;
		if (!e->group)
			continue;
		if (e->flags & FL_GROUPSLAVE)
			continue;
		chain = e;
		e->groupMaster = e;
		c++;
		c2++;
		for (j = i + 1, e2 = e + 1; j < globals.num_edicts; j++, e2++) {
			if (!e2->inuse)
				continue;
			if (!e2->group)
				continue;
			if (e2->flags & FL_GROUPSLAVE)
				continue;
			if (!strcmp(e->group, e2->group)) {
				c2++;
				chain->groupChain = e2;
				e2->groupMaster = e;
				chain = e2;
				e2->flags |= FL_GROUPSLAVE;
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
	edict_t *ent;
	int entnum;

	gi.FreeTags(TAG_LEVEL);

	memset(&level, 0, sizeof(level));
	memset(g_edicts, 0, game.sv_maxentities * sizeof(edict_t));

	Q_strncpyz(level.mapname, mapname, sizeof(level.mapname));
	level.day = day;

	G_ResetClientData();

	ent = NULL;
	level.activeTeam = TEAM_NO_ACTIVE;
	level.actualRound = 1;
	ai_waypointList = NULL;

	/* parse ents */
	entnum = 0;
	while (1) {
		/* parse the opening brace */
		const char *token = Com_Parse(&entities);
		if (!entities)
			break;
		if (token[0] != '{')
			gi.error("ED_LoadFromFile: found %s when expecting {", token);

		if (!ent)
			ent = g_edicts;
		else
			ent = G_Spawn();

		entities = ED_ParseEdict(entities, ent);

		ent->mapNum = entnum++;

		/* Set the position of the entity */
		VecToPos(ent->origin, ent->pos);

		/* Call this entity's specific initializer (sets ent->type) */
		ED_CallSpawn(ent);

		/* if this entity is an bbox (e.g. actor), then center its origin based on its position */
		if (ent->solid == SOLID_BBOX)
			gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
	}

	/* spawn ai players, if needed */
	if (level.num_spawnpoints[TEAM_CIVILIAN]) {
		if (AI_CreatePlayer(TEAM_CIVILIAN) == NULL)
			gi.dprintf("Could not create civilian\n");
	}

	if ((sv_maxclients->integer == 1 || ai_numactors->integer) && level.num_spawnpoints[TEAM_ALIEN]) {
		if (AI_CreatePlayer(TEAM_ALIEN) == NULL)
			gi.dprintf("Could not create alien\n");
	}

	G_FindEdictGroups();
}

/**
 * @sa G_Spawn
 */
static inline void G_InitEdict (edict_t * e)
{
	e->inuse = qtrue;
	e->classname = "noclass";
	e->number = e - g_edicts;
	e->fieldSize = ACTOR_SIZE_NORMAL;
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
	int i;
	edict_t *e;

	e = &g_edicts[1];
	for (i = 1; i < globals.num_edicts; i++, e++)
		if (!e->inuse) {
			G_InitEdict(e);
			return e;
		}

	if (i == game.sv_maxentities)
		gi.error("G_Spawn: no free edicts");

	globals.num_edicts++;
	G_InitEdict(e);
	return e;
}

/**
 * @brief Spawns a new entity at the floor
 * @note This is e.g. used to place dropped weapons/items at the floor
 */
edict_t *G_SpawnFloor (pos3_t pos)
{
	edict_t *floor;

	floor = G_Spawn();
	floor->classname = "item";
	floor->type = ET_ITEM;
	/* make sure that the item is always on a field that even the smallest actor can reach */
	floor->fieldSize = ACTOR_SIZE_NORMAL;
	VectorCopy(pos, floor->pos);
	floor->pos[2] = gi.GridFall(gi.routingMap, floor->fieldSize, floor->pos);
	gi.GridPosToVec(gi.routingMap, floor->fieldSize, floor->pos, floor->origin);
	return floor;
}

/**
 * This is only for particles that are spawned during a match - not for map particles.
 * @return A particle edict
 */
edict_t *G_ParticleSpawn (vec3_t origin, int spawnflags, const char *particle)
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
		gi.dprintf("G_ActorSpawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);

	gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);

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
		gi.dprintf("G_Actor2x2Spawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);

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
	if (gi.csi->currentMD && strcmp(gi.csi->currentMD->id, "ufocrash") == 0) {
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
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
}

/**
 * @brief Mission trigger
 * @todo use level.nextmap to spawn another map when every living actor has touched the mission trigger
 * @todo use level.actualRound to determine the 'King of the Hill' time
 * @note Don't set a client action here - otherwise the movement event might
 * be corrupted
 */
static qboolean Touch_Mission (edict_t *self, edict_t *activator)
{
	if (!self->owner)
		return qfalse;

	switch (self->owner->team) {
	case TEAM_ALIEN:
		if (activator->team == TEAM_ALIEN) {
			if (!self->count) {
				self->count = level.actualRound;
				gi.BroadcastPrintf(PRINT_HUD, _("Aliens entered target zone\n"));
			}
			return qtrue;
		} else {
			/* reset king of the hill counter */
			self->count = 0;
		}
	/* general case that also works for multiplayer teams */
	default:
		if (activator->team == self->owner->team) {
			if (!self->owner->count) {
				self->owner->count = level.actualRound;
				if (self->owner->item) {
					/* search the item in the activator's inventory */
					int j;
					invList_t *ic;

					for (j = 0; j < gi.csi->numIDs; j++)
						for (ic = activator->i.c[j]; ic; ic = ic->next) {
							objDef_t *od = ic->item.t;
							/* check whether we found the searched item in the
							 * actor's inventory */
							if (!strcmp(od->id, self->owner->item)) {
								/* drop the weapon - even if out of TUs */
								G_ActorInvMove(activator->number,
									INVDEF(j), ic, INVDEF(gi.csi->idFloor),
									NONE, NONE, qfalse, QUIET);
								gi.BroadcastPrintf(PRINT_HUD, _("Item was placed\n"));
								self->owner->count = level.actualRound;
								return qtrue;
							}
						}
				} else {
					gi.BroadcastPrintf(PRINT_HUD, _("Target zone is occupied\n"));
				}
			}
			return qtrue;
		} else {
			/* reset king of the hill counter */
			self->count = 0;
		}
	}
	return qfalse;
}

/**
 * @brief Mission trigger use function
 */
static qboolean Use_Mission (edict_t *self)
{
	edict_t *target = G_FindTargetEntity(self->target);
	if (!target) {
		gi.dprintf("Target '%s' wasn't found for misc_mission\n", self->target);
		G_FreeEdict(self);
		return qfalse;
	}

	if (target->destroy) {
		/* set this to zero to determine that this is a triggered destroy call */
		target->HP = 0;
		target->destroy(target);
		/* freed when the level changes */
		self->target = NULL;
		self->use = NULL;
	} else if (target->use)
		target->use(target);

	return qtrue;
}

/**
 * @note Think functions are only executed when level.activeTeam != -1
 * or in other word, the game has started
 */
static void Think_Mission (edict_t *self)
{
	edict_t *chain = self->groupMaster;
	edict_t *ent;
	int i, team;

	if (level.intermissionTime)
		return;

	/* when every player has joined the match - spawn the mission target
	 * particle (if given) to mark the trigger */
	if (self->particle) {
		G_ParticleSpawn(self->origin, self->spawnflags, self->particle);

		/* This is automatically freed on map shutdown */
		self->particle = NULL;
	}

	if (!chain)
		chain = self;
	while (chain) {
		if (chain->type == ET_MISSION) {
			if (chain->item) {
				const invList_t *ic;
				G_GetFloorItems(chain);
				ic = FLOOR(chain);
				if (!ic) {
					/* reset the counter if there is no item */
					chain->count = 0;
					return;
				}
				for (; ic; ic = ic->next) {
					const objDef_t *od = ic->item.t;
					assert(od);
					/* not the item we are looking for */
					if (!strcmp(od->id, chain->item))
						break;
				}
				if (!ic) {
					/* reset the counter if it's not the searched item */
					chain->count = 0;
					return;
				}
			}
			if (chain->time) {
				/* not every edict in the group chain has
				 * been occupied long enough */
				if (!chain->count || level.actualRound - chain->count < chain->time)
					return;
			}
			/* not destroyed yet */
			if ((chain->flags & FL_DESTROYABLE) && chain->HP)
				return;
		}
		chain = chain->groupChain;
	}

	if (self->use)
		self->use(self);

	team = self->team;
	chain = self->groupMaster;
	if (!chain)
		chain = self;
	while (chain) {
		/** @todo also remove the item on the floor - if any */
		/*const invList_t *ic = FLOOR(chain);*/
		ent = chain->groupChain;
		/* free the trigger */
		if (chain->child)
			G_FreeEdict(chain->child);
		/* free the group chain */
		G_FreeEdict(chain);
		chain = ent;
	}

	/* still active mission edicts left */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_MISSION) && ent->team == team) {
			return;
		}

	/* mission succeeds */
	gi.BroadcastPrintf(PRINT_HUD, _("Mission won for team %i\n"), team);

	G_MatchEndTrigger(self->team, 10);
}

/**
 * @brief Initializes the human/phalanx mission entity
 */
static void SP_misc_mission (edict_t *ent)
{
	edict_t *other;

	ent->classname = "mission";
	ent->type = ET_MISSION;

	/* maybe this was set to something else for multiplayer */
	if (!ent->team)
		ent->team = TEAM_PHALANX;

	ent->solid = SOLID_BBOX;

	if (ent->HP)
		ent->flags |= FL_DESTROYABLE;

	if (!ent->HP && !ent->time && !ent->target) {
		G_FreeEdict(ent);
		gi.dprintf("misc_mission given with no objective\n");
		return;
	}

	/* think function values */
	ent->think = Think_Mission;
	ent->nextthink = 1;

	VectorSet(ent->absmax, PLAYER_WIDTH * 3, PLAYER_WIDTH * 3, PLAYER_STAND);
	VectorSet(ent->absmin, -(PLAYER_WIDTH * 3), -(PLAYER_WIDTH * 3), PLAYER_MIN);

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = Touch_Mission;
	if (ent->target)
		ent->use = Use_Mission;
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
	ent->think = Think_Mission;
	ent->nextthink = 1;

	VectorSet(ent->absmax, PLAYER_WIDTH * 3, PLAYER_WIDTH * 3, PLAYER_STAND);
	VectorSet(ent->absmin, -(PLAYER_WIDTH * 3), -(PLAYER_WIDTH * 3), PLAYER_MIN);

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = Touch_Mission;
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
	ent->forbiddenListPos = G_TagMalloc(size * sizeof(pos3_t), TAG_LEVEL);
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

#define MISC_MODEL_SOLID 256
/**
 * @brief Spawns a misc_model if there is a solid state
 */
static void SP_misc_model (edict_t *ent)
{
	if (ent->spawnflags & MISC_MODEL_SOLID) {
		if (ent->model && *ent->model) {
			vec3_t modelMins, modelMaxs;
			if (gi.LoadModelMinsMaxs(ent->model, ent->frame, modelMins, modelMaxs)) {
				ent->classname = "model";
				VectorCopy(modelMaxs, ent->absmax);
				VectorCopy(modelMins, ent->absmin);
				ent->type = ET_SOLID;
				ent->fieldSize = ACTOR_SIZE_NORMAL;
				ent->solid = SOLID_BBOX;
				G_BuildForbiddenListForEntity(ent);
				gi.LinkEdict(ent);
			} else {
				gi.dprintf("Could not get mins/maxs for model '%s'\n", ent->model);
				G_FreeEdict(ent);
			}
		} else {
			gi.dprintf("server_solid misc_model with no model given\n");
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
	edict_t *floor;
	item_t item = {NONE_AMMO, NULL, NULL, 0, 0};
	objDef_t *od;

	if (!ent->item) {
		gi.dprintf("No item defined in misc_item\n");
		G_FreeEdict(ent);
		return;
	}

	od = INVSH_GetItemByIDSilent(ent->item);
	if (!od) {
		gi.dprintf("Could not find item '%s' for misc_item\n", ent->item);
		G_FreeEdict(ent);
		return;
	}

	/* Also sets FLOOR(ent) to correct value. */
	floor = G_GetFloorItems(ent);
	/* nothing on the ground yet? */
	if (!floor)
		floor = G_SpawnFloor(ent->pos);

	item.t = od;
	Com_TryAddToInventory(&floor->i, item, INVDEF(gi.csi->idFloor));

	/* now we can free the original edict */
	G_FreeEdict(ent);
}

/**
 * @brief a dummy to get rid of local entities
 */
static void SP_dummy (edict_t *ent)
{
	/* particles aren't client-server communicated items */
	/* they are completely client side */
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

	if (st.nextmap)
		Q_strncpyz(level.nextmap, st.nextmap, sizeof(level.nextmap));
	level.randomSpawn = st.randomSpawn;

	gi.ConfigString(CS_MAXCLIENTS, "%i", sv_maxclients->integer);

	/* only used in multi player */
	if (sv_maxclients->integer >= 2) {
		gi.ConfigString(CS_MAXSOLDIERSPERTEAM, "%i", sv_maxsoldiersperteam->integer);
		gi.ConfigString(CS_MAXSOLDIERSPERPLAYER, "%i", sv_maxsoldiersperplayer->integer);
		gi.ConfigString(CS_ENABLEMORALE, "%i", sv_enablemorale->integer);
		if (gi.csi->currentMD) {
			gi.ConfigString(CS_MAXTEAMS, "%i", gi.csi->currentMD->teams);
			gi.Cvar_Set("sv_maxteams", va("%i", gi.csi->currentMD->teams));
		} else
			gi.ConfigString(CS_MAXTEAMS, "%s", sv_maxteams->string);
	}
}
