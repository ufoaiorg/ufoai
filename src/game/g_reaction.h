/**
 * @file
 * @brief Reaction fire system
 *
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

void G_ReactionFirePreShot(const Edict* target, const int fdTime);
void G_ReactionFirePostShot(Edict* target);
void G_ReactionFireReset(int team);
void G_ReactionFireNofityClientStartMove(const Edict* target);
void G_ReactionFireNofityClientEndMove(const Edict* target);
void G_ReactionFireSettingsUpdate(Actor* actor, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t* od);
void G_ReactionFireNofityClientRFAborted(const Edict* shooter, const Edict* target, int step);
bool G_ReactionFireSettingsReserveTUs(Edict* ent);
bool G_ReactionFireOnMovement(Edict* target, int step);
void G_ReactionFireOnEndTurn(void);
void G_ReactionFireOnDead(const Edict* target);
void G_ReactionFireTargetsInit(void);
void G_ReactionFireTargetsCreate(const Edict* shooter);
