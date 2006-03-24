// cl_research.h
#ifndef TECHNOLOGY_DEFINED
#define TECHNOLOGY_DEFINED 1

#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256

typedef struct technology_s
{
	char	title[MAX_VAR]; // internal id
	char	name[MAX_VAR];
	float	time;
	char	provides[MAX_VAR];
	char	requires[MAX_VAR];
	char	description[MAX_VAR];
	char	type[MAX_VAR];
} technology_t;

#define	TECHFS(x)	(int)&(((technology_t *)0)->x)

// use this for saving and allocating
extern technology_t technologies[MAX_TECHNOLOGIES];

void MN_ResetResearch( void );
void CL_CheckResearchStatus( void );
void R_UpdateData ( void );
void MN_ParseTechnologies ( char* title, char** text );

#endif /*TECHNOLOGY_DEFINED*/
