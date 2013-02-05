/**
 * @file
 */

/*
All original material Copyright (C) 2002-2013 UFO: Alien Invasion.

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

	void *(*Alloc) (size_t size);
} inventoryImport_t;

class InventoryInterface
{
	const inventoryImport_t *import;
	invList_t* invList;
	item_t cacheItem;
	const csi_t* csi;
	const char *invName;

public:
	void initInventory (const char *name, const csi_t* csi, const inventoryImport_t *import);
	bool removeFromInventory (inventory_t* const i, const invDef_t *container, invList_t *fItem) __attribute__((warn_unused_result));

	invList_t* AddToInventory (inventory_t *const i, const item_t* const item, const invDef_t *container, int x, int y,
			int amount) __attribute__((warn_unused_result));

	inventory_action_t MoveInInventory (inventory_t* const i, const invDef_t *from, invList_t *item, const invDef_t *to,
			int tx, int ty, int *TU, invList_t ** icp);

	bool TryAddToInventory (inventory_t* const inv, const item_t *const item, const invDef_t *container);

	void DestroyInventory (inventory_t* const i);

	void DestroyInventoryInterface(void);

	void EmptyContainer (inventory_t* const i, const invDef_t *container);

	void EquipActor (character_t* const chr, const equipDef_t *ed, int maxWeight);

	void EquipActorMelee (inventory_t* const inv, const teamDef_t* td);

	void EquipActorRobot (inventory_t* const inv, const objDef_t* weapon);

	int GetUsedSlots ();

protected:
	inline void *alloc (size_t size)
	{
		return import->Alloc(size);
	}

	inline void free (void *data)
	{
		import->Free(data);
	}
	void removeInvList (invList_t *invList);
	invList_t* addInvList (invList_t **invList);
	float GetInventoryState (const inventory_t *inventory, int &slowestFd);
	int PackAmmoAndWeapon (character_t* const chr, const objDef_t* weapon, int missedPrimary, const equipDef_t *ed, int maxWeight);
};
