/**
 * @file
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

#pragma once

#include "g_local.h"

const equipDef_t* G_GetEquipDefByID(const char* equipID);
Edict* G_GetFloorItemFromPos(const pos3_t pos);
Edict* G_GetFloorItems(Edict* ent) __attribute__((nonnull));
bool G_InventoryRemoveItemByID(const char* itemID, Edict* ent, containerIndex_t index);
bool G_AddItemToFloor(const pos3_t pos, const char* itemID);
void G_InventoryToFloor(Edict* ent);
void G_ReadItem(Item* item, const invDef_t** container, int* x, int* y);
void G_WriteItem(const Item& item, const containerIndex_t contId, int x, int y);
void G_SendInventory(playermask_t player_mask, const Edict& ent);
