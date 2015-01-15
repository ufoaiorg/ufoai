/**
 * @file
 * @brief Brings new objects into the world.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_spawn.c
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

void G_SpawnEntities(const char* mapname, bool day, const char* entities);
Edict* G_Spawn(const char* classname = nullptr);
void G_SpawnSmokeField(const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius);
void G_SpawnFireField(const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius);
void G_SpawnStunSmokeField(const vec3_t vec, const char* particle, int rounds, int damage, vec_t radius);
Edict* G_SpawnFloor(const pos3_t pos);
Edict* G_SpawnParticle(const vec3_t origin, int spawnflags, const char* particle);
