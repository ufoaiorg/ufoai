/**
 * @file cl_hud.c
 * @brief HUD related routines.
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

#include "../client.h"
#include "cl_localentity.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_hud_callbacks.h"
#include "../cl_game.h"
#include "cl_view.h"
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"
#include "../menu/m_draw.h"
#include "../menu/m_render.h"
#include "../renderer/r_mesh_anim.h"
#include "../renderer/r_draw.h"

/** If this is set to qfalse HUD_DisplayFiremodes_f will not attempt to hide the list */
static qboolean visibleFiremodeListLeft = qfalse;
static qboolean visibleFiremodeListRight = qfalse;

static cvar_t *cl_hud_message_timeout;
static cvar_t *cl_show_cursor_tooltips;

enum {
	REMAINING_TU_RELOAD_RIGHT,
	REMAINING_TU_RELOAD_LEFT,
	REMAINING_TU_CROUCH,

	REMAINING_TU_MAX
};
static qboolean displayRemainingTus[REMAINING_TU_MAX];

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
} buttonTypes_t;

/** @brief a cbuf string for each button_types_t */
static const char *shootTypeStrings[] = {
	"primaryright",
	"reaction",
	"primaryleft",
	"reloadright",
	"reloadleft",
	"stand",
	"crouch",
	"headgear"
};
CASSERT(lengthof(shootTypeStrings) == BT_NUM_TYPES);

/**
 * @brief Defines the various states of a button.
 * @note Not all buttons do have all of these states (e.g. "unusable" is not very common).
 */
typedef enum {
	BT_STATE_DISABLE,		/**< 'Disabled' display (grey) */
	BT_STATE_DESELECT,		/**< Normal display (blue) */
	BT_STATE_UNUSABLE		/**< Normal + red (activated but unusable aka "impossible") */
} weaponButtonState_t;

/** @note Order of elements here must correspond to order of elements in walkType_t. */
static const char *moveModeDescriptions[] = {
	N_("Crouch walk"),
	N_("Autostand"),
	N_("Walk"),
	N_("Crouch walk")
};
CASSERT(lengthof(moveModeDescriptions) == WALKTYPE_MAX);

typedef struct reserveShot_s {
	actorHands_t hand;
	int fireModeIndex;
	int weaponIndex;
	int TUs;
} reserveShot_t;

static int hudTime;
static char hudText[256];

/**
 * @brief Displays a message on the hud.
 * @sa MN_DisplayNotice
 * @param[in] text text is already translated here
 */
void HUD_DisplayMessage (const char *text)
{
	assert(text);
	MN_DisplayNotice(text, cl_hud_message_timeout->integer, mn_hud->string);
}

/**
 * @brief Updates the global character cvars for battlescape.
 * @note This is only called when we are in battlescape rendering mode
 * It's assumed that every living actor - @c le_t - has a character assigned, too
 */
static void HUD_UpdateAllActors (void)
{
	int i;

	Cvar_SetValue("mn_numaliensspotted", cl.numAliensSpotted);
	for (i = 0; i < MAX_TEAMLIST; i++) {
		const le_t *le = cl.teamList[i];
		if (le && !LE_IsDead(le)) {
			invList_t *invList;
			const char* tooltip;
			const character_t *chr = CL_ActorGetChr(le);
			assert(chr);

			invList = RIGHT(le);
			if ((!invList || !invList->item.t || !invList->item.t->holdTwoHanded) && LEFT(le))
				invList = LEFT(le);

			tooltip = va(_("%s\nHP: %i/%i TU: %i\n%s"),
				chr->name, le->HP, le->maxHP, le->TU, (invList && invList->item.t) ? _(invList->item.t->name) : "");

			MN_ExecuteConfunc("updateactorvalues %i \"%s\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%s\"",
					i, le->model2->name, le->HP, le->maxHP, le->TU, le->maxTU, le->morale, le->maxMorale, le->STUN, tooltip);
		} else {
			MN_ExecuteConfunc("updateactorvalues %i \"\" \"0\" \"1\" \"0\" \"1\" \"0\" \"1\" \"0\" \"\"", i);
		}
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @todo This should be a confunc which also sets the tooltips
 */
static void HUD_SetWeaponButton (int button, weaponButtonState_t state)
{
	const char const* prefix;

	assert(button < BT_NUM_TYPES);

	if (state == BT_STATE_DESELECT)
		prefix = "deselect_";
	else if (state == BT_STATE_DISABLE)
		prefix = "disable_";
	else
		prefix = "disable_";

	/* Connect confunc strings to the ones as defined in "menu nohud". */
	MN_ExecuteConfunc("%s%s", prefix, shootTypeStrings[button]);
}

/**
 * @brief Makes all entries of the firemode lists invisible.
 */
void HUD_HideFiremodes (void)
{
	visibleFiremodeListLeft = qfalse;
	visibleFiremodeListRight = qfalse;
	MN_ExecuteConfunc("hide_firemodes");
}

/**
 * @brief Returns the amount of usable "reaction fire" TUs for this actor (depends on active/inactive RF)
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 * @todo Maybe only return "reaction" value if reaction-state is active? The value _should_ be 0, but one never knows :)
 */
static int HUD_UsableReactionTUs (const le_t * le)
{
	/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) */
	if (le->state & STATE_REACTION)
		/* CL_ActorUsableTUs DOES NOT return the stored value for "reaction" here. */
		return CL_ActorUsableTUs(le) + CL_ActorReservedTUs(le, RES_REACTION);
	else
		/* CL_ActorUsableTUs DOES return the stored value for "reaction" here. */
		return CL_ActorUsableTUs(le);
}

/**
 * @brief Check if at least one firemode is available for reservation.
 * @return qtrue if there is at least one firemode - qfalse otherwise.
 * @sa HUD_RefreshWeaponButtons
 * @sa HUD_PopupFiremodeReservation_f
 */
static qboolean HUD_CheckFiremodeReservation (void)
{
	actorHands_t hand = ACTOR_HAND_RIGHT;

	if (!selActor)
		return qfalse;

	do {	/* Loop for the 2 hands (l/r) to avoid unnecessary code-duplication and abstraction. */
		const fireDef_t *fireDef;

		/* Get weapon (and its ammo) from the hand. */
		fireDef = HUD_GetFireDefinitionForHand(selActor, hand);
		if (fireDef) {
			int i;
			const objDef_t *ammo = fireDef->obj;
			for (i = 0; i < ammo->numFiredefs[fireDef->weapFdsIdx]; i++) {
				/* Check if at least one firemode is available for reservation. */
				if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= ammo->fd[fireDef->weapFdsIdx][i].time)
					return qtrue;
			}
		}

		/* Prepare for next run or for end of loop. */
		if (hand == ACTOR_HAND_RIGHT)
			hand = ACTOR_HAND_LEFT;
		else
			break;
	} while (qtrue);

	/* No reservation possible */
	return qfalse;
}

static linkedList_t* popupListData;
static menuNode_t* popupListNode;

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @param[in] popupReload Prevent firemode reservation popup from being closed if
 * no firemode is available because of insufficient TUs.
 * @sa HUD_PopupFiremodeReservation_f
 * @sa HUD_CheckFiremodeReservation
 * @todo use components and confuncs here
 */
static void HUD_PopupFiremodeReservation (qboolean reset, qboolean popupReload)
{
	actorHands_t hand = ACTOR_HAND_RIGHT;
	int i;
	static char text[MAX_VAR];
	int selectedEntry;
	linkedList_t* popupListText = NULL;
	reserveShot_t reserveShotData;
	character_t* chr;

	if (!selActor)
		return;

	chr = CL_ActorGetChr(selActor);
	assert(chr);

	if (reset) {
		CL_ActorReserveTUs(selActor, RES_SHOT, 0);
		CL_ActorSetShotSettings(chr, ACTOR_HAND_NOT_SET, -1, NULL);
		return;
	}

	/* reset the list */
	MN_ResetData(TEXT_LIST);

	LIST_Delete(&popupListData);

	/* Add list-entry for deactivation of the reservation. */
	LIST_AddPointer(&popupListText, _("[0 TU] No reservation"));
	reserveShotData.hand = ACTOR_HAND_NOT_SET;
	reserveShotData.fireModeIndex = -1;
	reserveShotData.weaponIndex = NONE;
	reserveShotData.TUs = -1;
	LIST_Add(&popupListData, (byte *)&reserveShotData, sizeof(reserveShotData));
	selectedEntry = 0;

	do {	/* Loop for the 2 hands (l/r) to avoid unnecessary code-duplication and abstraction. */
		const fireDef_t *fd = HUD_GetFireDefinitionForHand(selActor, hand);

		if (fd) {
			const objDef_t *ammo = fd->obj;

			for (i = 0; i < ammo->numFiredefs[fd->weapFdsIdx]; i++) {
				const fireDef_t* ammoFD = &ammo->fd[fd->weapFdsIdx][i];
				if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= ammoFD->time) {
					/* Get firemode name and TUs. */
					Com_sprintf(text, lengthof(text), _("[%i TU] %s"), ammoFD->time, ammoFD->name);

					/* Store text for popup */
					LIST_AddString(&popupListText, text);

					/* Store Data for popup-callback. */
					reserveShotData.hand = hand;
					reserveShotData.fireModeIndex = i;
					reserveShotData.weaponIndex = ammo->weapons[fd->weapFdsIdx]->idx;
					reserveShotData.TUs = ammoFD->time;
					LIST_Add(&popupListData, (byte *)&reserveShotData, sizeof(reserveShotData));

					/* Remember the line that is currently selected (if any). */
					if (chr->reservedTus.shotSettings.hand == hand
					 && chr->reservedTus.shotSettings.fmIdx == i
					 && chr->reservedTus.shotSettings.weapon == ammo->weapons[fd->weapFdsIdx])
						selectedEntry = LIST_Count(popupListData) - 1;
				}
			}
		}

		/* Prepare for next run or for end of loop. */
		if (hand == ACTOR_HAND_RIGHT)
			/* First run. Set hand for second run of the loop (other hand) */
			hand = ACTOR_HAND_LEFT;
		else
			break;
	} while (qtrue);

	if (LIST_Count(popupListData) > 1 || popupReload) {
		/* We have more entries than the "0 TUs" one
		 * or we want to simply refresh/display the popup content (no matter how many TUs are left). */
		popupListNode = MN_PopupList(_("Shot Reservation"), _("Reserve TUs for firing/using."), popupListText, "hud_shotreserve <lineselected>");
		/* Set color for selected entry. */
		VectorSet(popupListNode->selectedColor, 0.0, 0.78, 0.0);
		popupListNode->selectedColor[3] = 1.0;
		MN_TextNodeSelectLine(popupListNode, selectedEntry);
	}
}

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @sa HUD_PopupFiremodeReservation
 */
static void HUD_PopupFiremodeReservation_f (void)
{
	/* A second parameter (the value itself will be ignored) was given.
	 * This is used to reset the shot-reservation.*/
	HUD_PopupFiremodeReservation(Cmd_Argc() == 2, qfalse);
}

/**
 * @brief Get selected firemode in the list of the currently selected actor.
 * @sa HUD_PopupFiremodeReservation_f
 */
static void HUD_ShotReserve_f (void)
{
	int selectedPopupIndex;
	const reserveShot_t* reserveShotData;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <popupindex>\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	/* read and range check */
	selectedPopupIndex = atoi(Cmd_Argv(1));
	if (selectedPopupIndex < 0 || selectedPopupIndex >= LIST_Count(popupListData))
		return;

	reserveShotData = LIST_GetByIdx(popupListData, selectedPopupIndex);
	if (!reserveShotData)
		return;

	/** @todo do this on the server */
	/* Check if we have enough TUs (again) */
	if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= reserveShotData->TUs) {
		const objDef_t *od = INVSH_GetItemByIDX(reserveShotData->weaponIndex);
		character_t* chr = CL_ActorGetChr(selActor);
		assert(chr);
		CL_ActorReserveTUs(selActor, RES_SHOT, max(0, reserveShotData->TUs));
		CL_ActorSetShotSettings(chr, reserveShotData->hand, reserveShotData->fireModeIndex, od);
		if (popupListNode)
			MN_TextNodeSelectLine(popupListNode, selectedPopupIndex);
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param[in] hand What list to display
 */
static void HUD_DisplayFiremodeEntry (const le_t* actor, const objDef_t* ammo, const int weapFdsIdx, const actorHands_t hand, int index)
{
	int usableTusForRF;
	char tuString[MAX_VAR];
	qboolean status;
	const fireDef_t *fd;
	const char *tooltip;

	if (index < ammo->numFiredefs[weapFdsIdx]) {
		/* We have a defined fd ... */
		fd = &ammo->fd[weapFdsIdx][index];
	} else {
		/* Hide this entry */
		if (hand == ACTOR_HAND_RIGHT)
			MN_ExecuteConfunc("set_right_inv %i", index);
		else
			MN_ExecuteConfunc("set_left_inv %i", index);
		return;
	}

	assert(actor);
	assert(hand == ACTOR_HAND_RIGHT || hand == ACTOR_HAND_LEFT);

	status = fd->time <= CL_ActorUsableTUs(actor);
	usableTusForRF = HUD_UsableReactionTUs(actor);

	if (usableTusForRF > fd->time) {
		Com_sprintf(tuString, sizeof(tuString), _("Remaining TUs: %i"), usableTusForRF - fd->time);
		tooltip = tuString;
	} else
		tooltip = _("No remaining TUs left after shot.");

	MN_ExecuteConfunc("set_firemode %c %i %i %i \"%s\" \"%s\" \"%s\" \"%s\"", ACTOR_GET_HAND_CHAR(hand),
			fd->fdIdx, fd->reaction, status, _(fd->name), va(_("TU: %i"), fd->time),
			va(_("Shots: %i"), fd->ammo), tooltip);

	/* Display checkbox for reaction firemode */
	if (fd->reaction) {
		character_t* chr = CL_ActorGetChr(actor);
		const qboolean active = THIS_FIREMODE(&chr->RFmode, hand, fd->fdIdx);
		/* Change the state of the checkbox. */
		MN_ExecuteConfunc("set_firemode_checkbox %c %i %i", ACTOR_GET_HAND_CHAR(hand), fd->fdIdx, active);
	}
}

void HUD_DisplayFiremodes (const le_t* actor, actorHands_t hand, qboolean firemodesChangeDisplay)
{
	const objDef_t *ammo;
	const fireDef_t *fd;
	int i;
	character_t* chr;

	if (!actor)
		return;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		HUD_HideFiremodes();
		return;
	}

	fd = HUD_GetFireDefinitionForHand(actor, hand);
	if (fd == NULL)
		return;

	ammo = fd->obj;
	if (!ammo) {
		Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes: no weapon or ammo found.\n");
		return;
	}

	if (firemodesChangeDisplay) {
		/* Toggle firemode lists if needed. */
		HUD_HideFiremodes();
		if (hand == ACTOR_HAND_RIGHT) {
			visibleFiremodeListRight = qtrue;
		} else {
			visibleFiremodeListLeft = qtrue;
		}
	}

	chr = CL_ActorGetChr(actor);
	assert(chr);

	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		/* Display the firemode information (image + text). */
		HUD_DisplayFiremodeEntry(actor, ammo, fd->weapFdsIdx, hand, i);
	}
}

/**
 * @brief Displays the firemodes for the given hand.
 */
static void HUD_DisplayFiremodes_f (void)
{
	actorHands_t hand;

	if (!selActor)
		return;

	if (Cmd_Argc() < 2)
		/* no argument given */
		hand = ACTOR_HAND_RIGHT;
	else
		hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]);

	HUD_DisplayFiremodes(selActor, hand, qtrue);
}

/**
 * @brief Changes the display of the firemode-list to a given hand, but only if the list is visible already.
 */
static void HUD_SwitchFiremodeList_f (void)
{
	/* no argument given */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s [l|r]\n", Cmd_Argv(0));
		return;
	}

	if (visibleFiremodeListRight || visibleFiremodeListLeft)
		HUD_DisplayFiremodes(selActor, ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]), qfalse);
}

/**
 * @brief Requests firemode settings from the server
 * @param[in] actor The actor to update the firemode for.
 * @param[in] handidx Index of hand with item, which will be used for reactionfiR_ Possible hand indices: 0=right, 1=right, -1=undef
 * @param[in] od Pointer to objDef_t for which we set up firemode.
 * @param[in] fdIdx Index of firedefinition for an item in given hand.
 */
static void HUD_RequestReactionFiremode (const le_t * actor, const actorHands_t hand, const objDef_t *od, const int fdIdx)
{
	character_t *chr;
	int usableTusForRF = 0;

	if (!actor)
		return;

	usableTusForRF = HUD_UsableReactionTUs(actor);

	chr = CL_ActorGetChr(actor);

	/* Store TUs needed by the selected firemode (if reaction-fire is enabled). Otherwise set it to 0. */
	if (od != NULL && fdIdx >= 0) {
		/* Get 'ammo' (of weapon in defined hand) and index of firedefinitions in 'ammo'. */
		const fireDef_t *fd = HUD_GetFireDefinitionForHand(actor, hand);
		if (fd) {
			int time = fd[fdIdx].time;
			/* Reserve the TUs needed by the selected firemode (defined in the ammo). */
			if (actor->state & STATE_REACTION_MANY)
				time *= (usableTusForRF / fd[fdIdx].time);

			CL_ActorReserveTUs(actor, RES_REACTION, time);
		}
	}

	/* Send RFmode[] to server-side storage as well. See g_local.h for more. */
	MSG_Write_PA(PA_REACT_SELECT, actor->entnum, hand, fdIdx, od ? od->idx : NONE);
}

/**
 * @brief Updates the information in RFmode for the selected actor with the given data from the parameters.
 * @param[in] actor The actor we want to update the reaction-fire firemode for.
 * @param[in] hand Which weapon(-hand) to use.
 * @param[in] firemodeActive Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
static void HUD_UpdateReactionFiremodes (le_t * actor, const actorHands_t hand, int firemodeActive)
{
	const fireDef_t *fd;
	const objDef_t *ammo, *od;

	assert(actor);

	fd = HUD_GetFireDefinitionForHand(actor, hand);
	if (fd == NULL)
		return;

	ammo = fd->obj;
	od = ammo->weapons[fd->weapFdsIdx];

	if (!GAME_ItemIsUseable(od))
		return;

	HUD_RequestReactionFiremode(actor, hand, od, firemodeActive);
}

/**
 * @brief Checks if the selected firemode checkbox is ok as a reaction firemode and updates data+display.
 */
static void HUD_SelectReactionFiremode_f (void)
{
	actorHands_t hand;
	int firemode;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]);
	firemode = atoi(Cmd_Argv(2));

	if (firemode >= MAX_FIREDEFS_PER_WEAPON || firemode < 0) {
		Com_Printf("HUD_SelectReactionFiremode_f: Firemode out of bounds (%i).\n", firemode);
		return;
	}

	HUD_UpdateReactionFiremodes(selActor, hand, firemode);
}

/**
 * @brief Remember if we hover over a button that would cost some TUs when pressed.
 * @note this is used in HUD_Update to update the "remaining TUs" bar correctly.
 */
static void HUD_RemainingTUs_f (void)
{
	qboolean state;
	const char *type;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <type> <popupindex>\n", Cmd_Argv(0));
		return;
	}

	type = Cmd_Argv(1);
	state = Com_ParseBoolean(Cmd_Argv(2));

	memset(displayRemainingTus, 0, sizeof(displayRemainingTus));

	if (!strcmp(type, "reload_r")) {
		displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] = state;
	} else if (!strcmp(type, "reload_l")) {
		displayRemainingTus[REMAINING_TU_RELOAD_LEFT] = state;
	} else if (!strcmp(type, "crouch")) {
		displayRemainingTus[REMAINING_TU_CROUCH] = state;
	}
}

/**
 * @return The minimum time needed to fire the weapons in the given @c invList
 */
static int HUD_GetMinimumTUsForUsage (const invList_t *invList)
{
	const fireDef_t *fdArray;
	int time = 100;
	const objDef_t *od;
	int i;

	assert(invList->item.t);

	fdArray = FIRESH_FiredefForWeapon(&invList->item);
	if (fdArray == NULL)
		return time;

	if (invList->item.m)
		od = invList->item.m;
	else
		od = invList->item.t;

	/* Search for the smallest TU needed to shoot. */
	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		if (!fdArray[i].time)
			continue;
		if (fdArray[i].time < time)
			time = fdArray[i].time;
	}

	return time;
}

/**
 * @brief Checks every case for reload buttons on the HUD.
 * @param[in] le Pointer of local entity being an actor.
 * @param[in] tu Remaining TU units of selected actor.
 * @param[in] containerID of the container to reload the weapon in. Used to get the movement TUs for moving something into the container.
 * @param[out] reason The reason why the reload didn't work - only set if @c -1 is the return value
 * @return TU units needed for reloading or -1 if weapon cannot be reloaded.
 */
static int HUD_WeaponCanBeReloaded (const le_t *le, int containerID, const char **reason)
{
	const int tu = CL_ActorUsableTUs(le);
	const invList_t *invList = CONTAINER(le, containerID);
	const objDef_t *weapon;

	assert(le);

	/* No weapon in hand. */
	if (!invList) {
		*reason = _("No weapon.");
		return -1;
	}

	weapon = invList->item.t;
	assert(weapon);

	/* This weapon cannot be reloaded. */
	if (!weapon->reload) {
		*reason = _("Weapon cannot be reloaded.");
		return -1;
	}

	/* Weapon is fully loaded. */
	if (invList->item.m && weapon->ammo == invList->item.a) {
		*reason = _("No reload possible, already fully loaded.");
		return -1;
	}

	/* Weapon is empty or not fully loaded, find ammo of any type loadable to this weapon. */
	if (!invList->item.m || weapon->ammo > invList->item.a) {
		const int tuCosts = HUD_CalcReloadTime(le, weapon, containerID);
		if (tuCosts >= 0) {
			if (tu >= tuCosts)
				return tuCosts;
			*reason = _("Not enough TUs for reloading weapon.");
		} else {
			/* Found no ammo which could be used for this weapon. */
			*reason = _("No reload possible, you don't have backup ammo.");
		}
	}

	return -1;
}

/**
 * @brief Checks if there is a weapon in the hand that can be used for reaction fire.
 * @param[in] actor What actor to check.
 */
static qboolean HUD_WeaponWithReaction (const le_t * actor)
{
	const fireDef_t *fd = INVSH_HasReactionFireEnabledWeapon(RIGHT(actor));
	if (fd)
		return qtrue;
	return INVSH_HasReactionFireEnabledWeapon(LEFT(actor)) != NULL;
}

/**
 * @brief Display 'impossible" (red) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 * @return qtrue if nothing changed message was sent otherwise qfalse.
 */
static qboolean HUD_DisplayImpossibleReaction (const le_t * actor)
{
	if (!actor)
		return qfalse;

	/* Given actor does not equal the currently selected actor. */
	if (!actor->selected)
		return qfalse;

	/* Display 'impossible" (red) reaction buttons */
	if (actor->state & STATE_REACTION_ONCE)
		MN_ExecuteConfunc("startreactiononce_impos");
	else if (actor->state & STATE_REACTION_MANY)
		MN_ExecuteConfunc("startreactionmany_impos");
	else
		return qtrue;

	return qfalse;
}

/**
 * @brief Display 'usable" (blue) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 */
static void HUD_DisplayPossibleReaction (const le_t * actor)
{
	if (!actor)
		return;

	/* Given actor does not equal the currently selected actor. This normally only happens on game-start. */
	if (!actor->selected)
		return;

	/* Display 'usable" (blue) reaction buttons */
	if (actor->state & STATE_REACTION_ONCE)
		MN_ExecuteConfunc("startreactiononce");
	else if (actor->state & STATE_REACTION_MANY)
		MN_ExecuteConfunc("startreactionmany");
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] le Pointer to local entity for which we refresh HUD buttons.
 * @sa HUD_Update
 */
static void HUD_RefreshWeaponButtons (const le_t *le)
{
	invList_t *weaponr;
	invList_t *weaponl;
	invList_t *headgear;
	int rightCanBeReloaded = -1, leftCanBeReloaded = -1;
	const int time = CL_ActorUsableTUs(le);
	const char *reason;

	if (!le)
		return;

	weaponr = RIGHT(le);
	headgear = HEADGEAR(le);

	/* check for two-handed weapon - if not, also define weaponl */
	if (!weaponr || !weaponr->item.t->holdTwoHanded)
		weaponl = LEFT(le);
	else
		weaponl = NULL;

	/* Crouch/stand button. */
	if (LE_IsCrouched(le)) {
		if (time + CL_ActorReservedTUs(le, RES_CROUCH) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for standing up."));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Stand up (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DESELECT);
		}
	} else {
		if (time + CL_ActorReservedTUs(le, RES_CROUCH) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for crouching."));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Crouch (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DESELECT);
		}
	}

	/* Crouch/stand reservation checkbox. */
	if (CL_ActorReservedTUs(le, RES_CROUCH) >= TU_CROUCH) {
		MN_ExecuteConfunc("crouch_checkbox_check");
		Cvar_Set("mn_crouch_reservation_tt", va(_("%i TUs reserved for crouching/standing up.\nClick to clear."),
				CL_ActorReservedTUs(le, RES_CROUCH)));
	} else if (time >= TU_CROUCH) {
		MN_ExecuteConfunc("crouch_checkbox_clear");
		Cvar_Set("mn_crouch_reservation_tt", va(_("Reserve %i TUs for crouching/standing up."), TU_CROUCH));
	} else {
		MN_ExecuteConfunc("crouch_checkbox_disable");
		Cvar_Set("mn_crouch_reservation_tt", _("Not enough TUs left to reserve for crouching/standing up."));
	}

	/* Shot reservation button. mn_shot_reservation_tt is the tooltip text */
	if (CL_ActorReservedTUs(le, RES_SHOT)) {
		MN_ExecuteConfunc("reserve_shot_check");
		Cvar_Set("mn_shot_reservation_tt", va(_("%i TUs reserved for shooting.\nClick to change.\nRight-Click to clear."),
				CL_ActorReservedTUs(le, RES_SHOT)));
	} else if (HUD_CheckFiremodeReservation()) {
		MN_ExecuteConfunc("reserve_shot_clear");
		Cvar_Set("mn_shot_reservation_tt", _("Reserve TUs for shooting."));
	} else {
		MN_ExecuteConfunc("reserve_shot_disable");
		Cvar_Set("mn_shot_reservation_tt", _("Reserving TUs for shooting not possible."));
	}

	/* reaction-fire button */
	if (!(le->state & STATE_REACTION)) {
		if (time >= CL_ActorReservedTUs(le, RES_REACTION) && HUD_WeaponWithReaction(le))
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DESELECT);
		else
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DISABLE);
	} else {
		if (HUD_WeaponWithReaction(le)) {
			HUD_DisplayPossibleReaction(le);
		} else {
			HUD_DisplayImpossibleReaction(le);
		}
	}

	/* Reload buttons */
	rightCanBeReloaded = HUD_WeaponCanBeReloaded(le, csi.idRight, &reason);
	if (rightCanBeReloaded != -1) {
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadright_tt", va(_("Reload weapon (%i TU)."), rightCanBeReloaded));
	} else {
		Cvar_Set("mn_reloadright_tt", reason);
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DISABLE);
	}

	leftCanBeReloaded = HUD_WeaponCanBeReloaded(le, csi.idLeft, &reason);
	if (leftCanBeReloaded != -1) {
		HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadleft_tt", va(_("Reload weapon (%i TU)."), leftCanBeReloaded));
	} else {
		Cvar_Set("mn_reloadleft_tt", reason);
		HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DISABLE);
	}

	/* Headgear button */
	if (headgear) {
		const int minheadgeartime = HUD_GetMinimumTUsForUsage(headgear);
		if (time < minheadgeartime)
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
	}

	/* Weapon firing buttons. */
	if (weaponr) {
		const int minweaponrtime = HUD_GetMinimumTUsForUsage(weaponr);
		if (time < minweaponrtime)
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
	}

	if (weaponl) {
		const int minweaponltime = HUD_GetMinimumTUsForUsage(weaponl);
		if (time < minweaponltime)
			HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
	}

	/* Check if the firemode reservation popup is shown and refresh its content. (i.e. close&open it) */
	{
		const char* menuName = MN_GetActiveWindowName();
		if (menuName[0] != '\0' && strstr(MN_GetActiveWindowName(), POPUPLIST_MENU_NAME)) {
			/* Update firemode reservation popup. */
			/** @todo this is called every frames... is it really need? */
			HUD_PopupFiremodeReservation(qfalse, qtrue);
		}
	}
}

/**
 * @brief Draw the mouse cursor tooltips in battlescape
 * @param xOffset
 * @param yOffset
 * @param textId The text id to get the tooltip string from.
 */
static void HUD_DrawMouseCursorText (int xOffset, int yOffset, int textId)
{
	const char *string = MN_GetText(textId);

	if (string && cl_show_cursor_tooltips->integer) {
		int width = 0;
		int height = 0;

		R_FontTextSize("f_verysmall", string, viddef.virtualWidth - mousePosX, LONGLINES_WRAP, &width, &height, NULL, NULL);

		if (!width)
			return;

		MN_DrawString("f_verysmall", ALIGN_UL, mousePosX + xOffset, mousePosY - yOffset, 0, 0, viddef.virtualWidth, viddef.virtualHeight, 12, string, 0, 0, NULL, qfalse, 0);
	}
}

/**
 * @brief Updates the cursor texts when in battlescape
 */
void HUD_UpdateCursor (void)
{
	/* Offset of the first icon on the x-axis. */
	int iconOffsetX = 16;
	/* Offset of the first icon on the y-axis. */
	int iconOffsetY = 16;
	/* the space between different icons. */
	const int iconSpacing = 2;
	le_t *le = selActor;
	if (le) {
		image_t *image;
		/* icon width */
		int iconW = 16;
		/* icon height. */
		int iconH = 16;
		int width = 0;
		int bgX = mousePosX + iconOffsetX / 2 - 2;
		int bgW = iconOffsetX / 2 + 4;
		int bgH = iconOffsetY + 6;

		/* checks if icons should be drawn */
		if (LE_IsCrouched(le) || (le->state & STATE_REACTION))
			bgW += iconW;
		else
			/* make place holder for icons */
			bgX += iconW + 4;

		/* if exists gets width of player name */
		if (MN_GetText(TEXT_MOUSECURSOR_PLAYERNAMES))
			R_FontTextSize("f_verysmall", MN_GetText(TEXT_MOUSECURSOR_PLAYERNAMES), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, NULL, NULL, NULL);

		/* check if second line should be drawn */
		if (width || (le->state & STATE_REACTION)) {
			bgH += iconH;
			bgW += width;
		}

		/* gets width of background */
		if (width == 0 && MN_GetText(TEXT_MOUSECURSOR_RIGHT)) {
			R_FontTextSize("f_verysmall", MN_GetText(TEXT_MOUSECURSOR_RIGHT), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, NULL, NULL, NULL);
			bgW += width;
		}

		/* Display 'crouch' icon if actor is crouched. */
		if (LE_IsCrouched(le)) {
			image = R_FindImage("pics/cursors/ducked", it_pic);
			if (image)
				R_DrawImage(mousePosX - image->width / 2 + iconOffsetX, mousePosY - image->height / 2 + iconOffsetY, image);
		}

		/* Height of 'crouched' icon. */
		iconOffsetY += 16;
		iconOffsetY += iconSpacing;

		/* Display 'Reaction shot' icon if actor has it activated. */
		if (le->state & STATE_REACTION_ONCE)
			image = R_FindImage("pics/cursors/reactionfire", it_pic);
		else if (le->state & STATE_REACTION_MANY)
			image = R_FindImage("pics/cursors/reactionfiremany", it_pic);
		else
			image = NULL;

		if (image)
			R_DrawImage(mousePosX - image->width / 2 + iconOffsetX, mousePosY - image->height / 2 + iconOffsetY, image);

		/* Height of 'reaction fire' icon. ... just in case we add further icons below.*/
		iconOffsetY += iconH;
		iconOffsetY += iconSpacing;

		/* Display weaponmode (text) heR_ */
		HUD_DrawMouseCursorText(iconOffsetX + iconW, -10, TEXT_MOUSECURSOR_RIGHT);
	}

	/* playernames */
	HUD_DrawMouseCursorText(iconOffsetX + 16, -26, TEXT_MOUSECURSOR_PLAYERNAMES);
	MN_ResetData(TEXT_MOUSECURSOR_PLAYERNAMES);

	if (cl_map_debug->integer & MAPDEBUG_TEXT) {
		/* Display ceiling text */
		HUD_DrawMouseCursorText(0, -64, TEXT_MOUSECURSOR_TOP);
		/* Display floor text */
		HUD_DrawMouseCursorText(0, 64, TEXT_MOUSECURSOR_BOTTOM);
		/* Display left text */
		HUD_DrawMouseCursorText(-64, 0, TEXT_MOUSECURSOR_LEFT);
	}
}

/**
 * @brief Updates console vars for an actor.
 *
 * This function updates the cvars for the hud (battlefield)
 * unlike CL_ActorCvars and CL_UGVCvars which updates them for
 * displaying the data in the menu system
 *
 * @sa CL_ActorCvars
 * @sa CL_UGVCvars
 */
void HUD_Update (void)
{
	static char infoText[MAX_SMALLMENUTEXTLEN];
	static char mouseText[MAX_SMALLMENUTEXTLEN];
	static char topText[MAX_SMALLMENUTEXTLEN];
	static char bottomText[MAX_SMALLMENUTEXTLEN];
	static char leftText[MAX_SMALLMENUTEXTLEN];
	static char tuTooltipText[MAX_SMALLMENUTEXTLEN];
	const char *animName;
	int time;
	pos3_t pos;

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	if (cls.state != ca_active)
		return;

	/* set Cvars for all actors */
	HUD_UpdateAllActors();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	if (selActor) {
		invList_t* invList;

		/* set generic cvars */
		Cvar_Set("mn_tu", va("%i", selActor->TU));
		Cvar_Set("mn_tumax", va("%i", selActor->maxTU));
		Cvar_Set("mn_tureserved", va("%i", CL_ActorReservedTUs(selActor, RES_ALL_ACTIVE)));

		Com_sprintf(tuTooltipText, lengthof(tuTooltipText),
			_("Time Units\n- Available: %i (of %i)\n- Reserved:  %i\n- Remaining: %i\n"),
			selActor->TU, selActor->maxTU,
			CL_ActorReservedTUs(selActor, RES_ALL_ACTIVE),
			CL_ActorUsableTUs(selActor));
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

		/* write info */
		time = 0;

		/* display special message */
		if (cl.time < hudTime)
			Q_strncpyz(infoText, hudText, sizeof(infoText));

		/* update HUD stats etc in more or shoot modes */
		if (displayRemainingTus[REMAINING_TU_CROUCH]) {
			if (CL_ActorUsableTUs(selActor) >= TU_CROUCH)
				time = TU_CROUCH;
		} else if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT]
		 || displayRemainingTus[REMAINING_TU_RELOAD_LEFT]) {
			const invList_t *invList;
			int container;

			if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] && RIGHT(selActor)) {
				invList = RIGHT(selActor);
				container = csi.idRight;
			} else if (displayRemainingTus[REMAINING_TU_RELOAD_LEFT] && LEFT(selActor)) {
				invList = HUD_GetLeftHandWeapon(selActor, &container);
			} else {
				invList = NULL;
				container = 0;
			}

			if (invList && invList->item.t && invList->item.m && invList->item.t->reload) {
				const int reloadtime = HUD_CalcReloadTime(selActor, invList->item.t, container);
				if (reloadtime != -1 && reloadtime <= CL_ActorUsableTUs(selActor))
					time = reloadtime;
			}
		} else if (CL_ActorFireModeActivated(selActor->actorMode)) {
			const invList_t *selWeapon;

			/* get weapon */
			if (IS_MODE_FIRE_HEADGEAR(selActor->actorMode)) {
				selWeapon = HEADGEAR(selActor);
			} else if (IS_MODE_FIRE_LEFT(selActor->actorMode)) {
				selWeapon = HUD_GetLeftHandWeapon(selActor, NULL);
			} else {
				selWeapon = RIGHT(selActor);
			}

			MN_ResetData(TEXT_MOUSECURSOR_RIGHT);

			if (selWeapon) {
				if (!selWeapon->item.t) {
					/* No valid weapon in the hand. */
					selActor->fd = NULL;
				} else {
					/* Check whether this item uses/has ammo. */
					if (!selWeapon->item.m) {
						selActor->fd = NULL;
						/* This item does not use ammo, check for existing firedefs in this item. */
						/* This is supposed to be a weapon or other usable item. */
						if (selWeapon->item.t->numWeapons > 0) {
							if (selWeapon->item.t->weapon || selWeapon->item.t->weapons[0] == selWeapon->item.t) {
								const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&selWeapon->item);
								/* Get firedef from the weapon (or other usable item) entry instead. */
								if (fdArray != NULL)
									selActor->fd = FIRESH_GetFiredef(selWeapon->item.t, fdArray->fdIdx, selActor->currentSelectedFiremode);
							}
						}
					} else {
						const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&selWeapon->item);
						if (fdArray != NULL) {
							const fireDef_t *old = FIRESH_GetFiredef(selWeapon->item.m, fdArray->fdIdx, selActor->currentSelectedFiremode);
							/* reset the align if we switched the firemode */
							if (old != selActor->fd)
								mousePosTargettingAlign = 0;
							selActor->fd = old;
						}
					}
				}

				if (!GAME_ItemIsUseable(selWeapon->item.t)) {
					HUD_DisplayMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"));
					CL_ActorSetMode(selActor, M_MOVE);
				} else if (selActor->fd) {
					const int hitProbability = CL_GetHitProbability(selActor);

					Com_sprintf(infoText, lengthof(infoText),
								"%s\n%s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(selActor->fd->name),
								selActor->fd->ammo, hitProbability, selActor->fd->time);

					/* Save the text for later display next to the cursor. */
					Q_strncpyz(mouseText, infoText, lengthof(mouseText));
					MN_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);

					time = selActor->fd->time;
					/* if no TUs left for this firing action
					 * or if the weapon is reloadable and out of ammo,
					 * then change to move mode */
					if ((selWeapon->item.t->reload && selWeapon->item.a <= 0) || CL_ActorUsableTUs(selActor) < time)
						CL_ActorSetMode(selActor, M_MOVE);
				} else if (selWeapon) {
					Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(selWeapon->item.t->name));
				}
			} else {
				CL_ActorSetMode(selActor, M_MOVE);
			}
		} else {
			const int reservedTUs = CL_ActorReservedTUs(selActor, RES_ALL_ACTIVE);
			/* If the mouse is outside the world, and we haven't placed the cursor in pend
			 * mode already or the selected grid field is not reachable (ROUTING_NOT_REACHABLE) */
			if ((mouseSpace != MS_WORLD && selActor->actorMode < M_PEND_MOVE) || selActor->actorMoveLength == ROUTING_NOT_REACHABLE) {
				selActor->actorMoveLength = ROUTING_NOT_REACHABLE;
				if (reservedTUs > 0)
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n"), selActor->morale, reservedTUs);
				else
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i"), selActor->morale);
				MN_ResetData(TEXT_MOUSECURSOR_RIGHT);
			}
			if (selActor->actorMoveLength != ROUTING_NOT_REACHABLE) {
				const int moveMode = CL_ActorMoveMode(selActor, selActor->actorMoveLength);
				if (reservedTUs > 0)
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n%s %i (%i|%i TU left)\n"),
						selActor->morale, reservedTUs, _(moveModeDescriptions[moveMode]), selActor->actorMoveLength,
						selActor->TU - selActor->actorMoveLength, selActor->TU - reservedTUs - selActor->actorMoveLength);
				else
					Com_sprintf(infoText, lengthof(infoText), _("Morale  %i\n%s %i (%i TU left)\n"), selActor->morale,
						moveModeDescriptions[moveMode] , selActor->actorMoveLength, selActor->TU - selActor->actorMoveLength);

				if (selActor->actorMoveLength <= CL_ActorUsableTUs(selActor))
					Com_sprintf(mouseText, lengthof(mouseText), "%i (%i)\n", selActor->actorMoveLength, CL_ActorUsableTUs(selActor));
				else
					Com_sprintf(mouseText, lengthof(mouseText), "- (-)\n");

				MN_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);
			}
			time = selActor->actorMoveLength;
		}

		/* handle actor in a panic */
		if (LE_IsPaniced(selActor)) {
			Com_sprintf(infoText, lengthof(infoText), _("Currently panics!\n"));
			MN_ResetData(TEXT_MOUSECURSOR_RIGHT);
		}

		/* Find the coordinates of the ceiling and floor we want. */
		VectorCopy(mousePos, pos);
		pos[2] = cl_worldlevel->integer;

		if (cl_map_debug->integer & MAPDEBUG_TEXT) {
			int dv;

			/* Display the floor and ceiling values for the current cell. */
			Com_sprintf(topText, lengthof(topText), "%u-(%i,%i,%i)\n", Grid_Ceiling(clMap, fieldSize, truePos), truePos[0], truePos[1], truePos[2]);
			/* Save the text for later display next to the cursor. */
			MN_RegisterText(TEXT_MOUSECURSOR_TOP, topText);

			/* Display the floor and ceiling values for the current cell. */
			Com_sprintf(bottomText, lengthof(bottomText), "%i-(%i,%i,%i)\n", Grid_Floor(clMap, fieldSize, truePos), mousePos[0], mousePos[1], mousePos[2]);
			/* Save the text for later display next to the cursor. */
			MN_RegisterText(TEXT_MOUSECURSOR_BOTTOM, bottomText);

			/* Display the floor and ceiling values for the current cell. */
			dv = Grid_MoveNext(clMap, fieldSize, selActor->pathMap, mousePos, 0);
			Com_sprintf(leftText, lengthof(leftText), "%i-%i\n", getDVdir(dv), getDVz(dv));
			/* Save the text for later display next to the cursor. */
			MN_RegisterText(TEXT_MOUSECURSOR_LEFT, leftText);
		}

		/* print ammo */
		if (RIGHT(selActor))
			Cvar_SetValue("mn_ammoright", RIGHT(selActor)->item.a);
		else
			Cvar_Set("mn_ammoright", "");

		invList = HUD_GetLeftHandWeapon(selActor, NULL);
		if (invList)
			Cvar_SetValue("mn_ammoleft", invList->item.a);
		else
			Cvar_Set("mn_ammoleft", "");

		/* Calculate remaining TUs. */
		/* We use the full count of TUs since the "reserved" bar is overlaid over this one. */
		time = max(0, selActor->TU - time);
		if (Cvar_GetInteger("mn_turemain") != time) {
			Cvar_Set("mn_turemain", va("%i", time));

			HUD_RefreshWeaponButtons(selActor);
		}

		MN_RegisterText(TEXT_STANDARD, infoText);
	} else if (!cl.numTeamList) {
		/* This will stop the drawing of the bars over the whole screen when we test maps. */
		Cvar_SetValue("mn_tu", 0);
		Cvar_SetValue("mn_tumax", 100);
		Cvar_SetValue("mn_morale", 0);
		Cvar_SetValue("mn_moralemax", 100);
		Cvar_SetValue("mn_hp", 0);
		Cvar_SetValue("mn_hpmax", 100);
		Cvar_SetValue("mn_stun", 0);
	}
}

/**
 * @brief Callback that is called when the cl_selected cvar was changed
 * @param cvarName The cvarname (cl_selected)
 * @param oldValue The old value (a sane actor idx)
 * @param newValue The new value (a sane actor idx)
 */
static void HUD_ActorSelectionChangeListener (const char *cvarName, const char *oldValue, const char *newValue)
{
	if (!CL_OnBattlescape())
		return;

	if (oldValue[0] != '\0') {
		const int actorIdx = atoi(oldValue);
		if (actorIdx >= 0 && actorIdx < MAX_TEAMLIST) {
			/* if the actor is still living */
			if (cl.teamList[actorIdx])
				MN_ExecuteConfunc("huddeselect %s", oldValue);
		}
	}

	if (newValue[0] != '\0') {
		const int actorIdx = atoi(newValue);
		if (actorIdx >= 0 && actorIdx < MAX_TEAMLIST)
			MN_ExecuteConfunc("hudselect %s", newValue);
	}
}

void HUD_InitStartup (void)
{
	HUD_InitCallbacks();

	Cmd_AddCommand("hud_remainingtus", HUD_RemainingTUs_f, "Define if remaining TUs should be displayed in the TU-bar for some hovered-over button.");
	Cmd_AddCommand("hud_shotreserve", HUD_ShotReserve_f, "Reserve The TUs for the selected entry in the popup.");
	Cmd_AddCommand("hud_shotreservationpopup", HUD_PopupFiremodeReservation_f, "Pop up a list of possible firemodes for reservation in the current turn.");
	Cmd_AddCommand("hud_switchfiremodelist", HUD_SwitchFiremodeList_f, "Switch firemode-list to one for the given hand, but only if the list is visible already.");
	Cmd_AddCommand("hud_selectreactionfiremode", HUD_SelectReactionFiremode_f, "Change/Select firemode used for reaction fire.");
	Cmd_AddCommand("hud_listfiremodes", HUD_DisplayFiremodes_f, "Display a list of firemodes for a weapon+ammo.");

	Cvar_RegisterChangeListener("cl_selected", HUD_ActorSelectionChangeListener);

	cl_hud_message_timeout = Cvar_Get("cl_hud_message_timeout", "2000", CVAR_ARCHIVE, "Timeout for HUD messages (milliseconds)");
	cl_show_cursor_tooltips = Cvar_Get("cl_show_cursor_tooltips", "1", CVAR_ARCHIVE, "Show cursor tooltips in tactical game mode");
}
