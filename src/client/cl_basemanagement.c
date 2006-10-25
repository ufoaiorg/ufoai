/**
 * @file cl_basemanagement.c
 * @brief Handles everything that is located in or accessed trough a base.
 *
 * See "base/ufos/basemanagement.ufo", "base/ufos/menu_bases.ufo" and "base/ufos/menu_buildings.ufo" for the underlying content.
 * @todo New game does not reset basemangagement
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

vec2_t newBasePos;
static cvar_t *mn_base_title;
static cvar_t *mn_base_count;

static int BuildingConstructionList[MAX_BUILDINGS];
static int numBuildingConstructionList;

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu but also several times from cl_basemanagement.c.
 */
void B_ResetBuildingCurrent(void)
{
	if (Cmd_Argc() > 2) {
		Com_Printf("Usage: reset_building_current [arg]\n");
		return;
	}

	if (Cmd_Argc() > 1)
		gd.instant_build = atoi(Cmd_Argv(1));

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
	{"max_employees", V_INT, offsetof(building_t, maxEmployees)},	/**< How many employees to hire on construction in the first base. */

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
		RADAR_ChangeRange(&(base->radar), amount);	/* inc_sensor */
	else if (!Q_strncmp(Cmd_Argv(0), "dec", 3))
		RADAR_ChangeRange(&(base->radar), -amount);	/* dec_sensor */
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
 * @brief  Hires some employees of appropriate type for a building
 * @param building	in which building
 * @param num  how many employees, if -1, hire building->maxEmployees
 *
 * @sa B_SetUpBase
 */
void B_HireForBuilding (building_t * building, int num)
{
	employeeType_t employeeType;

	assert(baseCurrent);

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
		case B_QUARTERS:
			return;
		case B_MISC:
			Com_DPrintf("B_HireForBuilding: Misc bulding type: %s with employees: %i.\n", building->buildingType, num);
			return;
		default:
			Com_DPrintf("B_HireForBuilding: Unknown bulding type: %s.\n", building->buildingType);
			return;
		}
		if (num > gd.numEmployees[employeeType])
			num = gd.numEmployees[employeeType];
		for (;num--;)
			if (!E_HireEmployee(baseCurrent, employeeType, -1)) {
				Com_DPrintf("B_HireForBuilding: Hiring %i employee(s) of type %i failed.\n", num, employeeType);
				return;
			}
	}
}

/**
 * @brief Checks whether a building as status B_STATUS_WORKING and sets hasLab, hasHospital and so on
 */
void B_UpdateBaseBuildingStatus(building_t* building, base_t* base, buildingStatus_t status)
{
	assert(base);
	assert(building);

	building->buildingStatus = status;

	switch (building->buildingType) {
	case B_LAB:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasLab = qtrue;
		break;
	case B_HOSPITAL:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasHospital = qtrue;
		break;
	case B_HANGAR:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasHangar = qtrue;
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

	/* this cvar is used for disabling the base build button on geoscape if MAX_BASES (8) was reached */
	Cvar_SetValue("mn_base_count", mn_base_count->value + 1.0f);

	for (i = 0; i < gd.numBuildingTypes; i++) {
		if (gd.buildingTypes[i].autobuild
			|| (gd.numBases == 1
				&& gd.buildingTypes[i].firstbase
				&& cl_start_buildings->value)) {
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
			/* fake a click to basemap */
			B_SetBuildingByClick((int) building->pos[0], (int) building->pos[1]);
			B_UpdateBaseBuildingStatus(building, baseCurrent, B_STATUS_WORKING);

			/* now call the onconstruct trigger */
			if (*building->onConstruct) {
				baseCurrent->buildingCurrent = building;
				Com_DPrintf("B_SetUpBase: %s %i;\n", building->onConstruct, baseCurrent->idx);
				Cbuf_AddText(va("%s %i;", building->onConstruct, baseCurrent->idx));
			}
			/*
			   if ( building->moreThanOne
			   && building->howManyOfThisType < BASE_SIZE*BASE_SIZE )
			   building->howManyOfThisType++;
			 */

			/* update the building-list */
			B_BuildingInit();

			if (cl_start_employees->value)
				B_HireForBuilding(building, -1);
		}
	}
	/* if no autobuild, set up zero build time for the first base */
	if (gd.numBases == 1 && !cl_start_buildings->value)
		gd.instant_build = 1;
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
/*		if ( building->dependsBuilding >= 0) */
/*			baseCurrent->map[building->dependsBuilding->pos[0]][building->dependsBuilding->pos[1]] = -1; */
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
	if (!B_CheckCredits(baseCurrent->buildingCurrent->fixCosts)) {
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
	if (!gd.instant_build) {
		baseCurrent->buildingCurrent->buildingStatus = B_STATUS_UNDER_CONSTRUCTION;
		baseCurrent->buildingCurrent->timeStart = ccs.date.day;
	} else {
		B_UpdateBaseBuildingStatus(baseCurrent->buildingCurrent, baseCurrent, B_STATUS_WORKING);
	}

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
		/* credits are updated in the construct function */
		if (B_ConstructBuilding()) {
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
			if (*baseCurrent->buildingCurrent->needs)
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
			/* credits are updated here, too */
			B_NewBuilding();

			baseCurrent->map[row][col] = baseCurrent->buildingCurrent->idx;

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

	/*maybe someone call this command before the buildings are parsed?? */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	menuText[TEXT_BUILDING_INFO] = buildingText;
	*menuText[TEXT_BUILDING_INFO] = '\0';

	building = baseCurrent->buildingCurrent;

	B_BuildingStatus();

	Com_sprintf(menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, va("%s\n", building->name));

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION && building->fixCosts)
		Com_sprintf(menuText[TEXT_BUILDING_INFO], MAX_LIST_CHAR, _("Costs:\t%1.0f c\n"), building->fixCosts);

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("%i Day(s) to build\n"), building->buildTime), MAX_LIST_CHAR );

	if (building->varCosts)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("Running Costs:\t%1.0f c\n"), building->varCosts), MAX_LIST_CHAR);

/*	if (employees_in_building->numEmployees)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("Employees:\t%i\n"), employees_in_building->numEmployees), MAX_LIST_CHAR);*/

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
	Com_DPrintf("B_GetNumOfBuildType: base: '%s' - num_b: %i - type_idx: %s\n", gd.bases[base_idx].name, NumberOfBuildings, gd.buildingTypes[type_idx].id);
	return NumberOfBuildings;
}

/**
 * @brief Get the maximum status of a building.
 *
 * This function is mostly used to check if the construction of a building with a given type is finished.
 * e.g.: "if (B_GetMaximumBuildingStatus(base_idx, B_LAB) >= B_STATUS_CONSTRUCTION_FINISHED) { ... }"
 *
 * @param[in] base_idx Which base
 * @param[in] buildingType Which buildingtype
 * @return The max./highest building status found.
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
 * @brief Gets the building in a given base by its index
 *
 * @param[in] base Pointer to base_t (base has to be founded already)
 * @param[in] buildingID Pointer to char
 * @return buildings_t pointer to gd.buildings
 */
building_t *B_GetBuildingInBase(base_t* base, char* buildingID)
{
	int row, col;

	if (base || !base->founded)
		return NULL;

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (!Q_strncmp(gd.buildings[base->idx][base->map[row][col]].id, buildingID, MAX_VAR))
				return &gd.buildings[base->idx][base->map[row][col]];

	Com_Printf("B_GetBuildingInBase: Building '%s' not found\n", buildingID);
	/* just that there are no warnings */
	return NULL;
}

/**
 * @brief Opens up the 'pedia if you right click on a building in the list.
 *
 * @todo Really only do this on rightclick.
 * @todo Left click should show building-status.
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

	if (Cmd_Argc() < 2 || !baseCurrent) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

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
	technology_t *tech_link = NULL;
	value_t *edp = NULL;
	char *errhead = "B_ParseBuildings: unexptected end of file (names ";
	char *token = NULL;
#if 0
	char *split = NULL;
	int employeesAmount = 0, i;
	employee_t* employee;
#endif
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
				} else if (!Q_strncmp(token, "hospital", MAX_VAR)) {
					building->buildingType = B_HOSPITAL;
				} else if (!Q_strncmp(token, "hangar", MAX_VAR)) {
					building->buildingType = B_HANGAR;
				} else if (!Q_strncmp(token, "quarters", MAX_VAR)) {
					building->buildingType = B_QUARTERS;
				} else if (!Q_strncmp(token, "workshop", MAX_VAR)) {
					building->buildingType = B_WORKSHOP;
				}
/*			} else if (!Q_strncmp(token, "max_employees", MAX_VAR)) {
				token = COM_EParse(text, errhead, id);
				if (!*text)
					return;
				employees_in_building = &building->assigned_employees;

				if (*token)
					employees_in_building->maxEmployees = atoi(token);
				else {
					employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
					Com_Printf("Set max employees to %i for building '%s'\n", MAX_EMPLOYEES_IN_BUILDING, building->id);
				}*/
			} else
				/* no linking yet */
				if (!Q_strncmp(token, "depends", MAX_VAR)) {
					token = COM_EParse(text, errhead, id);
					if (!*text)
						return;
				} else {
				for (edp = valid_vars; edp->string; edp++)
					if (!Q_strncmp(token, edp->string, sizeof(edp->string))) {
						/* found a definition */
						token = COM_EParse(text, errhead, id);
						if (!*text)
							return;

						Com_ParseValue(building, token, edp->type, edp->ofs);
						break;
					}
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
			if (building->visible)
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

	for (i = 0; i < gd.numBuildingTypes; i++) {
		building = &gd.buildingTypes[i];
		if (building->buildingType == type) {
			/* found correct building-type */
/*			employees_in_building = &building->assigned_employees;
			if (employees_in_building->numEmployees < employees_in_building->maxEmployees) {*/
				/* the bulding has free space for employees */
				/*return building;
			}*/
		}
	}
	/* no buildings available at all, no correct building type found or no building free */
	return NULL;
}

/**
 * @brief Gets a lab in the given base
 * @note You can run more than one research in a lab
 *
 * @param[in] base_id The number/id of the base to search in.
 *
 * @return The lab or NULL if base has no lab
 */
building_t *B_GetLab(int base_idx)
{
	int i;
	building_t *building = NULL;

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == B_LAB)
			return building;
	}
	return NULL;
}

/**
 * @brief Clears a base with all its characters
 * @sa CL_ResetCharacters
 * @sa CL_GenerateCharacter
 */
void B_ClearBase(base_t *const base)
{
	int row, col, i;

	CL_ResetCharacters(base);

	memset(base, 0, sizeof(base_t));

	/* only go further if we have a active campaign */
	if (!curCampaign)
		return;

	/* setup team */
	if (!E_CountUnhired(base, EMPL_SOLDIER)) {
		/* should be multiplayer (campaignmode TODO) or singleplayer */
		Com_DPrintf("B_ClearBase: create %i soldiers\n", curCampaign->soldiers);
		for (i = 0; i < curCampaign->soldiers; i++)
			E_CreateEmployee(EMPL_SOLDIER);
		Com_DPrintf("B_ClearBase: create %i scientists\n", curCampaign->scientists);
		for (i = 0; i < curCampaign->scientists; i++)
			E_CreateEmployee(EMPL_SCIENTIST);
		Com_DPrintf("B_ClearBase: create %i robots\n", curCampaign->ugvs);
		for (i = 0; i < curCampaign->ugvs; i++)
			E_CreateEmployee(EMPL_ROBOT);
		Com_DPrintf("B_ClearBase: create %i workers\n", curCampaign->workers);
		for (i = 0; i < curCampaign->workers; i++)
			E_CreateEmployee(EMPL_WORKER);
		Com_DPrintf("B_ClearBase: create %i medics\n", curCampaign->medics);
		for (i = 0; i < curCampaign->medics; i++)
			E_CreateEmployee(EMPL_MEDIC);
	}

	for (row = BASE_SIZE - 1; row >= 0; row--)
		for (col = BASE_SIZE - 1; col >= 0; col--)
			base->map[row][col] = -1;
}

/**
 * @brief Reads information about bases.
 * @sa CL_ParseScriptFirst
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

	mn_base_title = Cvar_Get("mn_base_title", "", 0, NULL);
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
					re.FontDrawString("f_small", 0, x + 10, y + 10, x + 10, y + 10, node->size[0], 0, node->texh[0], va(_("%i days left"), time));
					break;
				default:
					break;
				}
			}
		}
	}
	if (hoverBuilding) {
		re.DrawColor(color);
		re.FontDrawString("f_small", 0, mx + 3, my, mx + 3, my, node->size[0], 0, node->texh[0], hoverBuilding->name);
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

	Cvar_SetValue("mn_soldiers_in_base", E_CountHired(baseCurrent, EMPL_SOLDIER));
	Cvar_SetValue("mn_scientists_in_base", E_CountHired(baseCurrent, EMPL_SCIENTIST));

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
 * @sa B_PrevBase
 * @sa B_SelectBase
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
 * @sa B_NextBase
 * @sa B_SelectBase
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
			Cbuf_ExecuteText(EXEC_NOW, "set_base_to_normal");
		} else {
			Com_Printf("MaxBases reached\n");
			/* select the first base in list */
			baseCurrent = &gd.bases[0];
			gd.mapAction = MA_NONE;
		}
	} else if (baseID < MAX_BASES) {
		Com_DPrintf("B_SelectBase: select base with id %i\n", baseID);
		baseCurrent = &gd.bases[baseID];
		if (baseCurrent->founded) {
			gd.mapAction = MA_NONE;
			MN_PushMenu("bases");
			CL_AircraftSelect();
			switch (baseCurrent->baseStatus) {
			case BASE_UNDER_ATTACK:
				Cvar_Set("mn_base_status_name", _("Base is under attack"));
				Cbuf_ExecuteText(EXEC_NOW, "set_base_under_attack");
				break;
			default:
				Cbuf_ExecuteText(EXEC_NOW, "set_base_to_normal");
				break;
			}
		} else {
			gd.mapAction = MA_NEWBASE;
		}
	} else
		return;

	/* activate or deactivate the aircraft button */
	if (baseCurrent->numAircraftInBase <= 0)
		Cbuf_ExecuteText(EXEC_NOW, "set_base_no_aircraft");
	else
		Cbuf_ExecuteText(EXEC_NOW, "set_base_aircraft");

	Cvar_SetValue("mn_base_status_id", baseCurrent->baseStatus);
	Cvar_SetValue("mn_base_num_aircraft", baseCurrent->numAircraftInBase);
	Cvar_SetValue("mn_base_id", baseCurrent->idx);
	Cvar_Set("mn_base_title", baseCurrent->name);
}

#undef RIGHT
#undef HOLSTER
#define RIGHT(e) ((e)->inv->c[csi.idRight])
#define HOLSTER(e) ((e)->inv->c[csi.idHolster])
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 */
static void CL_SwapSkills(character_t *team[], int num)
{
	int j, i1, i2, skill, no1, no2, tmp1, tmp2;
	character_t *cp1, *cp2;

	j = num;
	while (j--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (i1 = 0; i1 < num - 1; i1++) {
				cp1 = team[i1];

				/* disregard left hand, or dual-wielding guys are too good */
				no1 = 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[FD_PRIMARY].weaponSkill)
					+ 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[FD_SECONDARY].weaponSkill)
					+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
					   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[FD_PRIMARY].weaponSkill)
					+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
					   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[FD_SECONDARY].weaponSkill);

				for (i2 = i1 + 1 ; i2 < num; i2++) {
					cp2 = team[i2];

					no2 = 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[FD_PRIMARY].weaponSkill)
						+ 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[FD_SECONDARY].weaponSkill)
						+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[FD_PRIMARY].weaponSkill)
						+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[FD_SECONDARY].weaponSkill);

					if ( no1 > no2 /* more use of this skill */
						 || (no1 && no1 == no2) ) { /* or earlier on list */
						tmp1 = cp1->skills[skill];
						tmp2 = cp2->skills[skill];
						cp1->skills[skill] = MAX(tmp1, tmp2);
						cp2->skills[skill] = MIN(tmp1, tmp2);

						switch (skill) {
						case SKILL_CLOSE:
							tmp1 = cp1->skills[ABILITY_SPEED];
							tmp2 = cp2->skills[ABILITY_SPEED];
							cp1->skills[ABILITY_SPEED] = MAX(tmp1, tmp2);
							cp2->skills[ABILITY_SPEED] = MIN(tmp1, tmp2);
							break;
						case SKILL_HEAVY:
							tmp1 = cp1->skills[ABILITY_POWER];
							tmp2 = cp2->skills[ABILITY_POWER];
							cp1->skills[ABILITY_POWER] = MAX(tmp1, tmp2);
							cp2->skills[ABILITY_POWER] = MIN(tmp1, tmp2);
							break;
						case SKILL_ASSAULT:
							/* no related basic attribute */
							break;
						case SKILL_SNIPER:
							tmp1 = cp1->skills[ABILITY_ACCURACY];
							tmp2 = cp2->skills[ABILITY_ACCURACY];
							cp1->skills[ABILITY_ACCURACY] = MAX(tmp1, tmp2);
							cp2->skills[ABILITY_ACCURACY] = MIN(tmp1, tmp2);
							break;
						case SKILL_EXPLOSIVE:
							tmp1 = cp1->skills[ABILITY_MIND];
							tmp2 = cp2->skills[ABILITY_MIND];
							cp1->skills[ABILITY_MIND] = MAX(tmp1, tmp2);
							cp2->skills[ABILITY_MIND] = MIN(tmp1, tmp2);
							break;
						default:
							Sys_Error("CL_SwapSkills: illegal skill %i.\n", skill);
						}
					} else if (no1 < no2) {
						tmp1 = cp1->skills[skill];
						tmp2 = cp2->skills[skill];
						cp2->skills[skill] = MAX(tmp1, tmp2);
						cp1->skills[skill] = MIN(tmp1, tmp2);

						switch (skill) {
						case SKILL_CLOSE:
							tmp1 = cp1->skills[ABILITY_SPEED];
							tmp2 = cp2->skills[ABILITY_SPEED];
							cp2->skills[ABILITY_SPEED] = MAX(tmp1, tmp2);
							cp1->skills[ABILITY_SPEED] = MIN(tmp1, tmp2);
							break;
						case SKILL_HEAVY:
							tmp1 = cp1->skills[ABILITY_POWER];
							tmp2 = cp2->skills[ABILITY_POWER];
							cp2->skills[ABILITY_POWER] = MAX(tmp1, tmp2);
							cp1->skills[ABILITY_POWER] = MIN(tmp1, tmp2);
							break;
						case SKILL_ASSAULT:
							break;
						case SKILL_SNIPER:
							tmp1 = cp1->skills[ABILITY_ACCURACY];
							tmp2 = cp2->skills[ABILITY_ACCURACY];
							cp2->skills[ABILITY_ACCURACY] = MAX(tmp1, tmp2);
							cp1->skills[ABILITY_ACCURACY] = MIN(tmp1, tmp2);
							break;
						case SKILL_EXPLOSIVE:
							tmp1 = cp1->skills[ABILITY_MIND];
							tmp2 = cp2->skills[ABILITY_MIND];
							cp2->skills[ABILITY_MIND] = MAX(tmp1, tmp2);
							cp1->skills[ABILITY_MIND] = MIN(tmp1, tmp2);
							break;
						default:
							Sys_Error("CL_SwapSkills: illegal skill %i.\n", skill);
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Assigns initial team of soldiers with equipment to aircraft
 * @note If assign_initial is called with one parameter (e.g. a 1), this is for
 * multiplayer - assign_initial with no parameters is for singleplayer
 */
static void B_AssignInitial_f(void)
{
	int i;

	/* check syntax */
	if (Cmd_Argc() > 2) {
		Com_Printf("Usage: assign_initial [<multiplayer>]\n");
		return;
	}

	if (!baseCurrent)
		return;

	if (Cmd_Argc() == 2) {
		CL_ResetTeamInBase();
		Cvar_Set("mn_teamname", _("NewTeam"));
		Cbuf_AddText("gennames;");
	}

	for (i = MAX_TEAMLIST; --i >= 0;)
		Cbuf_AddText(va("team_hire %i;", i));

	Cbuf_AddText("pack_initial;");
	if (Cmd_Argc() == 2) {
		MN_PushMenu("team");
	}
}

/**
 * @brief Assigns initial soldier equipment for the first base
 */
static void B_PackInitialEquipment_f(void)
{
	int i, price = 0;
	equipDef_t *ed;
	character_t *cp;
	char *name = curCampaign ? cl_initial_equipment->string : Cvar_VariableString("equip");

	/* check syntax */
	if (Cmd_Argc() > 1) {
		Com_Printf("Usage: pack_initial\n");
		return;
	}

	if (!baseCurrent)
		return;

	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(name, ed->name, MAX_VAR))
			break;
	if (i == csi.numEDs) {
		Com_DPrintf("B_PackInitialEquipment_f: Initial Phalanx equipment %s not found.\n", name);
	} else {
		for (i = 0; i < baseCurrent->teamNum[baseCurrent->aircraftCurrent]; i++) {
			cp = baseCurrent->curTeam[i];
			/* pack equipment */
			Com_DPrintf("B_PackInitialEquipment_f: Packing initial equipment for %s.\n", cp->name);
			Com_EquipActor(cp->inv, ed->num, name);
		}
		CL_AddCarriedToEq(&baseCurrent->storage);
		CL_SwapSkills(baseCurrent->curTeam, baseCurrent->teamNum[baseCurrent->aircraftCurrent]);

		/* pay for the initial equipment */
		for (i = 0; i < csi.numODs; i++)
			price += baseCurrent->storage.num[i] * csi.ods[i].price;
		CL_UpdateCredits(ccs.credits - price);
	}
}

/* FIXME: This value is in menu_geoscape, too */
/*		 make this variable?? */
#define BASE_COSTS 100000

/**
 * @brief Constructs a new base.
 * @sa CL_NewBase
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
			baseCurrent->numAircraftInBase = 0;
			stats.basesBuild++;
			gd.mapAction = MA_NONE;
			CL_UpdateCredits(ccs.credits - BASE_COSTS);
			Q_strncpyz(baseCurrent->name, mn_base_title->string, sizeof(baseCurrent->name));
			Q_strncpyz(messageBuffer, va(_("A new base has been built: %s."), mn_base_title->string), MAX_MESSAGE_TEXT);
			MN_AddNewMessage(_("Base built"), messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
			Radar_Initialise(&(baseCurrent->radar), 0);

			/* initial base equipment */
			if (gd.numBases == 1) {
				int i, price = 0;
				equipDef_t *ed;

				for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
					if (!Q_strncmp(curCampaign->equipment, ed->name, MAX_VAR))
						break;
				if (i != csi.numEDs)
					baseCurrent->storage = *ed; /* copied, including arrays! */

				/* initial soldiers and their equipment */
				if (cl_start_employees->value) {
					Cbuf_AddText("assign_initial;");
				} else {
					char *name = cl_initial_equipment->string;

					for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
						if (!Q_strncmp(name, ed->name, MAX_VAR))
							break;
					if (i == csi.numEDs) {
						Com_DPrintf("B_BuildBase: Initial Phalanx equipment %s not found.\n", name);
					} else {
						for (i = 0; i < csi.numODs; i++)
							baseCurrent->storage.num[i] += ed->num[i] / 5;
					}
				}
				/* pay for the initial equipment */
				for (i = 0; i < csi.numODs; i++)
					price += baseCurrent->storage.num[i] * csi.ods[i].price;
				CL_UpdateCredits(ccs.credits - price);
			}

			Cbuf_AddText(va("mn_select_base %i;", baseCurrent->idx));
			return;
		}
	} else {
		Q_strncpyz(messageBuffer, _("Not enough credits to set up a new base."), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Base built"), messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
	}
}

/**
 * @brief Sets the baseStatus to BASE_NOT_USED
 * @param[in] base Which base should be resetted?
 */
void B_BaseResetStatus (base_t* const base)
{
	assert(base);
	base->baseStatus = BASE_NOT_USED;
	if (gd.mapAction == MA_BASEATTACK)
		gd.mapAction = MA_NONE;
}

/**
 * @brief Initiates an attack on a base.
 * @sa B_BaseAttack_f
 * @param[in] base Which base is under attack?
 */
void B_BaseAttack (base_t* const base)
{
	assert(base);
	base->baseStatus = BASE_UNDER_ATTACK;
#if 0							/*TODO: run eventhandler for each building in base */
	if (b->onAttack)
		Cbuf_AddText(va("%s %i", b->onAttack, b->id));
#endif
}

/**
 * @brief Initiates an attack on a base.
 * @sa B_BaseAttack
 */
static void B_BaseAttack_f(void)
{
	int whichBaseID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: base_attack <baseID>\n");
		return;
	}

	whichBaseID = atoi(Cmd_Argv(1));

	if (whichBaseID >= 0 && whichBaseID < gd.numBases) {
		B_BaseAttack(&gd.bases[whichBaseID]);
	}
}

/**
 * @brief Builds a base map for tactical combat.
 * @sa SV_AssembleMap
 * @sa B_BaseAttack
 * @note Do we need day and night maps here, too? Sure!
 * @todo Search a empty field and add a alien craft there, also add alien
 * spawn points around the craft, also some trees, etc. for their cover
 * @todo Add soldier spawn points, the best place is quarters.
 * @todo We need to get rid of the tunnels to nirvana.
 */
void B_AssembleMap(void)
{
	int row, col;
	char baseMapPart[1024];
	building_t *entry;
	char maps[2024];
	char coords[2048];
	int setUnderAttack = 0, baseID = 0;
	base_t* base = baseCurrent;

	if (Cmd_Argc() < 2)
		Com_DPrintf("Usage: %s <baseID> <setUnderAttack>\n", Cmd_Argv(0));
	else {
		if (Cmd_Argc() == 3)
			setUnderAttack = atoi(Cmd_Argv(2));
		baseID = atoi(Cmd_Argv(1));
		if (baseID < 0 || baseID >= gd.numBases) {
			Com_DPrintf("Invalid baseID: %i\n", baseID);
			return;
		}
		base = &gd.bases[baseID];
	}

	if (!base) {
		Com_Printf("B_AssembleMap: No base to assemble\n");
		return;
	}

	if (setUnderAttack) {
		base->baseStatus = BASE_UNDER_ATTACK;
		gd.mapAction = MA_BASEATTACK;
		Com_DPrintf("Set base %i under attack\n", base->idx);
	}

	/* reset menu text */
	menuText[TEXT_STANDARD] = NULL;

	*maps = '\0';
	*coords = '\0';

	/* reset the used flag */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col] != -1) {
				entry = B_GetBuildingByIdx(base->map[row][col]);
				entry->used = 0;
			}
		}

	/*TODO: If a building is still under construction, it will be assembled as a finished part */
	/*otherwise we need mapparts for all the maps under construction */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			baseMapPart[0] = '\0';

			if (base->map[row][col] != -1) {
				entry = B_GetBuildingByIdx(base->map[row][col]);

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
					Q_strncpyz(baseMapPart, va("b/%c/%s", base->mapChar, entry->mapPart), sizeof(baseMapPart));
				else
					Com_Printf("B_AssembleMap: Error - map has no mapPart set. Building '%s'\n'", entry->id);
			} else
				Q_strncpyz(baseMapPart, va("b/%c/empty", base->mapChar), sizeof(baseMapPart));

			if (*baseMapPart) {
				Q_strcat(maps, baseMapPart, sizeof(maps));
				Q_strcat(maps, " ", sizeof(maps));
				/* basetiles are 16 units in each direction */
				Q_strcat(coords, va("%i %i ", col * 16, (BASE_SIZE - row - 1) * 16), sizeof(coords));
			}
		}
	/* set maxlevel for base attacks to 5 */
	map_maxlevel_base = 5;

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
 * call B_AssembleMap with a random base over script command 'base_assemble_rand'
 */
static void B_AssembleRandomBase(void)
{
	int setUnderAttack = 0;
	int randomBase = rand() % gd.numBases;
	if (Cmd_Argc() < 2)
		Com_DPrintf("Usage: %s <setUnderAttack>\n", Cmd_Argv(0));
	else
		setUnderAttack = atoi(Cmd_Argv(1));

	if (!gd.bases[randomBase].founded) {
		Com_Printf("Base with id %i was not founded or already destroyed\n", randomBase);
		return;
	}

	Cbuf_AddText(va("base_assemble %i %i", randomBase, setUnderAttack));
}

/**
 * @brief Just lists all buildings with their data
 *
 * Just for debugging purposes - not needed in game
 * @todo To be extended for load/save purposes
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
 * @todo To be extended for load/save purposes
 */
static void B_BaseList_f(void)
{
	int i, row, col, j;
	base_t *base;

	for (i = 0, base = gd.bases; i < MAX_BASES; i++, base++) {
		Com_Printf("Base id %i\n", base->idx);
		Com_Printf("Base title %s\n", base->name);
		Com_Printf("Base founded %i\n", base->founded);
		Com_Printf("Base numAircraftInBase %i\n", base->numAircraftInBase);
		Com_Printf("Base sensorWidth %i\n", base->radar.range);
		Com_Printf("Base numSensoredAircraft %i\n", base->radar.numUfos);
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
static void B_ChangeBaseName_f(void)
{
	/* maybe called without base initialized or active */
	if (!baseCurrent)
		return;

	Q_strncpyz(baseCurrent->name, Cvar_VariableString("mn_base_title"), MAX_VAR);
}

void B_CheckMaxBases_f(void)
{
	if (gd.numBases >= MAX_BASES) {
		MN_PopMenu(qfalse);
	}
}

/**
 * @brief Resets console commands.
 * @sa MN_ResetMenus
 */
void B_ResetBaseManagement(void)
{
	Com_DPrintf("Reset basemanagement\n");

	/* add commands and cvars */
	Cmd_AddCommand("mn_prev_base", B_PrevBase, NULL);
	Cmd_AddCommand("mn_next_base", B_NextBase, NULL);
	Cmd_AddCommand("mn_select_base", B_SelectBase, NULL);
	Cmd_AddCommand("mn_build_base", B_BuildBase, NULL);
	Cmd_AddCommand("new_building", B_NewBuildingFromList, NULL);
	Cmd_AddCommand("set_building", B_SetBuilding, NULL);
	Cmd_AddCommand("mn_setbasetitle", B_SetBaseTitle, NULL);
	Cmd_AddCommand("bases_check_max", B_CheckMaxBases_f, NULL);
	Cmd_AddCommand("rename_base", B_RenameBase, NULL);
	Cmd_AddCommand("base_attack", B_BaseAttack_f, NULL);
	Cmd_AddCommand("base_changename", B_ChangeBaseName_f, NULL);
	Cmd_AddCommand("base_init", B_BaseInit, NULL);
	Cmd_AddCommand("base_assemble", B_AssembleMap, NULL);
	Cmd_AddCommand("base_assemble_rand", B_AssembleRandomBase, NULL);
	Cmd_AddCommand("building_init", B_BuildingInit, NULL);
	Cmd_AddCommand("building_status", B_BuildingStatus, NULL);
	Cmd_AddCommand("buildinginfo_click", B_BuildingInfoClick_f, NULL);
	Cmd_AddCommand("buildings_click", B_BuildingClick_f, NULL);
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent, NULL);
	Cmd_AddCommand("baselist", B_BaseList_f, NULL);
	Cmd_AddCommand("buildinglist", B_BuildingList_f, NULL);
	Cmd_AddCommand("pack_initial", B_PackInitialEquipment_f, NULL);
	Cmd_AddCommand("assign_initial", B_AssignInitial_f, NULL);
	mn_base_count = Cvar_Get("mn_base_count", "0", 0, NULL);
}

/**
 * @brief Counts the number of bases.
 * @return The number of founded bases.
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
			B_UpdateBaseBuildingStatus(building, &gd.bases[base_idx], B_STATUS_WORKING);

			if (*building->onConstruct) {
				gd.bases[base_idx].buildingCurrent = building;
				Com_DPrintf("B_CheckBuildingConstruction: %s %i;\n", building->onConstruct, base_idx);
				Cbuf_AddText(va("%s %i;", building->onConstruct, base_idx));
			}

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
 * @brief Counts the number of soldiers in a current aircraft/team.
 */
int B_GetNumOnTeam(void)
{
	if (!baseCurrent)
		return 0;
	return baseCurrent->teamNum[baseCurrent->aircraftCurrent];
}

/**
 * @brief
 * @param[in] base
 * @param[in] index
 */
aircraft_t *B_GetAircraftFromBaseByIndex(base_t* base,int index)
{
	if (index<base->numAircraftInBase) {
		return &base->aircraft[index];
	} else {
		Com_DPrintf("B_GetAircraftFromBaseByIndex: error: index bigger then number of aircrafts\n");
		return NULL;
	}
}

