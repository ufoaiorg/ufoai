/**
 * @file
 * @brief Reaction fire system
 *
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

void G_ReactionFirePreShot(const Actor* target, const int fdTime);
void G_ReactionFirePostShot(Actor* target);
void G_ReactionFireReset(int team);
void G_ReactionFireNotifyClientStartMove(const Actor* target);
void G_ReactionFireNotifyClientEndMove(const Actor* target);
void G_ReactionFireSettingsUpdate(Actor* actor, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t* od);
void G_ReactionFireNotifyClientRFAborted(const Actor* shooter, const Edict* target, int step);
bool G_ReactionFireSettingsReserveTUs(Actor* ent);
bool G_ReactionFireOnMovement(Actor* target, int step);
void G_ReactionFireOnEndTurn(void);
void G_ReactionFireOnDead(const Actor* target);
void G_ReactionFireTargetsInit(void);
void G_ReactionFireTargetsCreate(const Edict* shooter);
