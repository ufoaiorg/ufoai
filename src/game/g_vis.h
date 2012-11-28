/**
 * @file
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

bool G_FrustumVis(const edict_t *from, const vec3_t point);
float G_ActorVis(const vec3_t from, const edict_t *ent, const edict_t *check, bool full);
void G_VisFlagsClear(int team);
void G_VisFlagsAdd(edict_t *ent, vismask_t visMask);
void G_VisFlagsSwap(edict_t *ent, vismask_t visMask);
void G_VisFlagsReset(edict_t *ent);
void G_VisMakeEverythingVisible(void);
int G_CheckVis(edict_t *check, const int visFlags = VT_PERISH);
int G_CheckVisPlayer(player_t* player, int visFlags);
int G_TestVis(const int team, edict_t * check, const int flags);
bool G_Vis(const int team, const edict_t * from, const edict_t * check, const int flags);
int G_VisCheckDist(const edict_t *const ent);
