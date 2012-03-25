/**
 * @file g_move.c
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

#define ACTOR_SPEED_NORMAL 100
#define ACTOR_SPEED_CROUCHED (ACTOR_SPEED_NORMAL / 2)
static const float FALLING_DAMAGE_FACTOR = 10.0f;

/**
 * @brief The forbidden list is a list of entity positions that are occupied by an entity.
 * This list is checked everytime an actor wants to walk there.
 */
static pos_t *forbiddenList[MAX_FORBIDDENLIST];
static int forbiddenListLength;

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
static void G_BuildForbiddenList (int team, const edict_t *movingActor)
{
	edict_t *ent = NULL;
	int visMask;

	forbiddenListLength = 0;

	/* team visibility */
	if (team)
		visMask = G_TeamToVisMask(team);
	else
		visMask = TEAM_ALL;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		/* Dead 2x2 unit will stop walking, too. */
		if (G_IsBlockingMovementActor(ent) && (G_IsAI(movingActor) || (ent->visflags & visMask))) {
			forbiddenList[forbiddenListLength++] = ent->pos;
			forbiddenList[forbiddenListLength++] = (byte*) &ent->fieldSize;
		} else if (ent->type == ET_SOLID) {
			int j;
			for (j = 0; j < ent->forbiddenListSize; j++) {
				forbiddenList[forbiddenListLength++] = ent->forbiddenListPos[j];
				forbiddenList[forbiddenListLength++] = (byte*) &ent->fieldSize;
			}
		}
	}

	if (forbiddenListLength > MAX_FORBIDDENLIST)
		gi.Error("G_BuildForbiddenList: list too long\n");
}

/**
 * @brief Precalculates a move table for a given team and a given starting position.
 * This will calculate a routing table for all reachable fields with the given distance
 * from the given spot with the given actorsize
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] crouchingState The crouching state of the actor. 0=stand, 1=crouch
 * @param[in] distance The distance in TUs to calculate the move for.
 * @param[in] movingActor The actor to calculate the move for
 * @sa G_BuildForbiddenList
 * @sa G_MoveCalcLocal
 */
void G_MoveCalc (int team, const edict_t *movingActor, const pos3_t from, byte crouchingState, int distance)
{
	G_MoveCalcLocal(level.pathingMap, team, movingActor, from, crouchingState, distance);
}

/**
 * @brief Same as @c G_MoveCalc, except that it uses the pathing table passed as the first param
 * @param[in] pt the pathfinding table
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] crouchingState The crouching state of the actor. 0=stand, 1=crouch
 * @param[in] distance The distance in TUs to calculate the move for.
 * @param[in] movingActor The actor to calculate the move for
 * @sa G_MoveCalc
 * @sa G_BuildForbiddenList
 */
void G_MoveCalcLocal (pathing_t *pt, int team, const edict_t *movingActor, const pos3_t from, byte crouchingState, int distance)
{
	G_BuildForbiddenList(team, movingActor);
	gi.MoveCalc(gi.routingMap, movingActor->fieldSize, pt, from, crouchingState, distance,
			forbiddenList, forbiddenListLength);
}

/**
 * @brief Let an actor fall down if e.g. the func_breakable the actor was standing on was destroyed.
 * @param[in,out] ent The actor that should fall down
 * @todo Handle cases where the grid position the actor would fall to is occupied by another actor already.
 */
void G_ActorFall (edict_t *ent)
{
	edict_t* entAtPos;
	const int oldZ = ent->pos[2];

	ent->pos[2] = gi.GridFall(gi.routingMap, ent->fieldSize, ent->pos);

	if (oldZ == ent->pos[2])
		return;

	entAtPos = G_GetEdictFromPos(ent->pos, ET_NULL);
	if (entAtPos != NULL && (G_IsBreakable(entAtPos) || G_IsBlockingMovementActor(entAtPos))) {
		const int diff = oldZ - ent->pos[2];
		G_TakeDamage(entAtPos, (int)(FALLING_DAMAGE_FACTOR * (float)diff));
	}

	G_EdictCalcOrigin(ent);
	gi.LinkEdict(ent);

	G_CheckVis(ent, qtrue);

	G_EventActorFall(ent);

	gi.EndEvents();
}

/**
 * @brief Checks whether the actor should stop movement
 * @param ent The actors edict
 * @param visState The visibility check state @c VIS_PERISH, @c VIS_APPEAR
 * @return @c true if the actor should stop movement, @c false otherwise
 */
static qboolean G_ActorShouldStopInMidMove (const edict_t *ent, int visState, dvec_t* dvtab, int max)
{
	if (visState & VIS_STOP)
		 return qtrue;

	 /* check that the appearing unit is not on a grid position the actor wanted to walk to.
	  * this might be the case if the edict got visible in mid mode */
	 if (visState & VIS_APPEAR) {
		 pos3_t pos;
		 VectorCopy(ent->pos, pos);
		 while (max >= 0) {
			 int tmp = 0;
			 const edict_t *blockEdict;

			 PosAddDV(pos, tmp, dvtab[max]);
			 max--;
			 blockEdict = G_GetLivingActorFromPos(pos);

			 if (blockEdict && G_IsBlockingMovementActor(blockEdict)) {
				 const qboolean visible = G_IsVisibleForTeam(blockEdict, ent->team);
				 if (visible)
					 return qtrue;
			 }
		 }
	 }
	 return qfalse;
}

/**
 * @brief Writes a step of the move event to the net
 * @param[in] ent Edict to move
 * @param[in] stepAmount Pointer to the amount of steps in this move-event
 * @param[in] dvec The direction vector for the step to be added
 * @param[in] contentFlags The material we are walking over
 */
static void G_WriteStep (edict_t* ent, byte** stepAmount, const int dvec, const int contentFlags)
{
	/* write move header if not yet done */
	if (gi.GetEvent() != EV_ACTOR_MOVE) {
		gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
		gi.WriteShort(ent->number);
		/* stepAmount is a pointer to a location in the netchannel
		 * the value of this pointer depends on how far the actor walks
		 * and this might be influenced at a later stage inside this
		 * loop. That's why we can modify the value of this byte
		 * if e.g. a VIS_STOP occurred and no more steps should be made.
		 * But keep in mind, that no other events might be between
		 * this event and its successful end - otherwise the
		 * stepAmount pointer would no longer be valid and you would
		 * modify data in the new event. */
		*stepAmount = gi.WriteDummyByte(0);
		/* Add three more dummy bytes.  These will be the final actor position. */
		gi.WriteDummyByte(0); /* x */
		gi.WriteDummyByte(0); /* y */
		gi.WriteDummyByte(0); /* z */
	} else if (!*stepAmount) {
		gi.DPrintf("Event %i activate and no stepAmount pointer set\n", gi.GetEvent());
		return;
	}

	/* the moveinfo stuff is used inside the G_PhysicsStep think function */
	if (ent->moveinfo.steps >= MAX_DVTAB) {
		ent->moveinfo.steps = 0;
		ent->moveinfo.currentStep = 0;
	}
	ent->moveinfo.contentFlags[ent->moveinfo.steps] = contentFlags;
	ent->moveinfo.visflags[ent->moveinfo.steps] = ent->visflags;
	ent->moveinfo.steps++;

	/* store steps in netchannel */
	byte *pStep = *stepAmount;
	(*pStep)++;
	/* store the position too */
	*(pStep + 1) = ent->pos[0];
	*(pStep + 2) = ent->pos[1];
	*(pStep + 3) = ent->pos[2];

	/* write move header and always one step after another - because the next step
	 * might already be the last one due to some stop event */
	gi.WriteShort(dvec);
	gi.WriteShort(ent->speed);
	gi.WriteShort(contentFlags);
}

static int G_FillDirectionTable (dvec_t *dvtab, size_t size, byte crouchingState, pos3_t pos)
{
	int dvec;
	int numdv = 0;
	while ((dvec = gi.MoveNext(level.pathingMap, pos, crouchingState))
			!= ROUTING_UNREACHABLE) {
		const int oldZ = pos[2];
		/* dvec indicates the direction traveled to get to the new cell and the original cell height. */
		/* We are going backwards to the origin. */
		PosSubDV(pos, crouchingState, dvec);
		/* Replace the z portion of the DV value so we can get back to where we were. */
		dvtab[numdv++] = setDVz(dvec, oldZ);
		if (numdv >= size)
			break;
	}

	return numdv;
}

/**
 * @brief Generates the client events that are send over the netchannel to move an actor
 * @param[in] player Player who is moving an actor
 * @param[in] visTeam The team to check the visibility for - if this is 0 we build the forbidden list
 * above all edicts - for the human controlled actors this would mean that clicking to a grid
 * position that is not reachable because an invisible actor is standing there would not result in
 * a single step - as the movement is aborted before. For AI movement this is in general @c 0 - but
 * not if they e.g. hide.
 * @param[in] ent Edict to move
 * @param[in] to The grid position to walk to
 * @sa CL_ActorStartMove
 * @sa PA_MOVE
 */
void G_ClientMove (const player_t * player, int visTeam, edict_t* ent, const pos3_t to)
{
	int status, initTU;
	dvec_t dvtab[MAX_DVTAB];
	int dir;
	byte numdv, length;
	pos3_t pos;
	float div;
	int oldState;
	int oldHP;
	qboolean autoCrouchRequired = qfalse;
	byte crouchingState;

	if (VectorCompare(ent->pos, to))
		return;

	/* check if action is possible */
	if (!G_ActionCheckForCurrentTeam(player, ent, TU_MOVE_STRAIGHT))
		return;

	crouchingState = G_IsCrouched(ent) ? 1 : 0;
	oldState = oldHP = 0;

	/* calculate move table */
	G_MoveCalc(visTeam, ent, ent->pos, crouchingState, ent->TU);
	length = gi.MoveLength(level.pathingMap, to, crouchingState, qfalse);

	/* length of ROUTING_NOT_REACHABLE means not reachable */
	if (length && length >= ROUTING_NOT_REACHABLE)
		return;

	/* Autostand: check if the actor is crouched and player wants autostanding...*/
	if (crouchingState && player->autostand) {
		/* ...and if this is a long walk... */
		if (SHOULD_USE_AUTOSTAND(length)) {
			/* ...make them stand first. If the player really wants them to walk a long
			 * way crouched, he can move the actor in several stages.
			 * Uses the threshold at which standing, moving and crouching again takes
			 * fewer TU than just crawling while crouched. */
			G_ClientStateChange(player, ent, STATE_CROUCHED, qtrue); /* change to stand state */
			crouchingState = G_IsCrouched(ent) ? 1 : 0;
			if (!crouchingState) {
				G_MoveCalc(visTeam, ent, ent->pos, crouchingState, ent->TU);
				length = gi.MoveLength(level.pathingMap, to, crouchingState, qfalse);
				autoCrouchRequired = qtrue;
			}
		}
	}

	/* this let the footstep sounds play even over network */
	ent->think = G_PhysicsStep;
	ent->nextthink = level.time;

	/* assemble dvec-encoded move data */
	VectorCopy(to, pos);
	initTU = ent->TU;

	numdv = G_FillDirectionTable(dvtab, lengthof(dvtab), crouchingState, pos);

	/* make sure to end any other pending events - we rely on EV_ACTOR_MOVE not being active anymore */
	gi.EndEvents();

	/* everything ok, found valid route? */
	if (VectorCompare(pos, ent->pos)) {
		byte* stepAmount = NULL;
		int usedTUs = 0;
		/* no floor inventory at this point */
		FLOOR(ent) = NULL;

		while (numdv > 0) {
			/* A flag to see if we needed to change crouch state */
			int crouchFlag;
			const byte oldDir = ent->dir;
			int dvec;

			/* get next dvec */
			numdv--;
			dvec = dvtab[numdv];
			/* This is the direction to make the step into */
			dir = getDVdir(dvec);

			/* turn around first */
			status = G_ActorDoTurn(ent, dir);
			if (status & VIS_STOP) {
				autoCrouchRequired = qfalse;
				if (ent->moveinfo.steps == 0)
					usedTUs += TU_TURN;
				break;
			}

			if (G_ActorShouldStopInMidMove(ent, status, dvtab, numdv)) {
				/* don't autocrouch if new enemy becomes visible */
				autoCrouchRequired = qfalse;
				/* if something appears on our route that didn't trigger a VIS_STOP, we have to
				 * send the turn event if this is our first step */
				if (oldDir != ent->dir && ent->moveinfo.steps == 0) {
					G_EventActorTurn(ent);
					usedTUs += TU_TURN;
				}
				break;
			}

			/* decrease TUs */
			div = gi.GetTUsForDirection(dir, G_IsCrouched(ent));
			if ((int) (usedTUs + div) > ent->TU)
				break;
			usedTUs += div;

			/* This is now a flag to indicate a change in crouching - we need this for
			 * the stop in mid move call(s), because we need the updated entity position */
			crouchFlag = 0;
			/* Calculate the new position after the decrease in TUs, otherwise the game
			 * remembers the false position if the time runs out */
			PosAddDV(ent->pos, crouchFlag, dvec);

			/* slower if crouched */
			if (G_IsCrouched(ent))
				ent->speed = ACTOR_SPEED_CROUCHED;
			else
				ent->speed = ACTOR_SPEED_NORMAL;
			ent->speed *= g_actorspeed->value;

			if (crouchFlag == 0) { /* No change in crouch */
				edict_t* clientAction;
				int contentFlags;
				vec3_t pointTrace;

				G_EdictCalcOrigin(ent);
				VectorCopy(ent->origin, pointTrace);
				pointTrace[2] += PLAYER_MIN;

				contentFlags = gi.PointContents(pointTrace);

				/* link it at new position - this must be done for every edict
				 * movement - to let the server know about it. */
				gi.LinkEdict(ent);

				/* Only the PHALANX team has these stats right now. */
				if (ent->chr.scoreMission) {
					float truediv = gi.GetTUsForDirection(dir, 0);		/* regardless of crouching ! */
					if (G_IsCrouched(ent))
						ent->chr.scoreMission->movedCrouched += truediv;
					else
						ent->chr.scoreMission->movedNormal += truediv;
				}
				/* write the step to the net */
				G_WriteStep(ent, &stepAmount, dvec, contentFlags);

				/* check if player appears/perishes, seen from other teams */
				G_CheckVis(ent, qtrue);

				/* check for anything appearing, seen by "the moving one" */
				status = G_CheckVisTeamAll(ent->team, qfalse, ent);

				/* Set ent->TU because the reaction code relies on ent->TU being accurate. */
				G_ActorSetTU(ent, initTU - usedTUs);

				clientAction = ent->clientAction;
				oldState = ent->state;
				oldHP = ent->HP;
				/* check triggers at new position */
				if (G_TouchTriggers(ent)) {
					if (!clientAction)
						status |= VIS_STOP;
				}

				G_TouchSolids(ent, 10.0f);

				/* state has changed - maybe we walked on a trigger_hurt */
				if (oldState != ent->state)
					status |= VIS_STOP;
				else if (oldHP != ent->HP)
					status |= VIS_STOP;
			} else if (crouchFlag == 1) {
				/* Actor is standing */
				G_ClientStateChange(player, ent, STATE_CROUCHED, qtrue);
			} else if (crouchFlag == -1) {
				/* Actor is crouching and should stand up */
				G_ClientStateChange(player, ent, STATE_CROUCHED, qfalse);
			}

			/* check for reaction fire */
			if (G_ReactionFireOnMovement(ent)) {
				status |= VIS_STOP;

				autoCrouchRequired = qfalse;
			}

			/* check for death */
			if (((oldHP != 0 && oldHP != ent->HP) || (oldState != ent->state)) && !G_IsDazed(ent)) {
				/** @todo Handle dazed via trigger_hurt */
				/* maybe this was due to rf - then the G_ActorDie was already called */
				if (!G_IsDead(ent)) {
					G_CheckDeathOrKnockout(ent, NULL, NULL, oldHP - ent->HP);
				}
				return;
			}

			if (G_ActorShouldStopInMidMove(ent, status, dvtab, numdv - 1)) {
				/* don't autocrouch if new enemy becomes visible */
				autoCrouchRequired = qfalse;
				break;
			}

			/* Restore ent->TU because the movement code relies on it not being modified! */
			G_ActorSetTU(ent, initTU);
		}

		/* submit the TUs / round down */
		if (g_notu != NULL && g_notu->integer)
			G_ActorSetTU(ent, initTU);
		else
			G_ActorSetTU(ent, initTU - usedTUs);

		G_SendStats(ent);

		/* end the move */
		G_GetFloorItems(ent);
		gi.EndEvents();
	}

	if (autoCrouchRequired) {
		/* toggle back to crouched state */
		G_ClientStateChange(player, ent, STATE_CROUCHED, qtrue);
	}
}
