/**
 * @file cl_actor.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#define ACTOR_HAND_CHAR_RIGHT 'r'
#define ACTOR_HAND_CHAR_LEFT 'l'
#define ACTOR_HAND_RIGHT 0
#define ACTOR_HAND_LEFT 1

/** @param[in] hand Hand index (ACTOR_HAND_RIGHT, ACTOR_HAND_LEFT) */
#define ACTOR_GET_HAND_INDEX(hand) ((hand) == ACTOR_HAND_LEFT ? ACTOR_HAND_CHAR_LEFT : ACTOR_HAND_CHAR_RIGHT)
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

typedef enum {
	BT_RIGHT_FIRE,
	BT_REACTION,
	BT_LEFT_FIRE,
	BT_RIGHT_RELOAD,
	BT_LEFT_RELOAD,
	BT_STAND,
	BT_CROUCH,
	BT_HEADGEAR,
	BT_NUM_TYPES
} button_types_t;

extern le_t *selActor;
extern int actorMoveLength;
extern invList_t invList[MAX_INVLIST];

extern pos_t *fb_list[MAX_FORBIDDENLIST];
extern int fb_length;

void MSG_Write_PA(player_action_t player_action, int num, ...);

void CL_CharacterCvars(const character_t *chr);
void HUD_ActorUpdateCvars(void);
const char *CL_GetSkillString(const int skill);
qboolean CL_CheckMenuAction(int time, invList_t *weapon, int mode);

void CL_SetReactionFiremode(le_t *actor, const int handidx, const int obj_idx, const int fd_idx);
void CL_SetDefaultReactionFiremode(le_t *actor, const char hand);
void HUD_ResetWeaponButtons(void);
void HUD_DisplayFiremodes_f(void);
void HUD_SwitchFiremodeList_f(void);
void HUD_FireWeapon_f(void);
void HUD_SelectReactionFiremode_f(void);
void HUD_PopupFiremodeReservation_f(void);
void HUD_ReserveShot_f(void);
void HUD_RemainingTus_f(void);

character_t *CL_GetActorChr(const le_t *le);
qboolean CL_WorkingFiremode(const le_t *actor, qboolean reaction);
int CL_UsableTUs(const le_t *le);
int CL_ReservedTUs(const le_t *le, reservation_types_t type);
void CL_ReserveTUs(const le_t *le, reservation_types_t type, int tus);

#ifdef DEBUG
void CL_ListReactionAndReservations_f (void);
void CL_DisplayBlockedPaths_f(void);
void LE_List_f(void);
void LM_List_f(void);
#endif
void CL_ConditionalMoveCalcForCurrentSelectedActor(void);
qboolean CL_ActorSelect(le_t *le);
qboolean CL_ActorSelectList(int num);
qboolean CL_ActorSelectNext(void);
void CL_AddActorToTeamList(le_t *le);
void CL_RemoveActorFromTeamList(const le_t *le);
void CL_ActorSelectMouse(void);
void CL_ActorReload(int hand);
void CL_ActorTurnMouse(void);
void CL_ActorDoTurn(struct dbuffer *msg);
void CL_ActorStandCrouch_f(void);
void CL_ActorToggleCrouchReservation_f(void);
void CL_ActorToggleReaction_f(void);
void CL_ActorUseHeadgear_f(void);
void CL_ActorStartMove(const le_t *le, pos3_t to);
void CL_ActorShoot(const le_t *le, pos3_t at);
void CL_InvCheckHands(struct dbuffer *msg);
void CL_ActorDoMove(struct dbuffer *msg);
void CL_ActorDoorAction(struct dbuffer *msg);
void CL_ActorResetClientAction(struct dbuffer *msg);
void CL_ActorDoShoot(struct dbuffer *msg);
void CL_ActorShootHidden(struct dbuffer *msg);
void CL_ActorDoThrow(struct dbuffer *msg);
void CL_ActorStartShoot(struct dbuffer *msg);
void CL_ActorDie(struct dbuffer *msg);
void CL_PlayActorSound(const le_t *le, actorSound_t soundType);

void CL_ActorActionMouse(void);
void CL_ActorUseDoor(void);
void CL_ActorDoorAction_f(void);

void CL_NextRound_f(void);
void CL_DoEndRound(struct dbuffer *msg);

void CL_ResetMouseLastPos(void);
void CL_ResetActorMoveLength(void);
void CL_ActorMouseTrace(void);

qboolean CL_AddActor(le_t *le, entity_t *ent);
qboolean CL_AddUGV(le_t *le, entity_t *ent);

void CL_AddTargeting(void);
void CL_AddPathing(void);
void CL_ActorTargetAlign_f(void);
void CL_ActorInventoryOpen_f(void);

void CL_CharacterSetShotSettings(character_t *chr, int hand, int fireModeIndex, int weaponIndex);
void CL_CharacterSetRFMode(character_t *chr, int hand, int fireModeIndex, int weaponIndex);

void CL_DumpTUs_f(void);
void CL_DumpMoveMark_f(void);

void CL_DisplayFloorArrows(void);
void CL_DisplayObstructionArrows(void);
#endif
