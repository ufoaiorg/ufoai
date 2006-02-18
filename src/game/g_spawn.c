/*
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

spawn_temp_t	st;

void SP_light (edict_t *ent);
void SP_misc_dummy (edict_t *ent);
void SP_player_start (edict_t *ent);
void SP_human_start (edict_t *ent);
void SP_alien_start (edict_t *ent);
void SP_civilian_start (edict_t *ent);
void SP_func_breakable (edict_t *ent);
void SP_worldspawn (edict_t *ent);

typedef struct
{
	char	*name;
	void	(*spawn)(edict_t *ent);
} spawn_t;

spawn_t	spawns[] = {
	{"worldspawn", SP_worldspawn},
	{"light", SP_light},
	{"misc_model", SP_misc_dummy},
	{"misc_particle", SP_misc_dummy},
	{"info_player_start", SP_player_start},
	{"info_human_start", SP_human_start},
	{"info_alien_start", SP_alien_start},
	{"info_civilian_start", SP_civilian_start},
	{"func_breakable", SP_func_breakable},

	{NULL, NULL}
};

field_t fields[] = {
	{"classname", FOFS(classname), F_LSTRING},
	{"model", FOFS(model), F_LSTRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"accel", FOFS(accel), F_FLOAT},
	{"decel", FOFS(decel), F_FLOAT},
	{"target", FOFS(target), F_LSTRING},
	{"targetname", FOFS(targetname), F_LSTRING},
	{"team", FOFS(team), F_INT},
	{"wait", FOFS(wait), F_FLOAT},
	{"delay", FOFS(delay), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"style", FOFS(style), F_INT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(HP), F_INT},
	{"sounds", FOFS(sounds), F_INT},
	{"light", 0, F_IGNORE},
	{"dmg", FOFS(dmg), F_INT},
	{"origin", FOFS(origin), F_VECTOR},
	{"angles", FOFS(angles), F_VECTOR},
	{"angle", FOFS(angle), F_FLOAT},

	//need for item field in edict struct, FFL_SPAWNTEMP item will be skipped on saves
	{"gravity", STOFS(gravity), F_LSTRING, FFL_SPAWNTEMP},
	{"sky", STOFS(sky), F_LSTRING, FFL_SPAWNTEMP},
	{"skyrotate", STOFS(skyrotate), F_FLOAT, FFL_SPAWNTEMP},
	{"skyaxis", STOFS(skyaxis), F_VECTOR, FFL_SPAWNTEMP},
	{"minyaw", STOFS(minyaw), F_FLOAT, FFL_SPAWNTEMP},
	{"maxyaw", STOFS(maxyaw), F_FLOAT, FFL_SPAWNTEMP},
	{"minpitch", STOFS(minpitch), F_FLOAT, FFL_SPAWNTEMP},
	{"maxpitch", STOFS(maxpitch), F_FLOAT, FFL_SPAWNTEMP},
	{"nextmap", STOFS(nextmap), F_LSTRING, FFL_SPAWNTEMP},

	{0, 0, 0, 0}

};

/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn (edict_t *ent)
{
	spawn_t	*s;

	if (!ent->classname)
	{
		gi.dprintf ("ED_CallSpawn: NULL classname\n");
		return;
	}

	// check normal spawn functions
	for (s=spawns ; s->name ; s++)
	{
		if (!strcmp(s->name, ent->classname))
		{	// found it
			s->spawn (ent);
			return;
		}
	}

	gi.dprintf ("%s doesn't have a spawn function\n", ent->classname);
	ent->inuse = false;
}

/*
=============
ED_NewString
=============
*/
char *ED_NewString (char *string)
{
	char	*newb, *new_p;
	int		i,l;

	l = strlen(string) + 1;

	newb = gi.TagMalloc (l, TAG_LEVEL);

	new_p = newb;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return newb;
}




/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an edict
===============
*/
void ED_ParseField (char *key, char *value, edict_t *ent)
{
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;

	for (f=fields ; f->name ; f++)
	{
		if (!(f->flags & FFL_NOSPAWN) && !Q_stricmp(f->name, key))
		{	// found it
			if (f->flags & FFL_SPAWNTEMP)
				b = (byte *)&st;
			else
				b = (byte *)ent;

			switch (f->type)
			{
			case F_LSTRING:
				*(char **)(b+f->ofs) = ED_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *)(b+f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			case F_IGNORE:
				break;
			case F_GSTRING:
				break;
			case F_EDICT:
				break;
			case F_CLIENT:
				break;
			case F_FUNCTION:
				break;
			}
			return;
		}
	}
//	gi.dprintf ("%s is not a field\n", key);
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	qboolean	init;
	char		keyname[256];
	char		*com_token;

	init = false;
	memset (&st, 0, sizeof(st));

	// go through all the dictionary pairs
	while (1)
	{
		// parse key
		com_token = COM_Parse (&data);
		if (com_token[0] == '}')
			break;
		if (!data)
			gi.error ("ED_ParseEntity: EOF without closing brace");

		strncpy (keyname, com_token, sizeof(keyname)-1);

		// parse value
		com_token = COM_Parse (&data);
		if (!data)
			gi.error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			gi.error ("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		ED_ParseField (keyname, com_token, ent);
	}

	if (!init)
		memset (ent, 0, sizeof(*ent));

	return data;
}


/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities (char *mapname, char *entities)
{
	edict_t		*ent;
	int		inhibit, entnum;
	char		*com_token;

//	SaveClientData ();

	gi.FreeTags (TAG_LEVEL);

	memset (&level, 0, sizeof(level));
	memset (g_edicts, 0, game.maxentities * sizeof (g_edicts[0]));

	strncpy (level.mapname, mapname, sizeof(level.mapname)-1);

	ent = NULL;
	level.activeTeam = -1;
	inhibit = 0;

	// parse ents
	entnum = 0;
	while (1)
	{
		// parse the opening brace
		com_token = COM_Parse (&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.error ("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent)
			ent = g_edicts;
		else
			ent = G_Spawn ();
		entities = ED_ParseEdict (entities, ent);

		VecToPos( ent->origin, ent->pos );
		gi.GridPosToVec( gi.map, ent->pos, ent->origin );

		ent->mapNum = entnum++;
		ED_CallSpawn (ent);
	}

	gi.dprintf ("%i entities inhibited\n", inhibit);

#ifdef DEBUG
	i = 1;
	ent = EDICT_NUM(i);
	while (i < globals.num_edicts) {
		if (ent->inuse != 0 || ent->inuse != 1)
			Com_DPrintf("Invalid entity %d\n", i);
		i++, ent++;
	}
#endif

	// spawn ai players, if needed
	if ( level.num_spawnpoints[TEAM_CIVILIAN] )
		AI_CreatePlayer( TEAM_CIVILIAN );
	if ( (int)sv_maxclients->value == 1 && level.num_spawnpoints[TEAM_ALIEN] )
		AI_CreatePlayer( TEAM_ALIEN );
}


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8)
*/
void SP_light (edict_t *self)
{
	// lights aren't client-server communicated items
	// they are completely client side
	G_FreeEdict( self );
}

void G_ActorSpawn( edict_t *ent )
{
	// set properties
	level.num_spawnpoints[ent->team]++;
	ent->classname = "actor";
	ent->type = ET_ACTORSPAWN;

	// fall to ground
	ent->pos[2] = gi.GridFall( gi.map, ent->pos );
	gi.GridPosToVec( gi.map, ent->pos, ent->origin );

	// set stats
	ent->HP = 100;
	ent->AP = 100;

	// link it for collision detection
	ent->dir = AngleToDV( ent->angle );
	ent->solid = SOLID_BBOX;
	VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND );
	VectorSet( ent->mins,-PLAYER_WIDTH,-PLAYER_WIDTH, PLAYER_MIN );
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
Starting point for a player.
"team"	the number of the team for this player starting point
"0" is reserved for civilians and critters (use info_civilian_start instead)
*/
void SP_player_start (edict_t *ent)
{
	// only used in multi player
	if ( sv_maxclients->value == 1 )
	{
		G_FreeEdict( ent );
		return;
	}
	// maybe there are already the max soldiers allowed per team connected
	if ( (int)(maxsoldiers->value) >= level.num_spawnpoints[ent->team] )
		G_ActorSpawn( ent );
	else
		G_FreeEdict( ent );
}


/*QUAKED info_human_start (1 0 0) (-16 -16 -24) (16 16 32)
Starting point for a single player human.
*/
void SP_human_start (edict_t *ent)
{
	// only used in single player
	if ( sv_maxclients->value > 1 )
	{
		G_FreeEdict( ent );
		return;
	}
	ent->team = 1;
	G_ActorSpawn( ent );
}


/*QUAKED info_alien_start (1 0 0) (-16 -16 -24) (16 16 32)
Starting point for a single player alien.
*/
void SP_alien_start (edict_t *ent)
{
	// only used in single player
	if ( sv_maxclients->value > 1 )
	{
		G_FreeEdict( ent );
		return;
	}
	ent->team = TEAM_ALIEN;
	G_ActorSpawn( ent );
}


/*QUAKED info_civilian_start (0 1 1) (-16 -16 -24) (16 16 32)
Starting point for a civilian.
*/
void SP_civilian_start (edict_t *ent)
{
	ent->team = TEAM_CIVILIAN;
	G_ActorSpawn( ent );
}


/*
a dummy to get rid of local entities
*/
void SP_misc_dummy (edict_t *self)
{
	// models aren't client-server communicated items
	// they are completely client side
	G_FreeEdict( self );
}


/*QUAKED func_breakable (0.3 0.3 0.3) ?
Used for breakable objects.
*/
void SP_func_breakable (edict_t *self)
{
	self->type = ET_BREAKABLE;

	VectorSet( self->origin, 0, 0, 0 );
	gi.setmodel( self, self->model );

//	Com_Printf( "model (%s) num: %i mins: %i %i %i maxs: %i %i %i\n",
//		self->model, self->mapNum, (int)self->mins[0], (int)self->mins[1], (int)self->mins[2],
//		(int)self->maxs[0], (int)self->maxs[1], (int)self->maxs[2] );
}


/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"	environment map name
"skyaxis"	vector axis for rotating sky
"skyrotate"	speed of rotation in degrees/second
"sounds"	music cd track number
"gravity"	800 is default gravity
"message"	text to print at user logon
*/
void SP_worldspawn (edict_t *ent)
{
	ent->solid = SOLID_BSP;
	ent->inuse = true;			// since the world doesn't use G_Spawn()

	//---------------

	// reserve some spots for dead player bodies for coop / deathmatch
	//InitBodyQue ();

	// set configstrings for items
	//SetItemNames ();

	if (st.nextmap)
		strcpy (level.nextmap, st.nextmap);

	// make some data visible to the server

	if (ent->message && ent->message[0])
	{
		gi.configstring (CS_NAME, ent->message);
		strncpy (level.level_name, ent->message, sizeof(level.level_name));
	}
	else
		strncpy (level.level_name, level.mapname, sizeof(level.level_name));

/*	if (st.sky && st.sky[0])
		gi.configstring (CS_SKY, st.sky);
	else
		gi.configstring (CS_SKY, "unit1_");

	gi.configstring (CS_SKYROTATE, va("%f", st.skyrotate) );

	gi.configstring (CS_SKYAXIS, va("%f %f %f",
		st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]) );*/

	gi.configstring (CS_CDTRACK, va("%i", ent->sounds) );

	gi.configstring (CS_MAXCLIENTS, va("%i", (int)(maxplayers->value) ) );

	// only used in multi player
	if ( sv_maxclients->value == 1 )
	{
		gi.configstring (CS_MAXSOLDIERS, va("%i", (int)(maxsoldiers->value) ) );
	}
	//---------------

	if (!st.gravity)
		gi.cvar_set("sv_gravity", "800");
	else
		gi.cvar_set("sv_gravity", st.gravity);

	//
	// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
	//

	// 0 normal
	gi.configstring(CS_LIGHTS+0, "m");

	// 1 FLICKER (first variety)
	gi.configstring(CS_LIGHTS+1, "mmnmmommommnonmmonqnmmo");

	// 2 SLOW STRONG PULSE
	gi.configstring(CS_LIGHTS+2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 CANDLE (first variety)
	gi.configstring(CS_LIGHTS+3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 FAST STROBE
	gi.configstring(CS_LIGHTS+4, "mamamamamama");

	// 5 GENTLE PULSE 1
	gi.configstring(CS_LIGHTS+5,"jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	// 6 FLICKER (second variety)
	gi.configstring(CS_LIGHTS+6, "nmonqnmomnmomomno");

	// 7 CANDLE (second variety)
	gi.configstring(CS_LIGHTS+7, "mmmaaaabcdefgmmmmaaaammmaamm");

	// 8 CANDLE (third variety)
	gi.configstring(CS_LIGHTS+8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	// 9 SLOW STROBE (fourth variety)
	gi.configstring(CS_LIGHTS+9, "aaaaaaaazzzzzzzz");

	// 10 FLUORESCENT FLICKER
	gi.configstring(CS_LIGHTS+10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	gi.configstring(CS_LIGHTS+11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	gi.configstring(CS_LIGHTS+63, "a");
}

