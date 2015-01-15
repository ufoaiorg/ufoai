/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "g_inventory.h"
#include "g_client.h"
#include "g_spawn.h"
#include "g_utils.h"
#include "g_vis.h"

const equipDef_t* G_GetEquipDefByID (const char* equipID)
{
	const equipDef_t* ed = gi.csi->eds;

	for (int i = 0;  i < gi.csi->numEDs; i++, ed++)
		if (Q_streq(equipID, ed->id))
			return ed;

	gi.DPrintf("Could not find the equipment with the id: '%s'\n", equipID);
	return nullptr;
}

/**
 * @brief Callback to G_GetEdictFromPos() for given position, used to get items from position.
 * @param[in] pos A position for which items are wanted.
 * @sa G_GetFloorItems
 */
Edict* G_GetFloorItemFromPos (const pos3_t pos)
{
	return G_GetEdictFromPos(pos, ET_ITEM);
}

/**
 * @brief Prepares a list of items on the floor at given entity position.
 * @param[in] ent Pointer to an entity being an actor.
 * @return pointer to Edict being a floor (with items) or @c nullptr in case no items were found
 * on the edict grid position.
 */
Edict* G_GetFloorItems (Edict* ent)
{
	Edict* floor = G_GetFloorItemFromPos(ent->pos);
	/* found items */
	if (floor) {
		ent->setFloor(floor);
		return floor;
	}

	/* no items on ground found */
	ent->resetFloor();
	return nullptr;
}

/**
 * @brief Removes one particular item from a given container
 * @param itemID The id of the item to remove
 * @param ent The edict that holds the inventory to remove the item from
 * @param container The container in the inventory of the edict to remove the searched item from.
 * @return @c true if the removal was successful, @c false otherwise.
 */
bool G_InventoryRemoveItemByID (const char* itemID, Edict* ent, containerIndex_t container)
{
	Item* ic = ent->getContainer(container);
	while (ic) {
		const objDef_t* item = ic->def();
		if (item != nullptr && Q_streq(item->id, itemID)) {
			/* remove the virtual item to update the inventory lists */
			if (!game.invi.removeFromInventory(&ent->chr.inv, INVDEF(container), ic))
				gi.Error("Could not remove item '%s' from inventory %i",
						ic->def()->id, container);
			G_EventInventoryDelete(*ent, G_VisToPM(ent->visflags), container, ic->getX(), ic->getY());
			return true;
		}
		ic = ic->getNext();
	}

	return false;
}

/**
 * @brief Checks whether the given container contains items that should be
 * dropped to the floor. Also removes virtual items.
 * @param[in,out] ent The entity to check the inventory containers for. The inventory of
 * this edict is modified in this function (the virtual items are removed).
 * @param[in] container The container of the entity inventory to check
 * @return @c true if there are items that should be dropped to floor, @c false otherwise
 */
static bool G_InventoryDropToFloorCheck (Edict* ent, containerIndex_t container)
{
	if (container == CID_ARMOUR || container == CID_IMPLANT)
		return false;

	Item* ic = ent->getContainer(container);
	if (!ic)
		return false;

	bool check = false;
	while (ic) {
		assert(ic->def());
		if (ic->def()->isVirtual) {
			Item* next = ic->getNext();
			/* remove the virtual item to update the inventory lists */
			if (!game.invi.removeFromInventory(&ent->chr.inv, INVDEF(container), ic))
				gi.Error("Could not remove virtual item '%s' from inventory %i",
						ic->def()->id, container);
			ic = next;
		} else {
			/* there are none virtual items left that should be send to the client */
			check = true;
			ic = ic->getNext();
		}
	}
	return check;
}

/**
 * @brief Adds a new item to an existing or new floor container edict at the given grid location
 * @param pos The grid location to spawn the item on the floor
 * @param itemID The item to spawn
 */
bool G_AddItemToFloor (const pos3_t pos, const char* itemID)
{
	const objDef_t* od = INVSH_GetItemByIDSilent(itemID);
	if (!od) {
		gi.DPrintf("Could not find item '%s'\n", itemID);
		return false;
	}

	/* Also sets FLOOR(ent) to correct value. */
	Edict* floor = G_GetFloorItemFromPos(pos);
	/* nothing on the ground yet? */
	if (!floor)
		floor = G_SpawnFloor(pos);

	Item item(od);
	return game.invi.tryAddToInventory(&floor->chr.inv, &item, INVDEF(CID_FLOOR));
}

/** @brief Move items to adjacent locations if the containers on the current
 * floor edict are full */
/* #define ADJACENT */

#ifdef ADJACENT
static bool G_InventoryPlaceItemAdjacent (Edict* ent)
{
	vec2_t oldPos; /* if we have to place it to adjacent  */
	int i;

	Vector2Copy(ent->pos, oldPos);
	Edict* floorAdjacent = nullptr;

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
			G_EventPerish(*floorAdjacent);
			G_VisFlagsReset(*floorAdjacent);
		}

		Item* ic = nullptr;
		int x, y;
		floorAdjacent->chr.inv.findSpace(INVDEF(CID_FLOOR), ic, &x, &y, ic);
		if (x != NONE) {
			ic->setX(x);
			ic->setY(y);
			ic->setNext(floorAdjacent->getFloor());
			floorAdjacent->chr.inv.setFloorContainer(ic);
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
		return false;
	}

	if (floorAdjacent)
		G_CheckVis(floorAdjacent, true);

	return true;
}
#endif

/**
 * @brief Move the whole given inventory to the floor and destroy the items that do not fit there.
 * @param[in] ent Pointer to an Edict being an actor.
 * @sa G_ActorDie
 */
void G_InventoryToFloor (Edict* ent)
{
	/* check for items that should be dropped */
	/* ignore items linked from any temp container */
	const Container* cont = nullptr;
	while ((cont = ent->chr.inv.getNextCont(cont))) {
		if (G_InventoryDropToFloorCheck(ent, cont->id))
			break;	/* found at least one item */
	}

	/* edict is not carrying any items */
	if (!cont)
		return;

	/* find the floor */
	Edict* floor = G_GetFloorItems(ent);
	if (!floor) {
		floor = G_SpawnFloor(ent->pos);
	} else {
		/* destroy this edict (send this event to all clients that see the edict) */
		G_EventPerish(*floor);
		G_VisFlagsReset(*floor);
	}

	/* drop items */
	/* cycle through all containers */
	for (containerIndex_t container = 0; container < CID_MAX; container++) {
		/* skip floor - we want to drop to floor */
		if (container == CID_FLOOR)
			continue;

		/* skip CID_ARMOUR, we will collect armours using armour container,
		 * not CID_FLOOR */
		if (container == CID_ARMOUR || container == CID_IMPLANT)
			continue;

		/* now cycle through all items for the container of the character (or the entity) */
		Item* ic, *next;
		for (ic = ent->getContainer(container); ic; ic = next) {
			/* Save the next inv-list before it gets overwritten below.
			 * Do not put this in the "for" statement,
			 * unless you want an endless loop. ;) */
			next = ic->getNext();
			Item item = *ic;

			/* only floor can summarize, so everything on the actor must have amount=1 */
			assert(item.getAmount() == 1);
			if (!game.invi.removeFromInventory(&ent->chr.inv, INVDEF(container), ic))
				gi.Error("Could not remove item '%s' from inventory %i of entity %i",
						ic->def()->id, container, ent->getIdNum());
			G_EventInventoryDelete(*ent, G_VisToPM(ent->visflags), container, ic->getX(), ic->getY());
			if (game.invi.addToInventory(&floor->chr.inv, &item, INVDEF(CID_FLOOR), NONE, NONE, 1) == nullptr)
				gi.Error("Could not add item '%s' from inventory %i of entity %i to floor container",
						ic->def()->id, container, ent->getIdNum());
#ifdef ADJACENT
			G_InventoryPlaceItemAdjacent(ent);
#endif
		}
		/* destroy link */
		ent->resetContainer(container);
	}

	ent->setFloor(floor);

	/* send item info to the clients */
	G_CheckVis(floor);
}

/**
 * @brief Read item from the network buffer
 * @param[in,out] item The Item being send through net.
 * @param[in,out] container Container which is being updated with item sent.
 * @param[in] x,y Position of item in given container.
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_ReadItem (Item* item, const invDef_t** container, int* x, int* y)
{
	int t, m;
	int ammoleft;
	int amount;
	containerIndex_t containerID;

	gi.ReadFormat("sbsbbbbs", &t, &ammoleft, &m, &containerID, x, y, &item->rotated, &amount);
	item->setAmmoLeft(ammoleft);
	item->setAmount(amount);

	if (t < 0 || t >= gi.csi->numODs)
		gi.Error("Item index out of bounds: %i", t);
	item->setDef(&gi.csi->ods[t]);

	if (m != NONE) {
		if (m < 0 || m >= gi.csi->numODs)
			gi.Error("Ammo index out of bounds: %i", m);
		item->setAmmoDef(&gi.csi->ods[m]);
	} else {
		item->setAmmoDef(nullptr);
	}

	if (isValidContId(containerID))
		*container = INVDEF(containerID);
	else
		gi.Error("container id is out of bounds: %i", containerID);
}

/**
 * @brief Write an item to the network buffer
 * @param[in,out] item The Item being send through net.
 * @param[in,out] contId Container which is being updated with item sent.
 * @param[in] x,y Position of item in given container.
 * @sa CL_NetReceiveItem
 * @sa EV_INV_TRANSFER
 */
void G_WriteItem (const Item& item, const containerIndex_t contId, int x, int y)
{
	assert(item.def());
	gi.WriteFormat("sbsbbbbs", item.def()->idx, item.getAmmoLeft(), item.ammoDef() ? item.ammoDef()->idx : NONE, contId, x, y, item.rotated, item.getAmount());
}

/**
 * @brief Sends whole inventory through the network buffer.
 * @param[in] playerMask The player mask to determine which clients should receive the event (@c G_VisToPM(ent->visflags)).
 * @param[in] ent Pointer to an actor or floor container with inventory to send.
 * @sa G_AppearPerishEvent
 * @sa CL_InvAdd
 */
void G_SendInventory (playermask_t playerMask, const Edict& ent)
{
	int nr = 0;

	/* test for pointless player mask */
	if (!playerMask)
		return;

	const Container* cont = nullptr;
	while ((cont = ent.chr.inv.getNextCont(cont, true))) {
		if (!G_IsItem(&ent) && INVDEF(cont->id)->temp)
			continue;
		nr += cont->countItems();
	}

	/* return if no inventory items to send */
	if (nr == 0)
		return;

	G_EventInventoryAdd(ent, playerMask, nr);
	cont = nullptr;
	while ((cont = ent.chr.inv.getNextCont(cont, true))) {
		if (!G_IsItem(&ent) && INVDEF(cont->id)->temp)
			continue;
		const Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			/* send a single item */
			assert(item->def());
			G_WriteItem(*item, cont->id, item->getX(), item->getY());
		}
	}
	G_EventEnd();
}
