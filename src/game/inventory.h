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

class InventoryInterface;
typedef InventoryInterface inventoryInterface_t;
class InventoryInterface
{
public:
	/* private */
	const inventoryImport_t *import;

	invList_t* invList;

	item_t cacheItem;

	const csi_t* csi;

	const char *name;
	/* public */
	bool RemoveFromInventory ( inventoryInterface_t* self, inventory_t* const i, const invDef_t *container, invList_t *fItem) __attribute__((nonnull(1), nonnull(2), warn_unused_result));

	invList_t* AddToInventory (inventoryInterface_t* self, inventory_t *const i, const item_t* const item, const invDef_t *container, int x, int y,
			int amount) __attribute__((nonnull(1), nonnull(2), warn_unused_result));

	inventory_action_t MoveInInventory (inventoryInterface_t* self, inventory_t* const i, const invDef_t *from, invList_t *item, const invDef_t *to,
			int tx, int ty, int *TU, invList_t ** icp) __attribute__((nonnull(1), nonnull(2)));

	bool TryAddToInventory (inventoryInterface_t* self, inventory_t* const inv, const item_t *const item, const invDef_t *container);

	void DestroyInventory (inventoryInterface_t* self, inventory_t* const i) __attribute__((nonnull(1), nonnull(2)));

	void EmptyContainer (inventoryInterface_t* self, inventory_t* const i, const invDef_t *container)__attribute__((nonnull(1), nonnull(2)));

	void EquipActor (inventoryInterface_t* self, character_t* const chr, const equipDef_t *ed, int maxWeight) __attribute__((nonnull(1), nonnull(2)));

	void EquipActorMelee (inventoryInterface_t* self, inventory_t* const inv, const teamDef_t* td) __attribute__((nonnull(1)));

	void EquipActorRobot (inventoryInterface_t* self, inventory_t* const inv, const objDef_t* weapon) __attribute__((nonnull(1), nonnull(2)));

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
	int PackAmmoAndWeapon (inventoryInterface_t *self, character_t* const chr, const objDef_t* weapon, int missedPrimary, const equipDef_t *ed, int maxWeight);
};

void INV_InitInventory(const char *name, inventoryInterface_t *ii, const csi_t* csi, const inventoryImport_t *iimport);
void INV_DestroyInventory(inventoryInterface_t *ii);
