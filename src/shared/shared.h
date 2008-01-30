/**
 * @file shared.h
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef SHARED_H
#define SHARED_H

#ifndef M_PI
#define M_PI 3.14159265358979323846  /* matches value in gcc v2 math.h */
#endif

/* important units */
#define UNIT_SIZE   32
#define UNIT_HEIGHT 64

#define MAX_VAR     64

#define LEAFNODE -1

#include <stddef.h>

/* to support the gnuc __attribute__ command */
#if defined __ICC || !defined __GNUC__
#  define __attribute__(x)  /*NOTHING*/
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

#ifdef _WIN32
# ifndef snprintf
#  define snprintf _snprintf
# endif
# define EXPORT __cdecl
# define IMPORT __cdecl
#else
# ifndef stricmp
#  define stricmp strcasecmp
# endif
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

/* angle indexes */
#define PITCH  0   /* up / down */
#define YAW    1   /* left / right */
#define ROLL   2   /* fall over */

#define	BASEDIRNAME	"base"

/** @brief Map boundary is +/- 4096 - to get into the positive area we add the
 * possible max negative value and divide by the size of a grid unit field */
#define VecToPos(v, p) (                  \
	p[0] = ((int)v[0] + 4096) / UNIT_SIZE,  \
	p[1] = ((int)v[1] + 4096) / UNIT_SIZE,  \
	p[2] =  (int)v[2]         / UNIT_HEIGHT \
)
/** @brief Pos boundary size is +/- 128 - to get into the positive area we add
 * the possible max negative value and multiply with the grid unit size to get
 * back the the vector coordinates - now go into the middle of the grid field
 * by adding the half of the grid unit size to this value */
#define PosToVec(p, v) ( \
	v[0] = ((int)p[0] - 128) * UNIT_SIZE   + UNIT_SIZE   / 2, \
	v[1] = ((int)p[1] - 128) * UNIT_SIZE   + UNIT_SIZE   / 2, \
	v[2] =  (int)p[2]        * UNIT_HEIGHT + UNIT_HEIGHT / 2  \
)

const char *COM_SkipPath(const char *pathname);
void COM_StripExtension(const char *in, char *out, size_t size);
void COM_FilePath(const char *in, char *out);

const char *COM_Parse(const char **data_p);

/*
==============================================================
COLLISION DETECTION
==============================================================
*/

/**
 * @note contents flags are seperate bits
 * a given brush can contribute multiple content bits
 * multiple brushes can be in a single leaf
 */

/** lower bits are stronger, and will eat weaker brushes completely */
#define CONTENTS_SOLID  0x0001 /**< an eye is never valid in a solid */
#define CONTENTS_WINDOW 0x0002 /**< translucent, but not watery */
#define CONTENTS_BURN   0x0008 /**< will keep burning when flamed */
#define CONTENTS_WATER  0x0020
/** max 16 bit please - otherwise change EV_ACTOR_MOVE to send a long and not a short */

#define LAST_VISIBLE_CONTENTS 0x80

#define CONTENTS_LEVEL_1 0x0100
#define CONTENTS_LEVEL_2 0x0200
#define CONTENTS_LEVEL_3 0x0400
#define CONTENTS_LEVEL_4 0x0800
#define CONTENTS_LEVEL_5 0x1000
#define CONTENTS_LEVEL_6 0x2000
#define CONTENTS_LEVEL_7 0x4000
#define CONTENTS_LEVEL_8 0x8000

/** remaining contents are non-visible, and don't eat brushes */
#define CONTENTS_ACTORCLIP   0x00010000
#define CONTENTS_PASSABLE    0x00020000
#define CONTENTS_ACTOR       0x00040000 /**< should never be on a brush, only in game */
#define CONTENTS_ORIGIN      0x01000000 /**< removed before bsping an entity */
#define CONTENTS_WEAPONCLIP  0x02000000 /**< stop bullets */
#define CONTENTS_DEADACTOR   0x04000000
#define CONTENTS_DETAIL      0x08000000 /**< brushes to be added after vis leafs */
#define CONTENTS_TRANSLUCENT 0x10000000 /**< auto set if any surface has trans */
#define CONTENTS_STEPON      0x40000000 /**< marks areas elevated passable areas */

#define SURF_LIGHT     0x00000001 /**< value will hold the light strength */
#define SURF_SLICK     0x00000002 /**< effects game physics */
#define SURF_WARP      0x00000008 /**< turbulent water warp */
#define SURF_TRANS33   0x00000010 /**< 0.33 alpha blending */
#define SURF_TRANS66   0x00000020 /**< 0.66 alpha blending */
#define SURF_FLOWING   0x00000040 /**< scroll towards angle */
#define SURF_NODRAW    0x00000080 /**< don't bother referencing the texture */
#define SURF_HINT      0x00000100 /**< make a primary bsp splitter */
#define SURF_SKIP      0x00000200 /**< completely ignored, allowing non-closed brushes */
#define SURF_ALPHATEST 0x02000000 /**< alpha test for transparent textures */

/* content masks */
#define MASK_ALL          (-1)
#define MASK_SOLID        (CONTENTS_SOLID | CONTENTS_WINDOW)
#define MASK_SHOT         (CONTENTS_SOLID | CONTENTS_ACTOR | CONTENTS_WEAPONCLIP | CONTENTS_WINDOW | CONTENTS_DEADACTOR)
#define MASK_VISIBILILITY (CONTENTS_SOLID | CONTENTS_WATER)
#define MASK_CLIP         (CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP | CONTENTS_STEPON)

/*============================================================== */

#define ROUTING_NOT_REACHABLE 0xFF

#define LEVEL_LASTVISIBLE 255
#define LEVEL_WEAPONCLIP 256
#define LEVEL_ACTORCLIP 257
#define LEVEL_STEPON 258
#define LEVEL_TRACING 259
#define LEVEL_MAX 260


#define	PSIDE_FRONT			1
#define	PSIDE_BACK			2
#define	PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING		4

#endif
