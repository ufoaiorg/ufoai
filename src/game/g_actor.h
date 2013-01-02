/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#define G_IsState(ent, s)		((ent)->state & (s))
#define G_IsShaken(ent)			G_IsState(ent, STATE_SHAKEN)
#define G_IsStunned(ent)		(G_IsState(ent, STATE_STUN) & ~STATE_DEAD)
#define G_IsPanicked(ent)		G_IsState(ent, STATE_PANIC)
#define G_IsReaction(ent)		G_IsState(ent, STATE_REACTION)
#define G_IsRaged(ent)			G_IsState(ent, STATE_RAGE)
#define G_IsInsane(ent)			G_IsState(ent, STATE_INSANE)
#define G_IsDazed(ent)			G_IsState(ent, STATE_DAZED)
#define G_IsCrouched(ent)		G_IsState(ent, STATE_CROUCHED)
/** @note This check also includes the IsStunned check - see the STATE_* bitmasks */
#define G_IsDead(ent)			G_IsState(ent, STATE_DEAD)

#define G_SetState(ent, s)		(ent)->state |= (s)
#define G_SetShaken(ent)		G_SetState((ent), STATE_SHAKEN)
#define G_SetDazed(ent)			G_SetState((ent), STATE_DAZED)
#define G_SetStunned(ent)		G_SetState((ent), STATE_STUN)
#define G_SetDead(ent)			G_SetState((ent), STATE_DEAD)
#define G_SetInsane(ent)		G_SetState((ent), STATE_INSANE)
#define G_SetRage(ent)			G_SetState((ent), STATE_RAGE)
#define G_SetPanic(ent)			G_SetState((ent), STATE_PANIC)
#define G_SetCrouched(ent)		G_SetState((ent), STATE_CROUCHED)

#define G_RemoveState(ent, s)	(ent)->state &= ~(s)
#define G_RemoveShaken(ent)		G_RemoveState((ent), STATE_SHAKEN)
#define G_RemoveDazed(ent)		G_RemoveState((ent), STATE_DAZED)
#define G_RemoveStunned(ent)	G_RemoveState((ent), STATE_STUN)
#define G_RemoveDead(ent)		G_RemoveState((ent), STATE_DEAD)
#define G_RemoveInsane(ent)		G_RemoveState((ent), STATE_INSANE)
#define G_RemoveRage(ent)		G_RemoveState((ent), STATE_RAGE)
#define G_RemovePanic(ent)		G_RemoveState((ent), STATE_PANIC)
#define G_RemoveCrouched(ent)	G_RemoveState((ent), STATE_CROUCHED)
#define G_RemoveReaction(ent)	G_RemoveState((ent), STATE_REACTION)

#define G_ToggleState(ent, s)	(ent)->state ^= (s)
#define G_ToggleCrouched(ent)	G_ToggleState(ent, STATE_CROUCHED)

bool G_IsLivingActor(const edict_t *ent) __attribute__((nonnull));
void G_ActorUseDoor(edict_t *actor, edict_t *door);
bool G_ActorIsInRescueZone(const edict_t *actor);
void G_ActorSetInRescueZone(edict_t *actor, bool inRescueZone);
void G_ActorSetClientAction(edict_t *actor, edict_t *ent);
int G_ActorGetReservedTUs(const edict_t *ent);
int G_ActorUsableTUs(const edict_t *ent);
int G_ActorGetTUForReactionFire(const edict_t *ent);
void G_ActorReserveTUs(edict_t *ent, int resReaction, int resShot, int resCrouch);
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
int G_ActorGetTimeForFiredef(const edict_t *ent, const fireDef_t *const fd, bool reaction);
