/**
 * @file cl_alienbase.c
 * @brief
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"
#include "cl_alienbase.h"

static alienBase_t alienBases[MAX_ALIEN_BASES];		/**< Alien bases spawned in game */
static int numAlienBases;							/**< Number of alien bases in game */

/**
 * @brief Reset global array and number of base
 * @note Use me when stating a new game
 */
void AB_ResetAlienBases (void)
{
	memset(&alienBases[MAX_ALIEN_BASES], 0, sizeof(alienBases[MAX_ALIEN_BASES]));
	numAlienBases = 0;
}

/**
 * @brief Build a new alien base
 * @param[in] pos Position of the new base.
 * @return Pointer to the base that has been built.
 */
alienBase_t* AB_BuildBase (vec2_t pos)
{
	alienBase_t *base;
	const float initialStealthValue = 50.0f;				/**< How hard PHALANX will find the base */

	if (numAlienBases >= MAX_ALIEN_BASES) {
		Com_Printf("AB_BuildBase: Too many alien bases build\n");
		return NULL;
	}

	base = &alienBases[numAlienBases];
	memset(base, 0, sizeof(base));

	Vector2Copy(pos, base->pos);
	base->stealth = initialStealthValue;
	base->idx = numAlienBases++;

	return base;
}

/**
 * @brief Destroy an alien base
 */
qboolean AB_DestroyBase (alienBase_t *base)
{
	int i;
	int idx = base->idx;

	numAlienBases--;
	assert(numAlienBases >= idx);
	memmove(base, base + 1, (numAlienBases - idx) * sizeof(*base));
	/* wipe the now vacant last slot */
	memset(&alienBases[numAlienBases], 0, sizeof(alienBases[numAlienBases]));

	for (i = idx; i < numAlienBases; i++)
		alienBases[i].idx--;

	return qtrue;
}

/**
 * @brief Get Alien Base per Idx
 * @param[in] checkIdx True if you want to check if baseIdx is lower than number of base
 */
alienBase_t* AB_GetBase (int baseIDX, qboolean checkIdx)
{
	if (baseIDX < 0 || baseIDX >= MAX_ALIEN_BASES)
		return NULL;

	if (checkIdx && baseIDX >= numAlienBases)
		return NULL;

	return &alienBases[baseIDX];
}

/**
 * @brief Load callback for alien base data
 * @sa AB_Save
 */
qboolean AB_Load (sizebuf_t *sb, void *data)
{
	int i;

	numAlienBases = MSG_ReadShort(sb);
	for (i = 0; i < presaveArray[PRE_MAXALB]; i++) {
		alienBase_t *base = &alienBases[i];
		base->idx = MSG_ReadShort(sb);
		MSG_Read2Pos(sb, base->pos);
		base->supply = MSG_ReadShort(sb);
		base->stealth = MSG_ReadFloat(sb);
	}
	return qtrue;
}

/**
 * @brief Save callback for alien base data
 * @sa AB_Load
 */
qboolean AB_Save (sizebuf_t *sb, void *data)
{
	int i;

	MSG_WriteShort(sb, numAlienBases);
	for (i = 0; i < presaveArray[PRE_MAXALB]; i++) {
		const alienBase_t *base = &alienBases[i];
		MSG_WriteShort(sb, base->idx);
		MSG_Write2Pos(sb, base->pos);
		MSG_WriteShort(sb, base->supply);
		MSG_WriteFloat(sb, base->stealth);
	}

	return qtrue;
}
