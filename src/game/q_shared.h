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
#define Com_ParseValue(base, token, type, ofs, size) Com_ParseValueDebug(base, token, type, ofs, size, __FILE__, __LINE__)
#define Q_strncpyz(string1,string2,length) Q_strncpyzDebug( string1, string2, length, __FILE__, __LINE__ )
#endif

#include "../shared/ufotypes.h"
#include "../shared/byte.h"
#include "../shared/shared.h"

#ifdef HAVE_CONFIG_H
# include "../../config.h"
#endif

#include <errno.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#define CURL_STATICLIB
#include <curl/curl.h>

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

#define UFO_EPSILON 0.00001f

#define DEFAULT_TEAMNUM 1

/** @sa Com_DeveloperSet_f */
#define DEBUG_ALL		(1<<0)
#define DEBUG_SHARED	(1<<1)
#define DEBUG_ENGINE	(1<<2)
#define DEBUG_SYSTEM	(1<<3)
#define DEBUG_COMMANDS	(1<<4)
#define DEBUG_CLIENT	(1<<5)
#define DEBUG_SERVER	(1<<6)
#define DEBUG_GAME		(1<<7)
#define DEBUG_RENDERER	(1<<8)
#define DEBUG_SOUND		(1<<9)

#define NONE		0xFF
#define NONE_AMMO	0

/* Battlescape map dimensions (WIDTH*WIDTH*HEIGHT) */
#define WIDTH       256         /* absolute max */
#define HEIGHT      8           /* 15 max */

/* Move/Routing values */
#define MAX_ROUTE       31
#define MAX_MOVELENGTH  60

/* Timeunits for the various actions. */
#define TU_CROUCH	1	/**< Time units for crouching and standing up */
#define TU_TURN		1	/**< Time units for turning (no matter how far). */
#define TU_REACTION_SINGLE	7	/**< Time units used to activate single RF. */
#define TU_REACTION_MULTI	14	/**< Time units used to activate multi-RF. */
#define TU_MOVE_STRAIGHT 2	/**< Time units used to move straight to the next field/square. */
#define TU_MOVE_DIAGONAL 3	/**< Time units used to move to a diagonal field/square. */

#define MAX_STRING_CHARS    1024    /* max length of a string passed to Cmd_TokenizeString */
#define MAX_STRING_TOKENS   80  /* max tokens resulting from Cmd_TokenizeString */
#define MAX_TOKEN_CHARS     256 /* max length of an individual token */

#define MAX_QPATH           64  /* max length of a quake game pathname */
/* windows + linux 256, macosx 32 */
#define MAX_OSPATH          128 /* max length of a filesystem pathname */
#define MAX_FILES           512 /* max files in a directory */

/* per-level limits */
/* 25 - bases are 5*5 - see BASE_SIZE*/
#define MAX_TILESTRINGS     25
#define MAX_TEAMS           8
#define MAX_CLIENTS         256 /* absolute limit */
#define MAX_EDICTS          1024    /* must change protocol to increase more */
#define MAX_LIGHTSTYLES     256
#define MAX_MODELS          256 /* these are sent over the net as bytes */
#define MAX_GENERAL         (MAX_CLIENTS*2) /* general config strings */

/* not really max hp - but an initial value */
#define MAX_HP 100

/* game print flags */
#define PRINT_NONE          -1  /* suppress printing */
#define PRINT_CHAT          0   /* chat messages */
#define PRINT_HUD           1   /* translated hud strings */
#define PRINT_CONSOLE       2   /* critical messages goes to the game console */

#define QUIET   (qtrue)
#define NOISY   (qfalse)

#define ERR_FATAL           0   /* exit the entire game with a popup window */
#define ERR_DROP            1   /* print to console and disconnect from game */
#define ERR_DISCONNECT      2   /* don't kill server */

/* substract this from the ent->pos[z] to get the ground position */
#define GROUND_DELTA        28
/* player height - 12 to be able to walk trough doors
 * UNIT_HEIGHT is the height of one level */
#define PLAYER_HEIGHT		(UNIT_HEIGHT-12)

/* earth map data */
/* values of sinus and cosinus of earth inclinaison (23,5 degrees) for faster day and night calculations */
#define SIN_ALPHA   0.39875
#define COS_ALPHA   0.91706
/*#define HIGH_LAT      +0.953 */
/*#define LOW_LAT       -0.805 */
/*#define CENTER_LAT    (HIGH_LAT+(LOW_LAT)) */
/*#define SIZE_LAT      (HIGH_LAT-(LOW_LAT)) */
#define HIGH_LAT    +1.0
#define LOW_LAT     -1.0
#define CENTER_LAT  0.0
#define SIZE_LAT    2.0

/**
 * @brief Number of angles from a position (2-dimensional)
 * @sa dvecs (in q_shared.c) for a description of its use.
 * @sa AngleToDV.
 * @sa BASE_DIRECTIONS
 */
#define DIRECTIONS 8

/**
 * @brief Number of direct connected fields for a position
 * @sa DIRECTIONS.
 */
#define BASE_DIRECTIONS 4

extern const int dvecs[DIRECTIONS][2];
extern const float dvecsn[DIRECTIONS][2];
extern const float dangle[DIRECTIONS];

extern const byte dvright[DIRECTIONS];
extern const byte dvleft[DIRECTIONS];

/* LINKED LIST STUFF */

typedef struct linkedList_s {
	byte *data;
	struct linkedList_s *next;
	qboolean ptr;	/**< don't call Mem_Free for data */
} linkedList_t;

void LIST_AddString(linkedList_t** list, const char* data);
void LIST_AddPointer(linkedList_t** listDest, void* data);
linkedList_t* LIST_Add(linkedList_t** list, const byte* data, size_t length);
qboolean LIST_ContainsString(linkedList_t* list, const char* string);
void LIST_Delete(linkedList_t *list);

/*
==============================================================
MATHLIB
==============================================================
*/

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef byte pos_t;
typedef pos_t pos3_t[3];

struct cBspPlane_s;

extern const vec3_t vec3_origin;
extern const vec4_t vec4_origin;

qboolean Q_IsPowerOfTwo(int i);

/* microsoft's fabs seems to be ungodly slow... */
#define Q_ftol(f) (long) (f)

/** @brief Returns the distance between two 3-dimensional vectors */
#define DotProduct(x,y)         (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)   (c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)        (c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorMul(scalar,b,c)       (c[0]=scalar*b[0],c[1]=scalar*b[1],c[2]=scalar*b[2])
#define Vector2Mul(scalar,b,c)      (c[0]=scalar*b[0],c[1]=scalar*b[1])
#define VectorCopy(a,b)         (b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define Vector2Copy(a,b)            (b[0]=a[0],b[1]=a[1])
#define Vector4Copy(a,b)        (b[0]=a[0],b[1]=a[1],b[2]=a[2],b[3]=a[3])
#define VectorClear(a)          (a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)       (b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)   (v[0]=(x), v[1]=(y), v[2]=(z))
#define Vector2Set(v, x, y)     ((v)[0]=(x), (v)[1]=(y))
#define Vector4Set(v, r, g, b, a)   (v[0]=(r), v[1]=(g), v[2]=(b), v[3]=(a))
#define VectorCompare(a,b)      (a[0]==b[0]?a[1]==b[1]?a[2]==b[2]?1:0:0:0)
#define Vector2Compare(a,b)     (a[0]==b[0]?a[1]==b[1]?1:0:0)
#define VectorDistSqr(a,b)      ((b[0]-a[0])*(b[0]-a[0])+(b[1]-a[1])*(b[1]-a[1])+(b[2]-a[2])*(b[2]-a[2]))
#define VectorDist(a,b)         (sqrt((b[0]-a[0])*(b[0]-a[0])+(b[1]-a[1])*(b[1]-a[1])+(b[2]-a[2])*(b[2]-a[2])))
#define VectorLengthSqr(a)      (a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
#define VectorNotEmpty(a)           (a[0]||a[1]||a[2])
#define Vector4NotEmpty(a)          (a[0]||a[1]||a[2]||a[3])

#define PosAddDV(p,dv)          (p[0]+=dvecs[dv&(DIRECTIONS-1)][0], p[1]+=dvecs[dv&(DIRECTIONS-1)][1], p[2]=(dv>>3)&(DIRECTIONS-1))
int AngleToDV(int angle);

void VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc);
void VectorClampMA(vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc);

void MatrixMultiply(const vec3_t a[3], const vec3_t b[3], vec3_t c[3]);
void GLMatrixMultiply(const float a[16], const float b[16], float c[16]);
void GLVectorTransform(const float m[16], const vec4_t in, vec4_t out);
void VectorRotate(const vec3_t m[3], const vec3_t va, vec3_t vb);

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);
int VectorCompareEps(const vec3_t v1, const vec3_t v2, float epsilon);
qboolean VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);
vec_t VectorLength(const vec3_t v);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t VectorNormalize(vec3_t v);    /* returns vector length */
vec_t VectorNormalize2(const vec3_t v, vec3_t out);
void VectorInverse(vec3_t v);
void VectorScale(const vec3_t in, const vec_t scale, vec3_t out);
int Q_log2(int val);

void VecToAngles(const vec3_t vec, vec3_t angles);

void Print2Vector(const vec2_t v);
void Print3Vector(const vec3_t v);

void VecToPolar(const vec3_t v, vec2_t a);
void PolarToVec(const vec2_t a, vec3_t v);

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cBspPlane_s *plane);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);

float LerpAngle(float a1, float a2, float frac);

qboolean FrustomVis(vec3_t origin, int dir, vec3_t point);

void PerpendicularVector(vec3_t dst, const vec3_t src);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

float frand(void);              /* 0 to 1 */
float crand(void);              /* -1 to 1 */
void gaussrand(float *gauss1, float *gauss2);   /* -inf to +inf, median 0, stdev 1 */

/*============================================= */

/* data is an in/out parm, returns a parsed out token */
const char *COM_EParse(const char **text, const char *errhead, const char *errinfo);

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/*============================================= */

/* portable case insensitive compare */
int Q_strncmp(const char *s1, const char *s2, size_t n) __attribute__((nonnull));
int Q_strcmp(const char *s1, const char *s2) __attribute__((nonnull));
int Q_stricmp(const char *s1, const char *s2) __attribute__((nonnull));
int Q_strcasecmp(const char *s1, const char *s2) __attribute__((nonnull));
int Q_strncasecmp(const char *s1, const char *s2, size_t n) __attribute__((nonnull));

int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap);

int Q_StringSort(const void *string1, const void *string2) __attribute__((nonnull));

#ifndef DEBUG
void Q_strncpyz(char *dest, const char *src, size_t destsize) __attribute__((nonnull));
#else
void Q_strncpyzDebug(char *dest, const char *src, size_t destsize, const char *file, int line) __attribute__((nonnull));
#endif
void Q_strcat(char *dest, const char *src, size_t size) __attribute__((nonnull));
char *Q_strlwr(char *str) __attribute__((nonnull));
int Q_putenv(const char *var, const char *value);

/*============================================= */

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));


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

/* FIXME: eliminate AREA_ distinction? */
#define AREA_SOLID			1

/**
 * @brief plane_t structure
 */
typedef struct cBspPlane_s {
	vec3_t normal;
	float dist;
	byte type;                  /**< for fast side tests */
	byte signbits;              /**< signx + (signy<<1) + (signz<<1) */
	byte pad[2];
} cBspPlane_t;

typedef struct cBspModel_s {
	vec3_t mins, maxs;
	vec3_t origin;				/**< for sounds or lights */
	int tile;					/**< which tile in assembly */
	int headnode;
} cBspModel_t;

typedef struct cBspSurface_s {
	char name[MAX_QPATH];	/**< not used except in loading CMod_LoadSurfaces */
	int flags;	/**< not used except in loading CMod_LoadSurfaces */
	int value;	/**< not used except in loading CMod_LoadSurfaces */
} cBspSurface_t;

/** a trace is returned when a box is swept through the world */
typedef struct {
	qboolean allsolid;		/**< if true, plane is not valid */
	qboolean startsolid;	/**< if true, the initial point was in a solid area */
	float fraction;			/**< time completed, 1.0 = didn't hit anything, 0.0 Inside of a brush */
	vec3_t endpos;			/**< final position */
	cBspPlane_t plane;			/**< surface normal at impact */
	cBspSurface_t *surface;	/**< surface hit */
	int contentFlags;		/**< contents on other side of surface hit */
	struct le_s *le;		/**< not set by CM_*() functions */
	struct edict_s *ent;	/**< not set by CM_*() functions */
} trace_t;

#define torad (M_PI/180.0f)
#define todeg (180.0f/M_PI)

/* entity->flags (render flags) */
#define RF_TRANSLUCENT      0x00000001
#define RF_BOX              0x00000002	/**< actor selection box */
#define RF_SHADOW           0x00000004	/**< shadow (when living) for this entity */
#define RF_BLOOD            0x00000008	/**< blood (when dead) for this entity */
#define RF_SELECTED         0x00000010	/**< selected actor */
#define RF_MEMBER           0x00000020	/**< actor in the same team */
#define RF_ALLIED           0x00000040	/**< actor in an allied team (controlled by another player) */
#define RF_LIGHTFIXED       0x00000080	/**< @sa LMF_LIGHTFIXED */
#define RF_NOSMOOTH         0x00000100	/**< @sa LMF_NOSMOOTH */

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

/* sound attenuation values */
#define ATTN_NONE               0   /* full volume the entire level */
#define ATTN_NORM               1
#define ATTN_IDLE               2
#define ATTN_STATIC             3   /* diminish very rapidly with distance */

#define MAX_VAR     64

typedef struct date_s {
	int day;	/**< Number of ellapsed days since 1st january of year 0 */
	int sec;	/**< Number of ellapsed seconds since the begining of current day */
} date_t;

/*
==========================================================
ELEMENTS COMMUNICATED ACROSS THE NET
==========================================================
*/

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
	EV_ENDROUND,	/**< ends the current team's round CL_DoEndRound */
	EV_ENDROUNDANNOUNCE,

	EV_RESULTS,
	EV_CENTERVIEW,

	EV_ENT_APPEAR,
	EV_ENT_PERISH,	/**< empty container or destroy inventory - set le inuse to qfalse
		* see CL_EntPerish */
	EV_ENT_EDICT,

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

	EV_MODEL_PERISH,
	EV_MODEL_EXPLODE,

	EV_SPAWN_PARTICLE,

	EV_DOOR_OPEN,
	EV_DOOR_CLOSE,

	EV_NUM_EVENTS
} event_t;


typedef enum {
	ET_NULL,
	ET_ACTORSPAWN,
	ET_ACTOR,
	ET_ITEM,
	ET_BREAKABLE,
	ET_DOOR,
	ET_ACTOR2x2SPAWN,
	ET_ACTOR2x2,
	ET_CIVILIANTARGET,
	ET_MISSION,
	ET_ACTORHIDDEN,
	ET_PARTICLE
} entity_type_t;


typedef enum {
	PA_NULL,
	PA_TURN,
	PA_MOVE,
	PA_STATE,
	PA_SHOOT,
	PA_INVMOVE,
	PA_REACT_SELECT
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

/* shoot flags */
#define SF_IMPACT	1
#define SF_BODY		2
#define SF_BOUNCING	4
#define SF_BOUNCED	8

#define MAX_DEATH	3	/**< @sa STATE_DEAD */

/* state flags - transfered as short (so max 16 bits please) */
/* public */
#define STATE_PUBLIC		0x00FF
#define STATE_DEAD			0x0003	/**< 0 alive, 1-3 different deaths @sa MAX_DEATH*/
#define STATE_CROUCHED		0x0004
#define STATE_PANIC			0x0008

#define STATE_RAGE			0x0010	/**< pretty self-explaining */
#define STATE_INSANE		0x0030
#define STATE_STUN			0x0043	/**< stunned - includes death */
#define STATE_DAZED			0x0080	/**< dazed and unable to move */

/* private */
#define STATE_REACTION_ONCE	0x0100
#define STATE_REACTION_MANY	0x0200
#define STATE_REACTION		0x0300	/**< reaction - once or many */
#define STATE_SHAKEN		0x0400	/**< forced reaction fire */
#define STATE_XVI			0x0800	/**< controlled by the other team */

#define EYE_STAND			15
#define EYE_CROUCH			3
#define PLAYER_STAND		20
#define PLAYER_CROUCH		5
#define PLAYER_DEAD			-12
#define PLAYER_MIN			-24
#define PLAYER_WIDTH		9

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

#define CS_TILES			16
#define CS_POSITIONS		(CS_TILES+MAX_TILESTRINGS)
#define CS_MODELS			(CS_POSITIONS+MAX_TILESTRINGS)
#define CS_LIGHTS			(CS_MODELS+MAX_MODELS)
#define CS_PLAYERNAMES		(CS_LIGHTS+MAX_LIGHTSTYLES)
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
	/** the index in the teamDef array */
	int index;
	/** id from script file */
	char id[MAX_VAR];
	/** translateable name */
	char name[MAX_VAR];
	/** tech id from research.ufo */
	char tech[MAX_VAR];
	/** names list per gender */
	linkedList_t *names[NAME_NUM_TYPES];
	/** amount of names in this list for all different genders */
	int numNames[NAME_NUM_TYPES];
	/** models list per gender */
	linkedList_t *models[NAME_LAST];
	/** amount of models in this list for all different genders */
	int numModels[NAME_LAST];
	/** sounds list per gender and per sound type */
	linkedList_t *sounds[SND_MAX][NAME_LAST];
	/** amount of sounds in this list for all different genders and soundtypes */
	int numSounds[SND_MAX][NAME_LAST];
	/** is this an alien teamdesc definition */
	qboolean alien;
	/** is this a civilian teamdesc definition */
	qboolean civilian;
	/** able to use weapons/armor */
	qboolean armor, weapons;
	/** if this team is not able to use 'normal' weapons, we have to assign a weapon to it
	 * The default value is -1 for every 'normal' actor - but e.g. bloodspiders only have
	 * the ability to melee attack their victims. They get a weapon assigned with several
	 * bloodspider melee attack firedefitions */
	int onlyWeaponIndex;
	/** What size is this unit on the field (1=1x1 or 2=2x2)? */
	int size;
	/** Particle id of what particle effect should be spawned if a unit of this type is hit.
	 * @sa fireDef_t->hitbody - only "hit_particle" is for blood. :)
	 * @todo "hitbody" will not spawn blood in the future. */
	char hitParticle[MAX_VAR];
} teamDef_t;

#endif /* GAME_Q_SHARED_H */
