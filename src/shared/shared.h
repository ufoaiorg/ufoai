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

#ifdef HAVE_CONFIG_H
# include "../../config.h"
#endif

/* important units */
#define UNIT_SIZE   32
#define UNIT_HEIGHT 64
/* player height - 12 to be able to walk trough doors
 * UNIT_HEIGHT is the height of one level */
#define PLAYER_HEIGHT		(UNIT_HEIGHT - 16)
#define QUANT	4

#define MAX_VAR     64

#define LEAFNODE -1
#define	PLANENUM_LEAF -1

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
#include <stddef.h>

/* to support the gnuc __attribute__ command */
#if defined __ICC || !defined __GNUC__
#  define __attribute__(x)  /*NOTHING*/
#endif

#ifndef NULL
#define NULL ((void *)0)
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

#define	BASEDIRNAME	"base"

const char *COM_SkipPath(const char *pathname);
void COM_StripExtension(const char *in, char *out, size_t size);
void COM_FilePath(const char *in, char *out);
void COM_DefaultExtension(char *path, size_t len, const char *extension);

const char *COM_Parse(const char **data_p);
const char *COM_EParse(const char **text, const char *errhead, const char *errinfo);

/** returns the amount of elements - not the amount of bytes */
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1]

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
int Q_StringSort(const void *string1, const void *string2) __attribute__((nonnull));

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
/* portable case insensitive compare */
int Q_strncmp(const char *s1, const char *s2, size_t n) __attribute__((nonnull));
int Q_strmatch(const char *s1, const char * s2);
int Q_strcmp(const char *s1, const char *s2) __attribute__((nonnull));
int Q_stricmp(const char *s1, const char *s2) __attribute__((nonnull));
int Q_strcasecmp(const char *s1, const char *s2) __attribute__((nonnull));
#ifdef HAVE_STRNCASECMP
# define Q_strncasecmp(s1, s2, n) strncasecmp(s1, s2, n)
#else
int Q_strncasecmp(const char *s1, const char *s2, size_t n) __attribute__((nonnull));
#endif
int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#ifndef DEBUG
void Q_strncpyz(char *dest, const char *src, size_t destsize) __attribute__((nonnull));
#else
void Q_strncpyzDebug(char *dest, const char *src, size_t destsize, const char *file, int line) __attribute__((nonnull));
#endif
void Q_strcat(char *dest, const char *src, size_t size) __attribute__((nonnull));
char *Q_strlwr(char *str) __attribute__((nonnull));
int Q_putenv(const char *var, const char *value);


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

#define CONTENTS_LEVEL_ALL 0xFF00
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
#define CONTENTS_FOOTSTEP    0x00040000 /**< only for grabbing footsteps textures */
#define CONTENTS_ACTOR       0x00800000 /**< should never be on a brush, only in game */
#define CONTENTS_ORIGIN      0x01000000 /**< removed before bsping an entity */
#define CONTENTS_WEAPONCLIP  0x02000000 /**< stop bullets */
#define CONTENTS_DEADACTOR   0x04000000
#define CONTENTS_DETAIL      0x08000000 /**< brushes to be added after vis leafs also used for debugging local entities */
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
#define SURF_PHONG     0x00000400 /**< phong interpolated lighting at compile time */
#define SURF_ALPHATEST 0x02000000 /**< alpha test for transparent textures */

/* content masks */
#define MASK_ALL          (-1)
#define MASK_SOLID        (CONTENTS_SOLID | CONTENTS_WINDOW)
#define MASK_SHOT         (CONTENTS_SOLID | CONTENTS_ACTOR | CONTENTS_WEAPONCLIP | CONTENTS_WINDOW | CONTENTS_DEADACTOR)
#define MASK_VISIBILILITY (CONTENTS_SOLID | CONTENTS_WATER)
#define MASK_CLIP         (CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP | CONTENTS_STEPON)

/*============================================================== */

#define ROUTING_NOT_REACHABLE 0xFF

/* Battlescape map dimensions (WIDTH*WIDTH*HEIGHT) */
#define PATHFINDING_WIDTH       256         /* absolute max */
#define PATHFINDING_HEIGHT      8           /* 15 max */

#define LEVEL_LASTVISIBLE 255
#define LEVEL_WEAPONCLIP 256
#define LEVEL_ACTORCLIP 257
#define LEVEL_STEPON 258
#define LEVEL_TRACING 259
#define LEVEL_MAX 260

#define LIGHTMAP_NIGHT 0
#define LIGHTMAP_DAY 1
#define LIGHTMAP_MAX 2

#define	PSIDE_FRONT			1
#define	PSIDE_BACK			2
#define	PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING		4

#define COLORED_GREEN 1

#define MAX_TOKEN_CHARS     256 /* max length of an individual token */

#endif
