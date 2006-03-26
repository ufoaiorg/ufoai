// cl_research.c

#include "client.h"
#include "cl_research.h"

void R_GetFirstRequired( char *id,  research_requirements_t *required);

byte researchList[MAX_RESEARCHLIST];
int researchListLength;
int globalResearchNum;
char infoResearchText[MAX_MENUTEXTLEN];
int numTechnologies;

technology_t technologies[MAX_TECHNOLOGIES];

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

/*
======================
R_GetName

// TODO
Return "name" if present, otherwise enter the correct .ufo file and read it from there.
======================
*/
void R_GetName( char *id, char *name )
{
	int i, j;
	technology_t *t;
	objDef_t *od;
	strcpy( name, id );
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			switch ( t->type ) //Search in correct data/name.
			{
			case RS_TECH:
				strcpy( name, t->name ); return;
				break;
			case RS_WEAPON:
				for ( j = 0, od = csi.ods; j < csi.numODs; j++ ) { 				
					od = &csi.ods[j];
					if ( !strcmp( id, od->kurz) ) {
						strcpy(name, od->name);
						return;
					}
				}
				break;
			case RS_ARMOR:	break;
			case RS_CRAFT:	break;
			case RS_BUILDING:	break;
			default:			break;
			}
			return;
		}
	}
	Com_Printf( _("R_GetName: technology \"%s\" not found.\n"), id );
}	

/*======================
R_ResearchDisplayInfo
======================*/
void R_ResearchDisplayInfo ( void  )
{
	// we are not in base view
	if ( ! baseCurrent )
		return;

	technology_t *t;
	t = &technologies[globalResearchNum];
	char tempname[MAX_VAR];
	R_GetName( t->id, tempname);
	Cvar_Set( "mn_research_selname",  tempname );
	Cvar_Set( "mn_research_seltime", va( "Time: %.1f\n", t->time ) );

	switch ( t->statusResearch )
	{
	case RS_RUNNING:
		Cvar_Set( "mn_research_selstatus", "Status: Under research\n" );
		break;
	case RS_PAUSED:
		Cvar_Set( "mn_research_selstatus", "Status: Research paused\n" );
		break;
	case RS_FINISH:
		Cvar_Set( "mn_research_selstatus", "Status: Research finished\n" );
		break;
	case RS_NONE:
		Cvar_Set( "mn_research_selstatus", "Status: Unknown technology\n" );
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
	R_ResearchDisplayInfo();
}

/*======================
R_ResearchStart

TODO: Check if laboratory is available
======================*/
void R_ResearchStart ( void )
{
	// we are not in base view
	if ( ! baseCurrent )
		return;

	technology_t *t;
	t = &technologies[globalResearchNum];
	switch ( t->statusResearch )
	{
	case RS_RUNNING:
		MN_Popup( _("Notice"), _("This item is already under research by your scientists.") );
		break;
	case RS_PAUSED:
		MN_Popup( _("Notice"), _("The research on this item continues.") );
		t->statusResearch = RS_RUNNING;
		break;
	case RS_FINISH:
		MN_Popup( _("Notice"), _("The research on this item is complete.") );
		break;
	case RS_NONE:
		t->statusResearch = RS_RUNNING;
		break;
	default:
		break;
	}
	R_UpdateData();
}

/*======================
R_ResearchStop

TODO: Check if laboratory is available
======================*/
void R_ResearchStop ( void )
{
	// we are not in base view
	if ( ! baseCurrent )
		return;

	technology_t *t;
	t = &technologies[globalResearchNum];
	switch ( t->statusResearch )
	{
	case RS_RUNNING:
		t->statusResearch = RS_PAUSED;
		break;
	case RS_PAUSED:
		MN_Popup( _("Notice"), _("The research on this item continues.") );
		t->statusResearch = RS_RUNNING;
		break;
	case RS_FINISH:
		MN_Popup( _("Notice"), _("The research on this item is complete.") );
		break;
	case RS_NONE:
		Com_Printf( "Can't pause research. Research not started.\n" );
		break;
	default:
		break;
	}
	R_UpdateData();
}

/*======================
R_UpdateData
======================*/
void R_UpdateData ( void )
{
	char name [MAX_VAR];
	int i, j;
	technology_t *t;
	for ( i=0, j=0; i < numTechnologies; i++ ) { //TODO: Debug what happens if there are more than 28 items (j>28) !
		t = &technologies[i];
		R_GetName( t->id, name );
		switch ( t->statusResearch ) // Set the text of the research items and mark them if they are currently researched.
		{
		case RS_RUNNING:
			strcat(name, " [under research]");	//TODO: colorcode "string txt_item%i" ?
			break;
		case RS_FINISH:
			strcat(name, " [finished]");
			break;
		case RS_PAUSED:
			strcat(name, " [paused]");
			break;
		case RS_NONE:
			break;
		default:
			break;
		}
		Cvar_Set( va("mn_researchitem%i", j),  name ); //TODO: colorcode maybe?
		researchList[j] = i;
		j++;
	}
	
	researchListLength = j;

	// Set rest of the list-entries to have no text at all. TODO: 28 should not be defined here.
	for ( ; j < 28; j++ )
		Cvar_Set( va( "mn_researchitem%i", j ), "" );

	// select first item that needs to be researched
	if ( researchListLength )
	{
		if (globalResearchNum) {
			Cbuf_AddText( va("researchselect%i\n",globalResearchNum) );
			CL_ItemDescription( researchList[globalResearchNum] );
			globalResearchNum = researchList[globalResearchNum];
		}else {
			Cbuf_AddText( "researchselect0\n" );
			CL_ItemDescription( researchList[0] );
			globalResearchNum = researchList[0];
		}
	} else {
		// reset description
		Cvar_Set( "mn_researchitemname", "" );
		Cvar_Set( "mn_researchitem", "" );
		Cvar_Set( "mn_researchweapon", "" );
		Cvar_Set( "mn_researchammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
	R_ResearchDisplayInfo();
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

	if ( ! researchListLength )
		return;

	technology_t *t;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( t->statusResearch == RS_RUNNING )
		{
			if ( t->time <= 0 )
			{
				if ( ! newResearch )
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("Research of %s finished\n"), t->id ); //TODO using id for now until R_GetName is fully working.
				else
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("%i researches finished\n"), newResearch+1 );
				// leave all other flags - maybe useful for statistic?
				t->statusResearch = RS_FINISH;
				globalResearchNum = 0;
				newResearch++;
			}
			else
			{
				// FIXME: Make this depending on how many scientists are hired
				// TODO: What time is stored here exactly? the formular below may be way of.
				//od->researchTime -= B_HowManyPeopleInBase2 ( baseCurrent, 1 );
				t->time--;					// DEBUG
				Com_Printf( _("%i\n"), t->time );	// DEBUG
				Com_Printf( _("time reduced\n") );	// DEBUG
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
	int i, j;
	technology_t* t;
	research_requirements_t* req;
	research_requirements_t* req_temp;
	for ( i = 0; i < numTechnologies; i++ )
	{
		t = &technologies[i];
		req = &t->requires;
		Com_Printf(_("Tech: %s\n"), t->id );
		Com_Printf(_("... time      -> %.2f\n"), t->time );
		Com_Printf(_("... name      -> %s\n"), t->name );
		Com_Printf(_("... requires  ->"));
		for ( j = 0; j < req->numEntries; j++ )
			Com_Printf( _(" %s"),req->list[j] );
		Com_Printf("\n");
		Com_Printf( _("... provides  -> %s"), t->provides );
		Com_Printf("\n");
		
		Com_Printf( _("... type      -> "));
		switch ( t->type )
		{
		case RS_TECH:	Com_Printf(_("tech\n") ); break;
		case RS_WEAPON:	Com_Printf(_("weapon\n") ); break;
		case RS_CRAFT:	Com_Printf(_("craft\n") ); break;
		case RS_ARMOR:	Com_Printf(_("armor\n") ); break;
		case RS_BUILDING:	Com_Printf(_("building\n") ); break;
		default:	break;
		}
		//Com_Printf(_("... Researched %i\n"), t->statusResearch );
		Com_Printf(_("... Collected -> %i\n"), t->statusCollected );
		
#if 0
		// FIXME: This is not initialized and produces a segfault
		req_temp->numEntries = 0;
		R_GetFirstRequired( t->id, req_temp );
		Com_Printf(_("... req_first ->"));
		for ( j = 0; j < req_temp->numEntries; j++ )
			Com_Printf( _(" %s"),req_temp->list[j] );
#endif
		
		Com_Printf("\n");
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
	Cmd_AddCommand( "techlist", R_TechnologyList_f );	// DEBUGGING ONLY
}

// NOTE: the BSFS define is the same like for bases and so on...
value_t valid_tech_vars[] =
{
	//name of technology
	{ "name",	V_STRING,	TECHFS( name ) },

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
void MN_ParseTechnologies ( char* id, char** text )
{
	value_t *var;
	technology_t *t;
	char	*errhead = _("MN_ParseTechnologies: unexptected end of file (names ");
	char	*token, *misp;
	char	single_requirement[MAX_VAR]; //256 ?
	research_requirements_t* required;

	// get body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseTechnologies: technology def \"%s\" without body ignored\n"), id );
		return;
	}
	if ( numTechnologies >= MAX_TECHNOLOGIES )
	{
		Com_Printf( _("MN_ParseTechnologies: too many technology entries\n"), id );
		return;
	}
	
	// new technology
	t = &technologies[numTechnologies++];
	required = &t->requires;
	memset( t, 0, sizeof( technology_t ) );

	//set standard values
	strcpy( t->id, id );
	strcpy( t->name, "" );
	strcpy( t->provides, "" );
	t->type = RS_TECH;
	t->statusResearch = RS_NONE;
	t->statusCollected  = false;
	

	do {
		// get the name type
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		// get values
		for ( var = valid_tech_vars; var->string; var++ )
			if ( !strcmp( token, var->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, id );
				if ( !*text ) return;

				if ( var->ofs && var->type != V_NULL )
					Com_ParseValue( t, token, var->type, var->ofs );
				else
					// NOTE: do we need a buffer here? for saving or something like that?
					Com_Printf(_("MN_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed\n"));
				break;
			}else
			if (  !strcmp( token, "type" ) ) {
				token = COM_EParse( text, errhead, id );
				if ( !*text ) return;
				if (  !strcmp( token, "tech" ) )	t->type = RS_TECH; // redundant, but oh well.
				else if ( !strcmp( token, "weapon" ) )	t->type = RS_WEAPON;
				else if ( !strcmp( token, "armor" ) )	t->type = RS_ARMOR;
				else if ( !strcmp( token, "craft" ) )		t->type = RS_CRAFT;
				else if ( !strcmp( token, "building" ) )	t->type = RS_BUILDING;
				else Com_Printf(_("MN_ParseTechnologies - Unknown techtype: \"%s\" - ignored.\n"), token);
			}else if ( !strcmp( token, "requires" ) )
			{
				token = COM_EParse( text, errhead, id );
				if ( !*text ) return;
				strncpy( single_requirement, token, MAX_VAR );
				single_requirement[strlen(single_requirement)] = 0;
				misp = single_requirement;

				required->numEntries = 0;
				do {
					token = COM_Parse( &misp );
					if ( !misp ) break;
					strncpy( required->list[required->numEntries++], token, MAX_VAR);
					if ( required->numEntries == MAX_TECHLINKS )
						Com_Printf( _("MN_ParseTechnologies: Too many \"required\" defined. Limit is %i - ignored\n"), MAX_TECHLINKS );
				}
				while ( misp && required->numEntries < MAX_TECHLINKS );
				continue;
			}
		if ( !var->string ) //TODO: escape "type weapon/tech/etc.." here
			Com_Printf( _("MN_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n"), token, id );

	} while ( *text );

}


/*
======================
R_GetRequired

returns the list of required items.
TODO: out of order ... seems to produce garbage
======================

void R_GetRequired( char *id, research_requirements_t *required)
{
	int i;
	technology_t *t;
	
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			required = &t->requires;	// is linking a good idea?
			return;
		}
	}
	Com_Printf( _("R_GetRequired: technology \"%s\" not found.\n"), id );
}
*/

/*
======================
R_TechIsResearched

Checks if the research item has been researched
======================
*/
byte R_TechIsResearched(char *id )
{
	int i;
	technology_t *t;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			if ( t->statusResearch == RS_FINISH )
				return true;
			return false;
		}
	}
	Com_Printf( _("R_TechIsResearched: research item \"%s\" not found.\n"), id );
	return false;	
}

/*
======================
R_ItemIsResearched

Checks if the research item has been researched
======================
*/
byte R_ItemIsResearched(char *id_provided )
{
	int i;
	technology_t *t;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id_provided, t->provides ) ) {
			if ( t->statusResearch == RS_FINISH )
				return true;
			return false;
		}
	}
	return true;	// no research needed
}

/*
======================
R_GetFirstRequired

Returns the first required (yet unresearched) technologies that are needed by "id".
That means you need to research the result to be able to research (and maybe use) "id".

TODO: check if the tech is already researched and add the previous instead.
======================
*/

void R_GetFirstRequired( char *id,  research_requirements_t *required)
{
	int i, j;
	technology_t *t;
	research_requirements_t	*required_temp;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			required_temp = &t->requires;
			for ( j=0; j < required_temp->numEntries; j++ ) {
				if ( ( 0 == j ) && ( ( !strcmp( required_temp->list[0], "initial" ) ) || ( !strcmp( required_temp->list[0] , "nothing" ) ) ) ) {
					if ( required->numEntries < MAX_TECHLINKS ) {
						strcpy( required->list[required->numEntries], id ); // copy _this_ tech to the list
						required->numEntries++;
						//Com_Printf( _("debug: 'initial' or 'nothing' found - \"%s\"\n"), t->id ); //DEBUG
					}
					return;
				}
				if ( R_TechIsResearched(required_temp->list[j]) ) {
					strcpy( required->list[required->numEntries], id ); // copy _this_ tech to the list
					required->numEntries++;
					//Com_Printf( _("debug: next item \"%s\" already researched . \"%s\"."), required_temp->list[j], t->id ); //DEBUG
				} else {
					R_GetFirstRequired( required_temp->list[j], required );
				}
			}
			return;
		}
	}
	Com_Printf( _("R_GetFirstRequired: technology \"%s\" not found.\n"), id );
}

/*
======================
R_DependsOn

Checks if the research item id1 depends on (requires) id2
======================
*/
byte R_DependsOn(char *id1, char *id2)
{
	int i, j;
	technology_t *t;
	research_requirements_t	required;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id1, t->id ) ) {
			required = t->requires;
			for ( j=0; j < required.numEntries; j++ ) {
				if ( !strcmp(required.list[j], id2 ) )
					return true;
			}
			return false;
		}
	}
	Com_Printf( _("R_DependsOn: research item \"%s\" not found.\n"), id1 );
	return false;	
}

/*
======================
R_GetProvided

Returns a list of .ufo items that are produceable when this item has been researched (=provided)
This list also incldues other items that "require" this one (id) and have a reseach_time of 0.

// TODO: MAX_RESLINK can exceed it's limit, since the added entries to provided can get longer.
======================
*/
void R_GetProvided( char *id, char *provided[MAX_TECHLINKS])
{
	int i, j;
	technology_t *t;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			for ( j=0; j < MAX_TECHLINKS; j++ )
				strcpy(provided[j], t->provides);
			//TODO: search for dependent items.
			for ( j=0; j < numTechnologies; j++ ) {
				if (R_DependsOn( t->id, id ) ) {
					// TODO: append researchtree[j]->provided to *provided 
				}
			}
			return;
		}
	}
	Com_Printf( _("R_GetProvided: research item \"%s\" not found.\n"), id );
}

/*
======================
R_MarkResearched

Mark technologies as researched. This includes techs that deoends in "id" and have time=0
======================
*/
void R_MarkResearched( char *id )
{
	int i;
	technology_t *t;
	// set depending technologies that have time=0 as researeched as well
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) )
			t->statusResearch = RS_FINISH;
		else
		if ( R_DependsOn( id, t->id ) && (t->time <= 0) ) {
			t->statusResearch = RS_FINISH;
			Com_Printf( _("Research of \"%s\" finished.\n"), id );
		}
	}
}
