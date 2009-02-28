/**
 * @file cl_ufopedia.c
 * @brief UFOpaedia script interpreter.
 * @todo Split the mail code into cl_mailclient.c/h
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

#include "../client.h"
#include "../cl_game.h"
#include "../cl_menu.h"
#include "../menu/m_nodes.h"
#include "../../shared/parse.h"
#include "cl_campaign.h"
#include "cl_mapfightequip.h"
#include "cp_time.h"

static cvar_t *mn_uppretext = NULL;
static cvar_t *mn_uppreavailable = NULL;

static pediaChapter_t *upChapters_displaylist[MAX_PEDIACHAPTERS];
static int numChapters_displaylist;

static technology_t	*upCurrentTech;
static pediaChapter_t *currentChapter = NULL;

#define MAX_UPTEXT 4096
static char upText[MAX_UPTEXT];

/* this buffer is for stuff like aircraft or building info */
static char upBuffer[MAX_UPTEXT];

/* this variable contains the current firemode number for ammos and weapons */
static int upFireMode;

/* this variable contains the current weapon to display (for an ammo) */
static int upResearchedLink;

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
 * @note This does not check for different display modes (only pre-research text, what stats, etc...). The content is mostly checked in UP_Article
 * @return qtrue if the tech gets displayed at all, otherwise qfalse.
 * @sa UP_Article
 */
static qboolean UP_TechGetsDisplayed (const technology_t *tech)
{
	return (RS_IsResearched_ptr(tech)	/* Is already researched OR ... */
	 || RS_Collected_(tech)	/* ... has collected items OR ... */
	 || (tech->statusResearchable && tech->pre_description.numDescriptions > 0))
	 && tech->type != RS_LOGIC	/* Is no logic block. */
	 && !tech->redirect;		/* Another technology will get displayed instead of this one. */
}

/**
 * @brief Modify the global display var
 * @sa UP_SetMailHeader
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
	 * only fetch this once after UFOpaedia menu was on the stack (was the
	 * current menu)
	 */
	if (!ufopedia || !ufopediaMail) {
		ufopedia = MN_GetNodeByPath("ufopedia.ufopedia");
		ufopediaMail = MN_GetNodeByPath("ufopedia.mailclient");
	}

	/* maybe we call this function and the UFOpaedia is not on the menu stack */
	if (ufopedia && ufopediaMail) {
		ufopedia->u.text.textScroll = ufopediaMail->u.text.textScroll = 0;
	}

	/* make sure, that we leave the mail header space */
	MN_ResetData(TEXT_UFOPEDIA_MAILHEADER);
	MN_ResetData(TEXT_UFOPEDIA_MAIL);
	MN_ResetData(TEXT_LIST);
	MN_ResetData(TEXT_STANDARD);
	MN_ResetData(TEXT_UFOPEDIA);

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		MN_ExecuteConfunc("mn_upfbig");
		currentChapter = NULL;
		upCurrentTech = NULL;
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		break;
	case UFOPEDIA_INDEX:
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		/* no break here */
	case UFOPEDIA_ARTICLE:
		MN_ExecuteConfunc("mn_upfsmall");
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
static const char* CL_WeaponSkillToName (int weaponSkill)
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
 * @brief Translate a aircraft stat int to a translated string
 * @sa aircraftParams_t
 * @sa CL_AircraftMenuStatsValues
 */
static const char* CL_AircraftStatToName (int stat)
{
	switch (stat) {
	case AIR_STATS_SPEED:
		return _("Cruising speed");
	case AIR_STATS_MAXSPEED:
		return _("Maximum speed");
	case AIR_STATS_SHIELD:
		return _("Armour");
	case AIR_STATS_ECM:
		return _("ECM");
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
 * @brief Translate a aircraft size int to a translated string
 * @sa aircraftSize_t
 */
static const char* CL_AircraftSizeToName (int aircraftSize)
{
	switch (aircraftSize) {
	case AIRCRAFT_SMALL:
		return _("Small");
	case AIRCRAFT_LARGE:
		return _("Large");
	default:
		return _("Unknown aircraft size");
	}
}

/**
 * @brief Diplays the tech tree dependencies in the UFOpaedia
 * @sa UP_DrawEntry
 * @todo Add support for "require_AND"
 * @todo re-iterate trough logic blocks (i.e. append the tech-names it references recursively)
 */
static void UP_DisplayTechTree (const technology_t* t)
{
	int i;
	static char up_techtree[1024];
	const requirements_t *required;

	required = &t->require_AND;
	up_techtree[0] = '\0';

	if (required->numLinks <= 0)
		Q_strcat(up_techtree, _("No requirements"), sizeof(up_techtree));
	else
		for (i = 0; i < required->numLinks; i++) {
			const requirement_t *req = &required->links[i];
			if (req->type == RS_LINK_TECH) {
				/** @todo support for RS_LINK_TECH_BEFORE and RS_LINK_TECH_XOR? */
				technology_t *techRequired = req->link;
				if (!techRequired)
					Sys_Error("Could not find the tech for '%s'\n", req->id);

				/** Only display tech if it is ok to do so.
				 * @todo If it is one (a logic tech) we may want to re-iterate from its requirements? */
				if (UP_TechGetsDisplayed(techRequired)) {
					Q_strcat(up_techtree, _(techRequired->name), sizeof(up_techtree));
				} else {
					/** @todo
					if (techRequired->type == RS_LOGIC) {
						Append strings from techRequired->require_AND etc...
						Make UP_DisplayTechTree a recursive function?
					}
					*/
					continue;
				}
				Q_strcat(up_techtree, "\n", sizeof(up_techtree));
			}
		}
	/* and now set the buffer to the right mn.menuText */
	MN_RegisterText(TEXT_LIST, up_techtree);
}

/**
 * @brief Prints the (UFOpaedia and other) description for items (weapons, armour, ...)
 * @param[in] od The object definition of the item
 * @sa UP_DrawEntry
 * @sa BS_BuySelect_f
 * @sa BS_BuyType_f
 * @sa BS_BuyItem_f
 * @sa BS_SellItem_f
 * @sa MN_Drag
 * Not only called from UFOpaedia but also from other places to display
 * weapon and ammo stats
 * @todo Do we need to add checks for @c od->isDummy here somewhere?
 */
void UP_ItemDescription (const objDef_t *od)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	const objDef_t *odAmmo;
	int i;
	int up_numresearchedlink = 0;
	int up_weapon_id = NONE;

	/* reset everything */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

	if (!od)	/* If nothing selected return */
		return;

	/* select item */
	Cvar_Set("mn_itemname", _(od->name));
	Cvar_Set("mn_item", od->id);

#ifdef DEBUG
	if (!od->tech) {
		Com_sprintf(itemText, sizeof(itemText), "Error - no tech assigned\n");
		MN_RegisterText(TEXT_STANDARD, itemText);
		odAmmo = NULL;
	} else
#endif

	/* Write attached ammo or weapon even if item is not researched */
	if (!Q_strcmp(od->type, "ammo")) {
		/* We store the current technology in upCurrentTech (needed for changing firemodes while in equip menu) */
		upCurrentTech = od->tech;

		/* We display the pre/next buttons for changing weapon only if there are at least 2 researched weapons */
		/* up_numresearchedlink contains the number of researched weapons useable with this ammo */
		for (i = 0; i < od->numWeapons; i++) {
			if (RS_IsResearched_ptr(od->weapons[i]->tech))
				up_numresearchedlink++;
		}
		if (up_numresearchedlink > 1)
			Cvar_Set("mn_changeweapon", "1"); /* use strings here - no int */

		/* Only display weapons if at least one has been researched */
		if (up_numresearchedlink > 0) {
			/* We check that upResearchedLink exists for this ammo (in case we switched from an ammo or a weapon with higher value)*/
			if (upResearchedLink >= od->numWeapons) {
				for (upResearchedLink = 0; upResearchedLink < od->numWeapons; upResearchedLink++) {
					if (RS_IsResearched_ptr(od->weapons[upResearchedLink]->tech))
						break;
					upResearchedLink++;
				}
			}

			up_weapon_id = upResearchedLink;

			/* Display the name of the associated weapon */
			Cvar_Set("mn_displayweapon", "1"); /* use strings here - no int */
			Cvar_Set("mn_researchedlinkname", _(od->weapons[upResearchedLink]->name));
			Cvar_Set("mn_upresearchedlinknametooltip", va(_("Go to '%s' UFOpaedia entry"), _(od->weapons[upResearchedLink]->name)));

			/* Needed for writing stats below */
			odAmmo = od;
		} else {
			upResearchedLink = 0;
			odAmmo = NULL;
		}
	} else if (od->weapon && !od->reload) {
		/* We store the current technology in upCurrentTech (needed for changing firemodes while in equip menu) */
		upCurrentTech = od->tech;

		/* Security reset */
		upResearchedLink = 0;

		/* ammo and weapon are the same item: must use up_weapon_id 0 */
		up_weapon_id = 0;

		/* Needed for writing stats below */
		odAmmo = od;
	} else if (od->weapon) {
		/* We have a weapon that uses ammos */
		/* We store the current technology in upCurrentTech (needed for changing firemodes while in equip menu) */
		upCurrentTech = od->tech;

		/* We display the pre/next buttons for changing ammo only if there are at least 2 researched ammo */
		/* up_numresearchedlink contains the number of researched ammos useable with this weapon */
		for (i = 0; i < od->numAmmos; i++) {
			if (RS_IsResearched_ptr(od->ammos[i]->tech))
				up_numresearchedlink++;
		}
		if (up_numresearchedlink > 1)
			Cvar_Set("mn_changeweapon", "2"); /* use strings here - no int */

		/* Only display ammos if at least one has been researched */
		if (up_numresearchedlink > 0) {
			/* We check that upResearchedLink exists for this weapon (in case we switched from an ammo or a weapon with higher value)*/
			if (upResearchedLink >= od->numAmmos) {
				for (upResearchedLink = 0; upResearchedLink < od->numAmmos; upResearchedLink++) {
					if (RS_IsResearched_ptr(od->ammos[upResearchedLink]->tech))
						break;
					upResearchedLink++;
				}
			}

			/* Everything that follows depends only of the ammunition, so we change od to it */
			odAmmo = od->ammos[upResearchedLink];
			for (i = 0; i < odAmmo->numWeapons; i++) {
				if (odAmmo->weapons[i] == od)	/** od->idx == item */
					up_weapon_id = i;
			}

			Cvar_Set("mn_displayweapon", "2"); /* use strings here - no int */
			Cvar_Set("mn_researchedlinkname", _(odAmmo->name));
			Cvar_Set("mn_upresearchedlinknametooltip", va(_("Go to '%s' UFOpaedia entry"), _(odAmmo->name)));
		} else {
			/* Reset upResearchedLink to make sure we don't hit an assert while drawing ammo */
			upResearchedLink = 0;
			odAmmo = NULL;
		}
	} else
		odAmmo = NULL;

	/* set description text if item as been researched or one of its ammo/weapon has been researched */
	if (RS_IsResearched_ptr(od->tech) || up_numresearchedlink > 0) {
		*itemText = '\0';
		if (!Q_strcmp(od->type, "armour")) {
			if (Q_strcmp(MN_GetActiveMenuName(), "equipment")) {
				/* next two lines are not merge in one to avoid to have several entries in .po files (first line will be used again) */
				Q_strcat(itemText, va(_("Size:\t%i\n"), od->size), sizeof(itemText));
				Q_strcat(itemText, "\n", sizeof(itemText));
			}
			Q_strcat(itemText, _("Type:\tProtection:\n"), sizeof(itemText));
			for (i = 0; i < csi.numDTs; i++) {
				if (!csi.dts[i].showInMenu)
					continue;
				Q_strcat(itemText, va(_("%s\t%i\n"), _(csi.dts[i].id), od->ratings[i]), sizeof(itemText));
			}
		} else if (!Q_strcmp(od->type, "ammo")) {
			if (Q_strcmp(MN_GetActiveMenuName(), "equipment"))
				Q_strcat(itemText, va(_("Size:\t%i\n"), od->size), sizeof(itemText));
			/* more will be written below */
		} else if (od->weapon && (od->reload || od->thrown)) {
			Com_sprintf(itemText, sizeof(itemText), _("%s weapon with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			if (Q_strcmp(MN_GetActiveMenuName(), "equipment"))
				Q_strcat(itemText, va(_("Size:\t%i\n"), od->size), sizeof(itemText));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
			/* more will be written below */
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			if (Q_strcmp(MN_GetActiveMenuName(), "equipment"))
				Q_strcat(itemText, va(_("Size:\t%i\n"), od->size), sizeof(itemText));
			/* more will be written below */
		} else if (od->craftitem.type <= AC_ITEM_BASE_LASER) {
			/* This is a weapon for base, can be displayed in equip menu */
			Com_sprintf(itemText, sizeof(itemText), _("Weapon for base defence system\n"));
		} else if (od->craftitem.type != AIR_STATS_MAX) {
			/* This is an item for aircraft or ammos for bases -- do nothing */
		} else {
			/* just an item */
			/* only primary definition */
			/** @todo We use the default firemodes here. We might need some change the "fd[0]" below to FIRESH_FiredefsIDXForWeapon(od,weapon_idx) on future changes. */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment with\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			if (Q_strcmp(MN_GetActiveMenuName(), "equipment"))
				Q_strcat(itemText, va(_("Size:\t%i\n"), od->size), sizeof(itemText));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0][0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0][0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0][0].range / UNIT_SIZE), sizeof(itemText));
		}

		if (odAmmo) {
			/* This contains everything common for weapons and ammos */

			/* We check if the wanted firemode to display exists. */
			if (upFireMode > odAmmo->numFiredefs[up_weapon_id] - 1)
				upFireMode = 0;
			if (upFireMode < 0)
				upFireMode = odAmmo->numFiredefs[up_weapon_id] - 1;

			/* We always display the name of the firemode for an ammo */
			Cvar_Set("mn_displayfiremode", "1"); /* use strings here - no int */
			Cvar_Set("mn_firemodename", _(odAmmo->fd[up_weapon_id][upFireMode].name));
			/* We display the pre/next buttons for changing firemode only if there are more than one */
			if (odAmmo->numFiredefs[up_weapon_id] > 1)
				Cvar_Set("mn_changefiremode", "1"); /* use strings here - no int */

			/* We display the characteristics of this firemode */
			Q_strcat(itemText, va(_("Skill:\t%s\n"), CL_WeaponSkillToName(odAmmo->fd[up_weapon_id][upFireMode].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText, va(_("Damage:\t%i\n"),
				(int) ((odAmmo->fd[up_weapon_id][upFireMode].damage[0] + odAmmo->fd[up_weapon_id][upFireMode].spldmg[0]) * odAmmo->fd[up_weapon_id][upFireMode].shots)),
						sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), odAmmo->fd[up_weapon_id][upFireMode].time),  sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), odAmmo->fd[up_weapon_id][upFireMode].range / UNIT_SIZE),  sizeof(itemText));
			Q_strcat(itemText, va(_("Spreads:\t%g\n"),
				(odAmmo->fd[up_weapon_id][upFireMode].spread[0] + odAmmo->fd[up_weapon_id][upFireMode].spread[1]) / 2),  sizeof(itemText));
		}

		MN_RegisterText(TEXT_STANDARD, itemText);
	} else { /* includes if (RS_Collected_(tech)) AFAIK*/
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
		MN_RegisterText(TEXT_STANDARD, itemText);
	}
}

/**
 * @brief Prints the UFOpaedia description for armour
 * @sa UP_DrawEntry
 */
static void UP_ArmourDescription (const technology_t* t)
{
	objDef_t *od;
	int i;

	/* select item */
	od = INVSH_GetItemByID(t->provides);

#ifdef DEBUG
	if (od == NULL)
		Com_sprintf(upBuffer, sizeof(upBuffer), "Could not find armour definition");
	else if (Q_strcmp(od->type, "armour"))
		Com_sprintf(upBuffer, sizeof(upBuffer), "Item %s is no armour but %s", od->id, od->type);
	else
#endif
	{
		if (!od)
			Sys_Error("Could not find armour definition '%s' which should have been provided by '%s'\n", t->provides, t->id);
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upimage_top", t->image);
		upBuffer[0] = '\0';
		Q_strcat(upBuffer, va(_("Size:\t%i\n"), od->size), sizeof(upBuffer));
		Q_strcat(upBuffer, "\n", sizeof(upBuffer));
		for (i = 0; i < csi.numDTs; i++) {
			if (!csi.dts[i].showInMenu)
				continue;
			Q_strcat(upBuffer, va(_("%s:\tProtection: %i\n"), _(csi.dts[i].id), od->ratings[i]), sizeof(upBuffer));
		}
	}
	MN_RegisterText(TEXT_STANDARD, upBuffer);
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the UFOpaedia description for technologies
 * @sa UP_DrawEntry
 */
static void UP_TechDescription (const technology_t* t)
{
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the UFOpaedia description for buildings
 * @sa UP_DrawEntry
 */
static void UP_BuildingDescription (const technology_t* t)
{
	const building_t* b = B_GetBuildingTemplate(t->provides);

	if (!b) {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Error - could not find building"));
	} else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Needs:\t%s\n"), b->dependsBuilding ? _(b->dependsBuilding->name) : _("None"));
		Q_strcat(upBuffer, va(ngettext("Construction time:\t%i day\n", "Construction time:\t%i days\n", b->buildTime), b->buildTime), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Cost:\t%i c\n"), b->fixCosts), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Running costs:\t%i c\n"), b->varCosts), sizeof(upBuffer));
	}
	MN_RegisterText(TEXT_STANDARD, upBuffer);
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the (UFOpaedia and other) description for aircraft items
 * @param item The object definition of the item
 * @sa UP_DrawEntry
 * Not only called from UFOpaedia but also from other places to display
 * @todo Don't display things like speed for base defence items - a missile
 * facility isn't getting slower or faster due a special weapon or ammunition
 */
void UP_AircraftItemDescription (const objDef_t *item)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	int i;

	/* Set menu text node content to null. */
	MN_ResetData(TEXT_STANDARD);

	/* no valid item id given */
	if (!item) {
		/* Reset all used menu variables/nodes. TEXT_STANDARD is already reset. */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
		Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
		Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
		Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
		Cvar_Set("mn_researchedlinkname", "");
		Cvar_Set("mn_upresearchedlinknametooltip", "");
		return;
	}

	/* select item */
	assert(item->craftitem.type >= 0);
	assert(item->tech);
	Cvar_Set("mn_itemname", _(item->tech->name));
	/** @todo Is this actually _used_ in the ufopedia?
	 * No, but in buy and production - and they are using these functions, too, no? (mattn) */
	Cvar_Set("mn_item", item->id);
	Cvar_Set("mn_upmodel_top", item->tech->mdl);
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

#ifdef DEBUG
	if (!item->tech) {
		Com_sprintf(itemText, sizeof(itemText), "Error - no tech assigned\n");
		MN_RegisterText(TEXT_STANDARD, itemText);
	} else
#endif
	/* set description text */
	if (RS_IsResearched_ptr(item->tech)) {
		*itemText = '\0';

		if (item->craftitem.type == AC_ITEM_WEAPON)
			Q_strcat(itemText, va(_("Weight:\t%s\n"), AII_WeightToName(AII_GetItemWeightBySize(item))), sizeof(itemText));
		else if (item->craftitem.type == AC_ITEM_AMMO) {
			/* We display the characteristics of this ammo */
			if (!item->craftitem.unlimitedAmmo)
				Q_strcat(itemText, va(_("Ammo:\t%i\n"), item->ammo), sizeof(itemText));
			if (!equal(item->craftitem.weaponDamage, 0))
				Q_strcat(itemText, va(_("Damage:\t%i\n"), (int) item->craftitem.weaponDamage), sizeof(itemText));
			Q_strcat(itemText, va(_("Reloading time:\t%i\n"),  (int) item->craftitem.weaponDelay), sizeof(itemText));
		}
		/* We write the range of the weapon */
		if (!equal(item->craftitem.stats[AIR_STATS_WRANGE], 0))
			Q_strcat(itemText, va("%s:\t%i\n", CL_AircraftStatToName(AIR_STATS_WRANGE),
				CL_AircraftMenuStatsValues(item->craftitem.stats[AIR_STATS_WRANGE], AIR_STATS_WRANGE)), sizeof(itemText));

		/* we scan all stats except weapon range */
		for (i = 0; i < AIR_STATS_MAX; i++) {
			if (i == AIR_STATS_WRANGE)
				continue;
			if (item->craftitem.stats[i] > 2.0f)
				Q_strcat(itemText, va("%s:\t+%i\n", CL_AircraftStatToName(i), CL_AircraftMenuStatsValues(item->craftitem.stats[i], i)), sizeof(itemText));
			else if (item->craftitem.stats[i] < -2.0f)
				Q_strcat(itemText, va("%s:\t%i\n", CL_AircraftStatToName(i), CL_AircraftMenuStatsValues(item->craftitem.stats[i], i)),  sizeof(itemText));
			else if (item->craftitem.stats[i] > 1.0f)
				Q_strcat(itemText, va(_("%s:\t+%i %%\n"), CL_AircraftStatToName(i), (int) (item->craftitem.stats[i] * 100) - 100), sizeof(itemText));
			else if (!equal(item->craftitem.stats[i], 0))
				Q_strcat(itemText, va(_("%s:\t%i %%\n"), CL_AircraftStatToName(i), (int) (item->craftitem.stats[i] * 100) - 100), sizeof(itemText));
		}
	}

	MN_RegisterText(TEXT_STANDARD, itemText);
}

/**
 * @brief Prints the UFOpaedia description for aircraft
 * @note Also checks whether the aircraft tech is already researched or collected
 * @sa BS_MarketAircraftDescription
 * @sa UP_DrawEntry
 */
void UP_AircraftDescription (const technology_t* t)
{
	/* Reset all sort of info for normal items */
	/** @todo Check if this is all needed. Any better way? */
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");
	/* ensure that the buffer is emptied in every case */
	upBuffer[0] = '\0';

	if (RS_IsResearched_ptr(t)) {
		const aircraft_t* aircraft = AIR_GetAircraft(t->provides);
		if (!aircraft) {
			Com_sprintf(upBuffer, sizeof(upBuffer), _("Error - could not find aircraft"));
		} else {
			int i;
			for (i = 0; i < AIR_STATS_MAX; i++) {
				switch (i) {
				case AIR_STATS_SPEED:
					/* speed may be converted to km/h : multiply by pi / 180 * earth_radius */
					Q_strcat(upBuffer, va(_("%s:\t%i km/h\n"), CL_AircraftStatToName(i),
						CL_AircraftMenuStatsValues(aircraft->stats[i], i)), sizeof(upBuffer));
					break;
				case AIR_STATS_MAXSPEED:
					/* speed may be converted to km/h : multiply by pi / 180 * earth_radius */
					Q_strcat(upBuffer, va(_("%s:\t%i km/h\n"), CL_AircraftStatToName(i),
						CL_AircraftMenuStatsValues(aircraft->stats[i], i)), sizeof(upBuffer));
					break;
				case AIR_STATS_FUELSIZE:
					Q_strcat(upBuffer, va(_("Operational range:\t%i km\n"),
						CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE] * aircraft->stats[AIR_STATS_SPEED], AIR_STATS_OP_RANGE)), sizeof(upBuffer));
				case AIR_STATS_ACCURACY:
					Q_strcat(upBuffer, va(_("%s:\t%i\n"), CL_AircraftStatToName(i),
						CL_AircraftMenuStatsValues(aircraft->stats[i], i)), sizeof(upBuffer));
					break;
				default:
					break;
				}
			}
			Q_strcat(upBuffer, va(_("Aircraft size:\t%s\n"), CL_AircraftSizeToName(aircraft->size)), sizeof(upBuffer));
			Q_strcat(upBuffer, va(_("Max. soldiers:\t%i\n"), aircraft->maxTeamSize), sizeof(upBuffer));
		}
	} else if (RS_Collected_(t)) {
		/** @todo Display crippled info and pre-research text here */
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Unknown - need to research this"));
	} else {
		Com_sprintf(upBuffer, sizeof(upBuffer), _("Unknown - need to research this"));
	}
	MN_RegisterText(TEXT_STANDARD, upBuffer);
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the description for robots/ugvs.
 * @param[in] ugvType What type of robot/ugv to print the description for.
 * @sa BS_MarketClick_f
 * @sa UP_DrawEntry
 */
void UP_UGVDescription (const ugv_t *ugvType)
{
	static char itemText[MAX_SMALLMENUTEXTLEN];
	const technology_t *tech;

	assert(ugvType);

	tech = RS_GetTechByProvided(ugvType->id);
	assert(tech);

	/* Set name of ugv/robot */
	Cvar_Set("mn_itemname", _(tech->name));

	/* Reset all sort of info for normal items */
	/** @todo Check if this is all needed. Any better way? */
	Cvar_Set("mn_item", "");
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");

	if (RS_IsResearched_ptr(tech)) {
		/** @todo make me shiny */
		Com_sprintf(itemText, sizeof(itemText), _("%s\n%s"), _(tech->name), ugvType->weapon);
		MN_RegisterText(TEXT_STANDARD, itemText);
	} else if (RS_Collected_(tech)) {
		/** @todo Display crippled info and pre-research text here */
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
		MN_RegisterText(TEXT_STANDARD, itemText);
	} else {
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - need to research this"));
		MN_RegisterText(TEXT_STANDARD, itemText);
	}
}

/**
 * @brief Sets the amount of unread/new mails
 * @note This is called every campaign frame - to update ccs.numUnreadMails
 * just set it to -1 before calling this function
 * @sa CL_CampaignRun
 */
int UP_GetUnreadMails (void)
{
	const message_t *m = cp_messageStack;

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
	Cvar_Set("mn_upunreadmail", va("%i", ccs.numUnreadMails));
	return ccs.numUnreadMails;
}

/**
 * @brief Binds the mail header (if needed) to the mn.menuText array.
 * @note if there is a mail header.
 * @param[in] tech The tech to generate a header for.
 * @param[in] type The type of mail (research proposal or finished research)
 * @sa UP_ChangeDisplay
 * @sa CL_EventAddMail_f
 * @sa CL_GetEventMail
 */
static void UP_SetMailHeader (technology_t* tech, techMailType_t type, eventMail_t* mail)
{
	static char mailHeader[8 * MAX_VAR] = ""; /* bigger as techMail_t (utf8) */
	char dateBuf[MAX_VAR] = "";
	const char *subjectType = "";
	const char *from, *to, *subject, *model;
	dateLong_t date;

	if (mail) {
		from = mail->from;
		to = mail->to;
		model = mail->model;
		subject = mail->subject;
		Q_strncpyz(dateBuf, _(mail->date), sizeof(dateBuf));
		mail->read = qtrue;
		/* reread the unread mails in UP_GetUnreadMails */
		ccs.numUnreadMails = -1;
	} else {
		assert(tech);
		assert(type < TECHMAIL_MAX);

		from = tech->mail[type].from;
		to = tech->mail[type].to;
		subject = tech->mail[type].subject;
		model = tech->mail[type].model;

		if (tech->mail[type].date) {
			Q_strncpyz(dateBuf, _(tech->mail[type].date), sizeof(dateBuf));
		} else {
			switch (type) {
			case TECHMAIL_PRE:
				CL_DateConvertLong(&tech->preResearchedDate, &date);
				Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
					date.year, Date_GetMonthName(date.month - 1), date.day);
				break;
			case TECHMAIL_RESEARCHED:
				CL_DateConvertLong(&tech->researchedDate, &date);
				Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
					date.year, Date_GetMonthName(date.month - 1), date.day);
				break;
			default:
				Sys_Error("UP_SetMailHeader: unhandled techMailType_t %i for date.\n", type);
			}
		}
		if (from != NULL) {
			if (!tech->mail[type].read) {
				tech->mail[type].read = qtrue;
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
					Sys_Error("UP_SetMailHeader: unhandled techMailType_t %i for subject.\n", type);
				}
			}
		} else {
			MN_ResetData(TEXT_UFOPEDIA_MAILHEADER);
			return;
		}
	}
	Com_sprintf(mailHeader, sizeof(mailHeader), _("FROM: %s\nTO: %s\nDATE: %s\nSUBJECT: %s%s\n"),
		_(from), _(to), dateBuf, subjectType, _(subject));
	Cvar_Set("mn_sender_head", model ? model : "");
	MN_RegisterText(TEXT_UFOPEDIA_MAILHEADER, mailHeader);
}

/**
 * @brief Display only the TEXT_UFOPEDIA part - don't reset any other mn.menuText pointers here
 * @param[in] tech The technology_t pointer to print the UFOpaedia article for
 * @sa UP_DrawEntry
 */
void UP_Article (technology_t* tech, eventMail_t *mail)
{
	int i;

	MN_ResetData(TEXT_UFOPEDIA);
	MN_ResetData(TEXT_LIST);

	if (mail) {
		/* event mail */
		Cvar_SetValue("mn_uppreavailable", 0);
		Cvar_SetValue("mn_updisplay", UFOPEDIA_CHAPTERS);
		UP_SetMailHeader(NULL, 0, mail);
		MN_RegisterText(TEXT_UFOPEDIA, _(mail->body));
		/* This allows us to use the index button in the UFOpaedia,
		 * eventMails don't have any chapter to go back to. */
		upDisplay = UFOPEDIA_INDEX;
	} else {
		assert(tech);

		if (RS_IsResearched_ptr(tech)) {
			Cvar_Set("mn_uptitle", va("%s *", _(tech->name)));
			/* If researched -> display research text */
			MN_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->description)));
			if (tech->pre_description.numDescriptions > 0) {
				/* Display pre-research text and the buttons if a pre-research text is available. */
				if (mn_uppretext->integer) {
					MN_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->pre_description)));
					UP_SetMailHeader(tech, TECHMAIL_PRE, NULL);
				} else {
					UP_SetMailHeader(tech, TECHMAIL_RESEARCHED, NULL);
				}
				Cvar_SetValue("mn_uppreavailable", 1);
			} else {
				/* Do not display the pre-research-text button if none is available (no need to even bother clicking there). */
				Cvar_SetValue("mn_uppreavailable", 0);
				Cvar_SetValue("mn_updisplay", UFOPEDIA_CHAPTERS);
				UP_SetMailHeader(tech, TECHMAIL_RESEARCHED, NULL);
			}

			if (upCurrentTech) {
				switch (tech->type) {
				case RS_ARMOUR:
					UP_ArmourDescription(tech);
					break;
				case RS_WEAPON:
					for (i = 0; i < csi.numODs; i++) {
						if (!Q_strcmp(tech->provides, csi.ods[i].id)) {
							UP_ItemDescription(&csi.ods[i]);
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
				case RS_CRAFTITEM:
				{
					const objDef_t *item = AII_GetAircraftItemByID(tech->provides);
					UP_AircraftItemDescription(item);
					break;
				}
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
				MN_RegisterText(TEXT_UFOPEDIA, _(RS_GetDescription(&tech->pre_description)));
				UP_SetMailHeader(tech, TECHMAIL_PRE, NULL);
			} else {
				MN_RegisterText(TEXT_UFOPEDIA, _("No pre-research description available."));
			}
		} else {
			Cvar_Set("mn_uptitle", _(tech->name));
			MN_ResetData(TEXT_UFOPEDIA);
		}
	}
}

/**
 * @brief Set the ammo model to display to selected ammo (only for a reloadable weapon)
 * @param tech technology_t pointer for the weapon's tech
 * @sa UP_DrawEntry
 * @sa UP_DecreaseWeapon_f
 * @sa UP_IncreaseWeapon_f
 */
static void UP_DrawAssociatedAmmo (const technology_t* tech)
{
	const objDef_t *od;

	if (!tech)
		return;

	/* select item */
	od = INVSH_GetItemByID(tech->provides);
	assert(od);
	/* If this is a weapon, we display the model of the associated ammunition in the lower right */
	if (od->numAmmos > 0) {
		/* We set t_associated to ammo to display */
		const technology_t *t_associated = od->ammos[upResearchedLink]->tech;
		assert(t_associated);
		Cvar_Set("mn_upmodel_bottom", t_associated->mdl);
	}
}

/**
 * @brief Displays the UFOpaedia information about a technology
 * @param tech technology_t pointer for the tech to display the information about
 * @sa UP_AircraftDescription
 * @sa UP_BuildingDescription
 * @sa UP_TechDescription
 * @sa UP_ArmourDescription
 * @sa UP_ItemDescription
 */
static void UP_DrawEntry (technology_t* tech, eventMail_t* mail)
{
	Cvar_Set("mn_upmodel_top", "");
	Cvar_Set("mn_upmodel_bottom", "");
	Cvar_Set("mn_upimage_top", "base/empty");

	if (tech) {
		upFireMode = 0;
		upResearchedLink = 0;	/** @todo if the first weapon of the firemode of an ammo is unresearched, its dammages,... will still be displayed */

		if (tech->mdl)
			Cvar_Set("mn_upmodel_top", tech->mdl);
		if (tech->type == RS_WEAPON)
			UP_DrawAssociatedAmmo(tech);
		if (!tech->mdl && tech->image)
			Cvar_Set("mn_upimage_top", tech->image);

		currentChapter = tech->upChapter;
	} else if (!mail)
		return;

	UP_ChangeDisplay(UFOPEDIA_ARTICLE);

	UP_Article(tech, mail);
}

/**
 * @brief
 * @sa CL_EventAddMail_f
 */
void UP_OpenEventMail (const char *eventMailID)
{
	eventMail_t* mail;
	mail = CL_GetEventMail(eventMailID, qfalse);
	if (!mail)
		return;

	MN_PushMenu("mail", NULL);
	UP_DrawEntry(NULL, mail);
}

/**
 * @brief Opens the mail view from everywhere with the entry given through name
 * @param name mail entry id
 * @sa UP_FindEntry_f
 */
static void UP_OpenMailWith (const char *name)
{
	if (!name)
		return;

	MN_PushMenu("mail", NULL);
	Cbuf_AddText(va("ufopedia %s\n", name));
}

/**
 * @brief Opens the UFOpaedia from everywhere with the entry given through name
 * @param name UFOpaedia entry id
 * @sa UP_FindEntry_f
 */
void UP_OpenWith (const char *name)
{
	if (!name)
		return;

	MN_PushMenu("ufopedia", NULL);
	Cbuf_AddText(va("ufopedia %s\n", name));
}

/**
 * @brief Opens the UFOpaedia with the entry given through name, not deleting copies
 * @param name UFOpaedia entry id
 * @sa UP_FindEntry_f
 */
void UP_OpenCopyWith (const char *name)
{
	Cmd_ExecuteString("mn_push_copy ufopedia");
	Cbuf_AddText(va("ufopedia %s\n", name));
}


/**
 * @brief Search and open the UFOpaedia with given id
 */
static void UP_FindEntry_f (void)
{
	const char *id;
	technology_t *tech;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <id>\n", Cmd_Argv(0));
		return;
	}

	/* what are we searching for? */
	id = Cmd_Argv(1);

	/* maybe we get a call like 'ufopedia ""' */
	if (!*id) {
		Com_Printf("UP_FindEntry_f: No UFOpaedia entry given as parameter\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "UP_FindEntry_f: id=\"%s\"\n", id);

	tech = RS_GetTechByID(id);
	if (tech) {
		if (tech->redirect) {
			Com_DPrintf(DEBUG_CLIENT, "UP_FindEntry_f: Tech %s redirected to %s\n", tech->id, tech->redirect->id);
			tech = tech->redirect;
		}

		upCurrentTech = tech;
		UP_DrawEntry(upCurrentTech, NULL);
		return;
	}

	/* if we can not find it */
	Com_DPrintf(DEBUG_CLIENT, "UP_FindEntry_f: No UFOpaedia entry found for %s\n", id);
}

/**
 * @brief Displays the chapters in the UFOpaedia
 * @sa UP_Next_f
 * @sa UP_Prev_f
 */
static void UP_Content_f (void)
{
	int i;

	numChapters_displaylist = 0;
	upText[0] = 0;

	for (i = 0; i < ccs.numChapters; i++) {
		/* Check if there are any researched or collected items in this chapter ... */
		qboolean researched_entries = qfalse;
		upCurrentTech = ccs.upChapters[i].first;
		do {
			if (UP_TechGetsDisplayed(upCurrentTech)) {
				researched_entries = qtrue;
				break;
			}
			if (upCurrentTech != upCurrentTech->upNext && upCurrentTech->upNext)
				upCurrentTech = upCurrentTech->upNext;
			else {
				upCurrentTech = NULL;
			}
		} while (upCurrentTech);

		/* .. and if so add them to the displaylist of chapters. */
		if (researched_entries) {
			assert(numChapters_displaylist<MAX_PEDIACHAPTERS);
			upChapters_displaylist[numChapters_displaylist++] = &ccs.upChapters[i];
			/** @todo integrate images into text better */
			Q_strcat(upText, va(TEXT_IMAGETAG"icons/ufopedia_%s %s\n", ccs.upChapters[i].id, _(ccs.upChapters[i].name)), sizeof(upText));
		}
	}

	UP_ChangeDisplay(UFOPEDIA_CHAPTERS);

	MN_RegisterText(TEXT_UFOPEDIA, upText);
	Cvar_Set("mn_uptitle", _("UFOpaedia Content"));
}

/**
 * @brief Displays the index of the current chapter
 * @sa UP_Content_f
 */
static void UP_Index_f (void)
{
	technology_t* t;
	char *upIndex;

	if (Cmd_Argc() < 2 && !currentChapter) {
		Com_Printf("Usage: %s <chapter-id>\n", Cmd_Argv(0));
		return;
	} else if (Cmd_Argc() == 2) {
		const int chapter = atoi(Cmd_Argv(1));
		if (chapter < ccs.numChapters && chapter >= 0) {
			currentChapter = &ccs.upChapters[chapter];
		}
	}

	if (!currentChapter)
		return;

	UP_ChangeDisplay(UFOPEDIA_INDEX);

	upIndex = upText;
	*upIndex = '\0';
	MN_RegisterText(TEXT_UFOPEDIA, upIndex);

	Cvar_Set("mn_uptitle", va(_("UFOpaedia Index: %s"), _(currentChapter->name)));

	t = currentChapter->first;

	/* get next entry */
	while (t) {
		if (UP_TechGetsDisplayed(t)) {
			/* Add this tech to the index - it gets dsiplayed. */
			Q_strcat(upText, va("%s\n", _(t->name)), sizeof(upText));
		}
		t = t->upNext;
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
 * @brief Displays the previous entry in the UFOpaedia
 * @sa UP_Next_f
 */
static void UP_Prev_f (void)
{
	technology_t *t;

	if (!upCurrentTech) /* if called from console */
		return;

	t = upCurrentTech;

	/* get previous entry */
	if (t->upPrev) {
		/* Check if the previous entry is researched already otherwise go to the next entry. */
		do {
			t = t->upPrev;
			assert(t);
			if (t == t->upPrev)
				Sys_Error("UP_Prev_f: The 'prev':%s entry is equal to '%s'.\n", t->upPrev->id, t->id);
		} while (t->upPrev && !UP_TechGetsDisplayed(t));

		if (UP_TechGetsDisplayed(t)) {
			upCurrentTech = t;
			UP_DrawEntry(t, NULL);
			return;
		}
	}

	/* Go to chapter index if no more previous entries available. */
	UP_Index_f();
}

/**
 * @brief Displays the next entry in the UFOpaedia
 * @sa UP_Prev_f
 */
static void UP_Next_f (void)
{
	technology_t *t;

	if (!upCurrentTech) /* if called from console */
		return;

	t = upCurrentTech;

	/* get next entry */
	if (t && t->upNext) {
		/* Check if the next entry is researched already otherwise go to the next entry. */
		do {
			t = t->upNext;
			if (t == t->upNext)
				Sys_Error("UP_Next_f: The 'next':%s entry is equal to '%s'.\n", t->upNext->id, t->id);
		} while (t->upNext && !UP_TechGetsDisplayed(t));

		if (UP_TechGetsDisplayed(t)) {
			upCurrentTech = t;
			UP_DrawEntry(t, NULL);
			return;
		}
	}

	/* Go to chapter index if no more previous entries available. */
	UP_Index_f();
}


/**
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
 * @sa UP_RightClick_f
 */
static void UP_Click_f (void)
{
	int num;
	technology_t *t;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		if (num < numChapters_displaylist && upChapters_displaylist[num]->first) {
			upCurrentTech = upChapters_displaylist[num]->first;
			do {
				if (UP_TechGetsDisplayed(upCurrentTech)) {
					Cbuf_AddText(va("mn_upindex %i;", upCurrentTech->upChapter->idx));
					return;
				}
				upCurrentTech = upCurrentTech->upNext;
			} while (upCurrentTech);
		}
		break;
	case UFOPEDIA_INDEX:
		assert(currentChapter);
		t = currentChapter->first;

		/* get next entry */
		while (t) {
			if (UP_TechGetsDisplayed(t)) {
				/* Add this tech to the index - it gets displayed. */
				if (num > 0)
					num--;
				else
					break;
			}
			t = t->upNext;
		}
		upCurrentTech = t;
		if (upCurrentTech)
			UP_DrawEntry(upCurrentTech, NULL);
		break;
	case UFOPEDIA_ARTICLE:
		/* we don't want the click function parameter in our index function */
		Cmd_BufClear();
		/* return back to the index */
		UP_Index_f();
		break;
	default:
		Com_Printf("Unknown UFOpaedia display value\n");
	}
}

/**
 * @todo The "num" value and the link-index will most probably not match.
 */
static void UP_TechTreeClick_f (void)
{
	int num;
	int i;
	requirements_t *required_AND;
	technology_t *techRequired;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	if (!upCurrentTech)
		return;

	required_AND = &upCurrentTech->require_AND;
	if (num < 0 || num >= required_AND->numLinks)
		return;

	/* skip every tech which have not been displayed in techtree */
	for (i = 0; i <= num; i++) {
#if 1
		if (required_AND->links[i].type != RS_LINK_TECH
		 && required_AND->links[i].type != RS_LINK_TECH_NOT)
#else
		if (required_AND->links[i].type != RS_LINK_TECH
		 && required_AND->links[i].type != RS_LINK_TECH_NOT
		 && required_AND->links[i].type != RS_LINK_TECH_BEFORE
		 && required_AND->links[i].type != RS_LINK_TECH_XOR)
#endif
			num++;
	}

	techRequired = required_AND->links[num].link;
	if (!techRequired)
		Sys_Error("Could not find the tech for '%s'\n", required_AND->links[num].id);

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
		UP_DrawEntry(upCurrentTech, NULL);
}

/**
 * @brief Mailclient click function callback
 * @sa UP_OpenMail_f
 */
static void UP_MailClientClick_f (void)
{
	message_t *m = cp_messageStack;
	int num;
	int cnt = -1;

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
				UP_OpenMailWith(m->pedia->id);
				return;
			}
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			cnt++;
			if (cnt == num) {
				Cvar_SetValue("mn_uppretext", 0);
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
}

/**
 * @brief Change UFOpaedia article when clicking on the name of associated ammo or weapon
 */
static void UP_ResearchedLinkClick_f (void)
{
	objDef_t *od;

	if (!upCurrentTech) /* if called from console */
		return;

	od = INVSH_GetItemByID(upCurrentTech->provides);
	assert(od);

	if (!Q_strcmp(od->type, "ammo")) {
		const technology_t *t = od->weapons[upResearchedLink]->tech;
		if (UP_TechGetsDisplayed(t))
			UP_OpenWith(t->id);
	} else if (od->weapon && od->reload) {
		const technology_t *t = od->ammos[upResearchedLink]->tech;
		if (UP_TechGetsDisplayed(t))
			UP_OpenWith(t->id);
	}
}

#define MAIL_LENGTH 256
#define MAIL_BUFFER_SIZE 0x4000
static char mailBuffer[MAIL_BUFFER_SIZE];
#define CHECK_MAIL_EOL if (tempBuf[MAIL_LENGTH-3] != '\n') tempBuf[MAIL_LENGTH-2] = '\n';
#define MAIL_CLIENT_LINES 15

/* the menu node of the mail client list */
static menuNode_t *mailClientListNode;

/**
 * @brief Set up mail icons
 * @sa UP_OpenMail_f
 * @note @c UP_OpenMail_f must be called before using this function - we need the
 * @c mailClientListNode pointer here
 */
static void UP_SetMailButtons_f (void)
{
	int i = 0, num;
	const message_t *m = cp_messageStack;

	/* maybe we called this from the console and UP_OpenMail_f wasn't called yet */
	if (!mailClientListNode)
		return;

	num = mailClientListNode->u.text.textScroll;

	while (m && (i < MAIL_CLIENT_LINES)) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (!m->pedia->mail[TECHMAIL_PRE].from)
				break;
			if (num) {
				num--;
			} else {
				Cvar_Set(va("mn_mail_read%i", i), m->pedia->mail[TECHMAIL_PRE].read ? "1": "0");
				Cvar_Set(va("mn_mail_icon%i", i++), m->pedia->mail[TECHMAIL_PRE].icon);
			}
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			if (num) {
				num--;
			} else {
				Cvar_Set(va("mn_mail_read%i", i), m->pedia->mail[TECHMAIL_RESEARCHED].read ? "1": "0");
				Cvar_Set(va("mn_mail_icon%i", i++), m->pedia->mail[TECHMAIL_RESEARCHED].icon);
			}
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from) {
				if (num) {
					num--;
				} else {
					Cvar_Set(va("mn_mail_read%i", i), m->pedia->mail[TECHMAIL_PRE].read ? "1": "0");
					Cvar_Set(va("mn_mail_icon%i", i++), m->pedia->mail[TECHMAIL_PRE].icon);
				}
			} else if (m->pedia->mail[TECHMAIL_RESEARCHED].from) {
				if (num) {
					num--;
				} else {
					Cvar_Set(va("mn_mail_read%i", i), m->pedia->mail[TECHMAIL_RESEARCHED].read ? "1": "0");
					Cvar_Set(va("mn_mail_icon%i", i++), m->pedia->mail[TECHMAIL_RESEARCHED].icon);
				}
			}
			break;
		case MSG_EVENT:
			if (m->eventMail->from) {
				if (num) {
					num--;
				} else {
					Cvar_Set(va("mn_mail_read%i", i), m->eventMail->read ? "1": "0");
					Cvar_Set(va("mn_mail_icon%i", i++), m->eventMail->icon ? m->eventMail->icon : "");
				}
			}
			break;
		default:
			break;
		}
		m = m->next;
	}
	while (i < MAIL_CLIENT_LINES) {
		Cvar_Set(va("mn_mail_read%i", i), "-1");
		Cvar_Set(va("mn_mail_icon%i", i++), "");
	}
}

/**
 * @brief Start the mailclient
 * @sa UP_MailClientClick_f
 * @note use TEXT_UFOPEDIA_MAIL in mn.menuText array (33)
 * @sa CP_GetUnreadMails
 * @sa CL_EventAddMail_f
 * @sa MS_AddNewMessage
 */
static void UP_OpenMail_f (void)
{
	char tempBuf[MAIL_LENGTH] = "";
	const message_t *m = cp_messageStack;
	dateLong_t date;

	mailClientListNode = MN_GetNodeByPath("mailclient.mailclient");
	if (!mailClientListNode)
		Sys_Error("Could not find the mailclient node in mailclient menu\n");

	mailBuffer[0] = '\0';

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			if (!m->pedia->mail[TECHMAIL_PRE].from)
				break;
			CL_DateConvertLong(&m->pedia->preResearchedDate, &date);
			if (!m->pedia->mail[TECHMAIL_PRE].read)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^BProposal: %s\t%i %s %02i\n"),
					_(m->pedia->mail[TECHMAIL_PRE].subject),
					date.year, Date_GetMonthName(date.month - 1), date.day);
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("Proposal: %s\t%i %s %02i\n"),
					_(m->pedia->mail[TECHMAIL_PRE].subject),
					date.year, Date_GetMonthName(date.month - 1), date.day);
			CHECK_MAIL_EOL
			Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			break;
		case MSG_RESEARCH_FINISHED:
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].from)
				break;
			CL_DateConvertLong(&m->pedia->researchedDate, &date);
			if (!m->pedia->mail[TECHMAIL_RESEARCHED].read)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^BRe: %s\t%i %s %02i\n"),
					_(m->pedia->mail[TECHMAIL_RESEARCHED].subject),
					date.year, Date_GetMonthName(date.month - 1), date.day);
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("Re: %s\t%i %s %02i\n"),
					_(m->pedia->mail[TECHMAIL_RESEARCHED].subject),
					date.year, Date_GetMonthName(date.month - 1), date.day);
			CHECK_MAIL_EOL
			Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			break;
		case MSG_NEWS:
			if (m->pedia->mail[TECHMAIL_PRE].from) {
				CL_DateConvertLong(&m->pedia->preResearchedDate, &date);
				if (!m->pedia->mail[TECHMAIL_PRE].read)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s\t%i %s %02i\n"),
						_(m->pedia->mail[TECHMAIL_PRE].subject),
						date.year, Date_GetMonthName(date.month - 1), date.day);
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s\t%i %s %02i\n"),
						_(m->pedia->mail[TECHMAIL_PRE].subject),
						date.year, Date_GetMonthName(date.month - 1), date.day);
				CHECK_MAIL_EOL
				Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			} else if (m->pedia->mail[TECHMAIL_RESEARCHED].from) {
				CL_DateConvertLong(&m->pedia->researchedDate, &date);
				if (!m->pedia->mail[TECHMAIL_RESEARCHED].read)
					Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s\t%i %s %02i\n"),
						_(m->pedia->mail[TECHMAIL_RESEARCHED].subject),
						date.year, Date_GetMonthName(date.month - 1), date.day);
				else
					Com_sprintf(tempBuf, sizeof(tempBuf), _("%s\t%i %s %02i\n"),
						_(m->pedia->mail[TECHMAIL_RESEARCHED].subject),
						date.year, Date_GetMonthName(date.month - 1), date.day);
				CHECK_MAIL_EOL
				Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
			}
			break;
		case MSG_EVENT:
			assert(m->eventMail);
			if (!m->eventMail->from)
				break;
			if (!m->eventMail->read)
				Com_sprintf(tempBuf, sizeof(tempBuf), _("^B%s\t%s\n"),
					_(m->eventMail->subject), _(m->eventMail->date));
			else
				Com_sprintf(tempBuf, sizeof(tempBuf), _("%s\t%s\n"),
					_(m->eventMail->subject), _(m->eventMail->date));
			CHECK_MAIL_EOL
			Q_strcat(mailBuffer, tempBuf, sizeof(mailBuffer));
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
	MN_RegisterText(TEXT_UFOPEDIA_MAIL, mailBuffer);

	UP_SetMailButtons_f();
}

/**
 * @brief Marks all mails read in mailclient
 */
static void UP_SetAllMailsRead_f (void)
{
	const message_t *m = cp_messageStack;

	while (m) {
		switch (m->type) {
		case MSG_RESEARCH_PROPOSAL:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_PRE].read = qtrue;
			break;
		case MSG_RESEARCH_FINISHED:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_RESEARCHED].read = qtrue;
			break;
		case MSG_NEWS:
			assert(m->pedia);
			m->pedia->mail[TECHMAIL_PRE].read = qtrue;
			m->pedia->mail[TECHMAIL_RESEARCHED].read = qtrue;
			break;
		case MSG_EVENT:
			assert(m->eventMail);
			m->eventMail->read = qtrue;
			break;
		default:
			break;
		}
		m = m->next;
	}

	ccs.numUnreadMails = 0;
	Cvar_Set("mn_upunreadmail", va("%i", ccs.numUnreadMails));
	UP_OpenMail_f();
}

/**
 * @brief Increases the number of the weapon to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 */
static void UP_IncreaseWeapon_f (void)
{
	const technology_t *t;
	int upResearchedLinkTemp;
	const objDef_t *od;

	if (!upCurrentTech) /* if called from console */
		return;

	t = upCurrentTech;

	/* select item */
	od = INVSH_GetItemByID(t->provides);
	assert(od);

	upResearchedLinkTemp = upResearchedLink;
	upResearchedLinkTemp++;
	/* We only try to change the value of upResearchedLink if this is possible */
	if (upResearchedLink < od->numWeapons) {
		/* this is an ammo */
		if (upResearchedLinkTemp >= od->numWeapons)
			upResearchedLinkTemp = 0;
		while (!RS_IsResearched_ptr(od->weapons[upResearchedLinkTemp]->tech)) {
			upResearchedLinkTemp++;
			if (upResearchedLinkTemp >= od->numWeapons)
				break;
		}

		if (upResearchedLinkTemp < od->numWeapons) {
			upResearchedLink = upResearchedLinkTemp;
			UP_ItemDescription(od);
		}
	} else if (upResearchedLink < od->numAmmos) {
		/* this is a weapon */
		if (upResearchedLinkTemp >= od->numAmmos)
			upResearchedLinkTemp = 0;
		while (!RS_IsResearched_ptr(od->ammos[upResearchedLinkTemp]->tech)) {
			upResearchedLinkTemp++;
			if (upResearchedLinkTemp >= od->numAmmos)
				break;
		}

		if (upResearchedLinkTemp < od->numAmmos) {
			upResearchedLink = upResearchedLinkTemp;
			UP_DrawAssociatedAmmo(t);
			UP_ItemDescription(od);
		}
	}
}

/**
 * @brief Decreases the number of the firemode to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 */
static void UP_DecreaseWeapon_f (void)
{
	const technology_t *t;
	int upResearchedLinkTemp;
	const objDef_t *od;

	if (!upCurrentTech) /* if called from console */
		return;

	t = upCurrentTech;

	/* select item */
	od = INVSH_GetItemByID(t->provides);
	assert(od);

	upResearchedLinkTemp = upResearchedLink;
	upResearchedLinkTemp--;
	/* We only try to change the value of upResearchedLink if this is possible */
	if (upResearchedLink >= 0 && od->numWeapons > 0) {
		/* this is an ammo */
		if (upResearchedLinkTemp < 0)
			upResearchedLinkTemp = od->numWeapons - 1;
		while (!RS_IsResearched_ptr(od->weapons[upResearchedLinkTemp]->tech)) {
			upResearchedLinkTemp--;
			if (upResearchedLinkTemp < 0)
				break;
		}

		if (upResearchedLinkTemp >= 0) {
			upResearchedLink = upResearchedLinkTemp;
			UP_ItemDescription(od);
		}
	} else if (upResearchedLink >= 0 && od->numAmmos > 0) {
		/* this is a weapon */
		if (upResearchedLinkTemp < 0)
			upResearchedLinkTemp = od->numAmmos - 1;
		while (!RS_IsResearched_ptr(od->ammos[upResearchedLinkTemp]->tech)) {
			upResearchedLinkTemp--;
			if (upResearchedLinkTemp < 0)
				break;
		}

		if (upResearchedLinkTemp >= 0) {
			upResearchedLink = upResearchedLinkTemp;
			UP_DrawAssociatedAmmo(t);
			UP_ItemDescription(od);
		}
	}
}

/**
 * @brief Increases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void UP_IncreaseFiremode_f (void)
{
	const objDef_t *od;

	if (!upCurrentTech) /* if called from console */
		return;

	upFireMode++;

	od = INVSH_GetItemByID(upCurrentTech->provides);
	if (od)
		UP_ItemDescription(od);
}

/**
 * @brief Decreases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void UP_DecreaseFiremode_f (void)
{
	const objDef_t *od;

	if (!upCurrentTech) /* if called from console */
		return;

	upFireMode--;

	od = INVSH_GetItemByID(upCurrentTech->provides);
	if (od)
		UP_ItemDescription(od);
}

/**
 * @sa MN_InitStartup
 */
void UP_InitStartup (void)
{
	/* add commands and cvars */
	Cmd_AddCommand("mn_upindex", UP_Index_f, "Shows the UFOpaedia index for the current chapter");
	Cmd_AddCommand("mn_upcontent", UP_Content_f, "Shows the UFOpaedia chapters");
	Cmd_AddCommand("mn_upback", UP_Back_f, "Goes back from article to index or from index to chapters");
	Cmd_AddCommand("mn_upprev", UP_Prev_f, "Goes to the previous entry in the UFOpaedia");
	Cmd_AddCommand("mn_upnext", UP_Next_f, "Goes to the next entry in the UFOpaedia");
	Cmd_AddCommand("mn_upupdate", UP_Update_f, "Redraw the current UFOpaedia article");
	Cmd_AddCommand("ufopedia", UP_FindEntry_f, "Open the UFOpaedia with the given article");
	Cmd_AddCommand("ufopedia_click", UP_Click_f, NULL);
	Cmd_AddCommand("mailclient_click", UP_MailClientClick_f, NULL);
	Cmd_AddCommand("mn_mail_readall", UP_SetAllMailsRead_f, "Mark all mails read");
	Cmd_AddCommand("ufopedia_rclick", UP_RightClick_f, NULL);
	Cmd_AddCommand("ufopedia_openmail", UP_OpenMail_f, "Start the mailclient");
	Cmd_AddCommand("ufopedia_scrollmail", UP_SetMailButtons_f, NULL);
	Cmd_AddCommand("techtree_click", UP_TechTreeClick_f, NULL);
	Cmd_AddCommand("mn_upgotoresearchedlink", UP_ResearchedLinkClick_f, NULL);
	Cmd_AddCommand("mn_increasefiremode", UP_IncreaseFiremode_f, "Increases the number of the firemode to display in the UFOpaedia");
	Cmd_AddCommand("mn_decreasefiremode", UP_DecreaseFiremode_f, "Decreases the number of the firemode to display in the UFOpaedia");
	Cmd_AddCommand("mn_increaseweapon", UP_IncreaseWeapon_f, "Increases the number of the weapon to display or the ammo to display in the UFOpaedia");
	Cmd_AddCommand("mn_decreaseweapon", UP_DecreaseWeapon_f, "Decreases the number of the weapon to display or the ammo to display in the UFOpaedia");

	mn_uppretext = Cvar_Get("mn_uppretext", "0", 0, "Show the pre-research text in the UFOpaedia");
	mn_uppreavailable = Cvar_Get("mn_uppreavailable", "0", 0, "True if there is a pre-research text available");
	Cvar_Set("mn_displayweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_displayfiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_changeweapon", "0"); /* use strings here - no int */
	Cvar_Set("mn_changefiremode", "0"); /* use strings here - no int */
	Cvar_Set("mn_researchedlinkname", "");
	Cvar_Set("mn_upresearchedlinknametooltip", "");
}

/**
 * @brief Parse the UFOpaedia chapters from UFO-scriptfiles
 * @param[in] name Chapter ID
 * @param[in] text Text for chapter ID
 * @sa CL_ParseFirstScript
 */
void UP_ParseChapters (const char *name, const char **text)
{
	const char *errhead = "UP_ParseChapters: unexpected end of file (names ";
	const char *token;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("UP_ParseChapters: chapter def \"%s\" without body ignored\n", name);
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
		if (ccs.numChapters >= MAX_PEDIACHAPTERS) {
			Com_Printf("UP_ParseChapters: too many chapter defs\n");
			return;
		}
		memset(&ccs.upChapters[ccs.numChapters], 0, sizeof(ccs.upChapters[ccs.numChapters]));
		ccs.upChapters[ccs.numChapters].id = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
		ccs.upChapters[ccs.numChapters].idx = ccs.numChapters;	/* set self-link */

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
		ccs.upChapters[ccs.numChapters].name = Mem_PoolStrDup(token, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

		ccs.numChapters++;
	} while (*text);
}
