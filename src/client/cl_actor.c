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

/* public */
le_t *selActor;
fireDef_t *selFD;
character_t *selChr;
int selToHit;
pos3_t mousePos;

int actorMoveLength;
invList_t invList[MAX_INVLIST];
qboolean visible_firemode_list_left = qfalse;
qboolean visible_firemode_list_right = qfalse;
qboolean firemodes_change_display = qtrue; /* If this set to qfalse so CL_DisplayFiremodes_f will not attempt to hide the list */

typedef enum {
	RF_HAND,	/**< Stores the used hand (0=right, 1=left, -1=undef) */
	RF_FM,		/**< Stores the used firemode index. Max. number is MAX_FIREDEFS_PER_WEAPON -1=undef*/
	RF_WPIDX, 	/**< Stores the weapon idx in ods. (for faster access and checks) -1=undef */

	RF_MAX
} cl_reaction_firemode_type_t;

static int reactionFiremode[MAX_EDICTS][RF_MAX]; /** < Per actor: Stores the firemode to be used for reaction fire (if the fireDef allows that) See also reaction_firemode_type_t */

#define THIS_REACTION( actoridx, hand, fd_idx )	( reactionFiremode[actoridx][RF_HAND] == hand && reactionFiremode[actoridx][RF_FM] == fd_idx )
#define SANE_REACTION( actoridx )	(( (reactionFiremode[actoridx ][RF_HAND] >= 0) && (reactionFiremode[actoridx ][RF_FM] >=0 && reactionFiremode[actoridx ][RF_FM] < MAX_FIREDEFS_PER_WEAPON) && (reactionFiremode[actoridx ][RF_FM] >= 0) ))

static le_t *mouseActor;
static pos3_t mouseLastPos;
pos3_t mousePendPos; /* for double-click movement and confirmations ... */
reactionmode_t selActorReactionState; /* keep track of reaction toggle */
reactionmode_t selActorOldReactionState = R_FIRE_OFF; /* and another to help set buttons! */

/* to optimise painting of HUD in move-mode, keep track of some globals: */
le_t *lastHUDActor; /* keeps track of selActor */
int lastMoveLength; /* keeps track of actorMoveLength */
int lastTU; /* keeps track of selActor->TU */

/* a cbuf string for each button_types_t */
static const char *shoot_type_strings[BT_NUM_TYPES] = {
	"pr\n",
	"reaction\n",
	"sr\n",
	"\n",
	"pl\n",
	"\n",
	"sl\n",
	"\n",
	"rr\n",
	"rl\n",
	"stand\n",
	"crouch\n"
};

static int weaponButtonState[BT_NUM_TYPES];

/**
 * @brief Writes player action with its data
 */
void MSG_Write_PA (player_action_t player_action, int num, ...)
{
	va_list ap;
	va_start(ap, num);
	MSG_WriteFormat(&cls.netchan.message, "bbs", clc_action, player_action, num);
	MSG_V_WriteFormat(&cls.netchan.message, pa_format[player_action], ap);
	va_end(ap);
}


/*
==============================================================
ACTOR MENU UPDATING
==============================================================
*/

/**
 * @brief Return the skill string for the given skill level
 * @return skill string
 * @param[in] skill a skill value between 0 and MAX_SKILL (TODO: 0? never reached?)
 */
static char *CL_GetSkillString (const int skill)
{
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value (%i/%i)\n", skill, MAX_SKILL);
	}
#endif
	switch (skill*10/MAX_SKILL) {
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
		Com_Printf("CL_GetSkillString: Unknown skill: %i (index: %i)\n", skill, skill*10/MAX_SKILL);
		return "";
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
extern void CL_CharacterCvars (character_t *chr)
{
	assert(chr);

	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", Com_CharGetBody(chr));
	Cvar_ForceSet("mn_head", Com_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	Cvar_Set("mn_chrmis", va("%i", chr->assigned_missions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->kills[KILLED_ALIENS]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->kills[KILLED_TEAM]));

	/* Display rank if not in multiplayer (numRanks==0) and the character has one. */
	if (chr->rank >= 0 && gd.numRanks) {
		Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Rank: %s"), gd.ranks[chr->rank].name);
		Cvar_Set("mn_chrrank", messageBuffer);
		Com_sprintf(messageBuffer, sizeof(messageBuffer), "%s", gd.ranks[chr->rank].image);
		Cvar_Set("mn_chrrank_img", messageBuffer);
	} else {
		Cvar_Set("mn_chrrank", "");
		Cvar_Set("mn_chrrank_img", "");
	}

	Cvar_Set("mn_vpwr", va("%i", chr->skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", va("%i", chr->skills[ABILITY_MIND]));
	Cvar_Set("mn_vcls", va("%i", chr->skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->skills[SKILL_EXPLOSIVE]));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_POWER]), chr->skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_SPEED]), chr->skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_ACCURACY]), chr->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_MIND]), chr->skills[ABILITY_MIND]));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_CLOSE]), chr->skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_HEAVY]), chr->skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_ASSAULT]), chr->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_SNIPER]), chr->skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_EXPLOSIVE]), chr->skills[SKILL_EXPLOSIVE]));
}

/**
 * @brief Updates the UGV cvars for the given "character".
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param chr Pointer to character_t (may not be null)
 * @sa CL_CharacterCvars
 * @sa CL_ActorSelect
 */
extern void CL_UGVCvars (character_t *chr)
{
	assert(chr);

	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", Com_CharGetBody(chr));
	Cvar_ForceSet("mn_head", Com_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	Cvar_Set("mn_chrmis", va("%i", chr->assigned_missions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->kills[KILLED_ALIENS]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->kills[KILLED_TEAM]));
	Cvar_Set("mn_chrrank_img", "");
	Cvar_Set("mn_chrrank", "");
	Cvar_Set("mn_chrrank_img", "");

	Cvar_Set("mn_vpwr", va("%i", chr->skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", "0");
	Cvar_Set("mn_vcls", va("%i", chr->skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->skills[SKILL_EXPLOSIVE]));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_POWER]), chr->skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_SPEED]), chr->skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_GetSkillString(chr->skills[ABILITY_ACCURACY]), chr->skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (0)", CL_GetSkillString(chr->skills[ABILITY_MIND])));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_CLOSE]), chr->skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_HEAVY]), chr->skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_ASSAULT]), chr->skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_SNIPER]), chr->skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_GetSkillString(chr->skills[SKILL_EXPLOSIVE]), chr->skills[SKILL_EXPLOSIVE]));
}

/**
 * @brief Updates the global character cvars.
 */
static void CL_ActorGlobalCVars (void)
{
	le_t *le;
	char str[MAX_VAR];
	int i;

	Cvar_SetValue("mn_numaliensspotted", cl.numAliensSpotted);
	for (i = 0; i < MAX_TEAMLIST; i++) {
		le = cl.teamList[i];
		if (le && !(le->state & STATE_DEAD)) {
			/* the model name is the first entry in model_t */
			Cvar_Set(va("mn_head%i", i), (char *) le->model2);
			Com_sprintf(str, MAX_VAR, "%i", le->HP);
			Cvar_Set(va("mn_hp%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->maxHP);
			Cvar_Set(va("mn_hpmax%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->TU);
			Cvar_Set(va("mn_tu%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->maxTU);
			Cvar_Set(va("mn_tumax%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->morale);
			Cvar_Set(va("mn_morale%i",i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->maxMorale);
			Cvar_Set(va("mn_moralemax%i",i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->STUN);
			Cvar_Set(va("mn_stun%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->AP);
			Cvar_Set(va("mn_ap%i", i), str);
		} else {
			Cvar_Set(va("mn_head%i", i), "");
			Cvar_Set(va("mn_hp%i", i), "0");
			Cvar_Set(va("mn_hpmax%i", i), "1");
			Cvar_Set(va("mn_tu%i", i), "0");
			Cvar_Set(va("mn_tumax%i", i), "1");
			Cvar_Set(va("mn_morale%i",i), "0");
			Cvar_Set(va("mn_moralemax%i",i), "1");
			Cvar_Set(va("mn_stun%i", i), "0");
			Cvar_Set(va("mn_ap%i", i), "0");
		}
	}
}

/**
 * @brief Get state of the reaction-fire button.
 * @param[in] *le Pointer to local entity structure, a soldier.
 * @return R_FIRE_MANY when STATE_REACTION_MANY.
 * @return R_FIRE_ONCE when STATE_REACTION_ONCE.
 * @return R_FIRE_OFF when no reaction fire.
 * @sa CL_RefreshWeaponButtons
 * @sa CL_ActorUpdateCVars
 * @sa CL_ActorSelect
 */
static int CL_GetReactionState (le_t *le)
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
 * @param[in] weapon_id Item in (currently only right) hand.
 * @return Time needed to reload or >= 999 if no suitable ammo found.
 * @note This routine assumes the time to reload a weapon
 * @note in the right hand is the same as the left hand.
 * @TODO Distinguish between LEFT(selActor) and RIGHT(selActor).
 * @sa CL_RefreshWeaponButtons
 * @sa CL_CheckMenuAction
 */
static int CL_CalcReloadTime (int weapon_id)
{
	invList_t *ic;
	int container;
	int tu = 999;

	if (!selActor)
		return tu;

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			for (ic = selActor->i.c[container]; ic; ic = ic->next)
				if (INV_LoadableInWeapon(&csi.ods[ic->item.t], weapon_id)) {
					tu = csi.ids[container].out;
					break;
				}
		}
	}

	/* total TU cost is the sum of 3 numbers:
	   TU for weapon reload + TU to get ammo out + TU to put ammo in hands */
	tu += csi.ods[weapon_id].reload + csi.ids[csi.idRight].in;
	return tu;
}

/**
 * @brief
 */
static void ClearHighlights (void)
{
	int i;

	for (i = 0; i < BT_NUM_TYPES; i++)
		if (weaponButtonState[i] == 2) {
			weaponButtonState[i] = -1;
			return;
		}
}

#if 0
/**
 * @brief
 */
static void HighlightWeaponButton (int button)
{
	char cbufText[MAX_VAR];

	/* only one button can be highlighted at once, so reset other buttons */
	ClearHighlights();

	Q_strncpyz(cbufText, "sel", MAX_VAR);
	Q_strcat(cbufText, shoot_type_strings[button], MAX_VAR);
	Cbuf_AddText(cbufText);
	weaponButtonState[button] = 2;
}
#endif

/**
 * @brief
 */
void CL_ResetWeaponButtons (void)
{
	memset(weaponButtonState, -1, sizeof(weaponButtonState));
}

/**
 * @brief Sets the display for a single weapon/reload HUD button
 */
static void SetWeaponButton (int button, int state)
{
	char cbufText[MAX_VAR];
	int currentState = weaponButtonState[button];

	/* don't reset it already in current state or if highlighted,
	 * as HighlightWeaponButton deals with the highlighted state */
	if (currentState == state || currentState == 2)
		return;

	if (state == 1)
		Q_strncpyz(cbufText, "desel", sizeof(cbufText));
	else
		Q_strncpyz(cbufText, "dis", sizeof(cbufText));

	Q_strcat(cbufText, shoot_type_strings[button], sizeof(cbufText));

	Cbuf_AddText(cbufText);
	weaponButtonState[button] = state;
}

/**
 * @brief Returns the number of the actor in the teamlist.
 * @param[in] le The actor to search.
 * @return The number of the actor in the teamlist.
 */
static int CL_GetActorNumber (le_t * le)
{
	int actor_idx;
	for (actor_idx = 0; actor_idx < cl.numTeamList; actor_idx++) {
		if (cl.teamList[actor_idx] == le)
			return actor_idx;
	}
	return -1;
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
		Cbuf_AddText(va("set_left_inv%i\n", i));
		Cbuf_AddText(va("set_right_inv%i\n", i));
	}

}

/**
 * @brief Stores the given firedef index and object index for reaction fire and sends in over the network as well.
 * @param[in] actor_idx Index of an actor.
 * @param[in] handidx Index of hand with item, which will be used for reactionfire. Possible hand indices: 0=right, 1=right, -1=undef
 * @param[in] fd_idx Index of firedefinition for an item in given hand.
 */
static void CL_SetReactionFiremode (int actor_idx, int handidx, int obj_idx, int fd_idx)
{
	le_t *le;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		/* This check is just here (additional to the one in CL_DisplayFiremodes_f) in case a possible situation was missed. */
		Com_DPrintf("CL_SetReactionFiremode: Function called on enemy/other turn.\n");
		return;
	}

	if (actor_idx < 0 || actor_idx >= MAX_EDICTS) {
		Com_DPrintf("CL_SetReactionFiremode: Actor index is negative or out of bounds. Abort.\n");
		return;
	}

	if (handidx < -1 || handidx > 1) {
		Com_DPrintf("CL_SetReactionFiremode: Bad hand index given. Abort.\n");
		return;
	}

	assert(actor_idx < MAX_TEAMLIST);

	le = cl.teamList[actor_idx];
	Com_DPrintf("CL_SetReactionFiremode: actor:%i entnum:%i hand:%i fd:%i\n", actor_idx,le->entnum, handidx, fd_idx);

	reactionFiremode[actor_idx][RF_HAND] = handidx;	/* Store the given hand. */
	reactionFiremode[actor_idx][RF_FM] = fd_idx;	/* Store the given firemode for this hand. */
	MSG_Write_PA(PA_REACT_SELECT, le->entnum, handidx, fd_idx); /* Send hand and firemode to server-side storage as well. See g_local.h for more. */
	reactionFiremode[actor_idx][RF_WPIDX] = obj_idx;	/* Store the weapon-idx of the object in the hand (for faster access). */
}

/**
 * @brief Sets the display for a single weapon/reload HUD button.
 * @param[in] *fd Pointer to the firedefinition/firemode to be displayed.
 * @param[in] hand Which list to display: 'l' for left hand list, 'r' for right hand list.
 * @param[in] status Display the firemode clickable/active (1) or inactive (0).
 */
static void CL_DisplayFiremodeEntry (fireDef_t *fd, char hand, qboolean status)
{
	/* char cbufText[MAX_VAR]; */
	if (!fd)
		return;

	if (!selActor)
		return;

	if (hand == 'r') {
		Cbuf_AddText(va("set_right_vis%i\n", fd->fd_idx)); /* Make this entry visible (in case it wasn't). */

		if (status) {
			Cbuf_AddText(va("set_right_a%i\n", fd->fd_idx));
		} else {
			Cbuf_AddText(va("set_right_ina%i\n", fd->fd_idx));
		}

		if (selActor->TU > fd->time)
			Cvar_Set(va("mn_r_fm_tt_tu%i", fd->fd_idx),va(_("Remaining TUs: %i"),selActor->TU - fd->time));
		else
			Cvar_Set(va("mn_r_fm_tt_tu%i", fd->fd_idx),_("No remaining TUs left after shot."));

		Cvar_Set(va("mn_r_fm_name%i", fd->fd_idx),  va("%s", fd->name));
		Cvar_Set(va("mn_r_fm_tu%i", fd->fd_idx),va(_("TU: %i"),  fd->time));
		Cvar_Set(va("mn_r_fm_shot%i", fd->fd_idx), va(_("Shots:%i"), fd->ammo));

	} else if (hand == 'l') {
		Cbuf_AddText(va("set_left_vis%i\n", fd->fd_idx)); /* Make this entry visible (in case it wasn't). */

		if (status) {
			Cbuf_AddText(va("set_left_a%i\n", fd->fd_idx));
		} else {
			Cbuf_AddText(va("set_left_ina%i\n", fd->fd_idx));
		}

		if (selActor->TU > fd->time)
			Cvar_Set(va("mn_l_fm_tt_tu%i", fd->fd_idx),va(_("Remaining TUs: %i"),selActor->TU - fd->time));
		else
			Cvar_Set(va("mn_l_fm_tt_tu%i", fd->fd_idx),_("No remaining TUs left after shot."));

		Cvar_Set(va("mn_l_fm_name%i", fd->fd_idx),  va("%s", fd->name));
		Cvar_Set(va("mn_l_fm_tu%i", fd->fd_idx),va(_("TU: %i"),  fd->time));
		Cvar_Set(va("mn_l_fm_shot%i", fd->fd_idx), va(_("Shots:%i"), fd->ammo));
	} else {
		Com_Printf("CL_DisplayFiremodeEntry: Bad hand [l|r] defined: '%c'\n", hand);
		return;
	}
}

/**
 * @brief Returns the weapon its ammo and the firemodes-index inside the ammo for a given hand.
 * @param[in] hand Which weapon(-hand) to use [l|r].
 * @param[out] weapon The weapon in the hand.
 * @param[out] ammo The ammo used in the weapon (is the same as weapon for grenades and similar).
 * @param[out] weap_fd_idx weapon_mod index in the ammo for the weapon (objDef.fd[x][])
 */
static void CL_GetWeaponAndAmmo (char hand, objDef_t **weapon, objDef_t **ammo, int *weap_fd_idx)
{
	invList_t *invlist_weapon = NULL;

	if (!selActor)
		return;

	if (hand == 'r')
		invlist_weapon = RIGHT(selActor);
	else
		invlist_weapon = LEFT(selActor);

	if (!invlist_weapon || invlist_weapon->item.t == NONE)
		return;

	*weapon = &csi.ods[invlist_weapon->item.t];

	if (!weapon)
		return;

	if ((*weapon)->numWeapons) /* TODO: "|| invlist_weapon->item.m == NONE" ... actually what does a negative number for ammo mean? */
		*ammo = *weapon; /* This weapon doesn't need ammo it already has firedefs */
	else
		*ammo = &csi.ods[invlist_weapon->item.m];

	*weap_fd_idx = INV_FiredefsIDXForWeapon(*ammo, invlist_weapon->item.t);
}

/**
 * @brief Updates the information in reactionFiremode for the selected actor with the given data from the parameters.
 * @param[in] hand Which weapon(-hand) to use (l|r).
 * @param[in] active_firemode Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
static void CL_UpdateReactionFiremodes (char hand, int actor_idx, int active_firemode)
{
	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weap_fd_idx = -1;
	int i;

	int handidx = (hand == 'r') ? 0 : 1;

	CL_GetWeaponAndAmmo(hand, &weapon, &ammo, &weap_fd_idx);
	if (weap_fd_idx == -1)
		return;

	if (!weapon) {
		Com_DPrintf("CL_UpdateReactionFiremodes: No weapon found for %c hand.\n", hand);
		return;
	}

	if (!RS_ItemIsResearched(csi.ods[ammo->weap_idx[weap_fd_idx]].id)) {
		Com_DPrintf("CL_UpdateReactionFiremodes: Weapon '%s' not researched, can't use for reaction fire.\n", csi.ods[ammo->weap_idx[weap_fd_idx]].id);
		return;
	}

	if (active_firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("CL_UpdateReactionFiremodes: Firemode index to big (%i). Highest possible number is %i.\n", active_firemode, MAX_FIREDEFS_PER_WEAPON-1);
		return;
	}

	if (active_firemode < 0) {
		/* Set default reaction firemode. */
		i = Com_GetDefaultReactionFire(ammo, weap_fd_idx);
		CL_SetReactionFiremode(actor_idx, handidx, ammo->weap_idx[weap_fd_idx], i);
		return;
	}

	if (actor_idx < 0)
		return;

	Com_DPrintf("CL_UpdateReactionFiremodes: act%i handidx%i weapfdidx%i\n", actor_idx, handidx, weap_fd_idx);
	if (reactionFiremode[actor_idx][RF_WPIDX] == ammo->weap_idx[weap_fd_idx]
	 && reactionFiremode[actor_idx][RF_HAND] == handidx) {
		if (ammo->fd[weap_fd_idx][active_firemode].reaction) {
			if (reactionFiremode[actor_idx][RF_FM] == active_firemode)
				/* Weapon is the same, firemode is already selected and reaction-usable. Nothing to do. */
				return;
		} else {
			/* Weapon is the same and firemode is not reaction-usable*/
			return;
		}
	}

	/* Search for a (reaction) firemode with the given index and store/send it. */
	CL_SetReactionFiremode( actor_idx, -1, -1, -1 );
	for (i = 0; i < ammo->numFiredefs[weap_fd_idx]; i++) {
		if (ammo->fd[weap_fd_idx][i].reaction) {
			if (i == active_firemode) {
				CL_SetReactionFiremode( actor_idx, handidx, ammo->weap_idx[weap_fd_idx], i );
				return;
			}
		}
	}
}

/**
 * @brief Displays the firemodes for the given hand.
 */
void CL_DisplayFiremodes_f (void)
{
	int actor_idx;
	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weap_fd_idx = -1;
	int i;
	char *hand;

	if (!selActor)
		return;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		HideFiremodes();
		visible_firemode_list_right = qfalse;
		visible_firemode_list_left = qfalse;
		return;
	}

	if (Cmd_Argc() < 2) { /* no argument given */
		hand = "r";
	} else {
		hand = Cmd_Argv(1);
	}

	if (hand[0] != 'r' && hand[0] != 'l') {
		Com_Printf("Usage: list_firemodes [l|r]\n");
		return;
	}

	CL_GetWeaponAndAmmo(hand[0], &weapon, &ammo, &weap_fd_idx);
	if (weap_fd_idx == -1)
		return;

	Com_DPrintf("CL_DisplayFiremodes_f: %s | %s | %i\n", weapon->name, ammo->name, weap_fd_idx);

	if (!weapon || !ammo) {
		Com_DPrintf("CL_DisplayFiremodes_f: no weapon or ammo found.\n");
		return;
	}

	Com_DPrintf("CL_DisplayFiremodes_f: displaying %s firemodes.\n", hand);

	if (firemodes_change_display) {
		/* Toggle firemode lists if needed. Mind you that HideFiremodes modifies visible_firemode_list_xxx to qfalse */
		if (hand[0] == 'r') {
			if (visible_firemode_list_right == qtrue) {
				HideFiremodes(); /* Modifies visible_firemode_list_xxxx */
				return;
			} else {
				HideFiremodes();
				visible_firemode_list_left = qfalse;
				visible_firemode_list_right = qtrue;
			}
		} else  { /* 'l' */
			if (visible_firemode_list_left == qtrue) {
				HideFiremodes(); /* Modifies visible_firemode_list_xxxx */
				return;
			} else {
				HideFiremodes(); /* Modifies visible_firemode_list_xxxx */
				visible_firemode_list_left = qtrue;
				visible_firemode_list_right = qfalse;
			}
		}
	}
	firemodes_change_display = qtrue;

	actor_idx = CL_GetActorNumber(selActor);
	Com_DPrintf("CL_DisplayFiremodes_f: actor index: %i\n", actor_idx);

	/* Set default firemode if the currenttly seleced one is not sane or for another weapon. */
	if (!SANE_REACTION(actor_idx)) {	/* No sane firemode selected. */
		/* Set default firemode (try right hand first, then left hand). */
		CL_UpdateReactionFiremodes('r', actor_idx, -1);
		if (!SANE_REACTION(actor_idx)) {
			CL_UpdateReactionFiremodes('l', actor_idx, -1);
		}
	} else {
		if (reactionFiremode[actor_idx][RF_WPIDX] != ammo->weap_idx[weap_fd_idx]) {
			if ((hand[0] == 'r') && (reactionFiremode[actor_idx][RF_HAND] == 0)) {
				/* Set default firemode (try right hand first, then left hand). */
				CL_UpdateReactionFiremodes('r', actor_idx, -1);
				if (!SANE_REACTION(actor_idx)) {
					CL_UpdateReactionFiremodes('l', actor_idx, -1);
				}
			}
			if ((hand[0] == 'l') && (reactionFiremode[actor_idx][RF_HAND] == 1)) {
				/* Set default firemode (try left hand first, then right hand). */
				CL_UpdateReactionFiremodes('l', actor_idx, -1);
				if (!SANE_REACTION(actor_idx)) {
					CL_UpdateReactionFiremodes('r', actor_idx, -1);
				}
			}
		}
	}

	for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
		if (i < ammo->numFiredefs[weap_fd_idx]) { /* We have a defined fd ... */
			/* Display the firemode information (image + text). */
			if (ammo->fd[weap_fd_idx][i].time <= selActor->TU ) {  /* Enough timeunits for this firemode?*/
				CL_DisplayFiremodeEntry(&ammo->fd[weap_fd_idx][i], hand[0], qtrue);
			} else{
				CL_DisplayFiremodeEntry(&ammo->fd[weap_fd_idx][i], hand[0], qfalse);
			}

			/* Display checkbox for reaction firemode (this needs a sane reactionFiremode array) */
			if (ammo->fd[weap_fd_idx][i].reaction) {
				Com_DPrintf("CL_DisplayFiremodes_f: This is a reaction firemode: %i\n", i);
				if (hand[0] == 'r') {
					if (THIS_REACTION(actor_idx,0,i)) {
						/* Set this checkbox visible+active. */
						Cbuf_AddText(va("set_right_cb_a%i\n", i));
					} else {
						/* Set this checkbox visible+inactive. */
						Cbuf_AddText(va("set_right_cb_ina%i\n", i));
					}
				} else { /* hand[0] == 'l' */
					if (THIS_REACTION(actor_idx,1,i)) {
						/* Set this checkbox visible+active. */
						Cbuf_AddText(va("set_left_cb_a%i\n", i));
					} else {
						/* Set this checkbox visible+active. */
						Cbuf_AddText(va("set_left_cb_ina%i\n", i));
					}
				}
			}

		} else { /* No more fd left in the list or weapon not researched. */
			if (hand[0] == 'r')
				Cbuf_AddText(va("set_right_inv%i\n", i)); /* Hide this entry */
			else
				Cbuf_AddText(va("set_left_inv%i\n", i)); /* Hide this entry */
		}
	}
}

/**
 * @brief Checks if the selected firemode checkbox is ok as a reaction firemode and updates data+display.
 */
void CL_SelectReactionFiremode_f (void)
{
	char *hand;
	int firemode;
	int actor_idx = -1;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: sel_reactmode [l|r] <num>   num=firemode number\n");
		return;
	}

	hand = Cmd_Argv(1);

	if (hand[0] != 'r' && hand[0] != 'l') {
		Com_Printf("Usage: sel_reactmode [l|r] <num>   num=firemode number\n");
		return;
	}

	if (!selActor)
		return;

	actor_idx = CL_GetActorNumber(selActor);

	firemode = atoi(Cmd_Argv(2));

	if (firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("CL_SelectReactionFiremode_f: Firemode index to big (%i). Highest possible number is %i.\n", firemode, MAX_FIREDEFS_PER_WEAPON-1);
		return;
	}

	CL_UpdateReactionFiremodes(hand[0], actor_idx, firemode);

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
	char *hand;
	int firemode;

	objDef_t *weapon = NULL;
	objDef_t *ammo = NULL;
	int weap_fd_idx = -1;

	if (Cmd_Argc() < 3) { /* no argument given */
		Com_Printf("Usage: fireweap [l|r] <num>   num=firemode number\n");
		return;
	}

	hand = Cmd_Argv(1);

	if (hand[0] != 'r' && hand[0] != 'l') {
		Com_Printf("Usage: fireweap [l|r] <num>   num=firemode number\n");
		return;
	}

	if (!selActor)
		return;

	firemode = atoi(Cmd_Argv(2));

	if (firemode >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("CL_FireWeapon_f: Firemode index to big (%i). Highest possible number is %i.\n", firemode, MAX_FIREDEFS_PER_WEAPON-1);
		return;
	}

	CL_GetWeaponAndAmmo(hand[0], &weapon, &ammo, &weap_fd_idx);
	if (weap_fd_idx == -1)
		return;

	/* Let's check if shooting is possible.
	 * Don't let the selActor->TU parameter irritate you, it is not checked/used here. */
	if (hand[0] == 'r') {
		if(!CL_CheckMenuAction(selActor->TU, RIGHT(selActor), EV_INV_AMMO))
			return;
	} else {
		if(!CL_CheckMenuAction(selActor->TU, LEFT(selActor), EV_INV_AMMO))
			return;
	}

	if (ammo->fd[weap_fd_idx][firemode].time <= selActor->TU) {
		/* Actually start aiming. This is done by changing the current mode of display. */
		if (hand[0] == 'r')
			cl.cmode = M_FIRE_R;
		else
			cl.cmode = M_FIRE_L;
		cl.cfiremode = firemode;	/* Store firemode. */
		HideFiremodes();
	} else {
		/* Cannot shoot because of not enough TUs - every other
		   case should be checked previously in this function. */
		CL_DisplayHudMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
		Com_DPrintf("CL_FireWeapon_f: Firemode not available (%s, %s).\n", hand, ammo->fd[weap_fd_idx][firemode].name);
		return;
	}
}

/**
 * @brief Checks if a there is a weapon in hte hand that can be used for reaction fire.
 * @param[in] hand Which hand to check: 'l' for left hand, 'r' for right hand.
 */
static qboolean CL_WeaponWithReaction (char hand)
{
	objDef_t *ammo = NULL;
	objDef_t *weapon = NULL;
	int weap_fd_idx;
	int i;

	/* Get ammo and weapon-index in ammo. */
	CL_GetWeaponAndAmmo(hand, &weapon, &ammo, &weap_fd_idx);

	if (weap_fd_idx == -1) {
		Com_DPrintf("CL_WeaponWithReaction: No weapondefinition in ammo found\n");
		return qfalse;
	}

	if (!weapon || !ammo)
		return qfalse;

	/* check ammo for reaction-enabled firemodes */
	for (i = 0; i < ammo->numFiredefs[weap_fd_idx]; i++) {
		if (ammo->fd[weap_fd_idx][i].reaction) {
			return qtrue;
		}
	}

	return qfalse;
}

/**
 * @brief Refreshes the weapon/reload buttons on the HUD.
 * @param[in] time The amount of TU (of an actor) left in case of action.
 * @sa CL_ActorUpdateCVars
 */
static void CL_RefreshWeaponButtons (int time)
{
	invList_t *weaponr, *weaponl = NULL;
	int minweaponrtime = 100, minweaponltime = 100;
	int weaponr_fds_idx = -1, weaponl_fds_idx = -1;
	qboolean isammo = qfalse;
	int i;

	if (!selActor)
		return;

	weaponr = RIGHT(selActor);

	/* check for two-handed weapon - if not, also define weaponl */
	if (!weaponr || !csi.ods[weaponr->item.t].holdtwohanded)
		weaponl = LEFT(selActor);

	/* crouch/stand button */
	if (selActor->state & STATE_CROUCHED) {
		weaponButtonState[BT_STAND] = -1;
		if (time < TU_CROUCH)
			SetWeaponButton(BT_CROUCH, qfalse);
		else
			SetWeaponButton(BT_CROUCH, qtrue);
	} else {
		weaponButtonState[BT_CROUCH] = -1;
		if (time < TU_CROUCH)
			SetWeaponButton(BT_STAND, qfalse);
		else
			SetWeaponButton(BT_STAND, qtrue);
	}

	/* reaction-fire button */
	if (CL_GetReactionState(selActor) == R_FIRE_OFF) {
		if ((time >= TU_REACTION_SINGLE)
		&& (CL_WeaponWithReaction('r') || CL_WeaponWithReaction('l')))
			SetWeaponButton(BT_RIGHT_PRIMARY_REACTION, qtrue);
		else
			SetWeaponButton(BT_RIGHT_PRIMARY_REACTION, qfalse);

	}

	/* reload buttons */
	if (!weaponr || weaponr->item.m == NONE
		 || !csi.ods[weaponr->item.t].reload
		 || time < CL_CalcReloadTime(weaponr->item.t))
		SetWeaponButton(BT_RIGHT_RELOAD, qfalse);
	else
		SetWeaponButton(BT_RIGHT_RELOAD, qtrue);

	if (!weaponl || weaponl->item.m == NONE
		 || !csi.ods[weaponl->item.t].reload
		 || time < CL_CalcReloadTime(weaponl->item.t))
		SetWeaponButton(BT_LEFT_RELOAD, qfalse);
	else
		SetWeaponButton(BT_LEFT_RELOAD, qtrue);

	/* Weapon firing buttons. */
	if (weaponr) {
		assert(weaponr->item.t != NONE);
		/* Check whether this item use ammo. */
		if (weaponr->item.m == NONE) {
			/* This item does not use ammo, check for existing firedefs in this item. */
			if (csi.ods[weaponr->item.t].numFiredefs > 0) {
				/* Get firedef from the weapon entry instead. */
				weaponr_fds_idx = INV_FiredefsIDXForWeapon(&csi.ods[weaponr->item.t], weaponr->item.t);
			} else {
				weaponr_fds_idx = -1;
			}
			isammo = qfalse;
		} else {
			/* This item uses ammo, get the firedefs from ammo. */
			weaponr_fds_idx = INV_FiredefsIDXForWeapon(&csi.ods[weaponr->item.m], weaponr->item.t);
			isammo = qtrue;
		}
		if (isammo) {
			/* Search for the smallest TU needed to shoot. */
			if (weaponr_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!csi.ods[weaponr->item.m].fd[weaponr_fds_idx][i].time)
						continue;
					if (csi.ods[weaponr->item.m].fd[weaponr_fds_idx][i].time < minweaponrtime)
					minweaponrtime = csi.ods[weaponr->item.m].fd[weaponr_fds_idx][i].time;
				}
		} else {
			if (weaponr_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!csi.ods[weaponr->item.t].fd[weaponr_fds_idx][i].time)
						continue;
					if (csi.ods[weaponr->item.t].fd[weaponr_fds_idx][i].time < minweaponrtime)
						minweaponrtime = csi.ods[weaponr->item.t].fd[weaponr_fds_idx][i].time;
				}
		}
		if (time < minweaponrtime)
			SetWeaponButton(BT_RIGHT_PRIMARY, qfalse);
		else
			SetWeaponButton(BT_RIGHT_PRIMARY, qtrue);
	} else {
		SetWeaponButton(BT_RIGHT_PRIMARY, qfalse);
	}

	if (weaponl) {
		assert(weaponl->item.t != NONE);
		/* Check whether this item use ammo. */
		if (weaponl->item.m == NONE) {
			/* This item does not use ammo, check for existing firedefs in this item. */
			if (csi.ods[weaponl->item.t].numFiredefs > 0) {
				/* Get firedef from the weapon entry instead. */
				weaponl_fds_idx = INV_FiredefsIDXForWeapon(&csi.ods[weaponl->item.t], weaponl->item.t);
			} else {
				weaponl_fds_idx = -1;
			}
			isammo = qfalse;
		} else {
			/* This item uses ammo, get the firedefs from ammo. */
			weaponl_fds_idx = INV_FiredefsIDXForWeapon(&csi.ods[weaponl->item.m], weaponl->item.t);
			isammo = qtrue;
		}
		if (isammo) {
			/* Search for the smallest TU needed to shoot. */
			if (weaponl_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!csi.ods[weaponl->item.m].fd[weaponl_fds_idx][i].time)
						continue;
					if (csi.ods[weaponl->item.m].fd[weaponl_fds_idx][i].time < minweaponltime)
						minweaponltime = csi.ods[weaponl->item.m].fd[weaponl_fds_idx][i].time;
				}
		} else {
			if (weaponl_fds_idx != -1)
				for (i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++) {
					if (!csi.ods[weaponl->item.t].fd[weaponl_fds_idx][i].time)
						continue;
					if (csi.ods[weaponl->item.t].fd[weaponl_fds_idx][i].time < minweaponltime)
						minweaponltime = csi.ods[weaponl->item.t].fd[weaponl_fds_idx][i].time;
				}
		}
		if (time < minweaponltime)
			SetWeaponButton(BT_LEFT_PRIMARY, qfalse);
		else
			SetWeaponButton(BT_LEFT_PRIMARY, qtrue);
	} else {
		SetWeaponButton(BT_LEFT_PRIMARY, qfalse);
	}
}

/**
 * @brief Checks whether an action on hud menu is valid and displays proper message.
 * @param[in] time The amount of TU (of an actor) left.
 * @param[in] *weapon An item in hands.
 * @param[in] mode EV_INV_AMMO in case of fire button, EV_INV_RELOAD in case of reload button
 * @return qfalse when action is not possible, otherwise qtrue
 * @sa CL_FireWeapon_f
 * @sa CL_ReloadLeft_f
 * @sa CL_ReloadRight_f
 * @todo Check for ammo in hand and give correct feedback in all cases.
 */
extern qboolean CL_CheckMenuAction (int time, invList_t *weapon, int mode)
{
	/* No item in hand. */
	/* TODO: ignore this condition when ammo in hand */
	if (!weapon || weapon->item.t == NONE) {
		CL_DisplayHudMessage(_("No item in hand.\n"), 2000);
		return qfalse;
	}

	switch (mode) {
	case EV_INV_AMMO:
		/* Check if shooting is possible.
		 * i.e. The weapon has ammo and can be fired with the 'available' hands.
		 * TUs (i.e. "time") are are _not_ checked here, this needs to be done
		 * elsewhere for the correct firemode. */

		/* Cannot shoot because of lack of ammo. */
		if (weapon->item.a <= 0 && csi.ods[weapon->item.t].reload) {
			CL_DisplayHudMessage(_("Can't perform action:\nout of ammo.\n"), 2000);
			return qfalse;
		}
		/* Cannot shoot because weapon is firetwohanded, yet both hands handle items. */
		if (csi.ods[weapon->item.t].firetwohanded && LEFT(selActor)) {
			CL_DisplayHudMessage(_("This weapon cannot be fired\none handed.\n"), 2000);
			return qfalse;
		}
		break;
	case EV_INV_RELOAD:
		/* Check if reload is possible. Also checks for the correct amount of TUs. */

		/* Cannot reload because this item is not reloadable. */
		if (!csi.ods[weapon->item.t].reload) {
			CL_DisplayHudMessage(_("Can't perform action:\nthis item is not reloadable.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of no ammo in inventory. */
		if (CL_CalcReloadTime(weapon->item.t) >= 999) {
			CL_DisplayHudMessage(_("Can't perform action:\nammo not available.\n"), 2000);
			return qfalse;
		}
		/* Cannot reload because of not enough TUs. */
		if (time < CL_CalcReloadTime(weapon->item.t)) {
			CL_DisplayHudMessage(_("Can't perform action:\nnot enough TUs.\n"), 2000);
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
void CL_ActorUpdateCVars (void)
{
	static char infoText[MAX_SMALLMENUTEXTLEN];
	static char mouseText[MAX_SMALLMENUTEXTLEN];
	qboolean refresh;
	char *name;
	int time;

	if (cls.state != ca_active)
		return;

	refresh = Cvar_VariableInteger("hud_refresh");
	if (refresh) {
		Cvar_Set("hud_refresh", "0");
		Cvar_Set("cl_worldlevel", cl_worldlevel->string);
		CL_ResetWeaponButtons();
	}

	/* set Cvars for all actors */
	CL_ActorGlobalCVars();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	if (selActor) {
		invList_t *selWeapon;

		/* set generic cvars */
		Cvar_Set("mn_tu", va("%i", selActor->TU));
		Cvar_Set("mn_tumax", va("%i", selActor->maxTU));
		Cvar_Set("mn_morale", va("%i", selActor->morale));
		Cvar_Set("mn_moralemax", va("%i", selActor->maxMorale));
		Cvar_Set("mn_hp", va("%i", selActor->HP));
		Cvar_Set("mn_hpmax", va("%i", selActor->maxHP));
		Cvar_Set("mn_stun", va("%i", selActor->STUN));
		Cvar_Set("mn_ap", va("%i", selActor->AP));

		/* animation and weapons */
		name = re.AnimGetName(&selActor->as, selActor->model1);
		if (name)
			Cvar_Set("mn_anim", name);
		if (RIGHT(selActor))
			Cvar_Set("mn_rweapon", csi.ods[RIGHT(selActor)->item.t].model);
		if (LEFT(selActor))
			Cvar_Set("mn_lweapon", csi.ods[LEFT(selActor)->item.t].model);

		/* get weapon */
		selWeapon = IS_MODE_FIRE_LEFT(cl.cmode) ? LEFT(selActor) : RIGHT(selActor);

		if (!selWeapon && RIGHT(selActor) && csi.ods[RIGHT(selActor)->item.t].holdtwohanded)
			selWeapon = RIGHT(selActor);

		if (selWeapon) {
			if (selWeapon->item.t == NONE) {
				/* No valid weapon in the hand. */
				selFD = NULL;
			} else {
#ifdef PARANOID
				GET_FIREDEFDEBUG(
					selWeapon->item.m,
					INV_FiredefsIDXForWeapon(
						&csi.ods[selWeapon->item.m],
						selWeapon->item.t),
					cl.cfiremode);
#endif
				/* Check whether this item uses/has ammo. */
				if (selWeapon->item.m == NONE) {
					/* This item does not use ammo, check for existing firedefs in this item. */
					if (csi.ods[selWeapon->item.t].numFiredefs > 0) {
						/* Get firedef from the weapon entry instead. */
						selFD = GET_FIREDEF(
							selWeapon->item.t,
							INV_FiredefsIDXForWeapon(
								&csi.ods[selWeapon->item.t],
								selWeapon->item.t),
							cl.cfiremode);
					} else {
						/* No firedefinitions found in this presumed 'weapon with no ammo'. */
						selFD = NULL;
					}
				} else {
					/* This item uses ammo, get the firedefs from ammo. */
					selFD = GET_FIREDEF(
						selWeapon->item.m,
						INV_FiredefsIDXForWeapon(
							&csi.ods[selWeapon->item.m],
							selWeapon->item.t),
						cl.cfiremode);
				}
			}
		}

		/* write info */
		time = 0;

		/* display special message */
		if (cl.time < cl.msgTime)
			Com_sprintf(infoText, sizeof(infoText), cl.msgText);

		/* update HUD stats etc in more or shoot modes */
		if (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE) {
			/* If the mouse is outside the world, blank move */
			/* or the movelength is 255 - not reachable e.g. */
			if ((mouseSpace != MS_WORLD && cl.cmode < M_PEND_MOVE) || actorMoveLength == 0xFF) {
				/* TODO: CHECKME Why do we check for (cl.cmode < M_PEND_MOVE) here? */
				actorMoveLength = 0xFF;
				Com_sprintf(infoText, sizeof(infoText), _("Armour  %i\tMorale  %i\n"), selActor->AP, selActor->morale);
				menuText[TEXT_MOUSECURSOR_RIGHT] = NULL;
			}
			if (cl.cmode != cl.oldcmode || refresh || lastHUDActor != selActor
						|| lastMoveLength != actorMoveLength || lastTU != selActor->TU) {
				if (actorMoveLength != 0xFF) {
					CL_RefreshWeaponButtons(selActor->TU - actorMoveLength);
					Com_sprintf(infoText, sizeof(infoText), _("Armour  %i\tMorale  %i\nMove %i (%i TU left)\n"), selActor->AP, selActor->morale, actorMoveLength, selActor->TU - actorMoveLength);
					if ( actorMoveLength <= selActor->TU )
						Com_sprintf(mouseText, sizeof(mouseText), "%i (%i)\n", actorMoveLength, selActor->TU);
					else
						Com_sprintf(mouseText, sizeof(mouseText), "- (-)\n" );
				} else {
					CL_RefreshWeaponButtons(selActor->TU);
				}
				lastHUDActor = selActor;
				lastMoveLength = actorMoveLength;
				lastTU = selActor->TU;
				menuText[TEXT_MOUSECURSOR_RIGHT] = mouseText;
			}
			time = actorMoveLength;
		} else {
			menuText[TEXT_MOUSECURSOR_RIGHT] = NULL;
			/* in multiplayer RS_ItemIsResearched always returns true,
			so we are able to use the aliens weapons */
			if (selWeapon && !RS_ItemIsResearched(csi.ods[selWeapon->item.t].id)) {
				CL_DisplayHudMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"), 2000);
				cl.cmode = M_MOVE;
			} else if (selWeapon && selFD) {
				Com_sprintf(infoText, sizeof(infoText),
							"%s\n%s (%i) [%i%%] %i\n", csi.ods[selWeapon->item.t].name, selFD->name, selFD->ammo, selToHit, selFD->time);
				Com_sprintf(mouseText, sizeof(mouseText),
							"%s: %s (%i) [%i%%] %i\n", csi.ods[selWeapon->item.t].name, selFD->name, selFD->ammo, selToHit, selFD->time);

				menuText[TEXT_MOUSECURSOR_RIGHT] = mouseText;	/* Save the text for later display next to the cursor. */

				time = selFD->time;
				/* if no TUs left for this firing action of if the weapon is reloadable and out of ammo, then change to move mode */
				if (selActor->TU < time || (csi.ods[selWeapon->item.t].reload && selWeapon->item.a <= 0)) {
					cl.cmode = M_MOVE;
					CL_RefreshWeaponButtons(selActor->TU - actorMoveLength);
				}
			} else if (selWeapon) {
				Com_sprintf(infoText, sizeof(infoText), _("%s\n(empty)\n"), csi.ods[selWeapon->item.t].name);
			} else {
				cl.cmode = M_MOVE;
				CL_RefreshWeaponButtons(selActor->TU - actorMoveLength);
			}
		}

		/* handle actor in a panic */
		if (selActor->state & STATE_PANIC) {
			Com_sprintf(infoText, sizeof(infoText), _("Currently panics!\n"));
			menuText[TEXT_MOUSECURSOR_RIGHT] = NULL;
		}

		/* Calculate remaining TUs. */
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

		if (!LEFT(selActor) && RIGHT(selActor)
			&& csi.ods[RIGHT(selActor)->item.t].holdtwohanded)
			Cvar_Set("mn_ammoleft", Cvar_VariableString("mn_ammoright"));

		/* change stand-crouch & reaction button state */
		if (cl.oldstate != selActor->state || refresh) {
			selActorReactionState = CL_GetReactionState(selActor);
			if (selActorOldReactionState != selActorReactionState) {
				selActorOldReactionState = selActorReactionState;
				switch (selActorReactionState) {
				case R_FIRE_MANY:
					Cbuf_AddText("startreactionmany\n");
					break;
				case R_FIRE_ONCE:
					Cbuf_AddText("startreactiononce\n");
					break;
				case R_FIRE_OFF: /* let RefreshWeaponButtons work it out */
					weaponButtonState[BT_RIGHT_PRIMARY_REACTION] = -1;
					break;
				}
			}

			cl.oldstate = selActor->state;
			if (actorMoveLength < 0xFF
				&& (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE))
				CL_RefreshWeaponButtons(time);
			else
				CL_RefreshWeaponButtons(selActor->TU);
		} else {
			/* no actor selected, reset cvars */
			/* TODO: this overwrites the correct values a bit to often.
			Cvar_Set("mn_tu", "0");
			Cvar_Set("mn_turemain", "0");
			Cvar_Set("mn_tumax", "30");
			Cvar_Set("mn_morale", "0");
			Cvar_Set("mn_moralemax", "1");
			Cvar_Set("mn_hp", "0");
			Cvar_Set("mn_hpmax", "1");
			Cvar_Set("mn_ammoright", "");
			Cvar_Set("mn_ammoleft", "");
			Cvar_Set("mn_stun", "0");
			Cvar_Set("mn_ap", "0");
			*/
			if (refresh)
				Cbuf_AddText("deselstand\n");

			/* this allows us to display messages even with no actor selected */
			if (cl.time < cl.msgTime) {
				/* special message */
				Com_sprintf(infoText, sizeof(infoText), cl.msgText);
			}
		}
		menuText[TEXT_STANDARD] = infoText;
	/* this will stop the drawing of the bars over the hole screen when we test maps */
	} else if (!cl.numTeamList) {
		Cvar_SetValue("mn_tu", 0);
		Cvar_SetValue("mn_tumax", 100);
		Cvar_SetValue("mn_morale", 0);
		Cvar_SetValue("mn_moralemax", 100);
		Cvar_SetValue("mn_hp", 0);
		Cvar_SetValue("mn_hpmax", 100);
		Cvar_SetValue("mn_stun", 0);
		Cvar_SetValue("mn_ap", 100);
	}

	/* mode */
	if (cl.oldcmode != cl.cmode || refresh) {
		switch (cl.cmode) {
		/* TODO: Better highlight for active firemode (from the list, not the buttons) needed ... */
		case M_FIRE_L:
		case M_PEND_FIRE_L:
			/* TODO: Display current firemode better*/
			break;
		case M_FIRE_R:
		case M_PEND_FIRE_R:
			/* TODO: Display current firemode better*/
			break;
		default:
			ClearHighlights();
		}
		cl.oldcmode = cl.cmode;
		if (selActor)
			CL_RefreshWeaponButtons(selActor->TU);
	}

	/* player bar */
	if (cl_selected->modified || refresh) {
		int i;

		for (i = 0; i < MAX_TEAMLIST; i++) {
			if (!cl.teamList[i] || cl.teamList[i]->state & STATE_DEAD) {
				Cbuf_AddText(va("huddisable%i\n", i));
			} else if (i == cl_selected->integer) {
				Cbuf_AddText(va("hudselect%i\n", i));
			} else {
				Cbuf_AddText(va("huddeselect%i\n", i));
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
 *
 * @sa CL_RemoveActorFromTeamList
 * @param le Pointer to local entity struct
 */
void CL_AddActorToTeamList (le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team || le->pnum != cl.pnum || (le->state & STATE_DEAD))
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
		Cbuf_AddText(va("numonteam%i\n", cl.numTeamList));
		Cbuf_AddText(va("huddeselect%i\n", i));
		if (cl.numTeamList == 1)
			CL_ActorSelectList(0);

		/* Initialize reactionmode (with undefined firemode) ... this will be checked for in CL_DoEndRound. */
		CL_SetReactionFiremode( i, -1, -1, -1);
	}
}


/**
 * @brief Removes an actor from the team list.
 *
 * @sa CL_AddActorToTeamList
 * @param le Pointer to local entity struct
 */
void CL_RemoveActorFromTeamList (le_t * le)
{
	int i;

	if (!le)
		return;

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			/* disable hud button */
			Cbuf_AddText(va("huddisable%i\n", i));

			/* remove from list */
			cl.teamList[i] = NULL;
			break;
		}
	}

	/* check selection */
	if (selActor == le) {
		/* TODO: This should probably be a while loop */
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
 *
 * @param le Pointer to local entity struct
 *
 * @sa CL_UGVCvars
 * @sa CL_CharacterCvars
 */
extern qboolean CL_ActorSelect (le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team ||
		(le->state & STATE_DEAD) || !le->inuse)
		return qfalse;

	if (blockEvents)
		return qfalse;

	/* select him */
	if (selActor)
		selActor->selected = qfalse;
	le->selected = qtrue;
	selActor = le;
	menuInventory = &selActor->i;
	selActorReactionState = CL_GetReactionState(selActor);

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			HideFiremodes();
			/* console commands, update cvars */
			Cvar_ForceSet("cl_selected", va("%i", i));
			if ( le->fieldSize == ACTOR_SIZE_NORMAL ) {
				selChr = baseCurrent->curTeam[i];
				CL_CharacterCvars(selChr);
			} else {
				selChr = baseCurrent->curTeam[i];
				CL_UGVCvars(selChr);
			}

			CL_ConditionalMoveCalc(&clMap, le, MAX_ROUTE);

			/* move first person camera to new actor */
			if (camera_mode == CAMERA_MODE_FIRSTPERSON)
				CL_CameraModeChange(CAMERA_MODE_FIRSTPERSON);

			cl.cmode = M_MOVE;
			return qtrue;
		}
	}

	return qfalse;
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
extern qboolean CL_ActorSelectList (int num)
{
	le_t *le;

	/* check if actor exists */
	if (num >= cl.numTeamList)
		return qfalse;

	/* select actor */
	le = cl.teamList[num];
	if (!CL_ActorSelect(le))
		return qfalse;

	/* center view (if wanted) */
	if (cl_centerview->integer)
		VectorCopy(le->origin, cl.cam.reforg);
	/* change to worldlevel were actor is right now */
	Cvar_SetValue("cl_worldlevel", le->pos[2]);

	return qtrue;
}

/**
 * @brief selects the next actor
 */
extern qboolean CL_ActorSelectNext (void)
{
	le_t* le;
	int selIndex = -1;
	int num = cl.numTeamList;
	int i;

	/* find index of currently selected actor */
	for (i = 0; i < num; i++) {
		le = cl.teamList[i];
		if (le && le->selected && le->inuse && !(le->state & STATE_DEAD)) {
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
 *
 * @see CL_BuildForbiddenList
 */
byte *fb_list[MAX_EDICTS];
/**
 * @brief Current length of fb_list.
 *
 * @see fb_list
 */
int fb_length;

/**
 * @brief Builds a list of locations that cannot be moved to.
 * @sa g_client:G_BuildForbiddenList <- shares quite some code
 *
 * @todo This probably belongs in the core logic.
 * This is used for pathfinding.
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
		if (!(le->state & STATE_DEAD) && (le->type == ET_ACTOR || le->type == ET_UGV))
			fb_list[fb_length++] = le->pos;
	}

#ifdef PARANOID
	if (fb_length > MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_BuildForbiddenList: list too long");
#endif
}

/**
 * @brief Recalculate forbidden list, available moves and actor's move length
 * if given le is the selected Actor.
 * @param[in] map Routing data
 * @param[in] le Calculate the move for this actor (if he's the selected actor)
 * @param[in] distance
 */
extern void CL_ConditionalMoveCalc (struct routing_s *map, le_t *le, int distance)
{
	if (selActor && selActor == le) {
		CL_BuildForbiddenList();
		Grid_MoveCalc(map, le->pos, distance, fb_list, fb_length);
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
		Com_sprintf(infoText, sizeof(infoText), _("Nobody selected\n"));
		return qfalse;
	}

/*	if (blockEvents) {
		Com_Printf("Can't do that right now.\n");
		return qfalse;
	}
*/
	if (cls.team != cl.actTeam) {
		Com_Printf("This isn't your round.\n");
		CL_DisplayHudMessage(_("This isn't your round\n"), 2000);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Draws the way to walk when confirm actions is activated.
 * @param[in] to
 */
static int CL_TraceMove (pos3_t to)
{
	int length;
	vec3_t vec, oldVec;
	pos3_t pos;
	int dv;

	length = Grid_MoveLength(&clMap, to, qfalse);

	if (!selActor || !length || length >= 0x3F)
		return 0;

	Grid_PosToVec(&clMap, to, oldVec);
	VectorCopy(to, pos);

	while ((dv = Grid_MoveNext(&clMap, pos)) < 0xFF) {
		length = Grid_MoveLength(&clMap, pos, qfalse);
		PosAddDV(pos, dv);
		Grid_PosToVec(&clMap, pos, vec);
		if (length > selActor->TU)
			CL_ParticleSpawn("longRangeTracer", 0, vec, oldVec, NULL);
		else if (selActor->state & STATE_CROUCHED)
			CL_ParticleSpawn("crawlTracer", 0, vec, oldVec, NULL);
		else
			CL_ParticleSpawn("moveTracer", 0, vec, oldVec, NULL);
		VectorCopy(vec, oldVec);
	}
	return 1;
}

/**
 * @brief Starts moving actor.
 * @param[in] le
 * @param[in] to
 * @sa CL_ActorActionMouse
 * @sa CL_ActorSelectMouse
 */
extern void CL_ActorStartMove (le_t * le, pos3_t to)
{
	int length;

	if (!CL_CheckAction())
		return;

	length = Grid_MoveLength(&clMap, to, qfalse);

	if (!length || length >= 0xFF) {
		/* move not valid, don't even care to send */
		return;
	}

	/* change mode to move now */
	cl.cmode = M_MOVE;

	/* move seems to be possible; send request to server */
	MSG_Write_PA(PA_MOVE, le->entnum, to);
}


/**
 * @brief Shoot with actor.
 * @param[in] le Who is shooting
 * @param[in] at Position you are targetting to
 */
void CL_ActorShoot (le_t * le, pos3_t at)
{
	int type;
	if (!CL_CheckAction())
		return;

	Com_DPrintf("CL_ActorShoot: cl.firemode %i.\n",  cl.cfiremode);

	/* TODO: Is there a better way to do this?
	 * This type value will travel until it is checked in at least g_combat.c:G_GetShotFromType.
	 */
	type = IS_MODE_FIRE_RIGHT(cl.cmode)
			? ST_RIGHT
			: ST_LEFT;
	MSG_Write_PA(PA_SHOOT, le->entnum, at, type, cl.cfiremode);
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
	int weapon, x, y, tu;
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
			   && csi.ods[inv->c[csi.idRight]->item.t].holdtwohanded) {
		/* Check for two-handed weapon */
		hand = csi.idRight;
		weapon = inv->c[hand]->item.t;
	} else
		/* otherwise we could use weapon uninitialized */
		return;

	/* return if the weapon is not reloadable */
	if (!csi.ods[weapon].reload)
		return;

	if (!RS_ItemIsResearched(csi.ods[weapon].id)) {
		CL_DisplayHudMessage(_("You cannot reload this unknown item.\nYou need to research it and its ammunition first.\n"), 2000);
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point */
			/* searching other containers if it would take longer */
			/* to retrieve the ammo from them than the one */
			/* we've already found. */
			for (ic = inv->c[container]; ic; ic = ic->next)
				if (INV_LoadableInWeapon(&csi.ods[ic->item.t], weapon)
				&& RS_ItemIsResearched(csi.ods[ic->item.t].id) ) {
					x = ic->x;
					y = ic->y;
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
 * @brief The client changed something in his hand-containers. This fucntion updates the reactionfire info.
 * @param[in] sb
 */
void CL_InvCheckHands (sizebuf_t * sb)
{
	int entnum;
	le_t *le;
	int actor_idx = -1;
	int hand = -1;	/**< 0=right, 1=left */

	MSG_ReadFormat(sb, ev_format[EV_INV_HANDS_CHANGED], &entnum, &hand);

	if ((entnum < 0) || (hand < 0)) {
		Com_Printf("CL_InvCheckHands: entnum or hand not sent/received correctly.\n");
	}

	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("CL_InvCheckHands: LE doesn't exist.\n");
		return;
	}

	actor_idx = CL_GetActorNumber(le);

	/* Update the cahnged hand with default firemode. */
	if (hand == 0)
		CL_UpdateReactionFiremodes('r', actor_idx, -1);
	else
		CL_UpdateReactionFiremodes('l', actor_idx, -1);
	HideFiremodes();
}

/**
 * @brief Moves actor.
 * @param[in] sb
 */
void CL_ActorDoMove (sizebuf_t * sb)
{
	le_t *le;

	/* get le */
	le = LE_Get(MSG_ReadShort(sb));
	if (!le || (le->type != ET_ACTOR && le->type != ET_UGV)) {
		Com_Printf("Can't move, LE doesn't exist or is not an actor\n");
		return;
	}

	if (le->state & STATE_DEAD) {
		Com_Printf( "Can't move, actor dead\n" );
		return;
	}

	/* get length */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_MOVE], &le->pathLength, le->path);

	/* activate PathMove function */
	FLOOR(le) = NULL;
	le->think = LET_StartPathMove;
	le->pathPos = 0;
	le->startTime = cl.time;
	le->endTime = cl.time;
	/* FIXME: speed should somehow depend on strength of character */
	if (le->state & STATE_CROUCHED)
		le->speed = 50;
	else
		le->speed = 100;
	blockEvents = qtrue;
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

	/* calculate dv */
	VectorSubtract(mousePos, selActor->pos, div);
	dv = AngleToDV((int) (atan2(div[1], div[0]) * todeg));

	/* send message to server */
	MSG_Write_PA(PA_TURN, selActor->entnum, dv);
}


/**
 * @brief Turns actor.
 * @param[in] sb
 */
void CL_ActorDoTurn (sizebuf_t *sb)
{
	le_t *le;
	int entnum, dir;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_TURN], &entnum, &dir);

	/* get le */
	le = LE_Get(entnum);
	if (!le || (le->type != ET_ACTOR && le->type != ET_UGV)) {
		Com_Printf("Can't turn, LE doesn't exist or is not an actor\n");
		return;
	}

	if (le->state & STATE_DEAD) {
		Com_Printf( "Can't turn, actor dead\n" );
		return;
	}

	le->dir = dir;
	le->angles[YAW] = dangle[le->dir];
}


/**
 * @brief Stands or crouches actor.
 */
extern void CL_ActorStandCrouch_f (void)
{
	if (!CL_CheckAction())
		return;

	if (selActor->fieldSize == ACTOR_SIZE_UGV )
		return;
	/* send a request to toggle crouch to the server */
	MSG_Write_PA(PA_STATE, selActor->entnum, STATE_CROUCHED);
}

#if 0
/**
 * @brief Stuns an actor.
 *
 * Stunning is handled as a dead actor but afterwards in AL_CollectingAliens we only collect aliens with STATE_STUN
 * @note: we can do this because STATE_STUN is 0x43 and STATE_DEAD is 0x03 (checking for STATE_DEAD is also true when STATE_STUN was set)
 * @note: Do we really need this as a script command? Currently there is no binding - but who knows?
 *
 * @note Unused atm
 */
void CL_ActorStun (void)
{
	if (!CL_CheckAction())
		return;

	/* send a request to the server to stun this actor */
	/* the server currently will refuse to stun an actor sent via PA_STATE */
	MSG_Write_PA(PA_STATE, selActor->entnum, STATE_STUN);
}
#endif

/**
 * @brief Toggles reaction fire between the states off/single/multi.
 */
extern void CL_ActorToggleReaction_f (void)
{
	int state = 0;
	int actor_idx = -1;

	if (!CL_CheckAction())
		return;

	actor_idx = CL_GetActorNumber(selActor);

	/* Check all hands for reaction-enabled ammo-firemodes. */
	if (CL_WeaponWithReaction('r') || CL_WeaponWithReaction('l')) {
		selActorReactionState++;
		if (selActorReactionState > R_FIRE_MANY)
			selActorReactionState = R_FIRE_OFF;

		switch (selActorReactionState) {
		case R_FIRE_OFF:
			state = ~STATE_REACTION;
			break;
		case R_FIRE_ONCE:
			state = STATE_REACTION_ONCE;
			/* TODO: Check if stored info for RF is up-to-date and set it to default if not. */
			/* Set default rf-mode. */
			if (!SANE_REACTION(actor_idx)) {
				CL_UpdateReactionFiremodes('r', actor_idx, -1);
				if (!SANE_REACTION(actor_idx))
					CL_UpdateReactionFiremodes('l', actor_idx, -1);
			}
			break;
		case R_FIRE_MANY:
			state = STATE_REACTION_MANY;
			/* TODO: Check if stored info for RF is up-to-date and set it to default if not. */
			/* Set default rf-mode. */
			if (!SANE_REACTION(actor_idx)) {
				CL_UpdateReactionFiremodes('r', actor_idx, -1);
				if (!SANE_REACTION(actor_idx))
					CL_UpdateReactionFiremodes('l', actor_idx, -1);
			}
			break;
		default:
			return;
		}

		/* send request to update actor's reaction state to the server */
		MSG_Write_PA(PA_STATE, selActor->entnum, state);
	} else {
		/* Send the "turn rf off" message just in case */
		state = ~STATE_REACTION;
		MSG_Write_PA(PA_STATE, selActor->entnum, state);
		/* Set RF-mode info to undef */
		CL_SetReactionFiremode(actor_idx, -1, -1, -1);
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
 */
void CL_ActorDoShoot (sizebuf_t * sb)
{
	fireDef_t *fd;
	le_t *le;
	vec3_t muzzle, impact;
	int flags, normal, number;
	int obj_idx;
	int weap_fds_idx, fd_idx;

	/* read data */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_SHOOT], &number, &obj_idx, &weap_fds_idx, &fd_idx, &flags, &muzzle, &impact, &normal);

	/* get le */
	le = LE_Get(number);

	/* get the fire def */
#ifdef DEBUG
	GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
	fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);

	/* TODO/FIXME: e.g. smoke grenades or particles that stay longer than a few seconds
	 * should become invisible if out of actor view */
	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0]
		&& !(flags & SF_BOUNCED))
		S_StartLocalSound(fd->fireSound);
	firstShot = qfalse;

	/* do actor related stuff */
	if (!le) {
		/* it's OK, the actor is not visible */
		return;
	}

	if (le->type != ET_ACTOR && le->type != ET_UGV) {
		Com_Printf("Can't shoot, LE is not an actor\n");
		return;
	}

	if (le->state & STATE_DEAD) {
		Com_Printf( "Can't shoot, actor dead\n" );
		return;
	}

	/* if actor on our team, set this le as the last moving */
	if (le->team == cls.team)
		CL_SetLastMoving(le);

	/* Animate - we have to check if it is right or left weapon usage. */
	/* TODO: FIXME the left/right info for actors in the enemy team/turn has to come from somewhere. */
	if (RIGHT(le) && IS_MODE_FIRE_RIGHT(cl.cmode)) {
		re.AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
		re.AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	} else if (LEFT(le) && IS_MODE_FIRE_LEFT(cl.cmode)) {
		re.AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->left, le->right, le->state));
		re.AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->left, le->right, le->state));
	} else {
		Com_DPrintf("CL_ActorDoShoot: No information about weapon hand found or left/right info out of sync somehow.\n");
		/* We use the default (right) animation now. */
		re.AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
		re.AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	}
}


/**
 * @brief Shoot with weapon but don't bother with animations - actor is hidden.
 * @sa CL_ActorShoot
 */
void CL_ActorShootHidden (sizebuf_t *sb)
{
	fireDef_t	*fd;
	int first;
	int obj_idx;
	int weap_fds_idx, fd_idx;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &obj_idx, &weap_fds_idx, &fd_idx);

	/* get the fire def */
#ifdef DEBUG
	GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
	fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);

	/* start the sound; TODO: is check for SF_BOUNCED needed? */
	if (((first && fd->soundOnce) || (!first && !fd->soundOnce)) && fd->fireSound[0] )
		S_StartLocalSound(fd->fireSound);

#if 0
	/* TODO/FIXME: e.g. smoke grenades or particles that stay longer than a few seconds should also spawn
	 * an invisible particles (until it becomes visible) */
	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal);
#endif

	/* if the shooting becomes visibile, don't repeat sounds! */
	firstShot = qfalse;
}


/**
 * @brief Throw item with actor.
 * @param[in] sb
 */
void CL_ActorDoThrow (sizebuf_t * sb)
{
	fireDef_t *fd;
	vec3_t muzzle, v0;
	int flags;
	int dtime;
	int obj_idx;
	int weap_fds_idx, fd_idx;

	/* read data */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_THROW], &dtime, &obj_idx, &weap_fds_idx, &fd_idx, &flags, &muzzle, &v0);

	/* get the fire def */
#ifdef DEBUG
	GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
	fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);

	/* add effect le (local entity) */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0]
		&& !(flags & SF_BOUNCED))
		S_StartLocalSound(fd->fireSound);
	firstShot = qfalse;
}


/**
 * @brief Starts shooting with actor.
 * @param[in] sb
 * @sa CL_ActorShootHidden
 * @sa CL_ActorShoot
 * @sa CL_ActorDoShoot
 * @todo Improve detection of left- or right animation.
 */
void CL_ActorStartShoot (sizebuf_t * sb)
{
	fireDef_t *fd;
	le_t *le;
	pos3_t from, target;
	int number;
	int obj_idx;
	int weap_fds_idx,fd_idx;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_START_SHOOT], &number, &obj_idx, &weap_fds_idx, &fd_idx, &from, &target);

#ifdef DEBUG
	GET_FIREDEFDEBUG(obj_idx, weap_fds_idx, fd_idx)
#endif
	fd = GET_FIREDEF(obj_idx, weap_fds_idx, fd_idx);

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

	if (le->type != ET_ACTOR && le->type != ET_UGV) {
		Com_Printf("Can't start shoot, LE not an actor\n");
		return;
	}

	if (le->state & STATE_DEAD) {
		Com_Printf( "Can't start shoot, actor dead\n" );
		return;
	}

	/* erase one-time weapons from storage --- which ones?
	if (curCampaign && le->team == cls.team && !csi.ods[type].ammo) {
		if (ccs.eMission.num[type])
			ccs.eMission.num[type]--;
	} */

	/* Animate - we have to check if it is right or left weapon usage. */
	/* TODO: FIXME the left/right info for actors in the enemy team/turn has to come from somewhere. */
	if (RIGHT(le) && IS_MODE_FIRE_RIGHT(cl.cmode)) {
		re.AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	} else if (LEFT(le) && IS_MODE_FIRE_LEFT(cl.cmode)) {
		re.AnimChange(&le->as, le->model1, LE_GetAnim("move", le->left, le->right, le->state));
	} else {
		Com_DPrintf("CL_ActorStartShoot: No information about weapon hand found or left/right info out of sync somehow.\n");
		/* We use the default (right) animation now. */
		re.AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	}
}

/**
 * @brief Kills actor.
 * @param[in] sb
 */
void CL_ActorDie (sizebuf_t * sb)
{
	le_t *le;
	int number, state;
	int i;
	int teamDescID = -1;
	char tmpbuf[128];

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_DIE], &number, &state);

	/* get le */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->entnum == number)
			break;

	if (le->entnum != number) {
		Com_DPrintf("CL_ActorDie: Can't kill, LE doesn't exist\n");
		return;
	} else if (le->type != ET_UGV && le->type != ET_ACTOR) {
		Com_Printf("CL_ActorDie: Can't kill, LE is not an actor\n");
		return;
	} else if (le->state & STATE_DEAD) {
		Com_Printf("CL_ActorDie: Can't kill, actor already dead\n");
		return;
	}/* else if (!le->inuse) {
		* LE not in use condition normally arises when CL_EntPerish has been
		* called on the le to hide it from the client's sight.
		* Probably can return without killing unused LEs, but testing reveals
		* killing an unused LE here doesn't cause any problems and there is
		* an outside chance it fixes some subtle bugs.
	}*/

	/* count spotted aliens */
	if (le->team != cls.team && le->team != TEAM_CIVILIAN && le->inuse)
		cl.numAliensSpotted--;

	/* set relevant vars */
	FLOOR(le) = NULL;
	le->STUN = 0;
	le->state = state;

	/* play animation */
	le->think = NULL;
	re.AnimChange(&le->as, le->model1, va("death%i", le->state & STATE_DEAD));
	re.AnimAppend(&le->as, le->model1, va("dead%i", le->state & STATE_DEAD));

	/* Print some info about the death or stun. */
	if (le->team == cls.team && baseCurrent) {
		i = CL_GetActorNumber(le);
		if (i >= 0 && ((le->state & STATE_STUN) & ~STATE_DEAD)) {
			Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s %s\n",
			gd.ranks[baseCurrent->curTeam[i]->rank].shortname,
			baseCurrent->curTeam[i]->name, _("was stunned.\n"));
			CL_DisplayHudMessage(tmpbuf, 2000);
		} else if (i >= 0) {
			Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s %s\n",
			gd.ranks[baseCurrent->curTeam[i]->rank].shortname,
			baseCurrent->curTeam[i]->name, _("was killed.\n"));
			CL_DisplayHudMessage(tmpbuf, 2000);
		}
	} else {
		switch (le->team) {
		case (TEAM_CIVILIAN):
			if ((le->state & STATE_STUN) & ~STATE_DEAD)
				CL_DisplayHudMessage(_("A civilian was stunned.\n"), 2000);
			else
				CL_DisplayHudMessage(_("A civilian was killed.\n"), 2000);
			break;
		case (TEAM_ALIEN):
			if (le->teamDesc) {
				teamDescID = le->teamDesc - 1;
				if (RS_IsResearched_idx(RS_GetTechIdxByName(teamDesc[teamDescID].tech))) {
					if ((le->state & STATE_STUN) & ~STATE_DEAD) {
						Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s.\n",
						_("An alien was stunned:"), _(teamDesc[teamDescID].name));
						CL_DisplayHudMessage(tmpbuf, 2000);
					} else {
						Com_sprintf(tmpbuf, sizeof(tmpbuf), "%s %s.\n",
						_("An alien was killed:"), _(teamDesc[teamDescID].name));
						CL_DisplayHudMessage(tmpbuf, 2000);
					}
				} else {
					if ((le->state & STATE_STUN) & ~STATE_DEAD)
						CL_DisplayHudMessage(_("An alien was stunned.\n"), 2000);
					else
						CL_DisplayHudMessage(_("An alien was killed.\n"), 2000);
				}
			} else {
				if ((le->state & STATE_STUN) & ~STATE_DEAD)
					CL_DisplayHudMessage(_("An alien was stunned.\n"), 2000);
				else
					CL_DisplayHudMessage(_("An alien was killed.\n"), 2000);
			}
			break;
		case (TEAM_PHALANX):
			if ((le->state & STATE_STUN) & ~STATE_DEAD)
				CL_DisplayHudMessage(_("A soldier was stunned.\n"), 2000);
			else
				CL_DisplayHudMessage(_("A soldier was killed.\n"), 2000);
			break;
		default:
			if ((le->state & STATE_STUN) & ~STATE_DEAD)
				CL_DisplayHudMessage(va(_("A member of team %i was stunned.\n"), le->team), 2000);
			else
				CL_DisplayHudMessage(va(_("A member of team %i was killed.\n"), le->team), 2000);
			break;
		}
	}

	VectorCopy(player_dead_maxs, le->maxs);
	CL_RemoveActorFromTeamList(le);

	CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);
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
void CL_ActorMoveMouse (void)
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
		if (confirm_movement->value || confirm_actions->value) {
			/* Set our mode to pending move. */
			VectorCopy(mousePos, mousePendPos);
			cl.cmode = M_PEND_MOVE;
		} else {
			/* Just move there. */
			CL_ActorStartMove(selActor, mousePos);
		}
	}
}

/**
 * @brief Selects an actor using the mouse.
 * @todo Comment on the cl.cmode stuff.
 */
void CL_ActorSelectMouse (void)
{
	if (mouseSpace != MS_WORLD)
		return;

	switch (cl.cmode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Try and select another team member */
		if (CL_ActorSelect(mouseActor)) {
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
		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->value) {
			cl.cmode = M_PEND_FIRE_R;
			VectorCopy(mousePos, mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	case M_FIRE_L:
		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->value) {
			cl.cmode = M_PEND_FIRE_L ;
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
extern void CL_NextRound_f (void)
{
	/* can't end round if we are not in battlescape */
	if (!CL_OnBattlescape())
		return;

	/* can't end round if we're not active */
	if (cls.team != cl.actTeam)
		return;

	/* send endround */
	MSG_WriteByte(&cls.netchan.message, clc_endround);

	/* change back to remote view */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
}

/**
 * @brief Displays a message on the hud.
 *
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void CL_DisplayHudMessage (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}

/**
 * @brief Performs end-of-turn processing.
 * @param[in] sb
 */
void CL_DoEndRound (sizebuf_t * sb)
{
	int actor_idx;

	/* hud changes */
	if (cls.team == cl.actTeam)
		Cbuf_AddText("endround\n");

	/* change active player */
	Com_Printf("Team %i ended round", cl.actTeam);
	cl.actTeam = MSG_ReadByte(sb);
	Com_Printf(", team %i's round started!\n", cl.actTeam);

	/* hud changes */
	if (cls.team == cl.actTeam) {
		/* check whether a particle has to go */
		CL_ParticleCheckRounds();
		Cbuf_AddText("startround\n");
		CL_DisplayHudMessage(_("Your round started!\n"), 2000);
		S_StartLocalSound("misc/roundstart.wav");
		CL_ConditionalMoveCalc(&clMap, selActor, MAX_ROUTE);

		for (actor_idx = 0; actor_idx < cl.numTeamList; actor_idx++) {
			if (cl.teamList[actor_idx]) {
				/* Check if any default firemode is defined and search for one if not. */
				if (!SANE_REACTION(actor_idx)) {
					CL_UpdateReactionFiremodes('r', actor_idx, -1);
					if (!SANE_REACTION(actor_idx))
						CL_UpdateReactionFiremodes('l', actor_idx, -1);
				}
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
 * @brief Recalculates the currently selected Actor's move length.
 * */
void CL_ResetActorMoveLength (void)
{
	actorMoveLength = Grid_MoveLength(&clMap, mousePos, qfalse);
	if (selActor->state & STATE_CROUCHED)
		actorMoveLength *= 1.5;
}

/**
 * @brief Battlescape cursor positioning.
 * @note Sets global var mouseActor to current selected le
 * @sa CL_ParseInput
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
	le_t *le;

	/* get cursor position as a -1 to +1 range for projection */
	cur[0] = (mx * viddef.rx - scr_vrect.width * 0.5 - scr_vrect.x) / (scr_vrect.width * 0.5);
	cur[1] = (my * viddef.ry - scr_vrect.height * 0.5 - scr_vrect.y) / (scr_vrect.height * 0.5);

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
		intersectionLevel = Grid_Fall(&clMap, testPos);
 		/* if looking up, raise the intersection level */
		if (cur[1] < 0.)
			intersectionLevel++;
	} else {
		VectorCopy(cl.cam.camorg, from);
		VectorCopy(cl.cam.axis[0], forward);
		VectorCopy(cl.cam.axis[1], right);
		VectorCopy(cl.cam.axis[2], up);
		intersectionLevel = cl_worldlevel->value;
	}

	if (cl_isometric->value)
		frustumslope[0] = 10.0 * cl.refdef.fov_x;
	else
		frustumslope[0] = tan(cl.refdef.fov_x * M_PI / 360) * projectiondistance;
	frustumslope[1] = frustumslope[0] * ((float)scr_vrect.height / scr_vrect.width);

	/* transform cursor position into perspective space */
	VectorMA(from, projectiondistance, forward, stop);
	VectorMA(stop, cur[0] * frustumslope[0], right, stop);
	VectorMA(stop, cur[1] * -frustumslope[1], up, stop);

	/* in isometric mode the camera position has to be calculated from the cursor position so that the trace goes in the right direction */
	if (cl_isometric->value)
		VectorMA(stop, -projectiondistance*2, forward, from);

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
		VectorSubtract(P3, from,  P3minusP1);
		u = DotProduct(mapNormal, P3minusP1)/nDotP2minusP1;
		VectorScale(P2minusP1, (vec_t)u, dir);
		VectorAdd(from, dir, end);
	} else { /* otherwise do a full trace */
		CM_TestLineDM(from, stop, end);
	}

	VecToPos(end, testPos);
	restingLevel = Grid_Fall(&clMap, testPos);

	/* hack to prevent cursor from getting stuck on the top of an invisible
	   playerclip surface (in most cases anyway) */
	PosToVec(testPos, pA);
	VectorCopy(pA, pB);
	pA[2] += UNIT_HEIGHT;
	pB[2] -= UNIT_HEIGHT;
	CM_TestLineDM(pA, pB, pC);
	VecToPos(pC, testPos);
	restingLevel = min(restingLevel, Grid_Fall(&clMap, testPos));

	/* if grid below intersection level, start a trace from the intersection */
	if (restingLevel < intersectionLevel) {
		VectorCopy(end, from);
		from[2] -= CURSOR_OFFSET;
		CM_TestLineDM(from, stop, end);
		VecToPos(end, testPos);
		restingLevel = Grid_Fall(&clMap, testPos);
	}

	/* test if the selected grid is out of the world */
	if (restingLevel < 0 || restingLevel >= HEIGHT)
		return;

	Vector2Copy(testPos, mousePos);
	mousePos[2] = restingLevel;

	/* search for an actor on this field */
	mouseActor = NULL;
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && !(le->state & STATE_DEAD) && (le->type == ET_ACTOR || le->type == ET_UGV) && VectorCompare(le->pos, mousePos)) {
			mouseActor = le;
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
 * @brief Adds an actor.
 * @param[in] le
 * @param[in] ent
 * @sa CL_AddUGV
 */
qboolean CL_AddActor (le_t * le, entity_t * ent)
{
	entity_t add;

	if (!(le->state & STATE_DEAD)) {
		/* add weapon */
		if (le->left != NONE) {
			memset(&add, 0, sizeof(entity_t));

			add.lightparam = &le->sunfrac;
			add.model = cl.model_weapons[le->left];

			add.tagent = V_GetEntity() + 2 + (le->right != NONE);
			add.tagname = "tag_lweapon";

			V_AddEntity(&add);
		}

		/* add weapon */
		if (le->right != NONE) {
			memset(&add, 0, sizeof(entity_t));

			add.lightparam = &le->sunfrac;
			add.alpha = le->alpha;
			add.model = cl.model_weapons[le->right];

			add.tagent = V_GetEntity() + 2;
			add.tagname = "tag_rweapon";

			V_AddEntity(&add);
		}
	}

	/* add head */
	memset(&add, 0, sizeof(entity_t));

	add.lightparam = &le->sunfrac;
	add.alpha = le->alpha;
	add.model = le->model2;
	add.skinnum = le->skinnum;

	add.tagent = V_GetEntity() + 1;
	add.tagname = "tag_head";
	le->fieldSize = ACTOR_SIZE_NORMAL;

	V_AddEntity(&add);

	/* add actor special effects */
	ent->flags |= RF_SHADOW;

	if (!(le->state & STATE_DEAD)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (cl_markactors->value && le->team == cls.team) {
			if (le->pnum == cl.pnum)
				ent->flags |= RF_MEMBER;
			if (le->pnum != cl.pnum)
				ent->flags |= RF_ALLIED;
		}
	}

	return qtrue;
}

/**
 * @brief Adds an UGV.
 * @param[in] le
 * @param[in] ent
 * @sa CL_AddActor
 */
qboolean CL_AddUGV (le_t * le, entity_t * ent)
{
	entity_t add;

	if (!(le->state & STATE_DEAD)) {
		/* add weapon */
		if (le->left != NONE) {
			memset(&add, 0, sizeof(entity_t));

			add.lightparam = &le->sunfrac;
			add.model = cl.model_weapons[le->left];

			add.tagent = V_GetEntity() + 2 + (le->right != NONE);
			add.tagname = "tag_lweapon";

			V_AddEntity(&add);
		}

		/* add weapon */
		if (le->right != NONE) {
			memset(&add, 0, sizeof(entity_t));

			add.lightparam = &le->sunfrac;
			add.alpha = le->alpha;
			add.model = cl.model_weapons[le->right];

			add.tagent = V_GetEntity() + 2;
			add.tagname = "tag_rweapon";

			V_AddEntity(&add);
		}
	}

	/* add head */
	memset(&add, 0, sizeof(entity_t));

	add.lightparam = &le->sunfrac;
	add.alpha = le->alpha;
	add.model = le->model2;
	add.skinnum = le->skinnum;

	/* FIXME */
	add.tagent = V_GetEntity() + 1;
	add.tagname = "tag_head";

	V_AddEntity(&add);

	/* add actor special effects */
	ent->flags |= RF_SHADOW;
	le->fieldSize = ACTOR_SIZE_UGV;

	if (!(le->state & STATE_DEAD)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (cl_markactors->value && le->team == cls.team) {
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

/**
 * @brief Calculates chance to hit.
 * @param[in] toPos
 */
float CL_TargetingToHit (pos3_t toPos)
{
	vec3_t shooter, target;
	float distance, pseudosin, width, height, acc, perpX, perpY, hitchance;
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
	if (distx * distx > disty * disty)
		pseudosin = distance / distx;
	else
		pseudosin = distance / disty;
	width = 2 * PLAYER_WIDTH * pseudosin;
	height = ((le->state & STATE_CROUCHED) ? PLAYER_CROUCH : PLAYER_STAND) - PLAYER_MIN;

	acc = torad
		* GET_ACC(selChr->skills[ABILITY_ACCURACY],
			selFD->weaponSkill
			? selChr->skills[selFD->weaponSkill]
			: 0);

	if ((selActor->state & STATE_CROUCHED) && selFD->crouch)
		acc *= selFD->crouch;

	hitchance = width / (2 * distance * tan(acc * (1+selFD->modif) + selFD->spread[0]));
	if (hitchance > 1)
		hitchance = 1;
	if (height / (2 * distance * tan(acc * selFD->spread[1])) < 1)
		hitchance *= height / (2 * distance * tan(acc * (1+selFD->modif) + selFD->spread[1]));

	/* Calculate cover: */
	n = 0;
	height = height / 18;
	width = width / 18;
	target[2] -= UNIT_HEIGHT/2;
	target[2] += height * 9;
	perpX = disty / distance * width;
	perpY = 0 - distx / distance * width;

	target[0] += perpX;
	perpX *= 2;
	target[1] += perpY;
	perpY *= 2;
	target[2] += 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] += 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[2] += 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX * 3;
	target[1] -= perpY * 3;
	target[2] -= 12 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 6 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] -= 4 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 10 * height;
/*	CL_ParticleSpawn("inRangeTracer", 0, shooter, target, NULL); */
	if (!CM_TestLine(shooter, target))
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
	ptl_t *ptl = NULL;

	assert(selFD);

	ptl = CL_ParticleSpawn("*circle", 0, center, NULL, NULL);
	ptl->size[0] = selFD->splrad; /* misuse size vector as radius */
	ptl->size[1] = 1; /* thickness */
	ptl->style = STYLE_CIRCLE;
	ptl->blend = BLEND_BLEND;
	/* free the particle every frame */
	ptl->life = 0.0001;
	Vector4Copy(color, ptl->color);
}


/**
 * @brief Draws line to target.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] toPos The (grid-) position of the target.
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargeting
 * @sa CL_Trace
 */
static void CL_TargetingStraight (pos3_t fromPos, pos3_t toPos)
{
	vec3_t start, end;
	vec3_t dir, mid;
	vec3_t mins, maxs;
	trace_t tr;
	int oldLevel, i;
	float d;
	qboolean crossNo;
	le_t *le;
	le_t *target = NULL;

	if (!selActor || !selFD)
		return;

	Grid_PosToVec(&clMap, fromPos, start);
	Grid_PosToVec(&clMap, toPos, end);

	/* calculate direction */
	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	/* calculate 'out of range point' if there is one */
	if (VectorDistSqr(start, end) > selFD->range * selFD->range) {
		VectorMA(start, selFD->range, dir, mid);
		crossNo = qtrue;
	} else{
		VectorCopy(end, mid);
		crossNo = qfalse;
	}

	/* switch up to top level, this is a bit of a hack to make sure our trace doesn't go through ceilings ... */
	oldLevel = cl_worldlevel->integer;
	cl_worldlevel->integer = map_maxlevel-1;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && !(le->state & STATE_DEAD) && (le->type == ET_ACTOR || le->type == ET_UGV) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

	/* trace for obstacles */
	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);
	tr = CL_Trace(start, mid, mins, maxs, selActor, target, MASK_SHOT);

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
 * @param[in] toPos The (grid-) position of the target.
 * @todo Find out why the ceiling is not checked against the parabola when calculating obstacles.
 * i.e. the line is drawn green even if a ceiling prevents the shot.
 * https://sourceforge.net/tracker/index.php?func=detail&aid=1701263&group_id=157793&atid=805242
 * @sa CL_TargetingStraight
 */
static void CL_TargetingGrenade (pos3_t fromPos, pos3_t toPos)
{
	vec3_t from, at, cross;
	float vz, dt;
	vec3_t v0, ds, next;
	vec3_t mins, maxs;
	trace_t tr;
	int oldLevel;
	qboolean obstructed = qfalse;
	int i;
	le_t *le;
	le_t *target = NULL;

	if (!selActor || (fromPos[0] == toPos[0] && fromPos[1] == toPos[1]))
		return;

	/* get vectors, paint cross */
	Grid_PosToVec(&clMap, fromPos, from);
	Grid_PosToVec(&clMap, toPos, at);
	from[2] += selFD->shotOrg[1];

	/* prefer to aim grenades at the ground */
	VectorCopy(at,cross);
	cross[2] -= 9;
	at[2] -= 28;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && !(le->state & STATE_DEAD) && (le->type == ET_ACTOR || le->type == ET_UGV) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

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
	cl_worldlevel->integer = map_maxlevel-1;

	/* paint */
	vz = v0[2];

	/* mins/maxs: Used for obstacle-tracing later on (CL_Trace) */
	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);

	for (i = 0; i < GRENADE_PARTITIONS; i++) {
		VectorAdd(from, ds, next);
		next[2] += dt * (vz - 0.5 * GRAVITY * dt);
		vz -= GRAVITY * dt;
		VectorScale(v0, (i + 1.0) / GRENADE_PARTITIONS, at);

		/* trace for obstacles */
		tr = CL_Trace(from, next, mins, maxs, selActor, target, MASK_SHOT);

		if (tr.fraction < 1.0) {
			obstructed = qtrue;
		}

		/* draw particles */
		/* TODO: character strength should be used here, too
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
		Grid_PosToVec(&clMap, toPos, at);
		CL_Targeting_Radius(at);
	}

	selToHit = 100 * CL_TargetingToHit(toPos);

	/* switch level back to where it was again */
	cl_worldlevel->integer = oldLevel;
}


/**
 * @brief field marker box
 */
static const vec3_t boxSize = { BOX_DELTA_WIDTH, BOX_DELTA_LENGTH, BOX_DELTA_HEIGHT };
#define BoxSize(i,source,target) (target[0]=i*source[0],target[1]=i*source[1],target[2]=source[2])

/**
 * @brief create a targeting box at the given position
 */
void CL_AddTargetingBox (pos3_t pos, qboolean pendBox)
{
	entity_t ent;
	vec3_t realBoxSize;

	memset(&ent, 0, sizeof(entity_t));
	ent.flags = RF_BOX;

	Grid_PosToVec(&clMap, pos, ent.origin);

	/* ok, paint the green box if move is possible */
	if (selActor && actorMoveLength < 0xFF && actorMoveLength <= selActor->TU)
		VectorSet(ent.angles, 0, 1, 0);
	/* and paint a dark blue one if move is impossible or the soldier */
	/* does not have enough TimeUnits left */
	else
		VectorSet(ent.angles, 0, 0, 0.6);

	VectorAdd(ent.origin, boxSize, ent.oldorigin);
	/* color */
	if (mouseActor) {
		ent.alpha = 0.4 + 0.2 * sin((float) cl.time / 80);
		/* paint the box red if the soldiers under the cursor is */
		/* not in our team and no civilian, too */
		if (mouseActor->team != cls.team) {
			switch (mouseActor->team) {
			case TEAM_CIVILIAN:
				/* civilians are yellow */
				VectorSet(ent.angles, 1, 1, 0);
				break;
			default:
				if (mouseActor->team == TEAM_ALIEN) {
					/* TODO: print alien team */
				} else {
					/* multiplayer names */
					menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum];
				}
				/* aliens (and players not in our team [multiplayer]) are red */
				VectorSet(ent.angles, 1, 0, 0);
				break;
			}
		} else {
			/* coop multiplayer games */
			if (mouseActor->pnum != cl.pnum)
				menuText[TEXT_MOUSECURSOR_PLAYERNAMES] = cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum];
			/* paint a light blue box if on our team */
			VectorSet(ent.angles, 0.2, 0.3, 1);
		}
		BoxSize(mouseActor->fieldSize, boxSize, realBoxSize);
		VectorSubtract(ent.origin, realBoxSize, ent.origin);
	} else {
		if (selActor) {
			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		} else {
			VectorSubtract(ent.origin, boxSize, ent.origin);
		}
		ent.alpha = 0.3;
	}
/*	V_AddLight(ent.origin, 256, 1.0, 0, 0); */
	/* if pendBox is true then ignore all the previous color considerations and use cyan */
	if (pendBox) {
		VectorSet(ent.angles, 0, 1, 1);
		ent.alpha = 0.15;
	}

	/* add it */
	V_AddEntity(&ent);
}



/**
 * @brief Adds a target.
 * @sa CL_TargetingStraight
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargetingBox
 * @sa CL_TraceMove
 * Draws the tracer (red, yellow, green box) on the grid
 */
extern void CL_AddTargeting (void)
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
			CL_TargetingStraight(selActor->pos, mousePos);
		else
			CL_TargetingGrenade(selActor->pos, mousePos);
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
			CL_TargetingStraight(selActor->pos, mousePendPos);
		else
			CL_TargetingGrenade(selActor->pos, mousePendPos);
		break;
	default:
		break;
	}
}
