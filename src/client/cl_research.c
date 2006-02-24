// cl_research.c

#include "client.h"
#include "cl_research.h"

byte researchList[MAX_RESEARCHLIST];
int researchListLength;
int globalResearchNum;
char infoResearchText[MAX_MENUTEXTLEN];

/*
Note:
ObjDef_t in q_shared.h holds the researchNeeded and researchStatus flags
*/

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
	od->researchStatus = RS_RUNNING;
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
	od->researchStatus = RS_NONE;
	R_UpdateData();
}

/*======================
R_UpdateData
======================*/
void R_UpdateData ( void )
{
	objDef_t *od;
	int i, j;

	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( od->researchNeeded && od->researchStatus == RS_NONE )
		{
			Cvar_Set( va("mn_researchitem%i", j), od->name );
			researchList[j] = i;
			j++;
		}
	researchListLength = j;

	for ( ; j < 28; j++ )
		Cvar_Set( va( "mn_researchitem%i", j ), "" );

	// select first item that needs to be researched
	if ( researchListLength )
	{
		Cbuf_AddText( "researchselect0\n" );
		CL_ItemDescription( researchList[0] );
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
	int i;

	// get the list
	R_UpdateData();

	// nothing to research here
	if ( ! researchListLength )
		Cbuf_AddText("mn_pop");
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
				od->researchTime--;
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
	Cmd_AddCommand( "research_update", R_UpdateData );}
