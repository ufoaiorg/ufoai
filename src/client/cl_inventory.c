/**
 * @file cl_inventory.c
 * @brief General actor related inventory function for are used in every game mode
 * @note Inventory functions prefix: INV_
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

#include "client.h"
#include "cl_inventory.h"
#include "cl_inventory_callbacks.h"
#include "../shared/parse.h"

/**
 * Collecting items functions.
 */

equipDef_t *INV_GetEquipmentDefinitionByID (const char *name)
{
	int i;

	for (i = 0, csi.eds; i < csi.numEDs; i++)
		if (!strcmp(name, csi.eds[i].name))
			return &csi.eds[i];

	Com_DPrintf(DEBUG_CLIENT, "INV_GetEquipmentDefinitionByID: equipment %s not found.\n", name);
	return NULL;
}

/**
 * @brief Move item between containers.
 * @param[in] inv Pointer to the list where the item is currently in.
 * @param[in] toContainer Pointer to target container, to place the item in.
 * @param[in] px target x position in the toContainer container.
 * @param[in] py target y position in the toContainer container.
 * @param[in] fromContainer Pointer to source container, the item is in.
 * @param[in] fItem Pointer to item being moved.
 * @note If you set px or py to -1/NONE the item is automatically placed on
 * @note a free spot in the targetContainer
 * @return qtrue if the move was successful.
 */
qboolean INV_MoveItem (inventory_t* inv, const invDef_t * toContainer, int px, int py,
	const invDef_t * fromContainer, invList_t *fItem)
{
	int moved;

	if (px >= SHAPE_BIG_MAX_WIDTH || py >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if (!fItem)
		return qfalse;

	/* move the item */
	moved = Com_MoveInInventory(inv, fromContainer, fItem, toContainer, px, py, NULL, NULL);

	switch (moved) {
	case IA_MOVE:
	case IA_ARMOUR:
		return qtrue;
	default:
		return qfalse;
	}
}

/**
 * @brief Load a weapon with ammo.
 * @param[in] weapon Pointer (invList_t) to weapon to load.
 * @param[in] inv Pointer (inventory_t) to inventory where the change happen.
 * @param[in] srcContainer Pointer (invDef_t) to inventorydef where to search ammo.
 * @param[in] destContainer Pointer (invDef_t) to inventorydef where the weapon is.
 */
void INV_LoadWeapon (invList_t *weapon, inventory_t *inv, const invDef_t *srcContainer, const invDef_t *destContainer)
{
	int equipType;
	invList_t *ic = NULL;
	int i = 0;

	assert(weapon);

	equipType = INV_GetFilterFromItem(weapon->item.t);
	/* search an ammo */
	while (i < weapon->item.t->numAmmos && !ic) {
		ic = Com_SearchInInventoryWithFilter(inv, srcContainer, NONE, NONE, weapon->item.t->ammos[i], equipType);
		i++;
	}
	if (ic)
		INV_MoveItem(inv, destContainer, weapon->x, weapon->y, srcContainer, ic);
}

/**
 * @brief Unload a weapon and put the ammo in a container.
 * @param[in] weapon Pointer (invList_t) to weapon to unload ammo.
 * @param[in] inv Pointer (inventory_t) to inventory where the change happen.
 * @param[in] container Pointer (invDef_t) to inventorydef where to put the removed ammo.
 */
void INV_UnloadWeapon (invList_t *weapon, inventory_t *inv, const invDef_t *container)
{
	assert(weapon);
	if (container && inv) {
		const item_t item = {NONE_AMMO, NULL, weapon->item.m, 0, 0};
		Com_AddToInventory(inv, item, container, -1, -1, 1);
	}
	weapon->item.m = NULL;
	weapon->item.a = 0;
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions.
 * @note called with debug_listinventory
 */
static void INV_InventoryList_f (void)
{
	int i;

	for (i = 0; i < csi.numODs; i++)
		INVSH_PrintItemDescription(&csi.ods[i]);
}
#endif

/**
 * @brief Make sure equipment definitions used to generate teams are proper.
 * @note Check that the sum of all proabilities is smaller or equal to 100 for a weapon type.
 * @sa INVSH_EquipActor
 */
qboolean INV_EquipmentDefSanityCheck (void)
{
	int i, j;
	int sum;
	qboolean result = qtrue;

	for (i = 0; i < csi.numEDs; i++) {
		const equipDef_t const *ed = &csi.eds[i];
		/* only check definitions used for generating teams */
		if (strncmp(ed->name, "alien", 5) && strncmp(ed->name, "phalanx", 7))
			continue;

		/* Check primary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (obj->weapon && obj->fireTwoHanded
			 && (INV_ItemMatchesFilter(obj, FILTER_S_PRIMARY) || INV_ItemMatchesFilter(obj, FILTER_S_HEAVY)))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for primary weapons greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Check secondary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (obj->weapon && obj->reload && !obj->deplete && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for secondary weapons greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Check armour */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t const *obj = &csi.ods[j];
			if (INV_ItemMatchesFilter(obj, FILTER_S_ARMOUR))
				sum += ed->num[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for armours greater than 100\n", ed->name);
			result = qfalse;
		}

		/* Don't check misc: the total probability can be greater than 100 */
	}

	return result;
}

void INV_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listinventory", INV_InventoryList_f, "Print the current inventory to the game console");
#endif
	INV_InitCallbacks();
}

