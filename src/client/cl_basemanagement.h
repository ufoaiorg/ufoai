/**
 * @file cl_basemangement.h
 * @brief Header for base management related stuff.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_BASEMANGEMENT_H
#define CLIENT_CL_BASEMANGEMENT_H

#define REPAIRTIME 14
#define REPAIRCOSTS 10

#define UPGRADECOSTS 100
#define UPGRADETIME  2

#define MAX_PRODUCTIONS		256

#define MAX_LIST_CHAR		1024
#define MAX_BUILDINGS		256
#define MAX_BASES		8
#define MAX_DESC		256

#define SCROLLSPEED		1000

/* this is not best - but better than hardcoded every time i used it */
#define RELEVANT_X		187.0
#define RELEVANT_Y		280.0
/* take the values from scriptfile */
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

#define BASE_SIZE		5
#define MAX_BASE_LEVELS		1

#define MAX_EMPLOYEES_IN_BUILDING 64
#define MAX_EMPLOYEES 256

/* Struct to be used in building definition - List of employees. */
typedef struct employees_s {
	int assigned[MAX_EMPLOYEES_IN_BUILDING];	/* List of employees (links to global list). */
	int numEmployees;			/* Current number of employees. */
	int maxEmployees;			/* Max. number of employees (from config file) */
	float cost_per_employee;	/* Costs per employee that are added to toom-total-costs- */
} employees_t;

/* allocate memory for menuText[TEXT_STANDARD] contained the information about a building */
char buildingText[MAX_LIST_CHAR];

typedef enum {
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,
	BASE_WORKING
} baseStatus_t;

typedef enum {
	B_STATUS_NOT_SET,			/* not build yet */
	B_STATUS_UNDER_CONSTRUCTION,	/* right now under construction */
	B_STATUS_CONSTRUCTION_FINISHED,	/* construction finished - no workers assigned */
	/* and building needs workers */
	B_STATUS_WORKING,			/* working normal (or workers assigned when needed) */
	B_STATUS_DOWN				/* totally damaged */
} buildingStatus_t;

typedef enum {
	B_MISC,
	B_LAB,
	B_QUARTERS,
	B_WORKSHOP,
	B_HANGAR
} buildingType_t;

typedef struct building_s {
	int idx;					/* self link in "buildings" list. */
	int type_idx;				/* self link in "buildingTypes" list. */
	int base_idx;				/* Number/index of base this building is located in. */

	char id[MAX_VAR];
	char name[MAX_VAR];
	/* needs determines the second building part */
	char image[MAX_VAR], mapPart[MAX_VAR], pedia[MAX_VAR];
	char needs[MAX_VAR];		/* if the buildign has a second part */
	float fixCosts, varCosts;

	int timeStart, buildTime;

	/* A list of employees assigned to this building. */
	/* FIXME: Remove me - only use global employee list */
	employees_t assigned_employees;

	/*if we can build more than one building of the same type: */
	buildingStatus_t buildingStatus;	/*[BASE_SIZE*BASE_SIZE]; */

	qboolean visible;
	/* needed for baseassemble */
	/* when there are two tiles (like hangar) - we only load the first tile */
	int used;

	/* event handler functions */
	char onConstruct[MAX_VAR];
	char onAttack[MAX_VAR];
	char onDestroy[MAX_VAR];
	char onUpgrade[MAX_VAR];
	char onRepair[MAX_VAR];
	char onClick[MAX_VAR];

	/*more than one building of the same type allowed? */
	int moreThanOne;

	/*position of autobuild */
	vec2_t pos;

	/*autobuild when base is set up */
	qboolean autobuild;

	/*autobuild when base is set up */
	qboolean firstbase;

	/*this way we can rename the buildings without loosing the control */
	buildingType_t buildingType;

	int tech;					/* link to the building-technology */
	/* if the building needs another one to work (= to be buildable) .. links to gd.buildingTypes */
	int dependsBuilding;
} building_t;

typedef struct base_s {
	int idx;					/* self link */
	char name[MAX_VAR];
	int map[BASE_SIZE][BASE_SIZE];

	qboolean founded;
	vec2_t pos;

	/* to decide which actions are available in the basemenu */
	qboolean hasHangar;
	qboolean hasLab;			/* TODO: still needed. */

	/*this is here to allocate the needed memory for the buildinglist */
	char allBuildingsList[MAX_LIST_CHAR];

	/*mapChar indicated which map to load (gras, desert, arctic,...) */
	/*d=desert, a=arctic, g=gras */
	char mapChar;

	/* all aircraft in this base */
	/* FIXME: make me a linked list (see cl_market.c aircraft selling) */
	aircraft_t aircraft[MAX_AIRCRAFT];
	int numAircraftInBase;
	int aircraftCurrent;

	int posX[BASE_SIZE][BASE_SIZE];
	int posY[BASE_SIZE][BASE_SIZE];

	int condition;

	baseStatus_t baseStatus;

	/* these are max. 19bits - see cvar cl_numnames */
	/* hiredMask is teamMask from current selected aircraft */
	int hiredMask;				/* hired mask for all soldiers in base */

	/* these are 31bit masks (see baseCurrent->numWholeTeam should never exceed MAX_WHOLETEAM-1) */
	/* NOTE: we only use the first 19 (because we can only select from 19 soldiers) */
	/* see cl_numnames cvars */
	int teamMask[MAX_AIRCRAFT];	/* assigned to a specific aircraft */
	int deathMask;				/* mask of soldiers (relative to wholeTeam) that are died */

	/* these should not be bigger than MAX_ACTIVETEAM */
	int numHired;
	int numOnTeam[MAX_AIRCRAFT];

	/* the onconstruct value of the buliding */
	/* building_radar increases the sensor width */
	radar_t	radar;

	/* equipment in base */
	inventory_t equipment;

	/* set in CL_GenerateEquipmentCmd and CL_LoadTeam */
	character_t *curTeam[MAX_ACTIVETEAM];
	character_t *curChr;

	int equipType;
	/* unified character id (base dependent) */
	int nextUCN;

	/* needed if there is another buildingpart to build (link to gd.buildingTypes) */
	int buildingToBuild;

	struct building_s *buildingCurrent;
} base_t;

/* Currently displayed/accessed base. */
extern base_t *baseCurrent;

void B_SetSensor(void);

void B_UpdateBaseData(void);
base_t *B_GetBase(int id);
void B_UpgradeBuilding(building_t * b);
void B_RepairBuilding(building_t * b);
int B_CheckBuildingConstruction(building_t * b, int baseID);
int B_GetNumOnTeam(void);
building_t *B_GetLab(int base_id);
void B_ClearBuilding(building_t * building);
void B_ParseBuildings(char *id, char **text, qboolean link);
void B_ParseBases(char *title, char **text);
void B_BuildingInit(void);
void B_AssembleMap(void);
void B_BaseAttack(void);
building_t *B_GetBuildingByIdx(int idx);
building_t *B_GetBuildingInBase(base_t* base, char* buildingID);
building_t *B_GetBuildingType(char *buildingName);

extern vec2_t newBasePos;

void B_BuildingInit(void);
int B_GetCount(void);
void B_SetUpBase(void);

void B_ParseProductions(char *title, char **text);
void B_SetBuildingByClick(int row, int col);
void B_ResetBaseManagement(void);
void B_ClearBase(base_t * base);
void B_NewBases(void);
void B_BuildingStatus(void);

building_t *B_GetFreeBuildingType(buildingType_t type);
int B_GetNumberOfBuildingsInBaseByType(int base_idx, int type_idx);

#endif /* CLIENT_CL_BASEMANGEMENT_H */
