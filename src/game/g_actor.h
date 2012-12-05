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

bool G_IsLivingActor(const edict_t *ent) __attribute__((nonnull));
void G_ActorUseDoor(edict_t *actor, edict_t *door);
bool G_ActorIsInRescueZone(const edict_t *actor);
void G_ActorSetInRescueZone(edict_t *actor, bool inRescueZone);
void G_ActorSetClientAction(edict_t *actor, edict_t *ent);
int G_ActorGetReservedTUs(const edict_t *ent);
int G_ActorUsableTUs(const edict_t *ent);
int G_ActorGetTUForReactionFire(const edict_t *ent);
void G_ActorReserveTUs(edict_t *ent, int resReaction, int resShot, int resCrouch);
edict_t *G_ActorGetByUCN(const int ucn, const int team);
int G_ActorDoTurn(edict_t *ent, byte dir);
void G_ActorSetMaxs(edict_t *ent);
int G_ActorCalculateMaxTU(const edict_t *ent);
void G_ActorGiveTimeUnits(edict_t *ent);
void G_ActorSetTU(edict_t *ent, int tus);
void G_ActorUseTU(edict_t *ent, int tus);
void G_ActorModifyCounters(const edict_t *attacker, const edict_t *victim, int deltaAlive, int deltaKills, int deltaStuns);
void G_ActorGetEyeVector(const edict_t *actor, vec3_t eye);
void G_ActorCheckRevitalise(edict_t *ent);
bool G_ActorDieOrStun(edict_t *ent, edict_t *attacker);
int G_ActorGetContentFlags(const vec3_t origin);
bool G_ActorInvMove(edict_t *ent, const invDef_t *from, invList_t *fItem, const invDef_t *to, int tx, int ty, bool checkaction);
void G_ActorReload(edict_t *ent, const invDef_t *invDef);
int G_GetActorTimeForFiredef(const edict_t *ent, const fireDef_t *const fd, bool reaction);
