/**
 * @file cl_actor.h
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

#ifndef CLIENT_CL_ACTOR_H
#define CLIENT_CL_ACTOR_H

/* vertical cursor offset */
#define CURSOR_OFFSET UNIT_HEIGHT * 0.4
/* distance from vertical center of grid-point to head when standing */
#define EYE_HT_STAND  UNIT_HEIGHT * 0.25
/* distance from vertical center of grid-point to head when crouched */
#define EYE_HT_CROUCH UNIT_HEIGHT * 0.06

/** @sa moveModeDescriptions */
typedef enum {
	WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH,
	WALKTYPE_AUTOSTAND_BEING_USED,
	WALKTYPE_WALKING,
	WALKTYPE_CROUCH_WALKING,

	WALKTYPE_MAX
} walkType_t;

extern le_t *selActor;
extern pos3_t truePos;
extern pos3_t mousePos;

#define ACTOR_GET_FIELDSIZE(actor) ((actor != NULL) ? (actor)->fieldSize : ACTOR_SIZE_NORMAL)

void MSG_Write_PA(player_action_t player_action, int num, ...);

void ACTOR_InitStartup(void);

const char *CL_ActorGetSkillString(const int skill);

int CL_ActorCheckAction(const le_t *le);
void CL_ActorInvMove(const le_t *le, containerIndex_t fromContainer, int fromX, int fromY, containerIndex_t toContainer, int toX, int toY);

character_t *CL_ActorGetChr(const le_t *le);
int CL_ActorUsableTUs(const le_t *le);
int CL_ActorReservedTUs(const le_t *le, reservation_types_t type);
void CL_ActorReserveTUs(const le_t *le, reservation_types_t type, int tus);

int CL_ActorMoveMode(const le_t *le, int length);
void CL_ActorSetMode(le_t *actor, actorModes_t actorMode);
qboolean CL_ActorFireModeActivated(const actorModes_t mode);
void CL_ActorConditionalMoveCalc(le_t *le);
qboolean CL_ActorSelect(le_t *le);
qboolean CL_ActorSelectList(int num);
qboolean CL_ActorSelectNext(void);
qboolean CL_ActorSelectPrev(void);
void CL_ActorAddToTeamList(le_t *le);
void CL_ActorRemoveFromTeamList(le_t *le);
void CL_ActorCleanup(le_t *le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(le_t *le, containerIndex_t containerID);
void CL_ActorTurnMouse(void);
void CL_ActorDoTurn(struct dbuffer *msg);
void CL_ActorStartMove(le_t *le, const pos3_t to);
void CL_ActorShoot(const le_t *le, const pos3_t at);
int CL_ActorGetContainerForReload(invList_t **ic, const inventory_t *inv, const objDef_t *weapon);
void CL_ActorPlaySound(const le_t *le, actorSound_t soundType);

void CL_ActorActionMouse(void);
void CL_ActorSetFireDef(le_t *actor, const fireDef_t *fd);

void CL_NextRound_f(void);

void CL_ResetMouseLastPos(void);
void CL_ActorResetMoveLength(le_t *le);
qboolean CL_ActorMouseTrace(void);

qboolean CL_AddActor(le_t *le, entity_t *ent);
qboolean CL_AddUGV(le_t *le, entity_t *ent);

void CL_AddTargeting(void);
void CL_AddPathing(void);
void CL_AddActorPathing(void);
void CL_ActorTargetAlign_f(void);

void CL_ActorSetShotSettings(character_t *chr, actorHands_t hand, int fireModeIndex, const objDef_t *weapon);
void CL_ActorSetRFMode(character_t *chr, actorHands_t hand, int fireModeInde, const objDef_t *weapon);

void CL_DisplayFloorArrows(void);
void CL_DisplayObstructionArrows(void);
#endif
