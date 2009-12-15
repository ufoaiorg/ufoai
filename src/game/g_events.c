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
void G_EventInventoryAdd (const edict_t* ent, int mask)
{
	gi.AddEvent(mask, EV_INV_ADD);
	gi.WriteShort(ent->number);
	gi.WriteShort(INV_INVENTORY_BYTES);
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
 */
void G_EventReactionFireHandChange (const edict_t* ent, int hand)
{
	gi.AddEvent(G_TeamToPM(ent->team), EV_INV_HANDS_CHANGED);
	gi.WriteShort(ent->number);
	gi.WriteShort(hand);
}
