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

char upText[1024];

// ===========================================================

/*=================
MN_UpDrawEntry
=================*/
void MN_UpDrawEntry( char *id )
{
	int i, j;
	technology_t* t;
	for ( i = 0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			//Com_Printf(_("Displaying %s\n"), t->id); //DEBUG
			Cvar_Set( "mn_uptitle", _(t->name) );
			menuText[TEXT_UFOPEDIA] = _(t->description);
			Cvar_Set( "mn_upmodel_top", "" );
			Cvar_Set( "mn_upmodel_bottom", "" );
			//Cvar_Set( "mn_upmodel_big", "" );
			Cvar_Set( "mn_upimage_top", "base/empty" );
			Cvar_Set( "mn_upimage_bottom", "base/empty" );
			if ( t->mdl_top ) Cvar_Set( "mn_upmodel_top", t->mdl_top );
			if ( t->mdl_bottom ) Cvar_Set( "mn_upmodel_bottom", t->mdl_bottom );
			//if ( t->mdl_big ) Cvar_Set( "mn_upmodel_big", t->mdl_big );
			if ( !t->mdl_top && t->image_top ) { Cvar_Set( "mn_upimage_top", t->image_top ); Com_Printf(_("MN_UpDrawEntry: image set %s\n"), t->image_top);	}
			if ( !t->mdl_bottom &&  t->image_bottom ) Cvar_Set( "mn_upimage_bottom", t->image_bottom );
			//if ( entry->sound )
			//{
				// TODO: play the specified sound
			//}
			Cbuf_AddText( "mn_upfsmall\n" );

			if ( upCurrent) {
				switch ( t->type ) 
				{
				case RS_ARMOR:
				case RS_WEAPON:
					for ( j = 0; j < csi.numODs; j++ ) {
						//Com_Printf(_("MN_UpDrawEntry: id=%s\n"),  t->id);				//DEBUG
						//Com_Printf(_("MN_UpDrawEntry: kurz=%s\n"),   csi.ods[j].kurz );	//DEBUG
						if ( !strcmp( t->provides, csi.ods[j].kurz ) ) {
							CL_ItemDescription( j );
							//Com_Printf(_("MN_UpDrawEntry: drawing item-desc for %s\n"),  t->id); //DEBUG
							break;
						}
					}
					break;
				case RS_TECH:
					// TODO
				case RS_CRAFT:
					// TODO
					break;
				case RS_BUILDING:
					// TODO
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
	
	if ( ! strcmp( id, "" ) ) {
		Com_Printf(_("MN_FindEntry_f: No PediaEntry given as parameter\n"));
		return;
	}
	
	Com_Printf(_("MN_FindEntry_f: id=\"%s\"\n"), id); //DEBUG

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
			if ( !strcmp ( t->id, id ) ) {
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
	Com_Printf(_("MN_FindEntry_f: No PediaEntry found for %s\n"), id );
}

/*=================
MN_UpContent_f
=================*/
void MN_UpContent_f( void )
{
	char *cp;
	int i;

	cp = upText;
	for ( i = 0; i < numChapters; i++ )
	{
		strcpy( cp, upChapters[i].name );
		cp += strlen( cp );
		*cp++ = '\n';
	}
	*cp = 0;

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
	char	*errhead = _("MN_ParseUupChapters: unexptected end of file (names ");
	char	*token;

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) ) {
		Com_Printf( _("MN_ParseUupChapters: chapter def \"%s\" without body ignored\n"), id );
		return;
	}

	do {
		// get the id
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// add chapter
		if ( numChapters >= MAX_PEDIACHAPTERS ) {
			Com_Printf( _("MN_ParseUupChapters: too many chapter defs\n"), id );
			return;
		}
		memset( &upChapters[numChapters], 0, sizeof( pediaChapter_t ) );
		strncpy( upChapters[numChapters].id, token, MAX_VAR );

		// get the name
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		strncpy( upChapters[numChapters].name, _(token), MAX_VAR );
		//Com_Printf( _("MN_ParseUupChapters: parsed chapter %s \n"), upChapters[numChapters].id ); //DEBUG

		numChapters++;
	} while ( *text );
}
