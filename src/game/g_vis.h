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

/** @todo make this a byte */
/* A bit mask. One bit for each affected team. */
typedef unsigned int teammask_t;
/* Bitmask for all teams */
#define TEAM_ALL		0xFFFFFFFF

/* Visibility changes -- see G_DoTestVis */
/** Edict became visible */
#define VIS_APPEAR	1
/** Edict became invisible */
#define VIS_PERISH	2
/** stop the current action if actor appears */
#define VIS_STOP	4

/* Visibility state -- see G_TestVis */
/** the visibility changed - (invisible to visible or vice versa) */
#define VS_CHANGE	1
/** actor is visible */
#define VS_YES		2

/* A bit mask. Modifiers for the way visibility check are performed. */
typedef unsigned int vischeckflags_t;
/* possible values are: */
/** check whether edict is still visible - it maybe is currently visible but this
 * might have changed due to some action */
#define VT_PERISHCHK	1
/** don't perform a frustum vis check via G_FrustumVis in G_Vis */
#define VT_NOFRUSTUM	2
/** If the actor doesn't become visible add it as a hidden actor
 * (sends the EV_ACTOR_ADD event) */
#define VT_NEW			4

/** Actor visibility constants */
#define ACTOR_VIS_100	1.0
#define ACTOR_VIS_50	0.5
#define ACTOR_VIS_10	0.1
#define ACTOR_VIS_0		0.0

bool G_FrustumVis(const Edict* from, const vec3_t point);
float G_ActorVis(const Edict* ent, const Edict* check, bool full);
void G_VisFlagsClear(int team);
void G_VisFlagsAdd(Edict& ent, teammask_t teamMask);
void G_VisFlagsSwap(Edict& ent, teammask_t teamMask);
void G_VisFlagsReset(Edict& ent);
void G_VisMakeEverythingVisible(void);
void G_CheckVis(Edict* check, const vischeckflags_t visFlags = VT_PERISHCHK);
void G_CheckVisPlayer(Player& player, const vischeckflags_t visFlags);
int G_CheckVisTeamAll(const int team, const vischeckflags_t visFlags, const Edict* ent);
int G_TestVis(const int team, Edict* check, const vischeckflags_t flags);
bool G_Vis(const int team, const Edict* from, const Edict* check, const vischeckflags_t flags);
int G_VisCheckDist(const Edict* const ent);
