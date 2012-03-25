/**
 * @file g_inventory.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

const equipDef_t *G_GetEquipDefByID (const char *equipID)
{
	int i;
	const equipDef_t *ed;

	for (i = 0, ed = gi.csi->eds; i < gi.csi->numEDs; i++, ed++)
		if (Q_streq(equipID, ed->id))
			return ed;

	gi.DPrintf("Could not find the equipment with the id: '%s'\n", equipID);
	return NULL;
}

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
 * @brief Removes one particular item from a given container
 * @param itemID The id of the item to remove
 * @param ent The edict that holds the inventory to remove the item from
 * @param container The container in the inventory of the edict to remove the searched item from.
 * @return @c true if the removal was successful, @c false otherwise.
 */
qboolean G_InventoryRemoveItemByID (const char *itemID, edict_t *ent, containerIndex_t container)
{
	invList_t *ic = CONTAINER(ent, container);
	while (ic) {
		const objDef_t *item = ic->item.t;
		if (item != NULL && Q_streq(item->id, itemID)) {
			/* remove the virtual item to update the inventory lists */
			if (!game.i.RemoveFromInventory(&game.i, &ent->chr.i, INVDEF(container), ic))
				gi.Error("Could not remove item '%s' from inventory %i",
						ic->item.t->id, container);
			G_EventInventoryDelete(ent, G_VisToPM(ent->visflags), INVDEF(container), ic->x, ic->y);
			return qtrue;
		}
		ic = ic->next;
	}

	return qfalse;
}

/**
 * @brief Checks whether the given container contains items that should be
 * dropped to the floor. Also removes virtual items.
 * @param[in,out] ent The entity to check the inventory containers for. The inventory of
 * this edict is modified in this function (the virtual items are removed).
 * @param[in] container The container of the entity inventory to check
 * @return @c true if there are items that should be dropped to floor, @c false otherwise
 */
static qboolean G_InventoryDropToFloorCheck (edict_t* ent, containerIndex_t container)
{
	invList_t* ic = CONTAINER(ent, container);

	if (container == gi.csi->idArmour)
		return qfalse;

	if (ic) {
		qboolean check = qfalse;
		while (ic) {
			assert(ic->item.t);
			if (ic->item.t->isVirtual) {
				invList_t *next = ic->next;
				/* remove the virtual item to update the inventory lists */
				if (!game.i.RemoveFromInventory(&game.i, &ent->chr.i, INVDEF(container), ic))
					gi.Error("Could not remove virtual item '%s' from inventory %i",
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

/**
 * @brief Adds a new item to an existing or new floor container edict at the given grid location
 * @param pos The grid location to spawn the item on the floor
 * @param itemID The item to spawn
 */
qboolean G_AddItemToFloor (const pos3_t pos, const char *itemID)
{
	edict_t *floor;
	item_t item = {NONE_AMMO, NULL, NULL, 0, 0};
	const objDef_t *od = INVSH_GetItemByIDSilent(itemID);
	if (!od) {
		gi.DPrintf("Could not find item '%s'\n", itemID);
		return qfalse;
	}

	/* Also sets FLOOR(ent) to correct value. */
	floor = G_GetFloorItemsFromPos(pos);
	/* nothing on the ground yet? */
	if (!floor)
		floor = G_SpawnFloor(pos);

	item.t = od;
	return game.i.TryAddToInventory(&game.i, &floor->chr.i, &item, INVDEF(gi.csi->idFloor));
}

/** @brief Move items to adjacent locations if the containers on the current
 * floor edict are full */
/* #define ADJACENT */

#ifdef ADJACENT
static qboolean G_InventoryPlaceItemAdjacent (edict_t *ent)
{
	vec2_t oldPos; /* if we have to place it to adjacent  */
	edict_t *floorAdjacent;
	int i;

	Vector2Copy(ent->pos, oldPos);
	floorAdjacent = NULL;

	for (i = 0; i < DIRECTIONS; i++) {
		/** @todo Check whether movement is possible here - otherwise don't use this field */
		/* extend pos with the direction vectors */
		/** @todo Don't know why the adjacent stuff has been disabled, but if it was buggy, it's probably */
		/** because the third ent->pos in the next line should be pos[1] ?!. (Duke, 13.1.11) */
		Vector2Set(ent->pos, ent->pos[0] + dvecs[i][0], ent->pos[0] + dvecs[i][1]);
		/* now try to get a floor entity for that new location */
		floorAdjacent = G_GetFloorItems(ent);
		if (!floorAdjacent) {
			floorAdjacent = G_SpawnFloor(ent->pos);
		} else {
			/* destroy this edict (send this event to all clients that see the edict) */
			G_EventPerish(floorAdjacent);
			G_VisFlagsReset(floorAdjacent);
		}

		INVSH_FindSpace(&floorAdjacent->i, &ic->item, INVDEF(gi.csi->idFloor), &x, &y, ic);
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
		return qfalse;
	}

	if (floorAdjacent)
		G_CheckVis(floorAdjacent, qtrue);

	return qtrue;
}
#endif

/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 * @param[in] ent Pointer to an edict_t being an actor.
 * @sa G_ActorDie
 */
void G_InventoryToFloor (edict_t *ent)
{
	invList_t *ic, *next;
	containerIndex_t container;
	edict_t *floor;
	item_t item;

	/* check for items */
	for (container = 0; container < gi.csi->numIDs; container++) {
		/* ignore items linked from any temp container */
		if (INVDEF(container)->temp)
			continue;
		if (G_InventoryDropToFloorCheck(ent, container))
			break;
	}

	/* edict is not carrying any items */
	if (container >= gi.csi->numIDs)
		return;

	/* find the floor */
	floor = G_GetFloorItems(ent);
	if (!floor) {
		floor = G_SpawnFloor(ent->pos);
	} else {
		/* destroy this edict (send this event to all clients that see the edict) */
		G_EventPerish(floor);
		G_VisFlagsReset(floor);
	}

	/* drop items */
	/* cycle through all containers */
	for (container = 0; container < gi.csi->numIDs; container++) {
		/* skip floor - we want to drop to floor */
		if (container == gi.csi->idFloor)
			continue;

		/* skip csi->idArmour, we will collect armours using idArmour container,
		 * not idFloor */
		if (container == gi.csi->idArmour)
			continue;

		/* now cycle through all items for the container of the character (or the entity) */
		for (ic = CONTAINER(ent, container); ic; ic = next) {
			/* Save the next inv-list before it gets overwritten below.
			 * Do not put this in the "for" statement,
			 * unless you want an endless loop. ;) */
			next = ic->next;
			item = ic->item;

			/* only floor can summarize, so everything on the actor must have amount=1 */
			assert(item.amount == 1);
			if (!game.i.RemoveFromInventory(&game.i, &ent->chr.i, INVDEF(container), ic))
				gi.Error("Could not remove item '%s' from inventory %i of entity %i",
						ic->item.t->id, container, ent->number);
			if (game.i.AddToInventory(&game.i, &floor->chr.i, &item, INVDEF(gi.csi->idFloor), NONE, NONE, 1) == NULL)
				gi.Error("Could not add item '%s' from inventory %i of entity %i to floor container",
						ic->item.t->id, container, ent->number);
#ifdef ADJACENT
			G_InventoryPlaceItemAdjacent(ent);
#endif
		}
		/* destroy link */
		CONTAINER(ent, container) = NULL;
	}

	FLOOR(ent) = FLOOR(floor);

	/* send item info to the clients */
	G_CheckVis(floor, qtrue);
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
void G_ReadItem (item_t *item, const invDef_t **container, int *x, int *y)
{
	int t, m;
	containerIndex_t containerID;

	gi.ReadFormat("sbsbbbbs", &t, &item->a, &m, &containerID, x, y, &item->rotated, &item->amount);

	if (t < 0 || t >= gi.csi->numODs)
		gi.Error("Item index out of bounds: %i", t);
	item->t = &gi.csi->ods[t];

	if (m != NONE) {
		if (m < 0 || m >= gi.csi->numODs)
			gi.Error("Ammo index out of bounds: %i", m);
		item->m = &gi.csi->ods[m];
	} else {
		item->m = NULL;
	}

	if (containerID >= 0 && containerID < gi.csi->numIDs)
		*container = INVDEF(containerID);
	else
		gi.Error("container id is out of bounds: %i", containerID);
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
void G_WriteItem (const item_t *item, const invDef_t *container, int x, int y)
{
	assert(item);
	assert(item->t);
	gi.WriteFormat("sbsbbbbs", item->t->idx, item->a, item->m ? item->m->idx : NONE, container->id, x, y, item->rotated, item->amount);
}

/**
 * @brief Sends whole inventory through the network buffer.
 * @param[in] playerMask The player mask to determine which clients should receive the event (@c G_VisToPM(ent->visflags)).
 * @param[in] ent Pointer to an actor or floor container with inventory to send.
 * @sa G_AppearPerishEvent
 * @sa CL_InvAdd
 */
void G_SendInventory (unsigned int playerMask, const edict_t *ent)
{
	invList_t *ic;
	int nr = 0;
	containerIndex_t container;

	/* test for pointless player mask */
	if (!playerMask)
		return;

	for (container = 0; container < gi.csi->numIDs; container++) {
		if (!G_IsItem(ent) && INVDEF(container)->temp)
			continue;
		for (ic = CONTAINER(ent, container); ic; ic = ic->next)
			nr++;
	}

	/* return if no inventory items to send */
	if (nr == 0)
		return;

	G_EventInventoryAdd(ent, playerMask, nr);
	for (container = 0; container < gi.csi->numIDs; container++) {
		if (!G_IsItem(ent) && INVDEF(container)->temp)
			continue;
		for (ic = CONTAINER(ent, container); ic; ic = ic->next) {
			/* send a single item */
			assert(ic->item.t);
			G_WriteItem(&ic->item, INVDEF(container), ic->x, ic->y);
		}
	}
	gi.EndEvents();
}
