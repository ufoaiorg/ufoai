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

/* FIXME: This value is in menu_geoscape, too */
/*              make this variable?? */
#define BASE_COSTS		100000

#define MAX_BATTERY_DAMAGE	50
#define MAX_BASE_DAMAGE		100
#define MAX_BASE_SLOT		4

/** @todo take the values from scriptfile */
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

/* see MAX_TILESTRINGS */
#define BASE_SIZE		5

#define MAX_EMPLOYEES_IN_BUILDING 64

/**
 * @brief Possible base states
 * @note: Don't change the order or you have to change the basemenu scriptfiles, too
 */
typedef enum {
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,	/**< base is under attack */
	BASE_WORKING		/**< nothing special */
} baseStatus_t;

/** @brief All possible base actions */
typedef enum {
	BA_NONE,
	BA_NEWBUILDING,		/**< hovering the needed base tiles for this building */

	BA_MAX
} baseAction_t;

/** @brief All possible building status. */
typedef enum {
	B_STATUS_NOT_SET,			/**< not build yet */
	B_STATUS_UNDER_CONSTRUCTION,	/**< right now under construction */
	B_STATUS_CONSTRUCTION_FINISHED,	/**< construction finished - no workers assigned */
	/* and building needs workers */
	B_STATUS_WORKING,			/**< working normal (or workers assigned when needed) */
	B_STATUS_DOWN				/**< totally damaged */
} buildingStatus_t;

/** @brief All different building types.
 * @note if you change the order, you'll load values of hasBuilding in wrong indice */
typedef enum {
	B_MISC,			/**< this building is nothing with a special function (used when a building appears twice in .ufo file) */
	B_LAB,			/**< this building is a lab */
	B_QUARTERS,		/**< this building is a quarter */
	B_STORAGE,		/**< this building is a storage */
	B_WORKSHOP,		/**< this building is a workshop */
	B_HOSPITAL,		/**< this building is a hospital */
	B_HANGAR,		/**< this building is a hangar */
	B_ALIEN_CONTAINMENT,	/**< this building is an alien containment */
	B_SMALL_HANGAR,		/**< this building is a small hangar */
	B_UFO_HANGAR,		/**< this building is a UFO hangar */
	B_UFO_SMALL_HANGAR,	/**< this building is a small UFO hangar */
	B_POWER,		/**< this building is power plant */
	B_COMMAND,		/**< this building is command centre */
	B_ANTIMATTER,		/**< this building is antimatter storage */
	B_ENTRANCE,		/**< this building is an entrance */
	B_DEFENSE_MISSILE,		/**< this building is a missile rack */
	B_DEFENSE_LASER,		/**< this building is a laser battery */
	B_RADAR,			/**< this building is a radar */
	B_TEAMROOM,			/**< this building is a Team Room */

	MAX_BUILDING_TYPE
} buildingType_t;

/** @brief All possible capacities in base. */
typedef enum {
	CAP_ALIENS,		/**< Alive aliens stored in base. */
	CAP_AIRCRAFTS_SMALL,	/**< Small aircraft in base. */
	CAP_AIRCRAFTS_BIG,	/**< Big aircraft in base. */
	CAP_EMPLOYEES,		/**< Personel in base. */
	CAP_ITEMS,		/**< Items in base. */
	CAP_LABSPACE,		/**< Space for scientists in laboratory. */
	CAP_WORKSPACE,		/**< Space for workers in workshop. */
	CAP_HOSPSPACE,		/**< Space for hurted people in hospital. */
	CAP_UFOHANGARS_SMALL,	/**< Space for small recovered UFOs. */
	CAP_UFOHANGARS_LARGE,	/**< Space for small and large recovered UFOs. */
	CAP_ANTIMATTER,		/**< Space for Antimatter Storage. */

	MAX_CAP
} baseCapacities_t;

/** @brief Store capacities in base. */
typedef struct cap_maxcur_s {
	int max;		/**< Maximum capacity. */
	int cur;		/**< Currently used capacity. */
} capacities_t;

/** @brief A building with all it's data. */
typedef struct building_s {
	int idx;				/**< Index in in "buildings" list.
							 * @todo What value is this supposed to be for building_t entries in buildingTemplates? */
	struct building_s *tpl;	/**< Self link in "buildingTemplates" list. */
	struct base_s *base;	/**< The base this building is located in. */

	char *id;
	char *name;
	char *image, *mapPart, *pedia;

	char *needs;		/**< "needs" determines the second building part. */
	int fixCosts, varCosts;

	/**
	 * level of the building.
	 * @note This value depends on the implementation of the affected building
	 * might e.g. be an factor */
	float level;

	int timeStart, buildTime;

	buildingStatus_t buildingStatus;	/**< [BASE_SIZE*BASE_SIZE]; */

	qboolean visible;	/**< Is this building visible in the building list. */
	/** needed for baseassemble
	 * when there are two tiles (like hangar) - we only load the first tile */
	int used;

	/** Event handler functions */
	char onConstruct[MAX_VAR];
	char onAttack[MAX_VAR];
	char onDestroy[MAX_VAR];
	char onUpgrade[MAX_VAR];
	char onRepair[MAX_VAR];
	char onClick[MAX_VAR];

	int moreThanOne;	/**< More than one building of the same type allowed? */


	vec2_t pos;			/**< Position of autobuild. */
	qboolean autobuild;	/**< Autobuild when base is set up. */
	qboolean firstbase;	/**< Autobuild when first base is set up. */

	/** How many employees to hire on construction in the first base */
	int maxEmployees;

	buildingType_t buildingType;	/**< This way we can rename the buildings without loosing the control. @note Not to be confused with "tpl".*/
	technology_t *tech;				/**< Link to the building-technology. */
	struct building_s *dependsBuilding;	/**< If the building needs another one to work (= to be buildable). @sa "buildingTemplates" list*/

	int capacity;		/**< Capacity of this building (used in calculate base capacities). */
} building_t;

typedef struct baseBuildingTile_s {
	building_t *building;	/**< NULL if free spot */
	qboolean	blocked;	/**< qtrue if the tile is usable for buildings otherwise it's qfalse (blocked somehow). */
	int posX;	/**< The x screen coordinate for the building on the basemap. */
	int posY;	/**< The y screen coordinate for the building on the basemap. */
} baseBuildingTile_t;

typedef struct baseWeapon_s {
	/* int idx; */
	aircraftSlot_t slot;	/**< Weapon. */
	aircraft_t *target;		/**< Aimed target for the weapon. */
} baseWeapon_t;

/** @brief A base with all it's data */
typedef struct base_s {
	int idx;					/**< Self link. Index in the global base-list. */
	char name[MAX_VAR];			/**< Name of the base */
	baseBuildingTile_t map[BASE_SIZE][BASE_SIZE];	/**< The base maps (holds building pointers)
													 * @todo  maybe integrate BASE_INVALID_SPACE and BASE_FREE_SPACE here? */

	qboolean founded;	/**< already founded? */
	vec3_t pos;		/**< pos on geoscape */

	/**
	 * @note These qbooleans does not say whether there is such building in the
	 * base or there is not. They are true only if such buildings are operational
	 * (for example, in some cases, if they are provided with power).
	 */
	qboolean hasBuilding[MAX_BUILDING_TYPE];

	/** this is here to allocate the needed memory for the buildinglist */
	char allBuildingsList[MAX_LIST_CHAR];

	/** mapZone indicated which map to load (grass, desert, arctic,...) */
	const char *mapZone;

	/** All aircraft in this base
	  @todo make me a linked list (see cl_market.c aircraft selling) */
	aircraft_t aircraft[MAX_AIRCRAFT];
	int numAircraftInBase;	/**< How many aircraft are in this base. */
	aircraft_t *aircraftCurrent;		/**< Currently selected aircraft in _this base_. (i.e. an entry in base_t->aircraft). */

	baseStatus_t baseStatus; /**< the current base status */

	float alienInterest;	/**< How much aliens know this base (and may attack it) */

	radar_t	radar;	/**< the onconstruct value of the buliding building_radar increases the sensor width */

	aliensCont_t alienscont[MAX_ALIENCONT_CAP];	/**< alien containment capacity */

	capacities_t capacities[MAX_CAP];		/**< Capacities. */

	equipDef_t storage;	/**< weapons, etc. stored in base */

	inventory_t equipByBuyType;	/**< idEquip sorted by buytype; needen't be saved;
		a hack based on assertion (MAX_CONTAINERS >= BUY_AIRCRAFT) ... see e.g. CL_GenerateEquipment_f */

	int equipType;	/**< the current selected category in equip menu */

	character_t *curChr;	/**< needn't be saved */

	int hospitalList[MAX_EMPL][MAX_EMPLOYEES];	/**< Hospital list of employees currently being healed. */
	int hospitalListCount[MAX_EMPL];				/**< Counter for hospitalList. */
	int hospitalMissionList[MAX_EMPLOYEES];		/**< Hospital list of soldiers being healed but moved to the mission. */
	int hospitalMissionListCount;			/**< Counter for hospitalMissionList. */

	baseWeapon_t batteries[MAX_BASE_SLOT];	/**< Missile batteries assigned to base. */
	int numBatteries;
	baseWeapon_t lasers[MAX_BASE_SLOT];		/**< Laser batteries assigned to base. */
	int numLasers;

	int batteryDamage;			/**< Hit points of defense system */
	int baseDamage;			/**< Hit points of base */

	building_t *buildingToBuild;	/**< needed if there is another buildingpart to build (link to gd.buildingTemplates) */

	building_t *buildingCurrent; /**< needn't be saved */
} base_t;

/** Currently displayed/accessed base. */
extern base_t *baseCurrent;

void B_UpdateBaseData(void);
int B_CheckBuildingConstruction(building_t *b, base_t* base);
int B_GetNumOnTeam(const aircraft_t *aircraft);
void B_ParseBuildings(const char *name, const char **text, qboolean link);
void B_ParseBases(const char *name, const char **text);
void B_BaseAttack(base_t* const base);
void B_BaseResetStatus(base_t* const base);
building_t *B_GetBuildingInBaseByType(const base_t* base, buildingType_t type, qboolean onlyWorking);
building_t *B_GetBuildingTemplate(const char *buildingName);
buildingType_t B_GetBuildingTypeByBuildingID(const char *buildingID);

/** Coordinates to place the new base at (long, lat) */
extern vec3_t newBasePos;

int B_GetFoundedBaseCount(void);
void B_SetUpBase(base_t* base, qboolean hire, qboolean buildings);
base_t* B_GetBase(int id);

void B_SetBuildingByClick(int row, int col);
void B_ResetBaseManagement(void);
void B_ClearBase(base_t *const base);
void B_NewBases(void);
void B_BuildingStatus(base_t* base, building_t* building);

building_t *B_GetFreeBuildingType(buildingType_t type);
int B_GetNumberOfBuildingsInBaseByTemplate(base_t *base, building_t *type);
int B_GetNumberOfBuildingsInBaseByBuildingType(base_t *base, buildingType_t type);

int B_ItemInBase(const objDef_t *item, const base_t *base);

aircraft_t *B_GetAircraftFromBaseByIndex(base_t* base, int index);
void B_ReviveSoldiersInBase(base_t* base); /* @todo */

int B_GetEmployeeCount(const base_t* const base);

qboolean B_CheckBuildingTypeStatus(const base_t* const base, buildingType_t type, buildingStatus_t status, int *cnt);
qboolean B_GetBuildingStatus (const base_t* const base, buildingType_t type);
void B_SetBuildingStatus (base_t* const base, buildingType_t type, qboolean newStatus);
qboolean B_CheckBuildingDependencesStatus(const base_t* const base, building_t* building);

void B_MarkBuildingDestroy(base_t* base, building_t* building);
qboolean B_BuildingDestroy(base_t* base, building_t* building);
void CL_AircraftReturnedToHomeBase(aircraft_t* aircraft);

void B_UpdateBaseCapacities(baseCapacities_t cap, base_t *base);
qboolean B_UpdateStorageAndCapacity(base_t* base, const objDef_t *obj, int amount, qboolean reset, qboolean ignorecap);
baseCapacities_t B_GetCapacityFromBuildingType(buildingType_t type);

qboolean B_ScriptSanityCheck(void);

/* menu functions that checks whether the buttons in the base menu are useable */
qboolean BS_BuySellAllowed(const base_t* base);
qboolean AIR_AircraftAllowed(const base_t* base);
qboolean RS_ResearchAllowed(const base_t* base);
qboolean PR_ProductionAllowed(const base_t* base);
qboolean E_HireAllowed(const base_t* base);
qboolean AC_ContainmentAllowed(const base_t* base);
qboolean HOS_HospitalAllowed(const base_t* base);

#endif /* CLIENT_CL_BASEMANGEMENT_H */
