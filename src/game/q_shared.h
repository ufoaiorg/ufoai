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
#define WriteByte(x) WriteByte( x, __FILE__, __LINE__ )
#define WriteShort(x) WriteShort( x, __FILE__, __LINE__ )
#endif

#include "../qcommon/ufotypes.h"

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

#ifdef LCC_WIN32
# ifndef C_ONLY
#  define C_ONLY
# endif
#endif

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
#else
# ifndef SHARED_EXT
#  define SHARED_EXT "so"
# endif
# ifndef stricmp
#  define stricmp strcasecmp
# endif
# include <unistd.h>
# include <dirent.h>
#endif

#if defined(_WIN32)
# ifndef snprintf
#  define snprintf _snprintf
# endif
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
#ifndef __GNUC__
#  define __attribute__(x)  /*NOTHING*/
#endif

#if (defined _M_IX86 || defined __i386__ || defined(__ia64__)) && !defined C_ONLY && !defined __sun__ && !defined _MSC_VER
#define id386   1
#else
#define id386   0
#endif

#if defined _M_ALPHA && !defined C_ONLY
#define idaxp   1
#else
#define idaxp   0
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

#define DEFAULT_TEAMNUM 1

/* NOTE: place this macro after stddef.h inclusion. */
/*#if !defined offsetof*/
/*#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)*/
/*#endif*/

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

#define DIRECTIONS 8

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

struct cplane_s;

extern vec3_t vec3_origin;
extern vec4_t vec4_origin;

#define nanmask (255<<23)

#define IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

/* microsoft's fabs seems to be ungodly slow... */
/*float Q_fabs (float f); */
/*#define   fabs(f) Q_fabs(f) */
#if defined _M_IX86 && !defined C_ONLY
extern long Q_ftol(float f);
#else
#define Q_ftol( f ) ( long ) (f)
#endif

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

void VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void VectorClampMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

void MatrixMultiply(vec3_t a[3], vec3_t b[3], vec3_t c[3]);
void GLMatrixMultiply(float a[16], float b[16], float c[16]);
void GLVectorTransform(float m[16], vec4_t in, vec4_t out);
void VectorRotate(vec3_t m[3], vec3_t va, vec3_t vb);

/* just in case you do't want to use the macros */
vec_t _DotProduct(vec3_t v1, vec3_t v2);
void _VectorSubtract(vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd(vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy(vec3_t in, vec3_t out);

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(vec3_t v, vec3_t mins, vec3_t maxs);
int VectorCompareEps(vec3_t v1, vec3_t v2);
qboolean VectorNearer(vec3_t v1, vec3_t v2, vec3_t comp);
vec_t VectorLength(vec3_t v);
void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross);
vec_t VectorNormalize(vec3_t v);    /* returns vector length */
vec_t VectorNormalize2(vec3_t v, vec3_t out);
void VectorInverse(vec3_t v);
void VectorScale(vec3_t in, vec_t scale, vec3_t out);
int Q_log2(int val);

void VecToAngles(const vec3_t vec, vec3_t angles);

void Print2Vector(const vec2_t v);
void Print3Vector(const vec3_t v);

void VecToPolar(const vec3_t v, vec2_t a);
void PolarToVec(const vec2_t a, vec3_t v);

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *plane);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);

float LerpAngle(float a1, float a2, float frac);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)  \
    (((p)->type < 3)?                       \
    (                                       \
        ((p)->dist <= (emins)[(p)->type])?  \
            1                               \
        :                                   \
        (                                   \
            ((p)->dist >= (emaxs)[(p)->type])?\
                2                           \
            :                               \
                3                           \
        )                                   \
    )                                       \
    :                                       \
        BoxOnPlaneSide( (emins), (emaxs), (p)))

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
void COM_DefaultExtension(char *path, char *extension);

char *COM_Parse(char **data_p);

/* data is an in/out parm, returns a parsed out token */
char *COM_EParse(char **text, const char *errhead, const char *errinfo);

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
void Q_strncpyzDebug(char *dest, const char *src, size_t destsize, char *file, int line) __attribute__((nonnull));
#endif
void Q_strcat(char *dest, const char *src, size_t size) __attribute__((nonnull));
char *Q_strlwr(char *str) __attribute__((nonnull));
char *Q_strdup(const char *str) __attribute__((nonnull));
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
char *va(char *format, ...) __attribute__((format(printf, 1, 2)));

/*============================================= */

/* key / value info strings */
#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

char *Info_ValueForKey(char *s, const char *key);
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
char *strlwr(char *s);          /* this is non ansi and is defined for some OSs */

/* large block stack allocation routines */
void *Hunk_Begin(int maxsize);
void *Hunk_Alloc(int size, const char *name);
void Hunk_Free(void *buf) __attribute__((nonnull));
int Hunk_End(void);

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

/**
 * @brief This is a cvar defintion. Cvars can be user modified and used in our menus e.g.
 * @note nothing outside the Cvar_*() functions should modify these fields!
 */
typedef struct cvar_s {
	char *name;				/**< cvar name */
	char *string;			/**< value as string */
	char *latched_string;	/**< for CVAR_LATCH vars */
	char *old_string;		/**< value of the cvar before we changed it */
	const char *description;		/**< cvar description */
	int flags;				/**< cvar flags CVAR_ARCHIVE|CVAR_NOSET.... */
	qboolean modified;		/**< set each time the cvar is changed */
	float value;			/**< value as float */
	int integer;			/**< value as integer */
	qboolean (*check) (struct cvar_s* cvar);	/**< cvar check function */
	struct cvar_s *next;
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
#define CONTENTS_AUX            4
#define CONTENTS_SMOKE          8
#define CONTENTS_SLIME          16
#define CONTENTS_WATER          32
#define CONTENTS_MIST           64
#define LAST_VISIBLE_CONTENTS   64

/* remaining contents are non-visible, and don't eat brushes */

#define CONTENTS_AREAPORTAL     0x8000

#define CONTENTS_ACTORCLIP      0x10000
#define CONTENTS_PASSABLE       0x20000

/* currents can be added to any other contents, and may be mixed */
#define CONTENTS_CURRENT_0      0x40000
#define CONTENTS_CURRENT_90     0x80000
#define CONTENTS_CURRENT_180    0x100000
#define CONTENTS_CURRENT_270    0x200000
#define CONTENTS_CURRENT_UP     0x400000
#define CONTENTS_CURRENT_DOWN   0x800000

#define CONTENTS_ORIGIN         0x1000000   /**< removed before bsping an entity */

#define CONTENTS_ACTOR          0x2000000   /**< should never be on a brush, only in game */
#define CONTENTS_DEADACTOR      0x4000000
#define CONTENTS_DETAIL         0x8000000   /**< brushes to be added after vis leafs */
#define CONTENTS_TRANSLUCENT    0x10000000  /**< auto set if any surface has trans */
#define CONTENTS_LADDER         0x20000000
#define CONTENTS_STEPON         0x40000000  /**< marks areas elevated passable areas */


#define SURF_LIGHT      0x1     /**< value will hold the light strength */
#define SURF_SLICK      0x2     /**< effects game physics */
#define SURF_WARP       0x8     /**< turbulent water warp */
#define SURF_TRANS33    0x10
#define SURF_TRANS66    0x20
#define SURF_FLOWING    0x40    /**< scroll towards angle */
#define SURF_NODRAW     0x80    /**< don't bother referencing the texture */

#define SURF_ALPHATEST	0x2000000	/**< alpha test for transparent textures */

/* content masks */
#define MASK_ALL                (-1)
#define MASK_SOLID              (CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_ACTORSOLID         (CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW|CONTENTS_ACTOR)
#define MASK_DEADSOLID          (CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW)
/*#define   MASK_MONSTERSOLID       (CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW|CONTENTS_ACTOR) */
#define MASK_WATER              (CONTENTS_WATER|CONTENTS_SLIME)
#define MASK_OPAQUE             (CONTENTS_SOLID|CONTENTS_SLIME)
#define MASK_SHOT               (CONTENTS_SOLID|CONTENTS_ACTOR|CONTENTS_WINDOW|CONTENTS_DEADACTOR)
#define MASK_VISIBILILITY       (CONTENTS_SOLID|CONTENTS_WATER|CONTENTS_SLIME)


/* FIXME: eliminate AREA_ distinction? */
#define AREA_SOLID      1


/**
 * @brief plane_t structure
 * @note !!! if this is changed, it must be changed in asm code too !!!
 */
typedef struct cplane_s {
	vec3_t normal;
	float dist;
	byte type;                  /**< for fast side tests */
	byte signbits;              /**< signx + (signy<<1) + (signz<<1) */
	byte pad[2];
} cplane_t;

/* structure offset for asm code */
#define CPLANE_NORMAL_X         0
#define CPLANE_NORMAL_Y         4
#define CPLANE_NORMAL_Z         8
#define CPLANE_DIST             12
#define CPLANE_TYPE             16
#define CPLANE_SIGNBITS         17
#define CPLANE_PAD0             18
#define CPLANE_PAD1             19

typedef struct cmodel_s {
	vec3_t mins, maxs;
	vec3_t origin;				/**< for sounds or lights */
	int tile;					/**< which tile in assembly */
	int headnode;
} cmodel_t;

typedef struct csurface_s {
	char name[16];	/**< not used except in loading CMod_LoadSurfaces */
	int flags;	/**< not used except in loading CMod_LoadSurfaces */
	int value;	/**< not used except in loading CMod_LoadSurfaces */
} csurface_t;

typedef struct mapsurface_s {   /* used internally due to name len probs //ZOID */
	csurface_t c;
	char rname[32];
} mapsurface_t;

/** a trace is returned when a box is swept through the world */
typedef struct {
	qboolean allsolid;		/**< if true, plane is not valid */
	qboolean startsolid;	/**< if true, the initial point was in a solid area */
	float fraction;			/**< time completed, 1.0 = didn't hit anything */
	vec3_t endpos;			/**< final position */
	cplane_t plane;			/**< surface normal at impact */
	csurface_t *surface;	/**< surface hit */
	int contents;			/**< contents on other side of surface hit */
	struct le_s *le;		/**< not set by CM_*() functions */
	struct edict_s *ent;	/**< not set by CM_*() functions */
} trace_t;

#define torad (M_PI/180.0f)
#define todeg (180.0f/M_PI)

/* entity->flags (render flags) */
/*unused*/
#define RF_MINLIGHT         0x00000001  /**< allways have some light (viewmodel) */
/*unused*/
#define RF_VIEWERMODEL      0x00000002  /**< don't draw through eyes, only mirrors */
#define RF_WEAPONMODEL      0x00000004  /**< only draw through eyes */
/*unused*/
#define RF_FULLBRIGHT       0x00000008  /**< allways draw full intensity */
#define RF_DEPTHHACK        0x00000010  /**< for view weapon Z crunching */
#define RF_TRANSLUCENT      0x00000020
/*unused*/
#define RF_FRAMELERP        0x00000040
#define RF_BEAM             0x00000080
/*unused*/
#define RF_CUSTOMSKIN       0x00000100  /**< skin is an index in image_precache */
/*unused*/
#define RF_GLOW             0x00000200  /**< pulse lighting for bonus items */
#define RF_SHELL_RED        0x00000400
#define RF_SHELL_GREEN      0x00000800
#define RF_SHELL_BLUE       0x00001000

#define RF_BOX              0x00002000	/**< actor selection box */
#define RF_SHADOW           0x00004000	/**< shadow for this entity */
#define RF_SELECTED         0x00008000	/**< selected actor */
#define RF_MEMBER           0x00010000	/**< actor in the same team */
#define RF_ALLIED           0x00020000	/**< actor in an allied team (controlled by another player) */

#define RF_LIGHTFIXED       0x00040000
#define RF_NOSMOOTH         0x00080000

/* player_state_t->refdef flags */
#define RDF_UNDERWATER      1   /* warp the screen as apropriate */
#define RDF_NOWORLDMODEL    2   /* e.g. used for sequences */

/*ROGUE */
#define RDF_IRGOGGLES       4
#define RDF_UVGOGGLES       8
/*ROGUE */

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

/*
==============================================================
SCRIPT VALUE PARSING
==============================================================
*/

#define MAX_VAR     64

/* this is here to let the parsing function determine the right size */

/** conditions for V_IF */
typedef enum menuIfCondition_s {
	IF_EQ, /**< == */
	IF_LE, /**< <= */
	IF_GE, /**< >= */
	IF_GT, /**< > */
	IF_LT, /**< < */
	IF_NE, /**< != */
	IF_EXISTS, /**< only cvar give - check for existence */

	IF_SIZE
} menuIfCondition_t;

/** @sa menuIfCondition_t */
typedef struct menuDepends_s {
	char var[MAX_VAR];
	char value[MAX_VAR];
	int cond;
} menuDepends_t;

/* end of V_IF */

#ifndef ALIGN_BYTES
#define ALIGN_BYTES 1
#endif
#define ALIGN(size)  ((size) + ((ALIGN_BYTES - ((size) % ALIGN_BYTES)) % ALIGN_BYTES))

#define MEMBER_SIZEOF(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)

/**
 * @brief possible values for parsing functions
 * @sa vt_names
 * @sa vt_sizes
 */
typedef enum {
	V_NULL,
	V_BOOL,
	V_CHAR,
	V_INT,
	V_INT2,
	V_FLOAT = 5,
	V_POS,
	V_VECTOR,
	V_COLOR,
	V_RGBA,
	V_STRING = 10,
	V_TRANSLATION_STRING,	/**< translate via gettext */
	V_TRANSLATION2_STRING,		/**< remove _ but don't translate */
	V_LONGSTRING,
	V_POINTER,
	V_ALIGN = 15,
	V_BLEND,
	V_STYLE,
	V_FADE,
	V_SHAPE_SMALL,
	V_SHAPE_BIG = 20,
	V_DMGTYPE,
	V_DATE,
	V_IF,
	V_RELABS,					/**< relative (e.g. 1.50) and absolute (e.g. +15) values */
	V_CLIENT_HUNK,				/**< only for client side data - not handled in Com_ParseValue */
	V_CLIENT_HUNK_STRING,		/**< same as for V_CLIENT_HUNK */

	V_NUM_TYPES
} valueTypes_t;

#define V_UNTYPED   0x7FFF

extern const char *vt_names[V_NUM_TYPES];

/** possible alien values - see also align_names */
typedef enum {
	ALIGN_UL,
	ALIGN_UC,
	ALIGN_UR,
	ALIGN_CL,
	ALIGN_CC,
	ALIGN_CR,
	ALIGN_LL,
	ALIGN_LC,
	ALIGN_LR,

	ALIGN_LAST
} align_t;

/** possible blend modes - see also blend_names */
typedef enum {
	BLEND_REPLACE,
	BLEND_BLEND,
	BLEND_ADD,
	BLEND_FILTER,
	BLEND_INVFILTER,

	BLEND_LAST
} blend_t;

typedef enum {
	STYLE_FACING,
	STYLE_ROTATED,
	STYLE_BEAM,
	STYLE_LINE,
	STYLE_AXIS,
	STYLE_CIRCLE,

	STYLE_LAST
} style_t;

typedef enum {
	FADE_NONE,
	FADE_IN,
	FADE_OUT,
	FADE_SIN,
	FADE_SAW,
	FADE_BLEND,

	FADE_LAST
} fade_t;

typedef struct date_s {
	int day;	/**< Number of ellapsed days since 1st january of year 0 */
	int sec;	/**< Number of ellapsed seconds since the begining of current day */
} date_t;

extern const char *align_names[ALIGN_LAST];
extern const char *blend_names[BLEND_LAST];
extern const char *style_names[STYLE_LAST];
extern const char *fade_names[FADE_LAST];

/** used e.g. in our parsers */
typedef struct value_s {
	const char *string;
	const int type;
	const size_t ofs;
	const size_t size;
} value_t;

#ifdef DEBUG
int Com_ParseValueDebug(void *base, char *token, valueTypes_t type, int ofs, size_t size, const char* file, int line);
int Com_SetValueDebug(void *base, void *set, valueTypes_t type, int ofs, size_t size, const char* file, int line);
#else
int Com_ParseValue(void *base, char *token, valueTypes_t type, int ofs, size_t size);
int Com_SetValue(void *base, void *set, valueTypes_t type, int ofs, size_t size);
#endif
char *Com_ValueToStr(void *base, valueTypes_t type, int ofs);

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
 */
typedef enum {
	EV_NULL = 0,
	EV_RESET,
	EV_START,
	EV_ENDROUND,	/**< ends the current team's round CL_DoEndRound */
	EV_RESULTS,
	EV_CENTERVIEW = 5,

	EV_ENT_APPEAR,
	EV_ENT_PERISH,	/**< empty container or destroy inventory - set le inuse to qfalse
		* see CL_EntPerish */
	EV_ENT_EDICT,

	EV_ACTOR_APPEAR,
	EV_ACTOR_START_MOVE = 10,
	EV_ACTOR_TURN,			/**< turn an actor around */
	EV_ACTOR_MOVE,
	EV_ACTOR_START_SHOOT,
	EV_ACTOR_SHOOT,
	EV_ACTOR_SHOOT_HIDDEN = 15,
	EV_ACTOR_THROW,
	EV_ACTOR_DIE,
	EV_ACTOR_STATS,
	EV_ACTOR_STATECHANGE,	/**< set an actor to crounched or reaction fire */

	EV_INV_ADD = 20,
	EV_INV_DEL,
	EV_INV_AMMO,
	EV_INV_RELOAD,
	EV_INV_HANDS_CHANGED,

	EV_MODEL_PERISH = 25,
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
	ET_UGVSPAWN,
	ET_UGV,
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


/* this is the absolute max for now */
#define MAX_OBJDEFS     128		/* Remember to adapt the "NONE" define (and similar) if this gets changed. */
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
#define MAX_DAMAGETYPES 32

/* #define GET_FIREDEF(type)   (&csi.ods[type & 0x7F].fd[0][!!(type & 0x80)]) @todo remove me */
/* @todo: might need some changes so the correct weapon (i.e. not 0) is used for the fd */

#define GET_FIREDEFDEBUG(obj_idx,weap_fds_idx,fd_idx) \
	if (obj_idx < 0 || obj_idx >= MAX_OBJDEFS) \
		Sys_Error("GET_FIREDEF: obj_idx out of bounds [%i] (%s: %i)\n", obj_idx, __FILE__, __LINE__); \
	if (weap_fds_idx < 0 || weap_fds_idx >= MAX_WEAPONS_PER_OBJDEF) \
		Sys_Error("GET_FIREDEF: weap_fds_idx out of bounds [%i] (%s: %i)\n", weap_fds_idx, __FILE__, __LINE__); \
	if (fd_idx < 0 || fd_idx >= MAX_FIREDEFS_PER_WEAPON) \
		Sys_Error("GET_FIREDEF: fd_idx out of bounds [%i] (%s: %i)\n", fd_idx, __FILE__, __LINE__);

#define GET_FIREDEF(obj_idx,weap_fds_idx,fd_idx) \
	(&csi.ods[obj_idx & (MAX_OBJDEFS-1)].fd[weap_fds_idx & (MAX_WEAPONS_PER_OBJDEF-1)][fd_idx & (MAX_FIREDEFS_PER_WEAPON-1)])

/** this is a fire definition for our weapons/ammo */
typedef struct fireDef_s {
	char name[MAX_VAR];			/**< script id */
	char projectile[MAX_VAR];	/**< projectile particle */
	char impact[MAX_VAR];		/**< impact particle */
	char hitBody[MAX_VAR];		/**< hit the body particles */
	char fireSound[MAX_VAR];	/**< the sound when a recruits fires */
	char impactSound[MAX_VAR];	/**< the sound that is played on impact */
	char hitBodySound[MAX_VAR];	/**< the sound that is played on hitting a body */
	char bounceSound[MAX_VAR];	/**< bouncing sound */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	int obj_idx;		/**< The weapon/ammo (csi.ods[obj_idx]) this fd is located in.
						 ** So you can get the containing object by acceessing e.g. csi.ods[obj_idx]. */
	int weap_fds_idx;	/**< The index of the "weapon_mod" entry (objDef_t->fd[weap_fds_idx]) this fd is located in.
						 ** Depending on this value you can find out via objDef_t->weap_idx[weap_fds_idx] what weapon this firemode is used for.
						 ** This does _NOT_ equal the index of the weapon object in ods.
						 */
	int fd_idx;		/**< Self link of the fd in the objDef_t->fd[][fd_idx] array. */

	qboolean soundOnce;
	qboolean gravity;			/**< Does gravity has any influence on this item? */
	qboolean launched;
	qboolean rolled;			/**< Can it be rolled - e.g. grenades */
	qboolean reaction;			/**< This firemode can be used/selected for reaction fire.*/
	int throughWall;		/**< allow the shooting through a wall */
	byte dmgtype;
	float speed;
	vec2_t shotOrg;
	vec2_t spread;
	float modif;
	int delay;
	int bounce;				/**< Is this item bouncing? e.g. grenades */
	float bounceFac;
	float crouch;
	float range;			/**< range of the weapon ammunition */
	int shots;
	int ammo;
	float rof;
	int time;
	vec2_t damage, spldmg;
	float splrad;			/**< splash damage radius */
	int weaponSkill;		/**< What weapon skill is needed to fire this weapon. */
	int irgoggles;			/**< Is this an irgoogle? */
} fireDef_t;

/**
 * @brief Defines all attributes of obejcts used in the inventory.
 * @todo Document the various (and mostly not obvious) varables here. The documentation in the .ufo file(s) might be a good starting point.
 * @note See also http://ufoai.ninex.info/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
 */
typedef struct objDef_s {
	/* Common */
	char name[MAX_VAR];	/**< item name/scriptfile id */
	char id[MAX_VAR];	/**< short identifier */
	char model[MAX_VAR];	/**< model name - relative to game dir */
	char image[MAX_VAR];	/**< object image file - relative to game dir */
	char type[MAX_VAR];	/**< melee, rifle, ammo */
	char extends_item[MAX_VAR];
	int shape;			/**< the shape in our inventory */

	byte sx, sy;		/**< Size in x and y direction. */
	float scale;		/**< scale value for images? and models */
	vec3_t center;		/**< origin for models */
	char category;		/**<  The animation index for the character with the weapon.
						 * Don't confuse this with buytype. */
	qboolean weapon;		/**< This item is a weapon or ammo. */
	qboolean holdtwohanded;	/**< The soldier needs both hands to hold this object.
				 		 * Influences model-animations and which hands are blocked int he inventory screen.*/
	qboolean firetwohanded;	/**< The soldier needs both hands to fire this object.
						  * Influences model-animations. */
	qboolean extension;		/**< Boolean: Is an extension. */
	qboolean headgear;		/**< Boolean: Is a headgear. */
	qboolean thrown;		/**< This item is thrown. */
	int price;		/**< the price for this item */
	int size;		/**< Size of an item, used in storage capacities. */
	int buytype;		/**< In which category of the buy menu is this item listed.
						 * see equipment_buytypes_t */

	/* Weapon specific */
	int ammo;			/**< How much can we load into this weapon at once. */
	int reload;			/**< Time units (TUs) for reloading the weapon. */
	qboolean oneshot;			/**< This weapon contains its own ammo (it is loaded in the base).
								 ** Don't confuse "oneshot" with "only one shot is possible",
								 ** the number of the "ammo" value above defines how many shots are possible. */
	qboolean deplete;			/**< Is this weapon useless after all ("oneshot") ammo is used up? If true this item will not be collected on mission-end. (see CL_CollectItems) */
	int useable;			/**< Defines which team can use this item: 0 - human, 1 - alien. */
	int ammo_idx[MAX_AMMOS_PER_OBJDEF];			/**< List of ammo-object indices. The index of the ammo in csi.ods[xxx]. */
	int numAmmos;							/**< Number of ammos this weapon can be used with.
											 ** it's <= MAX_AMMOS_PER_OBJDEF) */

	/* Firemodes (per weapon) */
	char weap_id[MAX_WEAPONS_PER_OBJDEF][MAX_VAR];	/**< List of weapon ids as parsed from the ufo file "weapon_mod <id>" */
	int weap_idx[MAX_WEAPONS_PER_OBJDEF];			/**< List of weapon-object indices. The index of the weapon in csi.ods[xxx].
												 ** You can get the correct index for this array from e.g. fireDef_t.weap_fds_idx. or with INV_FiredefsIDXForWeapon. */
	fireDef_t fd[MAX_WEAPONS_PER_OBJDEF][MAX_FIREDEFS_PER_WEAPON];	/**< List of firemodes per weapon (the ammo can be used in). */
	int numFiredefs[MAX_WEAPONS_PER_OBJDEF];	/**< Number of firemodes per weapon. (How many of the firemodes-per-weapons list is used.)
												 ** Maximum value for fireDef_t.fd_idx and it't <= MAX_FIREDEFS_PER_WEAPON */
	int numWeapons;							/**< Number of weapons this ammo can be used in.
											 ** Maximum value for fireDef_t.weap_fds_idx and it's <= MAX_WEAPONS_PER_OBJDEF) */

	/* Technology link */
	void *tech;		/**< Technology link to item to use this extension for (if this is an extension) */
	/** @todo: Can be used for menu highlighting and in ufopedia */
	void *extension_tech;

	/* Armor specific */
	short protection[MAX_DAMAGETYPES];
	short hardness[MAX_DAMAGETYPES];
} objDef_t;

#define MAX_INVDEFS     16

/** inventory definition for our menus */
typedef struct invDef_s {
	char name[MAX_VAR];	/**< id from script files */
	qboolean single, armor, all, temp, extension, headgear;	/**< type of this container or inventory */
	int shape[16];	/**< the inventory form/shape */
	int in, out;	/**< TU costs */
} invDef_t;

#define MAX_CONTAINERS  MAX_INVDEFS
#define MAX_INVLIST     1024

/** item definition */
typedef struct item_s {
	int a;	/**< number of ammo rounds left - see NONE_AMMO */
	int m;	/**< unique index of ammo type on csi->ods - see NONE */
	int t;	/**< unique index of weapon in csi.ods array - see NONE */
} item_t;

/** container/inventory list (linked list) with items */
typedef struct invList_s {
	item_t item;	/**< which item */
	int x, y;		/**< position of the item */
	struct invList_s *next;		/**< next entry in this list */
} invList_t;

/** inventory defintion with all its containers */
typedef struct inventory_s {
	invList_t *c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char name[MAX_VAR];
	int num[MAX_OBJDEFS];
	byte num_loose[MAX_OBJDEFS];
} equipDef_t;

/**
 * @brief The csi structure is the client-server-information structure
 * which contains all the UFO info needed by the server and the client.
 */
typedef struct csi_s {
	/** Object definitions */
	objDef_t ods[MAX_OBJDEFS];
	int numODs;

	/** Inventory definitions */
	invDef_t ids[MAX_INVDEFS];
	int numIDs;

	/** Special container ids */
	int idRight, idLeft, idExtension;
	int idHeadgear, idBackpack, idBelt, idHolster;
	int idArmor, idFloor, idEquip;

	/** Damage type ids */
	int damNormal, damBlast, damFire, damShock;
	/** Damage type ids */
	int damLaser, damPlasma, damParticle, damStun;

	/** Equipment definitions */
	equipDef_t eds[MAX_EQUIPDEFS];
	int numEDs;

	/** Damage types */
	char dts[MAX_DAMAGETYPES][MAX_VAR];
	int numDTs;
} csi_t;


/** @todo Medals. Still subject to (major) changes. */

#define MAX_SKILL           100

#define GET_HP_HEALING( ab ) (1 + (ab) * 10/MAX_SKILL)
#define GET_HP( ab )        (80 + (ab) * 90/MAX_SKILL)
#define GET_ACC( ab, sk )   ((1 - ((float)(ab)/MAX_SKILL + (float)(sk)/MAX_SKILL) / 2)) /**@todo Skill-influence needs some balancing. */
#define GET_TU( ab )        (27 + (ab) * 20/MAX_SKILL)
#define GET_MORALE( ab )        (100 + (ab) * 150/MAX_SKILL)

typedef enum {
	KILLED_ALIENS,
	KILLED_CIVILIANS,
	KILLED_TEAM,

	KILLED_NUM_TYPES
} killtypes_t;

typedef enum {
	ABILITY_POWER,
	ABILITY_SPEED,
	ABILITY_ACCURACY,
	ABILITY_MIND,

	SKILL_CLOSE,
	SKILL_HEAVY,
	SKILL_ASSAULT,
	SKILL_SNIPER,
	SKILL_EXPLOSIVE,
	SKILL_NUM_TYPES
} abilityskills_t;

#define ABILITY_NUM_TYPES SKILL_CLOSE


#define MAX_UGV         8
typedef struct ugv_s {
	char id[MAX_VAR];
	char weapon[MAX_VAR];
	char armor[MAX_VAR];
	int size;
	int tu;
} ugv_t;

#define MAX_RANKS           32

/** @brief Describes a rank that a recruit can gain */
typedef struct rank_s {
	char *id;		/**< Index in gd.ranks */
	char name[MAX_VAR];	/**< Rank name (Captain, Squad Leader) */
	char shortname[8];		/**< Rank shortname (Cpt, Sqd Ldr) */
	char *image;		/**< Image to show in menu */
	int type;			/**< employeeType_t */
	int mind;			/**< character mind attribute needed */
	int killed_enemies;		/**< needed amount of enemies killed */
	int killed_others;		/**< needed amount of other actors killed */
} rank_t;

extern rank_t ranks[MAX_RANKS]; /**< Global list of all ranks defined in medals.ufo. */
extern int numRanks;            /**< The number of entries in the list above. */

/** @brief Structure of scores being counted after soldier kill or stun. */
typedef struct chrScore_s {
	int alienskilled;	/**< Killed aliens. */
	int aliensstunned;	/**< Stunned aliens. */
	int civilianskilled;	/**< Killed civilians. @todo: use me. */
	int civiliansstunned;	/**< Stunned civilians. @todo: use me. */
	int teamkilled;		/**< Killed teammates. @todo: use me. */
	int teamstunned;	/**< Stunned teammates. @todo: use me. */
	int closekills;		/**< Aliens killed by CLOSE. */
	int heavykills;		/**< Aliens killed by HEAVY. */
	int assaultkills;	/**< Aliens killed by ASSAULT. */
	int sniperkills;	/**< Aliens killed by SNIPER. */
	int explosivekills;	/**< Aliens killed by EXPLOSIVE. */
	int accuracystat;	/**< Aliens kills or stuns counted to ACCURACY. */
	int powerstat;		/**< Aliens kills or stuns counted to POWER. */
	int survivedmissions;	/**< Missions survived. @todo: use me. */
} chrScore_t;

/** @brief Describes a character with all its attributes */
typedef struct character_s {
	int ucn;
	char name[MAX_VAR];
	char path[MAX_VAR];
	char body[MAX_VAR];
	char head[MAX_VAR];
	int skin;

	/** new abilities and skills: */
	int skills[SKILL_NUM_TYPES];

	/* health points, stun points, armor points and morale points */
	int HP, STUN, AP, morale;
	int maxHP;	/**< to be restored in hospital */

	/** score */
	chrScore_t chrscore;

	int kills[KILLED_NUM_TYPES];
/*  int     destroyed_objects; */
/*  int     hit_ratio; */
/*  int     inflicted_damage; */
/*  int     damage_taken; */
	int assigned_missions;
/*  int     crossed_distance; */
	/* date     joined_edc; */
	/* date     died; */
	int rank; /* index in gd.ranks */
	/* @todo: */
	/* *------------------** */
	/* *------------------** */

	int fieldSize;              /**< ACTOR_SIZE_* */
	inventory_t *inv;

	/** Backlink to employee-struct. */
	int empl_idx;
	int empl_type;

	qboolean armor, weapons; /**< able to use weapons/armor */
	int teamDesc;	/**< id in teamDesc array */
	int category; 	/**< nameCategory id in nameCat */
	int gender;	/**< the gender of this character */
} character_t;

/** @brief The types of employees */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,
	EMPL_MEDIC,
	EMPL_ROBOT,
	MAX_EMPL					/**< for counting over all available enums */
} employeeType_t;

#define MAX_CAMPAIGNS	16

/* min and max values for all teams can be defined via campaign script */
extern int skillValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];
extern int abilityValues[MAX_CAMPAIGNS][MAX_TEAMS][MAX_EMPL][2];

void Com_SetGlobalCampaignID (int campaignID);
int Com_StringToTeamNum(char* teamString) __attribute__((nonnull));
void Com_CharGenAbilitySkills(character_t * chr, int team) __attribute__((nonnull));
char *Com_CharGetBody(character_t* const chr) __attribute__((nonnull));
char *Com_CharGetHead(character_t* const chr) __attribute__((nonnull));

/* =========================================================== */

void Com_InitCSI(csi_t * import) __attribute__((nonnull));
void Com_InitInventory(invList_t * invChain) __attribute__((nonnull));
qboolean Com_CheckToInventory(const inventory_t* const i, const int item, const int container, int x, int y) __attribute__((nonnull(1)));
invList_t *Com_SearchInInventory(const inventory_t* const i, int container, int x, int y) __attribute__((nonnull(1)));
invList_t *Com_AddToInventory(inventory_t* const i, item_t item, int container, int x, int y) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventory(inventory_t* const i, int container, int x, int y) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventoryIgnore(inventory_t* const i, int container, int x, int y, qboolean ignore_type) __attribute__((nonnull(1)));
int Com_MoveInInventory(inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp) __attribute__((nonnull(1)));
int Com_MoveInInventoryIgnore(inventory_t* const i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t ** icp, qboolean ignore_type) __attribute__((nonnull(1)));
void Com_EmptyContainer(inventory_t* const i, const int container) __attribute__((nonnull(1)));
void Com_DestroyInventory(inventory_t* const i) __attribute__((nonnull(1)));
void Com_FindSpace(const inventory_t* const inv, const int item, const int container, int * const px, int * const py) __attribute__((nonnull(1)));
int Com_TryAddToInventory(inventory_t* const inv, item_t item, int container) __attribute__((nonnull(1)));
int Com_TryAddToBuyType(inventory_t* const inv, item_t item, int container) __attribute__((nonnull(1)));
void Com_EquipActor(inventory_t* const inv, const int equip[MAX_OBJDEFS], const char *name, character_t* chr) __attribute__((nonnull(1)));
void INV_PrintToConsole(inventory_t* const i);

/* =========================================================== */

/** @brief Available shoot types */
typedef enum {
	ST_RIGHT,
	ST_RIGHT_REACTION,
	ST_LEFT,
	ST_LEFT_REACTION,

	ST_NUM_SHOOT_TYPES,

	/* 20060905 LordHavoc: added reload types */
	ST_RIGHT_RELOAD,
	ST_LEFT_RELOAD
} shoot_types_t;

#define IS_SHOT_REACTION(x) ((x) == ST_RIGHT_REACTION || (x) == ST_LEFT_REACTION)
#define IS_SHOT(x)          ((x) == ST_RIGHT || (x) == ST_LEFT)
#define IS_SHOT_LEFT(x)     ((x) == ST_LEFT || (x) == ST_LEFT_REACTION)
#define IS_SHOT_RIGHT(x)    ((x) == ST_RIGHT || (x) == ST_RIGHT_REACTION)

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

void Com_PrintItemDescription(int i);
void Com_InventoryList_f(void);
int Com_GetItemByID(const char *id);
qboolean INV_LoadableInWeapon(objDef_t *od, int weapon_idx);
int INV_FiredefsIDXForWeapon(objDef_t *od, int weapon_idx);
int Com_GetDefaultReactionFire(objDef_t *ammo, int weapon_fds_idx);

/* g_spawn.c */

/* NOTE: this only allows quadratic units */
typedef enum {
	ACTOR_SIZE_NORMAL = 1,
	ACTOR_SIZE_UGV
} actorSizeEnum_t;

/** @brief Buytype categories in the various equipment screens (buy/sell, equip, etc...)
 ** Do not mess with the order (especially BUY_AIRCRAFT and BUY_MULTI_AMMO is/will be used for max-check in normal equipment screens)
 ** @sa scripts.c:buytypeNames
 ** @note Be sure to also update all usages of the buy_type" console function (defined in cl_market.c and mostly used there and in menu_buy.ufo) when changing this.
 **/
typedef enum {
	BUY_WEAP_PRI,	/**< All 'Primary' weapons and their ammo for soldiers. */
	BUY_WEAP_SEC,	/**< All 'Secondary' weapons and their ammo for soldiers. */
	BUY_MISC,	/**< Misc sodldier equipment. */
	BUY_ARMOUR,	/**< Armour for soldiers. */
	BUY_MULTI_AMMO, /**< Ammo (and other stuff) that is used in both Pri/Sec weapons. */
	/* MAX_SOLDIER_EQU_BUYTYPES ... possible better solution? */
	BUY_AIRCRAFT,	/**< Aircraft and craft-equipment. */
	BUY_DUMMY,	/**< Everything that is not equipment for soldiers. */
	MAX_BUYTYPES
} equipment_buytypes_t;

#define BUY_PRI(type)	( (((type) == BUY_WEAP_PRI) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the primary category. */
#define BUY_SEC(type)	( (((type) == BUY_WEAP_SEC) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the secondary category. */
#define BUYTYPE_MATCH(type1,type2) (\
	(  ((((type1) == BUY_WEAP_PRI) || ((type1) == BUY_WEAP_SEC)) && ((type2) == BUY_MULTI_AMMO)) \
	|| ((((type2) == BUY_WEAP_PRI) || ((type2) == BUY_WEAP_SEC)) && ((type1) == BUY_MULTI_AMMO)) \
	|| ((type1) == (type2)) ) \
	) /**< Check if the 2 buytypes (type1 and type2) are compatible) */
/** @brief Types of actor sounds being issued by CL_PlayActorSound(). */
typedef enum {
	SND_DEATH,	/**< Sound being played on actor death. */
	SND_HURT,	/**< Sound being played when an actor is being hit. */

	SND_MAX
} actorSound_t;

#endif /* GAME_Q_SHARED_H */
