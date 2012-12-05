/**
 * @file
 * @brief Artificial Intelligence functions.
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

#pragma once

#include "g_local.h"

/*
 * Shared functions (between C AI and LUA AI)
 */
void AI_TurnIntoDirection(edict_t *ent, const pos3_t pos);
bool AI_FindHidingLocation(int team, edict_t *ent, const pos3_t from, int *tuLeft);
bool AI_FindHerdLocation(edict_t *ent, const pos3_t from, const vec3_t target, int tu);
int AI_GetHidingTeam(const edict_t *ent);
const item_t *AI_GetItemForShootType(shoot_types_t shootType, const edict_t *ent);

/*
 * LUA functions
 */
void AIL_ActorThink(player_t * player, edict_t * ent);
int AIL_InitActor(edict_t * ent, const char *type, const char *subtype);
void AIL_Cleanup(void);
void AIL_Init(void);
void AIL_Shutdown(void);
