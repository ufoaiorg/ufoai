/**
 * @file ufotypes.h
 * @brief Cross-platform type definitions.
 *
 * For compilers that provide it, includes C99 inttypes.h for defining integer types.
 * For those that do not (e.g. MS Visual C), defines integer types with equivalent syntax.
 * Also defines float32_t, double64_t and bool_t, to match the naming convention used by integer types.
 */

/*
 * Copyright (C) 2002-2011 UFO: Alien Invasion.
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
#include <stdint.h>

typedef float float32_t;
typedef double double64_t;

typedef enum {qfalse, qtrue} qboolean;

#ifndef byte
typedef uint8_t byte;
#endif

#if defined _WIN64
# define UFO_SIZE_T "%I64u"
#elif defined _WIN32
# define UFO_SIZE_T "%u"
#else
# define UFO_SIZE_T "%zu"
#endif

#endif /* COMMON_UFOTYPES_H */
