// cl_ufopedia.c -- ufopedia script interpreter
//we need a cl_ufopedia.h to include in client.h (to avoid warnings from needed functions)

#include "cl_ufopedia.h"

typedef struct pediaChapter_s
{
	char	name[MAX_VAR];
	char	title[MAX_VAR];
	struct	pediaEntry_s *first;
	struct	pediaEntry_s *last;
} pediaChapter_t;

typedef struct pediaEntry_s
{
	pediaChapter_t *chapter;
	char	name[MAX_VAR];
	char	title[MAX_VAR];
	char	*modelTop, *modelBottom, *modelBig;
	char	*text;
	byte	item;
	struct	pediaEntry_s *prev;
	struct	pediaEntry_s *next;
} pediaEntry_t;

pediaChapter_t	upChapters[MAX_PEDIACHAPTERS];
pediaEntry_t	upEntries[MAX_PEDIAENTRIES];

pediaEntry_t	*upCurrent;

int numChapters;
int numEntries;

char *upData, *upDataStart;
int upDataSize = 0;

char upText[1024];

// ===========================================================

/*
=================
MN_UpDrawEntry
=================
*/
void MN_UpDrawEntry( pediaEntry_t *entry )
{
	menuText[TEXT_UFOPEDIA] = entry->text;
	Cvar_Set( "mn_upmodel_top", "" );
	Cvar_Set( "mn_upmodel_bottom", "" );
	Cvar_Set( "mn_upmodel_big", "" );
	if ( entry->modelTop ) Cvar_Set( "mn_upmodel_top", entry->modelTop );
	if ( entry->modelBottom ) Cvar_Set( "mn_upmodel_bottom", entry->modelBottom );
	if ( entry->modelBig ) Cvar_Set( "mn_upmodel_big", entry->modelBig );
	Cvar_Set( "mn_uptitle", _(entry->title) );
	Cbuf_AddText( "mn_upfsmall\n" );

	if ( upCurrent && upCurrent->item )
	{
		int i;
		for ( i = 0; i < csi.numODs; i++ )
			if ( !strcmp( entry->name, csi.ods[i].kurz ) )
			{
				CL_ItemDescription( i );
				break;
			}
	}
	else menuText[TEXT_STANDARD] = NULL;
}

/*
=================
UP_OpenWith

open the ufopedia from everywhere
with the entry given through name
=================
*/
void UP_OpenWith ( char *name )
{
	Cbuf_AddText( "mn_push ufopedia\n" );
	Cbuf_Execute();
	Cbuf_AddText( va( "ufopedia %s\n", name ) );
}


/*
=================
MN_FindEntry_f
=================
*/
void MN_FindEntry_f ()
{
	char *name;
	pediaEntry_t *entry;
	pediaChapter_t *upc;
	int i;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf("Usage: ufopedia <name>\n");
		return;
	}

	//what are we searching for?
	name = Cmd_Argv( 1 );

	if ( ! strcmp( name, "" ) )
		return;

	//search in all chapters
	for ( i = 0; i < numChapters; i++ )
	{
		upc = &upChapters[i];

		//search from beginning
		entry = upc->first;

		//empty chapter
		if ( ! entry )
			continue;

		do
		{
			if ( !strcmp ( entry->name, name ) )
			{
				upCurrent = entry;
				MN_UpDrawEntry( upCurrent );
				return;
			}

			if (entry->next)
				entry = entry->next;
			else
				entry = NULL;

		} while ( entry );

	}

	//if we can not find it
	Com_Printf(_("No PediaEntry found for %s\n"), name );
}

/*
=================
MN_UpContent_f
=================
*/
void MN_UpContent_f( void )
{
	char *cp;
	int i;

	cp = upText;
	for ( i = 0; i < numChapters; i++ )
	{
		strcpy( cp, upChapters[i].title );
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
	Cvar_Set( "mn_uptitle", _("Ufopedia Content") );
	Cbuf_AddText( "mn_upfbig\n" );
}


/*
=================
MN_UpPrev_f
=================
*/
void MN_UpPrev_f( void )
{
	pediaChapter_t *upc;

	// get previous entry
	if ( !upCurrent ) return;
	if ( upCurrent->prev )
	{
		upCurrent = upCurrent->prev;
		MN_UpDrawEntry( upCurrent );
		return;
	}

	// change chapter
	for ( upc = upCurrent->chapter - 1; upc - upChapters >= 0; upc-- )
		if ( upc->last )
		{
			upCurrent = upc->last;
			MN_UpDrawEntry( upCurrent );
			return;
		}

	// go to content then
	MN_UpContent_f();
}

/*
=================
MN_UpNext_f
=================
*/
void MN_UpNext_f( void )
{
	pediaChapter_t *upc;

	// get next entry
	if ( upCurrent && upCurrent->next )
	{
		upCurrent = upCurrent->next;
		MN_UpDrawEntry( upCurrent );
		return;
	}

	// change chapter
	if ( !upCurrent ) upc = upChapters;
	else upc = upCurrent->chapter + 1;

	for ( ; upc - upChapters < numChapters; upc++ )
		if ( upc->first )
		{
			upCurrent = upc->first;
			MN_UpDrawEntry( upCurrent );
			return;
		}

	// do nothing at the end
}


/*
=================
MN_UpClick_f
=================
*/
void MN_UpClick_f( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
		return;
	num = atoi( Cmd_Argv( 1 ) );

	if ( num < numChapters && upChapters[num].first )
	{
		upCurrent = upChapters[num].first;
		MN_UpDrawEntry( upCurrent );
	}
}


// ===========================================================


/*
=================
MN_ResetUfopedia
=================
*/
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

#define	EDOFS(x)	(int)&(((pediaEntry_t *)0)->x)

value_t edps[] =
{
	{ "chapter",	V_STRING,	0 },
	{ "title",		V_STRING,	EDOFS( title ) },
	{ "mdl_top",	V_NULL,		EDOFS( modelTop ) },
	{ "mdl_bottom",	V_NULL,		EDOFS( modelBottom ) },
	{ "mdl_big",	V_NULL,		EDOFS( modelBig ) },
	{ "item",		V_BOOL,		EDOFS( item ) },
	{ NULL,	0, 0 }
};

/*
======================
MN_ParseUpEntry
======================
*/
void MN_ParseUpEntry( char *title, char **text )
{
	pediaEntry_t *entry;
	value_t *edp;
	char	*errhead = _("MN_ParseUpEntry: unexptected end of file (names ");
	char	*token;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseUpEntry: entry def \"%s\" without body ignored\n"), title );
		return;
	}
	if ( numEntries >= MAX_PEDIAENTRIES )
	{
		Com_Printf( _("MN_ParseUpEntry: too many ufopedia entries\n"), title );
		return;
	}

	// new entry
	entry = &upEntries[numEntries++];
	memset( entry, 0, sizeof( pediaEntry_t ) );
	strcpy( entry->name, title );

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		if ( *token == '{' )
		{
			// parse text
			qboolean skip;

			entry->text = upData;
			token = *text;
			skip = true;
			while ( *token != '}' )
			{
				if ( *token > 32 )
				{
					skip = false;
					if ( *token == '\\' ) *upData++ = '\n';
					else *upData++ = *token;
				}
				else if ( *token == 32 )
				{
					if ( !skip ) *upData++ = 32;
				}
				else skip = true;
				token++;
			}
			*upData++ = 0;
			*text = token+1;
			continue;
		}

		// get values
		for ( edp = edps; edp->string; edp++ )
			if ( !strcmp( token, edp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				if ( edp->ofs && edp->type != V_NULL ) Com_ParseValue( entry, token, edp->type, edp->ofs );
				else if ( edp->type == V_NULL )
				{
					strcpy( upData, token );
					*(char **)((byte *)entry + edp->ofs) = upData;
					upData += strlen( token ) + 1;
				}
				else
				{
					// find chapter
					int i;
					for ( i = 0; i < numChapters; i++ )
						if ( !strcmp( upChapters[i].name, token ) )
						{
							// add entry to chapter
							entry->chapter = &upChapters[i];
							if ( !upChapters[i].first )
							{
								upChapters[i].first = entry;
								upChapters[i].last = entry;
							}
							else
							{
								pediaEntry_t *old;
								upChapters[i].last = entry;
								old = upChapters[i].first;
								while ( old->next ) old = old->next;
								old->next = entry;
								entry->prev = old;
							}
							break;
						}
					if ( i == numChapters )
						Com_Printf( _("MN_ParseUpEntry: chapter \"%s\" not found (entry %s)\n"), token, title );
				}
				break;
			}

		if ( !edp->string )
			Com_Printf( _("MN_ParseUpEntry: unknown token \"%s\" ignored (entry %s)\n"), token, title );

	} while ( *text );
}


/*
======================
MN_ParseUpChapters
======================
*/
void MN_ParseUpChapters( char *title, char **text )
{
	char	*errhead = _("MN_ParseUupChapters: unexptected end of file (names ");
	char	*token;

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseUupChapters: chapter def \"%s\" without body ignored\n"), title );
		return;
	}

	do {
		// get the name
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// add chapter
		if ( numChapters >= MAX_PEDIACHAPTERS )
		{
			Com_Printf( _("MN_ParseUupChapters: too many chapter defs\n"), title );
			return;
		}
		memset( &upChapters[numChapters], 0, sizeof( pediaChapter_t ) );
		strncpy( upChapters[numChapters].name, token, MAX_VAR );

		// get the title
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		strncpy( upChapters[numChapters].title, _(token), MAX_VAR );

		numChapters++;
	} while ( *text );
}
