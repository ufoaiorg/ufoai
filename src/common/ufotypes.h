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

#ifndef __MSVC
#include <inttypes.h>
#else

typedef signed __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#define INT8_MIN (-128) 
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483648)
#define INT64_MIN (-9223372036854775808)

#define INT8_MAX (127)
#define INT16_MAX (32767)
#define INT32_MAX (2147483647)
#define INT64_MAX (9223372036854775807)

#define UINT8_MAX (255)
#define UINT16_MAX (65535)
#define UINT32_MAX (4294967295)
#define UINT64_MAX (18446744073709551615)

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

