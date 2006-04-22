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

/*======================
cl_basemanagement.c
//
* Handles everything that is located an or accessed trough a base.

See
	base/ufos/basemanagement.ufo
and
	base/ufos/menu_bases.ufo
	base/ufos/menu_buildings.ufo
for the underlying content.

TODO: new game does not reset basemangagement
======================*/

#include "client.h"
#include "cl_basemanagement.h"

base_t	bmBases[MAX_BASES];				// A global list of _all_ bases. (see client.h)
vec2_t	newBasePos;
cvar_t*	mn_base_title;

building_t    bmBuildings[MAX_BASES][MAX_BUILDINGS];	// A global list of _all_ buildings (even unbuilt) in all bases. (see client.h)
int numBuildings;								// The global number of entries in the bmBuildings list (see client.h)

production_t  bmProductions[MAX_PRODUCTIONS];		// A list of _all_ productions. TODO: what exactly is this meant for?
int numProductions;							// Number of entries in the bmProductions list.

employee_t	employees[MAX_EMPLOYEES];			// This it the global list of employees (see client.h)
int   numEmployees;							// The global number of enris in the "employees" list. (see client.h)

//DrawBase
float bvCenterX, bvCenterY;
float bvScale;

int bmDataSize = 0;
char *bmData, *bmDataStart, *bmDataProductions;

/*======================
The valid definition names for BUILDINGS (building_t) in the basemagagement.ufo file.
NOTE: the BSFS macro (see cl_basemanagement.h) assignes the values from scriptfile
to the appropriate values in the corresponding struct
======================*/
value_t valid_vars[] =
{
	{ "map_name",   V_NULL,	        BSFS( mapPart ) },	// map_name for generating basemap
	{ "produce_type",V_NULL,	BSFS( produceType ) },	// what does the building produce
	{ "produce_time",V_FLOAT,	BSFS( produceTime ) },	// how long for one unit?
	{ "produce",    V_INT,		BSFS( production ) },		// how many units
	{ "more_than_one",    V_BOOL,	BSFS( moreThanOne ) },	// is the building allowed to be build more the one time?
	{ "title",	V_TRANSLATION_STRING,	BSFS( title ) },				// displayed building name
	{ "pedia",	V_NULL,		BSFS( pedia ) },			// the pedia-id string
	{ "status",	V_INT,		BSFS( buildingStatus[0] ) },	// the status of the building
	{ "image",	V_NULL,		BSFS( image ) },			// the string to identify the image for the building
	{ "desc",	V_TRANSLATION_STRING,		BSFS( text ) },			// short description

	//on which level is the building available
	//starts at level 0 for underground - going up to level 7 (like in-game)
	//this flag is only needed if you have several baselevels
	//defined with MAX_BASE_LEVELS in client.h
	{ "level",	V_INT,		BSFS( level ) },

	//if this flag is set nothing can built upon this building
	//this flag is only needed if you have several baselevels
	//defined with MAX_BASE_LEVELS in client.h
	{ "not_upon",	V_BOOL,		BSFS( notUpOn ) },

	//set first part of a building to 1 all others to 0
	//otherwise all building-parts will be on the list
	{ "visible",	V_BOOL,		BSFS( visible ) },
// 	{ "size",	V_POS,		BSFS( size ) },			// not needed yet
	{ "needs",	V_NULL,		BSFS( needs ) },			// for buildings with more than one part (dont forget to set the visibility of all non-main-parts to 0)
	{ "depends",	V_NULL,		BSFS( depends ) },	// only available if this building is availabel, too
	{ "energy",	V_FLOAT,	BSFS( energy ) },			// amount of energy needed for use
	{ "fixcosts",	V_FLOAT,	BSFS( fixCosts ) },		// buildcosts
	{ "varcosts",	V_FLOAT,	BSFS( varCosts ) },		// costs that will come up by using the building
	{ "worker_costs",V_FLOAT,	BSFS( workerCosts ) },
	{ "build_time",	V_INT,		BSFS( buildTime ) },	// how much days will it take to construct the building?

	//event handler functions
	{ "onconstruct",	V_STRING,	BSFS( onConstruct ) },
	{ "onattack",	V_STRING,	BSFS( onAttack ) },
	{ "ondestroy",	V_STRING,	BSFS( onDestroy ) },
	{ "onupgrade",	V_STRING,	BSFS( onUpgrade ) },
	{ "onrepair",	V_STRING,	BSFS( onRepair ) },
	{ "onclick",	V_STRING,	BSFS( onClick ) },
	{ "max_workers",V_INT,	BSFS( maxWorkers ) },		// how many workers should there be max
	{ "min_workers",V_INT,	BSFS( minWorkers ) },			// how many workers should there be min

	//place of a building
	//needed for flag autobuild
	{ "pos",		V_POS,			BSFS( pos ) },

	//automatically construct this building when a base it set up
	//set also the pos-flag
	{ "autobuild",		V_BOOL,			BSFS( autobuild ) },

	//automatically construct this building for the first base you build
	//set also the pos-flag
	{ "firstbase",		V_BOOL,			BSFS( firstbase ) },
	{ NULL,	0, 0 }
};

/*======================
The valid definition names for productions in the basemagagement.ufo file.
NOTE: the PRODFS macro (see cl_basemanagement.h) assignes the values from scriptfile
to the appropriate values in the corresponding struct
======================*/
value_t production_valid_vars[] =
{
	{ "title",	V_TRANSLATION_STRING,	PRODFS( title ) },
	{ "desc",	V_TRANSLATION_STRING,	PRODFS( text ) },
	{ "amount",	V_INT,	PRODFS( amount ) },
	{ "menu",	V_STRING,	PRODFS( menu ) },
	{ NULL,	0, 0 }
};

/*======================
MN_BuildingStatus

Displays the status of a building for baseview
======================*/
void MN_BuildingStatus( void )
{
	int daysLeft;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
	{
		Com_DPrintf( "MN_BuildingStatus: No Base or no Building set\n" );
		return;
	}

	daysLeft = baseCurrent->buildingCurrent->timeStart + baseCurrent->buildingCurrent->buildTime - ccs.date.day;

	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_NOT_SET )
		if ( ! baseCurrent->buildingCurrent->howManyOfThisType )
			Cvar_Set("mn_building_status", _("Not set") );
		else
			Cvar_Set("mn_building_status", va(_("Already %i in base"), baseCurrent->buildingCurrent->howManyOfThisType ));
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UNDER_CONSTRUCTION )
		Cvar_Set("mn_building_status", va ( _("Constructing: %i day(s)"), daysLeft ) );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_CONSTRUCTION_FINISHED )
		Cvar_Set("mn_building_status", _("Construction finished") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UPGRADE )
		Cvar_Set("mn_building_status", va ( _("Upgrade: %i day(s)"), daysLeft ) );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_WORKING_100 )
		Cvar_Set("mn_building_status", _("Working 100%") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_WORKING_120 )
		Cvar_Set("mn_building_status", _("Working 120%") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_WORKING_50 )
		Cvar_Set("mn_building_status", _("Working 50%") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_MAINTENANCE )
		Cvar_Set("mn_building_status", _("Maintenance") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_REPAIRING )
		Cvar_Set("mn_building_status", _("Repairing") );
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_DOWN )
		Cvar_Set("mn_building_status", _("Down") );
	else
		Cvar_Set("mn_building_status", "BUG");
}

/*======================
MN_BuildingInfoClick

Opens up the 'pedia if you right click on a building in the list.
======================*/
void MN_BuildingInfoClick( void )
{
	if ( baseCurrent && baseCurrent->buildingCurrent )
		UP_OpenWith ( baseCurrent->buildingCurrent->pedia );
}

/*======================
B_SetUpBase

TODO: document this
======================*/
void B_SetUpBase ( void )
{
	int i;

	assert( baseCurrent );
	MN_BuildingInit();
	Com_DPrintf("Set up for %i\n", baseCurrent->id );
	for (i = 0 ; i < numBuildings; i++ )
	{
		if ( ccs.numBases == 1 && bmBuildings[ccs.actualBaseID][i].firstbase )
		{
			baseCurrent->buildingCurrent = &bmBuildings[ccs.actualBaseID][i];
			Com_DPrintf("firstbase: %s (%i) at (%.0f:%.0f)\n", baseCurrent->buildingCurrent->name, i, bmBuildings[ccs.actualBaseID][i].pos[0], bmBuildings[ccs.actualBaseID][i].pos[1] );
			MN_SetBuildingByClick ( (int)bmBuildings[ccs.actualBaseID][i].pos[0], (int)bmBuildings[ccs.actualBaseID][i].pos[1] );
			bmBuildings[ccs.actualBaseID][i].buildingStatus[bmBuildings[ccs.actualBaseID][i].howManyOfThisType] = B_WORKING_100;
			if ( bmBuildings[ccs.actualBaseID][i].moreThanOne
			  && bmBuildings[ccs.actualBaseID][i].howManyOfThisType < BASE_SIZE*BASE_SIZE )
				bmBuildings[ccs.actualBaseID][i].howManyOfThisType++;
			//update the array
			MN_BuildingInit();
		}
		else if ( bmBuildings[ccs.actualBaseID][i].autobuild )
		{
			baseCurrent->buildingCurrent = &bmBuildings[ccs.actualBaseID][i];
			Com_DPrintf("autobuild: %s (%i) at (%.0f:%.0f)\n", baseCurrent->buildingCurrent->name, i, bmBuildings[ccs.actualBaseID][i].pos[0], bmBuildings[ccs.actualBaseID][i].pos[1] );
			MN_SetBuildingByClick ( (int)bmBuildings[ccs.actualBaseID][i].pos[0], (int)bmBuildings[ccs.actualBaseID][i].pos[1] );
			bmBuildings[ccs.actualBaseID][i].buildingStatus[bmBuildings[ccs.actualBaseID][i].howManyOfThisType] = B_WORKING_100;
			if ( bmBuildings[ccs.actualBaseID][i].moreThanOne
			  && bmBuildings[ccs.actualBaseID][i].howManyOfThisType < BASE_SIZE*BASE_SIZE )
				bmBuildings[ccs.actualBaseID][i].howManyOfThisType++;
			//update the array
			MN_BuildingInit();
		}
	}
}

/*======================
B_GetBuilding

Returns the building in the current base that has the unique name buildingName.
pa
IN
	buildingName:	The unique name of the building (building_t->name).

OUT
	building_t	B_GetBuilding	if a building was found it is returned,
						if no name was give the current building is returned,
						otherwise->NULL
======================*/
building_t* B_GetBuilding ( char *buildingName )
{
	int i = 0;

	if ( ! buildingName )
		return baseCurrent->buildingCurrent;

	Com_DPrintf("Searching for building %s\n", buildingName );
	for (i = 0 ; i < numBuildings; i++ )
		if ( !Q_strcasecmp( bmBuildings[ccs.actualBaseID][i].name, buildingName ) )
			return &bmBuildings[ccs.actualBaseID][i];

	Com_Printf("Building %s not found\n", buildingName );
	return NULL;
}

/*======================
MN_RemoveBuilding

Removes the current selected building
but this is only allowed if its still under construction
you will not get any money back
======================*/
void MN_RemoveBuilding( void )
{
	building_t* b;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	b = baseCurrent->buildingCurrent;

	//only allowed when it is still under construction
	if ( b->buildingStatus[b->howManyOfThisType] == B_UNDER_CONSTRUCTION )
	{
		b->buildingStatus[b->howManyOfThisType] = B_NOT_SET;
		b->howManyOfThisType--;
// 		baseCurrent->map[b->pos[0]][b->pos[1]][baseCurrent->baseLevel] = -1;
// 		if (b->dependsBuilding)
// 			baseCurrent->map[b->dependsBuilding->pos[0]][b->dependsBuilding->pos[1]][baseCurrent->baseLevel] = -1;
		MN_BuildingStatus();
	}
}

/*======================
MN_ConstructBuilding

Check whether the player has enough credits to construct
the current selected building.
Start construction if yes, otherwise
======================*/
void MN_ConstructBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	//enough credits to build this?
	if ( ccs.credits - baseCurrent->buildingCurrent->fixCosts < 0 )
	{
		MN_Popup( _("Notice"), _("Not enough credits to build this\n") );
		return;
	}

	Com_DPrintf("Construction of %s is starting\n", baseCurrent->buildingCurrent->title );

	// second building part
	if ( baseCurrent->buildingToBuild != NULL )
	{
		baseCurrent->buildingToBuild->buildingStatus[baseCurrent->buildingToBuild->howManyOfThisType] = B_UNDER_CONSTRUCTION;
		baseCurrent->buildingToBuild = NULL;
	}

	baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_UNDER_CONSTRUCTION;
	baseCurrent->buildingCurrent->timeStart = ccs.date.day;

	CL_UpdateCredits( ccs.credits - baseCurrent->buildingCurrent->fixCosts );
}

/*======================
MN_ResetBuildingCurrent

is called e.g. when leaving the build-menu
but also several times from cl_basemanagement.c
======================*/
void MN_ResetBuildingCurrent ( void )
{
	if ( baseCurrent )
		baseCurrent->buildingCurrent = NULL;
}

/*======================
MN_NewBuilding
======================*/
void MN_NewBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION )
		MN_ConstructBuilding();

	MN_BuildingStatus();
}

/*======================
MN_SetBuildingByClick

//level 0 - underground
======================*/
void MN_SetBuildingByClick ( int row, int col )
{
	building_t *building = NULL;
	building_t *secondBuildingPart = NULL;

	if ( row < BASE_SIZE && col < BASE_SIZE )
	{
		if ( baseCurrent->map[row][col][baseCurrent->baseLevel] == -1 )
		{
			if ( baseCurrent->buildingCurrent->needs && baseCurrent->buildingCurrent->visible )
				secondBuildingPart = B_GetBuilding( baseCurrent->buildingCurrent->needs );

			if ( baseCurrent->baseLevel >= 1 )
			{
				// a building on the upper level needs a building on the lower level
				if ( baseCurrent->map[row][col][baseCurrent->baseLevel-1] == -1 )
				{
					Com_Printf( "No ground where i can place building upon\n" );
					return;
				}
				else if ( secondBuildingPart && baseCurrent->map[row+1][col][baseCurrent->baseLevel-1] == -1 )
				{
					if ( baseCurrent->map[row][col+1][baseCurrent->baseLevel-1] == -1 )
					{
						Com_Printf( "No ground where i can place building upon\n" );
						return;
					}
				}
				else
				{
					building = B_GetBuildingByID( baseCurrent->map[row][col][baseCurrent->baseLevel-1] );
					if ( building->notUpOn )
					{
						Com_Printf( "Can't place any building upon this ground\n" );
						return;
					}
					else if ( secondBuildingPart )
					{
						building = B_GetBuildingByID( baseCurrent->map[row][col+1][baseCurrent->baseLevel-1] );
						if ( building->notUpOn )
						{
							Com_Printf( "Can't place any building upon this ground\n" );
							return;
						}
					}
				}
			}

			if ( secondBuildingPart )
			{
				if ( baseCurrent->map[row][col+1][baseCurrent->baseLevel] != -1 )
				{
					Com_Printf( "Can't place this building here - the second part overlapped with another building\n" );
					return;
				}

 				baseCurrent->map[row][col+1][baseCurrent->baseLevel] = baseCurrent->buildingCurrent->id;
				baseCurrent->buildingToBuild = secondBuildingPart;
			}
			else
				baseCurrent->buildingToBuild = NULL;
			if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] <= B_UNDER_CONSTRUCTION )
				MN_NewBuilding();

 			baseCurrent->map[row][col][baseCurrent->baseLevel] = baseCurrent->buildingCurrent->id;

 			if ( baseCurrent->buildingCurrent->buildingType == B_LAB )
 				baseCurrent->hasLab = 1;
 			if ( baseCurrent->buildingCurrent->buildingType == B_HANGAR )
 				baseCurrent->hasHangar = 1;
			MN_ResetBuildingCurrent();
		} else {
			Com_Printf( "There is already a building\n" );
			Com_DPrintf( "Building: %i at (row:%i, col:%i)\n", baseCurrent->map[row][col][baseCurrent->baseLevel], row, col );
		}
	}
	else
		Com_Printf( "Invalid coordinates\n" );


}

/*======================
MN_SetBuilding

Places the current building in the base (x/y give via console)
======================*/
void MN_SetBuilding( void )
{
	int row, col;

	if ( Cmd_Argc() < 3 )
	{
		Com_Printf( "Usage: set_building <x> <y>\n" );
		return;
	}

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	row = atoi( Cmd_Argv( 1 ) );
	col = atoi( Cmd_Argv( 2 ) );

	//emulate the mouseclick with the given coordinates
	MN_SetBuildingByClick ( row, col );
}

/*======================
MN_NewBuildingFromList
======================*/
void MN_NewBuildingFromList( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION )
		MN_NewBuilding();
	else
		MN_RemoveBuilding();

}

/*======================
MN_DamageBuilding
======================*/
void MN_DamageBuilding( void )
{
	int damage;
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: damage_building <amount>\n" );
		return;
	}

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	//which building?
	damage = atoi( Cmd_Argv( 1 ) );

	if ( ! ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_DOWN
	      || baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_NOT_SET
	      || baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UNDER_CONSTRUCTION
	      || baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_CONSTRUCTION_FINISHED ) )
		 baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] -= damage;

	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UNDER_CONSTRUCTION )
		baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] = 0;

	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_CONSTRUCTION_FINISHED )
		baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] = 0;

	if ( baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] <= 0 )
	{
		baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_DOWN;
		baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] = 0;
	}

	if ( baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] <= 50 )
		baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_WORKING_50;

}

/*======================
MN_UpgradeBuilding

Only the last building (if moreThanOne is true)
will describe the status of the buildings which
are the same type
======================*/
void MN_UpgradeBuilding ( void )
{
	building_t* b;

	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	b = baseCurrent->buildingCurrent;
	b->buildingStatus[b->howManyOfThisType] = B_UPGRADE;
}

/*======================
B_UpgradeBuilding

Only the last building (if moreThanOne is true)
will describe the status of the buildings which
are the same type
======================*/
void B_UpgradeBuilding( building_t* b )
{
	assert(b);

	if ( ccs.credits >= UPGRADECOSTS )
	{
		b->timeStart--;
		CL_UpdateCredits( ccs.credits - UPGRADECOSTS );
		b->techLevel += 1;
	}
	else
	{
		// TODO: Check whether building needs workers
		//       if so but none are assigned go to status
		//       B_CONSTRUCTION_FINISHED
		//       otherwise the building will maybe produce stuff with
		//       no workers assigned
		b->buildingStatus[b->howManyOfThisType] = B_WORKING_100;
	}
}

/*======================
MN_RepairBuilding

Only the last building (if moreThanOne is true)
will describe the status of the buildings which
are the same type
======================*/
void MN_RepairBuilding( void )
{
	building_t* b;

	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	b = baseCurrent->buildingCurrent;
	b->buildingStatus[b->howManyOfThisType] = B_REPAIRING;

	// we need to count back this pseudo time in B_RepairBuilding
	b->timeStart = REPAIRTIME;
}

/*======================
B_RepairBuilding

Only the last building (if moreThanOne is true)
will describe the status of the buildings which
are the same type
======================*/
void B_RepairBuilding( building_t* b )
{
	int i = 0;
	assert(b);

	if ( b->condition[i] < BUILDINGCONDITION )
	{
		if ( ccs.credits >= REPAIRCOSTS )
		{
			b->timeStart--;
			CL_UpdateCredits( ccs.credits - REPAIRCOSTS );
			b->condition[i]++;
		}
	}
	else
	{
		// TODO: Check whether building needs workers
		//       if so but none are assigned go to status
		//       B_CONSTRUCTION_FINISHED
		//       otherwise the building will maybe produce stuff with
		//       no workers assigned
		b->buildingStatus[b->howManyOfThisType] = B_WORKING_100;
	}
}

/*======================
MN_DrawBuilding
======================*/
void MN_DrawBuilding( void )
{
	int i = 0;

	building_t *entry;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	menuText[TEXT_BUILDING_INFO] = buildingText;
	*menuText[TEXT_BUILDING_INFO] = '\0';

	entry = baseCurrent->buildingCurrent;

	MN_BuildingStatus();

	if ( entry->buildingStatus[entry->howManyOfThisType] < B_UNDER_CONSTRUCTION && entry->fixCosts )
		Q_strcat( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Costs:\t%1.2f (%i Day[s] to build)\n"), entry->fixCosts, entry->buildTime ) );

	if ( entry->varCosts    ) Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Running Costs:\t%1.2f\n"), entry->varCosts ) );
	if ( entry->workerCosts ) Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Workercosts:\t%1.2f\n"), entry->workerCosts ) );
	if ( entry->assignedWorkers  ) Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Workers:\t%i\n"), entry->assignedWorkers ) );
	if ( entry->energy      ) Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Energy:\t%1.0f\n"), entry->energy ) );
	if ( entry->produceType )
		for (i = 0 ; i < numProductions; i++ )
			if ( !Q_stricmp( bmProductions[i].name, entry->produceType ) )
			{
				if ( entry->production && bmProductions[i].title )
					Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Production:\t%s (%i/day)\n"), bmProductions[i].title, entry->production ) );
				break;
			}

	if ( entry->buildingStatus[entry->howManyOfThisType] > B_CONSTRUCTION_FINISHED )
	{
		if ( entry->techLevel ) Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( _("Level:\t%i\n"), entry->techLevel ) );
		if ( entry->condition[entry->howManyOfThisType]   )
			Q_strcat ( menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va ( "%s:\t%i\n", _("Condition"), entry->condition[baseCurrent->buildingCurrent->howManyOfThisType] ) );
	}

	if ( entry->name  ) Cvar_Set( "mn_building_name", entry->name );
	if ( entry->title ) Cvar_Set( "mn_building_title", entry->title );
}

/*======================
MN_BuildingRemoveWorkers
======================*/
void MN_BuildingRemoveWorkers( void )
{
	int workers;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: remove_workers <amount>\n" );
		return;
	}

	//how many workers?
	workers = atoi( Cmd_Argv( 1 ) );

	if (baseCurrent->buildingCurrent->assignedWorkers - workers
	 >= baseCurrent->buildingCurrent->minWorkers
	    * ( baseCurrent->buildingCurrent->howManyOfThisType + 1 ) )
		baseCurrent->buildingCurrent->assignedWorkers -= workers;
	else
		Com_Printf( "Minimum amount of workers reached for this building\n" );

}

/*======================
MN_BuildingAddWorkers
======================*/
void MN_BuildingAddWorkers( void )
{
	int workers;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: add_workers <amount>\n" );
		return;
	}

	//how many workers?
	workers = atoi( Cmd_Argv( 1 ) );

	if ( baseCurrent->buildingCurrent->assignedWorkers + workers
	  <= baseCurrent->buildingCurrent->maxWorkers
	     * ( baseCurrent->buildingCurrent->howManyOfThisType + 1 ) )
		baseCurrent->buildingCurrent->assignedWorkers += workers;
	else
		Com_Printf( "Maximum amount of workers reached for this building\n" );

}

/*======================
MN_BuildingAddToList

Handles the building list of constructable buildings

Called everytime a building was constructed and
thus maybe other buildings get available.

menuText[TEXT_BUILDINGS] is a pointer to baseCurrent->allBuildingsList
This way every base can hold its own building list.
The pointer is updated everytime we select a base via mn_select_base
(see MN_SelectBase for further information)
======================*/
void MN_BuildingAddToList( char *title, int id )
{
	assert(baseCurrent);

	//is the title already in list?
	//if not, then add the title to the list
	if (!strstr( menuText[TEXT_BUILDINGS], title ) )
		Q_strcat ( menuText[TEXT_BUILDINGS], MAX_LIST_CHAR, va("%s\n", title ) );
}

/*======================
MN_BuildingInit
======================*/
void MN_BuildingInit( void )
{
	int i = 1;
	building_t *building;

	// maybe someone call this command before the bases are parsed??
	if ( ! baseCurrent )
		return;

	memset( baseCurrent->allBuildingsList, 0, sizeof(baseCurrent->allBuildingsList) );
	menuText[TEXT_BUILDINGS] = baseCurrent->allBuildingsList;

	for ( i = 0; i < numBuildings; i++)
	{
		//make an entry in list for this building
		if ( bmBuildings[ccs.actualBaseID][i].visible )
		{
			// only allowed once?
			if ( bmBuildings[ccs.actualBaseID][i].buildingStatus[0] > B_UNDER_CONSTRUCTION
			  && !bmBuildings[ccs.actualBaseID][i].moreThanOne )
				continue;

			// allowed more than one time - but limit of BASE_SIZE*BASE_SIZE exceeded
			if ( bmBuildings[ccs.actualBaseID][i].moreThanOne
			  && bmBuildings[ccs.actualBaseID][i].howManyOfThisType >= BASE_SIZE*BASE_SIZE )
				continue;

			if ( bmBuildings[ccs.actualBaseID][i].depends )
			{
				building = B_GetBuilding ( bmBuildings[ccs.actualBaseID][i].depends );
				if ( ! building )
					Sys_Error("Wrong dependency in basemangagement.ufo for building %s (depends on %s)", bmBuildings[ccs.actualBaseID][i].name, bmBuildings[ccs.actualBaseID][i].depends );
				bmBuildings[ccs.actualBaseID][i].dependsBuilding = building;
				if ( bmBuildings[ccs.actualBaseID][i].dependsBuilding->buildingStatus[0] >= B_UNDER_CONSTRUCTION )
					MN_BuildingAddToList( _(bmBuildings[ccs.actualBaseID][i].title), i );
			}
			else
			{
				MN_BuildingAddToList( _(bmBuildings[ccs.actualBaseID][i].title), i );
			}
		}
	}
	if ( baseCurrent->buildingCurrent )
	{
		MN_DrawBuilding();
	}
}

/*======================
B_GetBuildingByID
======================*/
building_t* B_GetBuildingByID ( int id )
{
	if ( baseCurrent )
		return &bmBuildings[baseCurrent->id][id];
	else
		Sys_Error( "Bases not initialized\n" );

	//just that there are no warnings
	return NULL;
}

/*======================
MN_BuildingClick

Script function for clicking the building list
text field
======================*/
void MN_BuildingClick( void )
{
	int	num, i;

	if ( Cmd_Argc() < 2 )
		return;

	//which building?
	num = atoi( Cmd_Argv( 1 ) );

	for ( i = 0; i < numBuildings; i++)
	{
		//make an entry in list for this building
		if ( !bmBuildings[ccs.actualBaseID][i].visible )
			continue;

		// only allowed to set up once
		if ( bmBuildings[ccs.actualBaseID][i].buildingStatus[0] > B_UNDER_CONSTRUCTION
		  && !bmBuildings[ccs.actualBaseID][i].moreThanOne )
			continue;

		// allowed more than one time - but limit of BASE_SIZE*BASE_SIZE exceeded
		if ( bmBuildings[ccs.actualBaseID][i].moreThanOne
		  && bmBuildings[ccs.actualBaseID][i].howManyOfThisType >= BASE_SIZE*BASE_SIZE )
			continue;


		// ok, this needs to be in the list
		num--;
		if ( num < 0 )
			break;
	}
	if (i != numBuildings)
	{
		baseCurrent->buildingCurrent = B_GetBuildingByID(i);
		MN_DrawBuilding();
	}
}

/*======================
MN_ParseBuildings

Parses one "building" entry in the basemanagement.ufo file and writes it into the next free entry in bmBuildings[0] ... which is teh list of buildings in the first base (building_t).

IN
	id:	Unique test-id of a building_t. This is parsed from "building xxx" -> id=xxx
	text:	TODO document this ... it appears to be the whole following text that is part of the "building" item definition in .ufo.
======================*/
void MN_ParseBuildings( char *id, char **text )
{
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;
	value_t *edp = NULL;
	char    *errhead = "MN_ParseBuildings: unexptected end of file (names ";
	char    *token = NULL;
	int	i = 0;
	int	j = 0;
	int	numEmployees_temp = 0;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || *token != '{' )
	{
		Com_Printf( "MN_ParseBuildings: building \"%s\" without body ignored\n", id );
		return;
	}
	if ( numBuildings >= MAX_BUILDINGS )
	{
		Com_Printf( "MN_ParseBuildings: too many buildings\n" );
		return;
	}

	// new entry
	building = &bmBuildings[0][numBuildings];
	memset( building, 0, sizeof( building_t ) );
	Q_strncpyz( building->name, id, MAX_VAR );

	Com_DPrintf("...found building %s\n", building->name );

	//set standard values
	building->techLevel = 1;
	building->visible = true;
	building->condition[0] = BUILDINGCONDITION;
	building->id = numBuildings;

	numBuildings++;
	do {
		// get the name type
		token = COM_EParse( text, errhead, id );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// get values
		if ( !Q_strncmp( token, "type", 4 ) ) {
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;

			if ( !Q_strncmp( token, "lab", 3 ) ) {
				building->buildingType = B_LAB;
			} else if ( !Q_strncmp( token, "hangar", 6 ) ) {
				building->buildingType = B_HANGAR;
			} else if ( !Q_strncmp( token, "quarters", 7 ) ){
				building->buildingType = B_QUARTERS;
			} else if ( !Q_strncmp( token, "workshop", 8 ) ){
				building->buildingType = B_WORKSHOP;
			}
		}
		else
		if ( !Q_strncmp( token, "max_employees", 13 ) ) {
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			employees_in_building = &building->assigned_employees;

			employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
			if ( *token ) {
				employees_in_building->maxEmployees = atoi(token);
			}
		}
		else
		if ( !Q_strncmp( token, "employees_firstbase", 19 ) ) {
			token = COM_EParse( text, errhead, id );
			if ( !*text ) return;
			if (*token) {
				employees_in_building = &building->assigned_employees;
				if ( employees_in_building->maxEmployees <= 0 )
					employees_in_building->maxEmployees = atoi(token);
				numEmployees_temp = atoi(token);
				for ( employees_in_building->numEmployees = 0; employees_in_building->numEmployees < numEmployees_temp; )
				{
					// assign random employee infos.
					employees_in_building->assigned[employees_in_building->numEmployees] = &employees[numEmployees++]; // link this employee in the building to the global employee-list.
					employee = employees_in_building->assigned[employees_in_building->numEmployees];
					employees_in_building->numEmployees++;
					memset( employee, 0, sizeof( employee_t ) );
				}
			}
		}
		else
		for ( edp = valid_vars; edp->string; edp++ )
			if ( !Q_strncmp( token, edp->string, sizeof(edp->string) ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, id );
				if ( !*text ) return;

				if ( edp->ofs && edp->type != V_NULL ) Com_ParseValue( building, token, edp->type, edp->ofs );
				else if ( edp->type == V_NULL )
				{
					strcpy( bmData, token );
					*(char **)((byte *)building + edp->ofs) = bmData;
					bmData += strlen( token ) + 1;
				}
				break;
			}

		if ( !edp->string )
			Com_Printf( "MN_ParseBuildings: unknown token \"%s\" ignored (building %s)\n", token, id );

	} while ( *text );

	// now copy all buildings to all bases
	// this is needed because every building in every base
	// can have altered parameters
	for ( i = 1; i < MAX_BASES; i++ )
		for ( j = 0; j < numBuildings; j++ )
		{
			building = &bmBuildings[i][j];
			memcpy( building, &bmBuildings[0][j], sizeof( building_t ) );
			building->base = i;	// link the building to the currently processed base.
		}

}

/*======================
MN_InitEmployees

Creates full information out of the parsed data for employees, and assign them into the (not yet assigned) correct rooms.

This should be called after setting up the first base.

TODO: this right now assumes that there are not more employees than free quarter space ... but it will not puke if there are.
======================*/
void MN_InitEmployees ( void )
{
	int i, j;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	// Loop trough the buildings to assign the type of employee.
	// TODO: this right now assumes that there are not more employees than free quarter space ... but it will not puke if there are.
	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[0][i];
		employees_in_building = &building->assigned_employees;
		employees_in_building->cost_per_employee = 100;			// TODO: fixed value right now, needs a configureable one.
		if ( employees_in_building->maxEmployees <= 0)
			employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
		for ( j = 0; j < employees_in_building->numEmployees; j++ ) {
			employee = employees_in_building->assigned[j];
			switch ( building->buildingType )
			{
			case B_QUARTERS:
				employee->type = EMPL_SOLDIER;
				break;
			case B_LAB:
				employee->type = EMPL_SCIENTIST;
				break;
			case B_WORKSHOP:
				employee->type = EMPL_WORKER;
				break;
			//EMPL_MEDIC
			//EMPL_ROBOT
			default:
				break;
			}
		}
	}
	building = NULL;
	// Generate stats for employees and assign the quarter-less to quarters.
	for ( i=0; i < numEmployees; i++) {
		employee = &employees[i];
		switch ( employee->type )
		{
		case EMPL_SOLDIER:
			// TODO: create random data for the employees depending on type and skill-min/max
			// employee->combat_stats =
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			employee->lab = NULL;
			employee->workshop = NULL;
			if ( employee->type == EMPL_SCIENTIST) {
				// TODO: create random data for the employees depending on type and skill-min/max
				employee->speed = 100;
			} else {
				// TODO: create random data for the employees depending on type and skill-min/max
				employee->speed = 100;
			}
			building = MN_GetFreeBuilding( 0, B_QUARTERS );
			employees_in_building = &building->assigned_employees;
			employees_in_building->assigned[employees_in_building->numEmployees++] = employee;
			break;
		//case EMPL_MEDIC: break;
		//case EMPL_ROBOT: break;
		default:
			break;
		}
	}

	// Remove them all from their assigned buildings except quarters .. this was just needed for firstbase.
	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[0][i];
		employees_in_building = &building->assigned_employees;
		if ( building->buildingType != B_QUARTERS ) {
			employees_in_building->numEmployees = 0;
		}
	}

}

/*======================
MN_GetFreeBuilding

Gets a free (with no assigned workers) building in the given base of the given type.

IN
	base_id:	The number/id of the base to search in.
	type:		Which type of building to search for.

OUT
	building	The (empty) building.
======================*/
building_t * MN_GetFreeBuilding( int base_id, buildingType_t type )
{
	int i;
	building_t *building = NULL;

	employees_t *employees_in_building = NULL;
	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[base_id][i];
			if ( building->buildingType == type ) {
				/* found correct building-type */
				employees_in_building = &building->assigned_employees;
				if ( employees_in_building->numEmployees < employees_in_building->maxEmployees ) {
					/* the bulding has free space for employees */
					return building;
				}
			}
	}
	/* no buildings available at all, no correct building type found or no building free */
	return NULL;
}

/*======================
MN_GetUnusedLab

Gets a lab in the given base with no research running.

IN
	base_id:	The number/id of the base to search in.

OUT
	MN_GetUnusedLab	the (unused) lab.
======================*/
building_t * MN_GetUnusedLab( int base_id )
{
	int i, j;
	building_t *building = NULL;
	technology_t *tech = NULL;
	byte used = false;

	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[base_id][i];
		if ( building->buildingType == B_LAB ) {
			used = false;
			// check in research tree if the lab is used
			for ( j=0; j < numTechnologies; j++ ) {
				tech = &technologies[j];
				if ( tech->lab == building ) {
					used = true;
					break;
				}
			}
			if ( !used )
				return building;
		}
		building = NULL;
	}
	return building;
}

/*======================
MN_GetUnusedLabs

Gets the number of unused labs (no assigned workers, no research) in the given base of the given type.

IN
	base_id:	The number/id of the base to search in.

OUT
	MN_GetUnusedLab	number of found (empty) building
======================*/
int MN_GetUnusedLabs( int base_id )
{
	int i, j;
	building_t *building = NULL;
	technology_t *tech = NULL;
	byte used = false;

	int numFreeLabs = 0;

	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[base_id][i];
		if ( building->buildingType == B_LAB ) {
			used = false;
			// check in research tree if the lab is used
			for ( j=0; j < numTechnologies; j++ ) {
				tech = &technologies[j];
				if ( tech->lab == building ) {
					used = true;
					break;
				}
			}
			if ( !used )
				numFreeLabs++;
		}
	}
	return numFreeLabs;
}

/*======================
MN_ClearBuilding

Removes all assigned scientists from a building.
TODO: If the building is of type "B_QUARTERS" before it's cleared all other buildings need to be checked if there is an employees there that also is in the qarter. These employees need to be removed from those buildings
======================*/
void MN_ClearBuilding( building_t *building )
{
	int i;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if ( ! building )
		return;

	employees_in_building = &building->assigned_employees;
	switch ( building->buildingType )
	{
	case B_QUARTERS:
		//TODO: ignored for now, will surely be usefull later on.
		break;
	case B_LAB:
		for ( i = 0; i < employees_in_building->numEmployees; i++ ) {
			employee = employees_in_building->assigned[i];
			employee->lab = NULL;
		}
		employees_in_building->numEmployees = 0;
		break;
	case B_WORKSHOP:
		for ( i = 0; i < employees_in_building->numEmployees; i++ ) {
			employee = employees_in_building->assigned[i];
			employee->workshop = NULL;
		}
		employees_in_building->numEmployees = 0;
		break;
	/* TODO:
	EMPL_MEDIC
	EMPL_ROBOT
	*/
	default:
		break;
	}
}

/*======================
MN_EmployeeIsFree

Returns true if the employee is only assigned to quarters, otherwise false.
======================*/
byte MN_EmployeeIsFree ( employee_t *employee )
{
	return ( ( employee->lab == NULL ) && ( employee->workshop == NULL ) );
}

/*======================
MN_AssignEmployee

Add a free employee from the quarters to building_dest. (the employee will be linked to both of them)

IN
	building_dest:	Which building to assign the employee to.
	employee_type:	What type of employee to assign to the building

OUT
	boolean MN_AssignEmployee: returns true if adding was possible/sane otherwise false.

TODO: Add check for destination building vs. employee_type and abort if they do not match.
TODO: Possibility to add emoployees to quarters (from the global list)
======================*/
byte MN_AssignEmployee ( building_t *building_dest, employeeType_t employee_type )
{
	int i, j;
	employee_t *employee = NULL;
	building_t *building_source = NULL;
	employees_t *employees_in_building_dest = NULL;
	employees_t *employees_in_building_source = NULL;


	if ( !baseCurrent ) {
		Com_DPrintf( "MN_AssignEmployee: No Base set\n" );
		return false;
	}

	if ( building_dest->buildingType == B_QUARTERS  ) {
		Com_DPrintf( "MN_AssignEmployee: No need to move from quarters to quarters.\n" );
		return false;
	}

	employees_in_building_dest = &building_dest->assigned_employees;
	employee = NULL;
	// check if there is enough space to add one employee in the destination building.
	if ( employees_in_building_dest->numEmployees < employees_in_building_dest->maxEmployees ) {
		// get free employee from quarters in current base
		for ( i = 0; i < numBuildings; i++ ) {
			building_source = &bmBuildings[building_dest->base][i];

			// check if there is a free employee in the quarters.
			if ( building_source->buildingType == B_QUARTERS ) {
				employees_in_building_source = &building_source->assigned_employees;
				for ( j = 0; j < employees_in_building_source->numEmployees; j++ ) {
					employee = employees_in_building_source->assigned[j];
					if ( ( employee->type == employee_type) && MN_EmployeeIsFree(employee) )
						break;
					else
						employee = NULL;
				}
			}
		}
		// if an employee was found add it to to the destination building
		if ( employee ) {
			employees_in_building_dest->assigned[employees_in_building_dest->numEmployees++] = employee;
			employee->lab = building_dest;
			return true;
		} else {
			Com_Printf( "No employee available in this base.\n" );
		}
	} else {
		Com_Printf( "No free room in destination building \"%s\".\n", building_dest->name);
	}
	return false;
}

/*======================
MN_RemoveEmployee

Remove one employee from building.

IN
	building:	Which building to remove the employee from. Can be any type of building that has employees in it. If quarters are given the employee will be removed from every other building as well.

OUT
	boolean MN_AssignEmployee: returns true if adding was possible/sane otherwise false.

TODO: Add check for destination building vs. employee_type and abort if they do not match.
======================*/
byte MN_RemoveEmployee ( building_t *building )
{
	/* TODO
	int i;
	*/
	employee_t *employee = NULL;
	employees_t *employees_in_building = NULL;

	/* TODO
	building_t *building_temp = NULL;
	byte found = false;
	*/

	employees_in_building = &building->assigned_employees;

	if (employees_in_building->numEmployees <= 0) {
		Com_DPrintf( "MN_RemoveEmployee: No employees in building. Can't remove one. %s\n", building->name );
		return false;
	}

	// Check where else (which buildings) the employee needs to be removed.
	switch ( building->buildingType )
	{
	/* TODO
	case B_QUARTERS:
		// unlink the employee from quarters and every other building. (he is now only stored in the global list)
		employee->quarters = NULL;

		//remove the employee from the other buildings (i.e their employee-list) they are listed as well before unlinking them.
		if ( employee->lab ) {
			building_temp = employee->lab;
			employees_in_building = &building_temp->assigned_employees;
			found = false;
			for ( i = 0; i < ( employees_in_building->numEmployees - 1 ); i++ ) {
				if ( (employees_in_building->assigned[i] == employee) || found ){
					employees_in_building->assigned[i] = employees_in_building->assigned[i+1];
					found = true;
				}
			}
			if ( found )
				employees_in_building->numEmployees--;
			employee->lab = NULL;
		}

	// TODO
	//	if ( employee->workshop ) {
	//	}

		return true;
		//break;
	*/
	case B_LAB:
		employees_in_building->numEmployees--;	// remove the employee from the list of assigned workers in the building.
		employee = employees_in_building->assigned[employees_in_building->numEmployees];	// get the last employee in the building.
		Com_DPrintf( "MN_RemoveEmployee: %s\n", building->name );
		// unlink the employee from lab (the current building).
		employee->lab = NULL;
		Com_DPrintf( "MN_RemoveEmployee: %s 2\n", building->name );
		return true;
		//break;
	/* TODO
	case B_WORKSHOP:
		// unlink the employee from workshop (the current building).
		employee->workshop = NULL;
		return true;
		//break;
	EMPL_MEDIC
	EMPL_ROBOT
	*/
	default:
		break;
	}

	return false;
}
/*======================
MN_EmployeesInBase + MN_EmployeesInBase2

Returns the number of employees in the given base (in the quaters) of the given type.
You can choose (free_only) if you want the nubmer of free ones or the total number.
If you call the function with employee_type set to MAX_EMPL it will return every type of employees.
======================*/
int MN_EmployeesInBase2 ( int base_id, employeeType_t employee_type, byte free_only )
{
	int i, j;
	int numEmployeesInBase = 0;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if ( !baseCurrent ) {
		Com_DPrintf( "MN_EmployeesInBase2: No Base set.\n" );
		return 0;
	}

	for ( i = 0; i < numBuildings; i++ ) {
		building = &bmBuildings[base_id][i];
		if ( building->buildingType == B_QUARTERS ) {
			/* quarters found */
			employees_in_building = &building->assigned_employees;

			//loop trough building and add to numEmployeesInBase if a match is found.
			for ( j = 0; j < employees_in_building->numEmployees; j++ ) {
				employee = employees_in_building->assigned[j];
				if ( ( (employee_type == employee->type) || (employee_type ==MAX_EMPL) )
				&& ( MN_EmployeeIsFree(employee) || !free_only ) )
					numEmployeesInBase++;
			}
		}
	}
	return numEmployeesInBase;
}

int MN_EmployeesInBase ( int base_id, employeeType_t employee_type )
{
	return MN_EmployeesInBase2 ( base_id, employee_type, false );
}

/*======================
MN_ClearBase
======================*/
void MN_ClearBase( base_t *base )
{
	int	row, col, levels, i;

	memset( base, 0, sizeof(base_t) );

	// FIXME: is this needed?
	//        the above memset should do this - why is this here?
	memset( &base->equipment, 0, sizeof(inventory_t) );
	base->nextUCN = 0;

	CL_ResetCharacters( base );

	// setup team
	if (!curCampaign) // should be multiplayer
	{
		while ( base->numWholeTeam < cl_numnames->value )
			CL_GenerateCharacter( Cvar_VariableString( "team" ), base );
	}
	else // should be multiplayer (campaignmode TODO) or singleplayer
	{
		for ( i = 0; i < curCampaign->soldiers; i++ )
			CL_GenerateCharacter( curCampaign->team, base );
	}

	for ( row = BASE_SIZE-1; row >= 0; row-- )
		for ( col = BASE_SIZE-1; col >= 0; col-- )
			for ( levels = MAX_BASE_LEVELS-1; levels >= 0; levels-- )
				base->map[row][col][levels] = -1;
}

/*======================
MN_ParseBases
======================*/
void MN_ParseBases( char *title, char **text )
{
	char	*errhead = "MN_ParseBases: unexptected end of file (names ";
	char	*token;
	base_t	*base;
	int	numBases = 0;

	// get token
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "MN_ParseBases: base \"%s\" without body ignored\n", title );
		return;
	}
	do {
		// add base
		if ( numBases > MAX_BASES )
		{
			Com_Printf( "MN_ParseBases: too many bases\n" );
			return;
		}

		// get the name
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		base = &bmBases[numBases];
		memset( base, 0, sizeof( base_t ) );

		// get the title
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		Q_strncpyz( base->title, token, MAX_VAR );
		Com_DPrintf("Found base %s\n", base->title );
		MN_ResetBuildingCurrent();
		numBases++;
	} while ( *text );

	mn_base_title = Cvar_Get( "mn_base_title", "", 0 );
}


/*======================
MN_ParseProductions
======================*/
void MN_ParseProductions( char *title, char **text )
{
	production_t	*entry;
	value_t	*edp;
	char	*errhead = "MN_ParseProductions: unexptected end of file (names ";
	char	*token;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || *token != '{' )
	{
		Com_Printf( "MN_ParseProductions: production type \"%s\" without body ignored\n", title );
		return;
	}
	if ( numProductions >= MAX_PRODUCTIONS )
	{
		Com_Printf( "MN_ParseProductions: to many production types\n" );
		return;
	}

	// new entry
	entry = &bmProductions[numProductions];
	numProductions++;
	memset( entry, 0, sizeof( production_t ) );
	Q_strncpyz( entry->name, title, MAX_VAR );
	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		if ( *token == '{' )
		{
			// parse text
			qboolean skip;

			entry->text = bmDataProductions;
			token = *text;
			skip = true;
			while ( *token != '}' )
			{
				if ( *token > 32 )
				{
					skip = false;
					if ( *token == '\\' ) *bmDataProductions++ = '\n';
					else *bmDataProductions++ = *token;
				}
				else if ( *token == 32 )
				{
					if ( !skip ) *bmDataProductions++ = 32;
				}
				else skip = true;
				token++;
			}
			*bmDataProductions++ = 0;
			*text = token+1;
			continue;
		}

		// get values
		for ( edp = production_valid_vars; edp->string; edp++ )
			if ( !Q_stricmp( token, edp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				if ( edp->ofs && edp->type != V_NULL ) Com_ParseValue( entry, token, edp->type, edp->ofs );
				else if ( edp->type == V_NULL )
				{
					strcpy( bmDataProductions, token );
					*(char **)((byte *)entry + edp->ofs) = bmDataProductions;
					bmDataProductions += strlen( token ) + 1;
				}
				break;
			}

		if ( !edp->string )
			Com_Printf( "MN_ParseProductions: unknown token \"%s\" ignored (entry %s)\n", token, title );

	} while ( *text );
}

#define MOUSEOVER mx > baseCurrent->posX[row][col][baseCurrent->baseLevel] && mx < baseCurrent->posX[row][col][baseCurrent->baseLevel] + ( picWidth * bvScale ) && my > baseCurrent->posY[row][col][baseCurrent->baseLevel] && my < baseCurrent->posY[row][col][baseCurrent->baseLevel] + ( picHeight * bvScale )

/*======================
MN_DrawBase

FIXME: faster rendering - this is all 512x512
without texture compression my notebook stutters around
======================*/
void MN_DrawBase( void )
{
	int row, col, rowCnt = 0;
	float x, y;
	int mx, my;
	vec2_t mouseover = {-1, -1};
	char *image, *statusImage = NULL;
	int width, height;
	building_t *entry;
	building_t *secondEntry;

#if 0
	vec4_t color;
	VectorSet( color, 0.0f, 1.0f, 0.0f );
	color[3] = 0.3f;
#endif

	if ( ! baseCurrent )
		Cbuf_ExecuteText( EXEC_NOW, "mn_pop" );

	bvScale = ccs.basezoom;
	bvCenterX = ccs.basecenter[0] * SCROLLSPEED;
	bvCenterY = ccs.basecenter[1] * SCROLLSPEED;

	IN_GetMousePos( &mx, &my );

	for ( row = 0; row < BASE_SIZE; row++ )
	{
		x = ( BASE_SIZE * 326 + ( rowCnt * 186 ) - bvCenterX ) * bvScale;
		y = ( ( rowCnt * 280 ) - bvCenterY ) * bvScale; // 512 - 71 => 71 pixel overlap
		for ( col = BASE_SIZE - 1; col >= 0; col-- )
		{
			if ( row >= 0 && row < BASE_SIZE && col >= 0 && col < BASE_SIZE )
			{
				if ( baseCurrent->map[row][col][baseCurrent->baseLevel] != -1 )
				{
					entry = B_GetBuildingByID(baseCurrent->map[row][col][baseCurrent->baseLevel]);

					// NOTE: time is not running here - commented out
// 					B_CheckBuildingConstruction( entry );

					if ( entry->buildingStatus[entry->howManyOfThisType] == B_UPGRADE )
						statusImage = "base/upgrade";
					else if ( entry->buildingStatus[entry->howManyOfThisType] == B_DOWN )
						statusImage = "base/down";
					else if ( entry->buildingStatus[entry->howManyOfThisType] == B_REPAIRING
						|| entry->buildingStatus[entry->howManyOfThisType] == B_MAINTENANCE )
						statusImage = "base/repair";
					else if ( entry->buildingStatus[entry->howManyOfThisType] == B_UNDER_CONSTRUCTION )
						statusImage = "base/construct";

					if ( !entry->used && entry->needs && entry->visible )
					{
						secondEntry = B_GetBuilding ( entry->needs );
						if ( ! secondEntry )
							Com_Printf( "Error in ufo-scriptfile - could not find the needed building\n" );
						entry->used = 1;
						image = secondEntry->image;
					}
					else
					{
						image = entry->image;
						entry->used = 0;
					}
				}
				else
				{
					image = "base/grid";
				}

				re.DrawGetPicSize ( &width, &height, image );

				if ( width == -1 || height == -1 )
				{
					Com_Printf( "Invalid picture dimension of %s\n", image );
					return;
				}

				picWidth = width;
				picHeight = height;

				if ( mouseover[0] == -1 && MOUSEOVER )
				{
					mouseover[0] = row;
					mouseover[1] = col;
				}

				baseCurrent->posX[row][col][baseCurrent->baseLevel] = x;
				baseCurrent->posY[row][col][baseCurrent->baseLevel] = y;
			}
			else
			{
				image = "base/grid";
				statusImage = NULL;
			}

			if ( image != NULL ) {
				re.DrawNormPic( x, y, width*bvScale, height*bvScale, 0, 0, 0, 0, 0, false, image );
#if 0
				re.DrawFill(x, y, width*bvScale, height*bvScale, ALIGN_UL, color);
				re.DrawColor( NULL );
#endif
			}
			if ( statusImage != NULL )
			{
				re.DrawGetPicSize ( &width, &height, statusImage );
				if ( width == -1 || height == -1 )
					Com_Printf( "Invalid picture dimension of %s\n", statusImage );
				else
					re.DrawNormPic( x + (20 * bvScale) , y + (60 * bvScale), width*bvScale, height*bvScale, 0, 0, 0, 0, 0, false, statusImage );

				statusImage = NULL;
			}

// 			re.DrawColor(NULL);

			//adjust trafo values for correct assembly
			x -= 325.0 * bvScale;
			y += 162.0 * bvScale;
		}
		rowCnt++;
	}

	if ( mouseover[0] == -1 )
		return;

	// FIXME: Better mousehandling over tiles
	if ( baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION )
	{
		image = "base/highlight";
		re.DrawNormPic( baseCurrent->posX[(int)mouseover[0]][(int)mouseover[1]][baseCurrent->baseLevel],
				baseCurrent->posY[(int)mouseover[0]][(int)mouseover[1]][baseCurrent->baseLevel],
				width*bvScale, height*bvScale, 0, 0, 0, 0, 0, false, image );
	}
}

/*======================
MN_BaseInit
======================*/
void MN_BaseInit( void )
{
	ccs.actualBaseID = (int) Cvar_VariableValue("mn_base_id");

	baseCurrent = &bmBases[ ccs.actualBaseID ];

	if ( ! baseCurrent->baseLevel )
		baseCurrent->baseLevel = 0;

	// stuff for homebase
	if ( ccs.actualBaseID == 0 )
	{

	}

	// set base view
	ccs.basecenter[0] = 0.2;
	ccs.basecenter[1] = 0.2;
	ccs.basezoom = 0.4;

	//these are the workers you can set on buildings
	Cvar_SetValue( "mn_available_workers", 0 );

	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );
}

/*======================
MN_RenameBase
======================*/
void MN_RenameBase( void )
{
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: rename_base <name>\n" );
		return;
	}

	if ( baseCurrent )
		Q_strncpyz( baseCurrent->title, Cmd_Argv( 1 ) , MAX_VAR );
}

/*======================
MN_BaseLevelDown
======================*/
void MN_BaseLevelDown( void )
{
	if ( baseCurrent && baseCurrent->baseLevel > 0 )
	{
		baseCurrent->baseLevel--;
		Cvar_SetValue( "mn_base_level", baseCurrent->baseLevel );
	}
}

/*======================
MN_BaseLevelUp
======================*/
void MN_BaseLevelUp( void )
{
	if ( baseCurrent && baseCurrent->baseLevel < MAX_BASE_LEVELS-1 )
	{
		baseCurrent->baseLevel++;
		Cvar_SetValue( "mn_base_level", baseCurrent->baseLevel );
	}
}

/*======================
MN_NextBase
======================*/
void MN_NextBase( void )
{
	ccs.actualBaseID = (int)Cvar_VariableValue( "mn_base_id" );
	Com_DPrintf( "cur-base=%i num-base=%i\n", ccs.actualBaseID,ccs.numBases );
	if ( ccs.actualBaseID < ccs.numBases-1 )
		ccs.actualBaseID++;
	else
		ccs.actualBaseID = 0;
	Com_DPrintf( "new-base=%i\n", ccs.actualBaseID );
	if ( ! bmBases[ccs.actualBaseID].founded )
		return;
	else
	{
		Cbuf_AddText( va( "mn_select_base %i\n", ccs.actualBaseID ) );
		Cbuf_Execute();
	}
}

/*======================
MN_PrevBase
======================*/
void MN_PrevBase( void )
{
	ccs.actualBaseID = (int)Cvar_VariableValue( "mn_base_id" );
	Com_DPrintf( "cur-base=%i num-base=%i\n", ccs.actualBaseID,ccs.numBases );
	if ( ccs.actualBaseID > 0 )
		ccs.actualBaseID--;
	else
		ccs.actualBaseID = ccs.numBases-1;
	Com_DPrintf( "new-base=%i\n", ccs.actualBaseID );

	// this must be false - but i'm paranoid'
	if ( ! bmBases[ccs.actualBaseID].founded )
		return;
	else
	{
		Cbuf_AddText( va( "mn_select_base %i\n", ccs.actualBaseID ) );
		Cbuf_Execute();
	}
}

/*======================
MN_SelectBase
======================*/
void MN_SelectBase( void )
{
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: mn_select_base <baseID>\n" );
		return;
	}
	ccs.actualBaseID = atoi( Cmd_Argv( 1 ) );

	// set up a new base
	// called from *.ufo with -1
	if ( ccs.actualBaseID < 0 )
	{
		mapAction = MA_NEWBASE;
		ccs.actualBaseID = 0;
		while ( bmBases[ccs.actualBaseID].founded && ccs.actualBaseID < MAX_BASES )
			ccs.actualBaseID++;
		if ( ccs.actualBaseID < MAX_BASES )
		{
			baseCurrent = &bmBases[ ccs.actualBaseID ];
		}
		else
		{
			Com_Printf( "MaxBases reached\n" );
			// select the first base in list
			ccs.actualBaseID = 0;
			baseCurrent = &bmBases[ ccs.actualBaseID ];
		}
	}
	else if ( ccs.actualBaseID < MAX_BASES )
	{
		baseCurrent = &bmBases[ ccs.actualBaseID ];
		menuText[TEXT_BUILDINGS] = baseCurrent->allBuildingsList;
		if ( baseCurrent->founded ) {
			mapAction = MA_NONE;
			MN_PushMenu( "bases" );
			CL_AircraftSelect();
		} else {
			mapAction = MA_NEWBASE;
		}
	}
	else
	{
		ccs.actualBaseID = 0;
		baseCurrent = &bmBases[ ccs.actualBaseID ];
	}
	Cvar_SetValue( "mn_base_id", baseCurrent->id );
	Cvar_Set( "mn_base_title", baseCurrent->title );
}


// FIXME: This value is in menu_geoscape, too
//       make this variable??
#define BASE_COSTS 100000

/*======================
MN_BuildBase

TODO: First base needs to be constructed automatically
======================*/
void MN_BuildBase( void )
{
	assert(baseCurrent);

	// FIXME: This should not be here - but we only build bases in singleplayer
	ccs.singleplayer = true;

	if ( ccs.credits - BASE_COSTS > 0 )
	{
		if ( CL_NewBase( newBasePos ) )
		{
			baseCurrent->id = ccs.numBases-1;
			baseCurrent->founded = true;
			stats.basesBuild++;
			mapAction = MA_NONE;
			CL_UpdateCredits( ccs.credits - BASE_COSTS );
			Q_strncpyz( baseCurrent->title, mn_base_title->string, sizeof(baseCurrent->title) );
			Cvar_SetValue( "mn_base_id", baseCurrent->id );
			Cvar_Set( "mn_base_title", baseCurrent->title );
			Cbuf_AddText( "mn_push bases\n" );
			Q_strncpyz( messageBuffer, va(_("A new base has been built: %s."), mn_base_title->string ), MAX_MESSAGE_TEXT );
			MN_AddNewMessage( _("Base built"), messageBuffer, false, MSG_CONSTRUCTION, NULL );
			return;
		}
	}
	else
		Com_Printf( "Not enough credits to set up a new base\n" );
}


/*======================
B_BaseAttack
======================*/
void B_BaseAttack ( void )
{
	int whichBaseID;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: base_attack <baseID>\n" );
		return;
	}

	whichBaseID = atoi( Cmd_Argv( 1 ) );

	if ( whichBaseID >= 0 && whichBaseID < ccs.numBases )
	{
		bmBases[whichBaseID].baseStatus = BASE_UNDER_ATTACK;
		// TODO: New menu for:
		//      defend -> call AssembleBase for this base
		//      continue -> return to geoscape
		mapAction = MA_BASEATTACK;
	}

#if 0	//TODO: run eventhandler for each building in base
	if ( b->onAttack )
		Cbuf_AddText( b->onAttack );
#endif

}

/*======================
B_AssembleMap

NOTE: Do we need day and night maps here, too?
TODO: Search a empty fild and add a alien craft there
FIXME: We need to get rid of the tunnels to nivana
======================*/
void B_AssembleMap ( void )
{
	int row, col;
	char baseMapPart[1024];
	building_t *entry;
	char maps[2024];
	char coords[2048];

	*maps = '\0';
	*coords = '\0';

	assert(baseCurrent);

	//TODO: If a building is still under construction, it will be assembled as a finished part
	//otherwise we need mapparts for all the maps under construction
	for ( row = 0; row < BASE_SIZE; row++ )
		for ( col = 0; col < BASE_SIZE; col++ )
		{
			baseMapPart[0] = '\0';

			if ( baseCurrent->map[row][col][0] != -1 )
			{
				entry = B_GetBuildingByID(baseCurrent->map[row][col][0]);

				if ( ! entry->visible )
				{
					Com_DPrintf( "Building %s will not be taken for baseassemble - it's' invisible\n", entry->name );
					continue;
				}

				if ( !entry->used && entry->needs )
					entry->used = 1;
				else if ( entry->needs )
					continue;

				if ( entry->mapPart )
					Q_strncpyz( baseMapPart, va("b/%c/%s", baseCurrent->mapChar, entry->mapPart ), sizeof(baseMapPart) );
			}
			else
				Q_strncpyz( baseMapPart, va("b/%c/empty", baseCurrent->mapChar ), sizeof(baseMapPart) );

			if ( *baseMapPart )
			{
				Q_strcat( maps, sizeof(maps), baseMapPart );
				Q_strcat( maps, sizeof(maps), " " );
				// basetiles are 16 units in each direction
				Q_strcat( coords, sizeof(coords), va ("%i %i ", col*16, row*16 ) );
			}
		}
	Cbuf_AddText( va( "map \"%s\" \"%s\"\n", maps, coords ) );
}


/*======================
MN_NewBases
======================*/
void MN_NewBases( void )
{
	// reset bases
	int	i;
	char	title[MAX_VAR];

	ccs.actualBaseID = 0;
	for ( i = 0; i < MAX_BASES; i++ )
	{
		Q_strncpyz( title, bmBases[i].title, MAX_VAR );
		MN_ClearBase( &bmBases[i] );
		Q_strncpyz( bmBases[i].title, title, MAX_VAR );
	}
}

/*======================
B_AssembleRandomBase
======================*/
void B_AssembleRandomBase( void )
{
	Cbuf_AddText( va("base_assemble %i", rand() % ccs.numBases ) );
}

/*======================
B_SaveBases
======================*/
void B_SaveBases( sizebuf_t *sb )
{
	// save bases
	base_t *base;
	building_t* building;
	int i, n, j;

	n = 0;
	for ( i = 0, base = bmBases; i < MAX_BASES; i++, base++ )
		if ( base->founded ) n++;
	MSG_WriteByte( sb, n );

	for ( i = 0, base = bmBases; i < MAX_BASES; i++, base++ )
		if ( base->founded )
		{
			MSG_WriteLong( sb, base->id );
			MSG_WriteString( sb, base->title );
			MSG_WriteFloat( sb, base->pos[0] );
			MSG_WriteFloat( sb, base->pos[1] );
			MSG_WriteByte( sb, base->hasHangar );
			MSG_WriteByte( sb, base->hasLab );
			MSG_WriteChar( sb, base->mapChar );
			MSG_WriteLong( sb, base->baseStatus );
			MSG_WriteLong( sb, base->baseLevel );
			MSG_WriteFloat( sb, base->condition );
			SZ_Write( sb, &base->map[0][0][0], sizeof(base->map) );

			// maybe count of buildings change due to an update
			MSG_WriteLong( sb, numBuildings );
			for ( j = 0, building = bmBuildings[i]; j < numBuildings; j++ )
			{
				SZ_Write( sb, &building->buildingStatus[0], sizeof(building->buildingStatus) );
				SZ_Write( sb, &building->condition[0], sizeof(building->condition) );
				MSG_WriteLong( sb, building->howManyOfThisType );
				MSG_WriteFloat( sb, building->pos[0] );
				MSG_WriteFloat( sb, building->pos[1] );
				//the production amount needs to be saved because
				//an update can increase the production size
				MSG_WriteLong( sb, building->production );
				MSG_WriteLong( sb, building->level );
				//maybe still constructing
				MSG_WriteLong( sb, building->timeStart );
				MSG_WriteLong( sb, building->buildTime );
				//which lebel?
				MSG_WriteLong( sb, building->techLevel );
				// how many workers?
				MSG_WriteLong( sb, building->assignedWorkers );
				building++;
			}
			AIR_SaveAircraft( sb, base );

			// store team
			CL_SendTeamInfo( sb, base->wholeTeam, base->numWholeTeam );

			// store assignement
			MSG_WriteLong( sb, base->teamMask );
			MSG_WriteByte( sb, base->numOnTeam );
			MSG_WriteByte( sb, base->numHired );
			MSG_WriteLong( sb, base->hiredMask );
		}
}

/*======================
B_LoadBases

This function is called by CL_GameLoad from cl_campaign.c
It loads back the bases and the buildings
You can use the buildinglist and baselist commands to verify
the loading process
======================*/
void B_LoadBases( sizebuf_t *sb, int version )
{
	// load bases
	base_t *base;
	building_t* building;
	int i, j, num, tmp;

	MN_NewBases();
	num = MSG_ReadByte( sb );
	//FIXME: This will align the bases
	//     If a base was attacked and destroyed - a save followed
	//     by a load will lead to the slot (where the base->found is
	//     not true) being lost. Do you understand what I mean??
	for ( i = 0, base = bmBases; i < num; i++, base++ )
	{
		ccs.actualBaseID = i;
		baseCurrent = base;
		baseCurrent->allBuildingsList[0] = '\0';
		baseCurrent->aircraftCurrent = NULL; // FIXME: Load the first one
		base->founded = true;
		if ( version >= 2 )
		{
			base->id = MSG_ReadLong( sb );
			Q_strncpyz( base->title, MSG_ReadString( sb ), sizeof(base->title) );
			base->pos[0] = MSG_ReadFloat( sb );
			base->pos[1] = MSG_ReadFloat( sb );
			base->hasHangar = MSG_ReadByte( sb );
			base->hasLab = MSG_ReadByte( sb );
			base->mapChar = MSG_ReadChar( sb );
			base->baseStatus = MSG_ReadLong( sb );
			base->baseLevel = MSG_ReadLong( sb );
			base->condition = MSG_ReadFloat( sb );
		}
		else
		{
			Q_strncpyz( base->title, MSG_ReadString( sb ), sizeof(base->title) );
			base->pos[0] = MSG_ReadFloat( sb );
			base->pos[1] = MSG_ReadFloat( sb );
		}
		memcpy( &base->map[0][0][0], sb->data + sb->readcount, sizeof(base->map) );
		sb->readcount += sizeof(base->map);
		if ( version >= 2 )
		{
			// maybe count of buildings change due to an update
			tmp = MSG_ReadLong( sb );
			if ( tmp != numBuildings )
				Com_Printf( "There was an update and there are new buildings available which aren't in your savegame. You may encounter problems. (%i:%i)\n", tmp, numBuildings );

			// it seams to me that there are buildings deleted since last save game
			if ( tmp > numBuildings )
				tmp = numBuildings;

			for ( j = 0, building = bmBuildings[i]; j < tmp; j++, building++ )
			{
				memcpy( &building->buildingStatus[0], sb->data + sb->readcount, sizeof(building->buildingStatus) );
				sb->readcount += sizeof(building->buildingStatus);

				memcpy( &building->condition[0], sb->data + sb->readcount, sizeof(building->condition) );
				sb->readcount += sizeof(building->condition);

				building->howManyOfThisType = MSG_ReadLong( sb );
				building->pos[0] = MSG_ReadFloat( sb );
				building->pos[1] = MSG_ReadFloat( sb );
				//the production amount needs to be saved because
				//an update can increase the production size
				building->production = MSG_ReadLong( sb );
				building->level = MSG_ReadLong( sb );
				//maybe still constructing
				building->timeStart = MSG_ReadLong( sb );
				building->buildTime = MSG_ReadLong( sb );
				//which lebel?
				building->techLevel = MSG_ReadLong( sb );
				// how many workers?
				building->assignedWorkers = MSG_ReadLong( sb );
			}
			MN_BuildingInit();
			AIR_LoadAircraft( sb, base, version );
			base->aircraftCurrent = &base->aircraft[0];

			// read whole team list
			CL_LoadTeam( sb, base, version );
		}
	}
	ccs.numBases = num;
}

/*======================
CL_BuildingList

TODO: To be extended for load/save purposes
======================*/
void CL_BuildingList ( void )
{
	int i, j, k;
	base_t* base;
	building_t* building;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
	{
		Com_Printf( "No base selected\n" );
		return;
	}

	for ( i = 0, base = bmBases; i < MAX_BASES; i++, base++ )
	{
		if ( base->founded == false )
			continue;

		building = bmBuildings[i];
		Com_Printf("\nBase id %i: %s\n", i, base->title );
		for ( j = 0; j < numBuildings; j++ )
		{
			Com_Printf("...Building: %s #%i - id: %i\n", building->name, building->howManyOfThisType, building->id );
			Com_Printf(".....Status:\n");
			for ( k = 0; k < BASE_SIZE*BASE_SIZE; k++ )
			{
				if ( k > 1 && k % BASE_SIZE == 0 )
					Com_Printf("\n");
				Com_Printf("%i ", building->buildingStatus[k] );
				if ( ! building->buildingStatus[k] )
					break;
			}
			Com_Printf("\n");
			building++;
		}
	}
}

/*======================
CL_BaseList

TODO: To be extended for load/save purposes
======================*/
void CL_BaseList ( void )
{
	int i, row, col;
	base_t* base;
	for ( i = 0, base = bmBases; i < MAX_BASES; i++, base++ )
	{
		Com_Printf("Base id %i\n", base->id );
		Com_Printf("Base title %s\n", base->title );
		Com_Printf("Base pos %f:%f\n", base->pos[0], base->pos[1] );
		Com_Printf("Base map:\n");
		for ( row = 0; row < BASE_SIZE; row++ )
		{
			if ( row )
				Com_Printf("\n");
			for ( col = 0; col < BASE_SIZE; col++ )
			{
				// just show the first level - all others are not used yet
				Com_Printf("%i ", base->map[row][col][0] );
			}
		}
		Com_Printf("\n");
	}
}

/*======================
MN_ResetBaseManagement
======================*/
void MN_ResetBaseManagement( void )
{
	// reset menu structures
	numBuildings = 0;
	numProductions = 0;
	ccs.actualBaseID = 0;

	Com_DPrintf("Reset basemanagement\n");

	// get data memory
	if ( bmDataSize )
		memset( bmDataStart, 0, bmDataSize );
	else
	{
		Hunk_Begin( 0x10000 );
		bmDataStart = Hunk_Alloc( 0x10000 );
		bmDataSize = Hunk_End();
	}
	bmData = bmDataStart;

	// add commands and cvars
	Cmd_AddCommand( "mn_base_level_down", MN_BaseLevelDown );
	Cmd_AddCommand( "mn_base_level_up", MN_BaseLevelUp );
	Cmd_AddCommand( "mn_prev_base", MN_PrevBase );
	Cmd_AddCommand( "mn_next_base", MN_NextBase );
	Cmd_AddCommand( "mn_select_base", MN_SelectBase );
	Cmd_AddCommand( "mn_build_base", MN_BuildBase );
	Cmd_AddCommand( "upgrade_building", MN_UpgradeBuilding );
	Cmd_AddCommand( "repair_building", MN_RepairBuilding );
	Cmd_AddCommand( "new_building", MN_NewBuildingFromList );
	Cmd_AddCommand( "set_building", MN_SetBuilding );
	Cmd_AddCommand( "damage_building", MN_DamageBuilding );
	Cmd_AddCommand( "add_workers", MN_BuildingAddWorkers );
	Cmd_AddCommand( "remove_workers", MN_BuildingRemoveWorkers );
	Cmd_AddCommand( "rename_base", MN_RenameBase );
	Cmd_AddCommand( "base_attack", B_BaseAttack );
	Cmd_AddCommand( "base_init", MN_BaseInit );
	Cmd_AddCommand( "base_assemble", B_AssembleMap );
	Cmd_AddCommand( "base_assemble_rand", B_AssembleRandomBase );
	Cmd_AddCommand( "building_init", MN_BuildingInit );
	Cmd_AddCommand( "building_status", MN_BuildingStatus );
	Cmd_AddCommand( "buildinginfo_click", MN_BuildingInfoClick );
	Cmd_AddCommand( "buildings_click", MN_BuildingClick );
	Cmd_AddCommand( "reset_building_current", MN_ResetBuildingCurrent );
	Cmd_AddCommand( "baselist", CL_BaseList );
	Cmd_AddCommand( "buildinglist", CL_BuildingList );
	Cvar_SetValue( "mn_base_id", ccs.actualBaseID );
}

/*======================
B_GetCount

returns the number of founded bases
======================*/
int B_GetCount ( void )
{
	int i, cnt = 0;

	for ( i = 0; i < MAX_BASES; i++ )
	{
		if ( ! bmBases[i].founded ) continue;
		cnt++;
	}

	return cnt;
}

/*======================
CL_UpdateBaseData

Called from the running campaign
======================*/
void CL_UpdateBaseData( void )
{
	building_t *b;
	int i, j;
	int newBuilding = 0, new;
	for ( i = 0; i < MAX_BASES; i++ )
	{
		if ( ! bmBases[i].founded ) continue;
		for ( j = 0; j < numBuildings; j++ )
		{
			b = &bmBuildings[i][j];
			if ( ! b ) continue;
			new = B_CheckBuildingConstruction( b );
			newBuilding += new;
			if ( new ) {
				MN_AddNewMessage( va(_("Building finished - Base %s"), bmBases[i].title ) , va(_("Construction of building %s finished."), b->title ), false, MSG_CONSTRUCTION, NULL );
				Com_sprintf( messageBuffer, MAX_MESSAGE_TEXT, _("Construction of building %s finished\\at base %s\n"), b->title, bmBases[i].title );
			}
		}
		// refresh the building list
		// and show a popup
		if ( newBuilding )
		{
			if ( newBuilding > 1 )
				Com_sprintf( messageBuffer, MAX_MENUTEXTLEN, _("There is at least one finished construction\\at base %s\n"), bmBases[i].title );
			MN_AddNewMessage( _("Construction finished"), messageBuffer, true, MSG_STANDARD, NULL );
		}

		// only the last occurence of a building can have a status
		// all other instances are seen as having the same
		if ( b->buildingStatus[b->howManyOfThisType] == B_REPAIRING )
			B_RepairBuilding( b );

		if ( b->buildingStatus[b->howManyOfThisType] == B_UPGRADE )
			B_UpgradeBuilding( b );
	}
	CL_CheckResearchStatus();
}

/*======================
B_CheckBuildingConstruction

Checks wheter the construction of a building is finished
Calls the onConstruct functions and assign workers, too
======================*/
int B_CheckBuildingConstruction ( building_t* b )
{
	int k, newBuilding = 0;

	for ( k = 0; k < b->howManyOfThisType + 1; k++ )
	{
		if ( b->buildingStatus[k] != B_UNDER_CONSTRUCTION )
			continue;
		if ( b->timeStart && ( b->timeStart + b->buildTime ) <= ccs.date.day )
		{
			b->buildingStatus[k] = B_WORKING_100;

			// more than one building of this type is allowed
			// so we need to allow further constructions
			// thus we increase the howManyOfThisType value
			if ( b->moreThanOne && b->howManyOfThisType < BASE_SIZE*BASE_SIZE )
				b->howManyOfThisType++;

			if ( b->onConstruct )
				Cbuf_AddText( b->onConstruct );

			if ( b->minWorkers )
			{
				Cbuf_AddText( va( "add_workers %i\n", b->minWorkers ) );
				Cbuf_Execute();
			}

			newBuilding++;
		}
	}
#if 0
	if ( b->buildingStatus[b->howManyOfThisType] == B_UNDER_CONSTRUCTION && b->timeStart + b->buildTime < ccs.date.day )
		b->buildingStatus[b->howManyOfThisType] = B_CONSTRUCTION_FINISHED;
#endif
	if ( newBuilding )
		MN_BuildingInit();

	return newBuilding;
}

/*======================
B_GetBase
======================*/
base_t* B_GetBase ( int id )
{
	int i;

	for ( i = 0; i < MAX_BASES; i++ )
	{
		if ( bmBases[i].id == id )
			return &bmBases[i];
	}
	return NULL;
}

int B_GetNumOnTeam ( void )
{
	// multiplayer
	if ( ! ccs.singleplayer && ! baseCurrent )
	{
		MN_ClearBase( &bmBases[0] );
		baseCurrent = &bmBases[0];
		Com_DPrintf( "no baseCurrent for mp\n" );
	}

	if ( ! baseCurrent )
	{
		Com_DPrintf( "Probably an error in B_GetNumOnTeam - no baseCurrent\n" );
		return 0;
	}

	return baseCurrent->numOnTeam;
}
