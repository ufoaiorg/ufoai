/**
 * @file g_move.c
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

#define ACTOR_SPEED_NORMAL 100
#define ACTOR_SPEED_CROUCHED (ACTOR_SPEED_NORMAL / 2)

/**
 * @brief The forbidden list is a list of entity positions that are occupied by an entity.
 * This list is checked everytime an actor wants to walk there.
 */
static pos_t *forbiddenList[MAX_FORBIDDENLIST];
static int forbiddenListLength;

/**
 * @brief Build the forbidden list for the pathfinding (server side).
 * @param[in] team The team number if the list should be calculated from the eyes of that team. Use 0 to ignore team.
 * @sa G_MoveCalc
 * @sa Grid_CheckForbidden
 * @sa CL_BuildForbiddenList <- shares quite some code
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void G_BuildForbiddenList (int team)
{
	edict_t *ent;
	int visMask;
	int i;

	forbiddenListLength = 0;

	/* team visibility */
	if (team)
		visMask = G_TeamToVisMask(team);
	else
		visMask = TEAM_ALL;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (!ent->inuse)
			continue;
		/* Dead 2x2 unit will stop walking, too. */
		/**
		 * @todo Just a note for the future.
		 * If we get any that does not block the map when dead this is the place to look.
		 */
		if (((ent->type == ET_ACTOR && !G_IsDead(ent)) || ent->type == ET_ACTOR2x2) && (ent->visflags & visMask)) {
			forbiddenList[forbiddenListLength++] = ent->pos;
			forbiddenList[forbiddenListLength++] = (byte*) &ent->fieldSize;
		} else if (ent->type == ET_SOLID) {
			/* they should always be solid */
			int j;
			for (j = 0; j < ent->forbiddenListSize; j++) {
				forbiddenList[forbiddenListLength++] = ent->forbiddenListPos[j];
				forbiddenList[forbiddenListLength++] = (byte*) &ent->fieldSize;
			}
		}
	}

	if (forbiddenListLength > MAX_FORBIDDENLIST)
		gi.error("G_BuildForbiddenList: list too long\n");
}

/**
 * @brief Precalculates a move table for a given team and a given starting position.
 * This will calculate a routing table for all reachable fields with the given distance
 * from the given spot with the given actorsize
 * @param[in] team The current team (see G_BuildForbiddenList)
 * @param[in] actorSize
 * @param[in] from Position in the map to start the move-calculation from.
 * @param[in] crouchingState The crouching state of the actor. 0=stand, 1=crouch
 * @param[in] distance The distance to calculate the move for.
 * @sa G_BuildForbiddenList
 */
void G_MoveCalc (int team, pos3_t from, int actorSize, byte crouchingState, int distance)
{
	G_BuildForbiddenList(team);
	gi.MoveCalc(gi.routingMap, actorSize, gi.pathingMap, from, crouchingState, distance,
			forbiddenList, forbiddenListLength);
}

void G_ClientFall (const edict_t *ent)
{
	/** @todo see dvecs, PosSubDV and NewDVZ */
	int dv = 13;
	gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
	gi.WriteShort(ent->number);
	gi.WriteByte(1);
	gi.WriteByte(ent->pos[0]);
	gi.WriteByte(ent->pos[1]);
	gi.WriteByte(ent->pos[2]);
	gi.WriteByte(dv);
	gi.WriteShort(800); /* gravity */
	gi.WriteShort(0);
	gi.EndEvents();
}

/**
 * @brief Generates the client events that are send over the netchannel to move an actor
 * @param[in] player Player who is moving an actor
 * @param[in] visTeam The team to check the visibility for - if this is 0 we build the forbidden list
 * above all edicts - for the human controlled actors this would mean that clicking to a grid
 * position that is not reachable because an invisible actor is standing there would not result in
 * a single step - as the movement is aborted before. For AI movement this is in general @c 0 - but
 * not if they e.g. hide.
 * @param[in] num Edict index to move
 * @param[in] to The grid position to walk to
 * @param[in] stopOnVisStop qfalse means that VIS_STOP is ignored
 * @param[in] quiet Don't print the console message (G_ActionCheck) if quiet is true.
 * @sa CL_ActorStartMove
 * @sa PA_MOVE
 */
void G_ClientMove (player_t * player, int visTeam, const int num, pos3_t to, qboolean stopOnVisStop, qboolean quiet)
{
	edict_t *ent;
	int status, initTU;
	byte dvtab[MAX_DVTAB];
	int dv, dir;
	byte numdv, length;
	pos3_t pos;
	float div, truediv, tu;
	int contentFlags;
	vec3_t pointTrace;
	byte* stepAmount = NULL;
	qboolean triggers = qfalse;
	int oldState;
	qboolean autoCrouchRequired = qfalse;
	byte crouchingState;

	ent = g_edicts + num;
	crouchingState = ent->state & STATE_CROUCHED ? 1 : 0;
	oldState = 0;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, TU_MOVE_STRAIGHT, quiet))
		return;

	/* calculate move table */
	G_MoveCalc(visTeam, ent->pos, ent->fieldSize, crouchingState, MAX_ROUTE);
	length = gi.MoveLength(gi.pathingMap, to, crouchingState, qfalse);

	/* length of ROUTING_NOT_REACHABLE means not reachable */
	if (length && length >= ROUTING_NOT_REACHABLE)
		return;

	/* Autostand: check if the actor is crouched and player wants autostanding...*/
	if (crouchingState && player->autostand) {
		/* ...and if this is a long walk... */
		if ((float) (2 * TU_CROUCH) < (float) length * (TU_CROUCH_MOVING_FACTOR - 1.0f)) {
			/* ...make them stand first. If the player really wants them to walk a long
			 * way crouched, he can move the actor in several stages.
			 * Uses the threshold at which standing, moving and crouching again takes
			 * fewer TU than just crawling while crouched. */
			G_ClientStateChange(player, num, STATE_CROUCHED, qtrue); /* change to stand state */
			crouchingState = ent->state & STATE_CROUCHED ? 1 : 0;
			if (!crouchingState) {
				G_MoveCalc(visTeam, ent->pos, ent->fieldSize, crouchingState, MAX_ROUTE);
				length = gi.MoveLength(gi.pathingMap, to, crouchingState, qfalse);
				autoCrouchRequired = qtrue;
			}
		}
	}

	/* this let the footstep sounds play even over network */
	ent->think = G_PhysicsStep;
	ent->nextthink = level.time;

	/* assemble dv-encoded move data */
	VectorCopy(to, pos);
	numdv = 0;
	tu = 0;
	initTU = ent->TU;

	while ((dv = gi.MoveNext(gi.routingMap, ent->fieldSize, gi.pathingMap, pos, crouchingState))
			!= ROUTING_UNREACHABLE) {
		const int oldZ = pos[2];
		/* dv indicates the direction traveled to get to the new cell and the original cell height. */
		/* We are going backwards to the origin. */
		PosSubDV(pos, crouchingState, dv);
		/* Replace the z portion of the DV value so we can get back to where we were. */
		dvtab[numdv++] = NewDVZ(dv, oldZ);
		if (numdv >= lengthof(dvtab))
			break;
	}

	/* everything ok, found valid route? */
	if (VectorCompare(pos, ent->pos)) {
		/* no floor inventory at this point */
		FLOOR(ent) = NULL;

		while (numdv > 0) {
			/* A flag to see if we needed to change crouch state */
			int crouchFlag;

			/* get next dv */
			numdv--;
			dv = dvtab[numdv];
			/* This is the direction to make the step into */
			dir = getDVdir(dv);

			/* turn around first */
			status = G_ActorDoTurn(ent, dir);
			if (stopOnVisStop && (status & VIS_STOP))
				break;

			/* decrease TUs */
			div = gi.TUsUsed(dir);
			truediv = div;
			if ((ent->state & STATE_CROUCHED) && dir < CORE_DIRECTIONS)
				div *= TU_CROUCH_MOVING_FACTOR;
			if ((int) (tu + div) > ent->TU)
				break;
			tu += div;

			/* slower if crouched */
			if (ent->state & STATE_CROUCHED)
				ent->speed = ACTOR_SPEED_CROUCHED;
			else
				ent->speed = ACTOR_SPEED_NORMAL;
			ent->speed *= g_actorspeed->value;

			/* This is now a flag to indicate a change in crouching */
			crouchFlag = 0;
			PosAddDV(ent->pos, crouchFlag, dv);
			if (crouchFlag == 0) { /* No change in crouch */
				edict_t* clientAction;

				gi.GridPosToVec(gi.routingMap, ent->fieldSize, ent->pos, ent->origin);
				VectorCopy(ent->origin, pointTrace);
				pointTrace[2] += PLAYER_MIN;

				contentFlags = gi.PointContents(pointTrace);

				/* link it at new position - this must be done for every edict
				 * movement - to let the server know about it. */
				gi.LinkEdict(ent);

				/* Only the PHALANX team has these stats right now. */
				if (ent->chr.scoreMission) {
					if (ent->state & STATE_CROUCHED)
						ent->chr.scoreMission->movedCrouched += truediv;
					else
						ent->chr.scoreMission->movedNormal += truediv;
				}

				/* write move header if not yet done */
				if (gi.GetEvent() != EV_ACTOR_MOVE) {
					gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_MOVE);
					gi.WriteShort(num);
					/* stepAmount is a pointer to a location in the netchannel
					 * the value of this pointer depends on how far the actor walks
					 * and this might be influenced at a later stage inside this
					 * loop. That's why we can modify the value of this byte
					 * if e.g. a VIS_STOP occurred and no more steps should be made.
					 * But keep in mind, that no other events might be between
					 * this event and its successful end - otherwise the
					 * stepAmount pointer would no longer be valid and you would
					 * modify data in the new event. */
					stepAmount = gi.WriteDummyByte(0);
					/* Add three more dummy bytes.  These will be the final actor position. */
					gi.WriteDummyByte(0); /* x */
					gi.WriteDummyByte(0); /* y */
					gi.WriteDummyByte(0); /* z */
				} else if (!stepAmount) {
					gi.dprintf("Event %i activate and no stepAmount pointer set\n", gi.GetEvent());
					break;
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
				(*stepAmount)++;
				/* store the position too */
				*(stepAmount + 1) = ent->pos[0];
				*(stepAmount + 2) = ent->pos[1];
				*(stepAmount + 3) = ent->pos[2];

				/* write move header and always one step after another - because the next step
				 * might already be the last one due to some stop event */
				gi.WriteByte(dv);
				gi.WriteShort(ent->speed);
				gi.WriteShort(contentFlags);

				/* check if player appears/perishes, seen from other teams */
				G_CheckVis(ent, qtrue);

				/* check for anything appearing, seen by "the moving one" */
				status = G_CheckVisTeam(ent->team, NULL, qfalse, ent);

				/* Set ent->TU because the reaction code relies on ent->TU being accurate. */
				ent->TU = max(0, initTU - (int) tu);

				clientAction = ent->clientAction;
				oldState = ent->state;
				/* check triggers at new position but only if no actor appeared */
				if (G_TouchTriggers(ent)) {
					triggers = qtrue;
					if (!clientAction)
						status |= VIS_STOP;
				}
				/* state has changed - maybe we walked on a trigger_hurt */
				if (oldState != ent->state)
					status |= VIS_STOP;
			} else if (crouchFlag == 1) { /* Actor is standing */
				G_ClientStateChange(player, num, STATE_CROUCHED, qtrue);
			} else if (crouchFlag == -1) { /* Actor is crouching and should stand up */
				G_ClientStateChange(player, num, STATE_CROUCHED, qfalse);
			}

			/* check for reaction fire */
			if (G_ReactToMove(ent, qtrue)) {
				if (G_ReactToMove(ent, qfalse))
					status |= VIS_STOP;

				autoCrouchRequired = qfalse;
				/* if the attacker is invisible let the target turn in the shooting direction
-				 * of the attacker (@see G_ActorDoTurn) */
				G_ActorDoTurn(ent->reactionTarget, dir);
			}

			/* Restore ent->TU because the movement code relies on it not being modified! */
			ent->TU = initTU;

			/* check for death */
			if (oldState != ent->state && !(ent->state & STATE_DAZED)) {
				/** @todo Handle dazed via trigger_hurt */
				/* maybe this was due to rf - then the G_ActorDie was already called */
				if (!G_IsDead(ent))
					G_ActorDie(ent, ent->HP == 0 ? STATE_DEAD : STATE_STUN, NULL);
				return;
			}

			if (stopOnVisStop && (status & VIS_STOP))
				break;
		}

		/* now we can send other events again - the EV_ACTOR_MOVE event has ended */

		/* submit the TUs / round down */
		if (g_notu != NULL && g_notu->integer)
			ent->TU = initTU;
		else
			ent->TU = max(0, initTU - (int) tu);

		G_SendStats(ent);

		/* only if triggers are touched - there was a client
		 * action set and there were steps made */
		if (!triggers && ent->clientAction) {
			/* no triggers, no client action */
			ent->clientAction = NULL;
			gi.AddEvent(G_TeamToPM(ent->team), EV_RESET_CLIENT_ACTION);
			gi.WriteShort(ent->number);
		}

		/* end the move */
		G_GetFloorItems(ent);
		gi.EndEvents();
	}

	if (autoCrouchRequired) {
		/* toggle back to crouched state */
		G_ClientStateChange(player, num, STATE_CROUCHED, qtrue);
	}
}
