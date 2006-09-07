/**
 * @file cl_actor.c
 * @brief Actor related routines.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

static le_t *mouseActor;
static pos3_t mouseLastPos;


/**
 * @brief Writes player action with its data
 */
void MSG_Write_PA(player_action_t player_action, int num, ...)
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
static char *CL_GetSkillString(const int skill)
{
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value\n");
	}
#endif
	switch (skill*10/MAX_SKILL) {
	case 0:
		return _("Pathetic");
	case 1:
		return _("Very Poor");
	case 2:
		return _("Poor");
	case 3:
		return _("Mediocre");
	case 4:
		return _("Good");
	case 5:
		return _("Very Good");
	case 6:
		return _("Excellent");
	case 7:
		return _("Amazing");
	case 8:
		return _("Superhuman");
	case 9:
		return _("Godlike");
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
void CL_CharacterCvars(character_t *chr)
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
void CL_UGVCvars(character_t *chr)
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
void CL_ActorGlobalCVars(void)
{
	le_t *le;
	char str[MAX_VAR];
	int i;

	Cvar_SetValue("mn_numaliensspotted", cl.numAliensSpotted);
	for (i = 0; i < MAX_TEAMLIST; i++) {
		le = cl.teamList[i];
		if (le && !(le->state & STATE_DEAD)) {
			Cvar_Set(va("mn_head%i", i), (char *) le->model2);
			Com_sprintf(str, MAX_VAR, "%i", le->HP);
			Cvar_Set(va("mn_hp%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->maxHP);
			Cvar_Set(va("mn_hpmax%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->TU);
			Cvar_Set(va("mn_tu%i", i), str);
			Com_sprintf(str, MAX_VAR, "%i", le->maxTU);
			Cvar_Set(va("mn_tumax%i", i), str);
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
			Cvar_Set(va("mn_stun%i", i), "0");
			Cvar_Set(va("mn_ap%i", i), "0");
		}
	}
}

/**
 * @brief Refreshes the buttons on the UI?
 *
 * @warning selActor may not be null in most cases
 * @sa CL_ActorUpdateCVars
 */
static void CL_RefreshWeaponButtons(int time)
{
	static int primary_right = -1;
	static int secondary_right = -1;
	static int primary_left = -1;
	static int secondary_left = -1;
	invList_t *weapon;

	if (cl.cmode != M_MOVE && cl.cmode != M_PEND_MOVE) {
		/* If we're not in move mode, leave the rendering of the fire buttons alone */
		primary_right = secondary_right = primary_left = secondary_left = -2;
		return;
	} else if (primary_right == -2) {
		/* skip one cycle to let things settle down */
		primary_right = -1;
		return;
	}

	weapon = RIGHT(selActor);

	if ( !weapon || weapon->item.m == NONE
		 || (csi.ods[weapon->item.t].reload && weapon->item.a == 0)
		 || time < csi.ods[weapon->item.m].fd[FD_PRIMARY].time ) {
		if (primary_right != 0) {
			Cbuf_AddText("dispr\n");
			primary_right = 0;
		}
	} else if (primary_right != 1) {
		Cbuf_AddText("deselpr\n");
		primary_right = 1;
	}

	if ( !weapon || weapon->item.m == NONE
		 || (csi.ods[weapon->item.t].reload && weapon->item.a == 0)
		 || time < csi.ods[weapon->item.m].fd[FD_SECONDARY].time ) {
		if (secondary_right != 0) {
			Cbuf_AddText("dissr\n");
			secondary_right = 0;
		}
	} else if (secondary_right != 1) {
		Cbuf_AddText("deselsr\n");
		secondary_right = 1;
	}

	/* check for two-handed weapon - if not, switch to left hand */
	if (!weapon || !csi.ods[weapon->item.t].twohanded)
		weapon = LEFT(selActor);

	if ( !weapon || weapon->item.m == NONE
		 || (csi.ods[weapon->item.t].reload && weapon->item.a == 0)
		 || time < csi.ods[weapon->item.m].fd[FD_PRIMARY].time ) {
		if (primary_left != 0) {
			Cbuf_AddText("displ\n");
			primary_left = 0;
		}
	} else if (primary_left != 1) {
		Cbuf_AddText("deselpl\n");
		primary_left = 1;
	}

	if ( !weapon || weapon->item.m == NONE
		 || (csi.ods[weapon->item.t].reload && weapon->item.a == 0)
		 || time < csi.ods[weapon->item.m].fd[FD_SECONDARY].time ) {
		if (secondary_left != 0) {
			Cbuf_AddText("dissl\n");
			secondary_left = 0;
		}
	} else if (secondary_left != 1) {
		Cbuf_AddText("deselsl\n");
		secondary_left = 1;
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
void CL_ActorUpdateCVars(void)
{
	static char infoText[MAX_MENUTEXTLEN];
	static char mousetext[MAX_MENUTEXTLEN];
	qboolean refresh;
	char *name;
	int time;

	if (cls.state != ca_active)
		return;

	refresh = (int) Cvar_VariableValue("hud_refresh");
	if (refresh) {
		Cvar_Set("hud_refresh", "0");
		Cvar_Set("cl_worldlevel", cl_worldlevel->string);
	}

	/* set Cvars for all actors */
	CL_ActorGlobalCVars();

	/* force them empty first */
	Cvar_Set("mn_anim", "stand0");
	Cvar_Set("mn_rweapon", "");
	Cvar_Set("mn_lweapon", "");

	menuText[TEXT_MOUSECURSOR_RIGHT] = "";

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
		if (cl.cmode > M_PEND_MOVE) {
			selWeapon = ((cl.cmode - M_PEND_FIRE_PR) / 2) ? LEFT(selActor) : RIGHT(selActor);
		} else {
			selWeapon = ((cl.cmode - M_FIRE_PR) / 2) ? LEFT(selActor) : RIGHT(selActor);
		}

		if (!selWeapon && RIGHT(selActor) && csi.ods[RIGHT(selActor)->item.t].twohanded)
			selWeapon = RIGHT(selActor);

		if (selWeapon) {
			if (selWeapon->item.m == NONE) {
				selFD = NULL;
			} else if (cl.cmode > M_PEND_MOVE) {
				selFD = &csi.ods[selWeapon->item.m].fd[(cl.cmode - M_PEND_FIRE_PR) % 2];
			} else {
				selFD = &csi.ods[selWeapon->item.m].fd[(cl.cmode - M_FIRE_PR) % 2];
			}
		} else {
			selFD = NULL;
		}

		/* write info */
		time = 0;
		if (cl.time < cl.msgTime) {
			/* special message */
			Com_sprintf(infoText, MAX_MENUTEXTLEN, cl.msgText);
		} else if (selActor->state & STATE_PANIC) {
			/* panic */
			Com_sprintf(infoText, MAX_MENUTEXTLEN, _("Currently panics!\n"));
		} else {
			/* in multiplayer RS_ItemIsResearched always returns true, 
			   so we are able to use the aliens weapons */
			if ( cl.cmode != M_MOVE && cl.cmode != M_PEND_MOVE && selWeapon
				 && !RS_ItemIsResearched(csi.ods[selWeapon->item.t].kurz) ) {
				CL_DisplayHudMessage(_("You cannot use this unknown item.\nYou need to research it first.\n"), 2000);
				cl.cmode = M_MOVE;
			}
			/* move or shoot */
			if (cl.cmode != M_MOVE && cl.cmode != M_PEND_MOVE) {
				CL_RefreshWeaponButtons(0);
				if (selWeapon && selFD) {
					Com_sprintf(infoText, MAX_MENUTEXTLEN,
								"%s\n%s (%i) [%i%%] %i\n", csi.ods[selWeapon->item.t].name, selFD->name, selFD->ammo, selToHit, selFD->time);
					Com_sprintf(mousetext, MAX_MENUTEXTLEN,
								"%s: %s (%i) [%i%%] %i\n", csi.ods[selWeapon->item.t].name, selFD->name, selFD->ammo, selToHit, selFD->time);

					menuText[TEXT_MOUSECURSOR_RIGHT] = mousetext;	/* Save the text for later display next to the cursor. */

					time = selFD->time;
				} else if (selWeapon) {
					Com_sprintf(infoText, MAX_MENUTEXTLEN, _("%s\n(empty)\n"), csi.ods[selWeapon->item.t].name);
				} else {
					cl.cmode = M_MOVE;
				}
			}
			if (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE) {
				/* If the mouse is outside the world, blank move */
				if (mouseSpace != MS_WORLD && cl.cmode < M_PEND_MOVE)
					actorMoveLength = 0xFF;

				if (actorMoveLength < 0xFF) {
					Com_sprintf(infoText, MAX_MENUTEXTLEN, _("Armor  %i\tMorale  %i\nMove %i (%i TU left)\n"), selActor->AP, selActor->morale, actorMoveLength, selActor->TU - actorMoveLength);
					CL_RefreshWeaponButtons(selActor->TU - actorMoveLength);
					if ( actorMoveLength <= selActor->TU )
						Com_sprintf(mousetext, MAX_MENUTEXTLEN, "%i (%i)\n", actorMoveLength, selActor->TU);
					else
						Com_sprintf(mousetext, MAX_MENUTEXTLEN, "- (-)\n" );
					menuText[TEXT_MOUSECURSOR_RIGHT] = mousetext;	/* Save the text for later display next to the cursor. */
				} else {
					Com_sprintf(infoText, MAX_MENUTEXTLEN, _("Armor  %i\tMorale  %i\n"), selActor->AP, selActor->morale);
					CL_RefreshWeaponButtons(selActor->TU);
				}
				time = actorMoveLength;
			}
		}

		/* calc remaining TUs */
		time = selActor->TU - time;
		if (time < 0)
			time = 0;
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
			&& csi.ods[RIGHT(selActor)->item.t].twohanded)
			Cvar_Set("mn_ammoleft", Cvar_VariableString("mn_ammoright"));

		/* change stand-crouch */
		if (cl.oldstate != selActor->state || refresh) {
			cl.oldstate = selActor->state;
			if (selActor->state & STATE_CROUCHED) {
				Cbuf_AddText("tocrouch\n");
			} else {
				Cbuf_AddText("tostand\n");
			}

			if (selActor->state & STATE_REACTION) {
				Cbuf_AddText("startreaction\n");
			} else {
				Cbuf_AddText("stopreaction\n");
			}

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
				Cbuf_AddText("tostand\n");

			/* this allows us to display messages even with no actor selected */
			if (cl.time < cl.msgTime) {
				/* special message */
				Com_sprintf(infoText, MAX_MENUTEXTLEN, cl.msgText);
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
		case M_FIRE_PL:
		case M_PEND_FIRE_PL:
			Cbuf_AddText("towpl\n");
			break;
		case M_FIRE_SL:
		case M_PEND_FIRE_SL:
			Cbuf_AddText("towsl\n");
			break;
		case M_FIRE_PR:
		case M_PEND_FIRE_PR:
			Cbuf_AddText("towpr\n");
			break;
		case M_FIRE_SR:
		case M_PEND_FIRE_SR:
			Cbuf_AddText("towsr\n");
			break;
		default:
			/* If we've just changing between move modes, don't reset */
			if (cl.oldcmode != M_MOVE && cl.oldcmode != M_PEND_MOVE)
				Cbuf_AddText("tomov\n");
			break;
		}
		cl.oldcmode = cl.cmode;
	}

	/* player bar */
	if (cl_selected->modified || refresh) {
		int i;

		for (i = 0; i < MAX_TEAMLIST; i++) {
			if (!cl.teamList[i] || cl.teamList[i]->state & STATE_DEAD) {
				Cbuf_AddText(va("huddisable%i\n", i));
			} else if (i == (int) cl_selected->value) {
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
void CL_AddActorToTeamList(le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team || le->pnum != cl.pnum || (le->state & STATE_DEAD))
		return;

	/* check list length */
	if (cl.numTeamList >= MAX_TEAMLIST)
		return;

	/* check list for that actor */
	/* TODO: This should probably be a while loop */
	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le)
			break;
	}

	/* add it */
	if (i == cl.numTeamList) {
		cl.teamList[cl.numTeamList++] = le;
		Cbuf_AddText(va("numonteam%i\n", cl.numTeamList));
		Cbuf_AddText(va("huddeselect%i\n", i));
		if (cl.numTeamList == 1)
			CL_ActorSelectList(0);
	}
}


/**
 * @brief Removes an actor from the team list.
 *
 * @sa CL_AddActorToTeamList
 * @param le Pointer to local entity struct
 */
void CL_RemoveActorFromTeamList(le_t * le)
{
	int i, j;

	if (!le)
		return;

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

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			/* disable hud button */
			Cbuf_AddText(va("huddisable%i\n", i));

			/* remove from list */
			cl.teamList[i] = NULL;

			/* campaign death */
			if (!curCampaign)
				return;

			for (j = 0; j < gd.numEmployees[EMPL_SOLDIER]; j++) {
				if (baseCurrent->curTeam[i]->ucn == gd.employees[EMPL_SOLDIER][j].chr.ucn) {
					/* Mark the soldier as dead, but do not delete him ... just in case there is a mission-retry. See CL_GameResultsCmd for more.*/
					baseCurrent->deathMask |= 1 << j;
					break;
				}
			}
#ifdef DEBUG
			if ( j == gd.numEmployees[EMPL_SOLDIER] )
				Com_Printf("CL_RemoveActorFromTeamList: could not mask as dead - ucn: %i; team:%i.\n", baseCurrent->curTeam[i]->ucn, i);
#endif
			return;
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
qboolean CL_ActorSelect(le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team || (le->state & STATE_DEAD))
		return qfalse;

	/* select him */
	if (selActor)
		selActor->selected = qfalse;
	le->selected = qtrue;
	selActor = le;
	menuInventory = &selActor->i;

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			/* console commands, update cvars */
			Cvar_ForceSet("cl_selected", va("%i", i));
			if ( le->fieldSize == ACTOR_SIZE_NORMAL ) {
				selChr = baseCurrent->curTeam[i];
				CL_CharacterCvars(selChr);
			} else {
				selChr = baseCurrent->curTeam[i];
				CL_UGVCvars(selChr);
			}

			/* calculate possible moves */
			CL_BuildForbiddenList();
			Grid_MoveCalc(&clMap, le->pos, MAX_ROUTE, fb_list, fb_length);

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
qboolean CL_ActorSelectList(int num)
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
	if ((int)cl_centerview->value)
		VectorCopy(le->origin, cl.cam.reforg);
	/* change to worldlevel were actor is right now */
	Cvar_SetValue("cl_worldlevel", le->pos[2]);

	return qtrue;
}


/*
==============================================================

ACTOR MOVEMENT AND SHOOTING

==============================================================
*/

/**
 * @brief A list of locations that cannot be moved to.
 *
 * @see CL_BuildForbiddenList(void)
 */
byte *fb_list[MAX_FB_LIST];
/**
 * @brief Current length of fb_list.
 *
 * @see fb_list
 */
int fb_length;

/**
 * @brief Builds a list of locations that cannot be moved to.
 *
 * TODO: This probably belongs in the core logic.
 * This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
void CL_BuildForbiddenList(void)
{
	le_t *le;
	int i, j, k;

	fb_length = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && !(le->state & STATE_DEAD) && (le->type == ET_ACTOR || le->type == ET_UGV) ) {
			if (le->fieldSize == ACTOR_SIZE_NORMAL) {
				fb_list[fb_length++] = le->pos;
			} else {
				/* FIXME: remove me after getting this stuff ready */
				Com_DPrintf("...unit bigger that 1 field (%i)\n", le->fieldSize);
				/* we don't need the height value because */
				/* the unit is always on the same level */
				for (j = 0; j < le->fieldSize; j++) {
					for (k = 0; k < le->fieldSize; k++) {
						/* FIXME: put all vec3_t's in here */
						fb_list[fb_length++] = le->pos;
					}
				}
			}
		}

	if (fb_length > MAX_FB_LIST)
		Com_Error(ERR_DROP, "CL_BuildForbiddenList: list too long");
}

/**
 * @brief Checks that an action is valid.
 */
int CL_CheckAction(void)
{
	static char infoText[MAX_MENUTEXTLEN];

	if (!selActor) {
		Com_Printf("Nobody selected.\n");
		Com_sprintf(infoText, MAX_MENUTEXTLEN, _("Nobody selected\n"));
		return qfalse;
	}

/*	if ( blockEvents ) {
		Com_Printf( "Can't do that right now.\n" );
		return qfalse;
	}
*/
	if (cls.team != cl.actTeam) {
		Com_Printf("This isn't your round.\n");
		Com_sprintf(infoText, MAX_MENUTEXTLEN, _("This isn't your round\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Draws the way to walk when confirm actions is activated.
 * @param[in] to
 */
int CL_TraceMove(pos3_t to)
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
void CL_ActorStartMove(le_t * le, pos3_t to)
{
	int length;

	if (!CL_CheckAction())
		return;

	length = Grid_MoveLength(&clMap, to, qfalse);

	if (!length || length >= 0xFF) {
		/* move not valid, don't even care to send */
		return;
	}

	/* move seems to be possible; send request to server */
	MSG_Write_PA(PA_MOVE, le->entnum, to);
}


/**
 * @brief Shoot with actor.
 * @param[in] le
 * @param[in] at
 */
void CL_ActorShoot(le_t * le, pos3_t at)
{
	int mode;

	if (!CL_CheckAction())
		return;

	/* send request to server */
	if (cl.cmode > M_PEND_MOVE)
		mode = cl.cmode - M_PEND_FIRE_PR;
	else
		mode = cl.cmode - M_FIRE_PR;
	if (mode >= ST_LEFT_PRIMARY && !LEFT(le))
		mode -= 2;

	MSG_Write_PA(PA_SHOOT, le->entnum, at, mode);
}


/**
 * @brief Reload weapon with actor.
 * @param[in] hand
 * @sa CL_CheckAction
 */
void CL_ActorReload(int hand)
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

	if (inv->c[hand])
		weapon = inv->c[hand]->item.t;
	else {
		/* Check for two-handed weapon */
		hand = 1 - hand;
		if (!inv->c[hand] || !csi.ods[inv->c[hand]->item.t].twohanded)
			return;
		weapon = inv->c[hand]->item.t;
	}

	if (weapon == NONE)
		return; /* TODO: assert? */

	if (!RS_ItemIsResearched(csi.ods[weapon].kurz)) {
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
				if ( csi.ods[ic->item.t].link == weapon
					 && RS_ItemIsResearched(csi.ods[ic->item.t].kurz) ) {
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
 * @brief Moves actor.
 * @param[in] sb
 */
void CL_ActorDoMove(sizebuf_t * sb)
{
	le_t *le;

	/* get le */
	le = LE_Get(MSG_ReadShort(sb));
	if (!le) {
		Com_Printf("Can't move, LE doesn't exist\n");
		return;
	}

	/* get length */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_MOVE], &le->pathLength, le->path);

	/* activate PathMove function */
	le->i.c[csi.idFloor] = NULL;
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
void CL_ActorTurnMouse(void)
{
	vec3_t div;
	byte dv;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	/* calculate dv */
	VectorSubtract(mousePos, selActor->pos, div);
	dv = AngleToDV((int) (atan2(div[1], div[0]) * 180 / M_PI));

	/* send message to server */
	MSG_Write_PA(PA_TURN, selActor->entnum, dv);
}


/**
 * @brief Turns actor.
 * @param[in] sb
 */
void CL_ActorDoTurn(sizebuf_t *sb)
{
	le_t *le;
	int entnum, dir;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_TURN], &entnum, &dir);

	/* get le */
	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("Can't turn, LE doesn't exist\n");
		return;
	}

	le->dir = dir;
	le->angles[YAW] = dangle[le->dir];

	/* calculate possible moves */
	CL_BuildForbiddenList();
	Grid_MoveCalc(&clMap, le->pos, MAX_ROUTE, fb_list, fb_length);
}


/**
 * @brief Stands or crouches actor.
 */
void CL_ActorStandCrouch(void)
{
	if (!CL_CheckAction())
		return;

	if (selActor->fieldSize == ACTOR_SIZE_UGV )
		return;
	/* send message to server */
	MSG_Write_PA(PA_STATE, selActor->entnum, selActor->state ^ STATE_CROUCHED);
}


/**
 * @brief Stuns an actor.
 *
 * Stunning is handled as a dead actor but afterwards in CL_CollectAliens we only collect aliens with STATE_STUN
 * @note: we can do this because STATE_STUN is 0x43 and STATE_DEAD is 0x03 (checking for STATE_DEAD is also true when STATE_STUN was set)
 * @note: Do we really need this as a script command? Currently there is no binding - but who knows?
 */
void CL_ActorStun(void)
{
	if (!CL_CheckAction())
		return;

	/* send message to server */
	MSG_Write_PA(PA_STATE, selActor->entnum, selActor->state ^ STATE_STUN);
}


/**
 * @brief Toggles reaction fire.
 */
void CL_ActorToggleReaction(void)
{
	if (!CL_CheckAction())
		return;

	/* send message to server */
	MSG_Write_PA(PA_STATE, selActor->entnum, selActor->state ^ STATE_REACTION);
}

/**
 * @brief Records if shot is first shot.
 */
static qboolean firstShot = qfalse;

/**
 * @brief Shoot with weapon.
 */
void CL_ActorDoShoot(sizebuf_t * sb)
{
	fireDef_t *fd;
	le_t *le;
	int type;
	vec3_t muzzle, impact;
	int flags, normal, number;

	/* read data */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_SHOOT], &number, &type, &flags, &muzzle, &impact, &normal);

	/* get le */
	le = LE_Get(number);

	/* get the fire def */
	fd = GET_FIREDEF(type);

	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0]
		&& !(flags & SF_BOUNCED))
		S_StartLocalSound(fd->fireSound);
	firstShot = qfalse;

	/* do actor related stuff */
	if (!le)
		return;

	/* animate */
	re.AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
	re.AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	Grid_MoveCalc(&clMap, le->pos, MAX_ROUTE, fb_list, fb_length);
}


/**
 * @brief Throw item with actor.
 * @param[in] sb
 */
void CL_ActorDoThrow(sizebuf_t * sb)
{
	fireDef_t *fd;
	int type;
	vec3_t muzzle, v0;
	int flags;
	int dtime;

	/* read data */
	MSG_ReadFormat(sb, ev_format[EV_ACTOR_THROW], &dtime, &type, &flags, &muzzle, &v0);

	/* get the fire def */
	fd = GET_FIREDEF(type);

	/* add effect le (local entity) */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0])
		S_StartLocalSound(fd->fireSound);
	firstShot = qfalse;
}


/**
 * @brief Starts shooting with actor.
 * @param[in] sb
 */
void CL_ActorStartShoot(sizebuf_t * sb)
{
	fireDef_t *fd;
	le_t *le;
	pos3_t from, target;
	int number, type;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_START_SHOOT], &number, &type, &from, &target);

	fd = GET_FIREDEF(type);
	le = LE_Get(number);

	/* center view (if wanted) */
	if ((int) cl_centerview->value && cl.actTeam != cls.team)
		CL_CameraRoute(from, target);

	/* first shot */
	firstShot = qtrue;

	/* actor dependant stuff following */
	if (!le)
		return;

	/* erase one-time weapons from storage --- which ones?
	if (curCampaign && le->team == cls.team && !csi.ods[type].ammo) {
		if (ccs.eMission.num[type])
			ccs.eMission.num[type]--;
	} */

	/* animate */
	re.AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
}


/**
 * @brief Kills actor.
 * @param[in] sb
 */
void CL_ActorDie(sizebuf_t * sb)
{
	le_t *le;
	int number, state;

	MSG_ReadFormat(sb, ev_format[EV_ACTOR_DIE], &number, &state);

	/* get le */
	le = LE_Get(number);
	if (!le)
		return;

	/* count spotted aliens */
	if (le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted--;

	/* set relevant vars */
	le->i.c[csi.idFloor] = NULL;
	le->HP = 0;
	le->STUN = 0;
	le->state = state;

	/* play animation */
	le->think = NULL;
	re.AnimChange(&le->as, le->model1, va("death%i", le->state & STATE_DEAD));
	re.AnimAppend(&le->as, le->model1, va("dead%i", le->state & STATE_DEAD));

	CL_RemoveActorFromTeamList(le);

	/* calculate possible moves */
	CL_BuildForbiddenList();
	if (selActor) {
		Grid_MoveCalc(&clMap, selActor->pos, MAX_ROUTE, fb_list, fb_length);
		CL_ResetActorMoveLength();
	}
}


/*
==============================================================

MOUSE INPUT

==============================================================
*/

/**
 * @brief Selects an actor using the mouse.
 */
void CL_ActorSelectMouse(void)
{
	if (mouseSpace != MS_WORLD)
		return;

	if (M_MOVE == cl.cmode || M_PEND_MOVE == cl.cmode) {
		cl.cmode = M_MOVE;

		/* Try and select another team member */
		if (!CL_ActorSelect(mouseActor)) {
			/* If another team member wasn't selected, move the currently */
			/* selected team member to wherever the mouse was clicked */
			if (confirm_actions->value) {
				cl.cmode = M_PEND_MOVE;
			} else {
				CL_ActorStartMove(selActor, mousePos);
			}
		}
	} else if (cl.cmode > M_PEND_MOVE) {
		cl.cmode -= M_PEND_FIRE_PR - M_FIRE_PR;
	} else if (confirm_actions->value) {
		cl.cmode += M_PEND_FIRE_PR - M_FIRE_PR;
	} else {
		CL_ActorShoot(selActor, mousePos);
	}
}


/**
 * @brief initiates action with mouse.
 * @sa CL_ActionDown
 * @sa CL_ActorStartMove
 */
void CL_ActorActionMouse(void)
{
	if (!selActor || mouseSpace != MS_WORLD)
		return;

	if (cl.cmode == M_MOVE) {
		if (confirm_actions->value)
			cl.cmode = M_PEND_MOVE;
		else
			CL_ActorStartMove(selActor, mousePos);
	} else
		cl.cmode = M_MOVE;
}


/*==============================================================

ROUND MANAGEMENT

==============================================================*/

/**
 * @brief Finishes the current round of the player in battlescape and starts the round for the next team.
 */
void CL_NextRound(void)
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
void CL_DisplayHudMessage(char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}

/**
 * @brief Performs end-of-turn processing.
 * @param[in] sb
 */
void CL_DoEndRound(sizebuf_t * sb)
{
	/* hud changes */
	if (cls.team == cl.actTeam)
		Cbuf_AddText("endround\n");

	/* change active player */
	Com_Printf("Team %i ended round", cl.actTeam);
	cl.actTeam = MSG_ReadByte(sb);
	Com_Printf(", team %i's round started!\n", cl.actTeam);

	/* check whether a particle has to go */
	CL_ParticleCheckRounds();

	/* hud changes */
	if (cls.team == cl.actTeam) {
		Cbuf_AddText("startround\n");
		CL_DisplayHudMessage(_("Your round started!\n"), 2000);
		S_StartLocalSound("misc/roundstart.wav");
		if (selActor) {
			CL_BuildForbiddenList();
			Grid_MoveCalc(&clMap, selActor->pos, MAX_ROUTE, fb_list, fb_length);
		}
	}
}


/*
==============================================================

MOUSE SCANNING

==============================================================
*/

/**
 * @brief Resets mouse possition.
 *
 * TODO: This really doesn't sound like it belongs in here.
 */
void CL_ResetMouseLastPos(void)
{
	mouseLastPos[0] = mouseLastPos[1] = mouseLastPos[2] = 0.0;
}

/**
 * @brief Recalculates the currently selected Actor's move length.
 * */
void CL_ResetActorMoveLength(void) {
	actorMoveLength = Grid_MoveLength(&clMap, mousePos, qfalse);
	if (selActor->state & STATE_CROUCHED)
		actorMoveLength *= 1.5;
}

/**
 * @brief Selects the actor on the battlescape.
 * @note Sets global var mouseActor to current selected le
 * @sa CL_ParseInput
 */
void CL_ActorMouseTrace(void)
{
	float d;
	vec3_t angles;
	vec3_t fw, forward, stop;
	vec3_t end;
	le_t *le;
	int i;
	vec3_t from;

	/* get camera parameters */
	d = (scr_vrect.width / 2) / tan((FOV / cl.cam.zoom) * M_PI / 360);
	angles[YAW] = atan((mx * viddef.rx - scr_vrect.width / 2 - scr_vrect.x) / d) * 180 / M_PI;
	angles[PITCH] = atan((my * viddef.ry - scr_vrect.height / 2 - scr_vrect.y) / d) * 180 / M_PI;
	angles[ROLL] = 0;

	/* get trace vectors */
	if (camera_mode == CAMERA_MODE_FIRSTPERSON) {
		VectorCopy(selActor->origin, from);
		if (!(selActor->state & STATE_CROUCHED))
			from[2] += 10;		/* raise from waist to head */
		angles[PITCH] += cl.cam.angles[PITCH];
		angles[YAW] = cl.cam.angles[YAW] - angles[YAW];
		angles[ROLL] += cl.cam.angles[ROLL];
		AngleVectors(angles, forward, NULL, NULL);
	} else {
		VectorCopy(cl.cam.camorg, from);
		AngleVectors(angles, fw, NULL, NULL);
		VectorRotate(cl.cam.axis, fw, forward);
	}
	VectorMA(from, 2048, forward, stop);

	/* do the trace */
	CM_EntTestLineDM(cl.cam.camorg, stop, end);

	/* the mouse is out of the world */
	if (end[2] < 0.0)
		return;

	/* get position */
	mousePos[2] = end[2] / UNIT_HEIGHT;
	if (mousePos[2] > cl_worldlevel->value)
		mousePos[2] = cl_worldlevel->value;

	/* FIXME: wrong values if cursor is not centered or outside the map */
	stop[2] = (mousePos[2] + 0.5) * UNIT_HEIGHT;
	stop[0] = end[0] + forward[0] * (end[2] - stop[2]);
	stop[1] = end[1] + forward[1] * (end[2] - stop[2]);

	VecToPos(stop, mousePos);
	mousePos[2] = Grid_Fall(&clMap, mousePos);

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
qboolean CL_AddActor(le_t * le, entity_t * ent)
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
qboolean CL_AddUGV(le_t * le, entity_t * ent)
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
float CL_TargetingToHit(pos3_t toPos)
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

	acc = M_PI / 180
		* GET_ACC(selChr->skills[ABILITY_ACCURACY],
			selFD->weaponSkill
			? selChr->skills[selFD->weaponSkill]
			: 0);

	if ((selActor->state & STATE_CROUCHED) && selFD->crouch)
		acc *= selFD->crouch;

	hitchance = width / (2 * distance * tan(acc * selFD->spread[0]));
	if (hitchance > 1)
		hitchance = 1;
	if (height / (2 * distance * tan(acc * selFD->spread[1])) < 1)
		hitchance *= height / (2 * distance * tan(acc * selFD->spread[1]));

	/*Calculate cover: */
	n = 0;
	height = height / 18;
	width = width / 18;
	target[2] = height * 9;
	perpX = disty / distance * width;
	perpY = 0 - distx / distance * width;

	target[0] += perpX;
	perpX *= 2;
	target[1] += perpY;
	perpY *= 2;
	target[2] += 6 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 6 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] += 4 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 10 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX * 4;
	target[1] -= perpY * 4;
	target[2] += height + height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 6 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] -= 4 * height;
	if (!CM_TestLine(shooter, target))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 10 * height;
	if (!CM_TestLine(shooter, target))
		n++;

	return (hitchance * (0.125) * n);
}

/**
 * @brief Draws line to target.
 * @param[in] fromPos
 * @param[in] toPos
 */
void CL_TargetingStraight(pos3_t fromPos, pos3_t toPos)
{
	vec3_t start, end;
	vec3_t dir, mid;

	if (!selActor || !selFD)
		return;

	Grid_PosToVec(&clMap, fromPos, start);
	Grid_PosToVec(&clMap, toPos, end);

	/* show cross and trace */
	if (VectorDistSqr(start, end) > selFD->range * selFD->range) {
		VectorSubtract(end, start, dir);
		VectorNormalize(dir);
		VectorMA(start, selFD->range, dir, mid);
		/* TODO: set mid to the first obstacle if there is one */
		CL_ParticleSpawn("inRangeTracer", 0, start, mid, NULL);
		CL_ParticleSpawn("longRangeTracer", 0, mid, end, NULL);
		CL_ParticleSpawn("cross_no", 0, end, NULL, NULL);
	} else {
		CL_ParticleSpawn("inRangeTracer", 0, start, end, NULL);
		CL_ParticleSpawn("cross", 0, end, NULL, NULL);
	}

	selToHit = 100 * CL_TargetingToHit(toPos);
}


#define GRENADE_PARTITIONS	20

/**
 * @brief Shows targetting for a grenade.
 * @param[in] fromPos
 * @param[in] toPos
 */
void CL_TargetingGrenade(pos3_t fromPos, pos3_t toPos)
{
	vec3_t from, at;
	float vz, dt;
	vec3_t v0, ds, next;
	int i;

	if (!selActor || (fromPos[0] == toPos[0] && fromPos[1] == toPos[1]))
		return;

	/* get vectors, paint cross */
	Grid_PosToVec(&clMap, fromPos, from);
	Grid_PosToVec(&clMap, toPos, at);

	at[2] -= 9;

	/* calculate parabola */
	dt = Com_GrenadeTarget(from, at, v0);
	if (!dt) {
		CL_ParticleSpawn("cross_no", 0, at, NULL, NULL);
		return;
	}
	if (VectorLength(v0) > selFD->range)
		CL_ParticleSpawn("cross_no", 0, at, NULL, NULL);
	else
		CL_ParticleSpawn("cross", 0, at, NULL, NULL);

	dt /= GRENADE_PARTITIONS;
	VectorSubtract(at, from, ds);
	VectorScale(ds, 1.0 / GRENADE_PARTITIONS, ds);
	ds[2] = 0;

	/* paint */
	vz = v0[2];
	for (i = 0; i < GRENADE_PARTITIONS; i++) {
		VectorAdd(from, ds, next);
		next[2] += dt * (vz - 0.5 * GRAVITY * dt);
		vz -= GRAVITY * dt;
		VectorScale(v0, (i + 1.0) / GRENADE_PARTITIONS, at);
		if (VectorLength(at) > selFD->range)
			/* TODO: also check for obstacles in this condition */
			CL_ParticleSpawn("longRangeTracer", 0, from, next, NULL);
		else
			CL_ParticleSpawn("inRangeTracer", 0, from, next, NULL);
		VectorCopy(next, from);
	}
	selToHit = 100 * CL_TargetingToHit(toPos);
}


/**
 * @brief field marker box
 */
const vec3_t boxSize = { BOX_DELTA_WIDTH, BOX_DELTA_LENGTH, BOX_DELTA_HEIGHT };
#define BoxSize(i,source,target) (target[0]=i*source[0],target[1]=i*source[1],target[2]=source[2])
/**
  * @brief Adds a target.
  *
  * Draws the tracer (red, yellow, green box) on the grid
  */
void CL_AddTargeting(void)
{
	vec3_t realBoxSize;

	if (mouseSpace != MS_WORLD && cl.cmode < M_PEND_MOVE)
		return;

	if (cl.cmode == M_MOVE || cl.cmode == M_PEND_MOVE) {
		entity_t ent;

		memset(&ent, 0, sizeof(entity_t));
		ent.flags = RF_BOX;

		Grid_PosToVec(&clMap, mousePos, ent.origin);

		/* ok, paint the green box if move is possible */
		if (selActor && actorMoveLength < 0xFF && actorMoveLength <= selActor->TU)
			VectorSet(ent.angles, 0, 1, 0);
		/* and paint the blue one if move is impossible or the soldier */
		/* does not have enough TimeUnits left */
		else
			VectorSet(ent.angles, 0, 0, 1);

		VectorAdd(ent.origin, boxSize, ent.oldorigin);
		/* color */
		if (mouseActor) {
			ent.alpha = 0.4 + 0.2 * sin((float) cl.time / 80);
			/* paint the box red if the soldiers under the cursor is */
			/* not in our team and no civilian, too */
			if (mouseActor->team != cls.team)
				switch (mouseActor->team) {
				case TEAM_CIVILIAN:
					/* civilians are yellow */
					VectorSet(ent.angles, 1, 1, 0);
					break;
				default:
					/* aliens (and players not in our team [multiplayer]) are red */
					VectorSet(ent.angles, 1, 0, 0);
					break;
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
/* 		V_AddLight( ent.origin, 256, 1.0, 0, 0 ); */

		/* add it */
		V_AddEntity(&ent);
		if (cl.cmode == M_PEND_MOVE)
			if (!CL_TraceMove(mousePos))
				cl.cmode = M_MOVE;
	} else {
		if (!selActor || !selFD)
			return;

		if (!selFD->gravity)
			CL_TargetingStraight(selActor->pos, mousePos);
		else
			CL_TargetingGrenade(selActor->pos, mousePos);
	}
}
