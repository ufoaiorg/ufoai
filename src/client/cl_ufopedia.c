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

static pediaChapter_t	*upChapters_displaylist[MAX_PEDIACHAPTERS];
static int numChapters_displaylist;

static technology_t	*upCurrent;

#define MAX_UPTEXT 4096
static char	upText[MAX_UPTEXT];

/* this buffer is for stuff like aircraft or building info */
static char	upBuffer[MAX_UPTEXT];

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
			Com_sprintf(itemText, MAX_MENUTEXTLEN, "");
			/* more will be written below */
		} else if (od->weapon && od->reload) {
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s weapon with\n"), (od->twohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Max ammo:\t%i\n"), (int) (od->ammo)), sizeof(itemText));
		} else if (od->weapon) {
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s ammo-less weapon with\n"), (od->twohanded ? _("Two-handed") : _("One-handed")));
			/* more will be written below */
		} else {
			/* just an item */
			/* only primary definition */
			Com_sprintf(itemText, MAX_MENUTEXTLEN, _("%s suxiliary equipment with\n"), (od->twohanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Action:\t%s\n"), od->fd[0].name), sizeof(itemText));
			Q_strcat(itemText, va(_("Time units:\t%i\n"), od->fd[0].time), sizeof(itemText));
			Q_strcat(itemText, va(_("Range:\t%g\n"), od->fd[0].range / 32.0), sizeof(itemText));
		}

		if ( !Q_strncmp(od->type, "ammo", 4)
			 || (od->weapon && !od->reload) ) {
			Q_strcat(itemText, va(_("Primary:\t%s\t(%s)\n"), od->fd[0].name, CL_WeaponSkillToName(od->fd[0].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText, va(_("Secondary:\t%s\t(%s)\n"), od->fd[1].name, CL_WeaponSkillToName(od->fd[1].weaponSkill)), sizeof(itemText));
			Q_strcat(itemText,
					va(_("Damage:\t%i / %i\n"), (int) (od->fd[0].damage[0] * od->fd[0].shots + od->fd[0].spldmg[0]),
						(int) (od->fd[1].damage[0] * od->fd[1].shots + od->fd[1].spldmg[0])), sizeof(itemText));
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
void UP_ArmorDescription ( technology_t* t )
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
void UP_TechDescription ( technology_t* t )
{
	UP_DisplayTechTree(t);
}

/**
 * @brief Prints the ufopedia description for buildings
 * @sa UP_DrawEntry
 */
void UP_BuildingDescription ( technology_t* t )
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
void UP_AircraftDescription ( technology_t* t )
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
void UP_DrawEntry( technology_t* tech )
{
	int i;
	if (!tech)
		return;

	menuText[TEXT_LIST] = NULL;
	Cvar_Set("mn_uptitle", _(tech->name));

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
	/* confunc */
	Cbuf_AddText("mn_upfsmall\n");

	if ( RS_IsResearched_ptr(tech) ) {
		/* Is research -> display research text */

		menuText[TEXT_UFOPEDIA] = _(tech->description);

		if (upCurrent) {
			menuText[TEXT_STANDARD] = NULL;
			switch ( tech->type ) {
			case RS_ARMOR:
				UP_ArmorDescription( tech );
				break;
			case RS_WEAPON:
				for (i = 0; i < csi.numODs; i++) {
					if ( !Q_strncmp( tech->provides, csi.ods[i].kurz, MAX_VAR ) ) {
						CL_ItemDescription( i );
						UP_DisplayTechTree(tech);
						break;
					}
				}
				break;
			case RS_TECH:
				UP_TechDescription( tech );
				break;
			case RS_CRAFT:
				UP_AircraftDescription( tech );
				break;
			case RS_BUILDING:
				UP_BuildingDescription( tech );
				break;
			default:
				break;
			}
		}
		else {
			menuText[TEXT_STANDARD] = NULL;
		}
	} else if ( RS_Collected_(tech) ) {
		/* Not researched but some items collected -> display pre-research text if available. */

		if ( tech->description_pre )
			menuText[TEXT_UFOPEDIA] = _(tech->description_pre);
		else
			menuText[TEXT_UFOPEDIA] = _("No pre-research description available.");
	}
}

/**
 * @brief Opens the ufopedia from everywhere with the entry given through name
 * @param name Ufopedia entry id
 * @sa UP_FindEntry_f
 */
void UP_OpenWith ( char *name )
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
void UP_OpenCopyWith ( char *name )
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
void UP_FindEntry_f ( void )
{
	char *id = NULL;
	technology_t *tech = NULL;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf("Usage: ufopedia <id>\n");
		return;
	}

	/*what are we searching for? */
	id = Cmd_Argv( 1 );

	/* maybe we get a call like ufopedia "" */
	if ( !*id ) {
		Com_Printf("UP_FindEntry_f: No PediaEntry given as parameter\n");
		return;
	}

	Com_DPrintf("UP_FindEntry_f: id=\"%s\"\n", id); /*DEBUG */

	tech = RS_GetTechByID( id );

	if (tech) {
		upCurrent = tech;
		UP_DrawEntry( upCurrent );
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

	for ( i = 0; i < gd.numChapters; i++ ) {
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
		} while ( upCurrent );

		/* .. and if so add them to the displaylist of chapters. */
		if ( researched_entries ) {
			upChapters_displaylist[numChapters_displaylist++] = &gd.upChapters[i];
			Q_strcat( cp, gd.upChapters[i].name, MAX_UPTEXT);
			Q_strcat( cp, "\n", MAX_UPTEXT);
		}
	}

	upCurrent = NULL;
	menuText[TEXT_STANDARD] = NULL;
	menuText[TEXT_UFOPEDIA] = upText;
	Cvar_Set("mn_upmodel_top", "");
	Cvar_Set("mn_upmodel_bottom", "");
	Cvar_Set("mn_upmodel_big", "");
	Cvar_Set("mn_upimage_top", "base/empty");
	Cvar_Set("mn_upimage_bottom", "base/empty");
	Cvar_Set("mn_uptitle", _("Ufopedia Content"));
	/* confunc */
	Cbuf_AddText("mn_upfbig\n");
}


/**
 * @brief Displays the previous entry in the ufopedia
 * @sa UP_Next_f
 */
void UP_Prev_f( void )
{
	if ( !upCurrent ) /* if called from console */
		return;

	/* get previous entry */
	if ( upCurrent->prev >= 0 ) {
		/* Check if the previous entry is researched already otherwise go to the next entry. */
		do {
			upCurrent = &gd.technologies[upCurrent->prev];
			assert (upCurrent);
			if (upCurrent->idx == upCurrent->prev)
				Sys_Error("UP_Prev_f: The 'prev':%d entry equals to 'idx' entry for '%s'.\n", upCurrent->prev, upCurrent->id);
		} while ( upCurrent->prev >= 0 && !RS_IsResearched_ptr(upCurrent) );

		if ( RS_IsResearched_ptr(upCurrent) ) {
			UP_DrawEntry( upCurrent );
			return;
		}
	}

	/* change chapter */
	{
		int upc = upCurrent->up_chapter;

		/* get previous chapter */
		while (upc-- > 0) {
			if ( gd.upChapters[upc].last >= 0 ) {
				upCurrent = &gd.technologies[gd.upChapters[upc].last];
				if ( RS_IsResearched_ptr(upCurrent) )
					UP_DrawEntry( upCurrent );
				else
					UP_Prev_f();
				return;
			}
		}
	}

	/* Go to pedia-index if no more previous entries available. */
	UP_Content_f();
}

/**
 * @brief Displays the next entry in the ufopedia
 * @sa UP_Prev_f
 */
void UP_Next_f( void )
{
	if ( !upCurrent ) /* if called from console */
		return;

	/* get next entry */
	if ( upCurrent && ( upCurrent->next >= 0) ) {
		/* Check if the next entry is researched already otherwise go to the next entry. */
		do {
			upCurrent = &gd.technologies[upCurrent->next];
			assert (upCurrent);
			if (upCurrent->idx == upCurrent->next)
				Sys_Error("UP_Next_f: The 'next':%d entry equals to 'idx' entry for '%s'.\n", upCurrent->next, upCurrent->id);
		} while ( upCurrent->next >= 0 && !RS_IsResearched_ptr(upCurrent) );

		if ( RS_IsResearched_ptr(upCurrent) ) {
			UP_DrawEntry( upCurrent );
			return;
		}
	}

	/* change chapter */
	{
		int upc = upCurrent->up_chapter;

		/* get next chapter */
		while (++upc < gd.numChapters) {
			if ( gd.upChapters[upc].first >= 0 ) {
				upCurrent = &gd.technologies[gd.upChapters[upc].first];
				if ( RS_IsResearched_ptr(upCurrent) )
					UP_DrawEntry( upCurrent );
				else
					UP_Next_f();
				return;
			}
		}
	}

	/* do nothing at the end */
}


/**
 * @brief
 * @param
 * @sa
 */
void UP_Click_f( void )
{
	int num;

	if ( Cmd_Argc() < 2 || upCurrent )
		return;
	num = atoi( Cmd_Argv( 1 ) );

	if ( num < numChapters_displaylist && upChapters_displaylist[num]->first ) {
		upCurrent = &gd.technologies[upChapters_displaylist[num]->first];
		do {
			if ( RS_IsResearched_ptr(upCurrent) ) {
				UP_DrawEntry( upCurrent );
				return;
			}
			upCurrent = &gd.technologies[upCurrent->next];
		} while ( upCurrent );
	}
}

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

	if ( Cmd_Argc() < 2 )
		return;
	num = atoi( Cmd_Argv( 1 ) );

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
	/*numEntries = 0; */

	/* add commands and cvars */
	Cmd_AddCommand("ufopedialist", UP_List_f, NULL);
	Cmd_AddCommand("mn_upcontent", UP_Content_f, NULL);
	Cmd_AddCommand("mn_upprev", UP_Prev_f, NULL);
	Cmd_AddCommand("mn_upnext", UP_Next_f, NULL);
	Cmd_AddCommand("ufopedia", UP_FindEntry_f, NULL);
	Cmd_AddCommand("ufopedia_click", UP_Click_f, NULL);
	Cmd_AddCommand("techtree_click", UP_TechTreeClick_f, NULL);
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
	token = COM_Parse( text );

	if ( !*text || *token !='{' ) {
		Com_Printf( "UP_ParseUpChapters: chapter def \"%s\" without body ignored\n", id );
		return;
	}

	do {
		/* get the id */
		token = COM_EParse( text, errhead, id );
		if ( !*text )
			break;
		if ( *token == '}' )
			break;

		/* add chapter */
		if ( gd.numChapters >= MAX_PEDIACHAPTERS ) {
			Com_Printf( "UP_ParseUpChapters: too many chapter defs\n", id );
			return;
		}
		memset( &gd.upChapters[gd.numChapters], 0, sizeof( pediaChapter_t ) );
		Q_strncpyz( gd.upChapters[gd.numChapters].id, token, MAX_VAR );
		gd.upChapters[gd.numChapters].idx = gd.numChapters;	/* set self-link */

		/* get the name */
		token = COM_EParse( text, errhead, id );
		if ( !*text )
			break;
		if ( *token == '}' )
			break;
		if ( *token == '_' )
			token++;
		if ( !*token )
			continue;
		Q_strncpyz( gd.upChapters[gd.numChapters].name, _(token), MAX_VAR );

		gd.numChapters++;
	} while ( *text );
}
