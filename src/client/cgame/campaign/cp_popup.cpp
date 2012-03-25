/**
 * @file cp_popup.c
 * @brief Manage popups
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

#include "../../cl_shared.h"
#include "../../ui/ui_main.h"
#include "../../ui/ui_popup.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_map.h"
#include "cp_popup.h"
#include "cp_missions.h"
#include "cp_time.h"
#include "cp_aircraft_callbacks.h"

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
	aircraft_t *aircraft;		/**< Aircraft linked to popup_aircraft */
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
	aircraft_t *aircraft[POPUP_INTERCEPT_MAX_AIRCRAFT];	/**< List of aircrafts. */
	mission_t* mission;	/**< Mission the selected aircraft have to move to */
	aircraft_t* ufo;		/**< UFO the selected aircraft have to move to */
} popup_intercept_t;

static popup_intercept_t popupIntercept;	/**< Data about popup_intercept */

/** Reservation-popup info */
static int popupNum;							/**< Number of entries in the popup list */
static linkedList_t* popupListData = NULL;		/**< Further datas needed when popup is clicked */
static uiNode_t* popupListNode = NULL;		/**< Node used for popup */

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
qboolean CL_DisplayHomebasePopup (aircraft_t *aircraft, qboolean alwaysDisplay)
{
	int homebase;
	int numAvailableBases = 0;
	baseCapacities_t capacity;
	linkedList_t* popupListText = NULL;
	base_t *base;

	assert(aircraft);

	capacity = AIR_GetCapacityByAircraftWeight(aircraft);

	LIST_Delete(&popupListData);

	popupNum = 0;
	homebase = -1;

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		char text[MAX_VAR];
		char const* msg;

		if (base == aircraft->homebase) {
			msg = _("current homebase of aircraft");
			LIST_Add(&popupListData, (byte *)&INVALID_BASE, sizeof(int));
			homebase = popupNum;
		} else {
			msg = AIR_CheckMoveIntoNewHomebase(aircraft, base, capacity);
			if (!msg) {
				msg = _("base can hold aircraft");
				LIST_Add(&popupListData, (byte *)&base->idx, sizeof(int));
				numAvailableBases++;
			} else {
				LIST_Add(&popupListData, (byte *)&INVALID_BASE, sizeof(int));
			}
		}

		Com_sprintf(text, sizeof(text), "%s\t%s", base->name, msg);
		LIST_AddString(&popupListText, text);
		popupNum++;
	}

	if (alwaysDisplay || numAvailableBases > 0) {
		CP_GameTimeStop();
		popupListNode = UI_PopupList(_("Change homebase of aircraft"), _("Base\tStatus"), popupListText, "change_homebase <lineselected>;");
		VectorSet(popupListNode->selectedColor, 0.0, 0.78, 0.0);	/**< Set color for selected entry. */
		popupListNode->selectedColor[3] = 1.0;
		UI_TextNodeSelectLine(popupListNode, homebase);
		MAP_SelectAircraft(aircraft);
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief User select a base in the popup_homebase
 * change homebase to selected base.
 */
static void CL_PopupChangeHomebase_f (void)
{
	linkedList_t* data = popupListData;	/**< Use this so we do not change the original popupListData pointer. */
	int selectedPopupIndex;
	int i;
	base_t *base;
	int baseIdx;
	aircraft_t *aircraft = MAP_GetSelectedAircraft();

	/* If popup is opened, that means an aircraft is selected */
	if (!aircraft) {
		Com_Printf("CL_PopupChangeHomebase_f: An aircraft must be selected\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <popupIndex>\tpopupIndex=num in base list\n", Cmd_Argv(0));
		return;
	}

	/* read and range check */
	selectedPopupIndex = atoi(Cmd_Argv(1));
	Com_DPrintf(DEBUG_CLIENT, "CL_PopupHomebaseClick_f (popupNum %i, selectedPopupIndex %i)\n", popupNum, selectedPopupIndex);
	if (selectedPopupIndex < 0 || selectedPopupIndex >= popupNum)
		return;

	/* Convert list index to base idx */
	baseIdx = INVALID_BASE;
	for (i = 0; data; data = data->next, i++) {
		if (i == selectedPopupIndex) {
			baseIdx = *(int*)data->data;
			break;
		}
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (base == NULL)
		return;

	AIR_MoveAircraftIntoNewHomebase(aircraft, base);

	UI_PopWindow(qfalse);
	CL_DisplayHomebasePopup(aircraft, qtrue);
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
	UI_RegisterText(TEXT_POPUP, popupAircraft.textPopup);

	/* Set static datas in popup_aircraft */
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_BACKTOBASE;
	Q_strcat(popupAircraft.textPopup, va(_("Back to base\t%s\n"), aircraft->homebase->name), lengthof(popupAircraft.textPopup));
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_STOP;
	Q_strcat(popupAircraft.textPopup, _("Stop\n"), lengthof(popupAircraft.textPopup));
	popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_CHANGE_HOMEBASE;
	Q_strcat(popupAircraft.textPopup, _("Change homebase\n"), lengthof(popupAircraft.textPopup));

	/* Set missions in popup_aircraft */
	if (AIR_GetTeamSize(aircraft) > 0) {
		MIS_Foreach(tempMission) {
			if (tempMission->stage == STAGE_NOT_ACTIVE || !tempMission->onGeoscape)
				continue;

			if (tempMission->pos) {
				popupAircraft.itemsId[popupAircraft.numItems] = MAP_GetIDXByMission(tempMission);
				popupAircraft.itemsAction[popupAircraft.numItems++] = POPUP_AIRCRAFT_ACTION_MOVETOMISSION;
				Q_strcat(popupAircraft.textPopup, va(_("Mission\t%s (%s)\n"),
					CP_MissionToTypeString(tempMission), _(tempMission->location)), lengthof(popupAircraft.textPopup));
			}
		}
	}

	/* Display popup_aircraft menu */
	UI_PushWindow("popup_aircraft", NULL, NULL);
}

/**
 * @brief User just select an item in the popup_aircraft
 */
static void CL_PopupAircraftClick_f (void)
{
	int num;
	aircraft_t* aircraft;
	mission_t *mission;

	Com_DPrintf(DEBUG_CLIENT, "CL_PopupAircraftClick\n");

	/* Get num of item selected in list */
	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= popupAircraft.numItems)
		return;

	UI_PopWindow(qfalse); /* Close popup */

	/* Get aircraft associated with the popup_aircraft */
	aircraft = popupAircraft.aircraft;
	if (aircraft == NULL)
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
		CL_DisplayHomebasePopup(aircraft, qtrue);
		break;
	case POPUP_AIRCRAFT_ACTION_MOVETOMISSION:	/* Aircraft move to mission */
		mission = MAP_GetMissionByIDX(popupAircraft.itemsId[num]);
		if (mission)
			AIR_SendAircraftToMission(aircraft, mission);
		break;
	case POPUP_AIRCRAFT_ACTION_NONE:
		break;
	default:
		Com_Printf("CL_PopupAircraftClick: type of action unknow %i\n", popupAircraft.itemsAction[num]);
	}
}

/*========================================
POPUP_INTERCEPT
========================================*/

static int AIR_SortByDistance (linkedList_t *aircraftEntry1, linkedList_t *aircraftEntry2, const void *userData)
{
	const vec_t* pos = (const vec_t*)userData;
	const aircraft_t *aircraft1 = (const aircraft_t*)aircraftEntry1->data;
	const aircraft_t *aircraft2 = (const aircraft_t*)aircraftEntry2->data;

	return GetDistanceOnGlobe(aircraft1->pos, pos) - GetDistanceOnGlobe(aircraft2->pos, pos);
}

/**
 * @brief Display the popup_mission
 * @sa CL_DisplayPopupAircraft
 */
void CL_DisplayPopupInterceptMission (mission_t* mission)
{
	linkedList_t *aircraftList = NULL;
	linkedList_t *aircraftListSorted;

	if (!mission)
		return;

	popupIntercept.mission = mission;
	popupIntercept.ufo = NULL;

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
			const char *statusName = AIR_AircraftStatusToName(aircraft);
			const char *time = CP_SecondConvert((float)SECONDS_PER_HOUR * distance / aircraft->stats[AIR_STATS_SPEED]);
			Com_sprintf(aircraftListText, sizeof(aircraftListText), _("%s (%i/%i)\t%s\t%s\t%s"), aircraft->name,
					teamSize, aircraft->maxTeamSize, statusName, aircraft->homebase->name, time);
			LIST_AddString(&aircraftList, aircraftListText);
			popupIntercept.aircraft[popupIntercept.numAircraft] = aircraft;
			popupIntercept.numAircraft++;
			if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
				break;
		}
	}
	LIST_Delete(&aircraftListSorted);

	if (popupIntercept.numAircraft)
		UI_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, aircraftList);
	else
		UI_RegisterText(TEXT_AIRCRAFT_LIST, _("No craft available, no pilot assigned, or no tactical teams assigned to available craft."));

	/* Stop time */
	CP_GameTimeStop();

	/* Display the popup */
	UI_PushWindow("popup_mission", NULL, NULL);
}


/**
 * @brief Display the popup_intercept
 * @sa CL_DisplayPopupAircraft
 */
void CL_DisplayPopupInterceptUFO (aircraft_t* ufo)
{
	linkedList_t *aircraftList = NULL;
	linkedList_t *aircraftListSorted;
	linkedList_t *baseList = NULL;
	base_t *base;

	if (!ufo)
		return;

	popupIntercept.mission = NULL;
	popupIntercept.ufo = ufo;

	/* Create the list of aircraft, and write the text to display in popup */
	popupIntercept.numAircraft = 0;

	AIR_ForeachSorted(aircraft, AIR_SortByDistance, ufo->pos, aircraftListSorted) {
		if (AIR_CanIntercept(aircraft)) {
			char aircraftListText[256] = "";
			/* don't show aircraft with no weapons or no ammo, or crafts that
			 * can't even reach the target */
			const char *enoughFuelMarker = "^B";

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
			LIST_AddString(&aircraftList, aircraftListText);
			popupIntercept.aircraft[popupIntercept.numAircraft] = aircraft;
			popupIntercept.numAircraft++;
			if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
				break;
		}
	}
	LIST_Delete(&aircraftListSorted);

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		/* Check if the base should be displayed in base list
		 * don't check range because maybe UFO will get closer */
		if (AII_BaseCanShoot(base))
			LIST_AddString(&baseList, va("^B%s", base->name));
	}	/* bases */

	if (popupIntercept.numAircraft)
		UI_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, aircraftList);
	else
		UI_RegisterText(TEXT_AIRCRAFT_LIST, _("No craft available, no pilot assigned, or no weapon or ammo equipped."));

	INS_Foreach(installation) {
		/* Check if the installation should be displayed in base list
		 * don't check range because maybe UFO will get closer */
		if (AII_InstallationCanShoot(installation))
			LIST_AddString(&baseList, va("^B%s", installation->name));
	}

	if (baseList)
		UI_RegisterLinkedListText(TEXT_BASE_LIST, baseList);
	else
		UI_RegisterText(TEXT_BASE_LIST, _("No defence system operational or no weapon or ammo equipped."));

	/* Stop time */
	CP_GameTimeStop();

	/* Display the popup */
	UI_PushWindow("popup_intercept", NULL, NULL);
}

/**
 * @brief return the selected aircraft in popup_intercept
 * Close the popup if required
 */
static aircraft_t* CL_PopupInterceptGetAircraft (void)
{
	int num;

	if (Cmd_Argc() < 2)
		return NULL;

	/* Get the selected aircraft */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= popupIntercept.numAircraft)
		return NULL;

	UI_PopWindow(qfalse);
	if (!popupIntercept.aircraft[num])
		return NULL;
	return popupIntercept.aircraft[num];
}

/**
 * @brief User select an item in the popup_aircraft
 * Make the aircraft attack the corresponding mission or UFO
 */
static void CL_PopupInterceptClick_f (void)
{
	aircraft_t *aircraft;
	base_t *base;

	/* Get the selected aircraft */
	aircraft = CL_PopupInterceptGetAircraft();
	if (aircraft == NULL)
		return;

	/* Aircraft can start if only Command Centre in base is operational. */
	base = aircraft->homebase;
	if (!B_GetBuildingStatus(base, B_COMMAND)) {
		/** @todo are these newlines really needed? at least the first should be handled by the menu code */
		UI_Popup(_("Notice"), _("No Command Centre operational in homebase\nof this aircraft.\n\nAircraft cannot start.\n"));
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
	if (aircraft == NULL)
		return;

	/* Display aircraft menu */
	AIR_AircraftSelect(aircraft);
	MAP_ResetAction();
	B_SelectBase(aircraft->homebase);
	UI_PushWindow("aircraft", NULL, NULL);
}

/**
 * @brief User select a base in the popup_aircraft
 * Make the base attack the corresponding UFO
 */
static void CL_PopupInterceptBaseClick_f (void)
{
	int num, i;
	base_t* base;
	installation_t *installation;
	qboolean atLeastOneBase = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\tnum=num in base list\n", Cmd_Argv(0));
		return;
	}

	/* If popup is opened, that means that ufo is selected on geoscape */
	if (MAP_GetSelectedUFO() == NULL)
		return;

	num = atoi(Cmd_Argv(1));

	base = NULL;
	while ((base = B_GetNext(base)) != NULL) {
		/* Check if the base should be displayed in base list */
		if (AII_BaseCanShoot(base)) {
			num--;
			atLeastOneBase = qtrue;
			if (num < 0)
				break;
		}
	}

	installation = NULL;
	if (num >= 0) { /* don't try to find an installation if we already found the right base */
		INS_Foreach(inst) {
			/* Check if the installation should be displayed in base list */
			if (AII_InstallationCanShoot(inst)) {
				num--;
				atLeastOneBase = qtrue;
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
	if (installation) {
		for (i = 0; i < installation->installationTemplate->maxBatteries; i++)
			installation->batteries[i].target = MAP_GetSelectedUFO();
	} else {
		for (i = 0; i < base->numBatteries; i++)
			base->batteries[i].target = MAP_GetSelectedUFO();
		for (i = 0; i < base->numLasers; i++)
			base->lasers[i].target = MAP_GetSelectedUFO();
	}

	UI_PopWindow(qfalse);
}

/**
 * @brief Initialise popups
 */
void CL_PopupInit (void)
{
	/* popup_aircraft commands */
	Cmd_AddCommand("popup_aircraft_action_click", CL_PopupAircraftClick_f, NULL);

	/* popup_intercept commands */
	Cmd_AddCommand("ships_click", CL_PopupInterceptClick_f, NULL);
	Cmd_AddCommand("ships_rclick", CL_PopupInterceptRClick_f, NULL);
	Cmd_AddCommand("bases_click", CL_PopupInterceptBaseClick_f, NULL);

	/* popup_homebase commands */
	Cmd_AddCommand("change_homebase", CL_PopupChangeHomebase_f, NULL);

	OBJZERO(popupIntercept);
	OBJZERO(popupAircraft);
}

/**
 * @brief Wrapper around @c UI_Popup which also stops the time
 */
void CP_PopupList (const char *title, const char *text)
{
	CP_GameTimeStop();
	CP_Popup(title, text);
}

/**
 * @brief Wrapper around @c UI_Popup
 */
void CP_Popup (const char *title, const char *text)
{
	UI_Popup(title, text);
}
