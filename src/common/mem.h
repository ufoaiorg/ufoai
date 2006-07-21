/**
 * @brief Memory allocation routines.
 * @file mem.h
 *
 * All dynamic memory allocation should be done using ufo_malloc(), ufo_calloc() and ufo_free().
 * With DEBUG defined, ufo_malloc & ufo_calloc will print a debugging message when a malloc fails.
 * With MEMDEBUG defined, ufo_malloc will count the number of and amount of memory allocated.
 *
 * Each compiliation unit (C file) should define the macro MEM_SUBSYS to be one of the MEM_* defines in this header.
 * The value specified is used to track which modules have allocated memory and will aid tracking of memory leaks.
 */

/*
 * All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.
 *
 * Adapted from Quake 2 v3.21: quake2-2.31/qcommon/common.c
 * Copyright (C) 1997-2001 Id Software, Inc.
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

#ifndef COMMON_MEM_H
#define COMMON_MEM_H

#include "../common/ufotypes.h"

/* Use the following defines to set MEM_SUBSYS in each compilation unit (c file). */
#define MEM_UNSPEC 0xFFFF
/* 0-99 Common components. */
#define MEM_COMMON 0
#define MEM_SCRIPT 1
#define MEM_NET    2
#define MEM_MODEL  3
/* 100 - 199 Client components. */
#define MEM_CLIENT 100
#define MEM_INPUT  101
#define MEM_RENDER 102
#define MEM_SOUND  103
#define MEM_SYS    104
/* 200 - 209 Game components. */
#define MEM_GAME   200
/* 300 - 309 Server components. */
#define MEM_SERVER 300

/* This is defined to allow */
#ifndef MEM_SUBSYS
#ifdef __GNUC__ 
#warning "MEM_SUBSYS not defined before including 'common/mem.h'"
#else
#pragma message("warning: MEM_SUBSYS not defined before including 'common/mem.h'")
#endif /* __GNUC__ */
#define MEM_SUBSYS MEM_UNSPEC
#endif /* MEM_SUBSYS */

#ifdef DEBUG
#ifndef MEMDEBUG
#define MEMDEBUG
#endif /* MEMDEBUG */
#endif /* DEBUG */

#ifdef MEMDEBUG
#define z_malloc(s) Z_TagMalloc(s, MEM_SUBSYS)
#define z_calloc(s) Z_TagMalloc(s, MEM_SUBSYS)
#define z_free(p) Z_Free(p)
#else
#define z_malloc(s) malloc(s)
#define z_calloc(s) calloc(s, 1)
#define z_free(p) free(p)
#endif /* MEMDEBUG */

#ifdef DEBUG
#define UFO_Malloc(v, s) if (!(v = z_malloc(s))) Com_Error(ERR_FATAL, "malloc() failed allocation of %i bytes in __FILE__ at line __LINE__", s)
#define UFO_Calloc(v, s) if (!(v = z_calloc(s))) Com_Error(ERR_FATAL, "calloc() failed allocation of %i bytes in __FILE__ at line __LINE__", s)
#else
#define UFO_Malloc(v, s) (v = z_malloc(s))
#define UFO_Calloc(v, s) (v = z_calloc(s))
#endif /* DEBUG */

#define UFO_Free(p) (z_free(p))

typedef struct zmemstats_s {
	int32_t blocks;
	int32_t amount;
} zmemstats_t;

void Z_InitMem(void);
void *Z_TagMalloc(size_t size, uint16_t tag);
void Z_Free(void *ptr);
zmemstats_t *Z_MemStats(zmemstats_t *stats);
void Z_FreeTags(uint16_t tag);

#endif /* COMMON_MEM_H */

