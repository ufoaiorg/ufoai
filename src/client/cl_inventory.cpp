/**
 * @file
 * @brief General actor related inventory function for are used in every game mode
 * @note Inventory functions prefix: INV_
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

#include "client.h"
#include "cl_inventory.h"
#include "cl_inventory_callbacks.h"
#include "../shared/parse.h"
#include "ui/ui_popup.h"
#include "cgame/cl_game.h"

/**
 * @brief Gets equipment definition by id.
 * @param[in] name An id taken from scripts.
 * @return Found @c equipDef_t or @c nullptr if no equipment definition found.
 */
const equipDef_t* INV_GetEquipmentDefinitionByID (const char* name)
{
	for (int i = 0; i < csi.numEDs; i++) {
		const equipDef_t* ed = &csi.eds[i];
		if (Q_streq(name, ed->id))
			return ed;
	}

	Com_Error(ERR_DROP, "Could not find equipment %s", name);
}

/**
 * @brief Move item between containers.
 * @param[in] inv Pointer to the inventory we are working on.
 * @param[in] toContainer Pointer to target container, to place the item in.
 * @param[in] toX target x position in the toContainer container.
 * @param[in] toY target y position in the toContainer container.
 * @param[in] fromContainer Pointer to source container, the item is in.
 * @param[in] fItem Pointer to item being moved.
 * @param[out] uponItem The item the moving item is eventually dropped upon.
 * @note If you set toX or toY to -1/NONE the item is automatically placed on
 * @note a free spot in the targetContainer
 * @return true if the move was successful.
 */
bool INV_MoveItem (Inventory* inv, const invDef_t* toContainer, int toX, int toY,
	const invDef_t* fromContainer, Item* fItem, Item** uponItem)
{
	if (toX >= SHAPE_BIG_MAX_WIDTH || toY >= SHAPE_BIG_MAX_HEIGHT)
		return false;

	if (!fItem)
		return false;

	const int maxWeight = GAME_GetChrMaxLoad(GAME_GetSelectedChr());
	if (!inv->canHoldItemWeight(fromContainer->id, toContainer->id, *fItem, maxWeight)) {
		UI_Popup(_("Warning"), _("This soldier can not carry anything else."));
		return false;
	}

	/* move the item */
	const int moved = cls.i.moveInInventory(inv, fromContainer, fItem, toContainer, toX, toY, nullptr, uponItem);

	switch (moved) {
	case IA_MOVE:
	case IA_ARMOUR:
	case IA_RELOAD:
	case IA_RELOAD_SWAP:
		return true;
	default:
		return false;
	}
}

/**
 * @brief Load a weapon with ammo.
 * @param[in] weaponList Pointer to weapon to load.
 * @param[in] inv Pointer to inventory where the change happen.
 * @param[in] srcContainer Pointer to inventorydef where to search ammo.
 * @param[in] destContainer Pointer to inventorydef where the weapon is.
 */
bool INV_LoadWeapon (const Item* weaponList, Inventory* inv, const invDef_t* srcContainer, const invDef_t* destContainer)
{
	assert(weaponList);

	int x, y;
	weaponList->getFirstShapePosition(&x, &y);
	x += weaponList->getX();
	y += weaponList->getY();

	const objDef_t* weapon = weaponList->def();
	if (weapon->weapons[0]) {
		Item* ic = inv->getItemAtPos(destContainer, x, y);
		if (ic) {
			ic->setAmmoLeft(weapon->ammo);
			ic->setAmmoDef(weapon);
		}
	} else if (weapon->isReloadable()) {
		const itemFilterTypes_t equipType = INV_GetFilterFromItem(weapon);
		/* search an ammo */
		for (int i = 0; i < weapon->numAmmos; i++) {
			const objDef_t* ammo = weapon->ammos[i];
			Item* ic = INV_SearchInInventoryWithFilter(inv, srcContainer, ammo, equipType);
			if (ic)
				return INV_MoveItem(inv, destContainer, x, y, srcContainer, ic, nullptr);
		}
	}

	return false;
}

/**
 * @brief Unload a weapon and put the ammo in a container.
 * @param[in,out] weapon The weapon to unload ammo.
 * @param[in,out] inv inventory where the change happen.
 * @param[in] container Inventory definition where to put the removed ammo.
 * @return @c true if the ammo was moved to the container, @c false otherwise
 */
bool INV_UnloadWeapon (Item* weapon, Inventory* inv, const invDef_t* container)
{
	assert(weapon);
	if (container && inv) {
		bool moved = false;
		if (weapon->def() != weapon->ammoDef() && weapon->getAmmoLeft() > 0) {
			const Item item(weapon->ammoDef());
			moved = cls.i.addToInventory(inv, &item, container, NONE, NONE, 1) != nullptr;
		}
		if (moved || weapon->def() == weapon->ammoDef()) {
			weapon->setAmmoDef(nullptr);
			weapon->setAmmoLeft(0);
			return moved;
		}
	}
	return false;
}

#ifdef DEBUG
/**
 * @brief Lists all object definitions.
 * @note called with debug_listinventory
 */
static void INV_InventoryList_f (void)
{
	for (int i = 0; i < csi.numODs; i++) {
		const objDef_t* od = INVSH_GetItemByIDX(i);

		Com_Printf("Item: %s\n", od->id);
		Com_Printf("... name          -> %s\n", od->name);
		Com_Printf("... type          -> %s\n", od->type);
		Com_Printf("... category      -> %i\n", od->animationIndex);
		Com_Printf("... weapon        -> %i\n", od->weapon);
		Com_Printf("... holdtwohanded -> %i\n", od->holdTwoHanded);
		Com_Printf("... firetwohanded -> %i\n", od->fireTwoHanded);
		Com_Printf("... thrown        -> %i\n", od->thrown);
		Com_Printf("... usable for weapon (if type is ammo):\n");
		for (int j = 0; j < od->numWeapons; j++) {
			if (od->weapons[j])
				Com_Printf("    ... %s\n", od->weapons[j]->name);
		}
		Com_Printf("\n");
	}
}
#endif

/**
 * @brief Make sure equipment definitions used to generate teams are proper.
 * @note Check that the sum of all probabilities is smaller or equal to 100 for a weapon type.
 * @sa INVSH_EquipActor
 */
static bool INV_EquipmentDefSanityCheck (void)
{
	int sum;
	bool result = true;

	for (int i = 0; i < csi.numEDs; i++) {
		int j;
		const equipDef_t* const ed = &csi.eds[i];
		/* only check definitions used for generating teams */
		if (!Q_strstart(ed->id, "alien") && !Q_strstart(ed->id, "phalanx"))
			continue;

		/* Check primary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t* const obj = INVSH_GetItemByIDX(j);
			if (obj->weapon && obj->fireTwoHanded
			 && (INV_ItemMatchesFilter(obj, FILTER_S_PRIMARY) || INV_ItemMatchesFilter(obj, FILTER_S_HEAVY)))
				sum += ed->numItems[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for primary weapons greater than 100\n", ed->id);
			result = false;
		}

		/* Check secondary */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t* const obj = INVSH_GetItemByIDX(j);
			if (obj->weapon && obj->isReloadable() && !obj->deplete && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY))
				sum += ed->numItems[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for secondary weapons greater than 100\n", ed->id);
			result = false;
		}

		/* Check armour */
		sum = 0;
		for (j = 0; j < csi.numODs; j++) {
			const objDef_t* const obj = INVSH_GetItemByIDX(j);
			if (INV_ItemMatchesFilter(obj, FILTER_S_ARMOUR))
				sum += ed->numItems[j];
		}
		if (sum > 100) {
			Com_Printf("INV_EquipmentDefSanityCheck: Equipment Def '%s' has a total probability for armours greater than 100\n", ed->id);
			result = false;
		}

		/* Don't check misc: the total probability can be greater than 100 */
	}

	return result;
}

itemFilterTypes_t INV_GetFilterFromItem (const objDef_t* obj)
{
	assert(obj);

	/* heavy weapons may be primary too. check heavy first */
	if (obj->isHeavy)
		return FILTER_S_HEAVY;
	if (obj->implant)
		return FILTER_S_IMPLANT;
	else if (obj->isPrimary)
		return FILTER_S_PRIMARY;
	else if (obj->isSecondary)
		return FILTER_S_SECONDARY;
	else if (obj->isMisc)
		return FILTER_S_MISC;
	else if (obj->isArmour())
		return FILTER_S_ARMOUR;

	/** @todo need to implement everything */
	Sys_Error("INV_GetFilterFromItem: unknown filter category for item '%s'", obj->id);
}

/**
 * @brief Checks if the given object/item matched the given filter type.
 * @param[in] obj A pointer to an objDef_t item.
 * @param[in] filterType Filter type to check against.
 * @return @c true if obj is in filterType
 */
bool INV_ItemMatchesFilter (const objDef_t* obj, const itemFilterTypes_t filterType)
{
	int i;

	if (!obj)
		return false;

	switch (filterType) {
	case FILTER_S_PRIMARY:
		if (obj->isPrimary && !obj->isHeavy)
			return true;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			const objDef_t* weapon = obj->weapons[i];
			if (weapon && weapon != obj && INV_ItemMatchesFilter(weapon, filterType))
				return true;
		}
		break;

	case FILTER_S_SECONDARY:
		if (obj->isSecondary && !obj->isHeavy)
			return true;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			const objDef_t* weapon = obj->weapons[i];
			if (weapon && weapon != obj && INV_ItemMatchesFilter(weapon, filterType))
				return true;
		}
		break;

	case FILTER_S_HEAVY:
		if (obj->isHeavy)
			return true;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			const objDef_t* weapon = obj->weapons[i];
			if (weapon && weapon != obj && INV_ItemMatchesFilter(weapon, filterType))
				return true;
		}
		break;

	case FILTER_S_IMPLANT:
		return obj->implant;

	case FILTER_S_ARMOUR:
		return obj->isArmour();

	case FILTER_S_MISC:
		return obj->isMisc;

	case FILTER_CRAFTITEM:
		/** @todo Should we handle FILTER_AIRCRAFT here as well? */
		return obj->isCraftItem();

	case FILTER_UGVITEM:
		return obj->isUGVitem;

	case FILTER_DUMMY:
		return obj->isDummy;

	case FILTER_AIRCRAFT:
		return Q_streq(obj->type, "aircraft");

	case FILTER_DISASSEMBLY:
		/** @todo I guess we should search for components matching this item here. */
		break;

	case MAX_SOLDIER_FILTERTYPES:
	case MAX_FILTERTYPES:
	case FILTER_ENSURE_32BIT:
		Com_Printf("INV_ItemMatchesFilter: Unknown filter type for items: %i\n", filterType);
		break;
	}

	/* The given filter type is unknown. */
	return false;
}

/**
 * @brief Searches if there is an item at location (x/y) in a scrollable container. You can also provide an item to search for directly (x/y is ignored in that case).
 * @note x = x-th item in a row, y = row. i.e. x/y does not equal the "grid" coordinates as used in those containers.
 * @param[in] inv Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] itemType The item to search. Will ignore "x" and "y" if set, it'll also search invisible items.
 * @param[in] filterType Enum definition of type (types of items for filtering purposes).
 * @return @c Item Pointer to the Item/item that is located at x/y or equals "item".
 * @sa Inventory::getItemAtPos
 */
Item* INV_SearchInInventoryWithFilter (const Inventory* const inv, const invDef_t* container, const objDef_t* itemType,  const itemFilterTypes_t filterType)
{
	Item* ic;

	if (inv == nullptr)
		return nullptr;

	if (itemType == nullptr)
		return nullptr;

	for (ic = inv->getContainer2(container->id); ic; ic = ic->getNext()) {
		/* Search only in the items that could get displayed. */
		if (ic && ic->def() && (filterType == MAX_FILTERTYPES || INV_ItemMatchesFilter(ic->def(), filterType))) {
			/* We search _everything_, no matter what location it is (i.e. x/y are ignored). */
			if (itemType == ic->def())
				return ic;
		}
	}

	/* No item with these coordinates (or matching item) found. */
	return nullptr;
}

/** Names of the filter types as used in console function. e.g. in .ufo files.
 * @sa inv_shared.h:itemFilterTypes_t */
static char const* const filterTypeNames[] = {
	"primary",		/**< FILTER_S_PRIMARY */
	"secondary",	/**< FILTER_S_SECONDARY */
	"heavy",		/**< FILTER_S_HEAVY */
	"misc",			/**< FILTER_S_MISC */
	"armour",		/**< FILTER_S_ARMOUR */
	"implant",		/**< FILTER_S_IMPLANT */
	"",				/**< MAX_SOLDIER_FILTERTYPES */
	"craftitem",	/**< FILTER_CRAFTITEM */
	"ugvitem",		/**< FILTER_UGVITEM */
	"aircraft",		/**< FILTER_AIRCRAFT */
	"dummy",		/**< FILTER_DUMMY */
	"disassembly"	/**< FILTER_DISASSEMBLY */
};
CASSERT(lengthof(filterTypeNames) == MAX_FILTERTYPES);

/**
 * @brief Searches for a filter type name (as used in console functions) and returns the matching itemFilterTypes_t enum.
 * @param[in] filterTypeID Filter type name so search for. @sa filterTypeNames.
 */
itemFilterTypes_t INV_GetFilterTypeID (const char* filterTypeID)
{
	if (!filterTypeID)
		return MAX_FILTERTYPES;

	/* default filter type is primary */
	if (filterTypeID[0] == '\0')
		return FILTER_S_PRIMARY;

	for (int i = 0; i < MAX_FILTERTYPES; i++) {
		const char* fileTypeName = filterTypeNames[i];
		if (fileTypeName && Q_streq(fileTypeName, filterTypeID))
			return (itemFilterTypes_t)i;
	}

	/* No matching filter type found, returning max value. */
	return MAX_FILTERTYPES;
}

/**
 * @param[in] id The filter type index
 */
const char* INV_GetFilterType (itemFilterTypes_t id)
{
	assert(id < MAX_FILTERTYPES);
	return filterTypeNames[id];
}

void INV_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listinventory", INV_InventoryList_f, "Print the current inventory to the game console");
#endif
	INV_InitCallbacks();

	INV_EquipmentDefSanityCheck();
}
