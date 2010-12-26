/**
 * @file cp_base.c
 * @brief Handles everything that is located in or accessed trough a base.
 * @note Basemanagement functions prefix: B_
 * @note See "base/ufos/basemanagement.ufo", "base/ufos/menu_bases.ufo" and "base/ufos/menu_buildings.ufo" for the underlying content.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../client.h" /* cl, cls */
#include "../cl_inventory.h" /* INV_GetEquipmentDefinitionByID */
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_missions.h"
#include "cp_map.h"
#include "cp_popup.h"
#include "cp_radar.h"
#include "cp_time.h"
#include "cp_base_callbacks.h"
#include "cp_ufo.h"
#include "save/save_base.h"

#define B_GetBuildingByIDX(baseIdx, buildingIdx) (&ccs.buildings[(baseIdx)][(buildingIdx)])
#define B_GetBuildingIDX(base, building) ((ptrdiff_t)((building) - ccs.buildings[base->idx]))
#define B_GetBaseIDX(base) ((ptrdiff_t)((base) - ccs.bases))

static void B_PackInitialEquipment(aircraft_t *aircraft, const equipDef_t *ed);

/**
 * @brief Returns the count of founded bases
 */
int B_GetCount (void)
{
	return ccs.numBases;
}

/**
 * @brief Iterates through founded base
 * @param[in] lastBase Pointer of the base to iterate from. call with NULL to get the first one.
 */
base_t *B_GetNext (base_t *lastBase)
{
	base_t* endOfBases = &ccs.bases[B_GetCount()];
	base_t* base;

	if (!B_GetCount())
		return NULL;

	if (!lastBase)
		return ccs.bases;
	assert(lastBase >= ccs.bases);
	assert(lastBase < endOfBases);

	base = lastBase;

	base++;
	if (base >= endOfBases)
		return NULL;
	else
		return base;
}

/**
 * @brief Iterates through founded base
 * @param[in] lastBase Pointer of the base to iterate from. call with NULL to get the first one.
 */
base_t* B_GetNextFounded (base_t *lastBase)
{
	base_t* base = lastBase;

	while ((base = B_GetNext(base)) != NULL) {
		if (base->founded)
			return base;
	}
	return NULL;
}

/**
 * @brief Array bound check for the base index. Will also return unfounded bases as
 * long as the index is in the valid ranges,
 * @param[in] baseIdx Index to check
 * @return Pointer to the base corresponding to baseIdx.
 */
base_t* B_GetBaseByIDX (int baseIdx)
{
	if (baseIdx >= MAX_BASES || baseIdx < 0)
		return NULL;

	return &ccs.bases[baseIdx];
}

/**
 * @brief Array bound check for the base index.
 * @param[in] baseIdx Index to check
 * @return Pointer to the base corresponding to baseIdx if base is founded, NULL else.
 */
base_t* B_GetFoundedBaseByIDX (int baseIdx)
{
	base_t *base = B_GetBaseByIDX(baseIdx);

	if (base && base->founded)
		return base;

	return NULL;
}

/**
 * @brief Iterates through buildings in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with NULL to get the first one.
 */
building_t* B_GetNextBuilding (const base_t *base, building_t *lastBuilding)
{
	building_t *endOfBuildings = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);
	building_t *building;

	if (!ccs.numBuildings[base->idx])
		return NULL;

	if (!lastBuilding)
		return ccs.buildings[base->idx];
	assert(lastBuilding >= ccs.buildings[base->idx]);
	assert(lastBuilding < endOfBuildings);

	building = lastBuilding;

	building++;
	if (building >= endOfBuildings)
		return NULL;
	else
		return building;
}

/**
 * @brief Iterates throught buildings of a type in a base
 * @param[in] base Pointer to the base which buildings asked
 * @param[in] lastBuilding Pointer to the building iterate from. Call with NULL to get the first one.
 * @param[in] buildingType Type of the buildings to search
 * @sa buildingType_t
 */
building_t* B_GetNextBuildingByType (const base_t *base, building_t *lastBuilding, buildingType_t buildingType)
{
	building_t *building = lastBuilding;

	while ((building = B_GetNextBuilding(base, building))) {
		if (building->buildingType == buildingType)
			break;
	}
	return building;
}

/**
 * @brief Searches the base for a given building type with the given status
 * @param[in] base Base to search
 * @param[in] type Building type to search
 * @param[in] status The status the building should have
 * @param[out] cnt This is a pointer to an int value which will hold the building
 * count of that type with the status you are searching - might also be NULL
 * if you are not interested in this value
 * @note If you are searching for a quarter (e.g.) you should perform a
 * <code>if (hasBuilding[B_QUARTERS])</code> check - this should speed things up a lot
 * @return true if building with status was found
 */
qboolean B_CheckBuildingTypeStatus (const base_t* const base, buildingType_t type, buildingStatus_t status, int *cnt)
{
	int cntlocal = 0;
	building_t *building = NULL;

	while ((building = B_GetNextBuildingByType(base, building, type))) {
		if (building->buildingStatus == status) {
			cntlocal++;
			/* don't count any further - the caller doesn't want to know the value */
			if (!cnt)
				return qtrue;
		}
	}

	/* set the cnt pointer if the caller wants to know this value */
	if (cnt)
		*cnt = cntlocal;

	return cntlocal ? qtrue : qfalse;
}

/**
 * @brief Get the capacity associated to a building type
 * @param[in] type The type of the building
 * @return capacity (baseCapacities_t), or MAX_CAP if building has no capacity
 */
baseCapacities_t B_GetCapacityFromBuildingType (buildingType_t type)
{
	switch (type) {
	case B_LAB:
		return CAP_LABSPACE;
	case B_QUARTERS:
		return CAP_EMPLOYEES;
	case B_STORAGE:
		return CAP_ITEMS;
	case B_WORKSHOP:
		return CAP_WORKSPACE;
	case B_HANGAR:
		return CAP_AIRCRAFT_BIG;
	case B_ALIEN_CONTAINMENT:
		return CAP_ALIENS;
	case B_SMALL_HANGAR:
		return CAP_AIRCRAFT_SMALL;
	case B_ANTIMATTER:
		return CAP_ANTIMATTER;
	default:
		return MAX_CAP;
	}
}

/**
 * @brief Get building type by base capacity.
 * @param[in] cap Enum type of baseCapacities_t.
 * @return Enum type of buildingType_t.
 * @sa B_UpdateBaseCapacities
 * @sa B_PrintCapacities_f
 */
buildingType_t B_GetBuildingTypeByCapacity (baseCapacities_t cap)
{
	switch (cap) {
	case CAP_ALIENS:
		return B_ALIEN_CONTAINMENT;
	case CAP_AIRCRAFT_SMALL:
		return B_SMALL_HANGAR;
	case CAP_AIRCRAFT_BIG:
		return B_HANGAR;
	case CAP_EMPLOYEES:
		return B_QUARTERS;
	case CAP_ITEMS:
		return B_STORAGE;
	case CAP_LABSPACE:
		return B_LAB;
	case CAP_WORKSPACE:
		return B_WORKSHOP;
	case CAP_ANTIMATTER:
		return B_ANTIMATTER;
	default:
		return MAX_BUILDING_TYPE;
	}
}

/**
 * @brief Get the status associated to a building
 * @param[in] base The base to search for the given building type
 * @param[in] buildingType value of building->buildingType
 * @return true if the building is functional
 * @sa B_SetBuildingStatus
 */
qboolean B_GetBuildingStatus (const base_t* const base, const buildingType_t buildingType)
{
	assert(base);

	if (buildingType == B_MISC)
		return qtrue;
	else if (buildingType < MAX_BUILDING_TYPE)
		return base->hasBuilding[buildingType];
	else {
		Com_Printf("B_GetBuildingStatus: Building-type %i does not exist.\n", buildingType);
		return qfalse;
	}
}


/**
 * @brief Set status associated to a building
 * @param[in] base Base to check
 * @param[in] buildingType value of building->buildingType
 * @param[in] newStatus New value of the status
 * @sa B_GetBuildingStatus
 */
void B_SetBuildingStatus (base_t* const base, const buildingType_t buildingType, qboolean newStatus)
{
	assert(base);

	if (buildingType == B_MISC)
		Com_Printf("B_SetBuildingStatus: No status is associated to B_MISC type of building.\n");
	else if (buildingType < MAX_BUILDING_TYPE) {
		base->hasBuilding[buildingType] = newStatus;
		Com_DPrintf(DEBUG_CLIENT, "B_SetBuildingStatus: set status for %i to %i\n", buildingType, newStatus);
	} else
		Com_Printf("B_SetBuildingStatus: Type of building %i does not exist\n", buildingType);
}

/**
 * @brief Check that the dependences of a building is operationnal
 * @param[in] building Pointer to the building to check
 * @return true if base contains needed dependence for entering building
 */
qboolean B_CheckBuildingDependencesStatus (const building_t* building)
{
	assert(building);

	if (!building->dependsBuilding)
		return qtrue;

	/* Make sure the dependsBuilding pointer is really a template .. just in case. */
	assert(building->dependsBuilding == building->dependsBuilding->tpl);

	return B_GetBuildingStatus(building->base, building->dependsBuilding->buildingType);
}

/**
 * @brief Resets the buildingCurrent variable and baseAction
 * @param[in,out] base Pointer to the base needs buildingCurrent to be reseted
 * @note Make sure you are not doing anything with the buildingCurrent pointer
 * in this function, the pointer might already be invalid
 */
void B_ResetBuildingCurrent (base_t* base)
{
	if (base)
		base->buildingCurrent = NULL;
	ccs.baseAction = BA_NONE;
}

/**
 * @brief Holds the names of valid entries in the basemanagement.ufo file.
 *
 * The valid definition names for BUILDINGS (building_t) in the basemanagement.ufo file.
 * to the appropriate values in the corresponding struct
 */
static const value_t valid_building_vars[] = {
	{"map_name", V_CLIENT_HUNK_STRING, offsetof(building_t, mapPart), 0},	/**< Name of the map file for generating basemap. */
	{"max_count", V_INT, offsetof(building_t, maxCount), MEMBER_SIZEOF(building_t, maxCount)},	/**< How many building of the same type allowed? */
	{"level", V_FLOAT, offsetof(building_t, level), MEMBER_SIZEOF(building_t, level)},	/**< building level */
	{"name", V_TRANSLATION_STRING, offsetof(building_t, name), 0},	/**< The displayed building name. */
	{"pedia", V_CLIENT_HUNK_STRING, offsetof(building_t, pedia), 0},	/**< The pedia-id string for the associated pedia entry. */
	{"status", V_INT, offsetof(building_t, buildingStatus), MEMBER_SIZEOF(building_t, buildingStatus)},	/**< The current status of the building. */
	{"image", V_CLIENT_HUNK_STRING, offsetof(building_t, image), 0},	/**< Identifies the image for the building. */
	{"visible", V_BOOL, offsetof(building_t, visible), MEMBER_SIZEOF(building_t, visible)}, /**< Determines whether a building should be listed in the construction list. Set the first part of a building to 1 all others to 0 otherwise all building-parts will be on the list */
	{"needs", V_CLIENT_HUNK_STRING, offsetof(building_t, needs), 0},	/**< For buildings with more than one part; the other parts of the building needed.*/
	{"fixcosts", V_INT, offsetof(building_t, fixCosts), MEMBER_SIZEOF(building_t, fixCosts)},	/**< Cost to build. */
	{"varcosts", V_INT, offsetof(building_t, varCosts), MEMBER_SIZEOF(building_t, varCosts)},	/**< Costs that will come up by using the building. */
	{"build_time", V_INT, offsetof(building_t, buildTime), MEMBER_SIZEOF(building_t, buildTime)},	/**< How many days it takes to construct the building. */
	{"starting_employees", V_INT, offsetof(building_t, maxEmployees), MEMBER_SIZEOF(building_t, maxEmployees)},	/**< How many employees to hire on construction in the first base. */
	{"capacity", V_INT, offsetof(building_t, capacity), MEMBER_SIZEOF(building_t, capacity)},	/**< A size value that is used by many buildings in a different way. */

	/*event handler functions */
	{"onconstruct", V_STRING, offsetof(building_t, onConstruct), 0}, /**< Event handler. */
	{"onattack", V_STRING, offsetof(building_t, onAttack), 0}, /**< Event handler. */
	{"ondestroy", V_STRING, offsetof(building_t, onDestroy), 0}, /**< Event handler. */
	{"mandatory", V_BOOL, offsetof(building_t, mandatory), MEMBER_SIZEOF(building_t, mandatory)}, /**< Automatically construct this building when a base is set up. Must also set the pos-flag. */
	{NULL, 0, 0, 0}
};

/**
 * @brief Get the maximum level of a building type in a base.
 * @param[in] base Pointer to base.
 * @param[in] type Building type to get the maximum level for.
 * @note This function checks base status for particular buildings.
 * @return 0.0f if there is no (operational) building of the requested type in the base, otherwise the maximum level.
 */
float B_GetMaxBuildingLevel (const base_t* base, const buildingType_t type)
{
	float max = 0.0f;

	if (B_GetBuildingStatus(base, type)) {
		building_t *building = NULL;
		while ((building = B_GetNextBuildingByType(base, building, type)))
			if (building->buildingStatus == B_STATUS_WORKING)
				max = max(building->level, max);
	}

	return max;
}

/**
 * @brief Perform the base assembling in case of an alien attack
 * @param[in,out] base The base to assemble
 * @return @c true if the assembly was successful, @c false if it failed
 * @todo Search a empty field and add a alien craft there
 * @todo If a building is still under construction, it will be assembled as a finished part.
 * Otherwise we need mapparts for all the maps under construction.
 */
qboolean B_AssembleMap (const base_t *base)
{
	int row, col;
	char maps[2048];
	char coords[2048];
	qboolean used[MAX_BUILDINGS];

	if (!base) {
		Com_Printf("B_AssembleMap: No base to assemble\n");
		return qfalse;
	}

	maps[0] = '\0';
	coords[0] = '\0';

	/* reset the used flag */
	memset(used, 0, sizeof(used));

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col].building) {
				const building_t *entry = base->map[row][col].building;

				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one
				 * this is why we check the used flag and continue if it was set already */
				if (entry->needs) {
					if (B_BuildingGetUsed(used, entry->idx))
						continue;
					B_BuildingSetUsed(used, entry->idx);
				}

				if (!entry->mapPart)
					Com_Error(ERR_DROP, "MapPart for building '%s' is missing'", entry->id);

				Q_strcat(maps, va("b/%s ", entry->mapPart), sizeof(maps));
			} else {
				Q_strcat(maps, "b/empty ", sizeof(maps));
			}

			Q_strcat(coords, va("%i %i %i ", col * BASE_TILE_UNITS, (BASE_SIZE - row - 1) * BASE_TILE_UNITS, 0), sizeof(coords));
		}
	}

	/* set maxlevel for base attacks */
	cl.mapMaxLevel = BASE_MAX_WORLDLEVEL;

	SAV_QuickSave();

	Cbuf_AddText(va("map %s \"%s\" \"%s\"\n", (MAP_IsNight(base->pos) ? "night" : "day"), maps, coords));

	return qtrue;
}

/**
 * @brief Check base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @note This function checks  base status for particular buildings and base capacities.
 * @return qtrue if a base status has been modified (but do not check capacities)
 */
static qboolean B_CheckUpdateBuilding (building_t* building)
{
	qboolean oldValue;

	assert(building);

	/* Status of Miscellenious buildings cannot change. */
	if (building->buildingType == B_MISC)
		return qfalse;

	oldValue = B_GetBuildingStatus(building->base, building->buildingType);
	if (building->buildingStatus == B_STATUS_WORKING
	 && B_CheckBuildingDependencesStatus(building))
		B_SetBuildingStatus(building->base, building->buildingType, qtrue);
	else
		B_SetBuildingStatus(building->base, building->buildingType, qfalse);

	if (B_GetBuildingStatus(building->base, building->buildingType) != oldValue) {
		Com_DPrintf(DEBUG_CLIENT, "Status of building %s is changed to %i.\n",
			building->name, B_GetBuildingStatus(building->base, building->buildingType));
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Actions to perform when a type of buildings goes from qfalse to qtrue.
 * @note This function is not only called when a building is enabled for the first time in base
 * @note but also when one of its dependencies is destroyed and then rebuilt
 * @param[in] type Type of building that has been modified from qfalse to qtrue
 * @param[in] base Pointer to base with given building.
 * @sa B_UpdateOneBaseBuildingStatusOnDisable
 * @sa onConstruct trigger
 */
static void B_UpdateOneBaseBuildingStatusOnEnable (buildingType_t type, base_t* base)
{
	switch (type) {
	case B_RADAR:
		Cmd_ExecuteString(va("update_base_radar_coverage %i", base->idx));
		break;
	case B_COMMAND:
		Cmd_ExecuteString("mn_installation_update_max_count");
		break;
	default:
		break;
	}
}

/**
 * @brief Actions to perform when a type of buildings goes from functional to non-functional.
 * @param[in] type Type of building which hasBuilding value has been modified from qtrue to qfalse
 * @param[in] base Pointer to base with given building.
 * @note That does not mean that a building of this type has been destroyed: maybe one of its dependencies
 * has been destroyed: don't use onDestroy trigger.
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 * @sa onDestroy trigger
 */
static void B_UpdateOneBaseBuildingStatusOnDisable (buildingType_t type, base_t* base)
{
	switch (type) {
	case B_ALIEN_CONTAINMENT:
		/* if an alien containment is not functional, aliens die... */
		AC_KillAll(base);
		break;
	case B_RADAR:
		Cmd_ExecuteString(va("update_base_radar_coverage %i", base->idx));
		break;
	case B_COMMAND:
		Cmd_ExecuteString("mn_installation_update_max_count");
		break;
	default:
		break;
	}
}

/**
 * @brief Update status of every building when a building has been built/destroyed
 * @param[in] base
 * @param[in] buildingType The building-type that has been built / removed.
 * @param[in] onBuilt qtrue if building has been built, qfalse else
 * @sa B_BuildingDestroy
 * @sa B_UpdateAllBaseBuildingStatus
 * @return qtrue if at least one building status has been modified
 */
static qboolean B_UpdateStatusBuilding (base_t* base, buildingType_t buildingType, qboolean onBuilt)
{
	qboolean test = qfalse;
	qboolean returnValue = qfalse;
	building_t *building = NULL;

	/* Construction / destruction may have changed the status of other building
	 * We check that, but only for buildings which needed building */
	while ((building = B_GetNextBuilding(base, building))) {
		const building_t *dependsBuilding = building->dependsBuilding;
		if (dependsBuilding && buildingType == dependsBuilding->buildingType) {
			/* ccs.buildings[base->idx][i] needs built/removed building */
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnDisable(building->buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				}
			}
		}
	}
	/* and maybe some updated status have changed status of other building.
	 * So we check again, until nothing changes. (no condition here for check, it's too complex) */
	while (test) {
		test = qfalse;
		building = NULL;
		while ((building = B_GetNextBuilding(base, building))) {
			if (onBuilt && !B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
					test = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, building->buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(building)) {
					B_UpdateOneBaseBuildingStatusOnDisable(building->buildingType, base);
					test = qtrue;
				}
			}
		}
	}

	return returnValue;
}

/**
 * @brief Update Antimatter Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
static void B_UpdateAntimatterCap (base_t *base)
{
	const objDef_t *od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od != NULL)
		base->capacities[CAP_ANTIMATTER].cur = B_ItemInBase(od, base);
}

/**
 * @brief Returns the free capacity of a type
 * @param[in] base Pointer to the base to check
 * @param[in] cap Capacity type
 * @sa baseCapacities_t
 */
int B_FreeCapacity (const base_t *base, baseCapacities_t cap)
{
	assert(base);
	return base->capacities[cap].max - base->capacities[cap].cur;
}

/**
 * @brief Recalculate status and capacities of one base
 * @param[in] base Pointer to the base where status and capacities must be recalculated
 * @param[in] firstEnable qtrue if this is the first time the function is called for this base
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 */
void B_ResetAllStatusAndCapacities (base_t *base, qboolean firstEnable)
{
	int i;
	qboolean test = qtrue;

	assert(base);

	Com_DPrintf(DEBUG_CLIENT, "Reseting base %s:\n", base->name);

	/* reset all values of hasBuilding[] */
	for (i = 0; i < MAX_BUILDING_TYPE; i++) {
		if (i != B_MISC)
			B_SetBuildingStatus(base, i, qfalse);
	}
	/* activate all buildings that needs to be activated */
	while (test) {
		building_t *building = NULL;
		test = qfalse;
		while ((building = B_GetNextBuilding(base, building))) {
			if (!B_GetBuildingStatus(base, building->buildingType)
			 && B_CheckUpdateBuilding(building)) {
				if (firstEnable)
					B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
				test = qtrue;
			}
		}
	}

	/* Update all capacities of base */
	B_UpdateBaseCapacities(MAX_CAP, base);

	/* calculate capacities.cur for every capacity */
	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ALIENS)))
		base->capacities[CAP_ALIENS].cur = AL_CountInBase(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_AIRCRAFT_SMALL)) ||
		B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_AIRCRAFT_BIG)))
		AIR_UpdateHangarCapForAll(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_EMPLOYEES)))
		base->capacities[CAP_EMPLOYEES].cur = E_CountAllHired(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ITEMS)))
		B_UpdateStorageCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_LABSPACE)))
		base->capacities[CAP_LABSPACE].cur = RS_CountScientistsInBase(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_WORKSPACE)))
		PR_UpdateProductionCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_ANTIMATTER)))
		B_UpdateAntimatterCap(base);

	/* Check that current capacity is possible -- if we changed values in *.ufo */
	for (i = 0; i < MAX_CAP; i++)
		if (base->capacities[i].cur > base->capacities[i].max)
			Com_Printf("B_ResetAllStatusAndCapacities: Warning, capacity of %i is bigger than maximum capacity\n", i);
}

/**
 * @brief Actions to perform when destroying one hangar.
 * @param[in] base Pointer to the base where hangar is destroyed.
 * @param[in] buildingType Type of hangar: B_SMALL_HANGAR for small hangar, B_HANGAR for large hangar
 * @note called when player destroy its building or hangar is destroyed during base attack.
 * @note These actions will be performed after we actually remove the building.
 * @pre we checked before calling this function that all parameters are valid.
 * @pre building is not under construction.
 * @sa B_BuildingDestroy_f
 * @todo If player choose to destroy the building, a popup should ask him if he wants to sell aircraft in it.
 */
void B_RemoveAircraftExceedingCapacity (base_t* base, buildingType_t buildingType)
{
	baseCapacities_t capacity = B_GetCapacityFromBuildingType(buildingType);
	linkedList_t *awayAircraft = NULL;
	int numAwayAircraft;
	int randomNum;
	aircraft_t *aircraft;

	/* destroy aircraft only if there's not enough hangar (hangar is already destroyed) */
	if (B_FreeCapacity(base, capacity) >= 0)
		return;

	/* destroy one aircraft (must not be sold: may be destroyed by aliens) */
	AIR_Foreach(aircraft) {
		const int aircraftSize = aircraft->size;

		if (!AIR_IsAircraftOfBase(aircraft, base))
			continue;

		switch (aircraftSize) {
		case AIRCRAFT_SMALL:
			if (buildingType != B_SMALL_HANGAR)
				continue;
			break;
		case AIRCRAFT_LARGE:
			if (buildingType != B_HANGAR)
				continue;
			break;
		default:
			Com_Error(ERR_DROP, "B_RemoveAircraftExceedingCapacity: Unknown type of aircraft '%i'", aircraftSize);
		}

		/* Only aircraft in hangar will be destroyed by hangar destruction */
		if (!AIR_IsAircraftInBase(aircraft)) {
			if (AIR_IsAircraftOnGeoscape(aircraft))
				LIST_AddPointer(&awayAircraft, (void*)aircraft);
			continue;
		}

		/* Remove aircraft and aircraft items, but do not fire employees */
		AIR_DeleteAircraft(aircraft);
		LIST_Delete(&awayAircraft);
		return;
	}
	numAwayAircraft = LIST_Count(awayAircraft);

	if (!numAwayAircraft)
		return;
	/* All aircraft are away from base, pick up one and change it's homebase */
	randomNum = rand() % numAwayAircraft;
	if (!CL_DisplayHomebasePopup((aircraft_t*)LIST_GetByIdx(awayAircraft, randomNum), qfalse)) {
		aircraft_t *aircraft = (aircraft_t*)LIST_GetByIdx(awayAircraft, randomNum);
		/* No base can hold this aircraft */
		UFO_NotifyPhalanxAircraftRemoved(aircraft);
		if (!MapIsWater(MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN)))
			CP_SpawnRescueMission(aircraft, NULL);
		else {
			/* Destroy the aircraft and everything onboard - the aircraft pointer
			 * is no longer valid after this point */
			AIR_DestroyAircraft(aircraft);
		}
	}
	LIST_Delete(&awayAircraft);
}

/**
 * @brief Removes a building from the given base
 * @param[in] building The building to remove
 * @note Also updates capacities and sets the hasBuilding[] values in base_t
 * @sa B_BuildingDestroy_f
 */
qboolean B_BuildingDestroy (building_t* building)
{
	const buildingType_t buildingType = building->buildingType;
	const building_t const *buildingTemplate = building->tpl;	/**< Template of the removed building */
	const qboolean onDestroyCommand = (building->onDestroy[0] != '\0') && (building->buildingStatus == B_STATUS_WORKING);
	base_t *base = building->base;

	/* Don't allow to destroy an entrance. */
	if (buildingType == B_ENTRANCE)
		return qfalse;

	if (!base->map[(int)building->pos[0]][(int)building->pos[1]].building
	 || base->map[(int)building->pos[0]][(int)building->pos[1]].building != building) {
		Com_Error(ERR_DROP, "B_BuildingDestroy: building mismatch at base %i pos %i,%i.",
				base->idx, (int)building->pos[0], (int)building->pos[1]);
	}

	/* Remove the building from the base map */
	if (building->needs) {
		/* "Child" building is always right to the "parent" building". */
		base->map[(int)building->pos[0]][((int)building->pos[1]) + 1].building = NULL;
	}
	base->map[(int)building->pos[0]][(int)building->pos[1]].building = NULL;

	building->buildingStatus = B_STATUS_NOT_SET;

	/* Update buildingCurrent */
	if (base->buildingCurrent > building)
		base->buildingCurrent--;
	else if (base->buildingCurrent == building)
		base->buildingCurrent = NULL;

	{
		int const baseIDX = base->idx;
		building_t* const buildings = ccs.buildings[baseIDX];
		int const idx = building->idx;
		int cntBldgs;
		int i;

		REMOVE_ELEM(buildings, idx, ccs.numBuildings[baseIDX]);

		/* Update the link of other buildings */
		cntBldgs = ccs.numBuildings[baseIDX];
		for (i = 0; i < cntBldgs; i++)
			if (buildings[i].idx >= idx) {
				buildings[i].idx--;
				base->map[(int)buildings[i].pos[0]][(int)buildings[i].pos[1]].building = &buildings[i];
				if (buildings[i].needs)
					base->map[(int)buildings[i].pos[0]][(int)buildings[i].pos[1] + 1].building = &buildings[i];
			}

		building = NULL;
	}
	/** @note Don't use the building pointer after this point - it's zeroed. */

	if (buildingType != B_MISC && buildingType != MAX_BUILDING_TYPE) {
		if (B_GetNumberOfBuildingsInBaseByBuildingType(base, buildingType) <= 0) {
			/* there is no more building of this type */
			B_SetBuildingStatus(base, buildingType, qfalse);
			/* check if this has an impact on other buildings */
			B_UpdateStatusBuilding(base, buildingType, qfalse);
			/* we may have changed status of several building: update all capacities */
			B_UpdateBaseCapacities(MAX_CAP, base);
		} else {
			/* there is still at least one other building of the same type in base: just update capacity */
			const baseCapacities_t cap = B_GetCapacityFromBuildingType(buildingType);
			if (cap != MAX_CAP)
				B_UpdateBaseCapacities(cap, base);
		}
	}

	Cmd_ExecuteString("base_init");

	/* call ondestroy trigger only if building is not under construction
	 * (we do that after base capacity has been updated) */
	if (onDestroyCommand) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i %i;\n",
			buildingTemplate->onDestroy, base->idx, buildingType);
		Cmd_ExecuteString(va("%s %i %i", buildingTemplate->onDestroy, base->idx, buildingType));
	}

	return qtrue;
}

/**
 * @brief Will ensure that aircraft on geoscape are not stored
 * in a base that no longer has any hangar left
 * @param[in] base The base that is going to be destroyed
 * @todo this should be merged into B_RemoveAircraftExceedingCapacity
 */
static void B_MoveAircraftOnGeoscapeToOtherBases (const base_t *base)
{
	aircraft_t *aircraft;
	aircraft_t *prevAircraft = NULL;

	AIR_ForeachFromBase(aircraft, base) {
		if (AIR_IsAircraftOnGeoscape(aircraft)) {
			base_t *newbase = NULL;
			qboolean moved = qfalse;
			while ((newbase = B_GetNextFounded(newbase)) != NULL) {
				/* found a new homebase? */
				if (base != newbase && AIR_MoveAircraftIntoNewHomebase(aircraft, newbase)) {
					moved = qtrue;
					break;
				}
			}
			if (!moved) {
				/* No base can hold this aircraft */
				UFO_NotifyPhalanxAircraftRemoved(aircraft);
				if (!MapIsWater(MAP_GetColor(aircraft->pos, MAPTYPE_TERRAIN))) {
					CP_SpawnRescueMission(aircraft, NULL);
				} else {
					/* Destroy the aircraft and everything onboard - the aircraft pointer
					 * is no longer valid after this point */
					AIR_DestroyAircraft(aircraft);
				}
				aircraft = prevAircraft;
			}
		}
		prevAircraft = aircraft;
	}
}

/**
 * @brief Destroy a base.
 * @param[in] base Pointer to base to be destroyed.
 * @note If you want to sell items or unhire employees, you should do it before
 * calling this function - they are going to be killed / destroyed.
 */
void B_Destroy (base_t *base)
{
	int buildingIdx;

	assert(base);
	base->baseStatus = BASE_DESTROYED;

	CP_MissionNotifyBaseDestroyed(base);
	B_MoveAircraftOnGeoscapeToOtherBases(base);

	/* do a reverse loop as buildings are going to be destroyed */
	for (buildingIdx = ccs.numBuildings[base->idx] - 1; buildingIdx >= 0; buildingIdx--) {
		building_t *building = B_GetBuildingByIDX(base->idx, buildingIdx);
		B_BuildingDestroy(building);
	}

	E_DeleteAllEmployees(base);

	/** @todo Destroy the base if we solved aircraft transfer issue
	 *	it should be done via moving other bases (and adjusting pointers
	 *  where needed. We don't allow holes in baselist
	 */
}

#ifdef DEBUG
/**
 * @brief Debug command for destroying a base.
 * @param[in] base index to be destroyed.
 */
static void CL_BaseDestroy_f (void)
{
	int baseIdx;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx>\n", Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("CL_BaseDestroy_f: baseIdx %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (!base) {
		Com_Printf("CL_BaseDestroy_f: Base %i not founded\n", baseIdx);
		return;
	}

	B_Destroy(base);
}
#endif

/**
 * @brief Mark a building for destruction - you only have to confirm it now
 * @note Also calls the ondestroy trigger
 */
void B_MarkBuildingDestroy (building_t* building)
{
	int cap;
	base_t *base = building->base;

	/* you can't destroy buildings if base is under attack */
	if (B_IsUnderAttack(base)) {
		UI_Popup(_("Notice"), _("Base is under attack, you can't destroy buildings!"));
		return;
	}

	cap = B_GetCapacityFromBuildingType(building->buildingType);
	/* store the pointer to the building you wanna destroy */
	base->buildingCurrent = building;

	/** @todo: make base destroyable by destroying entrance */
	if (building->buildingType == B_ENTRANCE) {
		UI_Popup(_("Destroy Entrance"), _("You can't destroy the entrance of the base!"));
		return;
	}

	if (building->buildingStatus == B_STATUS_WORKING) {
		const qboolean hasBases = B_AtLeastOneExists();
		switch (building->buildingType) {
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (base->capacities[cap].cur >= base->capacities[cap].max) {
				UI_PopupButton(_("Destroy Hangar"), _("If you destroy this hangar, you will also destroy the aircraft inside.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push aircraft;aircraft_select;", _("Go to hangar"), _("Go to hangar without destroying building"),
					"building_destroy;ui_pop;", _("Destroy"), _("Destroy the building"),
					hasBases ? "ui_pop;ui_push transfer;" : NULL, hasBases ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_QUARTERS:
			if (base->capacities[cap].cur + building->capacity > base->capacities[cap].max) {
				UI_PopupButton(_("Destroy Quarter"), _("If you destroy this Quarters, every employee inside will be killed.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push employees;employee_list 0;", _("Dismiss"), _("Go to hiring menu without destroying building"),
					"building_destroy;ui_pop;", _("Destroy"), _("Destroy the building"),
					hasBases ? "ui_pop;ui_push transfer;" : NULL, hasBases ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_STORAGE:
			if (base->capacities[cap].cur + building->capacity > base->capacities[cap].max) {
				UI_PopupButton(_("Destroy Storage"), _("If you destroy this Storage, every items inside will be destroyed.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push market;buy_type *mn_itemtype", _("Go to storage"), _("Go to buy/sell menu without destroying building"),
					"building_destroy;ui_pop;", _("Destroy"), _("Destroy the building"),
					hasBases ? "ui_pop;ui_push transfer;" : NULL, hasBases ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		default:
			break;
		}
	}

	UI_PopupButton(_("Destroy building"), _("Are you sure you want to destroy this building?"),
		NULL, NULL, NULL,
		"building_destroy;ui_pop;", _("Destroy"), _("Destroy the building"),
		NULL, NULL, NULL);
}

/**
 * @brief Displays the status of a building for baseview.
 * @note updates the cvar mn_building_status which is used in the building
 * construction menu to display the status of the given building
 * @note also script command function binding for 'building_status'
 */
void B_BuildingStatus (const building_t* building)
{
	int numberOfBuildings = 0;

	assert(building);

	Cvar_Set("mn_building_status", _("Not set"));

	switch (building->buildingStatus) {
	case B_STATUS_NOT_SET:
		numberOfBuildings = B_GetNumberOfBuildingsInBaseByTemplate(B_GetCurrentSelectedBase(), building->tpl);
		if (numberOfBuildings >= 0)
			Cvar_Set("mn_building_status", va(_("Already %i in base"), numberOfBuildings));
		break;
	case B_STATUS_UNDER_CONSTRUCTION:
		Cvar_Set("mn_building_status", "");
		break;
	case B_STATUS_CONSTRUCTION_FINISHED:
		Cvar_Set("mn_building_status", _("Construction finished"));
		break;
	case B_STATUS_WORKING:
		if (B_CheckBuildingDependencesStatus(building)) {
			Cvar_Set("mn_building_status", _("Working 100%"));
		} else {
			assert (building->dependsBuilding);
			/** @todo shorten text or provide more space in overview popup */
			Cvar_Set("mn_building_status", va("%s %s", _("Not operational, depends on"), _(building->dependsBuilding->name)));
		}
		break;
	case B_STATUS_DOWN:
		Cvar_Set("mn_building_status", _("Down"));
		break;
	default:
		break;
	}
}

/**
 * @brief Updates base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @param[in] status Enum of buildingStatus_t which is status of given building.
 * @note This function checks whether a building has B_STATUS_WORKING status, and
 * then updates base status for particular buildings and base capacities.
 */
static void B_UpdateAllBaseBuildingStatus (building_t* building, buildingStatus_t status)
{
	qboolean test;
	buildingStatus_t oldStatus;
	base_t* base = building->base;

	assert(base);

	oldStatus = building->buildingStatus;
	building->buildingStatus = status;

	/* we update the status of the building (we'll call this building building 1) */
	test = B_CheckUpdateBuilding(building);
	if (test)
		B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);

	/* now, the status of this building may have changed the status of other building.
	 * We check that, but only for buildings which needed building 1 */
	if (test) {
		B_UpdateStatusBuilding(base, building->buildingType, qtrue);
		/* we may have changed status of several building: update all capacities */
		B_UpdateBaseCapacities(MAX_CAP, base);
	} else {
		/* no other status than status of building 1 has been modified
		 * update only status of building 1 */
		const int cap = B_GetCapacityFromBuildingType(building->buildingType);
		if (cap != MAX_CAP)
			B_UpdateBaseCapacities(cap, base);
	}

	/** @todo this should be an user option defined in Game Options. */
	if (oldStatus == B_STATUS_UNDER_CONSTRUCTION && (status == B_STATUS_CONSTRUCTION_FINISHED || status == B_STATUS_WORKING)) {
		if (B_CheckBuildingDependencesStatus(building))
			CL_GameTimeStop();
	} else {
		CL_GameTimeStop();
	}
}

/**
 * @brief Build starting building in the first base, and hire employees.
 * @param[in,out] base The base to put the new building into
 * @param[in] buildingTemplate The building template to create a new building with
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] pos The position on the base grid
 */
static void B_AddBuildingToBasePos (base_t *base, const building_t const *buildingTemplate, qboolean hire, const vec2_t pos)
{
	/* new building in base (not a template) */
	building_t *buildingNew;

	/* fake a click to basemap */
	buildingNew = B_SetBuildingByClick(base, buildingTemplate, (int)pos[0], (int)pos[1]);
	if (!buildingNew)
		return;
	B_UpdateAllBaseBuildingStatus(buildingNew, B_STATUS_WORKING);
	Com_DPrintf(DEBUG_CLIENT, "Base %i new building: %s at (%.0f:%.0f)\n",
			base->idx, buildingNew->id, buildingNew->pos[0], buildingNew->pos[1]);

	if (hire)
		E_HireForBuilding(base, buildingNew, -1);

	/* now call the onconstruct trigger */
	if (buildingNew->onConstruct[0] != '\0') {
		Com_DPrintf(DEBUG_CLIENT, "B_AddBuildingToBasePos: %s %i;\n",
				buildingNew->onConstruct, base->idx);
		Cmd_ExecuteString(va("%s %i", buildingNew->onConstruct, base->idx));
	}
}

/**
 * @brief Prepares initial equipment for first base at the beginning of the campaign.
 * @param[in] base Pointer to first base.
 * @param[in] assignInitialAircraft aircraft on which the soldiers (to equip) are
 * @param[in] ed Initial equipment definition
 * @param[in] edTarget storage to put items to
 * @sa B_BuildBase_f
 * @todo Make sure all equipment including soldiers equipment is added to capacity.cur.
 */
static void B_InitialEquipment (base_t *base, aircraft_t *assignInitialAircraft, const equipDef_t *ed, equipDef_t *edTarget)
{
	int i, price = 0;

	assert(base);
	assert(edTarget);

	/* Initial soldiers and their equipment. */
	if (assignInitialAircraft) {
		B_PackInitialEquipment(assignInitialAircraft, ed);
	} else {
		for (i = 0; i < csi.numODs; i++)
			edTarget->numItems[i] += ed->numItems[i] / 5;
	}

	/* Pay for the initial equipment as well as update storage capacity. */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		price += edTarget->numItems[i] * od->price;
		base->capacities[CAP_ITEMS].cur += edTarget->numItems[i] * od->size;
	}

	/* Finally update credits. */
	CL_UpdateCredits(ccs.credits - price);
}

/**
 * @brief builds a base from template
 * @param[out] base The base to build
 * @param[in] templateName Templated used for building. If @c NULL no template
 * will be used.
 * @param[in] hire If hiring employee needed
 * @note It builds an empty base on NULL (or empty) templatename
 */
static void B_BuildFromTemplate (base_t *base, const char *templateName, qboolean hire)
{
	const baseTemplate_t *baseTemplate = B_GetBaseTemplate(templateName);
	int freeSpace = BASE_SIZE * BASE_SIZE;
	int i;

	assert(base);

	for (i = 0; i < MAX_CAP; i++)
		base->capacities[i].cur = 0;

	if (baseTemplate) {
		/* find each building in the template */
		for (i = 0; i < baseTemplate->numBuildings; i++) {
			vec2_t pos;
			const baseBuildingTile_t *buildingTile = &baseTemplate->buildings[i];

			Vector2Set(pos, buildingTile->posX, buildingTile->posY);

			if (!base->map[(int)pos[0]][(int)pos[1]].building) {
				B_AddBuildingToBasePos(base, buildingTile->building, hire, pos);
				freeSpace--;
			}
		}
	}

	/* we need to set up the mandatory buildings */
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t* building = &ccs.buildingTemplates[i];
		vec2_t pos;

		if ((!building->mandatory) || B_GetBuildingStatus(base, building->buildingType))
			continue;

		while ((freeSpace > 0) && !B_GetBuildingStatus(base, building->buildingType)) {
			Vector2Set(pos, rand() % BASE_SIZE, rand() % BASE_SIZE);
			if (!base->map[(int)pos[0]][(int)pos[1]].building) {
				B_AddBuildingToBasePos(base, building, hire, pos);
				freeSpace--;
			}
		}
		/** @todo if there is no more space for mandatory building, remove a non mandatory one
		 * or build mandatory ones first */
		if (!B_GetBuildingStatus(base, building->buildingType))
			Com_Error(ERR_DROP, "B_BuildFromTemplate: Cannot build base. No space for it's buildings!");
	}

	/* Create random blocked fields in the base.
	 * The first base never has blocked fields so we skip it. */
	if (ccs.campaignStats.basesBuilt) {
		const int j = round((frand() * (MAX_BLOCKEDFIELDS - MIN_BLOCKEDFIELDS)) + MIN_BLOCKEDFIELDS);

		for (i = 0; i < j; i++) {
			baseBuildingTile_t *mapPtr = &base->map[rand() % BASE_SIZE][rand() % (BASE_SIZE)];

			if (mapPtr->building || mapPtr->blocked)
				continue;

			mapPtr->blocked = qtrue;
			freeSpace--;
		}
	}
}

/**
 * @brief Setup buildings and equipment for first base. Uses the campaign
 * scriptable first base template to place the buildings in the base.
 * @param[in] campaign The campaign data structure
 * @param[in,out] base The base to set up
 * @sa B_SetUpBase
 */
static void B_SetUpFirstBase (const campaign_t *campaign, base_t* base)
{
	const equipDef_t *ed;

	if (campaign->firstBaseTemplate[0] == '\0')
		Com_Error(ERR_DROP, "No base template for setting up the first base given");

	RS_MarkResearchable(qtrue, base);
	BS_InitMarket(campaign);
	E_InitialEmployees(campaign);

	/* Find the initial equipment definition for current campaign. */
	ed = INV_GetEquipmentDefinitionByID(campaign->equipment);
	/* Copy it to base storage. */
	base->storage = *ed;

	B_BuildFromTemplate(base, campaign->firstBaseTemplate, qtrue);
	/* Add aircraft to the first base */
	/** @todo move aircraft to .ufo */
	/* buy two first aircraft and hire pilots for them. */
	if (B_GetBuildingStatus(base, B_HANGAR)) {
		const equipDef_t *equipDef = INV_GetEquipmentDefinitionByID(campaign->soldierEquipment);
		const char *firebird = Com_DropShipTypeToShortName(DROPSHIP_FIREBIRD);
		const aircraft_t *firebirdAircraft = AIR_GetAircraft(firebird);
		aircraft_t *aircraft = AIR_NewAircraft(base, firebirdAircraft);
		CL_UpdateCredits(ccs.credits - firebirdAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
		/* Assign and equip soldiers on Dropships */
		AIR_AssignInitial(aircraft);
		B_InitialEquipment(base, aircraft, equipDef, &base->storage);
	}
	if (B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
		const char *stiletto = Com_DropShipTypeToShortName(INTERCEPTOR_STILETTO);
		const aircraft_t *stilettoAircraft = AIR_GetAircraft(stiletto);
		aircraft_t *aircraft = AIR_NewAircraft(base, stilettoAircraft);
		CL_UpdateCredits(ccs.credits - stilettoAircraft->price);
		if (!E_HireEmployeeByType(base, EMPL_PILOT))
			Com_Error(ERR_DROP, "B_SetUpFirstBase: Hiring pilot failed.");
		AIM_AutoEquipAircraft(aircraft);
	}
}

/**
 * @brief Counts the actual installation count limit
 * @returns int number of installations can be built
 */
int B_GetInstallationLimit (void)
{
	int limit = 0;
	base_t *base = NULL;

	/* count working Command Centers */
	while ((base = B_GetNext(base)) != NULL) {
		if (B_GetBuildingStatus(base, B_COMMAND))
			limit++;
	}

	return min(MAX_INSTALLATIONS, limit * MAX_INSTALLATIONS_PER_BASE);
}

/**
 * @brief Update menu script related cvars when the amount of bases changed.
 */
void B_UpdateBaseCount (void)
{
	/* this cvar is used for disabling the base build button on geoscape
	 * if MAX_BASES was reached */
	Cvar_SetValue("mn_base_count", B_GetCount());
}

/**
 * @brief Set the base name
 * @param[out] base The base to set the name for
 * @param[in] name The name for the base. This might already be in utf-8 as
 * it's the user input from the UI
 */
void B_SetName (base_t *base, const char *name)
{
	Q_strncpyz(base->name, name, sizeof(base->name));
}

/**
 * @brief Setup new base, uses template for the first base
 * @param[in] campaign The campaign data structure
 * @param[in,out] base The base to set up
 * @param[in] pos Position (on Geoscape) the base built at
 * @param[in] name The name of the new base, this string might already be in utf-8
 * @sa B_NewBase
 * @sa B_SetUpFirstBase
 */
void B_SetUpBase (const campaign_t *campaign, base_t* base, const vec2_t pos, const char *name)
{
	const int newBaseAlienInterest = 1.0f;
	float level;

	B_SetName(base, name);
	Vector2Copy(pos, base->pos);

	base->idx = B_GetBaseIDX(base);
	base->founded = qtrue;
	base->baseStatus = BASE_WORKING;

	/* increase this early because a lot of functions rely on this
	 * value to get the base we are setting up here */
	ccs.numBases++;

	/* setup for first base */
	if (ccs.campaignStats.basesBuilt == 0)
		B_SetUpFirstBase(campaign, base);
	else
		B_BuildFromTemplate(base, NULL, qtrue);

	/* a new base is not discovered (yet) */
	base->alienInterest = newBaseAlienInterest;

	BDEF_InitialiseBaseSlots(base);

	/* Reset Radar range */
	level = B_GetMaxBuildingLevel(base, B_RADAR);
	RADAR_Initialise(&base->radar, RADAR_BASERANGE, RADAR_BASETRACKINGRANGE, level, qfalse);
	RADAR_InitialiseUFOs(&base->radar);

	B_ResetAllStatusAndCapacities(base, qtrue);
	AL_FillInContainment(base);
	PR_UpdateProductionCap(base);

	ccs.campaignStats.basesBuilt++;
	ccs.mapAction = MA_NONE;
}

/**
 * @brief Returns the building in the global building-types list that has the unique name buildingID.
 * @param[in] buildingName The unique id of the building (building_t->id).
 * @return building_t If a building was found it is returned, if no id was give the current building is returned, otherwise->NULL.
 */
building_t *B_GetBuildingTemplate (const char *buildingName)
{
	int i = 0;

	assert(buildingName);
	for (i = 0; i < ccs.numBuildingTemplates; i++)
		if (!strcmp(ccs.buildingTemplates[i].id, buildingName))
			return &ccs.buildingTemplates[i];

	Com_Printf("Building %s not found\n", buildingName);
	return NULL;
}

/**
 * @brief Returns the baseTemplate in the global baseTemplate list that has the unique name baseTemplateID.
 * @param[in] baseTemplateID The unique id of the building (baseTemplate_t->name).
 * @return baseTemplate_t If a Template was found it is returned, otherwise->NULL.
 */
const baseTemplate_t *B_GetBaseTemplate (const char *baseTemplateID)
{
	int i = 0;

	if (!baseTemplateID)
		return NULL;

	for (i = 0; i < ccs.numBaseTemplates; i++)
		if (!strcmp(ccs.baseTemplates[i].id, baseTemplateID))
			return &ccs.baseTemplates[i];

	Com_Printf("Base Template %s not found\n", baseTemplateID);
	return NULL;
}

/**
 * @brief Checks whether you have enough credits to build this building
 * @param costs buildcosts of the building
 * @return qboolean true - enough credits
 * @return qboolean false - not enough credits
 *
 * @sa B_ConstructBuilding
 * @sa B_NewBuilding
 * Checks whether the given costs are bigger than the current available credits
 */
static inline qboolean B_CheckCredits (int costs)
{
	if (costs > ccs.credits)
		return qfalse;
	return qtrue;
}

/**
 * @brief Builds new building. And checks whether the player has enough credits
 * to construct the current selected building before starting construction.
 * @sa B_MarkBuildingDestroy
 * @sa B_CheckCredits
 * @sa CL_UpdateCredits
 * @return qboolean
 * @sa B_NewBuilding
 * @param[in,out] base The base to construct the building in
 * @param[in,out] building The building to construct
 */
static qboolean B_ConstructBuilding (base_t* base, building_t *building)
{
	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !building)
		return qfalse;

	/* enough credits to build this? */
	if (!B_CheckCredits(building->fixCosts)) {
		Com_DPrintf(DEBUG_CLIENT, "B_ConstructBuilding: Not enough credits to build: '%s'\n", building->id);
		B_ResetBuildingCurrent(base);
		return qfalse;
	}

	Com_DPrintf(DEBUG_CLIENT, "Construction of %s is starting\n", building->id);

	building->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
	building->timeStart = ccs.date.day;

	CL_UpdateCredits(ccs.credits - building->fixCosts);
	Cmd_ExecuteString("base_init");
	return qtrue;
}

/**
 * @brief Build new building.
 * @param[in,out] base The base to construct the building in
 * @param[in,out] building The building to construct
 * @sa B_MarkBuildingDestroy
 * @sa B_ConstructBuilding
 */
static void B_NewBuilding (base_t* base, building_t *building)
{
	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !building)
		return;

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		/* credits are updated in the construct function */
		if (B_ConstructBuilding(base, building)) {
			B_BuildingStatus(building);
			Com_DPrintf(DEBUG_CLIENT, "B_NewBuilding: building->buildingStatus = %i\n", building->buildingStatus);
		}
}

/**
 * @brief Set the currently selected building.
 * @param[in,out] base The base to place the building in
 * @param[in] buildingTemplate The template of the building to place at the given location
 * @param[in] row Set building to row
 * @param[in] col Set building to col
 * @return building created in base (this is not a building template)
 */
building_t* B_SetBuildingByClick (base_t *base, const building_t const *buildingTemplate, int row, int col)
{
#ifdef DEBUG
	if (!base)
		Com_Error(ERR_DROP, "no current base\n");
	if (!buildingTemplate)
		Com_Error(ERR_DROP, "no current building\n");
#endif
	if (!B_CheckCredits(buildingTemplate->fixCosts)) {
		UI_Popup(_("Notice"), _("Not enough credits to build this\n"));
		return NULL;
	}

	/* template should really be a template */
	/*assert(template == template->tpl);*/

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		/* new building in base (not a template) */
		building_t *buildingNew = B_GetBuildingByIDX(base->idx, ccs.numBuildings[base->idx]);

		/* copy building from template list to base-buildings-list */
		*buildingNew = *buildingTemplate;

		/* self-link to building-list in base */
		buildingNew->idx = B_GetBuildingIDX(base, buildingNew);

		/* Link to the base. */
		buildingNew->base = base;

		if (!base->map[row][col].blocked && !base->map[row][col].building) {
			/* No building in this place */
			if (buildingNew->needs) {
				if (col + 1 == BASE_SIZE) {
					if (base->map[row][col - 1].building
					 || base->map[row][col - 1].blocked) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return NULL;
					}
					col--;
				} else if (base->map[row][col + 1].building || base->map[row][col + 1].blocked) {
					if (base->map[row][col - 1].building
					 || base->map[row][col - 1].blocked
					 || !col) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return NULL;
					}
					col--;
				}

				base->map[row][col + 1].building = buildingNew;
			}
			/* Update number of buildings on the base. */
			ccs.numBuildings[base->idx]++;
			/* Credits are updated here, too */
			B_NewBuilding(base, buildingNew);

			base->map[row][col].building = buildingNew;

			/* where is this building located in our base? */
			buildingNew->pos[0] = row;
			buildingNew->pos[1] = col;

			B_ResetBuildingCurrent(base);
			Cmd_ExecuteString("building_init");

			return buildingNew;
		}
	}
	return NULL;
}

#define MAX_BUILDING_INFO_TEXT_LENGTH 512

/**
 * @brief Draws a building.
 * @param[in] building The building to draw
 */
void B_DrawBuilding (const building_t* building)
{
	static char buildingText[MAX_BUILDING_INFO_TEXT_LENGTH];

	/* maybe someone call this command before the buildings are parsed?? */
	if (!building)
		return;

	buildingText[0] = '\0';

	B_BuildingStatus(building);

	Com_sprintf(buildingText, sizeof(buildingText), "%s\n", _(building->name));

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION && building->fixCosts)
		Com_sprintf(buildingText, sizeof(buildingText), _("Costs:\t%i c\n"), building->fixCosts);

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION)
		Q_strcat(buildingText, va(ngettext("%i Day to build\n", "%i Days to build\n", building->buildTime), building->buildTime), sizeof(buildingText));

	if (building->varCosts)
		Q_strcat(buildingText, va(_("Running costs:\t%i c\n"), building->varCosts), sizeof(buildingText));

	if (building->dependsBuilding)
		Q_strcat(buildingText, va(_("Needs:\t%s\n"), _(building->dependsBuilding->name)), sizeof(buildingText));

	if (building->name)
		Cvar_Set("mn_building_name", _(building->name));

	if (building->image)
		Cvar_Set("mn_building_image", building->image);
	else
		Cvar_Set("mn_building_image", "base/empty");

	/* link into menu text array */
	UI_RegisterText(TEXT_BUILDING_INFO, buildingText);
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 * @param[in] base Which base to count in.
 * @param[in] tpl The template type in the ccs.buildingTemplates list.
 * @return The number of buildings or -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByTemplate (const base_t *base, const building_t *tpl)
{
	int numberOfBuildings = 0;
	building_t *building = NULL;

	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: No base given!\n");
		return -1;
	}

	if (!tpl) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: no building-type given!\n");
		return -1;
	}

	/* Check if the template really is one. */
	if (tpl != tpl->tpl) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: No building-type given as parameter. It's probably a normal building!\n");
		return -1;
	}

	while ((building = B_GetNextBuilding(base, building))) {
		if (building->tpl == tpl && building->buildingStatus != B_STATUS_NOT_SET)
			numberOfBuildings++;
	}
	return numberOfBuildings;
}

/**
 * @brief Counts the number of buildings of a particular building type in a base.
 * @param[in] base Which base to count in.
 * @param[in] buildingType Building type value.
 * @return The number of buildings or -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByBuildingType (const base_t *base, const buildingType_t buildingType)
{
	int numberOfBuildings = 0;
	building_t *building = NULL;

	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: No base given!\n");
		return -1;
	}

	if (buildingType >= MAX_BUILDING_TYPE) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: no sane building-type given!\n");
		return -1;
	}

	while ((building = B_GetNextBuildingByType(base, building, buildingType)))
		if (building->buildingStatus != B_STATUS_NOT_SET)
			numberOfBuildings++;

	return numberOfBuildings;
}

/**
 * @brief Returns the building type for a given building identified by its building id
 * from the ufo script files
 * @sa B_ParseBuildings
 * @sa B_GetBuildingType
 * @param[in] buildingID The script building id that should get converted into the enum value
 * @note Do not use B_GetBuildingType here, this is also used for parsing the types!
 */
buildingType_t B_GetBuildingTypeByBuildingID (const char *buildingID)
{
	if (!strcmp(buildingID, "lab")) {
		return B_LAB;
	} else if (!strcmp(buildingID, "hospital")) {
		return B_HOSPITAL;
	} else if (!strcmp(buildingID, "aliencont")) {
		return B_ALIEN_CONTAINMENT;
	} else if (!strcmp(buildingID, "workshop")) {
		return B_WORKSHOP;
	} else if (!strcmp(buildingID, "storage")) {
		return B_STORAGE;
	} else if (!strcmp(buildingID, "hangar")) {
		return B_HANGAR;
	} else if (!strcmp(buildingID, "smallhangar")) {
		return B_SMALL_HANGAR;
	} else if (!strcmp(buildingID, "quarters")) {
		return B_QUARTERS;
	} else if (!strcmp(buildingID, "workshop")) {
		return B_WORKSHOP;
	} else if (!strcmp(buildingID, "power")) {
		return B_POWER;
	} else if (!strcmp(buildingID, "command")) {
		return B_COMMAND;
	} else if (!strcmp(buildingID, "amstorage")) {
		return B_ANTIMATTER;
	} else if (!strcmp(buildingID, "entrance")) {
		return B_ENTRANCE;
	} else if (!strcmp(buildingID, "missile")) {
		return B_DEFENCE_MISSILE;
	} else if (!strcmp(buildingID, "laser")) {
		return B_DEFENCE_LASER;
	} else if (!strcmp(buildingID, "radar")) {
		return B_RADAR;
	}
	return MAX_BUILDING_TYPE;
}

/**
 * @brief Copies an entry from the building description file into the list of building types.
 * @note Parses one "building" entry in the basemanagement.ufo file and writes
 * it into the next free entry in bmBuildings[0], which is the list of buildings
 * in the first base (building_t).
 * @param[in] name Unique script id of a building. This is parsed from "building xxx" -> id=xxx.
 * @param[in] text the whole following text that is part of the "building" item definition in .ufo.
 * @param[in] link Bool value that decides whether to link the tech pointer in or not
 * @sa CL_ParseScriptFirst (link is false here)
 * @sa CL_ParseScriptSecond (link it true here)
 */
void B_ParseBuildings (const char *name, const char **text, qboolean link)
{
	building_t *building;
	technology_t *techLink;
	const value_t *vp;
	const char *errhead = "B_ParseBuildings: unexpected end of file (names ";
	const char *token;

	/* get id list body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("B_ParseBuildings: building \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numBuildingTemplates >= MAX_BUILDINGS)
		Com_Error(ERR_DROP, "B_ParseBuildings: too many buildings");

	if (!link) {
		int i;
		for (i = 0; i < ccs.numBuildingTemplates; i++) {
			if (!strcmp(ccs.buildingTemplates[i].id, name)) {
				Com_Printf("B_ParseBuildings: Second building with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* new entry */
		building = &ccs.buildingTemplates[ccs.numBuildingTemplates];
		memset(building, 0, sizeof(*building));
		building->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

		Com_DPrintf(DEBUG_CLIENT, "...found building %s\n", building->id);

		/* set standard values */
		building->tpl = building;	/* Self-link just in case ... this way we can check if it is a template or not. */
		building->idx = -1;			/* No entry in buildings list (yet). */
		building->base = NULL;
		building->buildingType = MAX_BUILDING_TYPE;
		building->dependsBuilding = NULL;
		building->visible = qtrue;
		building->maxCount = -1;	/* Default: no limit */

		ccs.numBuildingTemplates++;
		do {
			/* get the name type */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;

			/* get values */
			if (!strcmp(token, "type")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				building->buildingType = B_GetBuildingTypeByBuildingID(token);
				if (building->buildingType >= MAX_BUILDING_TYPE)
					Com_Printf("didn't find buildingType '%s'\n", token);
			} else {
				/* no linking yet */
				if (!strcmp(token, "depends")) {
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
				} else {
					for (vp = valid_building_vars; vp->string; vp++)
						if (!strncmp(token, vp->string, sizeof(vp->string))) {
							/* found a definition */
							token = Com_EParse(text, errhead, name);
							if (!*text)
								return;

							switch (vp->type) {
							case V_NULL:
								break;
							case V_TRANSLATION_STRING:
								token++;
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)building + (int)vp->ofs), cp_campaignPool, 0);
								break;
							default:
								Com_EParseValue(building, token, vp->type, vp->ofs, vp->size);
								break;
							}
							break;
						}
					if (!vp->string)
						Com_Printf("B_ParseBuildings: unknown token \"%s\" ignored (building %s)\n", token, name);
				}
			}
		} while (*text);
	} else {
		building = B_GetBuildingTemplate(name);
		if (!building)
			Com_Error(ERR_DROP, "B_ParseBuildings: Could not find building with id %s\n", name);

		techLink = RS_GetTechByProvided(name);
		if (techLink)
			building->tech = techLink;
		else if (building->visible)
			Com_Error(ERR_DROP, "B_ParseBuildings: Could not find tech that provides %s\n", name);

		do {
			/* get the name type */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;
			/* get values */
			if (!strcmp(token, "depends")) {
				const building_t *dependsBuilding = B_GetBuildingTemplate(Com_EParse(text, errhead, name));
				if (!dependsBuilding)
					Com_Error(ERR_DROP, "Could not find building depend of %s\n", building->id);
				building->dependsBuilding = dependsBuilding;
				if (!*text)
					return;
			}
		} while (*text);
	}
}

/**
 * @brief Gets a building of a given type in the given base
 * @param[in] base The base to search the building in
 * @param[in] buildingType What building-type to get.
 * @param[in] onlyWorking If we're looking only for working buildings
 * @return The building or NULL if base has no building of that type
 */
const building_t *B_GetBuildingInBaseByType (const base_t* base, buildingType_t buildingType, qboolean onlyWorking)
{
	building_t *building = NULL;

	/* we maybe only want to get the working building (e.g. it might the
	 * case that we don't have a powerplant and thus the searched building
	 * is not functional) */
	if (onlyWorking && !B_GetBuildingStatus(base, buildingType))
		return NULL;

	while ((building = B_GetNextBuildingByType(base, building, buildingType)))
		return building;

	return NULL;
}

/**
 * @brief Reads a base layout template
 * @param[in] name The script id of the base template
 * @param[in] text The script block to parse
 * @sa CL_ParseScriptFirst
 */
void B_ParseBaseTemplate (const char *name, const char **text)
{
	const char *errhead = "B_ParseBaseTemplate: unexpected end of file (names ";
	const char *token;
	baseTemplate_t* baseTemplate;
	baseBuildingTile_t* tile;
	vec2_t pos;
	qboolean map[BASE_SIZE][BASE_SIZE];
	byte buildingNums[MAX_BUILDINGS];
	int i;

	/* get token */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBaseTemplate: Template \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numBaseTemplates >= MAX_BASETEMPLATES)
		Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many base templates");

	/* create new Template */
	baseTemplate = &ccs.baseTemplates[ccs.numBaseTemplates];
	baseTemplate->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

	/* clear map for checking duplicate positions and buildingNums for checking moreThanOne constraint */
	memset(&map, 0, sizeof(map));
	memset(&buildingNums, 0, sizeof(buildingNums));

	ccs.numBaseTemplates++;

	do {
		/* get the building */
		token = Com_EParse(text, errhead, baseTemplate->id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (baseTemplate->numBuildings >= MAX_BASEBUILDINGS)
			Com_Error(ERR_DROP, "B_ParseBaseTemplate: too many buildings");

		/* check if building type is known */
		tile = &baseTemplate->buildings[baseTemplate->numBuildings];
		baseTemplate->numBuildings++;

		for (i = 0; i < ccs.numBuildingTemplates; i++)
			if (!strcmp(ccs.buildingTemplates[i].id, token)) {
				tile->building = &ccs.buildingTemplates[i];
				if (tile->building->maxCount >= 0 && tile->building->maxCount <= buildingNums[i])
					Com_Error(ERR_DROP, "B_ParseBaseTemplate: Found more %s than allowed in template %s (%d))", token, baseTemplate->id, tile->building->maxCount);
				buildingNums[i]++;
				break;
			}

		if (!tile->building)
			Com_Error(ERR_DROP, "B_ParseBaseTemplate: Could not find building with id %s\n", baseTemplate->id);

		/* get the position */
		token = Com_EParse(text, errhead, baseTemplate->id);
		if (!*text)
			break;
		if (*token == '}')
			break;

		Com_EParseValue(pos, token, V_POS, 0, sizeof(vec2_t));
		tile->posX = pos[0];
		tile->posY = pos[1];
		if (tile->posX < 0 || tile->posX >= BASE_SIZE || tile->posY < 0 || tile->posY >= BASE_SIZE)
			Com_Error(ERR_DROP, "Invalid template coordinates for building %s in template %s given",
					tile->building->id, baseTemplate->id);

		/* check for buildings on same position */
		if (map[tile->posX][tile->posY])
			Com_Error(ERR_DROP, "Base template '%s' has ambiguous positions for buildings set.", baseTemplate->id);
		map[tile->posX][tile->posY] = qtrue;
	} while (*text);

	/* templates without the must-have buildings can't be used */
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t *building = &ccs.buildingTemplates[i];
		if (building && building->mandatory && !buildingNums[i]) {
			Com_Error(ERR_DROP, "Every base template needs one '%s'! '%s' has none.", building->id, baseTemplate->id);
		}
	}
}

/**
 * @brief Get the first unfounded base
 * @return first unfounded base or NULL if every available base slot is already filled
 */
base_t *B_GetFirstUnfoundedBase (void)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetBaseByIDX(baseIdx);
		if (!base->founded)
			return base;
	}

	return NULL;
}

/**
 * @sa B_SelectBase
 * @param[in] base The base that is going to be selected
 */
void B_SetCurrentSelectedBase (const base_t *base)
{
	base_t *b = NULL;
	while ((b = B_GetNextFounded(b)) != NULL) {
		if (b == base) {
			b->selected = qtrue;
			if (b->aircraftCurrent == NULL)
				b->aircraftCurrent = AIR_GetFirstFromBase(b);
		} else
			b->selected = qfalse;
	}

	if (base) {
		INS_SetCurrentSelectedInstallation(NULL);
		Cvar_Set("mn_base_title", base->name);
		Cvar_SetValue("mn_base_status_id", base->baseStatus);
	} else {
		Cvar_Set("mn_base_title", "");
		Cvar_Set("mn_base_status_id", "");
	}
}

base_t *B_GetCurrentSelectedBase (void)
{
	base_t *base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL)
		if (base->selected)
			return base;

	return NULL;
}

/**
 * @param[in] base If this is @c NULL we want to build a new base
 */
void B_SelectBase (const base_t *base)
{
	/* set up a new base */
	if (!base) {
		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (ccs.mapAction == MA_NEWBASE) {
			MAP_ResetAction();
			return;
		}

		if (B_GetCount() < MAX_BASES) {
			/* show radar overlay (if not already displayed) */
			if (!MAP_IsRadarOverlayActivated())
				MAP_SetOverlay("radar");
			ccs.mapAction = MA_NEWBASE;
		} else {
			ccs.mapAction = MA_NONE;
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: select base with id %i\n", base->idx);
		ccs.mapAction = MA_NONE;
		UI_PushWindow("bases", NULL, NULL);
		B_SetCurrentSelectedBase(base);
	}
}

/**
 * @brief Swaps one skill from character1 to character 2 and vice versa.
 */
static void CL_SwapSkill (character_t *cp1, character_t *cp2, abilityskills_t skill)
{
	int tmp1, tmp2;
	tmp1 = cp1->score.skills[skill];
	tmp2 = cp2->score.skills[skill];
	cp1->score.skills[skill] = tmp2;
	cp2->score.skills[skill] = tmp1;

	tmp1 = cp1->score.initialSkills[skill];
	tmp2 = cp2->score.initialSkills[skill];
	cp1->score.initialSkills[skill] = tmp2;
	cp2->score.initialSkills[skill] = tmp1;

	tmp1 = cp1->score.experience[skill];
	tmp2 = cp2->score.experience[skill];
	cp1->score.experience[skill] = tmp2;
	cp2->score.experience[skill] = tmp1;
}

static void CL_DoSwapSkills (character_t *cp1, character_t *cp2, const int skill)
{
	if (cp1->score.skills[skill] < cp2->score.skills[skill])
		CL_SwapSkill(cp1, cp2, skill);

	switch (skill) {
	case SKILL_CLOSE:
		if (cp1->score.skills[ABILITY_SPEED] < cp2->score.skills[ABILITY_SPEED])
			CL_SwapSkill(cp1, cp2, ABILITY_SPEED);
		break;
	case SKILL_HEAVY:
		if (cp1->score.skills[ABILITY_POWER] < cp2->score.skills[ABILITY_POWER])
			CL_SwapSkill(cp1, cp2, ABILITY_POWER);
		break;
	case SKILL_ASSAULT:
		/* no related basic attribute */
		break;
	case SKILL_SNIPER:
		if (cp1->score.skills[ABILITY_ACCURACY] < cp2->score.skills[ABILITY_ACCURACY])
			CL_SwapSkill(cp1, cp2, ABILITY_ACCURACY);
		break;
	case SKILL_EXPLOSIVE:
		if (cp1->score.skills[ABILITY_MIND] < cp2->score.skills[ABILITY_MIND])
			CL_SwapSkill(cp1, cp2, ABILITY_MIND);
		break;
	default:
		Com_Error(ERR_DROP, "CL_SwapSkills: illegal skill %i.\n", skill);
	}
}

/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 * @todo This currently always uses exactly the first two firemodes (see fmode1+fmode2) for calculation. This needs to be adapted to support less (1) or more 3+ firemodes. I think the function will even  break on only one firemode .. never tested it.
 * @todo i think currently also the different ammo/firedef types for each weapon (different weaponr_fd_idx and weaponr_fd_idx values) are ignored.
 */
static void CL_SwapSkills (chrList_t *team)
{
	int i, j, k, skill;
	const byte fmode1 = 0;
	const byte fmode2 = 1;

	i = team->num;
	while (i--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (j = 0; j < team->num - 1; j++) {
				character_t *cp1 = team->chr[j];
				const fireDef_t *fdRightArray = NULL;
				const fireDef_t *fdHolsterArray = NULL;

				if (RIGHT(cp1) && RIGHT(cp1)->item.m && RIGHT(cp1)->item.t)
					fdRightArray = FIRESH_FiredefForWeapon(&RIGHT(cp1)->item);
				if (HOLSTER(cp1) && HOLSTER(cp1)->item.m && HOLSTER(cp1)->item.t)
					fdHolsterArray = FIRESH_FiredefForWeapon(&HOLSTER(cp1)->item);
				/* disregard left hand, or dual-wielding guys are too good */

				if (fdHolsterArray != NULL && fdRightArray != NULL) {
					const int no1 = 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[fdRightArray->weapFdsIdx][fmode1].weaponSkill)
						+ 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[fdRightArray->weapFdsIdx][fmode2].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode1].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode2].weaponSkill);

					for (k = j + 1; k < team->num; k++) {
						character_t *cp2 = team->chr[k];
						fdRightArray = NULL;
						fdHolsterArray = NULL;

						if (RIGHT(cp2) && RIGHT(cp2)->item.m && RIGHT(cp2)->item.t)
							fdRightArray = FIRESH_FiredefForWeapon(&RIGHT(cp2)->item);
						if (HOLSTER(cp2) && HOLSTER(cp2)->item.m && HOLSTER(cp2)->item.t)
							fdHolsterArray = FIRESH_FiredefForWeapon(&HOLSTER(cp2)->item);

						if (fdHolsterArray != NULL && fdRightArray != NULL) {
							const int no2 = 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[fdRightArray->weapFdsIdx][fmode1].weaponSkill)
								+ 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[fdRightArray->weapFdsIdx][fmode2].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode1].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[fdHolsterArray->weapFdsIdx][fmode2].weaponSkill);

							if (no1 > no2 /* more use of this skill */
								 || (no1 && no1 == no2)) { /* or earlier on list */

								CL_DoSwapSkills(cp1, cp2, skill);
							} else if (no1 < no2) {
								CL_DoSwapSkills(cp2, cp1, skill);
							}
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Assigns initial soldier equipment for the first base
 * @todo Move this function to a better place - has nothing to do with bases anymore
 */
static void B_PackInitialEquipment (aircraft_t *aircraft, const equipDef_t *ed)
{
	base_t *base = aircraft->homebase;
	chrList_t chrListTemp;
	employee_t *employee;

	if (!aircraft)
		return;

	chrListTemp.num = 0;
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		character_t *chr = &employee->chr;
		/* pack equipment */
		Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment: Packing initial equipment for %s.\n", chr->name);
		cls.i.EquipActor(&cls.i, &chr->i, ed, chr->teamDef);
		chrListTemp.chr[chrListTemp.num] = chr;
		chrListTemp.num++;
	}

	if (base) {
		AIR_MoveEmployeeInventoryIntoStorage(aircraft, &base->storage);
		B_UpdateStorageCap(base);
	}
	CL_SwapSkills(&chrListTemp);
}

/**
 * @brief Sets the baseStatus to BASE_NOT_USED
 * @param[in] base Which base should be reseted?
 * @sa CL_CampaignRemoveMission
 */
void B_BaseResetStatus (base_t* const base)
{
	assert(base);
	base->baseStatus = BASE_NOT_USED;
	if (ccs.mapAction == MA_BASEATTACK)
		ccs.mapAction = MA_NONE;
}

#ifdef DEBUG
/**
 * @brief Just lists all buildings with their data
 * @note called with debug_listbuilding
 * @note Just for debugging purposes - not needed in game
 */
static void B_BuildingList_f (void)
{
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		int j;

		Com_Printf("\nBase id %i: %s\n", base->idx, base->name);
		/** @todo building count should not depend on base->idx. base->idx will not be an array index! */
		for (j = 0; j < ccs.numBuildings[base->idx]; j++) {
			const building_t *building = B_GetBuildingByIDX(base->idx, j);
			int k;

			Com_Printf("...Building: %s #%i - id: %i\n", building->id,
				B_GetNumberOfBuildingsInBaseByTemplate(base, building->tpl), base->idx);
			Com_Printf("...image: %s\n", building->image);
			Com_Printf(".....Status:\n");

			for (k = 0; k < BASE_SIZE * BASE_SIZE; k++) {
				if (k > 1 && k % BASE_SIZE == 0)
					Com_Printf("\n");
				Com_Printf("%i ", building->buildingStatus);
				if (!building->buildingStatus)
					break;
			}
			Com_Printf("\n");
		}
	}
}

/**
 * @brief Just lists all bases with their data
 * @note called with debug_listbase
 * @note Just for debugging purposes - not needed in game
 */
static void B_BaseList_f (void)
{
	int row, col, j;
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		if (!base->founded) {
			Com_Printf("Base idx %i not founded\n\n", base->idx);
			continue;
		}

		Com_Printf("Base idx %i\n", base->idx);
		Com_Printf("Base name %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base numMissileBattery %i\n", base->numBatteries);
		Com_Printf("Base numLaserBattery %i\n", base->numLasers);
		Com_Printf("Base radarRange %i\n", base->radar.range);
		Com_Printf("Base trackingRange %i\n", base->radar.trackingRange);
		Com_Printf("Base numSensoredAircraft %i\n", base->radar.numUFOs);
		Com_Printf("Base Alien interest %f\n", base->alienInterest);
		Com_Printf("Base hasBuilding[]:\n");
		Com_Printf("Misc  Lab Quar Stor Work Hosp Hang Cont SHgr UHgr SUHg Powr  Cmd AMtr Entr Miss Lasr  Rdr Team\n");
		for (j = 0; j < MAX_BUILDING_TYPE; j++)
			Com_Printf("  %i  ", B_GetBuildingStatus(base, j));
		Com_Printf("Base pos %.02f:%.02f\n", base->pos[0], base->pos[1]);
		Com_Printf("Base map:\n");
		for (row = 0; row < BASE_SIZE; row++) {
			if (row > 0)
				Com_Printf("\n");
			for (col = 0; col < BASE_SIZE; col++)
				Com_Printf("%2i (%3i: %3i)  ", (base->map[row][col].building ? base->map[row][col].building->idx : -1),
					base->map[row][col].posX, base->map[row][col].posY);
		}
		Com_Printf("\n\n");
	}
}
#endif

/**
 * @brief Checks whether the building menu or the pedia entry should be called
 * when you click a building in the baseview
 * @param[in] building The building we have clicked
 */
void B_BuildingOpenAfterClick (const building_t *building)
{
	const base_t *base = building->base;
	if (!B_GetBuildingStatus(base, building->buildingType)) {
		UP_OpenWith(building->pedia);
	} else {
		switch (building->buildingType) {
		case B_LAB:
			if (RS_ResearchAllowed(base))
				UI_PushWindow("research", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_HOSPITAL:
			if (HOS_HospitalAllowed(base))
				UI_PushWindow("hospital", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_ALIEN_CONTAINMENT:
			if (AC_ContainmentAllowed(base))
				UI_PushWindow("aliencont", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_QUARTERS:
			if (E_HireAllowed(base))
				UI_PushWindow("employees", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_WORKSHOP:
			if (PR_ProductionAllowed(base))
				UI_PushWindow("production", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_DEFENCE_LASER:
		case B_DEFENCE_MISSILE:
			UI_PushWindow("basedefence", NULL, NULL);
			break;
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (!AIR_AircraftAllowed(base)) {
				UP_OpenWith(building->pedia);
			} else if (AIR_BaseHasAircraft(base)) {
				UI_PushWindow("aircraft", NULL, NULL);
			} else {
				UI_PushWindow("buyaircraft", NULL, NULL);
				/* transfer is only possible when there are at least two bases */
				if (B_GetCount() > 1)
					UI_Popup(_("Note"), _("No aircraft in this base - You first have to purchase or transfer an aircraft\n"));
				else
					UI_Popup(_("Note"), _("No aircraft in this base - You first have to purchase an aircraft\n"));
			}
			break;
		case B_STORAGE:
			if (BS_BuySellAllowed(base))
				UI_PushWindow("market", NULL, NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_ANTIMATTER:
			Com_sprintf(popupText, sizeof(popupText), "%s %d/%d", _("Antimatter (current/max):"), base->capacities[CAP_ANTIMATTER].cur, base->capacities[CAP_ANTIMATTER].max);
			UI_Popup(_("Information"), popupText);
			break;
		default:
			UP_OpenWith(building->pedia);
			break;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Debug function for printing all capacities in given base.
 * @note called with debug_listcapacities
 */
static void B_PrintCapacities_f (void)
{
	int i, j;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);
	for (i = 0; i < MAX_CAP; i++) {
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(i);
		if (buildingType >= MAX_BUILDING_TYPE)
			Com_Printf("B_PrintCapacities_f: Could not find building associated with capacity %i\n", i);
		else {
			for (j = 0; j < ccs.numBuildingTemplates; j++) {
				if (ccs.buildingTemplates[j].buildingType == buildingType)
					break;
			}
			Com_Printf("Building: %s, capacity max: %i, capacity cur: %i\n",
			ccs.buildingTemplates[j].id, base->capacities[i].max, base->capacities[i].cur);
		}
	}
}

/**
 * @brief Finishes the construction of every building in the base
 */
static void B_BuildingConstructionFinished_f (void)
{
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (i = 0; i < ccs.numBuildings[base->idx]; i++) {
		building_t *building = B_GetBuildingByIDX(base->idx, i);

		if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
			B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);

			if (building->onConstruct[0] != '\0') {
				base->buildingCurrent = building;
				Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
			}
		}
	}
	/* update menu */
	B_SelectBase(base);
}

/**
 * @brief Recalculate status and capacities
 * @note function called with debug_basereset or after loading
 */
static void B_ResetAllStatusAndCapacities_f (void)
{
	base_t *base;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		/* set buildingStatus[] and capacities.max values */
		B_ResetAllStatusAndCapacities(base, qfalse);
	}
}
#endif

/**
 * @brief Resets console commands.
 * @sa UI_InitStartup
 */
void B_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listbase", B_BaseList_f, "Print base information to the game console");
	Cmd_AddCommand("debug_listbuilding", B_BuildingList_f, "Print building information to the game console");
	Cmd_AddCommand("debug_listcapacities", B_PrintCapacities_f, "Debug function to show all capacities in given base");
	Cmd_AddCommand("debug_basereset", B_ResetAllStatusAndCapacities_f, "Reset building status and capacities of all bases");
	Cmd_AddCommand("debug_destroybase", CL_BaseDestroy_f, "Destroy a base");
	Cmd_AddCommand("debug_buildingfinished", B_BuildingConstructionFinished_f, "Finish construction for every building in the current base");
#endif
}

/**
 * @brief Counts the number of bases.
 * @return The number of founded bases.
 */
int B_GetFoundedBaseCount (void)
{
	int cnt = 0;
	base_t *base = NULL;

	while ((base = B_GetNextFounded(base)) != NULL)
		cnt++;

	return cnt;
}

/**
 * @brief Checks whether the construction of a building is finished.
 * Calls the onConstruct functions and assign workers, too.
 */
static int B_CheckBuildingConstruction (building_t *building)
{
	int newBuilding = 0;

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		if (building->timeStart && building->timeStart + building->buildTime <= ccs.date.day) {
			B_UpdateAllBaseBuildingStatus(building, B_STATUS_WORKING);

			if (building->onConstruct[0] != '\0') {
				base_t *base = building->base;
				base->buildingCurrent = building;
				Com_DPrintf(DEBUG_CLIENT, "B_CheckBuildingConstruction: %s %i;\n", building->onConstruct, base->idx);
				Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
			}

			newBuilding++;
		}
	}

	if (newBuilding)
		Cmd_ExecuteString("building_init");

	return newBuilding;
}

/**
 * @brief Updates base data
 * @sa CL_CampaignRun
 * @note called every "day"
 * @sa AIRFIGHT_ProjectileHitsBase
 */
void B_UpdateBaseData (void)
{
	base_t *base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL) {
		building_t *building = NULL;
		while ((building = B_GetNextBuilding(base, building))) {
			if (B_CheckBuildingConstruction(building)) {
				Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
						_("Construction of %s building finished in %s."), _(building->name), base->name);
				MS_AddNewMessage(_("Building finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			}
		}
	}
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] aircraft Pointer to an aircraft landing in base.
 * @sa B_AircraftReturnedToHomeBase
 */
static void B_SellOrAddItems (aircraft_t *aircraft)
{
	int i;
	int numitems = 0;
	int gained = 0;
	int forcedsold = 0;
	int forcedgained = 0;
	itemsTmp_t *cargo;
	base_t *base;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemTypes; i++) {
		const objDef_t *item = cargo[i].item;
		const int amount = cargo[i].amount;
		technology_t *tech = RS_GetTechForItem(item);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			B_UpdateStorageAndCapacity(base, item, amount, qfalse, qtrue);
			if (amount > 0)
				RS_MarkCollected(tech);
			continue;
		} else {
			/* If the related technology is researched, check the autosell option. */
			if (ccs.eMarket.autosell[item->idx]) { /* Sell items if autosell is enabled. */
				BS_AddItemToMarket(item, amount);
				gained += (item->price * amount);
				numitems += amount;
			} else {
				int j;
				/* Check whether there is enough space for adding this item.
				 * If yes - add. If not - sell. */
				for (j = 0; j < amount; j++) {
					if (!B_UpdateStorageAndCapacity(base, item, 1, qfalse, qfalse)) {
						/* Not enough space, sell item. */
						BS_AddItemToMarket(item, 1);
						forcedgained += item->price;
						forcedsold++;
					}
				}
			}
			continue;
		}
	}

	if (numitems > 0) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("By selling %s you gathered %i credits."),
			va(ngettext("%i collected item", "%i collected items", numitems), numitems), gained);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	}
	if (forcedsold > 0) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("Not enough storage space in %s. %s"),
			base->name, va(ngettext("%i item was sold for %i credits.", "%i items were sold for %i credits.", forcedsold), forcedsold, forcedgained));
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	}
	CL_UpdateCredits(ccs.credits + gained + forcedgained);

	/* ship no longer has cargo aboard */
	aircraft->itemTypes = 0;

	/* Mark new technologies researchable. */
	RS_MarkResearchable(qfalse, aircraft->homebase);
	/* Recalculate storage capacity, to fix wrong capacity if a soldier drops something on the ground */
	B_UpdateStorageCap(aircraft->homebase);
}

/**
 * @brief Will unload all cargo to the homebase
 * @param[in,out] aircraft The aircraft to dump
 */
void B_DumpAircraftToHomeBase (aircraft_t *aircraft)
{
	aliensTmp_t *cargo;

	/* Don't call cargo functions if aircraft is not a transporter. */
	if (aircraft->type != AIRCRAFT_TRANSPORTER)
		return;

	/* Add aliens to Alien Containment. */
	AL_AddAliens(aircraft);
	/* Sell collected items or add them to storage. */
	B_SellOrAddItems(aircraft);

	/* Now empty alien/item cargo just in case. */
	cargo = AL_GetAircraftAlienCargo(aircraft);
	memset(cargo, 0, sizeof(*cargo));
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));
	AL_SetAircraftAlienCargoTypes(aircraft, 0);
}

/**
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft Returning aircraft.
 * @note Place here any stuff, which should be called when Drophip returns to base.
 * @sa CL_CampaignRunAircraft
 */
void B_AircraftReturnedToHomeBase (aircraft_t* aircraft)
{
	/* Reset UFO sensored on radar */
	RADAR_InitialiseUFOs(&aircraft->radar);
	/* Reload weapons */
	AII_ReloadAircraftWeapons(aircraft);

	B_DumpAircraftToHomeBase(aircraft);
}

/**
 * @brief Check if an item is available on a base
 * @param[in] base Pointer to the base to check at
 * @param[in] item Pointer to the item to check
 */
qboolean B_BaseHasItem (const base_t *base, const objDef_t *item)
{
	if (item->isVirtual)
		return qtrue;

	return B_ItemInBase(item, base) > 0;
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage) in the given base.
 * @param[in] item The item to check
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 */
int B_ItemInBase (const objDef_t *item, const base_t *base)
{
	const equipDef_t *ed;

	if (!item)
		return -1;
	if (item->isVirtual)
		return -1;
	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	return ed->numItems[item->idx];
}

/**
 * @brief Updates base capacities.
 * @param[in] cap Enum type of baseCapacities_t.
 * @param[in] base Pointer to the base.
 * @sa B_UpdateAllBaseBuildingStatus
 * @sa B_BuildingDestroy_f
 * @note If hasBuilding is qfalse, the capacity is still increase: if power plant is destroyed and rebuilt, you shouldn't have to hire employees again
 */
void B_UpdateBaseCapacities (baseCapacities_t cap, base_t *base)
{
	int i, capacity = 0, buildingTemplateIDX = -1;
	buildingType_t buildingType;
	building_t *building;

	/* Get building type. */
	buildingType = B_GetBuildingTypeByCapacity(cap);

	switch (cap) {
	case CAP_ALIENS:		/**< Update Aliens capacity in base. */
	case CAP_EMPLOYEES:		/**< Update employees capacity in base. */
	case CAP_LABSPACE:		/**< Update laboratory space capacity in base. */
	case CAP_WORKSPACE:		/**< Update workshop space capacity in base. */
	case CAP_ITEMS:			/**< Update items capacity in base. */
	case CAP_AIRCRAFT_SMALL:	/**< Update aircraft capacity in base. */
	case CAP_AIRCRAFT_BIG:		/**< Update aircraft capacity in base. */
	case CAP_ANTIMATTER:		/**< Update antimatter capacity in base. */
		/* Reset given capacity in current base. */
		base->capacities[cap].max = 0;
		/* Get building capacity. */
		for (i = 0; i < ccs.numBuildingTemplates; i++) {
			const building_t *b = &ccs.buildingTemplates[i];
			if (b->buildingType == buildingType) {
				capacity = b->capacity;
				Com_DPrintf(DEBUG_CLIENT, "Building: %s capacity: %i\n", b->id, capacity);
				buildingTemplateIDX = i;
				break;
			}
		}
		/* Finally update capacity. */
		building = NULL;
		while ((building = B_GetNextBuildingByType(base, building, buildingType)))
			if (building->buildingStatus >= B_STATUS_CONSTRUCTION_FINISHED)
				base->capacities[cap].max += capacity;

		if (buildingTemplateIDX != -1)
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: updated capacity of %s: %i\n",
				ccs.buildingTemplates[buildingTemplateIDX].id, base->capacities[cap].max);
		break;
	case MAX_CAP:			/**< Update all capacities in base. */
		Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: going to update ALL capacities.\n");
		/* Loop through all capacities and update them. */
		for (i = 0; i < cap; i++) {
			B_UpdateBaseCapacities(i, base);
		}
		break;
	default:
		Com_Error(ERR_DROP, "Unknown capacity limit for this base: %i \n", cap);
	}
}

/**
 * @brief Saves the missile and laser slots of a base or sam site.
 * @param[in] weapons Defence weapons array
 * @param[in] numWeapons Number of entries in weapons array
 * @param[out] node XML Node structure, where we write the information to
 */
void B_SaveBaseSlotsXML (const baseWeapon_t *weapons, const int numWeapons, mxml_node_t *node)
{
	int i;

	for (i = 0; i < numWeapons; i++) {
		mxml_node_t *sub = mxml_AddNode(node, SAVE_BASES_WEAPON);
		AII_SaveOneSlotXML(sub, &weapons[i].slot, qtrue);
		mxml_AddBoolValue(sub, SAVE_BASES_AUTOFIRE, weapons[i].autofire);
		if (weapons[i].target)
			mxml_AddInt(sub, SAVE_BASES_TARGET, weapons[i].target->idx);
	}
}

/**
 * @brief Saves base storage
 * @param[out] parent XML Node structure, where we write the information to
 * @param[in] equip Storage to save
 */
qboolean B_SaveStorageXML (mxml_node_t *parent, const equipDef_t equip)
{
	int k;
	for (k = 0; k < csi.numODs; k++) {
		const objDef_t *od = INVSH_GetItemByIDX(k);
		if (equip.numItems[k] || equip.numItemsLoose[k]) {
			mxml_node_t *node = mxml_AddNode(parent, SAVE_BASES_ITEM);

			mxml_AddString(node, SAVE_BASES_ODS_ID, od->id);
			mxml_AddIntValue(node, SAVE_BASES_NUM, equip.numItems[k]);
			mxml_AddByteValue(node, SAVE_BASES_NUMLOOSE, equip.numItemsLoose[k]);
		}
	}
	return qtrue;
}
/**
 * @brief Save callback for saving in xml format.
 * @param[out] parent XML Node structure, where we write the information to
 */
qboolean B_SaveXML (mxml_node_t *parent)
{
	mxml_node_t * bases;
	base_t *b;

	bases = mxml_AddNode(parent, SAVE_BASES_BASES);
	b = NULL;
	while ((b = B_GetNext(b)) != NULL) {
		int k;
		mxml_node_t * act_base, *node;
		building_t *building;

		if (!b->founded) {
			Com_Printf("B_SaveXML: Base (idx: %i) not founded!\n", b->idx);
			return qfalse;
		}

		Com_RegisterConstList(saveBaseConstants);

		act_base = mxml_AddNode(bases, SAVE_BASES_BASE);
		mxml_AddString(act_base, SAVE_BASES_NAME, b->name);
		mxml_AddPos3(act_base, SAVE_BASES_POS, b->pos);
		mxml_AddString(act_base, SAVE_BASES_BASESTATUS, Com_GetConstVariable(SAVE_BASESTATUS_NAMESPACE, b->baseStatus));
		mxml_AddFloat(act_base, SAVE_BASES_ALIENINTEREST, b->alienInterest);
		if (b->aircraftCurrent)
			mxml_AddInt(act_base, SAVE_BASES_CURRENTAIRCRAFTIDX, AIR_GetAircraftIDXInBase(b->aircraftCurrent));

		/* building space */
		node = mxml_AddNode(act_base, SAVE_BASES_BUILDINGSPACE);
		for (k = 0; k < BASE_SIZE; k++) {
			int l;
			for (l = 0; l < BASE_SIZE; l++) {
				mxml_node_t * snode = mxml_AddNode(node, SAVE_BASES_BUILDING);
				mxml_AddInt(snode, SAVE_BASES_X, k);
				mxml_AddInt(snode, SAVE_BASES_Y, l);
				if (b->map[k][l].building)
					mxml_AddInt(snode, SAVE_BASES_BUILDINGINDEX, b->map[k][l].building->idx);
				mxml_AddBoolValue(snode, SAVE_BASES_BLOCKED, b->map[k][l].blocked);
			}
		}
		/* buildings */
		node = mxml_AddNode(act_base, SAVE_BASES_BUILDINGS);
		building = NULL;
		while ((building = B_GetNextBuilding(b, building))) {
			mxml_node_t * snode;

			if (!building->tpl)
				continue;

			snode = mxml_AddNode(node, SAVE_BASES_BUILDING);
			mxml_AddString(snode, SAVE_BASES_BUILDINGTYPE, building->tpl->id);
			mxml_AddInt(snode, SAVE_BASES_BUILDING_PLACE, building->idx);
			mxml_AddString(snode, SAVE_BASES_BUILDINGSTATUS, Com_GetConstVariable(SAVE_BUILDINGSTATUS_NAMESPACE, building->buildingStatus));
			mxml_AddInt(snode, SAVE_BASES_BUILDINGTIMESTART, building->timeStart);
			mxml_AddInt(snode, SAVE_BASES_BUILDINGBUILDTIME, building->buildTime);
			mxml_AddFloat(snode, SAVE_BASES_BUILDINGLEVEL, building->level);
			mxml_AddPos2(snode, SAVE_BASES_POS, building->pos);
		}
		/* base defences */
		node = mxml_AddNode(act_base, SAVE_BASES_BATTERIES);
		B_SaveBaseSlotsXML(b->batteries, b->numBatteries, node);
		node = mxml_AddNode(act_base, SAVE_BASES_LASERS);
		B_SaveBaseSlotsXML(b->lasers, b->numLasers, node);
		/* store equipment */
		node = mxml_AddNode(act_base, SAVE_BASES_STORAGE);
		B_SaveStorageXML(node, b->storage);
		/* radar */
		mxml_AddIntValue(act_base, SAVE_BASES_RADARRANGE, b->radar.range);
		mxml_AddIntValue(act_base, SAVE_BASES_TRACKINGRANGE, b->radar.trackingRange);

		Com_UnregisterConstList(saveBaseConstants);
	}
	return qtrue;
}

/**
 * @brief Loads the missile and laser slots of a base or sam site.
 * @param[out] weapons Defence weapons array
 * @param[out] max Number of entries in weapons array
 * @param[in] p XML Node structure, where we load the information from
 * @sa B_Load
 * @sa B_SaveBaseSlots
 */
int B_LoadBaseSlotsXML (baseWeapon_t* weapons, int max, mxml_node_t *p)
{
	int i;
	mxml_node_t *s;
	for (i = 0, s = mxml_GetNode(p, SAVE_BASES_WEAPON); s && i < max; i++, s = mxml_GetNextNode(s, p, SAVE_BASES_WEAPON)) {
		const int target = mxml_GetInt(s, SAVE_BASES_TARGET, -1);
		AII_LoadOneSlotXML(s, &weapons[i].slot, qtrue);
		weapons[i].autofire = mxml_GetBool(s, SAVE_BASES_AUTOFIRE, qtrue);
		weapons[i].target = (target >= 0) ? UFO_GetByIDX(target) : NULL;
	}
	return i;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa B_PostLoadInit
 */
static qboolean B_PostLoadInitCapacity (void)
{
	base_t *base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL)
		B_ResetAllStatusAndCapacities(base, qtrue);

	return qtrue;
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
qboolean  B_PostLoadInit (void)
{
	return B_PostLoadInitCapacity();
}

/**
 * @brief Loads base storage
 * @param[in] parent XML Node structure, where we get the information from
 * @param[out] equip Storage to load
 */
qboolean B_LoadStorageXML (mxml_node_t *parent, equipDef_t *equip)
{
	mxml_node_t *node;
	for (node = mxml_GetNode(parent, SAVE_BASES_ITEM); node; node = mxml_GetNextNode(node, parent, SAVE_BASES_ITEM)) {
		const char *s = mxml_GetString(node, SAVE_BASES_ODS_ID);
		objDef_t *od = INVSH_GetItemByID(s);

		if (!od) {
			Com_Printf("B_Load: Could not find item '%s'\n", s);
		} else {
			equip->numItems[od->idx] = mxml_GetInt(node, SAVE_BASES_NUM, 0);
			equip->numItemsLoose[od->idx] = mxml_GetInt(node, SAVE_BASES_NUMLOOSE, 0);
		}
	}
	return qtrue;
}

/**
 * @brief Loads base data
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean B_LoadXML (mxml_node_t *parent)
{
	int i;
	int buildingIdx;
	mxml_node_t *bases, *base;

	bases = mxml_GetNode(parent, "bases");
	if (!bases) {
		Com_Printf("Error: Node 'bases' wasn't found in savegame\n");
		return qfalse;
	}

	ccs.numBases = 0;

	Com_RegisterConstList(saveBaseConstants);
	for (base = mxml_GetNode(bases, SAVE_BASES_BASE), i = 0; i < MAX_BASES && base; i++, base = mxml_GetNextNode(base, bases, SAVE_BASES_BASE)) {
		mxml_node_t * node, * snode;
		int aircraftIdxInBase;
		base_t *const b = B_GetBaseByIDX(i);
		const char *str = mxml_GetString(base, SAVE_BASES_BASESTATUS);
		int j;

		ccs.numBases++;

		b->idx = B_GetBaseIDX(b);
		b->founded = qtrue;
		if (!Com_GetConstIntFromNamespace(SAVE_BASESTATUS_NAMESPACE, str, (int*) &b->baseStatus)) {
			Com_Printf("Invalid base status '%s'\n", str);
			Com_UnregisterConstList(saveBaseConstants);
			return qfalse;
		}

		Q_strncpyz(b->name, mxml_GetString(base, SAVE_BASES_NAME), sizeof(b->name));
		mxml_GetPos3(base, SAVE_BASES_POS, b->pos);
		b->alienInterest = mxml_GetFloat(base, SAVE_BASES_ALIENINTEREST, 0.0);

		aircraftIdxInBase = mxml_GetInt(base, SAVE_BASES_CURRENTAIRCRAFTIDX, AIRCRAFT_INBASE_INVALID);
		if (aircraftIdxInBase != AIRCRAFT_INBASE_INVALID) {
			/* aircraft are not yet loaded! */
			/** @todo this must be set after the aircraft are loaded */
			/*b->aircraftCurrent = &b->aircraft[aircraftIdxInBase];*/
			b->aircraftCurrent = NULL;
		} else {
			b->aircraftCurrent = NULL;
		}

		/* building space*/
		node = mxml_GetNode(base, SAVE_BASES_BUILDINGSPACE);
		for (snode = mxml_GetNode(node, SAVE_BASES_BUILDING); snode; snode = mxml_GetNextNode(snode, node, SAVE_BASES_BUILDING)) {
			const int k = mxml_GetInt(snode, SAVE_BASES_X, 0);
			const int l = mxml_GetInt(snode, SAVE_BASES_Y, 0);
			baseBuildingTile_t* tile = &b->map[k][l];
			buildingIdx = mxml_GetInt(snode, SAVE_BASES_BUILDINGINDEX, -1);

			if (buildingIdx != -1)
				/* The buildings are actually parsed _below_. (See PRE_MAXBUI loop) */
				tile->building = B_GetBuildingByIDX(i, buildingIdx);
			else
				tile->building = NULL;
			tile->blocked = mxml_GetBool(snode, SAVE_BASES_BLOCKED, qfalse);
		}
		/* buildings */
		node = mxml_GetNode(base, SAVE_BASES_BUILDINGS);
		for (j = 0, snode = mxml_GetNode(node, SAVE_BASES_BUILDING); snode; snode = mxml_GetNextNode(snode, node, SAVE_BASES_BUILDING), j++) {
			const int buildId = mxml_GetInt(snode, SAVE_BASES_BUILDING_PLACE, MAX_BUILDINGS);
			building_t *building;
			char buildingType[MAX_VAR];

			if (buildId >= MAX_BUILDINGS) {
				Com_Printf("building ID is greater than MAX buildings\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			Q_strncpyz(buildingType, mxml_GetString(snode, SAVE_BASES_BUILDINGTYPE), sizeof(buildingType));
			if (buildingType[0] == '\0') {
				Com_Printf("No buildingtype set\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			building = B_GetBuildingTemplate(buildingType);
			if (!building)
				continue;

			ccs.buildings[i][buildId] = *building;
			building = B_GetBuildingByIDX(i, buildId);
			building->idx = B_GetBuildingIDX(b, building);
			if (building->idx != buildId) {
				Com_Printf("building ID doesn't match\n");
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}
			building->base = b;

			str = mxml_GetString(snode, SAVE_BASES_BUILDINGSTATUS);
			if (!Com_GetConstIntFromNamespace(SAVE_BUILDINGSTATUS_NAMESPACE, str, (int*) &building->buildingStatus)) {
				Com_Printf("Invalid building status '%s'\n", str);
				Com_UnregisterConstList(saveBaseConstants);
				return qfalse;
			}

			building->timeStart = mxml_GetInt(snode, SAVE_BASES_BUILDINGTIMESTART, 0);
			building->buildTime = mxml_GetInt(snode, SAVE_BASES_BUILDINGBUILDTIME, 0);
			building->level = mxml_GetFloat(snode, SAVE_BASES_BUILDINGLEVEL, 0);
			mxml_GetPos2(snode, SAVE_BASES_POS, building->pos);
		}
		ccs.numBuildings[i] = j;

		BDEF_InitialiseBaseSlots(b);
		/* read missile battery slots */
		node = mxml_GetNode(base, SAVE_BASES_BATTERIES);
		if (node)
			b->numBatteries = B_LoadBaseSlotsXML(b->batteries, MAX_BASE_SLOT, node);
		/* read laser battery slots */
		node = mxml_GetNode(base, SAVE_BASES_LASERS);
		if (node)
			b->numLasers = B_LoadBaseSlotsXML(b->lasers, MAX_BASE_SLOT, node);
		/* read equipment */
		node = mxml_GetNode(base, SAVE_BASES_STORAGE);
		B_LoadStorageXML(node, &(b->storage));
		/* read radar info */
		RADAR_InitialiseUFOs(&b->radar);
		RADAR_Initialise(&b->radar, mxml_GetInt(base, SAVE_BASES_RADARRANGE, 0), mxml_GetInt(base, SAVE_BASES_TRACKINGRANGE, 0), B_GetMaxBuildingLevel(b, B_RADAR), qtrue);

		/** @todo can't we use something like I_DestroyInventory here? */
		/* clear the mess of stray loaded pointers */
		memset(&b->bEquipment, 0, sizeof(b->bEquipment));
		/* reset capacities */
	}
	Com_UnregisterConstList(saveBaseConstants);

	B_UpdateBaseCount();

	return qtrue;
}

/**
 * @brief Check if an item is stored in storage.
 * @param[in] obj Pointer to the item to check.
 * @return True if item is stored in storage.
 */
qboolean B_ItemIsStoredInBaseStorage (const objDef_t *obj)
{
	/* antimatter is stored in antimatter storage */
	if (obj->isVirtual || !strcmp(obj->id, ANTIMATTER_TECH_ID))
		return qfalse;

	return qtrue;
}

/**
 * @brief Add/remove items to/from the storage.
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] obj The item.
 * @param[in] amount Amount to be added to removed
 * @return the added/removed amount
 * @note The main difference between B_AddToStorage and B_UpdateStorageAndCapacity is that
 * B_AddToStorage adds/removes as many items as possible if adding/removing all not possible
 * also B_AddToStorage don't have a reset method
 */
int B_AddToStorage (base_t* base, const objDef_t *obj, int amount)
{
	assert(base);
	assert(obj);

	if (!B_ItemIsStoredInBaseStorage(obj))
		return 0;

	if (amount > 0) {
		if (obj->size > 0) {
			const int freeSpace = base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur;
			/* correct amount and update capacity */
			amount = min(amount, freeSpace / obj->size);
			base->capacities[CAP_ITEMS].cur += (amount * obj->size);
		}
		base->storage.numItems[obj->idx] += amount;
	} else if (amount < 0) {
		/* correct amount */
		amount = max(amount, -B_ItemInBase(obj, base));
		if (obj->size > 0)
			base->capacities[CAP_ITEMS].cur += (amount * obj->size);
		base->storage.numItems[obj->idx] += amount;
	}

	return amount;
}

/**
 * @brief Update the storage amount and the capacities for the storages in the base
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] obj The item.
 * @param[in] amount Amount to be added to removed
 * @param[in] reset Set this to true (amount is not needed) if you want to reset the
 * storage amount and capacities (e.g. in case of a base ransack)
 * @param[in] ignorecap qtrue if we won't check freespace but will just add items.
 * @sa CL_BaseRansacked
 */
qboolean B_UpdateStorageAndCapacity (base_t* base, const objDef_t *obj, int amount, qboolean reset, qboolean ignorecap)
{
	assert(base);
	assert(obj);

	if (obj->isVirtual)
		return qtrue;

	if (reset) {
		base->storage.numItems[obj->idx] = 0;
		base->storage.numItemsLoose[obj->idx] = 0;
		base->capacities[CAP_ITEMS].cur = 0;
	} else {
		if (!B_ItemIsStoredInBaseStorage(obj)) {
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Item '%s' is not stored in storage: skip\n", obj->id);
			return qfalse;
		}

		if (!ignorecap && amount > 0) {
			/* Only add items if there is enough room in storage */
			if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < (obj->size * amount)) {
				Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Not enough storage space (item: %s, amount: %i)\n", obj->id, amount);
				return qfalse;
			}
		}

		base->storage.numItems[obj->idx] += amount;
		if (obj->size > 0)
			base->capacities[CAP_ITEMS].cur += (amount * obj->size);

		if (base->capacities[CAP_ITEMS].cur < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current storage capacity is negative (%i): reset to 0\n", base->capacities[CAP_ITEMS].cur);
			base->capacities[CAP_ITEMS].cur = 0;
		}

		if (base->storage.numItems[obj->idx] < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current number of item '%s' is negative: reset to 0\n", obj->id);
			base->storage.numItems[obj->idx] = 0;
		}
	}

	return qtrue;
}

/**
 * @brief Checks the parsed buildings for errors
 * @return false if there are errors - true otherwise
 */
qboolean B_ScriptSanityCheck (void)
{
	int i, error = 0;
	building_t* b;

	for (i = 0, b = ccs.buildingTemplates; i < ccs.numBuildingTemplates; i++, b++) {
		if (!b->mapPart && b->visible) {
			error++;
			Com_Printf("...... no mappart for building '%s' given\n", b->id);
		}
		if (!b->name) {
			error++;
			Com_Printf("...... no name for building '%s' given\n", b->id);
		}
		if (!b->image) {
			error++;
			Com_Printf("...... no image for building '%s' given\n", b->id);
		}
		if (!b->pedia) {
			error++;
			Com_Printf("...... no pedia link for building '%s' given\n", b->id);
		} else if (!RS_GetTechByID(b->pedia)) {
			error++;
			Com_Printf("...... could not get pedia entry tech (%s) for building '%s'\n", b->pedia, b->id);
		}
	}
	if (!error)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Remove items until everything fits in storage.
 * @note items will be randomly selected for removal.
 * @param[in] base Pointer to the base
 */
void B_RemoveItemsExceedingCapacity (base_t *base)
{
	int i;
	int objIdx[MAX_OBJDEFS];	/**< Will contain idx of items that can be removed */
	int num, cnt;

	if (base->capacities[CAP_ITEMS].cur <= base->capacities[CAP_ITEMS].max)
		return;

	for (i = 0, num = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);

		if (!B_ItemIsStoredInBaseStorage(obj))
			continue;

		/* Don't count item that we don't have in base */
		if (B_ItemInBase(obj, base) <= 0)
			continue;

		objIdx[num++] = i;
	}

	cnt = E_CountHired(base, EMPL_ROBOT);
	/* UGV takes room in storage capacity: we store them with a value MAX_OBJDEFS that can't be used by objIdx */
	for (i = 0; i < cnt; i++) {
		objIdx[num++] = MAX_OBJDEFS;
	}

	while (num && base->capacities[CAP_ITEMS].cur > base->capacities[CAP_ITEMS].max) {
		/* Select the item to remove */
		const int randNumber = rand() % num;
		if (objIdx[randNumber] >= MAX_OBJDEFS) {
			/* A UGV is destroyed: get first one */
			employee_t* employee = E_GetHiredRobot(base, 0);
			/* There should be at least a UGV */
			assert(employee);
			E_DeleteEmployee(employee);
		} else {
			/* items are destroyed. We guess that all items of a given type are stored in the same location
			 *	=> destroy all items of this type */
			const int idx = objIdx[randNumber];
			objDef_t *od = INVSH_GetItemByIDX(idx);
			B_UpdateStorageAndCapacity(base, od, -B_ItemInBase(od, base), qfalse, qfalse);
		}
		REMOVE_ELEM(objIdx, randNumber, num);

		/* Make sure that we don't have an infinite loop */
		if (num <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "B_RemoveItemsExceedingCapacity: Remains %i in storage for a maximum of %i\n",
		base->capacities[CAP_ITEMS].cur, base->capacities[CAP_ITEMS].max);
}

/**
 * @brief Update Storage Capacity.
 * @param[in] base Pointer to the base
 * @sa B_ResetAllStatusAndCapacities_f
 */
void B_UpdateStorageCap (base_t *base)
{
	int i;

	base->capacities[CAP_ITEMS].cur = 0;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);

		if (!B_ItemIsStoredInBaseStorage(obj))
			continue;

		base->capacities[CAP_ITEMS].cur += B_ItemInBase(obj, base) * obj->size;
	}

	/* UGV takes room in storage capacity */
	base->capacities[CAP_ITEMS].cur += UGV_SIZE * E_CountHired(base, EMPL_ROBOT);
}

/**
 * @brief returns the amount of antimatter stored in a base
 * @param[in] base Pointer to the base to check
 */
int B_AntimatterInBase (const base_t *base)
{
#ifdef DEBUG
	objDef_t *od;

	od = INVSH_GetItemByID(ANTIMATTER_TECH_ID);
	if (od == NULL)
		Com_Error(ERR_DROP, "Could not find "ANTIMATTER_TECH_ID" object definition");

	assert(base);
	assert(B_ItemInBase(od, base) == base->capacities[CAP_ANTIMATTER].cur);
#endif

	return base->capacities[CAP_ANTIMATTER].cur;
}

/**
 * @brief Manages antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in,out] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove (> 0 even if antimatter is removed)
 * @param[in] add True if we are adding antimatter, false when removing.
 * @note This function should be called whenever we add or remove antimatter from Antimatter Storage Facility.
 * @note Call with amount = 0 if you want to remove ALL antimatter from given base.
 */
void B_ManageAntimatter (base_t *base, int amount, qboolean add)
{
	objDef_t *od;

	assert(base);

	if (add && !B_GetBuildingStatus(base, B_ANTIMATTER)) {
		Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
			_("%s does not have Antimatter Storage Facility. %i units of antimatter got removed."),
			base->name, amount);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
		return;
	}

	od = INVSH_GetItemByIDSilent(ANTIMATTER_TECH_ID);
	if (od == NULL)
		Com_Error(ERR_DROP, "Could not find "ANTIMATTER_TECH_ID" object definition");

	if (add) {	/* Adding. */
		const int a = min(amount, base->capacities[CAP_ANTIMATTER].max - base->capacities[CAP_ANTIMATTER].cur);
		base->storage.numItems[od->idx] += a;
		base->capacities[CAP_ANTIMATTER].cur += a;
	} else {	/* Removing. */
		if (amount == 0) {
			base->capacities[CAP_ANTIMATTER].cur = 0;
			base->storage.numItems[od->idx] = 0;
		} else {
			const int a = min(amount, base->capacities[CAP_ANTIMATTER].cur);
			base->capacities[CAP_ANTIMATTER].cur -= a;
			base->storage.numItems[od->idx] -= a;
		}
	}
}

/**
 * @brief Remove exceeding antimatter if an antimatter tank has been destroyed.
 * @param[in] base Pointer to the base.
 */
void B_RemoveAntimatterExceedingCapacity (base_t *base)
{
	const int amount = base->capacities[CAP_ANTIMATTER].cur - base->capacities[CAP_ANTIMATTER].max;
	if (amount <= 0)
		return;

	B_ManageAntimatter(base, amount, qfalse);
}
