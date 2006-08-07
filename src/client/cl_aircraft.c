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

aircraft_t aircraft_samples[MAX_AIRCRAFT]; /* available aircraft types */
int numAircraft_samples = 0; /* TODO: should be reset to 0 each time scripts are read anew; also aircraft_samples memory should be freed at that time, or old memory used for new records */

#define AIRCRAFT_RADAR_RANGE	20

/* =========================================================== */
#define DISTANCE 1

/**
  * @brief
  */
void CL_ListAircraft_f(void)
{
	int i, j;
	base_t *base;
	aircraft_t *aircraft;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		Com_Printf("Aircrafts in base %s: %i\n", base->name, base->numAircraftInBase);
		for (i = 0; i < base->numAircraftInBase; i++) {
			aircraft = &base->aircraft[i];
			Com_Printf("Aircraft %s\n", aircraft->name);
			Com_Printf("...name %s\n", aircraft->id);
			Com_Printf("...speed %0.2f\n", aircraft->speed);
			Com_Printf("...type %i\n", aircraft->type);
			Com_Printf("...size %i\n", aircraft->size);
			Com_Printf("...status %s\n", CL_AircraftStatusToName(aircraft));
			Com_Printf("...pos %.0f:%.0f\n", aircraft->pos[0], aircraft->pos[1]);
		}
	}
}

/**
  * @brief Start an aircraft or stops the current mission and let the aircraft idle around
  */
void CL_AircraftStart_f(void)
{
	aircraft_t *aircraft;

	assert(baseCurrent);

	if (baseCurrent->aircraftCurrent < 0) {
		Com_DPrintf("Error - there is no aircraftCurrent in this base\n");
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if (aircraft->status < AIR_IDLE) {
		aircraft->pos[0] = baseCurrent->pos[0] + 2;
		aircraft->pos[1] = baseCurrent->pos[1] + 2;
	}
	MN_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
}

/**
  * @brief Assigns the tech pointers, homebase and teamsize pointers to all aircraft
  */
void CL_AircraftInit(void)
{
	aircraft_t *air_samp;
	int i = 0;

	for (i = 0; i < numAircraft_samples; i++) {
		air_samp = &aircraft_samples[i];
		/* link with tech pointer */
		Com_DPrintf("...aircraft: %s\n", air_samp->name);
		if (*air_samp->weapon_string) {
			Com_DPrintf("....weapon: %s\n", air_samp->weapon_string);
			air_samp->weapon = RS_GetTechByID(air_samp->weapon_string);
		} else
			air_samp->weapon = NULL;

		if (*air_samp->shield_string) {
			/* link with tech pointer */
			Com_DPrintf("....shield: %s\n", air_samp->shield_string);
			air_samp->shield = RS_GetTechByID(air_samp->shield_string);
		} else
			 air_samp->shield = NULL;
		air_samp->homebase = &gd.bases[air_samp->idxBase]; /* TODO: looks like a nonsense */
		air_samp->teamSize = &gd.bases[air_samp->idxBase].numOnTeam[air_samp->idxInBase];
	}

	Com_Printf("...aircraft inited\n");
}

/**
  * @brief Translates the aircraft status id to a translateable string
  * @param[in] aircraft Aircraft to translate the status of
  */
char *CL_AircraftStatusToName(aircraft_t * aircraft)
{
	assert(aircraft);
	switch (aircraft->status) {
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
	case AIR_UFO:
		return _("Purchasing an UFO");
	case AIR_DROP:
		return _("Ready for drop down");
	case AIR_INTERCEPT:
		return _("On interception");
	case AIR_TRANSPORT:
		return _("Transportmission");
	case AIR_RETURNING:
		return _("Back to base");
	default:
		Com_Printf("Error: Unknown aircraft status for %s\n", aircraft->name);
	}
	return NULL;
}

/**
  * @brief Calls CL_NewAircraft for given base with given aircraft type
  * @sa CL_NewAircraft
  */
void CL_NewAircraft_f(void)
{
	int i = -1;
	base_t *b = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: aircraft_new <type>\n");
		return;
	}

	if (!baseCurrent) {
		if (Cmd_Argc() == 3)
			i = atoi(Cmd_Argv(2));

		if (i < 0 || i > MAX_BASES)
			return;

		if (gd.bases[i].founded)
			b = &gd.bases[i];
	} else
		b = baseCurrent;

	if (b)
		CL_NewAircraft(b, Cmd_Argv(1));
}

/**
  * @brief Switch to next aircraft in base
  * @sa CL_AircraftSelect
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
  * @brief Switch to previous aircraft in base
  * @sa CL_AircraftSelect
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
extern void CL_AircraftReturnToBase(aircraft_t *aircraft)
{
	base_t *base;

	if (aircraft && aircraft->status != AIR_HOME) {
		base = (base_t *) aircraft->homebase;
		MAP_MapCalcLine(aircraft->pos, base->pos, &aircraft->route);
		aircraft->status = AIR_RETURNING;
		aircraft->time = 0;
		aircraft->point = 0;
		aircraft->mission = NULL;
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
	aircraft_t *aircraft;

	if (baseCurrent && baseCurrent->aircraftCurrent >= 0) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		CL_AircraftReturnToBase(aircraft);
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
	aircraft_t *aircraft;
	int aircraftID = (int) Cvar_VariableValue("mn_aircraft_id");
	static char aircraftInfo[256];

	/* calling from console? with no baseCurrent? */
	if (!baseCurrent)
		return;

	/* selecting the first aircraft in base (every base has at least one aircraft) */
	if (aircraftID >= baseCurrent->numAircraftInBase || aircraftID < 0)
		aircraftID = 0;

	aircraft = &baseCurrent->aircraft[aircraftID];

	baseCurrent->aircraftCurrent = aircraftID;

	CL_UpdateHireVar();

	Cvar_Set("mn_aircraftstatus", CL_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftname", aircraft->name);
	Cvar_Set("mn_aircraft_model", aircraft->model);
	Cvar_Set("mn_aircraft_weapon", aircraft->weapon ? aircraft->weapon->name : "");
	Cvar_Set("mn_aircraft_shield", aircraft->shield ? aircraft->shield->name : "");
	Cvar_Set("mn_aircraft_weapon_img", aircraft->weapon ? aircraft->weapon->image_top : "menu/airequip_no_weapon");
	Cvar_Set("mn_aircraft_shield_img", aircraft->shield ? aircraft->shield->image_top : "menu/airequip_no_shield");

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%.0f\n"), aircraft->speed);
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), aircraft->fuel / 1000, aircraft->fuelSize / 1000), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapon:\t%s\n"), aircraft->weapon ? aircraft->weapon->name : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Shield:\t%s"), aircraft->shield ? aircraft->shield->name : _("None")), sizeof(aircraftInfo));
	menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;
}

/**
  * @brief Searches the global array of aircraft types for a given aircraft
  * @param[in] name Aircraft id
  * @return aircraft_t pointer or NULL if not found
  */
aircraft_t *CL_GetAircraft(char *name)
{
	int i;

	for (i = 0; i < numAircraft_samples; i++) {
		if (!Q_strncmp(aircraft_samples[i].id, name, MAX_VAR))
			return &aircraft_samples[i];
	}

	Com_Printf("Aircraft %s not found (%i)\n", name, numAircraft_samples);
	return NULL;
}

/**
  * @brief Places a new aircraft in the given base
  * @param[in] base Pointer to base where aircraft should be added
  * @param[in] name Name of the aircraft to add
  */
void CL_NewAircraft(base_t *base, char *name)
{
	aircraft_t *aircraft;

	assert(base);

	/* first aircraft is default aircraft */
	base->aircraftCurrent = 0;

	aircraft = CL_GetAircraft(name);
	if (aircraft) {
		/* copy from global aircraft list to base aircraft list */
		/* we do this because every aircraft can have its own parameters */
		memcpy(&base->aircraft[base->numAircraftInBase], aircraft, sizeof(aircraft_t));
		/* now lets use the aircraft array for the base to set some parameters */
		aircraft->idx = gd.numAircraft++;
		aircraft = &base->aircraft[base->numAircraftInBase];
		aircraft->homebase = base;
		/* for saving and loading a base */
		aircraft->idxBase = base->idx;
		/* this is the aircraft array id in current base */
		/* NOTE: when we send the aircraft to another base this has to be changed, too */
		aircraft->idxInBase = base->numAircraftInBase;
		/* link the teamSize pointer in */
		/* NOTE: when we load a savegame, this has to be updated, too */
		aircraft->teamSize = &base->numOnTeam[base->numAircraftInBase];
		Q_strncpyz(messageBuffer, va(_("You've got a new aircraft (a %s) in base %s"), aircraft->name, base->name), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf("Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, aircraft->pos);
		Radar_Initialise(&(aircraft->radar), AIRCRAFT_RADAR_RANGE);

		base->numAircraftInBase++;
		Com_DPrintf("Aircraft for base %s: %s\n", base->name, aircraft->name);
		/* now update the aircraft list - maybe there is a popup active */
		Cbuf_ExecuteText(EXEC_NOW, "aircraft_list");
	}
}

/**
  * @brief Set pos to a random position on geoscape
  * @param[in] pos Pointer to vec2_t for aircraft position
  * @note Used to place UFOs on geoscape
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
extern qboolean CL_AircraftMakeMove(int dt, aircraft_t* aircraft)
{
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
#define GROUND_MISSION 0.5
void CL_CampaignRunAircraft(int dt)
{
	aircraft_t *aircraft;
	base_t *base;
	int i, j;
	byte *color;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		/* Run each aircraft */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
			if (aircraft->homebase) {
				if (aircraft->status > AIR_IDLE) {
					/* Aircraft is moving */
					if (CL_AircraftMakeMove(dt, aircraft)) {
						/* aircraft reach its destination */
						float *end;

						end = aircraft->route.p[aircraft->route.n - 1];
						aircraft->pos[0] = end[0];
						aircraft->pos[1] = end[1];

						if (aircraft->status == AIR_MISSION) {
							/* Aircraft reach its mission */
							aircraft->mission->def->active = qtrue;
							aircraft->status = AIR_DROP;
							MAP_SelectMission(aircraft->mission);
							gd.interceptAircraft = aircraft->idx;
							Com_DPrintf("gd.interceptAircraft: %i\n", gd.interceptAircraft);
							MN_PushMenu("popup_intercept_ready");
						} else if (aircraft->status == AIR_RETURNING) {
							/* aircraft enter in  homebase */
							aircraft->status = AIR_REFUEL;
						} else
							aircraft->status = AIR_IDLE;
					}
				} else if (aircraft->status == AIR_IDLE) {
					/* Aircraft idle out of base */
					aircraft->fuel -= dt;
				} else if (aircraft->status == AIR_REFUEL) {
					/* Aircraft is refluing at base */
					aircraft->fuel += dt;
					if (aircraft->fuel >= aircraft->fuelSize) {
						aircraft->fuel = aircraft->fuelSize;
						aircraft->status = AIR_HOME;
					}
				}

				/* Check aircraft low fuel */
				if (aircraft->fuel <= 0 && aircraft->status >= AIR_IDLE) {
					MN_AddNewMessage(_("Notice"), _("Your dropship has low fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
					CL_AircraftReturnToBase(aircraft);
				}

				/* Aircraft purchasing ufo */
				if (aircraft->status == AIR_UFO) {
					aircraft_t* ufo;

					ufo = gd.ufos + aircraft->ufo;
					if (abs(ufo->pos[0] - aircraft->pos[0]) < DISTANCE && abs(ufo->pos[1] - aircraft->pos[1]) < DISTANCE) {
						/* The aircraft can attack the ufo */
						color = CL_GetmapColor(ufo->pos);
						if (MapIsWater(color)) {
							/* ufo/aircraft crashes to water */
						} else if (frand() <= GROUND_MISSION ) {
							/* spawn new mission */
						}
						Com_Printf("Aircraft touch UFO, back to base\n");
						/* TODO : display an attack popup */
						CL_AircraftReturnToBase(aircraft);
					} else {
						/* TODO : Find better system to make the aircraft purchasing ufo */
						CL_SendAircraftPurchasingUfo(aircraft, ufo);
					}
				}
			}
	}
}

/**
 * @brief Fills the weapon and shield list of the aircraft equip menu
 * @sa CL_AircraftEquipmenuMenuWeaponsClick_f
 * @sa CL_AircraftEquipmenuMenuShieldsClick_f
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
		if (RS_IsResearched_ptr(*list))
			Q_strcat(bufferShields, va("%s\n", (*list)->name), sizeof(bufferShields) );
		list++;
	}
	menuText[TEXT_LIST] = bufferShields;

	/* weapons */
	Com_sprintf(bufferWeapons, sizeof(bufferWeapons), _("None\n"));
	list = RS_GetTechsByType(RS_CRAFTWEAPON);
	while (*list) {
		/*Com_Printf("%s\n", (*list)->id);*/
		if (RS_IsResearched_ptr(*list))
			Q_strcat(bufferWeapons, va("%s\n", (*list)->name), sizeof(bufferWeapons) );
		list++;
	}
	menuText[TEXT_AIRCRAFT_LIST] = bufferWeapons;

	/* shield / weapon description */
	menuText[TEXT_STANDARD] = NULL;
}

/**
 * @brief Assigns the weapon to current selected aircraft when clicked on the list
 * @sa CL_AircraftEquipmenuMenuInit_f
 */
void CL_AircraftEquipmenuMenuWeaponsClick_f(void)
{
	aircraft_t *aircraft;
	int num;
	static char weaponDesc[512];
	technology_t **list;

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which weapon? */
	num = atoi(Cmd_Argv(1));

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if ( num < 1 ) {
		Com_DPrintf("Reset the aircraft weapon\n");
		aircraft->weapon = NULL;
		Com_sprintf(weaponDesc, sizeof(weaponDesc), _("No weapon assigned"));
		CL_AircraftSelect();
	} else {
		list = RS_GetTechsByType(RS_CRAFTWEAPON);
		/* to prevent overflows we go through the list instead of address it directly */
		while (*list) {
			if (RS_IsResearched_ptr(*list))
				num--;
			/* found it */
			if (num <= 0) {
				aircraft->weapon = *list;
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
 * @sa CL_AircraftEquipmenuMenuInit_f
 */
void CL_AircraftEquipmenuMenuShieldsClick_f(void)
{
	aircraft_t *aircraft;
	int num;
	static char shieldDesc[512];
	technology_t **list;

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which shield? */
	num = atoi(Cmd_Argv(1));

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	if ( num < 1 ) {
		Com_DPrintf("Reset the aircraft shield\n");
		aircraft->shield = NULL;
		Com_sprintf(shieldDesc, sizeof(shieldDesc), _("No shield assigned"));
		CL_AircraftSelect();
	} else {
		list = RS_GetTechsByType(RS_CRAFTSHIELD);
		/* to prevent overflows we go through the list instead of address it directly */
		while (*list) {
			if (RS_IsResearched_ptr(*list))
				num--;
			/* found it */
			if (num <= 0) {
				aircraft->shield = *list;
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
extern aircraft_t* CL_AircraftGetFromIdx(int idx)
{
	base_t*		base;
	aircraft_t*	aircraft;

	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft ; aircraft--)
			if (aircraft->idx == idx)
				return aircraft;

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
	aircraft_t *air_samp;
	value_t *vp;
	char *token;

	if (numAircraft_samples >= MAX_AIRCRAFT) {
		Com_Printf("CL_ParseAircraft: too many aircraft definitions; def \"%s\" ignored\n", name);
		return;
	}

	/* initialize the menu */
	air_samp = &aircraft_samples[numAircraft_samples++];
	memset(air_samp, 0, sizeof(aircraft_t));

	Com_DPrintf("...found aircraft %s\n", name);
	air_samp->idx = gd.numAircraft++;
	air_samp->idx_sample = numAircraft_samples - 1;
	Q_strncpyz(air_samp->id, name, MAX_VAR);
	air_samp->status = AIR_HOME;

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

				Com_ParseValue(air_samp, token, vp->type, vp->ofs);
				break;
			}

		if (vp->string && !Q_strncmp(vp->string, "size", 4)) {
			if (air_samp->size > MAX_ACTIVETEAM) {
				Com_DPrintf("Set size for aircraft to the max value of %i\n", MAX_ACTIVETEAM);
				air_samp->size = MAX_ACTIVETEAM;
			}
		}

		if (!Q_strncmp(token, "type", 4)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			if (!Q_strncmp(token, "transporter", 11))
				air_samp->type = AIRCRAFT_TRANSPORTER;
			else if (!Q_strncmp(token, "interceptor", 11))
				air_samp->type = AIRCRAFT_INTERCEPTOR;
			else if (!Q_strncmp(token, "ufo", 3))
				air_samp->type = AIRCRAFT_UFO;
		} else if (!vp->string) {
			Com_Printf("CL_ParseAircraft: unknown token \"%s\" ignored (aircraft %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @brief Notify that a mission has been removed
 */
extern void CL_AircraftsNotifyMissionRemoved(const actMis_t* mission)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently moving to the mission will be redirect to base */
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

/**
 * @brief Notify that an ufo has been removed
 */
extern void CL_AircraftsNotifyUfoRemoved(const aircraft_t* ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently purchasing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1 ;
		aircraft >= base->aircraft ; aircraft--)
			if (aircraft->status == AIR_UFO) {
				if (ufo - gd.ufos == aircraft->ufo)
					CL_AircraftReturnToBase(aircraft);
				else if (ufo - gd.ufos < aircraft->ufo)
					aircraft->ufo--;
			}
}

/**
 * @brief Notify that an ufo disappear from radars
 */
extern void CL_AircraftsUfoDisappear(const aircraft_t* ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently purchasing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1 ; base >= gd.bases ; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1 ;
		aircraft >= base->aircraft ; aircraft--)
			if (aircraft->status == AIR_UFO)
				if (ufo - gd.ufos == aircraft->ufo)
					CL_AircraftReturnToBase(aircraft);
}

/**
 * @brief Make the specified aircraft purchasing an ufo
 */
extern void CL_SendAircraftPurchasingUfo(aircraft_t* aircraft,aircraft_t* ufo)
{
	int num = ufo - gd.ufos;

	if (num < 0 || num >= gd.numUfos || ! aircraft || ! ufo)
		return;

	MAP_MapCalcLine(aircraft->pos, ufo->pos, &(aircraft->route));
	aircraft->status = AIR_UFO;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->ufo = num;
}
