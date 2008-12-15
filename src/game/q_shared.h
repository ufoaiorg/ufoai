/**
 * @file q_shared.h
 * @brief Common header file.
 *
 * Apparently included by every file - unnecessary?
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef GAME_Q_SHARED_H
#define GAME_Q_SHARED_H

#ifdef DEBUG
#define Com_SetValue(base, set, type, ofs, size) Com_SetValueDebug(base, set, type, ofs, size, __FILE__, __LINE__)
#define Com_EParseValue(base, token, type, ofs, size) Com_EParseValueDebug(base, token, type, ofs, size, __FILE__, __LINE__)
#endif

#include "../shared/ufotypes.h"
#include "../shared/byte.h"
#include "../shared/shared.h"
#include "../shared/mathlib.h"

/* filesystem stuff */
#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <dirent.h>
#endif

#ifdef DEDICATED_ONLY
/* no gettext support for dedicated servers */
# define _(String) String
# define ngettext(x, y, cnt) x
#endif

#ifndef logf
#define logf(x) ((float)log((double)(x)))
#endif

#include "../shared/defines.h"

/* LINKED LIST STUFF */

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data */
} linkedList_t;

void LIST_AddString(linkedList_t** list, const char* data);
void LIST_AddPointer(linkedList_t** listDest, void* data);
linkedList_t* LIST_Add(linkedList_t** list, const byte* data, size_t length);
const linkedList_t* LIST_ContainsString(const linkedList_t* list, const char* string);
void LIST_Delete(linkedList_t **list);
void LIST_Remove(linkedList_t **list, linkedList_t *entry);
int LIST_Count(const linkedList_t *list);
void *LIST_GetByIdx(linkedList_t *list, int index);

/*============================================= */

struct cBspPlane_s;

/*
==========================================================
CVARS (console variables)
==========================================================
*/

#ifndef CVAR
#define CVAR

#define CVAR_ARCHIVE    1       /**< set to cause it to be saved to vars.rc */
#define CVAR_USERINFO   2       /**< added to userinfo  when changed */
#define CVAR_SERVERINFO 4       /**< added to serverinfo when changed */
#define CVAR_NOSET      8       /**< don't allow change from console at all, but can be set from the command line */
#define CVAR_LATCH      16      /**< save changes until server restart */
#define CVAR_DEVELOPER  32      /**< set from commandline (not from within the game) and hide from console */
#define CVAR_CHEAT      64      /**< clamp to the default value when cheats are off */
#define CVAR_IMAGES     128     /**< effects image filtering */
#define CVAR_CONTEXT    256     /**< vid shutdown if such a cvar was modified */

/**
 * @brief This is a cvar defintion. Cvars can be user modified and used in our menus e.g.
 * @note nothing outside the Cvar_*() functions should modify these fields!
 */
typedef struct cvar_s {
	char *name;				/**< cvar name */
	char *string;			/**< value as string */
	char *latched_string;	/**< for CVAR_LATCH vars */
	char *default_string;	/**< default string set on first init - only set for CVAR_CHEAT */
	char *old_string;		/**< value of the cvar before we changed it */
	const char *description;		/**< cvar description */
	int flags;				/**< cvar flags CVAR_ARCHIVE|CVAR_NOSET.... */
	qboolean modified;		/**< set each time the cvar is changed */
	float value;			/**< value as float */
	int integer;			/**< value as integer */
	qboolean (*check) (struct cvar_s* cvar);	/**< cvar check function */
	struct cvar_s *next;
	struct cvar_s *prev;
	struct cvar_s *hash_next;
} cvar_t;

typedef struct cvarList_s {
	const char *name;
	const char *value;
	cvar_t *var;
} cvarList_t;

cvar_t *Cvar_Get(const char *var_name, const char *var_value, int flags, const char* desc);

#endif                          /* CVAR */

/*
==============================================================
SYSTEM SPECIFIC
==============================================================
*/

int Sys_Milliseconds(void);
void Sys_Mkdir(const char *path);

/* directory searching */
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

/* pass in an attribute mask of things you wish to REJECT */
char *Sys_FindFirst(const char *path, unsigned musthave, unsigned canthave);
char *Sys_FindNext(unsigned musthave, unsigned canthave);
void Sys_FindClose(void);
char *Sys_Cwd(void);
void Sys_SetAffinityAndPriority(void);

/* this is only here so the functions in q_shared.c and q_shwin.c can link */
void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Com_Printf(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void Com_DPrintf(int level, const char *msg, ...) __attribute__((format(printf, 2, 3)));

extern cvar_t* sys_priority;
extern cvar_t* sys_affinity;
extern cvar_t* sys_os;

#define AREA_SOLID			1
#define AREA_TRIGGERS		2


/** entity->flags (render flags) */
#define RF_TRANSLUCENT      0x00000001
#define RF_BOX              0x00000002	/**< actor selection box */
#define RF_PATH             0x01000000	/**< pathing marker, debugging only */
#define RF_ARROW            0x02000000	/**< arrow, debugging only */

/** the following ent flags also draw entity effects */
#define RF_SHADOW           0x00000004	/**< shadow (when living) for this entity */
#define RF_BLOOD            0x00000008	/**< blood (when dead) for this entity */
#define RF_SELECTED         0x00000010	/**< selected actor */
#define RF_MEMBER           0x00000020	/**< actor in the same team */
#define RF_ALLIED           0x00000040	/**< actor in an allied team (controlled by another player) */
#define RF_ACTOR            0x00000080	/**< this is an actor */
#define RF_PULSE            0x00000100	/**< glowing entity */
#define RF_HIGHLIGHT        0x00000200  /**< hightlight the actor with a marker */

/* player_state_t->refdef bit flags */
#define RDF_NOWORLDMODEL    1	/* e.g. used for sequences and particle editor */
#define RDF_IRGOGGLES       2

/* sound channels */
/* channel 0 never willingly overrides */
/* other channels (1-7) allways override a playing sound on that channel */
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4

/* Time Constants */
#define DAYS_PER_YEAR 365
#define DAYS_PER_YEAR_AVG 365.25
/** DAYS_PER_MONTH -> @sa monthLength[] array in campaign.c */
#define MONTHS_PER_YEAR 12
#define SECONDS_PER_DAY	86400	/**< (24 * 60 * 60) */
#define SECONDS_PER_HOUR	3600	/**< (60 * 60) */

/* Time information. */
/**
 * @brief Engine-side time information in the game.
 * @note Use this in your custom structs that need to get saved or sent over the network.
 * @sa dateLong_t	For runtime use (human readable).
 * @sa CL_DateConvertLong
 */
typedef struct date_s {
	int day;	/**< Number of ellapsed days since 1st january of year 0 */
	int sec;	/**< Number of ellapsed seconds since the begining of current day */
} date_t;

/**
 * @brief Human readable time information in the game.
 * @note Use this on runtime - please avoid for structs that get saved.
 * @sa date_t	For storage & network transmitting (engine only).
 * @sa CL_DateConvertLong
 */
typedef struct dateLong_s {
	short year;	/**< Year in yyyy notation. */
	byte month;	/**< Number of month (starting with 1). */
	byte day;	/**< Number of day (starting with 1). */
	byte hour;	/**< Hour of the day. @todo check what number-range this gives) */
	byte min;	/**< Minute of the hour. */
	byte sec;	/**< Second of the minute. */
} dateLong_t;

/*
==========================================================
ELEMENTS COMMUNICATED ACROSS THE NET
==========================================================
*/

#define TEAM_NONE      -1
#define TEAM_CIVILIAN   0
#define TEAM_PHALANX    1
#define TEAM_ALIEN      7

/** add this flag for instant event execution */
#define EVENT_INSTANTLY   0x80

/**
 * @brief Possible event values
 * @sa cl_parse.c for event bindings
 * @sa ev_func
 * @sa ev_names
 * @sa ev_format
 */
typedef enum {
	EV_NULL = 0,
	EV_RESET,
	EV_START,
	EV_START_DONE,  /**< Signals that all information was sent fromt he server. */
	EV_ENDROUND,	/**< ends the current team's round CL_DoEndRound */
	EV_ENDROUNDANNOUNCE,

	EV_RESULTS,
	EV_CENTERVIEW,

	EV_ENT_APPEAR,
	EV_ENT_PERISH,	/**< empty container or destroy inventory - set le inuse to qfalse
		* see CL_EntPerish */
	EV_ADD_BRUSH_MODEL,
	EV_ADD_EDICT,

	EV_ACTOR_APPEAR,
	EV_ACTOR_ADD,
	EV_ACTOR_START_MOVE,
	EV_ACTOR_TURN,			/**< turn an actor around */
	EV_ACTOR_MOVE,
	EV_ACTOR_START_SHOOT,
	EV_ACTOR_SHOOT,
	EV_ACTOR_SHOOT_HIDDEN,
	EV_ACTOR_THROW,
	EV_ACTOR_DIE,
	EV_ACTOR_STATS,
	EV_ACTOR_STATECHANGE,	/**< set an actor to crounched or reaction fire */

	EV_INV_ADD,
	EV_INV_DEL,
	EV_INV_AMMO,
	EV_INV_RELOAD,
	EV_INV_HANDS_CHANGED,
	EV_INV_TRANSFER,

	/* func_breakables */
	EV_MODEL_EXPLODE,		/**< delay the event exceution by the impact time */
	EV_MODEL_EXPLODE_TRIGGERED,	/**< don't delay the model explode event */

	EV_SPAWN_PARTICLE,

	EV_DOOR_OPEN,
	EV_DOOR_CLOSE,
	EV_DOOR_ACTION,
	EV_RESET_CLIENT_ACTION,

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
	ET_DOOR,
	ET_ROTATING,
	ET_ACTOR2x2SPAWN,
	ET_ACTOR2x2,
	ET_CIVILIANTARGET,
	ET_MISSION,
	ET_ACTORHIDDEN,
	ET_PARTICLE,
	ET_SOUND
} entity_type_t;

#define DOOR_ROTATION_ANGLE 90

typedef enum {
	PA_NULL,
	PA_TURN,
	PA_MOVE,
	PA_STATE,
	PA_SHOOT,
	PA_USE_DOOR,
	PA_INVMOVE,
	PA_REACT_SELECT,
	PA_RESERVE_STATE
} player_action_t;

extern const char *pa_format[128];

/* =========================================================== */

/** @brief Available shoot types */
typedef enum {
	ST_RIGHT,
	ST_RIGHT_REACTION,
	ST_LEFT,
	ST_LEFT_REACTION,
	ST_HEADGEAR,
	ST_NUM_SHOOT_TYPES,

	/* 20060905 LordHavoc: added reload types */
	ST_RIGHT_RELOAD,
	ST_LEFT_RELOAD
} shoot_types_t;

#define IS_SHOT_REACTION(x) ((x) == ST_RIGHT_REACTION || (x) == ST_LEFT_REACTION)
#define IS_SHOT(x)          ((x) == ST_RIGHT || (x) == ST_LEFT)
#define IS_SHOT_LEFT(x)     ((x) == ST_LEFT || (x) == ST_LEFT_REACTION)
#define IS_SHOT_RIGHT(x)    ((x) == ST_RIGHT || (x) == ST_RIGHT_REACTION)
#define IS_SHOT_HEADGEAR(x)    ((x) == ST_HEADGEAR)

/** @brief Actor actions (character modes) */
typedef enum {
	M_MOVE,		/**< We are currently in move-mode (destination selection). */
	M_FIRE_R,	/**< We are currently in fire-mode for the right weapon (target selection). */
	M_FIRE_L,	/**< We are currently in fire-mode for the left weapon (target selection). */
	M_FIRE_HEADGEAR, /**< We'll fire headgear item shortly. */
	M_PEND_MOVE,	/**< A move target has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_R,	/**< A fire target (right weapon) has been selected, we are waiting for player-confirmation. */
	M_PEND_FIRE_L	/**< A fire target (left weapon) has been selected, we are waiting for player-confirmation. */
} cmodes_t;

#define IS_MODE_FIRE_RIGHT(x)	((x) == M_FIRE_R || (x) == M_PEND_FIRE_R)
#define IS_MODE_FIRE_LEFT(x)		((x) == M_FIRE_L || (x) == M_PEND_FIRE_L)
#define IS_MODE_FIRE_HEADGEAR(x)		((x) == M_FIRE_HEADGEAR)

/* shoot flags */
#define SF_IMPACT	1
#define SF_BODY		2
#define SF_BOUNCING	4
#define SF_BOUNCED	8

#define MAX_DEATH	3	/**< @sa STATE_DEAD */

/* State flags - transfered as short (so max 16 bits please) */
/* public states */
#define STATE_PUBLIC		0x00FF	/**< mask to seperate private flags from events */
#define STATE_DEAD			0x0003	/**< 0 alive, 1-3 different deaths @sa MAX_DEATH*/
#define STATE_CROUCHED		0x0004
#define STATE_PANIC			0x0008

#define STATE_RAGE			0x0010	/**< pretty self-explaining */
#define STATE_INSANE		0x0030
#define STATE_STUN			0x0043	/**< stunned - includes death */
#define STATE_DAZED			0x0080	/**< dazed and unable to move */

/* private states */
#define STATE_REACTION_ONCE	0x0100
#define STATE_REACTION_MANY	0x0200
#define STATE_REACTION		0x0300	/**< reaction - once or many */
#define STATE_SHAKEN		0x0400	/**< forced reaction fire */
#define STATE_XVI			0x0800	/**< controlled by the other team */

/* Sizes of the eye-levels and bounding boxes for actors/units. */
#define EYE_STAND			15
#define EYE_CROUCH			3
#define PLAYER_STAND		20
#define PLAYER_CROUCH		5
#define PLAYER_DEAD			-12
#define PLAYER_MIN			-24
#define PLAYER_WIDTH		9

#define PLAYER_STANDING_HEIGHT		(PLAYER_STAND - PLAYER_MIN)
#define PLAYER_CROUCHING_HEIGHT		(PLAYER_CROUCH - PLAYER_MIN)

#if 0
#define EYE2x2_STAND		15
#define EYE2x2_CROUCH		3
#define PLAYER2x2_STAND		20
#define PLAYER2x2_CROUCH	5
#define PLAYER2x2_DEAD		-12		/**< @todo  This may be the only one needed next to PLAYER2x2_WIDTH. */
#define PLAYER2x2_MIN		-24
#endif
#define PLAYER2x2_WIDTH		18	/**< Width of a 2x2 unit. @todo may need some tweaks. */

/* field marker box */
#define BOX_DELTA_WIDTH		11
#define BOX_DELTA_LENGTH	11
#define BOX_DELTA_HEIGHT	27

#define GRAVITY				500.0

/**
 * config strings are a general means of communication from
 * the server to all connected clients.
 * Each config string can be at most MAX_QPATH characters. */
#define CS_NAME				0
#define CS_MAPTITLE			1		/**< display map title string - translated client side */
#define CS_MAXCLIENTS		2
#define CS_MAPCHECKSUM		3		/**< for catching cheater maps */
#define CS_MAXSOLDIERSPERTEAM	4	/**< max soldiers per team */
#define CS_MAXSOLDIERSPERPLAYER	5	/**< max soldiers per player when in teamplay mode */
#define CS_ENABLEMORALE		6		/**< enable the morale states in multiplayer */
#define CS_MAXTEAMS			7		/**< how many multiplayer teams for this map */
#define CS_PLAYERCOUNT		8		/**< amount of already connected players */
#define CS_VERSION			9		/**< what is the servers version */
#define CS_UFOCHECKSUM		10		/**< checksum of ufo files */
#define CS_OBJECTAMOUNT		11		/**< amount of defined objects in the script files */
#define CS_LIGHTMAP			12		/**< which lightmap to use */

#define CS_TILES			16
#define CS_POSITIONS		(CS_TILES+MAX_TILESTRINGS)
#define CS_MODELS			(CS_POSITIONS+MAX_TILESTRINGS)
#define CS_PLAYERNAMES		(CS_MODELS+MAX_MODELS)
#define CS_GENERAL			(CS_PLAYERNAMES+MAX_CLIENTS)
#define MAX_CONFIGSTRINGS	(CS_GENERAL+MAX_GENERAL)

#define MAX_FORBIDDENLIST (MAX_EDICTS * 2)

/* g_spawn.c */

/* NOTE: this only allows quadratic units */
typedef enum {
	ACTOR_SIZE_NORMAL = 1,
	ACTOR_SIZE_2x2 = 2
} actorSizeEnum_t;

/** @brief Types of actor sounds being issued by CL_PlayActorSound(). */
typedef enum {
	SND_DEATH,	/**< Sound being played on actor death. */
	SND_HURT,	/**< Sound being played when an actor is being hit. */

	SND_MAX
} actorSound_t;

/* team definitions */

#define MAX_TEAMDEFS	128

#define LASTNAME	3
typedef enum {
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
} nametypes_t;

typedef struct teamDef_s {
	int idx;			/**< The index in the teamDef array. */
	char id[MAX_VAR];	/**< id from script file. */
	char name[MAX_VAR];	/**< Translateable name. */
	char tech[MAX_VAR];	/**< technology_t id from research.ufo */

	linkedList_t *names[NAME_NUM_TYPES];	/**< Names list per gender. */
	int numNames[NAME_NUM_TYPES];	/**< Amount of names in this list for all different genders. */

	linkedList_t *models[NAME_LAST];	/**< Models list per gender. */
	int numModels[NAME_LAST];	/**< Amount of models in this list for all different genders. */

	linkedList_t *sounds[SND_MAX][NAME_LAST];	/**< Sounds list per gender and per sound type. */
	int numSounds[SND_MAX][NAME_LAST];	/**< Amount of sounds in this list for all different genders and soundtypes. */

	qboolean alien;		/**< Is this an alien teamdesc definition? */
	qboolean civilian;	/**< Is this a civilian teamdesc definition? */
	qboolean robot;		/**< Is this a robotic team? */

	qboolean armour;	/**< Does this team use armour. */
	qboolean weapons;	/**< Does this team use weapons. */
	struct objDef_s *onlyWeapon;	/**< ods[] index - If this team is not able to use 'normal' weapons, we have to assign a weapon to it
	 						 * The default value is NONE for every 'normal' actor - but e.g. bloodspiders only have
							 * the ability to melee attack their victims. They get a weapon assigned with several
							 * bloodspider melee attack firedefitions */

	int size;	/**< What size is this unit on the field (1=1x1 or 2=2x2)? */
	char hitParticle[MAX_VAR]; /**< Particle id of what particle effect should be spawned if a unit of this type is hit.
								* @sa fireDef_t->hitbody - only "hit_particle" is for blood. :)
								* @todo "hitbody" will not spawn blood in the future. */
} teamDef_t;

/** @brief Reject messages that are send to the client from the game module */
#define REJ_PASSWORD_REQUIRED_OR_INCORRECT "Password required or incorrect."
#define REJ_BANNED "Banned."

#endif /* GAME_Q_SHARED_H */
