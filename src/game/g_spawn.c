/**
 * @file g_spawn.c
 * @brief Brings new objects into the world.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

spawn_temp_t st;

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
static void SP_misc_mission(edict_t *ent);
static void SP_misc_mission_aliens(edict_t *ent);

typedef struct {
	const char *name;
	void (*spawn) (edict_t * ent);
} spawn_t;

static const spawn_t spawns[] = {
	{"worldspawn", SP_worldspawn},
	{"light", SP_light},
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
	{"info_ugv_start", SP_2x2_start}, /* FIXME: Remove me */
	{"info_null", SP_dummy},
	{"func_breakable", SP_func_breakable},
	{"func_door", SP_func_door},
	{"func_rotating", SP_func_rotating},
	{"trigger_hurt", SP_trigger_hurt},

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
	{"particle", offsetof(edict_t, particle), F_LSTRING, 0},
	{"team", offsetof(edict_t, team), F_INT, 0},
	{"size", offsetof(edict_t, fieldSize), F_INT, 0},
	{"wait", offsetof(edict_t, wait), F_FLOAT, 0},
	{"delay", offsetof(edict_t, delay), F_FLOAT, 0},
	{"random", offsetof(edict_t, random), F_FLOAT, 0},
	{"count", offsetof(edict_t, count), F_INT, 0},
	{"time", offsetof(edict_t, time), F_INT, 0},
	{"health", offsetof(edict_t, HP), F_INT, 0},
	{"sounds", offsetof(edict_t, sounds), F_INT, 0},
	{"material", offsetof(edict_t, material), F_INT, 0},
	{"light", 0, F_IGNORE, 0},
	{"dmg", offsetof(edict_t, dmg), F_INT, 0},
	{"origin", offsetof(edict_t, origin), F_VECTOR, 0},
	{"angles", offsetof(edict_t, angles), F_VECTOR, 0},
	{"angle", offsetof(edict_t, angle), F_FLOAT, 0},

	/* need for item field in edict struct, FFL_SPAWNTEMP item will be skipped on saves */
	{"nextmap", offsetof(spawn_temp_t, nextmap), F_LSTRING, FFL_SPAWNTEMP},

	{0, 0, 0, 0}
};

/**
 * @brief Finds the spawn function for the entity and calls it
 */
static void ED_CallSpawn (edict_t * ent)
{
	const spawn_t *s;

	if (!ent->classname) {
		Com_DPrintf(DEBUG_GAME, "ED_CallSpawn: NULL classname\n");
		return;
	}

	/* check normal spawn functions */
	for (s = spawns; s->name; s++) {
		/* found it */
		if (!Q_strcmp(s->name, ent->classname)) {
			s->spawn(ent);
			return;
		}
	}

	Com_DPrintf(DEBUG_GAME, "%s doesn't have a spawn function\n", ent->classname);
	ent->inuse = qfalse;
}

static char *ED_NewString (const char *string)
{
	char *newb, *new_p;
	int i;
	size_t l;

	l = strlen(string) + 1;

	newb = gi.TagMalloc(l, TAG_LEVEL);

	new_p = newb;

	for (i = 0; i < l; i++) {
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
		if (!(f->flags & FFL_NOSPAWN) && !Q_stricmp(f->name, key)) {
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
	Com_DPrintf(DEBUG_GAME, "ED_ParseField: %s is not a valid field\n", key);
}

/**
 * @brief Parses an edict out of the given string, returning the new position
 * @param[in] ent should be a properly initialized empty edict.
 */
static const char *ED_ParseEdict (const char *data, edict_t * ent)
{
	qboolean init;
	char keyname[256];
	const char *com_token;

	init = qfalse;
	memset(&st, 0, sizeof(st));

	/* go through all the dictionary pairs */
	while (1) {
		/* parse key */
		com_token = COM_Parse(&data);
		if (com_token[0] == '}')
			break;
		if (!data)
			gi.error("ED_ParseEntity: EOF without closing brace");

		Q_strncpyz(keyname, com_token, sizeof(keyname));

		/* parse value */
		com_token = COM_Parse(&data);
		if (!data)
			gi.error("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			gi.error("ED_ParseEntity: closing brace without data");

		init = qtrue;

		/* keynames with a leading underscore are used for utility comments, */
		/* and are immediately discarded by ufo */
		if (keyname[0] == '_')
			continue;

		ED_ParseField(keyname, com_token, ent);
	}

	if (!init)
		memset(ent, 0, sizeof(*ent));

	return data;
}

/**
 * @brief Creates a server's entity / program execution context
 * by parsing textual entity definitions out of an ent file.
 * @sa CM_EntityString
 * @sa SV_SpawnServer
 */
void G_SpawnEntities (const char *mapname, const char *entities)
{
	edict_t *ent;
	int entnum;
	const char *com_token;

	gi.FreeTags(TAG_LEVEL);

	memset(&level, 0, sizeof(level));
	memset(g_edicts, 0, game.sv_maxentities * sizeof(edict_t));

	Q_strncpyz(level.mapname, mapname, sizeof(level.mapname));

	G_ResetClientData();

	ent = NULL;
	level.activeTeam = -1;

	/* parse ents */
	entnum = 0;
	while (1) {
		/* parse the opening brace */
		com_token = COM_Parse(&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.error("ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = g_edicts;
		else
			ent = G_Spawn();
		entities = ED_ParseEdict(entities, ent);

		VecToPos(ent->origin, ent->pos);
		gi.GridPosToVec(gi.routingMap, ent->pos, ent->origin);

		ent->mapNum = entnum++;
		ED_CallSpawn(ent);
	}

	/* spawn ai players, if needed */
	if (level.num_spawnpoints[TEAM_CIVILIAN]) {
		if (AI_CreatePlayer(TEAM_CIVILIAN) == NULL)
			Com_Printf("Could not create civilian\n");
	}
#ifdef DEBUG
	else
		Com_Printf("No civilian spawn points in this map or civilians deactivated\n");
#endif

	if ((sv_maxclients->integer == 1 || ai_numactors->integer) && level.num_spawnpoints[TEAM_ALIEN]) {
		if (AI_CreatePlayer(TEAM_ALIEN) == NULL)
			Com_Printf("Could not create alien\n");
#ifdef DEBUG
	} else {
		Com_Printf("No alien spawn points in this map or aliens are deactivated for multiplayer\n");
		Com_Printf("(sv_maxclients %i, ai_numactors: %i, alien spawnpoints: %i)\n", sv_maxclients->integer, ai_numactors->integer, level.num_spawnpoints[TEAM_ALIEN]);
#endif
	}
}

/**
 * @sa G_Spawn
 */
static inline void G_InitEdict (edict_t * e)
{
	e->inuse = qtrue;
	e->classname = "noclass";
	e->number = e - g_edicts;
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
	if (ent->pos[2] >= HEIGHT)
		ent->pos[2] = HEIGHT - 1;

	ent->pos[2] = gi.GridFall(gi.routingMap, ent->pos, ent->fieldSize);
	if (ent->pos[2] >= HEIGHT)
		Com_Printf("G_ActorSpawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);

	gi.GridPosToVec(gi.routingMap, ent->pos, ent->origin);

	/* link it for collision detection */
	ent->dir = AngleToDV(ent->angle);
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
	if (ent->pos[2] >= HEIGHT)
		ent->pos[2] = HEIGHT - 1;
	ent->pos[2] = gi.GridFall(gi.routingMap, ent->pos, ent->fieldSize);
	if (ent->pos[2] >= HEIGHT)
		Com_Printf("G_Actor2x2Spawn: Warning: z level is out of bounds: %i\n", ent->pos[2]);
	gi.GridPosToVec(gi.routingMap, ent->pos, ent->origin);

	/* link it for collision detection */
	ent->dir = AngleToDV(ent->angle);
	ent->solid = SOLID_BBOX;

	/* Set bounding box. Maybe this is already set in one of the spawn functions? */
	if (ent->maxs[0] == 0)
		VectorSet(ent->maxs, PLAYER2x2_WIDTH, PLAYER2x2_WIDTH, PLAYER_STAND);
	if (ent->mins[0] == 0)
		VectorSet(ent->mins, -PLAYER2x2_WIDTH, -PLAYER2x2_WIDTH, PLAYER_MIN);
}

/**
 * @brief QUAKED light (0 1 0) (-8 -8 -8) (8 8 8)
 */
static void SP_light (edict_t *ent)
{
	/* lights aren't client-server communicated items */
	/* they are completely client side */
	G_FreeEdict(ent);
}

/**
 * @brief QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
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

	/* FIXME: Wrong place here */
	/* maybe there are already the max soldiers allowed per team connected */
	if (sv_maxsoldiersperteam->integer > level.num_spawnpoints[ent->team]) {
		ent->STUN = 0;
		ent->HP = MAX_HP;
		G_ActorSpawn(ent);
	} else
		G_FreeEdict(ent);
}

/**
 * @brief QUAKED info_human_start (1 0 0) (-16 -16 -24) (16 16 32)
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
 * @brief QUAKED info_2x2_start (1 1 0) (-32 -32 -24) (32 32 32)
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
 * @brief QUAKED info_alien_start (1 0 0) (-16 -16 -24) (16 16 32)
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

	G_ActorSpawn(ent);
}


/**
 * @brief QUAKED info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
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
 * @brief QUAKED info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
 * Way point for a civilian.
 * @sa SP_civilian_start
 */
static void SP_civilian_target (edict_t *ent)
{
	/* target point for which team */
	ent->team = TEAM_CIVILIAN;
	ent->classname = "civtarget";
	ent->type = ET_CIVILIANTARGET;
	ent->fieldSize = ACTOR_SIZE_NORMAL; /* to let the grid fall function work */

	/* fall to ground */
	if (ent->pos[2] >= HEIGHT)
		ent->pos[2] = HEIGHT - 1;
	ent->pos[2] = gi.GridFall(gi.routingMap, ent->pos, ent->fieldSize);
	gi.GridPosToVec(gi.routingMap, ent->pos, ent->origin);
}

/**
 * @brief Mission trigger
 * @todo use level.nextmap to spawn another map when every living actor has touched the mission trigger
 * @todo use level.actualRound to determine the 'King of the Hill' time
 * @todo Implement trigger->owner health (to destroy the target)
 */
static qboolean Touch_Mission (edict_t *self, edict_t *activator)
{
	if (!self->owner)
		return qfalse;

	Com_Printf("Touched\n");
	switch (self->owner->team) {
	case TEAM_ALIEN:
		if (activator->team == TEAM_ALIEN) {
			if (!self->count) {
				self->count = level.actualRound;
				gi.bprintf(PRINT_HUD, _("Aliens entered target zone\n"));
			}
			return qtrue;
		} else {
			/* reset king of the hill counter */
			self->count = 0;
		}
	/* general case that also works for multiplayer teams */
	default:
		if (activator->team == self->owner->team) {
			if (!self->count) {
				self->count = level.actualRound;
				gi.bprintf(PRINT_HUD, _("Target zone is occupied\n"));
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
 * @note Think functions are only executed when level.activeTeam != -1
 * or in other word, the game has started
 */
static void Think_Mission (edict_t *self)
{
	/* when every player has joined the match - spawn the mission target
	 * particle (if given) to mark the trigger */
	if (self->particle) {
		gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
		gi.WriteShort(self->spawnflags);
		gi.WriteByte(0);
		gi.WritePos(self->origin);
		gi.WriteString(self->particle);
		gi.EndEvents();

		/* This is automatically freed on map shutdown */
		self->particle = NULL;
	}

	/* occupied by a special team */
	if (self->count) {
		if (level.actualRound - self->count >= self->time) {
			/* mission succeeds */
			/* @todo implement this */
		}
	}
}

/**
 * @brief Init the human/phalanx mission entity
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
 * @brief Init the alien mission entity
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

#define MISC_MODEL_SOLID 256
/**
 * @brief Spawns a misc_model if there is a solid state
 */
static void SP_misc_model (edict_t *ent)
{
	if (ent->spawnflags & MISC_MODEL_SOLID) {
		ent->solid = SOLID_BBOX;
		/* FIXME: Handling some misc_models serverside
		 * would be nice for pathfinding and clipping */
		G_FreeEdict(ent);
	} else {
		/* handled client side */
		G_FreeEdict(ent);
	}
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
 * @brief QUAKED worldspawn (0 0 0) ?
 *
 * Only used for the world.
 * "sounds"	music cd track number
 * "message"	text to print at user logon
 * "maxlevel"	max. level to use in the map
 * "maxteams"	max team amount for multiplayergames for the current map
 */
static void SP_worldspawn (edict_t *ent)
{
	ent->solid = SOLID_BSP;
	/* since the world doesn't use G_Spawn() */
	ent->inuse = qtrue;

	if (st.nextmap)
		Q_strncpyz(level.nextmap, st.nextmap, sizeof(level.nextmap));

	/* make some data visible to the server */
	if (ent->message && ent->message[0]) {
		gi.ConfigString(CS_NAME, ent->message);
		Q_strncpyz(level.level_name, ent->message, sizeof(level.level_name));
	} else
		Q_strncpyz(level.level_name, level.mapname, sizeof(level.level_name));

	gi.ConfigString(CS_MAXCLIENTS, va("%i", sv_maxclients->integer));

	/* only used in multi player */
	if (sv_maxclients->integer >= 2) {
		gi.ConfigString(CS_MAXSOLDIERSPERTEAM, va("%i", sv_maxsoldiersperteam->integer));
		gi.ConfigString(CS_MAXSOLDIERSPERPLAYER, va("%i", sv_maxsoldiersperplayer->integer));
		gi.ConfigString(CS_ENABLEMORALE, va("%i", sv_enablemorale->integer));
		if (gi.csi->currentMD) {
			gi.ConfigString(CS_MAXTEAMS, va("%i", gi.csi->currentMD->teams));
			gi.Cvar_Set("sv_maxteams", va("%i", gi.csi->currentMD->teams));
		} else
			gi.ConfigString(CS_MAXTEAMS, sv_maxteams->string);
	}
}
