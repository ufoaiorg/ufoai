/**
 * @file
 * @brief Mission related code - king of the hill and so on.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "g_local.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_inventory.h"
#include "g_match.h"

void G_MissionAddVictoryMessage (const char *message)
{
	gi.ConfigString(CS_VICTORY_CONDITIONS, "%s", message);
}

/**
 * @brief Mission trigger
 * @todo use level.nextmap to spawn another map when every living actor has touched the mission trigger
 * @todo use level.actualRound to determine the 'King of the Hill' time
 * @note Don't set a client action here - otherwise the movement event might
 * be corrupted
 */
bool G_MissionTouch (edict_t *self, edict_t *activator)
{
	if (!self->owner)
		return false;

	switch (self->owner->team) {
	case TEAM_ALIEN:
		if (G_IsAlien(activator)) {
			if (!self->count) {
				self->count = level.actualRound;
				gi.BroadcastPrintf(PRINT_HUD, _("Aliens entered target zone!"));
			}
			return true;
		} else {
			/* reset king of the hill counter */
			self->count = 0;
		}
	/* general case that also works for multiplayer teams */
	default:
		if (activator->team == self->owner->team) {
			if (!self->owner->count) {
				self->owner->count = level.actualRound;
				if (self->owner->item) {
					/* search the item in the activator's inventory */
					containerIndex_t container;

					for (container = 0; container < gi.csi->numIDs; container++) {
						const invDef_t *invDef = INVDEF(container);
						invList_t *ic;
						/* ignore items linked from any temp container the actor
						 * must have this in his hands */
						if (invDef->temp)
							continue;
						for (ic = CONTAINER(activator, container); ic; ic = ic->next) {
							const objDef_t *od = ic->item.item;
							/* check whether we found the searched item in the
							 * actor's inventory */
							if (Q_streq(od->id, self->owner->item)) {
								/* drop the weapon - even if out of TUs */
								G_ActorInvMove(activator, invDef, ic, INVDEF(gi.csi->idFloor),
									NONE, NONE, false);
								gi.BroadcastPrintf(PRINT_HUD, _("Item was placed."));
								self->owner->count = level.actualRound;
								return true;
							}
						}
					}
				} else {
					gi.BroadcastPrintf(PRINT_HUD, _("Target zone is occupied!"));
				}
			}
			return true;
		} else {
			/* reset king of the hill counter */
			self->count = 0;
		}
	}
	return false;
}

/**
 * @brief Mission trigger destroy function
 */
bool G_MissionDestroy (edict_t *self)
{
	return true;
}

/**
 * @brief Mission trigger use function
 */
bool G_MissionUse (edict_t *self, edict_t *activator)
{
	edict_t *target = G_EdictsFindTargetEntity(self->target);
	if (!target) {
		gi.DPrintf("Target '%s' wasn't found for misc_mission\n", self->target);
		G_FreeEdict(self);
		return false;
	}

	if (target->destroy) {
		/* set this to zero to determine that this is a triggered destroy call */
		target->HP = 0;
		target->destroy(target);
		/* freed when the level changes */
		self->target = NULL;
		self->use = NULL;
	} else if (target->use)
		target->use(target, activator);

	return true;
}

/**
 * @note Think functions are only executed when the match is running
 * or in other word, the game has started
 */
void G_MissionThink (edict_t *self)
{
	edict_t *chain = self->groupMaster;
	edict_t *ent;
	int team;

	if (!G_MatchIsRunning())
		return;

	/* when every player has joined the match - spawn the mission target
	 * particle (if given) to mark the trigger */
	if (self->particle) {
		G_SpawnParticle(self->origin, self->spawnflags, self->particle);

		/* This is automatically freed on map shutdown */
		self->particle = NULL;
	}

	if (!chain)
		chain = self;
	while (chain) {
		if (chain->type == ET_MISSION) {
			if (chain->item) {
				const invList_t *ic;
				G_GetFloorItems(chain);
				ic = FLOOR(chain);
				if (!ic) {
					/* reset the counter if there is no item */
					chain->count = 0;
					return;
				}
				for (; ic; ic = ic->next) {
					const objDef_t *od = ic->item.item;
					assert(od);
					/* not the item we are looking for */
					if (Q_streq(od->id, chain->item))
						break;
				}
				if (!ic) {
					/* reset the counter if it's not the searched item */
					chain->count = 0;
					return;
				}
			}
			if (chain->time) {
				const int endTime = level.actualRound - chain->count;
				const int spawnIndex = (self->team + level.teamOfs) % MAX_TEAMS;
				const int currentIndex = (level.activeTeam + level.teamOfs) % MAX_TEAMS;
				/* not every edict in the group chain has
				 * been occupied long enough */
				if (!chain->count || endTime < chain->time ||
						(endTime == level.actualRound && spawnIndex <= currentIndex))
					return;
			}
			/* not destroyed yet */
			if ((chain->flags & FL_DESTROYABLE) && chain->HP)
				return;
		}
		chain = chain->groupChain;
	}

	if (self->use)
		self->use(self, NULL);

	/* store team before the edict is released */
	team = self->team;

	chain = self->groupMaster;
	if (!chain)
		chain = self;
	while (chain) {
		if (chain->item != NULL) {
			edict_t *item = G_GetEdictFromPos(chain->pos, ET_ITEM);
			if (item != NULL) {
				if (!G_InventoryRemoveItemByID(chain->item, item, gi.csi->idFloor)) {
					Com_Printf("Could not remove item '%s' from floor edict %i\n",
							chain->item, item->number);
				} else {
					G_AppearPerishEvent(G_VisToPM(item->visflags), false, item, NULL);
				}
			}
		}
		if (chain->particle != NULL) {
			/** @todo not yet working - particle stays active */
			edict_t *particle = G_GetEdictFromPos(chain->pos, ET_PARTICLE);
			if (particle != NULL) {
				G_AppearPerishEvent(PM_ALL, false, particle, NULL);
				G_FreeEdict(particle);
			}
		}

		ent = chain->groupChain;
		/* free the trigger */
		if (chain->child)
			G_FreeEdict(chain->child);
		/* free the group chain */
		G_FreeEdict(chain);
		chain = ent;
	}
	self = NULL;

	/* still active mission edicts left */
	ent = NULL;
	while ((ent = G_EdictsGetNextInUse(ent)))
		if (ent->type == ET_MISSION && ent->team == team)
			return;

	G_MatchEndTrigger(team, 10);
}
