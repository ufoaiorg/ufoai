/**
 * @file cl_hud.c
 * @brief HUD related routines.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "menu/m_popup.h"
#include "menu/m_nodes.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "renderer/r_mesh_anim.h"

/** If this is set to qfalse HUD_DisplayFiremodes_f will not attempt to hide the list */
static qboolean firemodes_change_display = qtrue;
static qboolean visible_firemode_list_left = qfalse;
static qboolean visible_firemode_list_right = qfalse;

/** keep track of reaction toggle */
static reactionmode_t selActorReactionState;
/** and another to help set buttons! */
static reactionmode_t selActorOldReactionState = R_FIRE_OFF;

/** to optimise painting of HUD in move-mode, keep track of some globals: */
static le_t *lastHUDActor; /**< keeps track of selActor */
static int lastMoveLength; /**< keeps track of actorMoveLength */
static int lastTU; /**< keeps track of selActor->TU */
int selToHit;

enum {
	REMAINING_TU_RELOAD_RIGHT,
	REMAINING_TU_RELOAD_LEFT,
	REMAINING_TU_CROUCH,

	REMAINING_TU_MAX
};

static qboolean displayRemainingTus[REMAINING_TU_MAX];	/**< 0=reload_r, 1=reload_l, 2=crouch */

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

/** @brief a cbuf string for each button_types_t */
static const char *shoot_type_strings[BT_NUM_TYPES] = {
	"pr",
	"reaction",
	"pl",
	"rr",
	"rl",
	"stand",
	"crouch",
	"headgear"
};
CASSERT(lengthof(shoot_type_strings) == BT_NUM_TYPES);

/**
 * @brief Defines the various states of a button.
 * @note Not all buttons do have all of these states (e.g. "unusable" is not very common).
 */
typedef enum {
	BT_STATE_DISABLE,		/**< 'Disabled' display (grey) */
	BT_STATE_DESELECT,		/**< Normal display (blue) */
	BT_STATE_HIGHLIGHT,		/**< Normal + highlight (blue + glow)*/
	BT_STATE_UNUSABLE		/**< Normal + red (activated but unusable aka "impossible") */
} weaponButtonState_t;

static weaponButtonState_t weaponButtonState[BT_NUM_TYPES];

/** @note Order of elements here must correspond to order of elements in walkType_t. */
static const char *moveModeDescriptions[] = {
	N_("Crouch walk"),
	N_("Autostand"),
	N_("Walk"),
	N_("Crouch walk")
};
CASSERT(lengthof(moveModeDescriptions) == WALKTYPE_MAX);

/** Reservation-popup info */
static int popupNum;	/**< Number of entries in the popup list */
static qboolean popupReload = qfalse;

/**
 * @brief Displays a message on the hud.
 * @sa MN_DisplayNotice
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void HUD_DisplayMessage (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}
static void HUD_RefreshWeaponButtons(int time);
/**
 * @brief Updates the global character cvars for battlescape.
 * @note This is only called when we are in battlescape rendering mode
 * It's assumed that every living actor - @c le_t - has a character assigned, too
 */
static void HUD_ActorGlobalCvars (void)
{
	int i;

	Cvar_SetValue("mn_numaliensspotted", cl.numAliensSpotted);
	for (i = 0; i < MAX_TEAMLIST; i++) {
		const le_t *le = cl.teamList[i];
		if (le && !LE_IsDead(le)) {
			invList_t *invList;
			char tooltip[80] = "";
			const character_t *chr = CL_GetActorChr(le);
			assert(chr);

			/* the model name is the first entry in model_t */
			/** @todo fix this once model_t is known in the client */
			Cvar_Set(va("mn_head%i", i), (char *) le->model2);
			Cvar_SetValue(va("mn_hp%i", i), le->HP);
			Cvar_SetValue(va("mn_hpmax%i", i), le->maxHP);
			Cvar_SetValue(va("mn_tu%i", i), le->TU);
			Cvar_SetValue(va("mn_tumax%i", i), le->maxTU);
			Cvar_SetValue(va("mn_morale%i",i), le->morale);
			Cvar_SetValue(va("mn_moralemax%i",i), le->maxMorale);
			Cvar_SetValue(va("mn_stun%i", i), le->STUN);

			invList = RIGHT(le);
			if ((!invList || !invList->item.t->holdTwoHanded) && LEFT(le))
				invList = LEFT(le);

			Com_sprintf(tooltip, lengthof(tooltip), "%s\nHP: %i/%i TU: %i\n%s",
				chr->name, le->HP, le->maxHP, le->TU, invList ? _(invList->item.t->name) : "");
			Cvar_Set(va("mn_soldier%i_tt", i), tooltip);
		} else {
			Cvar_Set(va("mn_head%i", i), "");
			Cvar_Set(va("mn_hp%i", i), "0");
			Cvar_Set(va("mn_hpmax%i", i), "1");
			Cvar_Set(va("mn_tu%i", i), "0");
			Cvar_Set(va("mn_tumax%i", i), "1");
			Cvar_Set(va("mn_morale%i",i), "0");
			Cvar_Set(va("mn_moralemax%i",i), "1");
			Cvar_Set(va("mn_stun%i", i), "0");
			Cvar_Set(va("mn_soldier%i_tt", i), "");
		}
	}
}

/**
 * @brief Get state of the reaction-fire button.
 * @param[in] le Pointer to local entity structure, a soldier.
 * @return R_FIRE_MANY when STATE_REACTION_MANY.
 * @return R_FIRE_ONCE when STATE_REACTION_ONCE.
 * @return R_FIRE_OFF when no reaction fiR_
 * @sa HUD_RefreshWeaponButtons
 * @sa CL_ActorUpdateCVars
 * @sa CL_ActorSelect
 */
static int HUD_GetReactionState (const le_t * le)
{
	if (le->state & STATE_REACTION_MANY)
		return R_FIRE_MANY;
	else if (le->state & STATE_REACTION_ONCE)
		return R_FIRE_ONCE;
	else
		return R_FIRE_OFF;
}

void HUD_UpdateSelectedActorReactionState (void)
{
	selActorReactionState = HUD_GetReactionState(selActor);
}

void HUD_ResetWeaponButtons (void)
{
	memset(weaponButtonState, BT_STATE_DISABLE, sizeof(weaponButtonState));
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 */
static void HUD_SetWeaponButton (int button, weaponButtonState_t state)
{
	const weaponButtonState_t currentState = weaponButtonState[button];
	const char const* prefix;

	assert(button < BT_NUM_TYPES);

	if (state == currentState || state == BT_STATE_HIGHLIGHT) {
		/* Don't reset if it already is in current state or if highlighted,
		 * as HighlightWeaponButton deals with the highlighted state. */
		return;
	} else if (state == BT_STATE_DESELECT) {
		prefix = "desel";
	} else if (state == BT_STATE_DISABLE) {
		prefix = "dis";
	} else {
		prefix = "dis";
	}

	/* Connect confunc strings to the ones as defined in "menu nohud". */
	MN_ExecuteConfunc("%s%s", prefix, shoot_type_strings[button]);
	weaponButtonState[button] = state;
}

/**
 * @brief Makes all entries of the firemode lists invisible.
 */
void HUD_HideFiremodes (void)
{
	int i;

	visible_firemode_list_left = qfalse;
	visible_firemode_list_right = qfalse;
	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		MN_ExecuteConfunc("set_left_inv %i", i);
		MN_ExecuteConfunc("set_right_inv %i", i);
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param[in] fd Pointer to the firedefinition/firemode to be displayed.
 * @param[in] hand What list to display: 'l' for left hand list, 'r' for right hand list.
 * @param[in] status Display the firemode clickable/active (1) or inactive (0).
 */
static void HUD_DisplayFiremodeEntry (const fireDef_t * fd, const char hand, const qboolean status)
{
	int usableTusForRF;
	char cvarName[MAX_VAR];

	if (!fd || !selActor)
		return;

	assert(hand == ACTOR_HAND_CHAR_RIGHT || hand == ACTOR_HAND_CHAR_LEFT);

	usableTusForRF = CL_UsableReactionTUs(selActor);

	if (hand == ACTOR_HAND_CHAR_RIGHT) {
		MN_ExecuteConfunc("set_right_vis %i", fd->fdIdx); /* Make this entry visible (in case it wasn't). */

		if (status)
			MN_ExecuteConfunc("set_right_a %i", fd->fdIdx);
		else
			MN_ExecuteConfunc("set_right_ina %i", fd->fdIdx);
	} else if (hand == ACTOR_HAND_CHAR_LEFT) {
		MN_ExecuteConfunc("set_left_vis %i", fd->fdIdx); /* Make this entry visible (in case it wasn't). */

		if (status)
			MN_ExecuteConfunc("set_left_a %i", fd->fdIdx);
		else
			MN_ExecuteConfunc("set_left_ina %i", fd->fdIdx);
	}

	Com_sprintf(cvarName, lengthof(cvarName), "mn_%c_fm_tt_tu%i", hand, fd->fdIdx);
	if (usableTusForRF > fd->time)
		Cvar_Set(cvarName, va(_("Remaining TUs: %i"), usableTusForRF - fd->time));
	else
		Cvar_Set(cvarName, _("No remaining TUs left after shot."));

	Com_sprintf(cvarName, lengthof(cvarName), "mn_%c_fm_name%i", hand, fd->fdIdx);
	Cvar_Set(cvarName, _(fd->name));
	Com_sprintf(cvarName, lengthof(cvarName), "mn_%c_fm_tu%i", hand, fd->fdIdx);
	Cvar_Set(cvarName, va(_("TU: %i"), fd->time));
	Com_sprintf(cvarName, lengthof(cvarName), "mn_%c_fm_shot%i", hand, fd->fdIdx);
	Cvar_Set(cvarName, va(_("Shots: %i"), fd->ammo));
}

/**
 * @brief Check if at least one firemode is available for reservation.
 * @return qtrue if there is at least one firemode - qfalse otherwise.
 * @sa HUD_RefreshWeaponButtons
 * @sa HUD_PopupFiremodeReservation_f
 */
static qboolean CL_CheckFiremodeReservation (void)
{
	char hand = ACTOR_HAND_CHAR_RIGHT;

	if (!selActor)
		return qfalse;

	do {	/* Loop for the 2 hands (l/r) to avoid unneccesary code-duplication and abstraction. */
		objDef_t *ammo = NULL;
		objDef_t *weapon = NULL;
		int weapFdsIdx = -1;

		/* Get weapon (and its ammo) from the hand. */
		CL_GetWeaponAndAmmo(selActor, hand, &weapon, &ammo, &weapFdsIdx);

		if (weapon && ammo) {
			int i;
			for (i = 0; i < ammo->numFiredefs[weapFdsIdx]; i++) {
				/* Check if at least one firemode is available for reservation. */
				if ((CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT)) >= ammo->fd[weapFdsIdx][i].time)
					return qtrue;
			}
		}

		/* Prepare for next run or for end of loop. */
		if (hand == ACTOR_HAND_CHAR_RIGHT)
			hand = ACTOR_HAND_CHAR_LEFT;
		else
			break;
	} while (qtrue);

	/* No reservation possible */
	return qfalse;
}

static linkedList_t* popupListText = NULL;
static linkedList_t* popupListData = NULL;
static menuNode_t* popupListNode = NULL;

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @todo Fix the usage of LIST_Add with constant values like 0,1 and -1. See also the "tempxxx" variables.
 * @sa HUD_PopupFiremodeReservation_f
 * @sa CL_CheckFiremodeReservation
 */
static void HUD_PopupFiremodeReservation (qboolean reset)
{
	int tempInvalid = -1;
	int tempZero = 0;
	int tempOne = 1;
	char hand = ACTOR_HAND_CHAR_RIGHT;
	int i;
	static char text[MAX_VAR];
	int selectedEntry;

	if (!selActor)
		return;

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	if (reset) {
		CL_ReserveTUs(selActor, RES_SHOT, 0);
		CL_CharacterSetShotSettings(selChr, -1, -1, -1);
		MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, 0, selChr->reservedTus.shot); /* Update server-side settings */

		/* Refresh button and tooltip. */
		HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
		return;
	}

	/** @todo Why not using the MN_MenuTextReset function here? */
	LIST_Delete(&popupListText);
	/* also reset mn.menuTextLinkedList here - otherwise the
	 * pointer is no longer valid (because the list was freed) */
	MN_RegisterLinkedListText(TEXT_LIST, NULL);

	LIST_Delete(&popupListData);

	/* Reset the length of the TU-list. */
	popupNum = 0;

	/* Add list-entry for deactivation of the reservation. */
	LIST_AddPointer(&popupListText, _("[0 TU] No reservation"));
	LIST_Add(&popupListData, (byte *)&tempInvalid, sizeof(int));	/* hand */
	LIST_Add(&popupListData, (byte *)&tempInvalid, sizeof(int));	/* fmIdx */
	LIST_Add(&popupListData, (byte *)&tempInvalid, sizeof(int));	/* wpIdx */
	LIST_Add(&popupListData, (byte *)&tempZero, sizeof(int));	/* TUs */
	popupNum++;
	selectedEntry = 0;

	Com_DPrintf(DEBUG_CLIENT, "HUD_PopupFiremodeReservation\n");

	do {	/* Loop for the 2 hands (l/r) to avoid unneccesary code-duplication and abstraction. */
		objDef_t *ammo = NULL;
		objDef_t *weapon = NULL;
		int weapFdsIdx = -1;

		/* Get weapon (and its ammo) from the hand. */
		CL_GetWeaponAndAmmo(selActor, hand, &weapon, &ammo, &weapFdsIdx);

		if (weapon && ammo) {
			for (i = 0; i < ammo->numFiredefs[weapFdsIdx]; i++) {
				if ((CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT)) >= ammo->fd[weapFdsIdx][i].time) {
					/** Get weapon name, firemode name and TUs. */
					Com_sprintf(text, lengthof(text),
						_("[%i TU] %s - %s"),
						ammo->fd[weapFdsIdx][i].time,
						_(weapon->name),
						ammo->fd[weapFdsIdx][i].name);

					/* Store text for popup */
					LIST_AddString(&popupListText, text);
					/* Store Data for popup-callback. */
					if (hand == ACTOR_HAND_CHAR_RIGHT)
						LIST_Add(&popupListData, (byte *)&tempZero, sizeof(int));
					else
						LIST_Add(&popupListData, (byte *)&tempOne, sizeof(int));
					LIST_Add(&popupListData, (byte *)&i, sizeof(int));								/* fmIdx */
					LIST_Add(&popupListData, (byte *)&ammo->weapons[weapFdsIdx]->idx, sizeof(int));					/* wpIdx pointer*/
					LIST_Add(&popupListData, (byte *)&ammo->fd[weapFdsIdx][i].time, sizeof(int));	/* TUs */

					/* Remember the line that is currently selected (if any). */
					if (selChr->reservedTus.shotSettings.hand == ACTOR_GET_HAND_INDEX(hand)
					 && selChr->reservedTus.shotSettings.fmIdx == i
					 && selChr->reservedTus.shotSettings.wpIdx == ammo->weapons[weapFdsIdx]->idx)
						selectedEntry = popupNum;

					Com_DPrintf(DEBUG_CLIENT, "HUD_PopupFiremodeReservation: hand %i, fm %i, wp %i -- hand %i, fm %i, wp %i\n",
						selChr->reservedTus.shotSettings.hand, selChr->reservedTus.shotSettings.fmIdx, selChr->reservedTus.shotSettings.wpIdx,
						ACTOR_GET_HAND_INDEX(hand), i, ammo->weapons[weapFdsIdx]->idx);
					popupNum++;
				}
			}
		}

		/* Prepare for next run or for end of loop. */
		if (hand == ACTOR_HAND_CHAR_RIGHT)
			/* First run. Set hand for second run of the loop (other hand) */
			hand = ACTOR_HAND_CHAR_LEFT;
		else
			break;
	} while (qtrue);

	if (popupNum > 1 || popupReload) {
		/* We have more entries than the "0 TUs" one
		 * or we want ot simply refresh/display the popup content (no matter how many TUs are left). */
		popupListNode = MN_PopupList(_("Shot Reservation"), _("Reserve TUs for firing/using."), popupListText, "reserve_shot");
		VectorSet(popupListNode->selectedColor, 0.0, 0.78, 0.0);	/**< Set color for selected entry. */
		popupListNode->selectedColor[3] = 1.0;
		MN_TextNodeSelectLine(popupListNode, selectedEntry);
		popupReload = qfalse;
	}
}

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @todo Fix the usage of LIST_Add with constant values like 0,1 and -1. See also the "tempxxx" variables.
 * @sa HUD_PopupFiremodeReservation
 */
static void HUD_PopupFiremodeReservation_f (void)
{
	/* A second parameter (the value itself will be ignored) was given.
	 * This is used to reset the shot-reservation.*/
	if (Cmd_Argc() == 2)
		HUD_PopupFiremodeReservation(qtrue);
	else
		HUD_PopupFiremodeReservation(qfalse);
}

/**
 * @brief Get selected firemode in the list of the currently selected actor.
 * @sa HUD_PopupFiremodeReservation_f
 * @note console command: cl_input.c:reserve_shot
 */
static void HUD_ReserveShot_f (void)
{
	linkedList_t* popup = popupListData;	/**< Use this so we do not change the original popupListData pointer. */
	int selectedPopupIndex;
	int hand, fmIdx, wpIdx, TUs;
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <popupindex>\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	/* read and range check */
	selectedPopupIndex = atoi(Cmd_Argv(1));
	Com_DPrintf(DEBUG_CLIENT, "HUD_ReserveShot_f (popupNum %i, selectedPopupIndex %i)\n", popupNum, selectedPopupIndex);
	if (selectedPopupIndex < 0 || selectedPopupIndex >= popupNum)
		return;

	if (!popup || !popup->next)
		return;

	/* Get data from popup-list index */
	i = 0;
	while (popup) {
		assert(popup);
		hand = *(int*)popup->data;
		popup = popup->next;

		assert(popup);
		fmIdx = *(int*)popup->data;
		popup = popup->next;

		assert(popup);
		wpIdx = *(int*)popup->data;
		popup = popup->next;

		assert(popup);
		TUs	= *(int*)popup->data;
		popup = popup->next;

		if (i == selectedPopupIndex)
			break;

		i++;
	}

	/* Check if we have enough TUs (again) */
	Com_DPrintf(DEBUG_CLIENT, "HUD_ReserveShot_f: popup-index: %i TUs:%i (%i + %i)\n", selectedPopupIndex, TUs, CL_UsableTUs(selActor), CL_ReservedTUs(selActor, RES_SHOT));
	Com_DPrintf(DEBUG_CLIENT, "HUD_ReserveShot_f: hand %i, fmIdx %i, weapIdx %i\n", hand,fmIdx,wpIdx);
	if ((CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT)) >= TUs) {
		CL_ReserveTUs(selActor, RES_SHOT, TUs);
		CL_CharacterSetShotSettings(selChr, hand, fmIdx, wpIdx);
		MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, 0, selChr->reservedTus.shot); /* Update server-side settings */
		/** @todo
		MSG_Write_PA(PA_RESERVE_SHOT_FM, selActor->entnum, hand, fmIdx, weapIdx);
		*/
		if (popupListNode) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_ReserveShot_f: Setting currently selected line (from %i) to %i.\n", popupListNode->u.text.textLineSelected, selectedPopupIndex);
			MN_TextNodeSelectLine(popupListNode, selectedPopupIndex);
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "HUD_ReserveShot_f: can not select entry %i. It needs %i TUs, we have %i.\n", selectedPopupIndex, TUs, CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT));
	}

	/* Refresh button and tooltip. */
	HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
}


/**
 * @brief Display 'usable" (blue) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 */
void HUD_DisplayPossibleReaction (const le_t * actor)
{
	if (!actor)
		return;

	if (actor != selActor) {
		/* Given actor does not equal the currently selected actor. This normally only happens on game-start. */
		return;
	}

	/* Display 'usable" (blue) reaction buttons
	 * Code also used in CL_ActorToggleReaction_f */
	switch (HUD_GetReactionState(actor)) {
	case R_FIRE_ONCE:
		MN_ExecuteConfunc("startreactiononce");
		break;
	case R_FIRE_MANY:
		MN_ExecuteConfunc("startreactionmany");
		break;
	}
}

/**
 * @brief Display 'impossible" (red) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 * @return qtrue if nothing changed message was sent otherwise qfalse.
 */
qboolean CL_DisplayImpossibleReaction (const le_t * actor)
{
	if (!actor)
		return qfalse;

	if (actor != selActor) {
		/* Given actor does not equal the currently selectd actor. This normally only happens on game-start. */
		return qfalse;
	}

	/* Display 'impossible" (red) reaction buttons */
	switch (HUD_GetReactionState(actor)) {
	case R_FIRE_ONCE:
		weaponButtonState[BT_REACTION] = BT_STATE_UNUSABLE;	/* Set but not used anywhere (yet) */
		MN_ExecuteConfunc("startreactiononce_impos");
		break;
	case R_FIRE_MANY:
		weaponButtonState[BT_REACTION] = BT_STATE_UNUSABLE;	/* Set but not used anywhere (yet) */
		MN_ExecuteConfunc("startreactionmany_impos");
		break;
	default:
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Displays the firemodes for the given hand.
 * @note Console command: list_firemodes
 */
static void HUD_DisplayFiremodes_f (void)
{
	int actorIdx;
	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weapFdsIdx = -1;
	int i;
	char hand;

	if (!selActor)
		return;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		HUD_HideFiremodes();
		return;
	}

	if (Cmd_Argc() < 2) { /* no argument given */
		hand = ACTOR_HAND_CHAR_RIGHT;
	} else {
		hand = Cmd_Argv(1)[0];
	}

	if (hand != ACTOR_HAND_CHAR_RIGHT && hand != ACTOR_HAND_CHAR_LEFT) {
		Com_Printf("Usage: %s [l|r]\n", Cmd_Argv(0));
		return;
	}

	CL_GetWeaponAndAmmo(selActor, hand, &weapon, &ammo, &weapFdsIdx);
	if (weapFdsIdx == -1)
		return;

	Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: %s | %s | %i\n", weapon->name, ammo->name, weapFdsIdx);

	if (!weapon || !ammo) {
		Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: no weapon or ammo found.\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: displaying %c firemodes.\n", hand);

	if (firemodes_change_display) {
		/* Toggle firemode lists if needed. */
		HUD_HideFiremodes();
		if (hand == ACTOR_HAND_CHAR_RIGHT) {
			if (visible_firemode_list_right)
				return;
			visible_firemode_list_right = qtrue;
		} else { /* ACTOR_HAND_CHAR_LEFT */
			if (visible_firemode_list_left)
				return;
			visible_firemode_list_left = qtrue;
		}
	}
	firemodes_change_display = qtrue;

	actorIdx = CL_GetActorNumber(selActor);
	Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: actor index: %i\n", actorIdx);
	if (actorIdx == -1)
		Com_Error(ERR_DROP, "Could not get current selected actor's id");

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	/* Set default firemode if the currenttly seleced one is not sane or for another weapon. */
	if (!CL_WorkingFiremode(selActor, qtrue)) {	/* No usable firemode selected. */
		/* Set default firemode */
		CL_SetDefaultReactionFiremode(selActor, ACTOR_GET_HAND_CHAR(selChr->RFmode.hand));
	}

	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		if (i < ammo->numFiredefs[weapFdsIdx]) { /* We have a defined fd ... */
			/* Display the firemode information (image + text). */
			if (ammo->fd[weapFdsIdx][i].time <= CL_UsableTUs(selActor)) {	/* Enough timeunits for this firemode? */
				HUD_DisplayFiremodeEntry(&ammo->fd[weapFdsIdx][i], hand, qtrue);
			} else{
				HUD_DisplayFiremodeEntry(&ammo->fd[weapFdsIdx][i], hand, qfalse);
			}

			/* Display checkbox for reaction firemode (this needs a sane reactionFiremode array) */
			if (ammo->fd[weapFdsIdx][i].reaction) {
				Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: This is a reaction firemode: %i\n", i);
				if (hand == ACTOR_HAND_CHAR_RIGHT) {
					if (THIS_FIREMODE(&selChr->RFmode, ACTOR_HAND_RIGHT, i)) {
						/* Set this checkbox visible+active. */
						MN_ExecuteConfunc("set_right_cb_a %i", i);
					} else {
						/* Set this checkbox visible+inactive. */
						MN_ExecuteConfunc("set_right_cb_ina %i", i);
					}
				} else { /* hand[0] == ACTOR_HAND_CHAR_LEFT */
					if (THIS_FIREMODE(&selChr->RFmode, ACTOR_HAND_LEFT, i)) {
						/* Set this checkbox visible+active. */
						MN_ExecuteConfunc("set_left_cb_a %i", i);
					} else {
						/* Set this checkbox visible+active. */
						MN_ExecuteConfunc("set_left_cb_ina %i", i);
					}
				}
			}

		} else { /* No more fd left in the list or weapon not researched. */
			if (hand == ACTOR_HAND_CHAR_RIGHT)
				MN_ExecuteConfunc("set_right_inv %i", i); /* Hide this entry */
			else
				MN_ExecuteConfunc("set_left_inv %i", i); /* Hide this entry */
		}
	}
}

/**
 * @brief Changes the display of the firemode-list to a given hand, but only if the list is visible already.
 * @note Console command: switch_firemode_list
 */
static void HUD_SwitchFiremodeList_f (void)
{
	/* Cmd_Argv(1) ... = hand */
	if (Cmd_Argc() < 2 || (Cmd_Argv(1)[0] != ACTOR_HAND_CHAR_RIGHT && Cmd_Argv(1)[0] != ACTOR_HAND_CHAR_LEFT)) { /* no argument given */
		Com_Printf("Usage: %s [l|r]\n", Cmd_Argv(0));
		return;
	}

	if (visible_firemode_list_right || visible_firemode_list_left) {
		Cbuf_AddText(va("list_firemodes %s\n", Cmd_Argv(1)));
	}
}

/**
 * @brief Checks if the selected firemode checkbox is ok as a reaction firemode and updates data+display.
 */
static void HUD_SelectReactionFiremode_f (void)
{
	char hand;
	int firemode;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	hand = Cmd_Argv(1)[0];

	if (hand != ACTOR_HAND_CHAR_RIGHT && hand != ACTOR_HAND_CHAR_LEFT) {
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	firemode = atoi(Cmd_Argv(2));

	if (firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("HUD_SelectReactionFiremode_f: Firemode index to big (%i). Highest possible number is %i.\n",
			firemode, MAX_FIREDEFS_PER_WEAPON - 1);
		return;
	}

	CL_UpdateReactionFiremodes(selActor, hand, firemode);

	/* Update display of firemode checkbuttons. */
	firemodes_change_display = qfalse; /* So HUD_DisplayFiremodes_f doesn't hide the list. */
	HUD_DisplayFiremodes_f();
}

/**
 * @brief Checks whether an action on hud menu is valid and displays proper message.
 * @param[in] time The amount of TU (of an actor) left.
 * @param[in] weapon An item in hands.
 * @param[in] mode EV_INV_AMMO in case of fire button, EV_INV_RELOAD in case of reload button
 * @return qfalse when action is not possible, otherwise qtrue
 * @sa HUD_FireWeapon_f
 * @sa CL_ReloadLeft_f
 * @sa CL_ReloadRight_f
 * @todo Check for ammo in hand and give correct feedback in all cases.
 */
static qboolean CL_CheckMenuAction (int time, invList_t *weapon, int mode)
{
	/* No item in hand. */
	/** @todo Ignore this condition when ammo in hand. */
	if (!weapon || !weapon->item.t) {
		HUD_DisplayMessage(_("No item in hand.\n"), 2000);
		return qfalse;
	}

	switch (mode) {
	case EV_INV_AMMO:
		/* Check if shooting is possible.
		 * i.e. The weapon has ammo and can be fired with the 'available' hands.
		 * TUs (i.e. "time") are are _not_ checked here, this needs to be done
		 * elsewhere for the correct firemode. */

		/* Cannot shoot because of lack of ammo. */
		if (weapon->item.a <= 0 && weapon->item.t->reload) {
			HUD_DisplayMessage(_("Can't perform action:\nout of ammo.\n"), 2000);
			return qfalse;
		}
		/* Cannot shoot because weapon is fireTwoHanded, yet both hands handle items. */
		if (weapon->item.t->fireTwoHanded && LEFT(selActor)) {
			HUD_DisplayMessage(_("This weapon cannot be fired\none handed.\n"), 2000);
			return qfalse;
		}
		break;
	case EV_INV_RELOAD:
		/* Check if reload is possible. Also checks for the correct amount of TUs. */

		/* Cannot reload because this item is not reloadable. */
		if (!weapon->item.t->reload) {
			HUD_DisplayMessage(_("Can't perform action:\nthis item is not reloadable.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of no ammo in inventory. */
		if (CL_CalcReloadTime(weapon->item.t) >= 999) {
			HUD_DisplayMessage(_("Can't perform action:\nammo not available.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of not enough TUs. */
		if (time < CL_CalcReloadTime(weapon->item.t)) {
			HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
			return qfalse;
		}
		break;
	default:
		break;
	}

	return qtrue;
}

/**
 * @brief Starts aiming/target mode for selected left/right firemode.
 * @note Previously know as a combination of CL_FireRightPrimary, CL_FireRightSecondary,
 * @note CL_FireLeftPrimary and CL_FireLeftSecondary.
 */
static void HUD_FireWeapon_f (void)
{
	char hand;
	int firemode;
	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weapFdsIdx = -1;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	hand = Cmd_Argv(1)[0];

	if (hand != ACTOR_HAND_CHAR_RIGHT && hand != ACTOR_HAND_CHAR_LEFT) {
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	firemode = atoi(Cmd_Argv(2));

	if (firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("HUD_FireWeapon_f: Firemode index to big (%i). Highest possible number is %i.\n",
			firemode, MAX_FIREDEFS_PER_WEAPON - 1);
		return;
	}

	CL_GetWeaponAndAmmo(selActor, hand, &weapon, &ammo, &weapFdsIdx);
	if (weapFdsIdx == -1)
		return;

	/* Let's check if shooting is possible.
	 * Don't let the selActor->TU parameter irritate you, it is not checked/used here. */
	if (hand == ACTOR_HAND_CHAR_RIGHT) {
		if (!CL_CheckMenuAction(CL_UsableTUs(selActor), RIGHT(selActor), EV_INV_AMMO))
			return;
	} else {
		if (!CL_CheckMenuAction(CL_UsableTUs(selActor), LEFT(selActor), EV_INV_AMMO))
			return;
	}

	if (ammo->fd[weapFdsIdx][firemode].time <= CL_UsableTUs(selActor)) {
		/* Actually start aiming. This is done by changing the current mode of display. */
		if (hand == ACTOR_HAND_CHAR_RIGHT)
			cl.cmode = M_FIRE_R;
		else
			cl.cmode = M_FIRE_L;
		cl.cfiremode = firemode;	/* Store firemode. */
		HUD_HideFiremodes();
	} else {
		/* Cannot shoot because of not enough TUs - every other
		 * case should be checked previously in this function. */
		HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
		Com_DPrintf(DEBUG_CLIENT, "HUD_FireWeapon_f: Firemode not available (%c, %s).\n",
			hand, ammo->fd[weapFdsIdx][firemode].name);
	}
}

/**
 * @brief Remember if we hover over a button that would cost some TUs when pressed.
 * @note this is used in HUD_ActorUpdateCvars to update the "remaining TUs" bar correctly.
 */
static void HUD_RemainingTus_f (void)
{
	qboolean state;
	const char *type;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <type> <popupindex>\n", Cmd_Argv(0));
		return;
	}

	type = Cmd_Argv(1);
	state = atoi(Cmd_Argv(2));

	if (!Q_strcmp(type, "reload_r")) {
		displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] = state;
	} else if (!Q_strcmp(type, "reload_l")) {
		displayRemainingTus[REMAINING_TU_RELOAD_LEFT] = state;
	} else if (!Q_strcmp(type, "crouch")) {
		displayRemainingTus[REMAINING_TU_CROUCH] = state;
	}

	/* Update "remaining TUs" bar in HUD.*/
	HUD_ActorUpdateCvars();
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] time The amount of TU (of an actor) left in case of action.
 * @sa HUD_ActorUpdateCvars
 */
static void HUD_RefreshWeaponButtons (int time)
{
	invList_t *weaponr;
	invList_t *weaponl;
	invList_t *headgear;
	int minweaponrtime = 100, minweaponltime = 100;
	int minheadgeartime = 100;
	int weaponr_fds_idx = -1, weaponl_fds_idx = -1;
	int headgear_fds_idx = -1;
	qboolean isammo = qfalse;
	int i, reloadtime;

	if (!selActor)
		return;

	weaponr = RIGHT(selActor);
	headgear = HEADGEAR(selActor);

	/* check for two-handed weapon - if not, also define weaponl */
	if (!weaponr || !weaponr->item.t->holdTwoHanded)
		weaponl = LEFT(selActor);
	else
		weaponl = NULL;

	/* Crouch/stand button. */
	if (selActor->state & STATE_CROUCHED) {
		weaponButtonState[BT_STAND] = BT_STATE_DISABLE;
		if ((time + CL_ReservedTUs(selActor, RES_CROUCH)) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for standing up."));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Stand up (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DESELECT);
		}
	} else {
		weaponButtonState[BT_CROUCH] = BT_STATE_DISABLE;
		if ((time + CL_ReservedTUs(selActor, RES_CROUCH)) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for crouching."));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Crouch (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DESELECT);
		}
	}

	/* Crouch/stand reservation checkbox. */
	if (CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH) {
		MN_ExecuteConfunc("crouch_checkbox_check");
		Cvar_Set("mn_crouch_reservation_tt", va(_("%i TUs reserved for crouching/standing up.\nClick to clear."), CL_ReservedTUs(selActor, RES_CROUCH)));
	} else if (time >= TU_CROUCH) {
		MN_ExecuteConfunc("crouch_checkbox_clear");
		Cvar_Set("mn_crouch_reservation_tt", va(_("Reserve %i TUs for crouching/standing up."), TU_CROUCH));
	} else {
		MN_ExecuteConfunc("crouch_checkbox_disable");
		Cvar_Set("mn_crouch_reservation_tt", _("Not enough TUs left to reserve for crouching/standing up."));
	}

	/* Shot reservation button. mn_shot_reservation_tt is the tooltip text */
	if (CL_ReservedTUs(selActor, RES_SHOT)) {
		MN_ExecuteConfunc("reserve_shot_check");
		Cvar_Set("mn_shot_reservation_tt", va(_("%i TUs reserved for shooting.\nClick to change.\nRight-Click to clear."), CL_ReservedTUs(selActor, RES_SHOT)));
	} else if (CL_CheckFiremodeReservation()) {
		MN_ExecuteConfunc("reserve_shot_clear");
		Cvar_Set("mn_shot_reservation_tt", _("Reserve TUs for shooting."));
	} else {
		MN_ExecuteConfunc("reserve_shot_disable");
		Cvar_Set("mn_shot_reservation_tt", _("Reserving TUs for shooting not possible."));
	}

	/* Headgear button (nearly the same code as for weapon firing buttons below). */
	/** @todo Make a generic function out of this? */
	if (headgear) {
		assert(headgear->item.t);
		/* Check whether this item use ammo. */
		if (!headgear->item.m) {
			/* This item does not use ammo, check for existing firedefs in this item. */
			if (headgear->item.t->numWeapons > 0) {
				/* Get firedef from the weapon entry instead. */
				headgear_fds_idx = FIRESH_FiredefsIDXForWeapon(headgear->item.t, headgear->item.t);
			} else {
				headgear_fds_idx = -1;
			}
			isammo = qfalse;
		} else {
			/* This item uses ammo, get the firedefs from ammo. */
			headgear_fds_idx = FIRESH_FiredefsIDXForWeapon(headgear->item.m, headgear->item.t);
			isammo = qtrue;
		}
		if (isammo) {
			/* Search for the smallest TU needed to shoot. */
			if (headgear_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!headgear->item.m->fd[headgear_fds_idx][i].time)
						continue;
					if (headgear->item.m->fd[headgear_fds_idx][i].time < minheadgeartime)
						minheadgeartime = headgear->item.m->fd[headgear_fds_idx][i].time;
				}
		} else {
			if (headgear_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!headgear->item.t->fd[headgear_fds_idx][i].time)
						continue;
					if (headgear->item.t->fd[headgear_fds_idx][i].time < minheadgeartime)
						minheadgeartime = headgear->item.t->fd[headgear_fds_idx][i].time;
				}
		}
		if (time < minheadgeartime) {
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
		} else {
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DESELECT);
		}
	} else {
		HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
	}

	/* reaction-fire button */
	if (HUD_GetReactionState(selActor) == R_FIRE_OFF) {
		if ((time >= CL_ReservedTUs(selActor, RES_REACTION))
		 && (CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_LEFT)))
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DESELECT);
		else
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DISABLE);

	} else {
		if ((CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_LEFT))) {
			HUD_DisplayPossibleReaction(selActor);
		} else {
			CL_DisplayImpossibleReaction(selActor);
		}
	}

	/** Reload buttons @sa HUD_ActorUpdateCvars */
	{
		const qboolean fullyLoadedR = (weaponr && weaponr->item.t && (weaponr->item.t->ammo == weaponr->item.a));
		if (weaponr)
			reloadtime = CL_CalcReloadTime(weaponr->item.t);
		if (!weaponr || !weaponr->item.m || !weaponr->item.t->reload || time < reloadtime || fullyLoadedR) {
			HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DISABLE);
			if (fullyLoadedR)
				Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand, already fully loaded."));
			else
				Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand."));
		} else {
			HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DESELECT);
			Cvar_Set("mn_reloadright_tt", va(_("Reload weapon (%i TU)."), reloadtime));
		}
	}

	{
		const qboolean fullyLoadedL = (weaponl && weaponl->item.t && (weaponl->item.t->ammo == weaponl->item.a));
		if (weaponl)
			reloadtime = CL_CalcReloadTime(weaponl->item.t);
		if (!weaponl || !weaponl->item.m || !weaponl->item.t->reload || time < reloadtime || fullyLoadedL) {
			HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DISABLE);
			if (fullyLoadedL)
				Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand, already fully loaded."));
			else
				Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand."));
		} else {
			HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DESELECT);
			Cvar_Set("mn_reloadleft_tt", va(_("Reload weapon (%i TU)."), reloadtime));
		}
	}

	/* Weapon firing buttons. (nearly the same code as for headgear buttons above).*/
	/** @todo Make a generic function out of this? */
	if (weaponr) {
		assert(weaponr->item.t);
		/* Check whether this item use ammo. */
		if (!weaponr->item.m) {
			/* This item does not use ammo, check for existing firedefs in this item. */
			if (weaponr->item.t->numWeapons > 0) {
				/* Get firedef from the weapon entry instead. */
				weaponr_fds_idx = FIRESH_FiredefsIDXForWeapon(weaponr->item.t, weaponr->item.t);
			} else {
				weaponr_fds_idx = -1;
			}
			isammo = qfalse;
		} else {
			/* This item uses ammo, get the firedefs from ammo. */
			weaponr_fds_idx = FIRESH_FiredefsIDXForWeapon(weaponr->item.m, weaponr->item.t);
			isammo = qtrue;
		}
		if (isammo) {
			/* Search for the smallest TU needed to shoot. */
			if (weaponr_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!weaponr->item.m->fd[weaponr_fds_idx][i].time)
						continue;
					if (weaponr->item.m->fd[weaponr_fds_idx][i].time < minweaponrtime)
						minweaponrtime = weaponr->item.m->fd[weaponr_fds_idx][i].time;
				}
		} else {
			if (weaponr_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!weaponr->item.t->fd[weaponr_fds_idx][i].time)
						continue;
					if (weaponr->item.t->fd[weaponr_fds_idx][i].time < minweaponrtime)
						minweaponrtime = weaponr->item.t->fd[weaponr_fds_idx][i].time;
				}
		}
		if (time < minweaponrtime)
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
	}

	if (weaponl) {
		assert(weaponl->item.t);
		/* Check whether this item uses ammo. */
		if (!weaponl->item.m) {
			/* This item does not use ammo, check for existing firedefs in this item. */
			if (weaponl->item.t->numWeapons > 0) {
				/* Get firedef from the weapon entry instead. */
				weaponl_fds_idx = FIRESH_FiredefsIDXForWeapon(weaponl->item.t, weaponl->item.t);
			} else {
				weaponl_fds_idx = -1;
			}
			isammo = qfalse;
		} else {
			/* This item uses ammo, get the firedefs from ammo. */
			weaponl_fds_idx = FIRESH_FiredefsIDXForWeapon(weaponl->item.m, weaponl->item.t);
			isammo = qtrue;
		}
		if (isammo) {
			/* Search for the smallest TU needed to shoot. */
			if (weaponl_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!weaponl->item.m->fd[weaponl_fds_idx][i].time)
						continue;
					if (weaponl->item.m->fd[weaponl_fds_idx][i].time < minweaponltime)
						minweaponltime = weaponl->item.m->fd[weaponl_fds_idx][i].time;
				}
		} else {
			if (weaponl_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!weaponl->item.t->fd[weaponl_fds_idx][i].time)
						continue;
					if (weaponl->item.t->fd[weaponl_fds_idx][i].time < minweaponltime)
						minweaponltime = weaponl->item.t->fd[weaponl_fds_idx][i].time;
				}
		}
		if (time < minweaponltime)
			HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
	}

	/* Check if the firemode reservation popup is shown and refresh its content. (i.e. close&open it) */
	{
		const char* menuName = MN_GetActiveMenuName();
		if (menuName[0] != '\0')
			Com_DPrintf(DEBUG_CLIENT, "HUD_RefreshWeaponButtons: Active menu = %s\n", menuName);


		if (menuName[0] != '\0' && strstr(MN_GetActiveMenuName(), POPUPLIST_MENU_NAME)) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_RefreshWeaponButtons: reload popup\n");

			/* Prevent firemode reservation popup from being closed if
			 * no firemode is available because of insufficient TUs. */
			popupReload = qtrue;

			/* Close and reload firemode reservation popup. */
			MN_PopMenu(qfalse);
			HUD_PopupFiremodeReservation(qfalse);
		}
	}
}


/**
 * @brief Updates console vars for an actor.
 *
 * This function updates the cvars for the hud (battlefield)
 * unlike CL_CharacterCvars and CL_UGVCvars which updates them for
 * diplaying the data in the menu system
 *
 * @sa CL_CharacterCvars
 * @sa CL_UGVCvars
 */
void HUD_ActorUpdateCvars (void)
{
	static char infoText[MAX_SMALLMENUTEXTLEN];
	static char mouseText[MAX_SMALLMENUTEXTLEN];
	static char topText[MAX_SMALLMENUTEXTLEN];
	static char bottomText[MAX_SMALLMENUTEXTLEN];
	static char leftText[MAX_SMALLMENUTEXTLEN];
	static char tuTooltipText[MAX_SMALLMENUTEXTLEN];
	qboolean refresh;
	const char *animName;
	int time;
	pos3_t pos;
	int dv;

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	if (cls.state != ca_active)
		return;

	refresh = Cvar_VariableInteger("hud_refresh");
	if (refresh) {
		Cvar_Set("hud_refresh", "0");
		Cvar_Set("cl_worldlevel", cl_worldlevel->string);
		HUD_ResetWeaponButtons();
	}

	/* set Cvars for all actors */
	HUD_ActorGlobalCvars();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	if (selActor) {
		const invList_t *selWeapon;

		/* set generic cvars */
		Cvar_Set("mn_tu", va("%i", selActor->TU));
		Cvar_Set("mn_tumax", va("%i", selActor->maxTU));
		Cvar_Set("mn_tureserved", va("%i", CL_ReservedTUs(selActor, RES_ALL_ACTIVE)));

		Com_sprintf(tuTooltipText, lengthof(tuTooltipText),
			_("Time Units\n- Available: %i (of %i)\n- Reserved:  %i\n- Remaining: %i\n"),
			selActor->TU, selActor->maxTU,
			CL_ReservedTUs(selActor, RES_ALL_ACTIVE),
			CL_UsableTUs(selActor));
		Cvar_Set("mn_tu_tooltips", tuTooltipText);

		Cvar_Set("mn_morale", va("%i", selActor->morale));
		Cvar_Set("mn_moralemax", va("%i", selActor->maxMorale));
		Cvar_Set("mn_hp", va("%i", selActor->HP));
		Cvar_Set("mn_hpmax", va("%i", selActor->maxHP));
		Cvar_Set("mn_stun", va("%i", selActor->STUN));

		/* animation and weapons */
		animName = R_AnimGetName(&selActor->as, selActor->model1);
		if (animName)
			Cvar_Set("mn_anim", animName);
		if (RIGHT(selActor)) {
			const invList_t *i = RIGHT(selActor);
			assert(i->item.t >= &csi.ods[0] && i->item.t < &csi.ods[MAX_OBJDEFS]);
			Cvar_Set("mn_rweapon", i->item.t->model);
		}
		if (LEFT(selActor)) {
			const invList_t *i = LEFT(selActor);
			assert(i->item.t >= &csi.ods[0] && i->item.t < &csi.ods[MAX_OBJDEFS]);
			Cvar_Set("mn_lweapon", i->item.t->model);
		}

		/* get weapon */
		if (IS_MODE_FIRE_HEADGEAR(cl.cmode)) {
			selWeapon = HEADGEAR(selActor);
		} else if (IS_MODE_FIRE_LEFT(cl.cmode)) {
			selWeapon = LEFT(selActor);
		} else {
			selWeapon = RIGHT(selActor);
		}

		if (!selWeapon && RIGHT(selActor) && RIGHT(selActor)->item.t->holdTwoHanded)
			selWeapon = RIGHT(selActor);

		if (selWeapon) {
			if (!selWeapon->item.t) {
				/* No valid weapon in the hand. */
				selFD = NULL;
			} else {
				/* Check whether this item uses/has ammo. */
				if (!selWeapon->item.m) {
					/* This item does not use ammo, check for existing firedefs in this item. */
					/* This is supposed to be a weapon or other usable item. */
					if (selWeapon->item.t->numWeapons > 0) {
						if (selWeapon->item.t->weapon || selWeapon->item.t->weapons[0] == selWeapon->item.t) {
							/* Get firedef from the weapon (or other usable item) entry instead. */
							selFD = FIRESH_GetFiredef(
								selWeapon->item.t,
								FIRESH_FiredefsIDXForWeapon(selWeapon->item.t, selWeapon->item.t),
								cl.cfiremode);
						} else {
							/* This is ammo */
							selFD = NULL;
						}
					} else {
						/* No firedefinitions found in this presumed 'weapon with no ammo'. */
						selFD = NULL;
					}
				} else {
					/* This item uses ammo, get the firedefs from ammo. */
					const fireDef_t *old = FIRESH_GetFiredef(
						selWeapon->item.m,
						FIRESH_FiredefsIDXForWeapon(selWeapon->item.m, selWeapon->item.t),
						cl.cfiremode);
					/* reset the align if we switched the firemode */
					if (old != selFD)
						mousePosTargettingAlign = 0;
					selFD = old;
				}
			}
		}

		/* write info */
		time = 0;

		/* display special message */
		if (cl.time < cl.msgTime)
			Q_strncpyz(infoText, cl.msgText, sizeof(infoText));

		/* update HUD stats etc in more or shoot modes */
		if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT]
		 || displayRemainingTus[REMAINING_TU_RELOAD_LEFT]
		 || displayRemainingTus[REMAINING_TU_CROUCH]) {
			/* We are hovering over a "reload" button */
			/** @sa HUD_RefreshWeaponButtons */
			if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] && RIGHT(selActor)) {
				const invList_t *weapon = RIGHT(selActor);
				const int reloadtime = CL_CalcReloadTime(weapon->item.t);
				if (weapon->item.m
					 && weapon->item.t->reload
					 && CL_UsableTUs(selActor) >= reloadtime) {
					time = reloadtime;
				}
			} else if (displayRemainingTus[REMAINING_TU_RELOAD_LEFT] && LEFT(selActor)) {
				const invList_t *weapon = LEFT(selActor);
				const int reloadtime = CL_CalcReloadTime(weapon->item.t);
				if (weapon && weapon->item.m
					 && weapon->item.t->reload
					 && CL_UsableTUs(selActor) >= reloadtime) {
					time = reloadtime;
				}
			} else if (displayRemainingTus[REMAINING_TU_CROUCH]) {
				if (CL_UsableTUs(selActor) >= TU_CROUCH)
					time = TU_CROUCH;
			}
		} else if (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE) {
			const int reserved_tus = CL_ReservedTUs(selActor, RES_ALL_ACTIVE);
			/* If the mouse is outside the world, and we haven't placed the cursor in pend
			 * mode already or the selected grid field is not reachable (ROUTING_NOT_REACHABLE) */
			if ((mouseSpace != MS_WORLD && cl.cmode < M_PEND_MOVE) || actorMoveLength == ROUTING_NOT_REACHABLE) {
				actorMoveLength = ROUTING_NOT_REACHABLE;
				if (reserved_tus > 0)
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n"), selActor->morale, reserved_tus);
				else
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i"), selActor->morale);
				MN_MenuTextReset(TEXT_MOUSECURSOR_RIGHT);
			}
			if (cl.cmode != cl.oldcmode || refresh || lastHUDActor != selActor
						|| lastMoveLength != actorMoveLength || lastTU != selActor->TU) {
				if (actorMoveLength != ROUTING_NOT_REACHABLE) {
					HUD_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
					if (reserved_tus > 0)
						Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n%s %i (%i|%i TU left)\n"),
							selActor->morale, reserved_tus, moveModeDescriptions[CL_MoveMode(actorMoveLength)], actorMoveLength,
							selActor->TU - actorMoveLength, selActor->TU - reserved_tus - actorMoveLength);
					else
						Com_sprintf(infoText, lengthof(infoText), _("Morale  %i\n%s %i (%i TU left)\n"), selActor->morale,
							moveModeDescriptions[CL_MoveMode(actorMoveLength)] , actorMoveLength, selActor->TU - actorMoveLength);

					if (actorMoveLength <= CL_UsableTUs(selActor))
						Com_sprintf(mouseText, lengthof(mouseText), "%i (%i)\n", actorMoveLength, CL_UsableTUs(selActor));
					else
						Com_sprintf(mouseText, lengthof(mouseText), "- (-)\n");
				} else {
					HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
				}
				lastHUDActor = selActor;
				lastMoveLength = actorMoveLength;
				lastTU = selActor->TU;
				MN_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);
			}
			time = actorMoveLength;
		} else {
			MN_MenuTextReset(TEXT_MOUSECURSOR_RIGHT);
			/* in multiplayer RS_ItemIsResearched always returns true,
			 * so we are able to use the aliens weapons */
			if (selWeapon && !RS_IsResearched_ptr(selWeapon->item.t->tech)) {
				HUD_DisplayMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"), 2000);
				cl.cmode = M_MOVE;
			} else if (selWeapon && selFD) {
				Com_sprintf(infoText, lengthof(infoText),
							"%s\n%s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(selFD->name), selFD->ammo, selToHit, selFD->time);
				Com_sprintf(mouseText, lengthof(mouseText),
							"%s: %s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(selFD->name), selFD->ammo, selToHit, selFD->time);

				MN_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);	/* Save the text for later display next to the cursor. */

				time = selFD->time;
				/* if no TUs left for this firing action of if the weapon is reloadable and out of ammo, then change to move mode */
				if (CL_UsableTUs(selActor) < time || (selWeapon->item.t->reload && selWeapon->item.a <= 0)) {
					cl.cmode = M_MOVE;
					HUD_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
				}
			} else if (selWeapon) {
				Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(selWeapon->item.t->name));
			} else {
				cl.cmode = M_MOVE;
				HUD_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
			}
		}

		/* handle actor in a panic */
		if (selActor->state & STATE_PANIC) {
			Com_sprintf(infoText, lengthof(infoText), _("Currently panics!\n"));
			MN_MenuTextReset(TEXT_MOUSECURSOR_RIGHT);
		}

		/* Find the coordiantes of the ceiling and floor we want. */
		VectorCopy(mousePos, pos);
		pos[2] = cl_worldlevel->integer;

		/* Display the floor and ceiling values for the current cell. */
		Com_sprintf(topText, lengthof(topText), "%u-(%i,%i,%i)\n", Grid_Ceiling(clMap, fieldSize, truePos), truePos[0], truePos[1], truePos[2]);
		/* Save the text for later display next to the cursor. */
		MN_RegisterText(TEXT_MOUSECURSOR_TOP, topText);

		/* Display the floor and ceiling values for the current cell. */
		Com_sprintf(bottomText, lengthof(bottomText), "%i-(%i,%i,%i)\n", Grid_Floor(clMap, fieldSize, truePos), mousePos[0], mousePos[1], mousePos[2]);
		/* Save the text for later display next to the cursor. */
		MN_RegisterText(TEXT_MOUSECURSOR_BOTTOM, bottomText);

		/* Display the floor and ceiling values for the current cell. */
		dv = Grid_MoveNext(clMap, fieldSize, &clPathMap, mousePos, 0);
		Com_sprintf(leftText, lengthof(leftText), "%i-%i\n", getDVdir(dv), getDVz(dv));
		/* Save the text for later display next to the cursor. */
		MN_RegisterText(TEXT_MOUSECURSOR_LEFT, leftText);

		/* Calculate remaining TUs. */
		/* We use the full count of TUs since the "reserved" bar is overlaid over this one. */
		time = max(0, selActor->TU - time);

		Cvar_Set("mn_turemain", va("%i", time));

		/* print ammo */
		if (RIGHT(selActor)) {
			Cvar_SetValue("mn_ammoright", RIGHT(selActor)->item.a);
		} else {
			Cvar_Set("mn_ammoright", "");
		}
		if (LEFT(selActor)) {
			Cvar_SetValue("mn_ammoleft", LEFT(selActor)->item.a);
		} else {
			Cvar_Set("mn_ammoleft", "");
		}

		if (!LEFT(selActor) && RIGHT(selActor) && RIGHT(selActor)->item.t->holdTwoHanded)
			Cvar_Set("mn_ammoleft", Cvar_VariableString("mn_ammoright"));

		/* change stand-crouch & reaction button state */
		if (cl.oldstate != selActor->state || refresh) {
			selActorReactionState = HUD_GetReactionState(selActor);
			if (selActorOldReactionState != selActorReactionState) {
				selActorOldReactionState = selActorReactionState;
				switch (selActorReactionState) {
				case R_FIRE_MANY:
					MN_ExecuteConfunc("startreactionmany");
					break;
				case R_FIRE_ONCE:
					MN_ExecuteConfunc("startreactiononce");
					break;
				case R_FIRE_OFF: /* let RefreshWeaponButtons work it out */
					weaponButtonState[BT_REACTION] = BT_STATE_DISABLE;
					break;
				}
			}

			cl.oldstate = selActor->state;
			/** @todo Check if the use of "time" is correct here (e.g. are the reserved TUs ignored here etc...?) */
			if (actorMoveLength < ROUTING_NOT_REACHABLE && (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE))
				HUD_RefreshWeaponButtons(time);
			else
				HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
		} else {
			if (refresh)
				MN_ExecuteConfunc("deselstand");

			/* this allows us to display messages even with no actor selected */
			if (cl.time < cl.msgTime) {
				/* special message */
				Q_strncpyz(infoText, cl.msgText, sizeof(infoText));
			}
		}
		MN_RegisterText(TEXT_STANDARD, infoText);
	/* This will stop the drawing of the bars over the whole screen when we test maps. */
	} else if (!cl.numTeamList) {
		Cvar_SetValue("mn_tu", 0);
		Cvar_SetValue("mn_tumax", 100);
		Cvar_SetValue("mn_morale", 0);
		Cvar_SetValue("mn_moralemax", 100);
		Cvar_SetValue("mn_hp", 0);
		Cvar_SetValue("mn_hpmax", 100);
		Cvar_SetValue("mn_stun", 0);
	}

	/* mode */
	if (cl.oldcmode != cl.cmode || refresh) {
		cl.oldcmode = cl.cmode;
		if (selActor)
			HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
	}

	/* player bar */
	if (cl_selected->modified || refresh) {
		int i;

		for (i = 0; i < MAX_TEAMLIST; i++) {
			if (!cl.teamList[i] || LE_IsDead(cl.teamList[i])) {
				MN_ExecuteConfunc("huddisable %i", i);
			} else if (i == cl_selected->integer) {
				/* stored in menu_nohud.ufo - confunc that calls all the different
				 * hud select confuncs */
				MN_ExecuteConfunc("hudselect %i", i);
			} else {
				MN_ExecuteConfunc("huddeselect %i", i);
			}
		}
		cl_selected->modified = qfalse;
	}
}

/**
 * @brief Toggles if the current actor reserves Tus for crouching (e.g. at end of moving) or not.
 * @sa CL_StartingGameDone
 */
void CL_ActorToggleCrouchReservation_f (void)
{
	if (!CL_CheckAction())
		return;

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	if (CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH
	 || selChr->reservedTus.reserveCrouch) {
		/* Reset reserved TUs to 0 */
		CL_ReserveTUs(selActor, RES_CROUCH, 0);
		selChr->reservedTus.reserveCrouch = qfalse;
	} else {
		/* Reserve the exact amount for crouching/staning up (if we have enough to do so). */
		if (CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH)
			CL_ReserveTUs(selActor, RES_CROUCH, TU_CROUCH);

		/* Player wants to reserve Tus for crouching - remember this. */
		selChr->reservedTus.reserveCrouch = qtrue;
	}

	/* Refresh checkbox and tooltip. */
	HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));

	/** @todo Update shot-reservation popup if it is currently displayed.
	 * Alternatively just hide it. */
}

/**
 * @brief Toggles reaction fire between the states off/single/multi.
 * @note RF mode will change as follows (Current mode -> resulting mode after click)
 * off	-> single
 * single	-> multi
 * multi	-> off
 * single	-> off (if no TUs for multi available)
 */
static void CL_ActorToggleReaction_f (void)
{
	int state = 0;

	if (!CL_CheckAction())
		return;

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	selActorReactionState++;
	if (selActorReactionState > R_FIRE_MANY)
		selActorReactionState = R_FIRE_OFF;

	/* Check all hands for reaction-enabled ammo-firemodes. */
	if (CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_LEFT)) {
		/* At least one weapon is RF capable. */

		switch (selActorReactionState) {
		case R_FIRE_OFF:
			state = ~STATE_REACTION;
			break;
		case R_FIRE_ONCE:
			state = STATE_REACTION_ONCE;
			/* Check if stored info for RF is up-to-date and set it to default if not. */
			if (!CL_WorkingFiremode(selActor, qtrue)) {
				CL_SetDefaultReactionFiremode(selActor, ACTOR_HAND_CHAR_RIGHT);
			} else {
				/* We would reserve more TUs than are available - reserve nothing and abort. */
				if (CL_UsableReactionTUs(selActor) < CL_ReservedTUs(selActor, RES_REACTION))
					return;
			}
			break;
		case R_FIRE_MANY:
			state = STATE_REACTION_MANY;
			/* Check if stored info for RF is up-to-date and set it to default if not. */
			if (!CL_WorkingFiremode(selActor, qtrue)) {
				CL_SetDefaultReactionFiremode(selActor, ACTOR_HAND_CHAR_RIGHT);
			} else {
				/* Just in case. */
				if (CL_UsableReactionTUs(selActor) < CL_ReservedTUs(selActor, RES_REACTION))
					return;
			}
			break;
		default:
			return;
		}

		/* Update RF list if it is visible. */
		if (visible_firemode_list_left || visible_firemode_list_right)
			HUD_DisplayFiremodes_f();

		/* Send request to update actor's reaction state to the server. */
		MSG_Write_PA(PA_STATE, selActor->entnum, state);
		selChr->reservedTus.reserveReaction = state;
		/* Re-calc reserved values with already selected FM. Includes PA_RESERVE_STATE (Update server-side settings)*/
		CL_SetReactionFiremode(selActor, selChr->RFmode.hand, selChr->RFmode.wpIdx, selChr->RFmode.fmIdx);
	} else {
		/* No usable RF weapon. */
		switch (selActorReactionState) {
		case R_FIRE_OFF:
			state = ~STATE_REACTION;
			break;
		case R_FIRE_ONCE:
			state = STATE_REACTION_ONCE;
			break;
		case R_FIRE_MANY:
			state = STATE_REACTION_MANY;
			break;
		default:
			/* Display "impossible" reaction button or disable button. */
			CL_DisplayImpossibleReaction(selActor);
			break;
		}

		/* Send request to update actor's reaction state to the server. */
		MSG_Write_PA(PA_STATE, selActor->entnum, state);

		selChr->reservedTus.reserveReaction = state;

		/* Set RF-mode info to undef. */
		CL_SetReactionFiremode(selActor, -1, NONE, -1); /* Includes PA_RESERVE_STATE */
	}

	/* Refresh button and tooltip. */
	HUD_RefreshWeaponButtons(CL_UsableTUs(selActor));
}

/**
 * @brief returns the weapon the actor's left hand is touching
 */
static invList_t* CL_GetLeftHandWeapon (le_t *actor)
{
	invList_t *weapon = LEFT(actor);

	if (!weapon || !weapon->item.m) {
		weapon = RIGHT(actor);
		if (!weapon->item.t->holdTwoHanded)
			weapon = NULL;
	}

	return weapon;
}

/**
 * @brief Reload left weapon.
 */
static void CL_ReloadLeft_f (void)
{
	if (!selActor || !CL_CheckMenuAction(selActor->TU, CL_GetLeftHandWeapon(selActor), EV_INV_RELOAD))
		return;
	CL_ActorReload(csi.idLeft);
}

/**
 * @brief Reload right weapon.
 */
static void CL_ReloadRight_f (void)
{
	if (!selActor || !CL_CheckMenuAction(selActor->TU, RIGHT(selActor), EV_INV_RELOAD))
		 return;
	CL_ActorReload(csi.idRight);
}


void HUD_InitStartup (void)
{
	Cmd_AddCommand("reloadleft", CL_ReloadLeft_f, _("Reload the weapon in the soldiers left hand"));
	Cmd_AddCommand("reloadright", CL_ReloadRight_f, _("Reload the weapon in the soldiers right hand"));
	Cmd_AddCommand("remaining_tus", HUD_RemainingTus_f, "Define if remaining TUs should be displayed in the TU-bar for some hovered-over button.");
	Cmd_AddCommand("togglecrouchreserve", CL_ActorToggleCrouchReservation_f, _("Toggle reservation for crouching."));
	Cmd_AddCommand("reserve_shot", HUD_ReserveShot_f, "Reserve The TUs for the selected entry in the popup.");
	Cmd_AddCommand("sel_shotreservation", HUD_PopupFiremodeReservation_f, "Pop up a list of possible firemodes for reservation in the current turn.");
	Cmd_AddCommand("switch_firemode_list", HUD_SwitchFiremodeList_f, "Switch firemode-list to one for the given hand, but only if the list is visible already.");
	Cmd_AddCommand("sel_reactmode", HUD_SelectReactionFiremode_f, "Change/Select firemode used for reaction fire.");
	Cmd_AddCommand("list_firemodes", HUD_DisplayFiremodes_f, "Display a list of firemodes for a weapon+ammo.");
	Cmd_AddCommand("togglereaction", CL_ActorToggleReaction_f, _("Toggle reaction fire"));
	Cmd_AddCommand("fireweap", HUD_FireWeapon_f, "Start aiming the weapon.");
}
