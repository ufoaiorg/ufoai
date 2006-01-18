#ifndef BASEMANGEMENT_DEFINED
#define BASEMANGEMENT_DEFINED 1

#define	BSFS(x)	(int)&(((building_t *)0)->x)

#define	PRODFS(x)	(int)&(((production_t *)0)->x)

#define REPAIRTIME 14
#define REPAIRCOSTS 10

#define UPGRADECOSTS 100
#define UPGRADETIME  2

void MN_BuildingInit( void );
int B_GetIDFromList ( int id );

void MN_SetBuildingByClick ( int x, int y );
void MN_DrawBase( void );
void MN_ResetBaseManagement( void );

#endif /* BASEMANGEMENT_DEFINED */

