/**
 * @file
 * @brief All parts of the main game logic that are combat related
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

int G_ApplyProtection(const edict_t *target, const byte dmgWeight, int damage);
void G_CheckDeathOrKnockout(edict_t *target, edict_t *attacker, const fireDef_t *fd, int damage);
void G_GetShotOrigin(const edict_t *shooter, const fireDef_t *fd, const vec3_t dir, vec3_t shotOrigin);
bool G_ClientShoot(const player_t *player, edict_t *ent, const pos3_t at, shoot_types_t shootType, fireDefIndex_t firemode, shot_mock_t *mock, bool allowReaction, int z_align);
