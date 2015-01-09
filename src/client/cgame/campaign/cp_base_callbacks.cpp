/**
 * @file
 * @brief Menu related console command callbacks
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_base_callbacks.h"
#include "cp_base.h"
#include "cp_capacity.h"
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_time.h"
#include "cp_ufo.h"
#include "../../ui/ui_dataids.h"
#include "aliencontainment.h"

/** @brief Used from menu scripts as parameter for mn_base_select */
#define CREATE_NEW_BASE_ID -1

static cvar_t* mn_base_title;
static cvar_t* cl_start_buildings;
static building_t* buildingConstructionList[MAX_BUILDINGS];
static int buildingNumber = 0;

/**
 * @brief onDestroy Callback for Antimatter Storage
 */
static void B_Destroy_AntimaterStorage_f (void)
{
	base_t* base;
	const float prob = frand();

	if (cgi->Cmd_Argc() < 4) {	/** note: third parameter not used but we must be sure we have probability parameter */
		Com_Printf("Usage: %s <probability> <baseID> <buildingType>\n", cgi->Cmd_Argv(0));
		return;
	}

	base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(2)));
	if (!base)
		return;
	if (CAP_GetCurrent(base, CAP_ANTIMATTER) <= 0)
		return;

	CAP_RemoveAntimatterExceedingCapacity(base);

	if (base->baseStatus != BASE_WORKING)
		return;

	if (prob < atof(cgi->Cmd_Argv(1))) {
		MS_AddNewMessage(_("Notice"), va(_("%s has been destroyed by an antimatter storage breach."), base->name));
		cgi->UI_PopWindow(false);
		B_Destroy(base);
	}
}

/**
 * @brief Handles the list of constructable buildings.
 * @param[in] buildingList list of buildings to upate
 * @param[in] building Add this building to the construction list
 * @note Called everytime a building was constructed and thus maybe other buildings
 * get available. The content is updated everytime B_BuildingInit is called
 * (i.e everytime the buildings-list is displayed/updated)
 */
static void B_BuildingAddToList (linkedList_t** buildingList, building_t* building)
{
	int count;
	assert(building);
	assert(building->name);

	count = cgi->LIST_Count(*buildingList);
	cgi->LIST_AddPointer(buildingList, _(building->name));
	buildingConstructionList[count] = building->tpl;
}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 * For a new base the baseID is -1.
 */
static void B_SelectBase_f (void)
{
	int baseID;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", cgi->Cmd_Argv(0));
		return;
	}
	baseID = atoi(cgi->Cmd_Argv(1));
	/* check against MAX_BASES here! - only -1 will create a new base
	 * if we would check against ccs.numBases here, a click on the base summary
	 * base nodes would try to select unfounded bases */
	if (baseID >= 0 && baseID < MAX_BASES) {
		const base_t* base = B_GetFoundedBaseByIDX(baseID);
		/* don't create a new base if the index was valid */
		if (base)
			B_SelectBase(base);
	} else if (baseID == CREATE_NEW_BASE_ID) {
		/* create a new base */
		B_SelectBase(nullptr);
	}
}

/**
 * @brief Cycles to the next base.
 * @sa B_PrevBase
 * @sa B_SelectBase_f
 */
static void B_NextBase_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	base = B_GetNext(base);
	/* if it was the last base, select the first */
	if (!base)
		base = B_GetNext(nullptr);
	if (base)
		B_SelectBase(base);
}

/**
 * @brief Cycles to the previous base.
 * @sa B_NextBase
 * @sa B_SelectBase_f
 * @todo This should not rely on base->idx!
 */
static void B_PrevBase_f (void)
{
	const base_t* currentBase = B_GetCurrentSelectedBase();
	const base_t* prevBase;
	base_t* base;

	if (!currentBase)
		return;

	prevBase = nullptr;
	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		if (base == currentBase)
			break;
		prevBase = base;
	}
	/* if it was the first base, select the last */
	if (!prevBase) {
		while ((base = B_GetNext(base)) != nullptr) {
			prevBase = base;
		}
	}

	if (prevBase)
		B_SelectBase(prevBase);
}

/**
 * @brief Sets the title of the base to a cvar to prepare the rename menu.
 */
static void B_SetBaseTitle_f (void)
{
	int baseCount = B_GetCount();

	if (baseCount < MAX_BASES) {
		char baseName[MAX_VAR];

		if (baseCount > 0) {
			int j;
			int i = 2;
			do {
				j = 0;
				Com_sprintf(baseName, lengthof(baseName), _("Base #%i"), i);
				while (j <= baseCount && !Q_streq(baseName, ccs.bases[j].name)) {
					j++;
				}
			} while (i++ <= baseCount && j <= baseCount);
		} else {
			Q_strncpyz(baseName, _("Home"), lengthof(baseName));
		}

		cgi->Cvar_Set("mn_base_title", "%s", baseName);
	} else {
		MS_AddNewMessage(_("Notice"), _("You've reached the base limit."));
		cgi->UI_PopWindow(false);		/* remove the new base popup */
	}
}

/**
 * @brief Constructs a new base.
 * @sa B_NewBase
 */
static void B_BuildBase_f (void)
{
	const campaign_t* campaign = ccs.curCampaign;

	if (ccs.mapAction == MA_NEWBASE)
		ccs.mapAction = MA_NONE;

	if (ccs.credits - campaign->basecost > 0) {
		const nation_t* nation;
		const char* baseName = mn_base_title->string;
		base_t* base;
		/* there may be no " in the base name */
		if (!Com_IsValidName(baseName))
			baseName = _("Base");

		base = B_Build(campaign, ccs.newBasePos, baseName);
		if (!base)
			cgi->Com_Error(ERR_DROP, "Cannot build base");

		CP_UpdateCredits(ccs.credits - campaign->basecost);
		nation = GEO_GetNation(base->pos);
		if (nation)
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s (nation: %s)"), mn_base_title->string, _(nation->name));
		else
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s"), mn_base_title->string);
		MS_AddNewMessage(_("Base built"), cp_messageBuffer, MSG_CONSTRUCTION);

		/* First base */
		if (ccs.campaignStats.basesBuilt == 1)
			B_SetUpFirstBase(campaign, base);

		cgi->Cvar_SetValue("mn_base_count", B_GetCount());
		B_SelectBase(base);
	} else {
		if (GEO_IsRadarOverlayActivated())
			GEO_SetOverlay("radar", 0);

		CP_PopupList(_("Notice"), _("Not enough credits to set up a new base."));
	}
}

/**
 * @brief Creates console command to change the name of a base.
 * Copies the value of the cvar mn_base_title over as the name of the
 * current selected base
 */
static void B_ChangeBaseName_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	/* maybe called without base initialized or active */
	if (!base)
		return;

	/* basename should not contain " */
	if (!Com_IsValidName(cgi->Cvar_GetString("mn_base_title"))) {
		cgi->Cvar_Set("mn_base_title", "%s", base->name);
		return;
	}

	Q_strncpyz(base->name, cgi->Cvar_GetString("mn_base_title"), sizeof(base->name));
}

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu
 */
static void B_ResetBuildingCurrent_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	B_ResetBuildingCurrent(base);
}

/**
 * @brief Initialises base.
 * @note This command is executed in the init node of the base menu.
 * It is called everytime the base menu pops up and sets the cvars.
 * @todo integrate building status tooltips from B_CheckBuildingStatusForMenu_f into this one
 */
static void B_BaseInit_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* make sure the credits cvar is up-to-date */
	CP_UpdateCredits(ccs.credits);

	/* activate or deactivate the aircraft button */
	if (AIR_AircraftAllowed(base)) {
		if (AIR_BaseHasAircraft(base))
			cgi->UI_ExecuteConfunc("update_basebutton aircraft false \"%s\"", _("Aircraft management and crew equipment"));
		else
			cgi->UI_ExecuteConfunc("update_basebutton aircraft true \"%s\"", _("Buy or produce at least one aircraft first."));
	} else {
		cgi->UI_ExecuteConfunc("update_basebutton aircraft true \"%s\"", _("No Hangar functional in base."));
	}

	if (BS_BuySellAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton buysell false \"%s\"", _("Buy/Sell equipment, aircraft and UGV"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton buysell true \"%s\"", va(_("No %s functional in base."), _("Storage")));

	if (B_GetCount() > 1)
		cgi->UI_ExecuteConfunc("update_basebutton transfer false \"%s\"", _("Transfer equipment, vehicles, aliens and employees to other bases"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton transfer true \"%s\"", _("Build at least a second base to transfer equipment or personnel"));

	if (RS_ResearchAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton research false \"%s\"", _("Research new technology"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton research true \"%s\"", va(_("No %s functional in base."), _("Laboratory")));

	if (PR_ProductionAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton production false \"%s\"", _("Produce equipment, aircraft and UGV"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton production true \"%s\"", va(_("No %s functional in base."), _("Workshop")));

	if (E_HireAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton hire false \"%s\"", _("Hire or dismiss employees"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton hire true \"%s\"", va(_("No %s functional in base."), _("Living Quarters")));

	if (AC_ContainmentAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton containment false \"%s\"", _("Manage captured aliens"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton containment true \"%s\"", va(_("No %s functional in base."), _("Containment")));

	if (HOS_HospitalAllowed(base))
		cgi->UI_ExecuteConfunc("update_basebutton hospital false \"%s\"", _("Treat wounded soldiers and perform implant surgery"));
	else
		cgi->UI_ExecuteConfunc("update_basebutton hospital true \"%s\"", va(_("No %s functional in base."), _("Hospital")));

	/*
	 * Gather data on current/max space for living quarters, storage, lab and workshop
	 * clear_bld_space ensures 0/0 data for facilities which may not exist in base
	 */
	cgi->UI_ExecuteConfunc("clear_bld_space");
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* b = &ccs.buildingTemplates[i];

		/* Check if building matches one of our four types */
		if (b->buildingType != B_QUARTERS && b->buildingType != B_STORAGE && b->buildingType != B_WORKSHOP && b->buildingType != B_LAB && b->buildingType != B_ANTIMATTER)
			continue;

		/* only show already researched buildings */
		if (!RS_IsResearched_ptr(b->tech))
			continue;

		const baseCapacities_t capType = B_GetCapacityFromBuildingType(b->buildingType);
		if (capType == MAX_CAP)
			continue;

		const int count = B_GetNumberOfBuildingsInBaseByTemplate(base, b->tpl);
		if (count < 1)
			continue;

		const capacities_t& cap = *CAP_Get(base, capType);
		cgi->UI_ExecuteConfunc("show_bld_space \"%s\" \"%s\" %i %i %i %i", _(b->name), b->id, cap.cur, cap.max, count, b->tpl->maxCount);
	}

	/*
	 * Get the number of different employees in the base
	 * @todo: Get the number of injured soldiers if hospital exists
	 */
	cgi->UI_ExecuteConfunc("current_employees %i %i %i %i", E_CountHired(base, EMPL_SOLDIER), E_CountHired(base, EMPL_PILOT), E_CountHired(base, EMPL_SCIENTIST), E_CountHired(base, EMPL_WORKER));

	/*
	 * List the first five aircraft in the base if they exist
	 */
	cgi->UI_ExecuteConfunc("clear_aircraft");
	if (AIR_AircraftAllowed(base)) {
		AIR_ForeachFromBase(aircraft, base) {
			cgi->UI_ExecuteConfunc("show_aircraft %i \"%s\" \"%s\" \"%s\" %i", aircraft->idx, aircraft->name, aircraft->id, AIR_AircraftStatusToName(aircraft), AIR_IsAircraftInBase(aircraft));
		}
	}

	/* Get the research item closest to completion in the base if it exists */
	cgi->UI_ExecuteConfunc("clear_research");
	if (RS_ResearchAllowed(base)) {
		const technology_t* closestTech = nullptr;
		double finished = -1;
		for (int i = 0; i < ccs.numTechnologies; i++) {
			const technology_t* tech = RS_GetTechByIDX(i);
			if (!tech)
				continue;
			if (tech->base != base)
				continue;
			if (tech->statusResearch != RS_RUNNING)
				continue;
			const double percent = (1 - tech->time / tech->overallTime) * 100;
			if (percent > finished) {
				finished = percent;
				closestTech = tech;
			}
		}
		if (closestTech != nullptr)
			cgi->UI_ExecuteConfunc("show_research %s \"%s\" %i %3.0f", closestTech->id, closestTech->name, closestTech->scientists, finished);
	}

	/* Get the production item closest to completion in the base if it exists */
	cgi->UI_ExecuteConfunc("clear_production");
	if (PR_ProductionAllowed(base)) {
		const production_queue_t* queue = PR_GetProductionForBase(base);
		if (queue->numItems > 0) {
			const production_t* production = &queue->items[0];
			cgi->UI_ExecuteConfunc("show_production %i \"%s\" %3.0f", production->idx, PR_GetName(&production->data), PR_GetProgress(production) * 100);
		}
	}

	cgi->UI_ExecuteConfunc("restore_base_state");
}

/**
 * @brief Update the display of building space for all researched facilities
 * @sa B_BaseInit_f
 */
static void B_BuildingSpace_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	// Clear existing entries from the UI panel
	cgi->UI_ExecuteConfunc("clear_bld_space");
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* b = &ccs.buildingTemplates[i];

		/* skip mandatory buildings (like Entrance) which are built automatically */
		if (b->mandatory)
			continue;
		/* only show already researched buildings */
		if (!RS_IsResearched_ptr(b->tech))
			continue;

		const baseCapacities_t capType = B_GetCapacityFromBuildingType(b->buildingType);
		capacities_t cap;
		if (capType != MAX_CAP)
			cap = *CAP_Get(base, capType);
		else
			OBJZERO(cap);

		assert(b->tpl);
		const int count = B_GetNumberOfBuildingsInBaseByTemplate(base, b->tpl);

		cgi->UI_ExecuteConfunc("show_bld_space \"%s\" \"%s\" %i %i %i %i", _(b->name), b->id, cap.cur, cap.max, count, b->tpl->maxCount);
	}
}

/**
 * @brief Update the building-list.
 * @sa B_BuildingInit_f
 */
static void B_BuildingInit (base_t* base)
{
	/* maybe someone call this command before the bases are parsed?? */
	if (!base)
		return;

	linkedList_t* buildingList = nullptr;
	for (int i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t* tpl = &ccs.buildingTemplates[i];

		if (!tpl->mapPart)
			continue;

		const int numSameBuildings = B_GetNumberOfBuildingsInBaseByTemplate(base, tpl);
		if (tpl->maxCount >= 0 && tpl->maxCount <= numSameBuildings)
			continue;

		/* skip if limit of BASE_SIZE*BASE_SIZE exceeded */
		if (numSameBuildings >= BASE_SIZE * BASE_SIZE)
			continue;

		/* if the building is researched add it to the list */
		if (RS_IsResearched_ptr(tpl->tech))
			B_BuildingAddToList(&buildingList, tpl);
	}
	if (base->buildingCurrent)
		B_DrawBuilding(base->buildingCurrent);

	buildingNumber = cgi->LIST_Count(buildingList);
	cgi->UI_RegisterLinkedListText(TEXT_BUILDINGS, buildingList);
}

/**
 * @brief Script command binding for B_BuildingInit
 */
static void B_BuildingInit_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	B_BuildingInit(base);
}

/**
 * @brief Opens the UFOpedia for the current selected building.
 */
static void B_BuildingInfoClick_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (base->buildingCurrent)
		UP_OpenWith(base->buildingCurrent->pedia);
}

/**
 * @brief Script function for clicking the building list text field.
 */
static void B_BuildingClick_f (void)
{
	building_t* building;
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <building_id|building list index>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* which building? */
	building = B_GetBuildingTemplateSilent(cgi->Cmd_Argv(1));
	if (!building) {
		int num = atoi(cgi->Cmd_Argv(1));
		if (num > buildingNumber || num < 0) {
			Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: max exceeded %i/%i\n", num, buildingNumber);
			return;
		}
		building = buildingConstructionList[num];
	}

	base->buildingCurrent = building;
	B_DrawBuilding(building);

	/* Prevent building more if we reached the limit */
	if (building->maxCount >= 0 && B_GetNumberOfBuildingsInBaseByTemplate(base, building) >= building->maxCount)
		return;

	ccs.baseAction = BA_NEWBUILDING;
}

/**
 * @brief Mark a building for destruction - you only have to confirm it now
 * @param[in] building Pointer to the base to destroy
 */
static void B_MarkBuildingDestroy (building_t* building)
{
	baseCapacities_t cap;
	base_t* base = building->base;

	/* you can't destroy buildings if base is under attack */
	if (B_IsUnderAttack(base)) {
		CP_Popup(_("Notice"), _("Base is under attack, you can't destroy buildings!"));
		return;
	}

	cap = B_GetCapacityFromBuildingType(building->buildingType);
	/* store the pointer to the building you wanna destroy */
	base->buildingCurrent = building;

	/** @todo: make base destroyable by destroying entrance */
	if (building->buildingType == B_ENTRANCE) {
		CP_Popup(_("Destroy Entrance"), _("You can't destroy the entrance of the base!"));
		return;
	}

	if (!B_IsBuildingDestroyable(building)) {
			CP_Popup(_("Notice"), _("You can't destroy this building! It is the only connection to other buildings!"));
			return;
	}

	if (building->buildingStatus == B_STATUS_WORKING) {
		const bool hasMoreBases = B_GetCount() > 1;
		switch (building->buildingType) {
		case B_HANGAR:
		case B_SMALL_HANGAR:
			if (CAP_GetFreeCapacity(base, cap) <= 0) {
				cgi->UI_PopupButton(_("Destroy Hangar"), _("If you destroy this hangar, you will also destroy the aircraft inside.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push aircraft_equip;aircraft_select;", _("Go to hangar"), _("Go to hangar without destroying building"),
					va("building_destroy %i %i confirmed; ui_pop;", base->idx, building->idx), _("Destroy"), _("Destroy the building"),
					hasMoreBases ? "ui_pop;ui_push transfer;" : nullptr, hasMoreBases ? _("Transfer") : nullptr,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_QUARTERS:
			if (CAP_GetFreeCapacity(base, cap) < building->capacity) {
				cgi->UI_PopupButton(_("Destroy Quarter"), _("If you destroy this Quarters, every employee inside will be killed.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push employees;employee_list 0;", _("Dismiss"), _("Go to hiring menu without destroying building"),
					va("building_destroy %i %i confirmed; ui_pop;", base->idx, building->idx), _("Destroy"), _("Destroy the building"),
					hasMoreBases ? "ui_pop;ui_push transfer;" : nullptr, hasMoreBases ? _("Transfer") : nullptr,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		case B_STORAGE:
			if (CAP_GetFreeCapacity(base, cap) < building->capacity) {
				cgi->UI_PopupButton(_("Destroy Storage"), _("If you destroy this Storage, every items inside will be destroyed.\nAre you sure you want to destroy this building?"),
					"ui_pop;ui_push market;buy_type *mn_itemtype", _("Go to storage"), _("Go to buy/sell menu without destroying building"),
					va("building_destroy %i %i confirmed; ui_pop;", base->idx, building->idx), _("Destroy"), _("Destroy the building"),
					hasMoreBases ? "ui_pop;ui_push transfer;" : nullptr, hasMoreBases ? _("Transfer") : nullptr,
					_("Go to transfer menu without destroying the building"));
				return;
			}
			break;
		default:
			break;
		}
	}

	cgi->UI_PopupButton(_("Destroy building"), _("Are you sure you want to destroy this building?"),
		nullptr, nullptr, nullptr,
		va("building_destroy %i %i confirmed; ui_pop;", base->idx, building->idx), _("Destroy"), _("Destroy the building"),
		nullptr, nullptr, nullptr);
}

/**
 * @brief Destroy a base building
 * @sa B_MarkBuildingDestroy
 * @sa B_BuildingDestroy
 */
static void B_BuildingDestroy_f (void)
{
	base_t* base;
	building_t* building;

	if (cgi->Cmd_Argc() < 3) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <baseID> <buildingID> [confirmed]\n", cgi->Cmd_Argv(0));
		return;
	} else {
		const int baseID = atoi(cgi->Cmd_Argv(1));
		const int buildingID = atoi(cgi->Cmd_Argv(2));
		base = B_GetBaseByIDX(baseID);
		assert(base);
		building = &ccs.buildings[baseID][buildingID];
	}

	if (!base || !building)
		return;

	if (cgi->Cmd_Argc() == 4 && Q_streq(cgi->Cmd_Argv(3), "confirmed")) {
		/** @todo why not use the local building pointer here - we should
		 * reduce the access to these 'current' pointers */
		B_BuildingDestroy(base->buildingCurrent);

		B_ResetBuildingCurrent(base);
		B_BuildingInit(base);
	} else {
		B_MarkBuildingDestroy(building);
	}
}

/**
 * @brief Console callback for B_BuildingStatus
 * @sa B_BuildingStatus
 */
static void B_BuildingStatus_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();

	/* maybe someone called this command before the buildings are parsed?? */
	if (!base || !base->buildingCurrent)
		return;

	B_BuildingStatus(base->buildingCurrent);
}

/**
 * @brief Builds a base map for tactical combat.
 * @sa CP_BaseAttackChooseBase
 */
static void B_AssembleMap_f (void)
{
	const base_t* base;

	if (cgi->Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <baseID>\n", cgi->Cmd_Argv(0));
		base = B_GetCurrentSelectedBase();
	} else {
		const int baseID = atoi(cgi->Cmd_Argv(1));
		base = B_GetBaseByIDX(baseID);
	}

	char maps[2048];
	char coords[2048];
	B_AssembleMap(maps, sizeof(maps), coords, sizeof(coords), base);
	cgi->Cbuf_AddText("map %s \"%s\" \"%s\"\n", (GEO_IsNight(base->pos) ? "night" : "day"), maps, coords);
}

/**
 * @brief Checks why a button in base menu is disabled, and create a popup to inform player
 */
static void B_CheckBuildingStatusForMenu_f (void)
{
	int num;
	const char* buildingID;
	const building_t* building;
	const base_t* base = B_GetCurrentSelectedBase();

	if (cgi->Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <buildingID>\n", cgi->Cmd_Argv(0));
		return;
	}

	buildingID = cgi->Cmd_Argv(1);
	building = B_GetBuildingTemplate(buildingID);

	if (!building || !base)
		return;

	/* Maybe base is under attack ? */
	if (B_IsUnderAttack(base)) {
		CP_Popup(_("Notice"), _("Base is under attack, you can't access this building !"));
		return;
	}

	if (building->buildingType == B_HANGAR) {
		/* this is an exception because you must have a small or large hangar to enter aircraft menu */
		CP_Popup(_("Notice"), _("You need at least one Hangar (and its dependencies) to use aircraft."));
		return;
	}

	num = B_GetNumberOfBuildingsInBaseByBuildingType(base, building->buildingType);
	if (num > 0) {
		int numUnderConstruction;
		/* maybe all buildings of this type are under construction ? */
		B_CheckBuildingTypeStatus(base, building->buildingType, B_STATUS_UNDER_CONSTRUCTION, &numUnderConstruction);
		if (numUnderConstruction == num) {
			int minDay = 99999;
			building_t* b = nullptr;

			while ((b = B_GetNextBuildingByType(base, b, building->buildingType))) {
				if (b->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
					const float remaining = B_GetConstructionTimeRemain(b);
					minDay = std::min(minDay, (int)std::max(0.0f, remaining));
				}
			}

			CP_Popup(_("Notice"), ngettext("Construction of building will be over in %i day.\nPlease wait to enter.", "Construction of building will be over in %i days.\nPlease wait to enter.",
					minDay), minDay);
			return;
		}

		if (!B_CheckBuildingDependencesStatus(building)) {
			const building_t* dependenceBuilding = building->dependsBuilding;
			assert(building->dependsBuilding);
			if (B_GetNumberOfBuildingsInBaseByBuildingType(base, dependenceBuilding->buildingType) <= 0) {
				/* the dependence of the building is not built */
				CP_Popup(_("Notice"), _("You need a building %s to make building %s functional."), _(dependenceBuilding->name), _(building->name));
				return;
			} else {
				/* maybe the dependence of the building is under construction
				 * note that we can't use B_STATUS_UNDER_CONSTRUCTION here, because this value
				 * is not use for every building (for exemple Command Centre) */
				building_t* b = nullptr;

				while ((b = B_GetNextBuildingByType(base, b, dependenceBuilding->buildingType))) {
					if (!B_IsBuildingBuiltUp(b)) {
						CP_Popup(_("Notice"), _("Building %s is not finished yet, and is needed to use building %s."),
								_(dependenceBuilding->name), _(building->name));
						return;
					}
				}
				/* the dependence is built but doesn't work - must be because of their dependendes */
				CP_Popup(_("Notice"), _("Make sure that the dependencies of building %s (%s) are operational, so that building %s may be used."),
						_(dependenceBuilding->name), _(dependenceBuilding->dependsBuilding->name), _(building->name));
				return;
			}
		}
		/* all buildings are OK: employees must be missing */
		if (building->buildingType == B_WORKSHOP && E_CountHired(base, EMPL_WORKER) <= 0) {
			CP_Popup(_("Notice"), _("You need to recruit %s to use building %s."),
					E_GetEmployeeString(EMPL_WORKER, 2), _(building->name));
			return;
		} else if (building->buildingType == B_LAB && E_CountHired(base, EMPL_SCIENTIST) <= 0) {
			CP_Popup(_("Notice"), _("You need to recruit %s to use building %s."),
					E_GetEmployeeString(EMPL_SCIENTIST, 2), _(building->name));
			return;
		}
	} else {
		CP_Popup(_("Notice"), _("Build a %s first."), _(building->name));
		return;
	}
}

/** BaseSummary Callbacks: */

/**
 * @brief Base Summary menu init function.
 * @note Should be called whenever the Base Summary menu gets active.
 */
static void BaseSummary_Init (const base_t* base)
{
	/* Init base switcher */
	cgi->UI_ExecuteConfunc("bsum_clear_bases");
	base_t* otherBase = NULL;
	while ((otherBase = B_GetNext(otherBase))) {
		cgi->UI_ExecuteConfunc("bsum_add_base %d %d", otherBase->idx, (otherBase == base ? 1 : 0));
	}

	int i;
	/* Main */
	cgi->UI_ExecuteConfunc("bsum_main_clearlines");
	int line = 0;

	cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%s\" \"%s\" %d", line++, _("Buildings"), _("Capacity"), _("Amount"), 1);
	bool anyBuilding = false;
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* b = &ccs.buildingTemplates[i];

		/* only show already researched buildings */
		if (!RS_IsResearched_ptr(b->tech))
			continue;
		const baseCapacities_t cap = B_GetCapacityFromBuildingType(b->buildingType);
		if (cap == MAX_CAP)
			continue;
		if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType))
			continue;

		cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%i/%i\" \"%d\" %d", line++, _(b->name),
			CAP_GetCurrent(base, cap), CAP_GetMax(base, cap),
			B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType), 0);
		anyBuilding = true;
	}
	if (!anyBuilding)
		cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"\" \"\" %d", line++, _("No report"), 0);
	cgi->UI_ExecuteConfunc("bsum_main_addline %d \"\" \"\" \"\" 0", line++);

	cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%s\" \"%s\" %d", line++, _("Production"), _("Quantity"), _("Percent"), 1);
	const production_queue_t* queue = PR_GetProductionForBase(base);
	if (queue->numItems > 0) {
		for (i = 0; i < queue->numItems; i++) {
			const production_t* production = &queue->items[i];
			const char* name = PR_GetName(&production->data);
			cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%d\" \"%.2f%%\" %d", line++, name,
				production->amount, PR_GetProgress(production) * 100, 0);
		}
	} else {
		cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"\" \"\" %d", line++, _("Nothing"), 0);
	}
	cgi->UI_ExecuteConfunc("bsum_main_addline %d \"\" \"\" \"\" 0", line++);

	cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%s\" \"%s\" %d", line++, _("Research"), _("Scientists"), _("Percent"), 1);
	bool anyResearch = false;
	for (i = 0; i < ccs.numTechnologies; i++) {
		const technology_t* tech = RS_GetTechByIDX(i);
		if (tech->base == base && (tech->statusResearch == RS_RUNNING || tech->statusResearch == RS_PAUSED)) {
			cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"%d\" \"%.2f%%\" %d", line++, _(tech->name),
				tech->scientists, (1.0f - tech->time / tech->overallTime) * 100, 0);
			anyResearch = true;
		}
	}
	if (!anyResearch)
		cgi->UI_ExecuteConfunc("bsum_main_addline %d \"%s\" \"\" \"\" %d", line++, _("Nothing"), 0);

	static char textInfoBuffer[1024];
	textInfoBuffer[0] = 0;

	Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), _("^BAircraft\n"));
	for (i = 0; i <= MAX_HUMAN_AIRCRAFT_TYPE; i++) {
		const aircraftType_t airType = (aircraftType_t)i;
		const int count = AIR_CountTypeInBase(base, airType);
		if (count == 0)
			continue;
		Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), "\t%s:\t\t\t\t%i\n", AIR_GetAircraftString(airType),
			count);
	}
	Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), "\n");

	Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), _("^BEmployees\n"));
	for (i = 0; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		const int cnt = E_CountHired(base, emplType);
		if (cnt == 0)
			continue;
		const char* desc = E_GetEmployeeString(emplType, cnt);
		Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), "\t%s:\t\t\t\t%i\n", desc, cnt);
	}
	Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), "\n");

	Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), _("^BAliens\n"));
	if (base->alienContainment) {
		linkedList_t* list = base->alienContainment->list();
		LIST_Foreach(list, alienCargo_t, item) {
			Q_strcat(textInfoBuffer, sizeof(textInfoBuffer), "\t%s:\t\t\t\t%i/%i\n",
				_(item->teamDef->name), item->alive, item->dead);
		}
		cgi->LIST_Delete(&list);
	}
	cgi->UI_RegisterText(TEXT_STANDARD, textInfoBuffer);
}

/**
 * @brief Open menu for basesummary.
 */
static void BaseSummary_SelectBase_f (void)
{
	const base_t* base;

	if (cgi->Cmd_Argc() >= 2) {
		const int i = atoi(cgi->Cmd_Argv(1));
		base = B_GetFoundedBaseByIDX(i);
		if (base == nullptr) {
			Com_Printf("Invalid base index given (%i).\n", i);
			return;
		}
	} else {
		base = B_GetCurrentSelectedBase();
	}

	if (base != nullptr) {
		BaseSummary_Init(base);
	}
}

/**
 * @brief Makes a mapshot - called by basemapshot script command
 * @note Load a basemap and execute 'basemapshot' in console
 */
static void B_MakeBaseMapShot_f (void)
{
	if (!cgi->Com_ServerState()) {
		Com_Printf("Load the base map before you try to use this function\n");
		return;
	}

	cgi->Cmd_ExecuteString("camsetangles %i %i", 60, 90);
	cgi->Cvar_SetValue("r_isometric", 1);
	/* we are interested in the second level only */
	cgi->Cvar_SetValue("cl_worldlevel", 1);
	cgi->UI_PushWindow("hud_nohud");
	/* hide any active console */
	cgi->Cmd_ExecuteString("toggleconsole");
	cgi->Cmd_ExecuteString("r_screenshot tga");
}

/** Init/Shutdown functions */
static const cmdList_t baseCallbacks[] = {
	{"basemapshot", B_MakeBaseMapShot_f, "Command to make a screenshot for the baseview with the correct angles"},
	{"mn_base_prev", B_PrevBase_f, "Go to the previous base"},
	{"mn_base_next", B_NextBase_f, "Go to the next base"},
	{"mn_base_select", B_SelectBase_f, "Select a founded base by index"},
	{"mn_base_build", B_BuildBase_f, nullptr},
	{"mn_set_base_title", B_SetBaseTitle_f, nullptr},
	{"base_changename", B_ChangeBaseName_f, "Called after editing the cvar base name"},
	{"base_init", B_BaseInit_f, nullptr},
	{"base_assemble", B_AssembleMap_f, "Called to assemble the current selected base"},
	{"base_building_space", B_BuildingSpace_f, "Called to display building capacity in current selected base"},
	{"building_init", B_BuildingInit_f, nullptr},
	{"building_status", B_BuildingStatus_f, nullptr},
	{"building_destroy", B_BuildingDestroy_f, "Function to destroy a building (select via right click in baseview first)"},
	{"building_amdestroy", B_Destroy_AntimaterStorage_f, "Function called if antimatter storage destroyed"},
	{"building_ufopedia", B_BuildingInfoClick_f, "Opens the UFOpedia for the current selected building"},
	{"check_building_status", B_CheckBuildingStatusForMenu_f, "Create a popup to inform player why he can't use a button"},
	{"buildings_click", B_BuildingClick_f, "Opens the building information window in construction mode"},
	{"reset_building_current", B_ResetBuildingCurrent_f, nullptr},
	{"basesummary_selectbase", BaseSummary_SelectBase_f, "Opens Base Statistics menu in base"},
	{nullptr, nullptr, nullptr}
};

/** @todo unify the names into mn_base_* */
void B_InitCallbacks (void)
{
	mn_base_title = cgi->Cvar_Get("mn_base_title", "", 0, "The title of the current base");
	cl_start_buildings = cgi->Cvar_Get("cl_start_buildings", "1", CVAR_ARCHIVE, "Start with initial buildings in your first base");
	cgi->Cvar_Set("mn_base_cost", _("%i c"), ccs.curCampaign->basecost);
	cgi->Cvar_SetValue("mn_base_count", B_GetCount());
	cgi->Cvar_SetValue("mn_base_max", MAX_BASES);

	cgi->Cmd_TableAddList(baseCallbacks);
}

/** @todo unify the names into mn_base_* */
void B_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(baseCallbacks);

	cgi->Cvar_Delete("mn_base_max");
	cgi->Cvar_Delete("mn_base_cost");
	cgi->Cvar_Delete("mn_base_title");
	cgi->Cvar_Delete("mn_base_count");
}
