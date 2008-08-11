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
int Q_FloatSort(const void *float1, const void *float2);
int Q_StringSort(const void *string1, const void *string2) __attribute__((nonnull));

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
/* portable case insensitive compare */
int Q_strncmp(const char *s1, const char *s2, size_t n) __attribute__((nonnull));
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
#define Q_strncpyz(string1,string2,length) Q_strncpyzDebug( string1, string2, length, __FILE__, __LINE__ )
void Q_strncpyzDebug(char *dest, const char *src, size_t destsize, const char *file, int line) __attribute__((nonnull));
#endif
void Q_strcat(char *dest, const char *src, size_t size) __attribute__((nonnull));
char *Q_strlwr(char *str) __attribute__((nonnull));
int Q_putenv(const char *var, const char *value);

#endif
