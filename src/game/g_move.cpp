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

#include "g_move.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_inventory.h"
#include "g_reaction.h"
#include "g_utils.h"
#include "g_vis.h"
#include "g_match.h"

#define ACTOR_SPEED_NORMAL 100
#define ACTOR_SPEED_CROUCHED (ACTOR_SPEED_NORMAL / 2)
static const float FALLING_DAMAGE_FACTOR = 10.0f;

/**
 * @brief The forbidden list is a list of entity positions that are occupied by an entity.
 * This list is checked everytime an actor wants to walk there.
 */
static forbiddenList_t forbiddenList;

/**
 * @brief Build the forbidden list for the pathfinding (server side).
 * @param[in] team The team number if the list should be calculated from the eyes of that team. Use 0 to ignore team.
 * @param[in] movingActor The moving actor to build the forbidden list for. If this is an AI actor, everything other actor will be
 * included in the forbidden list - even the invisible ones. This is needed to ensure that they are not walking into each other
 * (civilians <=> aliens, aliens <=> civilians)
 * @sa G_MoveCalc
 * @sa Grid_CheckForbidden
 * @sa CL_BuildForbiddenList <- shares quite some code
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void G_BuildForbiddenList (int team, const Edict* movingActor)
{
	forbiddenList.reset();

	/* team visibility */
	const teammask_t teamMask = team ? G_TeamToVisMask(team) : TEAM_ALL;

	Edict* ent = nullptr;
	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* Dead 2x2 unit will stop walking, too. */
		if (G_IsBlockingMovementActor(ent) && (G_IsAI(movingActor) || (ent->visflags & teamMask))) {
			forbiddenList.add(ent->pos, (byte*) &ent->fieldSize);
		} else if (ent->type == ET_SOLID) {
			for (int j = 0; j < ent->forbiddenListSize; j++) {
				forbiddenList.add(ent->forbiddenListPos[j], (byte*) &ent->fieldSize);
			}
		}
	}
}

/**
 * @brief Precalculates a move table for a given team and a given starting position.
 * This will calculate a routing table for all reachable fields with the given distance
 * from the given spot with the given actorsize
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] distance The distance in TUs to calculate the move for.
 * @param[in] movingActor The actor to calculate the move for
 */
void G_MoveCalc (int team, const Actor* movingActor, const pos3_t from, int distance)
{
	G_MoveCalcLocal(level.pathingMap, team, movingActor, from, distance);
}

/**
 * @brief Same as @c G_MoveCalc, except that it uses the pathing table passed as the first param
 * @param[in] pt the pathfinding table
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] distance The distance in TUs to calculate the move for.
 * @param[in] movingActor The actor to calculate the move for
 */
void G_MoveCalcLocal (pathing_t* pt, int team, const Edict* movingActor, const pos3_t from, int distance)
{
	G_BuildForbiddenList(team, movingActor);
	gi.GridCalcPathing(movingActor->fieldSize, pt, from, distance, &forbiddenList);
}

bool G_FindPath (int team, const Edict* movingActor, const pos3_t from, const pos3_t targetPos, bool crouched, int maxTUs)
{
	G_BuildForbiddenList(team, movingActor);
	return gi.GridFindPath(movingActor->fieldSize, level.pathingMap, from, targetPos, crouched, maxTUs, &forbiddenList);
}

/**
 * @brief Let an actor fall down if e.g. the func_breakable the actor was standing on was destroyed.
 * @param[in,out] ent The actor that should fall down
 * @todo Handle cases where the grid position the actor would fall to is occupied by another actor already.
 */
void G_ActorFall (Edict* ent)
{
	const pos_t oldZ = ent->pos[2];

	ent->pos[2] = gi.GridFall(ent->fieldSize, ent->pos);

	if (oldZ == ent->pos[2])
		return;

	Edict* entAtPos = G_GetEdictFromPos(ent->pos, ET_NULL);
	if (entAtPos != nullptr && (G_IsBreakable(entAtPos) || G_IsBlockingMovementActor(entAtPos))) {
		const int diff = oldZ - ent->pos[2];
		G_TakeDamage(entAtPos, (int)(FALLING_DAMAGE_FACTOR * (float)diff));
	}

	ent->calcOrigin();
	gi.LinkEdict(ent);

	G_CheckVis(ent);

	G_EventActorFall(*ent);

	G_EventEnd();
}

/**
 * @brief Checks whether the actor should stop movement
 * @param ent The actors edict
 * @param visState The visibility check state @c VIS_PERISH, @c VIS_APPEAR
 * @param dvtab The direction vectors
 * @param max The index of the next step in dvtab
 * @return @c true if the actor should stop movement, @c false otherwise
 */
static bool G_ActorShouldStopInMidMove (const Edict* ent, int visState, dvec_t* dvtab, int max)
{
	if (visState & VIS_STOP)
		return true;

	/* check that the appearing unit is not on a grid position the actor wanted to walk to.
	 * this might be the case if the edict got visible in mid mode */
	if (visState & VIS_APPEAR) {
		pos3_t pos;
		VectorCopy(ent->pos, pos);
		while (max >= 0) {
			int tmp = 0;

			PosAddDV(pos, tmp, dvtab[max]);
			max--;
			const Actor* blockEdict = G_EdictsGetLivingActorFromPos(pos);

			if (blockEdict && G_IsBlockingMovementActor(blockEdict)) {
				const bool visible = G_IsVisibleForTeam(blockEdict, ent->getTeam());
				if (visible)
					return true;
			}
		}
	}
	return false;
}

static void G_SendFootstepSound (Edict* ent, const int contentFlags)
{
	const char* snd = nullptr;

	if (contentFlags & CONTENTS_WATER) {
		if (ent->contentFlags & CONTENTS_WATER) {
			/* looks like we already are in the water */
			/* send water moving sound */
			snd = "footsteps/water_under";
		} else {
			/* send water entering sound */
			snd = "footsteps/water_in";
		}
	} else if (ent->contentFlags & CONTENTS_WATER) {
		/* send water leaving sound */
		snd = "footsteps/water_out";
	} else if (Q_strvalid(ent->chr.teamDef->footstepSound)) {
		/* some teams have a fixed footstep or moving sound */
		snd = ent->chr.teamDef->footstepSound;
	} else {
		/* we should really hit the ground with this */
		const vec3_t to = {ent->origin[0], ent->origin[1], ent->origin[2] - UNIT_HEIGHT};
		const trace_t trace = G_Trace(Line(ent->origin, to), nullptr, MASK_SOLID);
		if (trace.surface) {
			snd = gi.GetFootstepSound(trace.surface->name);
		}
	}
	if (snd != nullptr) {
		G_EventSpawnFootstepSound(*ent, snd);
	}
}

/**
 * @brief Writes a step of the move event to the net
 * @param[in] ent Edict to move
 * @param[in] stepAmount Pointer to the amount of steps in this move-event
 * @param[in] dvec The direction vector for the step to be added
 * @param[in] contentFlags The material we are walking over
 */
static void G_WriteStep (Actor* ent, const int dvec, const int contentFlags)
{
	/* write move header if not yet done */
	if (gi.GetEvent() != EV_ACTOR_MOVE) {
		G_EventAdd(PM_ALL, EV_ACTOR_MOVE, ent->getIdNum());
	}

	if (ent->moveinfo.steps >= MAX_ROUTE) {
		ent->moveinfo.steps = 0;
	}
	gi.WriteByte(ent->moveinfo.steps);

	/* write move header and always one step after another - because the next step
	 * might already be the last one due to some stop event */
	gi.WriteShort(dvec);
	gi.WriteShort(ent->speed);
	gi.WriteShort(contentFlags);

	/* Send the sound effect to everyone who's not seeing the actor */
	if (!ent->isCrouched()) {
		G_SendFootstepSound(ent, contentFlags);
	}

	ent->contentFlags = contentFlags;

	ent->moveinfo.steps++;
}

int G_FillDirectionTable (dvec_t* dvtab, size_t dvtabSize, byte crouchingState, pos3_t pos)
{
	int dvec;
	int numdv = 0;
	while ((dvec = gi.MoveNext(level.pathingMap, pos, crouchingState)) != ROUTING_UNREACHABLE) {
		const int oldZ = pos[2];
		/* dvec indicates the direction traveled to get to the new cell and the original cell height. */
		/* We are going backwards to the origin. */
		PosSubDV(pos, crouchingState, dvec);
		/* Replace the z portion of the DV value so we can get back to where we were. */
		dvtab[numdv++] = setDVz(dvec, oldZ);
		if (numdv >= dvtabSize)
			break;
	}

	return numdv;
}

/**
* @brief Return the needed TUs to walk to a given position
* @param actor Edict to calculate move length for
* @param path Pointer to pathing table
* @param to Position to walk to
* @param stored Use the stored mask (the cached move) of the routing data
* @return ROUTING_NOT_REACHABLE if the move isn't possible, length of move otherwise (TUs)
*/
pos_t G_ActorMoveLength (const Actor* actor, const pathing_t* path, const pos3_t to, bool stored)
{
	byte crouchingState = actor->isCrouched() ? 1 : 0;
	const pos_t length = gi.MoveLength(path, to, crouchingState, stored);

	if (!length || length == ROUTING_NOT_REACHABLE)
		return length;

	pos3_t pos;
	VectorCopy(to, pos);
	int dvec, numSteps = 0;
	while ((dvec = gi.MoveNext(level.pathingMap, pos, crouchingState)) != ROUTING_UNREACHABLE) {
		++numSteps;
		PosSubDV(pos, crouchingState, dvec); /* We are going backwards to the origin. */
	}

	const bool useAutostand = crouchingState && (G_IsAI(actor) || actor->getPlayer().autostand)
				&& gi.GridShouldUseAutostand(path, to);
	const int autostandTU = useAutostand ? 2 * TU_CROUCH : 0;

	return std::min(ROUTING_NOT_REACHABLE, length + static_cast<int>(numSteps *
			G_ActorGetInjuryPenalty(actor, MODIFIER_MOVEMENT)) + autostandTU);
}

/**
 * @brief Generates the client events that are send over the netchannel to move an actor
 * @param[in] player Player who is moving an actor
 * @param[in] visTeam The team to check the visibility for - if this is 0 we build the forbidden list
 * above all edicts - for the human controlled actors this would mean that clicking to a grid
 * position that is not reachable because an invisible actor is standing there would not result in
 * a single step - as the movement is aborted before. For AI movement this is in general @c 0 - but
 * not if they e.g. hide.
 * @param[in] actor Edict to move
 * @param[in] to The grid position to walk to
 * @sa CL_ActorStartMove
 * @sa PA_MOVE
 */
void G_ClientMove (const Player& player, int visTeam, Actor* actor, const pos3_t to)
{
	if (VectorCompare(actor->pos, to))
		return;

	/* check if action is possible */
	if (!G_ActionCheckForCurrentTeam(player, actor, TU_MOVE_STRAIGHT))
		return;

	bool autoCrouchRequired = false;
	byte crouchingState = actor->isCrouched() ? 1 : 0;

	/* calculate move table */
	G_MoveCalc(visTeam, actor, actor->pos, actor->TU);

	/* Autostand: check if the actor is crouched and player wants autostanding...*/
	if (crouchingState && (G_IsAI(actor) || player.autostand)) {
		/* ...and if this is a long walk... */
		if (gi.CanActorStandHere(actor->fieldSize, actor->pos)
			&& gi.GridShouldUseAutostand(level.pathingMap, to)) {
			/* ...make them stand first. If the player really wants them to walk a long
			 * way crouched, he can move the actor in several stages.
			 * Uses the threshold at which standing, moving and crouching again takes
			 * fewer TU than just crawling while crouched. */
			G_ClientStateChange(player, actor, STATE_CROUCHED, true); /* change to stand state */
			crouchingState = actor->isCrouched() ? 1 : 0;
			if (!crouchingState)
				autoCrouchRequired = true;
		}
	}

	const pos_t length = std::min(G_ActorMoveLength(actor, level.pathingMap, to, false)
				+ (autoCrouchRequired ? TU_CROUCH : 0), ROUTING_NOT_REACHABLE);
	/* length of ROUTING_NOT_REACHABLE means not reachable */
	if (length && length >= ROUTING_NOT_REACHABLE)
		return;

	/* assemble dvec-encoded move data */
	pos3_t pos;
	VectorCopy(to, pos);

	dvec_t dvtab[MAX_ROUTE];
	byte numdv = G_FillDirectionTable(dvtab, lengthof(dvtab), crouchingState, pos);

	/* make sure to end any other pending events - we rely on EV_ACTOR_MOVE not being active anymore */
	G_EventEnd();

	/* everything ok, found valid route? */
	if (VectorCompare(pos, actor->pos)) {
		/* no floor inventory at this point */
		actor->resetFloor();

		if (!G_IsCivilian(actor))
			G_EventMoveCameraTo(G_VisToPM(actor->visflags & ~G_TeamToVisMask(actor->getTeam())), actor->pos);

		actor->moveinfo.steps = 0;
		G_ReactionFireNotifyClientStartMove(actor);
		const int initTU = actor->TU;
		int usedTUs = 0;
		const int movingModifier = G_ActorGetInjuryPenalty(actor, MODIFIER_MOVEMENT);
		while (numdv > 0) {
			const int step = actor->moveinfo.steps;
			const byte oldDir = actor->dir;

			/* get next dvec */
			numdv--;
			const int dvec = dvtab[numdv];
			/* This is the direction to make the step into */
			const int dir = getDVdir(dvec);

			/* turn around first */
			int status = G_ActorDoTurn(actor, dir);
			if ((status & VIS_STOP) && visTeam != 0) {
				autoCrouchRequired = false;
				if (step == 0) {
					usedTUs += TU_TURN;
				}
				break;
			}

			if (visTeam != 0 && G_ActorShouldStopInMidMove(actor, status, dvtab, numdv)) {
				/* don't autocrouch if new enemy becomes visible */
				autoCrouchRequired = false;
				/* if something appears on our route that didn't trigger a VIS_STOP, we have to
				 * send the turn event if this is our first step */
				if (oldDir != actor->dir && step == 0) {
					G_EventActorTurn(*actor);
					usedTUs += TU_TURN;
				}
				break;
			}

			/* decrease TUs */
			const float div = gi.GetTUsForDirection(dir, actor->isCrouched());
			const int stepTUs = div + movingModifier;
			if (usedTUs + stepTUs > actor->TU)
				break;
			usedTUs += stepTUs;

			/* This is now a flag to indicate a change in crouching - we need this for
			 * the stop in mid move call(s), because we need the updated entity position */
			int crouchFlag = 0;
			/* Calculate the new position after the decrease in TUs, otherwise the game
			 * remembers the false position if the time runs out */
			PosAddDV(actor->pos, crouchFlag, dvec);

			/* slower if crouched */
			if (actor->isCrouched())
				actor->speed = ACTOR_SPEED_CROUCHED;
			else
				actor->speed = ACTOR_SPEED_NORMAL;
			actor->speed *= g_actorspeed->value;

			const int oldState = actor->state;
			const int oldHP = actor->HP;
			const int oldSTUN = actor->getStun();
			if (crouchFlag == 0) { /* No change in crouch */
				actor->calcOrigin();

				/* link it at new position - this must be done for every edict
				 * movement - to let the server know about it. */
				gi.LinkEdict(actor);

				/* Only the PHALANX team has these stats right now. */
				if (actor->chr.scoreMission) {
					const float truediv = gi.GetTUsForDirection(dir, 0);		/* regardless of crouching ! */
					if (actor->isCrouched())
						actor->chr.scoreMission->movedCrouched += truediv;
					else
						actor->chr.scoreMission->movedNormal += truediv;
				}
				/* write the step to the net */
				const int contentFlags = G_ActorGetContentFlags(actor->origin);
				G_WriteStep(actor, dvec, contentFlags);

				status = 0;

				/* Set actor->TU because the reaction code relies on actor->TU being accurate. */
				G_ActorSetTU(actor, initTU - usedTUs);

				Edict* clientAction = actor->clientAction;
				/* check triggers at new position */
				if (G_TouchTriggers(actor)) {
					if (!clientAction)
						status |= VIS_STOP;
				}

				/* check if player appears/perishes, seen from other teams */
				G_CheckVis(actor);

				/* check for anything appearing, seen by "the moving one" */
				status |= G_CheckVisTeamAll(actor->getTeam(), 0, actor);

				G_TouchSolids(actor, 10.0f);

				/* state has changed - maybe we walked on a trigger_hurt */
				if (oldState != actor->state || oldHP != actor->HP || oldSTUN != actor->getStun())
					status |= VIS_STOP;
			} else if (crouchFlag == 1) {
				/* Actor is standing */
				G_ClientStateChange(player, actor, STATE_CROUCHED, true);
			} else if (crouchFlag == -1) {
				/* Actor is crouching and should stand up */
				G_ClientStateChange(player, actor, STATE_CROUCHED, false);
			}

			/* check for reaction fire */
			if (G_ReactionFireOnMovement(actor, step)) {
				status |= VIS_STOP;

				autoCrouchRequired = false;
			}

			/* check for death */
			if (oldHP != actor->HP || oldSTUN != actor->getStun() || oldState != actor->state) {
				/** @todo Handle dazed via trigger_hurt */
				/* maybe this was due to rf - then the G_ActorDie was already called */
				if (!actor->isDead()) {
					G_CheckDeathOrKnockout(actor, nullptr, nullptr, (oldHP - actor->HP) + (actor->getStun() - oldSTUN));
				}
				G_MatchEndCheck();
				autoCrouchRequired = false;
				break;
			}

			if (visTeam != 0 && G_ActorShouldStopInMidMove(actor, status, dvtab, numdv - 1)) {
				/* don't autocrouch if new enemy becomes visible */
				autoCrouchRequired = false;
				break;
			}

			/* Restore actor->TU because the movement code relies on it not being modified! */
			G_ActorSetTU(actor, initTU);
		}

		/* submit the TUs / round down */
		G_ActorSetTU(actor, initTU - usedTUs);

		G_SendStats(*actor);

		/* end the move */
		G_GetFloorItems(actor);
		G_EventEnd();
	}

	if (autoCrouchRequired) {
		/* toggle back to crouched state */
		G_ClientStateChange(player, actor, STATE_CROUCHED, true);
	}

	G_ReactionFireNotifyClientEndMove(actor);
}
