/**
 * @file
 * @brief Cross-platform type definitions.
 *
 * For compilers that provide it, includes C99 inttypes.h for defining integer types.
 * For those that do not (e.g. MS Visual C), defines integer types with equivalent syntax.
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

#include <stddef.h>
#include <stdint.h>

#ifndef byte
typedef uint8_t byte;
#endif

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

/* structures that can be mapped to vector arrays since they have the same size. */
typedef struct vec2_struct_s {
	vec_t a1, a2;
} vec2_struct_t;
typedef struct vec3_struct_s {
	vec_t a1, a2, a3;
} vec3_struct_t;
typedef struct vec4_struct_s {
	vec_t a1, a2, a3, a4;
} vec4_struct_t;
typedef struct vec5_struct_s {
	vec_t a1, a2, a3, a4, a5;
} vec5_struct_t;

typedef byte pos_t;
typedef pos_t pos3_t[3];
typedef pos_t pos4_t[4];

/* structures that can be mapped to position arrays since they have the same size. */
typedef struct pos3_struct_s {
	pos_t a1, a2, a3;
} pos3_struct_t;
typedef struct pos4_struct_s {
	pos_t a1, a2, a3, a4;
} pos4_struct_t;

typedef int ipos_t;
typedef ipos_t ipos3_t[3];

/* structures that can be mapped to integer position arrays since they have the same size. */
typedef struct ipos3_struct_s {
	ipos_t a1, a2, a3;
} ipos3_struct_t;

typedef int32_t actorSizeEnum_t;

#if defined _WIN32
#	define UFO_SIZE_T "%Iu"
#else
#ifdef __cplusplus
#if __WORDSIZE == 64
# define UFO_SIZE_T "%lu"
#else
# define UFO_SIZE_T "%u"
#endif
#else
# define UFO_SIZE_T "%zu"
#endif
#endif
