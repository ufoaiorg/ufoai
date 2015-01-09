/**
 * @file
 * @brief UFOpaedia script interpreter.
 * @todo Split the mail code into cl_mailclient.c/h
 * @todo Remove direct access to nodes
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

#include "../../cl_shared.h"
#include "../../cl_inventory.h"
#include "../../ui/ui_dataids.h"
#include "../../ui/node/ui_node_option.h" /* OPTIONEXTRADATA */
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_time.h"

static cvar_t* mn_uppretext;
static cvar_t* mn_uppreavailable;

static pediaChapter_t* upChaptersDisplayList[MAX_PEDIACHAPTERS];
static int numChaptersDisplayList;

static technology_t* upCurrentTech;
static pediaChapter_t* currentChapter;

#define MAX_UPTEXT 4096
static char upBuffer[MAX_UPTEXT];

/**
 * @note don't change the order or you have to change the if statements about mn_updisplay cvar
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
 * @brief Checks If a technology/UFOpaedia-entry will be displayed in the UFOpaedia (-list).
 * @note This does not check for different display modes (only pre-research text, what statistics, etc...). The
 * content is mostly checked in @c UP_Article
 * @return true if the tech gets displayed at all, otherwise false.
 * @sa UP_Article
 */
static bool UP_TechGetsDisplayed (const technology_t* tech)
{
	const objDef_t* item;

	assert(tech);
	/* virtual items are hidden */
	item = INVSH_GetItemByIDSilent(tech->provides);
	if (item && item->isVirtual)
		return false;
	/* Is already researched OR has collected items OR (researchable AND have description)
	 * AND not a logical block AND not redirected */
	return (RS_IsResearched_ptr(tech) || RS_Collected_(tech)
	 || (tech->statusResearchable && tech->preDescription.numDescriptions > 0))
	 && tech->type != RS_LOGIC && !tech->redirect;
}

/**
 * @brief Modify the global display var
 * @sa UP_SetMailHeader
 */
static void UP_ChangeDisplay (int newDisplay)
{
	if (newDisplay < UFOPEDIA_DISPLAYEND && newDisplay >= 0)
		upDisplay = newDisplay;
	else
		Com_Printf("Error in UP_ChangeDisplay (%i)\n", newDisplay);

	cgi->Cvar_SetValue("mn_uppreavailable", 0);

	/* make sure, that we leave the mail header space */
	cgi->UI_ResetData(TEXT_UFOPEDIA_MAILHEADER);
	cgi->UI_ResetData(TEXT_UFOPEDIA_MAIL);
	cgi->UI_ResetData(TEXT_UFOPEDIA_REQUIREMENT);
	cgi->UI_ResetData(TEXT_ITEMDESCRIPTION);
	cgi->UI_ResetData(TEXT_UFOPEDIA);

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		currentChapter = nullptr;
		upCurrentTech = nullptr;
		cgi->Cvar_Set("mn_upmodel_top", "");
		cgi->Cvar_Set("mn_upmodel_bottom", "");
		cgi->Cvar_Set("mn_upimage_top", "base/empty");
		cgi->UI_ExecuteConfunc("mn_up_empty");
		cgi->Cvar_Set("mn_uptitle", _("UFOpaedia"));
		break;
	case UFOPEDIA_INDEX:
		cgi->Cvar_Set("mn_upmodel_top", "");
		cgi->Cvar_Set("mn_upmodel_bottom", "");
		cgi->Cvar_Set("mn_upimage_top", "base/empty");
		/* no break here */
	case UFOPEDIA_ARTICLE:
		cgi->UI_ExecuteConfunc("mn_up_article");
		break;
	}
	cgi->Cvar_SetValue("mn_updisplay", upDisplay);
}

/**
 * @brief Translate a aircraft statistic integer to a translated string
 * @sa aircraftParams_t
 * @sa AIR_AircraftMenuStatsValues
 */
static const char* UP_AircraftStatToName (int stat)
{
	switch (stat) {
	case AIR_STATS_SPEED:
		return _("Cruising speed");
	case AIR_STATS_MAXSPEED:
		return _("Maximum speed");
	case AIR_STATS_SHIELD:
		return _("Armour");
	case AIR_STATS_ECM:
		return _("Evasion");
	case AIR_STATS_DAMAGE:
		return _("Aircraft damage");
	case AIR_STATS_ACCURACY:
		return _("Accuracy");
	case AIR_STATS_FUELSIZE:
		return _("Fuel size");
	case AIR_STATS_WRANGE:
		return _("Weapon range");
	default:
		return _("Unknown weapon skill");
	}
}

/**
 * @brief Displays the tech tree dependencies in the UFOpaedia
 * @sa UP_Article
 * @todo Add support for "requireAND"
 * @todo re-iterate trough logic blocks (i.e. append the tech-names it references recursively)
 */
static void UP_DisplayTechTree (const technology_t* t)
{
	linkedList_t* upTechtree;
	const requirements_t* required;

	required = &t->requireAND;
	upTechtree = nullptr;

	if (required->numLinks <= 0)
		cgi->LIST_AddString(&upTechtree, _("No requirements"));
	else {
		for (int i = 0; i < required->numLinks; i++) {
			const requirement_t* req = &required->links[i];
			if (req->type == RS_LINK_TECH) {
				const technology_t* techRequired = req->link.tech;
				if (!techRequired)
					cgi->Com_Error(ERR_DROP, "Could not find the tech for '%s'", req->id);

				/** Only display tech if it is ok to do so.
				 * @todo If it is one (a logic tech) we may want to re-iterate from its requirements? */
				if (!UP_TechGetsDisplayed(techRequired))
					continue;

				cgi->LIST_AddString(&upTechtree, _(techRequired->name));
			}
		}
	}

	/* and now register the buffer */
	cgi->Cvar_Set("mn_uprequirement", "1");
	cgi->UI_RegisterLinkedListText(TEXT_UFOPEDIA_REQUIREMENT, upTechtree);
}

/**
 * @brief Prints the UFOpaedia description for buildings
 * @sa UP_Article
 */
static void UP_BuildingDescription (const technology_t* t)
{
	const building_t* b = B_GetBuildingTemplate(t->provides);

	if (!b) {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Error - could not find building"));
	} else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Needs:\t%s\n"), b->dependsBuilding ? _(b->dependsBuilding->name) : _("None"));
		Q_strcat(upBuffer, sizeof(upBuffer), ngettext("Construction time:\t%i day\n", "Construction time:\t%i days\n", b->buildTime), b->buildTime);
		Q_strcat(upBuffer, sizeof(upBuffer), _("Cost:\t%i c\n"), b->fixCosts);
		Q_strcat(upBuffer, sizeof(upBuffer), _("Running costs:\t%i c\n"), b->varCosts);
	}

	cgi->Cvar_Set("mn_upmetadata", "1");
	cgi->UI_RegisterText(TEXT_ITEMDESCRIPTION, upBuffer);
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the (UFOpaedia and other) description for aircraft items
 * @param item The object definition of the item
 * @sa UP_Article
 * Not only called from UFOpaedia but also from other places to display
 * @todo Don't display things like speed for base defence items - a missile
 * facility isn't getting slower or faster due a special weapon or ammunition
 */
void UP_AircraftItemDescription (const objDef_t* item)
{
	static char itemText[1024];
	const technology_t* tech;

	/* Set menu text node content to null. */
	cgi->INV_ItemDescription(nullptr);
	*itemText = '\0';

	/* no valid item id given */
	if (!item) {
		cgi->Cvar_Set("mn_item", "");
		cgi->Cvar_Set("mn_itemname", "");
		cgi->Cvar_Set("mn_upmodel_top", "");
		cgi->UI_ResetData(TEXT_ITEMDESCRIPTION);
		return;
	}

	tech = RS_GetTechForItem(item);
	/* select item */
	cgi->Cvar_Set("mn_item", "%s", item->id);
	cgi->Cvar_Set("mn_itemname", "%s", _(item->name));
	if (tech->mdl)
		cgi->Cvar_Set("mn_upmodel_top", "%s", tech->mdl);
	else
		cgi->Cvar_Set("mn_upmodel_top", "");

	/* set description text */
	if (RS_IsResearched_ptr(tech)) {
		const objDef_t* ammo = nullptr;

		switch (item->craftitem.type) {
		case AC_ITEM_WEAPON:
				Q_strcat(itemText, sizeof(itemText), _("Weight:\t%s\n"), AII_WeightToName(AII_GetItemWeightBySize(item)));
				break;
		case AC_ITEM_BASE_MISSILE:
		case AC_ITEM_BASE_LASER:
				Q_strcat(itemText, sizeof(itemText), _("Weapon for base defence system\n"));
				break;
		case AC_ITEM_AMMO:
				ammo = item;
				break;
		default:
				break;
		}

		/* check ammo of weapons */
		if (item->craftitem.type <= AC_ITEM_WEAPON) {
			for(int i = 0; i < item->numAmmos; i++)
				if (item->ammos[i]->isVirtual) {
					ammo = item->ammos[i];
					break;
				}
		}

		if (ammo) {
			/* We display the characteristics of this ammo */
			Q_strcat(itemText, sizeof(itemText), _("Ammo:\t%i\n"), ammo->ammo);
			if (!EQUAL(ammo->craftitem.weaponDamage, 0))
				Q_strcat(itemText, sizeof(itemText), _("Damage:\t%i\n"), (int) ammo->craftitem.weaponDamage);
			Q_strcat(itemText, sizeof(itemText), _("Reloading time:\t%i\n"),  (int) ammo->craftitem.weaponDelay);
		}
		/* We write the range of the weapon */
		if (!EQUAL(item->craftitem.stats[AIR_STATS_WRANGE], 0))
			Q_strcat(itemText, sizeof(itemText), "%s:\t%i\n", UP_AircraftStatToName(AIR_STATS_WRANGE),
				AIR_AircraftMenuStatsValues(item->craftitem.stats[AIR_STATS_WRANGE], AIR_STATS_WRANGE));

		/* we scan all stats except weapon range */
		for (int i = 0; i < AIR_STATS_MAX; i++) {
			const char* statsName = UP_AircraftStatToName(i);
			if (i == AIR_STATS_WRANGE)
				continue;
			if (item->craftitem.stats[i] > 2.0f)
				Q_strcat(itemText, sizeof(itemText), "%s:\t+%i\n", statsName, AIR_AircraftMenuStatsValues(item->craftitem.stats[i], i));
			else if (item->craftitem.stats[i] < -2.0f)
				Q_strcat(itemText, sizeof(itemText), "%s:\t%i\n", statsName, AIR_AircraftMenuStatsValues(item->craftitem.stats[i], i));
			else if (item->craftitem.stats[i] > 1.0f)
				Q_strcat(itemText, sizeof(itemText), _("%s:\t+%i %%\n"), statsName, (int)(item->craftitem.stats[i] * 100) - 100);
			else if (!EQUAL(item->craftitem.stats[i], 0))
				Q_strcat(itemText, sizeof(itemText), _("%s:\t%i %%\n"), statsName, (int)(item->craftitem.stats[i] * 100) - 100);
		}
	} else {
		Q_strcat(itemText, sizeof(itemText), _("Unknown - need to research this"));
	}

	cgi->Cvar_Set("mn_upmetadata", "1");
	cgi->UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
}

/**
 * @brief Prints the UFOpaedia description for aircraft
 * @note Also checks whether the aircraft tech is already researched or collected
 * @sa BS_MarketAircraftDescription
 * @sa UP_Article
 */
void UP_AircraftDescription (const technology_t* tech)
{
	cgi->INV_ItemDescription(nullptr);

	/* ensure that the buffer is emptied in every case */
	upBuffer[0] = '\0';

	if (RS_IsResearched_ptr(tech)) {
		const aircraft_t* aircraft = AIR_GetAircraft(tech->provides);
		for (int i = 0; i < AIR_STATS_MAX; i++) {
			switch (i) {
			case AIR_STATS_SPEED:
				/* speed may be converted to km/h : multiply by pi / 180 * earth_radius */
				Q_strcat(upBuffer, sizeof(upBuffer), _("%s:\t%i km/h\n"), UP_AircraftStatToName(i),
					AIR_AircraftMenuStatsValues(aircraft->stats[i], i));
				break;
			case AIR_STATS_MAXSPEED:
				/* speed may be converted to km/h : multiply by pi / 180 * earth_radius */
				Q_strcat(upBuffer, sizeof(upBuffer), _("%s:\t%i km/h\n"), UP_AircraftStatToName(i),
					AIR_AircraftMenuStatsValues(aircraft->stats[i], i));
				break;
			case AIR_STATS_FUELSIZE:
				Q_strcat(upBuffer, sizeof(upBuffer), _("Operational range:\t%i km\n"),
					AIR_GetOperationRange(aircraft));
				break;
			case AIR_STATS_ACCURACY:
				Q_strcat(upBuffer, sizeof(upBuffer), _("%s:\t%i\n"), UP_AircraftStatToName(i),
					AIR_AircraftMenuStatsValues(aircraft->stats[i], i));
				break;
			default:
				break;
			}
		}

		const baseCapacities_t cap = AIR_GetCapacityByAircraftWeight(aircraft);
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(cap);
		const building_t* building = B_GetBuildingTemplateByType(buildingType);

		Q_strcat(upBuffer, sizeof(upBuffer), _("Required Hangar:\t%s\n"), _(building->name));
		/* @note: while MAX_ACTIVETEAM limits the number of soldiers on a craft
		 * there is no use to show this in case of an UFO (would be misleading): */
		if (!AIR_IsUFO(aircraft))
			Q_strcat(upBuffer, sizeof(upBuffer), _("Max. soldiers:\t%i\n"), aircraft->maxTeamSize);
	} else if (RS_Collected_(tech)) {
		/** @todo Display crippled info and pre-research text here */
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Unknown - need to research this"));
	} else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Unknown - need to research this"));
	}

	cgi->Cvar_Set("mn_upmetadata", "1");
	cgi->UI_RegisterText(TEXT_ITEMDESCRIPTION, upBuffer);
	UP_DisplayTechTree(tech);
}

/**
 * @brief Prints the description for robots/ugvs.
 * @param[in] ugvType What type of robot/ugv to print the description for.
 * @sa BS_MarketClick_f
 * @sa UP_Article
 */
void UP_UGVDescription (const ugv_t* ugvType)
{
	static char itemText[512];
	const technology_t* tech;

	assert(ugvType);

	tech = RS_GetTechByProvided(ugvType->id);
	assert(tech);

	cgi->INV_ItemDescription(nullptr);

	/* Set name of ugv/robot */
	cgi->Cvar_Set("mn_itemname", "%s", _(tech->name));
	cgi->Cvar_Set("mn_item", "%s", tech->provides);

	cgi->Cvar_Set("mn_upmetadata", "1");
	if (RS_IsResearched_ptr(tech)) {
		/** @todo make me shiny */
		Com_sprintf(itemText, sizeof(itemText), _("%s\n%s"), _(tech->name), ugvType->weapon);
	} else if (RS_Collected_(tech)) {
		/** @todo Display crippled info and pre-research text here */
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
	} else {
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
	}
	cgi->UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
}

/**
 * @brief Sets the amount of unread/new mails
 * @note This is called every campaign frame - to update ccs.numUnreadMails
 * just set it to -1 before calling this function
 * @sa CP_CampaignRun
 */
int UP_GetUnreadMails (void)
{
	const uiMessageListNodeMessage_t* m = cgi->UI_MessageGetStack();

	if (ccs.numUnreadMails != -1)
		return ccs.numUnreadMails;

	ccs.numUnreadMails = 0;

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			assert(m->pedia);
			if (m->pedia->mail[TECHMAIL_PRE].from && !m->pedia->mail[TECHMAIL_PRE].read)
				ccs.numUnreadMails++;
			break;
		case MSG_RESEARCH_FINISHED:
			assert(m->pedia);
			if (m->pedia->mail[TECHMAIL_RESEARCHED].from && RS_IsResearched_ptr(m->pedia) && !m->pedia->mail[TECHMAIL_RESEARCHED].read)
				ccs.numUnreadMails++;
			break;
		case MSG_NEWS:
			assert(m->pedia);
			if (m->pedia->mail[TECHMAIL_PRE].from && !m->pedia->mail[TECHMAIL_PRE].read)
				ccs.numUnreadMails++;
			if (m->pedia->mail[TECHMAIL_RESEARCHED].from && !m->pedia->mail[TECHMAIL_RESEARCHED].read)
				ccs.numUnreadMails++;
			break;
		case MSG_EVENT:
			assert(m->eventMail);
			if (!m->eventMail->read)
				ccs.numUnreadMails++;
			break;
		default:
			break;
		}
		m = m->next;
	}

	/* use strings here */
	cgi->Cvar_Set("mn_upunreadmail", "%i", ccs.numUnreadMails);
	return ccs.numUnreadMails;
}

/**
 * @brief Binds the mail header (if needed) to the mn.menuText array.
 * @note if there is a mail header.
 * @param[in] tech The tech to generate a header for.
 * @param[in] type The type of mail (research proposal or finished research)
 * @param[in] mail The mail descriptor structure
 * @sa UP_ChangeDisplay
 * @sa CL_EventAddMail_f
 * @sa CL_GetEventMail
 */
static void UP_SetMailHeader (technology_t* tech, techMailType_t type, eventMail_t* mail)
{
	static char mailHeader[8 * MAX_VAR] = ""; /* bigger as techMail_t (utf8) */
	char dateBuf[MAX_VAR] = "";
	const char* subjectType = "";
	const char* from, *to, *subject, *model;
	dateLong_t date;

	if (mail) {
		from = mail->from;
		to = mail->to;
		model = mail->model;
		subject = mail->subject;
		Q_strncpyz(dateBuf, _(mail->date), sizeof(dateBuf));
		mail->read = true;
		/* reread the unread mails in UP_GetUnreadMails */
		ccs.numUnreadMails = -1;
	} else {
		techMail_t* mail;
		assert(tech);
		assert(type < TECHMAIL_MAX);

		mail = &tech->mail[type];
		from = mail->from;
		to = mail->to;
		subject = mail->subject;
		model = mail->model;

		if (mail->date) {
			Q_strncpyz(dateBuf, _(mail->date), sizeof(dateBuf));
		} else {
			switch (type) {
			case TECHMAIL_PRE:
				CP_DateConvertLong(&tech->preResearchedDate, &date);
				Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
					date.year, Date_GetMonthName(date.month - 1), date.day);
				break;
			case TECHMAIL_RESEARCHED:
				CP_DateConvertLong(&tech->researchedDate, &date);
				Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
					date.year, Date_GetMonthName(date.month - 1), date.day);
				break;
			default:
				cgi->Com_Error(ERR_DROP, "UP_SetMailHeader: unhandled techMailType_t %i for date.", type);
			}
		}
		if (from != nullptr) {
			if (!mail->read) {
				mail->read = true;
				/* reread the unread mails in UP_GetUnreadMails */
				ccs.numUnreadMails = -1;
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
					cgi->Com_Error(ERR_DROP, "UP_SetMailHeader: unhandled techMailType_t %i for subject.", type);
				}
			}
		} else {
			cgi->UI_ResetData(TEXT_UFOPEDIA_MAILHEADER);
			return;
		}
	}
	from = cgi->CL_Translate(from);
	to = cgi->CL_Translate(to);
	subject = cgi->CL_Translate(subject);
	Com_sprintf(mailHeader, sizeof(mailHeader), _("FROM: %s\nTO: %s\nDATE: %s"), from, to, dateBuf);
	cgi->Cvar_Set("mn_mail_sender_head", "%s", model ? model : "");
	cgi->Cvar_Set("mn_mail_from", "%s", from);
	cgi->Cvar_Set("mn_mail_subject", "%s%s", subjectType, _(subject));
	cgi->Cvar_Set("mn_mail_to", "%s", to);
	cgi->Cvar_Set("mn_mail_date", "%s", dateBuf);
	cgi->UI_RegisterText(TEXT_UFOPEDIA_MAILHEADER, mailHeader);
}

/**
 * @brief Set the ammo model to display to selected ammo (only for a reloadable weapon)
 * @param tech technology_t pointer for the weapon's tech
 * @sa UP_Article
 */
static void UP_DrawAssociatedAmmo (const technology_t* tech)
{
	const objDef_t* od = INVSH_GetItemByID(tech->provides);
	/* If this is a weapon, we display the model of the associated ammunition in the lower right */
	if (od->numAmmos > 0) {
		const technology_t* associated = RS_GetTechForItem(od->ammos[0]);
		cgi->Cvar_Set("mn_upmodel_bottom", "%s", associated->mdl);
	}
}

/**
 * @brief Display only the TEXT_UFOPEDIA part for a given technology
 * @param[in] tech The technology_t pointer to print the UFOpaedia article for
 * @param[in] mail The mail parameters in case we produce a mail
 * @sa UP_Article
 */
static void UP_Article (technology_t* tech, eventMail_t* mail)
{
	UP_ChangeDisplay(UFOPEDIA_ARTICLE);

	if (tech) {
		if (tech->mdl)
			cgi->Cvar_Set("mn_upmodel_top", "%s", tech->mdl);
		else
			cgi->Cvar_Set("mn_upmodel_top", "");

		if (tech->image)
			cgi->Cvar_Set("mn_upimage_top", "%s", tech->image);
		else
			cgi->Cvar_Set("mn_upimage_top", "");

		cgi->Cvar_Set("mn_upmodel_bottom", "");

		if (tech->type == RS_WEAPON)
			UP_DrawAssociatedAmmo(tech);
		cgi->Cvar_Set("mn_uprequirement", "");
		cgi->Cvar_Set("mn_upmetadata", "");
	}

	cgi->UI_ResetData(TEXT_UFOPEDIA);
	cgi->UI_ResetData(TEXT_UFOPEDIA_REQUIREMENT);

	if (mail) {
		/* event mail */
		cgi->Cvar_SetValue("mn_uppreavailable", 0);
		cgi->Cvar_SetValue("mn_updisplay", UFOPEDIA_CHAPTERS);
		UP_SetMailHeader(nullptr, TECHMAIL_PRE, mail);
		cgi->UI_RegisterText(TEXT_UFOPEDIA, _(mail->body));
		/* This allows us to use the index button in the UFOpaedia,
		 * eventMails don't have any chapter to go back to. */
		upDisplay = UFOPEDIA_INDEX;
	} else if (tech) {
		currentChapter = tech->upChapter;
		upCurrentTech = tech;

		/* Reset itemdescription */
		cgi->UI_ExecuteConfunc("itemdesc_view 0 0;");
		if (RS_IsResearched_ptr(tech)) {
			cgi->Cvar_Set("mn_uptitle", _("UFOpaedia: %s (complete)"), _(tech->name));
			/* If researched -> display research text */
			cgi->UI_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->description)));
			if (tech->preDescription.numDescriptions > 0) {
				/* Display pre-research text and the buttons if a pre-research text is available. */
				if (mn_uppretext->integer) {
					cgi->UI_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->preDescription)));
					UP_SetMailHeader(tech, TECHMAIL_PRE, nullptr);
				} else {
					UP_SetMailHeader(tech, TECHMAIL_RESEARCHED, nullptr);
				}
				cgi->Cvar_SetValue("mn_uppreavailable", 1);
			} else {
				/* Do not display the pre-research-text button if none is available (no need to even bother clicking there). */
				cgi->Cvar_SetValue("mn_uppreavailable", 0);
				cgi->Cvar_SetValue("mn_updisplay", UFOPEDIA_CHAPTERS);
				UP_SetMailHeader(tech, TECHMAIL_RESEARCHED, nullptr);
			}

			switch (tech->type) {
			case RS_ARMOUR:
			case RS_WEAPON:
				for (int i = 0; i < cgi->csi->numODs; i++) {
					const objDef_t* od = INVSH_GetItemByIDX(i);
					if (Q_streq(tech->provides, od->id)) {
						cgi->INV_ItemDescription(od);
						UP_DisplayTechTree(tech);
						cgi->Cvar_Set("mn_upmetadata", "1");
						break;
					}
				}
				break;
			case RS_TECH:
				UP_DisplayTechTree(tech);
				break;
			case RS_CRAFT:
				UP_AircraftDescription(tech);
				break;
			case RS_CRAFTITEM:
				UP_AircraftItemDescription(INVSH_GetItemByID(tech->provides));
				break;
			case RS_BUILDING:
				UP_BuildingDescription(tech);
				break;
			case RS_UGV:
				UP_UGVDescription(cgi->Com_GetUGVByIDSilent(tech->provides));
				break;
			default:
				break;
			}
		/* see also UP_TechGetsDisplayed */
		} else if (RS_Collected_(tech) || (tech->statusResearchable && tech->preDescription.numDescriptions > 0)) {
			/* This tech has something collected or has a research proposal. (i.e. pre-research text) */
			cgi->Cvar_Set("mn_uptitle", _("UFOpaedia: %s"), _(tech->name));
			/* Not researched but some items collected -> display pre-research text if available. */
			if (tech->preDescription.numDescriptions > 0) {
				cgi->UI_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->preDescription)));
				UP_SetMailHeader(tech, TECHMAIL_PRE, nullptr);
			} else {
				cgi->UI_RegisterText(TEXT_UFOPEDIA, _("No pre-research description available."));
			}
		} else {
			cgi->Cvar_Set("mn_uptitle", _("UFOpaedia: %s"), _(tech->name));
			cgi->UI_ResetData(TEXT_UFOPEDIA);
		}
	} else {
		cgi->Com_Error(ERR_DROP, "UP_Article: No mail or tech given");
	}
}

/**
 * @sa CL_EventAddMail_f
 */
void UP_OpenEventMail (const char* eventMailID)
{
	eventMail_t* mail;
	mail = CL_GetEventMail(eventMailID);
	if (!mail)
		return;

	cgi->UI_PushWindow("mail");
	UP_Article(nullptr, mail);
}

/**
 * @brief Opens the mail view from everywhere with the entry given through name
 * @param techID mail entry id (technology script id)
 * @sa UP_FindEntry_f
 */
static void UP_OpenMailWith (const char* techID)
{
	if (!techID)
		return;

	cgi->UI_PushWindow("mail");
	cgi->Cbuf_AddText("ufopedia %s\n", techID);
}

/**
 * @brief Opens the UFOpaedia from everywhere with the entry given through name
 * @param techID UFOpaedia entry id (technology script id)
 * @sa UP_FindEntry_f
 */
void UP_OpenWith (const char* techID)
{
	if (!techID)
		return;

	cgi->UI_PushWindow("ufopedia");
	cgi->Cbuf_AddText("ufopedia %s\nupdate_ufopedia_layout\n", techID);
}

/**
 * @brief Opens the UFOpaedia with the entry given through name, not deleting copies
 * @param techID UFOpaedia entry id (technology script id)
 * @sa UP_FindEntry_f
 */
void UP_OpenCopyWith (const char* techID)
{
	cgi->Cmd_ExecuteString("ui_push ufopedia");
	cgi->Cbuf_AddText("ufopedia %s\n", techID);
}


/**
 * @brief Search and open the UFOpaedia with given id
 */
static void UP_FindEntry_f (void)
{
	const char* id;
	technology_t* tech;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <id>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* what are we searching for? */
	id = cgi->Cmd_Argv(1);

	/* maybe we get a call like 'ufopedia ""' */
	if (id[0] == '\0') {
		Com_Printf("UP_FindEntry_f: No UFOpaedia entry given as parameter\n");
		return;
	}

	tech = RS_GetTechByID(id);
	if (!tech) {
		Com_DPrintf(DEBUG_CLIENT, "UP_FindEntry_f: No UFOpaedia entry found for %s\n", id);
		return;
	}

	if (tech->redirect)
		tech = tech->redirect;

	UP_Article(tech, nullptr);
}

/**
 * @brief Generate a list of options for all allowed articles of a chapter
 * @param[in] parentChapter requested chapter
 * @return The first option of the list, else nullptr if no articles
 */
static uiNode_t* UP_GenerateArticlesSummary (pediaChapter_t* parentChapter)
{
	technology_t* tech = parentChapter->first;
	uiNode_t* first = nullptr;

	while (tech) {
		if (UP_TechGetsDisplayed(tech)) {
			const char* id = va("@%i", tech->idx);
			cgi->UI_AddOption(&first, id, va("_%s", tech->name), id);
		}
		tech = tech->upNext;
	}

	cgi->UI_SortOptions(&first);

	return first;
}

/**
 * @brief Generate a tree of option for all allowed chapters and articles
 * @note it update OPTION_UFOPEDIA
 */
static void UP_GenerateSummary (void)
{
	uiNode_t* chapters = nullptr;
	int num = 0;

	numChaptersDisplayList = 0;

	for (int i = 0; i < ccs.numChapters; i++) {
		/* hide chapters without name */
		pediaChapter_t* chapter = &ccs.upChapters[i];
		if (chapter->name == nullptr)
			continue;

		/* Check if there are any researched or collected items in this chapter ... */
		bool researchedEntries = false;
		upCurrentTech = chapter->first;
		while (upCurrentTech) {
			if (UP_TechGetsDisplayed(upCurrentTech)) {
				researchedEntries = true;
				break;
			}
			upCurrentTech = upCurrentTech->upNext;
		}

		/* .. and if so add them to the displaylist of chapters. */
		if (researchedEntries) {
			uiNode_t* chapterOption;
			if (numChaptersDisplayList >= sizeof(upChaptersDisplayList))
				cgi->Com_Error(ERR_DROP, "MAX_PEDIACHAPTERS hit");
			upChaptersDisplayList[numChaptersDisplayList++] = chapter;

			/* chapter section*/
			chapterOption = cgi->UI_AddOption(&chapters, chapter->id, va("_%s", chapter->name), va("%i", num));
			/** @todo use a confunc */
			OPTIONEXTRADATA(chapterOption).icon = cgi->UI_GetSpriteByName(va("icons/ufopedia_%s", chapter->id));
			chapterOption->firstChild = UP_GenerateArticlesSummary(chapter);

			num++;
		}
	}

	cgi->UI_RegisterOption(OPTION_UFOPEDIA, chapters);
	cgi->Cvar_Set("mn_uptitle", _("UFOpaedia"));
}

/**
 * @brief Displays the chapters in the UFOpaedia
 */
static void UP_Content_f (void)
{
	UP_GenerateSummary();
	UP_ChangeDisplay(UFOPEDIA_CHAPTERS);
}

/**
 * @brief Callback when we click on the ufopedia summary
 * @note when we click on a chapter, param=chapterId,
 * when we click on an article, param='@'+techIdx
 */
static void UP_Click_f (void)
{
	if (cgi->Cmd_Argc() < 2)
		return;

	/* article index starts with a @ */
	if (cgi->Cmd_Argv(1)[0] == '@') {
		const int techId = atoi(cgi->Cmd_Argv(1) + 1);
		technology_t* tech;
		assert(techId >= 0);
		assert(techId < ccs.numTechnologies);
		tech = &ccs.technologies[techId];
		if (tech)
			UP_Article(tech, nullptr);
		return;
	} else {
		/* Reset itemdescription */
		cgi->UI_ExecuteConfunc("itemdesc_view 0 0;");
	}

	/* it clean up the display */
	UP_ChangeDisplay(UFOPEDIA_CHAPTERS);
}

/**
 * @todo The "num" value and the link-index will most probably not match.
 */
static void UP_TechTreeClick_f (void)
{
	if (cgi->Cmd_Argc() < 2)
		return;

	int num = atoi(cgi->Cmd_Argv(1));

	if (!upCurrentTech)
		return;

	const requirements_t* required_AND = &upCurrentTech->requireAND;
	if (num < 0 || num >= required_AND->numLinks)
		return;

	/* skip every tech which have not been displayed in techtree */
	for (int i = 0; i <= num; i++) {
		const requirement_t* r = &required_AND->links[i];
		if (r->type != RS_LINK_TECH && r->type != RS_LINK_TECH_NOT)
			num++;
	}

	const technology_t* techRequired = required_AND->links[num].link.tech;
	if (!techRequired)
		cgi->Com_Error(ERR_DROP, "Could not find the tech for '%s'", required_AND->links[num].id);

	/* maybe there is no UFOpaedia chapter assigned - this tech should not be opened at all */
	if (!techRequired->upChapter)
		return;

	UP_OpenWith(techRequired->id);
}

/**
 * @brief Redraw the UFOpaedia article
 */
static void UP_Update_f (void)
{
	if (upCurrentTech)
		UP_Article(upCurrentTech, nullptr);
}

/**
 * @brief Mailclient click function callback
 * @sa UP_OpenMail_f
 */
static void UP_MailClientClick_f (void)
{
	if (cgi->Cmd_Argc() < 2)
		return;

	int cnt = -1;
	const int num = atoi(cgi->Cmd_Argv(1));

	uiMessageListNodeMessage_t* m = cgi->UI_MessageGetStack();
	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (!m->pedia->mail[TECHMAIL_PRE].from)
				break;
			cnt++;
			if (cnt == num) {
				cgi->Cvar_SetValue("mn_uppretext", 1);
				UP_OpenMailWith(m->pedia->id);
				return;
			}
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			cnt++;
			if (cnt == num) {
				cgi->Cvar_SetValue("mn_uppretext", 0);
				UP_OpenMailWith(m->pedia->id);
				return;
			}
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from || m->pedia->mail[TECHMAIL_RESEARCHED].from) {
				cnt++;
				if (cnt >= num) {
					UP_OpenMailWith(m->pedia->id);
					return;
				}
			}
			break;
		case MSG_EVENT:
			cnt++;
			if (cnt >= num) {
				UP_OpenEventMail(m->eventMail->id);
				return;
			}
			break;
		default:
			break;
		}
		m = m->next;
	}
	ccs.numUnreadMails = -1;
	UP_GetUnreadMails();
}

/**
 * @brief Change UFOpaedia article when clicking on the name of associated ammo or weapon
 */
static void UP_ResearchedLinkClick_f (void)
{
	const objDef_t* od;

	if (!upCurrentTech) /* if called from console */
		return;

	od = INVSH_GetItemByID(upCurrentTech->provides);
	assert(od);

	if (od->isAmmo()) {
		const technology_t* t = RS_GetTechForItem(od->weapons[0]);
		if (UP_TechGetsDisplayed(t))
			UP_OpenWith(t->id);
	} else if (od->weapon && od->isReloadable()) {
		const technology_t* t = RS_GetTechForItem(od->ammos[0]);
		if (UP_TechGetsDisplayed(t))
			UP_OpenWith(t->id);
	}
}

/**
 * @brief Start the mailclient
 * @sa UP_MailClientClick_f
 * @sa CP_GetUnreadMails
 * @sa CL_EventAddMail_f
 * @sa MS_AddNewMessage
 */
static void UP_OpenMail_f (void)
{
	cgi->UI_ExecuteConfunc("clear_mails");
	int idx = 0;
	for (const uiMessageListNodeMessage_t* m = cgi->UI_MessageGetStack(); m; m = m->next) {
		dateLong_t date;
		char headline[256] = "";
		char dateBuf[64] = "";
		const char* icon;
		bool read;
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL: {
			const techMail_t& mail = m->pedia->mail[TECHMAIL_PRE];
			if (!mail.from)
				continue;
			CP_DateConvertLong(&m->pedia->preResearchedDate, &date);
			Com_sprintf(headline, sizeof(headline), _("Proposal: %s"), _(m->pedia->mail[TECHMAIL_PRE].subject));
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);
			icon = mail.icon;
			read = mail.read;
			break;
		}
		case MSG_RESEARCH_FINISHED: {
			const techMail_t& mail = m->pedia->mail[TECHMAIL_RESEARCHED];
			if (!mail.from)
				continue;
			CP_DateConvertLong(&m->pedia->researchedDate, &date);
			Com_sprintf(headline, sizeof(headline), _("Re: %s"), _(m->pedia->mail[TECHMAIL_PRE].subject));
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);
			icon = mail.icon;
			read = mail.read;
			break;
		}
		case MSG_NEWS: {
			const techMail_t* mail = &m->pedia->mail[TECHMAIL_PRE];
			if (mail->from) {
				CP_DateConvertLong(&m->pedia->preResearchedDate, &date);
			} else {
				CP_DateConvertLong(&m->pedia->researchedDate, &date);
				mail = &m->pedia->mail[TECHMAIL_RESEARCHED];
			}
			if (!mail->from)
				continue;
			Com_sprintf(headline, sizeof(headline), "%s", _(mail->subject));
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"), date.year, Date_GetMonthName(date.month - 1), date.day);
			icon = mail->icon;
			read = mail->read;
			break;
		}
		case MSG_EVENT: {
			assert(m->eventMail);
			const eventMail_t& mail = *m->eventMail;
			if (!mail.from)
				continue;
			Com_sprintf(headline, sizeof(headline), "%s", _(mail.subject));
			Com_sprintf(dateBuf, sizeof(dateBuf), "%s", mail.date);
			icon = mail.icon;
			read = mail.read;
			break;
		}
		default:
			continue;
		}
		cgi->UI_ExecuteConfunc("add_mail %i \"%s\" \"%s\" %i \"%s\"", idx++, headline, icon, read, dateBuf);
	}
}

/**
 * @brief Marks all mails read in mailclient
 */
static void UP_SetAllMailsRead_f (void)
{
	const uiMessageListNodeMessage_t* m = cgi->UI_MessageGetStack();

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_PRE].read = true;
			break;
		case MSG_RESEARCH_FINISHED:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_RESEARCHED].read = true;
			break;
		case MSG_NEWS:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_PRE].read = true;
			m->pedia->mail[TECHMAIL_RESEARCHED].read = true;
			break;
		case MSG_EVENT:
			assert(m->eventMail);
			m->eventMail->read = true;
			break;
		default:
			break;
		}
		m = m->next;
	}

	ccs.numUnreadMails = 0;
	cgi->Cvar_Set("mn_upunreadmail", "%i", ccs.numUnreadMails);
	UP_OpenMail_f();
}

static const cmdList_t ufopediaCmds[] = {
	{"mn_upcontent", UP_Content_f, "Shows the UFOpaedia chapters"},
	{"mn_upupdate", UP_Update_f, "Redraw the current UFOpaedia article"},
	{"ufopedia", UP_FindEntry_f, "Open the UFOpaedia with the given article"},
	{"ufopedia_click", UP_Click_f, nullptr},
	{"mailclient_click", UP_MailClientClick_f, nullptr},
	{"mn_mail_readall", UP_SetAllMailsRead_f, "Mark all mails read"},
	{"ufopedia_openmail", UP_OpenMail_f, "Start the mailclient"},
	{"techtree_click", UP_TechTreeClick_f, nullptr},
	{"mn_upgotoresearchedlink", UP_ResearchedLinkClick_f, nullptr},
	{nullptr, nullptr, nullptr}
};
/**
 * @sa cgi->UI_InitStartup
 */
void UP_InitStartup (void)
{
	/* add commands and cvars */
	cgi->Cmd_TableAddList(ufopediaCmds);

	mn_uppretext = cgi->Cvar_Get("mn_uppretext", "0", 0, "Show the pre-research text in the UFOpaedia");
	mn_uppreavailable = cgi->Cvar_Get("mn_uppreavailable", "0", 0, "True if there is a pre-research text available");
	cgi->Cvar_Set("mn_uprequirement", "");
	cgi->Cvar_Set("mn_upmetadata", "");
}

/**
 * @sa UI_InitStartup
 */
void UP_Shutdown (void)
{
	/* add commands and cvars */
	cgi->Cmd_TableRemoveList(ufopediaCmds);

	cgi->Cvar_Delete("mn_uppretext");
	cgi->Cvar_Delete("mn_uppreavailable");
	cgi->Cvar_Delete("mn_uprequirement");
	cgi->Cvar_Delete("mn_upmetadata");
}

/**
 * @brief Parse the UFOpaedia chapters from scripts
 * @param[in] name Chapter ID
 * @param[in] text Text for chapter ID
 * @sa CL_ParseFirstScript
 */
void UP_ParseChapter (const char* name, const char** text)
{
	const char* errhead = "UP_ParseChapter: unexpected end of file (names ";
	const char* token;

	if (ccs.numChapters >= MAX_PEDIACHAPTERS) {
		cgi->Com_Error(ERR_DROP, "UP_ParseChapter: chapter def \"%s\": Too many chapter defs.\n", name);
	}

	pediaChapter_t chapter;
	OBJZERO(chapter);
	chapter.id = cgi->PoolStrDup(name, cp_campaignPool, 0);
	chapter.idx = ccs.numChapters;	/* set self-link */

	/* get begin block */
	token = Com_Parse(text);
	if (!*text || *token !='{') {
		cgi->Com_Error(ERR_DROP, "UP_ParseChapter: chapter def \"%s\": '{' token expected.\n", name);
	}

	do {
		token = Com_Parse(text);
		if (!*text)
			cgi->Com_Error(ERR_DROP, "UP_ParseChapter: end of file not expected \"%s\": '{' token expected.\n", name);
		if (token[0] == '}')
			break;

		if (Q_streq(token, "name")) {
			/* get the name */
			token = cgi->Com_EParse(text, errhead, name);
			if (!*text || *token == '}') {
				cgi->Com_Error(ERR_DROP, "UP_ParseChapter: chapter def \"%s\": Name token expected.\n", name);
			}
			if (*token == '_')
				token++;
			chapter.name = cgi->PoolStrDup(token, cp_campaignPool, 0);
		} else {
			cgi->Com_Error(ERR_DROP, "UP_ParseChapter: chapter def \"%s\": token \"%s\" not expected.\n", name, token);
		}
	} while (*text);

	/* add chapter to the game */
	ccs.upChapters[ccs.numChapters] = chapter;
	ccs.numChapters++;
}
