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

#define MAX_VAR     64

/** @sa CONTENTS_NODE - @todo replace this with CONTENTS_NODE? */
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

#define	BASEDIRNAME	"base"

const char *COM_SkipPath(const char *pathname);
void COM_StripExtension(const char *in, char *out, size_t size);
void COM_FilePath(const char *in, char *out);
void COM_DefaultExtension(char *path, size_t len, const char *extension);

/** returns the amount of elements - not the amount of bytes */
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1]

/** Is this the second or later byte of a multibyte UTF-8 character? */
/* The definition of UTF-8 guarantees that the second and later
 * bytes of a multibyte character have high bits 10, and that
 * singlebyte characters and the start of multibyte characters
 * never do. */
#define UTF8_CONTINUATION_BYTE(c) (((c) & 0xc0) == 0x80)

int UTF8_delete_char(char *s, int pos);
int UTF8_insert_char(char *s, int n, int pos, int codepoint);
int UTF8_char_len(unsigned char c);
int UTF8_encoded_len(int codepoint);

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
int Q_FloatSort(const void *float1, const void *float2);
int Q_StringSort(const void *string1, const void *string2) __attribute__((nonnull));

qboolean Com_sprintf(char *dest, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/* portable case sensitive compare */
#define Q_strcmp(a, b)     strcmp((a), (b))
#define Q_strncmp(a, b, n) strncmp((a), (b), (n))

#if defined _WIN32
#	define Q_strcasecmp(a, b) _stricmp((a), (b))
#else
#	define Q_strcasecmp(a, b) strcasecmp((a), (b))
#endif

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
const char *Q_stristr(const char *str, const char *substr) __attribute__((nonnull));

#endif
