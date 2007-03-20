/**
 * @file cl_ufopedia.c
 * @brief Ufopedia script interpreter.
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
 * @brief Checks If a technology/up-entry will be displayed in the ufopedia (list).
 * @note This does not check for different display modes (only pre-research text, what stats, etc...).
 * @return qtrue if the tech gets displayed at all, otherwise qfalse.
 */
static qboolean UP_TechGetsDisplayed (technology_t *tech)
{
	return RS_IsResearched_ptr(tech)	/* Is already researched OR ... */
	|| RS_Collected_(tech)			/* ... has collected items OR ... */
	|| ((tech->statusResearchable) && (*tech->pre_description));
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
		Cvar_Set("mn_upimage_bottom", "base/empty");
		currentChapter = -1;
		break;
	case UFOPEDIA_INDEX:
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		Cvar_Set("mn_upimage_bottom", "base/empty");
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
	for (; i<required->numLinks; i++) {
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
 */
extern void CL_ItemDescription (int item)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	objDef_t *od;

	/* select item */
	od = &csi.ods[item];
	Cvar_Set("mn_itemname", _(od->name));

	Cvar_Set("mn_item", od->id);
	Cvar_Set("mn_weapon", "");
	Cvar_Set("mn_ammo", "");

#ifdef DEBUG
	if (!od->tech && ccs.singleplayer) {
		Com_sprintf(itemText, sizeof(itemText), "Error - no tech assigned\n");
		menuText[TEXT_STANDARD] = itemText;
	} else
#endif
	/* set description text */
	if (RS_IsResearched_ptr(od->tech)) {
		if (!Q_strncmp(od->type, "armor", 5)) {
			/* TODO: Print protection */
			Com_sprintf(itemText, sizeof(itemText), _("Armor\n") );
		} else if (!Q_strncmp(od->type, "ammo", 4)) {
			*itemText = '\0';
			/* more will be written below */
		} else if (od->weapon && (od->reload || od->thrown)) {
			Com_sprintf(itemText, sizeof(itemText), _("%s weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			/* more will be written below */
		} else {
			/* just an item */
			/* only primary definition */
			/* TODO: We use the default firemodes here. We might need some change the "fd[0]" below to INV_FiredefsIDXForWeapon(od,weapon_idx) on future changes. */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0][0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0][0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0][0].range / UNIT_SIZE), sizeof(itemText));
		}

		if (!Q_strncmp(od->type, "ammo", 4) || (od->weapon && !od->reload)) {
			/* TODO: We use the default firemodes here. We might need some change the "fd[0]" below to INV_FiredefsIDXForWeapon(od,weapon_idx) on future changes. */
			Q_strcat(itemText, va(_("Primary:\t%s\t(%s)\n"), od->fd[0][0].name, CL_WeaponSkillToName(od->fd[0][0].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText, va(_("Secondary:\t%s\t(%s)\n"), od->fd[0][1].name, CL_WeaponSkillToName(od->fd[0][1].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText,
					va(_("Damage:\t%i / %i\n"), (int) ((od->fd[0][0].damage[0] + od->fd[0][0].spldmg[0]) * od->fd[0][0].shots),
						(int) ((od->fd[0][1].damage[0] + od->fd[0][1].spldmg[0]) * od->fd[0][1].shots)), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i / %i\n"), od->fd[0][0].time, od->fd[0][1].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g / %g\n"), od->fd[0][0].range / UNIT_SIZE, od->fd[0][1].range / 32.0), sizeof(itemText));
			Q_strcat(itemText,
					va(_("Spreads:\t%g / %g\n"), (od->fd[0][0].spread[0] + od->fd[0][0].spread[1]) / 2, (od->fd[0][1].spread[0] + od->fd[0][1].spread[1]) / 2), sizeof(itemText));
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
	for ( i = 0; i < csi.numODs; i++ )
		if ( !Q_strncmp( t->provides, csi.ods[i].id, MAX_VAR ) ) {
			od = &csi.ods[i];
			break;
		}

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
		Cvar_Set("mn_upimage_bottom", "base/empty");
		Cvar_Set("mn_upimage_top", t->image_top);
		upBuffer[0] = '\0';
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
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find building") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Depends:\t%s\n"), b->dependsBuilding >= 0 ? gd.buildingTypes[b->dependsBuilding].name : _("None") );
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

	if (RS_IsResearched_ptr(t)) {
		aircraft = CL_GetAircraft(t->provides);
		if (!aircraft) {
			Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find aircraft") );
		} else {
			Com_sprintf(upBuffer, MAX_UPTEXT, _("Speed:\t%.0f\n"), aircraft->speed );
			Q_strcat(upBuffer, va(_("Fuel:\t%i\n"), aircraft->fuelSize ), sizeof(upBuffer));
			/* Maybe there are standard equipments given */
			Q_strcat(upBuffer, va(_("Weapon:\t%s\n"), aircraft->weapon ? _(aircraft->weapon->name) : _("None") ), sizeof(upBuffer));
			Q_strcat(upBuffer, va(_("Shield:\t%s\n"), aircraft->shield ? _(aircraft->shield->name) : _("None") ), sizeof(upBuffer));
			Q_strcat(upBuffer, va(_("Equipment:\t%s\n"), aircraft->item ? _(aircraft->item->name) : _("None") ), sizeof(upBuffer));
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
		case MSG_RESEARCH:
			if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				gd.numUnreadMails++;
			if (RS_IsResearched_ptr(m->pedia) && m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
				gd.numUnreadMails++;
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from[0] && m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				gd.numUnreadMails++;
			if (m->pedia->mail[TECHMAIL_RESEARCHED].from[0] && m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
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
 * @brief Binds the mail header (if needed) to the menuText array
 * @note The cvar mn_up_mail is set to 1 (for activate mail nodes from menu_ufopedia.ufo)
 * if there is a mail header
 */
static void UP_SetMailHeader (technology_t* tech, techMailType_t type)
{
	static char mailHeader[8 * MAX_VAR] = ""; /* bigger as techMail_t (utf8) */
	char dateBuf[MAX_VAR] = "";
	char *subjectType = NULL;

	assert(tech);
	assert(type < TECHMAIL_MAX);

	if (tech->mail[type].date[0]) {
		Q_strncpyz(dateBuf, tech->mail[type].date, sizeof(dateBuf));
	} else {
		Com_sprintf(dateBuf, sizeof(dateBuf), _("%02i %s %i"),
			tech->researchedDateDay,
			CL_DateGetMonthName(tech->researchedDateMonth),
			tech->researchedDateYear);
	}
	if (tech->mail[type].from[0]) {
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
				Sys_Error("unhandled techMailType_t %i\n", type);
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
	int i, day, month, year;

	assert(tech);

	if (RS_IsResearched_ptr(tech)) {
		day = tech->researchedDateDay;
		month = tech->researchedDateMonth;
		year = tech->researchedDateYear;
		Cvar_Set("mn_uptitle", va("%s *", _(tech->name)));
		/* If researched -> display research text */
		menuText[TEXT_UFOPEDIA] = _(tech->description);
		if (*tech->pre_description) {
			/* Display pre-research text and the buttons if a pre-research text is available. */
			if (mn_uppretext->integer) {
				menuText[TEXT_UFOPEDIA] = _(tech->pre_description);
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
	} else if (RS_Collected_(tech) || ((tech->statusResearchable) && (*tech->pre_description))) {
		/* This tech has something collected or has a research proposal. (i.e. pre-research text) */
		day = tech->preResearchedDateDay;
		month = tech->preResearchedDateMonth;
		year = tech->preResearchedDateYear;
		Cvar_Set("mn_uptitle", _(tech->name));
		/* Not researched but some items collected -> display pre-research text if available. */
		if (*tech->pre_description) {
			menuText[TEXT_UFOPEDIA] = _(tech->pre_description);
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
	Cvar_Set("mn_upimage_bottom", "base/empty");
	if (*tech->mdl_top)
		Cvar_Set("mn_upmodel_top", tech->mdl_top);
	if (*tech->mdl_bottom )
		Cvar_Set("mn_upmodel_bottom", tech->mdl_bottom);
	if (!*tech->mdl_top && *tech->image_top)
		Cvar_Set("mn_upimage_top", tech->image_top);
	if (!*tech->mdl_bottom && *tech->mdl_bottom)
		Cvar_Set("mn_upimage_bottom", tech->image_bottom);

	currentChapter = tech->up_chapter;

	UP_ChangeDisplay(UFOPEDIA_ARTICLE);

	UP_Article(tech);
}

/**
 * @brief Opens the ufopedia from everywhere with the entry given through name
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
extern void UP_OpenWith (char *name)
{
	Cbuf_AddText("mn_push ufopedia\n");
	Cbuf_Execute();
	Cbuf_AddText(va("ufopedia %s\n", name));
}

/**
 * @brief Opens the ufopedia with the entry given through name, not deleting copies
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
extern void UP_OpenCopyWith (char *name)
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
	char *id = NULL;
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
			Q_strcat(cp, gd.upChapters[i].name, MAX_UPTEXT);
			Q_strcat(cp, "\n", MAX_UPTEXT);
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
	Cvar_Set("mn_uptitle", va(_("Ufopedia Index: %s"), gd.upChapters[currentChapter].name));

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
			assert (t);
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
			assert (upCurrent);
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

	UP_OpenCopyWith(techRequired->id);
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
 * TODO: Implement me
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
		/* research entries may have two mails - proposal and re */
		case MSG_RESEARCH:
			cnt++;
			/* pre is the first one - see UP_OpenMail_f */
			if (cnt == num) {
				Cvar_SetValue("mn_uppretext", 1);
				UP_OpenWith(m->pedia->id);
				return;
			}
			if (RS_IsResearched_ptr(m->pedia)) {
				cnt++;
				if (cnt == num) {
					UP_OpenWith(m->pedia->id);
					return;
				}
			}
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from[0] || m->pedia->mail[TECHMAIL_RESEARCHED].from[0]) {
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
 * @brief Start the mailclient
 * @sa UP_MailClientClick_f
 * @note use TEXT_UFOPEDIA_MAIL in menuText array (33)
 * @sa CP_GetUnreadMails
 */
static void UP_OpenMail_f (void)
{
	char tempBuf[256] = "";
	message_t *m = messageStack;

	/* FIXME: not all MSG_RESEARCH appear in our 'mailclient' */
	*upText = '\0';

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH:
#ifdef DEBUG
			if (!m->pedia->mail[TECHMAIL_PRE].from[0])
				Com_Printf("UP_OpenMail_f: No mail for '%s'\n", m->pedia->id);
#endif
			if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^BProposal: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("Proposal: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
			Q_strcat(upText, tempBuf, sizeof(upText));
			if (RS_IsResearched_ptr(m->pedia)) {
#ifdef DEBUG
				if (!m->pedia->mail[TECHMAIL_RESEARCHED].from[0])
					Com_Printf("UP_OpenMail_f: No mail for '%s'\n", m->pedia->id);
#endif
				if (m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^BRe: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("Re: %s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				Q_strcat(upText, tempBuf, sizeof(upText));
			}
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from[0]) {
				if (m->pedia->mail[TECHMAIL_PRE].read == qfalse)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s (%s)\n"), _(m->pedia->mail[TECHMAIL_PRE].subject), _(m->pedia->mail[TECHMAIL_PRE].from));
				Q_strcat(upText, tempBuf, sizeof(upText));
			} else if (m->pedia->mail[TECHMAIL_RESEARCHED].from[0]) {
				if (m->pedia->mail[TECHMAIL_RESEARCHED].read == qfalse)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s (%s)\n"), _(m->pedia->mail[TECHMAIL_RESEARCHED].subject), _(m->pedia->mail[TECHMAIL_RESEARCHED].from));
				Q_strcat(upText, tempBuf, sizeof(upText));
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
	menuText[TEXT_UFOPEDIA_MAIL] = upText;
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

	mn_uppretext = Cvar_Get("mn_uppretext", "0", 0, NULL);
	mn_uppreavailable = Cvar_Get("mn_uppreavailable", "0", 0, NULL);
}

/**
 * @brief Parse the ufopedia chapters from UFO-scriptfiles
 * @param id Chapter ID
 * @param text Text for chapter ID
 * @sa CL_ParseFirstScript
 */
extern void UP_ParseUpChapters (char *id, char **text)
{
	const char *errhead = "UP_ParseUpChapters: unexptected end of file (names ";
	char *token;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("UP_ParseUpChapters: chapter def \"%s\" without body ignored\n", id);
		return;
	}

	do {
		/* get the id */
		token = COM_EParse(text, errhead, id);
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
		Q_strncpyz(gd.upChapters[gd.numChapters].id, token, MAX_VAR);
		gd.upChapters[gd.numChapters].idx = gd.numChapters;	/* set self-link */

		/* get the name */
		token = COM_EParse(text, errhead, id);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token == '_')
			token++;
		if (!*token)
			continue;
		Q_strncpyz(gd.upChapters[gd.numChapters].name, _(token), MAX_VAR);

		gd.numChapters++;
	} while (*text);
}
