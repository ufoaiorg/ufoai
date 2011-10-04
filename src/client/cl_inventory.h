/**
 * @file cl_inventory.h
 * @brief Header file for inventory handling and Equipment menu.
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

#ifndef CLIENT_CL_INVENTORY_H
#define CLIENT_CL_INVENTORY_H

/**
 * @brief A list of filter types in the market and production view.
 * @note Run-time only, please do not use this in savegame/structures and the like.
 * Please also do not use hardcoded numbers to access this (e.g. in a .ufo script).
 * @sa inv_shared.c:INV_ItemMatchesFilter
 * @sa inv_shared.c:INV_GetFilterTypeID
 */
typedef enum {
	/* All types starting with "FILTER_S_" contain items that can used on/with soldiers (i.e. personal equipment). */
	FILTER_S_PRIMARY,		/**< All 'Primary' weapons and their ammo for soldiers (Except for heavy weapons). */
	FILTER_S_SECONDARY,		/**< All 'Secondary' weapons and their ammo for soldiers. */
	FILTER_S_HEAVY,			/**< Heavy weapons for soldiers. */
	FILTER_S_MISC,			/**< Misc. soldier equipment (i.e. everything else that is not in the other soldier-item filters) */
	FILTER_S_ARMOUR,		/**< Armour for soldiers. */
	MAX_SOLDIER_FILTERTYPES,

	/* Non-soldier items */
	FILTER_CRAFTITEM,	/**< Aircraft equipment. */
	FILTER_UGVITEM,	/**< Heavy equipment like tanks (i.e. these are actually employees) and their parts.
					 * Some of the content are special normal items (like for soldiers).
					 * The UGVs themself are specially handled.*/
	FILTER_AIRCRAFT,	/**< Aircrafts. */
	FILTER_DUMMY,		/**< Everything that is not in _any_ of the other filter types.
					 * Mostly plot-relevant stuff, unproducible stuff and stuff. */
	FILTER_DISASSEMBLY,

	MAX_FILTERTYPES,

	FILTER_ENSURE_32BIT = 0x7FFFFFFF
} itemFilterTypes_t;

qboolean INV_MoveItem(inventory_t* inv, const invDef_t * toContainer, int px, int py, const invDef_t * fromContainer, invList_t *fItem);
qboolean INV_LoadWeapon(const invList_t *weapon, inventory_t *inv, const invDef_t *srcContainer, const invDef_t *destContainer);
qboolean INV_UnloadWeapon(invList_t *weapon, inventory_t *inv, const invDef_t *container);
const equipDef_t *INV_GetEquipmentDefinitionByID(const char *name);
void INV_InitStartup(void);
itemFilterTypes_t INV_GetFilterFromItem(const objDef_t *obj);
const char *INV_GetFilterType(const int id);
itemFilterTypes_t INV_GetFilterTypeID(const char * filterTypeID);
qboolean INV_ItemMatchesFilter(const objDef_t *obj, const itemFilterTypes_t filterType);
invList_t *INV_SearchInInventoryWithFilter(const inventory_t* const i, const invDef_t * container, const objDef_t *item,  const itemFilterTypes_t filterType) __attribute__((nonnull(1)));
void INV_ItemDescription(const objDef_t *od);

#endif /* CLIENT_CL_INVENTORY_H */
