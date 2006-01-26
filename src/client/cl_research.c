// cl_research.c

#include "client.h"
#include "cl_research.h"

byte researchList[MAX_RESEARCHLIST];
int researchListLength;
int globalResearchNum;

/*
Note:
ObjDef_t in q_shared.h holds the researchNeeded and researchDone flags
cl_actor.c: We need to check wheter the flags are set before shooting or reloading
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

	Cbuf_AddText( va( "researchselect%i\n", num ) );
	CL_ItemDescription( researchList[num] );
	globalResearchNum = num;
}

/*======================
CL_ResearchStart

TODO: Check if laboratory is available
======================*/
void CL_ResearchStart ( void )
{
	objDef_t *od;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	od = &csi.ods[globalResearchNum];
	od->researchStatus = RS_RUNNING;
}

/*======================
CL_ResearchStop

TODO: Check if laboratory is available
======================*/
void CL_ResearchStop ( void )
{
	objDef_t *od;

	// we are not in base view
	if ( ! baseCurrent )
		return;

	od = &csi.ods[globalResearchNum];
	od->researchStatus = RS_NONE;
}

/*======================
MN_ResearchType
======================*/
void CL_ResearchType ( void )
{
	objDef_t *od;
	int i, j;

	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );

	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( od->researchNeeded && od->researchStatus != RS_FINISH )
		{
			Cvar_Set( va("mn_item%i", j), od->name );
			// TODO: the price must differ from buying the weapon
			Cvar_Set( va("mn_price%i", j), va( "%i $", od->price ) );
			researchList[j] = i;
			j++;
		}

	researchListLength = j;

	for ( ; j < 28; j++ )
	{
		Cvar_Set( va( "mn_item%i", j ), "" );
		Cvar_Set( va( "mn_price%i", j ), "" );
	}

	// select first item that needs to be researched
	if ( researchListLength )
	{
		Cbuf_AddText( "researchselect0\n" );
		CL_ItemDescription( researchList[0] );
	} else {
		// reset description
		Cvar_Set( "mn_itemname", "" );
		Cvar_Set( "mn_item", "" );
		Cvar_Set( "mn_weapon", "" );
		Cvar_Set( "mn_ammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
}

/*======================
CL_CheckResearchStatus

TODO: This needs to be called once a day/hour??
======================*/
void CL_CheckResearchStatus ( void )
{
	int i;
	objDef_t *od;

	if ( ! researchListLength )
		return;

	for ( i = 0; i < researchListLength; i++ )
	{
		od = &csi.ods[i];
		// FIXME: Make this depending on how many scientists are hired
		if ( od->researchStatus == RS_RUNNING && 1 )
		{
			// leave all other flags - maybe useful for statistic?
			od->researchStatus = RS_FINISH;
		}
	}
}

/*======================
MN_ResearchInit
======================*/
void MN_ResearchInit( void )
{

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
	Cmd_AddCommand( "mn_start_research", CL_ResearchStart );
	Cmd_AddCommand( "mn_stop_research", CL_ResearchStop );
}
