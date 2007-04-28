/**
 * @file cl_basemanagement.h
 * @brief Header for base management related stuff.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#define MAX_LIST_CHAR		1024
#define MAX_BUILDINGS		256
#define MAX_BASES		8

/** @todo take the values from scriptfile */
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

#define BASE_SIZE		5

#define MAX_EMPLOYEES_IN_BUILDING 64

/** @brief allocate memory for menuText[TEXT_STANDARD] contained the information about a building */
char buildingText[MAX_LIST_CHAR];

/**
 * @brief Possible base states
 * @note: Don't change the order or you have to change the basemenu scriptfiles, too
 */
typedef enum {
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,	/**< base is under attack */
	BASE_WORKING		/**< nothing special */
} baseStatus_t;

/** @brief All possible building status. */
typedef enum {
	B_STATUS_NOT_SET,			/**< not build yet */
	B_STATUS_UNDER_CONSTRUCTION,	/**< right now under construction */
	B_STATUS_CONSTRUCTION_FINISHED,	/**< construction finished - no workers assigned */
	/* and building needs workers */
	B_STATUS_WORKING,			/**< working normal (or workers assigned when needed) */
	B_STATUS_DOWN				/**< totally damaged */
} buildingStatus_t;

/** @brief All different building types. */
typedef enum {
	B_MISC,			/**< this building is nothing with a special function */
	B_LAB,			/**< this building is a lab */
	B_QUARTERS,		/**< this building is a quarter */
	B_STORAGE,		/**< this building is a storage */
	B_WORKSHOP,		/**< this building is a workshop */
	B_HOSPITAL,		/**< this building is a hospital */
	B_HANGAR,		/**< this building is a hangar */
	B_ALIEN_CONTAINMENT,	/**< this building is an alien containment */
	B_SMALL_HANGAR		/**< this building is a small hangar */
} buildingType_t;

/** @brief All possible capacities in base. */
typedef enum {
	CAP_ALIENS,		/**< Alive aliens stored in base. */
	CAP_AIRCRAFTS_SMALL,	/**< Small aircrafts in base. */
	CAP_AIRCRAFTS_BIG,	/**< Big aircrafts in base. */
	CAP_EMPLOYEES,		/**< Personel in base. */
	CAP_ITEMS,		/**< Items in base. */
	CAP_LABSPACE,		/**< Space for scientists in laboratory. */
	CAP_WORKSPACE,		/**< Space for workers in workshop. */
	CAP_HOSPSPACE,		/**< Space for hurted people in hospital. */

	MAX_CAP
} baseCapacities_t;

/** @brief Store capacities in base. */
typedef struct cap_maxcur_s {
	int max;		/**< Maximum capacity. */
	int cur;		/**< Currently used capacity. */
} capacities_t;

/** @brief A building with all it's data */
typedef struct building_s {
	int idx;					/**< self link in "buildings" list. */
	int type_idx;				/**< self link in "buildingTypes" list. */
	int base_idx;				/**< Number/index of base this building is located in. */

	char id[MAX_VAR];
	char name[MAX_VAR];
	char image[MAX_VAR], mapPart[MAX_VAR], pedia[MAX_VAR];
	/** needs determines the second building part */
	char needs[MAX_VAR];		/**< if the buildign has a second part */
	float fixCosts, varCosts;

	int timeStart, buildTime;

	buildingStatus_t buildingStatus;	/**< [BASE_SIZE*BASE_SIZE]; */

	/** if we can build more than one building of the same type: */
	qboolean visible;	/**< is this building visible in the building list */
	/** needed for baseassemble
	 * when there are two tiles (like hangar) - we only load the first tile */
	int used;

	/** event handler functions */
	char onConstruct[MAX_VAR];
	char onAttack[MAX_VAR];
	char onDestroy[MAX_VAR];
	char onUpgrade[MAX_VAR];
	char onRepair[MAX_VAR];
	char onClick[MAX_VAR];

	/** more than one building of the same type allowed? */
	int moreThanOne;

	/** position of autobuild */
	vec2_t pos;

	/** autobuild when base is set up */
	qboolean autobuild;

	/** autobuild when base is set up */
	qboolean firstbase;

	/** How many employees to hire on construction in the first base */
	int maxEmployees;

	/** this way we can rename the buildings without loosing the control */
	buildingType_t buildingType;

	int tech;					/**< link to the building-technology */
	/** if the building needs another one to work (= to be buildable) .. links to gd.buildingTypes */
	int dependsBuilding;

	/** this value is used by many buildings in a different way
	 * e.g. aliencont uses it for - how many aliens can be 'stored'
	 * the hangar uses it for - how many aircraft are manageable here
	 */
	int capacity;
} building_t;

/** @brief A base with all it's data */
typedef struct base_s {
	int idx;					/**< Self link. Index in the global base-list. */
	char name[MAX_VAR];			/**< Name of the base */
	int map[BASE_SIZE][BASE_SIZE];	/**< the base maps (holds building ids) */

	qboolean founded;	/**< already founded? */
	vec3_t pos;		/**< pos on geoscape */

	/** to decide which actions are available in the basemenu */
	qboolean hasHangar;		/**< does this base has a hangar */
	qboolean hasLab;		/**< does this base has a lab */
	qboolean hasHospital;		/**< does this base has a hospital */
	qboolean hasAlienCont;		/**< does this base has a alien containment */
	qboolean hasStorage;		/**< does this base has a storage */
	qboolean hasQuarters;		/**< does this base has quarters */
	qboolean hasWorkshop;		/**< does this base has a workshop */
	qboolean hasHangarSmall;	/**< does this base has a small hangar */

	/** this is here to allocate the needed memory for the buildinglist */
	char allBuildingsList[MAX_LIST_CHAR];

	/** mapChar indicated which map to load (gras, desert, arctic,...)
	 * d=desert, a=arctic, g=gras */
	char mapChar;

	/** all aircraft in this base
	  @todo: make me a linked list (see cl_market.c aircraft selling) */
	aircraft_t aircraft[MAX_AIRCRAFT];
	int numAircraftInBase;	/**< How many aircraft are in this base. */
	int aircraftCurrent;		/**< Index of the currently selected aircraft in this base (NOT a global one). Max is numAircraftInBase-1  */

	int posX[BASE_SIZE][BASE_SIZE];	/**< the x coordinates for the basemap (see map[BASE_SIZE][BASE_SIZE]) */
	int posY[BASE_SIZE][BASE_SIZE];	/**< the y coordinates for the basemap (see map[BASE_SIZE][BASE_SIZE]) */

	int condition; /**< base condition
			@todo: implement this */

	baseStatus_t baseStatus; /**< the current base status */

	int teamNum[MAX_AIRCRAFT];		/**< Number of soldiers assigned to a specific aircraft. */

	radar_t	radar;	/**< the onconstruct value of the buliding building_radar increases the sensor width */


	aliensCont_t alienscont[MAX_ALIENCONT_CAP];	/**< alien containment capacity */

	capacities_t capacities[MAX_CAP];		/**< Capacities. */

	equipDef_t storage;	/**< weapons, etc. stored in base */

	inventory_t equipByBuyType;	/**< idEquip sorted by buytype; needen't be saved;
		a hack based on assertion (MAX_CONTAINERS >= BUY_AIRCRAFT) ... see e.g. CL_GenerateEquipment_f */

	int equipType;	/**< the current selected category in equip menu */

	character_t *curTeam[MAX_ACTIVETEAM];	/**< set in CL_GenerateEquipment_f and CL_LoadTeamMultiplayer */
	character_t *curChr;	/**< needn't be saved */

	int buildingToBuild;	/**< needed if there is another buildingpart to build (link to gd.buildingTypes) */

	struct building_s *buildingCurrent; /**< needn't be saved */
} base_t;

/** Currently displayed/accessed base. */
extern base_t *baseCurrent;

void B_SetSensor_f(void);

void B_UpdateBaseData(void);
base_t *B_GetBase(int id);
int B_CheckBuildingConstruction(building_t *b, int baseID);
int B_GetNumOnTeam(void);
building_t *B_GetLab(int base_id);
void B_ClearBuilding(building_t *building);
void B_ParseBuildings(const char *name, char **text, qboolean link);
void B_ParseBases(const char *name, char **text);
void B_BaseAttack(base_t* const base);
void B_BaseResetStatus (base_t* const base);
building_t *B_GetBuildingByIdx(base_t* base, int idx);
/*building_t *B_GetBuildingInBase(base_t* base, char* buildingID);*/
building_t *B_GetBuildingType(const char *buildingName);

/** Coordinates to place the new base at (long, lat) */
extern vec3_t newBasePos;

int B_GetCount(void);
void B_SetUpBase(base_t* base);

void B_SetBuildingByClick(int row, int col);
void B_ResetBaseManagement(void);
void B_ClearBase(base_t *const base);
void B_NewBases(void);
void B_BuildingStatus(void);

building_t *B_GetFreeBuildingType(buildingType_t type);
int B_GetNumberOfBuildingsInBaseByTypeIDX(int base_idx, int type_idx);
int B_GetNumberOfBuildingsInBaseByType(int base_idx, buildingType_t type);

int B_ItemInBase(int item_idx, base_t *base);

aircraft_t *B_GetAircraftFromBaseByIndex(base_t* base, int index);
void B_ReviveSoldiersInBase(base_t* base); /* TODO */

int B_GetAvailableQuarterSpace(const base_t* const base);
#if 0
/*
@10042007 Zenerka - not needed/used anymore.
*/
int B_GetAvailableLabSpace(const base_t* const base);
#endif
int B_GetEmployeeCount(const base_t* const base);

qboolean B_CheckBuildingTypeStatus(const base_t* const base, buildingType_t type, buildingStatus_t status, int *cnt);

void B_BuildingDestroy(building_t* building, base_t* base);
void CL_DropshipReturned(base_t* base, aircraft_t* aircraft);

void B_UpdateBaseCapacities (baseCapacities_t cap, base_t *base);

#endif /* CLIENT_CL_BASEMANGEMENT_H */
