/**
 * @file
 * @brief Manage popups
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
#include "cp_mapfightequip.h"
#include "cp_geoscape.h"
#include "cp_popup.h"
#include "cp_missions.h"
#include "cp_time.h"
#include "cp_aircraft_callbacks.h"
#include "../../ui/ui_dataids.h"

/* popup_aircraft display the actions availables for an aircraft */

/** Max items displayed in popup_aircraft */
#define POPUP_AIRCRAFT_MAX_ITEMS	10
/** Max size of text displayed in popup_aircraft */
#define POPUP_AIRCRAFT_MAX_TEXT		2048

/**
 * @brief Enumerate type of actions available for popup_aircraft
 */
typedef enum {
	POPUP_AIRCRAFT_ACTION_BACKTOBASE = 1,	/**< Aircraft back to base */
	POPUP_AIRCRAFT_ACTION_STOP = 2,			/**< Aircraft stops */
	POPUP_AIRCRAFT_ACTION_MOVETOMISSION = 3,/**< Aircraft move to a mission */
	POPUP_AIRCRAFT_CHANGE_HOMEBASE = 4,		/**< Change aircraft homebase */
	POPUP_AIRCRAFT_ACTION_NONE,				/**< Do nothing */

	POPUP_AIRCRAFT_ACTION_MAX
} popup_aircraft_action_e;

/**
 * @brief Structure to store information about popup_aircraft
 */
typedef struct popup_aircraft_s {
	int numItems;			/**< Count of items displayed in popup_aircraft */
	aircraft_t* aircraft;		/**< Aircraft linked to popup_aircraft */
	popup_aircraft_action_e itemsAction[POPUP_AIRCRAFT_MAX_ITEMS];	/**< Action type of items */
	int itemsId[POPUP_AIRCRAFT_MAX_ITEMS];		/**< IDs corresponding to items */
	char textPopup[POPUP_AIRCRAFT_MAX_TEXT];	/**< Text displayed in popup_aircraft */
} popup_aircraft_t;

/** @todo Save me
 * why? The popup stuff should be regenerated from the campaign data --mattn */
static popup_aircraft_t popupAircraft; /**< Data about popup_aircraft */

/* popup_intercept display list of aircraft availables to move to a mission or a UFO */

/** Max aircraft in popup list */
#define POPUP_INTERCEPT_MAX_AIRCRAFT 64

typedef struct popup_intercept_s {
	int numAircraft;	/**< Count of aircraft displayed in list */
	aircraft_t* aircraft[POPUP_INTERCEPT_MAX_AIRCRAFT];	/**< List of aircrafts. */
	mission_t* mission;	/**< Mission the selected aircraft have to move to */
	aircraft_t* ufo;		/**< UFO the selected aircraft have to move to */
} popup_intercept_t;

static popup_intercept_t popupIntercept;	/**< Data about popup_intercept */

/** Reservation-popup info */
static int popupNum;							/**< Number of entries in the popup list */
static linkedList_t* popupListData = nullptr;		/**< Further datas needed when popup is clicked */
static uiNode_t* popupListNode = nullptr;		/**< Node used for popup */

static int INVALID_BASE = -1;

/*========================================
POPUP_HOMEBASE
========================================*/

/**
 * @brief Display the popup_homebase
 * @param[in] aircraft Pointer to aircraft we want to change homebase.
 * @param[in] alwaysDisplay False if popup should be displayed only if at least one base is available.
 * @return true if popup is displayed.
 */
bool CL_DisplayHomebasePopup (aircraft_t* aircraft, bool alwaysDisplay)
{
	int homebase;
	int numAvailableBases = 0;
	linkedList_t* popupListText = nullptr;
	base_t* base;

	assert(aircraft);

	cgi->LIST_Delete(&popupListData);

	popupNum = 0;
	homebase = -1;

	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		char text[MAX_VAR];
		char const* msg;

		if (base == aircraft->homebase) {
			msg = _("current homebase of aircraft");
			LIST_Add(&popupListData, INVALID_BASE);
			homebase = popupNum;
		} else {
			msg = AIR_CheckMoveIntoNewHomebase(aircraft, base);
			if (!msg) {
				msg = _("base can hold aircraft");
				LIST_Add(&popupListData, base->idx);
				numAvailableBases++;
			} else {
				LIST_Add(&popupListData, INVALID_BASE);
			}
		}

		Com_sprintf(text, sizeof(text), "%s\t%s", base->name, msg);
		cgi->LIST_AddString(&popupListText, text);
		popupNum++;
	}

	if (alwaysDisplay || numAvailableBases > 0) {
		CP_GameTimeStop();
		popupListNode = cgi->UI_PopupList(_("Change homebase of aircraft"), _("Base\tStatus"), popupListText, "change_homebase <lineselected>;");
		VectorSet(popupListNode->selectedColor, 0.0, 0.78, 0.0);	/**< Set color for selected entry. */
		popupListNode->selectedColor[3] = 1.0;
		cgi->UI_TextNodeSelectLine(popupListNode, homebase);
		GEO_SelectAircraft(aircraft);
		return true;
	}

	return false;
}

/**
 * @brief User select a base in the popup_homebase
 * change homebase to selected base.
 */
static void CL_PopupChangeHomebase_f (void)
{
	aircraft_t* aircraft = GEO_GetSelectedAircraft();

	/* If popup is opened, that means an aircraft is selected */
	if (!aircraft) {
		Com_Printf("CL_PopupChangeHomebase_f: An aircraft must be selected\n");
		return;
	}

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <popupIndex>\tpopupIndex=num in base list\n", cgi->Cmd_Argv(0));
		return;
	}

	/* read and range check */
	int selectedPopupIndex = atoi(cgi->Cmd_Argv(1));
	Com_DPrintf(DEBUG_CLIENT, "CL_PopupHomebaseClick_f (popupNum %i, selectedPopupIndex %i)\n", popupNum, selectedPopupIndex);
	if (selectedPopupIndex < 0 || selectedPopupIndex >= popupNum)
		return;

	/* Convert list index to base idx */
	linkedList_t* data = popupListData;	/**< Use this so we do not change the original popupListData pointer. */
	int baseIdx = INVALID_BASE;
	for (int i = 0; data; data = data->next, i++) {
		if (i == selectedPopupIndex) {
			baseIdx = *(int*)data->data;
			break;
		}
	}

	base_t* base = B_GetFoundedBaseByIDX(baseIdx);
	if (base == nullptr)
		return;

	if (!AIR_CheckMoveIntoNewHomebase(aircraft, base))
		AIR_MoveAircraftIntoNewHomebase(aircraft, base);

	cgi->UI_PopWindow(false);
	CL_DisplayHomebasePopup(aircraft, true);
}

/*========================================
POPUP_AIRCRAFT
========================================*/

/**
 * @brief Display the popup_aircraft
 * @sa CL_DisplayPopupIntercept
 */
void CL_DisplayPopupAircraft (aircraft_t* aircraft)
{
	/* Initialise popup_aircraft datas */
	if (!aircraft)
		return;
	popupAircraft.aircraft = aircraft;
	popupAircraft.numItems = 0;
	OBJZERO(popupAircraft.textPopup);
	cgi->UI_RegisterText(TEXT_POPUP, popupAircraft.textPopup);

	/* Set static datas in popup_aircraft */
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_BACKTOBASE;
	Q_strcat(popupAircraft.textPopup, lengthof(popupAircraft.textPopup), _("Back to base\t%s\n"), aircraft->homebase->name);
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_STOP;
	Q_strcat(popupAircraft.textPopup, lengthof(popupAircraft.textPopup), _("Stop\n"));
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_CHANGE_HOMEBASE;
	Q_strcat(popupAircraft.textPopup, lengthof(popupAircraft.textPopup), _("Change homebase\n"));

	/* Set missions in popup_aircraft */
	if (AIR_GetTeamSize(aircraft) > 0) {
		MIS_Foreach(tempMission) {
			if (tempMission->stage == STAGE_NOT_ACTIVE || !tempMission->onGeoscape)
				continue;

			if (tempMission->pos) {
				popupAircraft.itemsId[popupAircraft.numItems] = MIS_GetIdx(tempMission);
				popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_MOVETOMISSION;
				Q_strcat(popupAircraft.textPopup, lengthof(popupAircraft.textPopup), _("Mission\t%s\n"), MIS_GetName(tempMission));
			}
		}
	}

	/* Display popup_aircraft menu */
	cgi->UI_PushWindow("popup_aircraft");
}

/**
 * @brief User just select an item in the popup_aircraft
 */
static void CL_PopupAircraftClick_f (void)
{
	int num;
	aircraft_t* aircraft;
	mission_t* mission;

	Com_DPrintf(DEBUG_CLIENT, "CL_PopupAircraftClick\n");

	/* Get num of item selected in list */
	if (cgi->Cmd_Argc() < 2)
		return;

	num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= popupAircraft.numItems)
		return;

	cgi->UI_PopWindow(false); /* Close popup */

	/* Get aircraft associated with the popup_aircraft */
	aircraft = popupAircraft.aircraft;
	if (aircraft == nullptr)
		return;

	/* Execute action corresponding to item selected */
	switch (popupAircraft.itemsAction[num]) {
	case POPUP_AIRCRAFT_ACTION_BACKTOBASE:	/* Aircraft back to base */
		AIR_AircraftReturnToBase(aircraft);
		break;
	case POPUP_AIRCRAFT_ACTION_STOP:		/* Aircraft stop */
		aircraft->status = AIR_IDLE;
		break;
	case POPUP_AIRCRAFT_CHANGE_HOMEBASE:		/* Change Aircraft homebase */
		CL_DisplayHomebasePopup(aircraft, true);
		break;
	case POPUP_AIRCRAFT_ACTION_MOVETOMISSION:	/* Aircraft move to mission */
		mission = MIS_GetByIdx(popupAircraft.itemsId[num]);
		if (mission)
			AIR_SendAircraftToMission(aircraft, mission);
		break;
	case POPUP_AIRCRAFT_ACTION_NONE:
		break;
	default:
		Com_Printf("CL_PopupAircraftClick: type of action unknow %i\n", popupAircraft.itemsAction[num]);
		break;
	}
}

/*========================================
POPUP_INTERCEPT
========================================*/

static int AIR_SortByDistance (linkedList_t* aircraftEntry1, linkedList_t* aircraftEntry2, const void* userData)
{
	const vec_t* pos = (const vec_t*)userData;
	const aircraft_t* aircraft1 = (const aircraft_t*)aircraftEntry1->data;
	const aircraft_t* aircraft2 = (const aircraft_t*)aircraftEntry2->data;

	return GetDistanceOnGlobe(aircraft1->pos, pos) - GetDistanceOnGlobe(aircraft2->pos, pos);
}

/**
 * @brief Display the popup_mission
 * @sa CL_DisplayPopupAircraft
 */
void CL_DisplayPopupInterceptMission (mission_t* mission)
{
	linkedList_t* aircraftList = nullptr;
	linkedList_t* aircraftListSorted;

	if (!mission)
		return;

	popupIntercept.mission = mission;
	popupIntercept.ufo = nullptr;

	/* Create the list of aircraft, and write the text to display in popup */
	popupIntercept.numAircraft = 0;

	AIR_ForeachSorted(aircraft, AIR_SortByDistance, mission->pos, aircraftListSorted) {
		const int teamSize = AIR_GetTeamSize(aircraft);

		if (aircraft->status == AIR_CRASHED)
			continue;
		/* if aircraft is empty we can't send it on a ground mission */
		if (teamSize > 0 && AIR_CanIntercept(aircraft)) {
			char aircraftListText[256] = "";
			const float distance = GetDistanceOnGlobe(aircraft->pos, mission->pos);
			const char* statusName = AIR_AircraftStatusToName(aircraft);
			const char* time = CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED]);
			Com_sprintf(aircraftListText, sizeof(aircraftListText), _("%s (%i/%i)\t%s\t%s\t%s"), aircraft->name,
					teamSize, aircraft->maxTeamSize, statusName, aircraft->homebase->name, time);
			cgi->LIST_AddString(&aircraftList, aircraftListText);
			popupIntercept.aircraft[popupIntercept.numAircraft] = aircraft;
			popupIntercept.numAircraft++;
			if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
				break;
		}
	}
	cgi->LIST_Delete(&aircraftListSorted);

	if (popupIntercept.numAircraft)
		cgi->UI_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, aircraftList);
	else
		cgi->UI_RegisterText(TEXT_AIRCRAFT_LIST, _("No craft available, no pilot assigned, or no tactical teams assigned to available craft."));

	/* Stop time */
	CP_GameTimeStop();

	/* Display the popup */
	cgi->UI_PushWindow("popup_mission");
}


/**
 * @brief Display the popup_intercept
 * @sa CL_DisplayPopupAircraft
 */
void CL_DisplayPopupInterceptUFO (aircraft_t* ufo)
{
	linkedList_t* aircraftList = nullptr;
	linkedList_t* aircraftListSorted;
	linkedList_t* baseList = nullptr;
	base_t* base;

	if (!ufo)
		return;

	popupIntercept.mission = nullptr;
	popupIntercept.ufo = ufo;

	/* Create the list of aircraft, and write the text to display in popup */
	popupIntercept.numAircraft = 0;

	AIR_ForeachSorted(aircraft, AIR_SortByDistance, ufo->pos, aircraftListSorted) {
		if (AIR_CanIntercept(aircraft)) {
			char aircraftListText[256] = "";
			/* don't show aircraft with no weapons or no ammo, or crafts that
			 * can't even reach the target */
			const char* enoughFuelMarker = "^B";

			/* Does the aircraft has weapons and ammo ? */
			if (AIRFIGHT_ChooseWeapon(aircraft->weapons, aircraft->maxWeapons, aircraft->pos, aircraft->pos) == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
				Com_DPrintf(DEBUG_CLIENT, "CL_DisplayPopupIntercept: No useable weapon found in craft '%s' (%i)\n", aircraft->id, aircraft->maxWeapons);
				continue;
			}
			/* now check the aircraft range */
			if (!AIR_AircraftHasEnoughFuel(aircraft, ufo->pos)) {
				Com_DPrintf(DEBUG_CLIENT, "CL_DisplayPopupIntercept: Target out of reach for craft '%s'\n", aircraft->id);
				enoughFuelMarker = "";
			}

			Com_sprintf(aircraftListText, sizeof(aircraftListText), _("%s%s (%i/%i)\t%s\t%s"), enoughFuelMarker, aircraft->name,
				AIR_GetTeamSize(aircraft), aircraft->maxTeamSize, AIR_AircraftStatusToName(aircraft), aircraft->homebase->name);
			cgi->LIST_AddString(&aircraftList, aircraftListText);
			popupIntercept.aircraft[popupIntercept.numAircraft] = aircraft;
			popupIntercept.numAircraft++;
			if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
				break;
		}
	}
	cgi->LIST_Delete(&aircraftListSorted);

	base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		/* Check if the base should be displayed in base list
		 * don't check range because maybe UFO will get closer */
		if (AII_BaseCanShoot(base))
			cgi->LIST_AddString(&baseList, va("^B%s", base->name));
	}	/* bases */

	if (popupIntercept.numAircraft)
		cgi->UI_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, aircraftList);
	else
		cgi->UI_RegisterText(TEXT_AIRCRAFT_LIST, _("No craft available, no pilot assigned, or no weapon or ammo equipped."));

	INS_Foreach(installation) {
		/* Check if the installation should be displayed in base list
		 * don't check range because maybe UFO will get closer */
		if (AII_InstallationCanShoot(installation))
			cgi->LIST_AddString(&baseList, va("^B%s", installation->name));
	}

	if (baseList)
		cgi->UI_RegisterLinkedListText(TEXT_BASE_LIST, baseList);
	else
		cgi->UI_RegisterText(TEXT_BASE_LIST, _("No defence system operational or no weapon or ammo equipped."));

	/* Stop time */
	CP_GameTimeStop();

	/* Display the popup */
	cgi->UI_PushWindow("popup_intercept");
}

/**
 * @brief return the selected aircraft in popup_intercept
 * Close the popup if required
 */
static aircraft_t* CL_PopupInterceptGetAircraft (void)
{
	int num;

	if (cgi->Cmd_Argc() < 2)
		return nullptr;

	/* Get the selected aircraft */
	num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= popupIntercept.numAircraft)
		return nullptr;

	cgi->UI_PopWindow(false);
	if (!popupIntercept.aircraft[num])
		return nullptr;
	return popupIntercept.aircraft[num];
}

/**
 * @brief User select an item in the popup_aircraft
 * Make the aircraft attack the corresponding mission or UFO
 */
static void CL_PopupInterceptClick_f (void)
{
	aircraft_t* aircraft;
	base_t* base;

	/* Get the selected aircraft */
	aircraft = CL_PopupInterceptGetAircraft();
	if (aircraft == nullptr)
		return;

	/* Aircraft can start if only Command Centre in base is operational. */
	base = aircraft->homebase;
	if (!B_GetBuildingStatus(base, B_COMMAND)) {
		/** @todo are these newlines really needed? at least the first should be handled by the menu code */
		CP_Popup(_("Notice"), _("No Command Centre operational in homebase\nof this aircraft.\n\nAircraft cannot start.\n"));
		return;
	}

	/* Set action to aircraft */
	if (popupIntercept.mission)
		AIR_SendAircraftToMission(aircraft, popupIntercept.mission);	/* Aircraft move to mission */
	else if (popupIntercept.ufo)
		AIR_SendAircraftPursuingUFO(aircraft, popupIntercept.ufo);	/* Aircraft purchase ufo */
}

/**
 * @brief User select an item in the popup_aircraft with right click
 * Opens up the aircraft menu
 */
static void CL_PopupInterceptRClick_f (void)
{
	aircraft_t* aircraft;

	/* Get the selected aircraft */
	aircraft = CL_PopupInterceptGetAircraft();
	if (aircraft == nullptr)
		return;

	/* Display aircraft menu */
	AIR_AircraftSelect(aircraft);
	GEO_ResetAction();
	B_SelectBase(aircraft->homebase);
	cgi->UI_PushWindow("aircraft");
}

/**
 * @brief User select a base in the popup_aircraft
 * Make the base attack the corresponding UFO
 */
static void CL_PopupInterceptBaseClick_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\tnum=num in base list\n", cgi->Cmd_Argv(0));
		return;
	}

	/* If popup is opened, that means that ufo is selected on geoscape */
	if (GEO_GetSelectedUFO() == nullptr)
		return;

	int num = atoi(cgi->Cmd_Argv(1));

	base_t* base = nullptr;
	bool atLeastOneBase = false;
	while ((base = B_GetNext(base)) != nullptr) {
		/* Check if the base should be displayed in base list */
		if (AII_BaseCanShoot(base)) {
			num--;
			atLeastOneBase = true;
			if (num < 0)
				break;
		}
	}

	installation_t* installation = nullptr;
	if (num >= 0) { /* don't try to find an installation if we already found the right base */
		INS_Foreach(inst) {
			/* Check if the installation should be displayed in base list */
			if (AII_InstallationCanShoot(inst)) {
				num--;
				atLeastOneBase = true;
				if (num < 0) {
					installation = inst;
					break;
				}
			}
		}
	}

	if (!atLeastOneBase && !num) {
		/* no base in list: no error message
		 * note that num should always be 0 if we enter this loop, unless this function is called from console
		 * so 2nd part of the test should be useless in most case */
		return;
	} else if (num >= 0) {
		Com_Printf("CL_PopupInterceptBaseClick_f: Number given in argument (%i) is bigger than number of base in list.\n", num);
		return;
	}

	assert(base || installation);
	int i;
	if (installation) {
		for (i = 0; i < installation->installationTemplate->maxBatteries; i++)
			installation->batteries[i].target = GEO_GetSelectedUFO();
	} else {
		for (i = 0; i < base->numBatteries; i++)
			base->batteries[i].target = GEO_GetSelectedUFO();
		for (i = 0; i < base->numLasers; i++)
			base->lasers[i].target = GEO_GetSelectedUFO();
	}

	cgi->UI_PopWindow(false);
}

/**
 * @brief Initialise popups
 */
void CL_PopupInit (void)
{
	/* popup_aircraft commands */
	cgi->Cmd_AddCommand("popup_aircraft_action_click", CL_PopupAircraftClick_f, nullptr);

	/* popup_intercept commands */
	cgi->Cmd_AddCommand("ships_click", CL_PopupInterceptClick_f, nullptr);
	cgi->Cmd_AddCommand("ships_rclick", CL_PopupInterceptRClick_f, nullptr);
	cgi->Cmd_AddCommand("bases_click", CL_PopupInterceptBaseClick_f, nullptr);

	/* popup_homebase commands */
	cgi->Cmd_AddCommand("change_homebase", CL_PopupChangeHomebase_f, nullptr);

	OBJZERO(popupIntercept);
	OBJZERO(popupAircraft);
}

/**
 * @brief Wrapper around @c UI_Popup which also stops the time
 */
void CP_PopupList (const char* title, const char* text)
{
	CP_GameTimeStop();
	CP_Popup(title, "%s", text);
}

/**
 * @brief Wrapper around @c UI_Popup
 */
void CP_Popup (const char* title, const char* text, ...)
{
	static char msg[1024];
	va_list argptr;

	va_start(argptr, text);
	Q_vsnprintf(msg, sizeof(msg), text, argptr);
	va_end(argptr);

	cgi->UI_Popup(title, msg);
}
