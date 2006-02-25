// cl_basemanagement.c

// TODO:
// new game does not reset basemangagement
// for saving: bmBuildings[], bmBases[], bmProductions[] (in cl_basemanagement.c)
//     in client.h: buildingText[] *baseCurrent, numBases, bmBases[]
//                  and the part out of ccs_t (not really needed i think)

#include "client.h"
#include "cl_basemanagement.h"

building_t    bmBuildings[MAX_BASES][MAX_BUILDINGS];

base_t        bmBases[MAX_BASES];

production_t  bmProductions[MAX_PRODUCTIONS];

int numBases;
int numBuildings;
int numProductions;
int baseID;

int bmDataSize = 0;

//DrawBase
float bvCenterX, bvCenterY;
float bvScale;

char *bmData, *bmDataStart, *bmDataProductions;

char infoBuildingText[MAX_MENUTEXTLEN];

value_t valid_vars[] =
{
	//internal name of building
	{ "name",	V_NULL,		BSFS( name ) },

	//map_name for generating basemap
	{ "map_name",   V_NULL,	        BSFS( mapPart ) },

	//what does the building produce
	{ "produce_type",V_NULL,	BSFS( produceType ) },

	//how long for one unit?
	{ "produce_time",V_FLOAT,	BSFS( produceTime ) },

	//how many units
	{ "produce",    V_INT,		BSFS( production ) },

	//is the building allowed to be build more the one time?
	{ "more_than_one",    V_INT,	BSFS( moreThanOne ) },

	//displayed building name
	{ "title",	V_STRING,	BSFS( title ) },

	//the pedia-id string
	{ "pedia",	V_NULL,		BSFS( pedia ) },

	//the status of the building
	{ "status",	V_INT,		BSFS( buildingStatus[0] ) },

	//the string to identify the image for the building
	{ "image",	V_NULL,		BSFS( image ) },

	//short description
	{ "desc",	V_NULL,		BSFS( text ) },

	//on which level is the building available
	//starts at level 0 for underground - going up to level 7 (like in-game)
	//this flag is only needed if you have several baselevels
	//defined with MAX_BASE_LEVELS in client.h
	{ "level",	V_NULL,		BSFS( level ) },

	//if this flag is set nothing can built upon this building
	//this flag is only needed if you have several baselevels
	//defined with MAX_BASE_LEVELS in client.h
	{ "not_upon",	V_INT,		BSFS( notUpOn ) },

	//set first part of a building to 1 all others to 0
	//otherwise all building-parts will be on the list
	{ "visible",	V_BOOL,		BSFS( visible ) },

	//not needed yet
// 	{ "size",	V_POS,		BSFS( size ) },

	//for buildings with more than one part
	//dont forget to set the visibility of all non-main-parts to 0
	{ "needs",	V_NULL,		BSFS( needs ) },

	//only available if this one is availabel, too
	{ "depends",	V_NULL,		BSFS( depends ) },

	//amount of energy needed for use
	{ "energy",	V_FLOAT,	BSFS( energy ) },

	//buildcosts
	{ "fixcosts",	V_FLOAT,	BSFS( fixCosts ) },

	//costs that will come up by using the building
	{ "varcosts",	V_FLOAT,	BSFS( varCosts ) },
	{ "worker_costs",V_FLOAT,	BSFS( workerCosts ) },

	//how much days will it take to construct the building?
	{ "build_time",	V_INT,		BSFS( buildTime ) },

	//event handler functions
	{ "onconstruct",	V_STRING,	BSFS( onConstruct ) },
	{ "onattack",	V_STRING,	BSFS( onAttack ) },
	{ "ondestroy",	V_STRING,	BSFS( onDestroy ) },
	{ "onupgrade",	V_STRING,	BSFS( onUpgrade ) },
	{ "onrepair",	V_STRING,	BSFS( onRepair ) },

	//how many workers should there for max and for min
	{ "max_workers",V_INT,	BSFS( maxWorkers ) },
	{ "min_workers",V_INT,	BSFS( minWorkers ) },

	//place of a building
	//needed for flag autobuild
	{ "pos",		V_POS,			BSFS( pos ) },

	//automatically construct this building when a base it set up
	//set also the pos-flag
	{ "autobuild",		V_BOOL,			BSFS( autobuild ) },

	{ NULL,	0, 0 }
};

value_t production_valid_vars[] =
{
	{ "title",	V_STRING,	PRODFS( title ) },
	{ "desc",	V_NULL  ,	PRODFS( text ) },
	{ "amount",	V_INT  ,	PRODFS( amount ) },
	{ "menu",	V_STRING  ,	PRODFS( menu ) },
	{ NULL,	0, 0 }
};

// ===========================================================

/*
=====================
B_HowManyPeopleInBase

returns the hole amount of soldiers/workers in base
=====================
*/
int B_HowManyPeopleInBase( base_t *base )
{
	int a, b;
	building_t *entry;
	int amount = 0;

	if ( ! base && ! baseCurrent )
		return 0;

	if ( ! base )
		base = baseCurrent;

	for ( b = BASE_SIZE-1; b >= 0; b-- ) // dont mess with the order
		for ( a = 0; a < BASE_SIZE; a++ )
			if ( base->map[a][b][base->baseLevel] != -1 )
			{
				entry = &bmBuildings[baseID][ B_GetIDFromList( base->map[a][b][base->baseLevel] ) ];

				if ( !entry->used && entry->needs && entry->visible )
					entry->used = 1;
				else
				{
					amount += entry->addWorkers;
					entry->used = 0;
				}
			}

	return amount;
}

/*
=================
MN_BuildingStatus
=================
*/
void MN_BuildingStatus( void )
{
	int daysLeft;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
	{
		Com_DPrintf( _("MN_BuildingStatus: No Base or no Building set\n") );
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


	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );

}

/*
======================
MN_BuildingInfoClick_f
======================
*/
void MN_BuildingInfoClick_f ( void )
{
	if ( baseCurrent && baseCurrent->buildingCurrent )
		UP_OpenWith ( baseCurrent->buildingCurrent->name );
}

void B_SetUpBase ( void )
{
	int i;

	for (i = 0 ; i < numBuildings; i++ )
		if ( bmBuildings[baseID][i].autobuild && bmBuildings[baseID][i].pos[0] )
		{
			baseCurrent->buildingCurrent = &bmBuildings[baseID][i];
			MN_SetBuildingByClick ( bmBuildings[baseID][i].pos[0], bmBuildings[baseID][i].pos[1] );
			baseCurrent->buildingCurrent = NULL;
		}
}

/*
=================
B_GetBuilding
=================
*/
building_t* B_GetBuilding ( char *buildingName )
{
	int i = 0;

	if ( ! buildingName )
	{
		if ( baseCurrent->buildingCurrent )
		{
			return baseCurrent->buildingCurrent;
		}
		else
		{
			return NULL;
		}

	}
	for (i = 0 ; i < numBuildings; i++ )
		if ( !strcmp( bmBuildings[baseID][i].name, buildingName ) )
			return &bmBuildings[baseID][i];

	return NULL;
}

/*
=================
MN_RemoveBuilding
=================
*/
void MN_RemoveBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	//only allowed when it is still under construction
	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UNDER_CONSTRUCTION )
	{
		ccs.credits += baseCurrent->buildingCurrent->fixCosts;
		baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_NOT_SET;
// 		baseCurrent->map[x][y][baseCurrent->baseLevel] = -1;
		MN_BuildingStatus();
	}
}

/*
====================
MN_ConstructBuilding
====================
*/
void MN_ConstructBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	//enough credits to build this?
	if ( ccs.credits - baseCurrent->buildingCurrent->fixCosts < 0 )
	{
		Com_Printf( _("Not enough credits to build this\n") );
		return;
	}

	if ( baseCurrent->buildingToBuild != NULL )
	{
		baseCurrent->buildingToBuild->buildingStatus[baseCurrent->buildingToBuild->howManyOfThisType] = B_UNDER_CONSTRUCTION;
		baseCurrent->buildingToBuild = NULL;
	}

	baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_UNDER_CONSTRUCTION;
	baseCurrent->buildingCurrent->timeStart = ccs.date.day;

	ccs.credits -= baseCurrent->buildingCurrent->fixCosts;
}

/*
==============
MN_NewBuilding
==============
*/
void MN_NewBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION )
		MN_ConstructBuilding();

	MN_BuildingStatus();
}

/*
=====================
MN_SetBuildingByClick
=====================
*/
//level 0 - underground
void MN_SetBuildingByClick ( int x, int y )
{
//	building_t *building = NULL;
	building_t *secondBuildingPart = NULL;

	if ( x < BASE_SIZE && y < BASE_SIZE )
	{
		if ( baseCurrent->map[x][y][baseCurrent->baseLevel] == -1 )
		{
			if ( baseCurrent->buildingCurrent->needs && baseCurrent->buildingCurrent->visible )
				secondBuildingPart = B_GetBuilding ( baseCurrent->buildingCurrent->needs );

			if ( baseCurrent->baseLevel >= 1 )
			{
				// a building on the upper level needs a building on the lower level
				if ( baseCurrent->map[x][y][baseCurrent->baseLevel-1] == -1 )
				{
					Com_Printf( _("No ground where i can place building upon\n"), x, y);
					return;
				}
				else if ( secondBuildingPart && baseCurrent->map[x+1][y][baseCurrent->baseLevel-1] == -1 )
				{
					if ( baseCurrent->map[x][y+1][baseCurrent->baseLevel-1] == -1 )
					{
						Com_Printf( _("No ground where i can place building upon\n"), x, y);
						return;
					}
				}
				else
					if ( bmBuildings[baseID][ B_GetIDFromList( baseCurrent->map[x][y][baseCurrent->baseLevel-1] ) ].notUpOn )
					{
						Com_Printf( _("Can't place any building upon this ground\n"), x, y);
						return;
					}
					else if ( secondBuildingPart )
						if ( bmBuildings[baseID][ B_GetIDFromList( baseCurrent->map[x][y+1][baseCurrent->baseLevel-1] ) ].notUpOn )
						{
							Com_Printf( _("Can't place any building upon this ground\n"), x, y);
							return;
						}
			}

			if ( secondBuildingPart )
			{
				if ( baseCurrent->map[x][y+1][baseCurrent->baseLevel] != -1 )
				{
					Com_Printf( _("Can't place this building here - the second part overlapped with another building\n"), x, y);
					return;
				}

 				baseCurrent->map[x][y+1][baseCurrent->baseLevel] = baseCurrent->buildingCurrent->id;
				baseCurrent->buildingToBuild = secondBuildingPart;
			}
			else
				baseCurrent->buildingToBuild = NULL;

			if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] <= B_UNDER_CONSTRUCTION )
				MN_NewBuilding();

 			baseCurrent->map[x][y][baseCurrent->baseLevel] = baseCurrent->buildingCurrent->id;
			baseCurrent->buildingCurrent = NULL;
		} else
			Com_Printf( _("There is already a building\n"), x, y);
	}
	else
		Com_Printf( _("Invalid coordinates\n") );


}

/*
====================
MN_SetBuilding
====================
*/
void MN_SetBuilding( void )
{
	int x, y;

	if ( Cmd_Argc() < 3 )
	{
		Com_Printf( _("Usage: set_building <x> <y>\n") );
		return;
	}

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	x = atoi( Cmd_Argv( 1 ) );
	y = atoi( Cmd_Argv( 2 ) );

	//emulate the mouseclick with the given coordinates
	MN_SetBuildingByClick ( x, y );
}

/*
==============
MN_NewBuildingFromList
==============
*/
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


/*
==================
MN_DamageBuilding
==================
*/
void MN_DamageBuilding( void )
{
	int damage;
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: damage_building <amount>\n") );
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

/*
==================
MN_UpgradeBuilding
==================
*/
void MN_UpgradeBuilding( void )
{
//	int day, month;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;


	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] > B_UPGRADE
	  && baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_MAINTENANCE )
	{
		baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_UPGRADE;
		baseCurrent->buildingCurrent->timeStart = ccs.date.day;
	}
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UPGRADE )
		if ( ccs.credits )
		{
// 			CL_DateConvert( &ccs.date, &day, &month );
			//save in struct
// 			Cvar_Set("mn_building_time", va("%i.%i.%i", ccs.date.day/365, month, day) );
// 			Cvar_SetValue("mn_building_remaining", (int) UPGRADETIME );

			if ( baseCurrent->buildingCurrent->timeStart + (int) UPGRADETIME <= ccs.date.day )
			{
				baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_WORKING_100;
				baseCurrent->buildingCurrent->techLevel += 1;
			}
			else
				ccs.credits -= UPGRADECOSTS;
		}
}


/*
==================
MN_RepairBuilding
==================
*/
void MN_RepairBuilding( void )
{
//	int day, month;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] != B_REPAIRING
	  && baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType]      != BUILDINGCONDITION )
	{
		baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_REPAIRING;
		baseCurrent->buildingCurrent->timeStart = ccs.date.day;
	}
	else if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_REPAIRING )
		if ( ccs.credits )
		{
// 			CL_DateConvert( &ccs.date, &day, &month );
			//save in struct
// 			Cvar_Set("mn_building_time", va("%i.%i.%i", ccs.year, month, day) );
// 			Cvar_SetValue("mn_building_remaining", (int) REPAIRTIME - (REPAIRTIME * baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] / BUILDINGCONDITION) );

			if ( baseCurrent->buildingCurrent->timeStart
			   + (int) REPAIRTIME - ( REPAIRTIME * baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] / 100 ) <= ccs.date.day )
			{
				baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] = B_WORKING_100;
				baseCurrent->buildingCurrent->condition[baseCurrent->buildingCurrent->howManyOfThisType] = BUILDINGCONDITION;
			}
			else
				ccs.credits -= REPAIRCOSTS;
		}

}

/*
=================
MN_DrawBuilding
=================
*/
void MN_DrawBuilding( void )
{
	int i = 0;

	building_t *entry;
	building_t *entrySecondBuildingPart;

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
	{
		Com_Printf( _("Called MN_DrawBuilding with no buildingCurrent set\n") );
		return;
	}

	menuText[TEXT_BUILDING_INFO] = buildingText;
	*menuText[TEXT_BUILDING_INFO] = '\0';

	entry = baseCurrent->buildingCurrent;

	MN_BuildingStatus();

	if ( entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] > B_NOT_SET && entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UPGRADE )
		MN_NewBuilding();

	if ( entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_REPAIRING )
		MN_RepairBuilding();

	if ( entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_UPGRADE )
		MN_UpgradeBuilding();

	if ( entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION && entry->fixCosts )
		strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Costs:\t%1.2f (%i Day[s] to build)\n"), entry->fixCosts, entry->buildTime  ) );

	if ( entry->varCosts    ) strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Running Costs:\t%1.2f\n"), entry->varCosts ) );
	if ( entry->workerCosts ) strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Workercosts:\t%1.2f\n"), entry->workerCosts ) );
	if ( entry->addWorkers  ) strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Workers:\t%i\n"), entry->addWorkers ) );
	if ( entry->energy      ) strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Energy:\t%1.0f\n"), entry->energy ) );
	if ( entry->produceType )
		for (i = 0 ; i < numProductions; i++ )
			if ( !strcmp( bmProductions[i].name, entry->produceType ) )
			{
				if ( entry->production && bmProductions[i].title )
					strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Production:\t%s (%i/day)\n"), bmProductions[i].title, entry->production ) );
				break;
			}

	if ( entry->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] > B_CONSTRUCTION_FINISHED )
	{
		if ( entry->techLevel ) strcat ( menuText[TEXT_BUILDING_INFO], va ( _("Level:\t%i\n"), entry->techLevel ) );
		if ( entry->condition[baseCurrent->buildingCurrent->howManyOfThisType]   )
			strcat ( menuText[TEXT_BUILDING_INFO], va ( "%s:\t%i\n", _("Condition"), entry->condition[baseCurrent->buildingCurrent->howManyOfThisType] ) );
	}

	//draw a second picture if building has two units
	if ( entry->needs && entry->visible )
	{
		entrySecondBuildingPart = B_GetBuilding ( entry->needs );
		if ( entrySecondBuildingPart )
			Cvar_Set( "mn_building_image2", entrySecondBuildingPart->image );
		else
			Com_Printf( _("Error in scriptfile: Could not find the building \"%s\"\n"), entry->needs );
	}

	if ( entry->name  ) Cvar_Set( "mn_building_name", entry->name );
	if ( entry->title ) Cvar_Set( "mn_building_title", entry->title );
	if ( entry->image ) Cvar_Set( "mn_building_image", entry->image );
}

/*
========================
MN_BuildingRemoveWorkers
========================
*/
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

	if (baseCurrent->buildingCurrent->addWorkers - workers
	 >= baseCurrent->buildingCurrent->minWorkers
	    * ( baseCurrent->buildingCurrent->howManyOfThisType + 1 ) )
		baseCurrent->buildingCurrent->addWorkers -= workers;
	else
		Com_Printf( _("Minimum amount of workers reached for this building\n") );

}

/*
=====================
MN_BuildingAddWorkers
=====================
*/
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

	if ( baseCurrent->buildingCurrent->addWorkers + workers
	  <= baseCurrent->buildingCurrent->maxWorkers
	     * ( baseCurrent->buildingCurrent->howManyOfThisType + 1 ) )
		baseCurrent->buildingCurrent->addWorkers += workers;
	else
		Com_Printf( _("Maximum amount of workers reached for this building\n") );

}

/*
=================
MN_PrevBuilding
=================
*/
void MN_PrevBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	// get previous entry
	if ( baseCurrent->buildingCurrent->id > 0)
		baseCurrent->buildingCurrent = &bmBuildings[baseID][ baseCurrent->buildingListArray[ baseCurrent->buildingCurrent->id - 1 ] ];
	else
		baseCurrent->buildingCurrent = &bmBuildings[baseID][ baseCurrent->buildingListArray[ baseCurrent->numList - 1 ] ];

	MN_DrawBuilding ();
}


/*
=================
MN_NextBuilding
=================
*/
void MN_NextBuilding( void )
{
	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	// get next entry
	if ( baseCurrent->buildingCurrent->id < baseCurrent->numList-1)
		baseCurrent->buildingCurrent = &bmBuildings[baseID][ baseCurrent->buildingListArray[ baseCurrent->buildingCurrent->id + 1 ] ];
	else
		baseCurrent->buildingCurrent = &bmBuildings[baseID][ baseCurrent->buildingListArray[ 0 ] ];

	MN_DrawBuilding ();
}

/*
====================
MN_BuildingAddToList
====================
*/
void MN_BuildingAddToList( char *title, int id )
{
	char tmpTitle[MAX_VAR];
	strncpy ( tmpTitle, title , MAX_VAR);

	//is the title already in list?
	if (!strstr( menuText[TEXT_BUILDINGS], tmpTitle ) )
	{

		//buildings are seperated by a newline
		strcat ( tmpTitle, "\n" );

		//now add the buildingtitle to the list
		strcat ( menuText[TEXT_BUILDINGS], tmpTitle );

		baseCurrent->buildingListArray[ baseCurrent->numList ] = id;
		bmBuildings[baseID][id].id = baseCurrent->numList;
		baseCurrent->numList++;
	}
}

/*
=================
MN_BuildingInit
=================
*/
void MN_BuildingInit( void )
{
	int i = 1;
	building_t *building;

	// maybe someone call this command before the bases are parsed??
	if ( ! baseCurrent )
		return;

	menuText[TEXT_BUILDINGS] = baseCurrent->allBuildingsList;

	for ( i = 1; i < numBuildings; i++)
	{
		//set the prev and next pointer
		bmBuildings[baseID][i].prev = &bmBuildings[baseID][i-1];
		bmBuildings[baseID][i-1].next = &bmBuildings[baseID][i];

		//make an entry in list for this building
		if ( bmBuildings[baseID][i-1].visible )
		{
			if ( bmBuildings[baseID][i-1].depends )
			{
				building = B_GetBuilding ( bmBuildings[baseID][i-1].depends );
				bmBuildings[baseID][i-1].dependsBuilding = building;
				if ( bmBuildings[baseID][i-1].dependsBuilding->buildingStatus[0] > B_CONSTRUCTION_FINISHED )
					MN_BuildingAddToList( _(bmBuildings[baseID][i-1].title), i-1 );
			}
			else
			{
				MN_BuildingAddToList( _(bmBuildings[baseID][i-1].title), i-1 );
			}
		}
	}

	//puts also the last building to the list
	if ( bmBuildings[baseID][i-1].visible )
	{
		if ( bmBuildings[baseID][i-1].depends )
		{
			building = B_GetBuilding ( bmBuildings[baseID][i-1].depends );
			bmBuildings[baseID][i-1].dependsBuilding = building;
			if ( bmBuildings[baseID][i-1].dependsBuilding->buildingStatus[0] > B_CONSTRUCTION_FINISHED )
				MN_BuildingAddToList( _(bmBuildings[baseID][i-1].title), i-1 );
		}
		else
		{
			MN_BuildingAddToList( _(bmBuildings[baseID][i-1].title), i-1 );
		}
	}

	if ( ! baseCurrent->buildingCurrent )
		baseCurrent->buildingCurrent = &bmBuildings[baseID][ baseCurrent->buildingListArray[ 0 ] ];


	if ( baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] > B_NOT_SET )
		MN_NewBuilding();

	MN_DrawBuilding();
}

/*
=================
B_GetIDFromList
=================
*/
int B_GetIDFromList ( int id )
{
	if ( baseCurrent )
		return baseCurrent->buildingListArray[id];
	else
		Sys_Error( _("Bases not initialized\n") );

	//just that there are no warnings
	return 0;
}

/*
=================
MN_BaseMapClick_f
=================
*/
void MN_BaseMapClick_f( void )
{
	int x, y;

	if ( Cmd_Argc() < 3 )
	{
		Com_Printf( _("Usage: basemap_click <x> <y>\n") );
		return;
	}

	//maybe someone call this command before the buildings are parsed??
	if ( ! baseCurrent || ! baseCurrent->buildingCurrent )
		return;

	x = atoi( Cmd_Argv( 1 ) );
	y = atoi( Cmd_Argv( 2 ) );


}

/*
==================
MN_BuildingClick_f
==================
*/
void MN_BuildingClick_f( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
		return;

	//which building?
	num = atoi( Cmd_Argv( 1 ) );

	if ( num >= baseCurrent->numList )
		num = baseCurrent->buildingCurrent->id;

	baseCurrent->buildingCurrent = &bmBuildings[baseID][ B_GetIDFromList( num ) ];

	MN_DrawBuilding();
}

/*
======================
MN_ParseBuildings
======================
*/
void MN_ParseBuildings( char *title, char **text )
{
	building_t *entry;
	value_t *edp;
	char    *errhead = _("MN_ParseBuildings: unexptected end of file (names ");
	char    *token;
	int	i = 0;
	int	j = 0;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseBuildings: building \"%s\" without body ignored\n"), title );
		return;
	}
	if ( numBuildings >= MAX_BUILDINGS )
	{
		Com_Printf( _("MN_ParseBuildings: too many buildings\n"), title );
		return;
	}

	// new entry
	entry = &bmBuildings[0][numBuildings];
	memset( entry, 0, sizeof( building_t ) );
	strncpy( entry->name, title, MAX_VAR );

	//set standard values
	entry->next = entry->prev = NULL;
	entry->buildingStatus[0] = B_NOT_SET;
	entry->techLevel = 1;
	entry->notUpOn = 0;
	entry->used = 0;
	entry->visible = 1;
	entry->energy = 0;
	entry->varCosts = 0;
	entry->workerCosts = 0;
	entry->minWorkers = 0;
	entry->maxWorkers = 0;
	entry->moreThanOne = 0;
	entry->addWorkers = 0;
	entry->howManyOfThisType = 0;
	entry->condition[0] = BUILDINGCONDITION;

	numBuildings++;
	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		if ( *token == '{' )
		{
			// parse text
			qboolean skip;

			entry->text = bmData;
			token = *text;
			skip = true;
			while ( *token != '}' )
			{
				if ( *token > 32 )
				{
					skip = false;
					if ( *token == '\\' ) *bmData++ = '\n';
					else *bmData++ = *token;
				}
				else if ( *token == 32 )
				{
					if ( !skip ) *bmData++ = 32;
				}
				else skip = true;
				token++;
			}
			*bmData++ = 0;
			*text = token+1;
			continue;
		}

		// get values
		for ( edp = valid_vars; edp->string; edp++ )
			if ( !strcmp( token, edp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				if ( edp->ofs && edp->type != V_NULL ) Com_ParseValue( entry, token, edp->type, edp->ofs );
				else if ( edp->type == V_NULL )
				{
					strcpy( bmData, token );
					*(char **)((byte *)entry + edp->ofs) = bmData;
					bmData += strlen( token ) + 1;
				}
				break;
			}

		if ( !edp->string )
			Com_Printf( _("MN_ParseBuildings: unknown token \"%s\" ignored (entry %s)\n"), token, title );

	} while ( *text );

	for ( i = 1; i < MAX_BASES; i++ )
		for ( j = 0; j < numBuildings; j++ )
		{
			entry = &bmBuildings[i][j];
			memcpy( entry, &bmBuildings[0][j], sizeof( building_t ) );
		}

}

/*
======================
MN_ClearBase
======================
*/
void MN_ClearBase( base_t *base )
{
	int	a,b,c;

	memset( base, 0, sizeof(base_t) );

	for ( b = BASE_SIZE-1; b >= 0; b-- )
		for ( a = BASE_SIZE-1; a >= 0; a-- )
			for ( c = MAX_BASE_LEVELS-1; c >= 0; c-- )
				base->map[a][b][c] = -1;
}


/*
======================
MN_ParseBases
======================
*/
void MN_ParseBases( char *title, char **text )
{
	char	*errhead = _("MN_ParseBases: unexptected end of file (names ");
	char	*token;
	base_t *base;

	// get token
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseBases: base \"%s\" without body ignored\n"), title );
		return;
	}
	numBases = 0;
	do {
		// add base
		if ( numBases > MAX_BASES )
		{
			Com_Printf( _("MN_ParseBases: too many bases\n"), title );
			return;
		}

		// get the name
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		base = &bmBases[numBases];
		memset( base, 0, sizeof( base_t ) );
//		strncpy( base->name, token, MAX_VAR );

		// get the title
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;
		strncpy( base->title, token, MAX_VAR );
		base->numList = 0;
		base->buildingCurrent = NULL;
/*		if (numBases > 0)
		{
			base->prev = &bmBases[numBases-1];
			bmBases[numBases-1].next = base;
		}
*/		MN_ClearBase( &bmBases[numBases] );
		numBases++;
	} while ( *text );
}


/*
======================
MN_ParseProductions
======================
*/
void MN_ParseProductions( char *title, char **text )
{
	production_t *entry;
	value_t *edp;
	char    *errhead = _("MN_ParseProductions: unexptected end of file (names ");
	char    *token;

	// get name list body body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseProductions: production type \"%s\" without body ignored\n"), title );
		return;
	}
	if ( numProductions >= MAX_PRODUCTIONS )
	{
		Com_Printf( _("MN_ParseProductions: to many production types\n"), title );
		return;
	}

	// new entry
	entry = &bmProductions[numProductions];
	numProductions++;
	memset( entry, 0, sizeof( production_t ) );
	strncpy( entry->name, title, MAX_VAR );
	entry->next = entry->prev = NULL;
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
			if ( !strcmp( token, edp->string ) )
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
			Com_Printf( _("MN_ParseProductions: unknown token \"%s\" ignored (entry %s)\n"), token, title );

	} while ( *text );
}

/*
=================
MN_DrawBase
=================
*/
void MN_DrawBase( void )
{
	int a, b;
	float x, y;
	int mx, my;
	int cursorSet = 0;
	char *image, *statusImage = NULL;
	int width, height;
	building_t *entry;
	building_t *secondEntry;
	if ( ! baseCurrent )
		Cbuf_ExecuteText( EXEC_NOW, "mn_pop" );

	if (! bmBuildings[baseID] )
		MN_BuildingInit();

	bvScale = ccs.basezoom;
	bvCenterX = ccs.basecenter[0] * SCROLLSPEED;
	bvCenterY = ccs.basecenter[1] * SCROLLSPEED;

	if ( baseCurrent->title )
		Cvar_Set( "mn_base_title", baseCurrent->title );

	// begin ur
	for ( b = BASE_SIZE-1; b >= 0; b-- )
		for ( a = 0; a < BASE_SIZE; a++ )
		{
			//adjust trafo values for correct assembly
			x = ( RELEVANT_X * b +  RELEVANT_Y * a - bvCenterX + a * 4) * bvScale;
			y = ( ( RELEVANT_X - 1 ) * a + ( RELEVANT_Y - 4 ) * ( ( b - ( BASE_SIZE - 1 ) ) * (- 1) ) - bvCenterY) * bvScale;

			// draw it (relative to view center)
			// FIXME  better if i can handle the values out of the scriptfile here
			x += 0;
			y += 96;

			if ( baseCurrent->map[a][b][baseCurrent->baseLevel] != -1 )
			{
				entry = &bmBuildings[baseID][ B_GetIDFromList( baseCurrent->map[a][b][baseCurrent->baseLevel] ) ];

				if ( entry->buildingStatus[entry->howManyOfThisType] == B_UNDER_CONSTRUCTION )
				{
					if ( entry->timeStart && ( entry->timeStart + entry->buildTime ) <= ccs.date.day )
					{
						entry->buildingStatus[entry->howManyOfThisType] = B_WORKING_100;
						Cbuf_AddText( va( "add_workers %i\n", entry->minWorkers ) );
						Cbuf_Execute();

						if ( entry->moreThanOne )
							entry->howManyOfThisType++;
						// refresh the building list
						MN_BuildingInit();
					}
				}

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
						Com_Printf( _("Error in ufo-scriptfile - could not find the needed building\n") );
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
				Com_Printf( _("Invalid picture dimension of %s\n"), image );
				return;
			}

			picWidth = width;
			picHeight = height;

			baseCurrent->posX[b][a][baseCurrent->baseLevel] = x;
			baseCurrent->posY[b][a][baseCurrent->baseLevel] = y;

			if ( image != NULL )
				re.DrawNormPic( x, y, width*bvScale, height*bvScale, 0, 0, 0, 0, ALIGN_UL, true, image );
			if ( statusImage != NULL )
			{
				x += 20 * bvScale;
				y += 60 * bvScale;
				re.DrawGetPicSize ( &width, &height, statusImage );
				if ( width == -1 || height == -1 )
					Com_Printf( _("Invalid picture dimension of %s\n"), statusImage );
				else
					re.DrawNormPic( x, y, width*bvScale, height*bvScale, 0, 0, 0, 0, ALIGN_UL, true, statusImage );

				statusImage = NULL;
			}
		}

	// FIXME: Better mousehandling over tiles - make cursorSet obsolete
	for ( b = BASE_SIZE-1; b >= 0; b-- )
		for ( a = 0; a < BASE_SIZE; a++ )
			if ( !cursorSet && baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] < B_UNDER_CONSTRUCTION )
			{
				IN_GetMousePos( &mx, &my );

				if ( baseCurrent->map[a][b][baseCurrent->baseLevel] == -1
				  && mx > baseCurrent->posX[b][a][baseCurrent->baseLevel]
				  && mx < baseCurrent->posX[b][a][baseCurrent->baseLevel] + ( picWidth * bvScale )
				  && my > baseCurrent->posY[b][a][baseCurrent->baseLevel]
				  && my < baseCurrent->posY[b][a][baseCurrent->baseLevel] + ( picHeight * bvScale ) )
				{
					if ( baseCurrent->baseLevel >= 1)
					{
						if ( baseCurrent->map[b][a][baseCurrent->baseLevel-1] == -1 )
							continue;
						else if ( bmBuildings[baseID][ B_GetIDFromList( baseCurrent->map[b][a][baseCurrent->baseLevel-1] ) ].notUpOn )
							continue;
					}
					image = "base/highlight";

					if ( baseCurrent->buildingCurrent->needs )
					{
// 						Com_Printf("The Building needs %s\n", baseCurrent->buildingCurrent->needs );
						if ( b + 1 < BASE_SIZE )
						{
							if ( baseCurrent->map[a][b+1][baseCurrent->baseLevel] != -1 )
								continue;

							re.DrawNormPic( baseCurrent->posX[b+1][a][baseCurrent->baseLevel], baseCurrent->posY[b+1][a][baseCurrent->baseLevel], width*bvScale, height*bvScale, 0, 0, 0, 0, ALIGN_UL, true, image );
							re.DrawNormPic( baseCurrent->posX[b][a][baseCurrent->baseLevel], baseCurrent->posY[b][a][baseCurrent->baseLevel], width*bvScale, height*bvScale, 0, 0, 0, 0, ALIGN_UL, true, image );
						}
					}
					else
					{
						re.DrawNormPic( baseCurrent->posX[b][a][baseCurrent->baseLevel], baseCurrent->posY[b][a][baseCurrent->baseLevel], width*bvScale, height*bvScale, 0, 0, 0, 0, ALIGN_UL, true, image );
					}

					cursorSet = 1;

					//building cursor
// 					cursor->value = 3;
				}
			}
}

/*
=================
MN_BaseInit
=================
*/
void MN_BaseInit( void )
{
	baseID = (int) Cvar_VariableValue("mn_base_id");

	baseCurrent = &bmBases[ baseID ];

	if ( ! baseCurrent->baseLevel )
		baseCurrent->baseLevel = 0;

	// stuff for homebase
	if ( baseID == 0 )
	{

	}

	// set base view
	ccs.basecenter[0] = 0.2;
	ccs.basecenter[1] = 0.2;
	ccs.basezoom = 0.7;

	//these are the workers you can set on buildings
	Cvar_Set( "mn_available_workers", 0 );

	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );
}

/*
=================
MN_RenameBase
=================
*/
void MN_RenameBase( void )
{
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: rename_base <name>\n") );
		return;
	}

	if ( baseCurrent )
		strncpy( baseCurrent->title, Cmd_Argv( 1 ) , MAX_VAR );
}

/*
=================
MN_BaseLevelDown
=================
*/
void MN_BaseLevelDown( void )
{
	if ( baseCurrent && baseCurrent->baseLevel > 0 )
	{
		baseCurrent->baseLevel--;
		Cvar_SetValue( "mn_base_level", baseCurrent->baseLevel );
	}
}

/*
=================
MN_BaseLevelUp
=================
*/
void MN_BaseLevelUp( void )
{
	if ( baseCurrent && baseCurrent->baseLevel < MAX_BASE_LEVELS-1 )
	{
		baseCurrent->baseLevel++;
		Cvar_SetValue( "mn_base_level", baseCurrent->baseLevel );
	}
}

/*
=================
MN_NextBase
=================
*/
void MN_NextBase( void )
{
	int baseID;

	baseID = (int)Cvar_VariableValue( "mn_base_id" );
	if ( baseID < MAX_BASES )
		baseID++;

	if ( ! bmBases[baseID].founded )
		return;
	else
	{
		Cbuf_AddText( va( "mn_select_base %i\n", baseID ) );
		Cbuf_Execute();
	}
}

/*
=================
MN_PrevBase
=================
*/
void MN_PrevBase( void )
{
	int baseID;

	baseID = (int)Cvar_VariableValue( "mn_base_id" );
	if ( baseID > 0 )
		baseID--;

	// this must be false - but i'm paranoid'
	if ( ! bmBases[baseID].founded )
		return;
	else
	{
		Cbuf_AddText( va( "mn_select_base %i\n", baseID ) );
		Cbuf_Execute();
	}
}

/*
=================
MN_SelectBase
=================
*/
void MN_SelectBase( void )
{
	int whichBaseID;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: select_base <baseID>\n") );
		return;
	}
	whichBaseID = atoi( Cmd_Argv( 1 ) );

	if ( whichBaseID < 0 )
	{
		mapAction = MA_NEWBASE;
		whichBaseID = 0;
		while ( bmBases[whichBaseID].founded && whichBaseID < MAX_BASES )
			whichBaseID++;
		if ( whichBaseID < MAX_BASES )
		{
			Cvar_Set( "mn_basename", va( "base %i", whichBaseID ) );
			baseCurrent = &bmBases[ whichBaseID ];
		}
		else
			Com_Printf(_("MaxBases reached\n"));
	}
	else
	{
		baseCurrent = &bmBases[ whichBaseID ];
		if ( baseCurrent->founded ) {
			mapAction = MA_NONE;
			MN_PushMenu( "bases" );
		} else {
			mapAction = MA_NEWBASE;
			Cvar_Set( "mn_basename", va( "base %i", whichBaseID ) );
		}
	}
	Cvar_SetValue( "mn_base_id", whichBaseID );
}


/*
=================
MN_BuildBase

TODO: First base needs to be constructed automatically
=================
*/
void MN_BuildBase( void )
{
	assert(baseCurrent);
	baseCurrent->founded = true;
	mapAction = MA_NONE;

	strncpy( baseCurrent->title, Cvar_VariableString( "mn_basename" ), MAX_VAR );
	Cbuf_AddText( "mn_push bases\n" );
}


/*
=================
B_BaseAttack
=================
*/
void B_BaseAttack ( void )
{
	int whichBaseID;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("sage: base_attack <baseID>\n") );
		return;
	}

	whichBaseID = atoi( Cmd_Argv( 1 ) );

	if ( whichBaseID < MAX_BASES )
		bmBases[whichBaseID].baseStatus = BASE_UNDER_ATTACK;

#if 0	//TODO: run eventhandler for each building in base
	if ( b->onAttack )
		Cbuf_AddText( b->onAttack );
#endif

}

/*
=================
B_AssembleMap
=================
*/
void B_AssembleMap ( void )
{
	int a, b, whichBaseID;
	char *baseMapPart;
	building_t *entry;
	char maps[1024];
	char coords[1024];

	*maps = '\0';
	*coords = '\0';

	if ( Cmd_Argc() < 2 && ! baseCurrent )
	{
		Com_Printf( _("Usage: base_assemble <baseID>\n") );
		return;
	}

	if ( ! baseCurrent )
	{
		whichBaseID = atoi( Cmd_Argv( 1 ) );

		if ( whichBaseID >= MAX_BASES )
			return;

		baseCurrent = &bmBases[whichBaseID];

		if (! bmBuildings[whichBaseID] )
			MN_BuildingInit();
	}

	//TODO: If a building is still under construction, it will be assembled as a finished part
	//otherwise we need mapparts for all the maps under construction
	for ( b = BASE_SIZE-1; b >= 0; b-- )
		for ( a = 0; a < BASE_SIZE; a++ )
		{
			baseMapPart = "\0";

			if ( baseCurrent->map[a][b][0] != -1 )
			{
				entry = &bmBuildings[baseID][ B_GetIDFromList( baseCurrent->map[a][b][0] ) ];

				if ( !entry->used && entry->needs && entry->visible )
					entry->used = 1;
				else
				{
					if ( entry->mapPart )
						baseMapPart = entry->mapPart;
					entry->used = 0;
				}
			}
			else
				baseMapPart = "b/empty";

			if ( strcmp (baseMapPart, "") )
			{
				strcat( maps, baseMapPart );
				strcat( maps, " " );
				strcat( coords, va ("%i %i ", b*16, a*16 ) );
			}

		}

	Cbuf_AddText( va( "map \"%s\" \"%s\"\n", maps, coords ) );
}


/*
======================
MN_NewBases
======================
*/
void MN_NewBases( void )
{
	// reset bases
	int i;
	baseID = 0;
	for ( i = 0; i < numBases; i++ )
		MN_ClearBase( &bmBases[i] );
}


/*
======================
MN_SaveBases
======================
*/
void MN_SaveBases( sizebuf_t *sb )
{
	// save bases
	base_t *base;
	int i, n;

	n = 0;
	for ( i = 0, base = bmBases; i < numBases; i++, base++ )
		if ( base->founded ) n++;
	MSG_WriteByte( sb, n );

	for ( i = 0, base = bmBases; i < numBases; i++, base++ )
		if (base->founded )
		{
			MSG_WriteString( sb, base->title );
			MSG_WriteFloat( sb, base->pos[0] );
			MSG_WriteFloat( sb, base->pos[1] );
			SZ_Write( sb, &base->map[0][0][0], sizeof(base->map) );
		}
}

void B_AssembleRandomBase( void )
{
	int i = 0;
	int cnt = 0;
	for ( i = 1; i < numBases; i++ )
	{
		if ( ! bmBases[i].founded ) break;
		cnt++;
	}

	Cbuf_AddText( va("base_assemble %i", rand() % cnt ) );
}

/*
======================
MN_LoadBases
======================
*/
void MN_LoadBases( sizebuf_t *sb )
{
	// load bases
	base_t *base;
	int i, num;

	MN_NewBases();
	num = MSG_ReadByte( sb );

	for ( i = 0, base = bmBases; i < num; i++, base++ )
	{
		base->founded = true;
		strcpy( base->title, MSG_ReadString( sb ) );
		base->pos[0] = MSG_ReadFloat( sb );
		base->pos[1] = MSG_ReadFloat( sb );
		memcpy( &base->map[0][0][0], sb->data + sb->readcount, sizeof(base->map) );
		sb->readcount += sizeof(base->map);
	}
}

/*
======================
MN_ResetBaseManagement
======================
*/
void MN_ResetBaseManagement( void )
{
	// reset menu structures
	numBuildings = 0;
	numProductions = 0;
	numBases = MAX_BASES;
	baseID = 0;

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
	Cmd_AddCommand( "mn_prev_building", MN_PrevBuilding );
	Cmd_AddCommand( "mn_next_building", MN_NextBuilding );
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
	Cmd_AddCommand( "buildinginfo_click", MN_BuildingInfoClick_f );
	Cmd_AddCommand( "buildings_click", MN_BuildingClick_f );
	Cmd_AddCommand( "basemap_click", MN_BaseMapClick_f );
	Cvar_SetValue( "mn_base_id", baseID );
}

/*
B_GetCount

returns the number of founded bases
*/
int B_GetCount ( void )
{
	int i, cnt = 0;

	for ( i = 0; i < numBases; i++ )
	{
		if ( ! bmBases[i].founded ) continue;
		cnt++;
	}

	return cnt;
}

/*==========================
CL_UpdateBaseData
==========================*/
void CL_UpdateBaseData( void )
{
	building_t *b;
	int i, j, k;
	int newBuilding = 0;
	for ( i = 0; i < numBases; i++ )
	{
		if ( ! bmBases[i].founded ) continue;
		for ( j = 0; j < numBuildings; j++ )
		{
			b = &bmBuildings[i][j];
			if ( ! b ) continue;
			for ( k = 0; k < b->howManyOfThisType + 1; k++ )
			{
				if ( b->buildingStatus[k] != B_UNDER_CONSTRUCTION )
					continue;
				if ( b->timeStart && ( b->timeStart + b->buildTime ) <= ccs.date.day )
				{
					b->buildingStatus[k] = B_WORKING_100;

					if ( b->onConstruct )
						Cbuf_AddText( b->onConstruct );

					if ( b->minWorkers )
					{
						Cbuf_AddText( va( "add_workers %i\n", b->minWorkers ) );
						Cbuf_Execute();
					}
					if ( b->moreThanOne )
						b->howManyOfThisType++;
					if ( ! newBuilding )
						Com_sprintf( infoBuildingText, MAX_MENUTEXTLEN, _("Building %s at base %s\n"), b->title, bmBases[i].title );
					else
						Com_sprintf( infoBuildingText, MAX_MENUTEXTLEN, _("Constructions finished at base %s\n"), bmBases[i].title );

					newBuilding++;
				}
			}
#if 0
			if ( b->buildingStatus[b->howManyOfThisType] == B_UNDER_CONSTRUCTION && b->timeStart + b->buildTime < ccs.date.day )
				b->buildingStatus[b->howManyOfThisType] = B_CONSTRUCTION_FINISHED;
#endif
		}
		// refresh the building list
		// and show a popup
		if ( newBuilding )
		{
			MN_BuildingInit();
			MN_Popup( _("Construction finished"), infoBuildingText );
		}
	}
	CL_CheckResearchStatus();
}

