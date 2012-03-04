/**
 * @file cl_hud_callbacks.c
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
#include "cl_hud_callbacks.h"
#include "cl_hud.h"
#include "cl_actor.h"

/**
 * @brief returns the weapon the actor's left hand is touching. In case the actor
 * holds a two handed weapon in his right hand, this weapon is returned here.
 * This function only returns @c NULL if no two handed weapon is in the right hand
 * and the left hand is empty.
 */
invList_t* HUD_GetLeftHandWeapon (const le_t *actor, containerIndex_t *container)
{
	invList_t *invList = LEFT(actor);

	if (!invList) {
		invList = RIGHT(actor);
		if (invList == NULL || !invList->item.t->holdTwoHanded)
			invList = NULL;
		else if (container != NULL)
			*container = csi.idRight;
	}

	return invList;
}

/**
 * @brief Returns the fire definition of the item the actor has in the given hand.
 * @param[in] actor The pointer to the actor we want to get the data from.
 * @param[in] hand Which hand to use
 * @return the used @c fireDef_t
 */
const fireDef_t *HUD_GetFireDefinitionForHand (const le_t * actor, const actorHands_t hand)
{
	const invList_t *invlistWeapon;

	if (!actor)
		return NULL;

	invlistWeapon = ACTOR_GET_INV(actor, hand);
	if (!invlistWeapon || !invlistWeapon->item.t)
		return NULL;

	return FIRESH_FiredefForWeapon(&invlistWeapon->item);
}

/**
 * @brief Check if shooting is possible.
 * i.e. The weapon has ammo and can be fired with the 'available' hands.
 * TUs (i.e. "time") are are _not_ checked here, this needs to be done
 * elsewhere for the correct firemode.
 * @sa HUD_FireWeapon_f
 * @return qfalse when action is not possible, otherwise qtrue
 */
static qboolean HUD_CheckShooting (const le_t* le, invList_t *weapon)
{
	if (!le)
		return qfalse;

	/* No item in hand. */
	if (!weapon || !weapon->item.t) {
		HUD_DisplayMessage(_("No item in hand.\n"));
		return qfalse;
	}

	/* Cannot shoot because of lack of ammo. */
	if (weapon->item.a <= 0 && weapon->item.t->reload) {
		HUD_DisplayMessage(_("Can't perform action:\nout of ammo.\n"));
		return qfalse;
	}
	/* Cannot shoot because weapon is fireTwoHanded, yet both hands handle items. */
	if (weapon->item.t->fireTwoHanded && LEFT(le)) {
		HUD_DisplayMessage(_("This weapon cannot be fired\none handed.\n"));
		return qfalse;
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
	actorHands_t hand;
	fireDefIndex_t firemode;
	const objDef_t *ammo;
	const fireDef_t *fd;
	le_t *actor = selActor;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: %s <l|r> <firemode number>\n", Cmd_Argv(0));
		return;
	}

	if (actor == NULL)
		return;

	hand = ACTOR_GET_HAND_INDEX(Cmd_Argv(1)[0]);
	firemode = atoi(Cmd_Argv(2));
	if (firemode >= MAX_FIREDEFS_PER_WEAPON || firemode < 0)
		return;

	fd = HUD_GetFireDefinitionForHand(actor, hand);
	if (fd == NULL)
		return;

	ammo = fd->obj;

	/* Let's check if shooting is possible. */
	if (!HUD_CheckShooting(actor, ACTOR_GET_INV(actor, hand)))
		return;

	if (ammo->fd[fd->weapFdsIdx][firemode].time <= CL_ActorUsableTUs(actor)) {
		/* Actually start aiming. This is done by changing the current mode of display. */
		if (hand == ACTOR_HAND_RIGHT)
			CL_ActorSetMode(actor, M_FIRE_R);
		else
			CL_ActorSetMode(actor, M_FIRE_L);
		/* Store firemode. */
		actor->currentSelectedFiremode = firemode;
	} else {
		/* Cannot shoot because of not enough TUs - every other
		 * case should be checked previously in this function. */
		HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"));
	}
}

static void HUD_SetMoveMode_f (void)
{
	le_t *actor = selActor;

	if (actor == NULL)
		return;

	CL_ActorSetMode(actor, M_MOVE);
}


/**
 * @brief Toggles if the current actor reserves Tus for crouching.
 */
static void HUD_ToggleCrouchReservation_f (void)
{
	le_t *actor = selActor;

	if (!CL_ActorCheckAction(actor))
		return;

	if (CL_ActorReservedTUs(actor, RES_CROUCH) >= TU_CROUCH) {
		/* Reset reserved TUs to 0 */
		CL_ActorReserveTUs(actor, RES_CROUCH, 0);
	} else {
		/* Reserve the exact amount for crouching/standing up (if we have enough to do so). */
		CL_ActorReserveTUs(actor, RES_CROUCH, TU_CROUCH);
	}
}

/**
 * @brief Toggles reaction fire states for the current selected actor
 */
static void HUD_ToggleReaction_f (void)
{
	int state = 0;
	le_t *actor = selActor;

	if (!CL_ActorCheckAction(actor))
		return;

	if (!(actor->state & STATE_REACTION))
		state = STATE_REACTION;
	else
		state = ~STATE_REACTION;

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
 * @sa HUD_RefreshButtons
 * @sa HUD_CheckReload
 */
int HUD_CalcReloadTime (const le_t *le, const objDef_t *weapon, containerIndex_t toContainer)
{
	containerIndex_t container;
	invList_t *ic;

	if (toContainer == NONE)
		return -1;

	assert(le);
	assert(weapon);

	container = CL_ActorGetContainerForReload(&ic, &le->i, weapon);
	if (container == NONE)
		return -1;

	/* total TU cost is the sum of 3 numbers:
	 * TU for weapon reload + TU to get ammo out + TU to put ammo in hands */
	return TU_GET_RELOAD(container, toContainer, weapon);
}

/**
 * @brief Check if reload is possible.
 * @param[in] le Pointer to local entity for which we handle an action on hud menu.
 * @param[in] weapon An item in hands.
 * @param[in] container The container to get the ammo from
 * @return qfalse when action is not possible, otherwise qtrue
 * @sa HUD_ReloadLeft_f
 * @sa HUD_ReloadRight_f
 */
static qboolean HUD_CheckReload (const le_t* le, const invList_t *weapon, containerIndex_t container)
{
	int tus;

	if (!le)
		return qfalse;

	/* No item in hand. */
	if (!weapon || !weapon->item.t) {
		HUD_DisplayMessage(_("No item in hand.\n"));
		return qfalse;
	}

	/* Cannot reload because this item is not reloadable. */
	if (!weapon->item.t->reload) {
		HUD_DisplayMessage(_("Can't perform action:\nthis item is not reloadable.\n"));
		return qfalse;
	}

	tus = HUD_CalcReloadTime(le, weapon->item.t, container);
	/* Cannot reload because of no ammo in inventory. */
	if (tus == -1) {
		HUD_DisplayMessage(_("Can't perform action:\nammo not available.\n"));
		return qfalse;
	}
	/* Cannot reload because of not enough TUs. */
	if (le->TU < tus) {
		HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Reload left weapon.
 */
static void HUD_ReloadLeft_f (void)
{
	containerIndex_t container = csi.idLeft;
	le_t *actor = selActor;

	if (actor == NULL)
		return;

	if (!HUD_CheckReload(actor, HUD_GetLeftHandWeapon(actor, &container), container))
		return;
	CL_ActorReload(actor, container);
}

/**
 * @brief Reload right weapon.
 */
static void HUD_ReloadRight_f (void)
{
	le_t *actor = selActor;

	if (!actor || !HUD_CheckReload(actor, RIGHT(actor), csi.idRight))
		return;
	CL_ActorReload(actor, csi.idRight);
}

/**
 * Ask the current selected solider to execute an action
 * @todo extend it to open doors, or thing like that
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

	if (Q_strstart(Cmd_Argv(1), "fire_hand") && strlen(Cmd_Argv(1)) >= 13) {
		const char hand = Cmd_Argv(1)[9];
		const int index = atoi(Cmd_Argv(1) + 12);
		Cmd_ExecuteString(va("hud_fireweapon %c %i", hand, index));
		return;
	}

	Com_Printf("HUD_ExecuteAction_f: Action \"%s\" unknown.\n", Cmd_Argv(1));
}

void HUD_InitCallbacks (void)
{
	Cmd_AddCommand("hud_movemode", HUD_SetMoveMode_f, N_("Set selected actor to move mode (it cancels the fire mode)"));
	Cmd_AddCommand("hud_reloadleft", HUD_ReloadLeft_f, N_("Reload the weapon in the soldiers left hand"));
	Cmd_AddCommand("hud_reloadright", HUD_ReloadRight_f, N_("Reload the weapon in the soldiers right hand"));
	Cmd_AddCommand("hud_togglecrouchreserve", HUD_ToggleCrouchReservation_f, N_("Toggle reservation for crouching."));
	Cmd_AddCommand("hud_togglereaction", HUD_ToggleReaction_f, N_("Toggle reaction fire"));
	Cmd_AddCommand("hud_fireweapon", HUD_FireWeapon_f, N_("Start aiming the weapon."));
	Cmd_AddCommand("hud_executeaction", HUD_ExecuteAction_f, N_("Execute an action."));
}
