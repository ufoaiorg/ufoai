/**
 * @file src/shared/shared.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#define MAX_VAR     64

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
#include "ufotypes.h"	/* needed for Com_sprintf */

/* to support the gnuc __attribute__ command */
#if defined __ICC || !defined __GNUC__
#  define __attribute__(x)  /*NOTHING*/
#endif

#if defined(__GNUC__)
#define UFO_DEPRECATED __attribute__ ((deprecated))
#else
#define UFO_DEPRECATED
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
# define EXPORT
# define IMPORT
#endif

#if !defined __STDC_VERSION__
#  define inline
#elif __STDC_VERSION__ < 199901L
/* if we are using ansi - the compiler doesn't know about inline */
#  if defined __GNUC__
#    define inline __inline__
#  elif defined _MSVC
#    define inline __inline
#  else
#    define inline
#  endif
#endif

#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)

const char *Com_SkipPath(const char *pathname);
void Com_ReplaceFilename(const char *fileName, const char *name, char *path, size_t size);
void Com_StripExtension(const char *in, char *out, size_t size);
void Com_FilePath(const char *in, char *out);
const char *Com_GetExtension(const char *path);
void Com_DefaultExtension(char *path, size_t len, const char *extension);
int Com_Filter(const char *pattern, const char *text);
char *Com_Trim(char *s);
char *Com_ConvertToASCII7(char *s);
char *Com_Chop(char *s);

/** returns the amount of elements - not the amount of bytes */
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define endof(x)    ((x) + lengthof((x)))
#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1]

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
int Q_FloatSort(const void *float1, const void *float2);
int Q_StringSort(const void *string1, const void *string2) __attribute__((nonnull));

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/** @todo is this still the case in most recent mingw versions? */
#if defined(__MINGW32_VERSION) && defined(__STRICT_ANSI__)
/* function exists but are not defined */
_CRTIMP char* __cdecl	strdup (const char*) __MINGW_ATTRIB_MALLOC;
_CRTIMP int __cdecl	_stricmp (const char*, const char*);
_CRTIMP int __cdecl	_strnicmp (const char*, const char*, size_t);
#define strncasecmp _strnicmp
#endif

/* portable case sensitive compare */
#if defined(_WIN32)
#	define Q_strcasecmp(a, b) _stricmp((a), (b))
#	define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))
#else
#	define Q_strcasecmp(a, b) strcasecmp((a), (b))
#	define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))
#endif

#define Q_streq(a, b) (strcmp(a, b) == 0)
#define Q_strnull(string) ((string) == NULL || (string)[0] == '\0')
#define Q_strvalid(string) !Q_strnull(string)

#ifndef DEBUG
void Q_strncpyz(char *dest, const char *src, size_t destsize) __attribute__((nonnull));
#else
#define Q_strncpyz(string1,string2,length) Q_strncpyzDebug( string1, string2, length, __FILE__, __LINE__ )
void Q_strncpyzDebug(char *dest, const char *src, size_t destsize, const char *file, int line) __attribute__((nonnull));
#endif

int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap);
void Q_strcat(char *dest, const char *src, size_t size) __attribute__((nonnull));
char *Q_strlwr(char *str) __attribute__((nonnull));
const char *Q_stristr(const char *str, const char *substr) __attribute__((nonnull));
qboolean Q_strreplace(const char *source, const char *pattern, const char *replace, char *dest, size_t destsize);
/** Returns whether start is a prefix of str. */
qboolean Q_strstart(char const *str, char const *start) __attribute__((nonnull));

void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_DPrintf(int level, const char *msg, ...) __attribute__((format(printf, 2, 3)));
void Com_Error(int code, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

#define OBJSET(obj, val) (memset(&(obj), (val), sizeof(obj)))
#define OBJZERO(obj)     OBJSET((obj), 0)

#endif
