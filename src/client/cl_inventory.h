/**
 * @file cl_inventory.h
 * @brief Header file for inventory handling and Equipment menu.
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

#ifndef CLIENT_CL_INVENTORY_H
#define CLIENT_CL_INVENTORY_H

#define MAX_ASSEMBLIES	16
#define MAX_COMP	32
#define ANTIMATTER_SIZE 10

/**
 * @brief The definition of a "components" entry (i.e. an assembly of several items) parsed from a ufo-file.
 * @sa INV_ParseComponents
 */
typedef struct components_s {
	char asId[MAX_VAR];	/**< The name of the assemly (i.e. the UFO) */
	objDef_t *asItem;	/**< Index of object (that is an assembly) in csi.ods.*/

	int time;	/**< The time (in hours) until the disassembly is finished. */

	int numItemtypes;				/**< Number of item-types listed below. (max is MAX_COMP) */
	objDef_t *items[MAX_COMP];		/**< List of parts (item-types). */
	int item_amount[MAX_COMP];		/**< How many items of this type are in this assembly. */
	int item_amount2[MAX_COMP];		/**< How many items of this type are in this assembly when it crashed (max-value?). */
} components_t;

/** Size of a UGV in hangar capacity */
#define UGV_SIZE 300

extern cvar_t *cl_equip;

void INV_ParseComponents(const char *name, const char **text);
void INV_CarriedItems(const le_t *soldier);
components_t *INV_GetComponentsByItem(const objDef_t *item);
qboolean INV_ItemsIsStoredInStorage(const objDef_t *obj);
qboolean INV_MoveItem(inventory_t* inv, const invDef_t * toContainer, int px, int py, const invDef_t * fromContainer, invList_t *fItem);
equipDef_t *INV_GetEquipmentDefinitionByID(const char *name);
void INV_InitStartup(void);

qboolean INV_ItemsSanityCheck(void);
qboolean INV_EquipmentDefSanityCheck(void);

#endif /* CLIENT_CL_INVENTORY_H */
