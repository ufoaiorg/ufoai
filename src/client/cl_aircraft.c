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
		break;
	case AIR_HOME:
		return _("At homebase");
		break;
	case AIR_REFUEL:
		return _("Refuel");
		break;
	case AIR_IDLE:
		return _("Idle");
		break;
	case AIR_TRANSIT:
		return _("On transit");
		break;
	case AIR_DROP:
		return _("Ready for drop down");
		break;
	case AIR_INTERCEPT:
		return _("On interception");
		break;
	case AIR_TRANSPORT:
		return _("Transportmission");
		break;
	case AIR_RETURNING:
		return _("Returning to homebase");
		break;
	default:
		Com_Printf("Error: Unknown aircraft status for %s\n", air->name);
		break;
	}
	return NULL;
}

/**
  * @brief
  */
void CL_NewAircraft_f(void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: newaircraft <type>\n");
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
void CL_AircraftReturnToBase(aircraft_t *air)
{
	base_t *base;

	if (air && air->status != AIR_HOME) {
		base = (base_t *) air->homebase;
		MN_MapCalcLine(air->pos, base->pos, &air->route);
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

	Com_Printf("Aircraft %s not found\n", name);
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
	actMis_t *mis;

	if (air->fuel <= 0 && air->status >= AIR_IDLE) {
		MN_AddNewMessage(_("Notice"), _("Your dropship has low fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
		CL_AircraftReturnToBase(air);
	}

	/* no base assigned */
	if (!air->homebase || !selMis)
		return;

	mis = selMis;
	if (abs(mis->def->pos[0] - air->pos[0]) < DISTANCE && abs(mis->def->pos[1] - air->pos[1]) < DISTANCE) {
		mis->def->active = qtrue;
		if (air->status != AIR_DROP && air->fuel > 0) {
			air->status = AIR_DROP;
			if (gd.interceptAircraft < 0)
				gd.interceptAircraft = air->idxInBase;
			MN_PushMenu("popup_intercept_ready");
		}
	} else {
		mis->def->active = qfalse;
	}
}

/**
  * @brief
  */
void CP_GetRandomPosForAircraft(float *pos)
{
	pos[0] = (rand() % 180) - (rand() % 180);
	pos[1] = (rand() % 90) - (rand() % 90);
	Com_DPrintf("CP_GetRandomPosForAircraft: (%.0f:%.0f)\n", pos[0], pos[1]);
}


/**
  * @brief
  *
  * TODO: Fuel
  */
void CL_CampaignRunAircraft(int dt)
{
	aircraft_t *air;
	float dist, frac;
	base_t *base;
	int i, p, j;
	vec2_t pos;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		for (i = 0, air = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, air++)
			if (air->homebase) {
				if (air->status > AIR_IDLE) {
					/* calc distance */
					air->time += dt;
					air->fuel -= dt;
					dist = air->speed * air->time / 3600;

					/* check for end point */
					if (dist >= air->route.dist * (air->route.n - 1)) {
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

					/* calc new position */
					frac = dist / air->route.dist;
					p = (int) frac;
					frac -= p;
					air->point = p;

					air->pos[0] = (1 - frac) * air->route.p[p][0] + frac * air->route.p[p + 1][0];
					air->pos[1] = (1 - frac) * air->route.p[p][1] + frac * air->route.p[p + 1][1];
				} else if (air->status == AIR_IDLE)
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

	/* now the ufos are flying around, too */
	for (j = 0; j < ccs.numUfoOnGeoscape; j++) {
		air = ufoOnGeoscape[j];
		air->time += dt;
		dist = air->speed * air->time / 3600;

		/* calc new position */
		frac = dist / air->route.dist;
		p = (int) frac;
		frac -= p;
		air->point = p;

		/* check for end point */
		if (air->point >= air->route.n) {
			CP_GetRandomPosForAircraft(pos);
			air->time = 0;
			air->point = 0;
			MN_MapCalcLine(air->pos, pos, &air->route);
			Com_DPrintf("New route for ufo %i (%.0f:%.0f) air->pos:(%.0f:%.0f)\n", j, pos[0], pos[1], air->pos[0], air->pos[1]);
			continue;
		}

		air->fuel -= dt;

		air->pos[0] = (1 - frac) * air->route.p[p][0] + frac * air->route.p[p + 1][0];
		air->pos[1] = (1 - frac) * air->route.p[p][1] + frac * air->route.p[p + 1][1];

		/* TODO: Crash */
		if (air->fuel <= 0)
			air->fuel = air->fuelSize;
	}
}

/**
 * @brief
 */
void CL_AircraftEquipmenuMenuInit_f(void)
{
	static char bufferShields[1024];
	static char bufferWeapons[1024];
	Com_sprintf(bufferShields, sizeof(bufferShields), _("None\n"));
	Com_sprintf(bufferWeapons, sizeof(bufferWeapons), _("None\n"));
	/* shields */
	menuText[TEXT_LIST] = bufferShields;
	/* weapons */
	menuText[TEXT_AIRCRAFT_LIST] = bufferWeapons;
	/* shield / weapon description */
	menuText[TEXT_STANDARD] = NULL;
}

/**
 * @brief
 */
void CL_AircraftEquipmenuMenuWeaponsClick_f(void)
{
	aircraft_t *air;
	int num;
	static char weaponDesc[512];

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which weapon? */
	num = atoi(Cmd_Argv(1));

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if ( num <= 1 ) {
		Com_DPrintf("Reset the aircraft weapon\n");
		air->weapon = NULL;
		Com_sprintf(weaponDesc, sizeof(weaponDesc), _("No weapon assigned"));
	} else {
		/*Com_sprintf(weaponDesc, sizeof(weaponDesc), _(""));*/
	}
	menuText[TEXT_STANDARD] = weaponDesc;
}

/**
 * @brief
 */
void CL_AircraftEquipmenuMenuShieldsClick_f(void)
{
	aircraft_t *air;
	int num;
	static char shieldDesc[512];

	if ( baseCurrent->aircraftCurrent < 0 )
		return;

	if (Cmd_Argc() < 2)
		return;

	/* which shield? */
	num = atoi(Cmd_Argv(1));

	air = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	if ( num <= 1 ) {
		Com_DPrintf("Reset the aircraft shield\n");
		air->shield = NULL;
		Com_sprintf(shieldDesc, sizeof(shieldDesc), _("No shield assigned"));
	} else {
		/*Com_sprintf(shieldDesc, sizeof(shieldDesc), _(""));*/
	}
	menuText[TEXT_STANDARD] = shieldDesc;
}

