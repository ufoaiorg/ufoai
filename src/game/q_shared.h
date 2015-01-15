/**
 * @file
 * @brief Common header file.
 *
 * @todo Apparently included by every file - unnecessary?
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/q_shared.h
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

#include "../shared/ufotypes.h"
#include "../shared/shared.h"
#include "../shared/mathlib.h"
#include "../shared/defines.h"
#include "../common/list.h"

#include "inv_shared.h"
#include "chr_shared.h"
#include "q_sizes.h"

#ifdef DEDICATED_ONLY
/* no gettext support for dedicated servers */
# define _(String) String
# define ngettext(x, y, cnt) x
#endif

/*
==============================================================
SYSTEM SPECIFIC
==============================================================
*/

/* this is only here so the functions in q_shared.c can link */
void Sys_Error(const char* error, ...) __attribute__((noreturn, format(__printf__, 1, 2)));
void Com_Printf(const char* msg, ...) __attribute__((format(__printf__, 1, 2)));
void Com_DPrintf(int level, const char* msg, ...) __attribute__((format(__printf__, 2, 3)));

#define TEAM_NO_ACTIVE -1
#define TEAM_CIVILIAN   0
#define TEAM_PHALANX    1
#define TEAM_ALIEN      7
#define TEAM_MAX_HUMAN	TEAM_ALIEN - 1

/*
==========================================================
ELEMENTS COMMUNICATED ACROSS THE NET
==========================================================
*/

/** add this flag for instant event execution */
#define EVENT_INSTANTLY		0x80

/**
 * @brief Possible event values
 * @sa cl_parse.c for event bindings
 */
typedef enum {
	EV_NULL,
	EV_RESET,
	EV_START,
	EV_ENDROUND,	/**< ends the current team's turn CL_DoEndRound */
	EV_ENDROUNDANNOUNCE,

	EV_RESULTS,
	EV_CENTERVIEW,
	EV_MOVECAMERA,

	EV_ENT_APPEAR,
	EV_ENT_PERISH,	/**< empty container or destroy inventory - set le invis to true
		* see CL_EntPerish */
	EV_ENT_DESTROY,
	EV_ADD_BRUSH_MODEL,
	EV_ADD_EDICT,

	EV_ACTOR_APPEAR,
	EV_ACTOR_ADD,
	EV_ACTOR_TURN,			/**< turn an actor around */
	EV_ACTOR_MOVE,
	EV_ACTOR_REACTIONFIRECHANGE,
	EV_ACTOR_REACTIONFIREADDTARGET,
	EV_ACTOR_REACTIONFIREREMOVETARGET,
	EV_ACTOR_REACTIONFIRETARGETUPDATE,
	EV_ACTOR_REACTIONFIREABORTSHOT,

	EV_ACTOR_START_SHOOT,
	EV_ACTOR_SHOOT,
	EV_ACTOR_SHOOT_HIDDEN,
	EV_ACTOR_THROW,
	EV_ACTOR_END_SHOOT,

	EV_ACTOR_DIE,
	EV_ACTOR_REVITALISED,
	EV_ACTOR_STATS,
	EV_ACTOR_STATECHANGE,	/**< set an actor to crouched or reaction fire */
	EV_ACTOR_RESERVATIONCHANGE,
	EV_ACTOR_WOUND,

	EV_INV_ADD,
	EV_INV_DEL,
	EV_INV_AMMO,
	EV_INV_RELOAD,
	EV_INV_TRANSFER,

	/* func_breakables */
	EV_MODEL_EXPLODE,		/**< delay the event execution by the impact time */
	EV_MODEL_EXPLODE_TRIGGERED,	/**< don't delay the model explode event */

	EV_PARTICLE_APPEAR,
	EV_PARTICLE_SPAWN,

	EV_SOUND,

	EV_DOOR_OPEN,
	EV_DOOR_CLOSE,
	EV_CLIENT_ACTION,
	EV_RESET_CLIENT_ACTION,

	EV_CAMERA_APPEAR,

	EV_NUM_EVENTS
} event_t;

typedef enum {
	ET_NULL,
	ET_ACTORSPAWN,
	ET_ACTOR,
	ET_ITEM,
	ET_BREAKABLE,
	ET_TRIGGER,
	ET_TRIGGER_HURT,
	ET_TRIGGER_TOUCH,
	ET_TRIGGER_RESCUE,
	ET_TRIGGER_NEXTMAP,
	ET_DOOR,
	ET_DOOR_SLIDING,
	ET_ROTATING,
	ET_ACTOR2x2SPAWN,
	ET_ACTOR2x2,
	ET_CIVILIANTARGET,
	ET_MISSION,
	ET_ACTORHIDDEN,
	ET_PARTICLE,
	ET_SOUND,
	ET_SOLID,
	ET_MESSAGE,
	ET_SMOKE,
	ET_FIRE,
	ET_SMOKESTUN,
	ET_CAMERA,

	ET_MAX,

	ENTITY_TYPE_ENSURE_32BIT = 0x7FFFFFFF
} entity_type_t;

typedef enum {
	CAMERA_MOBILE,
	CAMERA_STATIONARY,

	CAMERA_MAX,

	CAMERA_ENSURE_32BIT = 0x7FFFFFFF
} camera_type_t;

#define DOOR_ROTATION_ANGLE 90

typedef enum {
	PA_NULL,
	PA_TURN,
	PA_MOVE,
	PA_STATE,
	PA_SHOOT,
	PA_USE,
	PA_INVMOVE,
	PA_REACT_SELECT,
	PA_RESERVE_STATE,

	PA_NUM_EVENTS
} player_action_t;

extern const char* pa_format[PA_NUM_EVENTS];

/** @brief Available shoot types - also see the @c ST_ constants */
typedef int32_t shoot_types_t;
/**
 * @brief The right hand should be used for shooting
 * @note shoot_types_t value
 */
#define ST_RIGHT 0
/**
 * @brief The right hand reaction fire should be used for shooting
 * @note shoot_types_t value
 */
#define ST_RIGHT_REACTION 1
/**
 * @brief The left hand should be used for shooting
 * @note shoot_types_t value
 */
#define ST_LEFT 2
/**
 * @brief The left hand reaction fire should be used for shooting
 * @note shoot_types_t value
 */
#define ST_LEFT_REACTION 3
/**
 * @brief The headgear slot item should be used when shooting/using the item in the slot
 * @note shoot_types_t value
 */
#define ST_HEADGEAR 4
/**
 * @brief Amount of shoottypes available
 * @note shoot_types_t value
 */
#define ST_NUM_SHOOT_TYPES 5

/** @brief Determine whether the selected shoot type is for reaction fire */
#define IS_SHOT_REACTION(x) ((x) == ST_RIGHT_REACTION || (x) == ST_LEFT_REACTION)
/** @brief Determine whether the selected shoot type is for the item in the left hand, either shooting or reaction fire */
#define IS_SHOT_LEFT(x)     ((x) == ST_LEFT || (x) == ST_LEFT_REACTION)
/** @brief Determine whether the selected shoot type is for the item in the right hand, either shooting or reaction fire */
#define IS_SHOT_RIGHT(x)    ((x) == ST_RIGHT || (x) == ST_RIGHT_REACTION)
/** @brief Determine whether the selected shoot type is for the item in the headgear slot */
#define IS_SHOT_HEADGEAR(x)    ((x) == ST_HEADGEAR)

/* shoot flags */
#define SF_IMPACT	1	/**< impact on none actor objects */
#define SF_BODY		2	/**< impact on actors */
#define SF_BOUNCING	4	/**< item should bounce around (like grenades) */
#define SF_BOUNCED	8	/**< at least hit the ground once and can now start to bounce */

/* is used in events where two edicts can be send, one actor and one receiver - but one of them can
 * be nullptr, e.g. in a case where the actor is a trigger */
#define SKIP_LOCAL_ENTITY (-1)

#define MAX_DEATH	3	/**< @sa STATE_DEAD */

/* State flags - transferred as short (so max 16 bits please) */
/* public states */
#define STATE_PUBLIC		0x00FF	/**< mask to separate private flags from events */
#define STATE_DEAD			0x0003	/**< 0 alive, 1-3 different deaths @sa MAX_DEATH*/
#define STATE_CROUCHED		0x0004
#define STATE_PANIC			0x0008

#define STATE_RAGE			0x0010	/**< pretty self-explaining */
#define STATE_INSANE		0x0020
#define STATE_STUN			0x0043	/**< stunned - includes death */
#define STATE_DAZED			0x0080	/**< dazed and unable to move */

/* private states */
#define STATE_REACTION		0x0300	/**< reaction fire */
#define STATE_SHAKEN		0x0400	/**< forced reaction fire */
#define STATE_XVI			0x0800	/**< controlled by the other team */

#define GRAVITY				500.0

#define MAX_SKILL	100
#define MAX_MAXHP		255
#define MIN_TU				39
#define WEIGHT_LIGHT	0.2f
#define WEIGHT_HEAVY	0.5f
#define WEIGHT_NORMAL_PENALTY	0.3f
#define WEIGHT_HEAVY_PENALTY	0.6f
#define WEIGHT_FACTOR	1000.0f

#define GET_ENCUMBRANCE_PENALTY(weight, max)	(1.0f - ((weight) > (max) * WEIGHT_FACTOR * WEIGHT_HEAVY ? WEIGHT_HEAVY_PENALTY : (weight) > (max) * WEIGHT_FACTOR * WEIGHT_LIGHT ? WEIGHT_NORMAL_PENALTY : 0.0f))
/** @todo Skill-influence needs some balancing. */
#define GET_ACC( ab, sk )   ((1 - (((float)(ab) - 10) / (MAX_SKILL / 2) + ((float)(sk) - 10) / (MAX_SKILL / 2)) / 2))
#define GET_MORALE( ab )        (std::min((100 + (ab) * 150/MAX_SKILL), 255))
#define GET_TU( ab, md )		(MIN_TU * (md) + (ab) * 20 / MAX_SKILL)

#define DOOR_OPEN_REVERSE 4
#define GET_SLIDING_DOOR_SHIFT_VECTOR(dir, speed, vecout) \
	do { const bool reverse = (dir) & DOOR_OPEN_REVERSE; VectorClear(vecout); vecout[dir & 3] = reverse ? -speed : speed; } while (0);

/* For a RMA with 50 tiles we need about 1k. So let's take 2k */
#define	MAX_TILESTRINGS			8
#define	MAX_CLIENTS				256		/* absolute limit */
#define	MAX_GENERAL				(MAX_CLIENTS*2) /* general config strings */
#define	MAX_ENTITYSTRINGS		2

/**
 * config strings are a general means of communication from
 * the server to all connected clients.
 * Each config string can usually be at most MAX_TOKEN_CHARS characters,
 * but some of them (eg. CS_TILES) can span a multiple of that.
 */
#define CS_NAME					0
#define CS_MAPTITLE				1		/**< display map title string - translated client side */
#define CS_MAXCLIENTS			2
#define CS_MAPCHECKSUM			3		/**< for catching cheater maps */
#define CS_MAXSOLDIERSPERTEAM	4		/**< max soldiers per team */
#define CS_MAXSOLDIERSPERPLAYER	5		/**< max soldiers per player when in teamplay mode */
#define CS_ENABLEMORALE			6		/**< enable the morale states in multiplayer */
#define CS_MAXTEAMS				7		/**< how many multiplayer teams for this map */
#define CS_PLAYERCOUNT			8		/**< amount of already connected players */
#define CS_VERSION				9		/**< what is the servers version */
#define CS_UFOCHECKSUM			10		/**< checksum of ufo files */
#define CS_OBJECTAMOUNT			11		/**< amount of defined objects in the script files */
#define CS_LIGHTMAP				12		/**< which lightmap to use */
#define CS_MAPZONE				13		/**< for terrain texture replacement */
#define CS_VICTORY_CONDITIONS	14		/**< a list of msgids seperated by , */
#define CS_ENTITYSTRING			15		/**< additional worldspawn settings */
#define CS_TILES				(CS_ENTITYSTRING+MAX_ENTITYSTRINGS)
#define CS_POSITIONS			(CS_TILES+MAX_TILESTRINGS)
#define CS_MODELS				(CS_POSITIONS+MAX_TILESTRINGS)
#define CS_PLAYERNAMES			(CS_MODELS+MAX_MODELS)
#define CS_GENERAL				(CS_PLAYERNAMES+MAX_CLIENTS)
#define MAX_CONFIGSTRINGS		(CS_GENERAL+MAX_GENERAL)

/** game types */
#define MAX_GAMETYPES 16

#define MAX_CVARLISTINGAMETYPE 16
typedef struct cvarlist_s {
	char name[MAX_VAR];
	char value[MAX_VAR];
} cvarlist_t;

typedef struct gametype_s {
	char id[MAX_VAR];	/**< script id */
	char name[MAX_VAR];	/**< translated menu name */
	cvarlist_t cvars[MAX_CVARLISTINGAMETYPE];
	int num_cvars;
} gametype_t;

/**
 * @brief Terrain property table entry
 * Terrain is defined by the file map_earth_terrain.png in pics/geoscape.
 * It is a map of the world where the different terrainTypes are drawn in different colors.
 * The colors used in that map must have the exact RGB values as in the table, or they won't be recognized.
 * @sa GEO_GetColor
 */
class TerrainDef		/* a single entry in the table */
{
public:
	byte rgbRed;
	byte rgbGreen;
	byte rgbBlue;
	char terrainName[20];
	float survivalChance;
	float rainChance;
	float snowChance;

	TerrainDef() : rgbRed(128), rgbGreen(255), rgbBlue(0), survivalChance(2.5f), rainChance(0.2f), snowChance(0.1f) {
		strcpy(terrainName, "grass");
	}
	TerrainDef(byte _rgbRed, byte _rgbGreen, byte _rgbBlue, const char* _terrainName, float _survivalChance, float _rainChance, float _snowChance) {
		rgbRed = _rgbRed;
		rgbGreen = _rgbGreen;
		rgbBlue = _rgbBlue;
		strcpy(terrainName, _terrainName);
		survivalChance = _survivalChance;
		rainChance = _rainChance;
		snowChance = _snowChance;
	}
};

/**
 * @brief Terrain property table
 * Terrain is defined by the file map_earth_terrain.png in pics/geoscape.
 * It is a map of the world where the different terrainTypes are drawn in different colors.
 * The colors used in that map must have the exact RGB values as in the table, or they won't be recognized.
 * @sa GEO_GetColor
 */

#define MAX_TERRAINDEFS 16
class TerrainDefs
{
	const TerrainDef* terrainDefTable[MAX_TERRAINDEFS];

	inline const TerrainDef* findByColor(const byte* const color) const {
		return findByColor(color[0], color[1], color[2]);
	}
	inline const TerrainDef* findByColor(byte red, byte green, byte blue) const {
		for (int i = 0; i < MAX_TERRAINDEFS; i++) {
			const TerrainDef* p = terrainDefTable[i];
			if (!p)
				break;
			if (p->rgbRed == red && p->rgbGreen == green && p->rgbBlue == blue)
				return p;
		}
		return nullptr;
	}
	inline const TerrainDef* findByName(const char* tname) const {
		for (int i = 0; i < MAX_TERRAINDEFS; i++) {
			const TerrainDef* p = terrainDefTable[i];
			if (!p)
				break;
			if (Q_streq(p->terrainName, tname))
				return p;
		}
		return nullptr;
	}
public:
	TerrainDefs() {
		terrainDefTable[0] = nullptr;
	}
	~TerrainDefs() {
		for (int i = 0; i < MAX_TERRAINDEFS; i++) {
			if (terrainDefTable[i])
				delete terrainDefTable[i];
		}
	}

	bool add(const TerrainDef* tdef);
	const char* getWeather (const byte* const color);

/**
 * @brief Translate color value to terrain type and then to survival probability
 * @param[in] color the color value from the terrain mask
 * @return the relative probability for survival (of pilots) in such a terrain
 */
	inline float getSurvivalChance(const byte* const color) const {
		const TerrainDef* p = findByColor(color);
		return p ? p->survivalChance : 0.0;
	}
	inline float getRainChance(const byte* const color) const {
		const TerrainDef* p = findByColor(color);
		return p ? p->rainChance : 0.0;
	}
	inline float getSnowChance(const byte* const color) const {
		const TerrainDef* p = findByColor(color);
		return p ? p->snowChance : 0.0;
	}

/**
 * @brief Translate color value to terrain type
 * @param[in] color the color value from the terrain mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 * @note Make sure that there are textures with the same name in base/textures/tex_terrain
 */
	inline const char* getTerrainName(const byte* const color) const {
		const TerrainDef* p = findByColor(color);
		return p ? p->terrainName : "grass";
	}
};

typedef struct mapDef_s {
	/* general */
	char* id;				/**< script file id */
	char* mapTheme;			/**< bsp or ump base filename (without extension and day or night char) */
	linkedList_t* params;	/**< in case of ump file, the assembly to use */
	char* description;		/**< the description to show in the menus */
	char* victoryCondition;	/**< the mission briefing victory condition */
	char* missionBriefing;	/**< the mission briefing description */
	char* size;				/**< small, medium, big */
	char* civTeam;			/**< the civilian team to use for this map - this can also be nullptr */

	/* multiplayer */
	bool multiplayer;		/**< is this map multiplayer ready at all */
	int teams;				/**< multiplayer teams */
	linkedList_t* gameTypes;/**< gametype strings this map is useable for */


	/* singleplayer */
	bool campaign;			/**< available in campaign mode? */
	bool singleplayer;		/**< is this map available in singleplayer games? */
	int hwclass;			/**< skip this map everywhere if hwclass is higher than that of the pc */
	int maxAliens;			/**< Number of spawning points on the map */
	bool hurtAliens;		/**< hurt the aliens on spawning them - e.g. for ufocrash missions */

	linkedList_t* terrains;		/**< terrain strings this map is useable for */
	linkedList_t* populations;	/**< population strings this map is useable for */
	linkedList_t* cultures;		/**< culture strings this map is useable for */
	bool storyRelated;			/**< Is this a mission story related? */
	int timesAlreadyUsed;		/**< Number of times the map has already been used */
	linkedList_t* ufos;			/**< Type of allowed UFOs on the map */
	linkedList_t* aircraft;		/**< Type of allowed aircraft on the map */

	/** @note Don't end with ; - the trigger commands get the base index as
	 * parameter - see CP_ExecuteMissionTrigger - If you don't need the base index
	 * in your trigger command, you can seperate more commands with ; of course */
	char* onwin;		/**< trigger command after you've won a battle */
	char* onlose;		/**< trigger command after you've lost a battle */
} mapDef_t;

#define MapDef_ForeachCondition(var, condition) \
	for (int var##__loopvar = 0; (var) = nullptr, var##__loopvar < Com_GetMapDefNumber(); var##__loopvar++) \
		if ((var) = Com_GetMapDefByIDX(var##__loopvar), !(condition)) {} else

#define MapDef_Foreach(var) MapDef_ForeachCondition(var, 1)
int Com_GetMapDefNumber(void);
mapDef_t* Com_GetMapDefByIDX(int index);
mapDef_t* Com_GetMapDefinitionByID(const char* mapDefID);

/**
 * @brief The csi structure is the client-server-information structure
 * which contains all the static data needed by the server and the client.
 * @note Most of this comes from the script files
 */
typedef struct csi_s {
	/** Object definitions */
	objDef_t ods[MAX_OBJDEFS];
	int numODs;

	implantDef_t implants[MAX_IMPLANTS];
	int numImplants;

	/** Inventory definitions */
	invDef_t ids[MAX_INVDEFS];
	int numIDs;

	/** Damage type ids */
	int damNormal, damBlast, damFire;
	int damShock;	/**< Flashbang-type 'damage' (i.e. Blinding). */

	/** Damage type ids */
	int damLaser, damPlasma, damParticle;
	int damStunGas;		/**< Stun gas attack (only effective against organic targets).
						 * @todo Maybe even make a differentiation between aliens/humans here? */
	int damStunElectro;	/**< Electro-Shock attack (effective against organic and robotic targets). */
	int damSmoke;		/**< smoke damage type that spawn smoke fields that blocks the visibility */
	int damIncendiary;	/**< incendiary damage type that spawns fire fields that will hurt actors */

	/** Equipment definitions */
	equipDef_t eds[MAX_EQUIPDEFS];
	int numEDs;

	/** Damage types */
	damageType_t dts[MAX_DAMAGETYPES];
	int numDTs;

	/** team definitions */
	teamDef_t teamDef[MAX_TEAMDEFS];
	int numTeamDefs;

	/** the current assigned teams for this mission
	 * @todo this does not belong here - this is only static data that is shared */
	const teamDef_t* alienTeams[MAX_TEAMS_PER_MISSION];
	const chrTemplate_t* alienChrTemplates[MAX_TEAMS_PER_MISSION];
	int numAlienTeams;

	/** character templates */
	chrTemplate_t chrTemplates[MAX_CHARACTER_TEMPLATES];
	int numChrTemplates;
	linkedList_t* bodyTemplates;

	linkedList_t* actorNames;

	ugv_t ugvs[MAX_UGV];
	int numUGV;

	gametype_t gts[MAX_GAMETYPES];
	int numGTs;

	/** Map definitions */
	mapDef_t mds[MAX_MAPDEFS];
	int numMDs;

	TerrainDefs terrainDefs;
} csi_t;

extern csi_t csi;

/** @brief Reject messages that are send to the client from the game module */
#define REJ_PASSWORD_REQUIRED_OR_INCORRECT "Password required or incorrect."
#define BAD_RCON_PASSWORD "Bad rcon_password."
#define REJ_BANNED "Banned."
#define REJ_SERVER_FULL "Server is full."
#define REJ_SERVER_VERSION_MISMATCH "The server is running a different version of the game."
#define REJ_GAME_ALREADY_STARTED "Game has started already."
#define REJ_CONNECTION_REFUSED "Connection refused."

#define SV_CMD_CONNECT "connect"
#define SV_CMD_RCON "rcon"
#define SV_CMD_STATUS "status"
#define SV_CMD_TEAMINFO "teaminfo"
#define SV_CMD_INFO "info"
#define SV_CMD_PRINT "print"

/* unused */
#define SV_CMD_ACK "ack"
#define CL_CMD_PING "ping"
#define CL_CMD_ECHO "echo"
/* unused end */

#define CL_PRECACHE "precache"
#define CL_SPAWNSOLDIERS "spawnsoldiers"
#define CL_STARTMATCH "startmatch"

#define CL_CMD_COMMAND "cmd"
#define CL_CMD_CLIENT_CONNECT "client_connect"

#define NET_STATE_NEW "new"
#define NET_STATE_BEGIN "begin"
#define NET_STATE_STARTMATCH "startmatch"
#define NET_STATE_DISCONNECT "disconnect"
#define NET_STATE_INFO "info"
