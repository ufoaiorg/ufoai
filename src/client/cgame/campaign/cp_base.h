/**
 * @file cp_base.h
 * @brief Header for base management related stuff.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CP_BASE_H
#define CP_BASE_H

#include "cp_capacity.h"
#include "cp_aliencont.h"
#include "cp_produce.h"
#include "cp_building.h"

#define MAX_BASES 8

#define MAX_BUILDINGS		32
#define MAX_BASETEMPLATES	5

#define MAX_BATTERY_DAMAGE	50
#define MAX_BASE_DAMAGE		100
#define MAX_BASE_SLOT		4

/** @todo take the values from scriptfile */
#define BASEMAP_SIZE_X		778.0
#define BASEMAP_SIZE_Y		672.0

/* see MAX_TILESTRINGS */
#define BASE_SIZE		5
#define MAX_BASEBUILDINGS	BASE_SIZE * BASE_SIZE

#define MAX_EMPLOYEES_IN_BUILDING 64

#define MAX_BLOCKEDFIELDS	4
#define MIN_BLOCKEDFIELDS	1

/** basetiles are 16 units in each direction
 * 512 / UNIT_SIZE = 16
 * 512 is the size in the mapeditor and the worldplane for a
 * single base map tile */
#define BASE_TILE_SIZE 512
#define BASE_TILE_UNITS (BASE_TILE_SIZE / UNIT_SIZE)

#define BASE_INITIALINTEREST 1.0

#define B_IsUnderAttack(base) ((base)->baseStatus == BASE_UNDER_ATTACK)

#define B_AtLeastOneExists() (B_GetNext(NULL) != NULL)

/**
 * @brief Possible base states
 * @note: Don't change the order or you have to change the basemenu scriptfiles, too
 */
typedef enum {
	BASE_NOT_USED,
	BASE_UNDER_ATTACK,	/**< base is under attack */
	BASE_WORKING,		/**< nothing special */
	BASE_DESTROYED		/**< the base is being destroyed (prevent double destroy attempt) */
} baseStatus_t;

/** @brief All possible base actions */
typedef enum {
	BA_NONE,
	BA_NEWBUILDING,		/**< hovering the needed base tiles for this building */

	BA_MAX
} baseAction_t;

typedef struct baseBuildingTile_s {
	building_t *building;	/**< NULL if free spot */
	qboolean	blocked;	/**< qtrue if the tile is usable for buildings otherwise it's qfalse (blocked somehow). */
	/* These are only used for baseTemplates: */
	int posX;	/**< The x screen coordinate for the building on the basemap. */
	int posY;	/**< The y screen coordinate for the building on the basemap. */
} baseBuildingTile_t;

typedef struct baseWeapon_s {
	/* int idx; */
	aircraftSlot_t slot;	/**< Weapon. */
	aircraft_t *target;		/**< Aimed target for the weapon. */
	qboolean autofire;		/**< If it should automatically open fire on ufos */
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

	aircraft_t *aircraftCurrent;		/**< Currently selected aircraft in _this base_. (i.e. an entry in base_t->aircraft). */

	baseStatus_t baseStatus; /**< the current base status */

	float alienInterest;	/**< How much aliens know this base (and may attack it) */

	struct radar_s radar;

	aliensCont_t alienscont[MAX_ALIENCONT_CAP];	/**< alien containment capacity */

	capacities_t capacities[MAX_CAP];		/**< Capacities. */

	equipDef_t storage;	/**< weapons, etc. stored in base */

	inventory_t bEquipment;	/**< The equipment of the base; needn't be saved */

	baseWeapon_t batteries[MAX_BASE_SLOT];	/**< Missile batteries assigned to base. */
	int numBatteries;
	int numActiveBatteries;
	baseWeapon_t lasers[MAX_BASE_SLOT];		/**< Laser batteries assigned to base. */
	int numLasers;
	int numActiveLasers;

	/* we will allow only one active production at the same time for each base */
	production_queue_t productions;

	qboolean selected;		/**< the current selected base */
	building_t *buildingCurrent; /**< needn't be saved */
} base_t;

/** @brief template for creating a base */
typedef struct baseTemplate_s {
	char* id;			/**< ID of the base template */
	baseBuildingTile_t buildings[MAX_BASEBUILDINGS]; /**< the buildings to be built for this template. */
	int numBuildings;		/**< Number of buildings in this template. */
} baseTemplate_t;

void B_UpdateBaseData(void);
float B_GetMaxBuildingLevel(const base_t* base, const buildingType_t type);
void B_ParseBaseTemplate(const char *name, const char **text);
void B_BaseResetStatus(base_t* const base);
const building_t *B_GetBuildingInBaseByType(const base_t* base, buildingType_t type, qboolean onlyWorking);
const baseTemplate_t *B_GetBaseTemplate(const char *baseTemplateName);

void B_InitStartup(void);

/* base functions */
base_t *B_Build(const struct campaign_s *campaign, const vec2_t pos, const char *name);
void B_SetUpFirstBase(const struct campaign_s *campaign, base_t* base);
base_t *B_GetNext(base_t *lastBase);
base_t* B_GetBaseByIDX(int baseIdx);
base_t* B_GetFoundedBaseByIDX(int baseIdx);
int B_GetCount(void);
void B_SelectBase(const base_t *base);
void B_Destroy(base_t *base);
void B_SetName(base_t *base, const char *name);

base_t *B_GetFirstUnfoundedBase(void);
base_t *B_GetCurrentSelectedBase(void);
void B_SetCurrentSelectedBase(const base_t *base);

qboolean B_AssembleMap(const base_t *base);

/* building functions */
#define B_IsTileBlocked(base, x, y) (base)->map[(int)(y)][(int)(x)].blocked
#define B_GetBuildingAt(base, x, y) (base)->map[(int)(y)][(int)(x)].building

buildingType_t B_GetBuildingTypeByCapacity(baseCapacities_t cap);

building_t* B_GetNextBuilding(const base_t *base, building_t *lastBuilding);
building_t* B_GetNextBuildingByType(const base_t *base, building_t *lastBuilding, buildingType_t buildingType);
void B_BuildingStatus(const building_t* building);
qboolean B_CheckBuildingTypeStatus(const base_t* const base, buildingType_t type, buildingStatus_t status, int *cnt);
qboolean B_GetBuildingStatus(const base_t* const base, const buildingType_t type);
void B_SetBuildingStatus(base_t* const base, const buildingType_t type, qboolean newStatus);

qboolean B_MapIsCellFree(const base_t *base, int col, int row);
building_t* B_SetBuildingByClick(base_t *base, const building_t const *buildingTemplate, int row, int col);
qboolean B_IsBuildingDestroyable(const building_t *building);
qboolean B_BuildingDestroy(building_t* building);

building_t *B_GetFreeBuildingType(buildingType_t type);
int B_GetNumberOfBuildingsInBaseByTemplate(const base_t *base, const building_t *type);
int B_GetNumberOfBuildingsInBaseByBuildingType(const base_t *base, const buildingType_t type);

void B_BuildingOpenAfterClick(const building_t *building);
void B_ResetBuildingCurrent(base_t* base);

/* storage functions */
qboolean B_ItemIsStoredInBaseStorage(const objDef_t *obj);
qboolean B_BaseHasItem(const base_t *base, const objDef_t *item);
int B_ItemInBase(const objDef_t *item, const base_t *base);

int B_AddToStorage(base_t* base, const objDef_t *obj, int amount);

/* aircraft functions */
void B_AircraftReturnedToHomeBase(aircraft_t* aircraft);
void B_DumpAircraftToHomeBase(aircraft_t *aircraft);

/* capacity functions */
void B_UpdateBaseCapacities(baseCapacities_t cap, base_t *base);
qboolean B_UpdateStorageAndCapacity(base_t* base, const objDef_t *obj, int amount, qboolean ignorecap);
baseCapacities_t B_GetCapacityFromBuildingType(buildingType_t type);
void B_ResetAllStatusAndCapacities(base_t *base, qboolean firstEnable);

/* menu functions */
void B_BaseMenuInit(const base_t *base);
void B_DrawBuilding(const building_t* building);

/* antimatter */
int B_AntimatterInBase(const base_t *base);
void B_ManageAntimatter(base_t *base, int amount, qboolean add);

/* savesystem */
void B_SaveBaseSlotsXML(const baseWeapon_t *weapons, const int numWeapons, xmlNode_t *p);
int B_LoadBaseSlotsXML(baseWeapon_t* weapons, int numWeapons, xmlNode_t *p);
qboolean B_SaveStorageXML(xmlNode_t *parent, const equipDef_t equip);
qboolean B_LoadStorageXML(xmlNode_t *parent, equipDef_t *equip);

/* other */
int B_GetInstallationLimit(void);

/* functions that checks whether the buttons in the base menu are useable */
qboolean BS_BuySellAllowed(const base_t* base);
qboolean AIR_AircraftAllowed(const base_t* base);
qboolean RS_ResearchAllowed(const base_t* base);
qboolean PR_ProductionAllowed(const base_t* base);
qboolean E_HireAllowed(const base_t* base);
qboolean AC_ContainmentAllowed(const base_t* base);
qboolean HOS_HospitalAllowed(const base_t* base);

#endif
