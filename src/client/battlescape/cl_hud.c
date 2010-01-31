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
int hitProbability;

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

static void HUD_RefreshWeaponButtons(const le_t* le, int additionalTime);
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
			const char* tooltip;
			const character_t *chr = CL_GetActorChr(le);
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
 * @brief Display 'usable" (blue) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 */
void HUD_DisplayPossibleReaction (const le_t * actor)
{
	if (!actor)
		return;

	/* Given actor does not equal the currently selected actor. This normally only happens on game-start. */
	if (!actor->selected)
		return;

	/* Display 'usable" (blue) reaction buttons
	 * Code also used in CL_ActorToggleReaction_f */
	if (actor->state & STATE_REACTION_ONCE)
		MN_ExecuteConfunc("startreactiononce");
	else if (actor->state & STATE_REACTION_MANY)
		MN_ExecuteConfunc("startreactionmany");
}

/**
 * @brief Display 'impossible" (red) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 * @return qtrue if nothing changed message was sent otherwise qfalse.
 */
qboolean HUD_DisplayImpossibleReaction (const le_t * actor)
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
 * @brief Displays the firemodes for the given hand.
 * @note Console command: list_firemodes
 */
static void HUD_DisplayFiremodes_f (void)
{
	int actorIdx;
	const objDef_t *ammo;
	const fireDef_t *fd;
	int i;
	char hand;

	if (!selActor)
		return;

	/* Not our turn */
	if (cls.team != cl.actTeam) {
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

	fd = CL_GetWeaponAndAmmo(selActor, hand);
	if (fd == NULL)
		return;

	ammo = fd->obj;
	if (!ammo)
		return;

	Com_DPrintf(DEBUG_CLIENT, "HUD_DisplayFiremodes_f: displaying %c firemodes.\n", hand);

	actorIdx = CL_GetActorNumber(selActor);
	if (actorIdx == -1)
		Com_Error(ERR_DROP, "Could not get current selected actor's id");

	for (i = 0; i < ammo->numFiredefs[fd->weapFdsIdx]; i++) {
		const fireDef_t *ammoFD = &ammo->fd[fd->weapFdsIdx][i];
		const int status = ammoFD->time <= CL_UsableTUs(selActor);
		qboolean reactionFireActive = qfalse;

		if (ammoFD->reaction) {
			const character_t* chr = CL_GetActorChr(selActor);
			reactionFireActive = THIS_FIREMODE(&chr->RFmode, ACTOR_GET_HAND_INDEX(hand), ammoFD->fdIdx);
		}

		MN_ExecuteConfunc("add_firemode_entry %c %i \"%s\" \"%s\" \"%s\" %i %i %i", hand,
				ammoFD->fdIdx, _(ammoFD->name), va(_("TU: %i"), ammoFD->time),
				va(_("Shots: %i"), ammoFD->ammo), ammoFD->reaction, status, reactionFireActive);
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

	if (firemode < 0 || firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("HUD_SelectReactionFiremode_f: Firemode index out of bounds (%i)\n",
			firemode);
		return;
	}

	CL_UpdateReactionFiremodes(selActor, hand, firemode);
}

/**
 * @brief Calculate total reload time for selected actor.
 * @param[in] le Pointer to local entity handling the weapon.
 * @param[in] weapon Item in (currently only right) hand.
 * @return Time needed to reload or >= 999 if no suitable ammo found.
 * @note This routine assumes the time to reload a weapon
 * @note in the right hand is the same as the left hand.
 * @sa HUD_RefreshWeaponButtons
 * @sa CL_CheckMenuAction
 * @todo Fix it - we can have different weapons in hands.
 */
static int CL_CalcReloadTime (const le_t *le, const objDef_t *weapon)
{
	int container;
	int tu = 999;

	if (!le)
		return tu;

	if (!weapon)
		return tu;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			const invList_t *ic;
			for (ic = le->i.c[container]; ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)) {
					tu = csi.ids[container].out;
					break;
				}
		}
	}

	/* total TU cost is the sum of 3 numbers:
	 * TU for weapon reload + TU to get ammo out + TU to put ammo in hands */
	tu += weapon->reload + csi.ids[csi.idRight].in;
	return tu;
}

/**
 * @brief Checks whether an action on hud menu is valid and displays proper message.
 * @param[in] le Pointer to local entity for which we handle an action on hud menu.
 * @param[in] weapon An item in hands.
 * @param[in] mode EV_INV_AMMO in case of fire button, EV_INV_RELOAD in case of reload button
 * @return qfalse when action is not possible, otherwise qtrue
 * @sa HUD_FireWeapon_f
 * @sa CL_ReloadLeft_f
 * @sa CL_ReloadRight_f
 * @todo Check for ammo in hand and give correct feedback in all cases.
 */
static qboolean CL_CheckMenuAction (const le_t* le, invList_t *weapon, int mode)
{
	/* No item in hand. */
	/** @todo Ignore this condition when ammo in hand. */
	if (!weapon || !weapon->item.t) {
		HUD_DisplayMessage(_("No item in hand.\n"));
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
			HUD_DisplayMessage(_("Can't perform action:\nout of ammo.\n"));
			return qfalse;
		}
		/* Cannot shoot because weapon is fireTwoHanded, yet both hands handle items. */
		if (weapon->item.t->fireTwoHanded && LEFT(le)) {
			HUD_DisplayMessage(_("This weapon cannot be fired\none handed.\n"));
			return qfalse;
		}
		break;
	case EV_INV_RELOAD:
		/* Check if reload is possible. Also checks for the correct amount of TUs. */

		/* Cannot reload because this item is not reloadable. */
		if (!weapon->item.t->reload) {
			HUD_DisplayMessage(_("Can't perform action:\nthis item is not reloadable.\n"));
			return qfalse;
		}
		/* Cannot reload because of no ammo in inventory. */
		if (CL_CalcReloadTime(le, weapon->item.t) >= 999) {
			HUD_DisplayMessage(_("Can't perform action:\nammo not available.\n"));
			return qfalse;
		}
		/* Cannot reload because of not enough TUs. */
		if (le->TU < CL_CalcReloadTime(le, weapon->item.t)) {
			HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"));
			return qfalse;
		}
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
	const objDef_t *ammo;
	const fireDef_t *fd;

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

	if (firemode >= MAX_FIREDEFS_PER_WEAPON || firemode < 0) {
		Com_Printf("HUD_FireWeapon_f: Firemode index to big (%i). Highest possible number is %i.\n",
			firemode, MAX_FIREDEFS_PER_WEAPON - 1);
		return;
	}

	fd = CL_GetWeaponAndAmmo(selActor, hand);
	if (fd == NULL)
		return;

	ammo = fd->obj;

	/* Let's check if shooting is possible.
	 * Don't let the selActor->TU parameter irritate you, it is not checked/used here. */
	if (hand == ACTOR_HAND_CHAR_RIGHT) {
		if (!CL_CheckMenuAction(selActor, RIGHT(selActor), EV_INV_AMMO))
			return;
	} else {
		if (!CL_CheckMenuAction(selActor, LEFT(selActor), EV_INV_AMMO))
			return;
	}

	if (ammo->fd[fd->weapFdsIdx][firemode].time <= CL_UsableTUs(selActor)) {
		/* Actually start aiming. This is done by changing the current mode of display. */
		if (hand == ACTOR_HAND_CHAR_RIGHT)
			selActor->actorMode = M_FIRE_R;
		else
			selActor->actorMode = M_FIRE_L;
		selActor->currentSelectedFiremode = firemode;	/* Store firemode. */
		HUD_HideFiremodes();
	} else {
		/* Cannot shoot because of not enough TUs - every other
		 * case should be checked previously in this function. */
		HUD_DisplayMessage(_("Can't perform action:\nnot enough TUs.\n"));
		Com_DPrintf(DEBUG_CLIENT, "HUD_FireWeapon_f: Firemode not available (%c, %s).\n",
			hand, ammo->fd[fd->weapFdsIdx][firemode].name);
	}
}

/**
 * @brief Remember if we hover over a button that would cost some TUs when pressed.
 * @note this is used in HUD_ActorUpdateCvars to update the "remaining TUs" bar correctly.
 * @todo move into the scripts
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
 * @param[in] weapon Pointer to weapon in hand.
 * @param[in] tu Remaining TU units of selected actor.
 * @param[in] hand ACTOR_HAND_LEFT for left, ACTOR_HAND_RIGHT for right hand
 * @return TU units needed for reloading or -1 if weapon cannot be reloaded.
 * @todo move parts of it into the scripts
 */
static int HUD_WeaponCanBeReloaded (const le_t *le, invList_t *weapon, int tu, int hand)
{
	int container;
	qboolean notEnoughTU = qfalse;

	assert(hand == ACTOR_HAND_LEFT || hand == ACTOR_HAND_RIGHT);

	if (!le)
		return -1;

	/* No weapon in hand. */
	if (!weapon) {
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("No weapon in right hand."));
		else
			Cvar_Set("mn_reloadleft_tt", _("No weapon in left hand."));
		return -1;
	}

	assert(weapon->item.t);

	/* This weapon cannot be reloaded. */
	if (!weapon->item.t->reload) {
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("Weapon in right hand cannot be reloaded."));
		else
			Cvar_Set("mn_reloadleft_tt", _("Weapon in left hand cannot be reloaded."));
		return -1;
	}

	/* Weapon is fully loaded. */
	if (weapon->item.m && weapon->item.a && weapon->item.t->ammo == weapon->item.a) {
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand, already fully loaded."));
		else
			Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand, already fully loaded."));
		return -1;
	}

	/* Weapon is empty, find ammo of any type loadable to this weapon. */
	if (!weapon->item.m || (weapon->item.a == 0)) {
		for (container = 0; container < csi.numIDs; container++) {
			if (csi.ids[container].out < tu && !csi.ids[container].temp)  {
				const invList_t *ic;
				for (ic = le->i.c[container]; ic; ic = ic->next) {
					if (INVSH_LoadableInWeapon(ic->item.t, weapon->item.t)) {
						notEnoughTU = qtrue;
						break;
					}
				}
				/* If we have enough TU to reload, return here. Otherwise
				 * search in another container, maybe another needs less TU. */
				if (tu >= weapon->item.t->reload + csi.ids[container].out)
					return (weapon->item.t->reload + csi.ids[container].out);
			}
		}
		/* Found no ammo which could be used for this weapon. */
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand, you don't have backup ammo."));
		else
			Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand, you don't have backup ammo."));
	}

	/* Weapon is not fully loaded but there is similar clip in the inventory. */
	if (weapon->item.m && weapon->item.a && weapon->item.t->ammo > weapon->item.a) {
		for (container = 0; container < csi.numIDs; container++) {
			if (csi.ids[container].out < tu && !csi.ids[container].temp)  {
				const invList_t *ic;
				for (ic = le->i.c[container]; ic; ic = ic->next) {
					if (INVSH_LoadableInWeapon(ic->item.t, weapon->item.t) && weapon->item.m == ic->item.t) {
						notEnoughTU = qtrue;
						break;
					}
				}
				/* If we have enough TU to reload, return here. Otherwise
				 * search in another container, maybe another needs less TU. */
				if (tu >= weapon->item.t->reload + csi.ids[container].out)
					return (weapon->item.t->reload + csi.ids[container].out);
			}
		}
		/* Found no backup ammo of the same type as loaded. */
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand, you don't have backup ammo."));
		else
			Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand, you don't have backup ammo."));
	}

	/* If notEnoughTU is qtrue, then we found backup ammo, but we don't have
	   enough TU to reload. Otherwise, we don't have backup ammo. */
	if (notEnoughTU) {
		if (hand == ACTOR_HAND_RIGHT)
			Cvar_Set("mn_reloadright_tt", _("Not enough TUs for reloading weapon in right hand."));
		else
			Cvar_Set("mn_reloadleft_tt", _("Not enough TUs for reloading weapon in left hand."));
	}

	return -1;
}

/**
 * @brief Checks if there is a weapon in the hand that can be used for reaction fire.
 * @param[in] actor What actor to check.
 * @param[in] hand Which hand to check: 'l' for left hand, 'r' for right hand.
 */
static qboolean CL_WeaponWithReaction (const le_t * actor, const char hand)
{
	int i;
	const fireDef_t *fd;

	/* Get ammo and weapon-index in ammo (if there is a weapon in that hand). */
	fd = CL_GetWeaponAndAmmo(actor, hand);

	if (fd == NULL)
		return qfalse;

	/* Check ammo for reaction-enabled firemodes. */
	for (i = 0; i < fd->obj->numFiredefs[fd->weapFdsIdx]; i++)
		if (fd[i].reaction)
			return qtrue;

	return qfalse;
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] le Pointer to local entity for which we refresh HUD buttons.
 * @sa HUD_ActorUpdateCvars
 * @todo move parts of it into the scripts
 */
static void HUD_RefreshWeaponButtons (const le_t *le, int additionalTime)
{
	invList_t *weaponr;
	invList_t *weaponl;
	invList_t *headgear;
	int rightCanBeReloaded = -1, leftCanBeReloaded = -1;
	const int time = additionalTime + CL_UsableTUs(le);

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
		if (time < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for standing up."));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Stand up (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_CROUCH, BT_STATE_DESELECT);
		}
	} else {
		if (time < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for crouching."));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Crouch (%i TU)"), TU_CROUCH));
			HUD_SetWeaponButton(BT_STAND, BT_STATE_DESELECT);
		}
	}

	/* reaction-fire button */
	if (!(le->state & STATE_REACTION)) {
		if (CL_WeaponWithReaction(le, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(le, ACTOR_HAND_CHAR_LEFT))
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DESELECT);
		else
			HUD_SetWeaponButton(BT_REACTION, BT_STATE_DISABLE);
	} else {
		if (CL_WeaponWithReaction(le, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(le, ACTOR_HAND_CHAR_LEFT))
			HUD_DisplayPossibleReaction(le);
		else
			HUD_DisplayImpossibleReaction(le);
	}

	/* Reload buttons */
	rightCanBeReloaded = HUD_WeaponCanBeReloaded(le, weaponr, time, ACTOR_HAND_RIGHT);
	if (rightCanBeReloaded > 0) {
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadright_tt", va(_("Reload weapon (%i TU)."), rightCanBeReloaded));
	} else {
		HUD_SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DISABLE);
	}

	leftCanBeReloaded = HUD_WeaponCanBeReloaded(le, weaponl, time, ACTOR_HAND_LEFT);
	if (leftCanBeReloaded > 0) {
		HUD_SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadleft_tt", va(_("Reload weapon (%i TU)."), leftCanBeReloaded));
	} else {
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
 * unlike CL_CharacterCvars and CL_UGVCvars which updates them for
 * displaying the data in the menu system
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
	const char *animName;
	int time, i;
	pos3_t pos;

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	if (cls.state != ca_active)
		return;

	/* set Cvars for all actors */
	HUD_ActorGlobalCvars();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	if (selActor) {
		/* set generic cvars */
		Cvar_Set("mn_tu", va("%i", selActor->TU));
		Cvar_Set("mn_tumax", va("%i", selActor->maxTU));

		Com_sprintf(tuTooltipText, lengthof(tuTooltipText),
			_("Time Units\n- Available: %i (of %i)\n- Remaining: %i\n"),
			selActor->TU, selActor->maxTU,
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

		/* write info */
		time = 0;

		/* display special message */
		if (cl.time < hudTime)
			Q_strncpyz(infoText, hudText, sizeof(infoText));

		/* update HUD stats etc in more or shoot modes */
		if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT]
		 || displayRemainingTus[REMAINING_TU_RELOAD_LEFT]
		 || displayRemainingTus[REMAINING_TU_CROUCH]) {
			/* We are hovering over a "reload" button */
			/** @sa HUD_RefreshWeaponButtons */
			if (displayRemainingTus[REMAINING_TU_RELOAD_RIGHT] && RIGHT(selActor)) {
				const invList_t *weapon = RIGHT(selActor);
				const int reloadtime = CL_CalcReloadTime(selActor, weapon->item.t);
				if (weapon->item.m && weapon->item.t->reload
					 && CL_UsableTUs(selActor) >= reloadtime) {
					time = reloadtime;
				}
			} else if (displayRemainingTus[REMAINING_TU_RELOAD_LEFT] && LEFT(selActor)) {
				const invList_t *weapon = LEFT(selActor);
				const int reloadtime = CL_CalcReloadTime(selActor, weapon->item.t);
				if (weapon && weapon->item.m && weapon->item.t->reload
					 && CL_UsableTUs(selActor) >= reloadtime) {
					time = reloadtime;
				}
			} else if (displayRemainingTus[REMAINING_TU_CROUCH]) {
				if (CL_UsableTUs(selActor) >= TU_CROUCH)
					time = TU_CROUCH;
			}
		} else if (selActor->actorMode == M_MOVE || selActor->actorMode == M_PEND_MOVE) {
			/* If the mouse is outside the world, and we haven't placed the cursor in pend
			 * mode already or the selected grid field is not reachable (ROUTING_NOT_REACHABLE) */
			if ((mouseSpace != MS_WORLD && selActor->actorMode < M_PEND_MOVE) || selActor->actorMoveLength == ROUTING_NOT_REACHABLE) {
				selActor->actorMoveLength = ROUTING_NOT_REACHABLE;
				Com_sprintf(infoText, lengthof(infoText), _("Morale  %i"), selActor->morale);
				MN_ResetData(TEXT_MOUSECURSOR_RIGHT);
			}
			if (selActor->actorMoveLength != ROUTING_NOT_REACHABLE) {
				const int moveMode = CL_MoveMode(selActor, selActor->actorMoveLength);
				Com_sprintf(infoText, lengthof(infoText), _("Morale  %i\n%s %i (%i TU left)\n"), selActor->morale,
					moveModeDescriptions[moveMode] , selActor->actorMoveLength, selActor->TU - selActor->actorMoveLength);

				if (selActor->actorMoveLength <= CL_UsableTUs(selActor))
					Com_sprintf(mouseText, lengthof(mouseText), "%i (%i)\n", selActor->actorMoveLength, CL_UsableTUs(selActor));
				else
					Com_sprintf(mouseText, lengthof(mouseText), "- (-)\n");

				MN_RegisterText(TEXT_MOUSECURSOR_RIGHT, mouseText);
			}
			time = selActor->actorMoveLength;
		} else {
			const invList_t *selWeapon;

			/* get weapon */
			if (IS_MODE_FIRE_HEADGEAR(selActor->actorMode)) {
				selWeapon = HEADGEAR(selActor);
			} else if (IS_MODE_FIRE_LEFT(selActor->actorMode)) {
				selWeapon = LEFT(selActor);
			} else {
				selWeapon = RIGHT(selActor);
			}

			if (!selWeapon && RIGHT(selActor) && RIGHT(selActor)->item.t->holdTwoHanded)
				selWeapon = RIGHT(selActor);

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
					selActor->actorMode = M_MOVE;
				} else if (selActor->fd) {
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
					if ((selWeapon->item.t->reload && selWeapon->item.a <= 0) || CL_UsableTUs(selActor) < time)
						selActor->actorMode = M_MOVE;
				} else if (selWeapon) {
					Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(selWeapon->item.t->name));
				}
			} else {
				selActor->actorMode = M_MOVE;
			}
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

		/* Calculate remaining TUs. */
		time = max(0, selActor->TU - time);

		Cvar_Set("mn_turemain", va("%i", time));

		/* print ammo */
		if (RIGHT(selActor))
			Cvar_SetValue("mn_ammoright", RIGHT(selActor)->item.a);
		else
			Cvar_Set("mn_ammoright", "");

		if (LEFT(selActor))
			Cvar_SetValue("mn_ammoleft", LEFT(selActor)->item.a);
		else
			Cvar_Set("mn_ammoleft", "");

		if (!LEFT(selActor) && RIGHT(selActor) && RIGHT(selActor)->item.t->holdTwoHanded)
			Cvar_Set("mn_ammoleft", Cvar_GetString("mn_ammoright"));

		selActor->oldstate = selActor->state;
		if (selActor->actorMoveLength >= ROUTING_NOT_REACHABLE || (selActor->actorMode != M_MOVE && selActor->actorMode != M_PEND_MOVE))
			time = CL_UsableTUs(selActor);

		HUD_RefreshWeaponButtons(selActor, time);

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

	/* player bar */
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

	if (!CL_CheckAction(selActor))
		return;

	if (selActor->state & STATE_REACTION_MANY)
		state = ~STATE_REACTION;
	else if (!(selActor->state & STATE_REACTION))
		state = STATE_REACTION_ONCE;
	else if (selActor->state & STATE_REACTION_ONCE)
		state = STATE_REACTION_MANY;

	/* Send request to update actor's reaction state to the server. */
	MSG_Write_PA(PA_STATE, selActor->entnum, state);
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
	if (!selActor || !CL_CheckMenuAction(selActor, CL_GetLeftHandWeapon(selActor), EV_INV_RELOAD))
		return;
	CL_ActorReload(selActor, csi.idLeft);
}

/**
 * @brief Reload right weapon.
 */
static void CL_ReloadRight_f (void)
{
	if (!selActor || !CL_CheckMenuAction(selActor, RIGHT(selActor), EV_INV_RELOAD))
		return;
	CL_ActorReload(selActor, csi.idRight);
}

void HUD_InitStartup (void)
{
	Cmd_AddCommand("reloadleft", CL_ReloadLeft_f, _("Reload the weapon in the soldiers left hand"));
	Cmd_AddCommand("reloadright", CL_ReloadRight_f, _("Reload the weapon in the soldiers right hand"));
	Cmd_AddCommand("remaining_tus", HUD_RemainingTUs_f, "Define if remaining TUs should be displayed in the TU-bar for some hovered-over button.");
	Cmd_AddCommand("sel_reactmode", HUD_SelectReactionFiremode_f, "Change/Select firemode used for reaction fire.");
	Cmd_AddCommand("list_firemodes", HUD_DisplayFiremodes_f, "Display a list of firemodes for a weapon+ammo.");
	Cmd_AddCommand("togglereaction", CL_ActorToggleReaction_f, _("Toggle reaction fire"));
	Cmd_AddCommand("fireweap", HUD_FireWeapon_f, "Start aiming the weapon.");

	cl_hud_message_timeout = Cvar_Get("cl_hud_message_timeout", "2000", CVAR_ARCHIVE, "Timeout for HUD messages (milliseconds)");
	cl_show_cursor_tooltips = Cvar_Get("cl_show_cursor_tooltips", "1", CVAR_ARCHIVE, "Show cursor tooltips in tactical game mode");
}
