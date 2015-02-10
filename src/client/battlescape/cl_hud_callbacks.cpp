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

#include "../client.h"
#include "cl_hud_callbacks.h"
#include "cl_hud.h"
#include "cl_actor.h"

/**
 * @brief returns the weapon the actor's left hand is touching. In case the actor
 * holds a two handed weapon in his right hand, this weapon is returned here.
 * This function only returns @c nullptr if no two handed weapon is in the right hand
 * and the left hand is empty.
 */
Item* HUD_GetLeftHandWeapon (const le_t* actor, containerIndex_t* container)
{
	Item* item = actor->getLeftHandItem();

	if (!item) {
		item = actor->getRightHandItem();
		if (item == nullptr || !item->isHeldTwoHanded())
			item = nullptr;
		else if (container != nullptr)
			*container = CID_RIGHT;
	}

	return item;
}

/**
 * @brief Returns the fire definition of the item the actor has in the given hand.
 * @param[in] actor The pointer to the actor we want to get the data from.
 * @param[in] hand Which hand to use
 * @return the used @c fireDef_t
 */
const fireDef_t* HUD_GetFireDefinitionForHand (const le_t* actor, const actorHands_t hand)
{
	if (!actor)
		return nullptr;

	const Item* weapon = actor->getHandItem(hand);
	if (!weapon || !weapon->def())
		return nullptr;

	return weapon->getFiredefs();
}

/**
 * @brief Check if shooting is possible.
 * i.e. The weapon has ammo and can be fired with the 'available' hands.
 * TUs (i.e. "time") are are _not_ checked here, this needs to be done
 * elsewhere for the correct firemode.
 * @sa HUD_FireWeapon_f
 * @return false when action is not possible, otherwise true
 */
static bool HUD_CheckShooting (const le_t* le, Item* weapon)
{
	if (!le)
		return false;

	/* No item in hand. */
	if (!weapon || !weapon->def()) {
		HUD_DisplayMessage(_("Can't perform action - no item in hand!"));
		return false;
	}
	/* Cannot shoot because of lack of ammo. */
	if (weapon->mustReload()) {
		HUD_DisplayMessage(_("Can't perform action - no ammo!"));
		return false;
	}
	/* Cannot shoot because weapon is fireTwoHanded, yet both hands handle items. */
	if (weapon->def()->fireTwoHanded && le->getLeftHandItem()) {
		HUD_DisplayMessage(_("Can't perform action - weapon cannot be fired one handed!"));
		return false;
	}

	return true;
}

/**
 * @brief Starts aiming/target mode for selected left/right firemode.
 * @note Previously know as a combination of CL_FireRightPrimary, CL_FireRightSecondary,
 * @note CL_FireLeftPrimary and CL_FireLeftSecondary.
 */
static void HUD_FireWeapon_f (void)
{
	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s <l|r> <firemode number>\n", Cmd_Argv(0));
		return;
	}

	le_t* actor = selActor;
	if (actor == nullptr)
		return;

	const actorHands_t hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]);
	const fireDefIndex_t firemode = atoi(Cmd_Argv(2));
	if (firemode >= MAX_FIREDEFS_PER_WEAPON || firemode < 0)
		return;

	const fireDef_t* fd = HUD_GetFireDefinitionForHand(actor, hand);
	if (fd == nullptr)
		return;

	const objDef_t* ammo = fd->obj;

	/* Let's check if shooting is possible. */
	if (!HUD_CheckShooting(actor, actor->getHandItem(hand)))
		return;

	if (CL_ActorTimeForFireDef(actor, &ammo->fd[fd->weapFdsIdx][firemode]) <= CL_ActorUsableTUs(actor)) {
		/* Actually start aiming. This is done by changing the current mode of display. */
		if (hand == ACTOR_HAND_RIGHT)
			CL_ActorSetMode(actor, M_FIRE_R);
		else
			CL_ActorSetMode(actor, M_FIRE_L);
		/* Store firemode. */
		actor->currentSelectedFiremode = firemode;
	} else {
		/* Can not shoot because of not enough TUs - every other
		 * case should be checked previously in this function. */
		HUD_DisplayMessage(_("Can't perform action - not enough TUs!"));
	}
}

static void HUD_SetMoveMode_f (void)
{
	le_t* actor = selActor;

	if (actor == nullptr)
		return;

	CL_ActorSetMode(actor, M_MOVE);
}

/**
 * @brief Toggles if the current actor reserves TUs for crouching.
 */
static void HUD_ToggleCrouchReservation_f (void)
{
	const le_t* actor = selActor;

	if (!CL_ActorCheckAction(actor))
		return;

	if (CL_ActorReservedTUs(actor, RES_CROUCH) >= TU_CROUCH) {
		/* Reset reserved TUs to 0 */
		CL_ActorReserveTUs(actor, RES_CROUCH, 0);
		HUD_DisplayMessage(_("Disabled automatic crouching/standing up."));
	} else {
		/* Reserve the exact amount for crouching/standing up (if we have enough to do so). */
		CL_ActorReserveTUs(actor, RES_CROUCH, TU_CROUCH);
		HUD_DisplayMessage(va(_("Reserved %i TUs for crouching/standing up."), TU_CROUCH));
	}
}

/**
 * @brief Toggles reaction fire states for the current selected actor
 */
static void HUD_ToggleReaction_f (void)
{
	int state = 0;
	const le_t* actor = selActor;

	if (!CL_ActorCheckAction(actor))
		return;

	if (!(actor->state & STATE_REACTION)) {
		if (actor->inv.holdsReactionFireWeapon() && CL_ActorUsableTUs(actor) >= HUD_ReactionFireGetTUs(actor)) {
			state = STATE_REACTION;
			HUD_DisplayMessage(_("Reaction fire enabled."));
		} else {
			return;
		}
	} else {
		state = ~STATE_REACTION;
		HUD_DisplayMessage(_("Reaction fire disabled."));
	}

	/* Send request to update actor's reaction state to the server. */
	MSG_Write_PA(PA_STATE, actor->entnum, state);
}

/**
 * @brief Calculate total reload time for selected actor.
 * @param[in] le Pointer to local entity handling the weapon.
 * @param[in] weapon Item in (currently only right) hand.
 * @param[in] toContainer The container index to get the ammo from (used
 * to calculate the TUs that are needed to move the item out of this container)
 * @return Time needed to reload or @c -1 if no suitable ammo found.
 * @note This routine assumes the time to reload a weapon
 * @note in the right hand is the same as the left hand.
 * @sa HUD_UpdateButtons
 * @sa HUD_CheckReload
 */
int HUD_CalcReloadTime (const le_t* le, const objDef_t* weapon, containerIndex_t toContainer)
{
	if (toContainer == NONE)
		return -1;

	assert(le);
	assert(weapon);

	Item* ic;
	const containerIndex_t container = CL_ActorGetContainerForReload(&ic, &le->inv, weapon);
	if (container == NONE)
		return -1;

	/* total TU cost is the sum of 3 numbers:
	 * TU for weapon reload + TU to get ammo out + TU to put ammo in hands */
	return INVDEF(container)->out + INVDEF(toContainer)->in + weapon->getReloadTime();
}

/**
 * @brief Check if reload is possible.
 * @param[in] le Pointer to local entity for which we handle an action on hud menu.
 * @param[in] weapon An item in hands.
 * @param[in] container The container to get the ammo from
 * @return false when action is not possible, otherwise true
 * @sa HUD_ReloadLeft_f
 * @sa HUD_ReloadRight_f
 */
static bool HUD_CheckReload (const le_t* le, const Item* weapon, containerIndex_t container)
{
	if (!le)
		return false;

	/* No item in hand. */
	if (!weapon || !weapon->def()) {
		HUD_DisplayMessage(_("Can't perform action - no item in hand!"));
		return false;
	}

	/* Cannot reload because this item is not reloadable. */
	if (!weapon->isReloadable()) {
		HUD_DisplayMessage(_("Can't perform action - this item is not reloadable!"));
		return false;
	}

	const int tus = HUD_CalcReloadTime(le, weapon->def(), container);
	/* Cannot reload because of no ammo in inventory. */
	if (tus == -1) {
		HUD_DisplayMessage(_("Can't perform action - no ammo!"));
		return false;
	}
	/* Cannot reload because of not enough TUs. */
	if (le->TU < tus) {
		HUD_DisplayMessage(_("Can't perform action - not enough TUs!"));
		return false;
	}

	return true;
}

/**
 * @brief Reload left weapon.
 */
static void HUD_ReloadLeft_f (void)
{
	le_t* actor = selActor;

	if (actor == nullptr)
		return;

	containerIndex_t container = CID_LEFT;
	if (!HUD_CheckReload(actor, HUD_GetLeftHandWeapon(actor, &container), container))
		return;
	CL_ActorReload(actor, container);
	HUD_DisplayMessage(_("Left hand weapon reloaded."));
}

/**
 * @brief Reload right weapon.
 */
static void HUD_ReloadRight_f (void)
{
	le_t* actor = selActor;

	if (!actor || !HUD_CheckReload(actor, actor->getRightHandItem(), CID_RIGHT))
		return;
	CL_ActorReload(actor, CID_RIGHT);
	HUD_DisplayMessage(_("Right hand weapon reloaded."));
}

/**
 * Ask the current selected soldier to execute an action
 * @todo extend it to open doors or things like that
 */
static void HUD_ExecuteAction_f (void)
{
	if (!selActor)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <actionid>\n", Cmd_Argv(0));
		return;
	}

	if (Q_streq(Cmd_Argv(1), "reload_handl")) {
		HUD_ReloadLeft_f();
		return;
	}

	if (Q_streq(Cmd_Argv(1), "reload_handr")) {
		HUD_ReloadRight_f();
		return;
	}

	if (char const* const rest = Q_strstart(Cmd_Argv(1), "fire_hand")) {
		if (strlen(rest) >= 4) {
			char const hand  = rest[0];
			int  const index = atoi(rest + 3);
			Cmd_ExecuteString("hud_fireweapon %c %i", hand, index);
			return;
		}
	}

	Com_Printf("HUD_ExecuteAction_f: Action \"%s\" unknown.\n", Cmd_Argv(1));
}

void HUD_InitCallbacks (void)
{
	Cmd_AddCommand("hud_movemode", HUD_SetMoveMode_f, N_("Set selected actor to move mode (it cancels the fire mode)."));
	Cmd_AddCommand("hud_reloadleft", HUD_ReloadLeft_f, N_("Reload the weapon in the soldiers left hand."));
	Cmd_AddCommand("hud_reloadright", HUD_ReloadRight_f, N_("Reload the weapon in the soldiers right hand."));
	Cmd_AddCommand("hud_togglecrouchreserve", HUD_ToggleCrouchReservation_f, N_("Toggle reservation for crouching."));
	Cmd_AddCommand("hud_togglereaction", HUD_ToggleReaction_f, N_("Toggle reaction fire."));
	Cmd_AddCommand("hud_fireweapon", HUD_FireWeapon_f, N_("Start aiming the weapon."));
	Cmd_AddCommand("hud_executeaction", HUD_ExecuteAction_f, N_("Execute an action."));
}
