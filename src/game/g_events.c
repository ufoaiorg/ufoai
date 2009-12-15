/**
 * @file g_events.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
}

/**
 * @brief Announce the actor die event for the clients that are seeing the actor
 * @param ent The actor that is dying
 * @param attacker The attacker that was responsible for this event
 */
void G_EventActorDie (const edict_t* ent, const edict_t* attacker)
{
	/* send death */
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_DIE);
	gi.WriteShort(ent->number);
	gi.WriteShort(attacker ? attacker->number : SKIP_LOCAL_ENTITY);
	gi.WriteShort(ent->state);
}

/**
 * @brief Tell the client to remove the item from the container
 * @param[in] mask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 */
void G_EventInventoryDelete (const edict_t* ent, int mask, const invDef_t* invDef, int x, int y)
{
	gi.AddEvent(mask, EV_INV_DEL);
	gi.WriteShort(ent->number);
	gi.WriteByte(invDef->id);
	gi.WriteByte(x);
	gi.WriteByte(y);
}

/**
 * @brief Tell the client to add the item from the container
 * @param[in] mask The player mask to determine which clients should receive the event (e.g. @c G_VisToPM(ent->visflags))
 * @note This event must be followed by a @c G_WriteItem call
 */
void G_EventInventoryAdd (const edict_t* ent, int mask, int itemAmount)
{
	gi.AddEvent(mask, EV_INV_ADD);
	gi.WriteShort(ent->number);
	gi.WriteShort(itemAmount * INV_INVENTORY_BYTES);
}

/**
 * @brief Send an event to all clients that are seeing the given edict, that it just has disappeared
 * @param ent The edict that disappeared
 */
void G_EventPerish (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ENT_PERISH);
	gi.WriteShort(ent->number);
}

/**
 * @brief Update reaction firemode when something is moved from/to a hand.
 * @param[in] ent The entity to change the reaction fire hand for
 * @param[in] hand The hand that should be used for the reaction fire.
 */
void G_EventReactionFireHandChange (const edict_t* ent, int hand)
{
	gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
	gi.WriteShort(ent->number);
	gi.WriteShort(hand);
}

/**
 * @brief Change the amount of available ammo for the given entity
 * @param ent The entity to change the amount of ammo for
 * @param ammo The ammo to change
 * @param amount The new amount of the left ammo
 * @param shootType The shooting type to determine which container to use
 */
void G_EventInventoryAmmo (const edict_t* ent, const objDef_t* ammo, int amount, int shootType)
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
}

/**
 * @brief Start the shooting event
 * @param ent The entity that starts the shooting
 * @param mask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param shootType The type of the shoot
 * @param at The grid position to target to
 */
void G_EventStartShoot (const edict_t* ent, int mask, const fireDef_t* fd, int shootType, const pos3_t at)
{
	gi.AddEvent(G_VisToPM(mask), EV_ACTOR_START_SHOOT);
	gi.WriteShort(ent->number);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.WriteByte(shootType);
	gi.WriteGPos(ent->pos);
	gi.WriteGPos(at);
}

/**
 * @brief Start the shooting event for hidden actors
 * @param mask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param firstShoot Is this the first shoot
 */
void G_EventShootHidden (int mask, const fireDef_t* fd, qboolean firstShoot)
{
	gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
	gi.WriteByte(firstShoot);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
}

/**
 * @brief Do the shooting
 * @param ent The entity that is doing the shooting
 * @param mask the vis mask to determine the clients from this event is send to
 * @param fd The firedefinition to use for the shoot
 * @param shootType The type of the shoot
 * @param flags Define some flags in a bitmask: @c SF_BODY, @c SF_IMPACT, @c SF_BOUNCING and @c SF_BOUNCING
 * @param trace The trace what was used to determine whether this shot has hit something
 * @param from The position the entity shoots from
 * @param impact The impact world vector for the shot
 */
void G_EventShoot (const edict_t* ent, int mask, const fireDef_t* fd, int shootType, int flags, trace_t* trace, const vec3_t from, const vec3_t impact)
{
	const edict_t *targetEdict = trace->ent;

	gi.AddEvent(G_VisToPM(mask), EV_ACTOR_SHOOT);
	gi.WriteShort(ent->number);
	if (targetEdict && G_IsBreakable(targetEdict))
		gi.WriteShort(targetEdict->number);
	else
		gi.WriteShort(SKIP_LOCAL_ENTITY);
	gi.WriteShort(fd->obj->idx);
	gi.WriteByte(fd->weapFdsIdx);
	gi.WriteByte(fd->fdIdx);
	gi.WriteByte(shootType);
	gi.WriteByte(flags);
	gi.WriteByte(trace->contentFlags);
	gi.WritePos(from);
	gi.WritePos(impact);
	gi.WriteDir(trace->plane.normal);
}

void G_EventActorFall (const edict_t* ent)
{
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
	gi.WriteShort(ent->number);
	gi.WriteByte(1);
	gi.WriteByte(ent->pos[0]);
	gi.WriteByte(ent->pos[1]);
	gi.WriteByte(ent->pos[2]);
	/** @todo see dvecs, PosSubDV and NewDVZ */
	gi.WriteByte(DIRECTION_FALL);
	gi.WriteShort(GRAVITY);
	gi.WriteShort(0);
}

/**
 * @brief Reset the client actions for the given entity
 * @param ent The entity to reset the client action for
 * @note This event is send to all the clients that belong that the edict's team
 */
void G_EventResetClientAction (const edict_t* ent)
{
	gi.AddEvent(G_TeamToPM(ent->team), EV_RESET_CLIENT_ACTION);
	gi.WriteShort(ent->number);
}

void G_EventActorStats (const edict_t* ent)
{
	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);
}

/**
 * @brief End of round event for the current active team
 * @note This event is send to every connected client
 */
void G_EventEndRound (void)
{
	gi.AddEvent(PM_ALL, EV_ENDROUND);
	gi.WriteByte(level.activeTeam);
}

void G_EventInventoryReload (const edict_t* ent, int mask, const item_t* item, const invDef_t* invDef, const invList_t* ic)
{
	gi.AddEvent(mask, EV_INV_RELOAD);
	gi.WriteShort(ent->number);
	gi.WriteByte(item->t->ammo);
	gi.WriteByte(item->m->idx);
	gi.WriteByte(invDef->id);
	gi.WriteByte(ic->x);
	gi.WriteByte(ic->y);
}
