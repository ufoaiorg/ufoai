/**
 * @file g_events.c
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

#include "g_local.h"

/**
 * @brief Spawns a sound (that will be spatialized on the client side)
 * @param ent The edict that is causing the sound
 * @param origin The origin of the sound
 * @param sound The sound file, path relative to sounds/. If there is a + at the end the client will
 * choose a random sound. See the event function for more information.
 * of the path, a random sound will be taken.
 */
void G_EventSpawnSound (unsigned int playerMask, qboolean instant, const edict_t* ent, const vec3_t origin, const char *sound)
{
	gi.AddEvent(playerMask, EV_SOUND | (instant ? EVENT_INSTANTLY : 0));
	gi.WriteShort(ent->number);

	/* use the entity origin unless it is a bmodel or explicitly specified */
	if (!origin) {
		if (ent->solid == SOLID_BSP) {
			vec3_t origin_v;
			VectorCenterFromMinsMaxs(ent->mins, ent->maxs, origin_v);
			VectorAdd(ent->origin, origin_v, origin_v);
			gi.WritePos(origin);
		} else {
			gi.WritePos(vec3_origin);
		}
	} else {
		gi.WritePos(origin);
	}
	gi.WriteString(sound);
	gi.EndEvents();
}

/**
 * @brief Send the turn event for the given entity
 * @param ent The entity to send the turn event for
 * @note Every player that can see this ent will reveive the turn event data
 * @note Make sure that the direction to turn into is already set
 */
void G_EventActorTurn (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->dir);
	gi.EndEvents();
}

/**
 * @brief Announce the actor die event for the clients that are seeing the actor
 * @param[in] ent The actor that is dying
 */
void G_EventActorDie (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_DIE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);
	gi.WriteByte(ent->pnum);
	gi.EndEvents();
}

/**
 * @brief Announce the actor die event for the clients that are seeing the actor
 * @param[in] ent The actor that was healed and woke up again
 */
void G_EventActorRevitalise (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_REVITALISED);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);
	gi.EndEvents();
}

/**
 * @brief Will inform the player about the real TU reservation
 * @param ent The actors edict.
 */
void G_EventActorSendReservations (const edict_t *ent)
{
	gi.AddEvent(G_PlayerToPM(G_PLAYER_FROM_ENT(ent)), EV_ACTOR_RESERVATIONCHANGE);

	gi.WriteShort(ent->number);
	gi.WriteShort(ent->chr.reservedTus.reaction);
	gi.WriteShort(ent->chr.reservedTus.shot);
	gi.WriteShort(ent->chr.reservedTus.crouch);

	gi.EndEvents();
}

/**
 * @brief Tell the client to remove the item from the container
 * @param[in] ent Pointer to entity having given inventory.
 * @param[in] playerMask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 * @param[in] invDef Pointer to inventory definition having given container.
 * @param[in] x Position of item in container.
 * @param[in] y Position of item in container.
 */
void G_EventInventoryDelete (const edict_t* ent, int playerMask, const invDef_t* invDef, int x, int y)
{
	gi.AddEvent(playerMask, EV_INV_DEL);
	gi.WriteShort(ent->number);
	gi.WriteByte(invDef->id);
	gi.WriteByte(x);
	gi.WriteByte(y);
	gi.EndEvents();
}

/**
 * @brief Tell the client to add the item from the container
 * @param[in] ent Pointer to entity having given inventory.
 * @param[in] playerMask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 * @param[in] itemAmount How many items to add.
 * @note This event must be followed by a @c G_WriteItem call
 */
void G_EventInventoryAdd (const edict_t* ent, int playerMask, int itemAmount)
{
	gi.AddEvent(playerMask, EV_INV_ADD);
	gi.WriteShort(ent->number);
	gi.WriteShort(itemAmount);
	/* do not end the pending events here - this is just a header, the items are following */
}

/**
 * @brief Send an event to all clients that are seeing the given edict, that it just has disappeared
 * @param ent The edict that disappeared
 */
void G_EventPerish (const edict_t* ent)
{
	G_EventEdictPerish(G_VisToPM(ent->visflags), ent);
}

/**
 * @brief Unregister an edict at the client
 * @param ent The edict to unregister
 */
void G_EventDestroyEdict (const edict_t* ent)
{
	assert(ent->inuse);
	gi.AddEvent(PM_ALL, EV_ENT_DESTROY);
	gi.WriteShort(ent->number);
	gi.EndEvents();
}

/**
 * @brief Change the amount of available ammo for the given entity
 * @param ent The entity to change the amount of ammo for
 * @param ammo The ammo to change
 * @param amount The new amount of the left ammo
 * @param shootType The shooting type to determine which container to use
 */
void G_EventInventoryAmmo (const edict_t* ent, const objDef_t* ammo, int amount, shoot_types_t shootType)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_AMMO);
	gi.WriteShort(ent->number);
	gi.WriteByte(amount);
	gi.WriteByte(ammo->idx);
	if (IS_SHOT_RIGHT(shootType))
		gi.WriteByte(gi.csi->idRight);
	else
		gi.WriteByte(gi.csi->idLeft);
	/* x and y value */
	gi.WriteByte(0);
	gi.WriteByte(0);
	gi.EndEvents();
}

/**
 * @brief Start the shooting event
 * @param ent The entity that starts the shooting
 * @param visMask the vis mask of the teams to determine the clients from this event is send to
 * @param shootType The type of the shoot
 * @param at The grid position to target to
 */
void G_EventStartShoot (const edict_t* ent, vismask_t visMask, shoot_types_t shootType, const pos3_t at)
{
	gi.AddEvent(G_VisToPM(visMask), EV_ACTOR_START_SHOOT);
	gi.WriteShort(ent->number);
	gi.WriteByte(shootType);
	gi.WriteGPos(ent->pos);
	gi.WriteGPos(at);
	gi.EndEvents();
}

/**
 * @brief Start the shooting event for hidden actors
 * @param visMask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param firstShoot Is this the first shoot
 */
void G_EventShootHidden (vismask_t visMask, const fireDef_t* fd, qboolean firstShoot)
{
	gi.AddEvent(~G_VisToPM(visMask), EV_ACTOR_SHOOT_HIDDEN);
	gi.WriteByte(firstShoot);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.EndEvents();
}

/**
 * @brief Do the shooting
 * @param ent The entity that is doing the shooting
 * @param visMask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param firstShoot Is this the first shoot
 * @param shootType The type of the shoot
 * @param flags Define some flags in a bitmask: @c SF_BODY, @c SF_IMPACT, @c SF_BOUNCING and @c SF_BOUNCING
 * @param trace The trace what was used to determine whether this shot has hit something
 * @param from The position the entity shoots from
 * @param impact The impact world vector for the shot
 */
void G_EventShoot (const edict_t* ent, vismask_t visMask, const fireDef_t* fd, qboolean firstShoot, shoot_types_t shootType, int flags, const trace_t* trace, const vec3_t from, const vec3_t impact)
{
	const edict_t *targetEdict = trace->ent;

	gi.AddEvent(G_VisToPM(visMask), EV_ACTOR_SHOOT);
	gi.WriteShort(ent->number);
	if (targetEdict && G_IsBreakable(targetEdict))
		gi.WriteShort(targetEdict->number);
	else
		gi.WriteShort(SKIP_LOCAL_ENTITY);
	gi.WriteByte(firstShoot ? 1 : 0);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.WriteByte(shootType);
	gi.WriteByte(flags);
	gi.WriteByte(trace->contentFlags);
	gi.WritePos(from);
	gi.WritePos(impact);
	gi.WriteDir(trace->plane.normal);
	gi.EndEvents();
}

void G_EventReactionFireChange (const edict_t* ent)
{
	const objDef_t *od = ent->chr.RFmode.weapon;

	gi.AddEvent(G_PlayerToPM(G_PLAYER_FROM_ENT(ent)), EV_ACTOR_REACTIONFIRECHANGE);

	gi.WriteShort(ent->number);
	gi.WriteByte(ent->chr.RFmode.fmIdx);
	gi.WriteByte(ent->chr.RFmode.hand);
	gi.WriteShort(od ? od->idx : NONE);

	gi.EndEvents();
}

/**
 * @brief Spawn a new particle for the client
 * @param[in] name The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] levelFlags Show at which levels
 * @param[in] s starting/location vector
 * @param[in] v velocity vector
 * @param[in] a acceleration vector
 */
void G_EventParticleSpawn (int playerMask, const char *name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a)
{
	gi.AddEvent(playerMask, EV_PARTICLE_SPAWN);
	gi.WriteByte(levelFlags);
	gi.WritePos(s);
	gi.WritePos(v);
	gi.WritePos(a);
	gi.WriteString(name);
	gi.EndEvents();
}

void G_EventActorFall (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
	gi.WriteShort(ent->number);
	gi.WriteByte(1);
	gi.WriteByte(ent->pos[0]);
	gi.WriteByte(ent->pos[1]);
	gi.WriteByte(ent->pos[2]);
	gi.WriteByte(makeDV(DIRECTION_FALL, ent->pos[2]));
	gi.WriteShort(GRAVITY);
	gi.WriteShort(0);
	gi.EndEvents();
}

/**
 * @brief Informs the client that an interaction with the world is possible
 * @note It's assumed that the clientAction is already set
 * @param[in] ent The edict that can execute the action (an actor)
 */
void G_EventSetClientAction (const edict_t *ent)
{
	assert(ent->clientAction);
	assert(ent->clientAction->flags & FL_CLIENTACTION);

	/* tell the hud to show the door buttons */
	gi.AddEvent(G_TeamToPM(ent->team), EV_CLIENT_ACTION);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->clientAction->number);
	gi.EndEvents();
}

/**
 * @brief Reset the client actions for the given entity
 * @param[in] ent The entity to reset the client action for
 * @note This event is send to the player this edict belongs to
 */
void G_EventResetClientAction (const edict_t* ent)
{
	const int playerMask = G_PlayerToPM(G_PLAYER_FROM_ENT(ent));
	gi.AddEvent(playerMask, EV_RESET_CLIENT_ACTION);
	gi.WriteShort(ent->number);
	gi.EndEvents();
}

void G_EventActorStats (const edict_t* ent, int playerMask)
{
	gi.AddEvent(playerMask, EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);
	gi.EndEvents();
}

/**
 * @brief End of turn event for the current active team
 * @note This event is send to every connected client
 */
void G_EventEndRound (void)
{
	gi.AddEvent(PM_ALL, EV_ENDROUND);
	gi.WriteByte(level.activeTeam);
	gi.EndEvents();
}

void G_EventInventoryReload (const edict_t* ent, int playerMask, const item_t* item, const invDef_t* invDef, const invList_t* ic)
{
	gi.AddEvent(playerMask, EV_INV_RELOAD);
	gi.WriteShort(ent->number);
	gi.WriteByte(item->t->ammo);
	gi.WriteByte(item->m->idx);
	gi.WriteByte(invDef->id);
	gi.WriteByte(ic->x);
	gi.WriteByte(ic->y);
	gi.EndEvents();
}

/**
 * @param[in] visMask the vis mask to determine the clients from this event is send to
 * @param[in] fd The firedefinition to use
 * @param[in] dt Delta time
 * @param[in] flags bitmask of the following values: @c SF_BODY, @c SF_IMPACT, @c SF_BOUNCING and @c SF_BOUNCED
 * @param[in] position The current position
 * @param[in] velocity The velocity of the throw
 */
void G_EventThrow (vismask_t visMask, const fireDef_t *fd, float dt, byte flags, const vec3_t position, const vec3_t velocity)
{
	gi.AddEvent(G_VisToPM(visMask), EV_ACTOR_THROW);
	gi.WriteShort(dt * 1000);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.WriteByte(flags);
	gi.WritePos(position);
	gi.WritePos(velocity);
	gi.EndEvents();
}

/**
 * @brief Send the bounding box info to the client.
 * @param[in] ent The edict to send the bounding box for
 */
void G_EventSendEdict (const edict_t *ent)
{
	gi.AddEvent(PM_ALL, EV_ADD_EDICT);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->type);
	gi.WritePos(ent->absmin);
	gi.WritePos(ent->absmax);
	gi.EndEvents();
}

void G_EventSendState (unsigned int playerMask, const edict_t *ent)
{
	G_EventActorStateChange(playerMask & G_TeamToPM(ent->team), ent);

	gi.AddEvent(playerMask & ~G_TeamToPM(ent->team), EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state & STATE_PUBLIC);
	gi.EndEvents();
}

/**
 * @brief Centers the view for all clients that are seeing the given edict on the world position of the edict
 * @param[in] ent The edict to use the position from
 */
void G_EventCenterView (const edict_t *ent)
{
	G_EventCenterViewAt(G_VisToPM(ent->visflags), ent->pos);
}

/**
 * @brief Centers the view for all clients that are seeing the given edict on the world position of the edict
 * @param[in] ent The edict to use the position from
 */
void G_EventCenterViewAt (int playerMask, const pos3_t pos)
{
	gi.AddEvent(playerMask, EV_CENTERVIEW);
	gi.WriteGPos(pos);
	gi.EndEvents();
}

/**
 * @sa CL_ActorAdd
 */
void G_EventActorAdd (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask, EV_ACTOR_ADD);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->team);
	gi.WriteByte(ent->chr.teamDef ? ent->chr.teamDef->idx : NONE);
	gi.WriteByte(ent->chr.gender);
	gi.WriteByte(ent->pnum);
	gi.WriteGPos(ent->pos);
	gi.WriteShort(ent->state & STATE_PUBLIC);
	gi.WriteByte(ent->fieldSize);
	gi.EndEvents();
}

/**
 * Send a particle spawn event to the client
 * @param[in] playerMask The clients that should see the particle
 * @param[in] ent The particle to spawn
 */
void G_EventSendParticle (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask, EV_PARTICLE_APPEAR);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->spawnflags);
	gi.WritePos(ent->origin);
	gi.WriteString(ent->particle);
	gi.EndEvents();
}

/**
 * @brief Send an appear event to the client.
 * @param playerMask The players to send the event to
 * @param ent The edict that should appear to the players included in the given mask.
 * @note Each following event that is relying on the fact that this edict must already
 * be known in the client, must also adopt the client side parsing of the event times.
 */
void G_EventEdictAppear (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask, EV_ENT_APPEAR);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->type);
	gi.WriteGPos(ent->pos);
	gi.EndEvents();
}

void G_EventActorAppear (unsigned int playerMask, const edict_t *check, const edict_t *ent)
{
	const int mask = G_TeamToPM(check->team) & playerMask;

	/* parsed in CL_ActorAppear */
	gi.AddEvent(playerMask, EV_ACTOR_APPEAR);
	gi.WriteShort(check->number);
	gi.WriteShort(ent && ent->number > 0 ? ent->number : SKIP_LOCAL_ENTITY);
	gi.WriteByte(check->team);
	gi.WriteByte(check->chr.teamDef ? check->chr.teamDef->idx : NONE);
	gi.WriteByte(check->chr.gender);
	gi.WriteShort(check->chr.ucn);
	gi.WriteByte(check->pnum);
	gi.WriteGPos(check->pos);
	gi.WriteByte(check->dir);
	if (RIGHT(check)) {
		gi.WriteShort(RIGHT(check)->item.t->idx);
	} else {
		gi.WriteShort(NONE);
	}

	if (LEFT(check)) {
		gi.WriteShort(LEFT(check)->item.t->idx);
	} else {
		gi.WriteShort(NONE);
	}

	if (check->body == 0 || check->head == 0) {
		gi.Error("invalid body and/or head model indices");
	}
	gi.WriteShort(check->body);
	gi.WriteShort(check->head);
	gi.WriteByte(check->chr.skin);
	/* strip the server private states */
	gi.WriteShort(check->state & STATE_PUBLIC);
	gi.WriteByte(check->fieldSize);
	/* get the max values for TU and morale */
	gi.WriteByte(G_ActorCalculateMaxTU(check));
	gi.WriteByte(min(MAX_SKILL, GET_MORALE(check->chr.score.skills[ABILITY_MIND])));
	gi.WriteShort(check->chr.maxHP);

	if (mask) {
		G_EventActorStateChange(mask, check);
		G_SendInventory(mask, check);
	}
}

/**
 * @brief Send disappear event
 * @param[in] playerMask The bitmask to determine the clients this event is send to
 * @param[in,out] ent The edict that perished
 */
void G_EventEdictPerish (unsigned int playerMask, const edict_t *ent)
{
	assert(ent->inuse);
	gi.AddEvent(playerMask, EV_ENT_PERISH);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->type);
	gi.EndEvents();
}

void G_EventActorStateChange (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask, EV_ACTOR_STATECHANGE);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->state);
	gi.EndEvents();
}

void G_EventAddBrushModel (unsigned int playerMask, const edict_t *ent)
{
	gi.AddEvent(playerMask, EV_ADD_BRUSH_MODEL);
	gi.WriteByte(ent->type);
	gi.WriteShort(ent->number);
	gi.WriteShort(ent->modelindex);
	/* strip the higher bits - only send levelflags */
	gi.WriteByte(ent->spawnflags & 0xFF);
	gi.WritePos(ent->origin);
	gi.WritePos(ent->angles);
	gi.WriteShort(ent->speed);
	gi.WriteByte(ent->angle);
	gi.WriteByte(ent->dir);
	gi.EndEvents();
}

void G_EventEndRoundAnnounce (const player_t *player)
{
	gi.AddEvent(PM_ALL, EV_ENDROUNDANNOUNCE | EVENT_INSTANTLY);
	gi.WriteByte(player->num);
	gi.WriteByte(player->pers.team);
	gi.EndEvents();
}

void G_EventStart (const player_t *player, qboolean teamplay)
{
	gi.AddEvent(G_PlayerToPM(player), EV_START | EVENT_INSTANTLY);
	gi.WriteByte(teamplay);
	gi.EndEvents();
}

void G_EventReset (const player_t *player, int activeTeam)
{
	gi.AddEvent(G_PlayerToPM(player), EV_RESET | EVENT_INSTANTLY);
	gi.WriteByte(player->pers.team);
	gi.WriteByte(activeTeam);
	gi.EndEvents();
}

void G_EventDoorOpen (const edict_t *door)
{
	gi.AddEvent(PM_ALL, EV_DOOR_OPEN);
	gi.WriteShort(door->number);
	gi.EndEvents();
}

void G_EventDoorClose (const edict_t *door)
{
	gi.AddEvent(PM_ALL, EV_DOOR_CLOSE);
	gi.WriteShort(door->number);
	gi.EndEvents();
}

void G_EventModelExplodeTriggered (const edict_t *ent)
{
	assert(ent->inuse);
	gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE_TRIGGERED);
	gi.WriteShort(ent->number);
	gi.EndEvents();
}

void G_EventModelExplode (const edict_t *ent)
{
	assert(ent->inuse);
	gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE);
	gi.WriteShort(ent->number);
	gi.EndEvents();
}
