/**
 * @file cl_popup.c
 * @brief Manage popups
 */

#include "client.h"
#include "cl_global.h"

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

/* global_popup */
extern void CL_PopupInit(void);
extern void CL_PopupNotifyMIssionRemoved(const actMis_t* mission);
extern void CL_PopupNotifyUfoRemoved(const aircraft_t* ufo);
extern void CL_PopupNotifyUfoDisappeared(const aircraft_t* ufo);

#if 0

/* popup_interception_ready */
extern void CL_DisplayPopupInterceptionReady(aircraft_t* aircraft, actMis_t* mission);
static void CL_PopupInterceptionReadyEnter_f(void);
static void CL_PopupInterceptionReadyAuto_f(void);
static void CL_PopupInterceptionReadyCancel_f(void);

#endif

/* popup_aircraft */
extern void CL_DisplayPopupAircraft(const aircraft_t* aircraft);
static void CL_PopupAircraftClick_f(void);
static void CL_PopupAircraftNotifyMissionRemoved(const actMis_t* mission);

/* popup_intercept */
extern void CL_DisplayPopupIntercept(struct actMis_s* mission, aircraft_t* ufo);
static aircraft_t* CL_PopupInterceptGetAircraft(void);
static void CL_PopupInterceptClick_f(void);
static void CL_PopupInterceptRClick_f(void);
static void CL_PopupInterceptNotifyMissionRemoved(const actMis_t* mission);
static void CL_PopupInterceptNotifyUfoRemoved(const aircraft_t* ufo);
static void CL_PopupInterceptNotifyUfoDisappeared(const aircraft_t* ufo);

/**
 * @brief Initialise popups
 */
extern void CL_PopupInit(void)
{
	/* popup_aircraft commands */
	Cmd_AddCommand("popup_aircraft_action_click", CL_PopupAircraftClick_f, NULL);

	/* popup_intercept commands */
	Cmd_AddCommand("ships_click", CL_PopupInterceptClick_f, NULL);
	Cmd_AddCommand("ships_rclick", CL_PopupInterceptRClick_f, NULL);

	/* popup_interception_ready commands */
	/*Cmd_AddCommand("popup_interception_ready_enter", CL_PopupInterceptionReadyEnter_f, NULL);
	Cmd_AddCommand("popup_interception_ready_auto", CL_PopupInterceptionReadyAuto_f, NULL);
	Cmd_AddCommand("popup_interception_ready_cancel", CL_PopupInterceptionReadyCancel_f, NULL);
	*/
}

/**
 * @brief Notify that a mission has been removed
 */
extern void CL_PopupNotifyMIssionRemoved(const actMis_t* mission)
{
	/* Notify all popups */
	CL_PopupAircraftNotifyMissionRemoved(mission);
	CL_PopupInterceptNotifyMissionRemoved(mission);
}

/**
 * @brief Notify that an ufo has been removed
 */
extern void CL_PopupNotifyUfoRemoved(const aircraft_t* ufo)
{
	/* Notify all popups */
	CL_PopupInterceptNotifyUfoRemoved(ufo);
}

/**
 * @brief Notify popups that an ufo has disappeared on radars
 */
extern void CL_PopupNotifyUfoDisappeared(const aircraft_t* ufo)
{
	/* Notify all popups */
	CL_PopupInterceptNotifyUfoDisappeared(ufo);
}

#if 0
/*========================================
 *
 * POPUP_INTERCEPT_READY
 *
 *========================================*/

/* Ask confirmation just before entering in mission */

/**
 * @brief Structure to store information about popup_interception_ready
 */
typedef struct popup_interception_ready_s {
	aircraft_t* aircraft;
	actMis_t* mission;
} popup_interception_ready_t;

popup_interception_ready_t popupInterceptionReady;	/**< Data about popup_interception_ready */

/**
 * @brief Display the popup_interception_ready
 */
extern void CL_DisplayPopupInterceptionReady(aircraft_t* aircraft, actMis_t* mission)
{
	if (! popupInterceptionReady.aircraft || ! popupInterceptionReady.mission)
		return;

	popupInterceptionReady.aircraft = aircraft;
	popupInterceptionReady.mission = mission;
	MN_PushMenu("popup_intercept_ready");
}

/**
 * @brief Enter in the mission of the popup_interception_ready
 */
static void CL_PopupInterceptionReadyEnter_f(void)
{
	MN_PopMenu(qfalse);	/* Close popup */
	if (! popupInterceptionReady.aircraft || ! popupInterceptionReady.mission)
		return;

	CL_GameGo(popupInterceptionReady.mission->def, popupInterceptionReady.aircraft);
}

/**
 * @brief Auto mission of the popup_interception_ready
 */
static void CL_PopupInterceptionReadyAuto_f(void)
{
	MN_PopMenu(qfalse);	/* Close popup */
	if (! popupInterceptionReady.aircraft || ! popupInterceptionReady.mission)
		return;
	if (popupInterceptionReady.aircraft->status != AIR_DROP)
		return;

	/* TO DO : launch auto mission */
}

/**
 * @brief Cancel the popup_interception_ready
 */
static void CL_PopupInterceptionReadyCancel_f(void)
{
	MN_PopMenu(qfalse);	/* Close popup */

	/* Stop the aircraft */
	if (popupInterceptionReady.aircraft->status == AIR_DROP)
		CL_AircraftStop(popupInterceptionReady.aircraft);
}

#endif
/*========================================
 *
 * POPUP_AIRCRAFT
 *
 *========================================*/

/* popup_aircraft display the actions availables for an aircraft */

#define POPUP_AIRCRAFT_MAX_ITEMS	10		/**< Max items displayed in popup_aircraft */
#define POPUP_AIRCARFT_MAX_TEXT		2048	/**< Max size of text displayed in popup_aircraft */

/**
 * Enumerate type of actions available for popup_aircraft
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
typedef struct popup_aircarft_s {
	int nbItems;			/**< Count of items displayed in popup_aircraft */
	int aircraft_idx;		/**< Aircraft linked to popup_aircraft */
	popup_aircraft_action_e itemsAction[POPUP_AIRCRAFT_MAX_ITEMS];	/**< Action type of items */
	int itemsId[POPUP_AIRCRAFT_MAX_ITEMS];	/**< IDs corresponding to items */
	char text_popup[POPUP_AIRCARFT_MAX_TEXT];	/**< Text displayed in popup_aircraft */

} popup_aircraft_t;

popup_aircraft_t popupAircraft; /** Data about popup_aircraft */

/**
 * @brief Display the popup_aircraft
 */
extern void CL_DisplayPopupAircraft(const aircraft_t* aircraft)
{
	int i;

	/* Initialise popup_aircraft datas */
	if (! aircraft)
		return;
	popupAircraft.aircraft_idx = aircraft->idx;
	popupAircraft.nbItems = 0;
	memset(popupAircraft.text_popup, 0, POPUP_AIRCARFT_MAX_TEXT);
	menuText[TEXT_POPUP] = popupAircraft.text_popup;

	/* Set static datas in popup_aircraft */
	popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_BACKTOBASE;
	Q_strcat(popupAircraft.text_popup, va(_("Back to base\t%s\n"), gd.bases[aircraft->idxBase].name), POPUP_AIRCARFT_MAX_TEXT);
	popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_STOP;
	Q_strcat(popupAircraft.text_popup, _("Stop\n"), POPUP_AIRCARFT_MAX_TEXT);

	/* Set missions in popup_aircraft */
	if (*aircraft->teamSize > 0) {
		for (i = 0 ; i < ccs.numMissions ; i++) {
			popupAircraft.itemsId[popupAircraft.nbItems] = i;
			popupAircraft.itemsAction[popupAircraft.nbItems++] = POPUP_AIRCRAFT_ACTION_MOVETOMISSION;
			Q_strcat(popupAircraft.text_popup, va(_("Mission\t%s (%s)\n"), _(ccs.mission[i].def->type), _(ccs.mission[i].def->location)), POPUP_AIRCARFT_MAX_TEXT);
		}
	}

	/* Display popup_aircraft menu */
	MN_PushMenu("popup_aircraft");
}

/**
 * @ brief User just select an item in the popup_aircraft
 */
static void CL_PopupAircraftClick_f(void)
{
	int num, id;
	aircraft_t* aircraft;

	Com_DPrintf("CL_PopupAircraftClick\n");

	/* Get num of item selected in list */
	if (Cmd_Argc() < 2)
		return;
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num > popupAircraft.nbItems++)
		return;

	MN_PopMenu(qfalse); /* Close popup */

	/* Get aircraft associated with the popup_aircraft */
	aircraft = CL_AircraftGetFromIdx(popupAircraft.aircraft_idx);
	if (aircraft == NULL)
		return;

	/* Execute action corresponding to item selected */
	switch (popupAircraft.itemsAction[num]) {
	case POPUP_AIRCRAFT_ACTION_BACKTOBASE:	/* Aircraft back to base */
		CL_AircraftReturnToBase(aircraft);
		break;
	case POPUP_AIRCRAFT_ACTION_STOP:		/* Aircraft stop */
		aircraft->status = AIR_IDLE;
		break;
	case POPUP_AIRCRAFT_ACTION_MOVETOMISSION:	/* Aircraft move to mission */
		/* Get mission */
		id = popupAircraft.itemsId[num];
		if (id >= 0 && id < ccs.numMissions)
			CL_SendAircraftToMission(aircraft, ccs.mission + id);
		break;
	case POPUP_AIRCRAFT_ACTION_NONE:
		break;
	default:
		Com_Printf("CL_PopupAircraftClick: type of action unknow %i\n", popupAircraft.itemsAction[num]);
	}
}

/**
 * @brief Notify the popup_aircraft system that a mission has been removed
 */
static void CL_PopupAircraftNotifyMissionRemoved(const actMis_t* mission)
{
	int num = mission - ccs.mission, i;

	/* Deactive elements of list that allow to move to removed mission */
	for (i = 0 ; i < popupAircraft.nbItems ; i++)
		if (popupAircraft.itemsAction[i] == POPUP_AIRCRAFT_ACTION_MOVETOMISSION) {
			if (popupAircraft.itemsId[i] == num)
				popupAircraft.itemsAction[i] = POPUP_AIRCRAFT_ACTION_NONE;
			else if (popupAircraft.itemsId[i] > num)
				popupAircraft.itemsId[i]--;
		}
}


/*========================================
 *
 * POPUP_INTERCEPT
 *
 *========================================*/

/* popup_intercept display list of aircraft availables to move to a mission or an ufo */

#define POPUP_INTERCEPT_MAX_AIRCRAFT 10	/**< Max aircrafts in popup list */

typedef struct popup_intercept_s {
	int numAircraft;	/**< Count of aircrafts displayed in list */
	int idBaseAircraft[POPUP_INTERCEPT_MAX_AIRCRAFT];	/**< Base ID of aircrafts in list */
	int idInBaseAircraft[POPUP_INTERCEPT_MAX_AIRCRAFT];	/**< ID in base of aircrafts in list */
	actMis_t*	mission;	/**< Mission the selected aircraft have to move to */
	aircraft_t*	ufo;		/**< UFO the selected aircraft have to move to */
} popup_intercept_t;

popup_intercept_t popupIntercept;	/**< Data about popup_intercept */


/**
 * @brief Display the popup_intercept
 */
extern void CL_DisplayPopupIntercept(actMis_t* mission, aircraft_t* ufo)
{
	static char aircraftListText[1024];
	char *s;
	int i, j;
	aircraft_t *air;

	/* One parameter must be specified, mission or ufo */
	if (! mission && ! ufo)
		return;
	popupIntercept.mission = mission;
	popupIntercept.ufo = mission ? NULL : ufo;

	/* Create the list of aircrafts, and write the text to display in popup */
	popupIntercept.numAircraft = 0;
	memset(aircraftListText, 0, sizeof(aircraftListText));
	for (j = 0; j < gd.numBases; j++) {
		if (!gd.bases[j].founded)
			continue;

		for (i = 0; i < gd.bases[j].numAircraftInBase; i++) {
			air = &gd.bases[j].aircraft[i];
			s = va("%s (%i/%i)\t%s\t%s\n", air->name, *air->teamSize, air->size, CL_AircraftStatusToName(air), gd.bases[j].name);
			Q_strcat(aircraftListText, s, sizeof(aircraftListText));
			popupIntercept.idBaseAircraft[popupIntercept.numAircraft] = j;
			popupIntercept.idInBaseAircraft[popupIntercept.numAircraft] = i;
			popupIntercept.numAircraft++;
		}
	}
	menuText[TEXT_AIRCRAFT_LIST] = aircraftListText;

	/* Display the popup */
	if (popupIntercept.numAircraft > 0)
		MN_PushMenu("popup_intercept");
}

/**
 * @brief return the selected aircraft in popup_intercept
 * Close the popup if required
 */
static aircraft_t* CL_PopupInterceptGetAircraft(void)
{
	int num;

	if (Cmd_Argc() < 2)
		return NULL;

	/* Get the selected aircraft */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= popupIntercept.numAircraft)
		return NULL;
	MN_PopMenu(qfalse);
	if (popupIntercept.idBaseAircraft[num] >= gd.numBases
		|| popupIntercept.idInBaseAircraft[num] >= gd.bases[popupIntercept.idBaseAircraft[num]].numAircraftInBase)
		return NULL;
	return gd.bases[popupIntercept.idBaseAircraft[num]].aircraft + num;
}

/**
 * @brief User select an item in the popup_aircraft
 * Make the aircraft attack the corresponding mission or UFO
 */
static void CL_PopupInterceptClick_f(void)
{
	aircraft_t* aircraft;

	/* Get the selected aircraft */
	aircraft = CL_PopupInterceptGetAircraft();
	if (aircraft == NULL)
		return;


	/* Set action to aircraft */
	if (popupIntercept.mission)
		CL_SendAircraftToMission(aircraft, popupIntercept.mission);	/* Aircraft move to mission */
	else if (popupIntercept.ufo)
		CL_SendAircraftPurchasingUfo(aircraft, popupIntercept.ufo);	/* Aircraft purchase ufo */
}

/**
 * @brief User select an item in the popup_aircraft with right click
 * Opens up the aircraftn
 */
static void CL_PopupInterceptRClick_f(void)
{
	aircraft_t* aircraft;

	/* Get the selected aircraft */
	aircraft = CL_PopupInterceptGetAircraft();
	if (aircraft == NULL)
		return;

	/* Display aircraft menu */
	baseCurrent = gd.bases + aircraft->idxBase;
	baseCurrent->aircraftCurrent = aircraft->idxInBase;
	CL_AircraftSelect();
	MAP_ResetAction();
	Cbuf_ExecuteText(EXEC_NOW, va("mn_select_base %i\n", baseCurrent->idx));
	MN_PushMenu("aircraft");
}

/**
 * @brief Notify the popup_intercept system that a mission has been removed
 */
static void CL_PopupInterceptNotifyMissionRemoved(const actMis_t* mission)
{
	if (popupIntercept.mission == mission)
		popupIntercept.mission = NULL;
}

/**
 * @brief Notify the popup_intercept system that an ufo has been removed
 */
static void CL_PopupInterceptNotifyUfoRemoved(const aircraft_t* ufo)
{
	if (popupIntercept.ufo == ufo)
		popupIntercept.ufo = NULL;
}

/**
 * @brief Notify the popup_intercept system than an ufo has disappeared on radars
 */
static void CL_PopupInterceptNotifyUfoDisappeared(const aircraft_t* ufo)
{
	CL_PopupInterceptNotifyUfoRemoved(ufo);
}
