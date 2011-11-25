/**
 * @file g_edicts.c
 * @brief functions to handle the storage of all edicts in the game module.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_utils.c
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

#include "g_local.h"

/** This is where we store the edicts */
static edict_t *g_edicts;

/**
 * @brief Allocate space for the entity pointers.
 * @note No need to set it to zero, G_TagMalloc will do that for us
 */
edict_t* G_EdictsInit (void)
{
	g_edicts = (edict_t *)G_TagMalloc(game.sv_maxentities * sizeof(g_edicts[0]), TAG_GAME);
	return g_edicts;
}

/**
 * @brief Reset the entity pointers for eg. a new game.
 */
void G_EdictsReset (void)
{
	memset(g_edicts, 0, game.sv_maxentities * sizeof(g_edicts[0]));
}

/**
 * @brief Get an entity's ID number
 * @param ent The entity
 * @return the entity's ID number, currently the index in the array
 * @note DO NOT use this number as anything other than an identifier !
 */
int G_EdictsGetNumber (const edict_t* ent)
{
	int idx = ent - g_edicts;
	assert(idx >= 0 && idx < globals.num_edicts);
	return idx;
}

/**
 * @brief Check if the given number could point to an existing entity
 * @note entity must also be 'in use' to be really valid
 * @param[in] num The entity's index in the array of entities
 */
qboolean G_EdictsIsValidNum (const int num)
{
	if (num >= 0 && num < globals.num_edicts)
		return qtrue;
	return qfalse;
}

/**
 * @brief Get an entity by it's number
 * @param[in] num The entity's index in the array of entities
 */
edict_t* G_EdictsGetByNum (const int num)
{
	if (!G_EdictsIsValidNum(num)) {
		gi.DPrintf("Invalid edict num %i\n", num);
		return NULL;
	}

	return g_edicts + num;
}

/**
 * @brief Returns the first entity
 * @note This is always a world edict - but in case of rma there might be several parts
 * of the world spread all over the array.
 */
edict_t* G_EdictsGetFirst (void)
{
	return &g_edicts[0];
}

/**
 * @brief Iterate through the list of entities
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNext (edict_t* lastEnt)
{
	edict_t* endOfEnts = &g_edicts[globals.num_edicts];
	edict_t* ent;

	if (!globals.num_edicts)
		return NULL;

	if (!lastEnt)
		return g_edicts;
	assert(lastEnt >= g_edicts);
	assert(lastEnt < endOfEnts);

	ent = lastEnt;

	ent++;
	if (ent >= endOfEnts)
		return NULL;
	else
		return ent;
}

edict_t* G_EdictDuplicate (const edict_t *edict)
{
	edict_t *duplicate = G_EdictsGetNewEdict();
	if (duplicate == NULL)
		return NULL;
	memcpy(duplicate, edict, sizeof(*edict));
	duplicate->number = G_EdictsGetNumber(duplicate);
	return duplicate;
}

/**
 * @brief Find an entity that is not in use
 */
edict_t* G_EdictsGetNewEdict (void)
{
	edict_t* ent = NULL;

	/* try to recycle an edict */
	while ((ent = G_EdictsGetNext(ent))) {
		if (!ent->inuse)
			return ent;
	}

	/* no unused edict found, create a new one */
	ent = &g_edicts[globals.num_edicts];
	globals.num_edicts++;
	if (globals.num_edicts > game.sv_maxentities)
		return NULL;
	else
		return ent;
}

/**
 * @brief Iterate through the entities that are in use
 * @note we can hopefully get rid of this function once we know when it makes sense
 * to iterate through entities that are NOT in use
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNextInUse (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNext(ent))) {
		if (ent->inuse)
			break;
	}
	return ent;
}

/**
 * @brief Iterator through all the trigger_nextmap edicts
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetTriggerNextMaps (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		if (G_IsTriggerNextMap(ent))
			break;
	}
	return ent;
}

/**
 * @brief Iterate through the living actor entities
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNextLivingActor (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		if (G_IsLivingActor(ent))
			break;
	}
	return ent;
}

/**
 * @brief Iterate through the living actor entities of the given team
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 * @param team The team we are looking for
 */
edict_t* G_EdictsGetNextLivingActorOfTeam (edict_t* lastEnt, const int team)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (ent->team == team)
			break;
	}
	return ent;
}

/**
 * @brief Iterate through the actor entities (even the dead! - but skips the invisible)
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNextActor (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	assert(lastEnt < &g_edicts[globals.num_edicts]);

	while ((ent = G_EdictsGetNextInUse(ent))) {
		if (G_IsActor(ent))
			break;
	}
	return ent;
}

/**
 * @brief Calculate the edict's origin vector from it's grid position
 * @param ent The entity
 */
void G_EdictCalcOrigin (edict_t* ent)
{
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
}

/**
 * @brief Set the edict's pos and origin vector to the given grid position
 * @param ent The entity
 * @param newPos The new grid position
 */
void G_EdictSetOrigin (edict_t* ent, const pos3_t newPos)
{
	VectorCopy(newPos, ent->pos);
	gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
}

/**
 * @brief Set the edict's pos and origin vector to the given grid position
 * @param ent The entity
 * @param cmpPos The grid position to compare to
 * @return qtrue if positions are equal
 */
qboolean G_EdictPosIsSameAs (edict_t* ent, const pos3_t cmpPos)
{
	return VectorCompare(cmpPos, ent->pos);
}
