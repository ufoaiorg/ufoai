// cl_research.c

#include "client.h"
#include "cl_research.h"

byte researchList[MAX_RESEARCHLIST];
int researchListLength;


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
	if ( num > researchListLength )
		return;

	Cbuf_AddText( va( "researchselect%i\n", num ) );
	CL_ItemDescription( researchList[num] );
}

/*======================
CL_ResearchStart
======================*/
void CL_ResearchStart ( void )
{
}

/*======================
CL_ResearchStop
======================*/
void CL_ResearchStop ( void )
{
}

/*======================
MN_ResearchType
======================*/
void CL_ResearchType ( void )
{
	objDef_t *od;
	int i, j;
	char str[MAX_VAR];

	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );

	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( od->researchNeeded && ! od->researchDone )
		{
			sprintf( str, "mn_item%i", j );		Cvar_Set( str, od->name );
			sprintf( str, "mn_storage%i", j );	Cvar_SetValue( str, ccs.eCampaign.num[i] );
			sprintf( str, "mn_supply%i", j );	Cvar_SetValue( str, ccs.eMarket.num[i] );
			// TODO: the price must differ from buying the weapon
			sprintf( str, "mn_price%i", j );	Cvar_Set( str, va( "%i $", od->price ) );
			researchList[j] = i;
			j++;
		}

	researchListLength = j;

	for ( ; j < 28; j++ )
	{
		Cvar_Set( va( "mn_item%i", j ), "" );
		Cvar_Set( va( "mn_storage%i", j ), "" );
		Cvar_Set( va( "mn_supply%i", j ), "" );
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
	researchListLength = -1;
	// add commands and cvars
	Cmd_AddCommand( "research_init", MN_ResearchInit );
	Cmd_AddCommand( "research_select", CL_ResearchSelectCmd );
	Cmd_AddCommand( "research_type", CL_ResearchType );
	Cmd_AddCommand( "mn_start_research", CL_ResearchStart );
	Cmd_AddCommand( "mn_stop_research", CL_ResearchStop );
}
