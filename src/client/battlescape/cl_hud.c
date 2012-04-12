/**
 * @file cl_hud.c
 * @brief HUD related routines.
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

#include "../client.h"
#include "cl_localentity.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_hud_callbacks.h"
#include "cl_view.h"
#include "../cgame/cl_game.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "../ui/ui_nodes.h"
#include "../ui/ui_draw.h"
#include "../ui/ui_render.h"
#include "../ui/ui_tooltip.h"
#include "../renderer/r_mesh_anim.h"
#include "../renderer/r_draw.h"
#include "../../common/grid.h"

static cvar_t *cl_hud_message_timeout;
static cvar_t *cl_show_cursor_tooltips;
cvar_t *cl_worldlevel;
cvar_t *cl_hud;

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
static char const* const shootTypeStrings[] = {
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
	BT_STATE_DESELECT		/**< Normal display (blue) */
} weaponButtonState_t;

/** @note Order of elements here must correspond to order of elements in walkType_t. */
static char const* const moveModeDescriptions[] = {
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

/**
 * @brief Displays a message on the hud.
 * @sa UI_DisplayNotice
 * @param[in] text text is already translated here
 */
void HUD_DisplayMessage (const char *text)
{
	assert(text);
	UI_DisplayNotice(text, cl_hud_message_timeout->integer, cl_hud->string);
}

/**
 * @brief Updates the global character cvars for battlescape.
 * @note This is only called when we are in battlescape rendering mode
 * It's assumed that every living actor - @c le_t - has a character assigned, too
 */
static void HUD_UpdateAllActors (void)
{
	int i;
	const size_t size = lengthof(cl.teamList);

	Cvar_SetValue("mn_numaliensspotted", cl.numEnemiesSpotted);
	for (i = 0; i < size; i++) {
		const le_t *le = cl.teamList[i];
		if (le && !LE_IsDead(le)) {
			const invList_t *invList;
			const char* tooltip;
			const character_t *chr = CL_ActorGetChr(le);
			assert(chr);

			invList = RIGHT(le);
			if ((!invList || !invList->item.t || !invList->item.t->holdTwoHanded) && LEFT(le))
				invList = LEFT(le);

			tooltip = va(_("%s\nHP: %i/%i TU: %i\n%s"),
				chr->name, le->HP, le->maxHP, le->TU, (invList && invList->item.t) ? _(invList->item.t->name) : "");

			UI_ExecuteConfunc("updateactorvalues %i \"%s\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%s\"",
					i, le->model2->name, le->HP, le->maxHP, le->TU, le->maxTU, le->morale, le->maxMorale, le->STUN, tooltip);
		}
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @todo This should be a confunc which also sets the tooltips
 */
static void HUD_SetWeaponButton (buttonTypes_t button, weaponButtonState_t state)
{
	const char const* prefix;

	switch (state) {
	case BT_STATE_DESELECT:
		prefix = "deselect_";
		break;
	case BT_STATE_DISABLE:
		prefix = "disable_";
		break;
	default:
		prefix = "";
		break;
	}

	/* Connect confunc strings to the ones as defined in "menu hud_nohud". */
	UI_ExecuteConfunc("%s%s", prefix, shootTypeStrings[button]);
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
 * @sa HUD_RefreshButtons
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


/**
 * @brief Sets TU-reservation and firemode
 * @param[in] le The local entity of the actor to change the tu reservation for.
 * @param[in] tus How many TUs to set.
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weapon Pointer to weapon in the hand.
 */
static void HUD_SetShootReservation (const le_t* le, const int tus, const actorHands_t hand, const int fireModeIndex, const objDef_t *weapon)
{
	character_t* chr = CL_ActorGetChr(le);
	assert(chr);

	CL_ActorReserveTUs(le, RES_SHOT, tus);
	CL_ActorSetShotSettings(chr, hand, fireModeIndex, weapon);
}

static linkedList_t* popupListData;
static uiNode_t* popupListNode;

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @param[in] le The actor local entity
 * @param[in] popupReload Prevent firemode reservation popup from being closed if
 * no firemode is available because of insufficient TUs.
 * @sa HUD_PopupFiremodeReservation_f
 * @sa HUD_CheckFiremodeReservation
 * @todo use components and confuncs here
 */
static void HUD_PopupFiremodeReservation (const le_t *le, qboolean popupReload)
{
	actorHands_t hand = ACTOR_HAND_RIGHT;
	int i;
	static char text[MAX_VAR];
	int selectedEntry;
	linkedList_t* popupListText = NULL;
	reserveShot_t reserveShotData;

	/* reset the list */
	UI_ResetData(TEXT_LIST);

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
		const fireDef_t *fd = HUD_GetFireDefinitionForHand(le, hand);
		character_t* chr = CL_ActorGetChr(le);
		assert(chr);

		if (fd) {
			const objDef_t *ammo = fd->obj;

			for (i = 0; i < ammo->numFiredefs[fd->weapFdsIdx]; i++) {
				const fireDef_t* ammoFD = &ammo->fd[fd->weapFdsIdx][i];
				if (CL_ActorUsableTUs(le) + CL_ActorReservedTUs(le, RES_SHOT) >= ammoFD->time) {
					/* Get firemode name and TUs. */
					Com_sprintf(text, lengthof(text), _("[%i TU] %s"), ammoFD->time, _(ammoFD->name));

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
		popupListNode = UI_PopupList(_("Shot Reservation"), _("Reserve TUs for firing/using."), popupListText, "hud_shotreserve <lineselected>");
		/* Set color for selected entry. */
		VectorSet(popupListNode->selectedColor, 0.0, 0.78, 0.0);
		popupListNode->selectedColor[3] = 1.0;
		UI_TextNodeSelectLine(popupListNode, selectedEntry);
	}
}

/**
 * @brief Creates a (text) list of all firemodes of the currently selected actor.
 * @sa HUD_PopupFiremodeReservation
 */
static void HUD_PopupFiremodeReservation_f (void)
{
	if (!selActor)
		return;

	/* A second parameter (the value itself will be ignored) was given.
	 * This is used to reset the shot-reservation.*/
	if (Cmd_Argc() == 2) {
		HUD_SetShootReservation(selActor, 0, ACTOR_HAND_NOT_SET, -1, NULL);
	} else {
		HUD_PopupFiremodeReservation(selActor, qfalse);
	}
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

	reserveShotData = (const reserveShot_t *)LIST_GetByIdx(popupListData, selectedPopupIndex);
	if (!reserveShotData)
		return;

	if (reserveShotData->weaponIndex == NONE) {
		HUD_SetShootReservation(selActor, 0, ACTOR_HAND_NOT_SET, -1, NULL);
		return;
	}

	/** @todo do this on the server */
	/* Check if we have enough TUs (again) */
	if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= reserveShotData->TUs) {
		const objDef_t *od = INVSH_GetItemByIDX(reserveShotData->weaponIndex);
		if (GAME_ItemIsUseable(od)) {
			HUD_SetShootReservation(selActor, max(0, reserveShotData->TUs), reserveShotData->hand, reserveShotData->fireModeIndex, od);
			if (popupListNode)
				UI_TextNodeSelectLine(popupListNode, selectedPopupIndex);
		}
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param[in] hand What list to display
 */
static void HUD_DisplayFiremodeEntry (const char* callback, const le_t* actor, const objDef_t* ammo, const weaponFireDefIndex_t weapFdsIdx, const actorHands_t hand, int index)
{
	int usableTusForRF;
	char tuString[MAX_VAR];
	qboolean status;
	const fireDef_t *fd;
	const char *tooltip;
	char id[32];

	if (index < ammo->numFiredefs[weapFdsIdx]) {
		/* We have a defined fd ... */
		fd = &ammo->fd[weapFdsIdx][index];
	} else {
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

	/* unique identifier of the action */
	/* @todo use this id as action callback instead of hand and index (we can extend it with any other soldier action we need (open door, reload...)) */
	Com_sprintf(id, sizeof(id), "fire_hand%c_i%i", ACTOR_GET_HAND_CHAR(hand), index);

	UI_ExecuteConfunc("%s firemode %s %c %i %i %i \"%s\" \"%i\" \"%i\" \"%s\"", callback, id, ACTOR_GET_HAND_CHAR(hand),
			fd->fdIdx, fd->reaction, status, _(fd->name), fd->time, fd->ammo, tooltip);

	/* Display checkbox for reaction firemode */
	if (fd->reaction) {
		character_t* chr = CL_ActorGetChr(actor);
		const qboolean active = THIS_FIREMODE(&chr->RFmode, hand, fd->fdIdx);
		/* Change the state of the checkbox. */
		UI_ExecuteConfunc("%s reaction %s %c %i", callback, id, ACTOR_GET_HAND_CHAR(hand), active);
	}
}

/**
 * List actions from a soldier to a callback confunc
 * @param callback confunc callback
 * @param actor actor who can do the actions
 * @param right if true, list right firemode
 * @param left if true, list left firemode
 * @param reloadRight if true, list right weapon reload actions
 * @param reloadLeft if true, list left weapon reload actions
 * @todo we can extend it with short cut equip action, more reload, action on the map (like open doors)...
 */
static void HUD_DisplayActions (const char* callback, const le_t* actor, qboolean right, qboolean left, qboolean reloadRight, qboolean reloadLeft)
{
	const objDef_t *ammo;
	const fireDef_t *fd;
	int i;

	if (!actor)
		return;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		return;
	}

	UI_ExecuteConfunc("%s begin", callback);

	if (right) {
		const actorHands_t hand = ACTOR_HAND_RIGHT;
		fd = HUD_GetFireDefinitionForHand(actor, hand);
		if (fd == NULL)
			return;

		ammo = fd->obj;
		if (!ammo) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes: no weapon or ammo found.\n");
			return;
		}

		for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
			/* Display the firemode information (image + text). */
			HUD_DisplayFiremodeEntry(callback, actor, ammo, fd->weapFdsIdx, hand, i);
		}
	}

	if (reloadRight) {
		invList_t* weapon = RIGHT(actor);

		/* Reloeadable item in hand. */
		if (weapon && weapon->item.t && weapon->item.t->reload) {
			int tus;
			containerIndex_t container = csi.idRight;
			qboolean noAmmo;
			qboolean noTU;
			const char *actionId = "reload_handr";

			tus = HUD_CalcReloadTime(actor, weapon->item.t, container);
			noAmmo = tus == -1;
			noTU = actor->TU < tus;
			UI_ExecuteConfunc("%s reload %s %c %i %i %i", callback, actionId, 'r', tus, !noAmmo, !noTU);
		}
	}

	if (left) {
		const actorHands_t hand = ACTOR_HAND_LEFT;
		fd = HUD_GetFireDefinitionForHand(actor, hand);
		if (fd == NULL)
			return;

		ammo = fd->obj;
		if (!ammo) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes: no weapon or ammo found.\n");
			return;
		}

		for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
			/* Display the firemode information (image + text). */
			HUD_DisplayFiremodeEntry(callback, actor, ammo, fd->weapFdsIdx, hand, i);
		}
	}


	if (reloadLeft) {
		invList_t* weapon = LEFT(actor);

		/* Reloeadable item in hand. */
		if (weapon && weapon->item.t && weapon->item.t->reload) {
			int tus;
			containerIndex_t container = csi.idLeft;
			qboolean noAmmo;
			qboolean noTU;
			const char *actionId = "reload_handl";

			tus = HUD_CalcReloadTime(actor, weapon->item.t, container);
			noAmmo = tus == -1;
			noTU = actor->TU < tus;
			UI_ExecuteConfunc("%s reload %s %c %i %i %i", callback, actionId, 'l', tus, !noAmmo, !noTU);
		}
	}

	UI_ExecuteConfunc("%s end", callback);
}

/**
 * @brief Displays the firemodes for the given hand.
 */
static void HUD_DisplayActions_f (void)
{
	char callback[32];
	qboolean right;
	qboolean left;
	qboolean rightReload;
	qboolean leftReload;

	if (!selActor)
		return;

	right = strchr(Cmd_Argv(2), 'r') != NULL;
	left = strchr(Cmd_Argv(2), 'l') != NULL;
	rightReload = strchr(Cmd_Argv(2), 'R') != NULL;
	leftReload = strchr(Cmd_Argv(2), 'L') != NULL;

	Q_strncpyz(callback, Cmd_Argv(1), sizeof(callback));
	HUD_DisplayActions(callback, selActor, right, left, rightReload, leftReload);
}

/**
 * @brief Displays the firemodes for the given hand.
 */
static void HUD_DisplayFiremodes_f (void)
{
	actorHands_t hand;
	char callback[32];

	if (!selActor)
		return;

	if (Cmd_Argc() < 3)
		/* no argument given */
		hand = ACTOR_HAND_RIGHT;
	else
		hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(2)[0]);

	Q_strncpyz(callback, Cmd_Argv(1), sizeof(callback));
	HUD_DisplayActions(callback, selActor, hand == ACTOR_HAND_RIGHT, hand == ACTOR_HAND_LEFT, qfalse, qfalse);
}

/**
 * @brief Changes the display of the firemode-list to a given hand, but only if the list is visible already.
 * @todo Delete that function: Should be done from within the scripts
 */
static void HUD_SwitchFiremodeList_f (void)
{
	/* no argument given */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s callback [l|r]\n", Cmd_Argv(0));
		return;
	}

	{
		char callback[32];
		actorHands_t hand;
		hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(2)[0]);
		Q_strncpyz(callback, Cmd_Argv(1), sizeof(callback));
		HUD_DisplayActions(callback, selActor, hand == ACTOR_HAND_RIGHT, hand == ACTOR_HAND_LEFT, qfalse, qfalse);
	}
}

/**
 * @brief Updates the information in RFmode for the selected actor with the given data from the parameters.
 * @param[in] actor The actor we want to update the reaction-fire firemode for.
 * @param[in] hand Which weapon(-hand) to use.
 * @param[in] firemodeActive Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
static void HUD_UpdateReactionFiremodes (const le_t * actor, const actorHands_t hand, fireDefIndex_t firemodeActive)
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

	MSG_Write_PA(PA_REACT_SELECT, actor->entnum, hand, firemodeActive, od ? od->idx : NONE);
}

/**
 * @brief Checks if the selected firemode checkbox is ok as a reaction firemode and updates data+display.
 */
static void HUD_SelectReactionFiremode_f (void)
{
	actorHands_t hand;
	fireDefIndex_t firemode;

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

	OBJZERO(displayRemainingTus);

	if (Q_streq(type, "reload_r")) {
		displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] = state;
	} else if (Q_streq(type, "reload_l")) {
		displayRemainingTus[REMAINING_TU_RELOAD_LEFT] = state;
	} else if (Q_streq(type, "crouch")) {
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
	int i;

	assert(invList->item.t);

	fdArray = FIRESH_FiredefForWeapon(&invList->item);
	if (fdArray == NULL)
		return time;

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
 * @param[in] containerID of the container to reload the weapon in. Used to get the movement TUs for moving something into the container.
 * @param[out] reason The reason why the reload didn't work - only set if @c -1 is the return value
 * @return TU units needed for reloading or -1 if weapon cannot be reloaded.
 */
static int HUD_WeaponCanBeReloaded (const le_t *le, containerIndex_t containerID, const char **reason)
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
	const objDef_t *weapon = INVSH_HasReactionFireEnabledWeapon(RIGHT(actor));
	if (weapon)
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
	if (actor->state & STATE_REACTION) {
		UI_ExecuteConfunc("startreaction_impos");
		return qfalse;
	}

	return qtrue;
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
	if (actor->state & STATE_REACTION)
		UI_ExecuteConfunc("startreaction");
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] le Pointer to local entity for which we refresh HUD buttons.
 */
static void HUD_RefreshButtons (const le_t *le)
{
	invList_t *weaponr;
	invList_t *weaponl;
	invList_t *headgear;
	int rightCanBeReloaded, leftCanBeReloaded;
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
		UI_ExecuteConfunc("crouch_checkbox_check");
		Cvar_Set("mn_crouch_reservation_tt", va(_("%i TUs reserved for crouching/standing up.\nClick to clear."),
				CL_ActorReservedTUs(le, RES_CROUCH)));
	} else if (time >= TU_CROUCH) {
		UI_ExecuteConfunc("crouch_checkbox_clear");
		Cvar_Set("mn_crouch_reservation_tt", va(_("Reserve %i TUs for crouching/standing up."), TU_CROUCH));
	} else {
		UI_ExecuteConfunc("crouch_checkbox_disable");
		Cvar_Set("mn_crouch_reservation_tt", _("Not enough TUs left to reserve for crouching/standing up."));
	}

	/* Shot reservation button. mn_shot_reservation_tt is the tooltip text */
	if (CL_ActorReservedTUs(le, RES_SHOT)) {
		UI_ExecuteConfunc("reserve_shot_check");
		Cvar_Set("mn_shot_reservation_tt", va(_("%i TUs reserved for shooting.\nClick to change.\nRight-Click to clear."),
				CL_ActorReservedTUs(le, RES_SHOT)));
	} else if (HUD_CheckFiremodeReservation()) {
		UI_ExecuteConfunc("reserve_shot_clear");
		Cvar_Set("mn_shot_reservation_tt", _("Reserve TUs for shooting."));
	} else {
		UI_ExecuteConfunc("reserve_shot_disable");
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
		const char* menuName = UI_GetActiveWindowName();
		if (menuName[0] != '\0' && strstr(UI_GetActiveWindowName(), POPUPLIST_NODE_NAME)) {
			/* Update firemode reservation popup. */
			/** @todo this is called every frames... is this really needed? */
			HUD_PopupFiremodeReservation(le, qtrue);
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
	const char *string = UI_GetText(textId);
	if (string && cl_show_cursor_tooltips->integer)
		UI_DrawTooltip(string, mousePosX + xOffset, mousePosY - yOffset, viddef.virtualWidth - mousePosX);
}

/**
 * @brief Updates the cursor texts when in battlescape
 */
void HUD_UpdateCursor (void)
{
	/* Offset of the first icon on the x-axis. */
	int iconOffsetX = 16;
	/* Offset of the first icon on the y-axis. */
	/* the space between different icons. */
	const int iconSpacing = 2;
	le_t *le = selActor;
	if (le) {
		int iconOffsetY = 16;
		image_t *image;
		/* icon width */
		int iconW = 16;
		/* icon height. */
		int iconH = 16;
		int width = 0;
		int bgX = mousePosX + iconOffsetX / 2 - 2;

		/* checks if icons should be drawn */
		if (!(LE_IsCrouched(le) || (le->state & STATE_REACTION)))
			/* make place holder for icons */
			bgX += iconW + 4;

		/* if exists gets width of player name */
		if (UI_GetText(TEXT_MOUSECURSOR_PLAYERNAMES))
			R_FontTextSize("f_verysmall", UI_GetText(TEXT_MOUSECURSOR_PLAYERNAMES), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, NULL, NULL, NULL);

		/* gets width of background */
		if (width == 0 && UI_GetText(TEXT_MOUSECURSOR_RIGHT)) {
			R_FontTextSize("f_verysmall", UI_GetText(TEXT_MOUSECURSOR_RIGHT), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, NULL, NULL, NULL);
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
		if (le->state & STATE_REACTION)
			image = R_FindImage("pics/cursors/reactionfire", it_pic);
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
	UI_ResetData(TEXT_MOUSECURSOR_PLAYERNAMES);

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
 * @brief Shows map pathfinding debugging parameters (if activated)
 * @param[in] le The current selected actors entity
 */
static void HUD_MapDebugCursor (const le_t *le)
{
	if (cl_map_debug->integer & MAPDEBUG_TEXT) {
		int dvec;

		static char topText[UI_MAX_SMALLTEXTLEN];
		static char bottomText[UI_MAX_SMALLTEXTLEN];
		static char leftText[UI_MAX_SMALLTEXTLEN];

		/* Display the floor and ceiling values for the current cell. */
		Com_sprintf(topText, lengthof(topText), "%u-(%i,%i,%i)\n",
				Grid_Ceiling(cl.mapData->map, ACTOR_GET_FIELDSIZE(le), truePos), truePos[0], truePos[1], truePos[2]);
		/* Save the text for later display next to the cursor. */
		UI_RegisterText(TEXT_MOUSECURSOR_TOP, topText);

		/* Display the floor and ceiling values for the current cell. */
		Com_sprintf(bottomText, lengthof(bottomText), "%i-(%i,%i,%i)\n",
				Grid_Floor(cl.mapData->map, ACTOR_GET_FIELDSIZE(le), truePos), mousePos[0], mousePos[1], mousePos[2]);
		/* Save the text for later display next to the cursor. */
		UI_RegisterText(TEXT_MOUSECURSOR_BOTTOM, bottomText);

		/* Display the floor and ceiling values for the current cell. */
		dvec = Grid_MoveNext(&cl.pathMap, mousePos, 0);
		Com_sprintf(leftText, lengthof(leftText), "%i-%i\n", getDVdir(dvec), getDVz(dvec));
		/* Save the text for later display next to the cursor. */
		UI_RegisterText(TEXT_MOUSECURSOR_LEFT, leftText);
	}
}

/**
 * @param actor The actor to update the hud for
 * @return The amount of TUs needed for the current pending action
 */
static int HUD_UpdateActorFireMode (le_t *actor)
{
	const invList_t *selWeapon;
	int time = 0;

	/* get weapon */
	if (IS_MODE_FIRE_HEADGEAR(actor->actorMode)) {
		selWeapon = HEADGEAR(actor);
	} else if (IS_MODE_FIRE_LEFT(actor->actorMode)) {
		selWeapon = HUD_GetLeftHandWeapon(actor, NULL);
	} else {
		selWeapon = RIGHT(actor);
	}

	UI_ResetData(TEXT_MOUSECURSOR_RIGHT);

	if (selWeapon) {
		static char infoText[UI_MAX_SMALLTEXTLEN];

		if (!selWeapon->item.t) {
			/* No valid weapon in the hand. */
			CL_ActorSetFireDef(actor, NULL);
		} else {
			/* Check whether this item uses/has ammo. */
			if (!selWeapon->item.m) {
				CL_ActorSetFireDef(actor, NULL);
				/* This item does not use ammo, check for existing firedefs in this item. */
				/* This is supposed to be a weapon or other usable item. */
				if (selWeapon->item.t->numWeapons > 0) {
					if (selWeapon->item.t->weapon || selWeapon->item.t->weapons[0] == selWeapon->item.t) {
						const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&selWeapon->item);
						if (fdArray != NULL) {
							/* Get firedef from the weapon (or other usable item) entry instead. */
							const fireDef_t *old = FIRESH_GetFiredef(selWeapon->item.t, fdArray->weapFdsIdx, actor->currentSelectedFiremode);
							CL_ActorSetFireDef(actor, old);
						}
					}
				}
			} else {
				const fireDef_t *fdArray = FIRESH_FiredefForWeapon(&selWeapon->item);
				if (fdArray != NULL) {
					const fireDef_t *old = FIRESH_GetFiredef(selWeapon->item.m, fdArray->weapFdsIdx, actor->currentSelectedFiremode);
					/* reset the align if we switched the firemode */
					CL_ActorSetFireDef(actor, old);
				}
			}
		}

		if (!GAME_ItemIsUseable(selWeapon->item.t)) {
			HUD_DisplayMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"));
			CL_ActorSetMode(actor, M_MOVE);
		} else if (actor->fd) {
			const int hitProbability = CL_GetHitProbability(actor);
			static char mouseText[UI_MAX_SMALLTEXTLEN];

			Com_sprintf(infoText, lengthof(infoText),
						"%s\n%s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(actor->fd->name),
						actor->fd->ammo, hitProbability, actor->fd->time);

			/* Save the text for later display next to the cursor. */
			Q_strncpyz(mouseText, infoText, lengthof(mouseText));
			UI_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);

			time = actor->fd->time;
			/* if no TUs left for this firing action
			 * or if the weapon is reloadable and out of ammo,
			 * then change to move mode */
			if ((selWeapon->item.t->reload && selWeapon->item.a <= 0) || CL_ActorUsableTUs(actor) < time)
				CL_ActorSetMode(actor, M_MOVE);
		} else if (selWeapon) {
			Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(selWeapon->item.t->name));
		}

		UI_RegisterText(TEXT_STANDARD, infoText);
	} else {
		CL_ActorSetMode(actor, M_MOVE);
	}

	return time;
}

/**
 * @param[in] actor The actor to update the hud for
 * @return The amount of TUs needed for the current pending action
 */
static int HUD_UpdateActorMove (const le_t *actor)
{
	const int reservedTUs = CL_ActorReservedTUs(actor, RES_ALL_ACTIVE);
	static char infoText[UI_MAX_SMALLTEXTLEN];
	if (actor->actorMoveLength == ROUTING_NOT_REACHABLE) {
		UI_ResetData(TEXT_MOUSECURSOR_RIGHT);
		if (reservedTUs > 0)
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n"), actor->morale, reservedTUs);
		else
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i"), actor->morale);
	} else {
		static char mouseText[UI_MAX_SMALLTEXTLEN];
		const int moveMode = CL_ActorMoveMode(actor, actor->actorMoveLength);
		if (reservedTUs > 0)
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n%s %i (%i|%i TU left)\n"),
				actor->morale, reservedTUs, _(moveModeDescriptions[moveMode]), actor->actorMoveLength,
				actor->TU - actor->actorMoveLength, actor->TU - reservedTUs - actor->actorMoveLength);
		else
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i\n%s %i (%i TU left)\n"), actor->morale,
				_(moveModeDescriptions[moveMode]), actor->actorMoveLength, actor->TU - actor->actorMoveLength);

		if (actor->actorMoveLength <= CL_ActorUsableTUs(actor))
			Com_sprintf(mouseText, lengthof(mouseText), "%i (%i)\n", actor->actorMoveLength, CL_ActorUsableTUs(actor));
		else
			Com_sprintf(mouseText, lengthof(mouseText), "- (-)\n");

		UI_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);
	}

	UI_RegisterText(TEXT_STANDARD, infoText);

	return actor->actorMoveLength;
}

static void HUD_UpdateActorCvar (const le_t *actor, const char *cvarPrefix)
{
	const invList_t* invList;
	const char *animName;
	static char tuTooltipText[UI_MAX_SMALLTEXTLEN];

	Cvar_SetValue(va("%s%s", cvarPrefix, "hp"), actor->HP);
	Cvar_SetValue(va("%s%s", cvarPrefix, "hpmax"), actor->maxHP);
	Cvar_SetValue(va("%s%s", cvarPrefix, "tu"), actor->TU);
	Cvar_SetValue(va("%s%s", cvarPrefix, "tumax"), actor->maxTU);
	Cvar_SetValue(va("%s%s", cvarPrefix, "tureserved"), CL_ActorReservedTUs(actor, RES_ALL_ACTIVE));
	Cvar_SetValue(va("%s%s", cvarPrefix, "morale"), actor->morale);
	Cvar_SetValue(va("%s%s", cvarPrefix, "moralemax"), actor->maxMorale);
	Cvar_SetValue(va("%s%s", cvarPrefix, "stun"), actor->STUN);

	Com_sprintf(tuTooltipText, lengthof(tuTooltipText),
		_("Time Units\n- Available: %i (of %i)\n- Reserved:  %i\n- Remaining: %i\n"),
				actor->TU, actor->maxTU, CL_ActorReservedTUs(actor, RES_ALL_ACTIVE), CL_ActorUsableTUs(actor));
	Cvar_Set(va("%s%s", cvarPrefix, "tu_tooltips"), tuTooltipText);

	/* animation and weapons */
	animName = R_AnimGetName(&actor->as, actor->model1);
	if (animName)
		Cvar_Set(va("%s%s", cvarPrefix, "anim"), animName);
	if (RIGHT(actor)) {
		const invList_t *i = RIGHT(actor);
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon"), i->item.t->model);
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon_item"), i->item.t->id);
	} else {
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon"), "");
		Cvar_Set(va("%s%s", cvarPrefix, "rweapon_item"), "");
	}
	if (LEFT(actor)) {
		const invList_t *i = LEFT(actor);
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon"), i->item.t->model);
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon_item"), i->item.t->id);
	} else {
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon"), "");
		Cvar_Set(va("%s%s", cvarPrefix, "lweapon_item"), "");
	}

	/* print ammo */
	invList = RIGHT(actor);
	if (invList)
		Cvar_SetValue(va("%s%s", cvarPrefix, "ammoright"), invList->item.a);
	else
		Cvar_Set(va("%s%s", cvarPrefix, "ammoright"), "");

	invList = HUD_GetLeftHandWeapon(actor, NULL);
	if (invList)
		Cvar_SetValue(va("%s%s", cvarPrefix, "ammoleft"), invList->item.a);
	else
		Cvar_Set(va("%s%s", cvarPrefix, "ammoleft"), "");
}

/**
 * @brief Update cvars according to a soldier from a list while we are on battlescape
 */
static void HUD_ActorGetCvarData_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <soldiernum> <cvarprefix>\n", Cmd_Argv(0));
		return;
	}

	/* check whether we are connected (tactical mission) */
	if (CL_BattlescapeRunning()) {
		const int num = atoi(Cmd_Argv(1));
		const char *cvarPrefix = Cmd_Argv(2);
		le_t *le;
		character_t *chr;

		/* check if actor exists */
		if (num >= cl.numTeamList || num < 0)
			return;

		/* select actor */
		le = cl.teamList[num];
		if (!le)
			return;

		chr = CL_ActorGetChr(le);
		if (!chr) {
			Com_Error(ERR_DROP, "No character given for local entity");
			return;
		}

		CL_UpdateCharacterValues(chr, cvarPrefix);

		/* override some cvar with HUD data */
		HUD_UpdateActorCvar(le, cvarPrefix);

		return;
	}
}

/**
 * @brief Updates the hud for one actor
 * @param actor The actor to update the hud values for
 */
static void HUD_UpdateActor (le_t *actor)
{
	int time;

	HUD_UpdateActorCvar(actor, "mn_");

	/* write info */
	time = 0;

	/* handle actor in a panic */
	if (LE_IsPaniced(actor)) {
		UI_RegisterText(TEXT_STANDARD, _("Currently panics!\n"));
	} else if (displayRemainingTus[REMAINING_TU_CROUCH]) {
		if (CL_ActorUsableTUs(actor) >= TU_CROUCH)
			time = TU_CROUCH;
	} else if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT]
	 || displayRemainingTus[REMAINING_TU_RELOAD_LEFT]) {
		const invList_t *invList;
		containerIndex_t container;

		if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] && RIGHT(actor)) {
			container = csi.idRight;
			invList = RIGHT(actor);
		} else if (displayRemainingTus[REMAINING_TU_RELOAD_LEFT] && LEFT(actor)) {
			container = NONE;
			invList = HUD_GetLeftHandWeapon(actor, &container);
		} else {
			container = NONE;
			invList = NULL;
		}

		if (invList && invList->item.t && invList->item.m && invList->item.t->reload) {
			const int reloadtime = HUD_CalcReloadTime(actor, invList->item.t, container);
			if (reloadtime != -1 && reloadtime <= CL_ActorUsableTUs(actor))
				time = reloadtime;
		}
	} else if (CL_ActorFireModeActivated(actor->actorMode)) {
		time = HUD_UpdateActorFireMode(actor);
	} else {
		/* If the mouse is outside the world, and we haven't placed the cursor in pend
		 * mode already */
		if (IN_GetMouseSpace() != MS_WORLD && actor->actorMode < M_PEND_MOVE)
			actor->actorMoveLength = ROUTING_NOT_REACHABLE;
		time = HUD_UpdateActorMove(actor);
	}

	/* Calculate remaining TUs. */
	/* We use the full count of TUs since the "reserved" bar is overlaid over this one. */
	time = max(0, actor->TU - time);
	Cvar_Set("mn_turemain", va("%i", time));

	HUD_MapDebugCursor(actor);
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
	if (cls.state != ca_active)
		return;

	/* worldlevel */
	if (cl_worldlevel->modified) {
		int i;
		for (i = 0; i < PATHFINDING_HEIGHT; i++) {
			int status = 0;
			if (i == cl_worldlevel->integer)
				status = 2;
			else if (i < cl.mapMaxLevel)
				status = 1;
			UI_ExecuteConfunc("updateLevelStatus %i %i", i, status);
		}
		cl_worldlevel->modified = qfalse;
	}

	/* set Cvars for all actors */
	HUD_UpdateAllActors();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	if (selActor) {
		HUD_UpdateActor(selActor);
	} else if (!cl.numTeamList) {
		/* This will stop the drawing of the bars over the whole screen when we test maps. */
		Cvar_SetValue("mn_hp", 0);
		Cvar_SetValue("mn_hpmax", 100);
		Cvar_SetValue("mn_tu", 0);
		Cvar_SetValue("mn_tumax", 100);
		Cvar_SetValue("mn_tureserved", 0);
		Cvar_SetValue("mn_morale", 0);
		Cvar_SetValue("mn_moralemax", 100);
		Cvar_SetValue("mn_stun", 0);
	}
}

/**
 * @brief Callback that is called when the cl_selected cvar was changed
 * @param cvarName The cvar name (cl_selected)
 * @param oldValue The old value of the cvar (a sane actor idx)
 * @param newValue The new value of the cvar (a sane actor idx)
 */
static void HUD_ActorSelectionChangeListener (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	if (!CL_OnBattlescape())
		return;

	if (newValue[0] != '\0') {
		const int actorIdx = atoi(newValue);
		const size_t size = lengthof(cl.teamList);
		if (actorIdx >= 0 && actorIdx < size)
			UI_ExecuteConfunc("hudselect %s", newValue);
	}
}

/**
 * @brief Callback that is called when the right hand weapon of the current selected actor changed
 * @param cvarName The cvar name
 * @param oldValue The old value of the cvar
 * @param newValue The new value of the cvar
 */
static void HUD_RightHandChangeListener (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	if (!CL_OnBattlescape())
		return;

	HUD_RefreshButtons(selActor);
}

/**
 * @brief Callback that is called when the left hand weapon of the current selected actor changed
 * @param cvarName The cvar name
 * @param oldValue The old value of the cvar
 * @param newValue The new value of the cvar
 */
static void HUD_LeftHandChangeListener (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	if (!CL_OnBattlescape())
		return;

	HUD_RefreshButtons(selActor);
}

/**
 * @brief Callback that is called when the remaining TUs for the current selected actor changed
 * @param cvarName The cvar name
 * @param oldValue The old value of the cvar
 * @param newValue The new value of the cvar
 */
static void HUD_TUChangeListener (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	if (!CL_OnBattlescape())
		return;

	HUD_RefreshButtons(selActor);
}

static qboolean CL_CvarWorldLevel (cvar_t *cvar)
{
	const int maxLevel = cl.mapMaxLevel ? cl.mapMaxLevel - 1 : PATHFINDING_HEIGHT - 1;
	return Cvar_AssertValue(cvar, 0, maxLevel, qtrue);
}

/**
 * @brief Checks that the given cvar is a valid hud cvar
 * @param cvar The cvar to check
 * @return @c true if cvar is valid, @c false otherwise
 */
static qboolean HUD_CheckCLHud (cvar_t *cvar)
{
	uiNode_t *window = UI_GetWindow(cvar->string);
	if (window == NULL) {
		return qfalse;
	}

	if (window->super == NULL) {
		return qfalse;
	}

	/**
	 * @todo check for multiple base classes
	 */
	return Q_streq(window->super->name, "hud");
}

/**
 * @brief Display the user interface
 * @param optionWindowName Name of the window used to display options, else NULL if nothing
 * @param popAll If true
 * @todo Remove popAll when it is possible. It should always be true
 */
void HUD_InitUI (const char *optionWindowName, qboolean popAll)
{
	if (!HUD_CheckCLHud(cl_hud)) {
		Cvar_Set("cl_hud", "hud_default");
	}
	UI_InitStack(cl_hud->string, optionWindowName, popAll, qtrue);

	UI_ExecuteConfunc("hudinit");
}

/**
 * @brief Checks that the given cvar is a valid hud cvar
 * @param cvar The cvar to check and to modify if the value is invalid
 * @return @c true if the valid is invalid, @c false otherwise
 */
static qboolean HUD_CvarCheckMNHud (cvar_t *cvar)
{
	if (!HUD_CheckCLHud(cl_hud)) {
		Cvar_Reset(cvar);
		return qtrue;
	}
	return qfalse;
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
	Cmd_AddCommand("hud_listactions", HUD_DisplayActions_f, "Display a list of action from the selected soldier.");
	Cmd_AddCommand("hud_getactorcvar", HUD_ActorGetCvarData_f, "Update cvars from actor from list");

	/** @note We can't check the value at startup cause scripts are not yet loaded */
	cl_hud = Cvar_Get("cl_hud", "hud_default", CVAR_ARCHIVE | CVAR_LATCH, "Current selected HUD");
	Cvar_SetCheckFunction("cl_hud", HUD_CvarCheckMNHud);

	cl_worldlevel = Cvar_Get("cl_worldlevel", "0", 0, "Current worldlevel in tactical mode");
	Cvar_SetCheckFunction("cl_worldlevel", CL_CvarWorldLevel);
	cl_worldlevel->modified = qfalse;

	Cvar_Get("mn_ammoleft", "", 0, "The remaining amount of ammunition in for the left hand weapon");
	Cvar_Get("mn_lweapon", "", 0, "The left hand weapon model of the current selected actor - empty if no weapon");
	Cvar_RegisterChangeListener("mn_ammoleft", HUD_LeftHandChangeListener);
	Cvar_RegisterChangeListener("mn_lweapon", HUD_LeftHandChangeListener);

	Cvar_Get("mn_ammoright", "", 0, "The remaining amount of ammunition in for the right hand weapon");
	Cvar_Get("mn_rweapon", "", 0, "The right hand weapon model of the current selected actor - empty if no weapon");
	Cvar_RegisterChangeListener("mn_ammoright", HUD_RightHandChangeListener);
	Cvar_RegisterChangeListener("mn_rweapon", HUD_RightHandChangeListener);

	Cvar_Get("mn_turemain", "", 0, "Remaining TUs for the current selected actor");
	Cvar_RegisterChangeListener("mn_turemain", HUD_TUChangeListener);

	Cvar_RegisterChangeListener("cl_selected", HUD_ActorSelectionChangeListener);

	cl_hud_message_timeout = Cvar_Get("cl_hud_message_timeout", "2000", CVAR_ARCHIVE, "Timeout for HUD messages (milliseconds)");
	cl_show_cursor_tooltips = Cvar_Get("cl_show_cursor_tooltips", "1", CVAR_ARCHIVE, "Show cursor tooltips in tactical game mode");
}
