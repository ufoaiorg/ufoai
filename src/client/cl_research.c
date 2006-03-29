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

// cl_research.c

#include "client.h"
#include "cl_research.h"
#include "cl_ufopedia.h"

void RS_GetFirstRequired( char *id,  stringlist_t *required);
byte RS_TechIsResearchable(char *id );
byte RS_TechIsResearched(char *id );

byte researchList[MAX_RESEARCHLIST];
int researchListLength;
int globalResearchNum;
char infoResearchText[MAX_MENUTEXTLEN];

technology_t technologies[MAX_TECHNOLOGIES];
int numTechnologies;

/*====================== NOT USED RIGHT NOW
RS_ResearchPossible

checks if there is at least one base with a lab and scientists available and tells the player the status.
======================
char RS_ResearchPossible( void )
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
*/

/*======================
RS_MarkOneCollected

Marks one tech if an item it 'provides' (= id) has been collected.
======================*/
void RS_MarkOneCollected ( char *id )
{
	int i;
	technology_t *t = NULL;


	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( t->provides, id ) ) {
			t->statusCollected = true;
			return;
		}
	}
	Com_Printf( _("RS_MarkOneCollected: Linked item \"%s\" not found.\n"), id );
}

/*======================
RS_MarkCollected

Marks all techs if an item they 'provide' have been collected.
Should be run after items have been collected/looted from the battlefield (cl_campaign.c -> "CL_CollectItems") and after techtree/inventory init (for all).
======================*/
void RS_MarkCollected ( void )
{
	int i;

	for ( i=0; i < MAX_OBJDEFS; i++ ) {
		if ( ccs.eCampaign.num[i] ) {
			RS_MarkOneCollected( csi.ods[i].kurz );
		}
	}
}

/*======================
RS_MarkOneResearchable

Marks one tech as researchedable.
======================*/
void RS_MarkOneResearchable ( char *id )
{
	int i;
	technology_t *t = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( t->id, id ) ) {
			Com_Printf( _("RS_MarkOneResearchable: marked  \"%s\" as researchable.\n"), id );
			t->statusResearchable = true;
			return;
		}
	}
	Com_Printf( _("RS_MarkOneResearchable: technology \"%s\" not found.\n"), id );
}

/*======================
RS_MarkResearchable

Marks all the techs that can be researched.
Should be called when a new item is researched (RS_MarkResearched) and after the tree-initialisation (RS_InitTree)
======================*/
void RS_MarkResearchable( void )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t required;
	byte required_are_researched;


	// set all entries to initial value
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		t->statusResearchable = false;
	}

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !t->statusResearchable ) {	// Redundant, since we set them all to false, but you never know.
			if ( t->statusResearch != RS_FINISH) {
				Com_Printf( _("RS_MarkResearchable: handling %s \n"), t->id );
				required.numEntries = 0;
				RS_GetFirstRequired( t->id,  &required );

				// If the tech has an collected item, mark the first-required techs as researchable //TODO doesn't work yet?
				if ( t->statusCollected ) {
					for ( j=0; j < required.numEntries; j++ ) {
						Com_Printf( _("RS_MarkResearchable: MARKED firstreq %s \n"), required.list[j] );
						RS_MarkOneResearchable( required.list[j] );
					}
				}

				// if previous techs are all researched, mark this as researched as well
				required.numEntries = 0;
				required = t->requires;
				required_are_researched = true;
				for ( j=0; j < required.numEntries; j++ ) {
					if ( !RS_TechIsResearched(required.list[j] ) ) {
						required_are_researched = false;
						break;

					}
				}
				if ( required_are_researched ) {
					Com_Printf( _("RS_MarkResearchable: MARKED required %s \n"), t->id  );
					t->statusResearchable = true;
					//RS_MarkOneResearchable( t->id  );
				}


				// If the tech is an initial one,  mark it as as researchable.

				for ( j=0; j < required.numEntries; j++ ) {

					if ( !strcmp( required.list[j], "initial" ) ) {
						Com_Printf( _("RS_MarkResearchable: marked %s \n"), t->id );
						t->statusResearchable = true;
						break;
					}
				}
			}
		}
	}
	Com_Printf( _("RS_MarkResearchable: Done.\n"));
}

/*======================
RS_InitTree

Gets all needed names/file-paths/etc... for each technology entry.
Should be executed after the parsing of _all_ the ufo files and e.g. the research tree/inventory/etc... are initialised.
======================*/
void RS_InitTree( void )
{
	int i, j;
	technology_t *t = NULL;
	objDef_t *item = NULL;
	building_t	*building = NULL;
	byte	found;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];

		t->overalltime = t->time;	// set the overall reseach time (now fixed) to the one given in the ufo-file.

		// Search in correct data/.ufo
		switch ( t->type )
		{
		case RS_TECH:
			if ( !strcmp( t->name, "" ) )
				Com_Printf( _("RS_InitTree: A 'type tech' item needs to have a 'name\txxx' defined \"%s\"."), t->id );
			break;
		case RS_WEAPON:
		case RS_ARMOR:
			found = false;
			for ( j = 0; j < csi.numODs; j++ ) {
				item = &csi.ods[j];
				if ( !strcmp( t->provides, item->kurz ) ) { // This item has been 'provided',
					found = true;
					if ( !strcmp(t->name, "" ) ) {
						Com_Printf( _("RS_InitTree: found name '%s' and '%s' for %s.\n"),  t->name, item->name,  t->id);
						strcpy( t->name, item->name );
					}
					if ( !strcmp( t->mdl_top, "" ) )
						strcpy( t->mdl_top, item->model );
					if ( !strcmp( t->image_top, "" ) )
						strcpy( t->image_top, item->image );

					break;	// Should return to CASE RS_xxx.
				}

			}
			//no id found in csi.ods
			if ( !found ) {
				Com_Printf( _("RS_InitTree: Linked weapon or armor (\"%s\": provided=\"%s\") not found.\n"), t->id, t->provides );
				Com_Printf( _("RS_InitTree: This is most probably an error in the \"provided\" definition in the research.ufo file.\n") );
			}
			break;
		case RS_BUILDING:
			found = false;
			for ( j = 0; j < numBuildings; j++ ) {
				building = &bmBuildings[0][j];
				if ( !strcmp( t->provides, building->name ) ) { // This building has been 'provided',
					found = true;
					if ( !strcmp( t->name, "") )
						strcpy( t->name, building->title );
					if ( !strcmp( t->image_top, "") )
						strcpy( t->image_top, building->image );

					break;	// Should return to CASE RS_xxx.
				}
			}
			if ( !found ) {
				Com_Printf( _("RS_InitTree: Linked building (\"%s\": provided=\"%s\") not found.\n"), t->id, t->provides );
				Com_Printf( _("RS_InitTree: This is most probably an error in the \"provided\" definition in the research.ufo file.\n") );
			}
			break;
		case RS_CRAFT:
			// unused right now.
			break;
		} // switch
	}
	RS_MarkCollected();
	RS_MarkResearchable();
	Com_Printf( _("RS_InitTree: Technology tree initialised. %i entries found.\n"), i );
}

/*======================
RS_GetName

Return "name" if present, otherwise enter the correct .ufo file and get it from the definition there.
======================*/
void RS_GetName( char *id, char *name )
{
	int i;
	technology_t *t = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			// If something is defined as name use it.
			if ( strcmp( t->name, "" ) ) {
				strcpy( name, t->name );
				return;
			} else {
				Com_Printf( _("RS_GetName: No name defined for \"%s\" not found. Name set to id.\n"), id );
				strcpy( name, id );	// set the name to the id.
				return;
			}
		}
	}
	strcpy( name, id );	// set the name to the id.
	Com_Printf( _("RS_GetName: technology \"%s\" not found. Name set to id\n"), id );
}

/*======================
RS_ResearchDisplayInfo
======================*/
void RS_ResearchDisplayInfo ( void  )
{
	technology_t *t = NULL;
	char dependencies[MAX_VAR];
	char tempstring[MAX_VAR];
	int i;
	stringlist_t req_temp;

	t = &technologies[globalResearchNum];

	// we are not in base view
	if ( ! baseCurrent )
		return;

	Cvar_Set( "mn_research_selname",  t->name );
	if ( t->overalltime ) {
		Cvar_Set( "mn_research_seltime", va( "Progress: %.1f\%\n", 100-(t->time*100/t->overalltime) ) );
	} else {
		Cvar_Set( "mn_research_seltime", "" );
	}

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

	req_temp.numEntries = 0;
	RS_GetFirstRequired( t->id, &req_temp );
	strcpy( dependencies, "Dependencies: ");
	if ( req_temp.numEntries > 0 ) {
		for ( i = 0; i < req_temp.numEntries; i++ ) {
			RS_GetName( req_temp.list[i], tempstring ); //name_temp gets initialised in getname
			strcat( dependencies, tempstring );

			if ( i < req_temp.numEntries-1 )
				strcat( dependencies, ", ");
		}
	} else {
		strcpy( dependencies, "" );
	}
	Cvar_Set( "mn_research_seldep", dependencies );


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
	RS_ResearchDisplayInfo();
}

/*======================
RS_ResearchStart

TODO: Check if laboratory is available
======================*/
void RS_ResearchStart ( void )
{
	technology_t *t = NULL;

	// we are not in base view
	if ( ! baseCurrent )
		return;

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
	RS_UpdateData();
}

/*======================
RS_ResearchStop

TODO: Check if laboratory is available
======================*/
void RS_ResearchStop ( void )
{
	technology_t *t = NULL;

	// we are not in base view
	if ( ! baseCurrent )
		return;

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
	RS_UpdateData();
}

/*======================
RS_UpdateData
======================*/
void RS_UpdateData ( void )
{
	char name [MAX_VAR];
	int i, j;
	technology_t *t = NULL;

	for ( i=0, j=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( ( t->statusResearch != RS_FINISH ) && ( t->statusResearchable ) ) { //(  ( t->statusResearch != RS_FINISH ) && ( RS_TechIsResearchable( t->id ) ) ) {
			strcpy( name, t->name );
			switch ( t->statusResearch ) // Set the text of the research items and mark them if they are currently researched.
			{
			case RS_RUNNING:
				strcat(name, " [under research]");	//TODO: colorcode "string txt_item%i" ?
				break;
			case RS_FINISH:
				strcat(name, " [finished]");	// DEBUG: normaly these will not be shown at all. see "if" above
				break;
			case RS_PAUSED:
				strcat(name, " [paused]");
				break;
			case RS_NONE:
				strcat(name, " [unknown]");	// DEBUG
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

	// Set rest of the list-entries to have no text at all.
	for ( ; j < MAX_RESEARCHDISPLAY; j++ )
		Cvar_Set( va( "mn_researchitem%i", j ), "" );

	// select first item that needs to be researched
	if ( researchListLength ) {
		/* TODO: Fix this mess. globalResearchNum and really selected entry are not the same.
		if (globalResearchNum && ( researchListLength < MAX_RESEARCHDISPLAY ) ) {
			Cbuf_AddText( va("researchselect%i\n",globalResearchNum) );
			CL_ItemDescription( researchList[globalResearchNum] );
			globalResearchNum = researchList[globalResearchNum];
		}else {*/
			Cbuf_AddText( "researchselect0\n" );
			CL_ItemDescription( researchList[0] );
			globalResearchNum = researchList[0];
		//}
	} else {
		// reset description
		Cvar_Set( "mn_researchitemname", "" );
		Cvar_Set( "mn_researchitem", "" );
		Cvar_Set( "mn_researchweapon", "" );
		Cvar_Set( "mn_researchammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
	RS_ResearchDisplayInfo();
}

/*======================
CL_ResearchType
======================*/
void CL_ResearchType ( void )
{
	// get the list
	RS_UpdateData();

	// nothing to research here
	if ( ! researchListLength || !ccs.numBases )
		Cbuf_ExecuteText( EXEC_NOW, "mn_pop" );
	else if ( baseCurrent && ! baseCurrent->hasLab )
		MN_Popup( _("Notice"), _("Build a laboratory first") );
}

/*======================
RS_DependsOn

Checks if the research item id1 depends on (requires) id2
======================*/
byte RS_DependsOn(char *id1, char *id2)
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t	required;

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
	Com_Printf( _("RS_DependsOn: research item \"%s\" not found.\n"), id1 );
	return false;
}

/*======================
RS_MarkResearched

Mark technologies as researched. This includes techs that depends in "id" and have time=0
======================*/
void RS_MarkResearched( char *id )
{
	int i;
	technology_t *t = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			t->statusResearch = RS_FINISH;
			Com_Printf( _("Research of \"%s\" finished.\n"), t->id );
		}
		else if ( RS_DependsOn( t->id, id ) && (t->time <= 0) ) {
			t->statusResearch = RS_FINISH;
			Com_Printf( _("Depending tech \"%s\" has been researched as well.\n"),  t->id );
		}
	}
	RS_MarkResearchable();
	RS_MarkResearchable();
}

/*======================
CL_CheckResearchStatus
======================*/
void CL_CheckResearchStatus ( void )
{
	int i, newResearch = 0;
	technology_t *t = NULL;

	if ( ! researchListLength )
		return;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( t->statusResearch == RS_RUNNING )
		{
			if ( t->time <= 0 ) {
				if ( ! newResearch )
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("Research of %s finished\n"), t->name );
				else
					Com_sprintf( infoResearchText, MAX_MENUTEXTLEN, _("%i researches finished\n"), newResearch+1 );
				RS_MarkResearched( t->id );
				globalResearchNum = 0;
				newResearch++;
			} else {
				// TODO/FIXME: Make this depending on how many scientists are hired
				// t->time -= pow( 1.2, t->lab->assignedWorkers );	// The 1.2 may need some finetuning (do not use values lower that 1). A DEFINE for it might also be a good idea.
				// or
				// t->time -= pow( t->lab->assignedWorkers, 1,1 ) - (  t->lab->assignedWorkers / 4 );
				t->time--;		// reduce one time-unit
			}
		}
	}

	if ( newResearch ) {
		MN_Popup( _("Research finished"), infoResearchText );
		CL_ResearchType();
	}
}

/*======================
RS_TechnologyList_f

list all parsed technologies
======================*/
void RS_TechnologyList_f ( void )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t *req = NULL;
	stringlist_t req_temp;


	for ( i = 0; i < numTechnologies; i++ )
	{
		t = &technologies[i];
		req = &t->requires;
		Com_Printf("Tech: %s\n", t->id );
		Com_Printf("... time      -> %.2f\n", t->time );
		Com_Printf("... name      -> %s\n", t->name );
		Com_Printf("... requires  ->");
		for ( j = 0; j < req->numEntries; j++ )
			Com_Printf( " %s",req->list[j] );
		Com_Printf("\n");
		Com_Printf( "... provides  -> %s", t->provides );
		Com_Printf("\n");

		Com_Printf( "... type      -> ");
		switch ( t->type )
		{
		case RS_TECH:	Com_Printf("tech"); break;
		case RS_WEAPON:	Com_Printf("weapon"); break;
		case RS_CRAFT:	Com_Printf("craft"); break;
		case RS_ARMOR:	Com_Printf("armor"); break;
		case RS_BUILDING:	Com_Printf("building"); break;
		default:	break;
		}
		Com_Printf("\n");

		Com_Printf( "... research  -> ");
		switch ( t->type )
		{
		case RS_NONE:	Com_Printf("unknown tech"); break;
		case RS_RUNNING:	Com_Printf("running"); break;
		case RS_PAUSED:	Com_Printf("paused"); break;
		case RS_FINISH:	Com_Printf("done"); break;
		default:	break;
		}
		Com_Printf("\n");

		Com_Printf("... Res.able  -> %i\n", t->statusResearchable );
		Com_Printf("... Collected -> %i\n", t->statusCollected );
		

		Com_Printf("... req_first ->");
		req_temp.numEntries = 0;
		RS_GetFirstRequired( t->id, &req_temp );
		for ( j = 0; j < req_temp.numEntries; j++ )
			Com_Printf( " %s", req_temp.list[j] );

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
	Cmd_AddCommand( "mn_start_research", RS_ResearchStart );
	Cmd_AddCommand( "mn_stop_research", RS_ResearchStop );
	Cmd_AddCommand( "research_update", RS_UpdateData );
	Cmd_AddCommand( "technologylist", RS_TechnologyList_f );
	Cmd_AddCommand( "techlist", RS_TechnologyList_f );	// DEBUGGING ONLY
	Cmd_AddCommand( "invlist", Com_InventoryList_f );
}

// NOTE: the BSFS define is the same like for bases and so on...
value_t valid_tech_vars[] =
{
	{ "name",		V_STRING,	TECHFS( name ) },		//name of technology
	//{ "up_chapter",	V_STRING,	TECHFS( name ) },
	{ "description",	V_STRING,	TECHFS( description ) },
	{ "provides",		V_STRING,	TECHFS( provides ) },	//what does this research provide
	{ "time",			V_FLOAT,		TECHFS( time ) },				//how long will this research last
	{ "image_top"	,	V_STRING,	TECHFS( image_top ) },
	{ "image_bottom",	V_STRING,	TECHFS( image_bottom ) },
	{ "mdl_top",		V_STRING,	TECHFS( mdl_top ) },
	{ "mdl_bottom",	V_STRING,	TECHFS( mdl_bottom ) },
	{ NULL,	0, 0 }
};

/*======================
RS_ParseTechnologies
======================*/
void RS_ParseTechnologies ( char* id, char** text )
{
	value_t *var = NULL;
	technology_t *t = NULL;
	char	*errhead = _("RS_ParseTechnologies: unexptected end of file (names ");
	char	*token = NULL;
	char *misp = NULL;
	char	temp_text[MAX_VAR]; //256 ?
	stringlist_t *required = NULL;

	// get body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("RS_ParseTechnologies: technology def \"%s\" without body ignored\n"), id );
		return;
	}
	if ( numTechnologies >= MAX_TECHNOLOGIES )
	{
		Com_Printf( _("RS_ParseTechnologies: too many technology entries\n"), id );
		return;
	}

	// new technology
	t = &technologies[numTechnologies++];
	required = &t->requires;
	memset( t, 0, sizeof( technology_t ) );

	//set standard values
	strcpy( t->id, id );
	strcpy( t->name, "" );
	//strcpy( t->up_chapter, "tech" ); //default chapter
	strcpy( t->provides, "" );
	strcpy( t->image_top, "" );
	strcpy( t->image_bottom, "" );
	strcpy( t->mdl_top, "" );
	strcpy( t->mdl_bottom, "" );
	t->type = RS_TECH;
	t->statusResearch = RS_NONE;
	t->statusResearchable = false;
	t->statusCollected  = false;
	t->time = 0;
	t->overalltime = 0;

	do {
		// get the name type
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		// get values
		if (  !strcmp( token, "type" ) ) {
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			if (  !strcmp( token, "tech" ) )	t->type = RS_TECH; // redundant, but oh well.
			else if ( !strcmp( token, "weapon" ) )	t->type = RS_WEAPON;
			else if ( !strcmp( token, "armor" ) )	t->type = RS_ARMOR;
			else if ( !strcmp( token, "craft" ) )		t->type = RS_CRAFT;
			else if ( !strcmp( token, "building" ) )	t->type = RS_BUILDING;
			else Com_Printf(_("RS_ParseTechnologies - Unknown techtype: \"%s\" - ignored.\n"), token);
		}
		else
		if ( !strcmp( token, "requires" ) )
		{
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			strncpy( temp_text, token, MAX_VAR );
			temp_text[strlen(temp_text)] = 0;
			misp = temp_text;

			required->numEntries = 0;
			do {
				token = COM_Parse( &misp );
				if ( !misp ) break;
				strncpy( required->list[required->numEntries++], token, MAX_VAR);
				if ( required->numEntries == MAX_TECHLINKS )
					Com_Printf( _("RS_ParseTechnologies: Too many \"required\" defined. Limit is %i - ignored\n"), MAX_TECHLINKS );
			}
			while ( misp && required->numEntries < MAX_TECHLINKS );
			continue;
		}
		else
		if ( !strcmp( token, "researched" ) )
		{
			token = COM_EParse( text, errhead, id );
			if ( !strcmp( token, "true" ) || !strcmp( token, "1" ) )
				t->statusResearch = RS_FINISH;
		}
		else
		if ( !strcmp( token, "up_chapter" ) ) {
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;

			if ( strcmp( token, "" ) ) {
				// find chapter
				int i;
				for ( i = 0; i < numChapters; i++ )
					if ( !strcmp( upChapters[i].id, token ) ) {
						// add entry to chapter
						t->up_chapter = &upChapters[i];
						if ( !upChapters[i].first )	{
							upChapters[i].first = t;
							upChapters[i].last = t;
						} else {
							technology_t *old;
							upChapters[i].last = t;
							old = upChapters[i].first;
							while ( old->next ) old = old->next;
							old->next = t;
							t->prev = old;
						}
						break;
					}
				if ( i == numChapters )
					Com_Printf( _("RS_ParseTechnologies: chapter \"%s\" not found (entry %s)\n"), token, id );
			}
		}
		else
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
					Com_Printf(_("RS_ParseTechnologies Error: - no buffer for technologies - V_NULL not allowed\n"));
				break;
			}
		//TODO: debug this thing, it crashes the game at startup.
		//if ( !var->string ) //TODO: escape "type weapon/tech/etc.." here
		//	Com_Printf( _("RS_ParseTechnologies: unknown token \"%s\" ignored (entry %s)\n"), token, id );

	} while ( *text );
}

/*======================
RS_GetRequired

returns the list of required items.
TODO: out of order ... seems to produce garbage
======================*/

void RS_GetRequired( char *id, stringlist_t *required)
{
	int i;
	technology_t *t = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			required = &t->requires;	// is linking a good idea?
			return;
		}
	}
	Com_Printf( _("RS_GetRequired: technology \"%s\" not found.\n"), id );
}

/*======================
RS_ItemIsResearched

Checks if the item (as listed in "provides") has been researched
======================*/
byte RS_ItemIsResearched(char *id_provided )
{
	int i;
	technology_t *t = NULL;
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

/*======================
RS_TechIsResearched

Checks if the technology (tech-id) has been researched
======================*/
byte RS_TechIsResearched(char *id )
{
	int i;
	technology_t *t = NULL;
	if ( ( !strcmp( id, "initial" ) ) || ( !strcmp( id, "nothing" ) ) )
		return true;	// initial and nothing are always researchs. as they are just a starting "technology" that is never used.
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			if ( t->statusResearch == RS_FINISH )
				return true;
			return false;
		}
	}

	Com_Printf( _("RS_TechIsResearched: research item \"%s\" not found.\n"), id );
	return false;
}

/*======================
RS_TechIsResearchable

Checks if the technology (tech-id) is researchable.
======================*/
byte RS_TechIsResearchable(char *id )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t* required = NULL;

	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			if ( t->statusResearch == RS_FINISH )
				return false;
			if ( ( !strcmp(  t->id, "initial" ) ) || ( !strcmp(  t->id, "nothing" ) ) )
				return true;
			required = &t->requires;
			for (j=0; j < required->numEntries; j++)  {
				if ( !RS_TechIsResearched( required->list[j]) )	// Research of "id" not finished (RS_FINISH) at this point.
					return false;
			}
			return true;
		}
	}
	Com_Printf( _("RS_TechIsResearchable: research item \"%s\" not found.\n"), id );
	return false;
}

/*======================
RS_GetFirstRequired + RS_GetFirstRequired2

Returns the first required (yet unresearched) technologies that are needed by "id".
That means you need to research the result to be able to research (and maybe use) "id".
======================*/
void RS_GetFirstRequired2 ( char *id, char *first_id,  stringlist_t *required )
{
	int i, j;
	technology_t *t = NULL;
	stringlist_t *required_temp = NULL;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];

		if ( !strcmp( id, t->id ) ) {
			required_temp = &t->requires;
			//Com_Printf( "RS_GetFirstRequired2: %s - %s \n", id, first_id ); //DEBUG
			//Com_Printf( "RS_GetFirstRequired2: %s\n",  required_temp->list[0] ); //DEBUG
			for ( j=0; j < required_temp->numEntries; j++ ) {
				if ( ( !strcmp( required_temp->list[0] , "initial" ) ) || ( !strcmp( required_temp->list[0] , "nothing" ) ) ) {
					if ( !strcmp( id, first_id ) )
						return;
					if ( 0 == j ) {
						if ( required->numEntries < MAX_TECHLINKS ) {
							strcpy( required->list[required->numEntries], id );
							required->numEntries++;
							//Com_Printf("RS_GetFirstRequired2: 'initial' or 'nothing' found - \"%s\"\n", t->id ); //DEBUG
						}
						return;
					}
				}
				if ( RS_TechIsResearched(required_temp->list[j]) ) {
					strcpy( required->list[required->numEntries], required_temp->list[j] );
					required->numEntries++;
					//Com_Printf( "RS_GetFirstRequired2: next item \"%s\" already researched  \"%s\"\n", required_temp->list[j], t->id ); //DEBUG
				} else {
					RS_GetFirstRequired2( required_temp->list[j], first_id, required );
				}
			}
			return;
		}
	}
	if ( !strcmp( id, first_id ) )
		Com_Printf( _("RS_GetFirstRequired: technology \"%s\" not found.\n"), id );
}

void RS_GetFirstRequired( char *id,  stringlist_t *required )
{
	 RS_GetFirstRequired2( id, id, required);
}

/*======================
RS_GetProvided

Returns a list of .ufo items that are produceable when this item has been researched (=provided)
This list also incldues other items that "require" this one (id) and have a reseach_time of 0.
======================
void RS_GetProvided( char *id, char *provided )
{
	int i, j;
	technology_t *t = NULL;
	for ( i=0; i < numTechnologies; i++ ) {
		t = &technologies[i];
		if ( !strcmp( id, t->id ) ) {
			for ( j=0; j < MAX_TECHLINKS; j++ )
				strcpy(provided[j], t->provides);
			//TODO: search for dependent items.
			for ( j=0; j < numTechnologies; j++ ) {
				if (RS_DependsOn( t->id, id ) ) {
					// TODO: append researchtree[j]->provided to *provided
				}
			}
			return;
		}
	}
	Com_Printf( _("RS_GetProvided: research item \"%s\" not found.\n"), id );
}*/

/*======================
RS_SaveTech
======================*/
void RS_SaveTech( sizebuf_t *sb )
{
	int i;

	MSG_WriteLong( sb, numTechnologies );
	Com_DPrintf(_("Saving %i technologies\n"), numTechnologies );
	for ( i = 0; i < numTechnologies; i++ )
	{
		MSG_WriteByte( sb, technologies[i].statusResearch );
		MSG_WriteByte( sb, technologies[i].statusCollected );
		MSG_WriteFloat( sb, technologies[i].time);
	}
}

/*======================
RS_LoadTech
======================*/
void RS_LoadTech( sizebuf_t *sb, int version )
{
	int tmp, i;
	if ( version >= 3 )
	{
		tmp = MSG_ReadLong( sb );
		if ( tmp != numTechnologies )
			Com_Printf(_("There was an update and there are new technologies available which aren't in your savegame. You may encounter problems. (%i:%i)\n"), tmp, numTechnologies );
		for ( i = 0; i < tmp; i++ )
		{
			technologies[i].statusResearch = MSG_ReadByte( sb );
			technologies[i].statusCollected = MSG_ReadByte( sb );
			technologies[i].time = MSG_ReadFloat( sb );
		}
		
	}
	RS_MarkResearchable ();
}
