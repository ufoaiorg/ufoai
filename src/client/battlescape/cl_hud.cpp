/**
 * @file
 * @brief HUD related routines.
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

static cvar_t* cl_hud_message_timeout;
static cvar_t* cl_show_cursor_tooltips;
static cvar_t* cl_hud;
cvar_t* cl_worldlevel;

enum {
	REMAINING_TU_RELOAD_RIGHT,
	REMAINING_TU_RELOAD_LEFT,
	REMAINING_TU_CROUCH,

	REMAINING_TU_MAX
};
static bool displayRemainingTus[REMAINING_TU_MAX];

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

typedef enum {
	FIRE_RIGHT,
	FIRE_LEFT,
	RELOAD_RIGHT,
	RELOAD_LEFT
} actionType_t;

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
	BT_STATE_UNINITIALZED,
	BT_STATE_DISABLE,		/**< 'Disabled' display (grey) */
	BT_STATE_DESELECT,		/**< Normal display (blue) */
	BT_STATE_SELECT			/**< Selected display (blue) */
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

typedef enum {
	BUTTON_TURESERVE_UNINITIALIZED,

	BUTTON_TURESERVE_SHOT_RESERVED,
	BUTTON_TURESERVE_SHOT_DISABLED,
	BUTTON_TURESERVE_SHOT_CLEAR,

	BUTTON_TURESERVE_CROUCH_RESERVED,
	BUTTON_TURESERVE_CROUCH_DISABLED,
	BUTTON_TURESERVE_CROUCH_CLEAR
} reserveButtonState_t;

static weaponButtonState_t buttonStates[BT_NUM_TYPES];
static reserveButtonState_t shotReserveButtonState;
static reserveButtonState_t crouchReserveButtonState;

/**
 * @brief Displays a message on the hud.
 * @sa UI_DisplayNotice
 * @param[in] text text is already translated here
 */
void HUD_DisplayMessage (const char* text)
{
	assert(text);
	UI_DisplayNotice(text, cl_hud_message_timeout->integer, cl_hud->string);
}

void HUD_UpdateActorStats (const le_t* le)
{
	if (LE_IsDead(le))
		return;

	const Item* item = le->getRightHandItem();
	if ((!item || !item->def() || !item->isHeldTwoHanded()) && le->getLeftHandItem())
		item = le->getLeftHandItem();

	const character_t* chr = CL_ActorGetChr(le);
	assert(chr);
	const char* tooltip = va(_("%s\nHP: %i/%i TU: %i\n%s"),
		chr->name, le->HP, le->maxHP, le->TU, (item && item->def()) ? _(item->def()->name) : "");

	const int idx = CL_ActorGetNumber(le);
	UI_ExecuteConfunc("updateactorvalues %i \"%s\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%i\" \"%s\"",
			idx, le->model2->name, le->HP, le->maxHP, le->TU, le->maxTU, le->morale, le->maxMorale, le->STUN, tooltip);
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @todo This should be a confunc which also sets the tooltips
 */
static void HUD_SetWeaponButton (buttonTypes_t button, weaponButtonState_t state)
{
	if (buttonStates[button] == state)
		return;

	const char* prefix;

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
	buttonStates[button] = state;
}

/**
 * @brief Returns the amount of usable "reaction fire" TUs for this actor (depends on active/inactive RF)
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 * @todo Maybe only return "reaction" value if reaction-state is active? The value _should_ be 0, but one never knows :)
 */
static int HUD_UsableReactionTUs (const le_t* le)
{
	/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) */
	if (le->state & STATE_REACTION)
		/* CL_ActorUsableTUs DOES NOT return the stored value for "reaction" here. */
		return CL_ActorUsableTUs(le) + CL_ActorReservedTUs(le, RES_REACTION);

	/* CL_ActorUsableTUs DOES return the stored value for "reaction" here. */
	return CL_ActorUsableTUs(le);
}

/**
 * @brief Check if at least one firemode is available for reservation.
 * @return true if there is at least one firemode - false otherwise.
 * @sa HUD_UpdateButtons
 * @sa HUD_PopupFiremodeReservation_f
 */
static bool HUD_CheckFiremodeReservation (void)
{
	if (!selActor)
		return false;

	actorHands_t hand;
	foreachhand(hand) {
		/* Get weapon (and its ammo) from the hand. */
		const fireDef_t* fireDef = HUD_GetFireDefinitionForHand(selActor, hand);
		if (!fireDef)
			continue;

		const objDef_t* ammo = fireDef->obj;
		for (int i = 0; i < ammo->numFiredefs[fireDef->weapFdsIdx]; i++) {
			/* Check if at least one firemode is available for reservation. */
			if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= CL_ActorTimeForFireDef(selActor, &ammo->fd[fireDef->weapFdsIdx][i]))
				return true;
		}
	}

	/* No reservation possible */
	return false;
}


/**
 * @brief Sets TU-reservation and firemode
 * @param[in] le The local entity of the actor to change the tu reservation for.
 * @param[in] tus How many TUs to set.
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weapon Pointer to weapon in the hand.
 */
static void HUD_SetShootReservation (const le_t* le, const int tus, const actorHands_t hand, const int fireModeIndex, const objDef_t* weapon)
{
	character_t* chr = CL_ActorGetChr(le);
	assert(chr);

	CL_ActorReserveTUs(le, RES_SHOT, tus);
	chr->reservedTus.shotSettings.set(hand, fireModeIndex, weapon);
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
static void HUD_PopupFiremodeReservation (const le_t* le, bool popupReload)
{
	/* reset the list */
	UI_ResetData(TEXT_LIST);

	LIST_Delete(&popupListData);

	/* Add list-entry for deactivation of the reservation. */
	linkedList_t* popupListText = nullptr;
	LIST_AddPointer(&popupListText, (void*)(_("[0 TU] No reservation")));
	reserveShot_t reserveShotData = {ACTOR_HAND_NOT_SET, NONE, NONE, NONE};
	LIST_Add(&popupListData, reserveShotData);
	int selectedEntry = 0;

	actorHands_t hand;
	foreachhand(hand) {
		const fireDef_t* fd = HUD_GetFireDefinitionForHand(le, hand);
		if (!fd)
			continue;
		character_t* chr = CL_ActorGetChr(le);
		assert(chr);

		const objDef_t* ammo = fd->obj;
		for (int i = 0; i < ammo->numFiredefs[fd->weapFdsIdx]; i++) {
			const fireDef_t* ammoFD = &ammo->fd[fd->weapFdsIdx][i];
			const int time = CL_ActorTimeForFireDef(le, ammoFD);
			if (CL_ActorUsableTUs(le) + CL_ActorReservedTUs(le, RES_SHOT) >= time) {
				static char text[MAX_VAR];
				/* Get firemode name and TUs. */
				Com_sprintf(text, lengthof(text), _("[%i TU] %s"), time, _(ammoFD->name));

				/* Store text for popup */
				LIST_AddString(&popupListText, text);

				/* Store Data for popup-callback. */
				reserveShotData.hand = hand;
				reserveShotData.fireModeIndex = i;
				reserveShotData.weaponIndex = ammo->weapons[fd->weapFdsIdx]->idx;
				reserveShotData.TUs = time;
				LIST_Add(&popupListData, reserveShotData);

				/* Remember the line that is currently selected (if any). */
				if (chr->reservedTus.shotSettings.getHand() == hand
				 && chr->reservedTus.shotSettings.getFmIdx() == i
				 && chr->reservedTus.shotSettings.getWeapon() == ammo->weapons[fd->weapFdsIdx])
					selectedEntry = LIST_Count(popupListData) - 1;
			}
		}
	}

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
		HUD_SetShootReservation(selActor, 0, ACTOR_HAND_NOT_SET, -1, nullptr);
	} else {
		HUD_PopupFiremodeReservation(selActor, false);
	}
}

/**
 * @brief Get selected firemode in the list of the currently selected actor.
 * @sa HUD_PopupFiremodeReservation_f
 */
static void HUD_ShotReserve_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <popupindex>\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	/* read and range check */
	const int selectedPopupIndex = atoi(Cmd_Argv(1));
	if (selectedPopupIndex < 0 || selectedPopupIndex >= LIST_Count(popupListData))
		return;

	const reserveShot_t* reserveShotData = (const reserveShot_t*)LIST_GetByIdx(popupListData, selectedPopupIndex);
	if (!reserveShotData)
		return;

	if (reserveShotData->weaponIndex == NONE) {
		HUD_SetShootReservation(selActor, 0, ACTOR_HAND_NOT_SET, -1, nullptr);
		return;
	}

	/** @todo do this on the server */
	/* Check if we have enough TUs (again) */
	if (CL_ActorUsableTUs(selActor) + CL_ActorReservedTUs(selActor, RES_SHOT) >= reserveShotData->TUs) {
		const objDef_t* od = INVSH_GetItemByIDX(reserveShotData->weaponIndex);
		if (GAME_ItemIsUseable(od)) {
			HUD_SetShootReservation(selActor, std::max(0, reserveShotData->TUs), reserveShotData->hand, reserveShotData->fireModeIndex, od);
			if (popupListNode)
				UI_TextNodeSelectLine(popupListNode, selectedPopupIndex);
		}
	}
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param callback confunc callback
 * @param actor The actor local entity
 * @param ammo The ammo used by this firemode
 * @param weapFdsIdx the weapon index in the fire definition array
 * @param hand What list to display
 * @param index Index of the actual firemode
 */
static void HUD_DisplayFiremodeEntry (const char* callback, const le_t* actor, const objDef_t* ammo, const weaponFireDefIndex_t weapFdsIdx, const actorHands_t hand, const int index)
{
	if (index >= ammo->numFiredefs[weapFdsIdx])
		return;

	assert(actor);
	assert(hand == ACTOR_HAND_RIGHT || hand == ACTOR_HAND_LEFT);

	/* We have a defined fd ... */
	const fireDef_t* fd = &ammo->fd[weapFdsIdx][index];
	const int time = CL_ActorTimeForFireDef(actor, fd);
	const int usableTusForRF = HUD_UsableReactionTUs(actor);

	char tuString[MAX_VAR];
	const char* tooltip;
	if (usableTusForRF > time) {
		Com_sprintf(tuString, sizeof(tuString), _("Remaining TUs: %i"), usableTusForRF - time);
		tooltip = tuString;
	} else
		tooltip = _("No remaining TUs left after shot.");

	/* unique identifier of the action */
	/* @todo use this id as action callback instead of hand and index (we can extend it with any other soldier action we need (open door, reload...)) */
	char id[32];
	Com_sprintf(id, sizeof(id), "fire_hand%c_i%i", ACTOR_GET_HAND_CHAR(hand), index);

	const bool status = time <= CL_ActorUsableTUs(actor);
	UI_ExecuteConfunc("%s firemode %s %c %i %i %i \"%s\" \"%i\" \"%i\" \"%s\"", callback, id, ACTOR_GET_HAND_CHAR(hand),
			fd->fdIdx, fd->reaction, status, _(fd->name), time, fd->ammo, tooltip);

	/* Display checkbox for reaction firemode */
	if (fd->reaction) {
		character_t* chr = CL_ActorGetChr(actor);
		const bool active = THIS_FIREMODE(&chr->RFmode, hand, fd->fdIdx);
		/* Change the state of the checkbox. */
		UI_ExecuteConfunc("%s reaction %s %c %i", callback, id, ACTOR_GET_HAND_CHAR(hand), active);
	}
}

/**
 * @brief List actions from a soldier to a callback confunc
 * @param callback confunc callback
 * @param actor actor who can do the actions
 * @param type The action to display
 */
static void HUD_DisplayActions (const char* callback, const le_t* actor, actionType_t type)
{
	if (!actor)
		return;

	if (!cls.isOurRound()) {	/**< Not our turn */
		return;
	}

	const ScopedCommand c(callback, "begin", "end");

	switch (type) {
	case FIRE_RIGHT: {
		const actorHands_t hand = ACTOR_HAND_RIGHT;
		const fireDef_t* fd = HUD_GetFireDefinitionForHand(actor, hand);
		if (fd == nullptr) {
			UI_PopWindow();
			return;
		}

		const objDef_t* ammo = fd->obj;
		if (!ammo) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes: no weapon or ammo found.\n");
			return;
		}

		for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
			/* Display the firemode information (image + text). */
			HUD_DisplayFiremodeEntry(callback, actor, ammo, fd->weapFdsIdx, hand, i);
		}
	}
	break;

	case RELOAD_RIGHT: {
		Item* weapon = actor->getRightHandItem();

		/* Reloeadable item in hand. */
		if (weapon && weapon->def() && weapon->isReloadable()) {
			containerIndex_t container = CID_RIGHT;
			const char* actionId = "reload_handr";
			const int tus = HUD_CalcReloadTime(actor, weapon->def(), container);
			const bool noAmmo = tus == -1;
			const bool noTU = actor->TU < tus;
			UI_ExecuteConfunc("%s reload %s %c %i %i %i", callback, actionId, 'r', tus, !noAmmo, !noTU);
		}
	}
	break;

	case FIRE_LEFT: {
		const actorHands_t hand = ACTOR_HAND_LEFT;
		const fireDef_t* fd = HUD_GetFireDefinitionForHand(actor, hand);
		if (fd == nullptr) {
			UI_PopWindow();
			return;
		}

		const objDef_t* ammo = fd->obj;
		if (!ammo) {
			Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes: no weapon or ammo found.\n");
			return;
		}

		for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
			/* Display the firemode information (image + text). */
			HUD_DisplayFiremodeEntry(callback, actor, ammo, fd->weapFdsIdx, hand, i);
		}
	}
	break;

	case RELOAD_LEFT: {
		Item* weapon = actor->getLeftHandItem();

		/* Reloadable item in hand. */
		if (weapon && weapon->def() && weapon->isReloadable()) {
			containerIndex_t container = CID_LEFT;
			const char* actionId = "reload_handl";
			const int tus = HUD_CalcReloadTime(actor, weapon->def(), container);
			const bool noAmmo = tus == -1;
			const bool noTU = actor->TU < tus;
			UI_ExecuteConfunc("%s reload %s %c %i %i %i", callback, actionId, 'l', tus, !noAmmo, !noTU);
		}
	}
	break;
	}
}

/**
 * @brief Displays the firemodes for the given hand.
 * @todo we can extend it with short cut equip action, more reload, action on the map (like open doors)...
 */
static void HUD_DisplayActions_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s callback [l|r|L|R]\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	actionType_t type;
	if (strchr(Cmd_Argv(2), 'r') != nullptr) {
		type = FIRE_RIGHT;
	} else if (strchr(Cmd_Argv(2), 'l') != nullptr) {
		type = FIRE_LEFT;
	} else if (strchr(Cmd_Argv(2), 'R') != nullptr) {
		type = RELOAD_RIGHT;
	} else if (strchr(Cmd_Argv(2), 'L') != nullptr) {
		type = RELOAD_LEFT;
	} else {
		return;
	}

	char callback[32];
	Q_strncpyz(callback, Cmd_Argv(1), sizeof(callback));
	HUD_DisplayActions(callback, selActor, type);
}

/**
 * @brief Displays the firemodes for the given hand.
 */
static void HUD_DisplayFiremodes_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s callback [l|r]\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	if (selActor->isMoving())
		return;

	const actorHands_t hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(2)[0]);
	const actionType_t type = hand == ACTOR_HAND_RIGHT ? FIRE_RIGHT : FIRE_LEFT;
	char callback[32];
	Q_strncpyz(callback, Cmd_Argv(1), sizeof(callback));
	HUD_DisplayActions(callback, selActor, type);
}

/**
 * @brief Updates the information in RFmode for the selected actor with the given data from the parameters.
 * @param[in] actor The actor we want to update the reaction-fire firemode for.
 * @param[in] hand Which weapon(-hand) to use.
 * @param[in] firemodeActive Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
static void HUD_UpdateReactionFiremodes (const le_t* actor, const actorHands_t hand, fireDefIndex_t firemodeActive)
{
	assert(actor);

	const fireDef_t* fd = HUD_GetFireDefinitionForHand(actor, hand);
	if (fd == nullptr)
		return;

	const objDef_t* ammo = fd->obj;
	const objDef_t* od = ammo->weapons[fd->weapFdsIdx];

	if (!GAME_ItemIsUseable(od))
		return;

	MSG_Write_PA(PA_REACT_SELECT, actor->entnum, hand, firemodeActive, od ? od->idx : NONE);
}

/**
 * @brief Checks if the selected firemode checkbox is ok as a reaction firemode and updates data+display.
 */
static void HUD_SelectReactionFiremode_f (void)
{
	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s [l|r] <num>   num=firemode number\n", Cmd_Argv(0));
		return;
	}

	if (!selActor)
		return;

	const fireDefIndex_t firemode = atoi(Cmd_Argv(2));
	if (firemode >= MAX_FIREDEFS_PER_WEAPON || firemode < 0) {
		Com_Printf("HUD_SelectReactionFiremode_f: Firemode out of bounds (%i).\n", firemode);
		return;
	}

	const actorHands_t hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]);
	HUD_UpdateReactionFiremodes(selActor, hand, firemode);
}

/**
 * @brief Remember if we hover over a button that would cost some TUs when pressed.
 * @note this is used in HUD_Update to update the "remaining TUs" bar correctly.
 */
static void HUD_RemainingTUs_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <type> <popupindex>\n", Cmd_Argv(0));
		return;
	}

	const char* type = Cmd_Argv(1);
	const bool state = Com_ParseBoolean(Cmd_Argv(2));

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
 * @return The minimum time needed to fire the weapon
 */
static int HUD_GetMinimumTUsForUsage (const Item* item)
{
	assert(item->def());

	const fireDef_t* fd = item->getFastestFireDef();

	return fd ? fd->time : 900;
}

/**
 * @brief Checks every case for reload buttons on the HUD.
 * @param[in] le Pointer of local entity being an actor.
 * @param[in] containerID of the container to reload the weapon in. Used to get the movement TUs for moving something into the container.
 * @param[out] reason The reason why the reload didn't work - only set if @c -1 is the return value
 * @return TU units needed for reloading or -1 if weapon cannot be reloaded.
 */
static int HUD_WeaponCanBeReloaded (const le_t* le, containerIndex_t containerID, const char** reason)
{
	const Item* invList = le->inv.getContainer2(containerID);

	/* No weapon in hand. */
	if (!invList) {
		*reason = _("No weapon.");
		return -1;
	}

	const objDef_t* weapon = invList->def();
	assert(weapon);

	/* This weapon cannot be reloaded. */
	if (!weapon->isReloadable()) {
		*reason = _("Weapon cannot be reloaded.");
		return -1;
	}

	/* Weapon is fully loaded. */
	if (invList->ammoDef() && weapon->ammo == invList->getAmmoLeft()) {
		*reason = _("No reload possible, already fully loaded.");
		return -1;
	}

	/* Weapon is empty or not fully loaded, find ammo of any type loadable to this weapon. */
	if (!invList->ammoDef() || weapon->ammo > invList->getAmmoLeft()) {
		const int tuCosts = HUD_CalcReloadTime(le, weapon, containerID);
		if (tuCosts >= 0) {
			const int tu = CL_ActorUsableTUs(le);
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
 * @brief Display 'impossible' (red) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 * @return true if nothing changed message was sent otherwise false.
 * @note this is called when reaction is enabled with no suitable weapon.
 * @todo at the moment of writing, this should be impossible: reaction
 * should be disabled whenever the actor loses its reaction weapon.
 */
static bool HUD_DisplayImpossibleReaction (const le_t* actor)
{
	if (!actor)
		return false;

	/* Given actor does not equal the currently selected actor. */
	if (!LE_IsSelected(actor))
		return false;

	if (buttonStates[BT_REACTION] == BT_STATE_UNINITIALZED)
		return false;

	/* Display 'impossible" (red) reaction buttons */
	if (actor->state & STATE_REACTION) {
		Com_Printf("HUD_DisplayImpossibleReaction: Warning! Reaction fire enabled but no suitable weapon found!\n");
		UI_ExecuteConfunc("startreaction_impos");
		buttonStates[BT_REACTION] = BT_STATE_UNINITIALZED;
		return false;
	}

	return true;
}

/**
 * @brief Display 'usable' (blue) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 */
static void HUD_DisplayPossibleReaction (const le_t* actor)
{
	if (!actor)
		return;
	/* Given actor does not equal the currently selected actor. This normally only happens on game-start. */
	if (!LE_IsSelected(actor))
		return;

	if (buttonStates[BT_REACTION] == BT_STATE_SELECT)
		return;

	/* Display 'usable" (blue) reaction buttons */
	if (actor->state & STATE_REACTION) {
		UI_ExecuteConfunc("startreaction");
		buttonStates[BT_REACTION] = BT_STATE_SELECT;
	}
}

/**
 * @brief Get the weapon firing TUs for reaction fire.
 * @param[in] actor the actor to check for his reaction TUs.
 * @return The TUs needed for the reaction fireDef for this actor or -1 if no valid reaction settings
 */
int HUD_ReactionFireGetTUs (const le_t* actor)
{
	if (!actor)
		return -1;

	const FiremodeSettings& fmSetting = CL_ActorGetChr(actor)->RFmode;
	const Item* weapon = actor->getHandItem(fmSetting.getHand());

	if (!weapon)
		weapon = actor->getRightHandItem();
	if (!weapon)
		weapon = actor->getLeftHandItem();

	if (weapon && weapon->ammoDef() && weapon->isWeapon()) {
		const fireDef_t* fdArray = weapon->getFiredefs();
		if (fdArray == nullptr)
			return -1;

		const fireDefIndex_t fmIdx = fmSetting.getFmIdx();
		if (fmIdx >= 0 && fmIdx < MAX_FIREDEFS_PER_WEAPON) {
			return CL_ActorTimeForFireDef(actor, &fdArray[fmIdx], true);
		}
	}

	return -1;
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] le Pointer to local entity for which we refresh HUD buttons.
 */
static void HUD_UpdateButtons (const le_t* le)
{
	if (!le)
		return;

	const int time = CL_ActorUsableTUs(le);
	/* Crouch/stand button. */
	if (LE_IsCrouched(le)) {
		buttonStates[BT_STAND] =  BT_STATE_UNINITIALZED;
		if (time + CL_ActorReservedTUs(le, RES_CROUCH) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for standing up."));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", _("Stand up (%i TU)"), TU_CROUCH);
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DESELECT);
		}
	} else {
		buttonStates[BT_CROUCH] =  BT_STATE_UNINITIALZED;
		if (time + CL_ActorReservedTUs(le, RES_CROUCH) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for crouching."));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", _("Crouch (%i TU)"), TU_CROUCH);
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DESELECT);
		}
	}

	/* Crouch/stand reservation checkbox. */
	if (CL_ActorReservedTUs(le, RES_CROUCH) >= TU_CROUCH) {
		if (crouchReserveButtonState != BUTTON_TURESERVE_CROUCH_RESERVED) {
			UI_ExecuteConfunc("crouch_checkbox_check");
			Cvar_Set("mn_crouch_reservation_tt", _("%i TUs reserved for crouching/standing up.\nClick to clear."),
					CL_ActorReservedTUs(le, RES_CROUCH));
			crouchReserveButtonState = BUTTON_TURESERVE_CROUCH_RESERVED;
		}
	} else if (time >= TU_CROUCH) {
		if (crouchReserveButtonState != BUTTON_TURESERVE_CROUCH_CLEAR) {
			UI_ExecuteConfunc("crouch_checkbox_clear");
			Cvar_Set("mn_crouch_reservation_tt", _("Reserve %i TUs for crouching/standing up."), TU_CROUCH);
			crouchReserveButtonState = BUTTON_TURESERVE_CROUCH_CLEAR;
		}
	} else {
		if (crouchReserveButtonState != BUTTON_TURESERVE_CROUCH_DISABLED) {
			UI_ExecuteConfunc("crouch_checkbox_disable");
			Cvar_Set("mn_crouch_reservation_tt", _("Not enough TUs left to reserve for crouching/standing up."));
			crouchReserveButtonState = BUTTON_TURESERVE_CROUCH_DISABLED;
		}
	}

	/* Shot reservation button. mn_shot_reservation_tt is the tooltip text */
	if (CL_ActorReservedTUs(le, RES_SHOT)) {
		if (shotReserveButtonState != BUTTON_TURESERVE_SHOT_RESERVED) {
			UI_ExecuteConfunc("reserve_shot_check");
			Cvar_Set("mn_shot_reservation_tt", _("%i TUs reserved for shooting.\nClick to change.\nRight-Click to clear."),
					CL_ActorReservedTUs(le, RES_SHOT));
			shotReserveButtonState = BUTTON_TURESERVE_SHOT_RESERVED;
		}
	} else if (HUD_CheckFiremodeReservation()) {
		if (shotReserveButtonState != BUTTON_TURESERVE_SHOT_CLEAR) {
			UI_ExecuteConfunc("reserve_shot_clear");
			Cvar_Set("mn_shot_reservation_tt", _("Reserve TUs for shooting."));
			shotReserveButtonState = BUTTON_TURESERVE_SHOT_CLEAR;
		}
	} else {
		if (shotReserveButtonState != BUTTON_TURESERVE_SHOT_DISABLED) {
			UI_ExecuteConfunc("reserve_shot_disable");
			Cvar_Set("mn_shot_reservation_tt", _("Reserving TUs for shooting not possible."));
			shotReserveButtonState = BUTTON_TURESERVE_SHOT_DISABLED;
		}
	}

	/* reaction-fire button */
	if (!(le->state & STATE_REACTION)) {
		if (le->inv.holdsReactionFireWeapon() && time >= HUD_ReactionFireGetTUs(le))
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DESELECT);
		else
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DISABLE);
	} else {
		if (le->inv.holdsReactionFireWeapon()) {
			HUD_DisplayPossibleReaction(le);
		} else {
			HUD_DisplayImpossibleReaction(le);
		}
	}

	const char* reason;

	/* Reload buttons */
	const int rightCanBeReloaded = HUD_WeaponCanBeReloaded(le, CID_RIGHT, &reason);
	if (rightCanBeReloaded != -1) {
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadright_tt", _("Reload weapon (%i TU)."), rightCanBeReloaded);
	} else {
		Cvar_Set("mn_reloadright_tt", "%s", reason);
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DISABLE);
	}

	const int leftCanBeReloaded = HUD_WeaponCanBeReloaded(le, CID_LEFT, &reason);
	if (leftCanBeReloaded != -1) {
		HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadleft_tt", _("Reload weapon (%i TU)."), leftCanBeReloaded);
	} else {
		Cvar_Set("mn_reloadleft_tt", "%s", reason);
		HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DISABLE);
	}

	const float shootingPenalty = CL_ActorInjuryModifier(le, MODIFIER_SHOOTING);
	/* Headgear button */
	const Item* headgear = le->inv.getHeadgear();
	if (headgear) {
		const int minheadgeartime = HUD_GetMinimumTUsForUsage(headgear) * shootingPenalty;
		if (time < minheadgeartime)
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
	}

	/* Weapon firing buttons. */
	const Item* weaponR = le->getRightHandItem();
	if (weaponR) {
		const int minweaponrtime = HUD_GetMinimumTUsForUsage(weaponR) * shootingPenalty;
		if (time < minweaponrtime)
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
		else
			HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DESELECT);
	} else {
		HUD_SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
	}

	const Item* weaponL = nullptr;
	/* check for two-handed weapon - if not, also define weaponL */
	if (!weaponR || !weaponR->isHeldTwoHanded())
		weaponL = le->getLeftHandItem();
	if (weaponL) {
		const int minweaponltime = HUD_GetMinimumTUsForUsage(weaponL) * shootingPenalty;
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
			HUD_PopupFiremodeReservation(le, true);
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
	const char* string = UI_GetText(textId);
	if (string && cl_show_cursor_tooltips->integer)
		UI_DrawTooltip(string, mousePosX + xOffset, mousePosY - yOffset, viddef.virtualWidth - mousePosX);
}

/**
 * @brief Updates the cursor texts when in battlescape
 */
void HUD_UpdateCursor (void)
{
	/* Offset of the first icon on the x-axis. */
	const int iconOffsetX = 16;
	le_t* le = selActor;
	if (le) {
		/* Offset of the first icon on the y-axis. */
		int iconOffsetY = 16;
		/* the space between different icons. */
		const int iconSpacing = 2;
		image_t* image;
		/* icon width */
		const int iconW = 16;
		/* icon height. */
		const int iconH = 16;
		int width = 0;
		int bgX = mousePosX + iconOffsetX / 2 - 2;

		/* checks if icons should be drawn */
		if (!(LE_IsCrouched(le) || (le->state & STATE_REACTION)))
			/* make place holder for icons */
			bgX += iconW + 4;

		/* if exists gets width of player name */
		if (UI_GetText(TEXT_MOUSECURSOR_PLAYERNAMES))
			R_FontTextSize("f_verysmall", UI_GetText(TEXT_MOUSECURSOR_PLAYERNAMES), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, nullptr, nullptr, nullptr);

		/* gets width of background */
		if (width == 0 && UI_GetText(TEXT_MOUSECURSOR_RIGHT)) {
			R_FontTextSize("f_verysmall", UI_GetText(TEXT_MOUSECURSOR_RIGHT), viddef.virtualWidth - bgX, LONGLINES_WRAP, &width, nullptr, nullptr, nullptr);
		}

		/* Display 'crouch' icon if actor is crouched. */
		if (LE_IsCrouched(le)) {
			image = R_FindImage("pics/cursors/ducked", it_pic);
			if (image)
				R_DrawImage(mousePosX - image->width / 2 + iconOffsetX, mousePosY - image->height / 2 + iconOffsetY, image);
		}

		/* Height of 'crouched' icon. */
		iconOffsetY += iconH;
		iconOffsetY += iconSpacing;

		/* Display 'Reaction shot' icon if actor has it activated. */
		if (le->state & STATE_REACTION)
			image = R_FindImage("pics/cursors/reactionfire", it_pic);
		else
			image = nullptr;

		if (image)
			R_DrawImage(mousePosX - image->width / 2 + iconOffsetX, mousePosY - image->height / 2 + iconOffsetY, image);

		/* Height of 'reaction fire' icon. ... in case we add further icons below.
		iconOffsetY += iconH;
		iconOffsetY += iconSpacing;
		*/

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
static void HUD_MapDebugCursor (const le_t* le)
{
	if (!(cl_map_debug->integer & MAPDEBUG_TEXT))
		return;

	/* Display the floor and ceiling values for the current cell. */
	static char topText[UI_MAX_SMALLTEXTLEN];
	Com_sprintf(topText, lengthof(topText), "%u-(%i,%i,%i)\n",
			Grid_Ceiling(cl.mapData->routing, ACTOR_GET_FIELDSIZE(le), truePos), truePos[0], truePos[1], truePos[2]);
	/* Save the text for later display next to the cursor. */
	UI_RegisterText(TEXT_MOUSECURSOR_TOP, topText);

	/* Display the floor and ceiling values for the current cell. */
	static char bottomText[UI_MAX_SMALLTEXTLEN];
	Com_sprintf(bottomText, lengthof(bottomText), "%i-(%i,%i,%i)\n",
			Grid_Floor(cl.mapData->routing, ACTOR_GET_FIELDSIZE(le), truePos), mousePos[0], mousePos[1], mousePos[2]);
	/* Save the text for later display next to the cursor. */
	UI_RegisterText(TEXT_MOUSECURSOR_BOTTOM, bottomText);

	/* Display the floor and ceiling values for the current cell. */
	const int dvec = Grid_MoveNext(&cl.pathMap, mousePos, 0);
	static char leftText[UI_MAX_SMALLTEXTLEN];
	Com_sprintf(leftText, lengthof(leftText), "%i-%i\n", getDVdir(dvec), getDVz(dvec));
	/* Save the text for later display next to the cursor. */
	UI_RegisterText(TEXT_MOUSECURSOR_LEFT, leftText);
}

/**
 * @param actor The actor to update the hud for
 * @return The amount of TUs needed for the current pending action
 */
static int HUD_UpdateActorFireMode (le_t* actor)
{
	const Item* selWeapon;

	/* get weapon */
	if (IS_MODE_FIRE_HEADGEAR(actor->actorMode)) {
		selWeapon = actor->inv.getHeadgear();
	} else if (IS_MODE_FIRE_LEFT(actor->actorMode)) {
		selWeapon = HUD_GetLeftHandWeapon(actor, nullptr);
	} else {
		selWeapon = actor->getRightHandItem();
	}

	UI_ResetData(TEXT_MOUSECURSOR_RIGHT);

	if (!selWeapon) {
		CL_ActorSetMode(actor, M_MOVE);
		return 0;
	}

	static char infoText[UI_MAX_SMALLTEXTLEN];
	int time = 0;
	const objDef_t* def = selWeapon->def();
	if (!def) {
		/* No valid weapon in the hand. */
		CL_ActorSetFireDef(actor, nullptr);
	} else {
		/* Check whether this item uses/has ammo. */
		if (!selWeapon->ammoDef()) {
			const fireDef_t* old = nullptr;
			/* This item does not use ammo, check for existing firedefs in this item. */
			/* This is supposed to be a weapon or other usable item. */
			if (def->numWeapons > 0) {
				if (selWeapon->isWeapon() || def->weapons[0] == def) {
					const fireDef_t* fdArray = selWeapon->getFiredefs();
					if (fdArray != nullptr) {
						/* Get firedef from the weapon (or other usable item) entry instead. */
						old = FIRESH_GetFiredef(def, fdArray->weapFdsIdx, actor->currentSelectedFiremode);
					}
				}
			}
			CL_ActorSetFireDef(actor, old);
		} else {
			const fireDef_t* fdArray = selWeapon->getFiredefs();
			if (fdArray != nullptr) {
				const fireDef_t* old = FIRESH_GetFiredef(selWeapon->ammoDef(), fdArray->weapFdsIdx, actor->currentSelectedFiremode);
				/* reset the align if we switched the firemode */
				CL_ActorSetFireDef(actor, old);
			}
		}
	}

	if (!GAME_ItemIsUseable(def)) {
		HUD_DisplayMessage(_("You cannot use this unknown item.\nYou need to research it first."));
		CL_ActorSetMode(actor, M_MOVE);
	} else if (actor->fd) {
		const int hitProbability = CL_GetHitProbability(actor);
		static char mouseText[UI_MAX_SMALLTEXTLEN];

		Com_sprintf(infoText, lengthof(infoText),
					"%s\n%s (%i) [%i%%] %i\n", _(def->name), _(actor->fd->name),
					actor->fd->ammo, hitProbability, CL_ActorTimeForFireDef(actor, actor->fd));

		/* Save the text for later display next to the cursor. */
		Q_strncpyz(mouseText, infoText, lengthof(mouseText));
		UI_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);

		time = CL_ActorTimeForFireDef(actor, actor->fd);
		/* if no TUs left for this firing action
		 * or if the weapon is reloadable and out of ammo,
		 * then change to move mode */
		if (selWeapon->mustReload() || CL_ActorUsableTUs(actor) < time)
			CL_ActorSetMode(actor, M_MOVE);
	} else if (selWeapon) {
		Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(def->name));
	}

	UI_RegisterText(TEXT_STANDARD, infoText);
	return time;
}

/**
 * @param[in] actor The actor to update the hud for
 * @return The amount of TUs needed for the current pending action
 */
static int HUD_UpdateActorMove (const le_t* actor)
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
		const int moveMode = CL_ActorMoveMode(actor);
		if (reservedTUs > 0)
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i | Reserved TUs: %i\n%s %i (%i|%i TUs left)\n"),
				actor->morale, reservedTUs, _(moveModeDescriptions[moveMode]), actor->actorMoveLength,
				actor->TU - actor->actorMoveLength, actor->TU - reservedTUs - actor->actorMoveLength);
		else
			Com_sprintf(infoText, lengthof(infoText), _("Morale  %i\n%s %i (%i TUs left)\n"), actor->morale,
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

static void HUD_UpdateActorCvar (const le_t* actor)
{
	Cvar_SetValue("mn_hp", actor->HP);
	Cvar_SetValue("mn_hpmax", actor->maxHP);
	Cvar_SetValue("mn_tu", actor->TU);
	Cvar_SetValue("mn_tumax", actor->maxTU);
	Cvar_SetValue("mn_tureserved", CL_ActorReservedTUs(actor, RES_ALL_ACTIVE));
	Cvar_SetValue("mn_morale", actor->morale);
	Cvar_SetValue("mn_moralemax", actor->maxMorale);
	Cvar_SetValue("mn_stun", actor->STUN);

	Cvar_Set("mn_tu_tooltips", _("Time Units\n- Available: %i (of %i)\n- Reserved:  %i\n- Remaining: %i\n"),
			actor->TU, actor->maxTU, CL_ActorReservedTUs(actor, RES_ALL_ACTIVE), CL_ActorUsableTUs(actor));

	/* animation and weapons */
	const char* animName = R_AnimGetName(&actor->as, actor->model1);
	if (animName)
		Cvar_Set("mn_anim", "%s", animName);
	if (actor->getRightHandItem()) {
		const Item* item = actor->getRightHandItem();
		Cvar_Set("mn_rweapon", "%s", item->def()->model);
		Cvar_Set("mn_rweapon_item", "%s", item->def()->id);
	} else {
		Cvar_Set("mn_rweapon", "");
		Cvar_Set("mn_rweapon_item", "");
	}
	if (actor->getLeftHandItem()) {
		const Item* item = actor->getLeftHandItem();
		Cvar_Set("mn_lweapon", "%s", item->def()->model);
		Cvar_Set("mn_lweapon_item", "%s", item->def()->id);
	} else {
		Cvar_Set("mn_lweapon", "");
		Cvar_Set("mn_lweapon_item", "");
	}

	/* print ammo */
	const Item* itemRight = actor->getRightHandItem();
	if (itemRight)
		Cvar_SetValue("mn_ammoright", itemRight->getAmmoLeft());
	else
		Cvar_Set("mn_ammoright", "");

	const Item* itemLeft = HUD_GetLeftHandWeapon(actor, nullptr);
	if (itemLeft)
		Cvar_SetValue("mn_ammoleft", itemLeft->getAmmoLeft());
	else
		Cvar_Set("mn_ammoleft", "");
}

static const char* HUD_GetPenaltyString (const int type)
{
	switch (type) {
	case MODIFIER_ACCURACY:
		return _("accuracy");
	case MODIFIER_SHOOTING:
		return _("shooting speed");
	case MODIFIER_MOVEMENT:
		return _("movement speed");
	case MODIFIER_SIGHT:
		return _("sight range");
	case MODIFIER_REACTION:
		return _("reaction speed");
	case MODIFIER_TU:
		return _("TU");
	default:
		return "";
	}
}

static void HUD_ActorWoundData_f (void)
{
	if (!CL_BattlescapeRunning())
		return;

	/* check if actor exists */
	if (!selActor)
		return;

	woundInfo_t* wounds = &selActor->wounds;
	const BodyData* bodyData = selActor->teamDef->bodyTemplate;

	for (int bodyPart = 0; bodyPart < bodyData->numBodyParts(); ++bodyPart) {
		const int woundThreshold = selActor->maxHP * bodyData->woundThreshold(bodyPart);

		if (wounds->woundLevel[bodyPart] + wounds->treatmentLevel[bodyPart] * 0.5 > woundThreshold) {
			const int bleeding = wounds->woundLevel[bodyPart] * (wounds->woundLevel[bodyPart] > woundThreshold
								 ? bodyData->bleedingFactor(bodyPart) : 0);
			char text[256];

			Com_sprintf(text, lengthof(text), CHRSH_IsTeamDefRobot(selActor->teamDef) ?
					_("Damaged %s (deterioration: %i)\n") : _("Wounded %s (bleeding: %i)\n"), _(bodyData->name(bodyPart)), bleeding);
			for (int penalty = MODIFIER_ACCURACY; penalty < MODIFIER_MAX; penalty++)
				if (bodyData->penalty(bodyPart, static_cast<modifier_types_t>(penalty)) != 0)
					Q_strcat(text, lengthof(text), _("- Reduced %s\n"), HUD_GetPenaltyString(penalty));
			UI_ExecuteConfunc("actor_wounds %s %i \"%s\"", bodyData->id(bodyPart), bleeding, text);
		}
	}
}

/**
 * @brief Update the equipment weight for the selected actor.
 */
static void HUD_UpdateActorLoad_f (void)
{
	if (!CL_BattlescapeRunning())
		return;

	/* check if actor exists */
	if (!selActor)
		return;

	const character_t* chr = CL_ActorGetChr(selActor);
	const int invWeight = selActor->inv.getWeight();
	const int maxWeight = GAME_GetChrMaxLoad(chr);
	const float penalty = GET_ENCUMBRANCE_PENALTY(invWeight, maxWeight);
	const int normalTU = GET_TU(chr->score.skills[ABILITY_SPEED], 1.0f - WEIGHT_NORMAL_PENALTY);
	const int tus = GET_TU(chr->score.skills[ABILITY_SPEED], penalty);
	const int tuPenalty = tus - normalTU;
	int count = 0;

	const Container* cont = nullptr;
	while ((cont = chr->inv.getNextCont(cont))) {
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			const fireDef_t* fireDef = item->getFiredefs();
			if (fireDef == nullptr)
				continue;
			for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
				const fireDef_t& fd = fireDef[i];
				if (fd.time > 0 && fd.time > tus) {
					if (count <= 0)
						Com_sprintf(popupText, sizeof(popupText), _("This soldier no longer has enough TUs to use the following items:\n\n"));
					Q_strcat(popupText, sizeof(popupText), "%s: %s (%i)\n", _(item->def()->name), _(fd.name), fd.time);
					++count;
				}
			}
		}
	}

	if (count > 0)
		UI_Popup(_("Warning"), popupText);

	char label[MAX_VAR];
	char tooltip[MAX_VAR];
	Com_sprintf(label, sizeof(label), "%g/%i %s", invWeight / WEIGHT_FACTOR, maxWeight, _("Kg"));
	Com_sprintf(tooltip, sizeof(tooltip), "%s %i (%+i)", _("TU:"), tus, tuPenalty);
	UI_ExecuteConfunc("inv_actorload \"%s\" \"%s\" %f %i", label, tooltip, WEIGHT_NORMAL_PENALTY - (1.0f - penalty), count);
}

/**
 * @brief Updates the hud for one actor
 * @param actor The actor to update the hud values for
 */
static void HUD_UpdateActor (le_t* actor)
{
	HUD_UpdateActorCvar(actor);
	/* write info */
	int time = 0;

	/* handle actor in a panic */
	if (LE_IsPanicked(actor)) {
		UI_RegisterText(TEXT_STANDARD, _("Currently panics!\n"));
	} else if (displayRemainingTus[REMAINING_TU_CROUCH]) {
		if (CL_ActorUsableTUs(actor) >= TU_CROUCH)
			time = TU_CROUCH;
	} else if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT]
	 || displayRemainingTus[REMAINING_TU_RELOAD_LEFT]) {
		const Item* item;
		containerIndex_t container;

		if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] && actor->getRightHandItem()) {
			container = CID_RIGHT;
			item = actor->getRightHandItem();
		} else if (displayRemainingTus[REMAINING_TU_RELOAD_LEFT] && actor->getLeftHandItem()) {
			container = NONE;
			item = HUD_GetLeftHandWeapon(actor, &container);
		} else {
			container = NONE;
			item = nullptr;
		}

		if (item && item->def() && item->ammoDef() && item->isReloadable()) {
			const int reloadtime = HUD_CalcReloadTime(actor, item->def(), container);
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
	time = std::max(0, actor->TU - time);
	Cvar_Set("mn_turemain", "%i", time);

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
		for (int i = 0; i < PATHFINDING_HEIGHT; i++) {
			int status = 0;
			if (i == cl_worldlevel->integer)
				status = 2;
			else if (i < cl.mapMaxLevel)
				status = 1;
			UI_ExecuteConfunc("updateLevelStatus %i %i", i, status);
		}
		cl_worldlevel->modified = false;
	}

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
 * @param data Unused here, but required by cvarChangeListenerFunc_t
 */
static void HUD_ActorSelectionChangeListener (const char* cvarName, const char* oldValue, const char* newValue, void* data)
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
 * @param data Unused here, but required by cvarChangeListenerFunc_t
 */
static void HUD_RightHandChangeListener (const char* cvarName, const char* oldValue, const char* newValue, void* data)
{
	if (!CL_OnBattlescape())
		return;

	if (Q_streq(oldValue, newValue))
		return;

	HUD_UpdateButtons(selActor);
}

/**
 * @brief Callback that is called when the left hand weapon of the current selected actor changed
 * @param cvarName The cvar name
 * @param oldValue The old value of the cvar
 * @param newValue The new value of the cvar
 * @param data Unused here, but required by cvarChangeListenerFunc_t
 */
static void HUD_LeftHandChangeListener (const char* cvarName, const char* oldValue, const char* newValue, void* data)
{
	if (!CL_OnBattlescape())
		return;

	if (Q_streq(oldValue, newValue))
		return;

	HUD_UpdateButtons(selActor);
}

/**
 * @brief Callback that is called when the remaining TUs for the current selected actor changed
 * @param cvarName The cvar name
 * @param oldValue The old value of the cvar
 * @param newValue The new value of the cvar
 * @param data Unused here, but required by cvarChangeListenerFunc_t
 */
static void HUD_TUChangeListener (const char* cvarName, const char* oldValue, const char* newValue, void* data)
{
	if (!CL_OnBattlescape())
		return;

	if (Q_streq(oldValue, newValue))
		return;

	HUD_UpdateButtons(selActor);
}

static bool CL_CvarWorldLevel (cvar_t* cvar)
{
	const int maxLevel = cl.mapMaxLevel ? cl.mapMaxLevel - 1 : PATHFINDING_HEIGHT - 1;
	return Cvar_AssertValue(cvar, 0, maxLevel, true);
}

/**
 * @brief Checks that the given cvar is a valid hud cvar
 * @param cvar The cvar to check
 * @return @c true if cvar is valid, @c false otherwise
 */
static bool HUD_CheckCLHud (cvar_t* cvar)
{
	const uiNode_t* window = UI_GetWindow(cvar->string);
	if (window == nullptr) {
		return false;
	}

	if (window->super == nullptr) {
		return false;
	}

	/**
	 * @todo check for multiple base classes
	 */
	return Q_streq(window->super->name, "hud");
}

/**
 * @brief Display the user interface
 * @param optionWindowName Name of the window used to display options, else nullptr if nothing
 */
void HUD_InitUI (const char* optionWindowName)
{
	OBJZERO(buttonStates);
	if (!HUD_CheckCLHud(cl_hud)) {
		Cvar_Set("cl_hud", "hud_default");
	}
	UI_InitStack(cl_hud->string, optionWindowName);

	UI_ExecuteConfunc("hudinit");
}

/**
 * @brief Checks that the given cvar is a valid hud cvar
 * @param cvar The cvar to check and to modify if the value is invalid
 * @return @c true if the valid is invalid, @c false otherwise
 */
static bool HUD_CvarCheckMNHud (cvar_t* cvar)
{
	if (!HUD_CheckCLHud(cl_hud)) {
		Cvar_Reset(cvar);
		return true;
	}
	return false;
}

void HUD_InitStartup (void)
{
	HUD_InitCallbacks();

	Cmd_AddCommand("hud_remainingtus", HUD_RemainingTUs_f, "Define if remaining TUs should be displayed in the TU-bar for some hovered-over button.");
	Cmd_AddCommand("hud_shotreserve", HUD_ShotReserve_f, "Reserve TUs for the selected entry in the popup.");
	Cmd_AddCommand("hud_shotreservationpopup", HUD_PopupFiremodeReservation_f, "Pop up a list of possible firemodes for reservation in the current turn.");
	Cmd_AddCommand("hud_selectreactionfiremode", HUD_SelectReactionFiremode_f, "Change/Select firemode used for reaction fire.");
	Cmd_AddCommand("hud_listfiremodes", HUD_DisplayFiremodes_f, "Display a list of firemodes for a weapon+ammo.");
	Cmd_AddCommand("hud_listactions", HUD_DisplayActions_f, "Display a list of action from the selected soldier.");
	Cmd_AddCommand("hud_updateactorwounds", HUD_ActorWoundData_f, "Update info on actor wounds.");
	Cmd_AddCommand("hud_updateactorload", HUD_UpdateActorLoad_f, "Update the HUD with the selected actor inventory load.");

	/** @note We can't check the value at startup cause scripts are not yet loaded */
	cl_hud = Cvar_Get("cl_hud", "hud_default", CVAR_ARCHIVE | CVAR_LATCH, "Current selected HUD.");
	Cvar_SetCheckFunction("cl_hud", HUD_CvarCheckMNHud);

	cl_worldlevel = Cvar_Get("cl_worldlevel", "0", 0, "Current worldlevel in tactical mode.");
	Cvar_SetCheckFunction("cl_worldlevel", CL_CvarWorldLevel);
	cl_worldlevel->modified = false;

	Cvar_Get("mn_ammoleft", "", 0, "The remaining amount of ammunition in the left hand weapon.");
	Cvar_Get("mn_lweapon", "", 0, "The left hand weapon model of the current selected actor - empty if no weapon.");
	Cvar_RegisterChangeListener("mn_ammoleft", HUD_LeftHandChangeListener);
	Cvar_RegisterChangeListener("mn_lweapon", HUD_LeftHandChangeListener);

	Cvar_Get("mn_ammoright", "", 0, "The remaining amount of ammunition in the right hand weapon.");
	Cvar_Get("mn_rweapon", "", 0, "The right hand weapon model of the current selected actor - empty if no weapon.");
	Cvar_RegisterChangeListener("mn_ammoright", HUD_RightHandChangeListener);
	Cvar_RegisterChangeListener("mn_rweapon", HUD_RightHandChangeListener);

	Cvar_Get("mn_turemain", "", 0, "Remaining TUs for the current selected actor.");
	Cvar_RegisterChangeListener("mn_turemain", HUD_TUChangeListener);

	Cvar_RegisterChangeListener("cl_selected", HUD_ActorSelectionChangeListener);

	cl_hud_message_timeout = Cvar_Get("cl_hud_message_timeout", "2000", CVAR_ARCHIVE, "Timeout for HUD messages (milliseconds).");
	cl_show_cursor_tooltips = Cvar_Get("cl_show_cursor_tooltips", "1", CVAR_ARCHIVE, "Show cursor tooltips in tactical game mode.");
}
