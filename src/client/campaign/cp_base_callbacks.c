/**
 * @file cp_base_callbacks.c
 * @brief Menu related console command callbacks
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

#include "../cl_shared.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "cp_campaign.h"
#include "cp_base_callbacks.h"
#include "cp_base.h"
#include "cp_map.h"
#include "cp_popup.h"
#include "cp_ufo.h"

/** @brief Used from menu scripts as parameter for mn_base_select */
#define CREATE_NEW_BASE_ID -1

static cvar_t *mn_base_title;
static cvar_t *cl_start_buildings;
static building_t *buildingConstructionList[MAX_BUILDINGS];
static int buildingNumber = 0;

/**
 * @brief onDestroy Callback for Antimatter Storage
 */
static void B_Destroy_AntimaterStorage_f (void)
{
	base_t *base;
	const float prob = frand();

	if (Cmd_Argc() < 4) {	/** note: third parameter not used but we must be sure we have probability parameter */
		Com_Printf("Usage: %s <probability> <baseID> <buildingType>\n", Cmd_Argv(0));
		return;
	}

	base = B_GetFoundedBaseByIDX(atoi(Cmd_Argv(2)));
	if (!base)
		return;
	if (base->capacities[CAP_ANTIMATTER].cur <= 0)
		return;

	B_RemoveAntimatterExceedingCapacity(base);

	if (base->baseStatus != BASE_WORKING)
		return;

	if (prob < atof(Cmd_Argv(1))) {
		MS_AddNewMessage(_("Notice"), va(_("%s has been destroyed by an antimatter storage breach."), base->name), qfalse, MSG_STANDARD, NULL);
		UI_PopWindow(qfalse);
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
static void B_BuildingAddToList (linkedList_t **buildingList, building_t *building)
{
	int count;
	assert(building);
	assert(building->name);

	count = LIST_Count(*buildingList);
	LIST_AddPointer(buildingList, _(building->name));
	buildingConstructionList[count] = building->tpl;
}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 * For a new base the baseID is -1.
 */
static void B_SelectBase_f (void)
{
	int baseID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}
	baseID = atoi(Cmd_Argv(1));
	/* check against MAX_BASES here! - only -1 will create a new base
	 * if we would check against ccs.numBases here, a click on the base summary
	 * base nodes would try to select unfounded bases */
	if (baseID >= 0 && baseID < MAX_BASES) {
		const base_t *base = B_GetFoundedBaseByIDX(baseID);
		/* don't create a new base if the index was valid */
		if (base)
			B_SelectBase(base);
	} else if (baseID == CREATE_NEW_BASE_ID) {
		/* create a new base */
		B_SelectBase(NULL);
	}
}

/**
 * @brief Cycles to the next base.
 * @sa B_PrevBase
 * @sa B_SelectBase_f
 */
static void B_NextBase_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	base = B_GetNextFounded(base);
	/* if it was the last base, select the first */
	if (!base)
		base = B_GetNextFounded(NULL);
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
	const base_t *currentBase = B_GetCurrentSelectedBase();
	const base_t *prevBase;
	base_t *base;

	if (!currentBase)
		return;

	prevBase = NULL;
	base = NULL;
	while ((base = B_GetNextFounded(base)) != NULL) {
		if (base == currentBase)
			break;
		prevBase = base;
	}
	/* if it was the first base, select the last */
	if (!prevBase) {
		while ((base = B_GetNextFounded(base)) != NULL) {
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
				while (j <= baseCount && strcmp(baseName, ccs.bases[j++].name));
			} while (i++ <= baseCount && j <= baseCount);
		} else {
			Q_strncpyz(baseName, _("Home"), lengthof(baseName));
		}

		Cvar_Set("mn_base_title", baseName);
	} else {
		MS_AddNewMessage(_("Notice"), _("You've reached the base limit."), qfalse, MSG_STANDARD, NULL);
		UI_PopWindow(qfalse);		/* remove the new base popup */
	}
}

/**
 * @brief Constructs a new base.
 * @sa B_NewBase
 */
static void B_BuildBase_f (void)
{
	const nation_t *nation;
	base_t *base = B_GetFirstUnfoundedBase();
	const campaign_t *campaign = ccs.curCampaign;

	if (!base)
		return;

	if (ccs.credits - campaign->basecost > 0) {
		const char *baseName = mn_base_title->string;
		if (baseName[0] == '\0')
			baseName = "Base";

		/* set up the base with buildings from template */
		B_SetUpBase(campaign, base, ccs.newBasePos, baseName);

		CL_UpdateCredits(ccs.credits - campaign->basecost);
		nation = MAP_GetNation(base->pos);
		if (nation)
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s (nation: %s)"), mn_base_title->string, _(nation->name));
		else
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s"), mn_base_title->string);
		MS_AddNewMessage(_("Base built"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);

		B_UpdateBaseCount();
		B_SelectBase(base);
	} else {
		if (MAP_IsRadarOverlayActivated())
			MAP_SetOverlay("radar");
		if (ccs.mapAction == MA_NEWBASE)
			ccs.mapAction = MA_NONE;

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
	base_t *base = B_GetCurrentSelectedBase();

	/* maybe called without base initialized or active */
	if (!base)
		return;

	Q_strncpyz(base->name, Cvar_GetString("mn_base_title"), sizeof(base->name));
}

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu
 */
static void B_ResetBuildingCurrent_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

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
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* make sure the credits cvar is up-to-date */
	CL_UpdateCredits(ccs.credits);

	Cvar_SetValue("mn_base_num_aircraft", AIR_BaseCountAircraft(base));

	/* activate or deactivate the aircraft button */
	if (AIR_AircraftAllowed(base)) {
		if (AIR_BaseHasAircraft(base))
			UI_ExecuteConfunc("update_basebutton aircraft false \"%s\"", _("Aircraft management and crew equipment"));
		else
			UI_ExecuteConfunc("update_basebutton aircraft true \"%s\"", _("Buy or produce at least one aircraft first."));
	} else {
		UI_ExecuteConfunc("update_basebutton aircraft true \"%s\"", _("No Hangar functional in base."));
	}

	if (BS_BuySellAllowed(base))
		UI_ExecuteConfunc("update_basebutton buysell false \"%s\"", _("Buy/Sell equipment, aircraft and UGV"));
	else
		UI_ExecuteConfunc("update_basebutton buysell true \"%s\"", va(_("No %s functional in base."), _("Storage")));

	if (B_GetFoundedBaseCount() > 1)
		UI_ExecuteConfunc("update_basebutton transfer false \"%s\"", _("Transfer equipment, vehicles, aliens and employees to other bases"));
	else
		UI_ExecuteConfunc("update_basebutton transfer true \"%s\"", _("Build at least a second base to transfer equipment or personnel"));

	if (RS_ResearchAllowed(base))
		UI_ExecuteConfunc("update_basebutton research false \"%s\"", _("Research new technology"));
	else
		UI_ExecuteConfunc("update_basebutton research true \"%s\"", va(_("No %s functional in base."), _("Laboratory")));

	if (PR_ProductionAllowed(base))
		UI_ExecuteConfunc("update_basebutton production false \"%s\"", _("Produce equipment, aircraft and UGV"));
	else
		UI_ExecuteConfunc("update_basebutton production true \"%s\"", va(_("No %s functional in base."), _("Workshop")));

	if (E_HireAllowed(base))
		UI_ExecuteConfunc("update_basebutton hire false \"%s\"", _("Hire or dismiss employees"));
	else
		UI_ExecuteConfunc("update_basebutton hire true \"%s\"", va(_("No %s functional in base."), _("Living Quarters")));

	if (AC_ContainmentAllowed(base))
		UI_ExecuteConfunc("update_basebutton containment false \"%s\"", _("Manage captured aliens"));
	else
		UI_ExecuteConfunc("update_basebutton containment true \"%s\"", va(_("No %s functional in base."), _("Containment")));

	if (HOS_HospitalAllowed(base))
		UI_ExecuteConfunc("update_basebutton hospital false \"%s\"", _("Treat wounded soldiers and perform implant surgery"));
	else
		UI_ExecuteConfunc("update_basebutton hospital true \"%s\"", va(_("No %s functional in base."), _("Hospital")));
}

/**
 * @brief On destroy function for several building type.
 * @note this function is only used for sanity checks, and send to related function depending on building type.
 * @pre Functions below will be called AFTER the building is actually destroyed.
 * @sa B_BuildingDestroy_f
 * @todo Why does this exist? why is this not part of B_BuildingDestroy?
 */
static void B_BuildingOnDestroy_f (void)
{
	int baseIdx, buildingType;
	base_t *base;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <baseIdx> <buildingType>\n", Cmd_Argv(0));
		return;
	}

	buildingType = atoi(Cmd_Argv(2));
	if (buildingType < 0 || buildingType >= MAX_BUILDING_TYPE) {
		Com_Printf("B_BuildingOnDestroy_f: buildingType '%i' outside limits\n", buildingType);
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("B_BuildingOnDestroy_f: %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (base) {
		switch (buildingType) {
		case B_WORKSHOP:
			PR_UpdateProductionCap(base);
			break;
		case B_STORAGE:
			B_RemoveItemsExceedingCapacity(base);
			break;
		case B_ALIEN_CONTAINMENT:
			AL_RemoveAliensExceedingCapacity(base);
			break;
		case B_LAB:
			RS_RemoveScientistsExceedingCapacity(base);
			break;
		case B_HANGAR: /* the Dropship Hangar */
		case B_SMALL_HANGAR:
			B_RemoveAircraftExceedingCapacity(base, buildingType);
			break;
		case B_QUARTERS:
			E_DeleteEmployeesExceedingCapacity(base);
			break;
		default:
			/* handled in a seperate function, or number of buildings have no impact
			 * on how the building works */
			break;
		}
	} else
		Com_Printf("B_BuildingOnDestroy_f: base %i is not founded\n", baseIdx);
}

/**
 * @brief Update the building-list.
 * @sa B_BuildingInit_f
 */
static void B_BuildingInit (base_t* base)
{
	int i;
	linkedList_t *buildingList = NULL;

	/* maybe someone call this command before the bases are parsed?? */
	if (!base)
		return;

	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t *tpl = &ccs.buildingTemplates[i];
		/* make an entry in list for this building */

		if (tpl->visible) {
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
	}
	if (base->buildingCurrent)
		B_DrawBuilding(base->buildingCurrent);
	else
		UI_ExecuteConfunc("mn_buildings_reset");

	buildingNumber = LIST_Count(buildingList);
	UI_RegisterLinkedListText(TEXT_BUILDINGS, buildingList);
}

/**
 * @brief Script command binding for B_BuildingInit
 */
static void B_BuildingInit_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	B_BuildingInit(base);
}

/**
 * @brief Opens the UFOpedia for the current selected building.
 */
static void B_BuildingInfoClick_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

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
	int num;
	building_t *building;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <building list index>\n", Cmd_Argv(0));
		return;
	}

	/* which building? */
	num = atoi(Cmd_Argv(1));

	if (num > buildingNumber || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: max exceeded %i/%i\n", num, buildingNumber);
		return;
	}

	building = buildingConstructionList[num];

	base->buildingCurrent = building;
	B_DrawBuilding(building);

	ccs.baseAction = BA_NEWBUILDING;
}

/**
 * @brief We are doing the real destroy of a building here
 * @sa B_BuildingDestroy
 * @sa B_NewBuilding
 */
static void B_BuildingDestroy_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !base->buildingCurrent)
		return;

	B_BuildingDestroy(base->buildingCurrent);

	B_ResetBuildingCurrent(base);
	B_BuildingInit(base);
}

/**
 * @brief Console callback for B_BuildingStatus
 * @sa B_BuildingStatus
 */
static void B_BuildingStatus_f (void)
{
	const base_t *base = B_GetCurrentSelectedBase();

	/* maybe someone called this command before the buildings are parsed?? */
	if (!base || !base->buildingCurrent)
		return;

	B_BuildingStatus(base->buildingCurrent);
}

/**
 * @brief Builds a base map for tactical combat.
 * @sa SV_AssembleMap
 * @sa CP_BaseAttackChooseBase
 */
static void B_AssembleMap_f (void)
{
	const base_t* base;

	if (Cmd_Argc() < 2) {
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <baseID>\n", Cmd_Argv(0));
		base = B_GetCurrentSelectedBase();
	} else {
		const int baseID = atoi(Cmd_Argv(1));
		base = B_GetBaseByIDX(baseID);
	}

	B_AssembleMap(base);
}

/**
 * @brief Checks why a button in base menu is disabled, and create a popup to inform player
 */
static void B_CheckBuildingStatusForMenu_f (void)
{
	int num;
	const char *buildingID;
	const building_t *building;
	const base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <buildingID>\n", Cmd_Argv(0));
		return;
	}

	buildingID = Cmd_Argv(1);
	building = B_GetBuildingTemplate(buildingID);

	if (!building || !base)
		return;

	/* Maybe base is under attack ? */
	if (B_IsUnderAttack(base)) {
		UI_Popup(_("Notice"), _("Base is under attack, you can't access this building !"));
		return;
	}

	if (building->buildingType == B_HANGAR) {
		/* this is an exception because you must have a small or large hangar to enter aircraft menu */
		UI_Popup(_("Notice"), _("You need at least one Hangar (and its dependencies) to use aircraft."));
		return;
	}

	num = B_GetNumberOfBuildingsInBaseByBuildingType(base, building->buildingType);
	if (num > 0) {
		int numUnderConstruction;
		/* maybe all buildings of this type are under construction ? */
		B_CheckBuildingTypeStatus(base, building->buildingType, B_STATUS_UNDER_CONSTRUCTION, &numUnderConstruction);
		if (numUnderConstruction == num) {
			int minDay = 99999;
			building_t *b = NULL;

			while ((b = B_GetNextBuildingByType(base, b, building->buildingType))) {
				if (b->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
					const int delta = b->buildTime - (ccs.date.day - b->timeStart);
					minDay = min(minDay, delta);
				}
			}

			Com_sprintf(popupText, sizeof(popupText), ngettext("Construction of building will be over in %i day.\nPlease wait to enter.", "Construction of building will be over in %i days.\nPlease wait to enter.",
				minDay), minDay);
			UI_Popup(_("Notice"), popupText);
			return;
		}

		if (!B_CheckBuildingDependencesStatus(building)) {
			const building_t *dependenceBuilding = building->dependsBuilding;
			assert(building->dependsBuilding);
			if (B_GetNumberOfBuildingsInBaseByBuildingType(base, dependenceBuilding->buildingType) <= 0) {
				/* the dependence of the building is not built */
				Com_sprintf(popupText, sizeof(popupText), _("You need a building %s to make building %s functional."), _(dependenceBuilding->name), _(building->name));
				UI_Popup(_("Notice"), popupText);
				return;
			} else {
				/* maybe the dependence of the building is under construction
				 * note that we can't use B_STATUS_UNDER_CONSTRUCTION here, because this value
				 * is not use for every building (for exemple Command Centre) */
				building_t *b = NULL;

				while ((b = B_GetNextBuildingByType(base, b, dependenceBuilding->buildingType))) {
					if (b->buildTime > (ccs.date.day - b->timeStart)) {
						Com_sprintf(popupText, sizeof(popupText), _("Building %s is not finished yet, and is needed to use building %s."),
							_(dependenceBuilding->name), _(building->name));
						UI_Popup(_("Notice"), popupText);
						return;
					}
				}
				/* the dependence is built but doesn't work - must be because of their dependendes */
				Com_sprintf(popupText, sizeof(popupText), _("Make sure that the dependencies of building %s (%s) are operational, so that building %s may be used."),
					_(dependenceBuilding->name), _(dependenceBuilding->dependsBuilding->name), _(building->name));
				UI_Popup(_("Notice"), popupText);
				return;
			}
		}
		/* all buildings are OK: employees must be missing */
		if (building->buildingType == B_WORKSHOP && E_CountHired(base, EMPL_WORKER) <= 0) {
			Com_sprintf(popupText, sizeof(popupText), _("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_WORKER), _(building->name));
			UI_Popup(_("Notice"), popupText);
			return;
		} else if (building->buildingType == B_LAB && E_CountHired(base, EMPL_SCIENTIST) <= 0) {
			Com_sprintf(popupText, sizeof(popupText), _("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_SCIENTIST), _(building->name));
			UI_Popup(_("Notice"), popupText);
			return;
		}
	} else {
		Com_sprintf(popupText, sizeof(popupText), _("Build a %s first."), _(building->name));
		UI_Popup(_("Notice"), popupText);
		return;
	}
}

/** BaseSummary Callbacks: */

/**
 * @brief Base Summary menu init function.
 * @note Should be called whenever the Base Summary menu gets active.
 */
static void BaseSummary_Init (const base_t *base)
{
	static char textStatsBuffer[1024];
	static char textInfoBuffer[256];
	const aliensCont_t *containment = base->alienscont;
	int i;

	baseCapacities_t cap;
	const production_queue_t *queue;
	const technology_t *tech;
	int tmp;

	/* wipe away old buffers */
	textStatsBuffer[0] = textInfoBuffer[0] = 0;

	Q_strcat(textInfoBuffer, _("^BAircraft\n"), sizeof(textInfoBuffer));
	for (i = 0; i <= MAX_HUMAN_AIRCRAFT_TYPE; i++)
		Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", AIR_GetAircraftString(i),
			AIR_CountTypeInBase(base, i)), sizeof(textInfoBuffer));

	Q_strcat(textInfoBuffer, "\n", sizeof(textInfoBuffer));

	Q_strcat(textInfoBuffer, _("^BEmployees\n"), sizeof(textInfoBuffer));
	for (i = 0; i < MAX_EMPL; i++) {
		tmp = E_CountHired(base, i);
		Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", E_GetEmployeeString(i), tmp), sizeof(textInfoBuffer));
	}

	Q_strcat(textInfoBuffer, "\n", sizeof(textInfoBuffer));

	Q_strcat(textInfoBuffer, _("^BAliens\n"), sizeof(textInfoBuffer));
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (!containment[i].amountAlive && !containment[i].amountDead)
			continue;
		Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i/%i\n",
			_(containment[i].teamDef->name), containment[i].amountAlive,
			containment[i].amountDead), sizeof(textInfoBuffer));
	}

	/* link into the menu */
	UI_RegisterText(TEXT_STANDARD, textInfoBuffer);

	Q_strcat(textStatsBuffer, _("^BBuildings\t\t\t\t\t\tCapacity\t\t\t\tAmount\n"), sizeof(textStatsBuffer));
	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		const building_t* b = &ccs.buildingTemplates[i];

		/* only show already researched buildings */
		if (!RS_IsResearched_ptr(b->tech))
			continue;

		cap = B_GetCapacityFromBuildingType(b->buildingType);
		if (cap == MAX_CAP)
			continue;

		if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType))
			continue;

		/* Check if building is functional (see comments in B_UpdateBaseCapacities) */
		if (B_GetBuildingStatus(base, b->buildingType)) {
			Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name),
				base->capacities[cap].cur, base->capacities[cap].max), sizeof(textStatsBuffer));
		} else {
			if (b->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
				const int daysLeft = b->timeStart + b->buildTime - ccs.date.day;
				/* if there is no day left the status should not be B_STATUS_UNDER_CONSTRUCTION */
				assert(daysLeft);
				Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i %s", _(b->name), daysLeft, ngettext("day", "days", daysLeft)), sizeof(textStatsBuffer));
			} else {
				Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name), base->capacities[cap].cur, 0), sizeof(textStatsBuffer));
			}
		}
		Q_strcat(textStatsBuffer, va("\t\t\t\t%i\n", B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType)), sizeof(textStatsBuffer));
	}

	Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

	Q_strcat(textStatsBuffer, _("^BProduction\t\t\t\t\t\tQuantity\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
	queue = PR_GetProductionForBase(base);
	if (queue->numItems > 0) {
		for (i = 0; i < queue->numItems; i++) {
			const production_t *production = &queue->items[i];
			const char *name = PR_GetName(&production->data);
			/** @todo use the same method as we do in PR_ProductionInfo */
			Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%.2f%%\n"), name,
				production->amount, PR_GetProgress(production) * 100), sizeof(textStatsBuffer));
		}
	} else {
		Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));
	}

	Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

	Q_strcat(textStatsBuffer, _("^BResearch\t\t\t\t\t\tScientists\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
	tmp = 0;
	for (i = 0; i < ccs.numTechnologies; i++) {
		tech = RS_GetTechByIDX(i);
		if (tech->base == base && (tech->statusResearch == RS_RUNNING || tech->statusResearch == RS_PAUSED)) {
			Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%1.2f%%\n"), _(tech->name),
				tech->scientists, (1 - tech->time / tech->overallTime) * 100), sizeof(textStatsBuffer));
			tmp++;
		}
	}
	if (!tmp)
		Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));

	/* link into the menu */
	UI_RegisterText(TEXT_STATS_BASESUMMARY, textStatsBuffer);
}

/**
 * @brief Open menu for basesummary.
 */
static void BaseSummary_SelectBase_f (void)
{
	const base_t *base;

	if (Cmd_Argc() >= 2) {
		const int i = atoi(Cmd_Argv(1));
		base = B_GetFoundedBaseByIDX(i);
		if (base == NULL) {
			Com_Printf("Invalid base index given (%i).\n", i);
			return;
		}
	} else {
		base = B_GetCurrentSelectedBase();
	}

	if (base != NULL) {
		BaseSummary_Init(base);
		UI_ExecuteConfunc("basesummary_change_color %i", base->idx);
	}
}

/**
 * @brief Makes a mapshot - called by basemapshot script command
 * @note Load a basemap and execute 'basemapshot' in console
 */
static void B_MakeBaseMapShot_f (void)
{
	if (!Com_ServerState()) {
		Com_Printf("Load the base map before you try to use this function\n");
		return;
	}

	Cmd_ExecuteString(va("camsetangles %i %i", 60, 90));
	Cvar_SetValue("r_isometric", 1);
	/* we are interested in the second level only */
	Cvar_SetValue("cl_worldlevel", 1);
	UI_PushWindow("hud_nohud", NULL);
	/* hide any active console */
	Cmd_ExecuteString("toggleconsole");
	Cmd_ExecuteString("r_screenshot tga");
}

/** Init/Shutdown functions */

/** @todo unify the names into mn_base_* */
void B_InitCallbacks (void)
{
	mn_base_title = Cvar_Get("mn_base_title", "", 0, NULL);
	cl_start_buildings = Cvar_Get("cl_start_buildings", "1", CVAR_ARCHIVE, "Start with initial buildings in your first base");
	Cvar_Set("mn_base_cost", va(_("%i c"), ccs.curCampaign->basecost));
	Cvar_SetValue("mn_base_count", B_GetCount());
	Cvar_SetValue("mn_base_max", MAX_BASES);

	Cmd_AddCommand("basemapshot", B_MakeBaseMapShot_f, "Command to make a screenshot for the baseview with the correct angles");
	Cmd_AddCommand("mn_base_prev", B_PrevBase_f, "Go to the previous base");
	Cmd_AddCommand("mn_base_next", B_NextBase_f, "Go to the next base");
	Cmd_AddCommand("mn_base_select", B_SelectBase_f, "Select a founded base by index");
	Cmd_AddCommand("mn_base_build", B_BuildBase_f, NULL);
	Cmd_AddCommand("mn_set_base_title", B_SetBaseTitle_f, NULL);
	Cmd_AddCommand("base_changename", B_ChangeBaseName_f, "Called after editing the cvar base name");
	Cmd_AddCommand("base_init", B_BaseInit_f, NULL);
	Cmd_AddCommand("base_assemble", B_AssembleMap_f, "Called to assemble the current selected base");
	Cmd_AddCommand("building_init", B_BuildingInit_f, NULL);
	Cmd_AddCommand("building_status", B_BuildingStatus_f, NULL);
	Cmd_AddCommand("building_destroy", B_BuildingDestroy_f, "Function to destroy a building (select via right click in baseview first)");
	Cmd_AddCommand("building_amdestroy", B_Destroy_AntimaterStorage_f, "Function called if antimatter storage destroyed");
	Cmd_AddCommand("building_ufopedia", B_BuildingInfoClick_f, "Opens the UFOpedia for the current selected building");
	Cmd_AddCommand("check_building_status", B_CheckBuildingStatusForMenu_f, "Create a popup to inform player why he can't use a button");
	Cmd_AddCommand("buildings_click", B_BuildingClick_f, "Opens the building information window in construction mode");
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent_f, NULL);
	Cmd_AddCommand("building_ondestroy", B_BuildingOnDestroy_f, "Destroy a building");
	Cmd_AddCommand("basesummary_selectbase", BaseSummary_SelectBase_f, "Opens Base Statistics menu in base");
}

/** @todo unify the names into mn_base_* */
void B_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("basemapshot");
	Cmd_RemoveCommand("basesummary_selectbase");
	Cmd_RemoveCommand("mn_base_prev");
	Cmd_RemoveCommand("mn_base_next");
	Cmd_RemoveCommand("mn_base_select");
	Cmd_RemoveCommand("mn_base_build");
	Cmd_RemoveCommand("base_changename");
	Cmd_RemoveCommand("mn_set_base_title");
	Cmd_RemoveCommand("base_init");
	Cmd_RemoveCommand("base_assemble");
	Cmd_RemoveCommand("building_init");
	Cmd_RemoveCommand("building_status");
	Cmd_RemoveCommand("building_destroy");
	Cmd_RemoveCommand("building_ufopedia");
	Cmd_RemoveCommand("check_building_status");
	Cmd_RemoveCommand("buildings_click");
	Cmd_RemoveCommand("reset_building_current");
	Cmd_RemoveCommand("building_ondestroy");
	Cvar_Delete("mn_base_max");
	Cvar_Delete("mn_base_cost");
	Cvar_Delete("mn_base_title");
	Cvar_Delete("mn_base_count");
}
