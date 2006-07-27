/**
 * @file cl_aircraft.c
 * @brief Most of the aircraft related stuff
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

aircraft_t aircraft[MAX_AIRCRAFT];
int numAircraft;

static void CL_AircraftReturnToBase(aircraft_t *air);
extern qboolean CL_SendAircraftToMission(aircraft_t* aircraft, actMis_t* mission);

/* == POPUP_AIRCRAFT ======================================================= */

#define POPUP_AIRCRAFT_MAX_ITEMS	10		/**< Max items displayed in popup_aircraft */
#define POPUP_AIRCARFT_MAX_TEXT		2048	/**< Max size of text displayed in popup_aircraft */

/**
 * Enumerate type of actions available for popup_aircraft
 */
typedef enum {
	POPUP_AIRCRAFT_ACTION_BACKTOBASE = 1,	/**< Aircraft back to base */
	POPUP_AIRCRAFT_ACTION_STOP = 2,			/**< Aircraft stops */
	POPUP_AIRCRAFT_ACTION_MOVETOMISSION = 3,/**< Aircraft move to a mission */

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
	char text_popup[POPUP_AIRCARFT_MAX_TEXT];	/** Text displayed in popup_aircraft */

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
extern void CL_PopupAircraftClick_f(void) {
	int num, id;
	aircraft_t* aircraft;

	Com_Printf("CL_PopupAircraftClick\n");

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
		default:
			Com_Printf("CL_PopupAircraftClick: type of action unknow %i\n", popupAircraft.itemsAction[num]);
	}
}

/* =========================================================== */
#define DISTANCE 1

/**
  * @brief
  */
void CL_ListAircraft_f(void)
{
	int i, j;
	base_t *base;
	aircraft_t *air;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		Com_Printf("Aircrafts in base %s: %i\n", base->name, base->numAircraftInBase);
		for (i = 0; i < base->numAircraftInBase; i++) {
			air = &base->aircraft[i];
			Com_Printf("Aircraft %s\n", air->name);
			Com_Printf("...name %s\n", air->id);
			Com_Printf("...speed %0.2f\n", air->speed);
			Com_Printf("...type %i\n", air->type);
			Com_Printf("...size %i\n", air->size);
			Com_Printf("...status %s\n", CL_AircraftStatusToName(air));
			Com_Printf("...pos %.0f:%.0f\n", air->pos[0], air->pos[1]);
		}
	}
}

/**
  * @brief Start an aircraft or stops the current mission and let the aircraft idle around
  */
void CL_AircraftStart_f(void)
{
	aircraft_t *air;

	assert(baseCurrent);

	if (baseCurrent->aircraftCurrent < 0) {
		Com_DPrintf("Error - there is no aircraftCurrent in this base\n");
		return;
	}

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if (air->status < AIR_IDLE) {
		air->pos[0] = baseCurrent->pos[0] + 2;
		air->pos[1] = baseCurrent->pos[1] + 2;
	}
	MN_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	air->status = AIR_IDLE;

	MAP_SelectAircraft(air);
}

/**
  * @brief
  */
void CL_AircraftInit(void)
{
	aircraft_t *air;
	int i = 0;

	for (i = 0; i < numAircraft; i++) {
		air = &aircraft[i];
		/* link with tech pointer */
		Com_DPrintf("...aircraft: %s\n", air->name);
		if (*air->weapon_string) {
			Com_DPrintf("....weapon: %s\n", air->weapon_string);
			air->weapon = RS_GetTechByID(air->weapon_string);
		} else
			air->weapon = NULL;

		if (*air->shield_string) {
			/* link with tech pointer */
			Com_DPrintf("....shield: %s\n", air->shield_string);
			air->shield = RS_GetTechByID(air->shield_string);
		} else
			air->shield = NULL;
		air->homebase = &gd.bases[air->idxBase];
		air->teamSize = &gd.bases[air->idxBase].numOnTeam[air->idxInBase];
	}
	Com_Printf("...aircraft inited\n");
}

/**
  * @brief
  */
char *CL_AircraftStatusToName(aircraft_t * air)
{
	assert(air);
	switch (air->status) {
	case AIR_NONE:
		return _("Nothing - should not be displayed");
	case AIR_HOME:
		return _("At homebase");
	case AIR_REFUEL:
		return _("Refuel");
	case AIR_IDLE:
		return _("Idle");
	case AIR_TRANSIT:
		return _("On transit");
	case AIR_MISSION:
		return _("Moving to mission");
	case AIR_DROP:
		return _("Ready for drop down");
	case AIR_INTERCEPT:
		return _("On interception");
	case AIR_TRANSPORT:
		return _("Transportmission");
	case AIR_RETURNING:
		return _("Returning to homebase");
	default:
		Com_Printf("Error: Unknown aircraft status for %s\n", air->name);
	}
	return NULL;
}

/**
  * @brief
  */
void CL_NewAircraft_f(void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: aircraft_new <type>\n");
		return;
	}

	if (!baseCurrent)
		return;

	CL_NewAircraft(baseCurrent, Cmd_Argv(1));
}

/**
  * @brief
  */
void MN_NextAircraft_f(void)
{
	if (!baseCurrent)
		return;

	if ((int) Cvar_VariableValue("mn_aircraft_id") < baseCurrent->numAircraftInBase) {
		Cvar_SetValue("mn_aircraft_id", (float) Cvar_VariableValue("mn_aircraft_id") + 1.0f);
		CL_AircraftSelect();
	} else
		Com_DPrintf("mn_aircraft_id: %i - numAircraftInBase: %i\n", (int) Cvar_VariableValue("mn_aircraft_id"), baseCurrent->numAircraftInBase);
}

/**
  * @brief
  */
void MN_PrevAircraft_f(void)
{
	if ((int) Cvar_VariableValue("mn_aircraft_id") > 0) {
		Cvar_SetValue("mn_aircraft_id", (float) Cvar_VariableValue("mn_aircraft_id") - 1.0f);
		CL_AircraftSelect();
	}
}

/**
  * @brief Returns the given aircraft back to homebase
  *
  * call this from baseview via "aircraft_return"
  * calculates the way back to homebase
  */
static void CL_AircraftReturnToBase(aircraft_t *air)
{
	base_t *base;

	if (air && air->status != AIR_HOME) {
		base = (base_t *) air->homebase;
		MAP_MapCalcLine(air->pos, base->pos, &air->route);
		air->status = AIR_RETURNING;
		air->time = 0;
		air->point = 0;
	}
}

/**
  * @brief Script function for CL_AircraftReturnToBase
  *
  * Sends the current aircraft back to homebase and updates
  * the cvars
  */
void CL_AircraftReturnToBase_f(void)
{
	aircraft_t *air;

	if (baseCurrent && baseCurrent->aircraftCurrent >= 0) {
		air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		CL_AircraftReturnToBase(air);
		CL_AircraftSelect();
	}
}

/**
  * @brief Sets aircraftCurrent and updates cvars
  *
  * uses cvar mn_aircraft_id to determine which aircraft to select
  */
void CL_AircraftSelect(void)
{
	aircraft_t *air;
	int aircraftID = (int) Cvar_VariableValue("mn_aircraft_id");
	static char aircraftInfo[256];

	/* calling from console? with no baseCurrent? */
	if (!baseCurrent)
		return;

	/* selecting the first aircraft in base (every base has at least one aircraft) */
	if (aircraftID >= baseCurrent->numAircraftInBase || aircraftID < 0)
		aircraftID = 0;

	air = &baseCurrent->aircraft[aircraftID];

	baseCurrent->aircraftCurrent = aircraftID;

	CL_UpdateHireVar();

	Cvar_Set("mn_aircraftstatus", CL_AircraftStatusToName(air));
	Cvar_Set("mn_aircraftname", air->name);
	Cvar_Set("mn_aircraft_model", air->model);
	Cvar_Set("mn_aircraft_weapon", air->weapon ? air->weapon->name : "");
	Cvar_Set("mn_aircraft_shield", air->shield ? air->shield->name : "");
	Cvar_Set("mn_aircraft_weapon_img", air->weapon ? air->weapon->image_top : "menu/airequip_no_weapon");
	Cvar_Set("mn_aircraft_shield_img", air->shield ? air->shield->image_top : "menu/airequip_no_shield");

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%.0f\n"), air->speed);
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), air->fuel / 1000, air->fuelSize / 1000), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapon:\t%s\n"), air->weapon ? air->weapon->name : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Shield:\t%s"), air->shield ? air->shield->name : _("None")), sizeof(aircraftInfo));
	menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;
}

/**
  * @brief Searches the global aircraft array for a given aircraft
  * @param[in] name Aircraft id
  * @return aircraft_t pointer or NULL if not found
  */
aircraft_t *CL_GetAircraft(char *name)
{
	int i;

	for (i = 0; i < numAircraft; i++) {
		if (!Q_strncmp(aircraft[i].id, name, MAX_VAR))
			return &aircraft[i];
	}

	Com_Printf("Aircraft %s not found (%i)\n", name, numAircraft);
	return NULL;
}

/**
  * @brief Places a new aircraft in the given base
  * @param[in] base Pointer to base where aircraft should be added
  * @param[in] name Name of the aircraft to add
  */
void CL_NewAircraft(base_t *base, char *name)
{
	aircraft_t *air;

	assert(base);

	/* first aircraft is default aircraft */
	base->aircraftCurrent = 0;

	air = CL_GetAircraft(name);
	if (air) {
		/* copy from global aircraft list to base aircraft list */
		/* we do this because every aircraft can have its own parameters */
		memcpy(&base->aircraft[base->numAircraftInBase], air, sizeof(aircraft_t));
		/* now lets use the aircraft array for the base to set some parameters */
		air = &base->aircraft[base->numAircraftInBase];
		air->homebase = base;
		/* for saving and loading a base */
		air->idxBase = base->idx;
		/* this is the aircraft array id in current base */
		/* NOTE: when we send the aircraft to another base this has to be changed, too */
		air->idxInBase = base->numAircraftInBase;
		/* link the teamSize pointer in */
		/* NOTE: when we load a savegame, this has to be updated, too */
		air->teamSize = &base->numOnTeam[base->numAircraftInBase];
		Q_strncpyz(messageBuffer, va(_("You've got a new aircraft (a %s) in base %s"), air->name, base->name), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf("Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, air->pos);

		base->numAircraftInBase++;
		Com_DPrintf("Aircraft for base %s: %s\n", base->name, air->name);
	}
}

/**
  * @brief
  */
void CL_CheckAircraft(aircraft_t * air)
{
	if (air->fuel <= 0 && air->status >= AIR_IDLE) {
		MN_AddNewMessage(_("Notice"), _("Your dropship has low fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
		CL_AircraftReturnToBase(air);
	}

	if (air->status == AIR_MISSION) {
		if (abs(air->mission->def->pos[0] - air->pos[0]) < DISTANCE && abs(air->mission->def->pos[1] - air->pos[1]) < DISTANCE) {
			air->mission->def->active = qtrue;
			if (air->status != AIR_DROP && air->fuel > 0) {
				air->status = AIR_DROP;
				gd.interceptAircraft = air->idxInBase;
				baseCurrent = air->homebase;
				MAP_SelectMission(air->mission);
				MN_PushMenu("popup_intercept_ready");
			}
		}
		else
			air->mission->def->active = qfalse;
	}
}

/**
  * @brief
  */
extern void CP_GetRandomPosForAircraft(float *pos)
{
	pos[0] = (rand() % 180) - (rand() % 180);
	pos[1] = (rand() % 90) - (rand() % 90);
}

/**
 * @brief Move the specified aircraft
 * Return true if the aircraft reached its destination
 */
extern qboolean CL_AircraftMakeMove(int dt, aircraft_t* aircraft) {
	float dist, frac;
	int p;

	/* calc distance */
	aircraft->time += dt;
	aircraft->fuel -= dt;
	dist = aircraft->speed * aircraft->time / 3600;
	
	/* Check if destination reached */
	if (dist >= aircraft->route.dist * (aircraft->route.n - 1))
		return qtrue;
	
	/* calc new position */
	frac = dist / aircraft->route.dist;
	p = (int) frac;
	frac -= p;
	aircraft->point = p;
	aircraft->pos[0] = (1 - frac) * aircraft->route.p[p][0] + frac * aircraft->route.p[p + 1][0];
	aircraft->pos[1] = (1 - frac) * aircraft->route.p[p][1] + frac * aircraft->route.p[p + 1][1];
	
	return qfalse;
}

/**
  * @brief
  *
  * TODO: Fuel
  */
void CL_CampaignRunAircraft(int dt)
{
	aircraft_t *air;
	base_t *base;
	int i, j;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		for (i = 0, air = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, air++)
			if (air->homebase) {
				if (air->status > AIR_IDLE) {
					if (CL_AircraftMakeMove(dt, air)) {
						float *end;

						end = air->route.p[air->route.n - 1];
						air->pos[0] = end[0];
						air->pos[1] = end[1];
						if (air->status == AIR_RETURNING) {
							air->status = AIR_REFUEL;
							if (air->fuel < 0)
								air->fuel = 0;
						} else
							air->status = AIR_IDLE;
						CL_CheckAircraft(air);
						continue;
					}

				}
				else if (air->status == AIR_IDLE)
					air->fuel -= dt;
				else if (air->status == AIR_REFUEL) {
					if (air->fuel + dt < air->fuelSize)
						air->fuel += dt;
					else {
						air->fuel = air->fuelSize;
						air->status = AIR_HOME;
					}
				}

				CL_CheckAircraft(air);
			}
	}
}

/**
 * @brief Fills the weapon and shield list of the aircraft equip menu
 */
void CL_AircraftEquipmenuMenuInit_f(void)
{
	static char bufferShields[1024];
	static char bufferWeapons[1024];
	technology_t **list;

	/* shields */
	Com_sprintf(bufferShields, sizeof(bufferShields), _("None\n"));
	list = RS_GetTechsByType(RS_CRAFTSHIELD);
	while (*list) {
		/*Com_Printf("%s\n", (*list)->id);*/
		Q_strcat(bufferShields, va("%s\n", (*list)->name), sizeof(bufferShields) );
		list++;
	}
	menuText[TEXT_LIST] = bufferShields;

	/* weapons */
	Com_sprintf(bufferWeapons, sizeof(bufferWeapons), _("None\n"));
	list = RS_GetTechsByType(RS_CRAFTWEAPON);
	while (*list) {
		/*Com_Printf("%s\n", (*list)->id);*/
		Q_strcat(bufferWeapons, va("%s\n", (*list)->name), sizeof(bufferWeapons) );
		list++;
	}
	menuText[TEXT_AIRCRAFT_LIST] = bufferWeapons;

	/* shield / weapon description */
	menuText[TEXT_STANDARD] = NULL;
}

/**
 * @brief Asseigns the weapon to current selected aircraft when clicked on the list
 */
void CL_AircraftEquipmenuMenuWeaponsClick_f(void)
{
	aircraft_t *air;
	int num;
	static char weaponDesc[512];
	technology_t **list;

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which weapon? */
	num = atoi(Cmd_Argv(1));

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if ( num < 1 ) {
		Com_DPrintf("Reset the aircraft weapon\n");
		air->weapon = NULL;
		Com_sprintf(weaponDesc, sizeof(weaponDesc), _("No weapon assigned"));
		CL_AircraftSelect();
	} else {
		list = RS_GetTechsByType(RS_CRAFTWEAPON);
		/* to prevent overflows we go through the list instead of address it directly */
		while (*list) {
			num--;
			/* found it */
			if (num <= 0) {
				air->weapon = *list;
				Com_sprintf(weaponDesc, sizeof(weaponDesc), (*list)->name);
				CL_AircraftSelect();
				CL_AircraftEquipmenuMenuInit_f();
				break;
			}
			list++;
		}
	}
	menuText[TEXT_STANDARD] = weaponDesc;
}

/**
 * @brief Asseigns the shield to current selected aircraft when clicked on the list
 */
void CL_AircraftEquipmenuMenuShieldsClick_f(void)
{
	aircraft_t *air;
	int num;
	static char shieldDesc[512];
	technology_t **list;

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which shield? */
	num = atoi(Cmd_Argv(1));

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	if ( num < 1 ) {
		Com_DPrintf("Reset the aircraft shield\n");
		air->shield = NULL;
		Com_sprintf(shieldDesc, sizeof(shieldDesc), _("No shield assigned"));
		CL_AircraftSelect();
	} else {
		list = RS_GetTechsByType(RS_CRAFTSHIELD);
		/* to prevent overflows we go through the list instead of address it directly */
		while (*list) {
			num--;
			/* found it */
			if (num <= 0) {
				air->shield = *list;
				Com_sprintf(shieldDesc, sizeof(shieldDesc), (*list)->name);
				CL_AircraftSelect();
				CL_AircraftEquipmenuMenuInit_f();
				break;
			}
			list++;
		}
	}
	menuText[TEXT_STANDARD] = shieldDesc;
}

/**
 * @brief Return an aircraft from its idx
 */
extern aircraft_t* CL_AircraftGetFromIdx(int idx) {
	base_t*		base;
	aircraft_t*	air;

	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		for (air = base->aircraft + base->numAircraftInBase - 1; air >= base->aircraft ; air--)
			if (air->idx == idx)
				return air;

	return NULL;
}

/**
 * @brief Sends the specified aircraft to specified mission
 */
extern qboolean CL_SendAircraftToMission(aircraft_t* aircraft, actMis_t* mission)
{
	if (!aircraft || !mission)
		return qfalse;

	if (! *(aircraft->teamSize)) {
		MN_Popup(_("Notice"), _("Assign a team to aircraft"));
		return qfalse;
	}

	MAP_MapCalcLine(aircraft->pos, mission->def->pos, &(aircraft->route));
	aircraft->status = AIR_MISSION;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->mission = mission;

	return qtrue;
}

static value_t aircraft_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(aircraft_t, name)}
	,
	{"speed", V_FLOAT, offsetof(aircraft_t, speed)}
	,
	{"size", V_INT, offsetof(aircraft_t, size)}
	,
	{"fuel", V_INT, offsetof(aircraft_t, fuel)}
	,
	{"fuelsize", V_INT, offsetof(aircraft_t, fuelSize)}
	,
	{"image", V_STRING, offsetof(aircraft_t, image)}
	,

	/* pointer to technology_t */
	{"weapon", V_STRING, offsetof(aircraft_t, weapon_string)}
	,
	{"shield", V_STRING, offsetof(aircraft_t, shield_string)}
	,

	{"model", V_STRING, offsetof(aircraft_t, model)}
	,
	/* price for selling/buying */
	{"price", V_INT, offsetof(aircraft_t, price)}
	,
	/* this is needed to let the buy and sell screen look for the needed building */
	/* to place the aircraft in */
	{"building", V_STRING, offsetof(aircraft_t, building)}
	,

	{NULL, 0, 0}
};

/**
  * @brief
  */
void CL_ParseAircraft(char *name, char **text)
{
	char *errhead = "CL_ParseAircraft: unexptected end of file (aircraft ";
	aircraft_t *ac;
	value_t *vp;
	char *token;

	if (numAircraft >= MAX_AIRCRAFT) {
		Com_Printf("CL_ParseAircraft: aircraft def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the menu */
	ac = &aircraft[numAircraft++];
	memset(ac, 0, sizeof(aircraft_t));

	Com_DPrintf("...found aircraft %s\n", name);
	ac->idx = numAircraft - 1;
	Q_strncpyz(ac->id, name, MAX_VAR);
	ac->status = AIR_HOME;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseAircraft: aircraft def \"%s\" without body ignored\n", name);
		numCampaigns--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = aircraft_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(ac, token, vp->type, vp->ofs);
				break;
			}

		if (vp->string && !Q_strncmp(vp->string, "size", 4)) {
			if (ac->size > MAX_ACTIVETEAM) {
				Com_DPrintf("Set size for aircraft to the max value of %i\n", MAX_ACTIVETEAM);
				ac->size = MAX_ACTIVETEAM;
			}
		}

		if (!Q_strncmp(token, "type", 4)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			if (!Q_strncmp(token, "transporter", 11))
				ac->type = AIRCRAFT_TRANSPORTER;
			else if (!Q_strncmp(token, "interceptor", 11))
				ac->type = AIRCRAFT_INTERCEPTOR;
			else if (!Q_strncmp(token, "ufo", 3))
				ac->type = AIRCRAFT_UFO;
		} else if (!vp->string) {
			Com_Printf("CL_ParseAircraft: unknown token \"%s\" ignored (aircraft %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @brief Notify that a mission has been removed
 * Aircraft currently moving to the mission will be redirect to base
 */
extern void CL_AircraftsNotifyMissionRemoved(const actMis_t* mission)
{
	base_t*		base;
	aircraft_t*	aircraft;
	
	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1 ;
		aircraft >= base->aircraft ; aircraft--)
			if (aircraft->status == AIR_MISSION) {
				if (aircraft->mission == mission)
					CL_AircraftReturnToBase(aircraft);					
				else if (aircraft->mission > mission)
					(aircraft->mission)--;
			}
}
