/**
 * @file
 * @brief functions to handle the storage and lifecycle of all edicts in the game module.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

Edict* G_EdictsConstruct(void);
void G_EdictsInit(void);
Edict* G_EdictsGetNewEdict(void);
Edict* G_EdictDuplicate(const Edict* edict);
int G_EdictsGetNumber(const Edict* ent);
bool G_EdictsIsValidNum(const int idx);
Edict* G_EdictsGetByNum(const int num);
Edict* G_EdictsGetFirst(void);
Edict* G_EdictsGetNext(Edict* lastEnt);
Edict* G_EdictsGetNextInUse(Edict* lastEnt);
Actor* G_EdictsGetNextActor(Actor* lastEnt);
Actor* G_EdictsGetNextLivingActor(Actor* lastEnt);
Actor* G_EdictsGetNextLivingActorOfTeam(Actor* lastEnt, const int team);
Actor* G_EdictsGetActorByUCN(const int ucn, const int team);
Edict* G_EdictsGetTriggerNextMaps(Edict* lastEnt);
Actor* G_EdictsGetLivingActorFromPos(const pos3_t pos);
Edict* G_EdictsFindTargetEntity(const char* target);
void G_EdictsThink(void);
