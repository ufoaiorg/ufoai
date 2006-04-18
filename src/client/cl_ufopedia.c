// cl_ufopedia.c -- ufopedia script interpreter
//we need a cl_ufopedia.h to include in client.h (to avoid warnings from needed functions)

#include "cl_ufopedia.h"
#include "cl_research.h"

pediaChapter_t	upChapters[MAX_PEDIACHAPTERS];

technology_t	*upCurrent;

int numChapters;
int numEntries;

char *upData, *upDataStart;
int upDataSize = 0;

#define MAX_UPTEXT 1024
char	upText[MAX_UPTEXT];

// this buffer is for stuff like aircraft or building info
char	upBuffer[MAX_UPTEXT];

// ===========================================================

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
	building_t* b = B_GetBuilding ( t->id );
	building_t* depends = NULL;

	if ( !b )
	{
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find building") );
	} else {
		// needs another building to be buildable
		if ( b->depends )
			depends = B_GetBuilding ( b->depends );

		Com_sprintf(upBuffer, MAX_UPTEXT,
			_("Depends:\t%s\nMore than once:\t%s\nBuildtime:\t%i day(s)\nFixcosts:\t%i $\nRunning costs:\t%i $\nEnergy:\t%i\nWorkercosts:\t%i $\n"),
			depends ? depends->title : _("None"),
			b->moreThanOne ? _("Yes") : _("No"),
			(int)b->buildTime,
			(int)b->fixCosts,
			(int)b->varCosts,
			(int)b->energy,
			(int)b->workerCosts );
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
	aircraft_t* air = CL_GetAircraft ( t->id );
	if ( !air )
	{
		Com_sprintf(upBuffer, MAX_UPTEXT, _("Error - could not find aircraft") );
	} else {
		Com_sprintf(upBuffer, MAX_UPTEXT,
			_("Speed:\t%.0f\nFuel:\t%i\nWeapon:\t%s\nShield:\t%s\n"),
			air->speed,
			air->fuelSize,
			air->weapon ? air->weapon->name : _("None"),
			air->shield ? air->shield->name : _("None") );
	}
	menuText[TEXT_STANDARD] = upBuffer;
}

/*=================
MN_UpDrawEntry
=================*/
void MN_UpDrawEntry( char *id )
{
	int i, j;
	technology_t* t;
	for ( i = 0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !Q_strncmp( id, t->id, MAX_VAR ) ) {
			Cvar_Set( "mn_uptitle", _(t->name) );
			menuText[TEXT_UFOPEDIA] = _(t->description);
			Cvar_Set( "mn_upmodel_top", "" );
			Cvar_Set( "mn_upmodel_bottom", "" );
			Cvar_Set( "mn_upimage_top", "base/empty" );
			Cvar_Set( "mn_upimage_bottom", "base/empty" );
			if ( *t->mdl_top ) Cvar_Set( "mn_upmodel_top", t->mdl_top );
			if ( *t->mdl_bottom ) Cvar_Set( "mn_upmodel_bottom", t->mdl_bottom );
			if ( !*t->mdl_top && *t->image_top ) Cvar_Set( "mn_upimage_top", t->image_top );
			if ( !*t->mdl_bottom && *t->mdl_bottom ) Cvar_Set( "mn_upimage_bottom", t->image_bottom );
			Cbuf_AddText( "mn_upfsmall\n" );

			if ( upCurrent) {
				switch ( t->type )
				{
				case RS_ARMOR:
				case RS_WEAPON:
					for ( j = 0; j < csi.numODs; j++ ) {
						if ( !Q_strncmp( t->provides, csi.ods[j].kurz, MAX_VAR ) ) {
							CL_ItemDescription( j );
							break;
						}
					}
					break;
				case RS_TECH:
					UP_TechDescription( t );
					break;
				case RS_CRAFT:
					UP_AircraftDescription( t );
					break;
				case RS_BUILDING:
					UP_BuildingDescription( t );
					break;
				default:
					menuText[TEXT_STANDARD] = NULL;
					break;
				}
			}
			else
				menuText[TEXT_STANDARD] = NULL;
		}
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
	char *id;
	technology_t *t;
	pediaChapter_t *upc;
	int i;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf(_("Usage: ufopedia <id>\n"));
		return;
	}

	//what are we searching for?
	id = Cmd_Argv( 1 );

	// FIXME: Is this possible? Cmd_Argc needs to be at least 2 here
	if ( !*id ) {
		Com_Printf(_("MN_FindEntry_f: No PediaEntry given as parameter\n"));
		return;
	}

	Com_DPrintf(_("MN_FindEntry_f: id=\"%s\"\n"), id); //DEBUG

	//search in all chapters
	for ( i = 0; i < numChapters; i++ ) {
		upc = &upChapters[i];

		//search from beginning
		t = upc->first;

		//empty chapter
		if ( ! t )
			continue;
		do
		{
			if ( !Q_strncmp ( t->id, id, MAX_VAR ) ) {
				upCurrent = t;
				MN_UpDrawEntry( upCurrent->id );
				return;
			}
			if (t->next)
				t = t->next;
			else
				t = NULL;
		} while ( t );
	}
	//if we can not find it
	Com_DPrintf(_("MN_FindEntry_f: No PediaEntry found for %s\n"), id );
}

/*=================
MN_UpContent_f
=================*/
void MN_UpContent_f( void )
{
	char *cp;
	int i;

	cp = upText;
	*cp = '\0';
	for ( i = 0; i < numChapters; i++ )
	{
		Q_strcat( cp, MAX_UPTEXT, upChapters[i].name );
		Q_strcat( cp, MAX_UPTEXT, "\n" );
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
	pediaChapter_t *upc;

	// get previous entry
	if ( !upCurrent ) return;

	if ( upCurrent->prev )
	{
		upCurrent = upCurrent->prev;
		MN_UpDrawEntry( upCurrent->id );
		return;
	}

	// change chapter
	for ( upc = upCurrent->up_chapter - 1; upc - upChapters >= 0; upc-- )
		if ( upc->last )
		{
			upCurrent = upc->last;
			MN_UpDrawEntry( upCurrent->id );
			return;
		}

	// go to content then
	MN_UpContent_f();
}

/*=================
MN_UpNext_f
=================*/
void MN_UpNext_f( void )
{
	pediaChapter_t *upc;

	// get next entry
	if ( upCurrent && upCurrent->next )
	{
		upCurrent = upCurrent->next;
		MN_UpDrawEntry( upCurrent->id );
		return;
	}

	// change chapter
	if ( !upCurrent ) upc = upChapters;
	else upc = upCurrent->up_chapter + 1;

	for ( ; upc - upChapters < numChapters; upc++ )
		if ( upc->first )
		{
			upCurrent = upc->first;
			MN_UpDrawEntry( upCurrent->id );
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

	if ( num < numChapters && upChapters[num].first )
	{
		upCurrent = upChapters[num].first;
		MN_UpDrawEntry( upCurrent->id );
	}
}


// ===========================================================


/*=================
MN_ResetUfopedia
=================*/
void MN_ResetUfopedia( void )
{
	// reset menu structures
	numChapters = 0;
	numEntries = 0;

	// add commands and cvars
	Cmd_AddCommand( "mn_upcontent", MN_UpContent_f );
	Cmd_AddCommand( "mn_upprev", MN_UpPrev_f );
	Cmd_AddCommand( "mn_upnext", MN_UpNext_f );
	Cmd_AddCommand( "ufopedia", MN_FindEntry_f );
	Cmd_AddCommand( "ufopedia_click", MN_UpClick_f );

	// get data memory
	if ( upDataSize )
		memset( upDataStart, 0, upDataSize );
	else
	{
		Hunk_Begin( 0x10000 );
		upDataStart = Hunk_Alloc( 0x10000 );
		upDataSize = Hunk_End();
	}
	upData = upDataStart;
}


// ===========================================================

/*======================
MN_ParseUpChapters
======================*/
void MN_ParseUpChapters( char *id, char **text )
{
	char	*errhead = _("MN_ParseUpChapters: unexptected end of file (names ");
	char	*token;

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token !='{' ) {
		Com_Printf( _("MN_ParseUpChapters: chapter def \"%s\" without body ignored\n"), id );
		return;
	}

	do {
		// get the id
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// add chapter
		if ( numChapters >= MAX_PEDIACHAPTERS ) {
			Com_Printf( _("MN_ParseUpChapters: too many chapter defs\n"), id );
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
