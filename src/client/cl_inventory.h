/**
 * @file cl_inventory.h
 * @brief Header file for inventory handling and Equipment menu.
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

#ifndef CLIENT_CL_INVENTORY_H
#define CLIENT_CL_INVENTORY_H

/** Size of a UGV in hangar capacity */
#define UGV_SIZE 300

qboolean INV_MoveItem(inventory_t* inv, const invDef_t * toContainer, int px, int py, const invDef_t * fromContainer, invList_t *fItem);
void INV_LoadWeapon(invList_t *weapon, inventory_t *inv, const invDef_t *srcContainer, const invDef_t *destContainer);
void INV_UnloadWeapon(invList_t *weapon, inventory_t *inv, const invDef_t *container);
equipDef_t *INV_GetEquipmentDefinitionByID(const char *name);
void INV_InitStartup(void);
void INV_ItemDescription(const objDef_t *od);
qboolean INV_EquipmentDefSanityCheck(void);

#endif /* CLIENT_CL_INVENTORY_H */
