/**
 * @file q_shared.c
 * @brief Zone memory allocation
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/game/q_shared.c
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

#include "qcommon.h"

#define	Z_MAGIC		0x1d1d

typedef struct zhead_s {
	struct zhead_s *prev, *next;
	short magic;
	short tag;					/* for group free */
	size_t size;
} zhead_t;

zhead_t z_chain;
int z_count, z_bytes;

/**
 * @brief Frees a Mem_Alloc'ed pointer
 */
extern void Mem_Free (void *ptr)
{
	zhead_t *z;

	if (!ptr)
		return;

	z = ((zhead_t *) ptr) - 1;

	if (z->magic != Z_MAGIC)
		Com_Error(ERR_FATAL, "Mem_Free: bad magic (%i)", z->magic);

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free(z);
}


/**
 * @brief Stats about the allocated bytes via Mem_Alloc
 */
static void Mem_Stats_f (void)
{
	Com_Printf("%i bytes in %i blocks\n", z_bytes, z_count);
}

/**
 * @brief Frees a memory block with a given tag
 */
extern void Mem_FreeTags (int tag)
{
	zhead_t *z, *next;

	for (z = z_chain.next; z != &z_chain; z = next) {
		next = z->next;
		if (z->tag == tag)
			Mem_Free((void *) (z + 1));
	}
}

/**
 * @brief Allocates a memory block with a given tag
 *
 * and fills with 0
 */
extern void *Mem_TagMalloc (size_t size, int tag)
{
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = malloc(size);
	if (!z)
		Com_Error(ERR_FATAL, "Mem_TagMalloc: failed on allocation of "UFO_SIZE_T" bytes", size);
	memset(z, 0, size);
	z_count++;
	z_bytes += size;
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	return (void *) (z + 1);
}

/**
 * @brief Allocate a memory block with default tag
 *
 * and fills with 0
 */
extern void *Mem_Alloc (size_t size)
{
	return Mem_TagMalloc(size, 0);
}

/**
 * @brief
 * @sa Qcommon_Init
 */
extern void Mem_RegisterCommands (void)
{
	Cmd_AddCommand("mem_stats", Mem_Stats_f, "Stats about the allocated bytes via Mem_Alloc");
}

/**
 * @brief
 * @sa Qcommon_Init
 */
extern void Mem_Init (void)
{
	z_chain.next = z_chain.prev = &z_chain;
}

