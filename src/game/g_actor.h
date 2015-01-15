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

#define G_IsState(ent, s)		((ent)->state & (s))
#define G_IsStunned(ent)		(G_IsState(ent, STATE_STUN) & ~STATE_DEAD)
#define G_IsPanicked(ent)		G_IsState(ent, STATE_PANIC)
#define G_IsCrouched(ent)		G_IsState(ent, STATE_CROUCHED)
/** @note This check also includes the IsStunned check - see the STATE_* bitmasks */
#define G_IsDead(ent)			G_IsState(ent, STATE_DEAD)

#define G_SetState(ent, s)		(ent)->state |= (s)
#define G_SetStunned(ent)		G_SetState((ent), STATE_STUN)
#define G_SetDead(ent)			G_SetState((ent), STATE_DEAD)
#define G_SetCrouched(ent)		G_SetState((ent), STATE_CROUCHED)

#define G_ToggleState(ent, s)	(ent)->state ^= (s)
#define G_ToggleCrouched(ent)	G_ToggleState(ent, STATE_CROUCHED)

bool G_IsLivingActor(const Edict* ent) __attribute__((nonnull));
void G_ActorUseDoor(Actor* actor, Edict* door);
void G_ActorSetClientAction(Edict* actor, Edict* ent);
int G_ActorUsableTUs(const Edict* ent);
int G_ActorGetTUForReactionFire(const Edict* ent);
void G_ActorReserveTUs(Edict* ent, int resReaction, int resShot, int resCrouch);
int G_ActorDoTurn(Edict* ent, byte dir);
void G_ActorSetMaxs(Actor* actor);
int G_ActorCalculateMaxTU(const Edict* ent);
void G_ActorGiveTimeUnits(Actor* actor);
void G_ActorSetTU(Edict* ent, int tus);
void G_ActorUseTU(Edict* ent, int tus);
void G_ActorModifyCounters(const Edict* attacker, const Edict* victim, int deltaAlive, int deltaKills, int deltaStuns);
void G_ActorGetEyeVector(const Edict* actor, vec3_t eye);
void G_ActorCheckRevitalise(Actor* actor);
bool G_ActorDieOrStun(Actor* actor, Edict* attacker);
int G_ActorGetContentFlags(const vec3_t origin);
bool G_ActorInvMove(Actor* actor, const invDef_t* from, Item* fItem, const invDef_t* to, int tx, int ty, bool checkaction);
bool G_ActorReload(Actor* actor, const invDef_t* invDef);
int G_ActorGetModifiedTimeForFiredef(const Edict* ent, const fireDef_t* const fd, bool reaction);
