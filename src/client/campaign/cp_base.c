/**
 * @file cp_base.c
 * @brief Handles everything that is located in or accessed trough a base.
 * @note Basemanagement functions prefix: B_
 * @note See "base/ufos/basemanagement.ufo", "base/ufos/menu_bases.ufo" and "base/ufos/menu_buildings.ufo" for the underlying content.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_game.h"
#include "../cl_team.h"
#include "cl_mapfightequip.h"
#include "cp_aircraft.h"
#include "cp_aircraft_callbacks.h"
#include "cp_missions.h"
#include "../cl_view.h"
#include "cl_map.h"
#include "cl_uforecovery.h"
#include "cl_popup.h"
#include "../renderer/r_draw.h"
#include "../menu/m_popup.h"
#include "cp_geoscape_actions.h"
#include "cp_time.h"
#include "cp_base_callbacks.h"
#include "../../shared/parse.h"

vec3_t newBasePos;
building_t *buildingConstructionList[MAX_BUILDINGS];

static void B_AssignInitial(aircraft_t *aircraft, const equipDef_t *ed);

/**
 * @brief Array bound check for the base index.
 * @return Pointer to the base corresponding to baseIdx.
 */
base_t* B_GetBaseByIDX (int baseIdx)
{
	if (baseIdx >= MAX_BASES || baseIdx < 0)
		Com_Error(ERR_DROP, "Invalid base index of %i given", baseIdx);
	return &gd.bases[baseIdx];
}

/**
 * @brief Array bound check for the base index.
 * @return Pointer to the base corresponding to baseIdx if base is founded, NULL else.
 */
base_t* B_GetFoundedBaseByIDX (int baseIdx)
{
	base_t *base = B_GetBaseByIDX(baseIdx);

	if (base->founded)
		return base;

	return NULL;
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
	int cntlocal = 0, i;

	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		if (gd.buildings[base->idx][i].buildingType == type
		 && gd.buildings[base->idx][i].buildingStatus == status) {
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
	case B_UFO_HANGAR:
		return CAP_UFOHANGARS_LARGE;
	case B_UFO_SMALL_HANGAR:
		return CAP_UFOHANGARS_SMALL;
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
	case CAP_UFOHANGARS_SMALL:
		return B_UFO_SMALL_HANGAR;
	case CAP_UFOHANGARS_LARGE:
		return B_UFO_HANGAR;
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
	assert(buildingType >= 0);

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
	assert(buildingType >= 0);

	if (buildingType == B_MISC)
		Com_Printf("B_SetBuildingStatus: No status is associated to B_MISC type of building.\n");
	else if (buildingType < MAX_BUILDING_TYPE) {
		base->hasBuilding[buildingType] = newStatus;
		Com_DPrintf(DEBUG_CLIENT, "B_SetBuildingStatus: set status for %i to %i\n", buildingType, newStatus);
	} else
		Com_Printf("B_SetBuildingStatus: Type of building %i does not exists\n", buildingType);
}

/**
 * @brief Check that the dependences of a building is operationnal
 * @param[in] base Base to check
 * @param[in] building
 * @return true if base contains needed dependence for entering building
 */
qboolean B_CheckBuildingDependencesStatus (const base_t* const base, const building_t* building)
{
	assert(base);
	assert(building);

	if (!building->dependsBuilding)
		return qtrue;

	/* Make sure the dependsBuilding pointer is really a template .. just in case. */
	assert(building->dependsBuilding == building->dependsBuilding->tpl);

	return B_GetBuildingStatus(base, building->dependsBuilding->buildingType);
}

/**
 * @note Make sure you are not doing anything with the buildingCurrent pointer
 * in this function, the pointer might already be invalid
 */
void B_ResetBuildingCurrent (base_t* base)
{
	if (base)
		base->buildingCurrent = NULL;
	gd.baseAction = BA_NONE;
}

/**
 * @brief Holds the names of valid entries in the basemanagement.ufo file.
 *
 * The valid definition names for BUILDINGS (building_t) in the basemagagement.ufo file.
 * to the appropriate values in the corresponding struct
 */
static const value_t valid_building_vars[] = {
	{"map_name", V_CLIENT_HUNK_STRING, offsetof(building_t, mapPart), 0},	/**< Name of the map file for generating basemap. */
	{"more_than_one", V_BOOL, offsetof(building_t, moreThanOne), MEMBER_SIZEOF(building_t, moreThanOne)},	/**< Is the building allowed to be build more the one time? */
	{"level", V_FLOAT, offsetof(building_t, level), MEMBER_SIZEOF(building_t, level)},	/**< building level */
	{"name", V_TRANSLATION_STRING, offsetof(building_t, name), 0},	/**< The displayed building name. */
	{"pedia", V_CLIENT_HUNK_STRING, offsetof(building_t, pedia), 0},	/**< The pedia-id string for the associated pedia entry. */
	{"status", V_INT, offsetof(building_t, buildingStatus), MEMBER_SIZEOF(building_t, buildingStatus)},	/**< The current status of the building. */
	{"image", V_CLIENT_HUNK_STRING, offsetof(building_t, image), 0},	/**< Identifies the image for the building. */
	{"visible", V_BOOL, offsetof(building_t, visible), MEMBER_SIZEOF(building_t, visible)}, /**< Determines whether a building should be listed in the construction list. Set the first part of a building to 1 all others to 0 otherwise all building-parts will be on the list */
	{"needs", V_CLIENT_HUNK_STRING, offsetof(building_t, needs), 0},	/**<  For buildings with more than one part; the other parts of the building needed.*/
	{"fixcosts", V_INT, offsetof(building_t, fixCosts), MEMBER_SIZEOF(building_t, fixCosts)},	/**< Cost to build. */
	{"varcosts", V_INT, offsetof(building_t, varCosts), MEMBER_SIZEOF(building_t, varCosts)},	/**< Costs that will come up by using the building. */
	{"build_time", V_INT, offsetof(building_t, buildTime), MEMBER_SIZEOF(building_t, buildTime)},	/**< How many days it takes to construct the building. */
	{"starting_employees", V_INT, offsetof(building_t, maxEmployees), MEMBER_SIZEOF(building_t, maxEmployees)},	/**< How many employees to hire on construction in the first base. */
	{"capacity", V_INT, offsetof(building_t, capacity), MEMBER_SIZEOF(building_t, capacity)},	/**< A size value that is used by many buildings in a different way. */

	/*event handler functions */
	{"onconstruct", V_STRING, offsetof(building_t, onConstruct), 0}, /**< Event handler. */
	{"onattack", V_STRING, offsetof(building_t, onAttack), 0}, /**< Event handler. */
	{"ondestroy", V_STRING, offsetof(building_t, onDestroy), 0}, /**< Event handler. */
	{"pos", V_POS, offsetof(building_t, pos), MEMBER_SIZEOF(building_t, pos)}, /**< Place of a building. Needed for flag autobuild */
	{"autobuild", V_BOOL, offsetof(building_t, autobuild), MEMBER_SIZEOF(building_t, autobuild)}, /**< Automatically construct this building when a base is set up. Must also set the pos-flag. */
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
	int i;
	float max = 0.0f;

	if (B_GetBuildingStatus(base, type)) {
		for (i = 0; i < gd.numBuildings[base->idx]; i++) {
			if (gd.buildings[base->idx][i].buildingType == type
			 && gd.buildings[base->idx][i].buildingStatus == B_STATUS_WORKING) {
				max = max(gd.buildings[base->idx][i].level, max);
			}
		}
	}

	return max;
}

/**
 * @brief Check base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @param[in] base Pointer to base with given building.
 * @note This function checks  base status for particular buildings and base capacities.
 * @return qtrue if a base status has been modified (but do not check capacities)
 */
static qboolean B_CheckUpdateBuilding (building_t* building, base_t* base)
{
	qboolean oldValue;

	assert(base);
	assert(building);

	/* Status of Miscellenious buildings cannot change. */
	if (building->buildingType == B_MISC)
		return qfalse;

	oldValue = B_GetBuildingStatus(base, building->buildingType);
	if (building->buildingStatus == B_STATUS_WORKING
	 && B_CheckBuildingDependencesStatus(base, building))
		B_SetBuildingStatus(base, building->buildingType, qtrue);
	else
		B_SetBuildingStatus(base, building->buildingType, qfalse);

	if (B_GetBuildingStatus(base, building->buildingType) != oldValue) {
		Com_DPrintf(DEBUG_CLIENT, "Status of building %s is changed to %i.\n",
			building->name, B_GetBuildingStatus(base, building->buildingType));
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
		Cmd_ExecuteString(va("update_base_radar_coverage %i;", base->idx));
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
		Cmd_ExecuteString(va("update_base_radar_coverage %i;", base->idx));
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
	int i;

	/* Construction / destruction may have changed the status of other building
	 * We check that, but only for buildings which needed building */
	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		building_t *dependsBuilding = gd.buildings[base->idx][i].dependsBuilding;
		if (dependsBuilding && buildingType == dependsBuilding->buildingType) {
			/* gd.buildings[base->idx][i] needs built/removed building */
			if (onBuilt && !B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnEnable(gd.buildings[base->idx][i].buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnDisable(gd.buildings[base->idx][i].buildingType, base);
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
		for (i = 0; i < gd.numBuildings[base->idx]; i++) {
			if (onBuilt && !B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnEnable(gd.buildings[base->idx][i].buildingType, base);
					test = qtrue;
				}
			} else if (!onBuilt && B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnDisable(gd.buildings[base->idx][i].buildingType, base);
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
	int i;

	for (i = 0; i < csi.numODs; i++) {
		if (!Q_strncmp(csi.ods[i].id, "antimatter", 10)) {
			base->capacities[CAP_ANTIMATTER].cur = (base->storage.num[i] * ANTIMATTER_SIZE);
			return;
		}
	}
}

/**
 * @brief Recalculate status and capacities of one base
 * @param[in] base Pointer to the base where status and capacities must be recalculated
 * @param[in] firstEnable qtrue if this is the first time the function is called for this base
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 */
void B_ResetAllStatusAndCapacities (base_t *base, qboolean firstEnable)
{
	int buildingIdx, i;
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
		test = qfalse;
		for (buildingIdx = 0; buildingIdx < gd.numBuildings[base->idx]; buildingIdx++) {
			building_t *building = &gd.buildings[base->idx][buildingIdx];
			if (!B_GetBuildingStatus(base, building->buildingType)
			 && B_CheckUpdateBuilding(building, base)) {
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
		base->capacities[CAP_LABSPACE].cur = RS_CountInBase(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_WORKSPACE)))
		PR_UpdateProductionCap(base);

	if (B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_UFOHANGARS_SMALL)) ||
		B_GetBuildingStatus(base, B_GetBuildingTypeByCapacity(CAP_UFOHANGARS_LARGE)))
		UR_UpdateUFOHangarCapForAll(base);

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
	baseCapacities_t capacity;
	int aircraftIdx;
	aircraft_t *awayAircraft[MAX_AIRCRAFT];
	int numawayAircraft, randomNum;

	memset(awayAircraft, 0, sizeof(awayAircraft));

	/* destroy aircraft only if there's not enough hangar (hangar is already destroyed) */
	capacity = B_GetCapacityFromBuildingType(buildingType);
	if (base->capacities[capacity].cur <= base->capacities[capacity].max)
		return;

	/* destroy one aircraft (must not be sold: may be destroyed by aliens) */
	for (aircraftIdx = 0, numawayAircraft = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
		const int aircraftSize = base->aircraft[aircraftIdx].size;
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
			Sys_Error("B_RemoveAircraftExceedingCapacity: Unkown type of aircraft '%i'\n", aircraftSize);
		}

		/** @todo move aircraft being transfered to the destBase */

		/* Only aircraft in hangar will be destroyed by hangar destruction */
		if (!AIR_IsAircraftInBase(&base->aircraft[aircraftIdx])) {
			if (AIR_IsAircraftOnGeoscape(&base->aircraft[aircraftIdx]))
				awayAircraft[numawayAircraft++] = &base->aircraft[aircraftIdx];
			continue;
		}

		/* Remove aircraft and aircraft items, but do not fire employees */
		AIR_DeleteAircraft(base, &base->aircraft[aircraftIdx]);
		awayAircraft[numawayAircraft++] = NULL;
		return;
	}

	if (!numawayAircraft)
		return;
	/* All aircraft are away from base, pick up one and change it's homebase */
	randomNum = rand() % numawayAircraft;
	if (!CL_DisplayHomebasePopup(awayAircraft[randomNum], qfalse)) {
		/* No base can hold this aircraft
		 @todo fixme Better solution ? */
		AIR_DeleteAircraft(awayAircraft[randomNum]->homebase, awayAircraft[randomNum]);
	}
}

/** @todo should go into cp_base_callbacks.c */
void B_BaseMenuInit (const base_t *base)
{
	/* make sure the credits cvar is up-to-date */
	CL_UpdateCredits(ccs.credits);

	Cvar_SetValue("mn_base_num_aircraft", base->numAircraftInBase);

	MN_ExecuteConfunc("mn_buildings_reset");
	/* activate or deactivate the aircraft button */
	if (AIR_AircraftAllowed(base) && base->numAircraftInBase)
		MN_ExecuteConfunc("set_aircraft_enabled");
	else
		MN_ExecuteConfunc("set_aircraft_disabled");

	if (BS_BuySellAllowed(base))
		MN_ExecuteConfunc("set_buysell_enabled");
	else
		MN_ExecuteConfunc("set_buysell_disabled");

	if (gd.numBases > 1 && base->baseStatus != BASE_UNDER_ATTACK)
		MN_ExecuteConfunc("set_transfer_enabled");
	else
		MN_ExecuteConfunc("set_transfer_disabled");

	if (RS_ResearchAllowed(base))
		MN_ExecuteConfunc("set_research_enabled");
	else
		MN_ExecuteConfunc("set_research_disabled");

	if (PR_ProductionAllowed(base)) {
		MN_ExecuteConfunc("set_prod_enabled");
	} else {
		MN_ExecuteConfunc("set_prod_disabled");
	}

	if (E_HireAllowed(base))
		MN_ExecuteConfunc("set_hire_enabled");
	else
		MN_ExecuteConfunc("set_hire_disabled");

	if (AC_ContainmentAllowed(base))
		MN_ExecuteConfunc("set_containment_enabled");
	else
		MN_ExecuteConfunc("set_containment_disabled");

	if (HOS_HospitalAllowed(base))
		MN_ExecuteConfunc("set_hospital_enabled");
	else
		MN_ExecuteConfunc("set_hospital_disabled");
}

/**
 * @brief Removes a building from the given base
 * @param[in] base Base to remove the building in
 * @param[in] building The building to remove
 * @note Also updates capacities and sets the hasBuilding[] values in base_t
 * @sa B_BuildingDestroy_f
 */
qboolean B_BuildingDestroy (base_t* base, building_t* building)
{
	const buildingType_t buildingType = building->buildingType;
	const building_t const *template = building->tpl;	/**< Template of the removed building */
	const qboolean onDestroyCommand = (building->onDestroy[0] != '\0') && (building->buildingStatus == B_STATUS_WORKING);
	baseCapacities_t cap = MAX_CAP; /* init but don't set to first value of enum */
	qboolean test;

	/* Don't allow to destroy an entrance. */
	if (buildingType == B_ENTRANCE)
		return qfalse;

	if (!base->map[(int)building->pos[0]][(int)building->pos[1]].building
	 || base->map[(int)building->pos[0]][(int)building->pos[1]].building != building) {
		Sys_Error("B_BuildingDestroy: building mismatch at base %i pos %i,%i.\n", base->idx, (int)building->pos[0], (int)building->pos[1]);
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
		int         const base_idx  = base->idx;
		building_t* const buildings = gd.buildings[base_idx];
		int         const idx       = building->idx;
		int               cntBldgs;
		int               i;

		REMOVE_ELEM(buildings, idx, gd.numBuildings[base_idx]);

		/* Update the link of other buildings */
		cntBldgs = gd.numBuildings[base_idx];
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

	test = qfalse;

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
			cap = B_GetCapacityFromBuildingType(buildingType);
			if (cap != MAX_CAP)
				B_UpdateBaseCapacities(cap, base);
		}
	}

	B_BaseMenuInit(base);

	/* call ondestroy trigger only if building is not under construction
	 * (we do that after base capacity has been updated) */
	if (onDestroyCommand) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i %i;\n",
			template->onDestroy, base->idx, buildingType);
		Cmd_ExecuteString(va("%s %i %i;", template->onDestroy, base->idx, buildingType));
	}

	return qtrue;
}

/**
 * @brief Destroy a base.
 * @param[in] base Pointer to base to be destroyed.
 * @note If you want to sell items or unhire employees, you should do it before
 * calling this function - they are going to be killed / destroyed.
 */
void CL_BaseDestroy (base_t *base)
{
	int buildingIdx;

	assert(base);

	CP_MissionNotifyBaseDestroyed(base);

	/* do a reverse loop as buildings are going to be destroyed */
	for (buildingIdx = gd.numBuildings[base->idx] - 1; buildingIdx >= 0; buildingIdx--) {
		B_BuildingDestroy(base, &gd.buildings[base->idx][buildingIdx]);
	}
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

	CL_BaseDestroy(base);
}
#endif

/**
 * @brief Mark a building for destruction - you only have to confirm it now
 * @note Also calls the ondestroy trigger
 */
void B_MarkBuildingDestroy (base_t* base, building_t* building)
{
	int cap;
	assert(base);

	/* you can't destroy buildings if base is under attack */
	if (base->baseStatus == BASE_UNDER_ATTACK) {
		Com_sprintf(popupText, sizeof(popupText), _("Base is under attack, you can't destroy buildings !"));
		MN_Popup(_("Notice"), popupText);
		return;
	}

	assert(building);
	cap = B_GetCapacityFromBuildingType(building->buildingType);
	/* store the pointer to the building you wanna destroy */
	base->buildingCurrent = building;

	/** @todo: make base destroyable by destroying entrance */
	if (building->buildingType == B_ENTRANCE){
		MN_Popup(_("Destroy Entrance"), _("You cannot destroy the entrance of the base!"));
		return;
	}

	if (building->buildingStatus == B_STATUS_WORKING) {
		switch (building->buildingType) {
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (base->capacities[cap].cur >= base->capacities[cap].max) {
				MN_PopupButton(_("Destroy Hangar"), _("If you destroy this hangar, you will also destroy the aircraft inside.\nAre you sure you want to destroy this building?"),
					"mn_pop;mn_push aircraft;aircraft_select;", _("Go to hangar"), _("Go to hangar without destroying building"),
					"building_destroy;mn_pop;", _("Destroy"), _("Destroy the building"),
					(gd.numBases > 1) ? "mn_pop;mn_push transfer;" : NULL, (gd.numBases > 1) ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_QUARTERS:
			if (base->capacities[cap].cur + building->capacity > base->capacities[cap].max) {
				MN_PopupButton(_("Destroy Quarter"), _("If you destroy this Quarters, every employee inside will be killed.\nAre you sure you want to destroy this building?"),
					"mn_pop;mn_push employees;employee_list 0;", _("Dismiss"), _("Go to hiring menu without destroying building"),
					"building_destroy;mn_pop;", _("Destroy"), _("Destroy the building"),
					(gd.numBases > 1) ? "mn_pop;mn_push transfer;" : NULL, (gd.numBases > 1) ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_STORAGE:
			if (base->capacities[cap].cur + building->capacity > base->capacities[cap].max) {
				MN_PopupButton(_("Destroy Storage"), _("If you destroy this Storage, every items inside will be destroyed.\nAre you sure you want to destroy this building?"),
					"mn_pop;mn_push buy;buy_type *mn_itemtype", _("Go to storage"), _("Go to buy/sell menu without destroying building"),
					"building_destroy;mn_pop;", _("Destroy"), _("Destroy the building"),
					(gd.numBases > 1) ? "mn_pop;mn_push transfer;" : NULL, (gd.numBases > 1) ? _("Transfer") : NULL,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		default:
			break;
		}
	}

	MN_PopupButton(_("Destroy building"), _("Are you sure you want to destroy this building?"),
		NULL, NULL, NULL,
		"building_destroy;mn_pop;", _("Destroy"), _("Destroy the building"),
		NULL, NULL, NULL);
}

/**
 * @brief Displays the status of a building for baseview.
 * @note updates the cvar mn_building_status which is used in the building
 * construction menu to display the status of the given building
 * @note also script command function binding for 'building_status'
 */
void B_BuildingStatus (const base_t* base, const building_t* building)
{
	int numberOfBuildings = 0;

	assert(building);
	assert(base);

	Cvar_Set("mn_building_status", _("Not set"));

	switch (building->buildingStatus) {
	case B_STATUS_NOT_SET:
		numberOfBuildings = B_GetNumberOfBuildingsInBaseByTemplate(base, building->tpl);
		if (numberOfBuildings >= 0)
			Cvar_Set("mn_building_status", va(_("Already %i in base"), numberOfBuildings));
		break;
	case B_STATUS_UNDER_CONSTRUCTION:
		{
			/** @todo Was this planned to be used anywhere (e.g. for B_STATUS_UNDER_CONSTRUCTION text)
			 * or was it removed intentionally?
			 * const int daysLeft = building->timeStart + building->buildTime - ccs.date.day; */
			Cvar_Set("mn_building_status", "");
		}
		break;
	case B_STATUS_CONSTRUCTION_FINISHED:
		Cvar_Set("mn_building_status", _("Construction finished"));
		break;
	case B_STATUS_WORKING:
		Cvar_Set("mn_building_status", _("Working 100%"));
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
 * @param[in] base Pointer to base with given building.
 * @param[in] status Enum of buildingStatus_t which is status of given building.
 * @note This function checks whether a building has B_STATUS_WORKING status, and
 * then updates base status for particular buildings and base capacities.
 */
static void B_UpdateAllBaseBuildingStatus (building_t* building, base_t* base, buildingStatus_t status)
{
	qboolean test;
	buildingStatus_t oldstatus;

	assert(base);
	assert(building);

	oldstatus = building->buildingStatus;
	building->buildingStatus = status;

	/* we update the status of the building (we'll call this building building 1) */
	test = B_CheckUpdateBuilding(building, base);
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
	if (oldstatus == B_STATUS_UNDER_CONSTRUCTION && (status == B_STATUS_CONSTRUCTION_FINISHED || status == B_STATUS_WORKING)) {
		if (B_CheckBuildingDependencesStatus(base, building))
			CL_GameTimeStop();
	} else {
		CL_GameTimeStop();
	}
}

/**
 * @brief Build starting building in the first base, and hire employees.
 * @param[in,out] base The base to put the new building into
 * @param[in] template The building template to create a new building with
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] pos The position on the base grid
 */
static void B_AddBuildingToBasePos (base_t *base, const building_t const *template, qboolean hire, const vec2_t pos)
{
	building_t *buildingNew;		/**< new building in base (not a template) */

	/* fake a click to basemap */
	buildingNew = B_SetBuildingByClick(base, template, (int)pos[0], (int)pos[1]);
	B_UpdateAllBaseBuildingStatus(buildingNew, base, B_STATUS_WORKING);
	Com_DPrintf(DEBUG_CLIENT, "Base %i new building:%s at (%.0f:%.0f)\n", base->idx, buildingNew->id, buildingNew->pos[0], buildingNew->pos[1]);

	/* update the building-list */
	B_BuildingInit(base);

	if (hire)
		E_HireForBuilding(base, buildingNew, -1);

	/* now call the onconstruct trigger */
	if (buildingNew->onConstruct[0] != '\0') {
		Com_DPrintf(DEBUG_CLIENT, "B_SetUpBase: %s %i;\n", buildingNew->onConstruct, base->idx);
		Cmd_ExecuteString(va("%s %i;", buildingNew->onConstruct, base->idx));
	}
}

/**
 * @brief Build starting building in the first base, and hire employees (calls B_AddBuildingToBasePos).
 * @param[in,out] base The base to put the new building into
 * @param[in] template The building template to create a new building with
 * @param[in] hire Hire employees for the building we create from the template
 * @sa B_AddBuildingToBasePos
 * @sa B_SetUpFirstBase
 */
static inline void B_AddBuildingToBase (base_t *base, const building_t const *template, qboolean hire)
{
	B_AddBuildingToBasePos(base, template, hire, template->pos);
}

/**
 * @brief Prepares initial equipment for first base at the beginning of the campaign.
 * @param[in] base Pointer to first base.
 * @param[in] campaign The current running campaign
 * @sa B_BuildBase_f
 * @todo Make sure all equipment including soldiers equipment is added to capacity.cur.
 */
static void B_InitialEquipment (base_t *base, aircraft_t *assignInitialAircraft, const campaign_t* campaign, const char *eqname, equipDef_t *edTarget, const equipDef_t *ed)
{
	int i, price = 0;

	assert(base);
	assert(edTarget);

	/* Copy it to base storage. */
	if (ed)
		*edTarget = *ed;

	/* Initial soldiers and their equipment. */
	ed = INV_GetEquipmentDefinitionByID(eqname);
	if (ed == NULL) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildBase_f: Initial Phalanx equipment %s not found.\n", eqname);
	} else {
		if (assignInitialAircraft) {
			B_AssignInitial(assignInitialAircraft, ed);
		} else {
			for (i = 0; i < csi.numODs; i++)
				edTarget->num[i] += ed->num[i] / 5;
		}
	}

	/* Pay for the initial equipment as well as update storage capacity. */
	for (i = 0; i < csi.numODs; i++) {
		price += edTarget->num[i] * csi.ods[i].price;
		base->capacities[CAP_ITEMS].cur += edTarget->num[i] * csi.ods[i].size;
	}

	/* Finally update credits. */
	CL_UpdateCredits(ccs.credits - price);
}

/**
 * @brief Setup buildings and equipment for first base
 * @param[in,out] base The base to set up
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] buildings Add buildings to the initial base
 * @sa B_SetUpBase
 */
static void B_SetUpFirstBase (base_t* base, qboolean hire, qboolean buildings)
{
	assert(curCampaign);
	assert(curCampaign->firstBaseTemplate[0]);

	RS_MarkResearchable(qtrue, base);
	CP_InitMarket(qfalse);

	if (buildings) {
		int i;
		/* get template for base */
		const baseTemplate_t *template = B_GetBaseTemplate(curCampaign->firstBaseTemplate);
		const equipDef_t *ed = NULL;

		/* find each building in the template */
		for (i = 0; i < template->numBuildings; ++i) {
			vec2_t pos;
			Vector2Set(pos, template->buildings[i].posX, template->buildings[i].posY);
			B_AddBuildingToBasePos(base, template->buildings[i].building, hire, pos);
		}

		/* Add aircraft to the first base */
		/** @todo move aircraft to .ufo */
		/* buy two first aircraft and hire pilots for them. */
		if (B_GetBuildingStatus(base, B_HANGAR)) {
			aircraft_t *aircraft = AIR_GetAircraft("craft_drop_firebird");
			if (!aircraft)
				Sys_Error("Could not find craft_drop_firebird definition");
			AIR_NewAircraft(base, "craft_drop_firebird");
			CL_UpdateCredits(ccs.credits - aircraft->price);
			if (hire) {
				if (!E_HireEmployeeByType(base, EMPL_PILOT)) {
					Com_DPrintf(DEBUG_CLIENT, "B_SetUpFirstBase: Hiring pilot failed.\n");
				}
			}
		}
		if (B_GetBuildingStatus(base, B_SMALL_HANGAR)) {
			aircraft_t *aircraft = AIR_GetAircraft("craft_inter_stiletto");
			if (!aircraft)
				Sys_Error("Could not find craft_inter_stiletto definition");
			AIR_NewAircraft(base, "craft_inter_stiletto");
			CL_UpdateCredits(ccs.credits - aircraft->price);
			if (hire) {
				if (!E_HireEmployeeByType(base, EMPL_PILOT)) {
					Com_DPrintf(DEBUG_CLIENT, "B_SetUpFirstBase: Hiring pilot failed.\n");
				}
			}
		}

		/* Find the initial equipment definition for current campaign. */
		ed = INV_GetEquipmentDefinitionByID(curCampaign->equipment);

		/* initial base equipment */
		B_InitialEquipment(base, hire ? base->aircraftCurrent : NULL, curCampaign, cl_initial_equipment->string, &base->storage, ed);

		/* Auto equip interceptors with weapons and ammos */
		for (i = 0; i < base->numAircraftInBase; i++) {
			aircraft_t *aircraft = &base->aircraft[i];
			assert(aircraft);
			if (aircraft->type == AIRCRAFT_INTERCEPTOR)
				AIM_AutoEquipAircraft(aircraft);
		}

		/** @todo remove this - has nothing to do with base setup */
		CL_GameTimeFast();
		CL_GameTimeFast();
	} else {
		/* if no autobuild, set up zero build time for the first base */
		ccs.instant_build = 1;
	}
}

/**
 * @brief Setup new base
 * @param[in,out] base The base to set up
 * @param[in] hire Hire employees for the building we create from the template
 * @param[in] buildings Add buildings to the initial base
 * @sa B_NewBase
 * @sa B_SetUpFirstBase
 */
void B_SetUpBase (base_t* base, qboolean hire, qboolean buildings)
{
	int i;
	const int newBaseAlienInterest = 1.0f;

	assert(base);
	/* Reset current capacities. */
	for (i = 0; i < MAX_CAP; i++)
		base->capacities[i].cur = 0;

	/* update the building-list */
	B_BuildingInit(base);
	Com_DPrintf(DEBUG_CLIENT, "Set up for %i\n", base->idx);

	/* this cvar is used for disabling the base build button on geoscape
	 * if MAX_BASES was reached */
	Cvar_Set("mn_base_count", va("%i", gd.numBases));

	base->numAircraftInBase = 0;

	/* setup for first base */
	/** @todo this will propably also be called if all player bases are
	 * destroyed (mimics old behaviour), do we want this? */
	if (gd.numBases == 1) {
		B_SetUpFirstBase(base, hire, buildings);
	}

	/* add auto build buildings if it's not the first base */
	if (gd.numBases > 1 && buildings) {
		for (i = 0; i < gd.numBuildingTemplates; i++) {
			if (gd.buildingTemplates[i].autobuild) {
				B_AddBuildingToBase(base, &gd.buildingTemplates[i], hire);
			}
		}
	}

	if (!buildings) {
		/* we need to set up the entrance in case autobuild is off */
		for (i = 0; i < gd.numBuildingTemplates; ++i) {
			building_t* entrance = &gd.buildingTemplates[i];
			if (entrance->buildingType == B_ENTRANCE) {
				/* set up entrance to base */
				vec2_t pos;
				Vector2Set(pos, rand() % BASE_SIZE, rand() % BASE_SIZE);
				B_AddBuildingToBasePos(base, entrance, hire, pos);

				/* we are done here */
				i = gd.numBuildingTemplates;
			}
		}
	}

	/* Create random blocked fields in the base.
	 * The first base never has blocked fields so we skip it. */
	if (base->idx > 0) {
		const int j = round ((frand() * (MAX_BLOCKEDFIELDS - MIN_BLOCKEDFIELDS)) + MIN_BLOCKEDFIELDS);
		for (i = 0; i < j; i++) {
			/* Note that the rightmost column (containing the entrance) should never contain any blocked tiles, lest the entrance be boxed in. */
			baseBuildingTile_t *mapPtr = &base->map[rand() % BASE_SIZE][rand() % (BASE_SIZE - 1)];
			/* set this field to invalid if there is no building yet */
			if (!mapPtr->building)
				mapPtr->blocked = qtrue;
		}
	}

	if (B_GetNumberOfBuildingsInBaseByBuildingType(base, B_ENTRANCE)) {
		/* Set hasBuilding[B_ENTRANCE] to correct value, because it can't be updated afterwards.*/
		B_SetBuildingStatus(base, B_ENTRANCE, qtrue);
	} else {
		/* base can't start without an entrance, because this is where the aliens will arrive during base attack */
		/* autobuild and base templates should contain a base entrance */
		Sys_Error("B_SetUpBase: A new base should have an entrance.");
	}

	/* a new base is not discovered (yet) */
	base->alienInterest = newBaseAlienInterest;

	/* intialise hit points */
	base->batteryDamage = MAX_BATTERY_DAMAGE;
	base->baseDamage = MAX_BASE_DAMAGE;
	BDEF_InitialiseBaseSlots(base);

	/* Reset Radar range */
	RADAR_Initialise(&(base->radar), 0.0f, 0.0f, 1.0f, qtrue);
	RADAR_InitialiseUFOs(&base->radar);
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
	for (i = 0; i < gd.numBuildingTemplates; i++)
		if (!Q_strcasecmp(gd.buildingTemplates[i].id, buildingName))
			return &gd.buildingTemplates[i];

	Com_Printf("Building %s not found\n", buildingName);
	return NULL;
}

/**
 * @brief Returns the baseTemplate in the global baseTemplate list that has the unique name baseTemplateID.
 * @param[in] baseTemplateName The unique id of the building (baseTemplate_t->name).
 * @return baseTemplate_t If a Template was found it is returned, otherwise->NULL.
 */
const baseTemplate_t *B_GetBaseTemplate (const char *baseTemplateName)
{
	int i = 0;

	assert(baseTemplateName);
	for (i = 0; i < gd.numBaseTemplates; i++)
		if (!Q_strcasecmp(gd.baseTemplates[i].name, baseTemplateName))
			return &gd.baseTemplates[i];

	Com_Printf("Base Template %s not found\n", baseTemplateName);
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
 * @param[in,out] secondBuildingPart The second building part in case of e.g. big hangars
 */
static qboolean B_ConstructBuilding (base_t* base, building_t *building, building_t *secondBuildingPart)
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

	/* second building part */
	if (secondBuildingPart)
		secondBuildingPart->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;

	if (!ccs.instant_build) {
		building->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		building->timeStart = ccs.date.day;
	} else {
		/* call the onconstruct trigger */
		if (building->onConstruct[0] != '\0') {
			Com_DPrintf(DEBUG_CLIENT, "B_SetUpBase: %s %i;\n", building->onConstruct, base->idx);
			Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
		}
		B_UpdateAllBaseBuildingStatus(building, base, B_STATUS_WORKING);
	}

	CL_UpdateCredits(ccs.credits - building->fixCosts);
	B_BaseMenuInit(base);
	return qtrue;
}

/**
 * @brief Build new building.
 * @param[in,out] base The base to construct the building in
 * @param[in,out] building The building to construct
 * @param[in,out] secondBuildingPart The second building part in case of e.g. big hangars
 * @sa B_MarkBuildingDestroy
 * @sa B_ConstructBuilding
 */
static void B_NewBuilding (base_t* base, building_t *building, building_t *secondBuildingPart)
{
	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !building)
		return;

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		/* credits are updated in the construct function */
		if (B_ConstructBuilding(base, building, secondBuildingPart)) {
			B_BuildingStatus(base, building);
			Com_DPrintf(DEBUG_CLIENT, "B_NewBuilding: building->buildingStatus = %i\n", building->buildingStatus);
		}
}

/**
 * @brief Set the currently selected building.
 * @param[in,out] base The base to place the building in
 * @param[in] template The template of the building to place at the given location
 * @param[in] row Set building to row
 * @param[in] col Set building to col
 * @return building created in base (this is not a building template)
 */
building_t* B_SetBuildingByClick (base_t *base, const building_t const *template, int row, int col)
{
#ifdef DEBUG
	if (!base)
		Sys_Error("no current base\n");
	if (!template)
		Sys_Error("no current building\n");
#endif
	if (!B_CheckCredits(template->fixCosts)) {
		MN_Popup(_("Notice"), _("Not enough credits to build this\n"));
		return NULL;
	}

	/* template should really be a template */
	assert(template == template->tpl);

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		/* new building in base (not a template) */
		building_t *buildingNew = &gd.buildings[base->idx][gd.numBuildings[base->idx]];

		/* copy building from template list to base-buildings-list */
		*buildingNew = *template;

		/* self-link to building-list in base */
		buildingNew->idx = gd.numBuildings[base->idx];
		gd.numBuildings[base->idx]++;

		/* Link to the base. */
		buildingNew->base = base;

		if (base->map[row][col].blocked) {
			Com_DPrintf(DEBUG_CLIENT, "This base field is marked as invalid - you can't build here\n");
		} else if (!base->map[row][col].building) {
			building_t *secondBuildingPart = NULL;
			/* No building in this place */
			if (template->needs) {
				secondBuildingPart = B_GetBuildingTemplate(template->needs);	/* template link */

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
				/* where is this building located in our base? */
				secondBuildingPart->pos[1] = col + 1;
				secondBuildingPart->pos[0] = row;
			}
			/* Credits are updated here, too */
			B_NewBuilding(base, buildingNew, secondBuildingPart);

			base->map[row][col].building = buildingNew;

			/* where is this building located in our base? */
			buildingNew->pos[0] = row;
			buildingNew->pos[1] = col;

			B_ResetBuildingCurrent(base);
			B_BuildingInit(base);	/* update the building-list */

			return buildingNew;
		} else {
			Com_DPrintf(DEBUG_CLIENT, "There is already a building\n");
			Com_DPrintf(DEBUG_CLIENT, "Building: %s at (row:%i, col:%i)\n", base->map[row][col].building->id, row, col);
		}
	} else
		Com_DPrintf(DEBUG_CLIENT, "Invalid coordinates\n");

	return NULL;
}

#define MAX_BUILDING_INFO_TEXT_LENGTH 512

/**
 * @brief Draws a building.
 */
void B_DrawBuilding (base_t* base, building_t* building)
{
	static char buildingText[MAX_BUILDING_INFO_TEXT_LENGTH];

	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !building)
		return;

	*buildingText = '\0';

	B_BuildingStatus(base, building);

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
	MN_RegisterText(TEXT_BUILDING_INFO, buildingText);
}

/**
 * @brief Handles the list of constructable buildings.
 * @param[in] base The base to update the building list for
 * @param[in] building Add this building to the construction list
 * @note Called everytime a building was constructed and thus maybe other buildings
 * get available. The content is updated everytime B_BuildingInit is called
 * (i.e everytime the buildings-list is displayed/updated)
 */
static void B_BuildingAddToList (base_t *base, building_t *building)
{
	int count;
	assert(base);
	assert(building);
	assert(building->name);

	count = LIST_Count(base->buildingList);
	LIST_AddPointer(&base->buildingList, _(building->name));
	buildingConstructionList[count] = building->tpl;
}


/**
 * @brief Counts the number of buildings of a particular type in a base.
 *
 * @param[in] base Which base to count in.
 * @param[in] tpl The template type in the gd.buildingTemplates list.
 * @return The number of buildings.
 * @return -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByTemplate (const base_t *base, const building_t *tpl)
{
	int i;
	int NumberOfBuildings = 0;

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
		Com_Printf("B_GetNumberOfBuildingsInBaseByTemplate: No building-type given as paramter. It's probably a normal building!\n");
		return -1;
	}

	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		if (gd.buildings[base->idx][i].tpl == tpl
		 && gd.buildings[base->idx][i].buildingStatus != B_STATUS_NOT_SET)
			NumberOfBuildings++;
	}
	return NumberOfBuildings;
}

/**
 * @brief Counts the number of buildings of a particular buuilding type in a base.
 *
 * @param[in] base Which base to count in.
 * @param[in] buildingType Building type value.
 * @return The number of buildings.
 * @return -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByBuildingType (const base_t *base, const buildingType_t buildingType)
{
	int i;
	int NumberOfBuildings = 0;

	if (!base) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: No base given!\n");
		return -1;
	}

	if (buildingType >= MAX_BUILDING_TYPE || buildingType < 0) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByBuildingType: no sane building-type given!\n");
		return -1;
	}

	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		if (gd.buildings[base->idx][i].buildingType == buildingType
		 && gd.buildings[base->idx][i].buildingStatus != B_STATUS_NOT_SET)
			NumberOfBuildings++;
	}
	return NumberOfBuildings;
}

/**
 * @brief Update the building-list.
 * @sa B_BuildingInit_f
 */
void B_BuildingInit (base_t* base)
{
	int i;

	/* maybe someone call this command before the bases are parsed?? */
	if (!base)
		return;

	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Updating b-list for '%s' (%i)\n", base->name, base->idx);
	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Buildings in base: %i\n", gd.numBuildings[base->idx]);

	/* delete the whole linked list - it's rebuild in the loop below */
	LIST_Delete(&base->buildingList);

	for (i = 0; i < gd.numBuildingTemplates; i++) {
		building_t *tpl = &gd.buildingTemplates[i];
		/* make an entry in list for this building */

		if (tpl->visible) {
			const int numSameBuildings = B_GetNumberOfBuildingsInBaseByTemplate(base, tpl);

			if (tpl->moreThanOne) {
				/* skip if limit of BASE_SIZE*BASE_SIZE exceeded */
				if (numSameBuildings >= BASE_SIZE * BASE_SIZE)
					continue;
			} else if (numSameBuildings > 0) {
				continue;
			}

			/* if the building is researched add it to the list */
			if (RS_IsResearched_ptr(tpl->tech)) {
				B_BuildingAddToList(base, tpl);
			} else {
				Com_DPrintf(DEBUG_CLIENT, "Building not researched yet %s (tech idx: %i)\n",
					tpl->id, tpl->tech ? tpl->tech->idx : 0);
			}
		}
	}
	if (base->buildingCurrent)
		B_DrawBuilding(base, base->buildingCurrent);

	MN_RegisterLinkedListText(TEXT_BUILDINGS, base->buildingList);
}

/**
 * @brief Returns the building type for a given building identified by its building id
 * from the ufo script files
 * @sa B_ParseBuildings
 * @sa B_GetBuildingType
 * @note Do not use B_GetBuildingType here, this is also used for parsing the types!
 */
buildingType_t B_GetBuildingTypeByBuildingID (const char *buildingID)
{
	if (!Q_strncmp(buildingID, "lab", MAX_VAR)) {
		return B_LAB;
	} else if (!Q_strncmp(buildingID, "hospital", MAX_VAR)) {
		return B_HOSPITAL;
	} else if (!Q_strncmp(buildingID, "aliencont", MAX_VAR)) {
		return B_ALIEN_CONTAINMENT;
	} else if (!Q_strncmp(buildingID, "workshop", MAX_VAR)) {
		return B_WORKSHOP;
	} else if (!Q_strncmp(buildingID, "storage", MAX_VAR)) {
		return B_STORAGE;
	} else if (!Q_strncmp(buildingID, "hangar", MAX_VAR)) {
		return B_HANGAR;
	} else if (!Q_strncmp(buildingID, "smallhangar", MAX_VAR)) {
		return B_SMALL_HANGAR;
	} else if (!Q_strncmp(buildingID, "ufohangar", MAX_VAR)) {
		return B_UFO_HANGAR;
	} else if (!Q_strncmp(buildingID, "smallufohangar", MAX_VAR)) {
		return B_UFO_SMALL_HANGAR;
	} else if (!Q_strncmp(buildingID, "quarters", MAX_VAR)) {
		return B_QUARTERS;
	} else if (!Q_strncmp(buildingID, "workshop", MAX_VAR)) {
		return B_WORKSHOP;
	} else if (!Q_strncmp(buildingID, "power", MAX_VAR)) {
		return B_POWER;
	} else if (!Q_strncmp(buildingID, "command", MAX_VAR)) {
		return B_COMMAND;
	} else if (!Q_strncmp(buildingID, "amstorage", MAX_VAR)) {
		return B_ANTIMATTER;
	} else if (!Q_strncmp(buildingID, "entrance", MAX_VAR)) {
		return B_ENTRANCE;
	} else if (!Q_strncmp(buildingID, "missile", MAX_VAR)) {
		return B_DEFENCE_MISSILE;
	} else if (!Q_strncmp(buildingID, "radar", MAX_VAR)) {
		return B_RADAR;
	} else if (!Q_strncmp(buildingID, "teamroom", MAX_VAR)) {
		return B_TEAMROOM;
	}
	return MAX_BUILDING_TYPE;
}

/**
 * @brief Copies an entry from the building description file into the list of building types.
 * @note Parses one "building" entry in the basemanagement.ufo file and writes
 * it into the next free entry in bmBuildings[0], which is the list of buildings
 * in the first base (building_t).
 * @param[in] name Unique test-id of a building_t. This is parsed from "building xxx" -> id=xxx.
 * @param[in] text @todo document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
 * @param[in] link Bool value that decides whether to link the tech pointer in or not
 * @sa CL_ParseScriptFirst (link is false here)
 * @sa CL_ParseScriptSecond (link it true here)
 */
void B_ParseBuildings (const char *name, const char **text, qboolean link)
{
	building_t *building;
	building_t *dependsBuilding;
	technology_t *tech_link;
	const value_t *vp;
	const char *errhead = "B_ParseBuildings: unexpected end of file (names ";
	const char *token;
	int i;

	/* get id list body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("B_ParseBuildings: building \"%s\" without body ignored\n", name);
		return;
	}
	if (gd.numBuildingTemplates >= MAX_BUILDINGS) {
		Com_Printf("B_ParseBuildings: too many buildings\n");
		gd.numBuildingTemplates = MAX_BUILDINGS;	/* just in case it's bigger. */
		return;
	}

	if (!link) {
		for (i = 0; i < gd.numBuildingTemplates; i++) {
			if (!Q_strcmp(gd.buildingTemplates[i].id, name)) {
				Com_Printf("B_ParseBuildings: Second building with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* new entry */
		building = &gd.buildingTemplates[gd.numBuildingTemplates];
		memset(building, 0, sizeof(*building));
		building->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

		Com_DPrintf(DEBUG_CLIENT, "...found building %s\n", building->id);

		/*set standard values */
		building->tpl = building;	/* Self-link just in case ... this way we can check if it is a template or not. */
		building->idx = -1;			/* No entry in buildings list (yet). */
		building->base = NULL;
		building->buildingType = MAX_BUILDING_TYPE;
		building->dependsBuilding = NULL;
		building->visible = qtrue;

		gd.numBuildingTemplates++;
		do {
			/* get the name type */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;

			/* get values */
			if (!Q_strncmp(token, "type", MAX_VAR)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				building->buildingType = B_GetBuildingTypeByBuildingID(token);
				if (building->buildingType >= MAX_BUILDING_TYPE)
					Com_Printf("didn't find buildingType '%s'\n", token);
			} else {
				/* no linking yet */
				if (!Q_strncmp(token, "depends", MAX_VAR)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
				} else {
					for (vp = valid_building_vars; vp->string; vp++)
						if (!Q_strncmp(token, vp->string, sizeof(vp->string))) {
							/* found a definition */
							token = COM_EParse(text, errhead, name);
							if (!*text)
								return;

							switch (vp->type) {
							case V_NULL:
								break;
							case V_TRANSLATION_STRING:
								token++;
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)building + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
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
		if (!building)			/* i'm paranoid */
			Sys_Error("B_ParseBuildings: Could not find building with id %s\n", name);

		tech_link = RS_GetTechByProvided(name);
		if (tech_link) {
			building->tech = tech_link;
		} else {
			if (building->visible)
				/** @todo are the techs already parsed? */
				Com_DPrintf(DEBUG_CLIENT, "B_ParseBuildings: Could not find tech that provides %s\n", name);
		}

		do {
			/* get the name type */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;
			/* get values */
			if (!Q_strncmp(token, "depends", MAX_VAR)) {
				dependsBuilding = B_GetBuildingTemplate(COM_EParse(text, errhead, name));
				if (!dependsBuilding)
					Sys_Error("Could not find building depend of %s\n", building->id);
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
 * @return The building or NULL if base has no building of that type
 */
building_t *B_GetBuildingInBaseByType (const base_t* base, buildingType_t buildingType, qboolean onlyWorking)
{
	int i;
	building_t *building;

	/* we maybe only want to get the working building (e.g. it might the
	 * case that we don't have a powerplant and thus the searched building
	 * is not functional) */
	if (onlyWorking && !B_GetBuildingStatus(base, buildingType))
		return NULL;

	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		building = &gd.buildings[base->idx][i];
		if (building->buildingType == buildingType)
			return building;
	}
	return NULL;
}


/**
 * @brief Hack to get a random nation for the initial
 */
static inline nation_t *B_RandomNation (void)
{
	int nationIndex = rand() % gd.numNations;
	return &gd.nations[nationIndex];
}

/**
 * @brief Remove all character_t information (and linked to that employees & team info) from the game.
 * @param[in] base The base to remove all this stuff from.
 * @sa CL_GenerateCharacter
 * @sa AIR_ResetAircraftTeam
 */
static void B_ResetHiredEmployeesInBase (base_t* const base)
{
	int i;
	linkedList_t *hiredEmployees = NULL;
	linkedList_t *hiredEmployeesTemp;

	/* Reset inventory data of all hired employees that can be sent into combat (i.e. characters with inventories).
	 * i.e. these are soldiers and robots right now. */
	E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredEmployees);
	hiredEmployeesTemp = hiredEmployees;
	while (hiredEmployeesTemp) {
		employee_t *employee = (employee_t*)hiredEmployeesTemp->data;
		if (employee)
			INVSH_DestroyInventory(&employee->chr.inv);
		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	E_GetHiredEmployees(base, EMPL_ROBOT, &hiredEmployees);
	hiredEmployeesTemp = hiredEmployees;
	while (hiredEmployeesTemp) {
		employee_t *employee = (employee_t*)hiredEmployeesTemp->data;
		if (employee)
			INVSH_DestroyInventory(&employee->chr.inv);
		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	LIST_Delete(&hiredEmployees);

	/* Reset hire info. */
	Cvar_ForceSet("cl_selected", "0");

	/* Fire 'em all (in multiplayer they are not hired) */
	for (i = 0; i < MAX_EMPL; i++) {
		E_UnhireAllEmployees(base, i);
	}

	/* Purge all team-info from the aircraft */
	for (i = 0; i < base->numAircraftInBase; i++) {
		AIR_ResetAircraftTeam(B_GetAircraftFromBaseByIndex(base, i));
	}
}

/**
 * @brief Clears a base with all its characters
 * @sa B_ResetHiredEmployeesInBase
 * @sa CL_GenerateCharacter
 */
void B_ClearBase (base_t *const base)
{
	int i;
	int j = 0;

	B_ResetHiredEmployeesInBase(base);

	memset(base, 0, sizeof(*base));

	/* only go further if we have a active campaign */
	if (!GAME_CP_IsRunning())
		return;

	/* setup team */
	/** @todo I think this should be made only once per game, not once per base, no ? -- Kracken 19/12/07 */
	if (!E_CountUnhired(EMPL_SOLDIER)) {
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i soldiers\n", curCampaign->soldiers);
		for (i = 0; i < curCampaign->soldiers; i++)
			E_CreateEmployee(EMPL_SOLDIER, B_RandomNation(), NULL);
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i scientists\n", curCampaign->scientists);
		for (i = 0; i < curCampaign->scientists; i++)
			E_CreateEmployee(EMPL_SCIENTIST, B_RandomNation(), NULL);
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i robots\n", curCampaign->ugvs);
		for (i = 0; i < curCampaign->ugvs; i++) {
			if (frand() > 0.5)
				E_CreateEmployee(EMPL_ROBOT, B_RandomNation(), CL_GetUgvByID("ugv_ares_w"));
			else
				E_CreateEmployee(EMPL_ROBOT, B_RandomNation(), CL_GetUgvByID("ugv_phoenix"));
		}
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i workers\n", curCampaign->workers);
		for (i = 0; i < curCampaign->workers; i++)
			E_CreateEmployee(EMPL_WORKER, B_RandomNation(), NULL);

		/* Fill the global data employee list with pilots, evenly distributed between nations */
		for (i = 0; i < MAX_EMPLOYEES; i++) {
			nation_t *nation = &gd.nations[++j % gd.numNations];
			if (!E_CreateEmployee(EMPL_PILOT, nation, NULL))
				break;
		}
	}

	memset(base->map, 0, sizeof(base->map));
}

/**
 * @brief Reads information about bases.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void B_ParseBaseNames (const char *name, const char **text)
{
	const char *errhead = "B_ParseBaseNames: unexpected end of file (names ";
	const char *token;
	base_t *base;

	gd.numBaseNames = 0;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBaseNames: base \"%s\" without body ignored\n", name);
		return;
	}
	do {
		/* add base */
		if (gd.numBaseNames > MAX_BASES) {
			Com_Printf("B_ParseBaseNames: too many bases\n");
			return;
		}

		/* get the name */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		base = B_GetBaseByIDX(gd.numBaseNames);
		memset(base, 0, sizeof(*base));
		base->idx = gd.numBaseNames;
		memset(base->map, 0, sizeof(base->map));

		/* get the title */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token == '_')
			token++;
		Q_strncpyz(base->name, _(token), sizeof(base->name));
		Com_DPrintf(DEBUG_CLIENT, "Found base %s\n", base->name);
		B_ResetBuildingCurrent(base);
		gd.numBaseNames++; /** @todo Use this value instead of MAX_BASES in the for loops */
	} while (*text);

	Cvar_Set("mn_base_title", "");
}

/**
 * @brief Reads a base layout template
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void B_ParseBaseTemplate (const char *name, const char **text)
{
	const char *errhead = "B_ParseBaseTemplate: unexpected end of file (names ";
	const char *token;
	qboolean hasEntrance = qfalse;
	baseTemplate_t* template;
	baseBuildingTile_t* tile;
	vec2_t pos;
	qboolean map[BASE_SIZE][BASE_SIZE];
	byte buildingnums[MAX_BUILDINGS];
	int i;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBaseTemplate: Template \"%s\" without body ignored\n", name);
		return;
	}

	if (gd.numBaseTemplates >= MAX_BASETEMPLATES) {
		Com_Printf("B_ParseBaseTemplate: too many base templates\n");
		gd.numBuildingTemplates = MAX_BASETEMPLATES;	/* just in case it's bigger. */
		return;
	}

	/* create new Template */
	template = &gd.baseTemplates[gd.numBaseTemplates];
	template->name = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	/* clear map for checking duplicate positions and buildingnums for checking moreThanOne constraint */
	memset(&map, qfalse, sizeof(map));
	memset(&buildingnums, 0, sizeof(buildingnums));

	gd.numBaseTemplates++;

	Com_DPrintf(DEBUG_CLIENT, "Found Base Template %s\n", name);
	do {
		/* get the building */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (template->numBuildings >= MAX_BASEBUILDINGS) {
			Com_Printf("B_ParseBaseTemplate: too many buildings\n");
			gd.numBuildingTemplates = MAX_BASEBUILDINGS;	/* just in case it's bigger. */
			return;
		}

		/* check if building type is known */
		tile = &template->buildings[template->numBuildings];
		template->numBuildings++;

		for (i = 0; i < gd.numBuildingTemplates; i++)
			if (!Q_strcasecmp(gd.buildingTemplates[i].id, token)) {
				tile->building = &gd.buildingTemplates[i];
				if (!tile->building->moreThanOne && buildingnums[i]++ > 0)
					Sys_Error("B_ParseBaseTemplate: Found more %s than allowed in template %s\n", token, name);
			}

		if (!tile->building)
			Sys_Error("B_ParseBaseTemplate: Could not find building with id %s\n", name);

		if (tile->building->buildingType == B_ENTRANCE)
			hasEntrance = qtrue;

		Com_DPrintf(DEBUG_CLIENT, "...found Building %s ", token);

		/* get the position */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		Com_DPrintf(DEBUG_CLIENT, "on position %s\n", token);

		Com_EParseValue(pos, token, V_POS, 0, sizeof(vec2_t));
		tile->posX = pos[0];
		tile->posY = pos[1];

		/* check for buildings on same position */
		assert(!map[tile->posX][tile->posY]);
		map[tile->posX][tile->posY] = qtrue;
	} while (*text);

	/* templates without Entrance can't be used */
	if (!hasEntrance)
		Sys_Error("Every base template needs one entrace! '%s' has none.", template->name);
}

/**
 * @brief Get the lower IDX of unfounded base.
 * @return baseIdx of first Base Unfounded, or MAX_BASES is maximum base number is reached.
 */
static int B_GetFirstUnfoundedBase (void)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			return baseIdx;
	}

	return MAX_BASES;
}

/**
 * @param[in] base If this is @c NULL we want to build a new base
 */
void B_SelectBase (base_t *base)
{
	/* set up a new base */
	if (!base) {
		int baseID;

		/* if player hit the "create base" button while creating base mode is enabled
		 * that means that player wants to quit this mode */
		if (gd.mapAction == MA_NEWBASE) {
			MAP_ResetAction();
			if (!radarOverlayWasSet)
				MAP_DeactivateOverlay("radar");
			return;
		}

		gd.mapAction = MA_NEWBASE;
		baseID = B_GetFirstUnfoundedBase();
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: new baseID is %i\n", baseID);
		if (baseID < MAX_BASES) {
			installationCurrent = NULL;
			baseCurrent = B_GetBaseByIDX(baseID);
			baseCurrent->idx = baseID;
			Cvar_Set("mn_base_newbasecost", va(_("%i c"), curCampaign->basecost));
			Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: baseID is valid for base: %s\n", baseCurrent->name);
			MN_ExecuteConfunc("set_base_to_normal");
			/* show radar overlay (if not already displayed) */
			if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
				MAP_SetOverlay("radar");
		} else {
			Com_Printf("MaxBases reached\n");
			/* select the first base in list */
			installationCurrent = NULL;
			baseCurrent = B_GetBaseByIDX(0);
			gd.mapAction = MA_NONE;
		}
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: select base with id %i\n", base->idx);
		installationCurrent = NULL;
		baseCurrent = base;
		gd.mapAction = MA_NONE;
		MN_PushMenu("bases", NULL);
		AIR_AircraftSelect(base->aircraftCurrent);
		switch (base->baseStatus) {
		case BASE_UNDER_ATTACK:
			Cvar_Set("mn_base_status_name", _("Base is under attack"));
			Cmd_ExecuteString("set_base_under_attack");
			break;
		default:
			Cmd_ExecuteString("set_base_to_normal");
			break;
		}
	}

	/**
	 * this is only needed when we are going to be show up the base
	 * in our base view port
	 */
	if (gd.mapAction != MA_NEWBASE) {
		assert(baseCurrent);
		Cvar_Set("mn_base_title", baseCurrent->name);
		Cvar_SetValue("mn_numbases", gd.numBases);
		Cvar_SetValue("mn_base_status_id", baseCurrent->baseStatus);
	}
}

#undef RIGHT
#undef HOLSTER
#define RIGHT(e) ((e)->inv.c[csi.idRight])
#define HOLSTER(e) ((e)->inv.c[csi.idHolster])

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

/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 * @todo This currently always uses exactly the first two firemodes (see fmode1+fmode2) for calculation. This needs to be adapted to support less (1) or more 3+ firemodes. I think the function will even  break on only one firemode .. never tested it.
 * @todo i think currently also the different ammo/firedef types for each weapon (different weaponr_fd_idx and weaponr_fd_idx values) are ignored.
 */
static void CL_SwapSkills (chrList_t *team)
{
	int j, i1, i2, skill;
	const byte fmode1 = 0;
	const byte fmode2 = 1;

	j = team->num;
	while (j--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (i1 = 0; i1 < team->num - 1; i1++) {
				character_t *cp1 = team->chr[i1];
				int weaponr_fd_idx = -1;
				int weaponh_fd_idx = -1;
				if (RIGHT(cp1) && RIGHT(cp1)->item.m && RIGHT(cp1)->item.t)
					weaponr_fd_idx = FIRESH_FiredefsIDXForWeapon(RIGHT(cp1)->item.m, RIGHT(cp1)->item.t);
				if (HOLSTER(cp1) && HOLSTER(cp1)->item.m && HOLSTER(cp1)->item.t)
					weaponh_fd_idx = FIRESH_FiredefsIDXForWeapon(HOLSTER(cp1)->item.m, HOLSTER(cp1)->item.t);
				/* disregard left hand, or dual-wielding guys are too good */

				if (weaponr_fd_idx < 0 || weaponh_fd_idx < 0) {
					/** @todo Is there a better way to check for this case? */
					Com_DPrintf(DEBUG_CLIENT, "CL_SwapSkills: Bad or no firedef indices found (weaponr_fd_idx=%i and weaponh_fd_idx=%i)... skipping\n", weaponr_fd_idx, weaponh_fd_idx);
				} else {
					const int no1 = 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[weaponr_fd_idx][fmode1].weaponSkill)
						+ 2 * (RIGHT(cp1) && skill == RIGHT(cp1)->item.m->fd[weaponr_fd_idx][fmode2].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[weaponh_fd_idx][fmode1].weaponSkill)
						+ (HOLSTER(cp1) && HOLSTER(cp1)->item.t->reload
						   && skill == HOLSTER(cp1)->item.m->fd[weaponh_fd_idx][fmode2].weaponSkill);

					for (i2 = i1 + 1; i2 < team->num; i2++) {
						character_t *cp2 = team->chr[i2];
						weaponr_fd_idx = -1;
						weaponh_fd_idx = -1;

						if (RIGHT(cp2) && RIGHT(cp2)->item.m && RIGHT(cp2)->item.t)
							weaponr_fd_idx = FIRESH_FiredefsIDXForWeapon(RIGHT(cp2)->item.m, RIGHT(cp2)->item.t);
						if (HOLSTER(cp2) && HOLSTER(cp2)->item.m && HOLSTER(cp2)->item.t)
							weaponh_fd_idx = FIRESH_FiredefsIDXForWeapon(HOLSTER(cp2)->item.m, HOLSTER(cp2)->item.t);

						if (weaponr_fd_idx < 0 || weaponh_fd_idx < 0) {
							/** @todo Is there a better way to check for this case? */
							Com_DPrintf(DEBUG_CLIENT, "CL_SwapSkills: Bad or no firedef indices found (weaponr_fd_idx=%i and weaponh_fd_idx=%i)... skipping\n", weaponr_fd_idx, weaponh_fd_idx);
						} else {
							const int no2 = 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[weaponr_fd_idx][fmode1].weaponSkill)
								+ 2 * (RIGHT(cp2) && skill == RIGHT(cp2)->item.m->fd[weaponr_fd_idx][fmode2].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[weaponh_fd_idx][fmode1].weaponSkill)
								+ (HOLSTER(cp2) && HOLSTER(cp2)->item.t->reload
								   && skill == HOLSTER(cp2)->item.m->fd[weaponh_fd_idx][fmode2].weaponSkill);

							if (no1 > no2 /* more use of this skill */
								 || (no1 && no1 == no2)) { /* or earlier on list */

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
									Sys_Error("CL_SwapSkills: illegal skill %i.\n", skill);
								}
							} else if (no1 < no2) {
								if (cp2->score.skills[skill] < cp1->score.skills[skill])
										CL_SwapSkill(cp1, cp2, skill);

								switch (skill) {
								case SKILL_CLOSE:
									if (cp2->score.skills[ABILITY_SPEED] < cp1->score.skills[ABILITY_SPEED])
										CL_SwapSkill(cp1, cp2, ABILITY_SPEED);
									break;
								case SKILL_HEAVY:
									if (cp2->score.skills[ABILITY_POWER] < cp1->score.skills[ABILITY_POWER])
										CL_SwapSkill(cp1, cp2, ABILITY_POWER);
									break;
								case SKILL_ASSAULT:
									break;
								case SKILL_SNIPER:
									if (cp2->score.skills[ABILITY_ACCURACY] < cp1->score.skills[ABILITY_ACCURACY])
										CL_SwapSkill(cp1, cp2, ABILITY_ACCURACY);
									break;
								case SKILL_EXPLOSIVE:
									if (cp2->score.skills[ABILITY_MIND] < cp1->score.skills[ABILITY_MIND])
										CL_SwapSkill(cp1, cp2, ABILITY_MIND);
									break;
								default:
									Sys_Error("CL_SwapSkills: illegal skill %i.\n", skill);
								}
							}
						} /* if xx_fd_xx < 0 */
					} /* for */
				} /* if xx_fd_xx < 0 */
			}
		}
	}
}

/**
 * @brief Assigns initial soldier equipment for the first base
 * @sa B_AssignInitial
 * @todo Move this function to a better place - has nothing to do with bases anymore
 */
static void B_PackInitialEquipment (aircraft_t *aircraft, const equipDef_t *ed)
{
	int i;
	base_t *base = aircraft->homebase;
	int container, price = 0;
	chrList_t chrListTemp;

	if (!aircraft)
		return;

	chrListTemp.num = 0;
	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i]) {
			character_t *chr = &aircraft->acTeam[i]->chr;
			/* pack equipment */
			Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment: Packing initial equipment for %s.\n", chr->name);
			/** @todo maybe give equipDef_t directly? */
			INVSH_EquipActor(&chr->inv, ed->num, MAX_OBJDEFS, ed->name, chr);
			Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment: armour: %i, weapons: %i\n", chr->armour, chr->weapons);
			chrListTemp.chr[chrListTemp.num] = chr;
			chrListTemp.num++;
		}
	}

	if (base) {
		AIR_MoveEmployeeInventoryIntoStorage(aircraft, &base->storage);
		B_UpdateStorageCap(base);
	}
	CL_SwapSkills(&chrListTemp);

	/* pay for the initial equipment */
	for (container = 0; container < csi.numIDs; container++) {
		int p;
		for (p = 0; p < aircraft->maxTeamSize; p++) {
			if (aircraft->acTeam[p]) {
				const invList_t *ic;
				const character_t *chr = &aircraft->acTeam[p]->chr;
				for (ic = chr->inv.c[container]; ic; ic = ic->next) {
					const item_t item = ic->item;
					price += item.t->price;
					Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment: adding price for %s, price: %i\n", item.t->id, price);
				}
			}
		}
	}
	CL_UpdateCredits(ccs.credits - price);
}

/**
 * @brief Assigns initial team of soldiers with equipment to aircraft
 * @sa B_PackInitialEquipment
 */
static void B_AssignInitial (aircraft_t *aircraft, const equipDef_t *ed)
{
	int i, num;
	base_t *base;

	if (!aircraft) {
		Com_Printf("B_AssignInitial: No aircraft given\n");
		return;
	}

	base = aircraft->homebase;
	/* homebase is only set in campaign mode */
	num = E_GenerateHiredEmployeesList(base);
	num = min(num, MAX_TEAMLIST);
	for (i = 0; i < num; i++)
		AIM_AddEmployeeFromMenu(aircraft, i);

	B_PackInitialEquipment(aircraft, ed);
}

/**
 * @brief Sets the baseStatus to BASE_NOT_USED
 * @param[in] base Which base should be resetted?
 * @sa CL_CampaignRemoveMission
 */
void B_BaseResetStatus (base_t* const base)
{
	assert(base);
	base->baseStatus = BASE_NOT_USED;
	if (gd.mapAction == MA_BASEATTACK)
		gd.mapAction = MA_NONE;
}

/**
 * @brief Cleans all bases and related structures to prepare a new campaign mode game
 * @note restores the original base names
 * @sa CL_GameNew
 */
void B_NewBases (void)
{
	int i;
	char title[MAX_VAR];

	/* base setup */
	gd.numBases = 0;
	Cvar_Set("mn_base_count", "0");

	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetBaseByIDX(i);
		Q_strncpyz(title, base->name, sizeof(title));
		B_ClearBase(base);
		Q_strncpyz(base->name, title, sizeof(title));
	}
}

#ifdef DEBUG
/**
 * @brief Just lists all buildings with their data
 * @note called with debug_listbuilding
 * @note Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void B_BuildingList_f (void)
{
	int baseIdx, j, k;
	building_t *building;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent) {
		Com_Printf("No base selected\n");
		return;
	}

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		building = &gd.buildings[base->idx][baseIdx];
		Com_Printf("\nBase id %i: %s\n", baseIdx, base->name);
		for (j = 0; j < gd.numBuildings[baseIdx]; j++) {
			Com_Printf("...Building: %s #%i - id: %i\n", building->id,
				B_GetNumberOfBuildingsInBaseByTemplate(baseCurrent, building->tpl), baseIdx);
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
			building++;
		}
	}
}

/**
 * @brief Just lists all bases with their data
 * @note called with debug_listbase
 * @note Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void B_BaseList_f (void)
{
	int i, row, col, j;
	base_t *base;

	for (i = 0, base = gd.bases; i < MAX_BASES; i++, base++) {
		if (!base->founded) {
			Com_Printf("Base idx %i not founded\n\n", i);
			continue;
		}

		Com_Printf("Base idx %i\n", base->idx);
		Com_Printf("Base name %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base numAircraftInBase %i\n", base->numAircraftInBase);
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
		Com_Printf("\nBase aircraft %i\n", base->numAircraftInBase);
		for (j = 0; j < base->numAircraftInBase; j++) {
			Com_Printf("Base aircraft-team %i\n", base->aircraft[j].teamSize);
		}
		Com_Printf("Base pos %.02f:%.02f\n", base->pos[0], base->pos[1]);
		Com_Printf("Base map:\n");
		for (row = 0; row < BASE_SIZE; row++) {
			if (row)
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
 * @param[in] base The current active base we are viewing right now
 * @param[in] building The building we have clicked
 */
void B_BuildingOpenAfterClick (const base_t *base, const building_t *building)
{
	assert(base);
	assert(building);
	if (!B_GetBuildingStatus(base, building->buildingType)) {
		UP_OpenWith(building->pedia);
	} else {
		switch (building->buildingType) {
		case B_LAB:
			if (RS_ResearchAllowed(base))
				MN_PushMenu("research", NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_HOSPITAL:
			if (HOS_HospitalAllowed(base))
				MN_PushMenu("hospital", NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_ALIEN_CONTAINMENT:
			if (AC_ContainmentAllowed(base))
				MN_PushMenu("aliencont", NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_QUARTERS:
			if (E_HireAllowed(base))
				MN_PushMenu("employees", NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_WORKSHOP:
			if (PR_ProductionAllowed(base))
				MN_PushMenu("production", NULL);
			else
				UP_OpenWith(building->pedia);
			break;
		case B_DEFENCE_LASER:
		case B_DEFENCE_MISSILE:
			MN_PushMenu("basedefence", NULL);
			break;
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (!AIR_AircraftAllowed(base)) {
				UP_OpenWith(building->pedia);
			} else if (base->numAircraftInBase) {
				MN_PushMenu("aircraft", NULL);
			} else {
				MN_PushMenu("buyaircraft", NULL);
				/* transfer is only possible when there are at least two bases */
				if (gd.numBases > 1)
					MN_Popup(_("Note"), _("No aircraft in this base - You first have to purchase or transfer an aircraft\n"));
				else
					MN_Popup(_("Note"), _("No aircraft in this base - You first have to purchase an aircraft\n"));
			}
			break;
		case B_STORAGE:
		case B_ANTIMATTER:
			if (BS_BuySellAllowed(base))
				MN_PushMenu("buy", NULL);
			else
				UP_OpenWith(building->pedia);
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
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);
	for (i = 0; i < MAX_CAP; i++) {
		const buildingType_t buildingType = B_GetBuildingTypeByCapacity(i);
		if (buildingType >= MAX_BUILDING_TYPE)
			Com_Printf("B_PrintCapacities_f: Could not find building associated with capacity %i\n", i);
		else {
			for (j = 0; j < gd.numBuildingTemplates; j++) {
				if (gd.buildingTemplates[j].buildingType == buildingType)
					break;
			}
			Com_Printf("Building: %s, capacity max: %i, capacity cur: %i\n",
			gd.buildingTemplates[j].id, base->capacities[i].max, base->capacities[i].cur);
		}
	}
}

/**
 * @brief Finishes the construction of every building in the base
 */
static void B_BuildingConstructionFinished_f (void)
{
	int i;
	base_t *base;

	if (!baseCurrent)
		return;

	base = baseCurrent;
	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		building_t *building = &gd.buildings[base->idx][i];
		B_UpdateAllBaseBuildingStatus(building, base, B_STATUS_WORKING);
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
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		/* set buildingStatus[] and capacities.max values */
		B_ResetAllStatusAndCapacities(base, qfalse);
	}
}
#endif

/**
 * @brief Resets console commands.
 * @sa MN_InitStartup
 */
void B_InitStartup (void)
{
	Com_DPrintf(DEBUG_CLIENT, "Reset basemanagement\n");
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
	int i, cnt = 0;

	for (i = 0; i < MAX_BASES; i++) {
		if (!gd.bases[i].founded)
			continue;
		cnt++;
	}

	return cnt;
}

/**
 * @brief Updates base data
 * @sa CL_CampaignRun
 * @note called every "day"
 * @sa AIRFIGHT_ProjectileHitsBase
 */
void B_UpdateBaseData (void)
{
	int baseIdx, j;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		for (j = 0; j < gd.numBuildings[baseIdx]; j++) {
			building_t *b = &gd.buildings[baseIdx][j];
			if (!b)
				continue;
			if (B_CheckBuildingConstruction(b, base)) {
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Construction of %s building finished in base %s."), _(b->name), gd.bases[baseIdx].name);
				MS_AddNewMessage(_("Building finished"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			}
		}

		/* Repair base buildings */
		if (gd.bases[baseIdx].batteryDamage <= MAX_BATTERY_DAMAGE) {
			gd.bases[baseIdx].batteryDamage += 20;
			if (gd.bases[baseIdx].batteryDamage > MAX_BATTERY_DAMAGE)
				gd.bases[baseIdx].batteryDamage = MAX_BATTERY_DAMAGE;
		}
		if (gd.bases[baseIdx].baseDamage <= MAX_BASE_DAMAGE) {
			gd.bases[baseIdx].baseDamage += 20;
			if (gd.bases[baseIdx].baseDamage > MAX_BASE_DAMAGE)
				gd.bases[baseIdx].baseDamage = MAX_BASE_DAMAGE;
		}
	}
}

/**
 * @brief Checks whether the construction of a building is finished.
 * Calls the onConstruct functions and assign workers, too.
 */
int B_CheckBuildingConstruction (building_t *building, base_t *base)
{
	int newBuilding = 0;

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		if (building->timeStart && building->timeStart + building->buildTime <= ccs.date.day) {
			B_UpdateAllBaseBuildingStatus(building, base, B_STATUS_WORKING);

			if (*building->onConstruct) {
				base->buildingCurrent = building;
				Com_DPrintf(DEBUG_CLIENT, "B_CheckBuildingConstruction: %s %i;\n", building->onConstruct, base->idx);
				Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
			}

			newBuilding++;
		}
	}
	if (newBuilding)
		/* update the building-list */
		B_BuildingInit(base);

	return newBuilding;
}

/**
 * @brief Counts the number of soldiers in given aircraft.
 * @param[in] aircraft Pointer to the aircraft, for which we return the amount of soldiers.
 * @return Amount of soldiers.
 */
int B_GetNumOnTeam (const aircraft_t *aircraft)
{
	assert(aircraft);
	return aircraft->teamSize;
}

/**
 * @brief Returns the aircraft pointer from the given base and perform some sanity checks
 * @param[in] base Base the to get the aircraft from - may not be null
 * @param[in] index Base aircraft index
 */
aircraft_t *B_GetAircraftFromBaseByIndex (base_t* base, int index)
{
	assert(base);
	if (index < base->numAircraftInBase) {
		return &base->aircraft[index];
	} else {
		Com_DPrintf(DEBUG_CLIENT, "B_GetAircraftFromBaseByIndex: error: index bigger than number of aircraft in this base\n");
		return NULL;
	}
}

/**
 * @brief Sell items to the market or add them to base storage.
 * @param[in] aircraft Pointer to an aircraft landing in base.
 * @sa CL_AircraftReturnedToHomeBase
 */
static void B_SellOrAddItems (aircraft_t *aircraft)
{
	int i;
	int numitems = 0;
	int gained = 0;
	int forcedsold = 0;
	int forcedgained = 0;
	char str[128];
	itemsTmp_t *cargo;
	base_t *base;

	assert(aircraft);
	base = aircraft->homebase;
	assert(base);

	cargo = aircraft->itemcargo;

	for (i = 0; i < aircraft->itemtypes; i++) {
		technology_t *tech = cargo[i].item->tech;
		if (!tech)
			Sys_Error("B_SellOrAddItems: No tech for %s / %s\n", cargo[i].item->id, cargo[i].item->name);
		/* If the related technology is NOT researched, don't sell items. */
		if (!RS_IsResearched_ptr(tech)) {
			/* Items not researched cannot be thrown out even if not enough space in storage. */
			B_UpdateStorageAndCapacity(base, cargo[i].item, cargo[i].amount, qfalse, qtrue);
			if (cargo[i].amount > 0)
				RS_MarkCollected(tech);
			continue;
		} else {
			/* If the related technology is researched, check the autosell option. */
			if (gd.autosell[cargo[i].item->idx]) { /* Sell items if autosell is enabled. */
				ccs.eMarket.num[cargo[i].item->idx] += cargo[i].amount;
				gained += (cargo[i].item->price * cargo[i].amount);
				numitems += cargo[i].amount;
			} else {
				int j;
				/* Check whether there is enough space for adding this item.
				 * If yes - add. If not - sell. */
				for (j = 0; j < cargo[i].amount; j++) {
					if (!B_UpdateStorageAndCapacity(base, cargo[i].item, 1, qfalse, qfalse)) {
						/* Not enough space, sell item. */
						ccs.eMarket.num[cargo[i].item->idx]++;
						forcedgained += cargo[i].item->price;
						forcedsold++;
					}
				}
			}
			continue;
		}
	}

	if (numitems > 0) {
		Com_sprintf(str, sizeof(str), _("By selling %s you gathered %i credits."),
			va(ngettext("%i collected item", "%i collected items", numitems), numitems), gained);
		MS_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	if (forcedsold > 0) {
		Com_sprintf(str, sizeof(str), _("Not enough storage space in base %s. %s"),
			base->name, va(ngettext("%i item was sold for %i credits.", "%i items were sold for %i credits.", forcedsold), forcedsold, forcedgained));
		MS_AddNewMessage(_("Notice"), str, qfalse, MSG_STANDARD, NULL);
	}
	CL_UpdateCredits(ccs.credits + gained + forcedgained);

	/* ship no longer has cargo aboard */
	aircraft->itemtypes = 0;
}

/**
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft Returning aircraft.
 * @note Place here any stuff, which should be called
 * @note when Drophip returns to base.
 * @sa CL_CampaignRunAircraft
 */
void CL_AircraftReturnedToHomeBase (aircraft_t* aircraft)
{
	aliensTmp_t *cargo;

	AII_ReloadWeapon(aircraft);				/**< Reload weapons */

	/* Don't call cargo functions if aircraft is not a transporter. */
	if (aircraft->type != AIRCRAFT_TRANSPORTER)
		return;
	AL_AddAliens(aircraft);			/**< Add aliens to Alien Containment. */
	B_SellOrAddItems(aircraft);		/**< Sell collected items or add them to storage. */
	RS_MarkResearchable(qfalse, aircraft->homebase);		/**< Mark new technologies researchable. */
	RADAR_InitialiseUFOs(&aircraft->radar);		/**< Reset UFO sensored on radar */

	/** @note Recalculate storage capacity, to fix wrong capacity if a soldier drops something on the ground
	 * @todo this should be removed when new inventory code will be over */
	assert(aircraft->homebase);
	B_UpdateStorageCap(aircraft->homebase);

	/* Now empty alien/item cargo just in case. */
	cargo = AL_GetAircraftAlienCargo(aircraft);
	memset(cargo, 0, sizeof(*cargo));
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));
	AL_SetAircraftAlienCargoTypes(aircraft, 0);
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

	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	/* Com_DPrintf(DEBUG_CLIENT, "B_ItemInBase: DEBUG idx %s\n",  csi.ods[item_idx].id); */

	return ed->num[item->idx];
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
	int i, j, capacity = 0, b_idx = -1;
	buildingType_t buildingType;

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
	case CAP_UFOHANGARS_SMALL:	/**< Base capacities for UFO hangars. */
	case CAP_UFOHANGARS_LARGE:	/**< Base capacities for UFO hangars. */
	case CAP_ANTIMATTER:		/**< Update antimatter capacity in base. */
		/* Reset given capacity in current base. */
		base->capacities[cap].max = 0;
		/* Get building capacity. */
		for (i = 0; i < gd.numBuildingTemplates; i++) {
			if (gd.buildingTemplates[i].buildingType == buildingType) {
				capacity = gd.buildingTemplates[i].capacity;
				Com_DPrintf(DEBUG_CLIENT, "Building: %s capacity: %i\n", gd.buildingTemplates[i].id, capacity);
				b_idx = i;
				break;
			}
		}
		/* Finally update capacity. */
		for (j = 0; j < gd.numBuildings[base->idx]; j++) {
			if ((gd.buildings[base->idx][j].buildingType == buildingType)
			&& (gd.buildings[base->idx][j].buildingStatus >= B_STATUS_CONSTRUCTION_FINISHED)) {
				base->capacities[cap].max += capacity;
			}
		}
		if (b_idx != -1)
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: updated capacity of %s: %i\n",
				gd.buildingTemplates[b_idx].id, base->capacities[cap].max);
		break;
	case MAX_CAP:			/**< Update all capacities in base. */
		Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities: going to update ALL capacities.\n");
		/* Loop through all capacities and update them. */
		for (i = 0; i < cap; i++) {
			B_UpdateBaseCapacities(i, base);
		}
		break;
	default:
		Sys_Error("Unknown capacity limit for this base: %i \n", cap);
		break;
	}

}

/**
 * @brief Saves an item slot
 * @param[in] slot Pointer to the slot where item is.
 * @param[in] sb buffer where information are written.
 * @param[in] weapon True if the slot is a weapon slot.
 * @sa B_LoadOneSlot
 * @sa B_SaveAircraftSlots
 * @sa B_SaveBaseSlots
 * @sa AII_InitialiseSlot
 */
static void B_SaveOneSlot (const aircraftSlot_t* slot, sizebuf_t* sb, qboolean weapon)
{
	const objDef_t *ammo = slot->ammo;
	const objDef_t *nextAmmo = slot->nextAmmo;
	MSG_WriteString(sb, slot->item ? slot->item->id : "");
	if (weapon)
		MSG_WriteString(sb, ammo ? ammo->id : "");
	MSG_WriteString(sb, slot->nextItem ? slot->nextItem->id : "");
	if (weapon)
		MSG_WriteString(sb, nextAmmo ? nextAmmo->id : "");
	MSG_WriteShort(sb, slot->installationTime);
	/* everything below is only for weapon */
	if (!weapon)
		return;
	MSG_WriteShort(sb, slot->ammoLeft);
	MSG_WriteShort(sb, slot->delayNextShot);
}

/**
 * @brief Saves an item slot
 * @param[in] slot Pointer to the slot where item is.
 * @param[in] num Number of slots for this aircraft.
 * @param[in] sb buffer where information are written.
 * @param[in] weapon True if the slot is a weapon slot.
 * @sa B_LoadAircraftSlots
 * @sa B_Save
 * @sa AII_InitialiseSlot
 */
static void B_SaveAircraftSlots (const aircraftSlot_t* slot, const int num, sizebuf_t* sb, qboolean weapon)
{
	int i;

	MSG_WriteByte(sb, num);

	for (i = 0; i < num; i++) {
		B_SaveOneSlot(&slot[i], sb, weapon);
	}
}

/**
 * @brief Saves the weapon slots of a base.
 */
void B_SaveBaseSlots (const baseWeapon_t *weapons, const int numWeapons, sizebuf_t* sb)
{
	int i;

	MSG_WriteByte(sb, numWeapons);

	for (i = 0; i < numWeapons; i++) {
		B_SaveOneSlot(&weapons[i].slot, sb, qtrue);

		/* save target of this weapon */
		MSG_WriteShort(sb, weapons[i].target ? weapons[i].target->idx : -1);
	}
}

/**
 * @brief Save callback for savegames
 * @sa B_Load
 * @sa SAV_GameSave
 */
qboolean B_Save (sizebuf_t* sb, void* data)
{
	int i, k, l;

	MSG_WriteShort(sb, gd.numAircraft);
	for (i = 0; i < presaveArray[PRE_MAXBAS]; i++) {
		const base_t *b = B_GetBaseByIDX(i);
		MSG_WriteByte(sb, b->founded);
		if (!b->founded)
			continue;
		MSG_WriteString(sb, b->name);
		MSG_WritePos(sb, b->pos);
		for (k = 0; k < presaveArray[PRE_BASESI]; k++)
			for (l = 0; l < presaveArray[PRE_BASESI]; l++) {
				MSG_WriteByte(sb, b->map[k][l].building ? b->map[k][l].building->idx : BYTES_NONE);
				MSG_WriteByte(sb, b->map[k][l].blocked);
				MSG_WriteShort(sb, b->map[k][l].posX);
				MSG_WriteShort(sb, b->map[k][l].posY);
			}
		for (k = 0; k < presaveArray[PRE_MAXBUI]; k++) {
			const building_t *building = &gd.buildings[i][k];
			MSG_WriteByte(sb, building->tpl ? building->tpl - gd.buildingTemplates : BYTES_NONE);
			MSG_WriteByte(sb, building->buildingStatus);
			MSG_WriteLong(sb, building->timeStart);
			MSG_WriteLong(sb, building->buildTime);
			MSG_WriteByte(sb, building->level);
			MSG_Write2Pos(sb, building->pos);
		}
		MSG_WriteShort(sb, gd.numBuildings[i]);
		MSG_WriteByte(sb, b->baseStatus);
		MSG_WriteFloat(sb, b->alienInterest);

		B_SaveBaseSlots(b->batteries, b->numBatteries, sb);

		B_SaveBaseSlots(b->lasers, b->numLasers, sb);

		MSG_WriteShort(sb, AIR_GetAircraftIdxInBase(b->aircraftCurrent));
		MSG_WriteByte(sb, b->numAircraftInBase);
		for (k = 0; k < b->numAircraftInBase; k++) {
			const aircraft_t *aircraft = &b->aircraft[k];
			const int alienCargoTypes = AL_GetAircraftAlienCargoTypes(aircraft);
			MSG_WriteString(sb, aircraft->id);
			MSG_WriteShort(sb, aircraft->idx);
			MSG_WriteByte(sb, aircraft->status);
			MSG_WriteLong(sb, aircraft->fuel);
			MSG_WriteLong(sb, aircraft->damage);
			MSG_WritePos(sb, aircraft->pos);
			MSG_WriteLong(sb, aircraft->time);
			MSG_WriteShort(sb, aircraft->point);
			MSG_WriteByte(sb, aircraft->hangar);
			/* Save target of the ufo */
			if (aircraft->aircraftTarget)
				MSG_WriteByte(sb, aircraft->aircraftTarget - gd.ufos);
			else
				MSG_WriteByte(sb, BYTES_NONE);

			/* save weapon slots */
			B_SaveAircraftSlots(aircraft->weapons, aircraft->maxWeapons, sb, qtrue);

			/* save shield slots - currently only one */
			B_SaveAircraftSlots(&aircraft->shield, 1, sb, qfalse);

			/* save electronics slots */
			B_SaveAircraftSlots(aircraft->electronics, aircraft->maxElectronics, sb, qfalse);

			/** Save team on board
			 * @note presaveArray[PRE_ACTTEA]==MAX_ACTIVETEAM and NOT teamSize or maxTeamSize */
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				MSG_WriteByte(sb, (aircraft->acTeam[l] ? aircraft->acTeam[l]->idx : BYTES_NONE));
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				MSG_WriteShort(sb, (aircraft->acTeam[l] ? aircraft->acTeam[l]->type : MAX_EMPL));

			MSG_WriteByte(sb, (aircraft->pilot ? aircraft->pilot->idx : BYTES_NONE));

			MSG_WriteShort(sb, aircraft->numUpgrades);
			MSG_WriteShort(sb, aircraft->radar.range);
			MSG_WriteShort(sb, aircraft->radar.trackingRange);
			MSG_WriteShort(sb, aircraft->route.numPoints);
			MSG_WriteFloat(sb, aircraft->route.distance);
			for (l = 0; l < aircraft->route.numPoints; l++)
				MSG_Write2Pos(sb, aircraft->route.point[l]);
			MSG_WriteShort(sb, alienCargoTypes);
			MSG_WriteShort(sb, aircraft->itemtypes);
			/* aliencargo */
			for (l = 0; l < alienCargoTypes; l++) {
				const aliensTmp_t *cargo = AL_GetAircraftAlienCargo(aircraft);
				assert(cargo[l].teamDef);
				MSG_WriteString(sb, cargo[l].teamDef->id);
				MSG_WriteShort(sb, cargo[l].amount_alive);
				MSG_WriteShort(sb, cargo[l].amount_dead);
			}
			/* itemcargo */
			for (l = 0; l < aircraft->itemtypes; l++) {
				assert(aircraft->itemcargo[l].item);
				MSG_WriteString(sb, aircraft->itemcargo[l].item->id);
				MSG_WriteShort(sb, aircraft->itemcargo[l].amount);
			}
			if (aircraft->status == AIR_MISSION) {
				assert(aircraft->mission);
				MSG_WriteString(sb, aircraft->mission->id);
			}
			MSG_WritePos(sb, aircraft->direction);
			for (l = 0; l < presaveArray[PRE_AIRSTA]; l++) {
#ifdef DEBUG
				if (aircraft->stats[l] < 0)
					Com_Printf("Warning: aircraft '%s' stats %i is smaller than 0\n", aircraft->id, aircraft->stats[l]);
#endif
				MSG_WriteLong(sb, aircraft->stats[l]);
			}
		}

		/* store equipment */
		for (k = 0; k < presaveArray[PRE_NUMODS]; k++) {
			MSG_WriteString(sb, csi.ods[k].id);
			MSG_WriteShort(sb, b->storage.num[k]);
			MSG_WriteByte(sb, b->storage.numLoose[k]);
		}

		MSG_WriteShort(sb, b->radar.range);
		MSG_WriteShort(sb, b->radar.trackingRange);

		/* Alien Containment. */
		for (k = 0; k < presaveArray[PRE_NUMALI]; k++) {
			MSG_WriteString(sb, b->alienscont[k].teamDef->id);
			MSG_WriteShort(sb, b->alienscont[k].amount_alive);
			MSG_WriteShort(sb, b->alienscont[k].amount_dead);
		}

	}
	return qtrue;
}

/**
 * @brief Loads one slot (base, installation or aircraft)
 * @param[in] slot Pointer to the slot where item should be added.
 * @param[in] sb buffer where information are.
 * @param[in] weapon True if the slot is a weapon slot.
 * @sa B_Load
 * @sa B_SaveAircraftSlots
 */
static void B_LoadOneSlot (aircraftSlot_t* slot, sizebuf_t* sb, qboolean weapon)
{
	const char *name;
	name = MSG_ReadString(sb);
	if (name[0] != '\0') {
		technology_t *tech = RS_GetTechByProvided(name);
		/* base is NULL here to not check against the storage amounts - they
		 * are already loaded in the campaign load function and set to the value
		 * after the craftitem was already removed from the initial game - thus
		 * there might not be any of these items in the storage at this point.
		 * Furthermore, they have already be taken from storage during game. */
		if (tech)
			AII_AddItemToSlot(NULL, tech, slot, qfalse);
	}

	/* current ammo */
	if (weapon) {
		name = MSG_ReadString(sb);
		if (name[0] != '\0') {
			technology_t *tech = RS_GetTechByProvided(name);
			if (tech)
				AII_AddAmmoToSlot(NULL, tech, slot);	/* next Item must not be loaded yet in order to
															 * install ammo properly */
		}
	}
	/* item to install after current one is removed */
	name = MSG_ReadString(sb);
	if (name[0] != '\0') {
		technology_t *tech = RS_GetTechByProvided(name);
		if (tech)
			AII_AddItemToSlot(NULL, tech, slot, qtrue);
	}
	if (weapon) {
		/* ammo to install after current one is removed */
		name = MSG_ReadString(sb);
		if (name[0] != '\0') {
			technology_t *tech = RS_GetTechByProvided(name);
			if (tech)
				AII_AddAmmoToSlot(NULL, tech, slot);
		}
	}

	slot->installationTime = MSG_ReadShort(sb);

	/* everything below is weapon specific */
	if (!weapon)
		return;

	slot->ammoLeft = MSG_ReadShort(sb);
	slot->delayNextShot = MSG_ReadShort(sb);
}

/**
 * @brief Loads the weapon slots of an aircraft.
 * @param[in] aircraft Pointer to the aircraft.
 * @param[in] slot Pointer to the slot where item should be added.
 * @param[in] num Number of slots for this aircraft that should be loaded.
 * @param[in] sb buffer where information are.
 * @param[in] weapon True if the slot is a weapon slot.
 * @sa B_Load
 * @sa B_SaveAircraftSlots
 */
static void B_LoadAircraftSlots (aircraft_t *aircraft, aircraftSlot_t* slot, int num, sizebuf_t* sb, qboolean weapon)
{
	int i;

	for (i = 0; i < num; i++) {
		slot[i].aircraft = aircraft;
		B_LoadOneSlot(&slot[i], sb, weapon);
	}
}

/**
 * @brief Loads the missile and laser slots of a base.
 * @sa B_Load
 * @sa B_SaveBaseSlots
 */
void B_LoadBaseSlots (baseWeapon_t* weapons, int numWeapons, sizebuf_t* sb)
{
	int i, target;

	for (i = 0; i < numWeapons; i++) {
		B_LoadOneSlot(&weapons[i].slot, sb, qtrue);

		target = MSG_ReadShort(sb);
		weapons[i].target = (target >= 0) ? &gd.ufos[target] : NULL;
	}
}

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa B_PostLoadInit
 */
static void B_PostLoadInitCapacity (void)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		B_ResetAllStatusAndCapacities(base, qtrue);
	}
}

#ifdef DEBUG
/**
 * @brief Make some test after loading a savegame
 * @sa B_PostLoadInit
 */
static void B_PostLoadInitCheckAircraft (void)
{
	int baseIdx;
	int totalNumberOfAircraft = 0;
	int i;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		for (i = 0; i < base->numAircraftInBase; i++) {
			aircraft_t *aircraft = &base->aircraft[i];
			totalNumberOfAircraft++;

			/* if aircraft is attacking a UFO, it should have a target */
			if (aircraft->status == AIR_UFO && (!aircraft->aircraftTarget || !aircraft->aircraftTarget->tpl)) {
				Com_Printf("Error in B_PostLoadInitCheckAircraft(): aircraft '%s' is attacking but has no target\n", aircraft->name);
				aircraft->aircraftTarget = NULL;
				AIR_AircraftReturnToBase(aircraft);
			}
		}
	}

	if (totalNumberOfAircraft != gd.numAircraft)
		Com_Printf("Error in B_PostLoadInitCheckAircraft(): Total number of aircraft (%i) does not fit saved value (%i)\n", totalNumberOfAircraft, gd.numAircraft);
}
#endif

/**
 * @brief Set the capacity stuff for all the bases after loading a savegame
 * @sa SAV_GameActionsAfterLoad
 */
void B_PostLoadInit (void)
{
	B_PostLoadInitCapacity();

#ifdef DEBUG
	B_PostLoadInitCheckAircraft();
#endif
}

#define MAX_TEAMLIST_SIZE_FOR_LOADING 32
/**
 * @brief Load callback for savegames
 * @sa B_Save
 * @sa SAV_GameLoad
 * @sa B_LoadItemSlots
 */
qboolean B_Load (sizebuf_t* sb, void* data)
{
	int i, k, l, amount, ufoIdx;
	int aircraftIdxInBase;
	int teamIdxs[MAX_TEAMLIST_SIZE_FOR_LOADING];	/**< Temp list of employee indices. */
	int teamTypes[MAX_TEAMLIST_SIZE_FOR_LOADING];	/**< Temp list of employee-types. */
	int buildingIdx;
	int pilotIdx;

	gd.numAircraft = MSG_ReadShort(sb);
	for (i = 0; i < presaveArray[PRE_MAXBAS]; i++) {
		base_t *const b = B_GetBaseByIDX(i);
		b->founded = MSG_ReadByte(sb);
		if (!b->founded)
			continue;
		Q_strncpyz(b->name, MSG_ReadStringRaw(sb), sizeof(b->name));
		MSG_ReadPos(sb, b->pos);

		for (k = 0; k < presaveArray[PRE_BASESI]; k++)
			for (l = 0; l < presaveArray[PRE_BASESI]; l++) {
				buildingIdx = MSG_ReadByte(sb);
				if (buildingIdx != BYTES_NONE)
					b->map[k][l].building = &gd.buildings[i][buildingIdx];	/** The buildings are actually parsed _below_. (See PRE_MAXBUI loop) */
				else
					b->map[k][l].building = NULL;
				b->map[k][l].blocked = MSG_ReadByte(sb);
				b->map[k][l].posX = MSG_ReadShort(sb);
				b->map[k][l].posY = MSG_ReadShort(sb);
			}
		for (k = 0; k < presaveArray[PRE_MAXBUI]; k++) {
			building_t *const building = &gd.buildings[i][k];
			byte buildingTpl;
			buildingTpl = MSG_ReadByte(sb);
			if (buildingTpl != BYTES_NONE)
				*building = gd.buildingTemplates[buildingTpl];
			building->idx = k;
			building->base = b;
			building->buildingStatus = MSG_ReadByte(sb);
			building->timeStart = MSG_ReadLong(sb);
			building->buildTime = MSG_ReadLong(sb);
			building->level = MSG_ReadByte(sb);
			MSG_Read2Pos(sb, building->pos);
		}
		gd.numBuildings[i] = MSG_ReadShort(sb);
		b->baseStatus = MSG_ReadByte(sb);
		b->alienInterest = MSG_ReadFloat(sb);
		BDEF_InitialiseBaseSlots(b);

		/* read missile battery slots */
		b->numBatteries = MSG_ReadByte(sb);
		if (b->numBatteries > MAX_BASE_SLOT) {
			Com_Printf("B_Load: number of batteries in base '%s' (%i) exceed maximum value (%i)\n", b->name, b->numBatteries, MAX_BASE_SLOT);
			return qfalse;
		}
		B_LoadBaseSlots(b->batteries, b->numBatteries, sb);

		/* read laser battery slots */
		b->numLasers = MSG_ReadByte(sb);
		if (b->numLasers > MAX_BASE_SLOT) {
			Com_Printf("B_Load: number of lasers in base '%s' (%i) exceed maximum value (%i)\n", b->name, b->numLasers, MAX_BASE_SLOT);
			return qfalse;
		}
		B_LoadBaseSlots(b->lasers, b->numLasers, sb);

		b->aircraftCurrent = NULL;
		aircraftIdxInBase = MSG_ReadShort(sb);
		if (aircraftIdxInBase != AIRCRAFT_INBASE_INVALID)
			b->aircraftCurrent = &b->aircraft[aircraftIdxInBase];

		b->numAircraftInBase = MSG_ReadByte(sb);
		if (b->numAircraftInBase > MAX_AIRCRAFT) {
			Com_Printf("B_Load: number of aircraft in base '%s' (%i) exceed maximum value (%i)\n", b->name, b->numAircraftInBase, MAX_AIRCRAFT);
			return qfalse;
		}
		for (k = 0; k < b->numAircraftInBase; k++) {
			aircraft_t *aircraft;
			int alienCargoTypes;

			const aircraft_t *const model = AIR_GetAircraft(MSG_ReadString(sb));
			if (!model)
				return qfalse;
			/* copy generic aircraft description to individal aircraft in base */
			aircraft = &b->aircraft[k];
			*aircraft = *model;
			aircraft->idx = MSG_ReadShort(sb);
			aircraft->homebase = b;
			/* this is the aircraft array id in current base */
			aircraft->status = MSG_ReadByte(sb);
			aircraft->fuel = MSG_ReadLong(sb);
			aircraft->damage = MSG_ReadLong(sb);
			MSG_ReadPos(sb, aircraft->pos);
			aircraft->time = MSG_ReadLong(sb);
			aircraft->point = MSG_ReadShort(sb);
			aircraft->hangar = MSG_ReadByte(sb);
			/* load aircraft target */
			ufoIdx = MSG_ReadByte(sb);
			if (ufoIdx == BYTES_NONE)
				aircraft->aircraftTarget = NULL;
			else
				aircraft->aircraftTarget = gd.ufos + ufoIdx;

			/* read weapon slots */
			amount = MSG_ReadByte(sb);
			if (aircraft->maxWeapons < amount) {
				Com_Printf("B_Load: number of weapons in aircraft '%s' (%i) exceed maximum value (%i)\n", aircraft->id, amount, aircraft->maxWeapons);
				return qfalse;
			}
			B_LoadAircraftSlots(aircraft, aircraft->weapons, amount, sb, qtrue);

			/* read shield slot */
			amount = MSG_ReadByte(sb);
			if (amount != 1) {
				Com_Printf("B_Load: There should be only one slot for shield in aircraft '%s'\n", aircraft->id);
				return qfalse;
			}
			B_LoadAircraftSlots(aircraft, &aircraft->shield, 1, sb, qfalse);

			/* read electronic slots */
			amount = MSG_ReadByte(sb);
			if (aircraft->maxElectronics < amount) {
				Com_Printf("B_Load: number of electronics in aircraft '%s' (%i) exceed maximum value (%i)\n", aircraft->id, amount, aircraft->maxElectronics);
				return qfalse;
			}
			B_LoadAircraftSlots(aircraft, aircraft->electronics, amount, sb, qfalse);

			/** Load team on board
			 * @note presaveArray[PRE_ACTTEA] == MAX_ACTIVETEAM and NOT teamSize or maxTeamSize */
			assert(presaveArray[PRE_ACTTEA] < MAX_TEAMLIST_SIZE_FOR_LOADING);
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				teamIdxs[l] = MSG_ReadByte(sb);
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				teamTypes[l] = MSG_ReadShort(sb);

			aircraft->teamSize = 0;
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++) {
				if (teamIdxs[l] != BYTES_NONE) {
					assert(teamTypes[l] != MAX_EMPL);
					/** assert(gd.numEmployees[teamTypes[l]] > 0); @todo We currently seem to link to not yet parsed employees. */
					aircraft->acTeam[l] = &gd.employees[teamTypes[l]][teamIdxs[l]];
					aircraft->teamSize++;
				}
			}

			pilotIdx = MSG_ReadByte(sb);
			/* the employee subsystem is loaded after the base subsystem
			 * this means, that the pilot pointer is not (really) valid until
			 * E_Load was called, too */
			if (pilotIdx != BYTES_NONE)
				aircraft->pilot = &gd.employees[EMPL_PILOT][pilotIdx];
			else
				aircraft->pilot = NULL;

			aircraft->numUpgrades = MSG_ReadShort(sb);
			RADAR_InitialiseUFOs(&aircraft->radar);
			aircraft->radar.range = MSG_ReadShort(sb);
			aircraft->radar.trackingRange = MSG_ReadShort(sb);
			aircraft->route.numPoints = MSG_ReadShort(sb);
			if (aircraft->route.numPoints > LINE_MAXPTS) {
				Com_Printf("B_Load: number of points (%i) for aircraft route exceed maximum value (%i)\n", aircraft->route.numPoints, LINE_MAXPTS);
				return qfalse;
			}
			aircraft->route.distance = MSG_ReadFloat(sb);
			for (l = 0; l < aircraft->route.numPoints; l++)
				MSG_Read2Pos(sb, aircraft->route.point[l]);
			alienCargoTypes = MSG_ReadShort(sb);
			AL_SetAircraftAlienCargoTypes(aircraft, alienCargoTypes);
			alienCargoTypes = AL_GetAircraftAlienCargoTypes(aircraft);
			if (alienCargoTypes > MAX_CARGO) {
				Com_Printf("B_Load: number of alien types (%i) exceed maximum value (%i)\n", alienCargoTypes, MAX_CARGO);
				return qfalse;
			}
			aircraft->itemtypes = MSG_ReadShort(sb);
			if (aircraft->itemtypes > MAX_CARGO) {
				Com_Printf("B_Load: number of item types (%i) exceed maximum value (%i)\n", aircraft->itemtypes, MAX_CARGO);
				return qfalse;
			}
			/* aliencargo */
			for (l = 0; l < alienCargoTypes; l++) {
				aliensTmp_t *cargo = AL_GetAircraftAlienCargo(aircraft);
				cargo[l].teamDef = Com_GetTeamDefinitionByID(MSG_ReadString(sb));
				if (!cargo[l].teamDef)
					return qfalse;
				cargo[l].amount_alive = MSG_ReadShort(sb);
				cargo[l].amount_dead = MSG_ReadShort(sb);
			}
			/* itemcargo */
			for (l = 0; l < aircraft->itemtypes; l++) {
				const char *const s = MSG_ReadString(sb);
				const objDef_t *od = INVSH_GetItemByID(s);
				if (!od) {
					Com_Printf("B_Load: Could not find aircraftitem '%s'\n", s);
					MSG_ReadShort(sb);
				} else {
					aircraft->itemcargo[l].item = od;
					aircraft->itemcargo[l].amount = MSG_ReadShort(sb);
				}
			}
			if (aircraft->status == AIR_MISSION)
				aircraft->missionID = Mem_PoolStrDup(MSG_ReadString(sb), cl_localPool, 0);
			MSG_ReadPos(sb, aircraft->direction);
			for (l = 0; l < presaveArray[PRE_AIRSTA]; l++)
				aircraft->stats[l] = MSG_ReadLong(sb);
		}

		/* read equipment */
		for (k = 0; k < presaveArray[PRE_NUMODS]; k++) {
			const char *const s = MSG_ReadString(sb);
			const objDef_t *od = INVSH_GetItemByID(s);
			if (!od) {
				Com_Printf("B_Load: Could not find item '%s'\n", s);
				MSG_ReadShort(sb);
				MSG_ReadByte(sb);
			} else {
				b->storage.num[od->idx] = MSG_ReadShort(sb);
				b->storage.numLoose[od->idx] = MSG_ReadByte(sb);
			}
		}

		RADAR_InitialiseUFOs(&b->radar);
		RADAR_Initialise(&b->radar, MSG_ReadShort(sb), MSG_ReadShort(sb), B_GetMaxBuildingLevel(b, B_RADAR), qtrue);

		/* Alien Containment. */

		/* Fill Alien Containment with default values like the tech pointer. */
		AL_FillInContainment(b);
		for (k = 0; k < presaveArray[PRE_NUMALI]; k++) {
			const char *const s = MSG_ReadString(sb);
			b->alienscont[k].teamDef = Com_GetTeamDefinitionByID(s);
			if (!b->alienscont[k].teamDef) {
				Com_Printf("B_Load: Could not find teamDef: '%s' (aliencont %i) ... skipped!\n", s, k);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
			} else {
				b->alienscont[k].amount_alive = MSG_ReadShort(sb);
				b->alienscont[k].amount_dead = MSG_ReadShort(sb);
			}
		}

		/* clear the mess of stray loaded pointers */
		memset(&b->bEquipment, 0, sizeof(b->bEquipment));
	}
	gd.numBases = B_GetFoundedBaseCount();
	Cvar_Set("mn_base_count", va("%i", gd.numBases));

	return qtrue;
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
	if (reset) {
		base->storage.num[obj->idx] = 0;
		base->storage.numLoose[obj->idx] = 0; /** @todo needed? */
		base->capacities[CAP_ITEMS].cur = 0;
	} else {
		if (!INV_ItemsIsStoredInStorage(obj)) {
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Item '%s' is not stored in storage: skip\n", obj->id);
			return qfalse;
		}

		if (!ignorecap && (amount > 0)) {
			/* Only add items if there is enough room in storage */
			if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < (obj->size * amount)) {
				Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Not enough storage space (item: %s, amount: %i)\n", obj->id, amount);
				return qfalse;
			}
		}

		base->storage.num[obj->idx] += amount;
		if (obj->size > 0)
			base->capacities[CAP_ITEMS].cur += (amount * obj->size);

		if (base->capacities[CAP_ITEMS].cur < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current storage capacity is negative (%i): reset to 0\n", base->capacities[CAP_ITEMS].cur);
			base->capacities[CAP_ITEMS].cur = 0;
		}

		if (base->storage.num[obj->idx] < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current number of item '%s' is negative: reset to 0\n", obj->id);
			base->storage.num[obj->idx] = 0;
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

	for (i = 0, b = gd.buildingTemplates; i < gd.numBuildingTemplates; i++, b++) {
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
	int numObj;

	if (base->capacities[CAP_ITEMS].cur <= base->capacities[CAP_ITEMS].max)
		return;

	for (i = 0, numObj = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];

		if (!INV_ItemsIsStoredInStorage(obj))
			continue;

		/* Don't count item that we don't have in base */
		if (!base->storage.num[i])
			continue;

		objIdx[numObj++] = i;
	}

	/* UGV takes room in storage capacity: we store them with a value MAX_OBJDEFS that can't be used by objIdx */
	for (i = 0; i < E_CountHired(base, EMPL_ROBOT); i++) {
		objIdx[numObj++] = MAX_OBJDEFS;
	}

	while (numObj && base->capacities[CAP_ITEMS].cur > base->capacities[CAP_ITEMS].max) {
		/* Select the item to remove */
		const int randNumber = rand() % numObj;
		if (objIdx[randNumber] >= MAX_OBJDEFS) {
			/* A UGV is destroyed: get first one */
			employee_t* employee = E_GetHiredRobot(base, 0);
			/* There should be at least a UGV */
			assert(employee);
			E_DeleteEmployee(employee, EMPL_ROBOT);
		} else {
			/* items are destroyed. We guess that all items of a given type are stored in the same location
			 *	=> destroy all items of this type */
			const int idx = objIdx[randNumber];
			assert(idx >= 0);
			assert(idx < MAX_OBJDEFS);
			B_UpdateStorageAndCapacity(base, &csi.ods[idx], -base->storage.num[idx], qfalse, qfalse);
		}
		REMOVE_ELEM(objIdx, randNumber, numObj);

		/* Make sure that we don't have an infinite loop */
		if (numObj <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "B_RemoveItemsExceedingCapacity: Remains %i in storage for a maximum of %i\n",
		base->capacities[CAP_ITEMS].cur, base->capacities[CAP_ITEMS].max);
}

/**
 * @brief Remove ufos until everything fits in ufo hangars.
 * @param[in] base Pointer to the base
 * @param[in] ufohangar type
 */
void B_RemoveUFOsExceedingCapacity (base_t *base, const buildingType_t buildingType)
{
	const baseCapacities_t capacity_type = B_GetCapacityFromBuildingType (buildingType);
	int i;
	int objIdx[MAX_OBJDEFS];	/**< Will contain idx of items that can be removed */
	int numObj;

	if (capacity_type != CAP_UFOHANGARS_SMALL && capacity_type != CAP_UFOHANGARS_LARGE)
		return;

	if (base->capacities[capacity_type].cur <= base->capacities[capacity_type].max)
		return;

	for (i = 0, numObj = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];
		aircraft_t *ufocraft;

		/* Don't count what isn't an aircraft */
		assert(obj->tech);
		if (obj->tech->type != RS_CRAFT) {
			continue;
		}

		/* look for corresponding aircraft in global array */
		ufocraft = AIR_GetAircraft (obj->id);
		if (!ufocraft) {
			Com_DPrintf(DEBUG_CLIENT, "B_RemoveUFOsExceedingCapacity: Did not find UFO %s\n", obj->id);
			continue;
		}

		if (ufocraft->size == AIRCRAFT_LARGE && capacity_type != CAP_UFOHANGARS_LARGE)
			continue;
		if (ufocraft->size == AIRCRAFT_SMALL && capacity_type != CAP_UFOHANGARS_SMALL)
			continue;

		/* Don't count item that we don't have in base */
		if (!base->storage.num[i])
			continue;

		objIdx[numObj++] = i;
	}

	while (numObj && base->capacities[capacity_type].cur > base->capacities[capacity_type].max) {
		/* Select the item to remove */
		const int randNumber = rand() % numObj;
		/* items are destroyed. We guess that all items of a given type are stored in the same location
		 *	=> destroy all items of this type */
		const int idx = objIdx[randNumber];

		assert(idx >= 0);
		assert(idx < MAX_OBJDEFS);
		B_UpdateStorageAndCapacity(base, &csi.ods[idx], -base->storage.num[idx], qfalse, qfalse);

		REMOVE_ELEM(objIdx, randNumber, numObj);
		UR_UpdateUFOHangarCapForAll(base);

		/* Make sure that we don't have an infinite loop */
		if (numObj <= 0)
			break;
	}
	Com_DPrintf(DEBUG_CLIENT, "B_RemoveUFOsExceedingCapacity: Remains %i in storage for a maxium of %i\n",
		base->capacities[capacity_type].cur, base->capacities[capacity_type].max);
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
		const objDef_t *obj = &csi.ods[i];

		if (!INV_ItemsIsStoredInStorage(obj))
			continue;

		base->capacities[CAP_ITEMS].cur += base->storage.num[i] * obj->size;
	}

	/* UGV takes room in storage capacity */
	base->capacities[CAP_ITEMS].cur += UGV_SIZE * E_CountHired(base, EMPL_ROBOT);
}

/**
 * @brief Manages Antimatter (adding, removing) through Antimatter Storage Facility.
 * @param[in] base Pointer to the base.
 * @param[in] amount quantity of antimatter to add/remove (> 0 even if antimatter is removed)
 * @param[in] add True if we are adding antimatter, false when removing.
 * @note This function should be called whenever we add or remove antimatter from Antimatter Storage Facility.
 * @note Call with amount = 0 if you want to remove ALL antimatter from given base.
 */
void B_ManageAntimatter (base_t *base, int amount, qboolean add)
{
	int i, j;
	objDef_t *od;

	assert(base);

	if (!B_GetBuildingStatus(base, B_ANTIMATTER) && add) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Base %s does not have Antimatter Storage Facility. %i units of Antimatter got removed."),
			base->name, amount);
		MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
		return;
	}

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		if (!Q_strncmp(od->id, "antimatter", 10))
			break;
	}

	if (i == csi.numODs)
		Sys_Error("Could not find antimatter object definition");

	if (add) {	/* Adding. */
		if (base->capacities[CAP_ANTIMATTER].cur + (amount * ANTIMATTER_SIZE) <= base->capacities[CAP_ANTIMATTER].max) {
			base->storage.num[i] += amount;
			base->capacities[CAP_ANTIMATTER].cur += (amount * ANTIMATTER_SIZE);
		} else {
			for (j = 0; j < amount; j++) {
				if (base->capacities[CAP_ANTIMATTER].cur + ANTIMATTER_SIZE <= base->capacities[CAP_ANTIMATTER].max) {
					base->storage.num[i]++;
					base->capacities[CAP_ANTIMATTER].cur += ANTIMATTER_SIZE;
				} else
					break;
			}
		}
	} else {	/* Removing. */
		if (amount == 0) {
			base->capacities[CAP_ANTIMATTER].cur = 0;
			base->storage.num[i] = 0;
		} else {
			base->capacities[CAP_ANTIMATTER].cur -= amount * ANTIMATTER_SIZE;
			base->storage.num[i] -= amount;
		}
	}
}

/**
 * @brief Remove exceeding antimatter if an antimatter tank has been destroyed.
 * @param[in] base Pointer to the base.
 */
void B_RemoveAntimatterExceedingCapacity (base_t *base)
{
	const int amount = ceil(((float) (base->capacities[CAP_ANTIMATTER].cur - base->capacities[CAP_ANTIMATTER].max)) / ANTIMATTER_SIZE);
	if (amount < 0)
		return;

	B_ManageAntimatter(base, amount, qfalse);
}
