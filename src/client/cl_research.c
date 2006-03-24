// cl_research.c

#include "client.h"
#include "cl_research.h"


byte researchList[MAX_RESEARCHLIST];
int researchListLength;
int globalResearchNum;
char infoResearchText[MAX_MENUTEXTLEN];
int numTechnologies;

technology_t technologies[MAX_TECHNOLOGIES];

/*
Note:
ObjDef_t in q_shared.h holds the researchNeeded and researchStatus flags
*/

/*======================
R_ResearchPossible

checks if there is at least one base with a lab and scientists available and tells the player the status.
======================*/
char R_ResearchPossible ( void )
{
	if ( baseCurrent ) {
		if ( baseCurrent->hasLab ) {
			if ( B_HowManyPeopleInBase2 ( baseCurrent, 1 ) > 0)  {
				return true;
			} else {
				Com_Printf( _("You need to hire some scientists before research.\n") );
				MN_Popup( _("Notice"), _("You need to hire some scientists before research.") );
			}
		} else {
			Com_Printf( _("Build a laboratory first and hire/transfer some scientists.\n") );
			MN_Popup( _("Notice"), _("Build a laboratory first and hire/transfer some scientists.") );
		}
	}
	return false;
}

/*======================
R_ResearchDisplayInfo
======================*/
void R_ResearchDisplayInfo ( int num  )
{
	// we are not in base view
	if ( ! baseCurrent )
		return;
	objDef_t *od;
	od = &csi.ods[globalResearchNum];
	Cvar_Set( "mn_research_selname",  od->name );
	Cvar_Set( "mn_research_seltime", va( "Time: %i\n", od->researchTime ) );

	switch ( od->researchStatus )
	{
	case RS_RUNNING:
		Cvar_Set( "mn_research_selstatus", "Status: Under research\n" );
		break;
	case RS_FINISH:
		Cvar_Set( "mn_research_selstatus", "Status: Research finished\n" );
		break;
	case RS_NONE:
		Cvar_Set( "mn_research_selstatus", "Status: Unknown\n" );
		break;
	default:
		break;
	}


}

/*======================
CL_ResearchSelectCmd
======================*/
void CL_ResearchSelectCmd( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: research_select <num>\n" );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num >= researchListLength )
	{
		menuText[TEXT_STANDARD] = NULL;
		return;
	}

	// call researchselect function from menu_research.ufo
	Cbuf_AddText( va( "researchselect%i\n", num ) );
	globalResearchNum = researchList[num];
	R_ResearchDisplayInfo( num );
}

/*======================
R_ResearchStart

TODO: Check if laboratory is available
======================*/
void R_ResearchStart ( void )
{
	objDef_t *od;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	od = &csi.ods[globalResearchNum];
	if (od->researchStatus != RS_NONE) {
		if  (od->researchStatus == RS_FINISH)
			MN_Popup( _("Notice"), _("The research on this item is complete.") );
		else
			MN_Popup( _("Notice"), _("This item is already under research by your scientists.") );
	} else {
		od->researchStatus = RS_RUNNING;
	}
	R_UpdateData();
}

/*======================
R_ResearchStop

TODO: Check if laboratory is available
======================*/
void R_ResearchStop ( void )
{
	objDef_t *od;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	od = &csi.ods[globalResearchNum];
	if (od->researchStatus != RS_FINISH)
		od->researchStatus = RS_NONE;
	else
		MN_Popup( _("Notice"), _("The research on this item is complete.") );
	R_UpdateData();
}

/*======================
R_UpdateData
======================*/
void R_UpdateData ( void )
{
	objDef_t *od;
	int i, j;
	char name [MAX_VAR];
	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ ) { //TODO: Debug what happens if there are more than 28 items (j>28) !
		if ( od->researchNeeded) { // only handle items if the need researching
			strcpy(name, od->name);
			//if (od->researchStatus =!= RS_NONE ) // Set the text of the research items and mark them if they are currently researched.
			//	strcat(name, " [under research]");	//TODO: colorcode "string txt_item%i" ?
			switch ( od->researchStatus )
			{
			case RS_RUNNING:
				strcat(name, " [under research]");	//TODO: colorcode "string txt_item%i" ?
				break;
			case RS_FINISH:
				strcat(name, " [finished]");
				break;
			default:
				break;
			}
			Cvar_Set( va("mn_researchitem%i", j),  name ); //TODO: colorcode maybe?
			researchList[j] = i;
			j++;

		}
	}
	researchListLength = j;

	// Set rest of the list-entries to have no text at all. TODO: 28 should not be defined here.
	for ( ; j < 28; j++ )
		Cvar_Set( va( "mn_researchitem%i", j ), "" );

	// select first item that needs to be researched
	if ( researchListLength )
	{
		Cbuf_AddText( "researchselect0\n" );
		CL_ItemDescription( researchList[0] );
		globalResearchNum = researchList[0];
		R_ResearchDisplayInfo( 0 );
	} else {
		// reset description
		Cvar_Set( "mn_researchitemname", "" );
		Cvar_Set( "mn_researchitem", "" );
		Cvar_Set( "mn_researchweapon", "" );
		Cvar_Set( "mn_researchammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
}

/*======================
MN_ResearchType
======================*/
void CL_ResearchType ( void )
{
	// get the list
	R_UpdateData();

	// nothing to research here
	if ( ! researchListLength || !ccs.numBases )
		Cbuf_ExecuteText( EXEC_NOW, "mn_pop" );
	else if ( baseCurrent && ! baseCurrent->hasLab )
		MN_Popup( _("Notice"), _("Build a laboratory first") );
}

/*======================
CL_CheckResearchStatus
======================*/
void CL_CheckResearchStatus ( void )
{
	int i, newResearch = 0;
	objDef_t *od;

	if ( ! researchListLength )
		return;

	for ( i = 0; i < researchListLength; i++ )
	{
		od = &csi.ods[researchList[i]];
		if ( od->researchStatus == RS_RUNNING )
		{
			if ( od->researchTime <= 0 )
			{
				if ( ! newResearch )
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("Research of %s finished\n"), od->name );
				else
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("%i researches finished\n"), newResearch+1 );
				// leave all other flags - maybe useful for statistic?
				od->researchStatus = RS_FINISH;
				newResearch++;
			}
			else
			{
				// FIXME: Make this depending on how many scientists are hired
				// TODO: What time is stored here exactly? the formular below may be way of.
				//od->researchTime -= B_HowManyPeopleInBase2 ( baseCurrent, 1 );
				od->researchTime--;					// DEBUG
				Com_Printf( _("%i\n"),od->researchTime );	// DEBUG
				Com_Printf( _("time reduced\n") );			// DEBUG
			}
		}
	}

	if ( newResearch )
	{
		MN_Popup( _("Research finished"), infoResearchText );
		CL_ResearchType();
	}
}

/*======================
R_TechnologyList_f

list all parsed technologies
======================*/
void R_TechnologyList_f ( void )
{
	int i = 0;
	technology_t* t;
	for ( i = 0; i < numTechnologies; i++ )
	{
		t = &technologies[i];
		Com_Printf(_("Tech: %s\n"), t->title );
		Com_Printf(_("... time %.2f\n"), t->time );
		Com_Printf(_("... name %s\n"), t->name );
		Com_Printf(_("... provides %s\n"), t->provides );
		Com_Printf(_("... requires %s\n"), t->requires );
		Com_Printf(_("... type %s\n"), t->type );
	}
}

/*======================
MN_ResearchInit
======================*/
void MN_ResearchInit( void )
{
	CL_ResearchType();
}

/*======================
MN_ResetResearch
======================*/
void MN_ResetResearch( void )
{
	researchListLength = 0;
	// add commands and cvars
	Cmd_AddCommand( "research_init", MN_ResearchInit );
	Cmd_AddCommand( "research_select", CL_ResearchSelectCmd );
	Cmd_AddCommand( "research_type", CL_ResearchType );
	Cmd_AddCommand( "mn_start_research", R_ResearchStart );
	Cmd_AddCommand( "mn_stop_research", R_ResearchStop );
	Cmd_AddCommand( "research_update", R_UpdateData );
	Cmd_AddCommand( "technologylist", R_TechnologyList_f );
}

// NOTE: the BSFS define is the same like for bases and so on...
value_t valid_tech_vars[] =
{
	//name of technology
	{ "name",	V_STRING,	TECHFS( name ) },

	//which type is it? weapon, craft, ...
	{ "type",	V_STRING,	TECHFS( type ) },

	//what does it require
	{ "requires",	V_STRING,	TECHFS( requires ) },

	//what does this research provide
	{ "provides",	V_STRING,	TECHFS( provides ) },

	//how long will this research last
	{ "time",	V_FLOAT,	TECHFS( time ) },
	{ "description",	V_STRING,	TECHFS( description ) },

	{ NULL,	0, 0 }
};

/*======================
MN_ResetResearch
======================*/
void MN_ParseTechnologies ( char* title, char** text )
{
	value_t *var;
	technology_t *t;
	char	*errhead = _("MN_ParseTechnologies: unexptected end of file (names ");
	char	*token;

	// get body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseTechnologies: technology def \"%s\" without body ignored\n"), title );
		return;
	}
	if ( numTechnologies >= MAX_TECHNOLOGIES )
	{
		Com_Printf( _("MN_ParseTechnologies: too many technology entries\n"), title );
		return;
	}

	// new technology
	t = &technologies[numTechnologies++];
	memset( t, 0, sizeof( technology_t ) );
	strcpy( t->title, title );
	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		// get values
		for ( var = valid_tech_vars; var->string; var++ )
			if ( !strcmp( token, var->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				if ( var->ofs && var->type != V_NULL )
					Com_ParseValue( t, token, var->type, var->ofs );
				else
					// NOTE: do we need a buffer here? for saving or something like that?
					Com_Printf(_("Error - no buffer for technologies - V_NULL not allowed\n"));
				break;
			}

		if ( !var->string )
			Com_Printf( _("MN_ParseUpEntry: unknown token \"%s\" ignored (entry %s)\n"), token, title );

	} while ( *text );

}
