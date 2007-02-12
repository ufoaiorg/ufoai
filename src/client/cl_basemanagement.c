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

vec3_t newBasePos;
static cvar_t *mn_base_title;
static cvar_t *mn_base_count;

static int BuildingConstructionList[MAX_BUILDINGS];
static int numBuildingConstructionList;

/**
 * @brief Count all employees (hired) in the given base
 */
extern int B_GetEmployeeCount (const base_t* const base)
{
	int cnt = 0;
	employeeType_t type;

	for (type = EMPL_SOLDIER; type < MAX_EMPL; type++)
		cnt += E_CountHired(base, type);
	Com_DPrintf("B_GetEmployeeCount: %i\n", cnt);

	return cnt;
}

/**
 * @brief Sums up max_employees quarter values
 */
extern int B_GetAvailableQuarterSpace (const base_t* const base)
{
	int cnt = 0, i;

	if (base->hasQuarters)
		for (i = 0; i < gd.numBuildings[base->idx]; i++) {
			if (gd.buildings[base->idx][i].buildingType == B_QUARTERS
			&& gd.buildings[base->idx][i].buildingStatus != B_STATUS_NOT_SET)
				cnt += gd.buildings[base->idx][i].maxEmployees;
		}
	Com_DPrintf("B_GetAvailableQuarterSpace: %i\n", cnt);

	return cnt;
}

/**
 * @brief
 */
static void B_ResetBuildingCurrent (void)
{
	if (baseCurrent) {
		baseCurrent->buildingCurrent = NULL;
		baseCurrent->buildingToBuild = -1;
	}
}

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu but also several times from cl_basemanagement.c.
 */
static void B_ResetBuildingCurrent_f (void)
{
	if (Cmd_Argc() == 2)
		gd.instant_build = atoi(Cmd_Argv(1));

	B_ResetBuildingCurrent();
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
	{"capacity", V_INT, offsetof(building_t, capacity)},	/**< A size value that is used by many buldings in a different way. */

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
 * @brief We are doing the real destroy of a buliding here
 * @sa B_BuildingDestroy
 */
static void B_BuildingDestroy_f (void)
{
	building_t *b1, *b2 = NULL;

	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	b1 = baseCurrent->buildingCurrent;

	if (baseCurrent->map[(int)b1->pos[0]][(int)b1->pos[1]] >= 0) {
		if (*b1->needs) {
			b2 = B_GetBuildingType(b1->needs);
			assert(b2);
			baseCurrent->map[(int)b2->pos[0]][(int)b2->pos[1]] = -1;
		}

		baseCurrent->map[(int)b1->pos[0]][(int)b1->pos[1]] = -1;
	}
	b1->buildingStatus = B_STATUS_NOT_SET;
	if (b2)
		b2->buildingStatus = B_STATUS_NOT_SET;

#if 0 /* this would be a clean approach - but we have to fix all the linkage */
	gd.numBuildings[baseCurrent->idx]--;

	if (b1->idx == gd.numBuildings[baseCurrent->idx]) {
	memset(b1, 0, sizeof(building_t));
	} else {
		for (i = b1->idx; i < gd.numBuildings[baseCurrent->idx]; i++) {
			Com_Printf("Move building %i to pos %i (%i)\n", i+1, i, gd.numBuildings[baseCurrent->idx]);
			memmove(&gd.buildings[baseCurrent->idx][i], &gd.buildings[baseCurrent->idx][i+1], sizeof(building_t));
			gd.buildings[baseCurrent->idx][i].idx = i;
		}
	}
#endif

	switch (b1->buildingType) {
	case B_WORKSHOP:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasWorkshop = qfalse;
		break;
	case B_STORAGE:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasStorage = qfalse;
		break;
	case B_ALIEN_CONTAINMENT:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasAlienCont = qfalse;
		break;
	case B_LAB:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasLab = qfalse;
		break;
	case B_HOSPITAL:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasHospital = qfalse;
		break;
	case B_HANGAR: /* the Dropship Hangar */
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasHangar = qfalse;
		break;
	case B_QUARTERS:
		if (!B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, b1->buildingType))
			baseCurrent->hasQuarters = qfalse;
		break;
	case B_MISC:
		break;
	default:
		Com_Printf("B_BuildingDestroy_f: Unknown bulding type: %i.\n", b1->buildingType);
		break;
	}

	/* call ondestroy trigger */
	if (*b1->onDestroy) {
		Com_DPrintf("B_BuildingDestroy: %s %i;\n", baseCurrent->buildingCurrent->onDestroy, baseCurrent->idx);
		Cbuf_AddText(va("%s %i;", baseCurrent->buildingCurrent->onDestroy, baseCurrent->idx));
	}

	B_ResetBuildingCurrent();
	B_BuildingStatus();
}

/**
 * @brief Mark a building for destruction - you only have to confirm it now
 * @note Also calls the ondestroy trigger
 */
extern void B_BuildingDestroy (building_t* building, base_t* base)
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
		NumberOfBuildings = B_GetNumberOfBuildingsInBaseByTypeIDX(baseCurrent->idx, baseCurrent->buildingCurrent->type_idx);
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
		case B_ALIEN_CONTAINMENT:
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
			Com_DPrintf("B_HireForBuilding: Misc bulding type: %i with employees: %i.\n", building->buildingType, num);
			return;
		default:
			Com_DPrintf("B_HireForBuilding: Unknown bulding type: %i.\n", building->buildingType);
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
	case B_ALIEN_CONTAINMENT:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasAlienCont = qtrue;
		break;
	case B_QUARTERS:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasQuarters = qtrue;
		break;
	case B_STORAGE:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasStorage = qtrue;
		break;
	case B_LAB:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasLab = qtrue;
		break;
	case B_WORKSHOP:
		if (building->buildingStatus == B_STATUS_WORKING)
			base->hasWorkshop = qtrue;
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
void B_SetBuildingByClick (int row, int col)
{
	int j;
	qboolean freeSlot = qfalse;
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
		/* search for a free slot - in case of building destruction */
		for (j = 0; j < gd.numBuildings[baseCurrent->idx]; j++) {
			if (gd.buildings[baseCurrent->idx][j].buildingStatus == B_STATUS_NOT_SET) {
				building = &gd.buildings[baseCurrent->idx][j];
				freeSlot = qtrue;
				break;
			}
		}
		if (!building)
			building = &gd.buildings[baseCurrent->idx][gd.numBuildings[baseCurrent->idx]];

		/* copy building from type-list to base-buildings-list */
		memcpy(building, &gd.buildingTypes[baseCurrent->buildingCurrent->type_idx], sizeof(building_t));

		if (!freeSlot) {
			/* self-link to building-list in base */
			building->idx = gd.numBuildings[baseCurrent->idx];
			gd.numBuildings[baseCurrent->idx]++;
		} else {
			/* use existing slot */
			building->idx = j;
		}

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
				/* where is this building located in our base? */
				secondBuildingPart->pos[1] = col + 1;
				secondBuildingPart->pos[0] = row;
			} else {
				baseCurrent->buildingToBuild = -1;
			}
			/* credits are updated here, too */
			B_NewBuilding();

			baseCurrent->map[row][col] = baseCurrent->buildingCurrent->idx;

			/* where is this building located in our base? */
			baseCurrent->buildingCurrent->pos[0] = row;
			baseCurrent->buildingCurrent->pos[1] = col;

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

	*buildingText = '\0';

	building = baseCurrent->buildingCurrent;

	B_BuildingStatus();

	Com_sprintf(buildingText, MAX_LIST_CHAR, va("%s\n", building->name));

	if (building->buildingStatus < B_STATUS_UNDER_CONSTRUCTION && building->fixCosts)
		Com_sprintf(buildingText, MAX_LIST_CHAR, _("Costs:\t%1.0f c\n"), building->fixCosts);

	if (building->buildingStatus == B_STATUS_UNDER_CONSTRUCTION)
		Q_strcat(buildingText, va(ngettext("%i Day to build\n", "%i Days to build\n", building->buildTime), building->buildTime), MAX_LIST_CHAR );

	if (building->varCosts)
		Q_strcat(buildingText, va(_("Running Costs:\t%1.0f c\n"), building->varCosts), MAX_LIST_CHAR);

/*	if (employees_in_building->numEmployees)
		Q_strcat(menuText[TEXT_BUILDING_INFO], va(_("Employees:\t%i\n"), employees_in_building->numEmployees), MAX_LIST_CHAR);*/

	/* FIXME: Rename mn_building_name and mn_building_title */
	if (building->id)
		Cvar_Set("mn_building_name", building->id);

	if (building->name)
		Cvar_Set("mn_building_title", building->name);

	/* link into menu text array */
	menuText[TEXT_BUILDING_INFO] = buildingText;
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
 */
int B_GetNumberOfBuildingsInBaseByTypeIDX(int base_idx, int type_idx)
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
	Com_DPrintf("B_GetNumOfBuildType: base: '%s' - num_b: %i - type_idx: %s\n", gd.bases[base_idx].name, NumberOfBuildings, gd.buildingTypes[type_idx].id);
	return NumberOfBuildings;
}

/**
 * @brief Counts the number of buildings of a particular type in a base.
 *
 * @param[in] base_idx Which base
 * @param[in] type Building type value
 * @sa B_GetNumberOfBuildingsInBaseByTypeIDX
 */
int B_GetNumberOfBuildingsInBaseByType(int base_idx, buildingType_t type)
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
		Com_Printf("B_GetMaximumBuildingStatus: Bad base-index given: %i (numbases %i)\n", base_idx, gd.numBases);
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
void B_BuildingInit (void)
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
			numSameBuildings = B_GetNumberOfBuildingsInBaseByTypeIDX(baseCurrent->idx, buildingType->type_idx);

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
 * @param[in] base Pointer to base_t (base has to be founded already)
 * @param[in] idx The index of the building in gd.buildings[]
 * @return buildings_t pointer to gd.buildings[idx]
 */
building_t *B_GetBuildingByIdx(base_t* base, int idx)
{
	if (base)
		return &gd.buildings[base->idx][idx];
	else
		Sys_Error("B_GetBuildingByIdx: Base not initialized\n");

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

	/* which building? */
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
				} else if (!Q_strncmp(token, "aliencont", MAX_VAR)) {
					building->buildingType = B_ALIEN_CONTAINMENT;
				} else if (!Q_strncmp(token, "workshop", MAX_VAR)) {
					building->buildingType = B_WORKSHOP;
				} else if (!Q_strncmp(token, "storage", MAX_VAR)) {
					building->buildingType = B_STORAGE;
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
	if (!E_CountUnhired(EMPL_SOLDIER)) {
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
				building = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
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
					re.FontDrawString("f_small", 0, x + 10, y + 10, x + 10, y + 10, node->size[0], 0, node->texh[0], va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse);
					break;
				default:
					break;
				}
			}
		}
	}
	if (hoverBuilding) {
		re.DrawColor(color);
		re.FontDrawString("f_small", 0, mx + 3, my, mx + 3, my, node->size[0], 0, node->texh[0], hoverBuilding->name, 0, 0, NULL, qfalse);
		re.DrawColor(NULL);
	}
}

/**
 * @brief Initialises base.
 */
void B_BaseInit(void)
{
	int baseID = Cvar_VariableInteger("mn_base_id");

	baseCurrent = &gd.bases[baseID];

	Cvar_SetValue("mn_medics_in_base", E_CountHired(baseCurrent, EMPL_MEDIC));
	Cvar_SetValue("mn_soldiers_in_base", E_CountHired(baseCurrent, EMPL_SOLDIER));
	Cvar_SetValue("mn_scientists_in_base", E_CountHired(baseCurrent, EMPL_SCIENTIST));

	Cvar_Set("mn_credits", va(_("%i c"), ccs.credits));
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
	int baseID = Cvar_VariableInteger("mn_base_id");

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
	int baseID = Cvar_VariableInteger("mn_base_id");

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
			CL_AircraftSelect(NULL);
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

	/**
	 * this is only needed when we are going to be show up the base
	 * in our base view port
	 */
	if (gd.mapAction != MA_NEWBASE) {
		/* activate or deactivate the aircraft button */
		if (baseCurrent->numAircraftInBase <= 0)
			Cbuf_ExecuteText(EXEC_NOW, "set_base_no_aircraft");
		else
			Cbuf_ExecuteText(EXEC_NOW, "set_base_aircraft");

		Cvar_SetValue("mn_base_status_id", baseCurrent->baseStatus);
		Cvar_SetValue("mn_base_prod_allowed", PR_ProductionAllowed());
		Cvar_SetValue("mn_base_num_aircraft", baseCurrent->numAircraftInBase);
		Cvar_SetValue("mn_base_id", baseCurrent->idx);
		Cvar_SetValue("mn_numbases", gd.numBases);
		if (gd.numBases > 1) {
			Cbuf_AddText("set_base_transfer;");
		} else {
			Cbuf_AddText("set_base_no_transfer;");
		}
		Cvar_Set("mn_base_title", baseCurrent->name);
	}
}

#undef RIGHT
#undef HOLSTER
#define RIGHT(e) ((e)->inv->c[csi.idRight])
#define HOLSTER(e) ((e)->inv->c[csi.idHolster])
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
/**
 * @brief Swaps skills of the initial team of soldiers so that they match inventories
 * @todo This currently always uses exactly the first two firemodes (see fmode1+fmode2) for calculation. This needs to be adapted to support less (1) or more 3+ firemodes. I think the function will even  break on only one firemode .. never tested it.
 * @todo i think currently also the different ammo/firedef types for each weapon (different weaponr_fd_idx and weaponr_fd_idx values) are ignored.
 */
static void CL_SwapSkills (character_t *team[], int num)
{
	int j, i1, i2, skill, no1, no2, tmp1, tmp2;
	character_t *cp1, *cp2;
	int weaponr_fd_idx, weaponh_fd_idx;
	const byte fmode1 = 0;
	const byte fmode2 = 1;

	j = num;
	while (j--) {
		/* running the loops below is not enough, we need transitive closure */
		/* I guess num times is enough --- could anybody prove this? */
		/* or perhaps 2 times is enough as long as weapons have 1 skill? */
		for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++) {
			for (i1 = 0; i1 < num - 1; i1++) {
				cp1 = team[i1];
				weaponr_fd_idx = -1;
				weaponh_fd_idx = -1;
				if (RIGHT(cp1) && RIGHT(cp1)->item.m != NONE && RIGHT(cp1)->item.t != NONE)
					weaponr_fd_idx = INV_FiredefsIDXForWeapon(&csi.ods[RIGHT(cp1)->item.m], RIGHT(cp1)->item.t);
				if (HOLSTER(cp1) && HOLSTER(cp1)->item.m != NONE && HOLSTER(cp1)->item.t != NONE)
					weaponh_fd_idx = INV_FiredefsIDXForWeapon(&csi.ods[HOLSTER(cp1)->item.m], HOLSTER(cp1)->item.t);
				/* disregard left hand, or dual-wielding guys are too good */

				/* FIXME This will crash if weaponh_fd_idx or weaponr_fd_idx is -1 */
				no1 = 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[weaponr_fd_idx][fmode1].weaponSkill)
					+ 2 * (RIGHT(cp1) && skill == csi.ods[RIGHT(cp1)->item.m].fd[weaponr_fd_idx][fmode2].weaponSkill)
					+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
					   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[weaponh_fd_idx][fmode1].weaponSkill)
					+ (HOLSTER(cp1) && csi.ods[HOLSTER(cp1)->item.t].reload
					   && skill == csi.ods[HOLSTER(cp1)->item.m].fd[weaponh_fd_idx][fmode2].weaponSkill);

				for (i2 = i1 + 1 ; i2 < num; i2++) {
					cp2 = team[i2];
					weaponr_fd_idx = -1;
					weaponh_fd_idx = -1;
					if (RIGHT(cp2) && RIGHT(cp2)->item.m != NONE && RIGHT(cp2)->item.t != NONE)
						weaponr_fd_idx = INV_FiredefsIDXForWeapon(&csi.ods[RIGHT(cp2)->item.m], RIGHT(cp2)->item.t);
					if (HOLSTER(cp2) && HOLSTER(cp2)->item.m != NONE && HOLSTER(cp2)->item.t != NONE)
						weaponh_fd_idx = INV_FiredefsIDXForWeapon(&csi.ods[HOLSTER(cp2)->item.m], HOLSTER(cp2)->item.t);

					/* FIXME This will crash if weaponh_fd_idx or weaponr_fd_idx is -1 */
					no2 = 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[weaponr_fd_idx][fmode1].weaponSkill)
						+ 2 * (RIGHT(cp2) && skill == csi.ods[RIGHT(cp2)->item.m].fd[weaponr_fd_idx][fmode2].weaponSkill)
						+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[weaponh_fd_idx][fmode1].weaponSkill)
						+ (HOLSTER(cp2) && csi.ods[HOLSTER(cp2)->item.t].reload
						   && skill == csi.ods[HOLSTER(cp2)->item.m].fd[weaponh_fd_idx][fmode2].weaponSkill);

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
static void B_AssignInitial_f (void)
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
static void B_PackInitialEquipment_f (void)
{
	int i, price = 0;
	equipDef_t *ed;
	character_t *chr;
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
			chr = baseCurrent->curTeam[i];
			/* pack equipment */
			Com_DPrintf("B_PackInitialEquipment_f: Packing initial equipment for %s.\n", chr->name);
			Com_EquipActor(chr->inv, ed->num, name, chr);
			Com_DPrintf("B_PackInitialEquipment_f: armor: %i, weapons: %i\n", chr->armor, chr->weapons);
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
void B_BuildBase (void)
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
			AL_FillInContainment();

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
				CL_GameTimeFast();
				CL_GameTimeFast();
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
		Com_Printf("B_AssembleMap_f: No base to assemble\n");
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
				entry = B_GetBuildingByIdx(base, base->map[row][col]);
				entry->used = 0;
			}
		}

	/* TODO: If a building is still under construction, it will be assembled as a finished part */
	/* otherwise we need mapparts for all the maps under construction */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			baseMapPart[0] = '\0';

			if (base->map[row][col] != -1) {
				entry = B_GetBuildingByIdx(base, base->map[row][col]);

				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one */
				/* this is why we check the used flag and continue if it was set already */
				if (!entry->used && *entry->needs) {
					entry->used = 1;
				} else if (*entry->needs) {
					Com_DPrintf("B_AssembleMap_f: '%s' needs '%s' (used: %i)\n", entry->id, entry->needs, entry->used );
					entry->used = 0;
					continue;
				}

				if (*entry->mapPart)
					Q_strncpyz(baseMapPart, va("b/%c/%s", base->mapChar, entry->mapPart), sizeof(baseMapPart));
				else
					Com_Printf("B_AssembleMap_f: Error - map has no mapPart set. Building '%s'\n'", entry->id);
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

/**
 * @brief Checks whether the building menu or the pedia entry should be called when baseCurrent->buildingCurrent is set
 */
void B_BuildingOpen_f (void)
{
	if (baseCurrent && baseCurrent->buildingCurrent) {
		if (baseCurrent->buildingCurrent->buildingStatus != B_STATUS_WORKING) {
			UP_OpenWith(baseCurrent->buildingCurrent->pedia);
		} else
			switch (baseCurrent->buildingCurrent->buildingType) {
			case B_LAB:
				MN_PushMenu("research");
				break;
			case B_HOSPITAL:
				MN_PushMenu("hospital");
				break;
			case B_ALIEN_CONTAINMENT:
				MN_PushMenu("aliencont");
				break;
			case B_QUARTERS:
				MN_PushMenu("employees");
				break;
			case B_WORKSHOP:
				MN_PushMenu("production");
				break;
			case B_HANGAR:
				MN_PushMenu("aircraft");
				break;
			default:
				UP_OpenWith(baseCurrent->buildingCurrent->pedia);
				break;
			}
	} else {
		Com_Printf("Usage: Only call me from baseview\n");
	}
}

/**
 * @brief
 */
void B_CheckMaxBases_f(void)
{
	if (gd.numBases >= MAX_BASES) {
		MN_PopMenu(qfalse);
	}
}

/**
 * Transfer menu functions
 */

/** @brief current selected base for transfer */
static base_t* transferBase = NULL;

/** @brief current selected aircraft */
static aircraft_t* transferAircraft = NULL;

/** @brief current transfer type (item, employee, tech, ...) */
static int transferType = -1;

/**
 * @brief
 * @sa B_TransferStart_f
 * @sa B_TransferInit_f
 */
static void B_TransferSelect_f (void)
{
	static char transferList[1024];
	int type;
	int i, cnt = 0;
	char str[128];

	if (!transferBase)
		return;

	if (Cmd_Argc() < 2)
		type = transferType;
	else
		type = atoi(Cmd_Argv(1));

	transferList[0] = '\0';

	switch (type) {
	/* items */
	case 0:
		if (transferBase->hasStorage) {
			for (i = 0; i < csi.numODs; i++)
				if (baseCurrent->storage.num[i]) {
					if (transferAircraft && transferAircraft->num[i])
						Com_sprintf(str, sizeof(str), "%s (%i on board, %i left)\n", csi.ods[i].name, transferAircraft->num[i], baseCurrent->storage.num[i]);
					else
						Com_sprintf(str, sizeof(str), "%s (%i available)\n", csi.ods[i].name, baseCurrent->storage.num[i]);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			if (!cnt)
				Q_strncpyz(transferList, _("Storage is empty\n"), sizeof(transferList));
		} else
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have a storage building"), sizeof(transferList));
		break;
	/* humans */
	case 1:
		if (transferBase->hasQuarters)
			Q_strcat(transferList, "TODO: employees", sizeof(transferList));
		else
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have quarters"), sizeof(transferList));
		break;
	/* techs */
	case 2:
		if (transferBase->hasLab)
			Q_strcat(transferList, "TODO: techs", sizeof(transferList));
		else
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have a lab"), sizeof(transferList));
		break;
	/* aliens */
	case 3:
		if (transferBase->hasAlienCont) {
			for (i = 0; i < numTeamDesc; i++) {
				if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
					if (transferAircraft && transferAircraft->num[i])
						Com_sprintf(str, sizeof(str), "%s (corpse of %i on board, %i left)\n",
						_(AL_AlienTypeToName(i)), transferAircraft->num[i],
						baseCurrent->alienscont[i].amount_dead);
					else
						Com_sprintf(str, sizeof(str), "%s (corpse of %i available)\n",
						_(AL_AlienTypeToName(i)), baseCurrent->alienscont[i].amount_dead);
					Q_strcat(transferList, str, sizeof(transferList));
					cnt++;
				}
			}
		} else {
			Q_strcat(transferList, _("Transfer is not possible - the base doesn't have an alien containment"), sizeof(transferList));
		}
		break;
	default:
		Com_Printf("B_TransferSelect_f: Unknown type id\n");
		return;
	}

	transferType = type;
	menuText[TEXT_TRANSFER_LIST] = transferList;
}

/**
 * @brief Unload everything from aircraft storage back to base storage
 */
static void B_TransferEmptyAircraftStorage_f (void)
{
	if (!transferAircraft)
		return;

	/* we can't unload if we are not in our homebase */
	if (transferAircraft->status != AIR_HOME) {
		MN_Popup(_("Notice"), _("Can't unload while we are not in the homebase"));
		return;
	}

	/* this is a transfer mission to the homebase (though we are still in the homebase) */
	transferAircraft->transferBase = (void*)transferAircraft->homebase;
	/* to pass the sanity check in B_TransferEnd */
	transferAircraft->status = AIR_TRANSPORT;
	B_TransferEnd(transferAircraft);
	transferAircraft->status = AIR_HOME;

	/* clear the command buffer
	 * needed to erase all B_TransferListSelect_f
	 * paramaters */
	Cmd_BufClear();
	B_TransferSelect_f();
}

/**
 * @brief Shows potential targets for aircraft on transfer mission
 */
extern void B_TransferAircraftMenu (aircraft_t* aircraft)
{
	int i;
	static char transferBaseSelectPopup[512];

	transferBaseSelectPopup[0] = '\0';
	transferAircraft = aircraft;

	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].founded)
			continue;
		Q_strcat(transferBaseSelectPopup, gd.bases[i].name, sizeof(transferBaseSelectPopup));
	}
	menuText[TEXT_LIST] = transferBaseSelectPopup;
	MN_PushMenu("popup_transferbaselist");
}

/**
 * @brief Ends the transfer and let the aircraft return to homebase
 */
extern void B_TransferEnd (aircraft_t* aircraft)
{
	base_t* b;
	base_t* homebase;
	int i;

	if (aircraft->status != AIR_TRANSPORT)
		return;

	b = (base_t*)aircraft->transferBase;
	assert(b);

	/* maybe it was invaded in the meantime */
	if (!b->founded) {
		MN_Popup(_("Notice"), _("The base is not longer existent"));
		return;
	}

	homebase = (base_t*)aircraft->homebase;
	assert(homebase);

	/* drop all equipment */
	if (b->hasStorage) {
		for (i = 0; i < csi.numODs; i++)
			if (transferAircraft->num[i]) {
				b->storage.num[i] += transferAircraft->num[i];
				transferAircraft->num[i] = 0;
			}
	} else {
		for (i = 0; i < csi.numODs; i++)
			if (transferAircraft->num[i]) {
				MN_Popup(_("Notice"), _("The base doesn't have a storage"));
				break;
			}
	}

	if (b->hasLab) {
		/* don't handle alien techs here - see below for hasAlienCont check */
		/* TODO unload the techs here */
	} else {
		/* TODO check whether we have techs on board */
		MN_Popup(_("Notice"), _("The base doesn't have a lab - can't unload the technologies here"));
	}

	if (b->hasQuarters) {
		/**
		 * first unhire this employee (this will also unlink the inventory from current base
		 * and remove him from any buildings he is currently assigned to) and then hire him
		 * again in the new base
		 */
		/*E_UnhireEmployee(homebase, ...)*/
		/*E_HireEmployee(b, ...)*/
		/* TODO unload the employees here */
	} else {
		/* TODO check whether we have employees on board */
		MN_Popup(_("Notice"), _("The base doesn't have quarters - can't unload the employees here"));
	}

	/* aliens are stored as techs - but seperatly checked for an alien containment is needed here */
	if (b->hasAlienCont) {
		/* TODO unload the alien techs here */
	} else {
		/* TODO check whether we have aliens on board */
		MN_Popup(_("Notice"), _("The base doesn't have an alien containment - can't unload the aliens here"));
	}

	MN_AddNewMessage(_("Transport mission"), _("Transport mission ended, returning to homebase now"), qfalse, MSG_TRANSFERFINISHED, NULL);
	CL_AircraftReturnToBase(aircraft);
}

/**
 * @brief
 * @sa B_TransferSelect_f
 * @sa B_TransferInit_f
 */
static void B_TransferStart_f (void)
{
	if (!transferAircraft) {
		Com_DPrintf("B_TransferStart_f: No aircraft selected\n");
		return;
	}

	if (!transferBase) {
		Com_DPrintf("B_TransferStart_f: No base selected\n");
		return;
	}

	if (transferAircraft->status != AIR_HOME) {
		MN_Popup(_("Notice"), _("Can't start the transport mission while we are not in the homebase"));
		return;
	}

	transferAircraft->status = AIR_TRANSPORT;
	transferAircraft->transferBase = transferBase;

	MAP_MapCalcLine(transferAircraft->pos, transferBase->pos, &transferAircraft->route);
	/* leave to geoscape */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

/**
 * @brief Adds a thing to aircraft for transfer by left mouseclick.
 * @sa B_TransferSelect_f
 * @sa B_TransferInit_f
 */
static void B_TransferListSelect_f (void)
{
	int num, cnt = 0, i;

	if (!baseCurrent)
		return;

	if (!transferBase) {
		MN_Popup(_("No target base selected"), _("Please select the target base from the list"));
		return;
	}

	if (!transferAircraft) {
		MN_Popup(_("No aircraft selected"), _("Please select the aircraft to use from the list"));
		return;
	}

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	switch (transferType) {
	case -1:
		/* no list was inited before you call this */
		return;
	/* items */
	case 0:
		for (i = 0; i < csi.numODs; i++)
			if (baseCurrent->storage.num[i]) {
				if (cnt == num) {
					/* TODO: Check space */
					transferAircraft->num[i]++;
					/* remove it from base storage */
					baseCurrent->storage.num[i]--;
					break;
				}
				cnt++;
			}
		break;
	/* humans */
	case 1:
		break;
	/* techs */
	case 2:
		break;
	/* aliens */
	case 3:
		for (i = 0; i < numTeamDesc; i++) {
			if (baseCurrent->alienscont[i].alientype && baseCurrent->alienscont[i].amount_dead > 0) {
				if (cnt == num) {
					/* TODO: Check space */
					transferAircraft->num[i]++;
					/* Remove the corpse from Alien Containment. */
					baseCurrent->alienscont[i].amount_dead--;
					break;
				}
				cnt++;
			}
		}
		break;
	}

	/* clear the command buffer
	 * needed to erase all B_TransferListSelect_f
	 * paramaters */
	Cmd_BufClear();
	B_TransferSelect_f();
}

/**
 * @brief Display current selected aircraft info in transfer menu
 */
static void B_TransferDisplayAircraftInfo (void)
{
	if (!transferAircraft)
		return;

	/* TODO */
}

/**
 * @brief Callback for aircraft list click
 */
static void B_TransferAircraftListClick_f (void)
{
	int i, j = -1, num;
	aircraft_t* aircraft;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			j++;
			if (j == num)
				break;
		}
	}

	if (j < 0)
		return;

	transferAircraft = aircraft;
	Cvar_Set("mn_trans_aircraft_name", transferAircraft->shortname);

	B_TransferDisplayAircraftInfo();
}

/**
 * @brief
 */
static void B_TransferBaseSelectPopup_f (void)
{
	int i, j = 0, num;
	base_t* base;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_baselist_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* no base founded */
	if (j < 0)
		return;

	transferBase = base;
}

/**
 * @brief Callback for base list click
 */
static void B_TransferBaseSelect_f (void)
{
	static char baseInfo[1024];
	/*char str[128];*/
	int j = -1, num, i;
	base_t* base;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: trans_bases_click <type>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		j++;
		if (j == num) {
			break;
		}
	}

	/* no base founded */
	if (j < 0) {
		menuText[TEXT_BASE_INFO] = NULL;
		return;
	}

	Com_sprintf(baseInfo, sizeof(baseInfo), "%s\n\n", base->name);

	if (base->hasStorage) {
		/* TODO: Check whether they are free */
		Q_strcat(baseInfo, _("You can transfer equipment - this base has a storage building\n"), sizeof(baseInfo));
		/*Com_sprintf(str, sizeof(str), _(""), base->);*/
	} else {
		Q_strcat(baseInfo, _("No storage building in this base\n"), sizeof(baseInfo));
	}
	if (base->hasQuarters) {
		/* TODO: Check whether they are free */
		Q_strcat(baseInfo, _("You can transfer employees - this base has quarters\n"), sizeof(baseInfo));
		/*Com_sprintf(str, sizeof(str), _(""), base->);*/
	} else {
		Q_strcat(baseInfo, _("No quarters in this base\n"), sizeof(baseInfo));
	}
	if (base->hasLab) {
		Q_strcat(baseInfo, _("You can transfer techs - this base has a laboratory\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No laboratory in this base\n"), sizeof(baseInfo));
	}
	if (base->hasAlienCont ) {
		Q_strcat(baseInfo, _("You can transfer aliens - this base has alien containment\n"), sizeof(baseInfo));
	} else {
		Q_strcat(baseInfo, _("No alien containment in this base\n"), sizeof(baseInfo));
	}

	menuText[TEXT_BASE_INFO] = baseInfo;

	/* set global pointer to current selected base */
	transferBase = base;

	Cvar_Set("mn_trans_base_name", transferBase->name);

	/* update item list */
	B_TransferSelect_f();
}

/**
 * @brief Callback for transfer menu init function
 * @sa B_TransferStart_f
 * @sa B_TransferSelect_f
 */
static void B_TransferInit_f (void)
{
	static char baseList[1024];
	static char aircraftList[1024];
	int i;
	base_t* base;
	aircraft_t* aircraft;

	if (!baseCurrent)
		return;

	transferAircraft = NULL;
	transferBase = NULL;

	baseList[0] = '\0';
	aircraftList[0] = '\0';

	if (Cmd_Argc() < 2)
		Com_Printf("warning: you should call trans_init with parameter 0\n");

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++) {
		if (base->founded == qfalse || base == baseCurrent)
			continue;
		Q_strcat(baseList, base->name, sizeof(baseList));
		Q_strcat(baseList, "\n", sizeof(baseList));
	}

	/* select the first base */
	B_TransferBaseSelect_f();

	/* select first item list */
	B_TransferSelect_f();

	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		aircraft = &baseCurrent->aircraft[i];
		if (aircraft->status == AIR_HOME) {
			/* first suitable aircraft will be default selection */
			if (!transferAircraft)
				transferAircraft = aircraft;
			Q_strcat(aircraftList, aircraft->shortname, sizeof(aircraftList));
			Q_strcat(aircraftList, "\n", sizeof(aircraftList));
		}
	}

	if (transferAircraft)
		Cvar_Set("mn_trans_aircraft_name", transferAircraft->shortname);
	if (transferBase)
		Cvar_Set("mn_trans_base_name", transferBase->name);

	menuText[TEXT_BASE_LIST] = baseList;
	menuText[TEXT_AIRCRAFT_LIST] = aircraftList;
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
	Cmd_AddCommand("base_assemble", B_AssembleMap_f, "Called to assemble the current selected base");
	Cmd_AddCommand("base_assemble_rand", B_AssembleRandomBase, NULL);
	Cmd_AddCommand("building_open", B_BuildingOpen_f, NULL);
	Cmd_AddCommand("building_init", B_BuildingInit, NULL);
	Cmd_AddCommand("building_status", B_BuildingStatus, NULL);
	Cmd_AddCommand("building_destroy", B_BuildingDestroy_f, "Function to destroy a buliding (select via right click in baseview first)");
	Cmd_AddCommand("buildinginfo_click", B_BuildingInfoClick_f, NULL);
	Cmd_AddCommand("buildings_click", B_BuildingClick_f, NULL);
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent_f, NULL);
	Cmd_AddCommand("baselist", B_BaseList_f, NULL);
	Cmd_AddCommand("buildinglist", B_BuildingList_f, NULL);
	Cmd_AddCommand("pack_initial", B_PackInitialEquipment_f, NULL);
	Cmd_AddCommand("assign_initial", B_AssignInitial_f, NULL);

	Cmd_AddCommand("trans_start", B_TransferStart_f, "Starts the tranfer");
	Cmd_AddCommand("trans_select", B_TransferSelect_f, "Switch between transfer types (employees, techs, items)");
	Cmd_AddCommand("trans_emptyairstorage", B_TransferEmptyAircraftStorage_f, "Unload everything from aircraft storage back to base storage");
	Cmd_AddCommand("trans_init", B_TransferInit_f, "Init transfer menu");
	Cmd_AddCommand("trans_baselist_click", B_TransferBaseSelectPopup_f, "Callback for transfer base list popup");
	Cmd_AddCommand("trans_bases_click", B_TransferBaseSelect_f, "Callback for base list node click");
	Cmd_AddCommand("trans_list_click", B_TransferListSelect_f, "Callback for transfer list node click");
	Cmd_AddCommand("trans_aircraft_click", B_TransferAircraftListClick_f, "Callback for aircraft list node click");
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

/**
 * @brief Do anything when dropship returns to base
 * @param[in] aircraft
 * @param[in] base
 * @note Place here any stuff, which should be called
 * when Drophip returns to base.
 * @sa CL_CampaignRunAircraft
 */
void CL_DropshipReturned(base_t* base, aircraft_t* aircraft)
{
	baseCurrent = base;
	AL_AddAliens(); /* Add aliens to Alien Containment */
	AL_CountAll(); /* Count all alive aliens */
	RS_MarkResearchable(); /* Mark new technologies researchable */
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage .. and currently market) in the given base.
 * @param[in] item_idx The index of the item in the item-list.
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 * @todo TODO/FIXME: Make this work _only_ on base-storage, no market. See the comment in the code.
 * @note Formerly known as RS_ItemInBase.
 */
int B_ItemInBase(int item_idx, base_t *base)
{
	equipDef_t *ed = NULL;

	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	/* Com_DPrintf("B_ItemInBase: DEBUG idx %s\n",  csi.ods[item_idx].id); */

	/* FIXME/TODO: currently since all alien artifacts are added to the
	 * market, this check ensures market items are researchable too...
	 * otherwise the user must buy each item before researching it.
	 * Suggestion: if an unknown alien tech is found, sell all but the
	 * required number of items to perform research on that tech. Then
	 * the eMarket addition below would not be required.
	 * Right now this function is just confusing for every part
	 * of the code that uses it since they get higher numbers than are really in the base (as one would assume from the func.-name) */
	return ed->num[item_idx] + ccs.eMarket.num[item_idx];
}

/**
 * @brief Check if the item has been collected (i.e it is in the storage) in the given base.
 * @param[in] item_idx The index of the item in the item-list.
 * @param[in] base The base to search in.
 * @return amount Number of available items in base
 * @note Duplicate of B_ItemInBase with the correct behaviour as the function name implies.
 * @sa B_ItemInBase
 * @todo Use this fucntion instead of B_ItemInBase everywhere.
 */
int B_ItemInBase2(int item_idx, base_t *base)
{
	equipDef_t *ed = NULL;

	if (!base)
		return -1;

	ed = &base->storage;

	if (!ed)
		return -1;

	return ed->num[item_idx];
}
