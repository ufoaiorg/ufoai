/**
 * @brief Memory allocation routines.
 * @file mem.c
 *
 *
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

#include <stdlib.h>
#include <stdio.h>
#define MEM_SUBSYS MEM_UNSPEC
#include "mem.h"

#define Z_MAGIC 0x1d1d

typedef struct zhead_s {
	struct zhead_s *prev; /**< @brief Previous block. */
	struct zhead_s *next; /**< @brief Next block. */
	uint16_t magic; /**< @brief Magic number for confirming integrity. */
	uint16_t tag; /**< @brief The component that allocated the memory. */
	size_t size; /**< @brief The size of the block allocated. */
} zhead_t;

#ifdef MEMDEBUG
static zhead_t z_chain;
static int32_t z_count = 0;
static int32_t z_bytes = 0;
#endif /* MEMDEBUG */

/**
 * @brief Initialises the memory sub-system.
 */
void Z_InitMem(void)
{
	z_chain.next = z_chain.prev = &z_chain;
}

/**
 * @brief Allocates a block of memory with the given size and tag.
 *
 * @note See TAG_LEVEL and TAG_GAME
 */
void *Z_TagMalloc(size_t size, uint16_t tag)
{
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = calloc(size, 1);
	if (z) {
		z_count++;
		z_bytes += size;
		z->magic = Z_MAGIC;
		z->tag = tag;
		z->size = size;
		z->next = z_chain.next;
		z->prev = &z_chain;
		z_chain.next->prev = z;
		z_chain.next = z;
	} else {
		fprintf(stderr, "Z_TagMalloc: failed on allocation of %Zu bytes", size);
		return NULL;
	}

	return (void *)(z + 1);
}

/**
 * @brief Frees a Z_Malloc'ed pointer.
 */
void Z_Free(void *ptr)
{
	zhead_t *z;

	if (!ptr)
		return;

	z = ((zhead_t *)ptr) - 1;

	if (!z || z->magic != Z_MAGIC) {
		fprintf(stderr, "Z_Free: bad magic (%i)", z->magic);
		return;
	}

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free(z);
}

/**
 * @brief Returns stats about the allocated bytes via Z_Malloc.
 *
 *
 */
zmemstats_t *Z_MemStats(zmemstats_t *stats)
{
	stats->blocks = z_count;
	stats->amount = z_bytes;
	return stats;
}

/**
 * @brief Frees a memory block with a given tag
 *
 *
 */
void Z_FreeTags(uint16_t tag)
{
	zhead_t *z, *next;

	for (z = z_chain.next; z != &z_chain; z = next) {
		next = z->next;
		if (z->tag == tag)
			Z_Free((void *) (z + 1));
	}
}
