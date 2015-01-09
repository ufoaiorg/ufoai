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

#pragma once

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

extern le_t* selActor;
extern pos3_t truePos;
extern pos3_t mousePos;

#define ACTOR_GET_FIELDSIZE(actor) ((actor != nullptr) ? (actor)->fieldSize : ACTOR_SIZE_NORMAL)

void MSG_Write_PA(player_action_t player_action, int num, ...);

void ACTOR_InitStartup(void);

int CL_ActorCheckAction(const le_t* le);
void CL_ActorInvMove(const le_t* le, containerIndex_t fromContainer, int fromX, int fromY, containerIndex_t toContainer, int toX, int toY);

le_t* CL_ActorGetFromCharacter(const character_t* chr);
character_t* CL_ActorGetChr(const le_t* le);
int CL_ActorUsableTUs(const le_t* le);
int CL_ActorReservedTUs(const le_t* le, reservation_types_t type);
void CL_ActorReserveTUs(const le_t* le, reservation_types_t type, int tus);
bool CL_ActorIsReactionFireOutOfRange(const le_t* shooter, const le_t* target);
const fireDef_t* CL_ActorGetReactionFireFireDef(const le_t* shooter);

int CL_ActorMoveMode(const le_t* le);
void CL_ActorSetMode(le_t* actor, actorModes_t actorMode);
bool CL_ActorFireModeActivated(const actorModes_t mode);
void CL_ActorConditionalMoveCalc(le_t* le);
le_t* CL_ActorGetClosest(const vec3_t origin, int team);
bool CL_ActorSelect(le_t* le);
bool CL_ActorSelectList(int num);
bool CL_ActorSelectNext(void);
bool CL_ActorSelectPrev(void);
void CL_ActorAddToTeamList(le_t* le);
void CL_ActorRemoveFromTeamList(le_t* le);
void CL_ActorCleanup(le_t* le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(le_t* le, containerIndex_t containerID);
void CL_ActorTurnMouse(void);
void CL_ActorStartMove(le_t* le, const pos3_t to);
void CL_ActorShoot(const le_t* le, const pos3_t at);
int CL_ActorGetContainerForReload(Item** ic, const Inventory* inv, const objDef_t* weapon);
void CL_ActorPlaySound(const le_t* le, actorSound_t soundType);
float CL_ActorInjuryModifier(const le_t* le, const modifier_types_t type);
int CL_ActorTimeForFireDef(const le_t* le, const fireDef_t* fd, bool reaction = false);

void CL_ActorActionMouse(void);
void CL_ActorSetFireDef(le_t* actor, const fireDef_t* fd);

void CL_NextRound_f(void);

void CL_ResetMouseLastPos(void);
void CL_ActorResetMoveLength(le_t* le);
bool CL_ActorMouseTrace(void);
void CL_GetWorldCoordsUnderMouse(vec3_t groundIntersection, vec3_t upperTracePoint, vec3_t lowerTracePoint);
void CL_InitBattlescapeMouseDragging(void);
void CL_BattlescapeMouseDragging(void);

bool CL_AddActor(le_t* le, entity_t* ent);
bool CL_AddUGV(le_t* le, entity_t* ent);

int CL_ActorGetNumber(const le_t* le);

void CL_AddTargeting(void);
void CL_AddPathing(void);
void CL_AddActorPathing(void);
void CL_ActorTargetAlign_f(void);

void CL_DisplayFloorArrows(void);
void CL_DisplayObstructionArrows(void);
