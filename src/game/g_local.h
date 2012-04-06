/**
 * @file g_local.h
 * @brief Local definitions for game module
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "inventory.h"
#include "../shared/infostring.h"
#include "lua/lua.h"

/** no gettext support for game lib - but we must be able to mark the strings */
#define _(String) String
#define ngettext(x, y, cnt) x

/** @note define GAME_INCLUDE so that game.h does not define the
 * short, server-visible player_t and edict_t structures,
 * because we define the full size ones in this file */
#define	GAME_INCLUDE
#include "game.h"

/** the "gameversion" client command will print this plus compile date */
#define	GAMEVERSION	"baseufo"

#define MAX_SPOT_DIST	4096 /* 768 */

/**
 * Bitmask for all players
 */
#define PM_ALL			0xFFFFFFFF
/**
 * Bitmask for all teams
 */
#define TEAM_ALL		0xFFFFFFFF

#define G_PlayerToPM(player) ((player)->num < game.sv_maxplayersperteam ? 1 << ((player)->num) : 0)

/** server is running at 10 fps */
#define	SERVER_FRAME_SECONDS		0.1

/** memory tags to allow dynamic memory to be cleaned up */
#define	TAG_GAME	765			/* clear when unloading the game library */
#define	TAG_LEVEL	766			/* clear when loading a new level */

/** Macros for faster access to the inventory-container. */
#define CONTAINER(e, containerID) ((e)->chr.i.c[(containerID)])
#define ARMOUR(e) (CONTAINER(e, gi.csi->idArmour))
#define RIGHT(e) (CONTAINER(e, gi.csi->idRight))
#define LEFT(e)  (CONTAINER(e, gi.csi->idLeft))
#define EXTENSION(e)  (CONTAINER(e, gi.csi->idExtension))
#define HEADGEAR(e)  (CONTAINER(e, gi.csi->idHeadgear))
#define FLOOR(e) (CONTAINER(e, gi.csi->idFloor))
#define EQUIP(e) (CONTAINER(e, gi.csi->idEquip))

#define INVDEF(containerID) (&gi.csi->ids[(containerID)])

/** Actor visibility constants */
#define ACTOR_VIS_100	1.0
#define ACTOR_VIS_50	0.5
#define ACTOR_VIS_10	0.1
#define ACTOR_VIS_0		0.0

#define MIN_TU				27

#define G_FreeTags(tag) gi.FreeTags((tag), __FILE__, __LINE__)
#define G_TagMalloc(size, tag) gi.TagMalloc((size), (tag), __FILE__, __LINE__)
#define G_MemFree(ptr) gi.TagFree((ptr), __FILE__, __LINE__)

#define G_PLAYER_FROM_ENT(ent) (game.players + (ent)->pnum)

/** @brief this structure is left intact through an entire game
 * it should be initialized at game library load time */
typedef struct {
	player_t *players;			/* [maxplayers] */

	/* store latched cvars here that we want to get at often */
	int sv_maxplayersperteam;
	int sv_maxentities;

	inventoryInterface_t i;
} game_locals_t;

/** @brief this structure is cleared as each map is entered */
typedef struct {
	int framenum;		/**< the current frame (10fps) */
	float time;			/**< seconds the game is running already
						 * calculated through framenum * SERVER_FRAME_SECONDS */

	char mapname[MAX_QPATH];	/**< the server name (base1, etc) */
	char *mapEndCommand;
	qboolean routed;
	qboolean day;
	qboolean hurtAliens;
	qboolean nextMapSwitch;		/**< trigger the nextmap command when ending the match */

	/* intermission state */
	float intermissionTime;		/**< the seconds to wait until the game will be closed.
								 * This value is relative to @c level.time
								 * @sa G_MatchDoEnd */
	int winningTeam;			/**< the team that won this match */
	float roundstartTime;		/**< the time the team started the turn */

	/* turn statistics */
	int numplayers;
	int activeTeam;
	int nextEndRound;
	int actualRound;	/**< the current running turn counter */

	pathing_t *pathingMap;	/**< This is where the data for TUS used to move and actor locations go */

	int randomSpawn;	/**< can be set via worldspawn to force random spawn point order for each team */
	int noEquipment;	/**< can be set via worldspawn to force the players to collect their equipment in the map */

	int initialAlienActorsSpawned;
	byte num_alive[MAX_TEAMS];		/**< the number of alive actors per team */
	byte num_spawned[MAX_TEAMS];	/**< the number of spawned actors per team */
	byte num_spawnpoints[MAX_TEAMS];	/**< the number of spawn points in the map per team */
	byte num_2x2spawnpoints[MAX_TEAMS]; /**< the number of spawn points for 2x2 units in the map per team */
	byte num_kills[MAX_TEAMS][MAX_TEAMS];	/**< the amount of kills per team, the first dimension contains the attacker team, the second the victim team */
	byte num_stuns[MAX_TEAMS][MAX_TEAMS];	/**< the amount of stuns per team, the first dimension contains the attacker team, the second the victim team */
} level_locals_t;


/**
 * @brief this is only used to hold entity field values that can be set from
 * the editor, but aren't actually present in edict_t during gameplay
 */
typedef struct {
	/* world vars */
	int randomSpawn;	/**< spawn the actors on random spawnpoints */
	int noEquipment;	/**< spawn the actors with no equipment - must be collected in the map */
} spawn_temp_t;

/** @brief used in shot probability calculations (pseudo shots) */
typedef struct {
	int enemyCount;			/**< shot would hit that much enemies */
	int friendCount;		/**< shot would hit that much friends */
	int civilian;			/**< shot would hit that much civilians */
	int self;				/**< @todo incorrect actor facing or shotOrg, or bug in trace code? */
	int damage;
	qboolean allow_self;
} shot_mock_t;

extern game_locals_t game;
extern level_locals_t level;
extern game_import_t gi;
extern game_export_t globals;

#define random()	((rand() & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

#define G_IsState(ent, s)		((ent)->state & (s))
#define G_IsShaken(ent)			G_IsState(ent, STATE_SHAKEN)
#define G_IsStunned(ent)		(G_IsState(ent, STATE_STUN) & ~STATE_DEAD)
#define G_IsPaniced(ent)		G_IsState(ent, STATE_PANIC)
#define G_IsReaction(ent)		G_IsState(ent, STATE_REACTION)
#define G_IsRaged(ent)			G_IsState(ent, STATE_RAGE)
#define G_IsInsane(ent)			G_IsState(ent, STATE_INSANE)
#define G_IsDazed(ent)			G_IsState(ent, STATE_DAZED)
#define G_IsCrouched(ent)		G_IsState(ent, STATE_CROUCHED)
/** @note This check also includes the IsStunned check - see the STATE_* bitmasks */
#define G_IsDead(ent)			G_IsState(ent, STATE_DEAD)
#define G_IsActor(ent)			((ent)->type == ET_ACTOR || (ent)->type == ET_ACTOR2x2)
#define G_IsSmoke(ent)			((ent)->type == ET_SMOKE)
#define G_IsFire(ent)			((ent)->type == ET_FIRE)
#define G_IsTriggerNextMap(ent)	((ent)->type == ET_TRIGGER_NEXTMAP)
#define G_IsItem(ent)			((ent)->type == ET_ITEM)
#define G_IsDoor(ent)			((ent)->type == ET_DOOR || (ent)->type == ET_DOOR_SLIDING)

#define G_IsBreakable(ent)		((ent)->flags & FL_DESTROYABLE)
#define G_IsBrushModel(ent)		((ent)->type == ET_BREAKABLE || G_IsDoor(ent) || (ent)->type == ET_ROTATING)
/** @note Every none solid (none-bmodel) edict that is visible for the client */
#define G_IsVisibleOnBattlefield(ent)	(G_IsActor((ent)) || (ent)->type == ET_ITEM || (ent)->type == ET_PARTICLE)
#define G_IsAI(ent)				(G_PLAYER_FROM_ENT((ent))->pers.ai)
#define G_IsAIPlayer(player)	((player)->pers.ai)
#define G_TeamToVisMask(team)	(1 << (team))
#define G_IsVisibleForTeam(ent, team) ((ent)->visflags & G_TeamToVisMask(team))
/** @note check for actor first */
#define G_IsCivilian(ent)		((ent)->team == TEAM_CIVILIAN)
#define G_IsAlien(ent)			((ent)->team == TEAM_ALIEN)
#define G_IsBlockingMovementActor(ent)	(((ent)->type == ET_ACTOR && !G_IsDead(ent)) || ent->type == ET_ACTOR2x2)

#define G_ToggleState(ent, s)	(ent)->state ^= (s)
#define G_ToggleCrouched(ent)	G_ToggleState(ent, STATE_CROUCHED)

#define G_SetState(ent, s)		(ent)->state |= (s)
#define G_SetShaken(ent)		G_SetState((ent), STATE_SHAKEN)
#define G_SetDazed(ent)			G_SetState((ent), STATE_DAZED)
#define G_SetStunned(ent)		G_SetState((ent), STATE_STUN)
#define G_SetDead(ent)			G_SetState((ent), STATE_DEAD)
#define G_SetInsane(ent)		G_SetState((ent), STATE_INSANE)
#define G_SetRage(ent)			G_SetState((ent), STATE_RAGE)
#define G_SetPanic(ent)			G_SetState((ent), STATE_PANIC)
#define G_SetCrouched(ent)		G_SetState((ent), STATE_CROUCHED)

#define G_RemoveState(ent, s)	(ent)->state &= ~(s)
#define G_RemoveShaken(ent)		G_RemoveState((ent), STATE_SHAKEN)
#define G_RemoveDazed(ent)		G_RemoveState((ent), STATE_DAZED)
#define G_RemoveStunned(ent)	G_RemoveState((ent), STATE_STUN)
#define G_RemoveDead(ent)		G_RemoveState((ent), STATE_DEAD)
#define G_RemoveInsane(ent)		G_RemoveState((ent), STATE_INSANE)
#define G_RemoveRage(ent)		G_RemoveState((ent), STATE_RAGE)
#define G_RemovePanic(ent)		G_RemoveState((ent), STATE_PANIC)
#define G_RemoveCrouched(ent)	G_RemoveState((ent), STATE_CROUCHED)
#define G_RemoveReaction(ent)	G_RemoveState((ent), STATE_REACTION)

extern cvar_t *sv_maxentities;
extern cvar_t *password;
extern cvar_t *sv_needpass;
extern cvar_t *sv_dedicated;

extern cvar_t *logstats;
extern FILE *logstatsfile;

extern cvar_t *sv_filterban;

extern cvar_t *sv_maxvelocity;

extern cvar_t *sv_maxclients;
extern cvar_t *sv_shot_origin;
extern cvar_t *sv_hurtaliens;
extern cvar_t *sv_maxplayersperteam;
extern cvar_t *sv_maxsoldiersperteam;
extern cvar_t *sv_maxsoldiersperplayer;
extern cvar_t *sv_enablemorale;
extern cvar_t *sv_roundtimelimit;

extern cvar_t *sv_maxteams;

extern cvar_t *sv_ai;
extern cvar_t *sv_teamplay;

extern cvar_t *ai_alien;
extern cvar_t *ai_civilian;
extern cvar_t *ai_equipment;
extern cvar_t *ai_numaliens;
extern cvar_t *ai_numcivilians;
extern cvar_t *ai_numactors;

extern cvar_t *mob_death;
extern cvar_t *mob_wound;
extern cvar_t *mof_watching;
extern cvar_t *mof_teamkill;
extern cvar_t *mof_civilian;
extern cvar_t *mof_enemy;
extern cvar_t *mor_pain;

/*everyone gets this times morale damage */
extern cvar_t *mor_default;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_distance;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_victim;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t *mor_attacker;

/* how much the morale depends on the size of the damaged team */
extern cvar_t *mon_teamfactor;

extern cvar_t *mor_regeneration;
extern cvar_t *mor_shaken;
extern cvar_t *mor_panic;
extern cvar_t *mor_brave;

extern cvar_t *m_sanity;
extern cvar_t *m_rage;
extern cvar_t *m_rage_stop;
extern cvar_t *m_panic_stop;

extern cvar_t *g_endlessaliens;
extern cvar_t *g_ailua;
extern cvar_t *g_aidebug;
extern cvar_t *g_drawtraces;
extern cvar_t *g_nodamage;
extern cvar_t *g_notu;
extern cvar_t *g_reactionnew;
extern cvar_t *g_nospawn;
extern cvar_t *g_actorspeed;

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *g_difficulty;

/* fields are needed for spawning from the entity string */
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2

/* g_stats.c */
void G_SendPlayerStats(const player_t *player);

/* g_inventory.c */
void G_WriteItem(const item_t *item, const invDef_t *container, int x, int y);
void G_ReadItem(item_t *item, const invDef_t **container, int *x, int *y);
void G_InventoryToFloor(edict_t *ent);
qboolean G_AddItemToFloor(const pos3_t pos, const char *itemID);
edict_t *G_GetFloorItemsFromPos(const pos3_t pos);
const equipDef_t *G_GetEquipDefByID(const char *equipID);

/* g_morale */
void G_MoraleBehaviour(int team);

/* g_phys.c */
void G_PhysicsRun(void);
void G_PhysicsStep(edict_t *ent);

/* g_actor.c */
void G_ActorReserveTUs(edict_t *ent, int resReaction, int resShot, int resCrouch);
int G_ActorGetTUForReactionFire(const edict_t *ent);
int G_ActorUsableTUs(const edict_t *ent);
int G_ActorGetReservedTUs(const edict_t *ent);
void G_ActorCheckRevitalise(edict_t *ent);
int G_ActorCalculateMaxTU(const edict_t *ent);
int G_ActorGetArmourTUPenalty(const edict_t *ent);

/* g_mission.c */
qboolean G_MissionTouch(edict_t *self, edict_t *activator);
qboolean G_MissionUse(edict_t *self, edict_t *activator);
qboolean G_MissionDestroy(edict_t *self);
void G_MissionThink(edict_t *self);

/* g_utils.c */
uint32_t G_GetLevelFlagsFromPos(const pos3_t pos);
edict_t *G_Find(edict_t *from, int fieldofs, char *match);
edict_t *G_FindRadius(edict_t *from, const vec3_t org, float rad, entity_type_t type);
edict_t *G_FindTargetEntity(const char *target);
const char* G_GetPlayerName(int pnum);
player_t* G_GetPlayerForTeam(int team);
int G_GetActiveTeam(void);
const char* G_GetWeaponNameForFiredef(const fireDef_t *fd);
void G_PrintActorStats(const edict_t *victim, const edict_t *attacker, const fireDef_t *fd);
void G_PrintStats(const char *buffer);
int G_TouchTriggers(edict_t *ent);
int G_TouchSolids(edict_t *ent, float extend);
void G_TouchEdicts(edict_t *ent, float extend);
edict_t *G_Spawn(void);
void G_SpawnSmokeField(const vec3_t vec, const char *particle, int rounds);
void G_SpawnFireField(const vec3_t vec, const char *particle, int rounds, int damage);
edict_t *G_SpawnParticle(const vec3_t origin, int spawnflags, const char *particle);
void G_FreeEdict(edict_t *e);
qboolean G_UseEdict(edict_t *ent, edict_t* activator);
edict_t *G_GetEdictFromPos(const pos3_t pos, const entity_type_t type);
edict_t *G_GetLivingActorFromPos(const pos3_t pos);
edict_t *G_GetEdictFromPosExcluding(const pos3_t pos, const int n, ...);
void G_TakeDamage(edict_t *ent, int damage);
trace_t G_Trace(const vec3_t start, const vec3_t end, const edict_t * passent, int contentmask);
qboolean G_TestLineWithEnts(const vec3_t start, const vec3_t end);
qboolean G_TestLine(const vec3_t start, const vec3_t end);

/* g_combat.c */
void G_GetShotOrigin (const edict_t *shooter, const fireDef_t *fd, const vec3_t dir, vec3_t shotOrigin);

/* g_reaction.c */
void G_ReactionFirePreShot(const edict_t *target, const int fdTime);
void G_ReactionFirePostShot(edict_t *target);
void G_ReactionFireReset(int team);
void G_ReactionFireUpdate(edict_t *ent, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t *od);
qboolean G_ReactionFireSettingsReserveTUs(edict_t *ent);
qboolean G_ReactionFireOnMovement(edict_t *target);
void G_ReactionFireOnEndTurn(void);
void G_ReactionFireTargetsInit (void);
void G_ReactionFireTargetsCreate (const edict_t *shooter);

void G_CompleteRecalcRouting(void);
void G_RecalcRouting(const char *model);
void G_GenerateEntList(const char *entList[MAX_EDICTS]);

/** @todo make this a byte */
typedef unsigned int vismask_t;

#include "g_events.h"

/* g_vis.c */
#define VIS_APPEAR	1
#define VIS_PERISH	2

/** the visibility changed - if it was visible - it's (the edict) now invisible */
#define VIS_CHANGE	1
/** actor visible? */
#define VIS_YES		2
/** stop the current action if actor appears */
#define VIS_STOP	4

/* g_client.c */
/** check whether edict is still visible - it maybe is currently visible but this
 * might have changed due to some action */
#define VT_PERISH		1
/** don't perform a frustum vis check via G_FrustumVis in G_Vis */
#define VT_NOFRUSTUM	2

#define MORALE_RANDOM( mod )	( (mod) * (1.0 + 0.3*crand()) )

#define MAX_DVTAB 32

edict_t* G_ClientGetFreeSpawnPointForActorSize(const player_t *player, const actorSizeEnum_t actorSize);
qboolean G_ClientUseEdict(const player_t *player, edict_t *actor, edict_t *door);
qboolean G_ActionCheckForCurrentTeam(const player_t *player, edict_t *ent, int TU);
qboolean G_ActionCheckForReaction(const player_t *player, edict_t *ent, int TU);
void G_SendStats(edict_t *ent) __attribute__((nonnull));
edict_t *G_SpawnFloor(const pos3_t pos);
int G_CheckVisTeam(const int team, edict_t *check, qboolean perish, const edict_t *ent);
int G_CheckVisTeamAll(const int team, qboolean perish, const edict_t *ent);
edict_t *G_GetFloorItems(edict_t *ent) __attribute__((nonnull));
qboolean G_InventoryRemoveItemByID(const char *itemID, edict_t *ent, containerIndex_t index);
qboolean G_SetTeamForPlayer(player_t* player, const int team);

qboolean G_ActorIsInRescueZone(const edict_t* actor);
void G_ActorSetInRescueZone(edict_t* actor, qboolean inRescueZone);
void G_ActorUseDoor(edict_t *actor, edict_t *door);
qboolean G_IsLivingActor(const edict_t *ent) __attribute__((nonnull));
void G_ActorSetClientAction(edict_t *actor, edict_t *ent);
edict_t *G_ActorGetByUCN(const int ucn, const int team);
void G_CheckForceEndRound(void);
qboolean G_ActorDieOrStun(edict_t *ent, edict_t *attacker);
int G_ActorCountAlive(int team);
void G_ActorSetMaxs(edict_t* ent);
void G_ActorGiveTimeUnits(edict_t *ent);
void G_ActorSetTU(edict_t *ent, int tus);
void G_ActorUseTU(edict_t *ent, int tus);
void G_ActorModifyCounters(const edict_t *attacker, const edict_t *victim, int deltaAlive, int deltaKills, int deltaStuns);
void G_ActorGetEyeVector(const edict_t *actor, vec3_t eye);
int G_ClientAction(player_t * player);
void G_ClientEndRound(player_t * player);
void G_ClientTeamInfo(const player_t * player);
void G_ClientInitActorStates(const player_t * player);
int G_ClientGetTeamNum(const player_t * player);
int G_ClientGetTeamNumPref(const player_t * player);
qboolean G_ClientIsReady(const player_t * player);
void G_ClientPrintf(const player_t * player, int printlevel, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void G_ResetClientData(void);

void G_ClientCommand(player_t * player);
void G_ClientUserinfoChanged(player_t * player, const char *userinfo);
qboolean G_ClientBegin(player_t * player);
void G_ClientStartMatch(player_t * player);
qboolean G_ClientConnect(player_t * player, char *userinfo, size_t userinfoSize);
void G_ClientDisconnect(player_t * player);

void G_ActorReload(edict_t* ent, const invDef_t *invDef);
qboolean G_ClientCanReload(edict_t *ent, containerIndex_t containerID);
void G_ClientGetWeaponFromInventory(edict_t *ent);
void G_ClientMove(const player_t * player, int visTeam, edict_t* ent, const pos3_t to);
void G_ActorFall(edict_t *ent);
void G_MoveCalc(int team, const edict_t *movingActor, const pos3_t from, byte crouchingState, int distance);
void G_MoveCalcLocal(pathing_t *pt, int team, const edict_t *movingActor, const pos3_t from, byte crouchingState, int distance);
qboolean G_ActorInvMove(edict_t *ent, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx, int ty, qboolean checkaction);
void G_ClientStateChange(const player_t* player, edict_t* ent, int reqState, qboolean checkaction);
int G_ActorDoTurn(edict_t * ent, byte dir);

void G_SendInvisible(const player_t *player);
void G_GiveTimeUnits(int team);

void G_AppearPerishEvent(unsigned int player_mask, qboolean appear, edict_t * check, const edict_t *ent);
unsigned int G_VisToPM(vismask_t vis_mask);
vismask_t G_PMToVis(unsigned int playerMask);
void G_SendInventory(unsigned int player_mask, const edict_t * ent);
unsigned int G_TeamToPM(int team);

player_t* G_PlayerGetNextHuman(player_t* lastPlayer);
player_t* G_PlayerGetNextActiveHuman(player_t* lastPlayer);
player_t* G_PlayerGetNextAI(player_t* lastPlayer);
player_t* G_PlayerGetNextActiveAI(player_t* lastPlayer);

void G_SpawnEntities(const char *mapname, qboolean day, const char *entities);
qboolean G_RunFrame(void);

#ifdef DEBUG
void G_InvList_f(const player_t *player);
#endif

/* g_vis.c */
qboolean G_FrustumVis(const edict_t *from, const vec3_t point);
float G_ActorVis(const vec3_t from, const edict_t *ent, const edict_t *check, qboolean full);
void G_VisFlagsClear(int team);
void G_VisFlagsAdd(edict_t *ent, vismask_t visMask);
void G_VisFlagsSwap(edict_t *ent, vismask_t visMask);
void G_VisFlagsReset(edict_t *ent);
void G_VisMakeEverythingVisible(void);
int G_CheckVis(edict_t *check, qboolean perish);
int G_CheckVisPlayer(player_t* player, qboolean perish);
int G_TestVis(const int team, edict_t * check, int flags);
qboolean G_Vis(const int team, const edict_t * from, const edict_t * check, int flags);

/* g_combat.c */
qboolean G_ClientShoot(const player_t *player, edict_t* ent, const pos3_t at, shoot_types_t shootType, fireDefIndex_t firemode, shot_mock_t *mock, qboolean allowReaction, int z_align);
void G_CheckDeathOrKnockout(edict_t *target, edict_t *attacker, const fireDef_t *fd, int damage);

/* g_ai.c */
void AI_Init(void);
void AI_CheckRespawn(int team);
extern edict_t *ai_waypointList;
void G_AddToWayPointList(edict_t *ent);
void AI_Run(void);
void AI_ActorThink(player_t *player, edict_t *ent);
player_t *AI_CreatePlayer(int team);
qboolean AI_CheckUsingDoor(const edict_t *ent, const edict_t *door);

/* g_svcmds.c */
void G_ServerCommand(void);
qboolean SV_FilterPacket(const char *from);

/* g_match.c */
qboolean G_MatchIsRunning(void);
void G_MatchEndTrigger(int team, int timeGap);
void G_MatchEndCheck(void);
qboolean G_MatchDoEnd(void);

/* g_trigger.c */
edict_t* G_TriggerSpawn(edict_t *owner);
qboolean G_TriggerRemoveFromList(edict_t *self, edict_t *activator);
qboolean G_TriggerIsInList(edict_t *self, edict_t *activator);
void G_TriggerAddToList(edict_t *self, edict_t *activator);
void SP_trigger_nextmap(edict_t *ent);
void SP_trigger_hurt(edict_t *ent);
void SP_trigger_touch(edict_t *ent);
void SP_trigger_rescue(edict_t *ent);

qboolean Touch_HurtTrigger(edict_t *self, edict_t *activator);
void Think_NextMapTrigger(edict_t *self);

/* g_func.c */
void SP_func_rotating(edict_t *ent);
void SP_func_door(edict_t *ent);
void SP_func_door_sliding(edict_t *ent);
void SP_func_breakable(edict_t *ent);

/** Functions handling the storage and lifecycle of all edicts */
edict_t* G_EdictsInit(void);
void G_EdictsReset(void);
edict_t* G_EdictsGetNewEdict(void);
edict_t* G_EdictDuplicate(const edict_t *edict);
int G_EdictsGetNumber(const edict_t* ent);
qboolean G_EdictsIsValidNum(const int idx);
edict_t* G_EdictsGetByNum(const int num);
edict_t* G_EdictsGetFirst(void);
edict_t* G_EdictsGetNext(edict_t* lastEnt);
edict_t* G_EdictsGetNextInUse(edict_t* lastEnt);
edict_t* G_EdictsGetNextActor(edict_t* lastEnt);
edict_t* G_EdictsGetNextLivingActor(edict_t* lastEnt);
edict_t* G_EdictsGetNextLivingActorOfTeam(edict_t* lastEnt, const int team);
edict_t* G_EdictsGetTriggerNextMaps(edict_t* lastEnt);

/** Functions to handle single edicts, trying to encapsulate edict->pos in the first place. */
void G_EdictCalcOrigin(edict_t* ent);
void G_EdictSetOrigin(edict_t* ent, const pos3_t newPos);
qboolean G_EdictPosIsSameAs(edict_t* ent, const pos3_t cmpPos);

/*============================================================================ */

/** @brief e.g. used for breakable objects */
typedef enum {
	MAT_GLASS,		/* default */
	MAT_METAL,
	MAT_ELECTRICAL,
	MAT_WOOD,

	MAT_MAX
} edictMaterial_t;

/** @brief actor movement */
typedef struct {
	int			contentFlags[MAX_DVTAB];
	vismask_t	visflags[MAX_DVTAB];
	byte		steps;
	int			currentStep;
} moveinfo_t;

/** @brief client data that stays across multiple level loads */
typedef struct {
	char userinfo[MAX_INFO_STRING];
	char netname[16];

	/** the number of the team for this player
	 * 0 is reserved for civilians and critters */
	int team;
	qboolean ai;				/**< client controlled by ai */

	/** ai specific data */
	edict_t *last; /**< set to the last actor edict that was handled for the ai in their think function */

	float		flood_locktill;		/**< locked from talking */
	float		flood_when[10];		/**< when messages were said */
	int			flood_whenhead;		/**< head pointer for when said */
} client_persistant_t;

/** @brief this structure is cleared on each PutClientInServer(),
 * except for 'client->pers'
 * @note shared between game and server - but server doesn't know all the fields */
struct player_s {
	/* known to server */
	qboolean inuse;
	int num;
	qboolean isReady;			/**< the player agreed to start the party */

	/* private to game */
	qboolean spawned;			/**< already spawned? */
	qboolean began;				/**< the player sent his 'begin' already */
	qboolean roundDone;			/**< ready to end his turn */

	int reactionLeftover;		/**< Minimum TU left over by reaction fire */
	qboolean autostand;			/**< autostand for long walks */

	client_persistant_t pers;
};

/**
 * @brief If an edict is destroyable (like ET_BREAKABLE, ET_DOOR [if health set]
 * or maybe a ET_MISSION [if health set])
 * @note e.g. misc_mission, func_breakable, func_door
 * @note If you mark an edict as breakable, you have to provide a destroy callback, too
 */
#define FL_DESTROYABLE	0x00000004
/**
 * @brief not the first on the team
 * @sa groupMaster and groupChain
 */
#define FL_GROUPSLAVE	0x00000008
/**
 * @brief Edict flag to indicate, that the edict can be used in the context of a client action
 */
#define FL_CLIENTACTION	0x00000010
/**
 * @brief Trigger the edict at spawn.
 */
#define FL_TRIGGERED	0x00000100

/** @brief Artificial intelligence of a character */
typedef struct AI_s {
	char type[MAX_QPATH];	/**< Lua file used by the AI. */
	char subtype[MAX_VAR];	/**< Subtype to be used by AI. */
	lua_State* L;			/**< The lua state used by the AI. */
} AI_t;

/**
 * @brief Everything that is not in the bsp tree is an edict, the spawnpoints,
 * the actors, the misc_models, the weapons and so on.
 */
struct edict_s {
	qboolean inuse;
	int linkcount;		/**< count the amount of server side links - if a link was called,
						 * something on the position or the size of the entity was changed */

	int number;			/**< the number in the global edict array */

	vec3_t origin;		/**< the position in the world */
	vec3_t angles;		/**< the rotation in the world (pitch, yaw, roll) */

	/** tracing info SOLID_BSP, SOLID_BBOX, ... */
	solid_t solid;

	vec3_t mins, maxs;		/**< position of min and max points - relative to origin */
	vec3_t absmin, absmax;	/**< position of min and max points - relative to world's origin */
	vec3_t size;

	edict_t *child;	/**< e.g. the trigger for this edict */
	edict_t *owner;	/**< e.g. the door model in case of func_door */
	int modelindex;	/**< inline model index */
	const char *classname;

	/*================================ */
	/* don't change anything above here - the server expects the fields in that order */
	/*================================ */

	int mapNum;			/**< entity number in the map file */
	const char *model;	/**< inline model (start with * and is followed by the model numberer)
						 * or misc_model model name (e.g. md2) */

	/** only used locally in game, not by server */

	edict_t *particleLink;
	const edict_t *link;		/**< can be used to store another edict that e.g. interacts with the current one */
	entity_type_t type;
	vismask_t visflags;			/**< bitmask of teams that can see this edict */

	int contentFlags;			/**< contents flags of the brush the actor is walking in */

	pos3_t pos;					/**< the position on the grid @sa @c UNIT_SIZE */
	byte dir;					/**< direction the player looks at */

	int TU;						/**< remaining timeunits for actors or timeunits needed to 'use' this entity */
	int HP;						/**< remaining healthpoints */
	int STUN;					/**< The stun damage received in a mission. */
	int morale;					/**< the current morale value */

	int state;					/**< the player state - dead, shaken.... */

	int team;					/**< player of which team? */
	int pnum;					/**< the actual player slot */
	/** the model indices */
	unsigned int body;
	unsigned int head;
	int frame;					/**< frame of the model to show */

	char *group;				/**< this can be used to trigger a group of entities
								 * e.g. for two-part-doors - set the group to the same
								 * string for each door part and they will open both
								 * if you open one */

	/** delayed reaction fire */
	const edict_t *reactionTarget;	/**< the moving actor who triggered the RF of this ent - only set when this actor has reaction fire enabled */
	float reactionTUs;				/**< TU value of the target at which reaction will occur */
	qboolean reactionNoDraw;
	qboolean inRescueZone;			/**< the actor is standing in a rescue zone if this is true - this means that
									 * when the mission is aborted the actor will not die */

	/** client actions - interact with the world */
	edict_t *clientAction;

	/** here are the character values */
	character_t chr;

	int spawnflags;	/**< set via mapeditor */

	float angle;	/**< entity yaw - (0-360 degree) set via mapeditor - sometimes used for movement direction,
					 * then -1=up; -2=down is used additionally */

	int radius;					/**< this is used to extend the bounding box of a trigger_touch for e.g. misc_mission */
	int speed;					/**< speed of entities - e.g. rotating or actors */
	const char *target;			/**< name of the entity to trigger or move towards - this name is stored in the target edicts targetname value */
	const char *targetname;		/**< name pointed to by target - see the target of the parent edict */
	const char *item;			/**< the item id that must be placed to e.g. the func_mission to activate the use function */
	const char *particle;
	const char *nextmap;
	const char *message;		/**< misc_message */
	const char *noise;			/**< sounds - e.g. for func_door */
	edictMaterial_t material;	/**< material value (e.g. for func_breakable) */
	int count;		/**< general purpose 'amount' variable - set via mapeditor often */
	int time;		/**< general purpose 'rounds' variable - set via mapeditor often */
	int sounds;		/**< type of sounds to play - e.g. doors */
	int dmg;		/**< damage done by entity */
	/** @sa memcpy in Grid_CheckForbidden */
	actorSizeEnum_t fieldSize;	/* ACTOR_SIZE_* */
	qboolean hiding;		/**< for ai actors - when they try to hide after the performed their action */

	/** function to call when triggered - this function should only return true when there is
	 * a client action associated with it */
	qboolean (*touch)(edict_t * self, edict_t * activator);
	/** reset function that is called before the touch triggers are called */
	void (*reset)(edict_t * self, edict_t * activator);
	float nextthink;
	void (*think)(edict_t *self);
	/** general use function that is called when the triggered client action is executed
	 * or when the server has to 'use' the entity
	 * @param activator Might be @c NULL if there is no activator */
	qboolean (*use)(edict_t *self, edict_t *activator);
	qboolean (*destroy)(edict_t *self);

	edict_t *touchedNext;	/**< entity list of edict that are currently touching the trigger_touch */
	int doorState;			/**< open or closed */

	moveinfo_t		moveinfo;

	/** flags will have FL_GROUPSLAVE set when the edict is part of a chain,
	 * but not the master - you can use the groupChain pointer to get all the
	 * edicts in the particular chain - and start out for the on that doesn't
	 * have the above mentioned flag set.
	 * @sa G_FindEdictGroups */
	edict_t *groupChain;
	edict_t *groupMaster;	/**< first entry in the list */
	int flags;				/**< FL_* */

	AI_t AI; /**< The character's artificial intelligence */

	pos3_t *forbiddenListPos;	/**< this is used for e.g. misc_models with the solid flag set - this will
								 * hold a list of grid positions that are blocked by the aabb of the model */
	int forbiddenListSize;		/**< amount of entries in the forbiddenListPos */
};

#endif /* GAME_G_LOCAL_H */
