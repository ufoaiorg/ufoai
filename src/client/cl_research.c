// cl_research.c

#include "client.h"
#include "cl_research.h"

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

	// add commands and cvars
	Cmd_AddCommand( "research_init", MN_ResearchInit );
}
