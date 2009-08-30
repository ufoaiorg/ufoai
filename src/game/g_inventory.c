/**
 * @file g_inventory.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

/**
 * @sa G_GetFloorItems
 */
static edict_t *G_GetFloorItemsFromPos (pos3_t pos)
{
	edict_t *floor;

	for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
		if (!floor->inuse || floor->type != ET_ITEM)
			continue;
		if (!VectorCompare(pos, floor->pos))
			continue;

		return floor;
	}
	/* nothing found at this pos */
	return NULL;
}

/**
 * @brief Prepares a list of items on the floor at given entity position.
 * @param[in] ent Pointer to an entity being an actor.
 * @return pointer to edict_t being a floor (with items) or @c NULL in case no items were found
 * on the edict grid position.
 * @note This function is somehow broken - it returns NULL in some cases of items on the floor.
 */
edict_t *G_GetFloorItems (edict_t * ent)
{
	edict_t *floor = G_GetFloorItemsFromPos(ent->pos);
	/* found items */
	if (floor) {
		FLOOR(ent) = FLOOR(floor);
		return floor;
	}

	/* no items on ground found */
	FLOOR(ent) = NULL;
	return NULL;
}

/**
 * @brief Read item from the network buffer
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_ReadItem (item_t *item, invDef_t **container, int *x, int *y)
{
	int t, m;
	int containerID;

	gi.ReadFormat("sbsbbbb", &t, &item->a, &m, &containerID, x, y, &item->rotated);

	assert(t != NONE);
	item->t = &gi.csi->ods[t];

	item->m = NULL;
	if (m != NONE)
		item->m = &gi.csi->ods[m];

	if (containerID >= 0 && containerID < gi.csi->numIDs)
		*container = INVDEF(containerID);
	else
		*container = NULL;
}

/**
 * @brief Write an item to the network buffer
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_WriteItem (item_t item, const invDef_t *container, int x, int y)
{
	assert(item.t);
	gi.WriteFormat("sbsbbbb", item.t->idx, item.a, item.m ? item.m->idx : NONE, container->id, x, y, item.rotated);
}

/**
 * @sa G_EndGame
 * @sa G_AppearPerishEvent
 * @sa CL_InvAdd
 */
void G_SendInventory (unsigned int playerMask, edict_t *ent)
{
	invList_t *ic;
	unsigned short nr = 0;
	int j;

	/* test for pointless player mask */
	if (!playerMask)
		return;

	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next)
			nr++;

	/* return if no inventory items to send */
	if (nr == 0 && ent->type != ET_ITEM)
		return;

	gi.AddEvent(playerMask, EV_INV_ADD);

	gi.WriteShort(ent->number);

	/* size of inventory */
	gi.WriteShort(nr * INV_INVENTORY_BYTES);
	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next) {
			/* send a single item */
			G_WriteItem(ic->item, INVDEF(j), ic->x, ic->y);
		}
}
