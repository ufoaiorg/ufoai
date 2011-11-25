/**
 * @file game.h
 * @brief Interface to game library.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../shared/defines.h"
#include "../shared/typedefs.h"
#include "../common/tracing.h"
#include "../common/cvar.h"

#define	GAME_API_VERSION	10

/** @brief edict->solid values */
typedef enum {
	SOLID_NOT,					/**< no interaction with other objects */
	SOLID_TRIGGER,				/**< only touch when inside, after moving (triggers) */
	SOLID_BBOX,					/**< touch on edge (monsters, etc) */
	SOLID_BSP					/**< bsp clip, touch on edge (solid walls, blocks, etc) */
} solid_t;

/*=============================================================== */

typedef struct edict_s edict_t;
typedef struct player_s player_t;


#ifndef GAME_INCLUDE

struct player_s {
	qboolean inuse;
	int num;					/**< communicated by server to clients */
	qboolean isReady;

	/** the game dll can add anything it wants after
	 * this point in the structure */
};

/** @note don't change the order - also see edict_s in g_local.h */
struct edict_s {
	qboolean inuse;
	int linkcount;		/**< count the amount of server side links - if a link was called,
						 * something on the position or the size of the entity was changed */

	int number;			/**< the number in the global edict array */

	vec3_t origin;		/**< the position in the world */
	vec3_t angles;		/**< the rotation in the world (pitch, yaw, roll) */

	/** tracing info SOLID_BSP, SOLID_BBOX, ... */
	solid_t solid;

	vec3_t mins, maxs; /**< position of min and max points - relative to origin */
	vec3_t absmin, absmax; /**< position of min and max points - relative to world's origin */
	vec3_t size;

	edict_t *child;	/**< e.g. the trigger for this edict */
	edict_t *owner;	/**< e.g. the door model in case of func_door */
	int modelindex;	/**< inline model index */

	const char *classname;
};


#endif	/* GAME_INCLUDE */

/*=============================================================== */

/** @brief functions provided by the main engine */
typedef struct {
	/* client/server information */
	int seed; /**< random seed */
	const csi_t *csi;
	routing_t *routingMap;	/**< server side routing table */

	/* special messages */

	/** sends message to all players */
	void (IMPORT *BroadcastPrintf) (int printlevel, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	/** print output to server console */
	void (IMPORT *DPrintf) (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	/** sends message to only one player (don't use this to send messages to an AI player struct) */
	void (IMPORT *PlayerPrintf) (const player_t * player, int printlevel, const char *fmt, va_list ap);

	/** configstrings hold all the index strings.
	 * All of the current configstrings are sent to clients when
	 * they connect, and changes are sent to all connected clients. */
	void (IMPORT *ConfigString) (int num, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

	/** @note The error message should not have a newline - it's added inside of this function */
	void (IMPORT *Error) (const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

	/** the *index functions create configstrings and some internal server state */
	unsigned int (IMPORT *ModelIndex) (const char *name);

	/** This updates the inline model's orientation */
	void (IMPORT *SetInlineModelOrientation) (const char *name, const vec3_t origin, const vec3_t angles);

	void (IMPORT *SetModel) (edict_t * ent, const char *name);

	/** @brief collision detection
	 * @note traces a box from start to end, ignoring entities passent, stopping if it hits an object of type specified
	 * via contentmask (MASK_*). Mins and maxs set the box which will do the tracing - if NULL then a line is used instead
	 * @return the trace data
	 */
	trace_t (IMPORT *Trace) (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const edict_t * passent, int contentmask);

	int (IMPORT *PointContents) (vec3_t point);
	const char* (IMPORT *GetFootstepSound) (const char* texture);
	float (IMPORT *GetBounceFraction) (const char *texture);
	qboolean (IMPORT *LoadModelMinsMaxs) (const char *model, int frame, vec3_t mins, vec3_t maxs);

	/** links entity into the world - so that it is sent to the client and used for
	 * collision detection, etc. Must be relinked if its size, position or solidarity changes */
	void (IMPORT *LinkEdict) (edict_t * ent);
	/** call before removing an interactive edict */
	void (IMPORT *UnlinkEdict) (edict_t * ent);
	int (IMPORT *BoxEdicts) (const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount);
	int (IMPORT *TouchEdicts) (const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, edict_t *skip);

	/** @brief fast version of a line trace but without including entities */
	qboolean (IMPORT *TestLine) (const vec3_t start, const vec3_t stop, const int levelmask);
	/** @brief fast version of a line trace that also includes entities */
	qboolean (IMPORT *TestLineWithEnt) (const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist);
	float (IMPORT *GrenadeTarget) (const vec3_t from, const vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0);

	void (IMPORT *MoveCalc) (const routing_t * map, actorSizeEnum_t actorSize, pathing_t * path, const pos3_t from, byte crouchingState, int distance, pos_t ** forbiddenList, int forbiddenListLength);
	void (IMPORT *MoveStore) (pathing_t * path);
	pos_t (IMPORT *MoveLength) (const pathing_t * path, const pos3_t to, byte crouchingState, qboolean stored);
	int (IMPORT *MoveNext) (const pathing_t *path, const pos3_t from, byte crouchingState);
	int (IMPORT *GridFloor) (const routing_t * map, actorSizeEnum_t actorSize, const pos3_t pos);
	int (IMPORT *GetTUsForDirection) (int dir, int crouched);
	pos_t (IMPORT *GridFall) (const routing_t * map, actorSizeEnum_t actorSize, const pos3_t pos);
	void (IMPORT *GridPosToVec) (const routing_t * map, actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec);
	void (IMPORT *GridRecalcRouting) (routing_t * map, const char *name, const char **list);
	float (IMPORT *GetVisibility) (const pos3_t position);

	/* filesystem functions */
	const char *(IMPORT *FS_Gamedir) (void);
	int (IMPORT *FS_LoadFile) (const char *path, byte **buffer);
	void (IMPORT *FS_FreeFile) (void *buffer);

	/* network messaging (writing) */
	void (IMPORT *WriteChar) (char c);

	void (IMPORT *WriteByte) (byte c);
	byte* (IMPORT *WriteDummyByte) (byte c);
	void (IMPORT *WriteShort) (int c);

	void (IMPORT *WriteLong) (int c);
	void (IMPORT *WriteString) (const char *s);
	void (IMPORT *WritePos) (const vec3_t pos);	/**< some fractional bits */
	void (IMPORT *WriteGPos) (const pos3_t pos);
	void (IMPORT *WriteDir) (const vec3_t pos);	/**< single byte encoded, very coarse */
	void (IMPORT *WriteAngle) (float f);
	void (IMPORT *WriteFormat) (const char *format, ...);

	void (IMPORT *AbortEvents) (void);
	void (IMPORT *EndEvents) (void);
	void (IMPORT *AddEvent) (unsigned int mask, int eType);
	int (IMPORT *GetEvent) (void);

	/* network messaging (reading) */
	int (IMPORT *ReadChar) (void);
	int (IMPORT *ReadByte) (void);
	int (IMPORT *ReadShort) (void);
	int (IMPORT *ReadLong) (void);
	int (IMPORT *ReadString) (char *str, size_t length);
	void (IMPORT *ReadPos) (vec3_t pos);
	void (IMPORT *ReadGPos) (pos3_t pos);
	void (IMPORT *ReadDir) (vec3_t vector);
	float (IMPORT *ReadAngle) (void);
	void (IMPORT *ReadData) (void *buffer, int size);
	void (IMPORT *ReadFormat) (const char *format, ...);

	qboolean (IMPORT *GetConstInt) (const char *name, int *value);
	qboolean (IMPORT *GetConstIntFromNamespace) (const char *space, const char *name, int *value);
	const char* (IMPORT *GetConstVariable) (const char *space, int value);
	void (IMPORT *RegisterConstInt) (const char *name, int value);
	qboolean (IMPORT *UnregisterConstVariable) (const char *name);

	/* misc functions */
	void (IMPORT *GetCharacterValues) (const char *teamDefinition, character_t *chr);

	/* managed memory allocation */
	void *(IMPORT *TagMalloc) (int size, int tag, const char *file, int line);
	void (IMPORT *TagFree) (void *block, const char *file, int line);
	void (IMPORT *FreeTags) (int tag, const char *file, int line);

	/* console variable interaction */
	cvar_t *(IMPORT *Cvar_Get) (const char *varName, const char *value, int flags, const char* desc);
	cvar_t *(IMPORT *Cvar_Set) (const char *varName, const char *value);
	const char *(IMPORT *Cvar_String) (const char *varName);

	/* ClientCommand and ServerCommand parameter access */
	int (IMPORT *Cmd_Argc) (void);
	const char *(IMPORT *Cmd_Argv) (int n);
	const char *(IMPORT *Cmd_Args) (void);		/**< concatenation of all argv >= 1 */

	/** add commands to the server console as if they were typed in
	 * for map changing, etc */
	void (IMPORT *AddCommandString) (const char *text);
} game_import_t;

/** @brief functions exported by the game subsystem */
typedef struct {
	int apiversion;

	/** the init function will only be called when a game starts,
	 * not each time a level is loaded.  Persistant data for clients
	 * and the server can be allocated in init */
	void (EXPORT *Init) (void);
	void (EXPORT *Shutdown) (void);

	/* each new level entered will cause a call to G_SpawnEntities */
	void (EXPORT *SpawnEntities) (const char *mapname, qboolean day, const char *entstring);

	qboolean (EXPORT *ClientConnect) (player_t * client, char *userinfo, size_t userinfoSize);
	qboolean (EXPORT *ClientBegin) (player_t * client);
	void (EXPORT *ClientStartMatch) (player_t * client);
	void (EXPORT *ClientUserinfoChanged) (player_t * client, const char *userinfo);
	void (EXPORT *ClientDisconnect) (player_t * client);
	void (EXPORT *ClientCommand) (player_t * client);

	int (EXPORT *ClientAction) (player_t * client);
	void (EXPORT *ClientEndRound) (player_t * client);
	void (EXPORT *ClientTeamInfo) (const player_t * client);
	void (EXPORT *ClientInitActorStates) (const player_t * client);
	int (EXPORT *ClientGetTeamNum) (const player_t * client);
	int (EXPORT *ClientGetTeamNumPref) (const player_t * client);
	qboolean (EXPORT *ClientIsReady) (const player_t * client);

	int (EXPORT *ClientGetActiveTeam) (void);
	const char* (EXPORT *ClientGetName) (int pnum);

	qboolean (EXPORT *RunFrame) (void);

	/** ServerCommand will be called when an "sv <command>" command is issued on the
	 * server console.
	 * The game can issue gi.Cmd_Argc() / gi.Cmd_Argv() commands to get the rest
	 * of the parameters */
	void (EXPORT *ServerCommand) (void);

	/* global variables shared between game and server */

	/* The edict array is allocated in the game dll so it */
	/* can vary in size from one game to another. */

	/** The size will be fixed when ge->Init() is called */
	struct edict_s *edicts;
	int edict_size;
	int num_edicts;				/**< current number, <= max_edicts */
	int max_edicts;

	struct player_s *players;
	int player_size;
	int maxplayersperteam;
} game_export_t;

game_export_t *GetGameAPI(game_import_t * import);

#endif /* GAME_GAME_H */
