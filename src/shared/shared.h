/**
 * @file
 */

/*
All original material Copyright (C) 2002-2025 UFO: Alien Invasion.

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

#pragma once

#ifdef HAVE_CONFIG_H
# ifdef ANDROID
#  include "../ports/android/config_android.h"
# else
#  include "../../config.h"
# endif
#endif

#define MAX_VAR     64

#include <cerrno>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <climits>
#include <cstddef>
#include "ufotypes.h"	/* needed for Com_sprintf */
#include <algorithm>

#include "sharedptr.h"
#include "autoptr.h"
#include "cxx.h"

#if defined(__GNUC__) || defined(__clang__)
#define UFO_DEPRECATED __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#define UFO_DEPRECATED __declspec(deprecated)
#else
#define UFO_DEPRECATED
#endif

#ifdef _WIN32
# define EXPORT
# define IMPORT
#else
# define EXPORT
# define IMPORT
#endif

#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)

const char* Com_SkipPath(const char* pathname);
void Com_ReplaceFilename(const char* fileName, const char* name, char* path, size_t size);
void Com_StripExtension(const char* in, char* out, size_t size);
void Com_FilePath(const char* in, char* out, size_t size);
const char* Com_GetExtension(const char* path);
void Com_DefaultExtension(char* path, size_t len, const char* extension);
int Com_Filter(const char* pattern, const char* text);
char* Com_Trim(char* s);
char* Com_ConvertToASCII7(char* s);
char* Com_Chop(char* s);
bool Com_IsValidName(const char* input);

/** returns the amount of elements - not the amount of bytes */
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define endof(x)    ((x) + lengthof((x)))
#define CASSERT(x) static_assert((x), #x)

const char* va(const char* format, ...) __attribute__((format(__printf__, 1, 2)));
int Q_FloatSort(const void* float1, const void* float2);
int Q_StringSort(const void* string1, const void* string2) __attribute__((nonnull));

unsigned int Com_HashKey(const char* name, int hashsize);
void Com_MakeTimestamp(char* ts, const size_t tslen);
bool Com_sprintf(char* dest, size_t size, const char* fmt, ...) __attribute__((format(__printf__, 3, 4)));

/* portable case insensitive compare */
#if defined(_WIN32)
#	define Q_strcasecmp(a, b) _stricmp((a), (b))
#	define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))
#else
#	define Q_strcasecmp(a, b) strcasecmp((a), (b))
#	define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))
#endif

#define Q_strcaseeq(a, b) (Q_strcasecmp(a, b) == 0)
#define Q_streq(a, b) (strcmp(a, b) == 0)
#define Q_strneq(a, b, n) (strncmp(a, b, n) == 0)
inline bool Q_strnull (const char* string) {
	return string == nullptr || string[0] == '\0';
}
#define Q_strvalid(string) !Q_strnull(string)

#ifndef DEBUG
void Q_strncpyz(char* dest, const char* src, size_t destsize) __attribute__((nonnull));
#else
#define Q_strncpyz(string1,string2,length) Q_strncpyzDebug( string1, string2, length, __FILE__, __LINE__ )
void Q_strncpyzDebug(char* dest, const char* src, size_t destsize, const char* file, int line) __attribute__((nonnull));
#endif

int Q_vsnprintf(char* str, size_t size, const char* format, va_list ap);
void Q_strcat(char* dest, size_t destsize, const char* src, ...) __attribute__((nonnull, format(__printf__, 3, 4)));
char* Q_strlwr(char* str) __attribute__((nonnull));
const char* Q_stristr(const char* str, const char* substr) __attribute__((nonnull));

/**
 * @brief Replaces the first occurence of the given pattern in the source string with the given replace string.
 * @param source The source string
 * @param pattern The pattern that should be replaced
 * @param replace The replacement string
 * @param dest The target buffer
 * @param destsize The size of the target buffer
 * @note If this function returns @c false, the target string might be in an undefined stage. E.g. don't
 * rely on it being 0-terminated.
 * @return @c false if the pattern wasn't found or the target buffer is to small to store the resulting
 * string, @c if the replacement was successful.
 */
bool Q_strreplace(const char* source, const char* pattern, const char* replace, char* dest, size_t destsize);

/** Returns a pointer just past the prefix in str, if start is a prefix of str,
 * otherwise a null pointer is returned */
char const* Q_strstart(char const* str, char const* start) __attribute__((nonnull));

void Com_Printf(const char* fmt, ...) __attribute__((format(__printf__, 1, 2)));
void Com_DPrintf(int level, const char* msg, ...) __attribute__((format(__printf__, 2, 3)));
void Com_Error(int code, const char* fmt, ...) __attribute__((noreturn, format(__printf__, 2, 3)));

#define OBJSET(obj, val) (memset(&(obj), (val), sizeof(obj)))
#define OBJZERO(obj)     OBJSET((obj), 0)

#ifdef NDEBUG
#define UFO_assert(condition, ...)
#else
void UFO_assert(bool condition, const char* fmt, ...) __attribute__((format(__printf__, 2, 3)));
#endif
