/**
 * @file src/server/server.h
 * @brief Main server include file.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/server.h
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

#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include "../common/common.h"
#include "../common/http.h"
#include "../shared/infostring.h"
#include "../shared/mutex.h"
#include "../game/game.h"

extern memPool_t *sv_genericPool;

typedef struct sv_edict_s {
	struct worldSector_s *worldSector;	/**< the sector this edict is linked into */
	struct sv_edict_s *nextEntityInWorldSector;
	qboolean linked;		/**< linked into the world */
	edict_t *ent;
} sv_edict_t;

/** @brief static mesh models (none-animated) can have a server side flag set to be clipped for pathfinding */
typedef struct sv_model_s {
	vec3_t mins, maxs;	/**< the mins and maxs of the model bounding box */
	int frame;			/**< the frame the mins and maxs were calculated for */
	char *name;			/**< the model path (relative to base/ */
} sv_model_t;

/**
 * @brief To avoid linearly searching through lists of entities during environment testing,
 * the world is carved up with an evenly spaced, axially aligned bsp tree.
 */
typedef struct worldSector_s {
	int axis;					/**< -1 = leaf node */
	float dist;
	struct worldSector_s *children[2];
	sv_edict_t *entities;
} worldSector_t;

#define	AREA_NODES	32

typedef struct pending_event_s {
	/** this is true when there was an event - and false if the event reached the end */
	qboolean pending;
	/** player mask of the current event */
	int playerMask;
	int type;
	struct dbuffer *buf;
} pending_event_t;

typedef struct {
	qboolean initialized;		/**< sv_init has completed */
	int realtime;				/**< always increasing, no clamping, etc */
	struct datagram_socket *netDatagramSocket;
	struct client_s *clients;	/**< [sv_maxclients->value]; */
	int lastHeartbeat;			/**< time where the last heartbeat was send to the master server
								 * Set to a huge negative value to send immmediately */
	int lastPing;
	qboolean abandon;			/**< shutdown server when all clients disconnect and don't accept new connections */
	qboolean killserver;		/**< will initiate shutdown once abandon is set */
	threads_mutex_t *serverMutex;
	SDL_cond *gameFrameCond;	/**< the signal that the game frame threads waits for */
	SDL_Thread *gameThread;
#ifndef HARD_LINKED_GAME
	void *gameLibrary;
#endif
	game_export_t *ge;
} serverInstanceStatic_t;

typedef enum {
	ss_dead,					/**< no map loaded */
	ss_restart,					/**< clients should reconnect, the server switched the map */
	ss_loading,					/**< spawning level edicts */
	ss_game,					/**< actively running */
	ss_game_shutdown			/**< tell the game lib to end */
} server_state_t;

/**
 * @brief Struct that is only valid for one map. It's deleted on every map load.
 */
typedef struct {
	server_state_t state;		/**< precache commands are only valid during load */

	char name[MAX_QPATH];		/**< map name */
	char assembly[MAX_QPATH];		/**< random map assembly name */
	struct cBspModel_s *models[MAX_MODELS];

	qboolean endgame;
	qboolean spawned;			/**< set when the actors have spawned - no further connections are allowed in this case */
	qboolean started;			/**< set when the match has started */

	char configstrings[MAX_CONFIGSTRINGS][MAX_TOKEN_CHARS];

	struct dbuffer *messageBuffer;
	pending_event_t pendingEvent;

	mapData_t mapData;

	mapTiles_t mapTiles;

	sv_model_t svModels[MAX_MOD_KNOWN];
	unsigned int numSVModels;
	sv_edict_t edicts[MAX_EDICTS];
	worldSector_t worldSectors[AREA_NODES];
	unsigned int numWorldSectors;

	memPool_t *gameSysPool;	/**< the mempool for the game lib */
} serverInstanceGame_t;

#define EDICT_NUM(n) ((edict_t *)((byte *)svs.ge->edicts + svs.ge->edict_size * (n)))
#define NUM_FOR_EDICT(e) (((byte *)(e) - (byte *)svs.ge->edicts) / svs.ge->edict_size)

#define PLAYER_NUM(n) ((player_t *)((byte *)svs.ge->players + svs.ge->player_size * (n)))
#define NUM_FOR_PLAYER(e) (((byte *)(e) - (byte *)svs.ge->players) / svs.ge->player_size)

typedef enum {
	cs_free,					/**< can be reused for a new connection */
	cs_connected,				/**< has been assigned to a client_t, but not in game yet */
	cs_spawning,				/**< received new, not begin yet */
	cs_began,					/**< began was received, client side rendering is active - in this stage the player is an spectator
								 * and still has to spawn his soldiers */
	cs_spawned					/**< client is fully in game and soldiers were spawned */
} client_state_t;

/**
 * a client can leave the server in one of four ways:
 * @li dropping properly by quitting or disconnecting
 * @li timing out if no valid messages are received
 * @li getting kicked off by the server operator
 * @li a program error, like an overflowed reliable buffer
 */
typedef struct client_s {
	client_state_t state;
	char userinfo[MAX_INFO_STRING];
	player_t *player;			/**< game client structure */
	char name[32];				/**< extracted from userinfo, high bits masked */
	int messagelevel;			/**< for filtering printed messages */
	int lastmessage;			/**< when packet was last received */
	char peername[256];
	struct net_stream *stream;
} client_t;

extern serverInstanceStatic_t svs;		/**< persistant server instance info */
extern serverInstanceGame_t * sv;			/**< server data per game/map */

extern cvar_t *sv_mapname;
extern cvar_t *sv_rma;
/** @brief display a character graphic of the tiles placed when RMA2 reaches a dead end. */
extern cvar_t *sv_rmadisplaythemap;
extern cvar_t *sv_public;			/**< should heartbeats be sent? (only for public servers) */
extern cvar_t *sv_dumpmapassembly;
extern cvar_t *sv_threads;	/**< run the game lib threaded */

/* sv_main.c */
void SV_DropClient(client_t *drop, const char *message);
int SV_CountPlayers(void);
void SV_InitOperatorCommands(void);
void SV_UserinfoChanged(client_t *cl);
void SV_ReadPacket(struct net_stream *s);
char *SV_GetConfigString(int index);
int SV_GetConfigStringInteger(int index);
char *SV_SetConfigString(int index, ...);
/* ensure that always two parameters are used */
#define SV_SetConfigString(index, value) SV_SetConfigString(index, value)
client_t* SV_GetNextClient(client_t *lastClient);
client_t *SV_GetClient(int index);

/* sv_mapcycle.c */
void SV_MapcycleInit(void);
void SV_NextMapcycle(void);
void SV_MapcycleClear(void);

/* sv_init.c */
void SV_Map(qboolean day, const char *levelstring, const char *assembly);

void SV_Multicast(int mask, struct dbuffer *msg);
void SV_ClientCommand(client_t *client, const char *fmt, ...) __attribute__((format(printf,2,3)));
void SV_ClientPrintf(client_t * cl, int level, const char *fmt, ...) __attribute__((format(printf,3,4)));
void SV_BroadcastPrintf(int level, const char *fmt, ...) __attribute__((format(printf,2,3)));

/* sv_user.c */
void SV_ExecuteClientMessage(client_t * cl, int cmd, struct dbuffer *msg);
void SV_SetClientState(client_t* client, int state);

/* sv_ccmds.c */
void SV_SetMaster_f(void);
void SV_Heartbeat_f(void);
qboolean SV_CheckMap(const char *map, const char *assembly);

/* sv_game.c */
int SV_RunGameFrameThread(void *data);
void SV_RunGameFrame(void);
void SV_InitGameProgs(void);
void SV_ShutdownGameProgs(void);

/*============================================================ */

void SV_ClearWorld(void);

void SV_UnlinkEdict(edict_t * ent);
void SV_LinkEdict(edict_t * ent);
int SV_AreaEdicts(const vec3_t mins, const vec3_t maxs, edict_t ** list, int maxcount);
int SV_TouchEdicts(const vec3_t mins, const vec3_t maxs, edict_t **list, int maxCount, edict_t *skip);

/*=================================================================== */

/* returns the CONTENTS_* value from the world at the given point. */
int SV_PointContents(vec3_t p);
const char *SV_GetFootstepSound(const char *texture);
float SV_GetBounceFraction(const char *texture);
qboolean SV_LoadModelMinsMaxs(const char *model, int frame, vec3_t mins, vec3_t maxs);
trace_t SV_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const edict_t * passedict, int contentmask);

#endif /* SERVER_SERVER_H */
