/**
 * @file inv_shared.c
 * @brief Common object-, inventory-, container- and firemode-related functions.
 * @note Shared inventory management functions prefix: INVSH_
 * @note Shared firemode management functions prefix: FIRESH_
 * @note Shared character generating functions prefix: CHRSH_
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

#include "inv_shared.h"
#include "../shared/shared.h"

/*================================
INVENTORY MANAGEMENT FUNCTIONS
================================*/

static csi_t *CSI;
static invList_t *invUnused;
static invList_t *invUnusedRevert;
static item_t cacheItem = {NONE_AMMO, NULL, NULL, 0, 0}; /* to crash as soon as possible */

/**
 * @brief Initializes csi_t *CSI pointer.
 * @param[in] import
 * @sa G_Init
 * @sa Com_ParseScripts
 */
void INVSH_InitCSI (csi_t * import)
{
	CSI = import;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type floor
 */
qboolean INV_IsFloorDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idFloor;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type right
 */
qboolean INV_IsRightDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idRight;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type left
 */
qboolean INV_IsLeftDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idLeft;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type equip
 */
qboolean INV_IsEquipDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idEquip;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type armour
 */
qboolean INV_IsArmourDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idArmour;
}

/**
 * @brief Get the fire definitions for a given object
 * @param[in] obj The object to get the firedef for
 * @param[in] weapFdsIdx the weapon index in the fire definition array
 * @param[in] fdIdx the fire definition index for the weapon (given by @c weapFdsIdx)
 * @return Will never return NULL
 * @sa FIRESH_FiredefForWeapon
 */
const fireDef_t* FIRESH_GetFiredef (const objDef_t *obj, const int weapFdsIdx, const int fdIdx)
{
	if (weapFdsIdx < 0 || weapFdsIdx >= MAX_WEAPONS_PER_OBJDEF)
		Sys_Error("FIRESH_GetFiredef: weapFdsIdx out of bounds [%i] for item '%s'", weapFdsIdx, obj->id);
	if (fdIdx < 0 || fdIdx >= MAX_FIREDEFS_PER_WEAPON)
		Sys_Error("FIRESH_GetFiredef: fdIdx out of bounds [%i] for item '%s'", fdIdx, obj->id);
	return &obj->fd[weapFdsIdx & (MAX_WEAPONS_PER_OBJDEF - 1)][fdIdx & (MAX_FIREDEFS_PER_WEAPON - 1)];
}

/**
 * @brief Initializes the inventory definition by linking the ->next pointers properly.
 * @param[in] invList Pointer to invList_t definition being initialized. This array must have at
 * least the size @c MAX_INVLIST.
 * @param[in] store When true, the last invUnused is backuped - @sa INVSH_InvUnusedRevert
 * @sa G_Init
 * @sa CL_ResetSinglePlayerData
 * @sa CL_InitLocal
 */
void INVSH_InitInventory (invList_t * invList, qboolean store)
{
	int i;

	assert(invList);

	/* store last Unused stack? */
	if (store)
		invUnusedRevert = invUnused;

	invUnused = invList;
	/* first entry doesn't have an ancestor: invList[0]->next = NULL */
	invUnused->next = NULL;

	/* now link the invList_t next pointers
	 * invList[1]->next = invList[0]
	 * invList[2]->next = invList[1]
	 * invList[3]->next = invList[2]
	 * ... and so on
	 */
	for (i = 0; i < MAX_INVLIST - 1; i++) {
		invList_t *last = invUnused++;
		invUnused->next = last;
	}
}

/**
 * @brief reverts invUnused back to previous structure list
 */
void INVSH_InvUnusedRevert (void)
{
	if (!invUnusedRevert)
		return;

	invUnused = invUnusedRevert;
	invUnusedRevert = NULL;
}

static int cache_Com_CheckToInventory = INV_DOES_NOT_FIT;

/**
 * @brief Will check if the item-shape is colliding with something else in the container-shape at position x/y.
 * @note The function expects an already rotated shape for itemShape. Use Com_ShapeRotate if needed.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemShape
 * @param[in] x
 * @param[in] y
 */
static qboolean Com_CheckShapeCollision (const uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	/* Negative positions not allowed (all items are supposed to have at least one bit set in the first row and column) */
	if (x < 0 || y < 0) {
		Com_DPrintf(DEBUG_SHARED, "Com_CheckShapeCollision: x or y value negative: x=%i y=%i!\n", x, y);
		return qtrue;
	}

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT; i++) {
		const int itemRow = (itemShape >> (i * SHAPE_SMALL_MAX_WIDTH)) & 0xFF; /**< 0xFF is the length of one row in a "small shape" i.e. SHAPE_SMALL_MAX_WIDTH */
		const uint32_t itemRowShifted = itemRow << x;	/**< Result has to be limited to 32bit (SHAPE_BIG_MAX_WIDTH) */

		/* Check x maximum. */
		if (itemRowShifted >> x != itemRow)
			/* Out of bounds (32bit; a few bits of this row in itemShape were truncated) */
			return qtrue;

		/* Check y maximum. */
		if (y + i >= SHAPE_BIG_MAX_HEIGHT && itemRow)
			/* This row (row "i" in itemShape) is outside of the max. bound and has bits in it. */
			return qtrue;

		/* Check for collisions of the item with the big mask. */
		if (itemRowShifted & shape[y + i])
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Checks if an item-shape can be put into a container at a certain position... ignores any 'special' types of containers.
 * @param[in] i
 * @param[in] container The container (index) to look into.
 * @param[in] itemShape The shape info of an item to fit into the container.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The x value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa Com_CheckToInventory
 * @return qfalse if the item does not fit, qtrue if it fits.
 */
static qboolean Com_CheckToInventory_shape (const inventory_t * const i, const invDef_t * container, const uint32_t itemShape, const int x, const int y, const invList_t *ignoredItem)
{
	int j;
	invList_t *ic;
	static uint32_t mask[SHAPE_BIG_MAX_HEIGHT];

	assert(container);

	if (container->scroll)
		Sys_Error("Com_CheckToInventory_shape: No scrollable container will ever use this. This type does not support grid-packing!");

	/* check bounds */
	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if (!cache_Com_CheckToInventory) {
		/* extract shape info */
		for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
			mask[j] = ~container->shape[j];

		/* Add other items to mask. (i.e. merge their shapes at their location into the generated mask) */
		for (ic = i->c[container->id]; ic; ic = ic->next) {
			if (ignoredItem == ic)
				continue;

			if (ic->item.rotated)
				Com_MergeShapes(mask, Com_ShapeRotate(ic->item.t->shape), ic->x, ic->y);
			else
				Com_MergeShapes(mask, ic->item.t->shape, ic->x, ic->y);
		}
	}

	/* Test for collisions with newly generated mask. */
	if (Com_CheckShapeCollision(mask, itemShape, x, y))
		return qfalse;

	/* Everything ok. */
	return qtrue;
}

/**
 * @param[in] i The inventory to check the item in.
 * @param[in] od The item to check in the inventory.
 * @param[in] container The index of the container in the inventory to check the item in.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa Com_CheckToInventory_mask
 * @return INV_DOES_NOT_FIT if the item does not fit
 * @return INV_FITS if it fits and
 * @return INV_FITS_ONLY_ROTATED if it fits only when rotated 90 degree (to the left).
 * @return INV_FITS_BOTH if it fits either normally or when rotated 90 degree (to the left).
 */
int Com_CheckToInventory (const inventory_t * const i, const objDef_t *od, const invDef_t * container, const int x, const int y, const invList_t *ignoredItem)
{
	int fits;
	assert(i);
	assert(container);
	assert(od);

	/* armour vs item */
	if (INV_IsArmour(od)) {
		if (!container->armour && !container->all) {
			return INV_DOES_NOT_FIT;
		}
	} else if (!od->extension && container->extension) {
		return INV_DOES_NOT_FIT;
	} else if (!od->headgear && container->headgear) {
		return INV_DOES_NOT_FIT;
	} else if (container->armour) {
		return INV_DOES_NOT_FIT;
	}

	/* twohanded item */
	if (od->holdTwoHanded) {
		if ((INV_IsRightDef(container) && i->c[CSI->idLeft]) || INV_IsLeftDef(container))
			return INV_DOES_NOT_FIT;
	}

	/* left hand is busy if right wields twohanded */
	if (INV_IsLeftDef(container)) {
		if (i->c[CSI->idRight] && i->c[CSI->idRight]->item.t->holdTwoHanded)
			return INV_DOES_NOT_FIT;

		/* can't put an item that is 'fireTwoHanded' into the left hand */
		if (od->fireTwoHanded)
			return INV_DOES_NOT_FIT;
	}

	/* Single item containers, e.g. hands, extension or headgear. */
	if (container->single) {
		if (i->c[container->id]) {
			/* There is already an item. */
			return INV_DOES_NOT_FIT;
		} else {
			fits = INV_DOES_NOT_FIT; /* equals 0 */

			if (Com_CheckToInventory_shape(i, container, od->shape, x, y, ignoredItem))
				fits |= INV_FITS;
			if (Com_CheckToInventory_shape(i, container, Com_ShapeRotate(od->shape), x, y, ignoredItem))
				fits |= INV_FITS_ONLY_ROTATED;

			if (fits != INV_DOES_NOT_FIT)
				return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */

			Com_DPrintf(DEBUG_SHARED, "Com_CheckToInventory: INFO: Moving to 'single' container but item would not fit normally.\n");
			return INV_FITS; /**< We are returning with status qtrue (1) if the item does not fit at all - unlikely but not impossible. */
		}
	}

	/* Scrolling container have endless room, the item always fits. */
	if (container->scroll)
		return INV_FITS;

	/* Check 'grid' containers. */
	fits = INV_DOES_NOT_FIT; /* equals 0 */
	if (Com_CheckToInventory_shape(i, container, od->shape, x, y, ignoredItem))
		fits |= INV_FITS;
	if (Com_CheckToInventory_shape(i, container, Com_ShapeRotate(od->shape), x, y, ignoredItem)
	 && !INV_IsEquipDef(container) && !INV_IsFloorDef(container))
		fits |= INV_FITS_ONLY_ROTATED;

	return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */
}

/**
 * @brief Check if the (physical) information of 2 items is exactly the same.
 * @param[in] item1 First item to compare.
 * @param[in] item2 Second item to compare.
 * @return qtrue if they are identical or qfalse otherwise.
 */
qboolean Com_CompareItem (item_t *item1, item_t *item2)
{
	if (item1->t == item2->t && item1->m == item2->m && item1->a == item2->a)
		return qtrue;

	return qfalse;
}

/**
 * @brief Check if a position in a container is used by an item (i.e. collides with the shape).
 * @param[in] ic A pointer to an invList_t struct. The position is checked against its contained item.
 * @param[in] x The x location in the container.
 * @param[in] y The y location in the container.
 */
static qboolean Com_ShapeCheckPosition (const invList_t *ic, const int x, const int y)
{
	assert(ic);

 	/* Check if the position is inside the shape (depending on rotation value) of the item. */
	if (ic->item.rotated) {
 		if (((Com_ShapeRotate(ic->item.t->shape) >> (x - ic->x) >> (y - ic->y) * SHAPE_SMALL_MAX_WIDTH)) & 1)
 			return qtrue;
	} else {
 		if (((ic->item.t->shape >> (x - ic->x) >> (y - ic->y) * SHAPE_SMALL_MAX_WIDTH)) & 1)
 			return qtrue;
 	}

	/* Position is out of bounds or position not inside item-shape. */
	return qfalse;
}

/**
 * @brief Calculates the first "true" bit in the shape and returns its position in the container.
 * @note Use this to get the first "grab-able" grid-location (in the container) of an item.
 * @param[in] ic A pointer to an invList_t struct.
 * @param[out] x The x location inside the item.
 * @param[out] y The x location inside the item.
 * @sa Com_CheckToInventory
 */
void Com_GetFirstShapePosition (const invList_t *ic, int* const x, int* const y)
{
	int tempX, tempY;

	assert(ic);

	for (tempX = 0; tempX < SHAPE_SMALL_MAX_HEIGHT; tempX++)
		for (tempY = 0; tempY < SHAPE_SMALL_MAX_HEIGHT; tempY++)
			if (Com_ShapeCheckPosition(ic, ic->x + tempX, ic->y + tempY)) {
				*x = tempX;
				*y = tempY;
				return;
			}

	*x = *y = NONE;
}

/**
 * @brief Searches if there is a specific item already in the inventory&container.
 * @param[in] inv Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] item The item to search for.
 * @return qtrue if there already is at least one item of this type, otherwise qfalse.
 */
qboolean Com_ExistsInInventory (const inventory_t* const inv, const invDef_t * container, item_t item)
{
	invList_t *ic;

	for (ic = inv->c[container->id]; ic; ic = ic->next)
		if (Com_CompareItem(&ic->item, &item)) {
			return qtrue;
		}

	return qfalse;
}

/** Names of the filter types as used in console function. e.g. in .ufo files.
 * @sa inv_shared.h:itemFilterTypes_t */
static const char *filterTypeNames[MAX_FILTERTYPES] = {
	"primary",		/**< FILTER_S_PRIMARY */
	"secondary",	/**< FILTER_S_SECONDARY */
	"heavy",		/**< FILTER_S_HEAVY */
	"misc",			/**< FILTER_S_MISC */
	"armour",		/**< FILTER_S_ARMOUR */
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
itemFilterTypes_t INV_GetFilterTypeID (const char * filterTypeID)
{
	itemFilterTypes_t i;

	if (!filterTypeID)
		return MAX_FILTERTYPES;

	/* default filter type is primary */
	if (filterTypeID[0] == '\0')
		return FILTER_S_PRIMARY;

	for (i = 0; i < MAX_FILTERTYPES; i++) {
		if (filterTypeNames[i] && !strcmp(filterTypeNames[i], filterTypeID))
			return i;
	}

	/* No matching filter type found, returning max value. */
	return MAX_FILTERTYPES;
}


/**
 * @param[in] id The filter type index
 */
const char *INV_GetFilterType (const int id)
{
	assert(id >= 0);
	assert(id < MAX_FILTERTYPES);
	return filterTypeNames[id];
}

/**
 * @brief Checks whether a given item is an aircraftitem item
 * @note This is done by checking whether it's a craftitem and not
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @param[in] obj pointer to item definition to check whether it's an aircraftiem item
 * @return true if the given item is an aircraftitem item
 * @sa INV_IsBaseDefenceItem
 */
qboolean INV_IsCraftItem (const objDef_t *obj)
{
	return obj->craftitem.type != MAX_ACITEMS && !obj->isDummy;
}

/**
 * @brief Checks whether a given item is a basedefence item
 * @note This is done by checking whether it's a craftitem and whether it's
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @param[in] obj pointer to item definition to check whether it's a basedefence item
 * @return true if the given item is a basedefence item
 * @sa INV_IsCraftItem
 */
qboolean INV_IsBaseDefenceItem (const objDef_t *obj)
{
	return obj->craftitem.type != MAX_ACITEMS && obj->isDummy;
}

itemFilterTypes_t INV_GetFilterFromItem (const objDef_t *obj)
{
	assert(obj);

	/* heavy weapons may be primary too. check heavy first */
	if (obj->isHeavy)
		return FILTER_S_HEAVY;
	else if (obj->isPrimary)
		return FILTER_S_PRIMARY;
	else if (obj->isSecondary)
		return FILTER_S_SECONDARY;
	else if (obj->isMisc)
		return FILTER_S_MISC;
	else if (INV_IsArmour(obj))
		return FILTER_S_ARMOUR;

	/** @todo need to implement everything */
	Sys_Error("INV_GetFilterFromItem: unknown filter category for item '%s'", obj->id);
}

/**
 * @brief Checks if the given object/item matched the given filter type.
 * @param[in] obj A pointer to an objDef_t item.
 * @param[in] filterType Filter type to check against.
 * @return qtrue if obj is in filterType
 */
qboolean INV_ItemMatchesFilter (const objDef_t *obj, const itemFilterTypes_t filterType)
{
	int i;

	if (!obj)
		return qfalse;

	switch (filterType) {
	case FILTER_S_PRIMARY:
		if (obj->isPrimary && !obj->isHeavy)
			return qtrue;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			if (obj->weapons[i] && obj->weapons[i] != obj  && INV_ItemMatchesFilter(obj->weapons[i], filterType))
				return qtrue;
		}
		break;

	case FILTER_S_SECONDARY:
		if (obj->isSecondary && !obj->isHeavy)
			return qtrue;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			if (obj->weapons[i] && obj->weapons[i] != obj && INV_ItemMatchesFilter(obj->weapons[i], filterType))
				return qtrue;
		}
		break;

	case FILTER_S_HEAVY:
		if (obj->isHeavy)
			return qtrue;

		/* Check if one of the items that uses this ammo matches this filter type. */
		for (i = 0; i < obj->numWeapons; i++) {
			if (obj->weapons[i] && obj->weapons[i] != obj && INV_ItemMatchesFilter(obj->weapons[i], filterType))
				return qtrue;
		}
		break;

	case FILTER_S_ARMOUR:
		return INV_IsArmour(obj);

	case FILTER_S_MISC:
		return obj->isMisc;

	case FILTER_CRAFTITEM:
		/** @todo Should we handle FILTER_AIRCRAFT here as well? */
		return INV_IsCraftItem(obj);

	case FILTER_UGVITEM:
		return obj->isUGVitem;

	case FILTER_DUMMY:
		return obj->isDummy;

	case FILTER_AIRCRAFT:
		return !strcmp(obj->type, "aircraft");

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
	return qfalse;
}

/**
 * @brief Searches if there is an item at location (x/y) in a scrollable container. You can also provide an item to search for directly (x/y is ignored in that case).
 * @note x = x-th item in a row, y = row. i.e. x/y does not equal the "grid" coordinates as used in those containers.
 * @param[in] i Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] x/y Position in the scrollable container that you want to check. Ignored if "item" is set.
 * @param[in] item The item to search. Will ignore "x" and "y" if set, it'll also search invisible items.
 * @param[in] filterType Enum definition of type (types of items for filtering purposes).
 * @return invList_t Pointer to the invList_t/item that is located at x/y or equals "item".
 * @sa Com_SearchInInventory
 */
invList_t *Com_SearchInInventoryWithFilter (const inventory_t* const i, const invDef_t * container, int x, int y, objDef_t *item,  const itemFilterTypes_t filterType)
{
	invList_t *ic;
	/** @todo is this function doing any reasonable stuff if the item is a null pointer?
	 * if not, we can return null without walking through the whole container */
	for (ic = i->c[container->id]; ic; ic = ic->next) {
		/* Search only in the items that could get displayed. */
		if (ic && ic->item.t && (INV_ItemMatchesFilter(ic->item.t, filterType) || filterType == MAX_FILTERTYPES)) {
			if (item) {
				/* We search _everything_, no matter what location it is (i.e. x/y are ignored). */
				if (item == ic->item.t)
					return ic;
			}
		}
	}

	/* No item with these coordinates (or matching item) found. */
	return NULL;
}

/**
 * @brief Searches if there is an item at location (x,y) in a container.
 * @param[in] i Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] x/y Position in the container that you want to check.
 * @return invList_t Pointer to the invList_t/item that is located at x/y.
 * @sa INV_SearchInScrollableContainer
 */
invList_t *Com_SearchInInventory (const inventory_t* const i, const invDef_t * container, const int x, const int y)
{
	invList_t *ic;

	assert(container);

	/* Only a single item. */
	if (container->single)
		return i->c[container->id];

	if (container->scroll)
		Sys_Error("Com_SearchInInventory: Scrollable containers (%i:%s) are not supported by this function.\nUse INV_SearchInScrollableContainer instead!",
				container->id, container->name);

	/* More than one item - search for the item that is located at location x/y in this container. */
	for (ic = i->c[container->id]; ic; ic = ic->next)
		if (Com_ShapeCheckPosition(ic, x, y))
			return ic;

	/* Found nothing. */
	return NULL;
}


/**
 * @brief Add an item to a specified container in a given inventory.
 * @note Set x and y to NONE if the item should get added but not displayed.
 * @param[in] i Pointer to inventory definition, to which we will add item.
 * @param[in] item Item to add to given container (needs to have "rotated" tag already set/checked, this is NOT checked here!)
 * @param[in] container Container in given inventory definition, where the new item will be stored.
 * @param[in] x The x location in the container.
 * @param[in] y The x location in the container.
 * @param[in] amount How many items of this type should be added. (this will overwrite the amount as defined in "item.amount")
 * @sa Com_RemoveFromInventory
 */
invList_t *Com_AddToInventory (inventory_t * const i, item_t item, const invDef_t * container, int x, int y, int amount)
{
	invList_t *ic;

	if (!item.t)
		return NULL;

	if (amount <= 0)
		return NULL;

	if (!invUnused)
		Sys_Error("Com_AddToInventory: No free inventory space!");

	assert(i);
	assert(container);

	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT) {
		/* No (sane) position in container given as parameter - find free space on our own. */
		Com_FindSpace(i, &item, container, &x, &y, NULL);
		if (x == NONE)
			return NULL;
	}

	/**
	 * What we are doing here.
	 * invList_t array looks like that: [u]->next = [w]; [w]->next = [x]; [...]; [z]->next = NULL.
	 * i->c[container->id] as well as ic are such invList_t.
	 * Now we want to add new item to this container and that means, we need to create some [t]
	 * and make sure, that [t]->next points to [u] (so the [t] will be the first in array).
	 * ic = i->c[container->id];
	 * So, we are storing old value of i->c[container->id] in ic to remember what was in the original
	 * container. If the i->c[container->id]->next pointed to [abc], the ic->next will also point to [abc].
	 * The ic is our [u] and [u]->next still points to our [w].
	 * i->c[container->id] = invUnused;
	 * Now we are creating new container - the "original" i->c[container->id] is being set to empty invUnused.
	 * This is our [t].
	 * invUnused = invUnused->next;
	 * Now we need to make sure, that our [t] will point to next free slot in our inventory. Remember, that
	 * invUnused was empty before, so invUnused->next will point to next free slot.
	 * i->c[container->id]->next = ic;
	 * We assigned our [t]->next to [u] here. Thanks to that we still have the correct ->next chain in our
	 * inventory list.
	 * ic = i->c[container->id];
	 * And now ic will be our [t], that is the newly added container.
	 * After that we can easily add the item (item.t, x and y positions) to our [t] being ic.
	 */

	/* idEquip and idFloor */
	if (container->temp) {
		for (ic = i->c[container->id]; ic; ic = ic->next)
			if (Com_CompareItem(&ic->item, &item)) {
				ic->item.amount += amount;
				Com_DPrintf(DEBUG_SHARED, "Com_AddToInventory: Amount of '%s': %i\n",
					ic->item.t->name, ic->item.amount);
				return ic;
			}
	}

	/* not found - add a new one */
	/* Get empty container */
	ic = invUnused;

	/* Ensure, that for later usage invUnused will be the next empty/free slot.
	 * It is not used _here_ anymore. */
	invUnused = ic->next;

	/* Set the "next" link of the new "first item" to the original "first item". */
	ic->next = i->c[container->id];

	/* Remember the new "first item" (i.e. the yet empty entry). */
	i->c[container->id] = ic;
/*	Com_Printf("Add to container %i: %s\n", container, item.t->id);*/

	/* Set the data in the new entry to the data we got via function-parameters.*/
	ic->item = item;
	ic->item.amount = amount;
	ic->x = x;
	ic->y = y;

	if (container->single && i->c[container->id]->next)
		Com_Printf("Com_AddToInventory: Error: single container %s has many items.\n", container->name);

	return ic;
}

/**
 * @param[in] i The inventory the container is in.
 * @param[in] container The container where the item should be removed.
 * @param[in] fItem The item to be removed.
 * @return qtrue If removal was successful.
 * @return qfalse If nothing was removed or an error occurred.
 * @sa Com_AddToInventory
 */
qboolean Com_RemoveFromInventory (inventory_t* const i, const invDef_t * container, invList_t *fItem)
{
	invList_t *ic, *previous;

	assert(i);
	assert(container);
	assert(fItem);

	ic = i->c[container->id];
	if (!ic) {
#ifdef PARANOID
		Com_DPrintf(DEBUG_SHARED, "Com_RemoveFromInventory - empty container %s\n", container->name);
#endif
		return qfalse;
	}

	/** @todo the problem here is, that in case of a move inside the same container
	 * the item don't just get updated x and y values but it is tried to remove
	 * one of the items => crap - maybe we have to change the inventory move function
	 * to check for this case of move and only update the x and y coordinates instead
	 * of calling the add and remove functions */
	if (container->single || ic == fItem) {
		cacheItem = ic->item;
		/* temp container like idEquip and idFloor */
		if (container->temp && ic->item.amount > 1) {
			ic->item.amount--;
			Com_DPrintf(DEBUG_SHARED, "Com_RemoveFromInventory: Amount of '%s': %i\n",
				ic->item.t->name, ic->item.amount);
			return qtrue;
		}

		if (container->single && ic->next)
			Com_Printf("Com_RemoveFromInventory: Error: single container %s has many items.\n", container->name);

		/* An item in other containers than idFloor or idEquip should
		 * always have an amount value of 1.
		 * The other container types do not support stacking.*/
		assert(ic->item.amount == 1);

		i->c[container->id] = ic->next;

		/* updated invUnused to be able to reuse this space later again */
		ic->next = invUnused;
		invUnused = ic;

		return qtrue;
	}

	for (previous = i->c[container->id]; ic; ic = ic->next) {
		if (ic == fItem) {
			cacheItem = ic->item;
			/* temp container like idEquip and idFloor */
			if (ic->item.amount > 1 && container->temp) {
				ic->item.amount--;
				Com_DPrintf(DEBUG_SHARED, "Com_RemoveFromInventory: Amount of '%s': %i\n",
					ic->item.t->name, ic->item.amount);
				return qtrue;
			}

			if (ic == i->c[container->id])
				i->c[container->id] = i->c[container->id]->next;
			else
				previous->next = ic->next;

			ic->next = invUnused;
			invUnused = ic;

			return qtrue;
		}
		previous = ic;
	}
	return qfalse;
}

/**
 * @brief Conditions for moving items between containers.
 * @param[in] i Inventory to move in.
 * @param[in] from Source container.
 * @param[in] fItem The item to be moved.
 * @param[in] to Destination container.
 * @param[in] tx X coordinate in destination container.
 * @param[in] ty Y coordinate in destination container.
 * @param[in,out] TU pointer to entity available TU at this moment
 * or @c NULL if TU doesn't matter (outside battlescape)
 * @param[out] icp
 * @return IA_NOTIME when not enough TU.
 * @return IA_NONE if no action possible.
 * @return IA_NORELOAD if you cannot reload a weapon.
 * @return IA_RELOAD_SWAP in case of exchange of ammo in a weapon.
 * @return IA_RELOAD when reloading.
 * @return IA_ARMOUR when placing an armour on the actor.
 * @return IA_MOVE when just moving an item.
 */
int Com_MoveInInventory (inventory_t* const i, const invDef_t * from, invList_t *fItem, const invDef_t * to, int tx, int ty, int *TU, invList_t ** icp)
{
	invList_t *ic;

	int time;
	int checkedTo = INV_DOES_NOT_FIT;
	qboolean alreadyRemovedSource = qfalse;

	assert(to);
	assert(from);

	if (icp)
		*icp = NULL;

	if (from == to && fItem->x == tx && fItem->y == ty)
		return IA_NONE;

	time = from->out + to->in;
	if (from == to) {
		if (INV_IsFloorDef(from))
			time = 0;
		else
			time /= 2;
	}

	if (TU && *TU < time)
		return IA_NOTIME;

	assert(i);

	/* Special case for moving an item within the same container. */
	if (from == to) {
		/* Do nothing if we move inside a scroll container. */
		if (from->scroll)
			return IA_NONE;

		ic = i->c[from->id];
		for (; ic; ic = ic->next) {
			if (ic == fItem) {
				if (ic->item.amount > 1) {
					checkedTo = Com_CheckToInventory(i, ic->item.t, to, tx, ty, fItem);
					if (checkedTo & INV_FITS) {
						ic->x = tx;
						ic->y = ty;
						if (icp)
							*icp = ic;
						return IA_MOVE;
					}
					return IA_NONE;
				}
			}
		}
	}

	/* If weapon is twohanded and is moved from hand to hand do nothing. */
	/* Twohanded weapon are only in CSI->idRight. */
	if (fItem->item.t->fireTwoHanded && INV_IsLeftDef(to) && INV_IsRightDef(from)) {
		return IA_NONE;
	}

	/* If non-armour moved to an armour slot then abort.
	 * Same for non extension items when moved to an extension slot. */
	if ((to->armour && !INV_IsArmour(fItem->item.t))
	 || (to->extension && !fItem->item.t->extension)
	 || (to->headgear && !fItem->item.t->headgear)) {
		return IA_NONE;
	}

	/* Check if the target is a blocked inv-armour and source!=dest. */
	if (to->single)
		checkedTo = Com_CheckToInventory(i, fItem->item.t, to, 0, 0, fItem);
	else {
		if (tx == NONE || ty == NONE)
			Com_FindSpace(i, &fItem->item, to, &tx, &ty, fItem);
		checkedTo = Com_CheckToInventory(i, fItem->item.t, to, tx, ty, fItem);
	}

	if (to->armour && from != to && !checkedTo) {
		item_t cacheItem2;
		invList_t *icTo;
		/* Store x/y origin coordinates of removed (source) item.
		 * When we re-add it we can use this. */
	 	const int cacheFromX = fItem->x;
	 	const int cacheFromY = fItem->y;

		/* Check if destination/blocking item is the same as source/from item.
		 * In that case the move is not needed -> abort. */
		icTo = Com_SearchInInventory(i, to, tx, ty);
		if (fItem->item.t == icTo->item.t)
			return IA_NONE;

		/* Actually remove the ammo from the 'from' container. */
		if (!Com_RemoveFromInventory(i, from, fItem))
			return IA_NONE;
		else
			alreadyRemovedSource = qtrue;	/**< Removal successful - store this info. */

		cacheItem2 = cacheItem; /* Save/cache (source) item. The cacheItem is modified in Com_MoveInInventory. */

		/* Move the destination item to the source. */
		Com_MoveInInventory(i, to, icTo, from, cacheFromX, cacheFromY, TU, icp);

		/* Reset the cached item (source) (It'll be move to container emptied by destination item later.) */
		cacheItem = cacheItem2;
	} else if (!checkedTo) {
		/* Get the target-invlist (e.g. a weapon). We don't need to check for
		 * scroll because checkedTo is always true here. */
		ic = Com_SearchInInventory(i, to, tx, ty);

		if (ic && !INV_IsEquipDef(to) && INVSH_LoadableInWeapon(fItem->item.t, ic->item.t)) {
			/* A target-item was found and the dragged item (implicitly ammo)
			 * can be loaded in it (implicitly weapon). */
			if (ic->item.a >= ic->item.t->ammo && ic->item.m == fItem->item.t) {
				/* Weapon already fully loaded with the same ammunition -> abort */
				return IA_NORELOAD;
			}
			time += ic->item.t->reload;
			if (!TU || *TU >= time) {
				if (TU)
					*TU -= time;
				if (ic->item.a >= ic->item.t->ammo) {
					/* exchange ammo */
					const item_t item = {NONE_AMMO, NULL, ic->item.m, 0, 0};

					/* Actually remove the ammo from the 'from' container. */
					if (!Com_RemoveFromInventory(i, from, fItem))
						return IA_NONE;

					/* Add the currently used ammo in a free place of the "from" container. */
					Com_AddToInventory(i, item, from, -1, -1, 1);

					ic->item.m = cacheItem.t;
					if (icp)
						*icp = ic;
					return IA_RELOAD_SWAP;
				} else {
					/* Actually remove the ammo from the 'from' container. */
					if (!Com_RemoveFromInventory(i, from, fItem))
						return IA_NONE;

					ic->item.m = cacheItem.t;
					/* loose ammo of type ic->item.m saved on server side */
					ic->item.a = ic->item.t->ammo;
					if (icp)
						*icp = ic;
					return IA_RELOAD;
				}
			}
			/* Not enough time -> abort. */
			return IA_NOTIME;
		}

		/* temp container like idEquip and idFloor */
		if (ic && to->temp) {
			/** We are moving to a blocked location container but it's the base-equipment floor or a battlescape floor.
			 * We add the item anyway but it'll not be displayed (yet)
			 * @todo change the other code to browse trough these things. */
			Com_FindSpace(i, &fItem->item, to, &tx, &ty, fItem);	/**< Returns a free place or NONE for x&y if no free space is available elsewhere.
												 * This is then used in Com_AddToInventory below.*/
			if (tx == NONE || ty == NONE) {
				Com_DPrintf(DEBUG_SHARED, "Com_MoveInInventory - item will be added non-visible\n");
			}
		} else {
			/* Impossible move -> abort. */
			return IA_NONE;
		}
	}

	/* twohanded exception - only CSI->idRight is allowed for fireTwoHanded weapons */
	if (fItem->item.t->fireTwoHanded && INV_IsLeftDef(to)) {
#ifdef DEBUG
		Com_DPrintf(DEBUG_SHARED, "Com_MoveInInventory - don't move the item to CSI->idLeft it's fireTwoHanded\n");
#endif
		to = &CSI->ids[CSI->idRight];
	}
#ifdef PARANOID
	else if (fItem->item.t->fireTwoHanded)
		Com_DPrintf(DEBUG_SHARED, "Com_MoveInInventory: move fireTwoHanded item to container: %s\n", to->name);
#endif

	if (checkedTo == INV_FITS_ONLY_ROTATED) {
		/* Set rotated tag */
		Com_DPrintf(DEBUG_SHARED, "Com_MoveInInventory: setting rotate tag.\n");
		fItem->item.rotated = qtrue;
	} else if (fItem->item.rotated) {
		/* Remove rotated tag */
		Com_DPrintf(DEBUG_SHARED, "Com_MoveInInventory: removing rotate tag.\n");
		fItem->item.rotated = qfalse;
	}

	/* Actually remove the item from the 'from' container (if it wasn't already removed). */
	if (!alreadyRemovedSource)
		if (!Com_RemoveFromInventory(i, from, fItem))
			return IA_NONE;

	/* successful */
	if (TU)
		*TU -= time;

	assert(cacheItem.t);
	ic = Com_AddToInventory(i, cacheItem, to, tx, ty, 1);

	/* return data */
	if (icp) {
		assert(ic);
		*icp = ic;
	}

	if (INV_IsArmourDef(to)) {
		assert(INV_IsArmour(cacheItem.t));
		return IA_ARMOUR;
	} else
		return IA_MOVE;
}

qboolean INVSH_UseableForTeam (const objDef_t *od, const int team)
{
	const qboolean isArmour = INV_IsArmour(od);
	if (isArmour && od->useable != team)
		return qfalse;

	return qtrue;
}

/**
 * @brief Clears the linked list of a container - removes all items from this container.
 * @param[in] i The inventory where the container is located.
 * @param[in] container Index of the container which will be cleared.
 * @sa INVSH_DestroyInventory
 * @note This should only be called for temp containers if the container is really a temp container
 * e.g. the container of a dropped weapon in tactical mission (ET_ITEM)
 * in every other case just set the pointer to NULL for a temp container like idEquip or idFloor
 */
void INVSH_EmptyContainer (inventory_t* const i, const invDef_t * container)
{
	invList_t *ic;
#ifdef DEBUG
	int cnt = 0;
#endif

	assert(i);
	assert(container);

#ifdef DEBUG
	if (container->temp)
		Com_DPrintf(DEBUG_SHARED, "INVSH_EmptyContainer: Emptying temp container %s.\n", container->name);
#endif

	ic = i->c[container->id];

	while (ic) {
		invList_t *old = ic;
		ic = ic->next;
		old->next = invUnused;
		invUnused = old;
#ifdef DEBUG
		if (cnt >= MAX_INVLIST) {
			Com_Printf("Error: There are more than the allowed entries in container %s (cnt:%i, MAX_INVLIST:%i) (INVSH_EmptyContainer)\n",
				container->name, cnt, MAX_INVLIST);
			break;
		}
		cnt++;
#endif
	}
	i->c[container->id] = NULL;
}

/**
 * @brief Destroys inventory.
 * @param[in] i Pointer to the inventory which should be erased.
 * @note Loops through all containers in inventory, NULL for temp containers and INVSH_EmptyContainer() call for real containers.
 * @sa INVSH_EmptyContainer
 */
void INVSH_DestroyInventory (inventory_t* const i)
{
	int k;

	if (!i)
		return;

	for (k = 0; k < CSI->numIDs; k++)
		if (!CSI->ids[k].temp)
			INVSH_EmptyContainer(i, &CSI->ids[k]);

	memset(i, 0, sizeof(*i));
}

/**
 * @brief Finds space for item in inv at container
 * @param[in] inv The inventory to search space in
 * @param[in] item The item to check the space for
 * @param[in] container The container to search in
 * @param[out] px The x position in the container
 * @param[out] py The y position in the container
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa Com_CheckToInventory
 */
void Com_FindSpace (const inventory_t* const inv, const item_t *item, const invDef_t * container, int* const px, int* const py, const invList_t *ignoredItem)
{
	int x, y;

	assert(inv);
	assert(container);
	assert(!cache_Com_CheckToInventory);

	/* Scrollable container always have room. We return a dummy location. */
	if (container->scroll) {
		*px = *py = 0;
		return;
	}

	/** @todo optimize for single containers */

	for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
		for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
			const int checkedTo = Com_CheckToInventory(inv, item->t, container, x, y, ignoredItem);
			if (checkedTo) {
				cache_Com_CheckToInventory = INV_DOES_NOT_FIT;
				*px = x;
				*py = y;
				return;
			} else {
				cache_Com_CheckToInventory = INV_FITS;
			}
		}
	}
	cache_Com_CheckToInventory = INV_DOES_NOT_FIT;

#ifdef PARANOID
	Com_DPrintf(DEBUG_SHARED, "Com_FindSpace: no space for %s: %s in %s\n",
		item->t->type, item->t->id, container->name);
#endif
	*px = *py = NONE;
}

/**
 * @brief Tries to add an item to a container (in the inventory inv).
 * @param[in] inv Inventory pointer to add the item.
 * @param[in] item Item to add to inventory.
 * @param[in] container Container id.
 * @sa Com_FindSpace
 * @sa Com_AddToInventory
 */
qboolean Com_TryAddToInventory (inventory_t* const inv, item_t item, const invDef_t * container)
{
	int x, y;

	Com_FindSpace(inv, &item, container, &x, &y, NULL);

	if (x == NONE) {
		assert(y == NONE);
		return qfalse;
	} else {
		const int checkedTo = Com_CheckToInventory(inv, item.t, container, x, y, NULL);
		if (!checkedTo)
			return qfalse;
		else if (checkedTo == INV_FITS_ONLY_ROTATED)
			item.rotated = qtrue;
		else
			item.rotated = qfalse;

		return Com_AddToInventory(inv, item, container, x, y, 1) != NULL;
	}
}

/**
 * @brief Debug function to print the inventory items for a given inventory_t pointer.
 * @param[in] i The inventory you want to see on the game console.
 */
void INVSH_PrintContainerToConsole (inventory_t* const i)
{
	int container;

	assert(i);

	for (container = 0; container < CSI->numIDs; container++) {
		const invList_t *ic = i->c[container];
		Com_Printf("Container: %i\n", container);
		while (ic) {
			Com_Printf(".. item.t: %i, item.m: %i, item.a: %i, x: %i, y: %i\n",
					(ic->item.t ? ic->item.t->idx : NONE), (ic->item.m ? ic->item.m->idx : NONE),
					ic->item.a, ic->x, ic->y);
			if (ic->item.t)
				Com_Printf(".... weapon: %s\n", ic->item.t->id);
			if (ic->item.m)
				Com_Printf(".... ammo:   %s (%i)\n", ic->item.m->id, ic->item.a);
			ic = ic->next;
		}
	}
}

#define WEAPONLESS_BONUS	0.4		/* if you got neither primary nor secondary weapon, this is the chance to retry to get one (before trying to get grenades or blades) */

/**
 * @brief Pack a weapon, possibly with some ammo
 * @param[in] inv The inventory that will get the weapon
 * @param[in] weapon The weapon type index in gi.csi->ods
 * @param[in] ed The equipment for debug messages
 * @param[in] missedPrimary if actor didn't get primary weapon, this is 0-100 number to increase ammo number.
 * @sa INVSH_LoadableInWeapon
 */
static int INVSH_PackAmmoAndWeapon (inventory_t* const inv, objDef_t* weapon, int missedPrimary, const equipDef_t *ed)
{
	objDef_t *ammo = NULL;
	item_t item = {NONE_AMMO, NULL, NULL, 0, 0};
	int i;
	qboolean allowLeft;
	qboolean packed;
	int ammoMult = 1;

#ifdef PARANOID
	if (weapon == NULL) {
		Com_Printf("Error in INVSH_PackAmmoAndWeapon - weapon is invalid\n");
	}
#endif

	assert(!INV_IsArmour(weapon));
	item.t = weapon;

	/* are we going to allow trying the left hand */
	allowLeft = !(inv->c[CSI->idRight] && inv->c[CSI->idRight]->item.t->fireTwoHanded);

	if (!weapon->reload) {
		item.m = item.t; /* no ammo needed, so fire definitions are in t */
	} else {
		if (weapon->oneshot) {
			/* The weapon provides its own ammo (i.e. it is charged or loaded in the base.) */
			item.a = weapon->ammo;
			item.m = weapon;
			Com_DPrintf(DEBUG_SHARED, "INVSH_PackAmmoAndWeapon: oneshot weapon '%s' in equipment '%s'.\n", weapon->id, ed->name);
		} else {
			/* find some suitable ammo for the weapon (we will have at least one if there are ammos for this
			 * weapon in equipment definition) */
			int totalAvailableAmmo = 0;
			for (i = 0; i < CSI->numODs; i++) {
				objDef_t *obj = &CSI->ods[i];
				if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
					totalAvailableAmmo++;
				}
			}
			if (totalAvailableAmmo) {
				int randNumber = rand() % totalAvailableAmmo;
				for (i = 0; i < CSI->numODs; i++) {
					objDef_t *obj = &CSI->ods[i];
					if (ed->numItems[i] && INVSH_LoadableInWeapon(obj, weapon)) {
						randNumber--;
						if (randNumber < 0) {
							ammo = obj;
							break;
						}
					}
				}
			}

			if (!ammo) {
				Com_DPrintf(DEBUG_SHARED, "INVSH_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s'.\n", weapon->id, ed->name);
				return 0;
			}
			/* load ammo */
			item.a = weapon->ammo;
			item.m = ammo;
		}
	}

	if (!item.m) {
		Com_Printf("INVSH_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s'.\n", weapon->id, ed->name);
		return 0;
	}

	/* now try to pack the weapon */
	packed = Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idRight]);
	if (packed)
		ammoMult = 3;
	if (!packed && allowLeft)
		packed = Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idLeft]);
	if (!packed)
		packed = Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idBelt]);
	if (!packed)
		packed = Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idHolster]);
	if (!packed)
		return 0;


	/* pack some more ammo in the backpack */
	if (ammo) {
		int num;
		int numpacked = 0;

		/* how many clips? */
		num = (1 + ed->numItems[ammo->idx])
			* (float) (1.0f + missedPrimary / 100.0);

		/* pack some ammo */
		while (num--) {
			item_t mun = {NONE_AMMO, NULL, NULL, 0, 0};

			mun.t = ammo;
			/* ammo to backpack; belt is for knives and grenades */
			numpacked += Com_TryAddToInventory(inv, mun, &CSI->ids[CSI->idBackpack]);
			/* no problem if no space left; one ammo already loaded */
			if (numpacked > ammoMult || numpacked * weapon->ammo > 11)
				break;
		}
	}

	return qtrue;
}


/**
 * @brief Equip melee actor with item defined per teamDefs.
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] chr Pointer to character data.
 * @note Weapons assigned here cannot be collected in any case. These are dummy "actor weapons".
 */
void INVSH_EquipActorMelee (inventory_t* const inv, character_t* chr)
{
	objDef_t *obj;
	item_t item;

	assert(chr->teamDef->onlyWeapon);

	/* Get weapon */
	obj = chr->teamDef->onlyWeapon;
	Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActorMelee: team %i: %s, weapon: %s\n",
		chr->teamDef->idx, chr->teamDef->id, obj->id);

	/* Prepare item. This kind of item has no ammo, fire definitions are in item.t. */
	item.t = obj;
	item.m = item.t;
	item.a = NONE_AMMO;
	/* Every melee actor weapon definition is firetwohanded, add to right hand. */
	if (!obj->fireTwoHanded)
		Sys_Error("INVSH_EquipActorMelee: melee weapon %s for team %s is not firetwohanded!\n", obj->id, chr->teamDef->id);
	Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idRight]);
}

/**
 * @brief Equip robot actor with default weapon. (defined in ugv_t->weapon)
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] chr Pointer to character data.
 * @param[in] weapon Pointer to the item which being added to robot's inventory.
 */
void INVSH_EquipActorRobot (inventory_t* const inv, character_t* chr, objDef_t* weapon)
{
	item_t item;

	assert(chr);
	assert(weapon);
	assert(chr->teamDef->race == RACE_ROBOT);

	Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActorMelee: team %i: %s, weapon %i: %s\n",
		chr->teamDef->idx, chr->teamDef->id, weapon->idx, weapon->id);

	/* Prepare weapon in item. */
	item.t = weapon;
	item.a = NONE_AMMO;

	/* Get ammo for item/weapon. */
	assert(weapon->numAmmos > 0);	/* There _has_ to be at least one ammo-type. */
	assert(weapon->ammos[0]);
	item.m = weapon->ammos[0];

	Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idRight]);
}

/**
 * @brief Types of weapon that can be selected
 */
typedef enum {
	WEAPON_PARTICLE_OR_NORMAL = 0,	/**< primary weapon is a particle or normal weapon */
	WEAPON_OTHER = 1,				/**< primary weapon is not a particle or normal weapon */
	WEAPON_NO_PRIMARY = 2			/**< no primary weapon */
} equipPrimaryWeaponType_t;

/**
 * @brief Fully equip one actor. The equipment that is added to the inventory of the given actor
 * is taken from the equipment script definition.
 * @param[in] inv The inventory that will get the weapon.
 * @param[in] ed The equipment that is added from to the actors inventory
 * @param[in] chr Pointer to character data - to get the weapon and armour bools.
 * @note The code below is a complete implementation
 * of the scheme sketched at the beginning of equipment_missions.ufo.
 * Beware: If two weapons in the same category have the same price,
 * only one will be considered for inventory.
 */
void INVSH_EquipActor (inventory_t* const inv, const equipDef_t *ed, character_t* chr)
{
	int i, sum;
	const int numEquip = lengthof(ed->numItems);
	int hasWeapon = 0, repeat = 0;
	int missedPrimary = 0; /**< If actor has a primary weapon, this is zero. Otherwise, this is the probability * 100
							* that the actor had to get a primary weapon (used to compensate the lack of primary weapon) */
	equipPrimaryWeaponType_t primary = WEAPON_NO_PRIMARY;
	const float AKIMBO_CHANCE = 0.3; 	/**< if you got a one-handed secondary weapon (and no primary weapon),
											 this is the chance to get another one (between 0 and 1) */

	if (chr->teamDef->weapons) {
		objDef_t *primaryWeapon = NULL;
		/* Primary weapons */
		const int maxWeaponIdx = min(CSI->numODs - 1, numEquip - 1);
		int randNumber = rand() % 100;
		for (i = 0; i < maxWeaponIdx; i++) {
			objDef_t *obj = &CSI->ods[i];
			if (ed->numItems[i] && obj->weapon && obj->fireTwoHanded && (INV_ItemMatchesFilter(obj, FILTER_S_PRIMARY) || INV_ItemMatchesFilter(obj, FILTER_S_HEAVY))) {
				randNumber -= ed->numItems[i];
				missedPrimary += ed->numItems[i];
				if (!primaryWeapon && randNumber < 0)
					primaryWeapon = obj;
			}
		}
		/* See if a weapon has been selected. */
		if (primaryWeapon) {
			hasWeapon += INVSH_PackAmmoAndWeapon(inv, primaryWeapon, 0, ed);
			if (hasWeapon) {
				int ammo;

				/* Find the first possible ammo to check damage type. */
				for (ammo = 0; ammo < CSI->numODs; ammo++)
					if (ed->numItems[ammo] && INVSH_LoadableInWeapon(&CSI->ods[ammo], primaryWeapon))
						break;
				if (ammo < CSI->numODs) {
					primary =
						/* To avoid two particle weapons. */
						!(CSI->ods[ammo].dmgtype == CSI->damParticle)
						/* To avoid SMG + Assault Rifle */
						&& !(CSI->ods[ammo].dmgtype == CSI->damNormal);
				}
				/* reset missedPrimary: we got a primary weapon */
				missedPrimary = 0;
			} else {
				Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: primary weapon '%s' couldn't be equiped in equipment '%s'.\n", primaryWeapon->id, ed->name);
				repeat = WEAPONLESS_BONUS > frand();
			}
		}

		/* Sidearms (secondary weapons with reload). */
		do {
			int randNumber = rand() % 100;
			objDef_t *secondaryWeapon = NULL;
			for (i = 0; i < CSI->numODs; i++) {
				objDef_t *obj = &CSI->ods[i];
				if (ed->numItems[i] && obj->weapon && obj->reload && !obj->deplete
				 && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY)) {
					randNumber -= ed->numItems[i] / (primary == WEAPON_PARTICLE_OR_NORMAL ? 2 : 1);
					if (randNumber < 0) {
						secondaryWeapon = obj;
						break;
					}
				}
			}

			if (secondaryWeapon) {
				hasWeapon += INVSH_PackAmmoAndWeapon(inv, secondaryWeapon, missedPrimary, ed);
				if (hasWeapon) {
					/* Try to get the second akimbo pistol if no primary weapon. */
					if (primary == WEAPON_NO_PRIMARY && !secondaryWeapon->fireTwoHanded && frand() < AKIMBO_CHANCE) {
						INVSH_PackAmmoAndWeapon(inv, secondaryWeapon, 0, ed);
					}
				}
			}
		} while (!hasWeapon && repeat--);

		/* Misc items and secondary weapons without reload. */
		if (!hasWeapon)
			repeat = WEAPONLESS_BONUS > frand();
		else
			repeat = 0;
		/* Misc object probability can be bigger than 100 -- you're sure to
		 * have one misc if it fits your backpack */
		sum = 0;
		for (i = 0; i < CSI->numODs; i++) {
			objDef_t *obj = &CSI->ods[i];
			if (ed->numItems[i] && ((obj->weapon && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY)
			 && (!obj->reload || obj->deplete)) || INV_ItemMatchesFilter(obj, FILTER_S_MISC))) {
				/* if ed->num[i] is greater than 100, the first number is the number of items you'll get:
				 * don't take it into account for probability
				 * Make sure that the probability is at least one if an item can be selected */
				sum += ed->numItems[i] ? max(ed->numItems[i] % 100, 1) : 0;
			}
		}
		if (sum) {
			do {
				int randNumber = rand() % sum;
				objDef_t *secondaryWeapon = NULL;
				for (i = 0; i < CSI->numODs; i++) {
					objDef_t *obj = &CSI->ods[i];
					if (ed->numItems[i] && ((obj->weapon && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY)
					 && (!obj->reload || obj->deplete)) || INV_ItemMatchesFilter(obj, FILTER_S_MISC))) {
						randNumber -= ed->numItems[i] ? max(ed->numItems[i] % 100,1) : 0;
						if (randNumber < 0) {
							secondaryWeapon = obj;
							break;
						}
					}
				}

				if (secondaryWeapon) {
					int num = ed->numItems[secondaryWeapon->idx] / 100 + (ed->numItems[secondaryWeapon->idx] % 100 >= 100 * frand());
					while (num--) {
						hasWeapon += INVSH_PackAmmoAndWeapon(inv, secondaryWeapon, 0, ed);
					}
				}
			} while (repeat--); /* Gives more if no serious weapons. */
		}

		/* If no weapon at all, bad guys will always find a blade to wield. */
		if (!hasWeapon) {
			int maxPrice = 0;
			objDef_t *blade = NULL;
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: no weapon picked in equipment '%s', defaulting to the most expensive secondary weapon without reload.\n", ed->name);
			for (i = 0; i < CSI->numODs; i++) {
				objDef_t *obj = &CSI->ods[i];
				if (ed->numItems[i] && obj->weapon && INV_ItemMatchesFilter(obj, FILTER_S_SECONDARY) && !obj->reload) {
					if (obj->price > maxPrice) {
						maxPrice = obj->price;
						blade = obj;
					}
				}
			}
			if (maxPrice)
				hasWeapon += INVSH_PackAmmoAndWeapon(inv, blade, 0, ed);
		}
		/* If still no weapon, something is broken, or no blades in equipment. */
		if (!hasWeapon)
			Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: cannot add any weapon; no secondary weapon without reload detected for equipment '%s'.\n", ed->name);

		/* Armour; especially for those without primary weapons. */
		repeat = (float) missedPrimary > frand() * 100.0;
	} else {
		Sys_Error("INVSH_EquipActor: character '%s' may not carry weapons\n", chr->name);
	}

	if (!chr->teamDef->armour) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_EquipActor: character '%s' may not carry armour\n", chr->name);
		return;
	}

	do {
		int randNumber = rand() % 100;
		for (i = 0; i < CSI->numODs; i++) {
			objDef_t *armour = &CSI->ods[i];
			if (ed->numItems[i] && INV_ItemMatchesFilter(armour, FILTER_S_ARMOUR)) {
				randNumber -= ed->numItems[i];
				if (randNumber < 0) {
					const item_t item = {NONE_AMMO, NULL, armour, 0, 0};
					if (Com_TryAddToInventory(inv, item, &CSI->ids[CSI->idArmour]))
						return;
				}
			}
		}
	} while (repeat--);
}

/*
================================
CHARACTER GENERATING FUNCTIONS
================================
*/

/**
 * @brief Determines the maximum amount of XP per skill that can be gained from any one mission.
 * @param[in] skill The skill for which to fetch the maximum amount of XP.
 * @sa G_UpdateCharacterSkills
 * @sa G_GetEarnedExperience
 * @note Explanation of the values here:
 * There is a maximum speed at which skills may rise over the course of 100 missions (the predicted career length of a veteran soldier).
 * For example, POWER will, at best, rise 10 points over 100 missions. If the soldier gets max XP every time.
 * Because the increase is given as experience^0.6, that means that the maximum XP cap x per mission is given as
 * log 10 / log x = 0.6
 * log x = log 10 / 0.6
 * x = 10 ^ (log 10 / 0.6)
 * x = 46
 * The division by 100 happens in G_UpdateCharacterSkills
 */
int CHRSH_CharGetMaxExperiencePerMission (abilityskills_t skill)
{
	switch (skill) {
	case ABILITY_POWER:
		return 46;
	case ABILITY_SPEED:
		return 91;
	case ABILITY_ACCURACY:
		return 290;
	case ABILITY_MIND:
		return 450;
	case SKILL_CLOSE:
		return 680;
	case SKILL_HEAVY:
		return 680;
	case SKILL_ASSAULT:
		return 680;
	case SKILL_SNIPER:
		return 680;
	case SKILL_EXPLOSIVE:
		return 680;
	case SKILL_NUM_TYPES: /* This is health. */
		return 2154;
	default:
		Com_DPrintf(DEBUG_GAME, "G_GetMaxExperiencePerMission: invalid skill type\n");
		return 0;
	}
}

/**
 * @brief Check if a team definition is alien.
 * @param[in] td Pointer to the team definition to check.
 */
qboolean CHRSH_IsTeamDefAlien (const teamDef_t* const td)
{
	return td->race == RACE_TAMAN || td->race == RACE_ORTNOK
		|| td->race == RACE_BLOODSPIDER || td->race == RACE_SHEVAAR;
}

/**
 * @brief Templates for the different unit types. Each element of the array is a tuple that
 * indicates the minimum and the maximum value for the relevant ability or skill.
 */
static const int commonSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {15, 25}, /* Close */
	 {15, 25}, /* Heavy */
	 {15, 25}, /* Assault */
	 {15, 25}, /* Sniper */
	 {15, 25}, /* Explosives */
	 {80, 110}}; /* Health */

static const int closeSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {25, 40}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int heavySoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {25, 40}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int assaultSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {25, 40}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int sniperSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {25, 40}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int blastSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {80, 110}}; /* Health */

static const int eliteSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {30, 40}, /* Accuracy */
	 {30, 45}, /* Mind */
	 {25, 40}, /* Close */
	 {25, 40}, /* Heavy */
	 {25, 40}, /* Assault */
	 {25, 40}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {100, 130}}; /* Health */

static const int civilSoldier[][2] =
	{{5, 10}, /* Strength */
	 {5, 10}, /* Speed */
	 {10, 15}, /* Accuracy */
	 {10, 15}, /* Mind */
	 {5, 10}, /* Close */
	 {5, 10}, /* Heavy */
	 {5, 10}, /* Assault */
	 {5, 10}, /* Sniper */
	 {5, 10}, /* Explosives */
	 {5, 10}}; /* Health */

static const int tamanSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {40, 50}, /* Accuracy */
	 {50, 85}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {100, 130}}; /* Health */

static const int ortnokSoldier[][2] =
	{{45, 65}, /* Strength */
	 {20, 30}, /* Speed */
	 {30, 45}, /* Accuracy */
	 {20, 40}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {150, 190}}; /* Health */

static const int shevaarSoldier[][2] =
	{{30, 40}, /* Strength */
	 {30, 40}, /* Speed */
	 {40, 70}, /* Accuracy */
	 {30, 60}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {120, 160}}; /* Health */

static const int bloodSoldier[][2] =
	{{55, 55}, /* Strength */
	 {50, 50}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {150, 150}}; /* Health */

static const int robotSoldier[][2] =
	{{55, 55}, /* Strength */
	 {40, 40}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {200, 200}}; /* Health */

/** @brief For multiplayer characters ONLY! */
static const int MPSoldier[][2] =
	{{25, 75}, /* Strength */
	 {25, 35}, /* Speed */
	 {20, 75}, /* Accuracy */
	 {30, 75}, /* Mind */
	 {20, 75}, /* Close */
	 {20, 75}, /* Heavy */
	 {20, 75}, /* Assault */
	 {20, 75}, /* Sniper */
	 {20, 75}, /* Explosives */
	 {80, 130}}; /* Health */

/**
 * @brief Generates a skill and ability set for any character.
 * @param[in] chr Pointer to the character, for which we generate stats.
 * @param[in] multiplayer If this is true we use the skill values from @c MPSoldier
 * mulitplayer is a special case here
 */
void CHRSH_CharGenAbilitySkills (character_t * chr, qboolean multiplayer)
{
	int i;
	const int (*chrTemplate)[2];

	/* Add modifiers for difficulty setting here! */
	switch (chr->teamDef->race) {
	case RACE_TAMAN:
		chrTemplate = tamanSoldier;
		break;
	case RACE_ORTNOK:
		chrTemplate = ortnokSoldier;
		break;
	case RACE_BLOODSPIDER:
		chrTemplate = bloodSoldier;
		break;
	case RACE_SHEVAAR:
		chrTemplate = shevaarSoldier;
		break;
	case RACE_CIVILIAN:
		chrTemplate = civilSoldier;
		break;
	case RACE_PHALANX_HUMAN: {
		if (multiplayer) {
			chrTemplate = MPSoldier;
		} else {
			/* Determine which soldier template to use.
			 * 25% of the soldiers will be specialists (5% chance each).
			 * 1% of the soldiers will be elite.
			 * 74% of the soldiers will be common. */
			const float soldierRoll = frand();
			if (soldierRoll <= 0.01f)
				chrTemplate = eliteSoldier;
			else if (soldierRoll <= 0.06)
				chrTemplate = closeSoldier;
			else if (soldierRoll <= 0.11)
				chrTemplate = heavySoldier;
			else if (soldierRoll <= 0.16)
				chrTemplate = assaultSoldier;
			else if (soldierRoll <= 0.22)
				chrTemplate = sniperSoldier;
			else if (soldierRoll <= 0.26)
				chrTemplate = blastSoldier;
			else
				chrTemplate = commonSoldier;
		}
		break;
	}
	case RACE_ROBOT:
		chrTemplate = robotSoldier;
		break;
	default:
		Sys_Error("CHRSH_CharGenAbilitySkills: unexpected race '%i'", chr->teamDef->race);
	}

	assert(chrTemplate);

	/* Abilities and skills -- random within the range */
	for (i = 0; i < SKILL_NUM_TYPES; i++) {
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		/* Reminder: In case if abilityWindow==0 the ability will be set to the lower limit. */
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.skills[i] = temp;
		chr->score.initialSkills[i] = temp;
	}

	{
		/* Health. */
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.initialSkills[SKILL_NUM_TYPES] = temp;
		chr->maxHP = temp;
		chr->HP = temp;
	}

	/* Morale */
	chr->morale = GET_MORALE(chr->score.skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;

	/* Initialize the experience values */
	for (i = 0; i <= SKILL_NUM_TYPES; i++) {
		chr->score.experience[i] = 0;
	}
}

/** @brief Used in CHRSH_CharGetHead and CHRSH_CharGetBody to generate the model path. */
static char CHRSH_returnModel[MAX_VAR];

/**
 * @brief Returns the body model for the soldiers for armoured and non armoured soldiers
 * @param chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 */
const char *CHRSH_CharGetBody (const character_t * const chr)
{
	/* models of UGVs don't change - because they are already armoured */
	if (chr->inv.c[CSI->idArmour] && chr->teamDef->race != RACE_ROBOT) {
		const objDef_t *od = chr->inv.c[CSI->idArmour]->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(CHRSH_returnModel, sizeof(CHRSH_returnModel), "%s%s/%s", chr->path, id, chr->body);
	} else
		Com_sprintf(CHRSH_returnModel, sizeof(CHRSH_returnModel), "%s/%s", chr->path, chr->body);
	return CHRSH_returnModel;
}

/**
 * @brief Returns the head model for the soldiers for armoured and non armoured soldiers
 * @param chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 */
const char *CHRSH_CharGetHead (const character_t * const chr)
{
	/* models of UGVs don't change - because they are already armoured */
	if (chr->inv.c[CSI->idArmour] && chr->fieldSize == ACTOR_SIZE_NORMAL) {
		const objDef_t *od = chr->inv.c[CSI->idArmour]->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(CHRSH_returnModel, sizeof(CHRSH_returnModel), "%s%s/%s", chr->path, id, chr->head);
	} else
		Com_sprintf(CHRSH_returnModel, sizeof(CHRSH_returnModel), "%s/%s", chr->path, chr->head);
	Com_DPrintf(DEBUG_SHARED, "CHRSH_CharGetBody: use '%s' as head model path for character\n", CHRSH_returnModel);
	return CHRSH_returnModel;
}

/**
 * @brief Prints a description of an object
 * @param[in] od object to display the info for
 */
void INVSH_PrintItemDescription (const objDef_t *od)
{
	int i;

	if (!od)
		return;

	Com_Printf("Item: %s\n", od->id);
	Com_Printf("... name          -> %s\n", od->name);
	Com_Printf("... type          -> %s\n", od->type);
	Com_Printf("... category      -> %i\n", od->animationIndex);
	Com_Printf("... weapon        -> %i\n", od->weapon);
	Com_Printf("... holdtwohanded -> %i\n", od->holdTwoHanded);
	Com_Printf("... firetwohanded -> %i\n", od->fireTwoHanded);
	Com_Printf("... thrown        -> %i\n", od->thrown);
	Com_Printf("... usable for weapon (if type is ammo):\n");
	for (i = 0; i < od->numWeapons; i++) {
		if (od->weapons[i])
			Com_Printf("    ... %s\n", od->weapons[i]->name);
	}
	Com_Printf("\n");
}

/**
 * @brief Returns the index of this item in the inventory.
 * @note check that id is a none empty string
 * @note previously known as RS_GetItem
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByID
 */
objDef_t *INVSH_GetItemByIDSilent (const char *id)
{
	int i;

	if (!id)
		return NULL;
	for (i = 0; i < CSI->numODs; i++) {	/* i = item index */
		objDef_t *item = &CSI->ods[i];
		if (!strcmp(id, item->id)) {
			return item;
		}
	}
	return NULL;
}

/**
 * @brief Returns the index of this item in the inventory.
 */
objDef_t *INVSH_GetItemByIDX (int index)
{
	if (index == NONE)
		return NULL;

	if (index < 0 || index >= CSI->numODs)
		Sys_Error("Invalid object index given: %i", index);

	return &CSI->ods[index];
}

/**
 * @brief Returns the item that belongs to the given id or @c NULL if it wasn't found.
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByIDSilent
 */
objDef_t *INVSH_GetItemByID (const char *id)
{
	objDef_t *od = INVSH_GetItemByIDSilent(id);
	if (!od)
		Com_Printf("INVSH_GetItemByID: Item \"%s\" not found.\n", id);

	return od;
}

/**
 * Searched an inventory container by a given container id
 * @param[in] id ID or name of the inventory container to search for
 * @return @c NULL if not found
 */
invDef_t *INVSH_GetInventoryDefinitionByID (const char *id)
{
	int i;
	invDef_t *container;

 	for (i = 0, container = CSI->ids; i < CSI->numIDs; container++, i++)
 		if (!strcmp(id, container->name))
 			return container;

 	return NULL;
}

/**
 * @brief Checks if an item can be used to reload a weapon.
 * @param[in] od The object definition of the ammo.
 * @param[in] weapon The weapon (in the inventory) to check the item with.
 * @return qboolean Returns qtrue if the item can be used in the given weapon, otherwise qfalse.
 * @sa INVSH_PackAmmoAndWeapon
 */
qboolean INVSH_LoadableInWeapon (const objDef_t *od, const objDef_t *weapon)
{
	int i;
	qboolean usable = qfalse;

#ifdef DEBUG
	if (!od) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No pointer given for 'od'.\n");
		return qfalse;
	}
	if (!weapon) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No weapon pointer given.\n");
		return qfalse;
	}
#endif

	if (od && od->numWeapons == 1 && od->weapons[0] && od->weapons[0] == od) {
		/* The weapon is only linked to itself. */
		return qfalse;
	}

	for (i = 0; i < od->numWeapons; i++) {
#ifdef DEBUG
		if (!od->weapons[i]) {
			Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No weapon pointer set for the %i. entry found in item '%s'.\n", i, od->id);
			break;
		}
#endif
		if (weapon == od->weapons[i]) {
			usable = qtrue;
			break;
		}
	}

	return usable;
}

/*
===============================
FIREMODE MANAGEMENT FUNCTIONS
===============================
*/

/**
 * @brief Returns the firedefinitions for a given weapon/ammo
 * @return The array (one-dimensional) of the firedefs of the ammo for a given weapon, or @c NULL if the ammo
 * doesn't support the given weapon
 * @sa FIRESH_GetFiredef
 */
const fireDef_t *FIRESH_FiredefForWeapon (const item_t *item)
{
	int i;
	const objDef_t *ammo = item->m;
	const objDef_t *weapon = item->t;

	/* this weapon does not use ammo, check for
	 * existing firedefs in the weapon. */
	if (weapon->numWeapons > 0)
		ammo = item->t;

	if (!ammo)
		return NULL;

	for (i = 0; i < ammo->numWeapons; i++) {
		if (weapon == ammo->weapons[i])
			return &ammo->fd[i][0];
	}

	return NULL;
}

/**
 * @brief Returns the default reaction firemode for a given ammo in a given weapon.
 * @param[in] ammo The ammo(or weapon-)object that contains the firedefs
 * @param[in] weaponFdsIdx The index in objDef[x]
 * @return Default reaction-firemode index in objDef->fd[][x]. -1 if an error occurs or no firedefs exist.
 */
int FIRESH_GetDefaultReactionFire (const objDef_t *ammo, int weaponFdsIdx)
{
	int fdIdx;
	if (weaponFdsIdx >= MAX_WEAPONS_PER_OBJDEF) {
		Com_Printf("FIRESH_GetDefaultReactionFire: bad weaponFdsIdx (%i) Maximum is %i.\n", weaponFdsIdx, MAX_WEAPONS_PER_OBJDEF - 1);
		return -1;
	}
	if (weaponFdsIdx < 0) {
		Com_Printf("FIRESH_GetDefaultReactionFire: Negative weaponFdsIdx given.\n");
		return -1;
	}

	if (ammo->numFiredefs[weaponFdsIdx] == 0) {
		Com_Printf("FIRESH_GetDefaultReactionFire: Probably not an ammo-object: %s\n", ammo->id);
		return -1;
	}

	for (fdIdx = 0; fdIdx < ammo->numFiredefs[weaponFdsIdx]; fdIdx++) {
		if (ammo->fd[weaponFdsIdx][fdIdx].reaction)
			return fdIdx;
	}

	return -1; /* -1 = undef firemode. Default for objects without a reaction-firemode */
}

/**
 * @brief Will merge the second shape (=itemShape) into the first one (=big container shape) on the position x/y.
 * @note The function expects an already rotated shape for itemShape. Use Com_ShapeRotate if needed.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemShape
 * @param[in] x
 * @param[in] y
 */
void Com_MergeShapes (uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	for (i = 0; (i < SHAPE_SMALL_MAX_HEIGHT) && (y + i < SHAPE_BIG_MAX_HEIGHT); i++)
		shape[y + i] |= ((itemShape >> i * SHAPE_SMALL_MAX_WIDTH) & 0xFF) << x;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] x
 * @param[in] y
 */
qboolean Com_CheckShape (const uint32_t *shape, const int x, const int y)
{
	const uint32_t row = shape[y];
	int position = pow(2, x);

	if (y >= SHAPE_BIG_MAX_HEIGHT || x >= SHAPE_BIG_MAX_WIDTH || x < 0 || y < 0) {
		Com_Printf("Com_CheckShape: Bad x or y value: (x=%i, y=%i)\n", x, y);
		return qfalse;
	}

	if ((row & position) == 0)
		return qfalse;
	else
		return qtrue;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape The shape to check in. (8x4)
 * @param[in] x
 * @param[in] y
 */
static qboolean Com_CheckShapeSmall (const uint32_t shape, const int x, const int y)
{
	if (y >= SHAPE_BIG_MAX_HEIGHT || x >= SHAPE_BIG_MAX_WIDTH || x < 0 || y < 0) {
		Com_Printf("Com_CheckShapeSmall: Bad x or y value: (x=%i, y=%i)\n", x, y);
		return qfalse;
	}

	return shape & (0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x));
}

/**
 * @brief Counts the used bits in a shape (item shape).
 * @param[in] shape The shape to count the bits in.
 * @return Number of bits.
 * @note Used to calculate the real field usage in the inventory
 */
int Com_ShapeUsage (const uint32_t shape)
{
 	int bitCounter = 0;
	int i;

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT * SHAPE_SMALL_MAX_WIDTH; i++)
		if (shape & (1 << i))
			bitCounter++;

	return bitCounter;
}

/**
 * @brief Sets one bit in a shape to true/1
 * @note Only works for V_SHAPE_SMALL!
 * If the bit is already set the shape is not changed.
 * @param[in] shape The shape to modify. (8x4)
 * @param[in] x The x (width) position of the bit to set.
 * @param[in] y The y (height) position of the bit to set.
 * @return The new shape.
 */
static uint32_t Com_ShapeSetBit (uint32_t shape, const int x, const int y)
{
	if (x >= SHAPE_SMALL_MAX_WIDTH || y >= SHAPE_SMALL_MAX_HEIGHT || x < 0 || y < 0) {
		Com_Printf("Com_ShapeSetBit: Bad x or y value: (x=%i, y=%i)\n", x,y);
		return shape;
	}

	shape |= 0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x);
	return shape;
}


/**
 * @brief Rotates a shape definition 90 degree to the left (CCW)
 * @note Only works for V_SHAPE_SMALL!
 * @param[in] shape The shape to rotate.
 * @return The new shape.
 */
uint32_t Com_ShapeRotate (const uint32_t shape)
{
	int h, w;
	uint32_t shapeNew = 0;
	int maxWidth = -1;

	for (w = SHAPE_SMALL_MAX_WIDTH - 1; w >= 0; w--) {
		for (h = 0; h < SHAPE_SMALL_MAX_HEIGHT; h++) {
			if (Com_CheckShapeSmall(shape, w, h)) {
				if (w >= SHAPE_SMALL_MAX_HEIGHT) {
					/* Object can't be rotated (code-wise), it is longer than SHAPE_SMALL_MAX_HEIGHT allows. */
					return shape;
				}

				if (maxWidth < 0)
					maxWidth = w;

				shapeNew = Com_ShapeSetBit(shapeNew, h, maxWidth - w);
			}
		}
	}

	return shapeNew;
}
