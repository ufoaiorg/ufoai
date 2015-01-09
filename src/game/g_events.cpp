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

#include "g_local.h"	/* also includes g_event.h */
#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_inventory.h"

/**
 * @brief Spawns a sound (that will be spatialized on the client side)
 * @param playerMask A bit mask. One bit for each affected player
 * @param ent The edict that is causing the sound
 * @param origin The origin of the sound
 * @param sound The sound file, path relative to sounds/. If there is a + at the end the client will
 * choose a random sound. See the event function for more information.
 * of the path, a random sound will be taken.
 */
void G_EventSpawnSound (playermask_t playerMask, const Edict& ent, const vec3_t origin, const char* sound)
{
	G_EventAdd(playerMask, EV_SOUND, ent.getIdNum());

	/* use the entity origin unless it is a bmodel or explicitly specified */
	if (!origin) {
		if (ent.solid == SOLID_BSP) {
			vec3_t origin_v;
			ent.entBox.getCenter(origin_v);
			VectorAdd(ent.origin, origin_v, origin_v);
			gi.WritePos(origin);
		} else {
			gi.WritePos(vec3_origin);
		}
	} else {
		gi.WritePos(origin);
	}
	gi.WriteByte(0xFF);
	gi.WriteString(sound);
	G_EventEnd();
}

void G_EventSpawnFootstepSound (const Edict& ent, const char* sound)
{
	const playermask_t playerMask = ~G_VisToPM(ent.visflags);
	gi.QueueEvent(playerMask, EV_SOUND, ent.getIdNum());
	gi.QueueWritePos(ent.origin);
	gi.QueueWriteByte(ent.moveinfo.steps);
	gi.QueueWriteString(sound);
}

/**
 * @brief Send the turn event for the given entity
 * @param ent The entity to send the turn event for
 * @note Every player that can see this ent will reveive the turn event data
 * @note Make sure that the direction to turn into is already set
 */
void G_EventActorTurn (const Edict& ent)
{
	G_EventAdd(G_VisToPM(ent.visflags), EV_ACTOR_TURN, ent.getIdNum());
	gi.WriteByte(ent.dir);
	G_EventEnd();
}

/**
 * @brief Announce the actor die event for the clients that are seeing the actor
 * @param[in] ent The actor that is dying
 * @param[in] attacker Whether the actor was killed or just died
 */
void G_EventActorDie (const Edict& ent, bool attacker)
{
	G_EventAdd(G_VisToPM(ent.visflags), EV_ACTOR_DIE, ent.getIdNum());
	gi.WriteShort(ent.state);
	gi.WriteByte(ent.getPlayerNum());
	gi.WriteByte(attacker);
	G_EventEnd();
}

/**
 * @brief Announce the actor revitalize event for the clients that are seeing the actor
 * @param[in] ent The actor that was healed and woke up again
 */
void G_EventActorRevitalise (const Edict& ent)
{
	G_EventAdd(G_VisToPM(ent.visflags), EV_ACTOR_REVITALISED, ent.getIdNum());
	gi.WriteShort(ent.state);
	G_EventEnd();
}

/**
 * @brief Will inform the player about the real TU reservation
 * @param ent The actors edict.
 */
void G_EventActorSendReservations (const Edict& ent)
{
	G_EventAdd(G_PlayerToPM(ent.getPlayer()), EV_ACTOR_RESERVATIONCHANGE, ent.getIdNum());
	const chrReservations_t& reservedTUs = ent.chr.reservedTus;
	gi.WriteShort(reservedTUs.reaction);
	gi.WriteShort(reservedTUs.shot);
	gi.WriteShort(reservedTUs.crouch);

	G_EventEnd();
}

/**
 * @brief Tell the client to remove the item from the container
 * @param[in] ent Pointer to entity having given inventory.
 * @param[in] playerMask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 * @param[in] containerId Id of the container the item is in.
 * @param[in] x,y Position of item in container.
 */
void G_EventInventoryDelete (const Edict& ent, playermask_t playerMask, const containerIndex_t containerId, int x, int y)
{
	G_EventAdd(playerMask, EV_INV_DEL, ent.getIdNum());
	gi.WriteByte(containerId);
	gi.WriteByte(x);
	gi.WriteByte(y);
	G_EventEnd();
}

/**
 * @brief Tell the client to add the item from the container
 * @param[in] ent Pointer to entity having given inventory.
 * @param[in] playerMask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 * @param[in] itemAmount How many items to add.
 * @note This event must be followed by a @c G_WriteItem call
 */
void G_EventInventoryAdd (const Edict& ent, playermask_t playerMask, int itemAmount)
{
	G_EventAdd(playerMask, EV_INV_ADD, ent.getIdNum());
	gi.WriteShort(itemAmount);
	/* do not end the pending events here - this is just a header, the items are following */
}

/**
 * @brief Send an event to all clients that are seeing the given edict, that it just has disappeared
 * @param ent The edict that disappeared
 */
void G_EventPerish (const Edict& ent)
{
	G_EventEdictPerish(G_VisToPM(ent.visflags), ent);
}

/**
 * @brief Unregister an edict at the client
 * @param ent The edict to unregister
 */
void G_EventDestroyEdict (const Edict& ent)
{
	assert(ent.inuse);
	G_EventAdd(PM_ALL, EV_ENT_DESTROY, ent.getIdNum());
	G_EventEnd();
}

/**
 * @brief Change the amount of available ammo for the given entity
 * @param ent The entity to change the amount of ammo for
 * @param ammo The ammo to change
 * @param amount The new amount of the left ammo
 * @param shootType The shooting type to determine which container to use
 */
void G_EventInventoryAmmo (const Edict& ent, const objDef_t* ammo, int amount, shoot_types_t shootType)
{
	G_EventAdd(G_VisToPM(ent.visflags), EV_INV_AMMO, ent.getIdNum());
	gi.WriteByte(amount);
	gi.WriteByte(ammo->idx);
	if (IS_SHOT_RIGHT(shootType))
		gi.WriteByte(CID_RIGHT);
	else
		gi.WriteByte(CID_LEFT);
	/* x and y value */
	gi.WriteByte(0);
	gi.WriteByte(0);
	G_EventEnd();
}

/**
 * @brief Start the shooting event
 * @param ent The entity that starts the shooting
 * @param teamMask the vis mask of the teams to determine the clients from this event is send to
 * @param shootType The type of the shoot
 * @param at The grid position to target to
 */
void G_EventStartShoot (const Edict& ent, teammask_t teamMask, shoot_types_t shootType, const pos3_t at)
{
	G_EventAdd(G_VisToPM(teamMask), EV_ACTOR_START_SHOOT, ent.getIdNum());
	gi.WriteByte(shootType);
	gi.WriteGPos(ent.pos);
	gi.WriteGPos(at);
	G_EventEnd();
}

/**
 * @brief Start the shooting event
 * @param ent The entity that starts the shooting
 * @param teamMask the vis mask of the teams to determine the clients from this event is send to
 */
void G_EventEndShoot (const Edict &ent, teammask_t teamMask)
{
	G_EventAdd(G_VisToPM(teamMask), EV_ACTOR_END_SHOOT, ent.getIdNum());
	G_EventEnd();
}

/**
 * @brief Start the shooting event for hidden actors
 * @param teamMask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param firstShoot Is this the first shoot
 */
void G_EventShootHidden (teammask_t teamMask, const fireDef_t* fd, bool firstShoot)
{
	G_EventAdd(~G_VisToPM(teamMask), EV_ACTOR_SHOOT_HIDDEN, -1);
	gi.WriteByte(firstShoot);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	G_EventEnd();
}

/**
 * @brief Do the shooting
 * @param ent The entity that is doing the shooting
 * @param teamMask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param firstShoot Is this the first shoot
 * @param shootType The type of the shoot
 * @param flags Define some flags in a bitmask: @c SF_BODY, @c SF_IMPACT, @c SF_BOUNCING and @c SF_BOUNCING
 * @param trace The trace what was used to determine whether this shot has hit something
 * @param from The position the entity shoots from
 * @param impact The impact world vector for the shot
 */
void G_EventShoot (const Edict& ent, teammask_t teamMask, const fireDef_t* fd, bool firstShoot, shoot_types_t shootType, int flags, const trace_t* trace, const vec3_t from, const vec3_t impact)
{
	const Edict* targetEdict = G_EdictsGetByNum(trace->entNum);	/* the ent possibly hit by the trace */

	G_EventAdd(G_VisToPM(teamMask), EV_ACTOR_SHOOT, ent.getIdNum());
	if (targetEdict && G_IsBreakable(targetEdict))
		gi.WriteShort(targetEdict->getIdNum());
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
	G_EventEnd();
}

void G_EventReactionFireChange (const Edict& ent)
{
	const FiremodeSettings& fireMode = ent.chr.RFmode;
	const objDef_t* od = fireMode.getWeapon();

	G_EventAdd(G_PlayerToPM(ent.getPlayer()), EV_ACTOR_REACTIONFIRECHANGE, ent.getIdNum());
	gi.WriteByte(fireMode.getFmIdx());
	gi.WriteByte(fireMode.getHand());
	gi.WriteShort(od ? od->idx : NONE);

	G_EventEnd();
}

void G_EventReactionFireAddTarget (const Edict& shooter, const Edict& target, int tus, int step)
{
	gi.QueueEvent(G_PlayerToPM(shooter.getPlayer()), EV_ACTOR_REACTIONFIREADDTARGET, shooter.getIdNum());
	gi.QueueWriteShort(target.getIdNum());
	gi.QueueWriteByte(tus);
	gi.QueueWriteByte(step);
}

void G_EventReactionFireRemoveTarget (const Edict& shooter, const Edict& target, int step)
{
	gi.QueueEvent(G_PlayerToPM(shooter.getPlayer()), EV_ACTOR_REACTIONFIREREMOVETARGET, shooter.getIdNum());
	gi.QueueWriteShort(target.getIdNum());
	gi.QueueWriteByte(step);
}

void G_EventReactionFireTargetUpdate (const Edict& shooter, const Edict& target, int tus, int step)
{
	gi.QueueEvent(G_PlayerToPM(shooter.getPlayer()), EV_ACTOR_REACTIONFIRETARGETUPDATE, shooter.getIdNum());
	gi.QueueWriteShort(target.getIdNum());
	gi.QueueWriteByte(tus);
	gi.QueueWriteByte(step);
}

void G_EventReactionFireAbortShot (const Edict& shooter, const Edict& target, int step)
{
	gi.QueueEvent(G_PlayerToPM(shooter.getPlayer()), EV_ACTOR_REACTIONFIREABORTSHOT, shooter.getIdNum());
	gi.QueueWriteShort(target.getIdNum());
	gi.QueueWriteByte(step);
}

/**
 * @brief Spawn a new particle for the client
 * @param[in] playerMask A bit mask. One bit for each affected player
 * @param[in] name The id of the particle (see ptl_*.ufo script files in base/ufos)
 * @param[in] levelFlags Show at which levels
 * @param[in] s starting/location vector
 * @param[in] v velocity vector
 * @param[in] a acceleration vector
 */
void G_EventParticleSpawn (playermask_t playerMask, const char* name, int levelFlags, const vec3_t s, const vec3_t v, const vec3_t a)
{
	G_EventAdd(playerMask, EV_PARTICLE_SPAWN, -1);
	gi.WriteByte(levelFlags);
	gi.WritePos(s);
	gi.WritePos(v);
	gi.WritePos(a);
	gi.WriteString(name);
	G_EventEnd();
}

void G_EventActorFall (const Edict& ent)
{
	G_EventAdd(G_VisToPM(ent.visflags), EV_ACTOR_MOVE, ent.getIdNum());
	gi.WriteByte(1);
	gi.WriteByte(ent.pos[0]);
	gi.WriteByte(ent.pos[1]);
	gi.WriteByte(ent.pos[2]);
	gi.WriteByte(makeDV(DIRECTION_FALL, ent.pos[2]));
	gi.WriteShort(GRAVITY);
	gi.WriteShort(0);
	G_EventEnd();
}

/**
 * @brief Informs the client that an interaction with the world is possible
 * @note It's assumed that the clientAction is already set
 * @param[in] ent The edict that can execute the action (an actor)
 */
void G_EventSetClientAction (const Edict& ent)
{
	assert(ent.clientAction);
	assert(ent.clientAction->flags & FL_CLIENTACTION);

	/* tell the hud to show the door buttons */
	G_EventAdd(G_TeamToPM(ent.getTeam()), EV_CLIENT_ACTION, ent.getIdNum());
	gi.WriteShort(ent.clientAction->getIdNum());
	G_EventEnd();
}

/**
 * @brief Reset the client actions for the given entity
 * @param[in] ent The entity to reset the client action for
 * @note This event is send to the player this edict belongs to
 */
void G_EventResetClientAction (const Edict& ent)
{
	const int playerMask = G_PlayerToPM(ent.getPlayer());
	G_EventAdd(playerMask, EV_RESET_CLIENT_ACTION, ent.getIdNum());
	G_EventEnd();
}

void G_EventActorStats (const Edict& ent, playermask_t playerMask)
{
	G_EventAdd(playerMask, EV_ACTOR_STATS, ent.getIdNum());
	gi.WriteByte(ent.TU);
	gi.WriteShort(ent.HP);
	gi.WriteByte(ent.getStun());
	gi.WriteByte(ent.morale);
	G_EventEnd();
}

/**
 * @brief Send info about an actor's wounds to the client.
 * @param[in] ent The actor whose wound status we are sending.
 * @param[in] bodyPart The body part index we are sending wound info about.
 * @note This event is sent to the player this actor belongs to
 */
void G_EventActorWound (const Edict& ent, const int bodyPart)
{
	const int mask = G_PlayerToPM(ent.getPlayer());
	G_EventAdd(mask, EV_ACTOR_WOUND, ent.getIdNum());
	gi.WriteByte(bodyPart);
	const woundInfo_t& wounds = ent.chr.wounds;
	gi.WriteByte(wounds.woundLevel[bodyPart]);
	gi.WriteByte(wounds.treatmentLevel[bodyPart]);
	G_EventEnd();
}

/**
 * @brief End of turn event for the current active team
 * @note This event is send to every connected client
 */
void G_EventEndRound (void)
{
	G_EventAdd(PM_ALL, EV_ENDROUND, -1);
	gi.WriteByte(level.activeTeam);
	G_EventEnd();
}

void G_EventInventoryReload (const Edict& ent, playermask_t playerMask, const Item* item, const invDef_t* invDef, const Item* ic)
{
	G_EventAdd(playerMask, EV_INV_RELOAD, ent.getIdNum());
	gi.WriteByte(item->def()->ammo);
	gi.WriteByte(item->ammoDef()->idx);
	gi.WriteByte(invDef->id);
	gi.WriteByte(ic->getX());
	gi.WriteByte(ic->getY());
	G_EventEnd();
}

/**
 * @param[in] teamMask the vis mask to determine the clients from this event is send to
 * @param[in] fd The firedefinition to use
 * @param[in] dt Delta time
 * @param[in] flags bitmask of the following values: @c SF_BODY, @c SF_IMPACT, @c SF_BOUNCING and @c SF_BOUNCED
 * @param[in] position The current position
 * @param[in] velocity The velocity of the throw
 */
void G_EventThrow (teammask_t teamMask, const fireDef_t* fd, float dt, byte flags, const vec3_t position, const vec3_t velocity)
{
	G_EventAdd(G_VisToPM(teamMask), EV_ACTOR_THROW, -1);
	gi.WriteShort(dt * 1000);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.WriteByte(flags);
	gi.WritePos(position);
	gi.WritePos(velocity);
	G_EventEnd();
}

/**
 * @brief Send the bounding box info to the client.
 * @param[in] ent The edict to send the bounding box for
 */
void G_EventSendEdict (const Edict& ent)
{
	G_EventAdd(PM_ALL, EV_ADD_EDICT, ent.getIdNum());
	gi.WriteByte(ent.type);
	gi.WritePos(ent.absBox.mins);
	gi.WritePos(ent.absBox.maxs);
	G_EventEnd();
}

void G_EventSendState (playermask_t playerMask, const Edict& ent)
{
	G_EventActorStateChange(playerMask & G_TeamToPM(ent.getTeam()), ent);

	G_EventAdd(playerMask & ~G_TeamToPM(ent.getTeam()), EV_ACTOR_STATECHANGE, ent.getIdNum());
	gi.WriteShort(ent.state & STATE_PUBLIC);
	G_EventEnd();
}

/**
 * @brief Centers the view for all clients that are seeing the given edict on the world position of the edict
 * @param[in] ent The edict to use the position from
 */
void G_EventCenterView (const Edict& ent)
{
	G_EventCenterViewAt(G_VisToPM(ent.visflags), ent.pos);
}

/**
 * @brief Centers the view for all clients that are seeing the given edict on the world position of the edict
 * @param playerMask The clients that should see the edict
 * @param pos The position to center the view
 */
void G_EventCenterViewAt (playermask_t playerMask, const pos3_t pos)
{
	G_EventAdd(playerMask, EV_CENTERVIEW, -1);
	gi.WriteGPos(pos);
	G_EventEnd();
}

/**
 * @brief Centers the view for all clients that are seeing the given edict on the world position of the edict
 * @param[in] playerMask The clients that should see the edict
 * @param[in] pos The position to center the view
 */
void G_EventMoveCameraTo (playermask_t playerMask, const pos3_t pos)
{
	G_EventAdd(playerMask, EV_MOVECAMERA, -1);
	gi.WriteGPos(pos);
	G_EventEnd();
}

/**
 * @sa CL_ActorAdd
 */
void G_EventActorAdd (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_ACTOR_ADD, ent.getIdNum());
	gi.WriteByte(ent.getTeam());
	gi.WriteByte(ent.chr.teamDef ? ent.chr.teamDef->idx : NONE);
	gi.WriteByte(ent.chr.gender);
	gi.WriteByte(ent.getPlayerNum());
	gi.WriteGPos(ent.pos);
	gi.WriteShort(ent.state & STATE_PUBLIC);
	gi.WriteByte(ent.fieldSize);
	G_EventEnd();
}

/**
 * Send a particle spawn event to the client
 * @param[in] playerMask The clients that should see the particle
 * @param[in] ent The particle to spawn
 */
void G_EventSendParticle (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_PARTICLE_APPEAR, ent.getIdNum());
	gi.WriteShort(ent.spawnflags);
	gi.WritePos(ent.origin);
	gi.WriteString(ent.particle);
	G_EventEnd();
}

/**
 * @brief Send an appear event to the client.
 * @param playerMask The players to send the event to
 * @param ent The camera that should appear to the players included in the given mask.
 */
void G_EventCameraAppear (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_CAMERA_APPEAR, ent.getIdNum());
	gi.WritePos(ent.origin);
	gi.WriteByte(ent.getTeam());
	gi.WriteByte(ent.dir);
	gi.WriteByte(ent.camera.cameraType);
	/* strip the higher bits - only send levelflags */
	gi.WriteByte(ent.spawnflags & 0xFF);
	gi.WriteByte(ent.camera.rotate);
	G_EventEnd();
}

/**
 * @brief Send an appear event to the client.
 * @param playerMask The players to send the event to
 * @param ent The edict that should appear to the players included in the given mask.
 * @note Each following event that is relying on the fact that this edict must already
 * be known in the client, must also adopt the client side parsing of the event times.
 */
void G_EventEdictAppear (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_ENT_APPEAR, ent.getIdNum());
	gi.WriteByte(ent.type);
	gi.WriteGPos(ent.pos);
	G_EventEnd();
}

void G_EventActorAppear (playermask_t playerMask, const Actor& check, const Edict* ent)
{
	const int mask = G_TeamToPM(check.getTeam()) & playerMask;

	/* parsed in CL_ActorAppear */
	G_EventAdd(playerMask, EV_ACTOR_APPEAR, check.getIdNum());
	gi.WriteShort(ent && ent->getIdNum() > 0 ? ent->getIdNum() : SKIP_LOCAL_ENTITY);
	gi.WriteByte(check.getTeam());
	gi.WriteByte(check.chr.teamDef ? check.chr.teamDef->idx : NONE);
	gi.WriteByte(check.chr.gender);
	gi.WriteShort(check.chr.ucn);
	gi.WriteByte(check.getPlayerNum());
	gi.WriteGPos(check.pos);
	gi.WriteByte(check.dir);
	if (check.getRightHandItem()) {
		gi.WriteShort(check.getRightHandItem()->def()->idx);
	} else {
		gi.WriteShort(NONE);
	}

	if (check.getLeftHandItem()) {
		gi.WriteShort(check.getLeftHandItem()->def()->idx);
	} else {
		gi.WriteShort(NONE);
	}

	if (check.getBody() == 0 || check.getHead() == 0) {
		gi.Error("invalid body and/or head model indices");
	}
	gi.WriteShort(check.getBody());
	gi.WriteShort(check.getHead());
	gi.WriteByte(check.chr.bodySkin);
	gi.WriteByte(check.chr.headSkin);
	/* strip the server private states */
	gi.WriteShort(check.state & STATE_PUBLIC);
	gi.WriteByte(check.fieldSize);
	/* get the max values for TU and morale */
	gi.WriteByte(G_ActorCalculateMaxTU(&check));
	gi.WriteByte(std::min(MAX_SKILL, GET_MORALE(check.chr.score.skills[ABILITY_MIND])));
	gi.WriteShort(check.chr.maxHP);
	G_EventEnd();

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
void G_EventEdictPerish (playermask_t playerMask, const Edict& ent)
{
	assert(ent.inuse);
	G_EventAdd(playerMask, EV_ENT_PERISH, ent.getIdNum());
	gi.WriteByte(ent.type);
	G_EventEnd();
}

void G_EventActorStateChange (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_ACTOR_STATECHANGE, ent.getIdNum());
	gi.WriteShort(ent.state);
	G_EventEnd();
}

void G_EventAddBrushModel (playermask_t playerMask, const Edict& ent)
{
	G_EventAdd(playerMask, EV_ADD_BRUSH_MODEL, ent.getIdNum());
	gi.WriteByte(ent.type);
	gi.WriteShort(ent.modelindex);
	/* strip the higher bits - only send levelflags */
	gi.WriteByte(ent.spawnflags & 0xFF);
	gi.WritePos(ent.origin);
	gi.WritePos(ent.angles);
	gi.WriteShort(ent.speed);
	gi.WriteByte(ent.angle);
	gi.WriteByte(ent.dir);
	G_EventEnd();
}

void G_EventEndRoundAnnounce (const Player& player)
{
	G_EventAdd(PM_ALL, EV_ENDROUNDANNOUNCE, -1);
	gi.WriteByte(player.getNum());
	gi.WriteByte(player.getTeam());
	G_EventEnd();
}

void G_EventStart (const Player& player, bool teamplay)
{
	G_EventAdd(G_PlayerToPM(player), EV_START | EVENT_INSTANTLY, -1);
	gi.WriteByte(teamplay);
	G_EventEnd();
}

void G_EventReset (const Player& player, int activeTeam)
{
	G_EventAdd(G_PlayerToPM(player), EV_RESET | EVENT_INSTANTLY, -1);
	gi.WriteByte(player.getTeam());
	gi.WriteByte(activeTeam);
	G_EventEnd();
}

void G_EventDoorOpen (const Edict& door)
{
	G_EventAdd(PM_ALL, EV_DOOR_OPEN, door.getIdNum());
	G_EventEnd();
}

void G_EventDoorClose (const Edict& door)
{
	G_EventAdd(PM_ALL, EV_DOOR_CLOSE, door.getIdNum());
	G_EventEnd();
}

void G_EventModelExplodeTriggered (const Edict& ent, const char* sound)
{
	assert(ent.inuse);
	G_EventAdd(PM_ALL, EV_MODEL_EXPLODE_TRIGGERED, ent.getIdNum());
	gi.WriteString(sound);
	G_EventEnd();
}

void G_EventModelExplode (const Edict& ent, const char* sound)
{
	assert(ent.inuse);
	G_EventAdd(PM_ALL, EV_MODEL_EXPLODE, ent.getIdNum());
	gi.WriteString(sound);
	G_EventEnd();
}

void G_EventAdd (playermask_t playerMask, int eType, int entnum)
{
	G_EventEnd();
	gi.AddEvent(playerMask, eType, entnum);
}

void G_EventEnd (void)
{
	if (gi.GetEvent() == EV_ACTOR_MOVE) {
		/* mark the end of the steps */
		gi.WriteLong(0);
		const Edict* ent = gi.GetEventEdict();
		assert(ent);
		gi.WriteGPos(ent->pos);
	}
	gi.EndEvents();
}
