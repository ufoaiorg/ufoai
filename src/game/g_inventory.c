/**
 * @file g_inventory.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
 * @brief Callback to G_GetEdictFromPos() for given position, used to get items from position.
 * @param[in] pos A position for which items are wanted.
 * @sa G_GetFloorItems
 */
edict_t *G_GetFloorItemsFromPos (const pos3_t pos)
{
	return G_GetEdictFromPos(pos, ET_ITEM);
}

/**
 * @brief Prepares a list of items on the floor at given entity position.
 * @param[in] ent Pointer to an entity being an actor.
 * @return pointer to edict_t being a floor (with items) or @c NULL in case no items were found
 * on the edict grid position.
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
 * @brief Checks whether the given container contains items that should be
 * dropped to the floor
 * @param[in,out] ent The entity to check the inventory containers for
 * @param[in] container The container of the entity inventory to check
 * @return @c true if there are items that should be dropped to floor, @c false otherwise
 */
static qboolean G_InventoryDropToFloorCheck (edict_t* ent, int container)
{
	invList_t* ic = ent->i.c[container];

	if (container == gi.csi->idArmour)
		return qfalse;

	if (ic) {
		qboolean check = qfalse;
		while (ic) {
			assert(ic->item.t);
			if (ic->item.t->virtual) {
				invList_t *next = ic->next;
				/* remove the virtual item to update the inventory lists */
				if (!game.i.RemoveFromInventory(&game.i, &ent->i, INVDEF(container), ic))
					gi.error("Could not remove virtual item '%s' from inventory %i",
							ic->item.t->id, container);
				ic = next;
			} else {
				/* there are none virtual items left that should be send to the client */
				check = qtrue;
				ic = ic->next;
			}
		}
		return check;
	}

	return qfalse;
}

/** @brief Move items to adjacent locations if the containers on the current
 * floor edict are full */
/* #define ADJACENT */

/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 * @param[in] ent Pointer to an edict_t being an actor.
 * @sa G_ActorDie
 */
void G_InventoryToFloor (edict_t *ent)
{
	invList_t *ic, *next;
	int k;
	edict_t *floor;
	item_t item;
#ifdef ADJACENT
	edict_t *floorAdjacent;
	int i;
#endif

	/* check for items */
	for (k = 0; k < gi.csi->numIDs; k++)
		if (G_InventoryDropToFloorCheck(ent, k))
			break;

	/* edict is not carrying any items */
	if (k >= gi.csi->numIDs)
		return;

	/* find the floor */
	floor = G_GetFloorItems(ent);
	if (!floor) {
		floor = G_SpawnFloor(ent->pos);
	} else {
		/* destroy this edict (send this event to all clients that see the edict) */
		G_EventPerish(floor);
		floor->visflags = 0;
	}

	/* drop items */
	/* cycle through all containers */
	for (k = 0; k < gi.csi->numIDs; k++) {
		/* skip floor - we want to drop to floor */
		if (k == gi.csi->idFloor)
			continue;

		/* skip csi->idArmour, we will collect armours using idArmour container,
		 * not idFloor */
		if (k == gi.csi->idArmour)
			continue;

		/* now cycle through all items for the container of the character (or the entity) */
		for (ic = ent->i.c[k]; ic; ic = next) {
#ifdef ADJACENT
			vec2_t oldPos; /* if we have to place it to adjacent  */
#endif
			/* Save the next inv-list before it gets overwritten below.
			 * Do not put this in the "for" statement,
			 * unless you want an endless loop. ;) */
			next = ic->next;
			item = ic->item;

			/* only floor can summarize, so everything on the actor must have amount=1 */
			assert(item.amount == 1);
			if (!game.i.RemoveFromInventory(&game.i, &ent->i, INVDEF(k), ic))
				gi.error("Could not remove item '%s' from inventory %i of entity %i",
						ic->item.t->id, k, ent->number);
			if (game.i.AddToInventory(&game.i, &floor->i, item, INVDEF(gi.csi->idFloor), NONE, NONE, 1) == NULL)
				gi.error("Could not add item '%s' from inventory %i of entity %i to floor container",
						ic->item.t->id, k, ent->number);
#ifdef ADJACENT
				Vector2Copy(ent->pos, oldPos);
				for (i = 0; i < DIRECTIONS; i++) {
					/** @todo Check whether movement is possible here - otherwise don't use this field */
					/* extend pos with the direction vectors */
					Vector2Set(ent->pos, ent->pos[0] + dvecs[i][0], ent->pos[0] + dvecs[i][1]);
					/* now try to get a floor entity for that new location */
					floorAdjacent = G_GetFloorItems(ent);
					if (!floorAdjacent) {
						floorAdjacent = G_SpawnFloor(ent->pos);
					} else {
						/* destroy this edict (send this event to all clients that see the edict) */
						G_EventPerish(floorAdjacent);
						floorAdjacent->visflags = 0;
					}

					INVSH_FindSpace(&floorAdjacent->i, &ic->item, INVDEF(gi.csi->idFloor], &x, &y, ic);
					if (x != NONE) {
						ic->x = x;
						ic->y = y;
						ic->next = FLOOR(floorAdjacent);
						FLOOR(floorAdjacent) = ic;
						break;
					}
					/* restore original pos */
					Vector2Copy(oldPos, ent->pos);
				}
				/* added to adjacent pos? */
				if (i < DIRECTIONS) {
					/* restore original pos - if no free space, this was done
					 * already in the for loop */
					Vector2Copy(oldPos, ent->pos);
					continue;
				}
#endif
		}
		/* destroy link */
		ent->i.c[k] = NULL;
	}

	FLOOR(ent) = FLOOR(floor);

	/* send item info to the clients */
	G_CheckVis(floor, qtrue);
#ifdef ADJACENT
	if (floorAdjacent)
		G_CheckVis(floorAdjacent, qtrue);
#endif
}

/**
 * @brief Read item from the network buffer
 * @param[in,out] item @c item_t being send through net.
 * @param[in,out] container Container which is being updated with item sent.
 * @param[in] x Position of item in given container.
 * @param[in] y Position of item in given container.
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_ReadItem (item_t *item, invDef_t **container, int *x, int *y)
{
	int t, m;
	int containerID;

	gi.ReadFormat("sbsbbbbs", &t, &item->a, &m, &containerID, x, y, &item->rotated, &item->amount);

	if (t < 0 || t >= gi.csi->numODs)
		gi.error("Item index out of bounds: %i", t);
	item->t = &gi.csi->ods[t];

	item->m = NULL;
	if (m != NONE) {
		if (m < 0 || m >= gi.csi->numODs)
			gi.error("Ammo index out of bounds: %i", m);
		item->m = &gi.csi->ods[m];
	}

	if (containerID >= 0 && containerID < gi.csi->numIDs)
		*container = INVDEF(containerID);
	else
		*container = NULL;
}

/**
 * @brief Write an item to the network buffer
 * @param[in,out] item @c item_t being send through net.
 * @param[in,out] container Container which is being updated with item sent.
 * @param[in] x Position of item in given container.
 * @param[in] y Position of item in given container.
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_WriteItem (item_t item, const invDef_t *container, int x, int y)
{
	assert(item.t);
	gi.WriteFormat("sbsbbbbs", item.t->idx, item.a, item.m ? item.m->idx : NONE, container->id, x, y, item.rotated, item.amount);
}

/**
 * @brief Sends whole inventory through the network buffer.
 * @param[in] playerMask The player mask to determine which clients should receive the event (@c G_VisToPM(ent->visflags)).
 * @param[in] ent Pointer to an actor or floor container with inventory to send.
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
	if (nr == 0)
		return;

	G_EventInventoryAdd(ent, playerMask, nr);
	for (j = 0; j < gi.csi->numIDs; j++)
		for (ic = ent->i.c[j]; ic; ic = ic->next) {
			/* send a single item */
			assert(ic->item.t);
			G_WriteItem(ic->item, INVDEF(j), ic->x, ic->y);
		}
}
