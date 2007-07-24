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

#include "../qcommon/ufotypes.h"
#include "../qcommon/mem.h"

#ifdef _MSC_VER
/* unknown pragmas are SUPPOSED to be ignored, but.... */
#pragma warning(disable : 4244) /* MIPS */
#pragma warning(disable : 4136) /* X86 */
#pragma warning(disable : 4051) /* ALPHA */

#pragma warning(disable : 4018) /* signed/unsigned mismatch */
#pragma warning(disable : 4305) /* truncation from const double to float */
#pragma warning(disable : 4152) /* nonstandard extension, function/data pointer conversion in expression */
#pragma warning(disable : 4201) /* nonstandard extension used : nameless struct/union */
#pragma warning(disable : 4100) /* unreferenced formal parameter */
#pragma warning(disable : 4127) /* conditional expression is constant */
#if defined _M_AMD64
# pragma warning(disable : 4267) /* conversion from 'size_t' to whatever, possible loss of data */
#endif
#endif /* _MSC_VER */

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

/* filesystem stuff */
#ifdef _WIN32
# include <direct.h>
# include <io.h>
# ifndef snprintf
#  define snprintf _snprintf
# endif
# define EXPORT __cdecl
# define IMPORT __cdecl
#else
# ifndef stricmp
#  define stricmp strcasecmp
# endif
# include <unistd.h>
# include <dirent.h>
# define EXPORT
# define IMPORT
#endif

#if defined __STDC_VERSION__
#  if __STDC_VERSION__ < 199901L
#    if defined __GNUC__
/* if we are using ansi - the compiler doesn't know about inline */
#      define inline __inline__
#    elif defined _MSVC
#      define inline __inline
#    else
#      define inline
#    endif
#  endif
#else
#  define inline
#endif

#ifdef NO_GETTEXT
#undef HAVE_GETTEXT
#endif
/* i18n support via gettext */
/* needs to be activated via -DHAVE_GETTEXT */
#ifdef HAVE_GETTEXT
# if defined(_WIN32)
#  ifdef _MSC_VER
#   ifndef LC_MESSAGES
#    define LC_MESSAGES 3
#   endif /* LC_MESSAGES */
#  endif /* _MSC_VER */
#  include "../ports/win32/libintl.h"
# else
#  include <libintl.h>
# endif

/* the used textdomain for gettext */
# define TEXT_DOMAIN "ufoai"
# include <locale.h>
# define _(String) gettext(String)
# define gettext_noop(String) String
# define N_(String) gettext_noop (String)
#else /* HAVE_GETTEXT */
/* no gettext support */
# define _(String) String
# define ngettext(x, y, cnt) x
#endif /* HAVE_GETTEXT */

/* to support the gnuc __attribute__ command */
#if defined __ICC || !defined __GNUC__
#  define __attribute__(x)  /*NOTHING*/
#endif

#ifndef byte
typedef uint8_t byte;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef logf
#define logf(x) ((float)log((double)(x)))
#endif

#define UFO_EPSILON 0.00001f

#define DEFAULT_TEAMNUM 1

#define LEVEL_LASTVISIBLE 255
#define LEVEL_WEAPONCLIP 256
#define LEVEL_ACTORCLIP 257
#define LEVEL_STEPON 258
#define LEVEL_TRACING 259
#define LEVEL_MAX 260

#define NONE		0xFF
#define NONE_AMMO	0

#define WIDTH       256         /* absolute max */
#define HEIGHT      8           /* 15 max */

#define MAX_ROUTE       31
#define MAX_MOVELENGTH  60

/* angle indexes */
#define PITCH               0   /* up / down */
#define YAW                 1   /* left / right */
#define ROLL                2   /* fall over */

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
#define MAX_SOUNDS          256 /* so they cannot be blindly increased */
#define MAX_IMAGES          256
#define MAX_GENERAL         (MAX_CLIENTS*2) /* general config strings */

/* not really max hp - but an initial value */
#define MAX_HP 100

/* game print flags */
#define PRINT_NONE          -1  /* suppress printing */
#define PRINT_LOW           0   /* pickup messages */
#define PRINT_MEDIUM        1   /* death messages */
#define PRINT_HIGH          2   /* critical messages */
#define PRINT_CHAT          3   /* chat messages */
#define PRINT_HUD           4   /* translated hud strings */

#define QUIET   (qtrue)
#define NOISY   (qfalse)

#define ERR_FATAL           0   /* exit the entire game with a popup window */
#define ERR_DROP            1   /* print to console and disconnect from game */
#define ERR_DISCONNECT      2   /* don't kill server */

#define PRINT_ALL           0
#define PRINT_DEVELOPER     1   /* only print when "developer 1" */
#define PRINT_ALERT         2

/* important units */
#define UNIT_SIZE           32
#define UNIT_HEIGHT         64
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

#define DIRECTIONS 8	/**<
			 * @brief Number of angles from a position (2-dimensional)
			 * @sa dvecs (in q_shared.c) for a description of its use.
			 * @sa AngleToDV.
			 */

extern const int dvecs[DIRECTIONS][2];
extern const float dvecsn[DIRECTIONS][2];
extern const float dangle[DIRECTIONS];

extern const byte dvright[DIRECTIONS];
extern const byte dvleft[DIRECTIONS];

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

#ifndef M_PI
#define M_PI	3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif

#ifndef M_TWOPI
#define M_TWOPI	6.28318530717958647692
#endif

struct cBspPlane_s;

extern vec3_t vec3_origin;
extern vec4_t vec4_origin;

#define nanmask (255<<23)

#define IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

qboolean Q_IsPowerOfTwo(int i);

/* microsoft's fabs seems to be ungodly slow... */
#define Q_ftol(f) (long) (f)

typedef union {
	float f[3];
	int i[3];
} vectorhack_t;

/** @brief Map boundary is +/- 4096 - to get into the positive area we
 * add the possible max negative value and divide by the size of a grid unit field
 */
#define VecToPos(v,p)       (p[0]=(((int)v[0]+4096)/UNIT_SIZE), p[1]=(((int)v[1]+4096)/UNIT_SIZE), p[2]=((int)v[2]/UNIT_HEIGHT))
/** @brief Pos boundary size is +/- 128 - to get into the positive area we add
 * the possible max negative value and multiply with the grid unit size to get
 * back the the vector coordinates - now go into the middle of the grid field
 * by adding the half of the grid unit size to this value */
#define PosToVec(p,v)       (v[0]=((int)p[0]-128)*UNIT_SIZE+UNIT_SIZE/2, v[1]=((int)p[1]-128)*UNIT_SIZE+UNIT_SIZE/2, v[2]=(int)p[2]*UNIT_HEIGHT+UNIT_HEIGHT/2)

#define DotProduct(x,y)         (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)   (c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)        (c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorMul(scalar,b,c)       (c[0]=scalar*b[0],c[1]=scalar*b[1],c[2]=scalar*b[2])
#define Vector2Mul(scalar,b,c)      (c[0]=scalar*b[0],c[1]=scalar*b[1])
#define FastVectorCopy(a,b)     (*(vectorhack_t*)&(b) = *(vectorhack_t*)&(a))
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

/* just in case you do't want to use the macros */
vec_t _DotProduct(const vec3_t v1, const vec3_t v2);
void _VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out);
void _VectorCopy(const vec3_t in, vec3_t out);

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

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cBspPlane_s *plane);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);

float LerpAngle(float a1, float a2, float frac);

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector(vec3_t dst, const vec3_t src);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

float frand(void);              /* 0 to 1 */
float crand(void);              /* -1 to 1 */
void gaussrand(float *gauss1, float *gauss2);   /* -inf to +inf, median 0, stdev 1 */

/*============================================= */

void stradd(char **str, const char *addStr);

const char *COM_SkipPath(char *pathname);
void COM_StripExtension(const char *in, char *out);
void COM_FileBase(const char *in, char *out);
void COM_FilePath(const char *in, char *out);

const char *COM_Parse(const char **data_p);

/* data is an in/out parm, returns a parsed out token */
const char *COM_EParse(const char **text, const char *errhead, const char *errinfo);

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

void Com_PageInMemory(byte * buffer, int size);

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
char *Q_getcwd(char *dest, size_t size) __attribute__((nonnull));

/*============================================= */

short BigShort(short l);
short LittleShort(short l);
int BigLong(int l);
int LittleLong(int l);
float BigFloat(float l);
float LittleFloat(float l);

void Swap_Init(void);
qboolean Q_IsBigEndian(void);

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));

/*============================================= */

/* key / value info strings */
#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

const char *Info_ValueForKey(const char *s, const char *key);
void Info_RemoveKey(char *s, const char *key);
void Info_SetValueForKey(char *s, const char *key, const char *value);

qboolean Info_Validate(const char *s);

/*
==============================================================
SYSTEM SPECIFIC
==============================================================
*/

extern int curtime;             /* time returned by last Sys_Milliseconds */

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
void Sys_DebugBreak(void);
char *Sys_Cwd(void);

/* this is only here so the functions in q_shared.c and q_shwin.c can link */
void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Com_Printf(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void Com_DPrintf(const char *msg, ...) __attribute__((format(printf, 1, 2)));


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
COLLISION DETECTION
==============================================================
*/

/* lower bits are stronger, and will eat weaker brushes completely */
#define CONTENTS_SOLID          1   /**< an eye is never valid in a solid */
#define CONTENTS_WINDOW         2   /**< translucent, but not watery */
#define CONTENTS_WATER          32
#define CONTENTS_MIST           64

/* remaining contents are non-visible, and don't eat brushes */

#define CONTENTS_ACTORCLIP      0x10000
#define CONTENTS_PASSABLE       0x20000
#define CONTENTS_ACTOR          0x40000   /**< should never be on a brush, only in game */
#define CONTENTS_ORIGIN         0x1000000   /**< removed before bsping an entity */
#define CONTENTS_WEAPONCLIP     0x2000000   /**< stop bullets */
#define CONTENTS_DEADACTOR      0x4000000
#define CONTENTS_DETAIL         0x8000000   /**< brushes to be added after vis leafs */
#define CONTENTS_TRANSLUCENT    0x10000000  /**< auto set if any surface has trans */
#define CONTENTS_STEPON         0x40000000  /**< marks areas elevated passable areas */


#define SURF_LIGHT      0x1     /**< value will hold the light strength */
#define SURF_SLICK      0x2     /**< effects game physics */
#define SURF_WARP       0x8     /**< turbulent water warp */
#define SURF_TRANS33	0x10	/* 0.33 alpha blending */
#define SURF_TRANS66	0x20	/* 0.66 alpha blending */
#define SURF_FLOWING    0x40    /**< scroll towards angle */
#define SURF_NODRAW     0x80    /**< don't bother referencing the texture */
#define SURF_ALPHATEST	0x2000000	/**< alpha test for transparent textures */

/* content masks */
#define MASK_ALL                (-1)
#define MASK_SOLID              (CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_SHOT               (CONTENTS_SOLID|CONTENTS_ACTOR|CONTENTS_WEAPONCLIP|CONTENTS_WINDOW|CONTENTS_DEADACTOR)
#define MASK_VISIBILILITY       (CONTENTS_SOLID|CONTENTS_WATER)


/* FIXME: eliminate AREA_ distinction? */
#define AREA_SOLID      1

/**
 * @brief plane_t structure
 * @note !!! if this is changed, it must be changed in asm code too !!!
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
	char name[16];	/**< not used except in loading CMod_LoadSurfaces */
	int flags;	/**< not used except in loading CMod_LoadSurfaces */
	int value;	/**< not used except in loading CMod_LoadSurfaces */
} cBspSurface_t;

typedef struct mapsurface_s {   /* used internally due to name len probs //ZOID */
	cBspSurface_t c;
	char rname[32];
} mapsurface_t;

/** a trace is returned when a box is swept through the world */
typedef struct {
	qboolean allsolid;		/**< if true, plane is not valid */
	qboolean startsolid;	/**< if true, the initial point was in a solid area */
	float fraction;			/**< time completed, 1.0 = didn't hit anything */
	vec3_t endpos;			/**< final position */
	cBspPlane_t plane;			/**< surface normal at impact */
	cBspSurface_t *surface;	/**< surface hit */
	int contents;			/**< contents on other side of surface hit */
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
/* modifier flags */
#define	CHAN_NO_PHS_ADD			8	/* send to all clients, not just ones in PHS (ATTN 0 will also do this) */
#define	CHAN_RELIABLE			16	/* send by reliable message, not datagram */

/* sound attenuation values */
#define ATTN_NONE               0   /* full volume the entire level */
#define ATTN_NORM               1
#define ATTN_IDLE               2
#define ATTN_STATIC             3   /* diminish very rapidly with distance */

/** @brief e.g. used for breakable objects */
typedef enum {
	MAT_GLASS,		/* default */
	MAT_METAL,
	MAT_ELECTRICAL,
	MAT_WOOD,

	MAT_MAX
} edictMaterial_t;

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
#define INSTANTLY   0x80

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
	ET_MISSION
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


typedef enum {
	IA_NONE,
	IA_MOVE,
	IA_RELOAD,
	IA_RELOAD_SWAP,
	IA_NOTIME,
	IA_NORELOAD,
	IA_ARMOR
} inventory_action_t;


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
#define SF_IMPACT           1
#define SF_BODY             2
#define SF_BOUNCING         4
#define SF_BOUNCED          8

#define MAX_DEATH	3	/**< @sa STATE_DEAD */

/* state flags */
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

#define ANGLE2SHORT(x)		((int)((x)*65536/360) & 65535)
#define SHORT2ANGLE(x)		((x)*(360.0/65536))

#define EYE_STAND			15
#define EYE_CROUCH			3
#define PLAYER_STAND		20
#define PLAYER_CROUCH		5
#define PLAYER_DEAD			-12
#define PLAYER_MIN			-24
#define PLAYER_WIDTH		9

#define ROUTING_NOT_REACHABLE 0xFF

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
#define CS_MAXSOLDIERS		4		/**< max soldiers per team */
#define CS_MAXSOLDIERSPERPLAYER	5	/**< max soldiers per player when in teamplay mode */
#define CS_ENABLEMORALE		6		/**< enable the morale states in multiplayer */
#define CS_MAXTEAMS			7		/**< how many multiplayer teams for this map */
#define CS_PLAYERCOUNT		8		/**< amount of already connected players */
#define CS_VERSION			9		/**< what is the servers version */
#define CS_UFOCHECKSUM		10		/**< checksum of ufo files */

#define CS_TILES			16
#define CS_POSITIONS		(CS_TILES+MAX_TILESTRINGS)
#define CS_MODELS			(CS_POSITIONS+MAX_TILESTRINGS)
#define CS_SOUNDS			(CS_MODELS+MAX_MODELS)
#define CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)
#define CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)
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

#endif /* GAME_Q_SHARED_H */
