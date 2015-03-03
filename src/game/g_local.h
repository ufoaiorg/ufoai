/**
 * @file
 * @brief Local definitions for game module
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "q_shared.h"
#include "inventory.h"				/* for InventoryInterface in game_locals_t */
#include "../shared/infostring.h"
extern "C" {
#include <lua.h>
}

/** no gettext support for game lib - but we must be able to mark the strings */
#define _(String) String
#define ngettext(x, y, cnt) x

/** @note define GAME_INCLUDE so that game.h does not define the
 * short, server-visible player_t and Edict structures,
 * because we define the full size ones in this file */
#define	GAME_INCLUDE
#include "game.h"

/** the "gameversion" client command will print this plus compile date */
#define	GAMEVERSION	"baseufo"

#define MAX_SPOT_DIST_CAMERA	768
#define MAX_SPOT_DIST	4096

/** server is running at 10 fps */
#define	SERVER_FRAME_SECONDS		0.1

/** memory tags to allow dynamic memory to be cleaned up */
#define	TAG_GAME	765			/* clear when unloading the game library */
#define	TAG_LEVEL	766			/* clear when loading a new level */

#define INVDEF(containerID) (&gi.csi->ids[(containerID)])

#define G_FreeTags(tag) gi.FreeTags((tag), __FILE__, __LINE__)
#define G_TagMalloc(size, tag) gi.TagMalloc((size), (tag), __FILE__, __LINE__)
#define G_MemFree(ptr) gi.TagFree((ptr), __FILE__, __LINE__)

/** @brief this structure is left intact through an entire game
 * it should be initialized at game library load time */
typedef struct game_locals_s {
	Player* players;			/* [maxplayers] */

	/* store latched cvars here that we want to get at often */
	int sv_maxplayersperteam;
	int sv_maxentities;

	InventoryInterface invi;
} game_locals_t;

/** @brief this structure is cleared as each map is entered */
typedef struct level_locals_s {
	int framenum;		/**< the current frame (10fps) */
	float time;			/**< seconds the game is running already
						 * calculated through framenum * SERVER_FRAME_SECONDS */

	char mapname[MAX_QPATH];	/**< the server name (base1, etc) */
	char* mapEndCommand;
	bool routed;
	bool day;
	bool hurtAliens;
	bool nextMapSwitch;			/**< trigger the nextmap command when ending the match */

	/* intermission state */
	float intermissionTime;		/**< the seconds to wait until the game will be closed.
								 * This value is relative to @c level.time
								 * @sa G_MatchDoEnd */
	int winningTeam;			/**< the team that won this match */
	float roundstartTime;		/**< the time the team started the turn */

	/* turn statistics */
	int numplayers;
	int activeTeam;
	int teamOfs;
	int nextEndRound;
	int actualRound;		/**< the current running round counter */

	pathing_t* pathingMap;	/**< This is where the data for TUS used to move and actor locations go */

	int noRandomSpawn;		/**< can be set via worldspawn to force random spawn point order for each team */
	int noEquipment;		/**< can be set via worldspawn to force the players to collect their equipment in the map */

	int initialAlienActorsSpawned;
	byte num_alive[MAX_TEAMS];				/**< the number of alive actors per team */
	byte num_spawned[MAX_TEAMS];			/**< the number of spawned actors per team */
	byte num_spawnpoints[MAX_TEAMS];		/**< the number of spawn points in the map per team */
	byte num_2x2spawnpoints[MAX_TEAMS]; 	/**< the number of spawn points for 2x2 units in the map per team */
	byte num_kills[MAX_TEAMS + 1][MAX_TEAMS];	/**< the amount of kills per team, the first dimension contains the attacker team, the second the victim team */
	byte num_stuns[MAX_TEAMS + 1][MAX_TEAMS];	/**< the amount of stuns per team, the first dimension contains the attacker team, the second the victim team */
	Edict* ai_waypointList;
} level_locals_t;


extern game_locals_t game;
extern level_locals_t level;
extern game_import_t gi;
extern game_export_t globals;

#define G_IsActor(ent)			((ent)->type == ET_ACTOR || (ent)->type == ET_ACTOR2x2)
#define G_IsActive(ent)			((ent)->active)
#define G_IsCamera(ent)			((ent)->type == ET_CAMERA)
#define G_IsActiveCamera(ent)	(G_IsCamera(ent) && G_IsActive(ent))
#define G_IsSmoke(ent)			((ent)->type == ET_SMOKE)
#define G_IsFire(ent)			((ent)->type == ET_FIRE)
#define G_IsTriggerNextMap(ent)	((ent)->type == ET_TRIGGER_NEXTMAP)
#define G_IsItem(ent)			((ent)->type == ET_ITEM)
#define G_IsDoor(ent)			((ent)->type == ET_DOOR || (ent)->type == ET_DOOR_SLIDING)

#define G_IsBreakable(ent)		((ent)->flags & FL_DESTROYABLE)
#define G_IsBrushModel(ent)		((ent)->type == ET_BREAKABLE || G_IsDoor(ent) || (ent)->type == ET_ROTATING)
/** @note Every none solid (none-bmodel) edict that is visible for the client */
#define G_IsVisibleOnBattlefield(ent)	(G_IsActor((ent)) || G_IsItem(ent) || G_IsCamera(ent) || (ent)->type == ET_PARTICLE)
#define G_IsAI(ent)				((ent)->getPlayer().pers.ai)
#define G_IsAIPlayer(player)	((player)->pers.ai)
#define G_TeamToVisMask(team)	(1 << (team))
#define G_IsVisibleForTeam(ent, team) ((ent)->visflags & G_TeamToVisMask(team))
#define G_IsMultiPlayer()		(sv_maxclients->integer > 1)
#define G_IsSinglePlayer()		(!G_IsMultiPlayer())
/** @note check for actor first */
#define G_IsCivilian(ent)		((ent)->getTeam() == TEAM_CIVILIAN)
#define G_IsAlien(ent)			((ent)->getTeam() == TEAM_ALIEN)
#define G_IsBlockingMovementActor(ent)	(((ent)->type == ET_ACTOR && !G_IsDead(ent)) || ent->type == ET_ACTOR2x2)

extern cvar_t* sv_maxentities;
extern cvar_t* password;
extern cvar_t* sv_needpass;
extern cvar_t* sv_dedicated;
extern cvar_t* developer;

extern cvar_t* logstats;
extern FILE* logstatsfile;

extern cvar_t* sv_filterban;

extern cvar_t* sv_maxvelocity;

extern cvar_t* sv_maxclients;
extern cvar_t* sv_shot_origin;
extern cvar_t* sv_hurtaliens;
extern cvar_t* sv_maxplayersperteam;
extern cvar_t* sv_maxsoldiersperteam;
extern cvar_t* sv_maxsoldiersperplayer;
extern cvar_t* sv_enablemorale;
extern cvar_t* sv_roundtimelimit;

extern cvar_t* sv_maxteams;

extern cvar_t* sv_ai;
extern cvar_t* sv_teamplay;

extern cvar_t* ai_alienteam;
extern cvar_t* ai_civilianteam;
extern cvar_t* ai_equipment;
extern cvar_t* ai_singleplayeraliens;
extern cvar_t* ai_numcivilians;
extern cvar_t* ai_multiplayeraliens;

extern cvar_t* mob_death;
extern cvar_t* mob_wound;
extern cvar_t* mob_shoot;
extern cvar_t* mof_watching;
extern cvar_t* mof_teamkill;
extern cvar_t* mof_civilian;
extern cvar_t* mof_enemy;
extern cvar_t* mor_pain;

/*everyone gets this times morale damage */
extern cvar_t* mor_default;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t* mor_distance;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t* mor_victim;

/* at this distance the following two get halfed (exponential scale) */
extern cvar_t* mor_attacker;

/* how much the morale depends on the size of the damaged team */
extern cvar_t* mon_teamfactor;

extern cvar_t* mor_regeneration;
extern cvar_t* mor_shaken;
extern cvar_t* mor_panic;
extern cvar_t* mor_brave;

extern cvar_t* m_sanity;
extern cvar_t* m_rage;
extern cvar_t* m_rage_stop;
extern cvar_t* m_panic_stop;

extern cvar_t* g_endlessaliens;
extern cvar_t* g_ailua;
extern cvar_t* g_aihumans;
extern cvar_t* g_aidebug;
extern cvar_t* g_drawtraces;
extern cvar_t* g_nodamage;
extern cvar_t* g_notu;
extern cvar_t* g_nospawn;
extern cvar_t* g_actorspeed;
extern cvar_t* g_lastseen;

extern cvar_t* flood_msgs;
extern cvar_t* flood_persecond;
extern cvar_t* flood_waitdelay;

extern cvar_t* g_difficulty;

/* g_camera */
void G_InitCamera(Edict* ent, camera_type_t cameraType, float angle, bool rotate);
Edict* G_SpawnCamera(const vec3_t origin, int team, camera_type_t cameraType);

/* g_cmds.c */
void G_ClientCommand(Player& player);
#ifdef DEBUG
void G_InvList_f(const Player& player);
#endif

/* g_morale */
void G_MoraleBehaviour(int team);
#define MORALE_RANDOM( mod )	( (mod) * (1.0 + 0.3*crand()) )

/* g_round */
void G_CheckForceEndRound(void);
void G_ClientEndRound(Player& player);

/* g_stats */
void G_SendStats(Edict& ent);
void G_SendPlayerStats(const Player& player);

/* g_svcmds.c */
void G_ServerCommand(void);
bool SV_FilterPacket(const char* from);

#include "g_events.h"

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
typedef struct moveinfo_s {
	byte		steps;
} moveinfo_t;

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
} AI_t;

typedef struct camera_edict_data_s {
	camera_type_t cameraType;
	bool rotate;
} camera_edict_data_t;

#include "g_edict.h"
