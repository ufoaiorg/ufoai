/**
 * @file cl_basemanagement.c
 * @brief Handles everything that is located in or accessed trough a base.
 * @note Basemanagement functions prefix: B_
 * @note See "base/ufos/basemanagement.ufo", "base/ufos/menu_bases.ufo" and "base/ufos/menu_buildings.ufo" for the underlying content.
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

#include "client.h"
#include "cl_global.h"
#include "cl_team.h"
#include "cl_mapfightequip.h"
#include "cl_hospital.h"
#include "cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "../renderer/r_draw.h"
#include "menu/m_nodes.h"
#include "menu/m_popup.h"

vec3_t newBasePos;
static cvar_t *mn_base_title;
static cvar_t *mn_base_count;
static cvar_t *mn_base_id;
static cvar_t *cl_equip;

/** @brief allocate memory for mn.menuText[TEXT_STANDARD] contained the information about a building */
static char buildingText[MAX_LIST_CHAR];

static int BuildingConstructionList[MAX_BUILDINGS];
static int numBuildingConstructionList;

static void B_BuildingInit(base_t* base);

/**
 * @brief Count all employees (hired) in the given base
 */
int B_GetEmployeeCount (const base_t* const base)
{
	int cnt = 0;
	employeeType_t type;

	for (type = EMPL_SOLDIER; type < MAX_EMPL; type++)
		cnt += E_CountHired(base, type);
	Com_DPrintf(DEBUG_CLIENT, "B_GetEmployeeCount: %i\n", cnt);

	return cnt;
}

/**
 * @brief Array bound check for the base id
 */
base_t* B_GetBase (int id)
{
	assert(id < MAX_BASES);
	assert(id >= 0);
	return &gd.bases[id];
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
	case B_HOSPITAL:
		return CAP_HOSPSPACE;
	case B_HANGAR:
		return CAP_AIRCRAFTS_BIG;
	case B_ALIEN_CONTAINMENT:
		return CAP_ALIENS;
	case B_SMALL_HANGAR:
		return CAP_AIRCRAFTS_SMALL;
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
static buildingType_t B_GetBuildingTypeByCapacity (baseCapacities_t cap)
{
	switch (cap) {
	case CAP_ALIENS:
		return B_ALIEN_CONTAINMENT;
	case CAP_AIRCRAFTS_SMALL:
		return B_SMALL_HANGAR;
	case CAP_AIRCRAFTS_BIG:
		return B_HANGAR;
	case CAP_EMPLOYEES:
		return B_QUARTERS;
	case CAP_ITEMS:
		return B_STORAGE;
	case CAP_LABSPACE:
		return B_LAB;
	case CAP_WORKSPACE:
		return B_WORKSHOP;
	case CAP_HOSPSPACE:
		return B_HOSPITAL;
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
 * @param[in] type value of building->buildingType
 * @param[in] building
 * @return true if the building is functional
 */
qboolean B_GetBuildingStatus (const base_t* const base, buildingType_t type)
{
	assert(base);

	if (type == B_MISC)
		return qtrue;
	else if (type < MAX_BUILDING_TYPE)
		return base->hasBuilding[type];
	else {
		Com_Printf("B_GetBuildingStatus()... Type %i does not exists.\n", type);
		return qfalse;
	}
}


/**
 * @brief Set status associated to a building
 * @param[in] base Base to check
 * @param[in] type value of building->buildingType
 * @param[in] newStatus New value of the status
 */
void B_SetBuildingStatus (base_t* const base, buildingType_t type, qboolean newStatus)
{
	assert(base);

	if (type == B_MISC)
		Com_Printf("B_SetBuildingStatus: No status is associated to B_MISC type of building.\n");
	else if (type < MAX_BUILDING_TYPE)
		base->hasBuilding[type] = newStatus;
	else
		Com_Printf("B_SetBuildingStatus()... Type of building %i does not exists\n", type);
}

/**
 * @brief Chech that the dependences of a building is operationnal
 * @param[in] base Base to check
 * @param[in] building
 * @return true if base contains needed dependence for entering building
 */
qboolean B_CheckBuildingDependencesStatus (const base_t* const base, building_t* building)
{
	building_t *dependsBuilding;

	assert(base);
	assert(building);

	if (building->dependsBuilding == -1)
		return qtrue;

	dependsBuilding = gd.buildingTypes + building->dependsBuilding;

	return B_GetBuildingStatus(base, dependsBuilding->buildingType);
}

/**
 * @note Make sure you are not doing anything with the buildingCurrent pointer
 * in this function, the pointer might already be invalid
 */
static void B_ResetBuildingCurrent (base_t* base)
{
	if (base) {
		base->buildingCurrent = NULL;
		base->buildingToBuild = -1;
	}
	gd.baseAction = BA_NONE;
}

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu but also several times from cl_basemanagement.c.
 */
static void B_ResetBuildingCurrent_f (void)
{
	if (Cmd_Argc() == 2)
		ccs.instant_build = atoi(Cmd_Argv(1));

	B_ResetBuildingCurrent(baseCurrent);
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
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(building_t, name), 0},	/**< The displayed building name. */
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
	{"onupgrade", V_STRING, offsetof(building_t, onUpgrade), 0}, /**< Event handler. */
	{"onrepair", V_STRING, offsetof(building_t, onRepair), 0}, /**< Event handler. */
	{"onclick", V_STRING, offsetof(building_t, onClick), 0}, /**< Event handler. */
	{"pos", V_POS, offsetof(building_t, pos), MEMBER_SIZEOF(building_t, pos)}, /**< Place of a building. Needed for flag autobuild */
	{"autobuild", V_BOOL, offsetof(building_t, autobuild), MEMBER_SIZEOF(building_t, autobuild)}, /**< Automatically construct this building when a base is set up. Must also set the pos-flag. */
	{"firstbase", V_BOOL, offsetof(building_t, firstbase), MEMBER_SIZEOF(building_t, firstbase)}, /**< Automatically construct this building for the first base you build. Must also set the pos-flag. */
	{NULL, 0, 0, 0}
};

/**
 * @brief Initialises base.
 * @note This command is executed in the init node of the base menu.
 * It is called everytime the base menu pops up and sets the cvars.
 * The current selected base is determined via cvar mn_base_id.
 */
static void B_BaseInit_f (void)
{
	if (!mn_base_id)
		return;

	/* sanity check */
	if (mn_base_id->integer < 0 || mn_base_id->integer > B_GetFoundedBaseCount()) {
		Com_Printf("B_BaseInit_f: mn_base_id value is invalid: %i\n", mn_base_id->integer);
		return;
	}

	baseCurrent = B_GetBase(mn_base_id->integer);

	CL_UpdateCredits(ccs.credits);

	/* activate or deactivate the aircraft button */
	if (AIR_AircraftAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_num_aircraft", baseCurrent->numAircraftInBase);
		Cmd_ExecuteString("set_aircraft_enabled");
	} else {
		Cvar_SetValue("mn_base_num_aircraft", -1);
		Cmd_ExecuteString("set_aircraft_disabled");
	}
	if (BS_BuySellAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_buysell_allowed", qtrue);
		Cmd_ExecuteString("set_buysell_enabled");
	} else {
		Cvar_SetValue("mn_base_buysell_allowed", qfalse);
		Cmd_ExecuteString("set_buysell_disabled");
	}
	if (gd.numBases > 1 && baseCurrent->baseStatus != BASE_UNDER_ATTACK) {
		Cmd_ExecuteString("set_transfer_enabled");
	} else {
		Cmd_ExecuteString("set_transfer_disabled");
	}
	if (RS_ResearchAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_research_allowed", qtrue);
		Cmd_ExecuteString("set_research_enabled");
	} else {
		Cvar_SetValue("mn_base_research_allowed", qfalse);
		Cmd_ExecuteString("set_research_disabled");
	}
	if (PR_ProductionAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_prod_allowed", qtrue);
		Cmd_ExecuteString("set_prod_enabled");
	} else {
		Cvar_SetValue("mn_base_prod_allowed", qfalse);
		Cmd_ExecuteString("set_prod_disabled");
	}
	if (E_HireAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_hire_allowed", qtrue);
		Cmd_ExecuteString("set_hire_enabled");
	} else {
		Cvar_SetValue("mn_base_hire_allowed", qfalse);
		Cmd_ExecuteString("set_hire_disabled");
	}
	if (AC_ContainmentAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_containment_allowed", qtrue);
		Cmd_ExecuteString("set_containment_enabled");
	} else {
		Cvar_SetValue("mn_base_containment_allowed", qfalse);
		Cmd_ExecuteString("set_containment_disabled");
	}
	if (HOS_HospitalAllowed(baseCurrent)) {
		Cvar_SetValue("mn_base_hospital_allowed", qtrue);
		Cmd_ExecuteString("set_hospital_enabled");
	} else {
		Cvar_SetValue("mn_base_hospital_allowed", qfalse);
		Cmd_ExecuteString("set_hospital_disabled");
	}
}

/**
 * @brief Get the maximum level of a building type in a base.
 * @param[in] base Pointer to base.
 * @param[in] Building type to get the maximum level for.
 * @note This function checks base status for particular buildings.
 * @return 0.0f if there is no (operational) building of the requested type in the base, otherwise the maximum level.
 */
static float B_GetMaxBuildingLevel (base_t* base, buildingType_t type)
{
	int i;
	float max = 0.0f;

	if (base->hasBuilding[type]) {
		for (i = 0; i < gd.numBuildings[base->idx]; i++) {
			if (gd.buildings[base->idx][i].buildingType == type && gd.buildings[base->idx][i].buildingStatus == B_STATUS_WORKING) {
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

	/* status of Entrance and Miscellenious buildings cannot change */
	if (building->buildingType == B_ENTRANCE && building->buildingType == B_MISC)
		return qfalse;

	oldValue = B_GetBuildingStatus(base, building->buildingType);
	if ((building->buildingStatus == B_STATUS_WORKING) && (B_CheckBuildingDependencesStatus(base, building)))
		B_SetBuildingStatus(base, building->buildingType, qtrue);
	else
		B_SetBuildingStatus(base, building->buildingType, qfalse);

	if (B_GetBuildingStatus(base, building->buildingType) != oldValue) {
		Com_DPrintf(DEBUG_CLIENT, "Status of building %s is changed to %i.\n", building->name, B_GetBuildingStatus(base, building->buildingType));
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Actions to perform when a type of buildings may have changed its status.
 * @note This function is not only called when a building is enabled for the first time in base
 * @note but also when one of its dependencies is destroyed and then rebuilt
 * @param[in] type Type of building that has been modified from qfalse to qtrue
 * @param[in] base Pointer to base with given building.
 */
static void B_UpdateOneBaseBuildingStatus (buildingType_t type, base_t* base)
{
	float level;

	switch (type) {
	case B_RADAR:
		level = B_GetMaxBuildingLevel(base, B_RADAR);
		Radar_Initialise(&base->radar, baseRadarRange, level);
		break;
	case B_WORKSHOP:
		PR_UpdateProductionCap(baseCurrent);
		break;
	default:
		break;
	}
}

/**
 * @brief Actions to perform when a type of buildings goes from qfalse to qtrue.
 * @note This function is not only called when a building is enabled for the first time in base
 * @note but also when one of its dependencies is destroyed and then rebuilt
 * @param[in] type Type of building that has been modified from qfalse to qtrue
 * @param[in] base Pointer to base with given building.
 * @sa B_UpdateOneBaseBuildingStatusOnDisable
 */
static void B_UpdateOneBaseBuildingStatusOnEnable (buildingType_t type, base_t* base)
{
	float level;

	switch (type) {
	case B_HOSPITAL:
		/* Reset all arrays . */
		memset(base->hospitalList, -1, sizeof(base->hospitalList));
		memset(base->hospitalListCount, 0, sizeof(base->hospitalListCount));
		memset(base->hospitalMissionList, -1, sizeof(base->hospitalMissionList));
		base->hospitalMissionListCount = 0;
		break;
	case B_RADAR:
		level = B_GetMaxBuildingLevel(base, B_RADAR);
		Radar_Initialise(&base->radar, baseRadarRange, level);
		break;
	default:
		break;
	}
}

/**
 * @brief Actions to perform when a type of buildings goes from qtrue to qfalse.
 * @param[in] type Type of building that has been modified from qtrue to qfalse
 * @param[in] base Pointer to base with given building.
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 */
static void B_UpdateOneBaseBuildingStatusOnDisable (buildingType_t type, base_t* base)
{
	switch (type) {
	case B_ALIEN_CONTAINMENT:
		/* if there an alien containment is not functional, aliens dies... */
		AC_KillAll(base);
		break;
	case B_RADAR:
		if (!base->hasBuilding[type])
			Radar_Initialise(&base->radar, 0.0f, 0.0f);
		break;
	default:
		break;
	}
}

/**
 * @brief Update status of every building when a building has been built/destroyed
 * @param[in] base
 * @param[in] building The building that has been built / removed
 * @param[in] onBuilt qtrue if building has been built, qfalse else
 * @sa B_BuildingDestroy
 * @sa B_UpdateAllBaseBuildingStatus
 * @return qtrue if at least one building status has been modified
 */
static qboolean B_UpdateStatusBuilding (base_t* base, buildingType_t type, qboolean onBuilt)
{
	qboolean test = qfalse;
	qboolean returnValue = qfalse;
	int i, buildingType;

	/* Construction / destruction may have changed the status of other building
	 * We check that, but only for buildings which needed building */
	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		buildingType = gd.buildings[base->idx][i].dependsBuilding;
		if (type == gd.buildingTypes[buildingType].buildingType) {
			/* gd.buildings[base->idx][i] needs built/removed building */
			if (onBuilt && !B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only activate a non operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnEnable(gd.buildings[base->idx][i].buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				} else
					B_UpdateOneBaseBuildingStatus(gd.buildings[base->idx][i].buildingType, base);
			} else if (!onBuilt && B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnDisable(gd.buildings[base->idx][i].buildingType, base);
					test = qtrue;
					returnValue = qtrue;
				} else
					B_UpdateOneBaseBuildingStatus(gd.buildings[base->idx][i].buildingType, base);
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
				} else
					B_UpdateOneBaseBuildingStatus(gd.buildings[base->idx][i].buildingType, base);
			} else if (!onBuilt && B_GetBuildingStatus(base, gd.buildings[base->idx][i].buildingType)) {
				/* we can only deactivate an operationnal building */
				if (B_CheckUpdateBuilding(&gd.buildings[base->idx][i], base)) {
					B_UpdateOneBaseBuildingStatusOnDisable(gd.buildings[base->idx][i].buildingType, base);
					test = qtrue;
				} else
					B_UpdateOneBaseBuildingStatus(gd.buildings[base->idx][i].buildingType, base);
			}
		}
	}

	return returnValue;
}

/**
 * @brief Recalculate status and capacities of one base
 * @param[in] base Pointer to the base where status and capacities must be recalculated
 * @param[in] firstEnable qtrue if this is the first time the function is called for this base
 * @sa B_UpdateOneBaseBuildingStatusOnEnable
 */
static void B_ResetAllStatusAndCapacities (base_t *base, qboolean firstEnable)
{
	int buildingIdx, i;
	qboolean test = qtrue;

	assert(base);

	Com_DPrintf(DEBUG_CLIENT, "Reseting base %s:\n", base->name);

	/* reset all values of hasBuilding[] */
	for (i = 0; i < MAX_BUILDING_TYPE; i++)
		base->hasBuilding[i] = qfalse;

	/* activate all buildings that needs to be activated */
	while (test) {
		test = qfalse;
		for (buildingIdx = 0; buildingIdx < gd.numBuildings[base->idx]; buildingIdx++) {
			if (!base->hasBuilding[gd.buildings[base->idx][buildingIdx].buildingType] && B_CheckUpdateBuilding(&gd.buildings[base->idx][buildingIdx], base)) {
				if (firstEnable)
					B_UpdateOneBaseBuildingStatusOnEnable(gd.buildings[base->idx][buildingIdx].buildingType, base);
				test = qtrue;
			} else
				B_UpdateOneBaseBuildingStatus(gd.buildings[base->idx][buildingIdx].buildingType, base);
		}
	}

	/* Update all capacities of base */
	B_UpdateBaseCapacities(MAX_CAP, base);
}

#ifdef DEBUG
/**
 * @brief Recalculate status and capacities
 * @note function called with debug_basereset or after loading
 */
static void B_ResetAllStatusAndCapacities_f (void)
{
	int baseIdx, i;
	base_t *base;

	for (baseIdx = 0; baseIdx < gd.numBases; baseIdx++){
		base = B_GetBase(baseIdx);
		if (base->founded) {
			/* set buildingStatus[] and capacities.max values */
			B_ResetAllStatusAndCapacities(base, qfalse);

			/* calculate capacities.cur for every capacity */
			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_ALIENS)])
				base->capacities[CAP_ALIENS].cur = AL_CountInBase(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_AIRCRAFTS_SMALL)] ||
				base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_AIRCRAFTS_BIG)])
				AIR_UpdateHangarCapForAll(baseIdx);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_EMPLOYEES)])
				base->capacities[CAP_EMPLOYEES].cur = E_CountAllHired(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_ITEMS)])
				INV_UpdateStorageCap(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_LABSPACE)])
				base->capacities[CAP_LABSPACE].cur = RS_CountInBase(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_WORKSPACE)])
				PR_UpdateProductionCap(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_HOSPSPACE)])
				HOS_UpdateHospitalCapForAll(base);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_UFOHANGARS_SMALL)] ||
				base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_UFOHANGARS_LARGE)])
				UFO_UpdateUFOHangarCapForAll(baseIdx);

			if (base->hasBuilding[B_GetBuildingTypeByCapacity(CAP_ANTIMATTER)])
				INV_UpdateAntimatterCap(base);

			/* Check that current capacity is possible -- if we changed values in *.ufo */
			for (i = 0; i < MAX_CAP; i++)
				if (base->capacities[i].cur > base->capacities[i].max)
					Com_Printf("B_ResetAllStatusAndCapacities_f: Warning, capacity of %i is bigger than maximum capacity\n", i);
		}
	}
}
#endif

/**
 * @brief Removes a building from the given base
 * @param[in] base Base to remove the building in
 * @param[in] building The building to remove
 * @note Also updates capacities and sets the hasBuilding[] values in base_t
 * @sa B_BuildingDestroy_f
 */
qboolean B_BuildingDestroy (base_t* base, building_t* building)
{
	buildingType_t type = building->buildingType;
	baseCapacities_t cap = MAX_CAP; /* init but don't set to first value of enum */
	char onDestroy[MAX_VAR];
	qboolean test;

	/* Don't allow to destroy an entrance. */
	if (type == B_ENTRANCE)
		return qfalse;

	Q_strncpyz(onDestroy, building->onDestroy, sizeof(onDestroy));

	switch (base->map[(int)building->pos[0]][(int)building->pos[1]]) {
	case BASE_FREESLOT:
	case BASE_INVALID_SPACE:
		assert(0);
		return qfalse;
	default:
		if (building->needs) {
			/* "Child" building is always right to the "parent" building". */
			base->map[(int)building->pos[0]][((int)building->pos[1]) + 1] = BASE_FREESLOT;
		}

		base->map[(int)building->pos[0]][(int)building->pos[1]] = BASE_FREESLOT;
		break;
	}
	building->buildingStatus = B_STATUS_NOT_SET;

	{
		building_t* const buildings = gd.buildings[base->idx];
		const int         cnt_bldgs = gd.numBuildings[base->idx] - 1;
		const int         idx       = building->idx;
		int               row;
		int               col;
		int               i;

		gd.numBuildings[base->idx] = cnt_bldgs;

		assert(idx <= cnt_bldgs);
		memmove(building, building + 1, (cnt_bldgs - idx) * sizeof(*building));
		/* wipe the now vacant last slot */
		memset(&buildings[cnt_bldgs], 0, sizeof(buildings[cnt_bldgs]));
		for (i = 0; i < cnt_bldgs; i++)
			if (buildings[i].idx >= idx)
				buildings[i].idx--;

		/* adjust the base map indices for the removed building */
		for (row = 0; row < BASE_SIZE; ++row) {
			for (col = 0; col < BASE_SIZE; ++col) {
				if (base->map[row][col] > idx) {
					--base->map[row][col];
				}
			}
		}
	}
	/* don't use the building pointer after this point - it's zeroed or points to a wrong entry now */

	test = qfalse;

	switch (type) {
	/* building has a capacity */
	case B_WORKSHOP:
	case B_STORAGE:
	case B_ALIEN_CONTAINMENT:
	case B_LAB:
	case B_HOSPITAL:
	case B_HANGAR: /* the Dropship Hangar */
	case B_SMALL_HANGAR:
	case B_COMMAND:
	case B_UFO_HANGAR:
	case B_UFO_SMALL_HANGAR:
		cap = B_GetCapacityFromBuildingType(type);
		/* no break, we want to update status below */
	/* building has no capacity */
	case B_POWER:
	case B_TEAMROOM:
	case B_QUARTERS:
	case B_DEFENSE_MISSILE:
	case B_DEFENSE_LASER:
	case B_RADAR:
		if (B_GetNumberOfBuildingsInBaseByType(base->idx, type) <= 0) {
			B_SetBuildingStatus(base, type, qfalse);
			test = qtrue;
		}
		B_UpdateOneBaseBuildingStatus(type, base);
		break;
	case B_ANTIMATTER:
		cap = CAP_ANTIMATTER;
		if (B_GetNumberOfBuildingsInBaseByType(base->idx, type) <= 0) {
			base->hasBuilding[B_ANTIMATTER] = qfalse;
			/* Remove antimatter. */
			INV_ManageAntimatter(base, 0, qfalse);
			test = qtrue;
		}
		break;
	case B_MISC:
		break;
	default:
		Com_Printf("B_BuildingDestroy: Unknown building type: %i.\n", type);
		break;
	}

	/* now, the destruction of this building may have changed the status of other building. */
	if (test) {
		B_UpdateStatusBuilding(base, type, qfalse);
		/* we may have changed status of several building: update all capacities */
		B_UpdateBaseCapacities(MAX_CAP, base);
	} else {
		/* no other status than status of destroyed building has been modified
		 * update only status of destroyed building */
		cap = B_GetCapacityFromBuildingType(building->buildingType);
		if (cap != MAX_CAP)
			B_UpdateBaseCapacities(cap, base);
	}

	/* call ondestroy trigger */
	if (*onDestroy) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingDestroy: %s %i;\n", onDestroy, base->idx);
		Cbuf_AddText(va("%s %i;", onDestroy, base->idx));
	}

	B_BaseInit_f();

	/* Remove aliens if needed. */
	if (type == B_ALIEN_CONTAINMENT) {
		if (!base->hasBuilding[B_ALIEN_CONTAINMENT]) {	/* Just clean containment. */
			AL_FillInContainment(base);
		} else {	/* Check capacities and remove needed amount. */
			if (base->capacities[CAP_ALIENS].cur - base->capacities[CAP_ALIENS].max > 0)
				AL_RemoveAliens(base, NULL, (base->capacities[CAP_ALIENS].cur - base->capacities[CAP_ALIENS].max), AL_RESEARCH);
		}
	}

	return qtrue;
}

/**
 * @brief We are doing the real destroy of a buliding here
 * @sa B_BuildingDestroy
 * @sa B_NewBuilding
 */
static void B_BuildingDestroy_f (void)
{
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	B_BuildingDestroy(baseCurrent, baseCurrent->buildingCurrent);

	B_ResetBuildingCurrent(baseCurrent);
}

/**
 * @brief Mark a building for destruction - you only have to confirm it now
 * @note Also calls the ondestroy trigger
 */
void B_MarkBuildingDestroy (base_t* base, building_t* building)
{
	assert(base);
	assert(building);

	base->buildingCurrent = building;

	MN_PushMenu("building_destroy");
}

/**
 * @brief Displays the status of a building for baseview.
 *
 * updates the cvar mn_building_status which is used in some menus to display
 * the building status
 * @note also script command function binding for 'building_status'
 */
void B_BuildingStatus (base_t* base, building_t* building)
{
	/* int daysLeft; */
	int numberOfBuildings = 0;

	assert(building);
	assert(base);

	/**
	 * @todo Was this planned to be used anywhere (e.g. for B_STATUS_UNDER_CONSTRUCTION text)
	 * or was it removed intentionally?
	 * daysLeft = building->timeStart + building->buildTime - ccs.date.day;
	*/

	Cvar_Set("mn_building_status", _("Not set"));

	switch (building->buildingStatus) {
	case B_STATUS_NOT_SET:
		numberOfBuildings = B_GetNumberOfBuildingsInBaseByTypeIDX(base->idx, building->type_idx);
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
 * @brief Console callback for B_BuildingStatus
 * @sa B_BuildingStatus
 */
static void B_BuildingStatus_f (void)
{
	/* maybe someone called this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	B_BuildingStatus(baseCurrent, baseCurrent->buildingCurrent);
}

/**
 * @brief  Hires some employees of appropriate type for a building
 * @param[in] building in which building
 * @param[in] num how many employees, if -1, hire building->maxEmployees
 * @sa B_SetUpBase
 */
static void B_HireForBuilding (base_t* base, building_t * building, int num)
{
	employeeType_t employeeType;

	assert(base);

	if (num < 0)
		num = building->maxEmployees;

	if (num) {
		switch (building->buildingType) {
		case B_WORKSHOP:
			employeeType = EMPL_WORKER;
			break;
		case B_LAB:
			employeeType = EMPL_SCIENTIST;
			break;
		case B_HOSPITAL:
			employeeType = EMPL_MEDIC;
			break;
		case B_HANGAR: /* the Dropship Hangar */
			employeeType = EMPL_SOLDIER;
			break;
		case B_MISC:
			Com_DPrintf(DEBUG_CLIENT, "B_HireForBuilding: Misc building type: %i with employees: %i.\n", building->buildingType, num);
			return;
		default:
			Com_DPrintf(DEBUG_CLIENT, "B_HireForBuilding: Unknown building type: %i.\n", building->buildingType);
			return;
		}
		/* don't try to hire more that available - see E_CreateEmployee */
		if (num > gd.numEmployees[employeeType])
			num = gd.numEmployees[employeeType];
		for (;num--;)
			if (!E_HireEmployeeByType(base, employeeType)) {
				Com_DPrintf(DEBUG_CLIENT, "B_HireForBuilding: Hiring %i employee(s) of type %i failed.\n", num, employeeType);
				return;
			}
	}
}

/**
 * @brief Updates base status for particular buildings as well as capacities.
 * @param[in] building Pointer to building.
 * @param[in] base Pointer to base with given building.
 * @param[in] status Enum of buildingStatus_t which is status of given building.
 * @note This function checks whether a building has B_STATUS_WORKING status, and
 * @note then updates base status for particular buildings and base capacities.
 */
static void B_UpdateAllBaseBuildingStatus (building_t* building, base_t* base, buildingStatus_t status)
{
	qboolean test;
	int cap;

	assert(base);
	assert(building);

	building->buildingStatus = status;

	/* we update the status of the building (we'll call this building building 1) */
	test = B_CheckUpdateBuilding(building, base);
	if (test)
		B_UpdateOneBaseBuildingStatusOnEnable(building->buildingType, base);
	else
		B_UpdateOneBaseBuildingStatus(building->buildingType, base);

	/* now, the status of this building may have changed the status of other building.
	 * We check that, but only for buildings which needed building 1 */
	if (test) {
		B_UpdateStatusBuilding(base, building->buildingType, qtrue);
		/* we may have changed status of several building: update all capacities */
		B_UpdateBaseCapacities(MAX_CAP, base);
	} else {
		/* no other status than status of building 1 has been modified
		 * update only status of building 1 */
		cap = B_GetCapacityFromBuildingType(building->buildingType);
		if (cap != MAX_CAP)
			B_UpdateBaseCapacities(cap, base);
	}

	/* @todo: this should be an user option defined in Game Options. */
	CL_GameTimeStop();
}

/**
 * @brief Setup new base
 * @sa CL_NewBase
 */
void B_SetUpBase (base_t* base)
{
	int i, j;
	building_t *building;
	const int newBaseAlienInterest = 1.0f;

	assert(base);
	/* Resent current capacities. */
	for (i = 0; i < MAX_CAP; i++)
		base->capacities[i].cur = 0;

	/* update the building-list */
	B_BuildingInit(base);
	Com_DPrintf(DEBUG_CLIENT, "Set up for %i\n", base->idx);

	/* this cvar is used for disabling the base build button on geoscape if MAX_BASES (8) was reached */
	Cvar_Set("mn_base_count", va("%i", mn_base_count->integer + 1));

	/* this cvar is needed by B_SetBuildingByClick below*/
	Cvar_SetValue("mn_base_id", base->idx);

	if (cl_start_buildings->integer) {
		for (i = 0; i < gd.numBuildingTypes; i++) {
			if (gd.buildingTypes[i].autobuild
		 	 || (gd.numBases == 1 && gd.buildingTypes[i].firstbase)) {
				/* @todo: implement check for moreThanOne */
				building = &gd.buildings[base->idx][gd.numBuildings[base->idx]];
				*building = gd.buildingTypes[i];
				/* self-link to building-list in base */
				building->idx = gd.numBuildings[base->idx];
				gd.numBuildings[base->idx]++;
				/* Link to the base. */
				building->base_idx = base->idx;
				Com_DPrintf(DEBUG_CLIENT, "Base %i new building:%s (%i) at (%.0f:%.0f)\n", base->idx, building->id, i, building->pos[0], building->pos[1]);
				base->buildingCurrent = building;
				/* fake a click to basemap */
				B_SetBuildingByClick((int) building->pos[0], (int) building->pos[1]);
				B_UpdateAllBaseBuildingStatus(building, base, B_STATUS_WORKING);

				/* A few more things for first base.
				 * @todo: this shouldn't be hard coded, but in *.ufo files */
				if (gd.numBases == 1) {
					/* build a second quarter */
					if (building->buildingType == B_QUARTERS) {
						building = &gd.buildings[base->idx][gd.numBuildings[base->idx]];
						*building = gd.buildingTypes[i];
						/* self-link to building-list in base */
						building->idx = gd.numBuildings[base->idx];
						gd.numBuildings[base->idx]++;
						/* Link to the base. */
						building->base_idx = base->idx;
						/* Build second quarter just above first one */
						Com_DPrintf(DEBUG_CLIENT, "Base %i new building:%s (%i) at (%.0f:%.0f)\n", base->idx, building->id, i, building->pos[0] - 1, building->pos[1]);
						base->buildingCurrent = building;
						/* fake a click to basemap */
						B_SetBuildingByClick((int) building->pos[0] - 1, (int) building->pos[1]);
						B_UpdateAllBaseBuildingStatus(building, base, B_STATUS_WORKING);
					}
				}

				/* now call the onconstruct trigger */
				if (*building->onConstruct) {
					base->buildingCurrent = building;
					Com_DPrintf(DEBUG_CLIENT, "B_SetUpBase: %s %i;\n", building->onConstruct, base->idx);
					Cbuf_AddText(va("%s %i;", building->onConstruct, base->idx));
				}

				/* update the building-list */
				B_BuildingInit(base);

				if (cl_start_employees->integer)
					B_HireForBuilding(base, building, -1);
			}
		}
	}
	/* if no autobuild, set up zero build time for the first base */
	if (gd.numBases == 1 && !cl_start_buildings->integer)
		ccs.instant_build = 1;

	/* Set up default buy/sell factors for this base. */
	base->sellfactor = 5;
	base->buyfactor = 1;
	/* the first base never has invalid fields */
	if (base->idx > 0) {
		j = (int) (frand() * 3 + 1.5f);
		for (i = 0; i < j; i++) {
			int *mapPtr = &base->map[rand() % BASE_SIZE][rand() % (BASE_SIZE - 1)];
			/* set this field to invalid if there is no building yet */
			if (*mapPtr == BASE_FREESLOT)
				*mapPtr = BASE_INVALID_SPACE;
		}
	}

	if (B_GetNumberOfBuildingsInBaseByType(base->idx, B_ENTRANCE)) {
		/* Set hasBuilding[B_ENTRANCE] to correct value, because it can't be updated afterwards.*/
		base->hasBuilding[B_ENTRANCE] = qtrue;
	} else {
		/* base can't start without an entrance, because this is where the alien will arrive during base attack */
		Com_Printf("B_SetUpBase()... A new base should have an entrance.\n");
	}

	/* Add aircraft to the first base */
	base->numAircraftInBase = 0;
	if (gd.numBases == 1) {
		aircraft_t *aircraft;
		/* buy two first aircraft */
		if (base->hasBuilding[B_HANGAR]) {
			aircraft = AIR_GetAircraft("craft_drop_firebird");
			if (!aircraft)
				Sys_Error("Could not find craft_drop_firebird definition");
			AIR_NewAircraft(base, "craft_drop_firebird");
			CL_UpdateCredits(ccs.credits - aircraft->price);
		}
		if (base->hasBuilding[B_SMALL_HANGAR]) {
			aircraft = AIR_GetAircraft("craft_inter_stiletto");
			if (!aircraft)
				Sys_Error("Could not find craft_inter_stiletto definition");
			AIR_NewAircraft(base, "craft_inter_stiletto");
			CL_UpdateCredits(ccs.credits - aircraft->price);
		}
	}

	/* a new base is not discovered (yet) */
	base->alienInterest = newBaseAlienInterest;

	/* intialise hit points */
	base->batteryDamage = MAX_BATTERY_DAMAGE;
	base->baseDamage = MAX_BASE_DAMAGE;
	BDEF_InitialiseBaseSlots(base);
}

/**
 * @brief Returns the building in the global building-types list that has the unique name buildingID.
 *
 * @param[in] buildingName The unique id of the building (building_t->id).
 *
 * @return building_t If a building was found it is returned, if no id was give the current building is returned, otherwise->NULL.
 */
building_t *B_GetBuildingType (const char *buildingName)
{
	int i = 0;

	assert(buildingName);
	for (i = 0; i < gd.numBuildingTypes; i++)
		if (!Q_strcasecmp(gd.buildingTypes[i].id, buildingName))
			return &gd.buildingTypes[i];

	Com_Printf("Building %s not found\n", buildingName);
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
 * @brief Builds new building.
 * @sa B_MarkBuildingDestroy
 * @sa B_CheckCredits
 * @sa CL_UpdateCredits
 * @return qboolean
 * @sa B_NewBuilding
 * Checks whether the player has enough credits to construct the current selected
 * building before starting construction.
 */
static qboolean B_ConstructBuilding (base_t* base)
{
	building_t *building_to_build = NULL;

	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !base->buildingCurrent)
		return qfalse;

	/* enough credits to build this? */
	if (!B_CheckCredits(base->buildingCurrent->fixCosts)) {
		Com_DPrintf(DEBUG_CLIENT, "B_ConstructBuilding: Not enough credits to build: '%s'\n", base->buildingCurrent->id);
		B_ResetBuildingCurrent(base);
		return qfalse;
	}

	Com_DPrintf(DEBUG_CLIENT, "Construction of %s is starting\n", base->buildingCurrent->id);

	/* second building part */
	if (base->buildingToBuild >= 0) {
		building_to_build = &gd.buildingTypes[base->buildingToBuild];
		building_to_build->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		base->buildingToBuild = -1;
	}
	if (!ccs.instant_build) {
		base->buildingCurrent->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		base->buildingCurrent->timeStart = ccs.date.day;
	} else {
		B_UpdateAllBaseBuildingStatus(base->buildingCurrent, base, B_STATUS_WORKING);
	}

	CL_UpdateCredits(ccs.credits - base->buildingCurrent->fixCosts);
	B_BaseInit_f();
	return qtrue;
}

/**
 * @brief Build new building.
 * @sa B_MarkBuildingDestroy
 * @sa B_ConstructBuilding
 */
static void B_NewBuilding (base_t* base)
{
	/* maybe someone call this command before the buildings are parsed?? */
	if (!base || !base->buildingCurrent)
		return;

	if (base->buildingCurrent->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		/* credits are updated in the construct function */
		if (B_ConstructBuilding(base)) {
			B_BuildingStatus(base, base->buildingCurrent);
			Com_DPrintf(DEBUG_CLIENT, "B_NewBuilding: buildingCurrent->buildingStatus = %i\n", base->buildingCurrent->buildingStatus);
		}
}

/**
 * @brief Set the currently selected building.
 *
 * @param[in] row Set building (baseCurrent->buildingCurrent) to row
 * @param[in] col Set building (baseCurrent->buildingCurrent) to col
 */
void B_SetBuildingByClick (int row, int col)
{
#ifdef DEBUG
	if (!baseCurrent)
		Sys_Error("no current base\n");
	if (!baseCurrent->buildingCurrent)
		Sys_Error("no current building\n");
#endif
	if (!B_CheckCredits(baseCurrent->buildingCurrent->fixCosts)) {
		MN_Popup(_("Notice"), _("Not enough credits to build this\n"));
		return;
	}

	/*@todo: this is bad style (baseCurrent->buildingCurrent shouldn't link to gd.buildingTypes at all ... it's just not logical) */
	/* if the building is in gd.buildingTypes[] */
	if (baseCurrent->buildingCurrent->base_idx < 0) {
		building_t *building = &gd.buildings[baseCurrent->idx][gd.numBuildings[baseCurrent->idx]];

		/* copy building from type-list to base-buildings-list */
		*building = gd.buildingTypes[baseCurrent->buildingCurrent->type_idx];

		/* self-link to building-list in base */
		building->idx = gd.numBuildings[baseCurrent->idx];
		gd.numBuildings[baseCurrent->idx]++;

		/* Link to the base. */
		building->base_idx = baseCurrent->idx;
		baseCurrent->buildingCurrent = building;
	}

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		switch (baseCurrent->map[row][col]) {
		case BASE_FREESLOT:
			if (baseCurrent->buildingCurrent->needs) {
				building_t *secondBuildingPart = B_GetBuildingType(baseCurrent->buildingCurrent->needs);

				if (col + 1 == BASE_SIZE) {
					if (baseCurrent->map[row][col - 1] != BASE_FREESLOT) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return;
					}
					col--;
				} else if (baseCurrent->map[row][col + 1] != BASE_FREESLOT) {
					if (baseCurrent->map[row][col - 1] != BASE_FREESLOT || !col) {
						Com_DPrintf(DEBUG_CLIENT, "Can't place this building here - the second part overlapped with another building or invalid field\n");
						return;
					}
					col--;
				}

				baseCurrent->map[row][col + 1] = baseCurrent->buildingCurrent->idx;
				baseCurrent->buildingToBuild = secondBuildingPart->idx;
				/* where is this building located in our base? */
				secondBuildingPart->pos[1] = col + 1;
				secondBuildingPart->pos[0] = row;
			} else {
				baseCurrent->buildingToBuild = -1;
			}
			/* credits are updated here, too */
			B_NewBuilding(baseCurrent);

			baseCurrent->map[row][col] = baseCurrent->buildingCurrent->idx;

			/* where is this building located in our base? */
			baseCurrent->buildingCurrent->pos[0] = row;
			baseCurrent->buildingCurrent->pos[1] = col;

			B_ResetBuildingCurrent(baseCurrent);
			B_BuildingInit(baseCurrent);	/* update the building-list */
			break;
		case BASE_INVALID_SPACE:
			Com_DPrintf(DEBUG_CLIENT, "This base field is marked as invalid - you can't bulid here\n");
			break;
		default:
			Com_DPrintf(DEBUG_CLIENT, "There is already a building\n");
			Com_DPrintf(DEBUG_CLIENT, "Building: %i at (row:%i, col:%i)\n", baseCurrent->map[row][col], row, col);
		}
	} else
		Com_DPrintf(DEBUG_CLIENT, "Invalid coordinates\n");
}

/**
 * @brief Places the current building in the base (x/y give via console).
 */
static void B_SetBuilding_f (void)
{
	int row, col;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <x> <y>\n", Cmd_Argv(0));
		return;
	}

	/* maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	row = atoi(Cmd_Argv(1));
	col = atoi(Cmd_Argv(2));

	/* emulate the mouseclick with the given coordinates */
	B_SetBuildingByClick(row, col);
}

/**
 * @brief Build building from the list of those available.
 */
static void B_NewBuildingFromList_f (void)
{
	/* maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (baseCurrent->buildingCurrent->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		B_NewBuilding(baseCurrent);
}

/**
 * @brief Draws a building.
 */
static void B_DrawBuilding (base_t* base, building_t* building)
{
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

	if (building->dependsBuilding >= 0)
		Q_strcat(buildingText, va(_("Needs:\t%s\n"), _(gd.buildingTypes[building->dependsBuilding].name)), sizeof(buildingText));

	if (building->name)
		Cvar_Set("mn_building_name", _(building->name));

	if (building->image)
		Cvar_Set("mn_building_image", building->image);
	else
		Cvar_Set("mn_building_image", "base/empty");

	/* link into menu text array */
	mn.menuText[TEXT_BUILDING_INFO] = buildingText;
}

/**
 * @brief Handles the list of constructable buildings.
 *
 * @param[in] building Add this building to the construction list
 * Called everytime a building was constructed and thus maybe other buildings get available.
 * mn.menuText[TEXT_BUILDINGS] is a pointer to baseCurrent->allBuildingsList which will be displayed in the build-screen.
 * This way every base can hold its own building list.
 * The content is updated everytime B_BuildingInit is called (i.e everytime the buildings-list is dispplayed/updated)
 */
static void B_BuildingAddToList (building_t * building)
{
	assert(baseCurrent);
	assert(building->name);

	Q_strcat(baseCurrent->allBuildingsList, va("%s\n", _(building->name)), MAX_LIST_CHAR);
	BuildingConstructionList[numBuildingConstructionList] = building->type_idx;
	numBuildingConstructionList++;
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 *
 * @param[in] base_idx Which base
 * @param[in] type_idx Which buildingtype
 * @sa B_GetNumberOfBuildingsInBaseByType
 * @return The number of buildings.
 * @return -1 on error (e.g. base index out of range)
 * @todo type index sanity check?
 */
int B_GetNumberOfBuildingsInBaseByTypeIDX (int base_idx, int type_idx)
{
	int i;
	int NumberOfBuildings = 0;

	if (base_idx < 0 || base_idx >= gd.numBases) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByTypeIDX: Bad base-index given: %i (numbases %i)\n", base_idx, gd.numBases);
		return -1;
	}

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		if (gd.buildings[base_idx][i].type_idx == type_idx
		 && gd.buildings[base_idx][i].buildingStatus != B_STATUS_NOT_SET)
			NumberOfBuildings++;
	}
	Com_DPrintf(DEBUG_CLIENT, "B_GetNumOfBuildType: base: '%s' - num_b: %i - type_idx: %s\n", gd.bases[base_idx].name, NumberOfBuildings, gd.buildingTypes[type_idx].id);
	return NumberOfBuildings;
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 *
 * @param[in] base_idx Which base
 * @param[in] type Building type value
 * @sa B_GetNumberOfBuildingsInBaseByTypeIDX
 * @return The number of buildings.
 * @return -1 on error (e.g. base index out of range)
 */
int B_GetNumberOfBuildingsInBaseByType (int base_idx, buildingType_t type)
{
	int i;
	int NumberOfBuildings = 0;

	if (base_idx < 0 || base_idx >= gd.numBases) {
		Com_Printf("B_GetNumberOfBuildingsInBaseByType: Bad base-index given: %i (numbases: %i)\n", base_idx, gd.numBases);
		return -1;
	}

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		if (gd.buildings[base_idx][i].buildingType == type
		 && gd.buildings[base_idx][i].buildingStatus != B_STATUS_NOT_SET)
			NumberOfBuildings++;
	}
	return NumberOfBuildings;
}

/**
 * @brief Update the building-list.
 * @sa B_BuildingInit_f
 */
static void B_BuildingInit (base_t* base)
{
	int i;
	int numSameBuildings;
	building_t *buildingType;

	/* maybe someone call this command before the bases are parsed?? */
	if (!base)
		return;

	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Updating b-list for '%s' (%i)\n", base->name, base->idx);
	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Buildings in base: %i\n", gd.numBuildings[base->idx]);

	/* initialising the vars used in B_BuildingAddToList */
	memset(base->allBuildingsList, 0, sizeof(base->allBuildingsList));
	mn.menuText[TEXT_BUILDINGS] = base->allBuildingsList;
	numBuildingConstructionList = 0;

	for (i = 0; i < gd.numBuildingTypes; i++) {
		buildingType = &gd.buildingTypes[i];
		/*make an entry in list for this building */

		if (buildingType->visible) {
			numSameBuildings = B_GetNumberOfBuildingsInBaseByTypeIDX(base->idx, buildingType->type_idx);

			if (buildingType->moreThanOne) {
				/* skip if limit of BASE_SIZE*BASE_SIZE exceeded */
				if (numSameBuildings >= BASE_SIZE * BASE_SIZE)
					continue;
			} else if (numSameBuildings > 0) {
				continue;
			}

			/* if the building is researched add it to the list */
			if (RS_IsResearched_ptr(buildingType->tech)) {
				B_BuildingAddToList(buildingType);
			} else {
				Com_DPrintf(DEBUG_CLIENT, "Building not researched yet %s (tech idx: %i)\n",
					buildingType->id, buildingType->tech ? buildingType->tech->idx : 0);
			}
		}
	}
	if (base->buildingCurrent)
		B_DrawBuilding(base, base->buildingCurrent);
}

/**
 * @brief Script command binding for B_BuildingInit
 */
static void B_BuildingInit_f (void)
{
	if (!baseCurrent)
		return;
	B_BuildingInit(baseCurrent);
}

/**
 * @brief Gets the type of building by its index.
 *
 * @param[in] base Pointer to base_t (base has to be founded already)
 * @param[in] idx The index of the building in gd.buildings[]
 * @return buildings_t pointer to gd.buildings[idx]
 */
building_t *B_GetBuildingByIdx (base_t* base, int idx)
{
	assert(base);
	assert(idx > BASE_FREESLOT);
	return &gd.buildings[base->idx][idx];
}

/**
 * @brief Opens up the 'pedia if you right click on a building in the list.
 *
 * @todo Really only do this on rightclick.
 * @todo Left click should show building-status.
 */
static void B_BuildingInfoClick_f (void)
{
	if (baseCurrent && baseCurrent->buildingCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingInfoClick_f: %s - %i\n", baseCurrent->buildingCurrent->id, baseCurrent->buildingCurrent->buildingStatus);
		UP_OpenWith(baseCurrent->buildingCurrent->pedia);
	}
}

/**
 * @brief Script function for clicking the building list text field.
 */
static void B_BuildingClick_f (void)
{
	int num;
	building_t *building;

	if (Cmd_Argc() < 2 || !baseCurrent) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which building? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: listnumber %i base %i\n", num, baseCurrent->idx);

	if (num > numBuildingConstructionList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: max exceeded %i/%i\n", num, numBuildingConstructionList);
		return;
	}

	building = &gd.buildingTypes[BuildingConstructionList[num]];

	baseCurrent->buildingCurrent = building;
	B_DrawBuilding(baseCurrent, building);

	gd.baseAction = BA_NEWBUILDING;
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
		return B_DEFENSE_MISSILE;
	} else if (!Q_strncmp(buildingID, "radar", MAX_VAR)) {
		return B_RADAR;
	} else if (!Q_strncmp(buildingID, "teamroom", MAX_VAR)) {
		return B_TEAMROOM;
	}
	return MAX_BUILDING_TYPE;
}

/**
 * @brief Copies an entry from the building description file into the list of building types.
 *
 * Parses one "building" entry in the basemanagement.ufo file and writes it into the next free entry in bmBuildings[0], which is the list of buildings in the first base (building_t).
 *
 * @param[in] id Unique test-id of a building_t. This is parsed from "building xxx" -> id=xxx.
 * @param[in] text @todo: document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
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
	if (gd.numBuildingTypes >= MAX_BUILDINGS) {
		Com_Printf("B_ParseBuildings: too many buildings\n");
		gd.numBuildingTypes = MAX_BUILDINGS;	/* just in case it's bigger. */
		return;
	}

	if (!link) {
		for (i = 0; i < gd.numBuildingTypes; i++) {
			if (!Q_strcmp(gd.buildingTypes[i].id, name)) {
				Com_Printf("B_ParseBuildings: Second buliding with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* new entry */
		building = &gd.buildingTypes[gd.numBuildingTypes];
		memset(building, 0, sizeof(building_t));
		building->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

		Com_DPrintf(DEBUG_CLIENT, "...found building %s\n", building->id);

		/*set standard values */
		building->type_idx = gd.numBuildingTypes;
		building->idx = -1;
		building->base_idx = -1;
		building->dependsBuilding = -1;
		building->visible = qtrue;

		gd.numBuildingTypes++;
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
				if (building->buildingType == MAX_BUILDING_TYPE)
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
							case V_TRANSLATION_MANUAL_STRING:
								token++;
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)building + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
								break;
							default:
								Com_ParseValue(building, token, vp->type, vp->ofs, vp->size);
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
		building = B_GetBuildingType(name);
		if (!building)			/* i'm paranoid */
			Sys_Error("B_ParseBuildings: Could not find building with id %s\n", name);

		tech_link = RS_GetTechByProvided(name);
		if (tech_link) {
			building->tech = tech_link;
		} else {
			if (building->visible)
				/* @todo: are the techs already parsed? */
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
				dependsBuilding = B_GetBuildingType(COM_EParse(text, errhead, name));
				if (!dependsBuilding)
					Sys_Error("Could not find building depend of %s\n", building->id);
				building->dependsBuilding = dependsBuilding->type_idx;
				if (!*text)
					return;
			}
		} while (*text);
	}
}

/**
 * @brief Gets a building of a given type in the given base
 * @param[in] base The base to search the building in
 * @return The building or NULL if base has no building of that type
 */
building_t *B_GetBuildingInBaseByType (const base_t* base, buildingType_t type, qboolean onlyWorking)
{
	int i;
	building_t *building;

	/* we maybe only want to get the working building (e.g. it might the
	 * case that we don't have a powerplant and thus the searched building
	 * is not functional) */
	if (onlyWorking && !base->hasBuilding[type])
		return NULL;

	for (i = 0; i < gd.numBuildings[base->idx]; i++) {
		building = &gd.buildings[base->idx][i];
		if (building->buildingType == type)
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
 * @brief Clears a base with all its characters
 * @sa CL_ResetCharacters
 * @sa CL_GenerateCharacter
 */
void B_ClearBase (base_t *const base)
{
	int i;

	CL_ResetCharacters(base);

	memset(base, 0, sizeof(base_t));

	/* only go further if we have a active campaign */
	if (!curCampaign)
		return;

	/* setup team
	 * FIXME: I think this should be made only once per game, not once per base, no ? -- Kracken 19/12/07 */
	if (!E_CountUnhired(EMPL_SOLDIER)) {
		/* should be multiplayer (campaignmode @todo) or singleplayer */
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i soldiers\n", curCampaign->soldiers);
		for (i = 0; i < curCampaign->soldiers; i++)
			E_CreateEmployee(EMPL_SOLDIER, B_RandomNation(), NULL);
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i scientists\n", curCampaign->scientists);
		for (i = 0; i < curCampaign->scientists; i++)
			E_CreateEmployee(EMPL_SCIENTIST, B_RandomNation(), NULL);
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i robots\n", curCampaign->ugvs);
		for (i = 0; i < curCampaign->ugvs; i++) {
			if (frand() > 0.5)
				E_CreateEmployee(EMPL_ROBOT, B_RandomNation(), CL_GetUgvById("ugv_ares_w"));
			else
				E_CreateEmployee(EMPL_ROBOT, B_RandomNation(), CL_GetUgvById("ugv_phoenix"));
		}
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i workers\n", curCampaign->workers);
		for (i = 0; i < curCampaign->workers; i++)
			E_CreateEmployee(EMPL_WORKER, B_RandomNation(), NULL);
		Com_DPrintf(DEBUG_CLIENT, "B_ClearBase: create %i medics\n", curCampaign->medics);
		for (i = 0; i < curCampaign->medics; i++)
			E_CreateEmployee(EMPL_MEDIC, B_RandomNation(), NULL);
	}

	memset(base->map, BASE_FREESLOT, sizeof(base->map));
}

/**
 * @brief Reads information about bases.
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void B_ParseBases (const char *name, const char **text)
{
	const char *errhead = "B_ParseBases: unexpected end of file (names ";
	const char *token;
	base_t *base;

	gd.numBaseNames = 0;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBases: base \"%s\" without body ignored\n", name);
		return;
	}
	do {
		/* add base */
		if (gd.numBaseNames > MAX_BASES) {
			Com_Printf("B_ParseBases: too many bases\n");
			return;
		}

		/* get the name */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		base = B_GetBase(gd.numBaseNames);
		memset(base, 0, sizeof(base_t));
		base->idx = gd.numBaseNames;
		base->buildingToBuild = -1;
		memset(base->map, BASE_FREESLOT, sizeof(base->map));

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
		gd.numBaseNames++; /* FIXME: Use this value instead of MAX_BASES in the for loops */
	} while (*text);

	mn_base_title = Cvar_Get("mn_base_title", "", 0, NULL);
}

/**
 * @brief Draw a small square with the menu layout of the given base
 */
void MN_BaseMapLayout (const menuNode_t * node)
{
	base_t *base;
	int height, width, x, y;
	int row, col;
	const vec4_t c_gray = {0.5, 0.5, 0.5, 1.0};
	vec2_t size;

	if (node->num >= MAX_BASES || node->num < 0)
		return;

	height = node->size[1] / BASE_SIZE;
	width = node->size[0] / BASE_SIZE;

	Vector2Copy(node->size, size);
	size[0] += (BASE_SIZE + 1) * node->padding;
	size[1] += (BASE_SIZE + 1) * node->padding;
	R_DrawFill(node->pos[0], node->pos[1], size[0], size[1], node->align, node->bgcolor);

	base = B_GetBase(node->num);

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			x = node->pos[0] + (width * col + node->padding * (col + 1));
			y = node->pos[1] + (height * row + node->padding * (row + 1));
			switch (base->map[row][col]) {
			case BASE_FREESLOT:
				break;
			case BASE_INVALID_SPACE:
				R_DrawFill(x, y, width, height, node->align, c_gray);
				break;
			default:
				/* maybe destroyed in the meantime */
				if (base->founded)
					R_DrawFill(x, y, width, height, node->align, node->color);
				break;
			}
		}
	}
}

/**
 * @brief Draws a base.
 * @sa MN_DrawMenus
 */
void MN_BaseMapDraw (const menuNode_t * node)
{
	int x, y, xHover = -1, yHover = -1, widthHover = 1;
	int width, height, row, col, time, colSecond;
	static vec4_t color = { 0.5f, 1.0f, 0.5f, 1.0 };
	char image[MAX_QPATH];
	building_t *building, *secondBuilding = NULL, *hoverBuilding = NULL;

	if (!baseCurrent) {
		MN_PopMenu(qfalse);
		return;
	}

	width = node->size[0] / BASE_SIZE;
	height = (node->size[1] + BASE_SIZE * 20) / BASE_SIZE;

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			/* 20 is the height of the part where the images overlap */
			x = node->pos[0] + col * width;
			y = node->pos[1] + row * height - row * 20;

			baseCurrent->posX[row][col] = x;
			baseCurrent->posY[row][col] = y;

			switch (baseCurrent->map[row][col]) {
			case BASE_INVALID_SPACE:
				building = NULL;
				Q_strncpyz(image, "base/invalid", sizeof(image));
				break;
			case BASE_FREESLOT:
				building = NULL;
				Q_strncpyz(image, "base/grid", sizeof(image));
				break;
			default:
				building = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
				secondBuilding = NULL;

				if (!building)
					Sys_Error("Error in DrawBase - no building with id %i\n", baseCurrent->map[row][col]);

				if (!building->used) {
					if (building->needs)
						building->used = 1;
					if (building->image) {	/* @todo:DEBUG */
						Q_strncpyz(image, building->image, sizeof(image));
					} else {
						/*Com_DPrintf(DEBUG_CLIENT, "B_DrawBase: no image found for building %s / %i\n",building->id ,building->idx); */
					}
				} else if (building->needs) {
					secondBuilding = B_GetBuildingType(building->needs);
					if (!secondBuilding)
						Sys_Error("Error in ufo-scriptfile - could not find the needed building\n");
					Q_strncpyz(image, secondBuilding->image, sizeof(image));
					building->used = 0;
				}
				break;
			}

			if (*image)
				R_DrawNormPic(x, y, width, height, 0, 0, 0, 0, 0, qfalse, image);

			/* check for hovering building name or outline border */
			if (mousePosX > x && mousePosX < x + width && mousePosY > y && mousePosY < y + height - 20) {
				switch (baseCurrent->map[row][col]) {
				case BASE_FREESLOT:
					if (gd.baseAction == BA_NEWBUILDING && xHover == -1) {
						assert(baseCurrent->buildingCurrent);
						colSecond = col;
						if (baseCurrent->buildingCurrent->needs) {
							if (colSecond + 1 == BASE_SIZE) {
								if (baseCurrent->map[row][colSecond - 1] == BASE_FREESLOT)
									colSecond--;
							} else if (baseCurrent->map[row][colSecond + 1] != BASE_FREESLOT) {
								if (baseCurrent->map[row][colSecond - 1] == BASE_FREESLOT)
									colSecond--;
							} else {
								colSecond++;
							}
							if (colSecond != col) {
								if (colSecond < col)
									xHover = node->pos[0] + colSecond * width;
								else
									xHover = x;
								widthHover = 2;
							}
						} else
							xHover = x;
						yHover = y;
					}
					break;
				default:
					hoverBuilding = building;
					break;
				}
			}

			/* only draw for first part of building */
			if (building && !secondBuilding) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
					time = building->buildTime - (ccs.date.day - building->timeStart);
					R_FontDrawString("f_small", 0, x + 10, y + 10, x + 10, y + 10, node->size[0], 0, node->texh[0], va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse);
					break;
				default:
					break;
				}
			}
		}
	}
	if (hoverBuilding) {
		R_Color(color);
		R_FontDrawString("f_small", 0, mousePosX + 3, mousePosY, mousePosX + 3, mousePosY, node->size[0], 0, node->texh[0], _(hoverBuilding->name), 0, 0, NULL, qfalse);
		R_Color(NULL);
	}
	if (xHover != -1) {
		if (widthHover == 1) {
			Q_strncpyz(image, "base/hover", sizeof(image));
			R_DrawNormPic(xHover, yHover, width, height, 0, 0, 0, 0, 0, qfalse, image);
		} else {
			Com_sprintf(image, sizeof(image), "base/hover%i", widthHover);
			R_DrawNormPic(xHover, yHover, width * widthHover, height, 0, 0, 0, 0, 0, qfalse, image);
		}
	}
}

/**
 * @brief Renames a base.
 */
static void B_RenameBase_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	if (baseCurrent)
		Q_strncpyz(baseCurrent->name, Cmd_Argv(1), sizeof(baseCurrent->name));
}

/**
 * @brief Cycles to the next base.
 * @sa B_PrevBase
 * @sa B_SelectBase_f
 */
static void B_NextBase_f (void)
{
	int baseID;

	if (!mn_base_id)
		return;

	baseID = mn_base_id->integer;

	Com_DPrintf(DEBUG_CLIENT, "cur-base=%i num-base=%i\n", baseID, gd.numBases);
	if (baseID < gd.numBases - 1)
		baseID++;
	else
		baseID = 0;
	Com_DPrintf(DEBUG_CLIENT, "new-base=%i\n", baseID);
	if (!gd.bases[baseID].founded)
		return;
	else
		Cmd_ExecuteString(va("mn_select_base %i", baseID));
}

/**
 * @brief Cycles to the previous base.
 * @sa B_NextBase
 * @sa B_SelectBase_f
 */
static void B_PrevBase_f (void)
{
	int baseID;

	if (!mn_base_id)
		return;

	baseID = mn_base_id->integer;

	Com_DPrintf(DEBUG_CLIENT, "cur-base=%i num-base=%i\n", baseID, gd.numBases);
	if (baseID > 0)
		baseID--;
	else
		baseID = gd.numBases - 1;
	Com_DPrintf(DEBUG_CLIENT, "new-base=%i\n", baseID);

	/* this must be false - but i'm paranoid' */
	if (!gd.bases[baseID].founded)
		return;
	else
		Cmd_ExecuteString(va("mn_select_base %i", baseID));
}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 *
 * For a new base the baseID is -1.
 */
static void B_SelectBase_f (void)
{
	int baseID;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}
	baseID = atoi(Cmd_Argv(1));

	/* set up a new base */
	/* called from *.ufo with -1 */
	if (baseID < 0) {
		gd.mapAction = MA_NEWBASE;
		baseID = gd.numBases;
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: new baseID is %i\n", baseID);
		if (baseID < MAX_BASES) {
			baseCurrent = B_GetBase(baseID);
			baseCurrent->idx = baseID;
			Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: baseID is valid for base: %s\n", baseCurrent->name);
			Cmd_ExecuteString("set_base_to_normal");
		} else {
			Com_Printf("MaxBases reached\n");
			/* select the first base in list */
			baseCurrent = B_GetBase(0);
			gd.mapAction = MA_NONE;
		}
	} else if (baseID < MAX_BASES) {
		Com_DPrintf(DEBUG_CLIENT, "B_SelectBase_f: select base with id %i\n", baseID);
		base = B_GetBase(baseID);
		if (base->founded) {
			baseCurrent = base;
			gd.mapAction = MA_NONE;
			MN_PushMenu("bases");
			AIR_AircraftSelect(NULL);
			switch (baseCurrent->baseStatus) {
			case BASE_UNDER_ATTACK:
				Cvar_Set("mn_base_status_name", _("Base is under attack"));
				Cmd_ExecuteString("set_base_under_attack");
				break;
			default:
				Cmd_ExecuteString("set_base_to_normal");
				break;
			}
		}
	} else
		return;

	/**
	 * this is only needed when we are going to be show up the base
	 * in our base view port
	 */
	if (gd.mapAction != MA_NEWBASE) {
		assert(baseCurrent);
		Cvar_SetValue("mn_base_id", baseCurrent->idx);
		Cvar_Set("mn_base_title", baseCurrent->name);
		Cvar_SetValue("mn_numbases", gd.numBases);
		Cvar_SetValue("mn_base_status_id", baseCurrent->baseStatus);
	}
}

#undef RIGHT
#undef HOLSTER
#define RIGHT(e) ((e)->inv->c[csi.idRight])
#define HOLSTER(e) ((e)->inv->c[csi.idHolster])

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
	int j, i1, i2, skill, no1, no2;
	character_t *cp1, *cp2;
	int weaponr_fd_idx, weaponh_fd_idx;
	const byte fmode1 = 0;
	const byte fmode2 = 1;

	j = team->num;
	while (j--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (i1 = 0; i1 < team->num - 1; i1++) {
				cp1 = team->chr[i1];
				weaponr_fd_idx = -1;
				weaponh_fd_idx = -1;
				if (RIGHT(cp1) && RIGHT(cp1)->item.m != NONE && RIGHT(cp1)->item.t != NONE)
					weaponr_fd_idx = FIRESH_FiredefsIDXForWeapon(&csi.ods[RIGHT(cp1)->item.m], RIGHT(cp1)->item.t);
				if (HOLSTER(cp1) && HOLSTER(cp1)->item.m != NONE && HOLSTER(cp1)->item.t != NONE)
					weaponh_fd_idx = FIRESH_FiredefsIDXForWeapon(&csi.ods[HOLSTER(cp1)->item.m], HOLSTER(cp1)->item.t);
				/* disregard left hand, or dual-wielding guys are too good */

				if (weaponr_fd_idx < 0 || weaponh_fd_idx < 0) {
					/* @todo: Is there a better way to check for this case? */
					Com_DPrintf(DEBUG_CLIENT, "CL_SwapSkills: Bad or no firedef indices found (weaponr_fd_idx=%i and weaponh_fd_idx=%i)... skipping\n", weaponr_fd_idx, weaponh_fd_idx);
				} else {
					no1 = 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[weaponr_fd_idx][fmode1].weaponSkill)
						+ 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[weaponr_fd_idx][fmode2].weaponSkill)
						+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[weaponh_fd_idx][fmode1].weaponSkill)
						+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[weaponh_fd_idx][fmode2].weaponSkill);

					for (i2 = i1 + 1; i2 < team->num; i2++) {
						cp2 = team->chr[i2];
						weaponr_fd_idx = -1;
						weaponh_fd_idx = -1;

						if (RIGHT(cp2) && RIGHT(cp2)->item.m != NONE && RIGHT(cp2)->item.t != NONE)
							weaponr_fd_idx = FIRESH_FiredefsIDXForWeapon(&csi.ods[RIGHT(cp2)->item.m], RIGHT(cp2)->item.t);
						if (HOLSTER(cp2) && HOLSTER(cp2)->item.m != NONE && HOLSTER(cp2)->item.t != NONE)
							weaponh_fd_idx = FIRESH_FiredefsIDXForWeapon(&csi.ods[HOLSTER(cp2)->item.m], HOLSTER(cp2)->item.t);

						if (weaponr_fd_idx < 0 || weaponh_fd_idx < 0) {
							/* @todo: Is there a better way to check for this case? */
							Com_DPrintf(DEBUG_CLIENT, "CL_SwapSkills: Bad or no firedef indices found (weaponr_fd_idx=%i and weaponh_fd_idx=%i)... skipping\n", weaponr_fd_idx, weaponh_fd_idx);
						} else {
							no2 = 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[weaponr_fd_idx][fmode1].weaponSkill)
								+ 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[weaponr_fd_idx][fmode2].weaponSkill)
								+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
								   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[weaponh_fd_idx][fmode1].weaponSkill)
								+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
								   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[weaponh_fd_idx][fmode2].weaponSkill);

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
 * @brief Assigns initial team of soldiers with equipment to aircraft
 * @note If assign_initial is called with one parameter (e.g. a 1), this is for
 * multiplayer - assign_initial with no parameters is for singleplayer	aircraft_t *aircraft = NULL;
 */
static void B_AssignInitial_f (void)
{
	int i;
	aircraft_t *aircraft;

	if (!baseCurrent) {
		if (ccs.singleplayer)
			return;
		aircraft = AIR_AircraftGetFromIdx(0);
		assert(aircraft);
		baseCurrent = aircraft->homebase;
	}
	assert(baseCurrent);

	if (!ccs.singleplayer) {
		CL_ResetMultiplayerTeamInBase();
		Cvar_Set("mn_teamname", _("NewTeam"));
	}

	for (i = MAX_TEAMLIST; --i >= 0;)
		Cbuf_AddText(va("team_hire %i;", i));

	Cbuf_AddText("pack_initial;");
	if (!ccs.singleplayer)
		MN_PushMenu("team");
}

/**
 * @brief Assigns initial soldier equipment for the first base
 */
static void B_PackInitialEquipment_f (void)
{
	int i, p, container, price = 0;
	equipDef_t *ed;
	base_t *base = baseCurrent;
	int aircraft_idxInBase;
	aircraft_t *aircraft;
	const char *name = curCampaign ? cl_initial_equipment->string : Cvar_VariableString("cl_equip");
	chrList_t chr_list_temp;

	/* check syntax */
	if (Cmd_Argc() > 1) {
		Com_Printf("Usage: %s\n", Cmd_Argv(0));
		return;
	}

	if (!base)
		return;
	aircraft_idxInBase = base->aircraftCurrent;

	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(name, ed->name, MAX_VAR))
			break;

	if (i == csi.numEDs) {
		Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment_f: Initial Phalanx equipment %s not found.\n", name);
	} else if ((aircraft_idxInBase != AIRCRAFT_INVALID) && (aircraft_idxInBase < base->numAircraftInBase)) {
		aircraft = &base->aircraft[base->aircraftCurrent];
		chr_list_temp.num = 0;
		for (i = 0; i < aircraft->maxTeamSize; i++) {
			if (aircraft->teamIdxs[i] != -1) {
				character_t *chr = &gd.employees[aircraft->teamTypes[i]][aircraft->teamIdxs[i]].chr;
				/* pack equipment */
				Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment_f: Packing initial equipment for %s.\n", chr->name);
				INVSH_EquipActor(chr->inv, ed->num, MAX_OBJDEFS, name, chr);
				Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment_f: armour: %i, weapons: %i\n", chr->armour, chr->weapons);
				chr_list_temp.chr[chr_list_temp.num] = chr;
				chr_list_temp.num++;
			}
		}
		CL_AddCarriedToEq(aircraft, &base->storage);
		INV_UpdateStorageCap(base);
		CL_SwapSkills(&chr_list_temp);

		/* pay for the initial equipment */
		for (container = 0; container < csi.numIDs; container++) {
			for (p = 0; p < aircraft->maxTeamSize; p++) {
				if (aircraft->teamIdxs[p] != -1) {
					const invList_t *ic;
					const character_t *chr = &gd.employees[aircraft->teamTypes[i]][aircraft->teamIdxs[i]].chr;
					for (ic = chr->inv->c[container]; ic; ic = ic->next) {
						const item_t item = ic->item;
						price += csi.ods[item.t].price;
						Com_DPrintf(DEBUG_CLIENT, "B_PackInitialEquipment_f()... adding price for %s, price: %i\n", csi.ods[item.t].id, price);
					}
				}
			}
		}
		CL_UpdateCredits(ccs.credits - price);
	}
}

/**
 * @brief Constructs a new base.
 * @sa CL_NewBase
 */
static void B_BuildBase_f (void)
{
	const nation_t *nation;

	if (!baseCurrent)
		return;

	assert(!baseCurrent->founded);
	assert(ccs.singleplayer);

	if (ccs.credits - BASE_COSTS > 0) {
		if (CL_NewBase(baseCurrent, newBasePos)) {
			Com_DPrintf(DEBUG_CLIENT, "B_BuildBase_f: numBases: %i\n", gd.numBases);
			baseCurrent->idx = gd.numBases - 1;
			baseCurrent->founded = qtrue;
			baseCurrent->baseStatus = BASE_WORKING;
			campaignStats.basesBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - BASE_COSTS);
			Q_strncpyz(baseCurrent->name, mn_base_title->string, sizeof(baseCurrent->name));
			nation = MAP_GetNation(baseCurrent->pos);
			if (nation)
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new base has been built: %s (nation: %s)"), mn_base_title->string, _(nation->name));
			else
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("A new base has been built: %s"), mn_base_title->string);
			MN_AddNewMessage(_("Base built"), mn.messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			Radar_Initialise(&(baseCurrent->radar), 0.0f, 1.0f);
			B_ResetAllStatusAndCapacities(baseCurrent, qtrue);
			AL_FillInContainment(baseCurrent);
			PR_UpdateProductionCap(baseCurrent);

			/* initial base equipment */
			if (gd.numBases == 1) {
				int i;

				INV_InitialEquipment(baseCurrent, curCampaign);
				/* Auto equip interceptors with weapons and ammos */
				for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
					aircraft_t *aircraft = &baseCurrent->aircraft[i];
					assert(aircraft);
					if (aircraft->type == AIRCRAFT_INTERCEPTOR)
						AIM_AutoEquipAircraft(aircraft);
				}
				CL_GameTimeFast();
				CL_GameTimeFast();
			}
			Cbuf_AddText(va("mn_select_base %i;", baseCurrent->idx));
			return;
		}
	} else {
		Com_sprintf(popupText, sizeof(popupText), _("Not enough credits to set up a new base."));
		MN_Popup(_("Notice"), popupText);
	}
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
 * @brief Builds a base map for tactical combat.
 * @sa SV_AssembleMap
 * @sa CP_BaseAttackChooseBase
 * @note Do we need day and night maps here, too? Sure!
 * @todo Search a empty field and add a alien craft there, also add alien
 * spawn points around the craft, also some trees, etc. for their cover
 * @todo Add soldier spawn points, the best place is quarters.
 * @todo We need to get rid of the tunnels to nirvana.
 */
static void B_AssembleMap_f (void)
{
	int row, col;
	char baseMapPart[1024];
	building_t *entry;
	char maps[2024];
	char coords[2048];
	int setUnderAttack = 0, baseID = 0;
	base_t* base = baseCurrent;

	if (Cmd_Argc() < 2)
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <baseID> <setUnderAttack>\n", Cmd_Argv(0));
	else {
		if (Cmd_Argc() == 3)
			setUnderAttack = atoi(Cmd_Argv(2));
		baseID = atoi(Cmd_Argv(1));
		if (baseID < 0 || baseID >= gd.numBases) {
			Com_DPrintf(DEBUG_CLIENT, "Invalid baseID: %i\n", baseID);
			return;
		}
		base = B_GetBase(baseID);
	}

	if (!base) {
		Com_Printf("B_AssembleMap_f: No base to assemble\n");
		return;
	}

#if 0
FIXME
	if (setUnderAttack) {
		B_BaseAttack(base);
		Com_DPrintf(DEBUG_CLIENT, "Set base %i under attack\n", base->idx);
	}
#endif

	/* reset menu text */
	MN_MenuTextReset(TEXT_STANDARD);

	*maps = '\0';
	*coords = '\0';

	/* reset the used flag */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col] != BASE_FREESLOT && base->map[row][col] != BASE_INVALID_SPACE) {
				entry = B_GetBuildingByIdx(base, base->map[row][col]);
				entry->used = 0;
			}
		}

	/* @todo: If a building is still under construction, it will be assembled as a finished part */
	/* otherwise we need mapparts for all the maps under construction */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			baseMapPart[0] = '\0';

			if (base->map[row][col] != BASE_FREESLOT && base->map[row][col] != BASE_INVALID_SPACE) {
				entry = B_GetBuildingByIdx(base, base->map[row][col]);

				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one */
				/* this is why we check the used flag and continue if it was set already */
				if (!entry->used && entry->needs) {
					entry->used = 1;
				} else if (entry->needs) {
					Com_DPrintf(DEBUG_CLIENT, "B_AssembleMap_f: '%s' needs '%s' (used: %i)\n", entry->id, entry->needs, entry->used );
					entry->used = 0;
					continue;
				}

				if (entry->mapPart)
					Q_strncpyz(baseMapPart, va("b/%s", entry->mapPart), sizeof(baseMapPart));
				else
					Com_Printf("B_AssembleMap_f: Error - map has no mapPart set. Building '%s'\n'", entry->id);
			} else
				Q_strncpyz(baseMapPart, "b/empty", sizeof(baseMapPart));

			if (*baseMapPart) {
				Q_strcat(maps, baseMapPart, sizeof(maps));
				Q_strcat(maps, " ", sizeof(maps));
				/* basetiles are 16 units in each direction
				 * 512 / UNIT_SIZE = 16
				 * 512 is the size in the mapeditor and the worldplane for a
				 * single base map tile */
				Q_strcat(coords, va("%i %i ", col * 16, (BASE_SIZE - row - 1) * 16), sizeof(coords));
			}
		}
	/* set maxlevel for base attacks to 5 */
	map_maxlevel_base = 6;

	if (curCampaign)
		SAV_QuickSave();

	Cbuf_AddText(va("map day \"%s\" \"%s\"\n", maps, coords));
}

/**
 * @brief Cleans all bases but restore the base names
 * @sa CL_GameNew
 */
void B_NewBases (void)
{
	/* reset bases */
	int i;
	char title[MAX_VAR];
	base_t *base;

	for (i = 0; i < MAX_BASES; i++) {
		base = B_GetBase(i);
		Q_strncpyz(title, base->name, sizeof(title));
		B_ClearBase(base);
		Q_strncpyz(base->name, title, sizeof(title));
	}
}

/**
 * @brief Builds a random base
 *
 * call B_AssembleMap with a random base over script command 'base_assemble_rand'
 */
static void B_AssembleRandomBase_f (void)
{
	int setUnderAttack = 0;
	int randomBase = rand() % gd.numBases;
	if (Cmd_Argc() < 2)
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <setUnderAttack>\n", Cmd_Argv(0));
	else
		setUnderAttack = atoi(Cmd_Argv(1));

	if (!gd.bases[randomBase].founded) {
		Com_Printf("Base with id %i was not founded or already destroyed\n", randomBase);
		return;
	}

	Cbuf_AddText(va("base_assemble %i %i\n", randomBase, setUnderAttack));
}

#ifdef DEBUG
/**
 * @brief Just lists all buildings with their data
 *
 * Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void B_BuildingList_f (void)
{
	int i, j, k;
	base_t *base;
	building_t *building;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent) {
		Com_Printf("No base selected\n");
		return;
	}

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse)
			continue;

		building = &gd.buildings[base->idx][i];
		Com_Printf("\nBase id %i: %s\n", i, base->name);
		for (j = 0; j < gd.numBuildings[base->idx]; j++) {
			Com_Printf("...Building: %s #%i - id: %i\n", building->id, B_GetNumberOfBuildingsInBaseByTypeIDX(baseCurrent->idx, building->buildingType),
				building->idx);
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
#endif

#ifdef DEBUG
/**
 * @brief Just lists all bases with their data
 *
 * Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
 */
static void B_BaseList_f (void)
{
	int i, row, col, j;
	base_t *base;

	for (i = 0, base = gd.bases; i < MAX_BASES; i++, base++) {
		Com_Printf("Base id %i\n", base->idx);
		Com_Printf("Base title %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base numAircraftInBase %i\n", base->numAircraftInBase);
		Com_Printf("Base numMissileBattery %i\n", base->maxBatteries);
		Com_Printf("Base numLaserBattery %i\n", base->maxLasers);
		Com_Printf("Base sensorWidth %i\n", base->radar.range);
		Com_Printf("Base numSensoredAircraft %i\n", base->radar.numUFOs);
		Com_Printf("Base Alien interest %f\n", base->alienInterest);
		Com_Printf("Base hasBuilding[]: ");
		for (j = 0; j < MAX_BUILDING_TYPE; j++)
			Com_Printf("%i ", base->hasBuilding[j]);
		Com_Printf("\nBase aircraft %i\n", base->numAircraftInBase);
		for (j = 0; j < base->numAircraftInBase; j++) {
			Com_Printf("Base aircraft-team %i\n", base->aircraft[j].teamSize);
		}
		Com_Printf("Base pos %f:%f\n", base->pos[0], base->pos[1]);
		Com_Printf("Base map:\n");
		for (row = 0; row < BASE_SIZE; row++) {
			if (row)
				Com_Printf("\n");
			for (col = 0; col < BASE_SIZE; col++)
				Com_Printf("%i ", base->map[row][col]);
		}
		Com_Printf("\n");
	}
}
#endif

/**
 * @brief Sets the title of the base.
 */
static void B_SetBaseTitle_f (void)
{
	Com_DPrintf(DEBUG_CLIENT, "B_SetBaseTitle_f: #bases: %i\n", gd.numBases);
	if (gd.numBases < MAX_BASES)
		Cvar_Set("mn_base_title", gd.bases[gd.numBases].name);
	else {
		MN_AddNewMessage(_("Notice"), _("You've reached the base limit."), qfalse, MSG_STANDARD, NULL);
		MN_PopMenu(qfalse);		/* remove the new base popup */
	}
}

/**
 * @brief Creates console command to change the name of a base.
 * Copies the value of the cvar mn_base_title over as the name of the
 * current selected base
 */
static void B_ChangeBaseName_f (void)
{
	/* maybe called without base initialized or active */
	if (!baseCurrent)
		return;

	Q_strncpyz(baseCurrent->name, Cvar_VariableString("mn_base_title"), sizeof(baseCurrent->name));
}

/**
 * @brief Checks why a button in base menu is disabled, and create a popup to inform player
 */
static void B_CheckBuildingStatusForMenu_f (void)
{
	int i, num;
	int baseIdx;
	const char *buildingID;
	building_t *building, *dependenceBuilding;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: %s buildingID\n", Cmd_Argv(0));
		return;
	}
	buildingID = Cmd_Argv(1);
	building = B_GetBuildingType(buildingID);

	if (!building) {
		Com_Printf("B_CheckBuildingStatusForMenu_f: building pointer is NULL\n");
		return;
	}

	if (!baseCurrent) {
		Com_Printf("B_CheckBuildingStatusForMenu_f: baseCurrent pointer is NULL\n");
		return;
	}

	/* Maybe base is under attack ? */
	if (baseCurrent->baseStatus == BASE_UNDER_ATTACK) {
		Com_sprintf(popupText, sizeof(popupText), _("Base is under attack, you can't access this building !"));
		MN_Popup(_("Notice"), popupText);
		return;
	}

	baseIdx = baseCurrent->idx;

	if (building->buildingType == B_HANGAR) {
		/* this is an exception because you must have a small or large hangar to enter aircraft menu */
		Com_sprintf(popupText, sizeof(popupText), _("You need at least one Hangar (and its dependencies) to use aircraft."));
		MN_Popup(_("Notice"), popupText);
		return;
	}

	num = B_GetNumberOfBuildingsInBaseByType(baseIdx, building->buildingType);
	if (num > 0) {
		int numUnderConstruction;
		/* maybe all buildings of this type are under construction ? */
		B_CheckBuildingTypeStatus(baseCurrent, building->buildingType, B_STATUS_UNDER_CONSTRUCTION, &numUnderConstruction);
		if (numUnderConstruction == num) {
			int minDay = 99999;
			/* Find the building whose construction will finish first */
			for (i = 0; i < gd.numBuildings[baseIdx]; i++) {
				if (gd.buildings[baseIdx][i].buildingType == building->buildingType
					&& gd.buildings[baseIdx][i].buildingStatus == B_STATUS_UNDER_CONSTRUCTION
					&& minDay > gd.buildings[baseIdx][i].buildTime - (ccs.date.day - gd.buildings[baseIdx][i].timeStart))
					minDay = gd.buildings[baseIdx][i].buildTime - (ccs.date.day - gd.buildings[baseIdx][i].timeStart);
			}
			Com_sprintf(popupText, sizeof(popupText), va(ngettext("Construction of building will be over in %i day.\nPlease wait to enter.", "Construction of building will be over in %i days.\nPlease wait to enter.",
				minDay), minDay));
			MN_Popup(_("Notice"), popupText);
			return;
		}
		dependenceBuilding = gd.buildingTypes + building->dependsBuilding;
		if (!B_CheckBuildingDependencesStatus(baseCurrent, building)) {
			if (B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, dependenceBuilding->buildingType) <= 0) {
				/* the dependence of the building is not built */
				Com_sprintf(popupText, sizeof(popupText), va(_("You need a building %s to make building %s functional."), _(dependenceBuilding->name), _(building->name)));
				MN_Popup(_("Notice"), popupText);
				return;
			} else {
				/* maybe the dependence of the building is under construction
				 * note that we can't use B_STATUS_UNDER_CONSTRUCTION here, because this value
				 * is not use for every building (for exemple Command Centre) */
				for (i = 0; i < gd.numBuildings[baseIdx]; i++) {
					if (gd.buildings[baseIdx][i].buildingType == dependenceBuilding->buildingType
					 && gd.buildings[baseIdx][i].buildTime > (ccs.date.day - gd.buildings[baseIdx][i].timeStart)) {
						Com_sprintf(popupText, sizeof(popupText), va(_("Building %s is not finished yet, and is needed to use building %s."),
							_(dependenceBuilding->name), _(building->name)));
						MN_Popup(_("Notice"), popupText);
						return;
					}
				}
				/* the dependence is built but doesn't work - must be because of their dependendes */
				Com_sprintf(popupText, sizeof(popupText), va(_("Make sure that the dependencies of building %s (%s) are operational, so that building %s may be used."),
					_(dependenceBuilding->name), _((gd.buildingTypes + dependenceBuilding->dependsBuilding)->name), _(building->name)));
				MN_Popup(_("Notice"), popupText);
				return;
			}
		}
		/* all buildings are OK: employees must be missing */
		if ((building->buildingType == B_WORKSHOP) && (E_CountHired(baseCurrent, EMPL_WORKER) <= 0)) {
			Com_sprintf(popupText, sizeof(popupText), va(_("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_WORKER), _(building->name)));
			MN_Popup(_("Notice"), popupText);
			return;
		} else if ((building->buildingType == B_HOSPITAL) && (E_CountHired(baseCurrent, EMPL_MEDIC) <= 0)) {
			Com_sprintf(popupText, sizeof(popupText), va(_("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_MEDIC), _(building->name)));
			MN_Popup(_("Notice"), popupText);
			return;
		} else if ((building->buildingType == B_LAB) && (E_CountHired(baseCurrent, EMPL_SCIENTIST) <= 0)) {
			Com_sprintf(popupText, sizeof(popupText), va(_("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_SCIENTIST), _(building->name)));
			MN_Popup(_("Notice"), popupText);
			return;
		}
	} else {
		Com_sprintf(popupText, sizeof(popupText), va(_("Build a %s first."), _(building->name)));
		MN_Popup(_("Notice"), popupText);
		return;
	}
}

/**
 * @brief Checks whether the building menu or the pedia entry should be called when baseCurrent->buildingCurrent is set
 */
static void B_BuildingOpen_f (void)
{
	if (baseCurrent && baseCurrent->buildingCurrent) {
		if (!B_GetBuildingStatus(baseCurrent, baseCurrent->buildingCurrent->buildingType)) {
			UP_OpenWith(baseCurrent->buildingCurrent->pedia);
		} else {
			switch (baseCurrent->buildingCurrent->buildingType) {
			case B_LAB:
				if (RS_ResearchAllowed(baseCurrent))
					MN_PushMenu("research");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			case B_HOSPITAL:
				if (HOS_HospitalAllowed(baseCurrent))
					MN_PushMenu("hospital");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			case B_ALIEN_CONTAINMENT:
				if (AC_ContainmentAllowed(baseCurrent))
					MN_PushMenu("aliencont");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			case B_QUARTERS:
				if (E_HireAllowed(baseCurrent))
					MN_PushMenu("employees");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			case B_WORKSHOP:
				if (PR_ProductionAllowed(baseCurrent))
					MN_PushMenu("production");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			case B_DEFENSE_LASER:
			case B_DEFENSE_MISSILE:
				MN_PushMenu("basedefense");
				break;
			case B_HANGAR:
			case B_SMALL_HANGAR:
				if (!AIR_AircraftAllowed(baseCurrent)) {
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
					break;
				}
				if (baseCurrent->numAircraftInBase)
					MN_PushMenu("aircraft");
				else {
					MN_PushMenu("buyaircraft");
					/* transfer is only possible when there are at least two bases */
					if (gd.numBases > 1)
						MN_Popup(_("Note"), _("No aircraft in this base - You first have to purchase or transfer an aircraft\n"));
					else
						MN_Popup(_("Note"), _("No aircraft in this base - You first have to purchase an aircraft\n"));
				}
				break;
			case B_STORAGE:
			case B_ANTIMATTER:
				if (BS_BuySellAllowed(baseCurrent))
					MN_PushMenu("buy");
				else
					UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			default:
				UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			}
		}
	} else {
		Com_Printf("Usage: Only call me from baseview\n");
	}
}

/**
 * @brief This function checks whether a user build the max allowed amount of bases
 * if yes, the MN_PopMenu will pop the base build menu from the stack
 */
static void B_CheckMaxBases_f (void)
{
	if (gd.numBases >= MAX_BASES) {
		MN_PopMenu(qfalse);
	}
}

#ifdef DEBUG
/**
 * @brief Debug function for printing all capacities in given base.
 */
static void B_PrintCapacities_f (void)
{
	int i, j;
	base_t *base;
	buildingType_t building;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBase(i);
	for (i = 0; i < MAX_CAP; i++) {
		building = B_GetBuildingTypeByCapacity(i);
		if (building == MAX_BUILDING_TYPE)
			Com_Printf("B_PrintCapacities_f()... Could not find building associated with capacity %i\n", i);
		else {
			for (j = 0; j < gd.numBuildingTypes; j++) {
				if (gd.buildingTypes[j].buildingType == building)
					break;
			}
			Com_Printf("Building: %s, capacity max: %i, capacity cur: %i\n",
			gd.buildingTypes[j].id, base->capacities[i].max, base->capacities[i].cur);
		}
	}
}
#endif

/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void B_ResetBaseManagement (void)
{
	Com_DPrintf(DEBUG_CLIENT, "Reset basemanagement\n");

	/* add commands and cvars */
	Cmd_AddCommand("mn_prev_base", B_PrevBase_f, "Go to the previous base");
	Cmd_AddCommand("mn_next_base", B_NextBase_f, "Go to the next base");
	Cmd_AddCommand("mn_select_base", B_SelectBase_f, NULL);
	Cmd_AddCommand("mn_build_base", B_BuildBase_f, NULL);
	Cmd_AddCommand("new_building", B_NewBuildingFromList_f, NULL);
	Cmd_AddCommand("set_building", B_SetBuilding_f, NULL);
	Cmd_AddCommand("mn_setbasetitle", B_SetBaseTitle_f, NULL);
	Cmd_AddCommand("bases_check_max", B_CheckMaxBases_f, NULL);
	Cmd_AddCommand("rename_base", B_RenameBase_f, "Rename the current base");
	Cmd_AddCommand("base_changename", B_ChangeBaseName_f, "Called after editing the cvar base name");
	Cmd_AddCommand("base_init", B_BaseInit_f, NULL);
	Cmd_AddCommand("base_assemble", B_AssembleMap_f, "Called to assemble the current selected base");
	Cmd_AddCommand("base_assemble_rand", B_AssembleRandomBase_f, NULL);
	Cmd_AddCommand("building_open", B_BuildingOpen_f, NULL);
	Cmd_AddCommand("building_init", B_BuildingInit_f, NULL);
	Cmd_AddCommand("building_status", B_BuildingStatus_f, NULL);
	Cmd_AddCommand("building_destroy", B_BuildingDestroy_f, "Function to destroy a buliding (select via right click in baseview first)");
	Cmd_AddCommand("buildinginfo_click", B_BuildingInfoClick_f, NULL);
	Cmd_AddCommand("check_building_status", B_CheckBuildingStatusForMenu_f, "Create a popup to inform player why he can't use a button");
	Cmd_AddCommand("buildings_click", B_BuildingClick_f, NULL);
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent_f, NULL);
	Cmd_AddCommand("pack_initial", B_PackInitialEquipment_f, NULL);
	Cmd_AddCommand("assign_initial", B_AssignInitial_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("debug_baselist", B_BaseList_f, "Print base information to the game console");
	Cmd_AddCommand("debug_buildinglist", B_BuildingList_f, "Print building information to the game console");
	Cmd_AddCommand("debug_capacities", B_PrintCapacities_f, "Debug function to show all capacities in given base");
	Cmd_AddCommand("debug_basereset", B_ResetAllStatusAndCapacities_f, "Reset building status and capacities of all bases");
#endif

	mn_base_count = Cvar_Get("mn_base_count", "0", 0, "Current amount of build bases");
	mn_base_id = Cvar_Get("mn_base_id", "-1", 0, "Internal id of the current selected base");
	cl_equip = Cvar_Get("cl_equip", "multiplayer_initial", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
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
	building_t *b;
	int i, j;
	int newBuilding = 0, new;

	for (i = 0; i < MAX_BASES; i++) {
		if (!gd.bases[i].founded)
			continue;
		for (j = 0; j < gd.numBuildings[i]; j++) {
			b = &gd.buildings[i][j];
			if (!b)
				continue;
			new = B_CheckBuildingConstruction(b, B_GetBase(i));
			newBuilding += new;
			if (new) {
				Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Construction of %s building finished in base %s."), _(b->name), gd.bases[i].name);
				MN_AddNewMessage(_("Building finished"), mn.messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			}
		}

		/* Repair base buildings */
		if (gd.bases[i].batteryDamage <= MAX_BATTERY_DAMAGE) {
			gd.bases[i].batteryDamage += 20;
			if (gd.bases[i].batteryDamage > MAX_BATTERY_DAMAGE)
				gd.bases[i].batteryDamage = MAX_BATTERY_DAMAGE;
		}
		if (gd.bases[i].baseDamage <= MAX_BASE_DAMAGE) {
			gd.bases[i].baseDamage += 20;
			if (gd.bases[i].baseDamage > MAX_BASE_DAMAGE)
				gd.bases[i].baseDamage = MAX_BASE_DAMAGE;
		}
	}
}

/**
 * @brief Checks whether the construction of a building is finished.
 *
 * Calls the onConstruct functions and assign workers, too.
 */
int B_CheckBuildingConstruction (building_t * building, base_t* base)
{
	int newBuilding = 0;

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		if (building->timeStart && (building->timeStart + building->buildTime) <= ccs.date.day) {
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
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft Returning aircraft.
 * @param[in] base Base, where aircraft lands.
 * @note Place here any stuff, which should be called
 * @note when Drophip returns to base.
 * @sa CL_CampaignRunAircraft
 */
void CL_AircraftReturnedToHomeBase (aircraft_t* aircraft)
{
	HOS_ReaddEmployeesInHospital(aircraft);		/**< Try to readd soldiers to hospital. */
	/* Don't call cargo functions if aircraft is not a transporter. */
	if (aircraft->type != AIRCRAFT_TRANSPORTER)
		return;
	AL_AddAliens(aircraft);			/**< Add aliens to Alien Containment. */
	AL_CountAll();				/**< Count all alive aliens. */
	INV_SellOrAddItems(aircraft);		/**< Sell collected items or add them to storage. */
	RS_MarkResearchable(qfalse);		/**< Mark new technologies researchable. */

	/* Now empty alien/item cargo just in case. */
	memset(aircraft->aliencargo, 0, sizeof(aircraft->aliencargo));
	memset(aircraft->itemcargo, 0, sizeof(aircraft->itemcargo));
	aircraft->alientypes = 0;

	AII_ReloadWeapon(aircraft);
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage) in the given base.
 * @param[in] item_idx The index of the item in the item-list.
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 * @note Formerly known as RS_ItemInBase.
 */
int B_ItemInBase (int item_idx, base_t *base)
{
	equipDef_t *ed;

	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	/* Com_DPrintf(DEBUG_CLIENT, "B_ItemInBase: DEBUG idx %s\n",  csi.ods[item_idx].id); */

	return ed->num[item_idx];
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
	buildingType_t building;

	/* Get building type. */
	building = B_GetBuildingTypeByCapacity(cap);

	switch (cap) {
	case CAP_ALIENS:		/**< Update Aliens capacity in base. */
	case CAP_EMPLOYEES:		/**< Update employees capacity in base. */
	case CAP_LABSPACE:		/**< Update laboratory space capacity in base. */
	case CAP_WORKSPACE:		/**< Update workshop space capacity in base. */
	case CAP_HOSPSPACE:		/**< Update hospital space capacity in base. */
	case CAP_ITEMS:			/**< Update items capacity in base. */
	case CAP_AIRCRAFTS_SMALL:	/**< Update aircraft capacity in base. */
	case CAP_AIRCRAFTS_BIG:		/**< Update aircraft capacity in base. */
	case CAP_UFOHANGARS_SMALL:	/**< Base capacities for UFO hangars. */
	case CAP_UFOHANGARS_LARGE:	/**< Base capacities for UFO hangars. */
	case CAP_ANTIMATTER:		/**< Update antimatter capacity in base. */
		/* Reset given capacity in current base. */
		base->capacities[cap].max = 0;
		/* Get building capacity. */
		for (i = 0; i < gd.numBuildingTypes; i++) {
			if (gd.buildingTypes[i].buildingType == building) {
				capacity = gd.buildingTypes[i].capacity;
				Com_DPrintf(DEBUG_CLIENT, "Building: %s capacity: %i\n", gd.buildingTypes[i].id, capacity);
				b_idx = i;
				break;
			}
		}
		/* Finally update capacity. */
		for (j = 0; j < gd.numBuildings[base->idx]; j++) {
			if ((gd.buildings[base->idx][j].buildingType == building)
			&& (gd.buildings[base->idx][j].buildingStatus >= B_STATUS_CONSTRUCTION_FINISHED)) {
				base->capacities[cap].max += capacity;
			}
		}
		if (b_idx != -1)
			Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities()... updated capacity of %s: %i\n",
				gd.buildingTypes[b_idx].id, base->capacities[cap].max);
		break;
	case MAX_CAP:			/**< Update all capacities in base. */
		Com_DPrintf(DEBUG_CLIENT, "B_UpdateBaseCapacities()... going to update ALL capacities.\n");
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
 * @sa B_LoadItemSlots
 * @sa B_Save
 * @sa AII_InitialiseSlot
 */
static void B_SaveItemSlots (aircraftSlot_t* slot, int num, int* targets, sizebuf_t* sb)
{
	int i;
	int ammoIdx;

	for (i = 0; i < num; i++) {
		if (slot[i].itemIdx != NONE) {
			ammoIdx = slot[i].ammoIdx;
			MSG_WriteString(sb, csi.ods[slot[i].itemIdx].id);
			MSG_WriteShort(sb, slot[i].ammoLeft);
			MSG_WriteShort(sb, slot[i].delayNextShot);
			MSG_WriteShort(sb, slot[i].installationTime);
			/* if there is no ammo MSG_WriteString will write an empty string */
			MSG_WriteString(sb, ammoIdx != NONE ? csi.ods[ammoIdx].id : "");
		} else {
			MSG_WriteString(sb, "");
			MSG_WriteShort(sb, -1);	/* must be the same value as in AII_InitialiseSlot */
			MSG_WriteShort(sb, 0);
			MSG_WriteShort(sb, 0);
			/* if there is no ammo MSG_WriteString will write an empty string */
			MSG_WriteString(sb, "");
		}
		if (targets) {
			/* save target of this weapon */
			MSG_WriteShort(sb, targets[i]);
		}
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
	base_t *b;
	aircraft_t *aircraft;
	building_t *building;

	MSG_WriteShort(sb, gd.numAircraft);
	MSG_WriteByte(sb, gd.numBases);
	for (i = 0; i < gd.numBases; i++) {
		b = B_GetBase(i);
		MSG_WriteString(sb, b->name);
		MSG_WritePos(sb, b->pos);
		MSG_WriteByte(sb, b->founded);
		for (k = 0; k < presaveArray[PRE_MBUITY]; k++)
			MSG_WriteByte(sb, b->hasBuilding[k]);
		for (k = 0; k < presaveArray[PRE_BASESI]; k++)
			for (l = 0; l < presaveArray[PRE_BASESI]; l++) {
				MSG_WriteShort(sb, b->map[k][l]);
				MSG_WriteShort(sb, b->posX[k][l]);
				MSG_WriteShort(sb, b->posY[k][l]);
			}
		for (k = 0; k < presaveArray[PRE_MAXBUI]; k++) {
			building = &gd.buildings[i][k];
			MSG_WriteByte(sb, building->type_idx);
			MSG_WriteByte(sb, building->buildingStatus);
			MSG_WriteLong(sb, building->timeStart);
			MSG_WriteLong(sb, building->buildTime);
			MSG_Write2Pos(sb, building->pos);
		}
		MSG_WriteShort(sb, gd.numBuildings[i]);
		MSG_WriteByte(sb, b->baseStatus);
		MSG_WriteFloat(sb, b->alienInterest);

		MSG_WriteByte(sb, b->maxBatteries);
		B_SaveItemSlots(b->batteries, b->maxBatteries, b->targetMissileIdx, sb);

		MSG_WriteByte(sb, b->maxLasers);
		B_SaveItemSlots(b->lasers, b->maxLasers, b->targetLaserIdx, sb);

		MSG_WriteShort(sb, b->aircraftCurrent); /* Might be AIRCRAFT_INBASE_INVALID (-1) */
		MSG_WriteByte(sb, b->numAircraftInBase);
		for (k = 0; k < b->numAircraftInBase; k++) {
			aircraft = &b->aircraft[k];
			MSG_WriteString(sb, aircraft->id);
			MSG_WriteShort(sb, aircraft->idx);
			MSG_WriteByte(sb, aircraft->status);
			MSG_WriteLong(sb, aircraft->fuel);
			MSG_WritePos(sb, aircraft->pos);
			MSG_WriteLong(sb, aircraft->time);
			MSG_WriteShort(sb, aircraft->point);
			MSG_WriteByte(sb, aircraft->hangar);
			/* Save target of the ufo */
			if (aircraft->aircraftTarget)
				MSG_WriteShort(sb, aircraft->aircraftTarget - gd.ufos);
			else
				MSG_WriteShort(sb, AIRFIGHT_NO_TARGET);
			/* save weapon slots */
			MSG_WriteByte(sb, aircraft->maxWeapons);
			B_SaveItemSlots(aircraft->weapons, aircraft->maxWeapons, NULL, sb);

			/* save shield slots - currently only one */
			MSG_WriteByte(sb, 1);
			if (aircraft->shield.itemIdx != NONE) {
				assert(aircraft->shield.itemIdx < csi.numODs);
				MSG_WriteString(sb, csi.ods[aircraft->shield.itemIdx].id);
				MSG_WriteShort(sb, aircraft->shield.installationTime);
			} else {
				MSG_WriteString(sb, "");
				MSG_WriteShort(sb, 0);
			}
			/* save electronics slots */
			MSG_WriteByte(sb, aircraft->maxElectronics);
			for (l = 0; l < aircraft->maxElectronics; l++) {
				if (aircraft->electronics[l].itemIdx != NONE) {
					assert(aircraft->electronics[l].itemIdx < csi.numODs);
					MSG_WriteString(sb, csi.ods[aircraft->electronics[l].itemIdx].id);
					MSG_WriteShort(sb, aircraft->electronics[l].installationTime);
				} else {
					MSG_WriteString(sb, "");
					MSG_WriteShort(sb, 0);
				}
			}

			/* Save team on board */
			/** @note presaveArray[PRE_ACTTEA]==MAX_ACTIVETEAM and NOT teamSize */
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				MSG_WriteShort(sb, aircraft->teamIdxs[l]);
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				MSG_WriteShort(sb, aircraft->teamTypes[l]);

			MSG_WriteShort(sb, aircraft->numUpgrades);
			MSG_WriteShort(sb, aircraft->radar.range);
			MSG_WriteShort(sb, aircraft->route.numPoints);
			MSG_WriteFloat(sb, aircraft->route.distance);
			for (l = 0; l < aircraft->route.numPoints; l++)
				MSG_Write2Pos(sb, aircraft->route.point[l]);
			MSG_WriteShort(sb, aircraft->alientypes);
			MSG_WriteShort(sb, aircraft->itemtypes);
			/* Save only needed if aircraft returns from a mission. */
			switch (aircraft->status) {
			case AIR_RETURNING:
				/* aliencargo */
				for (l = 0; l < aircraft->alientypes; l++) {
					assert(aircraft->aliencargo[l].teamDef);
					MSG_WriteString(sb, aircraft->aliencargo[l].teamDef->id);
					MSG_WriteShort(sb, aircraft->aliencargo[l].amount_alive);
					MSG_WriteShort(sb, aircraft->aliencargo[l].amount_dead);
				}
				/* itemcargo */
				for (l = 0; l < aircraft->itemtypes; l++) {
					MSG_WriteString(sb, csi.ods[aircraft->itemcargo[l].idx].id);
					MSG_WriteShort(sb, aircraft->itemcargo[l].amount);
				}
				break;
			case AIR_MISSION:
				assert(aircraft->mission);
				MSG_WriteString(sb, aircraft->mission->id);
				break;
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
		MSG_WriteByte(sb, b->equipType);

		/* store equipment */
		for (k = 0; k < presaveArray[PRE_NUMODS]; k++) {
			MSG_WriteString(sb, csi.ods[k].id);
			MSG_WriteShort(sb, b->storage.num[k]);
			MSG_WriteByte(sb, b->storage.num_loose[k]);
		}

		MSG_WriteShort(sb, b->radar.range);

		/* Alien Containment. */
		for (k = 0; k < presaveArray[PRE_NUMALI]; k++) {
			MSG_WriteString(sb, b->alienscont[k].teamDef->name);
			MSG_WriteShort(sb, b->alienscont[k].amount_alive);
			MSG_WriteShort(sb, b->alienscont[k].amount_dead);
		}

		/* Base capacities. */
		for (k = 0; k < presaveArray[PRE_MAXCAP]; k++) {
			MSG_WriteLong(sb, b->capacities[k].cur);
			MSG_WriteLong(sb, b->capacities[k].max);
		}

		/* Buy/Sell factors. */
		MSG_WriteByte(sb, b->buyfactor);
		MSG_WriteByte(sb, b->sellfactor);

		/* Hospital stuff. */
		for (k = 0; k < presaveArray[PRE_EMPTYP]; k++) {
			MSG_WriteShort(sb, b->hospitalListCount[k]);
			for (l = 0; l < presaveArray[PRE_MAXEMP]; l++) {
				MSG_WriteShort(sb, b->hospitalList[k][l]);
			}
		}
		MSG_WriteShort(sb, b->hospitalMissionListCount);
		for (k = 0; k < presaveArray[PRE_MAXEMP]; k++) {
			MSG_WriteShort(sb, b->hospitalMissionList[k]);
		}
	}
	return qtrue;
}

/**
 * @brief Loads the missile and laser slots data
 * @sa B_Load
 * @sa B_SaveItemSlots
 */
static void B_LoadItemSlots (base_t* base, aircraftSlot_t* slot, int num, int* targets, sizebuf_t* sb)
{
	int i;
	technology_t *tech;

	for (i = 0; i < num; i++) {
		tech = RS_GetTechByProvided(MSG_ReadString(sb));
		/* base is NULL here to not check against the storage amounts - they
		 * are already loaded in the campaign load function and set to the value
		 * after the craftitem was already removed from the initial game - thus
		 * there might not be any of these items in the storage at this point */
		/* @todo: Check whether storage and capacities needs updating now */
		if (tech)
			AII_AddItemToSlot(NULL, tech, slot + i);
		slot[i].ammoLeft = MSG_ReadShort(sb);
		slot[i].delayNextShot = MSG_ReadShort(sb);
		slot[i].installationTime = MSG_ReadShort(sb);
		tech = RS_GetTechByProvided(MSG_ReadString(sb));
		if (tech)
			slot[i].ammoIdx = AII_GetAircraftItemByID(tech->provides);
		else
			slot[i].ammoIdx = NONE;
		if (targets) {
			/* read target of this weapon */
			targets[i] = MSG_ReadShort(sb);
		}
	}
}

/**
 * @brief Load callback for savegames
 * @sa B_Save
 * @sa SAV_GameLoad
 * @sa B_LoadItemSlots
 */
qboolean B_Load (sizebuf_t* sb, void* data)
{
	int i, bases, k, l, m, amount, ufoIdx;

	gd.numAircraft = MSG_ReadShort(sb);
	bases = MSG_ReadByte(sb);
	for (i = 0; i < bases; i++) {
		base_t *const b = B_GetBase(i);
		Q_strncpyz(b->name, MSG_ReadStringRaw(sb), sizeof(b->name));
		MSG_ReadPos(sb, b->pos);
		if (b->founded) {
			const byte *const color = MAP_GetColor(b->pos, MAPTYPE_TERRAIN);
			b->mapZone = MAP_GetTerrainType(color);
		}
		b->founded = MSG_ReadByte(sb);
		for (k = 0; k < presaveArray[PRE_MBUITY]; k++)
			b->hasBuilding[k] = MSG_ReadByte(sb);
		for (k = 0; k < presaveArray[PRE_BASESI]; k++)
			for (l = 0; l < presaveArray[PRE_BASESI]; l++) {
				b->map[k][l] = MSG_ReadShort(sb);
				b->posX[k][l] = MSG_ReadShort(sb);
				b->posY[k][l] = MSG_ReadShort(sb);
			}
		for (k = 0; k < presaveArray[PRE_MAXBUI]; k++) {
			building_t *const building = &gd.buildings[i][k];
			*building = gd.buildingTypes[MSG_ReadByte(sb)];
			building->idx = k;
			building->base_idx = i;
			building->buildingStatus = MSG_ReadByte(sb);
			building->timeStart = MSG_ReadLong(sb);
			building->buildTime = MSG_ReadLong(sb);
			MSG_Read2Pos(sb, building->pos);
		}
		gd.numBuildings[i] = MSG_ReadShort(sb);
		b->baseStatus = MSG_ReadByte(sb);
#if 1
/* remove me in next commit */
		b->alienInterest = MSG_ReadShort(sb);
		b->alienInterest = 1.0f;
#endif
#if 0
/* add me in next commit */
		b->alienInterest = MSG_ReadFloat(sb);
#endif
		BDEF_InitialiseBaseSlots(b);

		/* read missile battery slots */
		b->maxBatteries = MSG_ReadByte(sb);
		B_LoadItemSlots(b, b->batteries, b->maxBatteries, b->targetMissileIdx, sb);

		/* read laser battery slots */
		b->maxLasers = MSG_ReadByte(sb);
		B_LoadItemSlots(b, b->lasers, b->maxLasers, b->targetLaserIdx, sb);

		b->aircraftCurrent = MSG_ReadShort(sb);
		b->numAircraftInBase = MSG_ReadByte(sb);
		for (k = 0; k < b->numAircraftInBase; k++) {
			aircraft_t *aircraft;

			const aircraft_t *const model = AIR_GetAircraft(MSG_ReadString(sb));
			if (!model)
				return qfalse;
			/* copy generic aircraft description to individal aircraft in base */
			aircraft = &b->aircraft[k];
			*aircraft = *model;
			aircraft->idx = MSG_ReadShort(sb);
			aircraft->homebase = b;
			/* this is the aircraft array id in current base */
			aircraft->idxInBase = k;
			aircraft->status = MSG_ReadByte(sb);
			aircraft->fuel = MSG_ReadLong(sb);
			MSG_ReadPos(sb, aircraft->pos);
			aircraft->time = MSG_ReadLong(sb);
			aircraft->point = MSG_ReadShort(sb);
			aircraft->hangar = MSG_ReadByte(sb);
			/* load aircraft target */
			ufoIdx = MSG_ReadShort(sb);
			if (ufoIdx == AIRFIGHT_NO_TARGET)
				aircraft->aircraftTarget = NULL;
			else
				aircraft->aircraftTarget = gd.ufos + ufoIdx;
			/* read weapon slot */
			amount = MSG_ReadByte(sb);
			/* only read aircraft->maxWeapons here - skip the rest in the following loop */
			B_LoadItemSlots(b, aircraft->weapons, min(amount, aircraft->maxWeapons), NULL, sb);
			/* just in case there are less slots in new game that in saved one */
			for (l = aircraft->maxWeapons; l < amount; l++) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadString(sb);
			}
			/* check for shield slot */
			/* there is only one shield - but who knows - breaking the savegames if this changes
			 * isn't worth it */
			amount = MSG_ReadByte(sb);
			for (l = 0; l < amount; l++) {
				const technology_t *const tech = RS_GetTechByProvided(MSG_ReadString(sb));
				if (tech)
					AII_AddItemToSlot(NULL, tech, &aircraft->shield);
				aircraft->shield.installationTime = MSG_ReadShort(sb);
			}
			/* read electronics slot */
			amount = MSG_ReadByte(sb);
			for (l = 0; l < amount; l++) {
				/* check that there are enough slots in this aircraft */
				if (l < aircraft->maxElectronics) {
					const technology_t *const tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						AII_AddItemToSlot(NULL, tech, aircraft->electronics + l);
					aircraft->electronics[l].installationTime = MSG_ReadShort(sb);
				} else {
					MSG_ReadString(sb);
					MSG_ReadShort(sb);
				}
			}

			/* Load team on board */
			/** @note presaveArray[PRE_ACTTEA]==MAX_ACTIVETEAM and NOT teamSize */
			aircraft->teamSize = 0;
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++) {
				aircraft->teamIdxs[l] = MSG_ReadShort(sb);
				if (aircraft->teamIdxs[l] >= 0)
					aircraft->teamSize++;
			}
			for (l = 0; l < presaveArray[PRE_ACTTEA]; l++)
				aircraft->teamTypes[l] = MSG_ReadShort(sb);

			aircraft->numUpgrades = MSG_ReadShort(sb);
			aircraft->radar.range = MSG_ReadShort(sb);
			aircraft->route.numPoints = MSG_ReadShort(sb);
			aircraft->route.distance = MSG_ReadFloat(sb);
			for (l = 0; l < aircraft->route.numPoints; l++)
				MSG_Read2Pos(sb, aircraft->route.point[l]);
			/* Load only needed if aircraft returns from a mission. */
			aircraft->alientypes = MSG_ReadShort(sb);
			aircraft->itemtypes = MSG_ReadShort(sb);
			switch (aircraft->status) {
			case AIR_RETURNING:
				/* aliencargo */
				for (l = 0; l < aircraft->alientypes; l++) {
					aircraft->aliencargo[l].teamDef = Com_GetTeamDefinitionByID(MSG_ReadString(sb));
					aircraft->aliencargo[l].amount_alive = MSG_ReadShort(sb);
					aircraft->aliencargo[l].amount_dead = MSG_ReadShort(sb);
				}
				/* itemcargo */
				for (l = 0; l < aircraft->itemtypes; l++) {
					const char *const s = MSG_ReadString(sb);
					m = INVSH_GetItemByID(s);
					if (m == NONE || m >= MAX_OBJDEFS) {
						Com_Printf("B_Load: Could not find item '%s'\n", s);
						MSG_ReadShort(sb);
					} else {
						aircraft->itemcargo[l].idx = m;
						aircraft->itemcargo[l].amount = MSG_ReadShort(sb);
					}
				}
				break;
			case AIR_MISSION:
				aircraft->missionID = Mem_PoolStrDup(MSG_ReadString(sb), cl_localPool, 0);
			}
			MSG_ReadPos(sb, aircraft->direction);
			for (l = 0; l < presaveArray[PRE_AIRSTA]; l++)
				aircraft->stats[l] = MSG_ReadLong(sb);
		}

		b->equipType = MSG_ReadByte(sb);

		/* read equipment */
		for (k = 0; k < presaveArray[PRE_NUMODS]; k++) {
			const char *const s = MSG_ReadString(sb);
			l = INVSH_GetItemByID(s);
			if (l == NONE || l >= MAX_OBJDEFS) {
				Com_Printf("B_Load: Could not find item '%s'\n", s);
				MSG_ReadShort(sb);
				MSG_ReadByte(sb);
			} else {
				b->storage.num[l] = MSG_ReadShort(sb);
				b->storage.num_loose[l] = MSG_ReadByte(sb);
			}
		}

		b->radar.range = MSG_ReadShort(sb);

		/* Alien Containment. */
		AL_FillInContainment(b);	/* Fill Alien Containment with default values. */
		for (k = 0; k < presaveArray[PRE_NUMALI]; k++) {
			const char *const s = MSG_ReadString(sb);
			for (l = 0; l < csi.numTeamDefs; l++) {
				if (!csi.teamDef[l].alien)
					continue;
				if (!Q_strncmp(s, csi.teamDef[l].name, MAX_VAR))
					break;
			}
			if (l == csi.numTeamDefs) {
				Com_Printf("B_Load: Could not find teamDef: %s - skipping the aliencont (%i)\n", s, csi.numTeamDefs);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
			} else {
				b->alienscont[k].amount_alive = MSG_ReadShort(sb);
				b->alienscont[k].amount_dead = MSG_ReadShort(sb);
			}
		}

		/* Base capacities. */
		for (k = 0; k < presaveArray[PRE_MAXCAP]; k++) {
			if (((saveFileHeader_t *)data)->version < 3) {
				/* @note this was a bug in version 2.2 */
				b->capacities[k].cur = MSG_ReadShort(sb);
				b->capacities[k].max = MSG_ReadShort(sb);
			} else {
				b->capacities[k].cur = MSG_ReadLong(sb);
				b->capacities[k].max = MSG_ReadLong(sb);
			}
		}

		/* Buy/Sell factors. */
		b->buyfactor = MSG_ReadByte(sb);
		b->sellfactor = MSG_ReadByte(sb);

		/* Hospital stuff. */
		for (k = 0; k < presaveArray[PRE_EMPTYP]; k++) {
			b->hospitalListCount[k] = MSG_ReadShort(sb);
			for (l = 0; l < presaveArray[PRE_MAXEMP]; l++) {
				b->hospitalList[k][l] = MSG_ReadShort(sb);
			}
		}
		b->hospitalMissionListCount = MSG_ReadShort(sb);
		for (k = 0; k < presaveArray[PRE_MAXEMP]; k++) {
			b->hospitalMissionList[k] = MSG_ReadShort(sb);
		}

		/* clear the mess of stray loaded pointers */
		memset(&b->equipByBuyType, 0, sizeof(inventory_t));

		/* some functions needs the baseCurrent pointer set */
		baseCurrent = b;
	}
	gd.numBases = bases;

	return qtrue;
}

/**
 * @brief Update the storage amount and the capacities for the storages in the base
 * @param[in] base The base which storage and capacity should be updated
 * @param[in] objIDX The idx in csi.ods
 * @param[in] amount Amount to be added to removed
 * @param[in] reset Set this to true (amount is not needed) if you want to reset the
 * storage amount and capacities (e.g. in case of a base ransack)
 * @param[in] ignorecap qtrue if we won't check freespace but will just add items.
 * @sa CL_BaseRansacked
 */
qboolean B_UpdateStorageAndCapacity (base_t* base, int objIDX, int amount, qboolean reset, qboolean ignorecap)
{
	assert(base);
	assert(objIDX < csi.numODs);
	if (reset) {
		base->storage.num[objIDX] = 0;
		base->storage.num_loose[objIDX] = 0; /* FIXME: needed? */
		base->capacities[CAP_ITEMS].cur = 0;
	} else {
		if (!ignorecap && (amount > 0)) {
			/* Only add items if there is enough room in storage */
			if (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur < (csi.ods[objIDX].size * amount)) {
				Com_DPrintf(DEBUG_CLIENT, "B_UpdateStorageAndCapacity: Not enough storage space (item: %i, amount: %i)\n", objIDX, amount);
				return qfalse;
			}
		}

		base->storage.num[objIDX] += amount;
		if (csi.ods[objIDX].size > 0)
			base->capacities[CAP_ITEMS].cur += (amount * csi.ods[objIDX].size);

		if (base->capacities[CAP_ITEMS].cur < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current storage capacity is negative (%i): reset to 0\n", base->capacities[CAP_ITEMS].cur);
			base->capacities[CAP_ITEMS].cur = 0;
		}

		if (base->storage.num[objIDX] < 0) {
			Com_Printf("B_UpdateStorageAndCapacity: current number of item '%s' is negative: reset to 0\n", csi.ods[objIDX].id);
			base->storage.num[objIDX] = 0;
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

	for (i = 0, b = gd.buildingTypes; i < gd.numBuildingTypes; i++, b++) {
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
