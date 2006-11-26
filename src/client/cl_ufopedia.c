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
 * @brief Modify the global display var
 */
void UP_ChangeDisplay (int newDisplay)
{
	if (newDisplay < UFOPEDIA_DISPLAYEND && newDisplay >= 0)
		upDisplay = newDisplay;
	else
		Com_Printf("Error in UP_ChangeDisplay (%i)\n", newDisplay);

	Cvar_SetValue("mn_uppreavailable", 0);

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		/* confunc */
		Cbuf_AddText("mn_upfbig\n");
		Cvar_Set("mn_upmodel_top", "");
		Cvar_Set("mn_upmodel_bottom", "");
		Cvar_Set("mn_upmodel_big", "");
		Cvar_Set("mn_upimage_top", "base/empty");
		Cvar_Set("mn_upimage_bottom", "base/empty");
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
char* CL_WeaponSkillToName(int weaponSkill)
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

#if DEPENDENCIES_OVERHAUL
/**
 * @brief Diplays the tech tree dependencies in the ufopedia
 * @sa UP_DrawEntry
 */
void UP_DisplayTechTree (technology_t* t)
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
				Q_strcat(up_techtree, techRequired->name, sizeof(up_techtree));
			}
			Q_strcat(up_techtree, "\n", sizeof(up_techtree));
		}
	}
	/* and now set the buffer to the right menuText */
	menuText[TEXT_LIST] = up_techtree;
}
#else /* overhaul */
/**
 * @brief Diplays the tech tree dependencies in the ufopedia
 * @sa UP_DrawEntry
 */
void UP_DisplayTechTree (technology_t* t)
{
	int i = 0;
	static char up_techtree[1024];
	stringlist_t *required = NULL;
	technology_t *techRequired = NULL;
	required = &t->requires;
	up_techtree[0] = '\0';
	for (; i<required->numEntries; i++) {
		if (!Q_strncmp(required->string[i], "nothing", MAX_VAR)
		|| !Q_strncmp(required->string[i], "initial", MAX_VAR)) {
			Q_strcat(up_techtree, _("No requirements"), sizeof(up_techtree));
			continue;
		} else {
			techRequired = RS_GetTechByID(required->string[i]);
			if (!techRequired)
				Sys_Error("Could not find the tech for '%s'\n", required->string[i]);
			Q_strcat(up_techtree, techRequired->name, sizeof(up_techtree));
		}
		Q_strcat(up_techtree, "\n", sizeof(up_techtree));
	}
	/* and now set the buffer to the right menuText */
	menuText[TEXT_LIST] = up_techtree;
}
#endif /* overhaul */

/**
 * @brief Prints the (ufopedia and other) description for items (weapons, armor, ...)
 * @param item Index in object definition array ods for the item
 * @sa UP_DrawEntry
 * @sa CL_BuySelectCmd
 * @sa CL_BuyType
 * @sa CL_BuyItem
 * @sa CL_SellItem
 * @sa MN_Drag
 * Not only called from Ufopedia but also from other places to display
 * weapon and ammo stats
 */
void CL_ItemDescription(int item)
{
	static char itemText[MAX_MENUTEXTLEN];
	objDef_t *od;

	/* select item */
	od = &csi.ods[item];
	Cvar_Set("mn_itemname", _(od->name));

	Cvar_Set("mn_item", od->kurz);
	Cvar_Set("mn_weapon", "");
	Cvar_Set("mn_ammo", "");

#ifdef DEBUG
	if (!od->tech && ccs.singleplayer) {
		Com_sprintf(itemText, MAX_MENUTEXTLEN, "Error - no tech assigned\n");
		menuText[TEXT_STANDARD] = itemText;
	} else
#endif
	/* set description text */
	if (RS_IsResearched_ptr(od->tech)) {
		if (!Q_strncmp(od->type, "armor", 5)) {
			/* TODO: Print protection */
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("Armor\n") );
		} else if (!Q_strncmp(od->type, "ammo", 4)) {
			*itemText = '\0';
			/* more will be written below */
		} else if (od->weapon && (od->reload || od->thrown)) {
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
		} else if (od->weapon) {
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s ammo-less weapon with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			/* more will be written below */
		} else {
			/* just an item */
			/* only primary definition */
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s auxiliary equipment with\n"), (od->firetwohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0].range / 32.0), sizeof(itemText));
		}

		if ( !Q_strncmp(od->type, "ammo", 4)
			 || (od->weapon && !od->reload) ) {
			Q_strcat(itemText, va(_("Primary:\t%s\t(%s)\n"), od->fd[0].name, CL_WeaponSkillToName(od->fd[0].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText, va(_("Secondary:\t%s\t(%s)\n"), od->fd[1].name, CL_WeaponSkillToName(od->fd[1].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText,
					va(_("Damage:\t%i / %i\n"), (int) ((od->fd[0].damage[0] + od->fd[0].spldmg[0]) * od->fd[0].shots),
						(int) ((od->fd[1].damage[0] + od->fd[1].spldmg[0]) * od->fd[1].shots)), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i / %i\n"), od->fd[0].time, od->fd[1].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g / %g\n"), od->fd[0].range / 32.0, od->fd[1].range / 32.0), sizeof(itemText));
			Q_strcat(itemText,
					va(_("Spreads:\t%g / %g\n"), (od->fd[0].spread[0] + od->fd[0].spread[1]) / 2, (od->fd[1].spread[0] + od->fd[1].spread[1]) / 2), sizeof(itemText));
		}

		menuText[TEXT_STANDARD] = itemText;
	} else {
		Com_sprintf(itemText, MAX_MENUTEXTLEN, _("Unknown - need to research this"));
		menuText[TEXT_STANDARD] = itemText;
	}
}


/**
 * @brief Prints the ufopedia description for armors
 * @sa UP_DrawEntry
 */
void UP_ArmorDescription (technology_t* t)
{
	objDef_t	*od = NULL;
	int	i;

	/* select item */
	for ( i = 0; i < csi.numODs; i++ )
		if ( !Q_strncmp( t->provides, csi.ods[i].kurz, MAX_VAR ) ) {
			od = &csi.ods[i];
			break;
		}

#ifdef DEBUG
	if ( od == NULL )
		Com_sprintf( upBuffer, MAX_UPTEXT, _("Could not find armor definition") );
	else if ( Q_strncmp(od->type, "armor", MAX_VAR ) )
		Com_sprintf( upBuffer, MAX_UPTEXT, _("Item %s is no armor but %s"), od->kurz, od->type );
	else
#endif
	{
		Cvar_Set( "mn_upmodel_top", "" );
		Cvar_Set( "mn_upmodel_bottom", "" );
		Cvar_Set( "mn_upimage_bottom", "base/empty" );
		Cvar_Set( "mn_upimage_top", t->image_top );
		upBuffer[0] = '\0';
		for ( i = 0; i < csi.numDTs; i++ )
			Q_strcat(upBuffer, va ( _("%s:\tProtection: %i\tHardness: %i\n"), _(csi.dts[i]), od->protection[i], od->hardness[i] ), sizeof(upBuffer));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for technologies
 * @sa UP_DrawEntry
 */
void UP_TechDescription (technology_t* t)
{
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for buildings
 * @sa UP_DrawEntry
 */
void UP_BuildingDescription (technology_t* t)
{
	building_t* b = B_GetBuildingType ( t->provides );

	if ( !b ) {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find building") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Depends:\t%s\n"), b->dependsBuilding >= 0 ? gd.buildingTypes[b->dependsBuilding].name : _("None") );
		Q_strcat(upBuffer, va(_("Buildtime:\t%i day(s)\n"), (int)b->buildTime ), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Fixcosts:\t%i c\n"), (int)b->fixCosts ), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Running costs:\t%i c\n"), (int)b->varCosts ), sizeof(upBuffer));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for aircraft
 * @sa UP_DrawEntry
 */
void UP_AircraftDescription (technology_t* t)
{
	aircraft_t* aircraft = CL_GetAircraft ( t->provides );
	if ( !aircraft ) {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find aircraft") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Speed:\t%.0f\n"), aircraft->speed );
		Q_strcat(upBuffer, va(_("Fuel:\t%i\n"), aircraft->fuelSize ), sizeof(upBuffer));
		Q_strcat(upBuffer, va(_("Weapon:\t%s\n"), aircraft->weapon ? aircraft->weapon->name : _("None") ), sizeof(upBuffer)); /* TODO: there is a weapon in a sample aircraft? */
		Q_strcat(upBuffer, va(_("Shield:\t%s\n"), aircraft->shield ? aircraft->shield->name : _("None") ), sizeof(upBuffer));
	}
	menuText[TEXT_STANDARD] = upBuffer;
	UP_DisplayTechTree(t);
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
void UP_DrawEntry (technology_t* tech)
{
	int i;
	if (!tech)
		return;

	menuText[TEXT_LIST] = menuText[TEXT_STANDARD] = menuText[TEXT_UFOPEDIA] = NULL;

	Cvar_Set("mn_uppreavailable", "0");
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

	if (RS_IsResearched_ptr(tech)) {
		Cvar_Set("mn_uptitle", va("%s *", _(tech->name)));
		/* If researched -> display research text */
		menuText[TEXT_UFOPEDIA] = _(tech->description);
		if (*tech->pre_description) {
			if (mn_uppretext->value)
				menuText[TEXT_UFOPEDIA] = _(tech->pre_description);
			Cvar_Set("mn_uppreavailable", "1");
		}

		if (upCurrent) {
			switch (tech->type) {
			case RS_ARMOR:
				UP_ArmorDescription(tech);
				break;
			case RS_WEAPON:
				for (i = 0; i < csi.numODs; i++) {
					if (!Q_strncmp(tech->provides, csi.ods[i].kurz, MAX_VAR)) {
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
	} else if (RS_Collected_(tech)) {
		Cvar_Set("mn_uptitle", _(tech->name));
		/* Not researched but some items collected -> display pre-research text if available. */
		if (*tech->pre_description)
			menuText[TEXT_UFOPEDIA] = _(tech->pre_description);
		else
			menuText[TEXT_UFOPEDIA] = _("No pre-research description available.");
	} else
		Cvar_Set("mn_uptitle", _(tech->name));
}

/**
 * @brief Opens the ufopedia from everywhere with the entry given through name
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
void UP_OpenWith (char *name)
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
void UP_OpenCopyWith (char *name)
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
void UP_FindEntry_f (void)
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
void UP_Content_f( void )
{
	char *cp = NULL;
	int i;
	char researched_entries;
	numChapters_displaylist = 0;

	cp = upText;
	*cp = '\0';

	for (i = 0; i < gd.numChapters; i++) {
		/* Check if there are any researched items in this chapter ... */
		researched_entries = qfalse;
		upCurrent = &gd.technologies[gd.upChapters[i].first];
		do {
			if ( RS_IsResearched_ptr(upCurrent) ) {
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
			Q_strcat( cp, gd.upChapters[i].name, MAX_UPTEXT);
			Q_strcat( cp, "\n", MAX_UPTEXT);
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
 * @sa UP_ChangeDisplay
 * @sa UP_Index_f
 * @sa UP_DrawEntry
 */
void UP_Back_f (void)
{
	switch (upDisplay) {
	case UFOPEDIA_ARTICLE:
		Cbuf_AddText("mn_upindex;");
		break;
	case UFOPEDIA_INDEX:
		Cbuf_AddText("mn_upcontent;");
		break;
	default:
		break;
	}
}

/**
 * @brief Displays the index of the current chapter
 * @sa UP_Content_f
 */
void UP_Index_f (void)
{
	technology_t* t;
	char *upIndex = NULL;
	int chapter = 0;

	if (Cmd_Argc() < 2 && !currentChapter) {
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
		if (RS_IsResearched_ptr(t)) {
			/* add this tech to the index - it is researched already */
			Q_strcat(upText, va("%s\n", _(t->name)), sizeof(upText));
		}
		if (t->next >= 0)
			t = &gd.technologies[t->next];
		else
			t = NULL;
	}
}


/**
 * @brief Displays the previous entry in the ufopedia
 * @sa UP_Next_f
 */
void UP_Prev_f( void )
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
		} while (t->prev >= 0 && !RS_IsResearched_ptr(t));

		if (RS_IsResearched_ptr(t)) {
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
void UP_Next_f( void )
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
		} while (t->next >= 0 && !RS_IsResearched_ptr(t));

		if (RS_IsResearched_ptr(t)) {
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
 * @param
 * @sa
 */
void UP_Click_f (void)
{
	int num;
	technology_t *t = NULL;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	switch (upDisplay) {
	case UFOPEDIA_CHAPTERS:
		if ( num < numChapters_displaylist && upChapters_displaylist[num]->first ) {
			upCurrent = &gd.technologies[upChapters_displaylist[num]->first];
			do {
				if (RS_IsResearched_ptr(upCurrent)) {
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
			if (RS_IsResearched_ptr(t)) {
				/* add this tech to the index - it is researched already */
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

#if DEPENDENCIES_OVERHAUL
/**
 * @brief
 * @sa
 * @todo The "num" value and the link-index will most probably not match.
 */
void UP_TechTreeClick_f (void)
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

	UP_OpenCopyWith(techRequired->id);
}
#else /* overhaul */
/**
 * @brief
 * @param
 * @sa
 */
void UP_TechTreeClick_f( void )
{
	int num;
	stringlist_t *required = NULL;
	technology_t *techRequired = NULL;

	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));

	if (!upCurrent)
		return;

	required = &upCurrent->requires;

	if (!Q_strncmp(required->string[num], "nothing", MAX_VAR)
		|| !Q_strncmp(required->string[num], "initial", MAX_VAR))
		return;

	if (num >= required->numEntries)
		return;

	techRequired = RS_GetTechByID(required->string[num]);
	if (!techRequired)
		Sys_Error("Could not find the tech for '%s'\n", required->string[num]);

	UP_OpenCopyWith(techRequired->id);
}
#endif /* overhaul */

/**
 * @brief
 */
void UP_SwitchDescriptions_f (void)
{
	if (!Q_strncmp(Cvar_VariableString("mn_up_desc"), "pre", 3))
		Cvar_Set("mn_up_desc", "normal");
	else
		Cvar_Set("mn_up_desc", "pre");
}

/**
 * @brief Redraw the ufopedia article
 */
void UP_Update_f (void)
{
	if (upCurrent)
		UP_DrawEntry(upCurrent);
}

/**
 * @brief Shows available ufopedia entries
 * TODO: Implement me
 */
void UP_List_f ( void )
{
}

/**
 * @brief
 * @sa CL_ResetMenus
 */
void UP_ResetUfopedia( void )
{
	/* reset menu structures */
	gd.numChapters = 0;

	/* add commands and cvars */
	Cmd_AddCommand("mn_upswitch", UP_SwitchDescriptions_f, "Cycles through the ufopedia emails that are available for the current tech");
	Cmd_AddCommand("ufopedialist", UP_List_f, NULL);
	Cmd_AddCommand("mn_upindex", UP_Index_f, "Shows the ufopedia index for the current chapter");
	Cmd_AddCommand("mn_upcontent", UP_Content_f, "Shows the ufopedia chapters");
	Cmd_AddCommand("mn_upback", UP_Back_f, "Goes back from article to index or from index to chapters");
	Cmd_AddCommand("mn_upprev", UP_Prev_f, NULL);
	Cmd_AddCommand("mn_upnext", UP_Next_f, NULL);
	Cmd_AddCommand("mn_upupdate", UP_Update_f, NULL);
	Cmd_AddCommand("ufopedia", UP_FindEntry_f, NULL);
	Cmd_AddCommand("ufopedia_click", UP_Click_f, NULL);
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
void UP_ParseUpChapters( char *id, char **text )
{
	char	*errhead = "UP_ParseUpChapters: unexptected end of file (names ";
	char	*token;

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
		if ( *token == '_' )
			token++;
		if ( !*token )
			continue;
		Q_strncpyz(gd.upChapters[gd.numChapters].name, _(token), MAX_VAR);

		gd.numChapters++;
	} while (*text);
}
