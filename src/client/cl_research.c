// cl_research.c

#include "client.h"
#include "cl_research.h"


/* 
Note: 
ObjDef_t in q_shared.h holds the researchNeeded and researchDone flags
cl_actor.c: We need to check wheter the flags are set before shooting or reloading
*/

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
