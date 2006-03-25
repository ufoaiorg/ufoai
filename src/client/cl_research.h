// cl_research.h
#ifndef TECHNOLOGY_DEFINED
#define TECHNOLOGY_DEFINED 1

#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256
#define MAX_TECHLINKS 32

typedef struct technology_s
{
	char	id[MAX_VAR];
	char	name[MAX_VAR];
	float	time;
	char	provides[MAX_TECHLINKS][MAX_VAR];
	char	requires[MAX_TECHLINKS][MAX_VAR];
	char	description[MAX_VAR];
	researchType_t	type;
	researchStatus_t	statusResearch;
	byte	statusCollected;	// Did we loot this item?
	
} technology_t;

#define	TECHFS(x)	(int)&(((technology_t *)0)->x)

// use this for saving and allocating
extern	technology_t	technologies[MAX_TECHNOLOGIES];

void MN_ResetResearch( void );
void CL_CheckResearchStatus( void );
void R_UpdateData ( void );
void MN_ParseTechnologies ( char* title, char** text );

#endif /*TECHNOLOGY_DEFINED*/
