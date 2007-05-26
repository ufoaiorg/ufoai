/**
 * @file cl_ufopedia.c
 * @brief Ufopedia script interpreter.
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

static cvar_t *mn_uppretext = NULL;
static cvar_t *mn_uppreavailable = NULL;

static pediaChapter_t	*upChapters_displaylist[MAX_PEDIACHAPTERS];
static int numChapters_displaylist;

static technology_t	*upCurrent;
static int currentChapter = -1;

#define MAX_UPTEXT 4096
static char	upText[MAX_UPTEXT];

/* this buffer is for stuff like aircraft or building info */
static char	upBuffer[MAX_UPTEXT];

/* this variable contains the current firemode number for ammos and weapons */
static int up_firemode;

/* this variable contains the current weapon to display (for an ammo) */
static int up_researchedlink;

/**
 * don't change the order or you have to change the if statements about mn_updisplay cvar
 * in menu_ufopedia.ufo, too
 */
enum {
	UFOPEDIA_CHAPTERS,
	UFOPEDIA_INDEX,
	UFOPEDIA_ARTICLE,

	UFOPEDIA_DISPLAYEND
};
static int upDisplay = UFOPEDIA_CHAPTERS;

/**
 * @brief Checks If a technology/up-entry will be displayed in the ufopedia (-list).
 * @note This does not check for different display modes (only pre-research text, what stats, etc...). The content is mostly checked in UP_Article
 * @return qtrue if the tech gets displayed at all, otherwise qfalse.
 * @sa UP_Article
 */
static qboolean UP_TechGetsDisplayed (technology_t *tech)
{
	return RS_IsResearched_ptr(tech)	/* Is already researched OR ... */
	 || RS_Collected_(tech)	/* ... has collected items OR ... */
	 || (tech->statusResearchable && (tech->pre_description.numDescriptions > 0));
}

/**
 * @brief Modify the global display var
 */
static void UP_ChangeDisplay (int newDisplay)
{
	/* for reseting the scrolling */
	static menuNode_t *ufopedia = NULL, *ufopediaMail = NULL;

	if (newDisplay < UFOPEDIA_DISPLAYEND && newDisplay >= 0)
		upDisplay = newDisplay;
	else
		Com_Printf("Error in UP_ChangeDisplay (%i)\n", newDisplay);

	Cvar_SetValue("mn_uppreavailable", 0);
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

	/**
	 * only fetch this once after ufopedia menu was on the stack (was the
	 * current menu)
	 */
	if (!ufopedia || !ufopediaMail) {
		ufopedia = MN_GetNodeFromCurrentMenu("ufopedia");
		ufopediaMail = MN_GetNodeFromCurrentMenu("ufopedia_mail");
	}

	/* maybe we call this function and the ufopedia is not on the menu stack */
	if (ufopedia && ufopediaMail) {
		ufopedia->textScroll = ufopediaMail->textScroll = 0;
	}

	/* make sure, that we leave the mail header space */
	menuText[TEXT_UFOPEDIA_MAILHEADER] = NULL;
	Cvar_Set("mn_up_mail", "0"); /* use strings here - no int */

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		/* confunc */
		Cbuf_AddText("mn_upfbig\n");
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		currentChapter = -1;
		break;
	case UFOPEDIA_INDEX:
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		/* no break here */
	case UFOPEDIA_ARTICLE:
		/* confunc */
		Cbuf_AddText("mn_upfsmall\n");
		break;
	}
	Cvar_SetValue("mn_updisplay", upDisplay);
}

/**
 * @brief Translate a weaponSkill int to a translated string
 *
 * The weaponSkills were defined in q_shared.h at abilityskills_t
 * @sa abilityskills_t
 */
static char* CL_WeaponSkillToName (int weaponSkill)
{
	switch (weaponSkill) {
	case SKILL_CLOSE:
		return _("Close");
		break;
	case SKILL_HEAVY:
		return _("Heavy");
		break;
	case SKILL_ASSAULT:
		return _("Assault");
		break;
	case SKILL_SNIPER:
		return _("Sniper");
		break;
	case SKILL_EXPLOSIVE:
		return _("Explosive");
		break;
	default:
		return _("Unknown weapon skill");
		break;
	}
}

/**
 * @brief Diplays the tech tree dependencies in the ufopedia
 * @sa UP_DrawEntry
 */
static void UP_DisplayTechTree (technology_t* t)
{
	int i = 0;
	static char up_techtree[1024];
	requirements_t *required = NULL;
	technology_t *techRequired = NULL;
	required = &t->require_AND;
	up_techtree[0] = '\0';
	for (; i < required->numLinks; i++) {
		if (required->type[i] == RS_LINK_TECH) {
			if (!Q_strncmp(required->id[i], "nothing", MAX_VAR)
			 || !Q_strncmp(required->id[i], "initial", MAX_VAR)) {
				Q_strcat(up_techtree, _("No requirements"), sizeof(up_techtree));
				continue;
			} else {
				techRequired = RS_GetTechByIDX(required->idx[i]);
				if (!techRequired)
					Sys_Error("Could not find the tech for '%s'\n", required->id[i]);
				Q_strcat(up_techtree, _(techRequired->name), sizeof(up_techtree));
			}
			Q_strcat(up_techtree, "\n", sizeof(up_techtree));
		}
	}
	/* and now set the buffer to the right menuText */
	menuText[TEXT_LIST] = up_techtree;
}

/**
 * @brief Prints the (ufopedia and other) description for items (weapons, armor, ...)
 * @param item Index in object definition array ods for the item
 * @sa UP_DrawEntry
 * @sa BS_BuySelect_f
 * @sa BS_BuyType_f
 * @sa BS_BuyItem_f
 * @sa BS_SellItem_f
 * @sa MN_Drag
 * Not only called from Ufopedia but also from other places to display
 * weapon and ammo stats
 * @todo Do we need to add checks for (od->type == "dummy") here somewhere?
 */
extern void CL_ItemDescription (int item)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	objDef_t *od;
	int i;
	int up_numresearchedlink = 0;
	int up_weapon_id = 0;
	menu_t *activeMenu = NULL;

	activeMenu = MN_ActiveMenu();

	/* select item */
	od = &csi.ods[item];
	Cvar_Set("mn_itemname", _(od->name));
	Cvar_Set("mn_item", od->id);
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

#ifdef DEBUG
	if (!od->tech && ccs.singleplayer) {
		Com_sprintf(itemText, sizeof(itemText), "Error - no tech assigned\n");
		menuText[TEXT_STANDARD] = itemText;
	} else
#endif
	/* set description text */
	if (RS_IsResearched_ptr(od->tech)) {
		*itemText = '\0';
		if (!Q_strncmp(od->type, "armor", 5)) {
			if (Q_strncmp(activeMenu->name, "equipment", 9)) {
				/* next two lines are not merge in one to avoid to have several entries in .po files (first line will be used again) */
				Q_strcat(itemText, va(_("Size:\t%i\n"),od->size), sizeof(itemText));
				Q_strcat(itemText, "\n", sizeof(itemText));
			}
			Q_strcat(itemText, _("Type:\tProtection / Hardness:\n"), sizeof(itemText));
			for (i = 0; i < csi.numDTs; i++)
				Q_strcat(itemText, va(_("%s\t%i / %i\n"), _(csi.dts[i]), od->protection[i], od->hardness[i]), sizeof(itemText));
		} else if (!Q_strncmp(od->type, "ammo", 4)) {
			if (Q_strncmp(activeMenu->name, "equipment", 9))
				Q_strcat(itemText, va(_("Size:\t%i\n"),od->size), sizeof(itemText));
			/* more will be written below */
		} else if (od->weapon && (od->reload || od->thrown)) {
			Com_sprintf(itemText, sizeof(itemText), _("%s weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			if (Q_strncmp(activeMenu->name, "equipment", 9))
				Q_strcat(itemText, va(_("Size:\t%i\n"),od->size), sizeof(itemText));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
			/* more will be written below */
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			if (Q_strncmp(activeMenu->name, "equipment", 9))
				Q_strcat(itemText, va(_("Size:\t%i\n"),od->size), sizeof(itemText));
			/* more will be written below */
		} else {
			/* just an item */
			/* only primary definition */
			/* @todo: We use the default firemodes here. We might need some change the "fd[0]" below to INV_FiredefsIDXForWeapon(od,weapon_idx) on future changes. */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			if (Q_strncmp(activeMenu->name, "equipment", 9))
				Q_strcat(itemText, va(_("Size:\t%i\n"),od->size), sizeof(itemText));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0][0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0][0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0][0].range / UNIT_SIZE), sizeof(itemText));
		}

		if (!Q_strncmp(od->type, "ammo", 4) || (od->weapon && !od->reload)) {
			/* We store the current technology in upCurrent (needed for changing firemodes while in equip menu) */
			upCurrent = od->tech;

			/* We display the pre/next buttons for changing weapon only if there are at least 2 researched weapons */
			/* up_numresearchedlink contains the number of researched weapons useable with this ammo */
			if (!(od->weapon && !od->reload)) {
				for (i = 0; i < od->numWeapons; i++) {
					if (RS_IsResearched_ptr(csi.ods[od->weap_idx[i]].tech))
						up_numresearchedlink++;
				}
			}
			if (up_numresearchedlink > 1)
				Cvar_Set("mn_changeweapon", "1"); /* use strings here - no int */

			/* We check that up_researchedlink exists for this ammo (in case we switched from an ammo or a weapon with higher value)*/
			if (up_researchedlink > od->numWeapons - 1)
				up_researchedlink = 0;

			up_weapon_id = up_researchedlink;

			/* We only display the name of the associated weapon for an ammo (and not for thrown weapons) */
			if (!(od->weapon && !od->reload)) {
				Cvar_Set("mn_displayweapon", "1"); /* use strings here - no int */
				Cvar_Set("mn_researchedlinkname", csi.ods[od->weap_idx[up_researchedlink]].name);
				Cvar_Set("mn_upresearchedlinknametooltip", va(_("Go to '%s' UFOpedia entry"), csi.ods[od->weap_idx[up_researchedlink]].name));
			}
		} else if (od->weapon) {
			/* We have a weapon that uses ammos */
			/* We store the current technology in upCurrent (needed for changing firemodes while in equip menu) */
			upCurrent = od->tech;

			/* We display the pre/next buttons for changing ammo only if there are at least 2 researched ammo */
			/* up_numresearchedlink contains the number of researched ammos useable with this weapon */
			for (i = 0; i < od->numAmmos; i++) {
				if (RS_IsResearched_ptr(csi.ods[od->ammo_idx[i]].tech))
					up_numresearchedlink++;
			}
			if (up_numresearchedlink > 1)
				Cvar_Set("mn_changeweapon", "2"); /* use strings here - no int */

			/* We check that up_researchedlink exists for this weapon (in case we switched from an ammo or a weapon with higher value)*/
			if (up_researchedlink > od->numAmmos - 1)
				up_researchedlink = 0;

			/* Everything that follows depends only of the ammunition, so we change od to it */
			od = &csi.ods[od->ammo_idx[up_researchedlink]];
			for (i = 0; i < od->numWeapons; i++) {
				if (od->weap_idx[i] == item)
					up_weapon_id = i;
			}

			Cvar_Set("mn_displayweapon", "2"); /* use strings here - no int */
			Cvar_Set("mn_researchedlinkname", od->name);
			Cvar_Set("mn_upresearchedlinknametooltip", va(_("Go to '%s' UFOpedia entry"), od->name));
		}

		if (od->weapon || !Q_strncmp(od->type, "ammo", 4)) {
			/* This contains everything common for weapons and ammos */

			/* We check if the wanted firemode to display exists. */
			if (up_firemode > od->numFiredefs[up_weapon_id] - 1)
				up_firemode = od->numFiredefs[up_weapon_id] - 1;
			if (up_firemode < 0)
				up_firemode = 0;

			/* We always display the name of the firemode for an ammo */
			Cvar_Set("mn_displayfiremode", "1"); /* use strings here - no int */
			Cvar_Set("mn_firemodename", od->fd[up_weapon_id][up_firemode].name);
			/* We display the pre/next buttons for changing firemode only if there are more than one */
			if (od->numFiredefs[up_weapon_id] > 1)
				Cvar_Set("mn_changefiremode", "1"); /* use strings here - no int */

			/* We display the characteristics of this firemode */
			Q_strcat(itemText, va(_("Skill:\t%s\n"), CL_WeaponSkillToName(od->fd[up_weapon_id][up_firemode].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText, va(_("Damage:\t%i\n"),
				(int) ((od->fd[up_weapon_id][up_firemode].damage[0] + od->fd[up_weapon_id][up_firemode].spldmg[0]) * od->fd[up_weapon_id][up_firemode].shots)),
						sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[up_weapon_id][up_firemode].time),  sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[up_weapon_id][up_firemode].range / UNIT_SIZE),  sizeof(itemText));
			Q_strcat(itemText, va(_("Spreads:\t%g\n"),
				(od->fd[up_weapon_id][up_firemode].spread[0] + od->fd[up_weapon_id][up_firemode].spread[1]) / 2),  sizeof(itemText));
		}

		menuText[TEXT_STANDARD] = itemText;
	} else { /* includes if (RS_Collected_(tech)) AFAIK*/
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
		menuText[TEXT_STANDARD] = itemText;
	}
}

/**
 * @brief Prints the ufopedia description for armors
 * @sa UP_DrawEntry
 */
static void UP_ArmorDescription (technology_t* t)
{
	objDef_t	*od = NULL;
	int	i;

	/* select item */
	i = Com_GetItemByID(t->provides);
	od = &csi.ods[i];

#ifdef DEBUG
	if (od == NULL)
		Com_sprintf(upBuffer, sizeof(upBuffer), "Could not find armor definition");
	else if (Q_strncmp(od->type, "armor", MAX_VAR))
		Com_sprintf(upBuffer, sizeof(upBuffer), "Item %s is no armor but %s", od->id, od->type);
	else
#endif
	{
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upimage_top", t->image_top);
		upBuffer[0] = '\0';
		Q_strcat(upBuffer, va(_("Size:\t%i\n"),od->size), sizeof(upBuffer));
		Q_strcat(upBuffer, "\n", sizeof(upBuffer));
		for (i = 0; i < csi.numDTs; i++)
			Q_strcat(upBuffer, va(_("%s:\tProtection: %i\tHardness: %i\n"), _(csi.dts[i]), od->protection[i], od->hardness[i]), sizeof(upBuffer));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for technologies
 * @sa UP_DrawEntry
 */
static void UP_TechDescription (technology_t* t)
{
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for buildings
 * @sa UP_DrawEntry
 */
static void UP_BuildingDescription (technology_t* t)
{
	building_t* b = B_GetBuildingType(t->provides);

	if (!b) {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Error - could not find building") );
	} else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Depends:\t%s\n"), b->dependsBuilding >= 0 ? gd.buildingTypes[b->dependsBuilding].name : _("None") );
		Q_strcat(upBuffer, va(ngettext("Buildtime:\t%i day\n", "Buildtime:\t%i days\n", (int)b->buildTime), (int)b->buildTime ), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Fixcosts:\t%i c\n"), (int)b->fixCosts ), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Running costs:\t%i c\n"), (int)b->varCosts ), sizeof(upBuffer));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for aircraft
 * @note Also checks whether the aircraft tech is already researched or collected
 *
 * @sa UP_DrawEntry
 */
extern void UP_AircraftDescription (technology_t* t)
{
	aircraft_t* aircraft;
	int idx;

	if (RS_IsResearched_ptr(t)) {
		aircraft = AIR_GetAircraft(t->provides);
		if (!aircraft) {
			Com_sprintf(upBuffer, sizeof(upBuffer), _("Error - could not find aircraft") );
		} else {
			Com_sprintf(upBuffer, sizeof(upBuffer), _("Speed:\t%i\n"), aircraft->stats[AIR_STATS_SPEED]);
			Q_strcat(upBuffer, va(_("Fuel:\t%i\n"), aircraft->fuelSize ), sizeof(upBuffer));
			/* Maybe there are standard equipments given */
			idx = aircraft->weapons[0].itemIdx;
			Q_strcat(upBuffer, va(_("Weapon:\t%s\n"), idx >= 0 ? _(gd.technologies[aircraftItems[idx].tech_idx].name) : _("None") ), sizeof(upBuffer));
			idx = aircraft->shield.itemIdx;
			Q_strcat(upBuffer, va(_("Shield:\t%s\n"), idx >= 0 ? _(gd.technologies[aircraftItems[idx].tech_idx].name) : _("None") ), sizeof(upBuffer));
			idx = aircraft->electronics[0].itemIdx;
			Q_strcat(upBuffer, va(_("Equipment:\t%s\n"), idx >= 0 ? _(gd.technologies[aircraftItems[idx].tech_idx].name) : _("None") ), sizeof(upBuffer));
		}
	}
#if 0
	else if RS_Collected_(t) {
		/* Display crippled info and pre-research text here */
	}
#endif
	else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Unknown - need to research this"));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
}

/**
 * @brief Sets the amount of unread/new mails
 * @note This is called every campaign frame - to update gd.numUnreadMails
 * just set it to -1 before calling this function
 * @sa CL_CampaignRun
 */
extern int UP_GetUnreadMails (void)
{
	message_t *m = messageStack;

	if (gd.numUnreadMails != -1)
		return gd.numUnreadMails;

	gd.numUnreadMails = 0;

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				gd.numUnreadMails++;
			break;
		case MSG_RESEARCH_FINISHED:
			if (RS_IsResearched_ptr(m->pedia) && m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
				gd.numUnreadMails++;
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from && m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				gd.numUnreadMails++;
			if (m->pedia->mail[TECHMAIL_RESEARCHED].from && m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
				gd.numUnreadMails++;
			break;
		default:
			break;
		}
		m = m->next;
	}
	return gd.numUnreadMails;
}

/**
 * @brief Binds the mail header (if needed) to the menuText array.
 * @note The cvar mn_up_mail is set to 1 (for activate mail nodes from menu_ufopedia.ufo)
 * @note if there is a mail header.
 * @param[in] tech The tech to generate a header for.
 * @param[in] type The type of mail (research proposal or finished research)
 */
static void UP_SetMailHeader (technology_t* tech, techMailType_t type)
{
	static char mailHeader[8 * MAX_VAR] = ""; /* bigger as techMail_t (utf8) */
	char dateBuf[MAX_VAR] = "";
	const char *subjectType = NULL;

	assert(tech);
	assert(type < TECHMAIL_MAX);

	if (tech->mail[type].date) {
		Q_strncpyz(dateBuf, tech->mail[type].date, sizeof(dateBuf));
	} else {
		switch (type) {
		case TECHMAIL_PRE:
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
				tech->preResearchedDateYear,
				CL_DateGetMonthName(tech->preResearchedDateMonth),
				tech->preResearchedDateDay);
			break;
		case TECHMAIL_RESEARCHED:
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
				tech->researchedDateYear,
				CL_DateGetMonthName(tech->researchedDateMonth),
				tech->researchedDateDay);
				break;
		default:
			Sys_Error("UP_SetMailHeader: unhandled techMailType_t %i for date.\n", type);
		}
	}
	if (tech->mail[type].from) {
		if (!tech->mail[type].read) {
			tech->mail[type].read = qtrue;
			/* reread the unread mails in UP_GetUnreadMails */
			gd.numUnreadMails = -1;
		}
		/* only if mail and mail_pre are available */
		if (tech->numTechMails == TECHMAIL_MAX) {
			switch (type) {
			case TECHMAIL_PRE:
				subjectType = _("Proposal: ");
				break;
			case TECHMAIL_RESEARCHED:
				subjectType = _("Re: ");
				break;
			default:
				Sys_Error("UP_SetMailHeader: unhandled techMailType_t %i for subject.\n", type);
			}
		} else {
			subjectType = "";
		}
		Com_sprintf(mailHeader, sizeof(mailHeader), _("FROM: %s\nTO: %s\nDATE: %s\nSUBJECT: %s%s\n"),
			_(tech->mail[type].from),
			_(tech->mail[type].to),
			dateBuf,
			subjectType,
			_(tech->mail[type].subject));
		menuText[TEXT_UFOPEDIA_MAILHEADER] = mailHeader;
		Cvar_Set("mn_up_mail", "1"); /* use strings here - no int */
	} else {
		menuText[TEXT_UFOPEDIA_MAILHEADER] = NULL;
		Cvar_Set("mn_up_mail", "0"); /* use strings here - no int */
	}
}

/**
 * @brief Display only the TEXT_UFOPEDIA part - don't reset any other menuText pointers here
 * @param[in] tech The technology_t pointer to print the ufopedia article for
 * @sa UP_DrawEntry
 */
extern void UP_Article (technology_t* tech)
{
	int i;

	assert(tech);

	if (RS_IsResearched_ptr(tech)) {
		Cvar_Set("mn_uptitle", va("%s *", _(tech->name)));
		/* If researched -> display research text */
		menuText[TEXT_UFOPEDIA] = _(RS_GetDescription(&tech->description));
		if (tech->pre_description.numDescriptions > 0) {
			/* Display pre-research text and the buttons if a pre-research text is available. */
			if (mn_uppretext->integer) {
				menuText[TEXT_UFOPEDIA] = _(RS_GetDescription(&tech->pre_description));
				UP_SetMailHeader(tech, TECHMAIL_PRE);
			} else {
				UP_SetMailHeader(tech, TECHMAIL_RESEARCHED);
			}
			Cvar_SetValue("mn_uppreavailable", 1);
		} else {
			/* Do not display the pre-research-text button if none is available (no need to even bother clicking there). */
			Cvar_SetValue("mn_uppreavailable", 0);
			Cvar_SetValue("mn_updisplay", 0);
			UP_SetMailHeader(tech, TECHMAIL_RESEARCHED);
			Cvar_SetValue("mn_updisplay", 0);
		}

		if (upCurrent) {
			switch (tech->type) {
			case RS_ARMOR:
				UP_ArmorDescription(tech);
				break;
			case RS_WEAPON:
				for (i = 0; i < csi.numODs; i++) {
					if (!Q_strncmp(tech->provides, csi.ods[i].id, MAX_VAR)) {
						CL_ItemDescription(i);
						UP_DisplayTechTree(tech);
						break;
					}
				}
				break;
			case RS_TECH:
				UP_TechDescription(tech);
				break;
			case RS_CRAFT:
				UP_AircraftDescription(tech);
				break;
			case RS_BUILDING:
				UP_BuildingDescription(tech);
				break;
			default:
				break;
			}
		}
	/* see also UP_TechGetsDisplayed */
	} else if (RS_Collected_(tech) || (tech->statusResearchable && (tech->pre_description.numDescriptions > 0))) {
		/* This tech has something collected or has a research proposal. (i.e. pre-research text) */
		Cvar_Set("mn_uptitle", _(tech->name));
		/* Not researched but some items collected -> display pre-research text if available. */
		if (tech->pre_description.numDescriptions > 0) {
			menuText[TEXT_UFOPEDIA] = _(RS_GetDescription(&tech->pre_description));
			UP_SetMailHeader(tech, TECHMAIL_PRE);
		} else {
			menuText[TEXT_UFOPEDIA] = _("No pre-research description available.");
		}
	} else {
		Cvar_Set("mn_uptitle", _(tech->name));
		menuText[TEXT_UFOPEDIA] = NULL;
	}
}

/**
 * @brief Set the ammo model to display to selected ammo (only for a reloadable weapon)
 * @param tech technology_t pointer for the weapon's tech
 * @sa UP_DrawEntry
 * @sa UP_DecreaseWeapon_f
 * @sa UP_IncreaseWeapon_f
 */
static void UP_DrawAssociatedAmmo (technology_t* tech)
{
	int idx;
	technology_t *t_associated = NULL;

	if (!tech)
		return;

	/* select item */
	idx = Com_GetItemByID(tech->provides);

	/* If this is a weapon, we display the model of the associated ammunition in the lower right */
	if (csi.ods[idx].numAmmos > 0) {
		/* We set t_associated to ammo to display */
		t_associated = csi.ods[csi.ods[idx].ammo_idx[up_researchedlink]].tech;
		assert(t_associated);
		Cvar_Set("mn_upmodel_bottom", t_associated->mdl_top);
	}
}

/**
 * @brief Displays the ufopedia information about a technology
 * @param tech technology_t pointer for the tech to display the information about
 * @sa UP_AircraftDescription
 * @sa UP_BuildingDescription
 * @sa UP_TechDescription
 * @sa UP_ArmorDescription
 * @sa CL_ItemDescription
 */
static void UP_DrawEntry (technology_t* tech)
{
	if (!tech)
		return;

	menuText[TEXT_LIST] = menuText[TEXT_STANDARD] = menuText[TEXT_UFOPEDIA] = NULL;

	Cvar_SetValue("mn_uppreavailable", 0);
	Cvar_Set("mn_upmodel_top", "");
	Cvar_Set("mn_upmodel_bottom", "");
	Cvar_Set("mn_upimage_top", "base/empty");
	if (tech->mdl_top)
		Cvar_Set("mn_upmodel_top", tech->mdl_top);
	if (tech->type == RS_WEAPON)
		UP_DrawAssociatedAmmo(tech);
	if (!tech->mdl_top && tech->image_top)
		Cvar_Set("mn_upimage_top", tech->image_top);
	up_firemode=0;
	up_researchedlink=0;	/*@todo: if the first weapon of the firemode of an ammo is unresearched, its dommages,... will still be displayed*/

	currentChapter = tech->up_chapter;

	UP_ChangeDisplay(UFOPEDIA_ARTICLE);

	UP_Article(tech);
}

/**
 * @brief Opens the ufopedia from everywhere with the entry given through name
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
extern void UP_OpenWith (const char *name)
{
	if (!name)
		return;
	Cbuf_AddText("mn_push ufopedia\n");
	Cbuf_Execute();
	Cbuf_AddText(va("ufopedia %s\n", name));
}

/**
 * @brief Opens the ufopedia with the entry given through name, not deleting copies
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
extern void UP_OpenCopyWith (const char *name)
{
	Cbuf_AddText("mn_push_copy ufopedia\n");
	Cbuf_Execute();
	Cbuf_AddText(va("ufopedia %s\n", name));
}


/**
 * @brief Search and open the ufopedia
 *
 * Usage: ufopedia <id>
 * opens the ufopedia with entry id
 */
static void UP_FindEntry_f (void)
{
	const char *id = NULL;
	technology_t *tech = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: ufopedia <id>\n");
		return;
	}

	/*what are we searching for? */
	id = Cmd_Argv(1);

	/* maybe we get a call like ufopedia "" */
	if (!*id) {
		Com_Printf("UP_FindEntry_f: No PediaEntry given as parameter\n");
		return;
	}

	Com_DPrintf("UP_FindEntry_f: id=\"%s\"\n", id); /*DEBUG */

	tech = RS_GetTechByID(id);

	if (tech) {
		upCurrent = tech;
		UP_DrawEntry(upCurrent);
		return;
	}

	/*if we can not find it */
	Com_DPrintf("UP_FindEntry_f: No PediaEntry found for %s\n", id );
}

/**
 * @brief Displays the chapters in the ufopedia
 * @sa UP_Next_f
 * @sa UP_Prev_f
 */
static void UP_Content_f (void)
{
	char *cp = NULL;
	int i;
	qboolean researched_entries = qfalse;	/* Are there any researched or collected items in this chapter? */
	numChapters_displaylist = 0;

	cp = upText;
	*cp = '\0';

	for (i = 0; i < gd.numChapters; i++) {
		/* Check if there are any researched or collected items in this chapter ... */
		researched_entries = qfalse;
		upCurrent = &gd.technologies[gd.upChapters[i].first];
		do {
			if (UP_TechGetsDisplayed(upCurrent)) {
				researched_entries = qtrue;
				break;
			}
			if (upCurrent->idx != upCurrent->next && upCurrent->next >= 0 )
				upCurrent = &gd.technologies[upCurrent->next];
			else {
				upCurrent = NULL;
			}
		} while (upCurrent);

		/* .. and if so add them to the displaylist of chapters. */
		if (researched_entries) {
			upChapters_displaylist[numChapters_displaylist++] = &gd.upChapters[i];
			Q_strcat(cp, _(gd.upChapters[i].name), sizeof(upText));
			Q_strcat(cp, "\n", sizeof(upText));
		}
	}

	UP_ChangeDisplay(UFOPEDIA_CHAPTERS);

	upCurrent = NULL;
	menuText[TEXT_STANDARD] = NULL;
	menuText[TEXT_UFOPEDIA] = upText;
	menuText[TEXT_LIST] = NULL;
	Cvar_Set("mn_uptitle", _("Ufopedia Content"));
}

/**
 * @brief Displays the index of the current chapter
 * @sa UP_Content_f
 */
static void UP_Index_f (void)
{
	technology_t* t;
	char *upIndex = NULL;
	int chapter = 0;

	if (Cmd_Argc() < 2 && currentChapter == -1) {
		Com_Printf("Usage: mn_upindex <chapter-id>\n");
		return;
	} else if (Cmd_Argc() == 2) {
		chapter = atoi(Cmd_Argv(1));
		if (chapter < gd.numChapters && chapter >= 0) {
			currentChapter = chapter;
		}
	}

	if (currentChapter < 0)
		return;

	UP_ChangeDisplay(UFOPEDIA_INDEX);

	upIndex = upText;
	*upIndex = '\0';

	menuText[TEXT_STANDARD] = NULL;
	menuText[TEXT_UFOPEDIA] = upIndex;
	menuText[TEXT_LIST] = NULL;
	Cvar_Set("mn_uptitle", va(_("Ufopedia Index: %s"), _(gd.upChapters[currentChapter].name)));

	t = &gd.technologies[gd.upChapters[currentChapter].first];

	/* get next entry */
	while (t) {
		if (UP_TechGetsDisplayed(t)) {
			/* Add this tech to the index - it gets dsiplayed. */
			Q_strcat(upText, va("%s\n", _(t->name)), sizeof(upText));
		}
		if (t->next >= 0)
			t = &gd.technologies[t->next];
		else
			t = NULL;
	}
}

/**
 * @brief Displays the index of the current chapter
 * @sa UP_Content_f
 * @sa UP_ChangeDisplay
 * @sa UP_Index_f
 * @sa UP_DrawEntry
 */
static void UP_Back_f (void)
{
	switch (upDisplay) {
	case UFOPEDIA_ARTICLE:
		UP_Index_f();
		break;
	case UFOPEDIA_INDEX:
		UP_Content_f();
		break;
	default:
		break;
	}
}

/**
 * @brief Displays the previous entry in the ufopedia
 * @sa UP_Next_f
 */
static void UP_Prev_f (void)
{
	technology_t *t = NULL;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	/* get previous entry */
	if (t->prev >= 0) {
		/* Check if the previous entry is researched already otherwise go to the next entry. */
		do {
			t = &gd.technologies[t->prev];
			assert(t);
			if (t->idx == t->prev)
				Sys_Error("UP_Prev_f: The 'prev':%d entry equals to 'idx' entry for '%s'.\n", t->prev, t->id);
		} while (t->prev >= 0 && !UP_TechGetsDisplayed(t));

		if (UP_TechGetsDisplayed(t)) {
			upCurrent = t;
			UP_DrawEntry(t);
			return;
		}
	}

	/* Go to chapter index if no more previous entries available. */
	Cbuf_AddText("mn_upindex;");
}

/**
 * @brief Displays the next entry in the ufopedia
 * @sa UP_Prev_f
 */
static void UP_Next_f (void)
{
	technology_t *t = NULL;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	/* get next entry */
	if (t && (t->next >= 0)) {
		/* Check if the next entry is researched already otherwise go to the next entry. */
		do {
			t = &gd.technologies[t->next];
			assert(upCurrent);
			if (t->idx == t->next)
				Sys_Error("UP_Next_f: The 'next':%d entry equals to 'idx' entry for '%s'.\n", t->next, t->id);
		} while (t->next >= 0 && !UP_TechGetsDisplayed(t));

		if (UP_TechGetsDisplayed(t)) {
			upCurrent = t;
			UP_DrawEntry(t);
			return;
		}
	}

	/* Go to chapter index if no more previous entries available. */
	Cbuf_AddText("mn_upindex;");
}


/**
 * @brief
 * @sa UP_Click_f
 */
static void UP_RightClick_f (void)
{
	switch (upDisplay) {
	case UFOPEDIA_INDEX:
	case UFOPEDIA_ARTICLE:
		Cbuf_AddText("mn_upback;");
	default:
		break;
	}
}

/**
 * @brief
 * @sa UP_RightClick_f
 */
static void UP_Click_f (void)
{
	int num;
	technology_t *t = NULL;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		if (num < numChapters_displaylist && upChapters_displaylist[num]->first) {
			upCurrent = &gd.technologies[upChapters_displaylist[num]->first];
			do {
				if (UP_TechGetsDisplayed(upCurrent)) {
					Cbuf_AddText(va("mn_upindex %i;", upCurrent->up_chapter));
					return;
				}
				upCurrent = &gd.technologies[upCurrent->next];
			} while (upCurrent);
		}
		break;
	case UFOPEDIA_INDEX:
		assert(currentChapter >= 0);
		t = &gd.technologies[gd.upChapters[currentChapter].first];

		/* get next entry */
		while (t) {
			if (UP_TechGetsDisplayed(t)) {
				/* Add this tech to the index - it gets displayed. */
				if (num > 0)
					num--;
				else
					break;
			}
			if (t->next >= 0)
				t = &gd.technologies[t->next];
			else
				t = NULL;
		}
		upCurrent = t;
		if (upCurrent)
			UP_DrawEntry(upCurrent);
		break;
	case UFOPEDIA_ARTICLE:
		/* we don't want the click function parameter in our index function */
		Cmd_BufClear();
		/* return back to the index */
		UP_Index_f();
		break;
	default:
		Com_Printf("Unknown ufopedia display value\n");
	}
}

/**
 * @brief
 * @sa
 * @todo The "num" value and the link-index will most probably not match.
 */
static void UP_TechTreeClick_f (void)
{
	int num;
	requirements_t *required_AND = NULL;
	technology_t *techRequired = NULL;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	if (!upCurrent)
		return;

	required_AND = &upCurrent->require_AND;

	if (!Q_strncmp(required_AND->id[num], "nothing", MAX_VAR)
		|| !Q_strncmp(required_AND->id[num], "initial", MAX_VAR))
		return;

	if (num >= required_AND->numLinks)
		return;

	techRequired = RS_GetTechByIDX(required_AND->idx[num]);
	if (!techRequired)
		Sys_Error("Could not find the tech for '%s'\n", required_AND->id[num]);

	/* maybe there is no ufopedia chapter assigned - this tech should not be opened at all */
	if (techRequired->up_chapter == -1)
		return;

	UP_OpenWith(techRequired->id);
}

/**
 * @brief Redraw the ufopedia article
 */
static void UP_Update_f (void)
{
	if (upCurrent)
		UP_DrawEntry(upCurrent);
}

/**
 * @brief Shows available ufopedia entries
 * @todo: Implement me
 */
static void UP_List_f (void)
{
}

/**
 * @brief Mailclient click function callback
 * @sa UP_OpenMail_f
 */
static void UP_MailClientClick_f (void)
{
	message_t *m = messageStack;
	int num, cnt = -1;

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (!m->pedia->mail[TECHMAIL_PRE].from)
				break;
			cnt++;
			if (cnt == num) {
				Cvar_SetValue("mn_uppretext", 1);
				UP_OpenWith(m->pedia->id);
				return;
			}
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			cnt++;
			if (cnt == num) {
				Cvar_SetValue("mn_uppretext", 0);
				UP_OpenWith(m->pedia->id);
				return;
			}
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from || m->pedia->mail[TECHMAIL_RESEARCHED].from) {
				cnt++;
				if (cnt >= num) {
					UP_OpenWith(m->pedia->id);
					return;
				}
			}
			break;
		default:
			break;
		}
		m = m->next;
	}
}

/**
 * @brief Change Ufopedia article when clicking on the name of associated ammo or weapon
 */
static void UP_ResearchedLinkClick_f (void)
{
	technology_t *t = NULL;
	int i;
	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	i = Com_GetItemByID(t->provides);

	if (!Q_strncmp(csi.ods[i].type, "ammo", 4)) {
		t = csi.ods[csi.ods[i].weap_idx[up_researchedlink]].tech;
		if (UP_TechGetsDisplayed (t))
			UP_OpenWith (t->id);
	} else if (csi.ods[i].weapon && csi.ods[i].reload) {
		t = csi.ods[csi.ods[i].ammo_idx[up_researchedlink]].tech;
		if (UP_TechGetsDisplayed (t))
			UP_OpenWith (t->id);
	}
}


#define MAIL_LENGTH 256
#define MAIL_BUFFER_SIZE 0x4000
static char mailBuffer[MAIL_BUFFER_SIZE];
#define CHECK_MAIL_EOL if (tempBuf[MAIL_LENGTH-3] != '\n') tempBuf[MAIL_LENGTH-2] = '\n';
/**
 * @brief Start the mailclient
 * @sa UP_MailClientClick_f
 * @note use TEXT_UFOPEDIA_MAIL in menuText array (33)
 * @sa CP_GetUnreadMails
 */
static void UP_OpenMail_f (void)
{
	char tempBuf[MAIL_LENGTH] = "";
	message_t *m = messageStack;

	*mailBuffer = '\0';

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (!m->pedia->mail[TECHMAIL_PRE].from)
				break;
			if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^BProposal: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("Proposal: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
			CHECK_MAIL_EOL
			Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			if (m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^BRe: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("Re: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
			CHECK_MAIL_EOL
			Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from) {
				if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
				CHECK_MAIL_EOL
				Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			} else if (m->pedia->mail[TECHMAIL_RESEARCHED].from) {
				if (m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				CHECK_MAIL_EOL
				Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			}
			break;
		default:
			break;
		}
#if 0
		if (m->pedia)
			Com_Printf("list: '%s'\n", m->pedia->id);
#endif
		m = m->next;
	}
	menuText[TEXT_UFOPEDIA_MAIL] = mailBuffer;
}

/**
 * @brief Increases the number of the weapon to display (for ammo) or the ammo to display (for weapon)
 * @sa CL_ItemDescription
 */
static void UP_IncreaseWeapon_f (void)
{
	technology_t *t = NULL;
	int up_researchedlink_temp;
	int i;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	/* select item */
	i = Com_GetItemByID(t->provides);

	up_researchedlink_temp = up_researchedlink;
	up_researchedlink_temp++;
	/* We only try to change the value of up_researchedlink if this is possible */
	if (up_researchedlink < csi.ods[i].numWeapons-1) {
		/* this is an ammo */
		while (!RS_IsResearched_ptr(csi.ods[csi.ods[i].weap_idx[up_researchedlink_temp]].tech)) {
			up_researchedlink_temp++;
			if (up_researchedlink_temp > csi.ods[i].numWeapons)
				break;
		}

		if (up_researchedlink_temp < csi.ods[i].numWeapons) {
			up_researchedlink = up_researchedlink_temp;
			CL_ItemDescription(i);
		}
	} else if (up_researchedlink < csi.ods[i].numAmmos-1) {
		/* this is a weapon */
		while (!RS_IsResearched_ptr(csi.ods[csi.ods[i].ammo_idx[up_researchedlink_temp]].tech)) {
			up_researchedlink_temp++;
			if (up_researchedlink_temp > csi.ods[i].numAmmos)
				break;
		}

		if (up_researchedlink_temp < csi.ods[i].numAmmos) {
			up_researchedlink = up_researchedlink_temp;
			UP_DrawAssociatedAmmo(t);
			CL_ItemDescription(i);
		}
	}
}

/**
 * @brief Decreases the number of the firemode to display (for ammo) or the ammo to display (for weapon)
 * @sa CL_ItemDescription
 */
static void UP_DecreaseWeapon_f (void)
{
	technology_t *t = NULL;
	int up_researchedlink_temp;
	int i;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	/* select item */
	i = Com_GetItemByID(t->provides);

	up_researchedlink_temp = up_researchedlink;
	up_researchedlink_temp--;
	/* We only try to change the value of up_researchedlink if this is possible */
	if (up_researchedlink > 0 && csi.ods[i].numWeapons > 0) {
		/* this is an ammo */
		while (!RS_IsResearched_ptr(csi.ods[csi.ods[i].weap_idx[up_researchedlink_temp]].tech)) {
			up_researchedlink_temp--;
			if (up_researchedlink_temp < 0)
				break;
		}

		if (up_researchedlink_temp >= 0) {
			up_researchedlink = up_researchedlink_temp;
			CL_ItemDescription(i);
		}
	} else if (up_researchedlink > 0 && csi.ods[i].numAmmos > 0) {
		/* this is a weapon */
		while (!RS_IsResearched_ptr(csi.ods[csi.ods[i].ammo_idx[up_researchedlink_temp]].tech)) {
			up_researchedlink_temp--;
			if (up_researchedlink_temp < 0)
				break;
		}

		if (up_researchedlink_temp >= 0) {
			up_researchedlink = up_researchedlink_temp;
			UP_DrawAssociatedAmmo(t);
			CL_ItemDescription(i);
		}
	}
}

/**
 * @brief Increases the number of the firemode to display
 * @sa CL_ItemDescription
 */
static void UP_IncreaseFiremode_f (void)
{
	technology_t *t = NULL;
	int i;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	up_firemode++;

	i = Com_GetItemByID(t->provides);
	if (i > -1)
		CL_ItemDescription(i);
}

/**
 * @brief Decreases the number of the firemode to display
 * @sa CL_ItemDescription
 */
static void UP_DecreaseFiremode_f (void)
{
	technology_t *t = NULL;
	int i;

	if (!upCurrent) /* if called from console */
		return;

	t = upCurrent;

	up_firemode--;

	i = Com_GetItemByID(t->provides);
	if (i > -1)
		CL_ItemDescription(i);
}

/**
 * @brief
 * @sa CL_ResetMenus
 */
extern void UP_ResetUfopedia (void)
{
	/* reset menu structures */
	gd.numChapters = 0;

	/* add commands and cvars */
	Cmd_AddCommand("ufopedialist", UP_List_f, NULL);
	Cmd_AddCommand("mn_upindex", UP_Index_f, "Shows the ufopedia index for the current chapter");
	Cmd_AddCommand("mn_upcontent", UP_Content_f, "Shows the ufopedia chapters");
	Cmd_AddCommand("mn_upback", UP_Back_f, "Goes back from article to index or from index to chapters");
	Cmd_AddCommand("mn_upprev", UP_Prev_f, NULL);
	Cmd_AddCommand("mn_upnext", UP_Next_f, NULL);
	Cmd_AddCommand("mn_upupdate", UP_Update_f, NULL);
	Cmd_AddCommand("ufopedia", UP_FindEntry_f, NULL);
	Cmd_AddCommand("ufopedia_click", UP_Click_f, NULL);
	Cmd_AddCommand("mailclient_click", UP_MailClientClick_f, NULL);
	Cmd_AddCommand("ufopedia_rclick", UP_RightClick_f, NULL);
	Cmd_AddCommand("ufopedia_openmail", UP_OpenMail_f, "Start the mailclient");
	Cmd_AddCommand("techtree_click", UP_TechTreeClick_f, NULL);
	Cmd_AddCommand("mn_upgotoresearchedlink", UP_ResearchedLinkClick_f, NULL);
	Cmd_AddCommand("mn_increasefiremode", UP_IncreaseFiremode_f, NULL);
	Cmd_AddCommand("mn_decreasefiremode", UP_DecreaseFiremode_f, NULL);
	Cmd_AddCommand("mn_increaseweapon", UP_IncreaseWeapon_f, NULL);
	Cmd_AddCommand("mn_decreaseweapon", UP_DecreaseWeapon_f, NULL);

	mn_uppretext = Cvar_Get("mn_uppretext", "0", 0, NULL);
	mn_uppreavailable = Cvar_Get("mn_uppreavailable", "0", 0, NULL);
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");
	up_firemode = 0;
	up_researchedlink = 0;	/*@todo: if the first weapon of the firemode of an ammo is unresearched, its dommages,... will still be displayed*/
}

/**
 * @brief Parse the ufopedia chapters from UFO-scriptfiles
 * @param id Chapter ID
 * @param text Text for chapter ID
 * @sa CL_ParseFirstScript
 */
extern void UP_ParseUpChapters (const char *name, char **text)
{
	const char *errhead = "UP_ParseUpChapters: unexpected end of file (names ";
	char *token;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("UP_ParseUpChapters: chapter def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		/* get the id */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* add chapter */
		if (gd.numChapters >= MAX_PEDIACHAPTERS) {
			Com_Printf("UP_ParseUpChapters: too many chapter defs\n");
			return;
		}
		memset(&gd.upChapters[gd.numChapters], 0, sizeof(pediaChapter_t));
		CL_ClientHunkStoreString(token, &gd.upChapters[gd.numChapters].id);
		gd.upChapters[gd.numChapters].idx = gd.numChapters;	/* set self-link */

		/* get the name */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token == '_')
			token++;
		if (!*token)
			continue;
		CL_ClientHunkStoreString(token, &gd.upChapters[gd.numChapters].name);

		gd.numChapters++;
	} while (*text);
}
