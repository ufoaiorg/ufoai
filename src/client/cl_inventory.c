/**
 * @file cl_inventory.c
 * @brief General actor related inventory function for are used in every game mode
 * @note Inventory functions prefix: INV_
 * @todo Remove those functions that doesn't really belong here
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
#include "cl_game.h"
#include "../shared/parse.h"

/**
 * Collecting items functions.
 */

equipDef_t *INV_GetEquipmentDefinitionByID (const char *name)
{
	int i;

	for (i = 0, csi.eds; i < csi.numEDs; i++)
		if (!Q_strcmp(name, csi.eds[i].name))
			return &csi.eds[i];

	Com_DPrintf(DEBUG_CLIENT, "INV_GetEquipmentDefinitionByID: equipment %s not found.\n", name);
	return NULL;
}

/**
 * @brief Check if an item is stored in storage.
 * @param[in] obj Pointer to the item to check.
 * @return True if item is stored in storage.
 */
qboolean INV_ItemsIsStoredInStorage (const objDef_t *obj)
{
	/* antimatter is stored in antimatter storage */
	if (!Q_strcmp(obj->id, "antimatter"))
		return qfalse;

	/* aircraft are stored in hangars */
	assert(obj->tech);
	if (obj->tech->type == RS_CRAFT)
		return qfalse;

	return qtrue;
}

/**
 * @param[in] inv list where the item is currently in
 * @param[in] toContainer target container to place the item in
 * @param[in] px target x position in the toContainer container
 * @param[in] py target y position in the toContainer container
 * @param[in] fromContainer Container the item is in
 * @param[in] fromX X position of the item to move (in container fromContainer)
 * @param[in] fromY y position of the item to move (in container fromContainer)
 * @note If you set px or py to -1/NONE the item is automatically placed on a free
 * spot in the targetContainer
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

	/** @todo this case should be removed as soon as right clicking in equip container
	 * will try to put the item in a reasonable container automatically */
	if ((px == -1 || py == -1) && toContainer == fromContainer)
		return qtrue;

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
 * @brief Make sure values of items after parsing are proper.
 */
qboolean INV_ItemsSanityCheck (void)
{
	int i;
	qboolean result = qtrue;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];

		/* Warn if item has no size set. */
		if (item->size <= 0 && !(item->notOnMarket && INV_ItemMatchesFilter(item, FILTER_DUMMY))) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has zero size set.\n", item->id);
		}

		/* Warn if no price is set. */
		if (item->price <= 0 && !item->notOnMarket) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has zero price set.\n", item->id);
		}

		/** @todo production is campaign mode only - shouldn't be here */
		if (item->price > 0 && item->notOnMarket && !PR_ItemIsProduceable(item)) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has a price set though it is neither available on the market and production.\n", item->id);
		}

		/* extension and headgear are mutual exclusive */
		if (item->extension && item->headgear) {
			result = qfalse;
			Com_Printf("INV_ItemsSanityCheck: Item %s has both extension and headgear set.\n",  item->id);
		}
	}

	return result;
}

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
		if (Q_strncmp(ed->name, "alien", 5) && Q_strncmp(ed->name, "phalanx", 7))
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
}
