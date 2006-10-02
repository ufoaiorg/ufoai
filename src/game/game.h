/**
 * @file game.h
 * @brief Interface to game library.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Added inclusion guard.

Original file from Quake 2 v3.21: quake2-2.31/game/game.h
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

#ifndef GAME_GAME_H
#define GAME_GAME_H

#define	GAME_API_VERSION	4

/* edict->svflags */

#define	SVF_NOCLIENT			0x00000001	/* don't send entity to clients, even if it has effects */
#define	SVF_DEADMONSTER			0x00000002	/* treat as CONTENTS_DEADMONSTER for collision */
#define	SVF_MONSTER				0x00000004	/* treat as CONTENTS_MONSTER for collision */

/* edict->solid values */

typedef enum {
	SOLID_NOT,					/* no interaction with other objects */
	SOLID_TRIGGER,				/* only touch when inside, after moving */
	SOLID_BBOX,					/* touch on edge */
	SOLID_BSP					/* bsp clip, touch on edge */
} solid_t;

/*=============================================================== */

/* link_t is only used for entity area links now */
typedef struct link_s {
	struct link_s *prev, *next;
} link_t;

#define	MAX_ENT_CLUSTERS	16


typedef struct edict_s edict_t;
typedef struct player_s player_t;


#ifndef GAME_INCLUDE

struct player_s {
	qboolean inuse;
	int num;					/* communicated by server to clients */
	int ping;

	/* the game dll can add anything it wants after */
	/* this point in the structure */
};


struct edict_s {
	qboolean inuse;
	int linkcount;

	int number;

	vec3_t origin;
	vec3_t angles;

	/* FIXME: move these fields to a server private sv_entity_t */
	link_t area;				/* linked to a division node or leaf */
	int headnode;				/* unused if num_clusters != -1 */

	/* tracing info */
	solid_t solid;
	int svflags;

	vec3_t mins, maxs;
	vec3_t absmin, absmax;
	vec3_t size;

	edict_t *owner;
	int clipmask;
	int modelindex;
};


#endif							/* GAME_INCLUDE */

/*=============================================================== */

/* functions provided by the main engine */
typedef struct {
	/* client/server information */
	int seed;
	csi_t *csi;
	struct routing_s *map;

	/* special messages */
	void (*bprintf) (int printlevel, char *fmt, ...);
	void (*dprintf) (char *fmt, ...);
	void (*cprintf) (player_t * player, int printlevel, char *fmt, ...);
	void (*centerprintf) (player_t * player, char *fmt, ...);

	/* config strings hold all the index strings, the lightstyles, */
	/* and misc data like the cdtrack. */
	/* All of the current configstrings are sent to clients when */
	/* they connect, and changes are sent to all connected clients. */
	void (*configstring) (int num, char *string);

	void (*error) (char *fmt, ...);

	/* the *index functions create configstrings and some internal server state */
	int (*modelindex) (char *name);
	int (*soundindex) (char *name);
	int (*imageindex) (char *name);

	void (*setmodel) (edict_t * ent, char *name);

	/* collision detection */
	trace_t(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t * passent, int contentmask);
	void (*linkentity) (edict_t * ent);
	void (*unlinkentity) (edict_t * ent);	/* call before removing an interactive edict */

	int (*TestLine) (vec3_t start, vec3_t stop);
	float (*GrenadeTarget) (vec3_t from, vec3_t at, float speed, qboolean launched, vec3_t v0);

	void (*MoveCalc) (struct routing_s * map, pos3_t from, int distance, byte ** fb_list, int fb_length);
	void (*MoveStore) (struct routing_s * map);
	int (*MoveLength) (struct routing_s * map, pos3_t to, qboolean stored);
	int (*MoveNext) (struct routing_s * map, pos3_t from);
	int (*GridHeight) (struct routing_s * map, pos3_t pos);
	int (*GridFall) (struct routing_s * map, pos3_t pos);
	void (*GridPosToVec) (struct routing_s * map, pos3_t pos, vec3_t vec);
	void (*GridRecalcRouting) (struct routing_s * map, char *name, char **list);

	/* network messaging (writing) */
	void (*multicast) (int mask);
	void (*unicast) (player_t * player);
	void (*WriteChar) (int c);

#ifdef DEBUG
	void (*WriteByte) (int c, char* file, int line);
#else
	void (*WriteByte) (int c);
#endif

#ifdef DEBUG
	void (*WriteShort) (int c, char* file, int line);
#else
	void (*WriteShort) (int c);
#endif

	void (*WriteLong) (int c);
	void (*WriteFloat) (float f);
	void (*WriteString) (char *s);
	void (*WritePos) (vec3_t pos);	/* some fractional bits */
	void (*WriteGPos) (pos3_t pos);
	void (*WriteDir) (vec3_t pos);	/* single byte encoded, very coarse */
	void (*WriteAngle) (float f);
	void (*WriteFormat) (char *format, ...);

	void (*WriteNewSave) (int c);
	void (*WriteToSave) (int c);

	void (*EndEvents) (void);
	void (*AddEvent) (int mask, int eType);

	/* network messaging (reading) */
	/* only use after a call from one of these functions: */
	/* ClientAction */
	/* (more to come?) */

	int (*ReadChar) (void);
	int (*ReadByte) (void);
	int (*ReadShort) (void);
	int (*ReadLong) (void);
	float (*ReadFloat) (void);
	char *(*ReadString) (void);
	void (*ReadPos) (vec3_t pos);
	void (*ReadGPos) (pos3_t pos);
	void (*ReadDir) (vec3_t vector);
	float (*ReadAngle) (void);
	void (*ReadData) (void *buffer, int size);
	void (*ReadFormat) (char *format, ...);

	/* misc functions */
	int (*GetModelAndName) (char *team, character_t *chr);

	/* managed memory allocation */
	void *(*TagMalloc) (size_t size, int tag);
	void (*TagFree) (void *block);
	void (*FreeTags) (int tag);

	/* console variable interaction */
	cvar_t *(*cvar) (char *var_name, char *value, int flags, char* desc);
	cvar_t *(*cvar_set) (char *var_name, char *value);
	cvar_t *(*cvar_forceset) (char *var_name, char *value);
	char *(*cvar_string) (char *var_name);

	/* ClientCommand and ServerCommand parameter access */
	int (*argc) (void);
	char *(*argv) (int n);
	char *(*args) (void);		/* concatenation of all argv >= 1 */

	/* add commands to the server console as if they were typed in */
	/* for map changing, etc */
	void (*AddCommandString) (char *text);
	void (*DebugGraph) (float value, int color);
} game_import_t;

/* functions exported by the game subsystem */
typedef struct {
	int apiversion;

	/* the init function will only be called when a game starts, */
	/* not each time a level is loaded.  Persistant data for clients */
	/* and the server can be allocated in init */
	void (*Init) (void);
	void (*Shutdown) (void);

	/* each new level entered will cause a call to SpawnEntities */
	void (*SpawnEntities) (char *mapname, char *entstring);

	qboolean(*ClientConnect) (player_t * client, char *userinfo);
	void (*ClientBegin) (player_t * client);
	void (*ClientUserinfoChanged) (player_t * client, char *userinfo);
	void (*ClientDisconnect) (player_t * client);
	void (*ClientCommand) (player_t * client);

	void (*ClientAction) (player_t * client);
	void (*ClientEndRound) (player_t * client, qboolean quiet);
	void (*ClientTeamInfo) (player_t * client);

	void (*RunFrame) (void);

	/* ServerCommand will be called when an "sv <command>" command is issued on the */
	/* server console. */
	/* The game can issue gi.argc() / gi.argv() commands to get the rest */
	/* of the parameters */
	void (*ServerCommand) (void);

	/* global variables shared between game and server */

	/* The edict array is allocated in the game dll so it */
	/* can vary in size from one game to another. */

	/* The size will be fixed when ge->Init() is called */
	struct edict_s *edicts;
	int edict_size;
	int num_edicts;				/* current number, <= max_edicts */
	int max_edicts;

	struct player_s *players;
	int player_size;
	int max_players;
} game_export_t;

typedef game_export_t *(*GetGameApi_t) (game_import_t * import);

#endif /* GAME_GAME_H */
