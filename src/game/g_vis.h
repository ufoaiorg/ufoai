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

/** @todo make this a byte */
/* A bit mask. One bit for each affected team. */
typedef unsigned int teammask_t;

#define VIS_APPEAR	1
#define VIS_PERISH	2
#define VIS_NEW		4

/** the visibility changed - if it was visible - it's (the edict) now invisible */
#define VIS_CHANGE	1
/** actor visible? */
#define VIS_YES		2
/** stop the current action if actor appears */
#define VIS_STOP	4

/* A bit mask. Modifiers for the way visibility check are performed. */
typedef unsigned int vischeckflags_t;
/* possible values are: */
/** check whether edict is still visible - it maybe is currently visible but this
 * might have changed due to some action */
#define VT_PERISH		1
/** don't perform a frustum vis check via G_FrustumVis in G_Vis */
#define VT_NOFRUSTUM	2

bool G_FrustumVis(const edict_t *from, const vec3_t point);
float G_ActorVis(const vec3_t from, const edict_t *ent, const edict_t *check, bool full);
void G_VisFlagsClear(int team);
void G_VisFlagsAdd(edict_t *ent, teammask_t teamMask);
void G_VisFlagsSwap(edict_t *ent, teammask_t teamMask);
void G_VisFlagsReset(edict_t *ent);
void G_VisMakeEverythingVisible(void);
int G_CheckVis(edict_t *check, const vischeckflags_t visFlags = VT_PERISH);
void G_CheckVisPlayer(player_t* player, const vischeckflags_t visFlags);
int G_TestVis(const int team, edict_t * check, const vischeckflags_t flags);
bool G_Vis(const int team, const edict_t * from, const edict_t * check, const vischeckflags_t flags);
int G_VisCheckDist(const edict_t *const ent);
