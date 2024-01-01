/**
 * @file
 * @brief Mission related code - king of the hill and so on.
 */

/*
All original material Copyright (C) 2002-2024 UFO: Alien Invasion.

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

#include "g_mission.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_inventory.h"
#include "g_match.h"
#include "g_spawn.h"
#include "g_utils.h"

void G_MissionAddVictoryMessage (const char* message)
{
	gi.ConfigString(CS_VICTORY_CONDITIONS, "%s\n", message);
}

static inline const char* G_MissionGetTeamString (const int team) {
	return (team == TEAM_PHALANX ? "PHALANX" : (team == TEAM_ALIEN ? "The alien" : va("Team %i's", team)));
}

/**
 * @brief Mission trigger
 * @todo use level.nextmap to spawn another map when every living actor has touched the mission trigger
 * @note Don't set a client action here - otherwise the movement event might
 * be corrupted
 */
bool G_MissionTouch (Edict* self, Edict* activator)
{
	if (!G_IsLivingActor(activator))
		return false;

	Actor* actor = makeActor(activator);
	const char* const actorTeam = G_MissionGetTeamString(actor->getTeam());
	if (!G_IsCivilian(actor) && self->isOpponent(actor)) {
		if (!self->item && self->count) {
			if (self->targetname) {
				gi.BroadcastPrintf(PRINT_HUD, _("%s forces are attacking the %s!"), actorTeam, self->targetname);
			} else {
				const char* const teamName = G_MissionGetTeamString(self->getTeam());
				gi.BroadcastPrintf(PRINT_HUD, _("%s forces are attacking %s target zone!"),
						actorTeam, teamName);
			}

			/* reset king of the hill counter */
			self->count = 0;
		}
		return false;
	}
	if (self->count)
		return false;

	if (self->isSameTeamAs(actor)) {
		if (!self->item) {
			linkedList_t* touched = self->touchedList;
			while (touched) {
				const Edict* const ent = static_cast<Edict*>(touched->data);
				if (!self->isSameTeamAs(ent) && !G_IsCivilian(ent) && !G_IsDead(ent)) {
					return false;
				}
				touched = touched->next;
			}
			self->count = level.actualRound;
			if (self->targetname) {
				gi.BroadcastPrintf(PRINT_HUD, _("%s forces have occupied the %s!"), actorTeam, self->targetname);
			} else {
				gi.BroadcastPrintf(PRINT_HUD, _("%s forces have occupied their target zone!"), actorTeam);
			}
			return true;
		}
	}

	/* search the item in the activator's inventory */
	/* ignore items linked from any temp container the actor must have this in his hands */
	const Container* cont = nullptr;
	while ((cont = actor->chr.inv.getNextCont(cont))) {
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			const objDef_t* od = item->def();
			/* check whether we found the searched item in the actor's inventory */
			if (!Q_streq(od->id, self->item))
				continue;

			/* drop the weapon - even if out of TUs */
			G_ActorInvMove(actor, cont->def(), item, INVDEF(CID_FLOOR), NONE, NONE, false);
			if (self->targetname) {
				gi.BroadcastPrintf(PRINT_HUD, _("The %s was placed at the %s."), item->def()->name, self->targetname);
			} else {
				gi.BroadcastPrintf(PRINT_HUD, _("The %s was placed."), item->def()->name);
			}
			self->count = level.actualRound;
			return true;
		}
	}

	return false;
}

void G_MissionReset (Edict* self, Edict* activator)
{
	/* Don't reset the mission timer for 'bring item' missions G_MissionThink will handle that */
	if (!self->time || self->item)
		return;
	linkedList_t* touched = self->touchedList;
	while (touched) {
		const Edict* const ent = static_cast<Edict*>(touched->data);
		if (self->isSameTeamAs(ent) && !(G_IsDead(ent) || ent == activator)) {
			return;
		}
		touched = touched->next;
	}
	if (activator->getTeam() == self->getTeam()) {
		const char* const actTeam = G_MissionGetTeamString(activator->getTeam());
		if (self->targetname)
			gi.BroadcastPrintf(PRINT_HUD, _("%s forces have left the %s!"), actTeam, self->targetname);
		else
			gi.BroadcastPrintf(PRINT_HUD, _("%s forces have left their target zone!"), actTeam);
	}
	/* All team actors are gone, reset counter */
	self->count = 0;
}

/**
 * @brief Mission trigger use function
 */
bool G_MissionUse (Edict* self, Edict* activator)
{
	Edict* target = G_EdictsFindTargetEntity(self->target);
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
		self->target = nullptr;
		self->use = nullptr;
	} else if (target->use)
		target->use(target, activator);

	return true;
}

static bool G_MissionIsTouched (Edict* self) {
	linkedList_t* touched = self->touchedList;
	while (touched) {
		const Edict* const ent = static_cast<Edict*>(touched->data);
		if (self->isSameTeamAs(ent) && !G_IsDead(ent))
			return true;
		touched = touched->next;
	}
	return false;
}

/**
 * @note Think functions are only executed when the match is running
 * or in other word, the game has started
 */
void G_MissionThink (Edict* self)
{
	if (!G_MatchIsRunning())
		return;

	/* when every player has joined the match - spawn the mission target
	 * particle (if given) to mark the trigger */
	if (self->particle) {
		self->link = G_SpawnParticle(self->origin, self->spawnflags, self->particle);

		/* This is automatically freed on map shutdown */
		self->particle = nullptr;
	}

	Edict* chain = self->groupMaster;
	if (!chain)
		chain = self;
	while (chain) {
		if (chain->type == ET_MISSION) {
			if (chain->item) {
				const Item* ic;
				G_GetFloorItems(chain);
				ic = chain->getFloor();
				if (!ic) {
					/* reset the counter if there is no item */
					chain->count = 0;
					return;
				}
				for (; ic; ic = ic->getNext()) {
					const objDef_t* od = ic->def();
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
				/* Check that the target zone is still occupied (last defender might have died) */
				if (!chain->item && !G_MissionIsTouched(chain)) {
						chain->count = 0;
				}
				const int endTime = level.actualRound - chain->count;
				const int spawnIndex = (chain->getTeam() + level.teamOfs) % MAX_TEAMS;
				const int currentIndex = (level.activeTeam + level.teamOfs) % MAX_TEAMS;
				/* not every edict in the group chain has
				 * been occupied long enough */
				if (!chain->count || endTime < chain->time ||
						(endTime == chain->time && spawnIndex < currentIndex))
					return;
			}
			if (chain->target && !chain->time && !chain->item) {
				if (!G_MissionIsTouched(chain))
					return;
			}
		}
		chain = chain->groupChain;
	}

	const bool endMission = self->target == nullptr;

	/* store team before the edict is released */
	const int team = self->getTeam();
	chain = self->groupMaster;
	if (!chain)
		chain = self;
	while (chain) {
		if (chain->type == ET_MISSION) {
			if (chain->item != nullptr) {
				Edict* item = G_GetEdictFromPos(chain->pos, ET_ITEM);
				if (item != nullptr) {
					if (!G_InventoryRemoveItemByID(chain->item, item, CID_FLOOR)) {
						Com_Printf("Could not remove item '%s' from floor edict %i\n", chain->item, item->getIdNum());
					} else if (!item->getFloor()) {
						G_EventPerish(*item);
						G_FreeEdict(item);
					}
				}
			}
			if (chain->link != nullptr) {
				Edict* particle = G_GetEdictFromPos(chain->pos, ET_PARTICLE);
				if (particle != nullptr) {
					G_AppearPerishEvent(G_VisToPM(particle->visflags), false, *particle, nullptr);
					G_FreeEdict(particle);
				}
				chain->link = nullptr;
			}

			/* Display mission message */
			if (G_ValidMessage(chain)) {
				const char* msg = chain->message;
				if (msg[0] == '_')
					++msg;
				gi.BroadcastPrintf(PRINT_HUD, "%s", msg);
			}
		}

		Edict* ent = chain->groupChain;
		/* free the group chain (G_MissionUse() will free the edict if it fails) */
		if (G_UseEdict(chain, nullptr))
			G_FreeEdict(chain);
		chain = ent;
	}

	if (endMission)
		G_MatchEndTrigger(team, level.activeTeam == TEAM_ALIEN ? 10 : 3);
}
