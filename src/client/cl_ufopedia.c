/*

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
// cl_ufopedia.c -- ufopedia script interpreter
//we need a cl_ufopedia.h to include in client.h (to avoid warnings from needed functions)

#include "cl_ufopedia.h"
#include "cl_research.h"

pediaChapter_t	upChapters[MAX_PEDIACHAPTERS];
int numChapters;
pediaChapter_t	*upChapters_displaylist[MAX_PEDIACHAPTERS];
int numChapters_displaylist;

technology_t	*upCurrent;


//int numEntries;

#define MAX_UPTEXT 1024
char	upText[MAX_UPTEXT];

// this buffer is for stuff like aircraft or building info
char	upBuffer[MAX_UPTEXT];

// ===========================================================

/*=================
UP_ArmorDescription

prints the ufopedia description for armors
called from MN_UpDrawEntry when type of technology_t is RS_ARMOR
=================*/
void UP_ArmorDescription ( technology_t* t )
{
	objDef_t	*od = NULL;
	int	i;

	// select item
	for ( i = 0; i < csi.numODs; i++ )
		if ( !Q_strncmp( t->provides, csi.ods[i].kurz, MAX_VAR ) )
		{
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
			Q_strcat( upBuffer, MAX_UPTEXT, va ( _("%s:\tProtection: %i\tHardness: %i\n"), _(csi.dts[i]), od->protection[i], od->hardness[i] ) );
	}
	menuText[TEXT_STANDARD] = upBuffer;
}

/*=================
UP_TechDescription

prints the ufopedia description for technologies
called from MN_UpDrawEntry when type of technology_t is RS_TECH
=================*/
void UP_TechDescription ( technology_t* t )
{

}

/*=================
UP_BuildingDescription

prints the ufopedia description for buildings
called from MN_UpDrawEntry when type of technology_t is RS_BUILDING
=================*/
void UP_BuildingDescription ( technology_t* t )
{
	building_t* b = B_GetBuilding ( t->provides );

	if ( !b )
	{
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find building") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Depends:\t%s\n"), b->dependsBuilding ? b->dependsBuilding->name : _("None") );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Buildtime:\t%i day(s)\n"), (int)b->buildTime ) );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Fixcosts:\t%i c\n"), (int)b->fixCosts ) );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Running costs:\t%i c\n"), (int)b->varCosts ) );
	}
	menuText[TEXT_STANDARD] = upBuffer;
}

/*=================
UP_AircraftDescription

prints the ufopedia description for aircraft
called from MN_UpDrawEntry when type of technology_t is RS_CRAFT
=================*/
void UP_AircraftDescription ( technology_t* t )
{
	aircraft_t* air = CL_GetAircraft ( t->provides );
	if ( !air )
	{
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find aircraft") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Speed:\t%.0f\n"), air->speed );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Fuel:\t%i\n"), air->fuelSize ) );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Weapon:\t%s\n"), air->weapon ? air->weapon->name : _("None") ) );
		Q_strcat(upBuffer, MAX_UPTEXT, va(_("Shield:\t%s\n"), air->shield ? air->shield->name : _("None") ) );
	}
	menuText[TEXT_STANDARD] = upBuffer;
}

/*=================
MN_UpDrawEntry
=================*/
void MN_UpDrawEntry( technology_t* tech )
{
	int i;
	if ( ! tech )
		return;

	Cvar_Set( "mn_uptitle", _(tech->name) );
	menuText[TEXT_UFOPEDIA] = _(tech->description);
	Cvar_Set( "mn_upmodel_top", "" );
	Cvar_Set( "mn_upmodel_bottom", "" );
	Cvar_Set( "mn_upimage_top", "base/empty" );
	Cvar_Set( "mn_upimage_bottom", "base/empty" );
	if ( *tech->mdl_top ) Cvar_Set( "mn_upmodel_top", tech->mdl_top );
	if ( *tech->mdl_bottom ) Cvar_Set( "mn_upmodel_bottom", tech->mdl_bottom );
	if ( !*tech->mdl_top && *tech->image_top ) Cvar_Set( "mn_upimage_top", tech->image_top );
	if ( !*tech->mdl_bottom && *tech->mdl_bottom ) Cvar_Set( "mn_upimage_bottom", tech->image_bottom );
	Cbuf_AddText( "mn_upfsmall\n" );

	if ( upCurrent) {
		menuText[TEXT_STANDARD] = NULL;
		switch ( tech->type )
		{
		case RS_ARMOR:
			UP_ArmorDescription( tech );
			break;
		case RS_WEAPON:
			for ( i = 0; i < csi.numODs; i++ ) {
				if ( !Q_strncmp( tech->provides, csi.ods[i].kurz, MAX_VAR ) ) {
					CL_ItemDescription( i );
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
}

/*=================
UP_OpenWith

open the ufopedia from everywhere
with the entry given through name
=================*/
void UP_OpenWith ( char *name )
{
	Cbuf_AddText( "mn_push ufopedia\n" );
	Cbuf_Execute();
	Cbuf_AddText( va( "ufopedia %s\n", name ) );
}

/*=================
MN_FindEntry_f
=================*/
void MN_FindEntry_f ( void )
{
	char *id = NULL;
	technology_t *tech = NULL;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf("Usage: ufopedia <id>\n");
		return;
	}

	//what are we searching for?
	id = Cmd_Argv( 1 );

	// maybe we get a call like ufopedia ""
	if ( !*id ) {
		Com_Printf("MN_FindEntry_f: No PediaEntry given as parameter\n");
		return;
	}

	Com_DPrintf("MN_FindEntry_f: id=\"%s\"\n", id); //DEBUG

	tech = RS_GetTechByID( id );
	
	if (tech) {
		upCurrent = tech;
		MN_UpDrawEntry( upCurrent );
		return;
	}

	//if we can not find it
	Com_DPrintf("MN_FindEntry_f: No PediaEntry found for %s\n", id );
}

/*=================
MN_UpContent_f
=================*/
void MN_UpContent_f( void )
{
	char *cp = NULL;
	int i;
	char researched_entries;
	numChapters_displaylist = 0;

	cp = upText;
	*cp = '\0';

	for ( i = 0; i < numChapters; i++ )
	{
		// Check if there are any researched items in this chapter ...
		researched_entries = qfalse;
		upCurrent = &technologies[upChapters[i].first];
		do
		{
			if ( RS_IsResearched_(upCurrent) ) {
				researched_entries = qtrue;
				break;
			}
			if (upCurrent->idx != upCurrent->next)
				upCurrent = &technologies[upCurrent->next];
			else {
				upCurrent = NULL;
			}
		} while ( upCurrent );

		// .. and if so add them to the displaylist of chapters.
		if ( researched_entries ) {
			upChapters_displaylist[numChapters_displaylist++] = &upChapters[i];
			Q_strcat( cp, MAX_UPTEXT, upChapters[i].name );
			Q_strcat( cp, MAX_UPTEXT, "\n" );
		}
	}

	upCurrent = NULL;
	menuText[TEXT_STANDARD] = NULL;
	menuText[TEXT_UFOPEDIA] = upText;
	Cvar_Set( "mn_upmodel_top", "" );
	Cvar_Set( "mn_upmodel_bottom", "" );
	Cvar_Set( "mn_upmodel_big", "" );
	Cvar_Set( "mn_upimage_top", "base/empty" );
	Cvar_Set( "mn_upimage_bottom", "base/empty" );
	Cvar_Set( "mn_uptitle", _("Ufopedia Content") );
	Cbuf_AddText( "mn_upfbig\n" );
}


/*=================
MN_UpPrev_f
=================*/
void MN_UpPrev_f( void )
{
	int upc;
	
	if ( !upCurrent ) return;

	// get previous chapter
	if (upc > 0)
		upc = upCurrent->up_chapter - 1;

	// get previous entry
	if ( upCurrent->prev ) {
		// Check if the previous entry is researched already otherwise go to the next entry.
		do { upCurrent = &technologies[upCurrent->prev]; } while ( upCurrent && !RS_IsResearched_(upCurrent) );
		if ( upCurrent ) {
			MN_UpDrawEntry( upCurrent );
			return;
		}
	}

	// change chapter
	for (; upc >= 0; upc-- )
		if ( upChapters[upc].last >= 0 ) {
			upCurrent = &technologies[upChapters[upc].last];
			if ( RS_IsResearched_(upCurrent) )
				MN_UpDrawEntry( upCurrent );
			else
				MN_UpPrev_f();
			return;
		}

	// Go to pedia-index if no more previous entries available.
	MN_UpContent_f();
}

/*=================
MN_UpNext_f
=================*/
void MN_UpNext_f( void )
{
	int upc;

	// change chapter
	if ( !upCurrent ) upc = 0; //upChapters;
	else upc = upCurrent->up_chapter + 1;

	// get next entry
	if ( upCurrent && ( upCurrent->next >= 0) ) {
		// Check if the next entry is researched already otherwise go to the next entry.
		do {
			if ( upCurrent->idx != upCurrent->next ) {
				upCurrent = &technologies[upCurrent->next];
			} else {
				Com_DPrintf("MN_UpNext_f: There was a 'next' entry for '%s' where there should not be one.\n",upCurrent->id);
				upCurrent = NULL;
			}
		} while ( upCurrent && !RS_IsResearched_(upCurrent) );

		if ( upCurrent ) {
			MN_UpDrawEntry( upCurrent );
			return;
		}
	}

	/* no 'next' entry defined (=NULL) or no current entry at all */

	// change chapter
	for ( ; upc < numChapters; upc++ )
		if ( upChapters[upc].first >= 0 ) {
			upCurrent = &technologies[upChapters[upc].first];
			if ( RS_IsResearched_(upCurrent) )
				MN_UpDrawEntry( upCurrent );
			else
				MN_UpNext_f();
			return;
		}

	// do nothing at the end
}


/*=================
MN_UpClick_f
=================*/
void MN_UpClick_f( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
		return;
	num = atoi( Cmd_Argv( 1 ) );

	if ( num < numChapters_displaylist && upChapters_displaylist[num]->first )
	{
		upCurrent = &technologies[upChapters_displaylist[num]->first];
		do
		{
			if ( RS_IsResearched_(upCurrent) )
			{
				MN_UpDrawEntry( upCurrent );
				return;
			}
			upCurrent = &technologies[upCurrent->next];
		} while ( upCurrent );
	}
}


// ===========================================================

/*=================
UP_List_f

shows available ufopedia entries
TODO: Implement me
=================*/
void UP_List_f ( void )
{
}

/*=================
UP_ResetUfopedia
=================*/
void UP_ResetUfopedia( void )
{
	// reset menu structures
	numChapters = 0;
	//numEntries = 0;

	// add commands and cvars
	Cmd_AddCommand( "ufopedialist", UP_List_f );
	Cmd_AddCommand( "mn_upcontent", MN_UpContent_f );
	Cmd_AddCommand( "mn_upprev", MN_UpPrev_f );
	Cmd_AddCommand( "mn_upnext", MN_UpNext_f );
	Cmd_AddCommand( "ufopedia", MN_FindEntry_f );
	Cmd_AddCommand( "ufopedia_click", MN_UpClick_f );
}


// ===========================================================

/*======================
UP_ParseUpChapters
======================*/
void UP_ParseUpChapters( char *id, char **text )
{
	char	*errhead = "UP_ParseUpChapters: unexptected end of file (names ";
	char	*token;

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token !='{' ) {
		Com_Printf( "UP_ParseUpChapters: chapter def \"%s\" without body ignored\n", id );
		return;
	}

	do {
		// get the id
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// add chapter
		if ( numChapters >= MAX_PEDIACHAPTERS ) {
			Com_Printf( "UP_ParseUpChapters: too many chapter defs\n", id );
			return;
		}
		memset( &upChapters[numChapters], 0, sizeof( pediaChapter_t ) );
		Q_strncpyz( upChapters[numChapters].id, token, MAX_VAR );

		// get the name
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		if ( *token == '_' ) token++;
		if ( !*token )
			continue;
		Q_strncpyz( upChapters[numChapters].name, _(token), MAX_VAR );

		numChapters++;
	} while ( *text );
}
