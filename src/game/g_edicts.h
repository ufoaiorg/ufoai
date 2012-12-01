/**
 * @file
 * @brief functions to handle the storage and lifecycle of all edicts in the game module.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_utils.c
Copyright (C) 1997-2001 Id Software, Inc.

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

edict_t* G_EdictsInit(void);
void G_EdictsReset(void);
edict_t* G_EdictsGetNewEdict(void);
edict_t* G_EdictDuplicate(const edict_t *edict);
int G_EdictsGetNumber(const edict_t *ent);
bool G_EdictsIsValidNum(const int idx);
edict_t* G_EdictsGetByNum(const int num);
edict_t* G_EdictsGetFirst(void);
edict_t* G_EdictsGetNext(edict_t *lastEnt);
edict_t* G_EdictsGetNextInUse(edict_t *lastEnt);
edict_t* G_EdictsGetNextActor(edict_t *lastEnt);
edict_t* G_EdictsGetNextLivingActor(edict_t *lastEnt);
edict_t* G_EdictsGetNextLivingActorOfTeam(edict_t *lastEnt, const int team);
edict_t* G_EdictsGetTriggerNextMaps(edict_t *lastEnt);
