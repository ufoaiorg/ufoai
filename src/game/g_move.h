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

void G_MoveCalc(int team, const Actor* movingActor, const pos3_t from, int distance);
void G_MoveCalcLocal(pathing_t* pt, int team, const Edict* movingActor, const pos3_t from, int distance);
bool G_FindPath(int team, const Edict* movingActor, const pos3_t from, const pos3_t targetPos, bool crouched = false, int maxTUs = 999999);
void G_ActorFall(Edict* ent);
int G_FillDirectionTable(dvec_t* dvtab, size_t dvtabSize, byte crouchingState, pos3_t pos);
pos_t G_ActorMoveLength(const Actor* actor, const pathing_t* path, const pos3_t to, bool stored);
void G_ClientMove(const Player& player, int visTeam, Actor* actor, const pos3_t to);
