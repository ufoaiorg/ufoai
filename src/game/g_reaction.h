/**
 * @file
 * @brief Reaction fire system
 *
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

void G_ReactionFirePreShot(const edict_t *target, const int fdTime);
void G_ReactionFirePostShot(edict_t *target);
void G_ReactionFireReset(int team);
void G_ReactionFireUpdate(edict_t *ent, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t *od);
bool G_ReactionFireSettingsReserveTUs(edict_t *ent);
bool G_ReactionFireOnMovement(edict_t *target);
void G_ReactionFireOnEndTurn(void);
void G_ReactionFireTargetsInit (void);
void G_ReactionFireTargetsCreate (const edict_t *shooter);
