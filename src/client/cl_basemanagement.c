/**
 * @file cl_basemanagement.c
 * @brief Handles everything that is located in or accessed trough a base.
 *
 * See "base/ufos/basemanagement.ufo", "base/ufos/menu_bases.ufo" and "base/ufos/menu_buildings.ufo" for the underlying content.
 * TODO: new game does not reset basemangagement
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

#include "client.h"
#include "cl_global.h"

building_t *B_GetFreeBuildingType(buildingType_t type);
int B_GetNumberOfBuildingsInBaseByType(int base_idx, int type_idx);
static int B_BuildingAddEmployees(building_t* b, employeeType_t type, int amount);

vec2_t newBasePos;
cvar_t *mn_base_title;

static int BuildingConstructionList[MAX_BUILDINGS];
static int numBuildingConstructionList;


/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu but also several times from cl_basemanagement.c.
 */
void B_ResetBuildingCurrent(void)
{
	if (baseCurrent) {
		baseCurrent->buildingCurrent = NULL;
		baseCurrent->buildingToBuild = -1;
	}
}

/**
 * @brief Holds the names of valid entries in the basemanagement.ufo file.
 *
 * The valid definition names for BUILDINGS (building_t) in the basemagagement.ufo file.
 * to the appropriate values in the corresponding struct
 */
static value_t valid_vars[] = {
	{"map_name", V_STRING, offsetof(building_t, mapPart)},	/**< Name of the map file for generating basemap. */
	{"more_than_one", V_BOOL, offsetof(building_t, moreThanOne)},	/**< Is the building allowed to be build more the one time? */
	{"name", V_TRANSLATION_STRING, offsetof(building_t, name)},	/**< The displayed building name. */
	{"pedia", V_STRING, offsetof(building_t, pedia)},	/**< The pedia-id string for the associated pedia entry. */
	{"status", V_INT, offsetof(building_t, buildingStatus)},	/**< The current status of the building. */
	{"image", V_STRING, offsetof(building_t, image)},	/**< Identifies the image for the building. */
	{"visible", V_BOOL, offsetof(building_t, visible)}, /**< Determines whether a building should be listed in the construction list. Set the first part of a building to 1 all others to 0 otherwise all building-parts will be on the list */
	{"needs", V_STRING, offsetof(building_t, needs)},	/**<  For buildings with more than one part; the other parts of the building needed.*/
	{"fixcosts", V_FLOAT, offsetof(building_t, fixCosts)},	/**< Cost to build. */
	{"varcosts", V_FLOAT, offsetof(building_t, varCosts)},	/**< Costs that will come up by using the building. */
	{"build_time", V_INT, offsetof(building_t, buildTime)},	/**< How many days it takes to construct the building. */

	/*event handler functions */
	{"onconstruct", V_STRING, offsetof(building_t, onConstruct)}, /**< Event handler. */
	{"onattack", V_STRING, offsetof(building_t, onAttack)}, /**< Event handler. */
	{"ondestroy", V_STRING, offsetof(building_t, onDestroy)}, /**< Event handler. */
	{"onupgrade", V_STRING, offsetof(building_t, onUpgrade)}, /**< Event handler. */
	{"onrepair", V_STRING, offsetof(building_t, onRepair)}, /**< Event handler. */
	{"onclick", V_STRING, offsetof(building_t, onClick)}, /**< Event handler. */
	{"pos", V_POS, offsetof(building_t, pos)}, /**< Place of a building. Needed for flag autobuild */
	{"autobuild", V_BOOL, offsetof(building_t, autobuild)}, /**< Automatically construct this building when a base is set up. Must also set the pos-flag. */
	{"firstbase", V_BOOL, offsetof(building_t, firstbase)}, /**< Automatically construct this building for the first base you build. Must also set the pos-flag. */
	{NULL, 0, 0}
};

/**
 * @brief Check if an item exists in a list, return its position in list or -1
 */
extern int CL_ListIsItemExists(void* list[], int numList, const void* item)
{
	int num;

	for (num = 0 ; num < numList ; num++, list++)
		if (*list == item)
			return num;

	return -1;
}

/**
 * @brief Add an item in a list
 */
extern qboolean CL_ListAddItem(void* list[], int* numList, int maxNumList, void* item)
{
	if (*numList >= maxNumList)
		return qfalse;

	list[*numList] = item;
	(*numList)++;
	return qtrue;
}

/**
 * @brief Remove an item from a list
 */
extern qboolean CL_ListRemoveItem(void* list[], int* numList, void* item)
{
	int i;

	for (i=0 ; i < *numList ; i++, list++)
		if (*list == item) {
			(*numList)--;
			*list = list[*numList];
			return qtrue;
		}

	return qfalse;
}

/**
 * @brief Remove an item from a list, from its position in list
 */
extern qboolean CL_ListRemoveItemNum(void* list[], int* numList, int numItem)
{
	if (numItem >= 0) {
		(*numList)--;
		list[numItem] = list[*numList];
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Notify bases that the specified aircraft has been removed from geoscape
 * It will be removed from bases sensors
 */
extern void B_NotifyAircraftRemove(const aircraft_t* aircraft)
{
	base_t* base;

	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		CL_ListRemoveItem((void**)base->sensoredAircraft, &base->numSensoredAircraft, (void*)aircraft);
}

/**
 * @brief Check if the specified  is inside the sensor range of base, and update base sensor and aircraft data
 * Return true if the aircraft is inside sensor
 */
extern qboolean B_CheckAircraftSensored(base_t* base, const aircraft_t* aircraft)
{
	int dist;
	int numAircraftSensored = CL_ListIsItemExists((void**)base->sensoredAircraft, base->numSensoredAircraft, aircraft);

	dist = CP_GetDistance(base->pos, aircraft->pos);

	if (base->sensorWidth >= dist) {
		/* Aircraft is in the sensor range */
		if (numAircraftSensored < 0) {
			Com_DPrintf("#0 New UFO in radar : %p - ufos in radar : %i\n", aircraft, base->numSensoredAircraft);
			CL_ListAddItem((void**)base->sensoredAircraft, &base->numSensoredAircraft, MAX_AIRCRAFT, (void*)aircraft);
			Com_DPrintf("#1 New UFO in radar : %p - ufos in radar : %i\n", aircraft, base->numSensoredAircraft);
		}
		return qtrue;
	}

	/* Aircraft is not in the sensor range */
	if (numAircraftSensored >= 0) {
		Com_DPrintf("#0 New UFO out of radar : %p - ufos in radar : %i\n", aircraft, base->numSensoredAircraft);
		CL_ListRemoveItemNum((void**)base->sensoredAircraft, &base->numSensoredAircraft, numAircraftSensored);
		Com_DPrintf("#1 New UFO ouf of radar : %p - ufos in radar : %i\n", aircraft, base->numSensoredAircraft);
	}
	return qfalse;
}

/**
 * @brief Calc and set in base the count of ufos within the base radar range
 */
static void B_SetUfoCountInSensor(base_t* base)
{
	aircraft_t** ufos;
	int numUfos;

	base->numSensoredAircraft = 0;
	for (UFO_GetUfosList(&ufos, &numUfos) ; numUfos > 0 ; numUfos--, ufos++)
		B_CheckAircraftSensored(base, *ufos);
}

/**
 * @brief Sets a sensor.
 *
 * inc_sensor and dec_sensor are script commands that increase the amount
 * of the radar width for a given base
 */
void B_SetSensor(void)
{
	int i = 0;
	int amount = 0;
	base_t* base;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <amount> <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(2));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(2));
		return;
	}
	amount = atoi(Cmd_Argv(1));
	base = gd.bases + i;

	if (!Q_strncmp(Cmd_Argv(0), "inc", 3))
		base->sensorWidth += amount;	/* inc_sensor */
	else if (!Q_strncmp(Cmd_Argv(0), "dec", 3))
		base->sensorWidth -= amount; /* dec_sensor */
	else
		return;

	/* Set the exact count of sensored ufos within the new range */
	B_SetUfoCountInSensor(base);

}

/**
 * @brief Convert string to employeeType_t
 * @param type Pointer to employee type string
 */
static employeeType_t B_GetEmployeeType(char* type)
{
	assert(type);
	if ( Q_strncmp(type, "EMPL_SCIENTIST", 7 ) )
		return EMPL_SCIENTIST;
	else if ( Q_strncmp(type, "EMPL_SOLDIER", 7 ) )
		return EMPL_SOLDIER;
	else if ( Q_strncmp(type, "EMPL_WORKER", 5 ) )
		return EMPL_WORKER;
	else if ( Q_strncmp(type, "EMPL_MEDIC", 5 ) )
		return EMPL_MEDIC;
	else if ( Q_strncmp(type, "EMPL_ROBOT", 5 ) )
		return EMPL_ROBOT;
	else {
		Com_Printf("Unknown employee type '%s'\n", type);
		return MAX_EMPL;
	}
}

/**
 * @brief Displays the status of a building for baseview.
 *
 * updates the cvar mn_building_status which is used in some menus to display
 * the building status
 */
extern void B_BuildingStatus(void)
{
	int daysLeft;
	int NumberOfBuildings = 0;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent) {
		Com_DPrintf("B_BuildingStatus: No Base or no Building set\n");
		return;
	}

	daysLeft = baseCurrent->buildingCurrent->timeStart + baseCurrent->buildingCurrent->buildTime - ccs.date.day;

	Cvar_Set("mn_building_status", _("Not set"));

	switch (baseCurrent->buildingCurrent->buildingStatus) {
	case B_STATUS_NOT_SET:
		NumberOfBuildings = B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, baseCurrent->buildingCurrent->type_idx);
		if (NumberOfBuildings)
			Cvar_Set("mn_building_status", va(_("Already %i in base"), NumberOfBuildings));
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
 * @brief Setup new base?
 */
void B_SetUpBase(void)
{
	int i;
	building_t *building = NULL;

	assert(baseCurrent);
	/* update the building-list */
	B_BuildingInit();
	Com_DPrintf("Set up for %i\n", baseCurrent->idx);

	for (i = 0; i < gd.numBuildingTypes; i++) {
		if ((gd.numBases == 1 && gd.buildingTypes[i].firstbase)
			|| gd.buildingTypes[i].autobuild) {
			/* TODO: implement check for moreThanOne */
			building = &gd.buildings[baseCurrent->idx][gd.numBuildings[baseCurrent->idx]];
			memcpy(building, &gd.buildingTypes[i], sizeof(building_t));
			/* self-link to building-list in base */
			building->idx = gd.numBuildings[baseCurrent->idx];
			gd.numBuildings[baseCurrent->idx]++;
			/* Link to the base. */
			building->base_idx = baseCurrent->idx;
			Com_DPrintf("Base %i new building:%s (%i) at (%.0f:%.0f)\n", baseCurrent->idx, building->id, i, building->pos[0], building->pos[1]);
			baseCurrent->buildingCurrent = building;
			B_SetBuildingByClick((int) building->pos[0], (int) building->pos[1]);
			building->buildingStatus = B_STATUS_WORKING;
			/*
			   if ( building->moreThanOne
			   && building->howManyOfThisType < BASE_SIZE*BASE_SIZE )
			   building->howManyOfThisType++;
			 */

			/* update the building-list */
			B_BuildingInit();
		}
	}
}

/**
 * Returns the building in the global building-types list that has the unique name buildingID.
 *
 * @param[in] buildingName The unique id of the building (building_t->id).
 *
 * @return building_t If a building was found it is returned, if no id was give the current building is returned, otherwise->NULL.
 */
building_t *B_GetBuildingType(char *buildingName)
{
	int i = 0;

	if (!buildingName)
		return baseCurrent->buildingCurrent;

	for (i = 0; i < gd.numBuildingTypes; i++)
		if (!Q_strcasecmp(gd.buildingTypes[i].id, buildingName))
			return &gd.buildingTypes[i];

	Com_Printf("Building %s not found\n", buildingName);
	return NULL;
}

/**
 * @brief Removes the current selected building.
 *
 * This is only allowed if its still under construction. You will not get any money back.
 */
static void B_RemoveBuilding(void)
{
	building_t *building = NULL;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	building = baseCurrent->buildingCurrent;

	/*only allowed when it is still under construction */
	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		building->buildingStatus = B_STATUS_NOT_SET;
/*		baseCurrent->map[building->pos[0]][building->pos[1]] = -1; */
/* 		if ( building->dependsBuilding >= 0) */
/* 			baseCurrent->map[building->dependsBuilding->pos[0]][building->dependsBuilding->pos[1]] = -1; */
		B_BuildingStatus();
	}
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
static qboolean B_CheckCredits( int costs )
{
	if (costs > ccs.credits)
		return qfalse;
	return qtrue;
}

/**
 * @brief Builds new building.
 *
 * @sa B_CheckCredits
 * @sa CL_UpdateCredits
 * @return qboolean
 *
 * Checks whether the player has enough credits to construct the current selected
 * building before starting construction.
 */
static qboolean B_ConstructBuilding(void)
{
	building_t *building_to_build = NULL;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return qfalse;

	/*enough credits to build this? */
	if ( !B_CheckCredits(baseCurrent->buildingCurrent->fixCosts) ) {
		Com_DPrintf("B_ConstructBuilding: Not enough credits to build: '%s'\n", baseCurrent->buildingCurrent->id);
		B_ResetBuildingCurrent();
		return qfalse;
	}

	Com_DPrintf("Construction of %s is starting\n", baseCurrent->buildingCurrent->id);

	/* second building part */
	if (baseCurrent->buildingToBuild >= 0) {
		building_to_build = &gd.buildingTypes[baseCurrent->buildingToBuild];
		building_to_build->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		baseCurrent->buildingToBuild = -1;
	}

	baseCurrent->buildingCurrent->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
	baseCurrent->buildingCurrent->timeStart = ccs.date.day;

	CL_UpdateCredits(ccs.credits - baseCurrent->buildingCurrent->fixCosts);
	return qtrue;
}

/**
 * @brief Build new building.
 */
void B_NewBuilding(void)
{
	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (baseCurrent->buildingCurrent->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		if ( B_ConstructBuilding() ) {
			B_BuildingStatus();
			Com_DPrintf("B_NewBuilding: buildingCurrent->buildingStatus = %i\n", baseCurrent->buildingCurrent->buildingStatus);
		}
}

/**
 * @brief Set the currently selected building.
 *
 * @param[in] row Set building (baseCurrent->buildingCurrent) to row
 * @param[in] col Set building (baseCurrent->buildingCurrent) to col
 */
void B_SetBuildingByClick(int row, int col)
{
	building_t *building = NULL;
	building_t *secondBuildingPart = NULL;

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

	/*TODO: this is bad style (baseCurrent->buildingCurrent shouldn't link to gd.buildingTypes at all ... it's just not logical) */
	/* if the building is in gd.buildingTypes[] */
	if (baseCurrent->buildingCurrent->base_idx < 0) {
		/* copy building from type-list to base-buildings-list */
		building = &gd.buildings[baseCurrent->idx][gd.numBuildings[baseCurrent->idx]];
		memcpy(building, &gd.buildingTypes[baseCurrent->buildingCurrent->type_idx], sizeof(building_t));
		/* self-link to building-list in base */
		building->idx = gd.numBuildings[baseCurrent->idx];
		gd.numBuildings[baseCurrent->idx]++;
		/* Link to the base. */
		building->base_idx = baseCurrent->idx;
		baseCurrent->buildingCurrent = building;
	}

	if (0 <= row && row < BASE_SIZE && 0 <= col && col < BASE_SIZE) {
		if (baseCurrent->map[row][col] < 0) {
			if (*baseCurrent->buildingCurrent->needs && baseCurrent->buildingCurrent->visible)
				secondBuildingPart = B_GetBuildingType(baseCurrent->buildingCurrent->needs);
			if (secondBuildingPart) {
				if (col + 1 == BASE_SIZE) {
					if (baseCurrent->map[row][col-1] >= 0) {
						Com_DPrintf("Can't place this building here - the second part overlapped with another building\n");
						return;
					}
					col--;
				} else if (baseCurrent->map[row][col+1] >= 0) {
					if (baseCurrent->map[row][col-1] >= 0 || !col) {
						Com_DPrintf("Can't place this building here - the second part overlapped with another building\n");
						return;
					}
					col--;
				}

				baseCurrent->map[row][col + 1] = baseCurrent->buildingCurrent->idx;
				baseCurrent->buildingToBuild = secondBuildingPart->idx;
			} else {
				baseCurrent->buildingToBuild = -1;
			}
			B_NewBuilding();

			baseCurrent->map[row][col] = baseCurrent->buildingCurrent->idx;

			switch (baseCurrent->buildingCurrent->buildingType) {
			case B_LAB:
				baseCurrent->hasLab = 1;
				break;
			case B_HANGAR:
				baseCurrent->hasHangar = 1;
				break;
			default:
				break;
			}
			B_ResetBuildingCurrent();
			B_BuildingInit();	/* update the building-list */
		} else {
			Com_DPrintf("There is already a building\n");
			Com_DPrintf("Building: %i at (row:%i, col:%i)\n", baseCurrent->map[row][col], row, col);
		}
	} else
		Com_DPrintf("Invalid coordinates\n");
}

/**
 * @brief Places the current building in the base (x/y give via console).
 */
static void B_SetBuilding(void)
{
	int row, col;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: set_building <x> <y>\n");
		return;
	}

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	row = atoi(Cmd_Argv(1));
	col = atoi(Cmd_Argv(2));

	/*emulate the mouseclick with the given coordinates */
	B_SetBuildingByClick(row, col);
}

/**
 * @brief Build building from the list of those available.
 */
static void B_NewBuildingFromList(void)
{
	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (baseCurrent->buildingCurrent->buildingStatus < B_STATUS_UNDER_CONSTRUCTION)
		B_NewBuilding();
	else
		B_RemoveBuilding();
}

/**
 * @brief Draws a building.
 */
static void B_DrawBuilding(void)
{
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	menuText[TEXT_BUILDING_INFO] = buildingText;
	*menuText[TEXT_BUILDING_INFO] = '\0';

	building = baseCurrent->buildingCurrent;
	employees_in_building = &building->assigned_employees;

	B_BuildingStatus();

	Com_sprintf(menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va("%s\n", building->name));

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION && building->fixCosts)
		Com_sprintf(menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, _("Costs:\t%1.0f c\n"), building->fixCosts);

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("%i Day(s) to build\n"), building->buildTime), MAX_LIST_CHAR );

	if (building->varCosts)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("Running Costs:\t%1.0f c\n"), building->varCosts), MAX_LIST_CHAR);

	if (employees_in_building->numEmployees)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("Workers:\t%i\n"), employees_in_building->numEmployees), MAX_LIST_CHAR);

	/* FIXME: Rename mn_building_name and mn_building_title */
	if (building->id)
		Cvar_Set("mn_building_name", building->id);

	if (building->name)
		Cvar_Set("mn_building_title", building->name);
}

/**
 * @brief Handles the list of constructable buildings.
 *
 * @param[in] building Add this building to the construction list
 * Called everytime a building was constructed and thus maybe other buildings get available.
 * menuText[TEXT_BUILDINGS] is a pointer to baseCurrent->allBuildingsList which will be displayed in the build-screen.
 * This way every base can hold its own building list.
 * The content is updated everytime B_BuildingInit is called (i.e everytime the buildings-list is dispplayed/updated)
 */
void B_BuildingAddToList(building_t * building)
{
	assert(baseCurrent);

	Q_strcat(menuText[TEXT_BUILDINGS], va("%s\n", _(building->name)), MAX_LIST_CHAR);
	BuildingConstructionList[numBuildingConstructionList] = building->type_idx;
	numBuildingConstructionList++;
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 *
 * @param[in] base_idx Which base
 * @param[in] type_idx Which buildingtype
 */
int B_GetNumberOfBuildingsInBaseByType(int base_idx, int type_idx)
{
	int i;
	int NumberOfBuildings;

	if (base_idx < 0 || base_idx >= gd.numBases) {
		Com_Printf("Bad base-index given: %i\n", base_idx);
		return -1;
	}

	NumberOfBuildings = 0;
	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		if (gd.buildings[base_idx][i].type_idx == type_idx)
			NumberOfBuildings++;
	}
	Com_DPrintf("B_GetNumberOfB...Type: '%s' - type_idx: %i - num_b: %i\n", gd.bases[base_idx].name, gd.buildingTypes[type_idx].id, NumberOfBuildings);
	return NumberOfBuildings;
}

/**
 * @brief Get the maximum status of a building.
 *
 * TODO: I have no idea what this does or is for.
 * @param[in] base_idx Which base
 * @param[in] buildingType Which buildingtype
 * @return
 */
buildingStatus_t B_GetMaximumBuildingStatus(int base_idx, buildingType_t buildingType)
{
	int i;
	buildingStatus_t status = B_STATUS_NOT_SET;

	if (base_idx < 0) {
		Com_Printf("Bad base-index given: %i\n", base_idx);
		return -1;
	}

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		if (gd.buildings[base_idx][i].buildingType == buildingType)
			if (gd.buildings[base_idx][i].buildingStatus > status)
				status = gd.buildings[base_idx][i].buildingStatus;
	}
	return status;
}

/**
 * @brief Update the building-list.
 */
void B_BuildingInit(void)
{
	int i;
	int numSameBuildings;
	building_t *buildingType = NULL;

	/* maybe someone call this command before the bases are parsed?? */
	if (!baseCurrent)
		return;

	Com_DPrintf("B_BuildingInit: Updating b-list for '%s' (%i)\n", baseCurrent->name, baseCurrent->idx);
	Com_DPrintf("B_BuildingInit: Buildings in base: %i\n", gd.numBuildings[baseCurrent->idx]);

	/* initialising the vars used in B_BuildingAddToList */
	memset(baseCurrent->allBuildingsList, 0, sizeof(baseCurrent->allBuildingsList));
	menuText[TEXT_BUILDINGS] = baseCurrent->allBuildingsList;
	numBuildingConstructionList = 0;
	/* ------------------ */

	for (i = 0; i < gd.numBuildingTypes; i++) {
		buildingType = &gd.buildingTypes[i];
		/*make an entry in list for this building */

		if (buildingType->visible) {
			numSameBuildings = B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, buildingType->type_idx);

			if (buildingType->moreThanOne) {
				/* skip if limit of BASE_SIZE*BASE_SIZE exceeded */
				if (numSameBuildings >= BASE_SIZE * BASE_SIZE)
					continue;
			} else if (numSameBuildings > 0) {
				continue;
			}

			/* if the building is researched add it to the list */
			if (RS_IsResearched_idx(buildingType->tech)) {
				/* TODO: check if this out-commented code is still needed */
				if (buildingType->dependsBuilding < 0
					|| B_GetMaximumBuildingStatus(baseCurrent->idx, buildingType->buildingType) >= B_STATUS_CONSTRUCTION_FINISHED) {
					B_BuildingAddToList(buildingType);
				}
			} else {
				Com_DPrintf("Building not researched yet %s\n", buildingType->id);
			}
		}
	}
	if (baseCurrent->buildingCurrent) {
		B_DrawBuilding();
	}
}

/**
 * @brief Gets the type of building by its index.
 *
 * @param[in] idx The index of the building in gd.buildings[]
 * @return buildings_t pointer to gd.buildings[idx]
 */
building_t *B_GetBuildingByIdx(int idx)
{
	if (baseCurrent)
		return &gd.buildings[baseCurrent->idx][idx];
	else
		Sys_Error("Bases not initialized\n");

	/*just that there are no warnings */
	return NULL;
}

/**
 * @brief Opens up the 'pedia if you right click on a building in the list.
 *
 * TODO: really only do this on rightclick.
 * TODO: left click should show building-status.
 */
void B_BuildingInfoClick_f(void)
{
	if (baseCurrent && baseCurrent->buildingCurrent) {
		Com_DPrintf("B_BuildingInfoClick_f: %s - %i\n", baseCurrent->buildingCurrent->id, baseCurrent->buildingCurrent->buildingStatus);
		UP_OpenWith(baseCurrent->buildingCurrent->pedia);
	}
}

/**
 * @brief Script function for clicking the building list text field.
 */
void B_BuildingClick_f(void)
{
	int num;
	building_t *building = NULL;

	if (Cmd_Argc() < 2 || !baseCurrent)
		return;

	/*which building? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf("B_BuildingClick_f: listnumber %i base %i\n", num, baseCurrent->idx);

	if (num > numBuildingConstructionList || num < 0) {
		Com_DPrintf("B_BuildingClick_f: max exceeded %i/%i\n", num, numBuildingConstructionList);
		return;
	}

	building = &gd.buildingTypes[BuildingConstructionList[num]];

	baseCurrent->buildingCurrent = building;
	B_DrawBuilding();
}

/**
 * @brief Copies an entry from the building description file into the list of building types.
 *
 * Parses one "building" entry in the basemanagement.ufo file and writes it into the next free entry in bmBuildings[0], which is the list of buildings in the first base (building_t).
 *
 * @param[in] id Unique test-id of a building_t. This is parsed from "building xxx" -> id=xxx.
 * @param[in] text TODO: document this ... It appears to be the whole following text that is part of the "building" item definition in .ufo.
 * @param[in] link Bool value that decides whether to link the tech pointer in or not
 */
void B_ParseBuildings(char *id, char **text, qboolean link)
{
	building_t *building = NULL;
	building_t *dependsBuilding = NULL;
	employees_t *employees_in_building = NULL;
	technology_t *tech_link = NULL;
	value_t *edp = NULL;
	char *errhead = "B_ParseBuildings: unexptected end of file (names ";
	char *token = NULL;

	/* get id list body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("B_ParseBuildings: building \"%s\" without body ignored\n", id);
		return;
	}
	if (gd.numBuildingTypes >= MAX_BUILDINGS) {
		Com_Printf("B_ParseBuildings: too many buildings\n");
		gd.numBuildingTypes = MAX_BUILDINGS;	/* just in case it's bigger. */
		return;
	}

	if (!link) {
		/* new entry */
		building = &gd.buildingTypes[gd.numBuildingTypes];
		memset(building, 0, sizeof(building_t));
		Q_strncpyz(building->id, id, MAX_VAR);

		Com_DPrintf("...found building %s\n", building->id);

		/*set standard values */
		building->type_idx = gd.numBuildingTypes;
		building->idx = -1;
		building->base_idx = -1;
		building->tech = -1;
		building->dependsBuilding = -1;
		building->visible = qtrue;

		gd.numBuildingTypes++;
		do {
			/* get the name type */
			token = COM_EParse(text, errhead, id);
			if (!*text)
				break;
			if (*token == '}')
				break;

			/* get values */
			if (!Q_strncmp(token, "type", MAX_VAR)) {
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;

				if (!Q_strncmp(token, "lab", MAX_VAR)) {
					building->buildingType = B_LAB;
				} else if (!Q_strncmp(token, "hangar", MAX_VAR)) {
					building->buildingType = B_HANGAR;
				} else if (!Q_strncmp(token, "quarters", MAX_VAR)) {
					building->buildingType = B_QUARTERS;
				} else if (!Q_strncmp(token, "workshop", MAX_VAR)) {
					building->buildingType = B_WORKSHOP;
				}
			} else if (!Q_strncmp(token, "max_employees", MAX_VAR)) {
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;
				employees_in_building = &building->assigned_employees;

				if (*token)
					employees_in_building->maxEmployees = atoi(token);
				else {
					employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
					Com_Printf("Set max employees to %i for building '%s'\n", MAX_EMPLOYEES_IN_BUILDING, building->id);
				}
			} else
			/* no linking yet */
			if (!Q_strncmp(token, "depends", MAX_VAR)) {
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;
			} else if (!Q_strncmp(token, "employees_firstbase", MAX_VAR)) {
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;
				if (*token)
					/* FIXME: EMPL_SCIENTIST */
					B_BuildingAddEmployees(building, EMPL_SCIENTIST, atoi(token));
			} else
				for (edp = valid_vars; edp->string; edp++)
					if (!Q_strncmp(token, edp->string, sizeof(edp->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, id);
						if (!*text)
							return;

						Com_ParseValue(building, token, edp->type, edp->ofs);
						break;
					}

			if (!edp->string)
				Com_Printf("B_ParseBuildings: unknown token \"%s\" ignored (building %s)\n", token, id);

		} while (*text);
	} else {
		building = B_GetBuildingType(id);
		if (!building)			/* i'm paranoid */
			Sys_Error("B_ParseBuildings: Could not find building with id %s\n", id);

		tech_link = RS_GetTechByProvided(id);
		if (tech_link) {
			building->tech = tech_link->idx;
		} else {
			/* TODO: are the techs already parsed? */
			Com_DPrintf("B_ParseBuildings: Could not find tech that provides %s\n", id);
		}

		do {
			/* get the name type */
			token = COM_EParse(text, errhead, id);
			if (!*text)
				break;
			if (*token == '}')
				break;
			/* get values */
			if (!Q_strncmp(token, "depends", MAX_VAR)) {
				dependsBuilding = B_GetBuildingType(COM_EParse(text, errhead, id));
				if (!dependsBuilding)
					Sys_Error("Could not find building depend of %s\n", building->id);
				building->dependsBuilding = dependsBuilding->idx;
				if (!*text)
					return;
			}
		} while (*text);
	}
}

/**
 * @brief Reads in information about employees and assigns them to the correct rooms in a base.
 *
 * This should be called after setting up the first base.
 * TODO: This right now assumes that there are not more employees than free quarter space ... but it will not puke if there are.
 * @sa CL_GameInit
 */
void B_InitEmployees(void)
{
	int i, j;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	/* Loop trough the buildings to assign the type of employee. */
	/* TODO: this right now assumes that there are not more employees than free quarter space ... but it will not puke if there are. */
	for (i = 0; i < gd.numBuildingTypes; i++) {
		Com_DPrintf("B_InitEmployees: 1 type %i\n", i);
		building = &gd.buildingTypes[i];
		employees_in_building = &building->assigned_employees;
		/* TODO: fixed value right now, needs a configureable one. */
		employees_in_building->cost_per_employee = 100;
		if (employees_in_building->maxEmployees <= 0)
			employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
		for (j = 0; j < employees_in_building->numEmployees; j++) {
			employee = &gd.employees[employees_in_building->assigned[j]];
			switch (building->buildingType) {
			case B_QUARTERS:
				employee->type = EMPL_SOLDIER;
				break;
			case B_LAB:
				employee->type = EMPL_SCIENTIST;
				break;
			case B_WORKSHOP:
				employee->type = EMPL_WORKER;
				break;
				/*EMPL_MEDIC */
				/*EMPL_ROBOT */
			default:
				break;
			}
		}
	}

	building = NULL;
	/* Generate stats for employees and assign the quarter-less to quarters. */
	for (i = 0; i < gd.numEmployees; i++) {
		employee = &gd.employees[i];
		switch (employee->type) {
		case EMPL_SOLDIER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			employee->lab = -1;
			employee->workshop = -1;
			if (employee->type == EMPL_SCIENTIST) {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			} else {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			}
			building = B_GetFreeBuildingType(B_QUARTERS);
			employees_in_building = &building->assigned_employees;
			employees_in_building->assigned[employees_in_building->numEmployees++] = employee->idx;
			break;
			/*case EMPL_MEDIC: break; */
			/*case EMPL_ROBOT: break; */
		default:
			break;
		}
	}

	/* Remove them all from their assigned buildings except quarters .. this was just needed for firstbase. */
	for (i = 0; i < gd.numBuildingTypes; i++) {
		building = &gd.buildingTypes[i];
		employees_in_building = &building->assigned_employees;
		if (building->buildingType != B_QUARTERS) {
			employees_in_building->numEmployees = 0;
		}
	}

}

/**
 * @brief Gets a free (with no assigned workers) building in the given base of the given type.
 *
 * @param[in] base_id The number/id of the base to search in.
 * @param[in] type Which type of building to search for.
 *
 * @return The (empty) building.
 */
#if 0
building_t *B_GetFreeBuilding(int base_idx, buildingType_t type)
{
	int i;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == type) {
			/* found correct building-type */
			employees_in_building = &building->assigned_employees;
			if (employees_in_building->numEmployees < employees_in_building->maxEmployees) {
				/* the bulding has free space for employees */
				return building;
			}
		}
	}
	/* no buildings available at all, no correct building type found or no building free */
	return NULL;
}
#endif

/**
 * @brief Gets a free (with no assigned workers) building of the given type.
 *
 * @param[in] type Which type of building to search for.
 *
 * @return The (empty) building.
 */
building_t *B_GetFreeBuildingType(buildingType_t type)
{
	int i;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;

	for (i = 0; i < gd.numBuildingTypes; i++) {
		building = &gd.buildingTypes[i];
		if (building->buildingType == type) {
			/* found correct building-type */
			employees_in_building = &building->assigned_employees;
			if (employees_in_building->numEmployees < employees_in_building->maxEmployees) {
				/* the bulding has free space for employees */
				return building;
			}
		}
	}
	/* no buildings available at all, no correct building type found or no building free */
	return NULL;
}

/**
 * @brief Gets a lab in the given base with no research running.
 *
 * @param[in] base_id The number/id of the base to search in.
 *
 * @return The (unused) lab.
 */
building_t *B_GetUnusedLab(int base_idx)
{
	int i, j;
	building_t *building = NULL;
	technology_t *tech = NULL;
	qboolean used = qfalse;

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == B_LAB) {
			used = qfalse;
			/* check in research tree if the lab is used */
			for (j = 0; j < gd.numTechnologies; j++) {
				tech = &gd.technologies[j];
				if (tech->lab == building->idx) {
					used = qtrue;
					break;
				}
			}
			if (!used)
				return building;
		}
		building = NULL;
	}
	return building;
}

/**
 * @brief Gets the number of unused labs (no assigned workers, no research) in the given base of the given type.
 *
 * @param[in] base_id The number/id of the base to search in.
 *
 * @return Number of found (empty) building.
 */
int B_GetUnusedLabs(int base_idx)
{
	int i, j;
	building_t *building = NULL;
	technology_t *tech = NULL;
	qboolean used = qfalse;

	int numFreeLabs = 0;

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == B_LAB) {
			used = qfalse;
			/* check in research tree if the lab is used */
			for (j = 0; j < gd.numTechnologies; j++) {
				tech = &gd.technologies[j];
				if (tech->lab == building->idx) {
					used = qtrue;
					break;
				}
			}
			if (!used) {
				/*Com_DPrintf("B_GetUnusedLabs: %s is unused in base %i\n", building->id, base_idx ); */
				numFreeLabs++;
			} else {
				/*Com_DPrintf("B_GetUnusedLabs: %s is used in base %i\n", building->id, base_idx ); */
			}
		}
	}
	return numFreeLabs;
}

/**
 * @brief Removes all assigned employees from a building.
 *
 * TODO: If the building is of type "B_QUARTERS" before it's cleared all other buildings need to be checked if there is an employees there that also is in the qarter. These employees need to be removed from those buildings.
 * @param[in] building Building pointer of the building to be cleared
 */
void B_ClearBuilding(building_t * building)
{
	int i;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if (!building)
		return;

	employees_in_building = &building->assigned_employees;
	switch (building->buildingType) {
	case B_QUARTERS:
		/*TODO: ignored for now, will surely be usefull later on. */
		break;
	case B_LAB:
		for (i = 0; i < employees_in_building->numEmployees; i++) {
			employee = &gd.employees[employees_in_building->assigned[i]];
			employee->lab = -1;
		}
		employees_in_building->numEmployees = 0;
		break;
	case B_WORKSHOP:
		for (i = 0; i < employees_in_building->numEmployees; i++) {
			employee = &gd.employees[employees_in_building->assigned[i]];
			employee->workshop = -1;
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

/**
 * @brief Returns true if the employee is only assigned to quarters but not to labs and workshops.
 *
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 */
qboolean B_EmployeeIsFree(employee_t * employee)
{
	return ((employee->lab < 0) && (employee->workshop < 0));
}

/**
 * @brief Add a free employee from the quarters to building_dest. (the employee will be linked to both of them)
 *
 * TODO: Add check for destination building vs. employee_type and abort if they do not match.
 * TODO: Possibility to add employees to quarters (from the global list)
 *
 * @param[in] building_dest Which building to assign the employee to.
 * @param[in] employee_type	What type of employee to assign to the building.
 * @sa B_RemoveEmployee
 * @return Returns true if adding was possible/sane otherwise false.
 */
qboolean B_AssignEmployee(building_t * building_dest, employeeType_t employee_type)
{
	int i, j;
	employee_t *employee = NULL;
	building_t *building_source = NULL;
	employees_t *employees_in_building_dest = NULL;
	employees_t *employees_in_building_source = NULL;

	if (!baseCurrent) {
		Com_DPrintf("B_AssignEmployee: No Base set\n");
		return qfalse;
	}

	if (building_dest->buildingType == B_QUARTERS) {
		Com_DPrintf("B_AssignEmployee: No need to move from quarters to quarters.\n");
		return qfalse;
	}

	employees_in_building_dest = &building_dest->assigned_employees;
	employee = NULL;
	/* check if there is enough space to add one employee in the destination building. */
	if (employees_in_building_dest->numEmployees < employees_in_building_dest->maxEmployees) {
		/* get free employee from quarters in current base */
		for (i = 0; i < gd.numBuildings[building_dest->base_idx]; i++) {
			building_source = &gd.buildings[building_dest->base_idx][i];

			/* check if there is a free employee in the quarters. */
			if (building_source->buildingType == B_QUARTERS) {
				employees_in_building_source = &building_source->assigned_employees;
				for (j = 0; j < employees_in_building_source->numEmployees; j++) {
					employee = &gd.employees[employees_in_building_source->assigned[j]];
					if ((employee->type == employee_type) && B_EmployeeIsFree(employee))
						break;
					else
						employee = NULL;
				}
			}
		}
		/* if an employee was found add it to to the destination building */
		if (employee) {
			employees_in_building_dest->assigned[employees_in_building_dest->numEmployees++] = employee->idx;
			employee->lab = building_dest->idx;
			return qtrue;
		} else {
			Com_Printf("No employee available in this base.\n");
		}
	} else {
		Com_Printf("No free room in destination building \"%s\".\n", building_dest->id);
	}
	return qfalse;
}

/**
 * @brief Remove one employee from building.
 *
 * TODO: Add check for destination building vs. employee_type and abort if they do not match.
 * @sa B_AssignEmployee
 * @param[in] building Which building to remove the employee from. Can be any type of building that has employees in it. If quarters are given the employee will be removed from every other building as well.
 *
 * @return Returns true if adding was possible/sane otherwise false.
 */
qboolean B_RemoveEmployee(building_t * building)
{
	/* TODO
	int i;
	*/
	employee_t *employee = NULL;
	employees_t *employees_in_building = NULL;

	/* TODO
	building_t *building_temp = NULL;
	qboolean found = qfalse;
	*/

	employees_in_building = &building->assigned_employees;

	if (employees_in_building->numEmployees <= 0) {
		Com_DPrintf("B_RemoveEmployee: No employees in building. Can't remove one. %s\n", building->id);
		return qfalse;
	}

	/* Check where else (which buildings) the employee needs to be removed. */
	switch (building->buildingType) {
#if 0
		/* TODO */
	case B_QUARTERS:
		/* unlink the employee from quarters and every other building. (he is now only stored in the global list) */
		employee->quarters = NULL;

		/*remove the employee from the other buildings (i.e their employee-list) they are listed as well before unlinking them. */
		if (employee->lab) {
			building_temp = employee->lab;
			employees_in_building = &building_temp->assigned_employees;
			found = qfalse;
			for (i = 0; i < (employees_in_building->numEmployees - 1); i++) {
				if ((employees_in_building->assigned[i] == employee) || found) {
					employees_in_building->assigned[i] = employees_in_building->assigned[i + 1];
					found = qtrue;
				}
			}
			if (found)
				employees_in_building->numEmployees--;
			employee->lab = NULL;
		}

		/* TODO */
		/*  if ( employee->workshop ) { */
		/*  } */

		return qtrue;
		/*break; */
#endif
	case B_LAB:
		employees_in_building->numEmployees--;	/* remove the employee from the list of assigned workers in the building. */
		employee = &gd.employees[employees_in_building->assigned[employees_in_building->numEmployees]];	/* get the last employee in the building. */
		Com_DPrintf("B_RemoveEmployee: %s\n", building->id);
		/* unlink the employee from lab (the current building). */
		employee->lab = -1;
		Com_DPrintf("B_RemoveEmployee: %s 2\n", building->id);
		return qtrue;
		/* break; */
#if 0
		/* TODO */
	case B_WORKSHOP:
		/* unlink the employee from workshop (the current building). */
		employee->workshop = NULL;
		return qtrue;
		break;
		EMPL_MEDIC EMPL_ROBOT
#endif
	default:
		break;
	}

	return qfalse;
}

/**
 * @brief Returns the number of employees in the given base (in the quaters) of the given type.
 * @sa B_EmployeesInBase
 * You can choose (free_only) if you want the number of free employees or the total number.
 * If you call the function with employee_type set to MAX_EMPL it will return every type of employees.
 */
int B_EmployeesInBase2(int base_idx, employeeType_t employee_type, qboolean free_only)
{
	int i, j;
	int numEmployeesInBase = 0;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if (!baseCurrent) {
		Com_DPrintf("B_EmployeesInBase2: No Base set.\n");
		return 0;
	}

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == B_QUARTERS) {
			/* quarters found */
			employees_in_building = &building->assigned_employees;

			/*loop trough building and add to numEmployeesInBase if a match is found. */
			for (j = 0; j < employees_in_building->numEmployees; j++) {
				employee = &gd.employees[employees_in_building->assigned[j]];
				if (((employee_type == employee->type) || (employee_type == MAX_EMPL))
					&& (B_EmployeeIsFree(employee) || !free_only))
					numEmployeesInBase++;
			}
		}
	}
	return numEmployeesInBase;
}

/**
 * @brief Returns the number of employees in the given base (in the quaters) of the given type.
 * @sa B_EmployeesInBase2
 * If you call the function with employee_type set to MAX_EMPL it will return every type of employees.
 */
int B_EmployeesInBase(int base_idx, employeeType_t employee_type)
{
	return B_EmployeesInBase2(base_idx, employee_type, qfalse);
}

/**
 * @brief Clears a base?
 */
void B_ClearBase(base_t * base)
{
	int row, col, i;

	memset(base, 0, sizeof(base_t));

	CL_ResetCharacters(base);

	/* setup team */
	if (!curCampaign) {
		/* should be multiplayer */
		while (base->numWholeTeam < cl_numnames->value)
			CL_GenerateCharacter(Cvar_VariableString("team"), base, ET_ACTOR);
	} else {
		/* should be multiplayer (campaignmode TODO) or singleplayer */
		for (i = 0; i < curCampaign->soldiers; i++)
			CL_GenerateCharacter(curCampaign->team, base, ET_ACTOR);
		for (i = 0; i < curCampaign->ugvs; i++)
			CL_GenerateCharacter("ugv", base, ET_UGV);
	}

	for (row = BASE_SIZE - 1; row >= 0; row--)
		for (col = BASE_SIZE - 1; col >= 0; col--)
			base->map[row][col] = -1;
}

/**
 * @brief Reads information about bases.
 */
void B_ParseBases(char *title, char **text)
{
	char *errhead = "B_ParseBases: unexptected end of file (names ";
	char *token;
	base_t *base;

	gd.numBaseNames = 0;

	/* get token */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("B_ParseBases: base \"%s\" without body ignored\n", title);
		return;
	}
	do {
		/* add base */
		if (gd.numBaseNames > MAX_BASES) {
			Com_Printf("B_ParseBases: too many bases\n");
			return;
		}

		/* get the name */
		token = COM_EParse(text, errhead, title);
		if (!*text)
			break;
		if (*token == '}')
			break;

		base = &gd.bases[gd.numBaseNames];
		memset(base, 0, sizeof(base_t));
		base->idx = gd.numBaseNames;
		base->buildingToBuild = -1;
		memset(base->map, -1, sizeof(int) * BASE_SIZE * BASE_SIZE);

		/* get the title */
		token = COM_EParse(text, errhead, title);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (*token == '_')
			token++;
		Q_strncpyz(base->name, _(token), MAX_VAR);
		Com_DPrintf("Found base %s\n", base->name);
		B_ResetBuildingCurrent();
		gd.numBaseNames++;
	} while (*text);

	mn_base_title = Cvar_Get("mn_base_title", "", 0);
}

/**
 * @brief Draws a base.
 */
void B_DrawBase(menuNode_t * node)
{
	float x, y;
	int mx, my, width, height, row, col, time;
	qboolean hover = qfalse;
	static vec4_t color = { 0.5f, 1.0f, 0.5f, 1.0 };
	char image[MAX_QPATH];
	building_t *building = NULL, *secondBuilding = NULL, *hoverBuilding = NULL;

	if (!baseCurrent)
		Cbuf_ExecuteText(EXEC_NOW, "mn_pop");

	width = node->size[0] / BASE_SIZE;
	height = (node->size[1] + BASE_SIZE * 20) / BASE_SIZE;

	IN_GetMousePos(&mx, &my);

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			/* 20 is the height of the part where the images overlap */
			x = node->pos[0] + col * width;
			y = node->pos[1] + row * height - row * 20;

			baseCurrent->posX[row][col] = x;
			baseCurrent->posY[row][col] = y;

			if (baseCurrent->map[row][col] >= 0) {
				building = B_GetBuildingByIdx(baseCurrent->map[row][col]);
				secondBuilding = NULL;

				if (!building)
					Sys_Error("Error in DrawBase - no building with id %i\n", baseCurrent->map[row][col]);

				if (!building->used) {
					if (*building->needs)
						building->used = 1;
					if (*building->image) {	/* TODO:DEBUG */
						Q_strncpyz(image, building->image, MAX_QPATH);
					} else {
						/*Com_DPrintf( "B_DrawBase: no image found for building %s / %i\n",building->id ,building->idx ); */
					}
				} else if (*building->needs) {
					secondBuilding = B_GetBuildingType(building->needs);
					if (!secondBuilding)
						Sys_Error("Error in ufo-scriptfile - could not find the needed building\n");
					Q_strncpyz(image, secondBuilding->image, MAX_QPATH);
					building->used = 0;
				}
			} else {
				building = NULL;
				Q_strncpyz(image, "base/grid", MAX_QPATH);
			}

			if (mx > x && mx < x + width && my > y && my < y + height - 20) {
				hover = qtrue;
				if (baseCurrent->map[row][col] >= 0)
					hoverBuilding = building;
			} else
				hover = qfalse;

			if (*image)
				re.DrawNormPic(x, y, width, height, 0, 0, 0, 0, 0, qfalse, image);

			/* only draw for first part of building */
			if (building && !secondBuilding) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
					time = building->buildTime - (ccs.date.day - building->timeStart);
					re.FontDrawString("f_small", 0, x + 10, y + 10, node->size[0], va(_("%i days left"), time));
					break;
				default:
					break;
				}
			}
		}
	}
	if (hoverBuilding) {
		re.DrawColor(color);
		re.FontDrawString("f_small", 0, mx + 3, my, node->size[0], hoverBuilding->name);
		re.DrawColor(NULL);
	}
}

/**
 * @brief Initialises base.
 */
void B_BaseInit(void)
{
	int baseID = (int) Cvar_VariableValue("mn_base_id");

	baseCurrent = &gd.bases[baseID];

	/*these are the workers you can set on buildings */
	Cvar_SetValue("mn_available_workers", 0);

	Cvar_Set("mn_credits", va("%i c", ccs.credits));
}

/**
 * @brief Renames a base.
 */
void B_RenameBase(void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: rename_base <name>\n");
		return;
	}

	if (baseCurrent)
		Q_strncpyz(baseCurrent->name, Cmd_Argv(1), MAX_VAR);
}

/**
 * @brief Cycles to the next base.
 */
void B_NextBase(void)
{
	int baseID = (int) Cvar_VariableValue("mn_base_id");

	Com_DPrintf("cur-base=%i num-base=%i\n", baseID, gd.numBases);
	if (baseID < gd.numBases - 1)
		baseID++;
	else
		baseID = 0;
	Com_DPrintf("new-base=%i\n", baseID);
	if (!gd.bases[baseID].founded)
		return;
	else {
		Cbuf_AddText(va("mn_select_base %i\n", baseID));
		Cbuf_Execute();
	}
}

/**
 * @brief Cycles to the previous base.
 */
void B_PrevBase(void)
{
	int baseID = (int) Cvar_VariableValue("mn_base_id");

	Com_DPrintf("cur-base=%i num-base=%i\n", baseID, gd.numBases);
	if (baseID > 0)
		baseID--;
	else
		baseID = gd.numBases - 1;
	Com_DPrintf("new-base=%i\n", baseID);

	/* this must be false - but i'm paranoid' */
	if (!gd.bases[baseID].founded)
		return;
	else {
		Cbuf_AddText(va("mn_select_base %i\n", baseID));
		Cbuf_Execute();
	}
}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 *
 * For a new base the baseID is -1.
 */
void B_SelectBase(void)
{
	int baseID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_select_base <baseID>\n");
		return;
	}
	baseID = atoi(Cmd_Argv(1));

	/* set up a new base */
	/* called from *.ufo with -1 */
	if (baseID < 0) {
		gd.mapAction = MA_NEWBASE;
		baseID = gd.numBases;
		Com_DPrintf("B_SelectBase: new baseID is %i\n", baseID);
		if (baseID < MAX_BASES) {
			baseCurrent = &gd.bases[baseID];
			baseCurrent->idx = baseID;
			Com_DPrintf("B_SelectBase: baseID is valid for base: %s\n", baseCurrent->name);
		} else {
			Com_Printf("MaxBases reached\n");
			/* select the first base in list */
			baseCurrent = &gd.bases[0];
		}
	} else if (baseID < MAX_BASES) {
		Com_DPrintf("B_SelectBase: select base with id %i\n", baseID);
		baseCurrent = &gd.bases[baseID];
		if (baseCurrent->founded) {
			gd.mapAction = MA_NONE;
			MN_PushMenu("bases");
			CL_AircraftSelect();
		} else {
			gd.mapAction = MA_NEWBASE;
		}
	} else
		baseCurrent = &gd.bases[0];

	Cvar_SetValue("mn_base_id", baseCurrent->idx);
	Cvar_Set("mn_base_title", baseCurrent->name);
}

/* FIXME: This value is in menu_geoscape, too */
/*       make this variable?? */
#define BASE_COSTS 100000

/**
 * @brief Constructs a new base.
 *
 * TODO: First base needs to be constructed automatically.
 */
void B_BuildBase(void)
{
	assert(baseCurrent);

	/* FIXME: This should not be here - but we only build bases in singleplayer */
	ccs.singleplayer = qtrue;

	if (ccs.credits - BASE_COSTS > 0) {
		if (CL_NewBase(newBasePos)) {
			Com_DPrintf("B_BuildBase: numBases: %i\n", gd.numBases);
			baseCurrent->idx = gd.numBases - 1;
			baseCurrent->founded = qtrue;
			stats.basesBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - BASE_COSTS);
			Q_strncpyz(baseCurrent->name, mn_base_title->string, sizeof(baseCurrent->name));
			Cvar_SetValue("mn_base_id", baseCurrent->idx);
			Cvar_Set("mn_base_title", baseCurrent->name);
			Cbuf_AddText("mn_push bases\n");
			Q_strncpyz(messageBuffer, va(_("A new base has been built: %s."), mn_base_title->string), MAX_MESSAGE_TEXT);
			MN_AddNewMessage(_("Base built"), messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			B_SetUfoCountInSensor(baseCurrent);
			return;
		}
	} else {
		Q_strncpyz(messageBuffer, _("Not enough credits to set up a new base."), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Base built"), messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
	}
}


/**
 * @brief Initiates an attack on a base.
 */
void B_BaseAttack(void)
{
	int whichBaseID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: base_attack <baseID>\n");
		return;
	}

	whichBaseID = atoi(Cmd_Argv(1));

	if (whichBaseID >= 0 && whichBaseID < gd.numBases) {
		gd.bases[whichBaseID].baseStatus = BASE_UNDER_ATTACK;
		/* TODO: New menu for: */
		/*      defend -> call AssembleBase for this base */
		/*      continue -> return to geoscape */
		gd.mapAction = MA_BASEATTACK;
	}
#if 0							/*TODO: run eventhandler for each building in base */
	if (b->onAttack)
		Cbuf_AddText(va("%s %i", b->onAttack, b->id));
#endif

}

/**
 * @brief Builds a base map for tactical combat.
 * @sa SV_AssembleMap
 * @sa B_BaseAttack
 * NOTE: Do we need day and night maps here, too?
 * TODO: Search a empty field and add a alien craft there.
 * FIXME: We need to get rid of the tunnels to nivana.
 */
void B_AssembleMap(void)
{
	int row, col;
	char baseMapPart[1024];
	building_t *entry;
	char maps[2024];
	char coords[2048];

	*maps = '\0';
	*coords = '\0';

	if (!baseCurrent) {
		Com_Printf("B_AssembleMap: No base to assemble\n");
		return;
	}

	/* reset menu text */
	menuText[TEXT_STANDARD] = NULL;

	/* reset the used flag */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			if (baseCurrent->map[row][col] != -1) {
				entry = B_GetBuildingByIdx(baseCurrent->map[row][col]);
				entry->used = 0;
			}
		}

	/*TODO: If a building is still under construction, it will be assembled as a finished part */
	/*otherwise we need mapparts for all the maps under construction */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			baseMapPart[0] = '\0';

			if (baseCurrent->map[row][col] != -1) {
				entry = B_GetBuildingByIdx(baseCurrent->map[row][col]);

				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one */
				/* this is why we check the used flag and continue if it was set already */
				if (!entry->used && *entry->needs) {
					entry->used = 1;
				} else if (*entry->needs) {
					Com_DPrintf("B_AssembleMap: '%s' needs '%s' (used: %i)\n", entry->id, entry->needs, entry->used );
					entry->used = 0;
					continue;
				}

				if (*entry->mapPart)
					Q_strncpyz(baseMapPart, va("b/%c/%s", baseCurrent->mapChar, entry->mapPart), sizeof(baseMapPart));
				else
					Com_Printf("B_AssembleMap: Error - map has no mapPart set. Building '%s'\n'", entry->id);
			} else
				Q_strncpyz(baseMapPart, va("b/%c/empty", baseCurrent->mapChar), sizeof(baseMapPart));

			if (*baseMapPart) {
				Q_strcat(maps, baseMapPart, sizeof(maps));
				Q_strcat(maps, " ", sizeof(maps));
				/* basetiles are 16 units in each direction */
				Q_strcat(coords, va("%i %i ", col * 16, row * 16), sizeof(coords));
			}
		}
	Cbuf_AddText(va("map \"%s\" \"%s\"\n", maps, coords));
}

/**
 * @brief Cleans all bases but restart the base names
 */
void B_NewBases(void)
{
	/* reset bases */
	int i;
	char title[MAX_VAR];

	for (i = 0; i < MAX_BASES; i++) {
		Q_strncpyz(title, gd.bases[i].name, MAX_VAR);
		B_ClearBase(&gd.bases[i]);
		Q_strncpyz(gd.bases[i].name, title, MAX_VAR);
	}
}

/**
 * @brief Builds a random base
 *
 * call B_AssembleMap with a random base over script command 'base_assemble'
 */
static void B_AssembleRandomBase(void)
{
	Cbuf_AddText(va("base_assemble %i", rand() % gd.numBases));
}

/**
 * @brief Just lists all buildings with their data
 *
 * Just for debugging purposes - not needed in game
 * TODO: To be extended for load/save purposes
 */
static void B_BuildingList_f(void)
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

			Com_Printf("...Building: %s #%i - id: %i\n", building->id, B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, building->buildingType),
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

/**
 * @brief Just lists all bases with their data
 *
 * Just for debugging purposes - not needed in game
 * TODO: To be extended for load/save purposes
 */
static void B_BaseList_f(void)
{
	int i, row, col, j;
	base_t *base;

	for (i = 0, base = gd.bases; i < MAX_BASES; i++, base++) {
		Com_Printf("Base id %i\n", base->idx);
		Com_Printf("Base title %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base sensorWidth %s\n", base->sensorWidth);
		Com_Printf("Base numSensoredAircraft %i\n", base->numSensoredAircraft);
		Com_Printf("Base aircraft %i\n", base->numAircraftInBase);
		for (j = 0; j < base->numAircraftInBase; j++) {
			Com_Printf("Base aircraft-team %i\n", *(base->aircraft[j].teamSize));
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

/**
 * @brief Sets the title of the base.
 */
static void B_SetBaseTitle(void)
{
	Com_DPrintf("B_SetBaseTitle: #bases: %i\n", gd.numBases);
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
static void B_ChangeBaseNameCmd(void)
{
	/* maybe called without base initialized or active */
	if (!baseCurrent)
		return;

	Q_strncpyz(baseCurrent->name, Cvar_VariableString("mn_base_title"), MAX_VAR);
}

/**
 * @brief Function that does the real employee adding
 * @param[in] b Pointer to building
 * @param[in] amount Amount of employees to assign
 * @sa B_BuildingAddEmployees
 * @sa B_BuildingRemoveEmployees
 * @sa B_BuildingAddEmployees_f
 */
static int B_BuildingAddEmployees(building_t *b, employeeType_t type, int amount)
{
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if ( gd.numEmployees >= MAX_EMPLOYEES ) {
		Com_Printf("Employee limit hit\n");
		return 0;
	}

	if ( type >= MAX_EMPL )
		return 0;

	employees_in_building = &b->assigned_employees;
	if (employees_in_building->maxEmployees <= 0) {
		Com_Printf("No employees for this building: '%s'\n", b->id);
		return 0;
	}

	for (employees_in_building->numEmployees = 0; employees_in_building->numEmployees < amount;) {
		/* assign random employee infos. */
		/* link this employee in the building to the global employee-list. */
		employees_in_building->assigned[employees_in_building->numEmployees] = gd.numEmployees;
		employee = &gd.employees[gd.numEmployees];
		memset(employee, 0, sizeof(employee_t));
		employee->idx = gd.numEmployees;
		employee->type = type;

		employees_in_building->numEmployees++;
		gd.numEmployees++;
	}
	return 0;
}

/**
 * @brief
 * @sa B_BuildingAddEmployees
 * Script function for adding new employees to a building
 */
void B_BuildingAddEmployees_f ( void )
{
	employeeType_t type;

	/* can be called from everywhere - so make a sanity check here */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: building_add_employee <type> <amount>\n");
		return;
	}

	type = B_GetEmployeeType(Cmd_Argv(1));
	if (type == MAX_EMPL)
		return;

	if (!B_BuildingAddEmployees(baseCurrent->buildingCurrent, type, atoi(Cmd_Argv(2))))
		Com_DPrintf("Employees not added - at least not all\n");
}

/**
 * @brief Remove some employees from the current building
 * @param amount[in] Amount of employees you would like to remove from the current building
 * @sa B_BuildingAddEmployees
 * @sa B_BuildingRemoveEmployees_f
 *
 * @note baseCurrent and baseCurrent->buildingCurrent may not be NULL
 */
static int B_BuildingRemoveEmployees (building_t *b, employeeType_t type, int amount)
{
	if ( type >= MAX_EMPL )
		return 0;
	return 0;
}

/**
 * @brief
 * @sa B_BuildingRemoveEmployees
 * Script function for removing employees from a building
 */
void B_BuildingRemoveEmployees_f ( void )
{
	employeeType_t type;
	/* can be called from everywhere - so make a sanity check here */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: building_remove_employee <type> <amount>\n");
		return;
	}

	type = B_GetEmployeeType(Cmd_Argv(1));
	if (type == MAX_EMPL)
		return;

	if (!B_BuildingRemoveEmployees(baseCurrent->buildingCurrent, type, atoi(Cmd_Argv(2))))
		Com_DPrintf("Employees not removed - at least not all\n");
}

/**
 * @brief Resets console commands.
 */
void B_ResetBaseManagement(void)
{
	Com_DPrintf("Reset basemanagement\n");

	/* add commands and cvars */
	Cmd_AddCommand("mn_prev_base", B_PrevBase);
	Cmd_AddCommand("mn_next_base", B_NextBase);
	Cmd_AddCommand("mn_select_base", B_SelectBase);
	Cmd_AddCommand("mn_build_base", B_BuildBase);
	Cmd_AddCommand("new_building", B_NewBuildingFromList);
	Cmd_AddCommand("set_building", B_SetBuilding);
	Cmd_AddCommand("mn_setbasetitle", B_SetBaseTitle);
	Cmd_AddCommand("building_add_employee", B_BuildingAddEmployees_f );
	Cmd_AddCommand("building_remove_employee", B_BuildingRemoveEmployees_f );
	Cmd_AddCommand("rename_base", B_RenameBase);
	Cmd_AddCommand("base_attack", B_BaseAttack);
	Cmd_AddCommand("base_changename", B_ChangeBaseNameCmd);
	Cmd_AddCommand("base_init", B_BaseInit);
	Cmd_AddCommand("base_assemble", B_AssembleMap);
	Cmd_AddCommand("base_assemble_rand", B_AssembleRandomBase);
	Cmd_AddCommand("building_init", B_BuildingInit);
	Cmd_AddCommand("building_status", B_BuildingStatus);
	Cmd_AddCommand("buildinginfo_click", B_BuildingInfoClick_f);
	Cmd_AddCommand("buildings_click", B_BuildingClick_f);
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent);
	Cmd_AddCommand("baselist", B_BaseList_f);
	Cmd_AddCommand("buildinglist", B_BuildingList_f);
}

/**
 * @brief Counts the number of bases.
 *
 * @return The number of found bases.
 */
int B_GetCount(void)
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
 */
void B_UpdateBaseData(void)
{
	building_t *b = NULL;
	int i, j;
	int newBuilding = 0, new;

	for (i = 0; i < MAX_BASES; i++) {
		if (!gd.bases[i].founded)
			continue;
		for (j = 0; j < gd.numBuildings[i]; j++) {
			b = &gd.buildings[i][j];
			if (!b)
				continue;
			new = B_CheckBuildingConstruction(b, i);
			newBuilding += new;
			if (new) {
				Com_sprintf(messageBuffer, MAX_MESSAGE_TEXT, _("Construction of %s building finished in base %s."), b->name, gd.bases[i].name);
				MN_AddNewMessage(_("Building finished"), messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			}
		}
	}
	CL_CheckResearchStatus();
}

/**
 * @brief Checks whether the construction of a building is finished.
 *
 * Calls the onConstruct functions and assign workers, too.
 */
int B_CheckBuildingConstruction(building_t * building, int base_idx)
{
	int newBuilding = 0;

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
		if (building->timeStart && (building->timeStart + building->buildTime) <= ccs.date.day) {
			building->buildingStatus = B_STATUS_WORKING;

			if (building->onConstruct)
				Cbuf_AddText(va("%s %i", building->onConstruct, base_idx));

			newBuilding++;
		}
	}
	if (newBuilding)
		/*update the building-list */
		B_BuildingInit();

	return newBuilding;
}

/**
 * @brief Selects a base by its index.
 */
base_t *B_GetBase(int idx)
{
	int i;

	for (i = 0; i < MAX_BASES; i++) {
		if (gd.bases[i].idx == idx)
			return &gd.bases[i];
	}
	return NULL;
}

/**
 * @brief Counts the number of soldiers in a team?
 */
int B_GetNumOnTeam(void)
{
	if (!baseCurrent)
		return 0;
	return baseCurrent->numOnTeam[baseCurrent->aircraftCurrent];
}
