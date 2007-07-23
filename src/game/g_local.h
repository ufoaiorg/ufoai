/**
 * @file g_local.h
 * @brief Local definitions for game module
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "inv_shared.h"

/* define GAME_INCLUDE so that game.h does not define the */
/* short, server-visible player_t and edict_t structures, */
/* because we define the full size ones in this file */
#define	GAME_INCLUDE
#include "game.h"

/* the "gameversion" client command will print this plus compile date */
#define	GAMEVERSION	"baseufo"

/*================================================================== */

#define MAX_SPOT_DIST	4096 /* 768 */

#define P_MASK(p)		(p->num < game.maxplayers ? 1<<(p->num) : 0)
#define PM_ALL			0xFFFFFFFF

/* server is running at 10 fps */
#define	FRAMETIME		0.1

/* memory tags to allow dynamic memory to be cleaned up */
#define	TAG_GAME	765			/* clear when unloading the dll */
#define	TAG_LEVEL	766			/* clear when loading a new level */

/* Macros for faster access to the inventory-container. */
#define RIGHT(e) (e->i.c[gi.csi->idRight])
#define LEFT(e)  (e->i.c[gi.csi->idLeft])
#define EXTENSION(e)  (e->i.c[gi.csi->idExtension])
#define HEADGEAR(e)  (e->i.c[gi.csi->idHeadgear])
#define FLOOR(e) (e->i.c[gi.csi->idFloor])

/* Actor visiblitly constants */
#define ACTOR_VIS_100	1.0
#define ACTOR_VIS_50	0.5
#define ACTOR_VIS_10	0.1
#define ACTOR_VIS_0		0.0

/* this structure is left intact through an entire game */
/* it should be initialized at dll load time, and read/written to */
/* the server.ssv file for savegames */
typedef struct {
	player_t *players;			/* [maxplayers] */

	/* store latched cvars here that we want to get at often */
	int maxplayers;
	int maxentities;
} game_locals_t;

/* this structure is cleared as each map is entered */
/* it is read/written to the level.sav file for savegames */
typedef struct {
	int framenum;		/**< the current frame (10fps) */
	float time;			/**< seconds the game is running already
		calculated through framenum * FRAMETIME */

	char level_name[MAX_QPATH];	/**< the descriptive name (Outer Base, etc) */
	char mapname[MAX_QPATH];	/**< the server name (base1, etc) */
	char nextmap[MAX_QPATH];	/**< go here when fraglimit is hit */
	qboolean routed;
	int maxteams; /**< the max team amount for multiplayer games for the current map */

	/* intermission state */
	float intermissionTime;
	int winningTeam;
	float roundstartTime;		/**< the time the team started the round */

	/* round statistics */
	int numplayers;
	int activeTeam;
	int nextEndRound;

	byte num_alive[MAX_TEAMS];
	byte num_spawned[MAX_TEAMS];
	byte num_spawnpoints[MAX_TEAMS];
	byte num_2x2spawnpoints[MAX_TEAMS];
	byte num_kills[MAX_TEAMS][MAX_TEAMS];
	byte num_stuns[MAX_TEAMS][MAX_TEAMS];
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

	/* left and right */
	float minyaw;
	float maxyaw;
	/* up and down */
	float minpitch;
	float maxpitch;

	int maxteams; /**< the max team amount for multiplayer games for the current map */
} spawn_temp_t;

/** @brief used in shot probability calculations (pseudo shots) */
typedef struct {
	int enemy;				/**< @brief shot would hit that much enemies */
	int friend;				/**< @brief shot would hit that much friends */
	int civilian;			/**< @brief shot would hit that much civilians */
	int self;				/**< FIXME: incorrect actor facing or shotOrg, or bug in trace code? */
	int damage;				/**< */
	qboolean allow_self;	/**< */
} shot_mock_t;

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

extern cvar_t *logstats;
extern FILE *logstatsfile;

extern cvar_t *filterban;

extern cvar_t *sv_gravity;
extern cvar_t *sv_maxvelocity;

extern cvar_t *sv_cheats;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_reaction_leftover;
extern cvar_t *sv_shot_origin;
extern cvar_t *maxplayers;
extern cvar_t *maxsoldiers;
extern cvar_t *maxsoldiersperplayer;
extern cvar_t *sv_enablemorale;
extern cvar_t *sv_roundtimelimit;
extern cvar_t *maxspectators;

extern cvar_t *sv_maxteams;

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

extern cvar_t *g_nodamage;

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *difficulty;

/*#define world	(&g_edicts[0])*/

/* fields are needed for spawning from the entity string */
/* and saving / loading games */
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2


/* g_cmds.c */
void Cmd_Help_f(edict_t * ent);
void Cmd_Score_f(edict_t * ent);

/* g_phys.c */
void G_PhysicsRun(void);

/* g_utils.c */
edict_t *G_Find(edict_t * from, int fieldofs, char *match);
edict_t *G_FindRadius(edict_t * from, vec3_t org, float rad, entity_type_t type);
const char* G_GetPlayerName(int pnum);
const char* G_GetWeaponNameForFiredef(fireDef_t* fd);
void G_PrintActorStats(edict_t* victim, edict_t* attacker, fireDef_t* fd);
void G_PrintStats(const char *buffer);
void Move_Calc(edict_t *ent, vec3_t dest, void(*func)(edict_t*));
void G_SetMovedir(vec3_t angles, vec3_t movedir);

edict_t *G_Spawn(void);
void G_FreeEdict(edict_t * e);

char *G_CopyString(const char *in);

float *tv(float x, float y, float z);
char *vtos(vec3_t v);

float vectoyaw(vec3_t vec);

void G_CompleteRecalcRouting(void);
void G_RecalcRouting(edict_t * ent);

/* g_client.c */
/* the visibile changed - if it was visible - it's (the edict) now invisible */
#define VIS_CHANGE	1
/* actor visible? */
#define VIS_YES		2
/* stop the current action if actor appears */
#define VIS_STOP	4

/** check whether edict is still visible - it maybe is currently visible but this
 * might have changed due to some action */
#define VT_PERISH		1
/** don't perform a frustom vis check via G_FrustumVis in G_Vis */
#define VT_NOFRUSTUM	2

/* Timeunits for the various actions. */
#define TU_CROUCH		1
#define TU_TURN		1
/* Reaction-fire timeuntis Must match defines in client/client.h */
#define TU_REACTION_SINGLE		7
#define TU_REACTION_MULTI		14

#define MORALE_RANDOM( mod )	( (mod) * (1.0 + 0.3*crand()) )

void flush_steps(void);

qboolean G_ActionCheck(player_t * player, edict_t * ent, int TU, qboolean quiet);
void G_SendStats(edict_t * ent);
edict_t *G_SpawnFloor(pos3_t pos);
int G_CheckVisTeam(int team, edict_t * check, qboolean perish);

void G_ForceEndRound(void);

void G_ActorDie(edict_t * ent, int state, edict_t *attacker);
int G_ClientAction(player_t * player);
void G_ClientEndRound(player_t * player, qboolean quiet);
void G_ClientTeamInfo(player_t * player);
int G_ClientGetTeamNum(player_t * player);
int G_ClientGetTeamNumPref(player_t * player);

void G_ClientCommand(player_t * player);
void G_ClientUserinfoChanged(player_t * player, char *userinfo);
void G_ClientBegin(player_t * player);
qboolean G_ClientSpawn(player_t * player);
qboolean G_ClientConnect(player_t * player, char *userinfo);
void G_ClientDisconnect(player_t * player);

int G_TestVis(int team, edict_t * check, int flags);
void G_ClientReload(player_t *player, int entnum, shoot_types_t st, qboolean quiet);
qboolean G_ClientCanReload(player_t *player, int entnum, shoot_types_t st);
void G_ClientGetWeaponFromInventory(player_t *player, int entnum, qboolean quiet);
void G_ClientMove(player_t * player, int team, int num, pos3_t to, qboolean stop, qboolean quiet);
void G_MoveCalc(int team, pos3_t from, int size, int distance);
void G_ClientInvMove(player_t * player, int num, int from, int fx, int fy, int to, int tx, int ty, qboolean checkaction, qboolean quiet);

qboolean G_FrustumVis(edict_t * from, vec3_t point);
float G_ActorVis(vec3_t from, edict_t * check, qboolean full);
void G_ClearVisFlags(int team);
int G_CheckVis(edict_t * check, qboolean perish);
void G_GiveTimeUnits(int team);

void G_AppearPerishEvent(int player_mask, int appear, edict_t * check);
int G_VisToPM(int vis_mask);
void G_SendInventory(int player_mask, edict_t * ent);
int G_TeamToPM(int team);

void SpawnEntities(const char *mapname, const char *entities);
qboolean G_RunFrame(void);

#ifdef DEBUG
void Cmd_InvList(player_t *player);
void G_KillTeam(void);
void G_StunTeam(void);
#endif

typedef enum {
	REACT_TUS,	/**< Stores the used TUs for Reaction fire for each edict. */
	REACT_FIRED,	/**< Stores if the edict has fired in reaction. */

	REACT_MAX
} g_reaction_storage_type_t;

extern int reactionTUs[MAX_EDICTS][REACT_MAX];	/**< Per actor: */

typedef enum {
	RF_HAND,	/**< Stores the used hand (0=right, 1=left, -1 undefined) */
	RF_FM,		/**< Stores the used firemode index. Max. number is MAX_FIREDEFS_PER_WEAPON. -1 = undefined */

	RF_MAX
} g_reaction_firemode_type_t;

extern int reactionFiremode[MAX_EDICTS][RF_MAX];
	/**< Per actor: Stores the firemode to be used for reaction fire (if the fireDef allows that)
	  * Used in g_combat.c for choosing correct reaction fire. It is (must be) filled with data from cl_actor.c
	  * The CLIENT has to make sure that any default firemodes (and unusable firemodes) are transferred correctly to this one.
	  * Otherwise reaction fire will not work at all.
	  */

extern int turnTeam;

/* g_combat.c */

qboolean G_ClientShoot(player_t * player, int num, pos3_t at, int type, int firemode, shot_mock_t *mock, qboolean allowReaction, int z_align);
void G_ResetReactionFire(int team);
qboolean G_ReactToMove(edict_t *target, qboolean mock);
void G_ReactToEndTurn(void);

/* g_ai.c */
void AI_Run(void);
void AI_ActorThink(player_t * player, edict_t * ent);
player_t *AI_CreatePlayer(int team);

/* g_svcmds.c */
void ServerCommand(void);
qboolean SV_FilterPacket(const char *from);

/* g_main.c */
void SaveClientData(void);
void FetchClientEntData(edict_t * ent);
void G_EndGame(int team);
void G_CheckEndGame(void);

/*============================================================================ */

typedef struct
{
	/* fixed data */
	vec3_t		start_origin;
	vec3_t		start_angles;
	vec3_t		end_origin;
	vec3_t		end_angles;

	float		accel;
	float		speed;
	float		decel;
	float		distance;

	float		wait;

	vec3_t		movedir;
	vec3_t		pos1, pos2;
	vec3_t		velocity;

	/* state data */
	int			state;
	vec3_t		dir;
	float		current_speed;
	float		move_speed;
	float		next_speed;
	float		remaining_distance;
	float		decel_distance;
	void		(*endfunc)(edict_t *);
} moveinfo_t;

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

	/** the number of the team for this player
	 * 0 is reserved for civilians and critters */
	int team;
	qboolean spectator;			/**< client is a spectator */
	qboolean ai;				/**< client controlled by ai */

	/** ai specific data */
	edict_t *last;

	float		flood_locktill;		/**< locked from talking */
	float		flood_when[10];		/**< when messages were said */
	int			flood_whenhead;		/**< head pointer for when said */
} client_persistant_t;

/* this structure is cleared on each PutClientInServer(),
 * except for 'client->pers'
 */
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
	link_t area;				/**< linked to a division node or leaf */
	int headnode;				/**< unused if num_clusters != -1 */

	/* tracing info */
	solid_t solid;

	vec3_t mins, maxs; /**< position of min and max points - relative to origin */
	vec3_t absmin, absmax; /**< position of min and max points - relative to world's origin */
	vec3_t size;

	/*
	 * usually the entity that spawned this entity - e.g. a bullet is owned
	 * but the player who fired it
	 */
	edict_t *owner;
	/*
	 * type of objects the entity will not pass through
	 * can be any of MASK_* or CONTENTS_*
	 */
	int clipmask;
	int modelindex;

	/*================================ */
	/* don't change anything above here - the server expects the fields in that order */
	/*================================ */

	int mapNum;
	const char *model;
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

	int reaction_minhit;		/* acceptable odds for reaction shots */

	int team;					/* player of which team? */
	int pnum;					/* the actual player slot */
	/* the models (hud) */
	int body;
	int head;
	int skin;

	/* delayed reaction fire */
	edict_t *reactionTarget;
	float	reactionTUs;
	qboolean reactionNoDraw;

	/* only set this for the attacked edict - to know who's shooting */
	edict_t *reactionAttacker;

	/* here are the character values */
	character_t chr;

	/* this is the inventory */
	inventory_t i;

	int spawnflags;	/* set via mapeditor */
	const char *classname;

	float angle;	/* entity yaw - set via mapeditor - -1=up; -2=down */

	float speed;	/* speed of entities - e.g. doors */
	float accel;	/* acceleration of plattforms/doors */
	float decel;	/* deceleration of plattforms/doors */
	const char *target;	/* name of the entity to trigger or move towards */
	const char *targetname;	/* name pointed to by target */
	const char *message;	/* message when entity is activated - set via mapeditor */
	const char *particle;
	float wait;		/* time to wait before platform moves to the next point on its path */
	float delay;	/* time between triggering switch and effect activating */
	float random;	/* used to add a bit of time variation of func_timer */
	int style;		/* type of entities */
	edictMaterial_t material;	/* material value (e.g. for func_breakable) */
	int count;		/* general purpose 'amount' variable - set via mapeditor often */
	int sounds;		/* type of sounds to play - e.g. doors */
	int dmg;		/* damage done by entity */
	int fieldSize;	/* ACTOR_SIZE_* */

	/* function to call when used */
	void (*use) (edict_t * self, edict_t * other, edict_t * activator);
	float nextthink;
	void (*think)(edict_t *self);

	/* e.g. doors */
	moveinfo_t		moveinfo;
};

#endif /* GAME_G_LOCAL_H */
