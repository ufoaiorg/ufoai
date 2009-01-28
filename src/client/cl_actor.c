/**
 * @file cl_actor.c
 * @brief Actor related routines.
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
#include "cl_global.h"
#include "cl_team.h"
#include "cl_sound.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_view.h"
#include "renderer/r_mesh_anim.h"
#include "menu/m_popup.h"
#include "../common/routing.h"
#include "cl_ugv.h"
#include "menu/node/m_node_container.h"

/* public */
le_t *selActor;
static const fireDef_t *selFD;
static character_t *selChr;
static int selToHit;
static pos3_t truePos; /**< The cell at the current worldlevel under the mouse cursor. */
static pos3_t mousePos; /**< The cell that an actor will move to when directed to move. */
static qboolean popupReload = qfalse;

enum {
	REMAINING_TU_RELOAD_RIGHT,
	REMAINING_TU_RELOAD_LEFT,
	REMAINING_TU_CROUCH,

	REMAINING_TU_MAX
};

static qboolean displayRemainingTus[REMAINING_TU_MAX];	/**< 0=reload_r, 1=reload_l, 2=crouch */

/**
 * @brief If you want to change the z level of targetting and shooting,
 * use this value. Negative and positive offsets are possible
 * @sa CL_ActorTargetAlign_f
 * @sa G_ClientShoot
 * @sa G_ShootGrenade
 * @sa G_ShootSingle
 */
static int mousePosTargettingAlign = 0;

/**
 * @brief The TUs that the current selected actor needs to walk to the
 * current grid position marked by the mouse cursor (mousePos)
 * @sa CL_MoveLength
 */
int actorMoveLength;

invList_t invList[MAX_INVLIST];
static qboolean visible_firemode_list_left = qfalse;
static qboolean visible_firemode_list_right = qfalse;
/** If this is set to qfalse CL_DisplayFiremodes_f will not attempt to hide the list */
static qboolean firemodes_change_display = qtrue;

static le_t *mouseActor;
static pos3_t mouseLastPos;
/** for double-click movement and confirmations ... */
pos3_t mousePendPos;
/** keep track of reaction toggle */
static reactionmode_t selActorReactionState;
/** and another to help set buttons! */
static reactionmode_t selActorOldReactionState = R_FIRE_OFF;

/** to optimise painting of HUD in move-mode, keep track of some globals: */
static le_t *lastHUDActor; /**< keeps track of selActor */
static int lastMoveLength; /**< keeps track of actorMoveLength */
static int lastTU; /**< keeps track of selActor->TU */

/** @brief a cbuf string for each button_types_t */
static const char *shoot_type_strings[BT_NUM_TYPES] = {
	"pr\n",
	"reaction\n",
	"pl\n",
	"rr\n",
	"rl\n",
	"stand\n",
	"crouch\n",
	"headgear\n"
};

/** Reservation-popup info */
static int popupNum;	/**< Number of entries in the popup list */

/**
 * @brief Defines the various states of a button.
 * @note Not all buttons do have all of these states (e.g. "unusable" is not very common).
 * @todo What does -1 mean here? it is used quite a bit
 */
typedef enum {
	BT_STATE_DISABLE,		/**< 'Disabled' display (grey) */
	BT_STATE_DESELECT,		/**< Normal display (blue) */
	BT_STATE_HIGHLIGHT,		/**< Normal + highlight (blue + glow)*/
	BT_STATE_UNUSABLE		/**< Normal + red (activated but unusable aka "impossible") */
} weaponButtonState_t;

static weaponButtonState_t weaponButtonState[BT_NUM_TYPES];

static void CL_RefreshWeaponButtons(int time);
/**
 * @brief Writes player action with its data
 * @param[in] player_action
 * @param[in] entnum The server side edict number of the actor
 */
void MSG_Write_PA (player_action_t player_action, int entnum, ...)
{
	va_list ap;
	struct dbuffer *msg = new_dbuffer();

	if (blockEvents)
		Com_Printf("still some pending events but starting %i (%s)\n",
			player_action, pa_format[player_action]);

	va_start(ap, entnum);
	NET_WriteFormat(msg, "bbs", clc_action, player_action, entnum);
	NET_vWriteFormat(msg, pa_format[player_action], ap);
	va_end(ap);
	NET_WriteMsg(cls.netStream, msg);
}

/*
==============================================================
ACTOR MENU UPDATING
==============================================================
*/

/**
 * @brief Return the skill string for the given skill level
 * @return skill string
 * @param[in] skill a skill value between 0 and MAX_SKILL (@todo 0? never reached?)
 */
const char *CL_GetSkillString (const int skill)
{
	const int skillLevel = skill * 10 / MAX_SKILL;
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value (%i/%i)\n", skill, MAX_SKILL);
	}
#endif
	switch (skillLevel) {
	case 0:
		return _("Poor");
	case 1:
		return _("Mediocre");
	case 2:
		return _("Average");
	case 3:
		return _("Competent");
	case 4:
		return _("Proficient");
	case 5:
		return _("Very Good");
	case 6:
		return _("Highly Proficient");
	case 7:
		return _("Excellent");
	case 8:
		return _("Outstanding");
	case 9:
	case 10:
		return _("Superhuman");
	default:
		Com_Printf("CL_GetSkillString: Unknown skill: %i (index: %i)\n", skill, skillLevel);
		return "";
	}
}

/** @sa moveModeDescriptions */
typedef enum {
	WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH,
	WALKTYPE_AUTOSTAND_BEING_USED,
	WALKTYPE_WALKING,
	WALKTYPE_CROUCH_WALKING,

	WALKTYPE_MAX
} walkType_t;

/** @note Order of elements here must correspond to order of elements in walkType_t. */
static const char *moveModeDescriptions[] = {
	N_("Crouch walk"),
	N_("Autostand"),
	N_("Walk"),
	N_("Crouch walk")
};
CASSERT(lengthof(moveModeDescriptions) == WALKTYPE_MAX);

/**
 * @brief Decide how the actor will walk, taking into account autostanding.
 * @param[in] length The distance to move: units are TU required assuming actor is standing.
 */
static int CL_MoveMode (int length)
{
	assert(selActor);
	if (selActor->state & STATE_CROUCHED) {
		if (cl_autostand->integer) { /* Is the player using autostand? */
			if ((float)(2 * TU_CROUCH) < (float)length * (TU_CROUCH_MOVING_FACTOR - 1.0f)) {
				return WALKTYPE_AUTOSTAND_BEING_USED;
			} else {
				return WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH;
			}
		} else {
			return WALKTYPE_CROUCH_WALKING;
		}
	} else {
		return WALKTYPE_WALKING;
	}
}

/**
 * @brief Updates the character cvars for the given character.
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param chr Pointer to character_t (may not be null)
 * @sa CL_UGVCvars
 * @sa CL_ActorSelect
 */
void CL_CharacterCvars (const character_t * chr)
{
	assert(chr);

	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ALIENS]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));

	/* Display rank if not in multiplayer (numRanks==0) and the character has one. */
	if (chr->score.rank >= 0 && gd.numRanks) {
		char buf[MAX_VAR];
		Com_sprintf(buf, sizeof(buf), _("Rank: %s"), _(gd.ranks[chr->score.rank].name));
		Cvar_Set("mn_chrrank", buf);
		Cvar_Set("mn_chrrank_img", gd.ranks[chr->score.rank].image);
	} else {
		Cvar_Set("mn_chrrank", "");
		Cvar_Set("mn_chrrank_img", "");
	}

	Cvar_Set("mn_vpwr", va("%i", chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", va("%i", chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_vcls", va("%i", chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vhp", va("%i", chr->HP));
	Cvar_Set("mn_vhpmax", va("%i", chr->maxHP));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_POWER]), chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_SPEED]), chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_ACCURACY]), chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_MIND]), chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_CLOSE]), chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_HEAVY]), chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_ASSAULT]), chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_SNIPER]), chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_EXPLOSIVE]), chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_thp", va("%i (%i)", chr->HP, chr->maxHP));
}

/**
 * @brief Updates the global character cvars for battlescape.
 * @note This is only called when we are in battlescape rendering mode
 * It's assumed that every living actor - @c le_t - has a character assigned, too
 */
static void CL_ActorGlobalCvars (void)
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
 * @sa CL_RefreshWeaponButtons
 * @sa CL_ActorUpdateCVars
 * @sa CL_ActorSelect
 */
static int CL_GetReactionState (const le_t * le)
{
	if (le->state & STATE_REACTION_MANY)
		return R_FIRE_MANY;
	else if (le->state & STATE_REACTION_ONCE)
		return R_FIRE_ONCE;
	else
		return R_FIRE_OFF;
}

/**
 * @brief Calculate total reload time for selected actor.
 * @param[in] weapon Item in (currently only right) hand.
 * @return Time needed to reload or >= 999 if no suitable ammo found.
 * @note This routine assumes the time to reload a weapon
 * @note in the right hand is the same as the left hand.
 * @todo Distinguish between LEFT(selActor) and RIGHT(selActor).
 * @sa CL_RefreshWeaponButtons
 * @sa CL_CheckMenuAction
 */
static int CL_CalcReloadTime (const objDef_t *weapon)
{
	int container;
	int tu = 999;

	if (!selActor)
		return tu;

	if (!weapon)
		return tu;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			const invList_t *ic;
			for (ic = selActor->i.c[container]; ic; ic = ic->next)
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

void CL_ResetWeaponButtons (void)
{
	memset(weaponButtonState, -1, sizeof(weaponButtonState));
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 */
static void SetWeaponButton (int button, weaponButtonState_t state)
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
 * @brief Returns the number of the actor in the teamlist.
 * @param[in] le The actor to search.
 * @return The number of the actor in the teamlist.
 */
static int CL_GetActorNumber (const le_t * le)
{
	int actorIdx;

	assert(le);

	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		if (cl.teamList[actorIdx] == le)
			return actorIdx;
	}
	return -1;
}

/**
 * @brief Returns the character information for an actor in the teamlist.
 * @param[in] le The actor to search.
 * @return A pointer to a character struct.
 * @todo We really needs a better way to sync this up.
 */
character_t *CL_GetActorChr (const le_t * le)
{
	const int actorIdx = CL_GetActorNumber(le);
	if (actorIdx < 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GetActorChr: BAD ACTOR INDEX!\n");
		return NULL;
	}

	return cl.chrList.chr[actorIdx];
}

/**
 * @brief Makes all entries of the firemode lists invisible.
 */
static void HideFiremodes (void)
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
 * @brief Returns the weapon its ammo and the firemodes-index inside the ammo for a given hand.
 * @param[in] actor The pointer to the actor we want to get the data from.
 * @param[in] hand Which weapon(-hand) to use [l|r].
 * @param[out] weapon The weapon in the hand.
 * @param[out] ammo The ammo used in the weapon (is the same as weapon for grenades and similar).
 * @param[out] weapFdsIdx weapon_mod index in the ammo for the weapon (objDef.fd[x][])
 */
static void CL_GetWeaponAndAmmo (const le_t * actor, char hand, objDef_t **weapon, objDef_t **ammo, int *weapFdsIdx)
{
	const invList_t *invlistWeapon;
	objDef_t *item, *itemAmmo;

	assert(weapFdsIdx);

	if (!actor)
		return;

	if (hand == ACTOR_HAND_CHAR_RIGHT)
		invlistWeapon = RIGHT(actor);
	else
		invlistWeapon = LEFT(actor);

	if (!invlistWeapon || !invlistWeapon->item.t)
		return;

	item = invlistWeapon->item.t;

	if (item->numWeapons) /** @todo "|| invlist_weapon->item.m == NONE" ... actually what does a negative number for ammo mean? */
		itemAmmo = item; /* This weapon doesn't need ammo it already has firedefs */
	else
		itemAmmo = invlistWeapon->item.m;

	*weapFdsIdx = FIRESH_FiredefsIDXForWeapon(itemAmmo, invlistWeapon->item.t);
	if (ammo)
		*ammo = itemAmmo;
	if (weapon)
		*weapon = item;
}

#ifdef DEBUG
/**
 * @brief Prints all reaction- and reservation-info for the team.
 * @note Console command: debug_listreservations
 */
void CL_ListReactionAndReservations_f (void)
{
	int actorIdx;

	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		if (cl.teamList[actorIdx]) {
			const character_t *chr = CL_GetActorChr(cl.teamList[actorIdx]);
			Com_Printf("%s\n", chr->name);
			Com_Printf(" - hand: %i | fm: %i | wpidx: %i\n", chr->RFmode.hand, chr->RFmode.fmIdx,chr->RFmode.wpIdx);
			Com_Printf(" - res... reaction: %i | crouch: %i\n", chr->reservedTus.reaction, chr->reservedTus.crouch);
		}
	}
}
#endif

/**
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weaponIndex Store the weapon-idx of the object in the hand (for faster access).
 */
void CL_CharacterSetRFMode (character_t *chr, int hand, int fireModeIndex, int weaponIndex)
{
	chr->RFmode.hand = hand;
	chr->RFmode.fmIdx = fireModeIndex;
	chr->RFmode.wpIdx = weaponIndex;
}

/**
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weaponIndex Store the weapon-idx of the object in the hand (for faster access).
 */
void CL_CharacterSetShotSettings (character_t *chr, int hand, int fireModeIndex, int weaponIndex)
{
	chr->reservedTus.shotSettings.hand = hand;
	chr->reservedTus.shotSettings.fmIdx = fireModeIndex;
	chr->reservedTus.shotSettings.wpIdx = weaponIndex;
}

/**
 * @brief Checks if the currently selected firemode is useable with the defined weapon.
 * @param[in] actor The actor to check the firemode for.
 * @param[in] reaction Use qtrue to check chr->RFmode or qfalse to check chr->reservedTus->shotSettings
 * @return qtrue if nothing has to be done.
 * @return qfalse if settings are outdated.
 */
qboolean CL_WorkingFiremode (const le_t * actor, qboolean reaction)
{
	const character_t *chr;
	const chrFiremodeSettings_t *fmSettings;
	objDef_t *ammo;
	int weapFdsIdx = -1;

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_WorkingFiremode: No actor given! Abort.\n");
		return qtrue;
	}

	chr = CL_GetActorChr(actor);
	if (!chr) {
		Com_DPrintf(DEBUG_CLIENT, "CL_WorkingFiremode: No character found! Abort.\n");
		return qtrue;
	}

	if (reaction)
		fmSettings = &chr->RFmode;
	else
		fmSettings = &chr->reservedTus.shotSettings;

	if (!SANE_FIREMODE(fmSettings)) {
		/* Settings out of range or otherwise invalid - update needed. */
		return qfalse;
	}

	/** @todo Get weapon & firedef and compare with settings. */
	/* Get 'ammo' (of weapon in defined hand) and index of firedefinitions in 'ammo'. */
	CL_GetWeaponAndAmmo(actor, ACTOR_GET_HAND_INDEX(fmSettings->hand), NULL, &ammo, &weapFdsIdx);

	if (!ammo || weapFdsIdx < 0) {
		/* No weapon&ammo found or bad index returned. Stored settings possibly bad. */
		return qfalse;
	}

	if (ammo->weapons[weapFdsIdx]->idx == fmSettings->wpIdx && fmSettings->fmIdx >= 0
	 && fmSettings->fmIdx < ammo->numFiredefs[weapFdsIdx]) {
		/* Stored firemode settings up to date - nothing has to be changed */
		return qtrue;
	}

	/* Return "settings unusable" */
	return qfalse;
}

/**
 * @brief Returns the amount of reserved TUs for a certain type.
 * @param[in] le The actor to check.
 * @param[in] type The type to check. Use RES_ALL_ACTIVE to get all reserved TUs that are not "active" (e.g. RF is skipped if disabled). RES_ALL returns ALL of them, no matter what. See reservation_types_t for a list of options.
 * @return The reserved TUs for the given type.
 * @return -1 on error.
 */
int CL_ReservedTUs (const le_t * le, const reservation_types_t type)
{
	character_t *chr;
	int reservedReaction, reservedCrouch, reservedShot;

	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: No le_t given.\n");
		return -1;
	}

	chr = CL_GetActorChr(le);
	if (!chr) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: No character found for le.\n");
		return -1;
	}

	reservedReaction = max(0, chr->reservedTus.reaction);
	reservedCrouch = max(0, chr->reservedTus.crouch);
	reservedShot = max(0, chr->reservedTus.shot);

	switch (type) {
	case RES_ALL:
		/* A summary of ALL TUs that are reserved. */
		return reservedReaction + reservedCrouch + reservedShot;
	case RES_ALL_ACTIVE: {
		/* A summary of ALL TUs that are reserved depending on their "status". */
		const int crouch = (chr->reservedTus.reserveCrouch) ? reservedCrouch : 0;
		/** @todo reserveReaction is not yet correct on the client side - at least not tested. */
		/* Only use reaction-value if we are have RF activated. */
		if ((le->state & STATE_REACTION))
			return reservedReaction + reservedShot + crouch;
		else
			return reservedShot + crouch;
	}
	case RES_REACTION:
		return reservedReaction;
	case RES_CROUCH:
		return reservedCrouch;
	case RES_SHOT:
		return reservedShot;
	default:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: Bad type given: %i\n", type);
		return -1;
	}
}

/**
 * @brief Returns the amount of usable (overall-reserved) TUs for this actor.
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 */
int CL_UsableTUs (const le_t * le)
{
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UsableTUs: No le_t given.\n");
		return -1;
	}

	return le->TU - CL_ReservedTUs(le, RES_ALL_ACTIVE);
}

/**
 * @brief Returns the amount of usable "reaction fire" TUs for this actor (depends on active/inactive RF)
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 * @todo Maybe only return "reaction" value if reaction-state is active? The value _should_ be 0, but one never knows :)
 */
static int CL_UsableReactionTUs (const le_t * le)
{
	/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) */
	if (le->state & STATE_REACTION)
		/* CL_UsableTUs DOES NOT return the stored value for "reaction" here. */
		return CL_UsableTUs(le) + CL_ReservedTUs(le, RES_REACTION);
	else
		/* CL_UsableTUs DOES return the stored value for "reaction" here. */
		return CL_UsableTUs(le);
}


/**
 * @brief Replace the reserved TUs for a certain type.
 * @param[in] le The actor to change it for.
 * @param[in] type The reservation type to be changed (i.e be replaced).
 * @param[in] tus How many TUs to set.
 * @todo Make the "type" into enum
 */
void CL_ReserveTUs (const le_t * le, const reservation_types_t type, const int tus)
{
	character_t *chr;

	if (!le || tus < 0) {
		return;
	}

	chr = CL_GetActorChr(le);
	if (!chr)
		return;

	Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: Debug: Reservation type=%i, TUs=%i\n", type, tus);

	switch (type) {
	case RES_ALL:
	case RES_ALL_ACTIVE:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: RES_ALL and RES_ALL_ACTIVE are not valid options.\n");
		return;
	case RES_REACTION:
		chr->reservedTus.reaction = tus;
		return;
	case RES_CROUCH:
		chr->reservedTus.crouch = tus;
		return;
	case RES_SHOT:
		chr->reservedTus.shot = tus;
		return;
	default:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: Bad reservation type given: %i\n", type);
		return;
	}
}

/**
 * @brief Stores the given firedef index and object index for reaction fire and sends in over the network as well.
 * @param[in] actor The actor to update the firemode for.
 * @param[in] handidx Index of hand with item, which will be used for reactionfiR_ Possible hand indices: 0=right, 1=right, -1=undef
 * @param[in] fdIdx Index of firedefinition for an item in given hand.
 */
void CL_SetReactionFiremode (le_t * actor, const int handidx, const int objIdx, const int fdIdx)
{
	character_t *chr;
	int usableTusForRF = 0;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		/* This check is just here (additional to the one in CL_DisplayFiremodes_f) in case a possible situation was missed. */
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Function called on enemy/other turn.\n");
		return;
	}

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: No actor given! Abort.\n");
		return;
	}

	usableTusForRF = CL_UsableReactionTUs(actor);

	if (handidx < -1 || handidx > 1) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Bad hand index given. Abort.\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: actor:%i entnum:%i hand:%i fd:%i\n",
		CL_GetActorNumber(actor), actor->entnum, handidx, fdIdx);

	chr = CL_GetActorChr(actor);

	/* Store TUs needed by the selected firemode (if reaction-fire is enabled). Otherwise set it to 0. */
	if (objIdx >= 0 && fdIdx >= 0) {
		objDef_t *ammo = NULL;
		const fireDef_t *fd;
		int weapFdsIdx = -1;

		/* Get 'ammo' (of weapon in defined hand) and index of firedefinitions in 'ammo'. */
		CL_GetWeaponAndAmmo(actor, ACTOR_GET_HAND_INDEX(handidx), NULL, &ammo, &weapFdsIdx);

		/* Get The firedefinition of the selected firemode */
		fd = &ammo->fd[weapFdsIdx][fdIdx];

		/* Reserve the TUs needed by the selected firemode (defined in the ammo). */
		if (fd) {
			if (chr->reservedTus.reserveReaction == STATE_REACTION_MANY) {
				Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Reserving %i x %i = %i TUs for RF.\n",
					usableTusForRF / fd->time, fd->time, fd->time * (usableTusForRF / fd->time));
				CL_ReserveTUs(actor, RES_REACTION, fd->time * (usableTusForRF / fd->time));
			} else {
				Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Reserving %i TUs for RF.\n", fd->time);
				CL_ReserveTUs(actor, RES_REACTION, fd->time);
			}
		} else {
			Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: No firedef found! No TUs will be reserved.\n");
		}
	}

	CL_CharacterSetRFMode(chr, handidx, fdIdx, objIdx);
	/* Send RFmode[] to server-side storage as well. See g_local.h for more. */
	MSG_Write_PA(PA_REACT_SELECT, actor->entnum, handidx, fdIdx, objIdx);
	/* Update server-side settings */
	MSG_Write_PA(PA_RESERVE_STATE, actor->entnum, RES_REACTION, chr->reservedTus.reserveReaction, chr->reservedTus.reaction);
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param[in] fd Pointer to the firedefinition/firemode to be displayed.
 * @param[in] hand What list to display: 'l' for left hand list, 'r' for right hand list.
 * @param[in] status Display the firemode clickable/active (1) or inactive (0).
 */
static void CL_DisplayFiremodeEntry (const fireDef_t * fd, const char hand, const qboolean status)
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
 * @sa CL_RefreshWeaponButtons
 * @sa CL_PopupFiremodeReservation_f
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
 * @sa CL_PopupFiremodeReservation_f
 * @sa CL_CheckFiremodeReservation
 */
static void CL_PopupFiremodeReservation (qboolean reset)
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
		CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
		return;
	}

	/** @todo Why not using the MN_MenuTextReset function here? */
	LIST_Delete(&popupListText);
	/* also reset mn.menuTextLinkedList here - otherwise the
	 * pointer is no longer valid (because the list was freed) */
	mn.menuTextLinkedList[TEXT_LIST] = NULL;

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

	Com_DPrintf(DEBUG_CLIENT, "CL_PopupFiremodeReservation\n");

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

					Com_DPrintf(DEBUG_CLIENT, "CL_PopupFiremodeReservation: hand %i, fm %i, wp %i -- hand %i, fm %i, wp %i\n",
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
 * @sa CL_PopupFiremodeReservation
 */
void CL_PopupFiremodeReservation_f (void)
{
	/* A second parameter (the value itself will be ignored) was given.
	 * This is used to reset the shot-reservation.*/
	if (Cmd_Argc() == 2)
		CL_PopupFiremodeReservation(qtrue);
	else
		CL_PopupFiremodeReservation(qfalse);
}

/**
 * @brief Get selected firemode in the list of the currently selected actor.
 * @sa CL_PopupFiremodeReservation_f
 * @note console command: cl_input.c:reserve_shot
 */
void CL_ReserveShot_f (void)
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
	Com_DPrintf(DEBUG_CLIENT, "CL_ReserveShot_f (popupNum %i, selectedPopupIndex %i)\n", popupNum, selectedPopupIndex);
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
	Com_DPrintf(DEBUG_CLIENT, "CL_ReserveShot_f: popup-index: %i TUs:%i (%i + %i)\n", selectedPopupIndex, TUs, CL_UsableTUs(selActor), CL_ReservedTUs(selActor, RES_SHOT));
	Com_DPrintf(DEBUG_CLIENT, "CL_ReserveShot_f: hand %i, fmIdx %i, weapIdx %i\n", hand,fmIdx,wpIdx);
	if ((CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT)) >= TUs) {
		CL_ReserveTUs(selActor, RES_SHOT, TUs);
		CL_CharacterSetShotSettings(selChr, hand, fmIdx, wpIdx);
		MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, 0, selChr->reservedTus.shot); /* Update server-side settings */
		/** @todo
		MSG_Write_PA(PA_RESERVE_SHOT_FM, selActor->entnum, hand, fmIdx, weapIdx);
		*/
		if (popupListNode) {
			Com_DPrintf(DEBUG_CLIENT, "CL_ReserveShot_f: Setting currently selected line (from %i) to %i.\n", popupListNode->u.text.textLineSelected, selectedPopupIndex);
			MN_TextNodeSelectLine(popupListNode, selectedPopupIndex);
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_ReserveShot_f: can not select entry %i. It needs %i TUs, we have %i.\n", selectedPopupIndex, TUs, CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_SHOT));
	}

	/* Refresh button and tooltip. */
	CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
}

/**
 * @brief Checks if there is a weapon in the hand that can be used for reaction fire.
 * @param[in] actor What actor to check.
 * @param[in] hand Which hand to check: 'l' for left hand, 'r' for right hand.
 */
static qboolean CL_WeaponWithReaction (const le_t * actor, const char hand)
{
	objDef_t *ammo = NULL;
	objDef_t *weapon = NULL;
	int weapFdsIdx = -1;
	int i;

	/* Get ammo and weapon-index in ammo (if there is a weapon in that hand). */
	CL_GetWeaponAndAmmo(actor, hand, &weapon, &ammo, &weapFdsIdx);

	if (weapFdsIdx == -1 || !weapon || !ammo)
		return qfalse;

	/* Check ammo for reaction-enabled firemodes. */
	for (i = 0; i < ammo->numFiredefs[weapFdsIdx]; i++) {
		if (ammo->fd[weapFdsIdx][i].reaction) {
			return qtrue;
		}
	}

	return qfalse;
}

/**
 * @brief Display 'usable" (blue) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 */
static void CL_DisplayPossibleReaction (const le_t * actor)
{
	if (!actor)
		return;

	if (actor != selActor) {
		/* Given actor does not equal the currently selected actor. This normally only happens on game-start. */
		return;
	}

	/* Display 'usable" (blue) reaction buttons
	 * Code also used in CL_ActorToggleReaction_f */
	switch (CL_GetReactionState(actor)) {
	case R_FIRE_ONCE:
		MN_ExecuteConfunc("startreactiononce");
		break;
	case R_FIRE_MANY:
		MN_ExecuteConfunc("startreactionmany");
		break;
	default:
		break;
	}
}

/**
 * @brief Display 'impossible" (red) reaction buttons.
 * @param[in] actor the actor to check for his reaction state.
 * @return qtrue if nothing changed message was sent otherwise qfalse.
 */
static qboolean CL_DisplayImpossibleReaction (const le_t * actor)
{
	if (!actor)
		return qfalse;

	if (actor != selActor) {
		/* Given actor does not equal the currently selectd actor. This normally only happens on game-start. */
		return qfalse;
	}

	/* Display 'impossible" (red) reaction buttons */
	switch (CL_GetReactionState(actor)) {
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
 * @brief Updates the information in RFmode for the selected actor with the given data from the parameters.
 * @param[in] actor The actor we want to update the reaction-fire firemode for.
 * @param[in] hand Which weapon(-hand) to use (l|r).
 * @param[in] firemodeActive Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
static void CL_UpdateReactionFiremodes (le_t * actor, const char hand, int firemodeActive)
{
	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weapFdsIdx = -1;
	int i = -1;
	character_t *chr;
	const int handidx = ACTOR_GET_HAND_INDEX(hand);

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: No actor given!\n");
		return;
	}

	CL_GetWeaponAndAmmo(actor, hand, &weapon, &ammo, &weapFdsIdx);

	if (weapFdsIdx == -1 || !weapon) {
		CL_DisplayImpossibleReaction(actor);
		return;
	}

	/* "ammo" is definitly set here - otherwise the check above
	 * would have left this function already. */
	assert(ammo);
	if (!RS_IsResearched_ptr(ammo->weapons[weapFdsIdx]->tech)) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: Weapon '%s' not researched, can't use for reaction fire.\n", ammo->weapons[weapFdsIdx]->id);
		return;
	}

	if (firemodeActive >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("CL_UpdateReactionFiremodes: Firemode index to big (%i). Highest possible number is %i.\n", firemodeActive, MAX_FIREDEFS_PER_WEAPON-1);
		return;
	}

	if (firemodeActive < 0) {
		/* Set default reaction firemode for this hand (firemodeActive=-1) */
		i = FIRESH_GetDefaultReactionFire(ammo, weapFdsIdx);

		if (i >= 0) {
			/* Found usable firemode for the weapon in _this_ hand. */
			CL_SetReactionFiremode(actor, handidx, ammo->weapons[weapFdsIdx]->idx, i);

			if (CL_UsableReactionTUs(actor) >= ammo->fd[weapFdsIdx][i].time) {
				/* Display 'usable" (blue) reaction buttons */
				CL_DisplayPossibleReaction(actor);
			} else {
				/* Display "impossible" (red) reaction button. */
				CL_DisplayImpossibleReaction(actor);
			}
		} else {
			/* Weapon in _this_ hand not RF-capable. */
			if (CL_WeaponWithReaction(actor, ACTOR_SWAP_HAND(hand))) {
				/* The _other_ hand has usable firemodes for RF, use it instead. */
				CL_UpdateReactionFiremodes(actor, ACTOR_SWAP_HAND(hand), -1);
			} else {
				/* No RF-capable item in either hand. */

				/* Display "impossible" (red) reaction button. */
				CL_DisplayImpossibleReaction(actor);
				/* Set RF-mode info to undef. */
				CL_SetReactionFiremode(actor, -1, NONE, -1);
				CL_ReserveTUs(actor, RES_REACTION, 0);
			}
		}
		/* The rest of this function assumes that firemodeActive is bigger than -1 -> finish. */
		return;
	}

	chr = CL_GetActorChr(actor);
	assert(chr);

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: act%s handidx%i weapfdidx%i\n", chr->name, handidx, weapFdsIdx);

	if (chr->RFmode.wpIdx == ammo->weapons[weapFdsIdx]->idx
	 && chr->RFmode.hand == handidx) {
		if (ammo->fd[weapFdsIdx][firemodeActive].reaction) {
			if (chr->RFmode.fmIdx == firemodeActive)
				/* Weapon is the same, firemode is already selected and reaction-usable. Nothing to do. */
				return;
		} else {
			/* Weapon is the same and firemode is not reaction-usable*/
			return;
		}
	}

	/* Search for a (reaction) firemode with the given index and store/send it. */
	/* CL_SetReactionFiremode(actor, -1, NONE, -1); */
	for (i = 0; i < ammo->numFiredefs[weapFdsIdx]; i++) {
		if (ammo->fd[weapFdsIdx][i].reaction) {
			if (i == firemodeActive) {
				/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) and abort if no use*/
				if (CL_UsableReactionTUs(actor) >= ammo->fd[weapFdsIdx][firemodeActive].time) {
					CL_SetReactionFiremode(actor, handidx, ammo->weapons[weapFdsIdx]->idx, i);
				}
				return;
			}
		}
	}
}

/**
 * @brief Sets the reaction-firemode of an actor/soldier to it's default value on client- and server-side.
 * @param[in] actor The actor to set the firemode for.
 * @param[in] hand Which weapon(-hand) to try first for reaction-firemode (r|l).
 */
void CL_SetDefaultReactionFiremode (le_t *actor, const char hand)
{
	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetDefaultReactionFiremode: No actor given! Abort.\n");
		return;
	}

	if (hand == ACTOR_HAND_CHAR_RIGHT) {
		/* Set default firemode (try right hand first, then left hand). */
		/* Try to set right hand */
		CL_UpdateReactionFiremodes(actor, ACTOR_HAND_CHAR_RIGHT, -1);
		if (!CL_WorkingFiremode(actor, qtrue)) {
			/* If that failed try to set left hand. */
			CL_UpdateReactionFiremodes(actor, ACTOR_HAND_CHAR_LEFT, -1);
		}
	} else {
		/* Set default firemode (try left hand first, then right hand). */
		/* Try to set left hand. */
		CL_UpdateReactionFiremodes(actor, ACTOR_HAND_CHAR_LEFT, -1);
		if (!CL_WorkingFiremode(actor, qtrue)) {
			/* If that failed try to set right hand. */
			CL_UpdateReactionFiremodes(actor, ACTOR_HAND_CHAR_RIGHT, -1);
		}
	}
}

/**
 * @brief Displays the firemodes for the given hand.
 * @note Console command: list_firemodes
 */
void CL_DisplayFiremodes_f (void)
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
		HideFiremodes();
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

	Com_DPrintf(DEBUG_CLIENT, "CL_DisplayFiremodes_f: %s | %s | %i\n", weapon->name, ammo->name, weapFdsIdx);

	if (!weapon || !ammo) {
		Com_DPrintf(DEBUG_CLIENT, "CL_DisplayFiremodes_f: no weapon or ammo found.\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_DisplayFiremodes_f: displaying %c firemodes.\n", hand);

	if (firemodes_change_display) {
		/* Toggle firemode lists if needed. */
		HideFiremodes();
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
	Com_DPrintf(DEBUG_CLIENT, "CL_DisplayFiremodes_f: actor index: %i\n", actorIdx);
	if (actorIdx == -1)
		Com_Error(ERR_DROP, "Could not get current selected actor's id");

	selChr = CL_GetActorChr(selActor);
	assert(selChr);

	/* Set default firemode if the currenttly seleced one is not sane or for another weapon. */
	if (!CL_WorkingFiremode(selActor, qtrue)) {	/* No usable firemode selected. */
		/** @todo TYPOS IN COMMENTS - I don't even understand what they are trying to explain here */
		/* Make sure we use the same hand if it's defined */
		if (selChr->RFmode.hand != 1) { /* No the left hand defined */
			/* Set default firemode (try right hand first, then left hand). */
			CL_SetDefaultReactionFiremode(selActor, ACTOR_HAND_CHAR_RIGHT);
		} else  if (selChr->RFmode.hand == 1) {
			/* Set default firemode (try left hand first, then right hand). */
			CL_SetDefaultReactionFiremode(selActor, ACTOR_HAND_CHAR_LEFT);
		}
	}

	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		if (i < ammo->numFiredefs[weapFdsIdx]) { /* We have a defined fd ... */
			/* Display the firemode information (image + text). */
			if (ammo->fd[weapFdsIdx][i].time <= CL_UsableTUs(selActor)) {	/* Enough timeunits for this firemode?*/
				CL_DisplayFiremodeEntry(&ammo->fd[weapFdsIdx][i], hand, qtrue);
			} else{
				CL_DisplayFiremodeEntry(&ammo->fd[weapFdsIdx][i], hand, qfalse);
			}

			/* Display checkbox for reaction firemode (this needs a sane reactionFiremode array) */
			if (ammo->fd[weapFdsIdx][i].reaction) {
				Com_DPrintf(DEBUG_CLIENT, "CL_DisplayFiremodes_f: This is a reaction firemode: %i\n", i);
				if (hand == ACTOR_HAND_CHAR_RIGHT) {
					if (THIS_FIREMODE(&selChr->RFmode, 0, i)) {
						/* Set this checkbox visible+active. */
						MN_ExecuteConfunc("set_right_cb_a %i", i);
					} else {
						/* Set this checkbox visible+inactive. */
						MN_ExecuteConfunc("set_right_cb_ina %i", i);
					}
				} else { /* hand[0] == ACTOR_HAND_CHAR_LEFT */
					if (THIS_FIREMODE(&selChr->RFmode, 1, i)) {
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
void CL_SwitchFiremodeList_f (void)
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
void CL_SelectReactionFiremode_f (void)
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
		Com_Printf("CL_SelectReactionFiremode_f: Firemode index to big (%i). Highest possible number is %i.\n",
			firemode, MAX_FIREDEFS_PER_WEAPON - 1);
		return;
	}

	CL_UpdateReactionFiremodes(selActor, hand, firemode);

	/* Update display of firemode checkbuttons. */
	firemodes_change_display = qfalse; /* So CL_DisplayFiremodes_f doesn't hide the list. */
	CL_DisplayFiremodes_f();
}


/**
 * @brief Starts aiming/target mode for selected left/right firemode.
 * @note Previously know as a combination of CL_FireRightPrimary, CL_FireRightSecondary,
 * @note CL_FireLeftPrimary and CL_FireLeftSecondary.
 */
void CL_FireWeapon_f (void)
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
		Com_Printf("CL_FireWeapon_f: Firemode index to big (%i). Highest possible number is %i.\n",
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
		HideFiremodes();
	} else {
		/* Cannot shoot because of not enough TUs - every other
		 * case should be checked previously in this function. */
		SCR_DisplayHudMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
		Com_DPrintf(DEBUG_CLIENT, "CL_FireWeapon_f: Firemode not available (%c, %s).\n", hand, ammo->fd[weapFdsIdx][firemode].name);
		return;
	}
}

/**
 * @brief Remember if we hover over a button that would cost some TUs when pressed.
 * @note tis is used in CL_ActorUpdateCvars to update the "remaining TUs" bar correctly.
 * @note commandline:remaining_tus
 */
void CL_RemainingTus_f (void)
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
	CL_ActorUpdateCvars();
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] time The amount of TU (of an actor) left in case of action.
 * @sa CL_ActorUpdateCvars
 */
static void CL_RefreshWeaponButtons (int time)
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
		weaponButtonState[BT_STAND] = -1;
		if ((time + CL_ReservedTUs(selActor, RES_CROUCH)) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for standing up."));
			SetWeaponButton(BT_CROUCH, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Stand up (%i TU)"), TU_CROUCH));
			SetWeaponButton(BT_CROUCH, BT_STATE_DESELECT);
		}
	} else {
		weaponButtonState[BT_CROUCH] = -1;
		if ((time + CL_ReservedTUs(selActor, RES_CROUCH)) < TU_CROUCH) {
			Cvar_Set("mn_crouchstand_tt", _("Not enough TUs for crouching."));
			SetWeaponButton(BT_STAND, BT_STATE_DISABLE);
		} else {
			Cvar_Set("mn_crouchstand_tt", va(_("Crouch (%i TU)"), TU_CROUCH));
			SetWeaponButton(BT_STAND, BT_STATE_DESELECT);
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
			SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
		} else {
			SetWeaponButton(BT_HEADGEAR, BT_STATE_DESELECT);
		}
	} else {
		SetWeaponButton(BT_HEADGEAR, BT_STATE_DISABLE);
	}

	/* reaction-fire button */
	if (CL_GetReactionState(selActor) == R_FIRE_OFF) {
		if ((time >= CL_ReservedTUs(selActor, RES_REACTION))
		 && (CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_LEFT)))
			SetWeaponButton(BT_REACTION, BT_STATE_DESELECT);
		else
			SetWeaponButton(BT_REACTION, BT_STATE_DISABLE);

	} else {
		if ((CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_RIGHT) || CL_WeaponWithReaction(selActor, ACTOR_HAND_CHAR_LEFT))) {
			CL_DisplayPossibleReaction(selActor);
		} else {
			CL_DisplayImpossibleReaction(selActor);
		}
	}

	/** Reload buttons @sa CL_ActorUpdateCvars */
	if (weaponr)
		reloadtime = CL_CalcReloadTime(weaponr->item.t);
	if (!weaponr || !weaponr->item.m || !weaponr->item.t->reload || time < reloadtime) {
		SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DISABLE);
		Cvar_Set("mn_reloadright_tt", _("No reload possible for right hand."));
	} else {
		SetWeaponButton(BT_RIGHT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadright_tt", va(_("Reload weapon (%i TU)."), reloadtime));
	}

	if (weaponl)
		reloadtime = CL_CalcReloadTime(weaponl->item.t);
	if (!weaponl || !weaponl->item.m || !weaponl->item.t->reload || time < reloadtime) {
		SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DISABLE);
		Cvar_Set("mn_reloadleft_tt", _("No reload possible for left hand."));
	} else {
		SetWeaponButton(BT_LEFT_RELOAD, BT_STATE_DESELECT);
		Cvar_Set("mn_reloadleft_tt", va(_("Reload weapon (%i TU)."), reloadtime));
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
			SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
		else
			SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DESELECT);
	} else {
		SetWeaponButton(BT_RIGHT_FIRE, BT_STATE_DISABLE);
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
			SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
		else
			SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DESELECT);
	} else {
		SetWeaponButton(BT_LEFT_FIRE, BT_STATE_DISABLE);
	}

	/* Check if the firemode reservation popup is shown and refresh its content. (i.e. close&open it) */
	{
		const menu_t* menu = MN_GetActiveMenu();
		if (menu)
			Com_DPrintf(DEBUG_CLIENT, "CL_ActorToggleReaction_f: Active menu = %s\n", menu->name);

		if (menu && strstr(menu->name, POPUPLIST_MENU_NAME)) {
			Com_DPrintf(DEBUG_CLIENT, "CL_ActorToggleReaction_f: reload popup\n");

			/* Prevent firemode reservation popup from being closed if
			 * no firemode is available because of insufficient TUs. */
			popupReload = qtrue;

			/* Close and reload firemode reservation popup. */
			MN_PopMenu(qfalse);
			CL_PopupFiremodeReservation(qfalse);
		}
	}
}

/**
 * @brief Checks whether an action on hud menu is valid and displays proper message.
 * @param[in] time The amount of TU (of an actor) left.
 * @param[in] weapon An item in hands.
 * @param[in] mode EV_INV_AMMO in case of fire button, EV_INV_RELOAD in case of reload button
 * @return qfalse when action is not possible, otherwise qtrue
 * @sa CL_FireWeapon_f
 * @sa CL_ReloadLeft_f
 * @sa CL_ReloadRight_f
 * @todo Check for ammo in hand and give correct feedback in all cases.
 */
qboolean CL_CheckMenuAction (int time, invList_t *weapon, int mode)
{
	/* No item in hand. */
	/** @todo Ignore this condition when ammo in hand. */
	if (!weapon || !weapon->item.t) {
		SCR_DisplayHudMessage(_("No item in hand.\n"), 2000);
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
			SCR_DisplayHudMessage(_("Can't perform action:\nout of ammo.\n"), 2000);
			return qfalse;
		}
		/* Cannot shoot because weapon is fireTwoHanded, yet both hands handle items. */
		if (weapon->item.t->fireTwoHanded && LEFT(selActor)) {
			SCR_DisplayHudMessage(_("This weapon cannot be fired\none handed.\n"), 2000);
			return qfalse;
		}
		break;
	case EV_INV_RELOAD:
		/* Check if reload is possible. Also checks for the correct amount of TUs. */

		/* Cannot reload because this item is not reloadable. */
		if (!weapon->item.t->reload) {
			SCR_DisplayHudMessage(_("Can't perform action:\nthis item is not reloadable.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of no ammo in inventory. */
		if (CL_CalcReloadTime(weapon->item.t) >= 999) {
			SCR_DisplayHudMessage(_("Can't perform action:\nammo not available.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of not enough TUs. */
		if (time < CL_CalcReloadTime(weapon->item.t)) {
			SCR_DisplayHudMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
			return qfalse;
		}
		break;
	default:
		break;
	}

	return qtrue;
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
void CL_ActorUpdateCvars (void)
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
		CL_ResetWeaponButtons();
	}

	/* set Cvars for all actors */
	CL_ActorGlobalCvars();

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
						if (selWeapon->item.t->weapon || (selWeapon->item.t->weapons[0] == selWeapon->item.t)) {
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
			/** @sa CL_RefreshWeaponButtons */
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
					CL_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
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
					CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
				}
				lastHUDActor = selActor;
				lastMoveLength = actorMoveLength;
				lastTU = selActor->TU;
				mn.menuText[TEXT_MOUSECURSOR_RIGHT] = mouseText;
			}
			time = actorMoveLength;
		} else {
			MN_MenuTextReset(TEXT_MOUSECURSOR_RIGHT);
			/* in multiplayer RS_ItemIsResearched always returns true,
			 * so we are able to use the aliens weapons */
			if (selWeapon && !RS_IsResearched_ptr(selWeapon->item.t->tech)) {
				SCR_DisplayHudMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"), 2000);
				cl.cmode = M_MOVE;
			} else if (selWeapon && selFD) {
				Com_sprintf(infoText, lengthof(infoText),
							"%s\n%s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(selFD->name), selFD->ammo, selToHit, selFD->time);
				Com_sprintf(mouseText, lengthof(mouseText),
							"%s: %s (%i) [%i%%] %i\n", _(selWeapon->item.t->name), _(selFD->name), selFD->ammo, selToHit, selFD->time);

				mn.menuText[TEXT_MOUSECURSOR_RIGHT] = mouseText;	/* Save the text for later display next to the cursor. */

				time = selFD->time;
				/* if no TUs left for this firing action of if the weapon is reloadable and out of ammo, then change to move mode */
				if (CL_UsableTUs(selActor) < time || (selWeapon->item.t->reload && selWeapon->item.a <= 0)) {
					cl.cmode = M_MOVE;
					CL_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
				}
			} else if (selWeapon) {
				Com_sprintf(infoText, lengthof(infoText), _("%s\n(empty)\n"), _(selWeapon->item.t->name));
			} else {
				cl.cmode = M_MOVE;
				CL_RefreshWeaponButtons(CL_UsableTUs(selActor) - actorMoveLength);
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
		mn.menuText[TEXT_MOUSECURSOR_TOP] = topText;

		/* Display the floor and ceiling values for the current cell. */
		Com_sprintf(bottomText, lengthof(bottomText), "%i-(%i,%i,%i)\n", Grid_Floor(clMap, fieldSize, truePos), mousePos[0], mousePos[1], mousePos[2]);
		/* Save the text for later display next to the cursor. */
		mn.menuText[TEXT_MOUSECURSOR_BOTTOM] = bottomText;

		/* Display the floor and ceiling values for the current cell. */
		dv = Grid_MoveNext(clMap, fieldSize, &clPathMap, mousePos, 0);
		Com_sprintf(leftText, lengthof(leftText), "%i-%i\n", getDVdir(dv), getDVz(dv));
		/* Save the text for later display next to the cursor. */
		mn.menuText[TEXT_MOUSECURSOR_LEFT] = leftText;

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
			selActorReactionState = CL_GetReactionState(selActor);
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
					weaponButtonState[BT_REACTION] = -1;
					break;
				}
			}

			cl.oldstate = selActor->state;
			/** @todo Check if the use of "time" is correct here (e.g. are the reserved TUs ignored here etc...?) */
			if (actorMoveLength < ROUTING_NOT_REACHABLE
				&& (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE))
				CL_RefreshWeaponButtons(time);
			else
				CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
		} else {
			if (refresh)
				MN_ExecuteConfunc("deselstand");

			/* this allows us to display messages even with no actor selected */
			if (cl.time < cl.msgTime) {
				/* special message */
				Q_strncpyz(infoText, cl.msgText, sizeof(infoText));
			}
		}
		mn.menuText[TEXT_STANDARD] = infoText;
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
			CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
	}

	/* player bar */
	if (cl_selected->modified || refresh) {
		int i;

		for (i = 0; i < MAX_TEAMLIST; i++) {
			if (!cl.teamList[i] || LE_IsDead(cl.teamList[i])) {
				MN_ExecuteConfunc("huddisable%i", i);
			} else if (i == cl_selected->integer) {
				/* stored in menu_nohud.ufo - confunc that calls all the different
				 * hud select confuncs */
				MN_ExecuteConfunc("hudselect%i", i);
			} else {
				MN_ExecuteConfunc("huddeselect%i", i);
			}
		}
		cl_selected->modified = qfalse;
	}
}

/*
==============================================================
ACTOR SELECTION AND TEAM LIST
==============================================================
*/

/**
 * @brief Adds the actor the the team list.
 * @sa CL_ActorAppear
 * @sa CL_RemoveActorFromTeamList
 * @param le Pointer to local entity struct
 */
void CL_AddActorToTeamList (le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team || le->pnum != cl.pnum || LE_IsDead(le))
		return;

	/* check list length */
	if (cl.numTeamList >= MAX_TEAMLIST)
		return;

	/* check list for that actor */
	i = CL_GetActorNumber(le);

	/* add it */
	if (i == -1) {
		i = cl.numTeamList;
		cl.teamList[cl.numTeamList++] = le;
		MN_ExecuteConfunc("numonteam%i", cl.numTeamList); /* althud */
		MN_ExecuteConfunc("huddeselect%i", i);
		if (cl.numTeamList == 1)
			CL_ActorSelectList(0);
	}
}


/**
 * @brief Removes an actor from the team list.
 * @sa CL_ActorStateChange
 * @sa CL_AddActorToTeamList
 * @param le Pointer to local entity struct
 */
void CL_RemoveActorFromTeamList (const le_t * le)
{
	int i;

	if (!le)
		return;

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			/* disable hud button */
			MN_ExecuteConfunc("huddisable%i", i);

			/* remove from list */
			cl.teamList[i] = NULL;
			break;
		}
	}

	/* check selection */
	if (selActor == le) {
		CL_ConditionalMoveCalcForCurrentSelectedActor();

		for (i = 0; i < cl.numTeamList; i++) {
			if (CL_ActorSelect(cl.teamList[i]))
				break;
		}

		if (i == cl.numTeamList) {
			selActor->selected = qfalse;
			selActor = NULL;
		}
	}
}

/**
 * @brief Selects an actor.
 * @param le Pointer to local entity struct
 * @sa CL_UGVCvars
 * @sa CL_CharacterCvars
 */
qboolean CL_ActorSelect (le_t * le)
{
	int actorIdx;
	qboolean sameActor = qfalse;

	/* test team */
	if (!le || le->team != cls.team || LE_IsDead(le) || !le->inuse)
		return qfalse;

	if (blockEvents)
		return qfalse;

	/* select him */
	if (selActor)
		selActor->selected = qfalse;
	le->selected = qtrue;

	/* reset the align if we switched the actor */
	if (selActor != le)
		mousePosTargettingAlign = 0;
	else
		sameActor = qtrue;

	selActor = le;
	menuInventory = &selActor->i;
	selActorReactionState = CL_GetReactionState(selActor);

	actorIdx = CL_GetActorNumber(le);
	if (actorIdx < 0)
		return qfalse;

	/* console commands, update cvars */
	Cvar_ForceSet("cl_selected", va("%i", actorIdx));

	selChr = CL_GetActorChr(le);
	assert(selChr);

	/* Right now we can only update this if the selChr is already set. */
	switch (le->fieldSize) {
	case ACTOR_SIZE_NORMAL:
		CL_CharacterCvars(selChr);
		break;
	case ACTOR_SIZE_2x2:
		CL_UGVCvars(selChr);
		break;
	default:
		Com_Error(ERR_DROP, "CL_ActorSelect: Unknown fieldsize");
	}

	/* Forcing the hud-display to refresh its displayed stuff. */
	Cvar_SetValue("hud_refresh", 1);
	CL_ActorUpdateCvars();

	CL_ConditionalMoveCalcForCurrentSelectedActor();

	/* move first person camera to new actor */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_FIRSTPERSON);

	/* Change to move-mode and hide firemodes.
	 * Only if it's a different actor - if it's the same we keep the current mode etc... */
	if (!sameActor) {
		HideFiremodes();
		cl.cmode = M_MOVE;
	}

	return qtrue;
}

/**
 * @brief Selects an actor from a list.
 *
 * This function is used to select an actor from the lists that are
 * used in equipment and team assemble screens
 *
 * @param num The index value from the list of actors
 *
 * @sa CL_ActorSelect
 * @return qtrue if selection was possible otherwise qfalse
 */
qboolean CL_ActorSelectList (int num)
{
	le_t *le;

	/* check if actor exists */
	if (num >= cl.numTeamList || num < 0)
		return qfalse;

	/* select actor */
	le = cl.teamList[num];
	if (!CL_ActorSelect(le))
		return qfalse;

	/* center view (if wanted) */
	if (cl_centerview->integer)
		VectorCopy(le->origin, cl.cam.origin);
	/* change to worldlevel were actor is right now */
	Cvar_SetValue("cl_worldlevel", le->pos[2]);

	return qtrue;
}

/**
 * @brief selects the next actor
 */
qboolean CL_ActorSelectNext (void)
{
	int selIndex = -1;
	int num = cl.numTeamList;
	int i;

	/* find index of currently selected actor */
	for (i = 0; i < num; i++) {
		const le_t *le = cl.teamList[i];
		if (le && le->selected && le->inuse && !LE_IsDead(le)) {
			selIndex = i;
			break;
		}
	}
	if (selIndex < 0)
		return qfalse;			/* no one selected? */

	/* cycle round */
	i = selIndex;
	while (qtrue) {
		i = (i + 1) % num;
		if (i == selIndex)
			break;
		if (CL_ActorSelectList(i))
			return qtrue;
	}
	return qfalse;
}


/*
==============================================================
ACTOR MOVEMENT AND SHOOTING
==============================================================
*/

/**
 * @brief A list of locations that cannot be moved to.
 * @note Pointer to le->pos or edict->pos followed by le->fieldSize or edict->fieldSize
 * @see CL_BuildForbiddenList
 */
pos_t *fb_list[MAX_FORBIDDENLIST];
/**
 * @brief Current length of fb_list.
 * @note all byte pointers in the fb_list list (pos + fieldSize)
 * @see fb_list
 */
int fb_length;

/**
 * @brief Builds a list of locations that cannot be moved to (client side).
 * @sa G_MoveCalc
 * @sa G_BuildForbiddenList <- server side
 * @sa Grid_CheckForbidden
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void CL_BuildForbiddenList (void)
{
	le_t *le;
	int i;

	fb_length = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || le->invis)
			continue;
		/* Dead ugv will stop walking, too. */
		if (le->type == ET_ACTOR2x2 || LE_IsLivingAndVisibleActor(le)) {
			fb_list[fb_length++] = le->pos;
			fb_list[fb_length++] = (byte*)&le->fieldSize;
		}
	}

#ifdef PARANOID
	if (fb_length > MAX_FORBIDDENLIST)
		Com_Error(ERR_DROP, "CL_BuildForbiddenList: list too long");
#endif
}

#ifdef DEBUG
/**
 * @brief Draws a marker for all blocked map-positions.
 * @note currently uses basically the same code as CL_BuildForbiddenList
 * @note usage in console: "debug_drawblocked"
 * @todo currently the particles stay a _very_ long time ... so everybody has to stand still in order for the display to be correct.
 * @sa CL_BuildForbiddenList
 */
void CL_DisplayBlockedPaths_f (void)
{
	le_t *le;
	int i, j;
	ptl_t *ptl;
	vec3_t s;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* draw blocking cursor at le->pos */
			if (!LE_IsDead(le))
				Grid_PosToVec(clMap, le->fieldSize, le->pos, s);
			break;
		case ET_DOOR:
		case ET_BREAKABLE:
		case ET_ROTATING:
			VectorCopy(le->origin, s);
			break;
		default:
			continue;
		}

		ptl = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
		ptl->rounds = 2;
		ptl->roundsCnt = 2;
		ptl->life = 10000;
		ptl->t = 0;
		if (le->fieldSize == ACTOR_SIZE_2x2) {
			/* If this actor blocks 4 fields draw them as well. */
			ptl_t *ptl2;
			vec3_t temp;
			for (j = 0; j < 3; j++) {
				VectorCopy(s, temp);
				temp[j] += UNIT_SIZE;
				ptl2 = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
				ptl2->rounds = ptl->rounds;
				ptl2->roundsCnt = ptl->roundsCnt;
				ptl2->life = ptl->life;
				ptl2->t = ptl->t;
			}
		}
	}
}
#endif

/**
 * @brief Recalculate forbidden list, available moves and actor's move length
 * for the current selected actor.
 */
void CL_ConditionalMoveCalcForCurrentSelectedActor (void)
{
	if (selActor) {
		const int crouching_state = (selActor->state & STATE_CROUCHED) ? 1 : 0;
		CL_BuildForbiddenList();
		Grid_MoveCalc(clMap, selActor->fieldSize, &clPathMap, selActor->pos, crouching_state, MAX_ROUTE, fb_list, fb_length);
		CL_ResetActorMoveLength();
	}
}

/**
 * @brief Checks that an action is valid.
 */
static int CL_CheckAction (void)
{
	static char infoText[MAX_SMALLMENUTEXTLEN];

	if (!selActor) {
		Com_Printf("Nobody selected.\n");
		Com_sprintf(infoText, lengthof(infoText), _("Nobody selected\n"));
		return qfalse;
	}

/*	if (blockEvents) {
		Com_Printf("Can't do that right now.\n");
		return qfalse;
	}
*/
	if (cls.team != cl.actTeam) {
		SCR_DisplayHudMessage(_("This isn't your round\n"), 2000);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Get the real move length (depends on crouch-state of the current actor).
 * @note The part of the line that is not reachable in this turn (i.e. not enough
 * TUs left) will be drawn differently.
 * @param[in] to The position in the map to calculate the move-length for.
 * @return The amount of TUs that are needed to walk to the given grid position
 */
static int CL_MoveLength (pos3_t to)
{
	const int crouching_state = selActor->state & STATE_CROUCHED ? 1 : 0;
	const float length = Grid_MoveLength(&clPathMap, to, crouching_state, qfalse);

	switch (CL_MoveMode(length)) {
	case WALKTYPE_AUTOSTAND_BEING_USED:
		return length /* + 2 * TU_CROUCH */;
	case WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH:
	case WALKTYPE_CROUCH_WALKING:
		return length /* * TU_CROUCH_MOVING_FACTOR */;
	default:
		Com_DPrintf(DEBUG_CLIENT,"CL_MoveLength: MoveMode not recognised.\n");
	case WALKTYPE_WALKING:
		return length;
	}
}

/**
 * @brief Recalculates the currently selected Actor's move length.
 */
void CL_ResetActorMoveLength (void)
{
	assert(selActor);
	actorMoveLength = CL_MoveLength(mousePos);
}

/**
 * @brief Draws the way to walk when confirm actions is activated.
 * @param[in] to The location we draw the line to (starting with the location of selActor)
 * @return qtrue if everything went ok, otherwise qfalse.
 * @sa CL_MaximumMove (similar algo.)
 */
static qboolean CL_TraceMove (pos3_t to)
{
	int length;
	vec3_t vec, oldVec;
	pos3_t pos;
	int dv;
	int crouching_state;
#ifdef DEBUG
	int counter = 0;
#endif

	if (!selActor)
		return qfalse;

	length = CL_MoveLength(to);
	if (!length || length >= 0x3F)
		return qfalse;

	crouching_state = selActor->state & STATE_CROUCHED ? 1 : 0;

	Grid_PosToVec(clMap, selActor->fieldSize, to, oldVec);
	VectorCopy(to, pos);

	Com_DPrintf(DEBUG_PATHING, "Starting pos: (%i, %i, %i).\n", pos[0], pos[1], pos[2]);

	while ((dv = Grid_MoveNext(clMap, selActor->fieldSize, &clPathMap, pos, crouching_state)) != ROUTING_UNREACHABLE) {
#ifdef DEBUG
		if (++counter > 100) {
			Com_Printf("First pos: (%i, %i, %i, %i).\n", to[0], to[1], to[2], selActor->state & STATE_CROUCHED ? 1 : 0);
			Com_Printf("Last pos: (%i, %i, %i, %i).\n", pos[0], pos[1], pos[2], crouching_state);
			Grid_DumpDVTable(&clPathMap);
			Com_Error(ERR_DROP, "CL_TraceMove: DV table loops.");
		}
#endif
		length = CL_MoveLength(pos);
		PosSubDV(pos, crouching_state, dv); /* We are going backwards to the origin. */
		Com_DPrintf(DEBUG_PATHING, "Next pos: (%i, %i, %i, %i) [%i].\n", pos[0], pos[1], pos[2], crouching_state, dv);
		Grid_PosToVec(clMap, selActor->fieldSize, pos, vec);
		if (length > CL_UsableTUs(selActor))
			CL_ParticleSpawn("longRangeTracer", 0, vec, oldVec, NULL);
		else if (crouching_state)
			CL_ParticleSpawn("crawlTracer", 0, vec, oldVec, NULL);
		else
			CL_ParticleSpawn("moveTracer", 0, vec, oldVec, NULL);
		VectorCopy(vec, oldVec);
	}
	return qtrue;
}

/**
 * @brief Return the last position we can walk to with a defined amount of TUs.
 * @param[in] to The location we want to reach.
 * @param[in] tus How many timeunits we have to move.
 * @param[out] pos The location we can reach with the given amount of TUs.
 * @sa CL_TraceMove (similar algo.)
 */
static void CL_MaximumMove (pos3_t to, int tus, pos3_t pos)
{
	int length;
	/* vec3_t vec; */
	int dv;
	int crouching_state;

	if (!selActor)
		return;

	crouching_state = selActor && (selActor->state & STATE_CROUCHED) ? 1 : 0;

	length = CL_MoveLength(to);
	if (!length || length >= 0x3F)
		return;

	VectorCopy(to, pos);

	while ((dv = Grid_MoveNext(clMap, selActor->fieldSize, &clPathMap, pos, crouching_state)) != ROUTING_UNREACHABLE) {
		length = CL_MoveLength(pos);
		if (length <= tus)
			return;
		PosSubDV(pos, crouching_state, dv); /* We are going backwards to the origin. */
		/* Grid_PosToVec(clMap, selActor->fieldSize, pos, vec); */
	}
}


/**
 * @brief Starts moving actor.
 * @param[in] le
 * @param[in] to
 * @sa CL_ActorActionMouse
 * @sa CL_ActorSelectMouse
 */
void CL_ActorStartMove (const le_t * le, pos3_t to)
{
	int length;
	pos3_t toReal;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	/* the actor is still moving */
	if (blockEvents)
		return;

	assert(selActor);
	length = CL_MoveLength(to);

	if (!length || length >= ROUTING_NOT_REACHABLE) {
		/* move not valid, don't even care to send */
		return;
	}

	/* Get the last position we can walk to with the usable TUs. */
	CL_MaximumMove(to, CL_UsableTUs(selActor), toReal);

	/* Get the cost of the new position just in case. */
	length = CL_MoveLength(toReal);

	if (CL_UsableTUs(selActor) < length) {
		/* We do not have enough _usable_ TUs to move so don't even try to send. */
		/* This includes a check for reserved TUs (which isn't done on the server!) */
		return;
	}

	/* change mode to move now */
	cl.cmode = M_MOVE;

	/* move seems to be possible; send request to server */
	MSG_Write_PA(PA_MOVE, le->entnum, toReal);
}


/**
 * @brief Shoot with actor.
 * @param[in] le Who is shooting
 * @param[in] at Position you are targetting to
 */
void CL_ActorShoot (const le_t * le, pos3_t at)
{
	int type;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	Com_DPrintf(DEBUG_CLIENT, "CL_ActorShoot: cl.firemode %i.\n", cl.cfiremode);

	/** @todo Is there a better way to do this?
	 * This type value will travel until it is checked in at least g_combat.c:G_GetShotFromType.
	 */
	if (IS_MODE_FIRE_RIGHT(cl.cmode)) {
		type = ST_RIGHT;
	} else if (IS_MODE_FIRE_LEFT(cl.cmode)) {
		type = ST_LEFT;
	} else if (IS_MODE_FIRE_HEADGEAR(cl.cmode)) {
		type = ST_HEADGEAR;
	} else
		return;

	MSG_Write_PA(PA_SHOOT, le->entnum, at, type, cl.cfiremode, mousePosTargettingAlign);
}


/**
 * @brief Reload weapon with actor.
 * @param[in] hand
 * @sa CL_CheckAction
 */
void CL_ActorReload (int hand)
{
	inventory_t *inv;
	invList_t *ic;
	objDef_t *weapon;
	int x, y, tu;
	int container, bestContainer;

	if (!CL_CheckAction())
		return;

	/* check weapon */
	inv = &selActor->i;

	/* search for clips and select the one that is available easily */
	x = 0;
	y = 0;
	tu = 100;
	bestContainer = NONE;

	if (inv->c[hand]) {
		weapon = inv->c[hand]->item.t;
	} else if (hand == csi.idLeft
		&& inv->c[csi.idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = csi.idRight;
		weapon = inv->c[hand]->item.t;
	} else
		/* otherwise we could use weapon uninitialized */
		return;

	if (!weapon)
		return;

	/* return if the weapon is not reloadable */
	if (!weapon->reload)
		return;

	if (!RS_IsResearched_ptr(weapon->tech)) {
		SCR_DisplayHudMessage(_("You cannot reload this unknown item.\nYou need to research it and its ammunition first.\n"), 2000);
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = inv->c[container]; ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)
				 && RS_IsResearched_ptr(ic->item.t->tech)) {
					Com_GetFirstShapePosition(ic, &x, &y);
					x += ic->x;
					y += ic->y;
					tu = csi.ids[container].out;
					bestContainer = container;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer != NONE)
		MSG_Write_PA(PA_INVMOVE, selActor->entnum, bestContainer, x, y, hand, 0, 0);
	else
		Com_Printf("No (researched) clip left.\n");
}

/**
 * @brief Opens a door.
 * @sa CL_ActorDoorAction
 * @sa G_ClientUseEdict
 */
void CL_ActorUseDoor (void)
{
	if (!CL_CheckAction())
		return;

	assert(selActor->client_action);

	MSG_Write_PA(PA_USE_DOOR, selActor->entnum, selActor->client_action);
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorUseDoor: Use door number: %i (actor %i)\n", selActor->client_action, selActor->entnum);
}

/**
 * @brief Reads the door entity number for client interaction
 * @sa EV_DOOR_ACTION
 * @sa Touch_DoorTrigger
 * @sa CL_ActorUseDoor
 */
void CL_ActorDoorAction (struct dbuffer *msg)
{
	le_t* le;
	int number, doornumber;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_DOOR_ACTION], &number, &doornumber);

	/* get actor le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorDoorAction: Could not get le %i\n", number);
		return;
	}
	/* set door number */
	le->client_action = doornumber;
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoorAction: Set door number: %i (for actor with entnum %i)\n", doornumber, number);
}

/**
 * @brief Hud callback to open/close a door
 */
void CL_ActorDoorAction_f (void)
{
	if (!CL_CheckAction())
		return;

	/* no client action */
	if (selActor->client_action == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoorAction_f: No client_action set for actor with entnum %i\n", selActor->entnum);
		return;
	}

	/* Check if we should even try to send this command (no TUs left or). */
	if (CL_UsableTUs(selActor) >= TU_DOOR_ACTION)
		CL_ActorUseDoor();
}

/**
 * @brief When no trigger is touched, the client actions are resetted
 * @sa EV_RESET_CLIENT_ACTION
 * @sa G_ClientMove
 */
void CL_ActorResetClientAction (struct dbuffer *msg)
{
	le_t* le;
	int number;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_RESET_CLIENT_ACTION], &number);

	/* get actor le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorResetClientAction: Could not get le %i\n", number);
		return;
	}
	/* set door number */
	le->client_action = 0;
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorResetClientAction: Reset client action for actor with entnum %i\n", number);
}

/**
 * @brief The client changed something in his hand-containers. This function updates the reactionfire info.
 * @param[in] msg The netchannel message
 */
void CL_InvCheckHands (struct dbuffer *msg)
{
	int entnum;
	le_t *le;
	int actorIdx = -1;
	int hand = -1;		/**< 0=right, 1=left -1=undef*/

	NET_ReadFormat(msg, ev_format[EV_INV_HANDS_CHANGED], &entnum, &hand);
	if ((entnum < 0) || (hand < 0)) {
		Com_Printf("CL_InvCheckHands: entnum or hand not sent/received correctly. (number: %i)\n", entnum);
	}

	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("CL_InvCheckHands: LE doesn't exist. (number: %i)\n", entnum);
		return;
	}

	actorIdx = CL_GetActorNumber(le);
	if (actorIdx == -1) {
		Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: Could not get local entity actor id via CL_GetActorNumber\n");
		Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG actor info: team=%i(%s) type=%i inuse=%i\n", le->team, le->teamDef ? le->teamDef->name : "No team", le->type, le->inuse);
		return;
	}

	/* No need to continue if stored firemode settings are still usable. */
	if (!CL_WorkingFiremode(le, qtrue)) {
		/* Firemode for reaction not sane and/or not usable. */
		/* Update the changed hand with default firemode. */
		if (hand == 0) {
			Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG right\n");
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_RIGHT, -1);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG left\n");
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_LEFT, -1);
		}
		HideFiremodes();
	}

	if (CL_WorkingFiremode(le, qfalse)) {
		/* Reserved firemode for current turn not sane and/or not usable. */
	}
}

/**
 * @brief Moves actor.
 * @param[in] msg The netchannel message
 * @sa LET_PathMove
 * @note EV_ACTOR_MOVE
 */
void CL_ActorDoMove (struct dbuffer *msg)
{
	le_t *le;
	int number, i;

	number = NET_ReadShort(msg);
	/* get le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorDoMove: Could not get LE with id %i\n", number);
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("Can't move, LE doesn't exist or is not an actor (number: %i, type: %i)\n",
			number, le ? le->type : -1);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't move, actor dead\n");
		return;
	}

	/* speed is set in the EV_ACTOR_START_MOVE event */
	assert(le->speed);

#ifdef DEBUG
	/* get length/steps */
	if (le->pathLength || le->pathPos)
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoMove: Looks like the previous movement wasn't finished but a new one was received already\n");
#endif

	le->pathLength = NET_ReadByte(msg);
	assert(le->pathLength <= MAX_LE_PATHLENGTH);

	/* Also get the final position */
	le->newPos[0] = NET_ReadByte(msg);
	le->newPos[1] = NET_ReadByte(msg);
	le->newPos[2] = NET_ReadByte(msg);
	Com_DPrintf(DEBUG_CLIENT, "EV_ACTOR_MOVE: %i steps, s:(%i %i %i) d:(%i %i %i)\n", le->pathLength
		, le->pos[0], le->pos[1], le->pos[2]
		, le->newPos[0], le->newPos[1], le->newPos[2]);

	for (i = 0; i < le->pathLength; i++) {
		le->path[i] = NET_ReadByte(msg); /** Don't adjust dv values here- the whole thing is needed to move the actor! */
		le->pathContents[i] = NET_ReadShort(msg);
	}

	/* activate PathMove function */
	FLOOR(le) = NULL;
	le->think = LET_StartPathMove;
	le->pathPos = 0;
	le->startTime = cl.time;
	le->endTime = cl.time;

	CL_BlockEvents();
}


/**
 * @brief Turns the actor around without moving
 */
void CL_ActorTurnMouse (void)
{
	vec3_t div;
	byte dv;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	if (CL_UsableTUs(selActor) < TU_TURN) {
		/* Cannot turn because of not enough usable TUs. */
		return;
	}

	/* check for fire-modes, and cancel them */
	switch (cl.cmode) {
	case M_FIRE_R:
	case M_FIRE_L:
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		cl.cmode = M_MOVE;
		return; /* and return without turning */
	default:
		break;
	}

	/* calculate dv */
	VectorSubtract(mousePos, selActor->pos, div);
	dv = AngleToDV((int) (atan2(div[1], div[0]) * todeg));

	/* send message to server */
	MSG_Write_PA(PA_TURN, selActor->entnum, dv);
}


/**
 * @brief Turns actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDoTurn (struct dbuffer *msg)
{
	le_t *le;
	int entnum, dir;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_TURN], &entnum, &dir);

	/* get le */
	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("CL_ActorDoTurn: Could not get LE with id %i\n", entnum);
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("Can't turn, LE doesn't exist or is not an actor (number: %i, type: %i)\n", entnum, le ? le->type : -1);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't turn, actor dead\n");
		return;
	}

	le->dir = dir;
	le->angles[YAW] = dangle[le->dir];
}


/**
 * @brief Stands or crouches actor.
 * @todo Maybe add a popup that asks if the player _really_ wants to crouch/stand up when only the reserved amount is left?
 */
void CL_ActorStandCrouch_f (void)
{
	if (!CL_CheckAction())
		return;

	if (selActor->fieldSize == ACTOR_SIZE_2x2)
		/** @todo future thoughs: maybe define this in team_*.ufo files instead? */
		return;

	/* Check if we should even try to send this command (no TUs left or). */
	if (CL_UsableTUs(selActor) >= TU_CROUCH || CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH) {
		/* send a request to toggle crouch to the server */
		MSG_Write_PA(PA_STATE, selActor->entnum, STATE_CROUCHED);
	}
}

/**
 * @brief Toggles the headgear for the current selected player
 */
void CL_ActorUseHeadgear_f (void)
{
	invList_t* headgear;
	int tmp_mouseSpace= mouseSpace;

	/* this can be executed by a click on a hud button
	 * but we need MS_WORLD mouse space to let the shooting
	 * function work */
	mouseSpace = MS_WORLD;

	if (!CL_CheckAction())
		return;

	headgear = HEADGEAR(selActor);
	if (!headgear)
		return;

	cl.cmode = M_FIRE_HEADGEAR;
	cl.cfiremode = 0; /** @todo make this a variable somewhere? */
	CL_ActorShoot(selActor, selActor->pos);
	cl.cmode = M_MOVE;

	/* restore old mouse space */
	mouseSpace = tmp_mouseSpace;
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
	||  selChr->reservedTus.reserveCrouch) {
		/* Reset reserved TUs to 0 */
		CL_ReserveTUs(selActor, RES_CROUCH, 0);
		selChr->reservedTus.reserveCrouch = qfalse;
		/* MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_CROUCH, selChr->reservedTus.reserveCrouch, selChr->reservedTus.crouch); Update server-side settings */
	} else {
		/* Reserve the exact amount for crouching/staning up (if we have enough to do so). */
		if ((CL_UsableTUs(selActor) + CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH)) {
			CL_ReserveTUs(selActor, RES_CROUCH, TU_CROUCH);
		}
		/* Player wants to reserve Tus for crouching - remember this. */
		selChr->reservedTus.reserveCrouch = qtrue;
		/* MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_CROUCH, selChr->reservedTus.reserveCrouch, selChr->reservedTus.crouch); Update server-side settings */
	}

	/* Refresh checkbox and tooltip. */
	CL_RefreshWeaponButtons(CL_UsableTUs(selActor));

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
void CL_ActorToggleReaction_f (void)
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
			CL_DisplayFiremodes_f();

		/* Send request to update actor's reaction state to the server. */
		MSG_Write_PA(PA_STATE, selActor->entnum, state);
		selChr->reservedTus.reserveReaction = state;
		CL_SetReactionFiremode(selActor, selChr->RFmode.hand, selChr->RFmode.wpIdx, selChr->RFmode.fmIdx); /**< Re-calc reserved values with already selected FM. Includes PA_RESERVE_STATE (Update server-side settings)*/
		/** @todo remove me
		MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, selChr->reservedTus.reserveReaction, selChr->reservedTus.reaction); Update server-side settings
		*/
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
	CL_RefreshWeaponButtons(CL_UsableTUs(selActor));
}

/**
 * @brief Spawns particle effects for a hit actor.
 * @param[in] le The actor to spawn the particles for.
 * @param[in] impact The impact location (where the particles are spawned).
 * @param[in] normal The index of the normal vector of the particles (think: impact angle).
 * @todo Get real impact location and direction?
 */
static void CL_ActorHit (const le_t * le, vec3_t impact, int normal)
{
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorHit: Can't spawn particles, LE doesn't exist\n");
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorHit: Can't spawn particles, LE is not an actor (type: %i)\n", le->type);
		return;
	}

	if (le->teamDef) {
		/* Spawn "hit_particle" if defined in teamDef. */
		if (le->teamDef->hitParticle[0])
			CL_ParticleSpawn(le->teamDef->hitParticle, 0, impact, bytedirs[normal], NULL);
	}
}

/**
 * @brief Records if shot is first shot.
 */
static qboolean firstShot = qfalse;

/**
 * @brief Shoot with weapon.
 * @sa CL_ActorShoot
 * @sa CL_ActorShootHidden
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_SHOOT
 */
void CL_ActorDoShoot (struct dbuffer *msg)
{
	const fireDef_t *fd;
	le_t *le;
	vec3_t muzzle, impact;
	int flags, normal, number;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx, surfaceFlags, clientType;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT], &number, &objIdx, &weapFdsIdx, &fdIdx, &clientType, &flags, &surfaceFlags, &muzzle, &impact, &normal);

	/* get le */
	le = LE_Get(number);

	/* get the fire def */
	obj = &csi.ods[objIdx];
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal, qtrue);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0] && !(flags & SF_BOUNCED)) {
		S_StartLocalSound(fd->fireSound);
	}

	firstShot = qfalse;

	if (fd->irgoggles)
		refdef.rdflags |= RDF_IRGOGGLES;

	/* do actor related stuff */
	if (!le)
		return; /* maybe hidden or inuse is false? */

	if (!LE_IsActor(le)) {
		Com_Printf("Can't shoot, LE not an actor (type: %i)\n", le->type);
		return;
	}

	/* no animations for hidden actors */
	if (le->type == ET_ACTORHIDDEN)
		return;

	/** Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead :)
	 * Don't do it if it's a stun-attack though.
	 * @todo Special particles for stun attack (mind you that there is electrical and gas/chemical stunning)? */
	if ((flags & SF_BODY) && fd->obj->dmgtype != csi.damStunGas) {	/**< @todo && !(flags & SF_BOUNCED) ? */
		CL_ActorHit(le, impact, normal);
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't shoot, actor dead or stunned.\n");
		return;
	}

	if (le->team == cls.team) {
		/* if actor on our team, set this le as the last moving */
		CL_SetLastMoving(le);

		if (clientType != 0xFF)
			Com_Printf("CL_ActorDoShoot: left/right info out of sync somehow (le: %i, server: %i, client: %i).\n", number, clientType, cl.cmode);
		clientType = cl.cmode;
	}

	/* Animate - we have to check if it is right or left weapon usage. */
	if (HEADGEAR(le) && IS_MODE_FIRE_HEADGEAR(clientType)) {
		/* No animation for this */
	} else if (RIGHT(le) && IS_MODE_FIRE_RIGHT(clientType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
		R_AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	} else if (LEFT(le) && IS_MODE_FIRE_LEFT(clientType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->left, le->right, le->state));
		R_AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->left, le->right, le->state));
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoShoot: No information about weapon hand found or left/right info out of sync somehow (entnum: %i).\n", number);
		/* We use the default (right) animation now. */
		R_AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
		R_AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	}
}


/**
 * @brief Shoot with weapon but don't bother with animations - actor is hidden.
 * @sa CL_ActorShoot
 */
void CL_ActorShootHidden (struct dbuffer *msg)
{
	const fireDef_t *fd;
	int first;
	int objIdx;
	objDef_t *obj;
	int weapFdsIdx, fdIdx;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &objIdx, &weapFdsIdx, &fdIdx);

	/* get the fire def */
	obj = &csi.ods[objIdx];
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* start the sound */
	/** @todo is check for SF_BOUNCED needed? */
	if (((first && fd->soundOnce) || (!first && !fd->soundOnce)) && fd->fireSound[0]) {
		S_StartLocalSound(fd->fireSound);
	}

	/* if the shooting becomes visible, don't repeat sounds! */
	firstShot = qfalse;
}


/**
 * @brief Throw item with actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDoThrow (struct dbuffer *msg)
{
	const fireDef_t *fd;
	vec3_t muzzle, v0;
	int flags;
	int dtime;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_ACTOR_THROW], &dtime, &objIdx, &weapFdsIdx, &fdIdx, &flags, &muzzle, &v0);

	/* get the fire def */
	obj = &csi.ods[objIdx];
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le (local entity) */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0] && !(flags & SF_BOUNCED)) {
		sfx_t *sfx = S_RegisterSound(fd->fireSound);
		S_StartSound(muzzle, sfx, DEFAULT_SOUND_PACKET_VOLUME);
	}

	firstShot = qfalse;
}


/**
 * @brief Starts shooting with actor.
 * @param[in] msg The netchannel message
 * @sa CL_ActorShootHidden
 * @sa CL_ActorShoot
 * @sa CL_ActorDoShoot
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_START_SHOOT
 */
void CL_ActorStartShoot (struct dbuffer *msg)
{
	const fireDef_t *fd;
	le_t *le;
	pos3_t from, target;
	int number;
	int objIdx;
	objDef_t *obj;
	int weapFdsIdx, fdIdx, clientType;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_START_SHOOT], &number, &objIdx, &weapFdsIdx, &fdIdx, &clientType, &from, &target);

	obj = &csi.ods[objIdx];
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* shooting actor */
	le = LE_Get(number);

	/* center view (if wanted) */
	if (cl_centerview->integer && cl.actTeam != cls.team)
		CL_CameraRoute(from, target);

	/* first shot */
	firstShot = qtrue;

	/* actor dependant stuff following */
	if (!le)
		/* it's OK, the actor not visible */
		return;

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorStartShoot: LE (%i) not an actor (type: %i)\n", number, le->type);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("CL_ActorStartShoot: Can't start shoot, actor (%i) dead or stunned.\n", number);
		return;
	}

	/* erase one-time weapons from storage --- which ones?
	if (curCampaign && le->team == cls.team && !csi.ods[type].ammo) {
		if (ccs.eMission.num[type])
			ccs.eMission.num[type]--;
	} */

	/* ET_ACTORHIDDEN actors don't have a model yet */
	if (le->type == ET_ACTORHIDDEN)
		return;

	if (le->team == cls.team) {
		if (clientType != 0xFF)
			Com_Printf("CL_ActorStartShoot: left/right info out of sync somehow (le: %i, server: %i, client: %i).\n", number, clientType, cl.cmode);
		clientType = cl.cmode;
	}

	/* Animate - we have to check if it is right or left weapon usage. */
	if (RIGHT(le) && IS_MODE_FIRE_RIGHT(clientType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	} else if (LEFT(le) && IS_MODE_FIRE_LEFT(clientType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->left, le->right, le->state));
	/** no animation change for headgear - @see CL_ActorDoShoot */
	} else if (!(HEADGEAR(le) && IS_MODE_FIRE_HEADGEAR(clientType))) {
		/* We use the default (right) animation now. */
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	}
}

/**
 * @brief Kills actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDie (struct dbuffer *msg)
{
	le_t *le;
	int number, state;
	int i;
	char tmpbuf[128];

	NET_ReadFormat(msg, ev_format[EV_ACTOR_DIE], &number, &state);

	/* get le */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->entnum == number)
			break;

	if (le->entnum != number) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDie: Can't kill, LE doesn't exist\n");
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorDie: Can't kill, LE is not an actor (type: %i)\n", le->type);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("CL_ActorDie: Can't kill, actor already dead\n");
		return;
	} else if (!le->inuse) {
		/* LE not in use condition normally arises when CL_EntPerish has been
		 * called on the le to hide it from the client's sight.
		 * Probably can return without killing unused LEs, but testing reveals
		 * killing an unused LE here doesn't cause any problems and there is
		 * an outside chance it fixes some subtle bugs. */
	}

	/* count spotted aliens */
	if (le->team != cls.team && le->team != TEAM_CIVILIAN && le->inuse)
		cl.numAliensSpotted--;

	/* set relevant vars */
	FLOOR(le) = NULL;
	le->STUN = 0;	/**< @todo Is there a reason this is reset? We _may_ need that in the future somehow.
					 * @sa g_client.c:G_ActorDie */
	le->state = state;

	/* play animation */
	le->think = NULL;
	R_AnimChange(&le->as, le->model1, va("death%i", LE_GetAnimationIndexForDeath(le)));
	R_AnimAppend(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));

	/* Print some info about the death or stun. */
	if (le->team == cls.team) {
		character_t *chr = CL_GetActorChr(le);
		if (chr && LE_IsStunned(le)) {
			Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s %s was stunned\n"),
			chr->score.rank >= 0 ? _(gd.ranks[chr->score.rank].shortname) : "", chr->name);
			SCR_DisplayHudMessage(tmpbuf, 2000);
		} else if (chr) {
			Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s %s was killed\n"),
			chr->score.rank >= 0 ? _(gd.ranks[chr->score.rank].shortname) : "", chr->name);
			SCR_DisplayHudMessage(tmpbuf, 2000);
		}
	} else {
		switch (le->team) {
		case (TEAM_CIVILIAN):
			if (LE_IsStunned(le))
				SCR_DisplayHudMessage(_("A civilian was stunned.\n"), 2000);
			else
				SCR_DisplayHudMessage(_("A civilian was killed.\n"), 2000);
			break;
		case (TEAM_ALIEN):
			if (le->teamDef) {
				if (RS_IsResearched_ptr(RS_GetTechByID(le->teamDef->tech))) {
					if (LE_IsStunned(le)) {
						Com_sprintf(tmpbuf, lengthof(tmpbuf), _("An alien was stunned: %s\n"), _(le->teamDef->name));
						SCR_DisplayHudMessage(tmpbuf, 2000);
					} else {
						Com_sprintf(tmpbuf, lengthof(tmpbuf), _("An alien was killed: %s\n"), _(le->teamDef->name));
						SCR_DisplayHudMessage(tmpbuf, 2000);
					}
				} else {
					if (LE_IsStunned(le))
						SCR_DisplayHudMessage(_("An alien was stunned.\n"), 2000);
					else
						SCR_DisplayHudMessage(_("An alien was killed.\n"), 2000);
				}
			} else {
				if (LE_IsStunned(le))
					SCR_DisplayHudMessage(_("An alien was stunned.\n"), 2000);
				else
					SCR_DisplayHudMessage(_("An alien was killed.\n"), 2000);
			}
			break;
		case (TEAM_PHALANX):
			if (LE_IsStunned(le))
				SCR_DisplayHudMessage(_("A soldier was stunned.\n"), 2000);
			else
				SCR_DisplayHudMessage(_("A soldier was killed.\n"), 2000);
			break;
		default:
			if (LE_IsStunned(le))
				SCR_DisplayHudMessage(va(_("A member of team %i was stunned.\n"), le->team), 2000);
			else
				SCR_DisplayHudMessage(va(_("A member of team %i was killed.\n"), le->team), 2000);
			break;
		}
	}

	CL_PlayActorSound(le, SND_DEATH);

	VectorCopy(player_dead_maxs, le->maxs);
	CL_RemoveActorFromTeamList(le);
}


/*
==============================================================
MOUSE INPUT
==============================================================
*/

/**
 * @brief handle select or action clicking in either move mode
 * @sa CL_ActorSelectMouse
 * @sa CL_ActorActionMouse
 */
static void CL_ActorMoveMouse (void)
{
	if (cl.cmode == M_PEND_MOVE) {
		if (VectorCompare(mousePos, mousePendPos)) {
			/* Pending move and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorStartMove(selActor, mousePos);
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, mousePendPos);
		}
	} else {
		if (confirm_actions->integer) {
			/* Set our mode to pending move. */
			VectorCopy(mousePos, mousePendPos);

			cl.cmode = M_PEND_MOVE;
		} else {
			/* Just move there */
			CL_ActorStartMove(selActor, mousePos);
		}
	}
}

/**
 * @brief Selects an actor using the mouse.
 * @todo Comment on the cl.cmode stuff.
 * @sa CL_ActorStartMove
 */
void CL_ActorSelectMouse (void)
{
	if (mouseSpace != MS_WORLD)
		return;

	switch (cl.cmode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Try and select another team member */
		if (mouseActor != selActor && CL_ActorSelect(mouseActor)) {
			/* Succeeded so go back into move mode. */
			cl.cmode = M_MOVE;
		} else {
			CL_ActorMoveMouse();
		}
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (VectorCompare(mousePos, mousePendPos)) {
			/* Pending shot and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorShoot(selActor, mousePos);

			/* We switch back to aiming mode. */
			if (cl.cmode == M_PEND_FIRE_R)
				cl.cmode = M_FIRE_R;
			else
				cl.cmode = M_FIRE_L;
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, mousePendPos);
		}
		break;
	case M_FIRE_R:
		if (mouseActor == selActor)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			cl.cmode = M_PEND_FIRE_R;
			VectorCopy(mousePos, mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	case M_FIRE_L:
		if (mouseActor == selActor)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			cl.cmode = M_PEND_FIRE_L;
			VectorCopy(mousePos, mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	default:
		break;
	}
}


/**
 * @brief initiates action with mouse.
 * @sa CL_ActionDown
 * @sa CL_ActorStartMove
 */
void CL_ActorActionMouse (void)
{
	if (!selActor || mouseSpace != MS_WORLD)
		return;

	if (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE) {
		CL_ActorMoveMouse();
	} else {
		cl.cmode = M_MOVE;
	}
}


/*==============================================================
ROUND MANAGEMENT
==============================================================*/

/**
 * @brief Finishes the current round of the player in battlescape and starts the round for the next team.
 */
void CL_NextRound_f (void)
{
	struct dbuffer *msg;
	/* can't end round if we are not in battlescape */
	if (!CL_OnBattlescape())
		return;

	/* can't end round if we're not active */
	if (cls.team != cl.actTeam)
		return;

	/* send endround */
	msg = new_dbuffer();
	NET_WriteByte(msg, clc_endround);
	NET_WriteMsg(cls.netStream, msg);

	/* change back to remote view */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
}

/**
 * @brief Performs end-of-turn processing.
 * @param[in] msg The netchannel message
 * @sa CL_EndRoundAnnounce
 */
void CL_DoEndRound (struct dbuffer *msg)
{
	int actorIdx;

	/* hud changes */
	if (cls.team == cl.actTeam)
		MN_ExecuteConfunc("endround");

	refdef.rdflags &= ~RDF_IRGOGGLES;

	/* change active player */
	Com_Printf("Team %i ended round", cl.actTeam);
	cl.actTeam = NET_ReadByte(msg);
	Com_Printf(", team %i's round started!\n", cl.actTeam);

	/* hud changes */
	if (cls.team == cl.actTeam) {
		/* check whether a particle has to go */
		CL_ParticleCheckRounds();
		MN_ExecuteConfunc("startround");
		SCR_DisplayHudMessage(_("Your round started!\n"), 2000);
		S_StartLocalSound("misc/roundstart");
		CL_ConditionalMoveCalcForCurrentSelectedActor();

		for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
			if (cl.teamList[actorIdx]) {
				/* Check for unusable RF setting - just in case. */
				if (!CL_WorkingFiremode(cl.teamList[actorIdx], qtrue)) {
					Com_DPrintf(DEBUG_CLIENT, "CL_DoEndRound: INFO Updating reaction firemode for actor %i! - We need to check why that happened.\n", actorIdx);
					/* At this point the rest of the code forgot to update RF-settings somewhere. */
					CL_SetDefaultReactionFiremode(cl.teamList[actorIdx], ACTOR_HAND_CHAR_RIGHT);
				}

				/** @todo Reset reservations for shots?
				CL_ReserveTUs(cl.teamList[actorIdx], RES_SHOT, 0);
				MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, 0, selChr->reservedTus.shot); * Update server-side settings *
				*/
			}
		}
	}
}


/*
==============================================================
MOUSE SCANNING
==============================================================
*/

/**
 * @brief Battlescape cursor positioning.
 * @note Sets global var mouseActor to current selected le
 * @sa IN_Parse
 */
void CL_ActorMouseTrace (void)
{
	int i, restingLevel, intersectionLevel;
	float cur[2], frustumslope[2], projectiondistance = 2048;
	float nDotP2minusP1, u;
	vec3_t forward, right, up, stop;
	vec3_t from, end, dir;
	vec3_t mapNormal, P3, P2minusP1, P3minusP1;
	vec3_t pA, pB, pC;
	pos3_t testPos;
	pos3_t actor2x2[3];

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	le_t *le;

	/* get cursor position as a -1 to +1 range for projection */
	cur[0] = (mousePosX * viddef.rx - viddef.viewWidth * 0.5 - viddef.x) / (viddef.viewWidth * 0.5);
	cur[1] = (mousePosY * viddef.ry - viddef.viewHeight * 0.5 - viddef.y) / (viddef.viewHeight * 0.5);

	/* get trace vectors */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON) {
		VectorCopy(selActor->origin, from);
		/* trace from eye-height */
		if (selActor->state & STATE_CROUCHED)
			from[2] += EYE_HT_CROUCH;
		else
			from[2] += EYE_HT_STAND;
		AngleVectors(cl.cam.angles, forward, right, up);
		/* set the intersection level to that of the selected actor */
		VecToPos(from, testPos);
		intersectionLevel = Grid_Fall(clMap, fieldSize, testPos);

		/* if looking up, raise the intersection level */
		if (cur[1] < 0.0f)
			intersectionLevel++;
	} else {
		VectorCopy(cl.cam.camorg, from);
		VectorCopy(cl.cam.axis[0], forward);
		VectorCopy(cl.cam.axis[1], right);
		VectorCopy(cl.cam.axis[2], up);
		intersectionLevel = cl_worldlevel->integer;
	}

	if (cl_isometric->integer)
		frustumslope[0] = 10.0 * refdef.fov_x;
	else
		frustumslope[0] = tan(refdef.fov_x * M_PI / 360.0) * projectiondistance;
	frustumslope[1] = frustumslope[0] * ((float)viddef.viewHeight / (float)viddef.viewWidth);

	/* transform cursor position into perspective space */
	VectorMA(from, projectiondistance, forward, stop);
	VectorMA(stop, cur[0] * frustumslope[0], right, stop);
	VectorMA(stop, cur[1] * -frustumslope[1], up, stop);

	/* in isometric mode the camera position has to be calculated from the cursor position so that the trace goes in the right direction */
	if (cl_isometric->integer)
		VectorMA(stop, -projectiondistance * 2, forward, from);

	/* set stop point to the intersection of the trace line with the desired plane */
	/* description of maths used:
	 *   The equation for the plane can be written:
	 *     mapNormal dot (end - P3) = 0
	 *     where mapNormal is the vector normal to the plane,
	 *         P3 is any point on the plane and
	 *         end is the point where the line interesects the plane
	 *   All points on the line can be calculated using:
	 *     P1 + u*(P2 - P1)
	 *     where P1 and P2 are points that define the line and
	 *           u is some scalar
	 *   The intersection of the line and plane occurs when:
	 *     mapNormal dot (P1 + u*(P2 - P1)) == mapNormal dot P3
	 *   The intersection therefore occurs when:
	 *     u = (mapNormal dot (P3 - P1))/(mapNormal dot (P2 - P1))
	 * Note: in the code below from & stop represent P1 and P2 respectively
	 */
	VectorSet(P3, 0., 0., intersectionLevel * UNIT_HEIGHT + CURSOR_OFFSET);
	VectorSet(mapNormal, 0., 0., 1.);
	VectorSubtract(stop, from, P2minusP1);
	nDotP2minusP1 = DotProduct(mapNormal, P2minusP1);

	/* calculate intersection directly if angle is not parallel to the map plane */
	if (nDotP2minusP1 > 0.01 || nDotP2minusP1 < -0.01) {
		VectorSubtract(P3, from, P3minusP1);
		u = DotProduct(mapNormal, P3minusP1)/nDotP2minusP1;
		VectorScale(P2minusP1, (vec_t)u, dir);
		VectorAdd(from, dir, end);
	} else { /* otherwise do a full trace */
		TR_TestLineDM(from, stop, end, TL_FLAG_ACTORCLIP);
	}

	VecToPos(end, testPos);
	restingLevel = Grid_Fall(clMap, fieldSize, testPos);

	/* hack to prevent cursor from getting stuck on the top of an invisible
	 * playerclip surface (in most cases anyway) */
	PosToVec(testPos, pA);
	/* ensure that the cursor is in the world, if this is not done, the tracer box is
	 * drawn in the void on the first level and the menu key bindings might get executed
	 * this could result in different problems like the zooming issue (where you can't zoom
	 * in again, because in_zoomout->state is not resetted). */
	if (CL_OutsideMap(pA, MAP_SIZE_OFFSET))
		return;

	VectorCopy(pA, pB);
	pA[2] += UNIT_HEIGHT;
	pB[2] -= UNIT_HEIGHT;
	/** @todo Shouldn't we check the return value of CM_TestLineDM here - maybe
	 * we don't have to do the second Grid_Fall call at all and can safe a lot
	 * of traces */
	TR_TestLineDM(pA, pB, pC, TL_FLAG_ACTORCLIP);
	VecToPos(pC, testPos);
	restingLevel = min(restingLevel, Grid_Fall(clMap, fieldSize, testPos));

	/* if grid below intersection level, start a trace from the intersection */
	if (restingLevel < intersectionLevel) {
		VectorCopy(end, from);
		from[2] -= CURSOR_OFFSET;
		TR_TestLineDM(from, stop, end, TL_FLAG_ACTORCLIP);
		VecToPos(end, testPos);
		restingLevel = Grid_Fall(clMap, fieldSize, testPos);
	}

	/* test if the selected grid is out of the world */
	if (restingLevel < 0 || restingLevel >= PATHFINDING_HEIGHT)
		return;

	/* Set truePos- test pos is under the cursor. */
	VectorCopy(testPos, truePos);
	truePos[2] = intersectionLevel;

	/* Set mousePos to the position that the actor will move to. */
	testPos[2] = restingLevel;
	VectorCopy(testPos, mousePos);


	/* search for an actor on this field */
	mouseActor = NULL;
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le))
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
				if (VectorCompare(le->pos, mousePos)) {
					mouseActor = le;
				}
				break;
			case ACTOR_SIZE_2x2:
				VectorSet(actor2x2[0], le->pos[0] + 1, le->pos[1],     le->pos[2]);
				VectorSet(actor2x2[1], le->pos[0],     le->pos[1] + 1, le->pos[2]);
				VectorSet(actor2x2[2], le->pos[0] + 1, le->pos[1] + 1, le->pos[2]);
				if (VectorCompare(le->pos, mousePos)
				|| VectorCompare(actor2x2[0], mousePos)
				|| VectorCompare(actor2x2[1], mousePos)
				|| VectorCompare(actor2x2[2], mousePos)) {
					mouseActor = le;
				}
				break;
			default:
				Com_Error(ERR_DROP, "Grid_MoveCalc: unknown actor-size: %i", le->fieldSize);
				break;
		}

	/* calculate move length */
	if (selActor && !VectorCompare(mousePos, mouseLastPos)) {
		VectorCopy(mousePos, mouseLastPos);
		CL_ResetActorMoveLength();
	}

	/* mouse is in the world */
	mouseSpace = MS_WORLD;
}


/*
==============================================================
ACTOR GRAPHICS
==============================================================
*/

/**
 * @brief Checks whether a weapon should be added to the entity's hand
 * @param[in] objID The item id that the actor is holding in his hand (@c le->left or @c le->right)
 * @return true if the weapon is a valid item and false if it's a dummy item or the actor has nothing
 * in the given hand
 */
static inline qboolean CL_AddActorWeapon (int objID)
{
	if (objID != NONE) {
		const objDef_t *od = &csi.ods[objID];
		if (od->isDummy)
			return qfalse;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Adds an actor.
 * @param[in] le The local entity to get the values from
 * @param[in] ent The body entity used in the renderer
 * @sa CL_AddUGV
 * @sa LE_AddToScene
 * @sa CL_ActorAppear
 * @note Called via addfunc for each local entity in every frame
 */
qboolean CL_AddActor (le_t * le, entity_t * ent)
{
	entity_t add;

	/* add the weapons to the actor's hands */
	if (!LE_IsDead(le)) {
		const qboolean addLeftHandWeapon = CL_AddActorWeapon(le->left);
		const qboolean addRightHandWeapon = CL_AddActorWeapon(le->right);
		/* add left hand weapon */
		if (addLeftHandWeapon) {
			memset(&add, 0, sizeof(add));

			add.model = cls.model_weapons[le->left];
			assert(add.model);

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2 + addRightHandWeapon;
			add.tagname = "tag_lweapon";
			add.lighting = &le->lighting; /* values from the actor */

			R_AddEntity(&add);
		}

		/* add right hand weapon */
		if (addRightHandWeapon) {
			memset(&add, 0, sizeof(add));

			add.alpha = le->alpha;
			add.model = cls.model_weapons[le->right];
			assert(add.model);

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2;
			add.tagname = "tag_rweapon";
			add.lighting = &le->lighting; /* values from the actor */

			R_AddEntity(&add);
		}
	}

	/* add head */
	memset(&add, 0, sizeof(add));

	/* the actor is hearing a sound */
	if (le->hearTime) {
		if (cls.realtime - le->hearTime > 3000) {
			le->hearTime = 0;
		} else {
			add.flags |= RF_HIGHLIGHT;
		}
	}

	add.alpha = le->alpha;
	add.model = le->model2;
	assert(add.model);
	add.skinnum = le->skinnum;

	/* point to the body ent which will be added last */
	add.tagent = R_GetFreeEntity() + 1;
	add.tagname = "tag_head";
	add.lighting = &le->lighting; /* values from the actor */

	R_AddEntity(&add);

	/** Add actor special effects.
	 * Only draw blood if the actor is dead or (if stunned) was damaged more than half its maximum HPs. */
	/** @todo Better value for this?	*/
	if (LE_IsStunned(le) && le->HP <= (le->maxHP / 2))
		ent->flags |= RF_BLOOD;
	else if (LE_IsDead(le))
		ent->flags |= RF_BLOOD;
	else
		ent->flags |= RF_SHADOW;

	ent->flags |= RF_ACTOR;

	if (!LE_IsDead(le) && !LE_IsStunned(le)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (le->team == cls.team) {
			if (le->pnum == cl.pnum)
				ent->flags |= RF_MEMBER;
			if (le->pnum != cl.pnum)
				ent->flags |= RF_ALLIED;
		}
	}

	return qtrue;
}

/*
==============================================================
TARGETING GRAPHICS
==============================================================
*/

#define LOOKUP_EPSILON 0.0001f

/**
 * @brief table for lookup_erf
 * lookup[]= {erf(0), erf(0.1), ...}
 */
static const float lookup[30]= {
	0.0f,    0.1125f, 0.2227f, 0.3286f, 0.4284f, 0.5205f, 0.6039f,
	0.6778f, 0.7421f, 0.7969f, 0.8427f, 0.8802f, 0.9103f, 0.9340f,
	0.9523f, 0.9661f, 0.9763f, 0.9838f, 0.9891f, 0.9928f, 0.9953f,
	0.9970f, 0.9981f, 0.9989f, 0.9993f, 0.9996f, 0.9998f, 0.9999f,
	0.9999f, 1.0000f
};

/**
 * @brief table for lookup_erf
 * lookup[]= {10*(erf(0.1)-erf(0.0)), 10*(erf(0.2)-erf(0.1)), ...}
 */
static const float lookupdiff[30]= {
	1.1246f, 1.1024f, 1.0592f, 0.9977f, 0.9211f, 0.8336f, 0.7395f,
	0.6430f, 0.5481f, 0.4579f, 0.3750f, 0.3011f, 0.2369f, 0.1828f,
	0.1382f, 0.1024f, 0.0744f, 0.0530f, 0.0370f, 0.0253f, 0.0170f,
	0.0112f, 0.0072f, 0.0045f, 0.0028f, 0.0017f, 0.0010f, 0.0006f,
	0.0003f, 0.0002f
};

/**
 * @brief calculate approximate erf, the error function
 * http://en.wikipedia.org/wiki/Error_function
 * uses lookup table and linear interpolation
 * approximation good to around 0.001.
 * easily good enough for the job.
 * @param[in] z the number to calculate the erf of.
 * @return for posotive arg, returns approximate erf. for -ve arg returns 0.0f.
 */
static float lookup_erf (float z)
{
	float ifloat;
	int iint;

	/* erf(-z)=-erf(z), but erf of -ve number should never be used here
	 * so return 0 here */
	if (z < LOOKUP_EPSILON)
		return 0.0f;
	if (z > 2.8f)
		return 1.0f;
	ifloat = floor(z * 10.0f);
	iint = (int)ifloat;
	assert(iint < 30);
	return lookup[iint] + (z - ifloat / 10.0f) * lookupdiff[iint];
}

/**
 * @brief Calculates chance to hit.
 * @param[in] toPos
 */
static float CL_TargetingToHit (pos3_t toPos)
{
	vec3_t shooter, target;
	float distance, pseudosin, width, height, acc, perpX, perpY, hitchance,
		stdevupdown, stdevleftright, crouch, commonfactor;
	int distx, disty, i, n;
	le_t *le;

	if (!selActor || !selFD)
		return 0.0;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && VectorCompare(le->pos, toPos))
			break;

	if (i >= numLEs)
		/* no target there */
		return 0.0;
	/* or suicide attempted */
	if (le == selActor && selFD->damage[0] > 0)
		return 0.0;

	VectorCopy(selActor->origin, shooter);
	VectorCopy(le->origin, target);

	/* Calculate HitZone: */
	distx = fabs(shooter[0] - target[0]);
	disty = fabs(shooter[1] - target[1]);
	distance = sqrt(distx * distx + disty * disty);
	if (distx > disty)
		pseudosin = distance / distx;
	else
		pseudosin = distance / disty;
	width = 2 * PLAYER_WIDTH * pseudosin;
	height = ((le->state & STATE_CROUCHED) ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT);

	acc = GET_ACC(selChr->score.skills[ABILITY_ACCURACY],
			selFD->weaponSkill ? selChr->score.skills[selFD->weaponSkill] : 0);

	crouch = ((selActor->state & STATE_CROUCHED) && selFD->crouch) ? selFD->crouch : 1;

	commonfactor = crouch * torad * distance * GET_INJURY_MULT(selChr->score.skills[ABILITY_MIND], selActor->HP, selActor->maxHP);
	stdevupdown = (selFD->spread[0] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	stdevleftright = (selFD->spread[1] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	hitchance = (stdevupdown > LOOKUP_EPSILON ? lookup_erf(height * 0.3536f / stdevupdown) : 1.0f)
			  * (stdevleftright > LOOKUP_EPSILON ? lookup_erf(width * 0.3536f / stdevleftright) : 1.0f);
	/* 0.3536=sqrt(2)/4 */

	/* Calculate cover: */
	n = 0;
	height = height / 18;
	width = width / 18;
	target[2] -= UNIT_HEIGHT / 2;
	target[2] += height * 9;
	perpX = disty / distance * width;
	perpY = 0 - distx / distance * width;

	target[0] += perpX;
	perpX *= 2;
	target[1] += perpY;
	perpY *= 2;
	target[2] += 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] += 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[2] += 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX * 3;
	target[1] -= perpY * 3;
	target[2] -= 12 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] -= 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 10 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;

	return (hitchance * (0.125) * n);
}

/**
 * @brief Show weapon radius
 * @param[in] center The center of the circle
 */
static void CL_Targeting_Radius (vec3_t center)
{
	const vec4_t color = {0, 1, 0, 0.3};
	ptl_t *particle;

	assert(selFD);

	particle = CL_ParticleSpawn("*circle", 0, center, NULL, NULL);
	particle->size[0] = selFD->splrad; /* misuse size vector as radius */
	particle->size[1] = 1; /* thickness */
	particle->style = STYLE_CIRCLE;
	particle->blend = BLEND_BLEND;
	/* free the particle every frame */
	particle->life = 0.0001;
	Vector4Copy(color, particle->color);
}


/**
 * @brief Draws line to target.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] toPos The (grid-) position of the target.
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargeting
 * @sa CL_Trace
 */
static void CL_TargetingStraight (pos3_t fromPos, int from_actor_size, pos3_t toPos)
{
	vec3_t start, end;
	vec3_t dir, mid;
	trace_t tr;
	int oldLevel, i;
	float d;
	qboolean crossNo;
	le_t *le;
	le_t *target = NULL;
	int to_actor_size;

	if (!selActor || !selFD)
		return;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

	/* Determine the target's size. */
	to_actor_size = target /**< Get size of selected actor or fall back to 1x1. */
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	/** @todo Adjust the toPos to the actor in case the actor is 2x2 */

	Grid_PosToVec(clMap, from_actor_size, fromPos, start);
	Grid_PosToVec(clMap, to_actor_size, toPos, end);
	if (mousePosTargettingAlign)
		end[2] -= mousePosTargettingAlign;

	/* calculate direction */
	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	/* calculate 'out of range point' if there is one */
	if (VectorDistSqr(start, end) > selFD->range * selFD->range) {
		VectorMA(start, selFD->range, dir, mid);
		crossNo = qtrue;
	} else {
		VectorCopy(end, mid);
		crossNo = qfalse;
	}

	/* switch up to top level, this is a bit of a hack to make sure our trace doesn't go through ceilings ... */
	oldLevel = cl_worldlevel->integer;
	cl_worldlevel->integer = cl.map_maxlevel - 1;


	tr = CL_Trace(start, mid, vec3_origin, vec3_origin, selActor, target, MASK_SHOT);

	if (tr.fraction < 1.0) {
		d = VectorDist(start, mid);
		VectorMA(start, tr.fraction * d, dir, mid);
		crossNo = qtrue;
	}

	/* switch level back to where it was again */
	cl_worldlevel->integer = oldLevel;

	/* spawn particles */
	CL_ParticleSpawn("inRangeTracer", 0, start, mid, NULL);
	if (crossNo) {
		CL_ParticleSpawn("longRangeTracer", 0, mid, end, NULL);
		CL_ParticleSpawn("cross_no", 0, end, NULL, NULL);
	} else {
		CL_ParticleSpawn("cross", 0, end, NULL, NULL);
	}

	selToHit = 100 * CL_TargetingToHit(toPos);
}


#define GRENADE_PARTITIONS	20

/**
 * @brief Shows targetting for a grenade.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] toPos The (grid-) position of the target (mousePos or mousePendPos).
 * @sa CL_TargetingStraight
 */
static void CL_TargetingGrenade (pos3_t fromPos, int from_actor_size, pos3_t toPos)
{
	vec3_t from, at, cross;
	float vz, dt;
	vec3_t v0, ds, next;
	trace_t tr;
	int oldLevel;
	qboolean obstructed = qfalse;
	int i;
	le_t *le;
	le_t *target = NULL;
	int to_actor_size;

	if (!selActor || Vector2Compare(fromPos, toPos))
		return;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

	/* Determine the target's size. */
	to_actor_size = target /**< Get size of selected actor or fall back to 1x1. */
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	/** @todo Adjust the toPos to the actor in case the actor is 2x2 */

	/* get vectors, paint cross */
	Grid_PosToVec(clMap, from_actor_size, fromPos, from);
	Grid_PosToVec(clMap, to_actor_size, toPos, at);
	from[2] += selFD->shotOrg[1];

	/* prefer to aim grenades at the ground */
	at[2] -= GROUND_DELTA;
	if (mousePosTargettingAlign)
		at[2] -= mousePosTargettingAlign;
	VectorCopy(at, cross);

	/* calculate parabola */
	dt = Com_GrenadeTarget(from, at, selFD->range, selFD->launched, selFD->rolled, v0);
	if (!dt) {
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
		return;
	}

	dt /= GRENADE_PARTITIONS;
	VectorSubtract(at, from, ds);
	VectorScale(ds, 1.0 / GRENADE_PARTITIONS, ds);
	ds[2] = 0;

	/* switch up to top level, this is a bit of a hack to make sure our trace doesn't go through ceilings ... */
	oldLevel = cl_worldlevel->integer;
	cl_worldlevel->integer = cl.map_maxlevel - 1;

	/* paint */
	vz = v0[2];

	for (i = 0; i < GRENADE_PARTITIONS; i++) {
		VectorAdd(from, ds, next);
		next[2] += dt * (vz - 0.5 * GRAVITY * dt);
		vz -= GRAVITY * dt;
		VectorScale(v0, (i + 1.0) / GRENADE_PARTITIONS, at);

		/* trace for obstacles */
		tr = CL_Trace(from, next, vec3_origin, vec3_origin, selActor, target, MASK_SHOT);

		/* something was hit */
		if (tr.fraction < 1.0) {
			obstructed = qtrue;
		}

		/* draw particles */
		/** @todo character strength should be used here, too
		 * the stronger the character, the further the throw */
		if (obstructed || VectorLength(at) > selFD->range)
			CL_ParticleSpawn("longRangeTracer", 0, from, next, NULL);
		else
			CL_ParticleSpawn("inRangeTracer", 0, from, next, NULL);
		VectorCopy(next, from);
	}
	/* draw targetting cross */
	if (obstructed || VectorLength(at) > selFD->range)
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
	else
		CL_ParticleSpawn("cross", 0, cross, NULL, NULL);

	if (selFD->splrad) {
		Grid_PosToVec(clMap, to_actor_size, toPos, at);
		CL_Targeting_Radius(at);
	}

	selToHit = 100 * CL_TargetingToHit(toPos);

	/* switch level back to where it was again */
	cl_worldlevel->integer = oldLevel;
}


/**
 * @brief field marker box
 * @sa ModelOffset in cl_le.c
 */
static const vec3_t boxSize = { BOX_DELTA_WIDTH, BOX_DELTA_LENGTH, BOX_DELTA_HEIGHT };
#define BoxSize(i,source,target) (target[0]=i*source[0]+((i-1)*UNIT_SIZE),target[1]=i*source[1]+((i-1)*UNIT_SIZE),target[2]=source[2])
#define BoxOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH), target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH), target[2]=0)

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 */
static void CL_AddTargetingBox (pos3_t pos, qboolean pendBox)
{
	entity_t ent;
	vec3_t realBoxSize;
	vec3_t cursorOffset;

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_BOX;

	Grid_PosToVec(clMap, fieldSize, pos, ent.origin);

	/**
	 * Paint the green box if move is possible ...
	 * OR paint a dark blue one if move is impossible or the
	 * soldier does not have enough TimeUnits left.
	 */
	if (selActor && actorMoveLength < ROUTING_NOT_REACHABLE && actorMoveLength <= CL_UsableTUs(selActor))
		VectorSet(ent.angles, 0, 1, 0); /* Green */
	else
		VectorSet(ent.angles, 0, 0, 0.6); /* Dark Blue */

	VectorAdd(ent.origin, boxSize, ent.oldorigin);

	/* color */
	if (mouseActor && (mouseActor != selActor)) {
		ent.alpha = 0.4 + 0.2 * sin((float) cl.time / 80);
		/* Paint the box red if the soldiers under the cursor is
		 * not in our team and no civilian either. */
		if (mouseActor->team != cls.team) {
			switch (mouseActor->team) {
			case TEAM_CIVILIAN:
				/* Civilians are yellow */
				VectorSet(ent.angles, 1, 1, 0); /* Yellow */
				break;
			default:
				if (mouseActor->team == TEAM_ALIEN) {
					if (mouseActor->teamDef) {
						if (RS_IsResearched_ptr(RS_GetTechByID(mouseActor->teamDef->tech)))
							mn.menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = _(mouseActor->teamDef->name);
						else
							mn.menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = _("Unknown alien race");
					}
				} else {
					/* multiplayer names */
					/* see CL_ParseClientinfo */
					mn.menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum];
				}
				/* Aliens (and players not in our team [multiplayer]) are red */
				VectorSet(ent.angles, 1, 0, 0); /* Red */
				break;
			}
		} else {
			/* coop multiplayer games */
			if (mouseActor->pnum != cl.pnum) {
				mn.menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum];
			} else {
				/* we know the names of our own actors */
				character_t* chr = CL_GetActorChr(mouseActor);
				assert(chr);
				mn.menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = chr->name;
			}
			/* Paint a light blue box if on our team */
			VectorSet(ent.angles, 0.2, 0.3, 1); /* Light Blue */
		}

		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);
			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		}
	} else {
		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);

			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		} else {
			VectorSubtract(ent.origin, boxSize, ent.origin);
		}
		ent.alpha = 0.3;
	}

	/* if pendBox is true then ignore all the previous color considerations and use cyan */
	if (pendBox) {
		VectorSet(ent.angles, 0, 1, 1); /* Cyan */
		ent.alpha = 0.15;
	}

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Key binding for fast inventory access
 */
void CL_ActorInventoryOpen_f (void)
{
	if (CL_OnBattlescape()) {
		const menu_t* menu = MN_GetActiveMenu();
		if (!strstr(menu->name, "hudinv")) {
			if (!Q_strcmp(mn_hud->string, "althud"))
				MN_PushMenu("ahudinv", NULL);
			else
				MN_PushMenu("hudinv", NULL);
		} else
			MN_PopMenu(qfalse);
	}
}

/**
 * @brief Targets to the ground when holding the assigned button
 * @sa mousePosTargettingAlign
 */
void CL_ActorTargetAlign_f (void)
{
	int align = GROUND_DELTA;
	static int currentPos = 0;

	/* no firedef selected */
	if (!selFD || !selActor)
		return;
	if (cl.cmode != M_FIRE_R && cl.cmode != M_FIRE_L
	 && cl.cmode != M_PEND_FIRE_R && cl.cmode != M_PEND_FIRE_L)
		return;

	/* user defined height align */
	if (Cmd_Argc() == 2) {
		align = atoi(Cmd_Argv(1));
	} else {
		switch (currentPos) {
		case 0:
			if (selFD->gravity)
				align = -align;
			currentPos = 1; /* next level */
			break;
		case 1:
			/* only allow to align to lower z level if the actor is
			 * standing at a higher z-level */
			if (selFD->gravity)
				align = -(2 * align);
			else
				align = -align;
			currentPos = 2;
			break;
		case 2:
			/* the static var is not reseted on weaponswitch or actorswitch */
			if (selFD->gravity) {
				align = 0;
				currentPos = 0; /* next level */
			} else {
				align = -(2 * align);
				currentPos = 3; /* next level */
			}
			break;
		case 3:
			align = 0;
			currentPos = 0; /* back to start */
			break;
		}
	}
	mousePosTargettingAlign = align;
}

/**
 * @brief Adds a target cursor when we render the world.
 * @sa CL_TargetingStraight
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargetingBox
 * @sa CL_TraceMove
 * @sa V_RenderView
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddTargeting (void)
{
	if (mouseSpace != MS_WORLD)
		return;

	switch (cl.cmode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Display Move-cursor. */
		CL_AddTargetingBox(mousePos, qfalse);

		if (cl.cmode == M_PEND_MOVE) {
			/* Also display a box for the pending move if we have one. */
			CL_AddTargetingBox(mousePendPos, qtrue);
			if (!CL_TraceMove(mousePendPos))
				cl.cmode = M_MOVE;
		}
		break;
	case M_FIRE_R:
	case M_FIRE_L:
		if (!selActor || !selFD)
			return;

		if (!selFD->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, mousePos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, mousePos);
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (!selActor || !selFD)
			return;

		/* Draw cursor at mousepointer */
		CL_AddTargetingBox(mousePos, qfalse);

		/* Draw (pending) Cursor at target */
		CL_AddTargetingBox(mousePendPos, qtrue);

		if (!selFD->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, mousePendPos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, mousePendPos);
		break;
	default:
		break;
	}
}

static const vec3_t boxShift = { PLAYER_WIDTH, PLAYER_WIDTH, UNIT_HEIGHT / 2 - DIST_EPSILON };

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 */
static void CL_AddPathingBox (pos3_t pos)
{
	entity_t ent;
	int height; /**< The total opening size */
	int base; /**< The floor relative to this cell */

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	const int crouching_state = selActor && (selActor->state & STATE_CROUCHED) ? 1 : 0;
	const int TUneed = Grid_MoveLength(&clPathMap, pos, crouching_state, qfalse);
	const int TUhave = CL_UsableTUs(selActor);

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_PATH;

	Grid_PosToVec(clMap, fieldSize, pos, ent.origin);
	VectorSubtract(ent.origin, boxShift, ent.origin);

	base = Grid_Floor(clMap, fieldSize, pos);

	/**
	 * Paint the box green if it is reachable,
	 * yellow if it can be entered but is too far,
	 * or red if it cannot be entered ever.
	 */
	if (base < -PATHFINDING_MAX_FALL * QUANT) {
		VectorSet(ent.angles, 0.0, 0.0, 0.0); /* Can't enter - black */
	} else {
		/**
		 * Can reach - green
		 * Passable but unreachable - yellow
		 * Not passable - red
		 */
		VectorSet(ent.angles, (TUneed > TUhave), (TUneed != ROUTING_NOT_REACHABLE), 0);
	}

	/**
	 * Set the box height to the ceiling value of the cell.
	 */
	height = 2 + min(TUneed * (UNIT_HEIGHT - 2) / ROUTING_NOT_REACHABLE, 16);
	ent.oldorigin[2] = height;
	ent.oldorigin[0] = TUneed;
	ent.oldorigin[1] = TUhave;

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Adds a pathing marker to the current floor when we render the world.
 * @sa V_RenderView
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddPathing (void)
{
	pos3_t pos;

	pos[2] = cl_worldlevel->integer;
	for (pos[1] = max(mousePos[1] - 8, 0); pos[1] <= min(mousePos[1] + 8, PATHFINDING_WIDTH - 1); pos[1]++)
		for (pos[0] = max(mousePos[0] - 8, 0); pos[0] <= min(mousePos[0] + 8, PATHFINDING_WIDTH - 1); pos[0]++)
			CL_AddPathingBox(pos);
}

/**
 * @brief Plays various sounds on actor action.
 * @param[in] le The actor
 * @param[in] soundType Type of action (among actorSound_t) for which we need a sound.
 */
void CL_PlayActorSound (const le_t *le, actorSound_t soundType)
{
	const char *actorSound = Com_GetActorSound(le->teamDef, le->gender, soundType);
	if (actorSound) {
		sfx_t *sfx = S_RegisterSound(actorSound);
		if (sfx) {
			Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "CL_PlayActorSound: ActorSound: '%s'\n", actorSound);
			S_StartSound(le->origin, sfx, DEFAULT_SOUND_PACKET_VOLUME);
		}
	}
}

/**
 * @brief Shows a table of the TUs that would be used by the current actor to move
 * relative to its current location
 */
void CL_DumpTUs_f (void)
{
	int x, y, crouching_state;
	pos3_t pos, loc;

	if (!selActor)
		return;

	crouching_state = selActor->state & STATE_CROUCHED ? 1 : 0;
	VectorCopy(selActor->pos, pos);

	Com_Printf("TUs around (%i, %i, %i)\n", pos[0], pos[1], pos[2]);

	for (y = max(0, pos[1] - 8); y <= min(PATHFINDING_WIDTH, pos[1] + 8); y++) {
		for (x = max(0, pos[0] - 8); x <= min(PATHFINDING_WIDTH, pos[0] + 8); x++) {
			VectorSet(loc, x, y, pos[2]);
			Com_Printf("%3i ", Grid_MoveLength(&clPathMap, loc, crouching_state, qfalse));
		}
		Com_Printf("\n");
	}
	Com_Printf("TUs at (%i, %i, %i) = %i\n", pos[0], pos[1], pos[2], Grid_MoveLength(&clPathMap, pos, crouching_state, qfalse));
}

/**
 * @brief create an arrow between from and to with the specified color ratios
 */
static void CL_AddArrow (vec3_t from, vec3_t to, float red, float green, float blue)
{
	entity_t ent;

	/* Com_Printf("Adding arrow (%f, %f, %f) to (%f, %f, %f).\n", from[0], from[1], from[2], to[0], to[1], to[2]); */

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_ARROW;
	VectorCopy(from, ent.origin);
	VectorCopy(to, ent.oldorigin);
	VectorSet(ent.angles, red, green, blue);

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief
 */
void CL_DisplayFloorArrows (void)
{
	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	vec3_t base, start;

	Grid_PosToVec(clMap, fieldSize, truePos, base);
	VectorCopy(base, start);
	base[2] -= QUANT;
	start[2] += QUANT;

	RT_CheckCell(clMap, fieldSize, truePos[0], truePos[1], truePos[2]);

	/* Display floor arrow */
	if (VectorNotEmpty(brushesHit.floor))
		CL_AddArrow(base, brushesHit.floor, 0.0, 0.5, 1.0);

	/* Display ceiling arrow */
	if (VectorNotEmpty(brushesHit.ceiling))
		CL_AddArrow(start, brushesHit.ceiling, 1.0, 1.0, 0.0);
}

/**
 * @brief
 */
void CL_DisplayObstructionArrows (void)
{
	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	int dir;
	vec3_t base, start;

	Grid_PosToVec(clMap, fieldSize, truePos, base);

	for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
		RT_UpdateConnection(clMap, fieldSize, truePos[0], truePos[1], truePos[2], dir);

		VectorCopy(base, start);
		start[0] += dvecs[dir][0] * QUANT;
		start[1] += dvecs[dir][1] * QUANT;

		/* Display floor arrow */
		if (VectorNotEmpty(brushesHit.floor)){
			if (brushesHit.obstructed) {
				CL_AddArrow(start, brushesHit.floor, 1.0, 1.0, 1.0);
			} else {
				CL_AddArrow(start, brushesHit.floor, 1.0, 0.5, 0.0);
			}
		}

		/* Display ceiling arrow */
		if (VectorNotEmpty(brushesHit.ceiling)){
			CL_AddArrow(start, brushesHit.ceiling, 0.0, 1.0, 1.0);
		}
	}
}

/**
 * @brief Triggers @c Grid_MoveMark in every direction at the current truePos.
 */
void CL_DumpMoveMark_f (void)
{
	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	const int crouching_state = selActor
		? (selActor->state & STATE_CROUCHED ? 1 : 0)
		: 0;
	const int temp = developer->integer;

	developer->integer |= DEBUG_PATHING;

	CL_BuildForbiddenList();
	Grid_MoveCalc(clMap, fieldSize, &clPathMap, truePos, crouching_state, MAX_ROUTE, fb_list, fb_length);

	developer->integer ^= DEBUG_PATHING;

	CL_ConditionalMoveCalcForCurrentSelectedActor();
	developer->integer = temp;
}
