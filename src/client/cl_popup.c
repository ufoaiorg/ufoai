/**
 * @file cl_popup.c
 * @brief Manage popups
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
#include "cl_mapfightequip.h"
#include "cl_map.h"
#include "cl_popup.h"
#include "menu/m_popup.h"

/* popup_aircraft display the actions availables for an aircraft */

#define POPUP_AIRCRAFT_MAX_ITEMS	10		/**< Max items displayed in popup_aircraft */
#define POPUP_AIRCRAFT_MAX_TEXT		2048	/**< Max size of text displayed in popup_aircraft */

/**
 * @brief Enumerate type of actions available for popup_aircraft
 */
typedef enum {
	POPUP_AIRCRAFT_ACTION_BACKTOBASE = 1,	/**< Aircraft back to base */
	POPUP_AIRCRAFT_ACTION_STOP = 2,			/**< Aircraft stops */
	POPUP_AIRCRAFT_ACTION_MOVETOMISSION = 3,/**< Aircraft move to a mission */
	POPUP_AIRCRAFT_ACTION_NONE,				/**< Do nothing */

	POPUP_AIRCRAFT_ACTION_MAX
} popup_aircraft_action_e;

/**
 * @brief Structure to store information about popup_aircraft
 */
typedef struct popup_aircraft_s {
	int nbItems;			/**< Count of items displayed in popup_aircraft */
	aircraft_t *aircraft;		/**< Aircraft linked to popup_aircraft */
	popup_aircraft_action_e itemsAction[POPUP_AIRCRAFT_MAX_ITEMS];	/**< Action type of items */
	int itemsId[POPUP_AIRCRAFT_MAX_ITEMS];		/**< IDs corresponding to items */
	char text_popup[POPUP_AIRCRAFT_MAX_TEXT];	/**< Text displayed in popup_aircraft */
} popup_aircraft_t;

/** FIXME: Save me */
static popup_aircraft_t popupAircraft; /**< Data about popup_aircraft */

/* popup_intercept display list of aircraft availables to move to a mission or a UFO */

#define POPUP_INTERCEPT_MAX_AIRCRAFT 64	/**< Max aircraft in popup list */

typedef struct popup_intercept_s {
	int numAircraft;	/**< Count of aircraft displayed in list */
	aircraft_t *aircraft[POPUP_INTERCEPT_MAX_AIRCRAFT];	/**< List of aircrafts. */
	mission_t* mission;	/**< Mission the selected aircraft have to move to */
	aircraft_t* ufo;		/**< UFO the selected aircraft have to move to */
} popup_intercept_t;

static popup_intercept_t popupIntercept;	/**< Data about popup_intercept */

/*========================================
POPUP_AIRCRAFT
========================================*/

/**
 * @brief Display the popup_aircraft
 * @sa CL_DisplayPopupIntercept
 */
void CL_DisplayPopupAircraft (aircraft_t* aircraft)
{
	const linkedList_t *list = ccs.missions;

	/* Initialise popup_aircraft datas */
	if (!aircraft)
		return;
	popupAircraft.aircraft = aircraft;
	popupAircraft.nbItems = 0;
	memset(popupAircraft.text_popup, 0, sizeof(popupAircraft.text_popup));
	mn.menuText[TEXT_POPUP] = popupAircraft.text_popup;

	/* Set static datas in popup_aircraft */
	popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_BACKTOBASE;
	Q_strcat(popupAircraft.text_popup, va(_("Back to base\t%s\n"), aircraft->homebase->name), POPUP_AIRCRAFT_MAX_TEXT);
	popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_STOP;
	Q_strcat(popupAircraft.text_popup, _("Stop\n"), POPUP_AIRCRAFT_MAX_TEXT);

	/* Set missions in popup_aircraft */
	if (aircraft->teamSize > 0) {
		for (; list; list = list->next) {
			mission_t *tempMission = (mission_t *)list->data;
			if ((tempMission->stage == STAGE_NOT_ACTIVE) || !tempMission->onGeoscape) {
				continue;
			}
			if (tempMission->pos) {
				popupAircraft.itemsId[popupAircraft.nbItems] = MAP_GetIdxByMission(tempMission);
				popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_MOVETOMISSION;
				Q_strcat(popupAircraft.text_popup, va(_("Mission\t%s (%s)\n"), CP_MissionToTypeString(tempMission), 	_(tempMission->location)), POPUP_AIRCRAFT_MAX_TEXT);
			}
		}
	}

	/* Display popup_aircraft menu */
	MN_PushMenu("popup_aircraft");
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
	if (num < 0 || num >= popupAircraft.nbItems)
		return;

	MN_PopMenu(qfalse); /* Close popup */

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
	case POPUP_AIRCRAFT_ACTION_MOVETOMISSION:	/* Aircraft move to mission */
		mission = MAP_GetMissionByIdx(popupAircraft.itemsId[num]);
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

/**
 * @brief Display the popup_intercept
 * @sa CL_DisplayPopupAircraft
 */
void CL_DisplayPopupIntercept (mission_t* mission, aircraft_t* ufo)
{
	static char aircraftListText[1024];
	static char baseListText[1024];
	char *s;
	int i, j;
	aircraft_t *air;
	qboolean somethingWritten, notEnoughFuel;

	/* One parameter must be specified, mission or ufo */
	if (!mission && !ufo)
		return;
	popupIntercept.mission = mission;
	popupIntercept.ufo = mission ? NULL : ufo;

	/* Create the list of aircraft, and write the text to display in popup */
	popupIntercept.numAircraft = 0;
	memset(aircraftListText, 0, sizeof(aircraftListText));
	for (j = 0; j < gd.numBases; j++) {
		if (!gd.bases[j].founded)
			continue;

		for (i = 0; i < gd.bases[j].numAircraftInBase; i++) {
			air = &gd.bases[j].aircraft[i];
			notEnoughFuel = qfalse;

			/* if dependencies of hangar are missing, you can't send aircraft */
			switch (air->weight) {
			case AIRCRAFT_SMALL:
				if (!gd.bases[j].hasBuilding[B_SMALL_HANGAR])
					continue;
				break;
			case AIRCRAFT_LARGE:
				if (!gd.bases[j].hasBuilding[B_HANGAR])
					continue;
				break;
			default:
				Com_Printf("CL_DisplayPopupIntercept: Unknown weight of aircraft '%s': %i\n", air->id, air->weight);
			}

			/* if aircraft is empty we can't send it on a ground mission */
			if (mission && (air->teamSize <= 0))
				continue;
			/* don't show aircraft with no weapons or no ammo, or crafts that
			 * can't even reach the target */
			if (ufo) {
				/* Does the aircraft has weapons and ammo ? */
				if (AIRFIGHT_ChooseWeapon(air->weapons, air->maxWeapons, air->pos, air->pos) == AIRFIGHT_WEAPON_CAN_NEVER_SHOOT) {
					Com_DPrintf(DEBUG_CLIENT, "CL_DisplayPopupIntercept: No useable weapon found in craft '%s' (%i)\n", air->id, air->maxWeapons);
					continue;
				}
				/* now check the aircraft range */
				if (!AIR_AircraftHasEnoughFuel(air, ufo->pos)) {
					Com_DPrintf(DEBUG_CLIENT, "CL_DisplayPopupIntercept: Target out of reach for craft '%s'\n", air->id);
					notEnoughFuel = qtrue;
				}
			}

			if (!notEnoughFuel)
				Q_strcat(aircraftListText, "^B", sizeof(aircraftListText));
			if (ufo)
				s = va("%s (%i/%i)\t%s\t%s\n", _(air->shortname), air->teamSize, air->maxTeamSize, AIR_AircraftStatusToName(air), gd.bases[j].name);
			else {
				const float distance = MAP_GetDistance(air->pos, mission->pos);
				s = va("%s (%i/%i)\t%s\t%s\t%s\n", _(air->shortname), air->teamSize, air->maxTeamSize, AIR_AircraftStatusToName(air), gd.bases[j].name, CL_SecondConvert(3600.0f * distance / air->stats[AIR_STATS_SPEED]));
			}
			Q_strcat(aircraftListText, s, sizeof(aircraftListText));
			assert(air->homebase == &gd.bases[j]);
			popupIntercept.aircraft[popupIntercept.numAircraft] = air;
			popupIntercept.numAircraft++;
			if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
				break;
		}
		/* also leave the outer loop */
		if (popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT)
			break;
	}
	if (popupIntercept.numAircraft)
		mn.menuText[TEXT_AIRCRAFT_LIST] = aircraftListText;
	else if (mission)
		mn.menuText[TEXT_AIRCRAFT_LIST] = _("No craft available, or no tactical teams assigned to available craft.");
	else if (ufo)
		mn.menuText[TEXT_AIRCRAFT_LIST] = _("No craft available or no weapon or ammo equipped.");

	if (ufo) {
		somethingWritten = qfalse;
		memset(baseListText, 0, sizeof(baseListText));
		/* Create the list of base, and write the text to display in popup
		 * don't use the same loop than above, to avoid leaving the loop if popupIntercept.numAircraft >= POPUP_INTERCEPT_MAX_AIRCRAFT */
		for (j = 0; j < gd.numBases; j++) {
			if (!gd.bases[j].founded)
				continue;

			/* Check if the base should be displayed in base list
			 * don't check range because maybe UFO will get closer */
			if (AII_BaseCanShoot(B_GetBase(j))) {
				Q_strcat(baseListText, va("^B%s\n", gd.bases[j].name), sizeof(baseListText));
				somethingWritten = qtrue;
			}
		}
		if (somethingWritten)
			mn.menuText[TEXT_BASE_LIST] = baseListText;
		else
			mn.menuText[TEXT_BASE_LIST] = _("No defense system operational or no weapon or ammo equipped.");
		/* Display base list in popup */
		Cvar_Set("mn_displaybaselist", "1");
	} else
		/* Don't display base list in popup */
		Cvar_Set("mn_displaybaselist", "0");

	/* Stop time */
	CL_GameTimeStop();

	/* Display the popup */
	MN_PushMenu("popup_intercept");
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

	MN_PopMenu(qfalse);
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
	if (!base->hasBuilding[B_COMMAND]) {
		MN_Popup(_("Notice"), _("No Command Centre operational in homebase\nof this aircraft.\n\nAircraft cannot start.\n"));
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
	Cmd_ExecuteString(va("mn_select_base %i", aircraft->homebase->idx));
	MN_PushMenu("aircraft");
}

/**
 * @brief User select a base in the popup_aircraft
 * Make the base attack the corresponding UFO
 */
static void CL_PopupInterceptBaseClick_f (void)
{
	int num, baseIdx, i;
	qboolean atLeastOneBase = qfalse;

	/* If popup is opened, that means that ufo is selected on geoscape */
	assert(selectedUFO);

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\tnum=num in base list\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (baseIdx = 0; baseIdx < gd.numBases; baseIdx++) {
		if (!gd.bases[baseIdx].founded)
			continue;

		/* Check if the base should be displayed in base list */
		if (AII_BaseCanShoot(B_GetBase(baseIdx))) {
			num--;
			atLeastOneBase = qtrue;
			if (num < 0)
				break;
		}
	}

	if (!atLeastOneBase && !num) {
		/* no base in list: no error message
		 * note that num should always be 0 if we enter this loop, unless this function is called from console
		 * so 2nd part of the test should be useless in most case */
		return;
	} else if (num >= 0) {
		Com_Printf("CL_PopupInterceptBaseClick_f()... Number given in argument (%i) is bigger than number of base in list.\n", num);
		return;
	}

	for (i = 0; i < gd.bases[baseIdx].maxBatteries; i++)
		gd.bases[baseIdx].targetMissileIdx[i] = selectedUFO - gd.ufos;
	for (i = 0; i < gd.bases[baseIdx].maxLasers; i++)
		gd.bases[baseIdx].targetLaserIdx[i] = selectedUFO - gd.ufos;

	MN_PopMenu(qfalse);
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

	memset(&popupIntercept, 0, sizeof(popup_intercept_t));
	memset(&popupAircraft, 0, sizeof(popup_aircraft_t));
}
