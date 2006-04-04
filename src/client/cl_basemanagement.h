#ifndef BASEMANGEMENT_DEFINED
#define BASEMANGEMENT_DEFINED 1

#define	BSFS(x)	(int)&(((building_t *)0)->x)

#define	PRODFS(x)	(int)&(((production_t *)0)->x)

#define REPAIRTIME 14
#define REPAIRCOSTS 10

#define UPGRADECOSTS 100
#define UPGRADETIME  2

#define MAX_PRODUCTIONS		256

typedef struct production_s
{
	char    name[MAX_VAR];
	char    title[MAX_VAR];
	char    *text;
	int	amount;
	char	menu[MAX_VAR];
} production_t;

extern vec2_t newBasePos;

void MN_BuildingInit( void );
int B_GetCount ( void );
void B_SetUpBase ( void );

void MN_ParseProductions( char *title, char **text );
void MN_SetBuildingByClick ( int row, int col );
void MN_DrawBase( void );
void MN_ResetBaseManagement( void );

#endif /* BASEMANGEMENT_DEFINED */

