/*
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

// q_shared.h -- included first by ALL program modules

// stop it from getting included twice
#ifndef _Q_SHARED_H
#define _Q_SHARED_H 1

#ifdef _MSC_VER
// unknown pragmas are SUPPOSED to be ignored, but....
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)		// truncation from const double to float

#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// i18n support via gettext
// needs to be activated via -DHAVE_GETTEXT
#ifdef HAVE_GETTEXT
#ifdef MACOS_X
	#include <intl/libintl.h>
#elif defined(_WIN32)
	#define snprintf _snprintf
	#include "../win32/libintl.h"
#else
	#include <libintl.h>
#endif

#include <locale.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)
#else //HAVE_GETTEXT
// no gettext support
#define _(String) String
#endif

#if (defined _M_IX86 || defined __i386__) && !defined C_ONLY && !defined __sun__
#define id386	1
#else
#define id386	0
#endif

#if defined _M_ALPHA && !defined C_ONLY
#define idaxp	1
#else
#define idaxp	0
#endif


typedef unsigned char 		byte;
typedef enum {false, true}	qboolean;


#ifndef NULL
#define NULL ((void *)0)
#endif


#define NONE				0xFF
#define FULL				0xFFFFFF00

#define WIDTH		256		// absolute max
#define HEIGHT		8		// 15 max

#define MAX_ROUTE		31
#define MAX_MOVELENGTH	60

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	80		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		256		// max length of an individual token

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			128		// max length of a filesystem pathname

//
// per-level limits
//
#define MAX_TILESTRINGS		8
#define MAX_TEAMS			8
#define	MAX_CLIENTS			256		// absolute limit
#define	MAX_EDICTS			1024	// must change protocol to increase more
#define	MAX_LIGHTSTYLES		256
#define	MAX_MODELS			256		// these are sent over the net as bytes
#define	MAX_SOUNDS			256		// so they cannot be blindly increased
#define	MAX_IMAGES			256
#define	MAX_ITEMS			256
#define MAX_GENERAL			(MAX_CLIENTS*2)	// general config strings

#define MAX_FB_LIST			1024

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages



#define	ERR_FATAL			0		// exit the entire game with a popup window
#define	ERR_DROP			1		// print to console and disconnect from game
#define	ERR_DISCONNECT		2		// don't kill server

#define	PRINT_ALL			0
#define PRINT_DEVELOPER		1		// only print when "developer 1"
#define PRINT_ALERT			2

// important units
#define UNIT_SIZE			32
#define UNIT_HEIGHT			64
#define US					UNIT_SIZE
#define UH					UNIT_HEIGHT

// earth map data
#define SIN_ALPHA	0.39875
#define COS_ALPHA	0.91706
//#define HIGH_LAT		+0.953
//#define LOW_LAT		-0.805
//#define CENTER_LAT	(HIGH_LAT+(LOW_LAT))
//#define SIZE_LAT		(HIGH_LAT-(LOW_LAT))
#define HIGH_LAT	+1.0
#define LOW_LAT		-1.0
#define CENTER_LAT	0.0
#define SIZE_LAT	2.0

extern const int dvecs[8][2];
extern const float dvecsn[8][2];
extern const float dangle[8];

extern const byte dvright[8];
extern const byte dvleft[8];

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

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct cplane_s;

extern vec3_t vec3_origin;
extern vec4_t vec4_origin;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// microsoft's fabs seems to be ungodly slow...
//float Q_fabs (float f);
//#define	fabs(f) Q_fabs(f)
#if !defined C_ONLY && !defined __linux__ && !defined __sgi && !defined __MINGW32__
extern long Q_ftol( float f );
#else
#define Q_ftol( f ) ( long ) (f)
#endif

#define VecToPos(v,p)		(p[0]=(((int)v[0]+4096)/US), p[1]=(((int)v[1]+4096)/US), p[2]=((int)v[2]/UH))
#define PosToVec(p,v)		(v[0]=((int)p[0]-128)*US+US/2, v[1]=((int)p[1]-128)*US+US/2, v[2]=(int)p[2]*UH+UH/2)

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b)			(b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define Vector4Copy(a,b)		(b[0]=a[0],b[1]=a[1],b[2]=a[2],b[3]=a[3])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))
#define VectorCompare(a,b)		(a[0]==b[0]?a[1]==b[1]?a[2]==b[2]?1:0:0:0)
#define VectorDistSqr(a,b)		((b[0]-a[0])*(b[0]-a[0])+(b[1]-a[1])*(b[1]-a[1])+(b[2]-a[2])*(b[2]-a[2]))
#define VectorDist(a,b)			(sqrt((b[0]-a[0])*(b[0]-a[0])+(b[1]-a[1])*(b[1]-a[1])+(b[2]-a[2])*(b[2]-a[2])))
#define VectorLengthSqr(a)		(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])

#define PosAddDV(p,dv)			(p[0]+=dvecs[dv&7][0], p[1]+=dvecs[dv&7][1], p[2]=(dv>>3)&7)
int AngleToDV( int angle );

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void VectorClampMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

void MatrixMultiply (vec3_t a[3], vec3_t b[3], vec3_t c[3]);
void GLMatrixMultiply (float a[16], float b[16], float c[16]);
void GLVectorTransform (float m[16], vec4_t in, vec4_t out);
void VectorRotate (vec3_t m[3], vec3_t va, vec3_t vb);

// just in case you do't want to use the macros
vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

void ClearBounds (vec3_t mins, vec3_t maxs);
void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
int VectorCompareEps (vec3_t v1, vec3_t v2);
qboolean VectorNearer (vec3_t v1, vec3_t v2, vec3_t comp);
vec_t VectorLength (vec3_t v);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (vec3_t v, vec3_t out);
void VectorInverse (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t out);
int Q_log2(int val);

void VecToAngles (vec3_t vec, vec3_t angles);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);
float	anglemod(float a);
float LerpAngle (float a1, float a2, float frac);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );

float	frand(void);	// 0 to 1
float	crand(void);	// -1 to 1

//=============================================

void stradd( char **str, const char *addStr );

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_FilePath (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);

char *COM_Parse (char **data_p);
// data is an in/out parm, returns a parsed out token
char *COM_EParse( char **text, char *errhead, char *errinfo );

void Com_sprintf (char *dest, int size, char *fmt, ...);

void Com_PageInMemory (byte *buffer, int size);

//=============================================

// portable case insensitive compare
int Q_stricmp (char *s1, char *s2);
int Q_strcasecmp (char *s1, char *s2);
int Q_strncasecmp (char *s1, char *s2, int n);

//=============================================

short	BigShort(short l);
short	LittleShort(short l);
int		BigLong (int l);
int		LittleLong (int l);
float	BigFloat (float l);
float	LittleFloat (float l);

void	Swap_Init (void);
char	*va(char *format, ...);

//=============================================

//
// key / value info strings
//
#define	MAX_INFO_KEY		64
#define	MAX_INFO_VALUE		64
#define	MAX_INFO_STRING		512

char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_SetValueForKey (char *s, char *key, char *value);
qboolean Info_Validate (char *s);

/*
==============================================================

SYSTEM SPECIFIC

==============================================================
*/

extern	int	curtime;		// time returned by last Sys_Milliseconds

int		Sys_Milliseconds (void);
void	Sys_Mkdir (char *path);

// large block stack allocation routines
void	*Hunk_Begin (int maxsize);
void	*Hunk_Alloc (int size);
void	Hunk_Free (void *buf);
int		Hunk_End (void);

// directory searching
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

/*
** pass in an attribute mask of things you wish to REJECT
*/
char	*Sys_FindFirst (char *path, unsigned musthave, unsigned canthave );
char	*Sys_FindNext ( unsigned musthave, unsigned canthave );
void	Sys_FindClose (void);


// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...);
void Com_Printf (char *msg, ...);


/*
==========================================================

CVARS (console variables)

==========================================================
*/

#ifndef CVAR
#define	CVAR

#define	CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define	CVAR_USERINFO	2	// added to userinfo  when changed
#define	CVAR_SERVERINFO	4	// added to serverinfo when changed
#define	CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define	CVAR_LATCH		16	// save changes until server restart

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	char		*name;
	char		*string;
	char		*latched_string;	// for CVAR_LATCH vars
	int			flags;
	qboolean	modified;	// set each time the cvar is changed
	float		value;
	struct cvar_s *next;
} cvar_t;

cvar_t *Cvar_Get (char *var_name, char *value, int flags);

#endif		// CVAR

/*
==============================================================

COLLISION DETECTION

==============================================================

*/

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			2		// translucent, but not watery
#define	CONTENTS_AUX			4
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_MIST			64
#define	LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_ACTORCLIP		0x10000
#define	CONTENTS_PASSABLE		0x20000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x40000
#define	CONTENTS_CURRENT_90		0x80000
#define	CONTENTS_CURRENT_180	0x100000
#define	CONTENTS_CURRENT_270	0x200000
#define	CONTENTS_CURRENT_UP		0x400000
#define	CONTENTS_CURRENT_DOWN	0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_ACTOR			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_DEADACTOR		0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000
#define	CONTENTS_STEPON			0x40000000	// marks areas elevated passable areas



#define	SURF_LIGHT		0x1		// value will hold the light strength

#define	SURF_SLICK		0x2		// effects game physics

#define	SURF_SKY		0x4		// don't draw, but add to skybox
#define	SURF_WARP		0x8		// turbulent water warp
#define	SURF_TRANS33	0x10
#define	SURF_TRANS66	0x20
#define	SURF_FLOWING	0x40	// scroll towards angle
#define	SURF_NODRAW		0x80	// don't bother referencing the texture



// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_WINDOW)
#define	MASK_ACTORSOLID			(CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW|CONTENTS_ACTOR)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW)
//#define	MASK_MONSTERSOLID		(CONTENTS_SOLID|CONTENTS_ACTORCLIP|CONTENTS_WINDOW|CONTENTS_ACTOR)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_ACTOR|CONTENTS_WINDOW|CONTENTS_DEADACTOR)
#define MASK_CURRENT			(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define	AREA_SOLID		1
#define	AREA_TRIGGERS	2


// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;

// structure offset for asm code
#define CPLANE_NORMAL_X			0
#define CPLANE_NORMAL_Y			4
#define CPLANE_NORMAL_Z			8
#define CPLANE_DIST				12
#define CPLANE_TYPE				16
#define CPLANE_SIGNBITS			17
#define CPLANE_PAD0				18
#define CPLANE_PAD1				19

typedef struct cmodel_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	int			tile;
	int			headnode;
} cmodel_t;

//extern int			numcmodels;
//extern cmodel_t	map_cmodels[1024];

typedef struct csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} csurface_t;

typedef struct mapsurface_s  // used internally due to name len probs //ZOID
{
	csurface_t	c;
	char		rname[32];
} mapsurface_t;

// a trace is returned when a box is swept through the world
typedef struct
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	struct le_s	*le;		// not set by CM_*() functions
	struct edict_s	*ent;	// not set by CM_*() functions
} trace_t;


// entity_state_t->renderfx flags
#define	RF_MINLIGHT			0x00000001		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL		0x00000002		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		0x00000004		// only draw through eyes
#define	RF_FULLBRIGHT		0x00000008		// allways draw full intensity
#define	RF_DEPTHHACK		0x00000010		// for view weapon Z crunching
#define	RF_TRANSLUCENT		0x00000020
#define	RF_FRAMELERP		0x00000040
#define RF_BEAM				0x00000080
#define	RF_CUSTOMSKIN		0x00000100		// skin is an index in image_precache
#define	RF_GLOW				0x00000200		// pulse lighting for bonus items
#define RF_SHELL_RED		0x00000400
#define	RF_SHELL_GREEN		0x00000800
#define RF_SHELL_BLUE		0x00001000

#define RF_BOX				0x00002000
#define RF_SHADOW			0x00004000
#define RF_SELECTED			0x00008000
#define RF_MEMBER			0x00010000
#define RF_ALLIED			0x00020000

#define RF_LIGHTFIXED		0x00040000
#define RF_NOSMOOTH			0x00080000

// player_state_t->refdef flags
#define	RDF_UNDERWATER		1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	2		// used for player configuration screen

//ROGUE
#define	RDF_IRGOGGLES		4
#define RDF_UVGOGGLES		8
//ROGUE



// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4
// modifier flags
#define	CHAN_NO_PHS_ADD			8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE			16	// send by reliable message, not datagram


// sound attenuation values
#define	ATTN_NONE               0	// full volume the entire level
#define	ATTN_NORM               1
#define	ATTN_IDLE               2
#define	ATTN_STATIC             3	// diminish very rapidly with distance


/*
==============================================================

SCRIPT VALUE PARSING

==============================================================
*/

#define MAX_VAR		64

typedef enum
{
	V_NULL,
	V_BOOL,
	V_CHAR,
	V_INT,
	V_FLOAT,
	V_POS,
	V_VECTOR,
	V_COLOR,
	V_RGBA,
	V_STRING,
	V_LONGSTRING,
	V_POINTER,
	V_ALIGN,
	V_BLEND,
	V_STYLE,
	V_FADE,
	V_SHAPE_SMALL,
	V_SHAPE_BIG,
	V_DMGTYPE,
	V_DATE,
	V_IF,

	V_NUM_TYPES
} value_types;

#define V_UNTYPED	0x7FFF

extern char *vt_names[V_NUM_TYPES];

typedef enum
{
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

typedef enum
{
	BLEND_REPLACE,
	BLEND_BLEND,
	BLEND_ADD,
	BLEND_FILTER,
	BLEND_INVFILTER,

	BLEND_LAST
} blend_t;

typedef enum
{
	STYLE_FACING,
	STYLE_ROTATED,
	STYLE_BEAM,
	STYLE_LINE,

	STYLE_LAST
} style_t;

typedef enum
{
	FADE_NONE,
	FADE_IN,
	FADE_OUT,
	FADE_SIN,
	FADE_SAW,
	FADE_BLEND,

	FADE_LAST
} fade_t;

typedef struct date_s
{
	int		day, sec;
} date_t;

extern char *align_names[ALIGN_LAST];
extern char *blend_names[BLEND_LAST];
extern char *style_names[STYLE_LAST];
extern char *fade_names[FADE_LAST];

typedef struct value_s
{
	char	*string;
	int		type;
	int		ofs;
} value_t;

int Com_ParseValue( void *base, char *token, int type, int ofs );
int Com_SetValue( void *base, void *set, int type, int ofs );
char *Com_ValueToStr( void *base, int type, int ofs );

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#define TEAM_CIVILIAN	0
#define TEAM_ALIEN		7

// add this flag for instant event execution
#define INSTANTLY	0x80

typedef enum {
	EV_NULL,
	EV_RESET,
	EV_START,
	EV_ENDROUND,
	EV_RESULTS,
	EV_CENTERVIEW,

	EV_ENT_APPEAR,
	EV_ENT_PERISH,

	EV_ACTOR_APPEAR,
	EV_ACTOR_START_MOVE,
	EV_ACTOR_TURN,
	EV_ACTOR_MOVE,
	EV_ACTOR_START_SHOOT,
	EV_ACTOR_SHOOT,
	EV_ACTOR_SHOOT_HIDDEN,
	EV_ACTOR_THROW,
	EV_ACTOR_DIE,
	EV_ACTOR_STATS,
	EV_ACTOR_STATECHANGE,

	EV_INV_ADD,
	EV_INV_DEL,
	EV_INV_AMMO,

	EV_MODEL_PERISH,
	EV_MODEL_EXPLODE,

	EV_NUM_EVENTS
} event_t;


typedef enum {
	ET_NULL,
	ET_ACTORSPAWN,
	ET_ACTOR,
	ET_ITEM,
	ET_BREAKABLE
} entity_type_t;


typedef enum {
	PA_NULL,
	PA_TURN,
	PA_MOVE,
	PA_STATE,
	PA_SHOOT,
	PA_INVMOVE
} player_action_t;


typedef enum {
	IA_NONE,
	IA_MOVE,
	IA_RELOAD,
	IA_NOTIME,
	IA_NORELOAD,
	IA_ARMOR
} inventory_action_t;


// ===========================================================

// this is the absolute max for now
#define MAX_OBJDEFS		128
#define MAX_DAMAGETYPES	32

#define GET_FIREDEF(type)	(&csi.ods[type & 0x7F].fd[!!(type & 0x80)])

typedef struct fireDef_s
{
	char	name[MAX_VAR];
	char	projectile[MAX_VAR];
	char	impact[MAX_VAR];
	char	hitBody[MAX_VAR];
	char	fireSound[MAX_VAR];
	char	impactSound[MAX_VAR];
	char	hitBodySound[MAX_VAR];
	char	bounceSound[MAX_VAR];
	byte	soundOnce;
	byte	gravity;
	byte	selfDetonate;
	byte	dmgtype;
	float	speed;
	vec2_t	shotOrg;
	vec2_t	spread;
	int		delay;
	int		bounce;
	float	bounceFac;
	float	crouch;
	float	range;
	int		shots;
	int		ammo;
	float	rof;
	int		time;
	vec2_t	damage, spldmg;
	float	splrad;
	int		weaponSkill;
	int		irgoggles;
} fireDef_t;

typedef enum researchStatus_s
{
	RS_NONE,
	RS_RUNNING,
	RS_PAUSED,
	RS_FINISH
} researchStatus_t;

typedef enum researchType_s
{
	RS_TECH,
	RS_WEAPON,
	RS_ARMOR,
	RS_CRAFT,
	RS_BUILDING
} researchType_t;

typedef struct objDef_s
{
	// common
	char	name[MAX_VAR];
	char	kurz[MAX_VAR];
	char	model[MAX_VAR];
	char	image[MAX_VAR];
	char	type[MAX_VAR];
	int		shape;
	byte	sx, sy;
	float	scale;
	vec3_t	center;
	char	category;
	byte	weapon;
	byte	twohanded;
	byte	thrown;
	int		price;
	int		buytype;
	int		link;

	// weapon specific
	int		ammo;
	int		reload;
	fireDef_t	fd[2];

	// armor specific
	short	protection[MAX_DAMAGETYPES];
	short	hardness[MAX_DAMAGETYPES];

	// research
	int researchNeeded; // all alien weapons need this
	int researchTime; // count down
	researchStatus_t researchStatus; // status of researching
} objDef_t;

#define MAX_INVDEFS		16

typedef struct invDef_s
{
	char	name[MAX_VAR];
	byte	single, armor, all, temp;
	int		shape[16];
	int		in, out;
} invDef_t;

#define MAX_CONTAINERS	32
#define MAX_INVLIST		1024

typedef struct item_s
{
	int	t; // twohanded
	int	a; // ammo
	int	m; // model
} item_t;

typedef struct invList_s
{
	item_t	item;
	int		x, y;
	struct	invList_s *next;
} invList_t;

typedef struct inventory_s
{
	invList_t	*c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS	64

typedef struct equipDef_s
{
	char	name[MAX_VAR];
	int		num[MAX_OBJDEFS];
	byte	num_loose[MAX_OBJDEFS];
} equipDef_t;

// the csi structure is the client-server-information structure
// which contains all the UFO info needed by the server and the client
typedef struct csi_s
{
	objDef_t	ods[MAX_OBJDEFS];
	int		numODs;

	invDef_t	ids[MAX_INVDEFS];
	int		numIDs;
	int		idRight, idLeft, idBelt, idArmor, idFloor, idEquip;

	equipDef_t	eds[MAX_EQUIPDEFS];
	int		numEDs;

	char	dts[MAX_DAMAGETYPES][MAX_VAR];
	int		numDTs;
} csi_t;

// ===========================================================

// TODO: Medals. Still subject to (major) changes.

#define MAX_SKILL			256

#define GET_HP( ab )		(80 + (ab) * 90/MAX_SKILL)
#define GET_ACC( ab, sk )	((1.5 - (float)(ab)/MAX_SKILL) * pow(0.8, (sk)/50) )
#define GET_TU( ab )		(27 + (ab) * 20/MAX_SKILL)
#define GET_MORALE( ab )		(100 + (ab) * 150/MAX_SKILL)

typedef enum
{
	ABILITY_POWER,
	ABILITY_SPEED,
	ABILITY_ACCURACY,
	ABILITY_MIND,

	SKILL_CLOSE,
	SKILL_HEAVY,
	SKILL_ASSAULT,
	SKILL_PRECISE,
	SKILL_EXPLOSIVE,
	SKILL_NUM_TYPES
} abilityskills_t;
#define ABILITY_NUM_TYPES SKILL_CLOSE


typedef enum
{
	MEDAL_CROSS,
	MEDAL_COIN
} medalType_t;

#define MAX_MEDALTEXT		256
#define MAX_MEDALTITLE		32
typedef struct medals_s
{
	char	title[MAX_MEDALTITLE];
	medalType_t	type;
	int	band;
	abilityskills_t	affectedSkill;
	int	skillIncrease;
	//date	date;
	char	text[MAX_MEDALTEXT];
	struct	medals_s	*next_medal;
} medals_t;

typedef struct character_s
{
	int		ucn;
	char	name[MAX_VAR];
	char	path[MAX_VAR];
	char	body[MAX_VAR];
	char	head[MAX_VAR];
	int		skin;

	// new abilities and skills:
	int		skills[SKILL_NUM_TYPES];

	// score
	int		kills_enemies;
	int		kills_civilians;
	int		kills_team;
	int		kills_animals;
	int		kills_neutral;
	int		kills_male;
	int		kills_female;
	int		destroyed_objects;
	int		hit_ratio;
	int		inflicted_damage;
	int		damage_taken;
	int		assigned_missions;
	int		crossed_distance;
	// date		joined_edc;
	// date		died;
	medals_t *medals;
	// TODO:
	// *------------------**
	// *------------------**

	inventory_t *inv;
} character_t;

void Com_CharGenAbilitySkills (character_t *chr, int minAbility, int maxAbility, int minSkill, int maxSkill);
char *Com_CharGetBody( character_t *chr );
char *Com_CharGetHead( character_t *chr );

// ===========================================================

void Com_InitCSI( csi_t *import );
void Com_InitInventory( invList_t *invChain );
qboolean Com_CheckToInventory( inventory_t *i, int item, int container, int x, int y );
invList_t *Com_SearchInInventory( inventory_t *i, int container, int x, int y );
invList_t *Com_AddToInventory( inventory_t *i, item_t item, int container, int x, int y );
qboolean Com_RemoveFromInventory( inventory_t *i, int container, int x, int y );
int Com_MoveInInventory( inventory_t *i, int from, int fx, int fy, int to, int tx, int ty, int *TU, invList_t **icp );
void Com_EmptyContainer( inventory_t *i, int container );
void Com_DestroyInventory( inventory_t *i );
void Com_FindSpace( inventory_t *inv, int item, int container, int *px, int *py );

// ===========================================================

typedef enum {
	ST_RIGHT_PRIMARY,
	ST_RIGHT_SECONDARY,
	ST_LEFT_PRIMARY,
	ST_LEFT_SECONDARY,

	ST_NUM_SHOOT_TYPES
} shoot_types_t;

// shoot flags
#define SF_IMPACT			1
#define SF_BODY				2
#define SF_BOUNCING			4
#define SF_BOUNCED			8

// state flags
// public
#define STATE_PUBLIC		0x00FF
#define STATE_DEAD			0x0003		// 0 alive, 1-3 different deaths
#define STATE_CROUCHED		0x0004
#define STATE_PANIC			0x0008
#define STATE_RAGE			0x0010		// pretty self-explaining
#define STATE_INSANE		0x0030
// private
#define STATE_REACTION		0x0100
#define STATE_SHAKEN		0x0300		// forced reaction fire


#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

//

#define EYE_STAND		15
#define EYE_CROUCH		3
#define PLAYER_STAND	20
#define PLAYER_CROUCH	5
#define PLAYER_DEAD		-12
#define PLAYER_MIN		-24
#define PLAYER_WIDTH	9

//

#define GRAVITY			500.0

//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define	CS_NAME				0
#define	CS_CDTRACK			1
#define	CS_SKY				2
#define	CS_SKYAXIS			3		// %f %f %f format
#define	CS_SKYROTATE		4
#define	CS_STATUSBAR		5		// display program string
#define	CS_MAXCLIENTS		6
#define	CS_MAPCHECKSUM		7		// for catching cheater maps
#define	CS_MAXSOLDIERS		8               // max soldiers per team
#define	CS_MAXSOLDIERSPERPLAYER	9               // max soldiers per player when in teamplay mode
#define	CS_ENABLEMORALE		10              // enable the morale states in multiplayer

#define CS_TILES			16
#define CS_POSITIONS		(CS_TILES+MAX_TILESTRINGS)
#define	CS_MODELS			(CS_POSITIONS+MAX_TILESTRINGS)
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)
#define	CS_ITEMS			(CS_LIGHTS+MAX_LIGHTSTYLES)
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)
#define CS_GENERAL			(CS_PLAYERSKINS+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS	(CS_GENERAL+MAX_GENERAL)


//==============================================

#endif  // _Q_SHARED_H
