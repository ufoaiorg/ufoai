/**
 * @file g_local.h
 * @brief Local definitions for game module
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Added inclusion guard.

Original file from Quake 2 v3.21: quake2-2.31/game/g_local.h
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

#ifndef GAME_G_LOCAL_H
#define GAME_G_LOCAL_H

#include "q_shared.h"

/* define GAME_INCLUDE so that game.h does not define the */
/* short, server-visible player_t and edict_t structures, */
/* because we define the full size ones in this file */
#define	GAME_INCLUDE
#include "game.h"

/* the "gameversion" client command will print this plus compile date */
#define	GAMEVERSION	"baseufo"

/*================================================================== */

#define MAX_SPOT_DIST	768

#define P_MASK(p)		(p->num < game.maxplayers ? 1<<(p->num) : 0)
#define PM_ALL			0xFFFFFFFF

#define	FRAMETIME		0.1

/* memory tags to allow dynamic memory to be cleaned up */
#define	TAG_GAME	765			/* clear when unloading the dll */
#define	TAG_LEVEL	766			/* clear when loading a new level */

/* Macros for faster access to the inventory-container. */
#define RIGHT(e) e->i.c[gi.csi->idRight]
#define LEFT(e)  e->i.c[gi.csi->idLeft]
#define FLOOR(e) e->i.c[gi.csi->idFloor]

/* this structure is left intact through an entire game */
/* it should be initialized at dll load time, and read/written to */
/* the server.ssv file for savegames */
typedef struct {
	char helpmessage1[512];
	char helpmessage2[512];
	int helpchanged;			/* flash F1 icon if non 0, play sound */
	/* and increment only if 1, 2, or 3 */

	player_t *players;			/* [maxplayers] */

	/* store latched cvars here that we want to get at often */
	int maxplayers;
	int maxentities;

	/* cross level triggers */
	int serverflags;

	/* items */
	int num_items;

	qboolean autosaved;
} game_locals_t;

/* this structure is cleared as each map is entered */
/* it is read/written to the level.sav file for savegames */
typedef struct {
	int framenum;
	float time;

	char level_name[MAX_QPATH];	/* the descriptive name (Outer Base, etc) */
	char mapname[MAX_QPATH];	/* the server name (base1, etc) */
	char nextmap[MAX_QPATH];	/* go here when fraglimit is hit */
	qboolean routed;

	/* intermission state */
	char *changemap;
	float intermissionTime;
	int winningTeam;

	/* round statistics */
	int numplayers;
	int activeTeam;
	int nextEndRound;

	byte num_alive[MAX_TEAMS];
	byte num_spawned[MAX_TEAMS];
	byte num_spawnpoints[MAX_TEAMS];
	byte num_ugvspawnpoints[MAX_TEAMS];
	byte num_kills[MAX_TEAMS][MAX_TEAMS];
} level_locals_t;


/* spawn_temp_t is only used to hold entity field values that */
/* can be set from the editor, but aren't actualy present */
/* in edict_t during gameplay */
typedef struct {
	/* world vars */
	char *nextmap;

	int lip;
	int distance;
	int height;
	char *noise;
	float pausetime;
	char *item;
	char *gravity;

	float minyaw;
	float maxyaw;
	float minpitch;
	float maxpitch;
} spawn_temp_t;

extern game_locals_t game;
extern level_locals_t level;
extern game_import_t gi;
extern game_export_t globals;

extern edict_t *g_edicts;

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

extern cvar_t *maxentities;
extern cvar_t *password;
extern cvar_t *spectator_password;
extern cvar_t *needpass;
extern cvar_t *dedicated;

extern cvar_t *filterban;

extern cvar_t *sv_gravity;
extern cvar_t *sv_maxvelocity;

extern cvar_t *sv_cheats;
extern cvar_t *sv_maxclients;
extern cvar_t *maxplayers;
extern cvar_t *maxsoldiers;
extern cvar_t *maxsoldiersperplayer;
extern cvar_t *sv_enablemorale;
extern cvar_t *maxspectators;

extern cvar_t *sv_ai;
extern cvar_t *sv_teamplay;

extern cvar_t *ai_alien;
extern cvar_t *ai_civilian;
extern cvar_t *ai_equipment;
extern cvar_t *ai_numaliens;
extern cvar_t *ai_numcivilians;
extern cvar_t *ai_numactors;
extern cvar_t *ai_autojoin;

extern cvar_t *mob_death;
extern cvar_t *mob_wound;
extern cvar_t *mof_watching;
extern cvar_t *mof_teamkill;
extern cvar_t *mof_civilian;
extern cvar_t *mof_enemy;
extern cvar_t *mor_pain;

/*everyone gets this times morale damage */
extern cvar_t *mor_default;

/*at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_distance;

/*at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_victim;

/*at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_attacker;

/* how much the morale depends on the size of the damaged team */
extern cvar_t *mon_teamfactor;

extern cvar_t *mor_regeneration;
extern cvar_t *mor_shaken;
extern cvar_t *mor_panic;

extern cvar_t *m_sanity;
extern cvar_t *m_rage;
extern cvar_t *m_rage_stop;
extern cvar_t *m_panic_stop;

extern cvar_t *difficulty;

#define world	(&g_edicts[0])

/* item spawnflags */
#define ITEM_TRIGGER_SPAWN		0x00000001
#define ITEM_NO_TOUCH			0x00000002
/* 6 bits reserved for editor flags */
/* 8 bits used as power cube id bits for coop games */
#define DROPPED_ITEM			0x00010000
#define	DROPPED_PLAYER_ITEM		0x00020000
#define ITEM_TARGETS_USED		0x00040000

/* fields are needed for spawning from the entity string */
/* and saving / loading games */
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2


/* g_cmds.c */
void Cmd_Help_f(edict_t * ent);
void Cmd_Score_f(edict_t * ent);


/* g_utils.c */
qboolean KillBox(edict_t * ent);
void G_ProjectSource(vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
edict_t *G_Find(edict_t * from, int fieldofs, char *match);
edict_t *findradius(edict_t * from, vec3_t org, float rad);
edict_t *G_PickTarget(char *targetname);
void G_UseTargets(edict_t * ent, edict_t * activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);

edict_t *G_Spawn(void);
void G_FreeEdict(edict_t * e);

void G_TouchTriggers(edict_t * ent);
void G_TouchSolids(edict_t * ent);

char *G_CopyString(char *in);

float *tv(float x, float y, float z);
char *vtos(vec3_t v);

float vectoyaw(vec3_t vec);

void G_CompleteRecalcRouting(void);
void G_RecalcRouting(edict_t * ent);


/* g_client.c */
#define VIS_CHANGE	1
#define VIS_YES		2
#define VIS_STOP	4

#define VT_PERISH		1
#define VT_NOFRUSTOM	2
#define VT_FULL			4

void G_ActorDie(edict_t * ent, int state);
void G_ClientAction(player_t * player);
void G_ClientEndRound(player_t * player);
void G_ClientTeamInfo(player_t * player);

void G_ClientCommand(player_t * player);
void G_ClientUserinfoChanged(player_t * player, char *userinfo);
void G_ClientBegin(player_t * player);
qboolean G_ClientConnect(player_t * player, char *userinfo);
void G_ClientDisconnect(player_t * player);

int G_TestVis(int team, edict_t * check, int flags);
void G_ClientShoot(player_t * player, int num, pos3_t at, int type);
void G_ClientReload(player_t *player, int entnum, shoot_types_t st);
void G_ClientMove(player_t * player, int team, int num, pos3_t to, qboolean stop);
void G_MoveCalc(int team, pos3_t from, int distance);
qboolean G_ReactionFire(edict_t * target);

float G_ActorVis(vec3_t from, edict_t * check, qboolean full);
void G_ClearVisFlags(int team);
int G_CheckVis(edict_t * check, qboolean perish);
void G_GiveTimeUnits(int team);

void G_AppearPerishEvent(int player_mask, int appear, edict_t * check);
int G_VisToPM(int vis_mask);
void G_SendInventory(int player_mask, edict_t * ent);
int G_TeamToPM(int team);

/* g_ai.c */
void AI_Run(void);
void AI_ActorThink(player_t * player, edict_t * ent);
player_t *AI_CreatePlayer(int team);

/* g_svcmds.c */
void ServerCommand(void);
qboolean SV_FilterPacket(char *from);


/* g_main.c */
void SaveClientData(void);
void FetchClientEntData(edict_t * ent);
void G_EndGame(int team);
void G_CheckEndGame(void);

/*============================================================================ */

/* client_t->anim_priority */
#define	ANIM_BASIC		0		/* stand / run */
#define	ANIM_WAVE		1
#define	ANIM_JUMP		2
#define	ANIM_PAIN		3
#define	ANIM_ATTACK		4
#define	ANIM_DEATH		5
#define	ANIM_REVERSE	6

/* client data that stays across multiple level loads */
typedef struct {
	char userinfo[MAX_INFO_STRING];
	char netname[16];

	/* the number of the team for this player */
	/* 0 is reserved for civilians and critters */
	int team;
	qboolean spectator;			/* client is a spectator */
	qboolean ai;				/* client controlled by ai */

	/* ai specific data */
	edict_t *last;
} client_persistant_t;

/* this structure is cleared on each PutClientInServer(), */
/* except for 'client->pers' */
struct player_s {
	/* known to server */
	qboolean inuse;
	int num;
	int ping;

	/* private to game */
	qboolean spawned;
	qboolean ready;
	client_persistant_t pers;
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

	/*================================ */

	int mapNum;
	char *model;
	float freetime;				/* sv.time when the object was freed */

	/* only used locally in game, not by server */

	int type;
	int visflags;

	pos3_t pos;
	byte dir;					/* direction the player looks at */

	int TU;						/* remaining timeunits */
	int HP;						/* remaining healthpoints */
	int AP;						/* remaining armor protection */
	int STUN;
	int morale;					/* the current morale value */

	int state;					/* the player state - dead, shaken.... */

	int team;					/* player of which team? */
	int pnum;					/* the actual player slot */
	/* the models (hud) */
	int body;
	int head;
	int skin;

	/* here are the character values */
	character_t chr;

	/* this is the inventory */
	inventory_t i;

	int spawnflags;
	char *classname;

	float angle;

	float speed;
	float accel;
	float decel;
	char *target;
	char *targetname;
	char *message;
	char *particle;
	float wait;
	float delay;
	float random;
	int style;
	int count;
	int sounds;
	int dmg;
	int fieldSize;				/* ACTOR_SIZE_* */

	void (*use) (edict_t * self, edict_t * other, edict_t * activator);
};

#endif /* GAME_G_LOCAL_H */
