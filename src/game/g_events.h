/**
 * @file
 * @brief Event related headers
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_local.h
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

#ifndef GAME_G_EVENTS_H
#define GAME_G_EVENTS_H

#include "g_vis.h"

/* A bit mask. One bit for each affected player. */
typedef unsigned int playermask_t;
/* Bitmask for all players */
#define PM_ALL			0xFFFFFFFF
#define G_PlayerToPM(player) ((player)->num < game.sv_maxplayersperteam ? 1 << ((player)->num) : 0)

void G_EventActorAdd(playermask_t playerMask, const edict_t *ent);
void G_EventActorAppear(playermask_t playerMask, const edict_t *check, const edict_t *ent);
void G_EventActorDie(const edict_t* ent, bool attacker);
void G_EventActorFall(const edict_t* ent);
void G_EventActorRevitalise(const edict_t* ent);
void G_EventActorSendReservations(const edict_t *ent);
void G_EventActorStateChange(playermask_t playerMask, const edict_t *ent);
void G_EventActorStats(const edict_t* ent, playermask_t playerMask);
void G_EventActorTurn(const edict_t* ent);
void G_EventAddBrushModel(playermask_t playerMask, const edict_t *ent);
void G_EventCenterView(const edict_t *ent);
void G_EventCenterViewAt(playermask_t playerMask, const pos3_t pos);
void G_EventDoorClose(const edict_t *door);
void G_EventDoorOpen(const edict_t *door);
void G_EventDestroyEdict(const edict_t* ent);
void G_EventEdictAppear(playermask_t playerMask, const edict_t *ent);
void G_EventEdictPerish(playermask_t playerMask, const edict_t *ent);
void G_EventCameraAppear(playermask_t playerMask, const edict_t *ent);
void G_EventEndRound(void);
void G_EventEndRoundAnnounce(const player_t *player);
void G_EventInventoryAdd(const edict_t* ent, playermask_t playerMask, int itemAmount);
void G_EventInventoryAmmo(const edict_t* ent, const objDef_t* ammo, int amount, shoot_types_t shootType);
void G_EventInventoryDelete(const edict_t* ent, playermask_t playerMask, const invDef_t* invDef, int x, int y);
void G_EventInventoryReload(const edict_t* ent, playermask_t playerMask, const item_t* item, const invDef_t* invDef, const invList_t* ic);
void G_EventModelExplodeTriggered(const edict_t *ent);
void G_EventModelExplode(const edict_t *ent);
void G_EventParticleSpawn(playermask_t playerMask, const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a);
void G_EventPerish(const edict_t* ent);
void G_EventReactionFireChange(const edict_t* ent);
void G_EventReset(const player_t *player, int activeTeam);
void G_EventResetClientAction(const edict_t* ent);
void G_EventSendEdict(const edict_t *ent);
void G_EventSendParticle(playermask_t playerMask, const edict_t *ent);
void G_EventSendState(playermask_t playerMask, const edict_t *ent);
void G_EventSetClientAction(const edict_t *ent);
void G_EventShootHidden(teammask_t teamMask, const fireDef_t* fd, bool firstShoot);
void G_EventShoot(const edict_t* ent, teammask_t teamMask, const fireDef_t* fd, bool firstShoot, shoot_types_t shootType, int flags, const trace_t* trace, const vec3_t from, const vec3_t impact);
void G_EventSpawnSound(playermask_t playerMask, bool instant, const edict_t* ent, const vec3_t origin, const char *sound);
void G_EventStart(const player_t *player, bool teamplay);
void G_EventStartShoot(const edict_t* ent, teammask_t teamMask, shoot_types_t shootType, const pos3_t at);
void G_EventThrow(teammask_t teamMask, const fireDef_t *fd, float dt, byte flags, const vec3_t position, const vec3_t velocity);
void G_EventAdd(playermask_t playerMask, int eType, int entnum);
void G_EventEnd(void);
void G_EventActorWound(const edict_t *ent, const int bodyPart);

#endif
