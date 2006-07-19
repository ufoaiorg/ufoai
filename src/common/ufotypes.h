/**
 * @file ufotypes.h
 * @brief Cross-platform type definitions.
 *
 * For compilers that provide it, includes C99 inttypes.h for defining integer types.
 * For those that do not (e.g. MS Visual C), defines integer types with equivalent syntax.
 * Also defines float32_t, double64_t and bool_t, to match the naming convention used by integer types.
 */

/*
 * Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef COMMON_UFOTYPES_H
#define COMMON_UFOTYPES_H

#include <stddef.h>

#ifndef _MSC_VER
#include <inttypes.h>
#else

#include <limits.h>

typedef   signed __int8  int8_t;
typedef   signed __int16 int16_t;
typedef   signed __int32 int32_t;
typedef   signed __int64 int64_t;

typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#define INT8_MIN  _I8_MIN
#define INT16_MIN _I16_MIN
#define INT32_MIN _I32_MIN
#define INT64_MIN _I64_MIN

#define INT8_MAX  _I8_MAX
#define INT16_MAX _I16_MAX
#define INT32_MAX _I32_MAX
#define INT64_MAX _I64_MAX

#define UINT8_MAX  _UI8_MAX
#define UINT16_MAX _UI16_MAX
#define UINT32_MAX _UI32_MAX
#define UINT64_MAX _UI64_MAX

#endif

typedef float float32_t;
typedef double double64_t;

/* NOTE: MSVC does NOT properly support 80-bit long doubles. They should NOT be used.*/

#ifdef __cplusplus
/* Most platforms define bool as 8 bits. If this is not the case, please register a bug on the tracker.
 * G++ & MSVC do on x86 & x86_64.
 */
typedef bool bool_t;
#else
typedef uint8_t bool_t;
#define false 0
#define true 1
#endif /* __cplusplus */

#endif /* COMMON_UFOTYPES_H */

