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

#include "g_local.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_health.h"

/**
 * @brief Send stats to network buffer
 * @sa CL_ActorStats
 */
void G_SendStats (Edict& ent)
{
	/* extra sanity checks */
	assert(ent.TU >= 0);
	ent.HP = std::max(ent.HP, 0);
	ent.setStun(std::min(ent.getStun(), 255));
	ent.morale = std::max(ent.morale, 0);

	G_EventActorStats(ent, G_TeamToPM(ent.getTeam()));
}

/**
 * @brief Write player stats to network buffer
 * @sa G_SendStats
 */
void G_SendPlayerStats (const Player& player)
{
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextActor(actor)))
		if (actor->getTeam() == player.getTeam()) {
			G_EventActorStats(*actor, G_PlayerToPM(player));
			G_SendWoundStats(actor);
		}
}
