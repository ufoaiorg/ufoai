/**
 * @file
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "q_shared.h"

typedef struct inventoryImport_s {
	void (*Free) (void* data);

	void (*FreeAll) (void);

	void* (*Alloc) (size_t size);
} inventoryImport_t;

class InventoryInterface
{
	const inventoryImport_t* import;
	Item* _invList;	/* @todo figure out WTF this is good for (Duke, 11.3.2013) */
	Item cacheItem;
	const csi_t* csi;
	const char* invName;

public:
	InventoryInterface ();

	void initInventory (const char* name, const csi_t* csi, const inventoryImport_t* import);
	bool removeFromInventory (Inventory* const inv, const invDef_t* container, Item* fItem) __attribute__((warn_unused_result));
	Item* addToInventory (Inventory* const inv, const Item* const item, const invDef_t* container, int x, int y,
			int amount) __attribute__((warn_unused_result));

	inventory_action_t moveInInventory (Inventory* const inv, const invDef_t* from, Item* item, const invDef_t* to,
			int tx, int ty, int* TU, Item**  icp);

	bool tryAddToInventory (Inventory* const inv, const Item* const item, const invDef_t* container);
	void destroyInventory (Inventory* const inv);
	void destroyInventoryInterface(void);
	void emptyContainer (Inventory* const inv, const containerIndex_t container);

	void EquipActor (character_t* const chr, const equipDef_t* ed, const objDef_t* weapon, int maxWeight);

	void EquipActorNormal (character_t* const chr, const equipDef_t* ed, int maxWeight);

	void EquipActorMelee (Inventory* const inv, const teamDef_t* td);

	void EquipActorRobot (Inventory* const inv, const objDef_t* weapon);

	int GetUsedSlots ();

protected:
	inline void* alloc (size_t size)
	{
		return import->Alloc(size);
	}

	inline void free (void* data)
	{
		import->Free(data);
	}
	void removeInvList (Item* invList);
	Item* addInvList (Inventory* const inv, const invDef_t* container);
	float GetInventoryState (const Inventory* inventory, int& slowestFd);
	int PackAmmoAndWeapon (character_t* const chr, const objDef_t* weapon, int missedPrimary, const equipDef_t* ed, int maxWeight);
};
