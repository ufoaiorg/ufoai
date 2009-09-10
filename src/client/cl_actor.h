/**
 * @file cl_actor.h
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

#ifndef CLIENT_CL_ACTOR_H
#define CLIENT_CL_ACTOR_H

#include "client.h"
#include "cl_le.h"

/* vertical cursor offset */
#define CURSOR_OFFSET UNIT_HEIGHT * 0.4
/* distance from vertical center of grid-point to head when standing */
#define EYE_HT_STAND  UNIT_HEIGHT * 0.25
/* distance from vertical center of grid-point to head when crouched */
#define EYE_HT_CROUCH UNIT_HEIGHT * 0.06

#define ACTOR_HAND_CHAR_RIGHT 'r'
#define ACTOR_HAND_CHAR_LEFT 'l'

/** @param[in] hand Hand index (ACTOR_HAND_RIGHT, ACTOR_HAND_LEFT) */
#define ACTOR_GET_HAND_CHAR(hand) ((hand) == ACTOR_HAND_LEFT ? ACTOR_HAND_CHAR_LEFT : ACTOR_HAND_CHAR_RIGHT)
/** @param[in] hand Hand index (ACTOR_HAND_CHAR_RIGHT, ACTOR_HAND_CHAR_LEFT) */
#define ACTOR_GET_HAND_INDEX(hand) ((hand) == ACTOR_HAND_CHAR_LEFT ? ACTOR_HAND_LEFT : ACTOR_HAND_RIGHT)
/** @param[in] hand Hand char (ACTOR_HAND_CHAR_RIGHT, ACTOR_HAND_CHAR_LEFT) */
#define ACTOR_SWAP_HAND(hand) ((hand) == ACTOR_HAND_CHAR_RIGHT ? ACTOR_HAND_CHAR_LEFT : ACTOR_HAND_CHAR_RIGHT)

/**
 * @brief Reaction fire toggle state, don't mess with the order!!!
 */
typedef enum {
	R_FIRE_OFF,
	R_FIRE_ONCE,
	R_FIRE_MANY
} reactionmode_t;


/** @sa moveModeDescriptions */
typedef enum {
	WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH,
	WALKTYPE_AUTOSTAND_BEING_USED,
	WALKTYPE_WALKING,
	WALKTYPE_CROUCH_WALKING,

	WALKTYPE_MAX
} walkType_t;

extern character_t *selChr;
extern const fireDef_t *selFD;
extern le_t *selActor;
extern pos3_t truePos;
extern pos3_t mousePos;
extern int mousePosTargettingAlign;

extern pos_t *fb_list[MAX_FORBIDDENLIST];
extern int fb_length;

void MSG_Write_PA(player_action_t player_action, int num, ...);

void ACTOR_InitStartup(void);

void CL_CharacterCvars(const character_t *chr);
const char *CL_GetSkillString(const int skill);

const fireDef_t *CL_GetWeaponAndAmmo(const le_t * actor, const char hand);
int CL_GetActorNumber(const le_t * le);
int CL_CheckAction(const le_t *le);
qboolean CL_WeaponWithReaction(const le_t * actor, const char hand);

int CL_UsableReactionTUs(const le_t * le);
void CL_SetReactionFiremode(le_t *actor, const int handidx, const objDef_t *od, const int fd_idx);
void CL_SetDefaultReactionFiremode(le_t *actor, const char hand);
void CL_UpdateReactionFiremodes(le_t * actor, const char hand, int firemodeActive);

character_t *CL_GetActorChr(const le_t *le);
qboolean CL_WorkingFiremode(const le_t *actor, qboolean reaction);
int CL_UsableTUs(const le_t *le);
int CL_ReservedTUs(const le_t *le, reservation_types_t type);
void CL_ReserveTUs(const le_t *le, reservation_types_t type, int tus);

int CL_MoveMode(const le_t *le, int length);

#ifdef DEBUG
void CL_ListReactionAndReservations_f (void);
void CL_DisplayBlockedPaths_f(void);
void LE_List_f(void);
void LM_List_f(void);
void CL_DumpTUs_f(void);
void CL_DumpMoveMark_f(void);
void CL_DebugPath_f(void);
#endif
void CL_ConditionalMoveCalcActor(le_t *le);
qboolean CL_ActorSelect(le_t *le);
qboolean CL_ActorSelectList(int num);
qboolean CL_ActorSelectNext(void);
void CL_AddActorToTeamList(le_t *le);
void CL_RemoveActorFromTeamList(le_t *le);
void CL_ActorCleanup(le_t *le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(le_t *le, int hand);
void CL_ActorTurnMouse(void);
void CL_ActorDoTurn(struct dbuffer *msg);
void CL_ActorStandCrouch_f(void);
void CL_ActorToggleCrouchReservation_f(void);
void CL_ActorUseHeadgear_f(void);
void CL_ActorStartMove(le_t *le, pos3_t to);
void CL_ActorShoot(const le_t *le, pos3_t at);
void CL_PlayActorSound(const le_t *le, actorSound_t soundType);

void CL_ActorActionMouse(void);
void CL_ActorUseDoor(const le_t *le);
void CL_ActorDoorAction_f(void);

void CL_NextRound_f(void);

void CL_ResetMouseLastPos(void);
void CL_ResetActorMoveLength(le_t *le);
void CL_ActorMouseTrace(void);

qboolean CL_AddActor(le_t *le, entity_t *ent);
qboolean CL_AddUGV(le_t *le, entity_t *ent);

void CL_AddTargeting(void);
void CL_AddPathing(void);
void CL_ActorTargetAlign_f(void);

void CL_CharacterSetShotSettings(character_t *chr, int hand, int fireModeIndex, const objDef_t *weapon);
void CL_CharacterSetRFMode(character_t *chr, int hand, int fireModeInde, const objDef_t *weapon);

void CL_DisplayFloorArrows(void);
void CL_DisplayObstructionArrows(void);
#endif
