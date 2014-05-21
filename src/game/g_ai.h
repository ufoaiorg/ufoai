/**
 * @file
 * @brief Artificial Intelligence functions.
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

/*
 * AI functions
 */
void AI_Init(void);
void AI_CheckRespawn(int team);
extern Edict* ai_waypointList;
void G_AddToWayPointList(Edict* ent);
void AI_Run(void);
void AI_ActorThink(Player& player, Actor* actor);
Player* AI_CreatePlayer(int team);
bool AI_CheckUsingDoor(const Edict* ent, const Edict* door);

/*
 * Shared functions (between C AI and LUA AI)
 */
void AI_TurnIntoDirection(Actor* actor, const pos3_t pos);
bool AI_FindHidingLocation(int team, Actor* actor, const pos3_t from, int tuLeft);
bool AI_FindHerdLocation(Actor* actor, const pos3_t from, const vec3_t target, int tu);
int AI_GetHidingTeam(const Edict* ent);
const Item* AI_GetItemForShootType(shoot_types_t shootType, const Edict* ent);
bool AI_FighterCheckShoot(const Actor* actor, const Edict* check, const fireDef_t* fd, float* dist);
bool AI_CheckLineOfFire(const Edict* shooter, const Edict* target, const fireDef_t* fd, int shots);
float AI_CalcShotDamage(Actor* actor, Actor* target, const fireDef_t* fd, shoot_types_t shotType);

/*
 * LUA functions
 */
void AIL_ActorThink(Player& player, Actor* actor);
int AIL_InitActor(Edict* ent, const char* type, const char* subtype);
void AIL_Cleanup(void);
void AIL_Init(void);
void AIL_Shutdown(void);
