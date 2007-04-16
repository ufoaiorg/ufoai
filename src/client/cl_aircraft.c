/**
 * @file cl_aircraft.c
 * @brief Most of the aircraft related stuff
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

aircraft_t aircraft_samples[MAX_AIRCRAFT]; /* available aircraft types */
int numAircraft_samples = 0; /* TODO: should be reset to 0 each time scripts are read anew; also aircraft_samples memory should be freed at that time, or old memory used for new records */
static int airequipID = -1;
static qboolean noparams = qfalse;
static int numAircraftItems = 0;
aircraftItem_t aircraftItems[MAX_AIRCRAFTITEMS];

#define AIRCRAFT_RADAR_RANGE	20

/* =========================================================== */
#define DISTANCE 1

/**
 * @brief Calculates the fight between aircraft and ufo
 * @param[in] aircraft the aircraft we attack with
 * @param[in] the ufo we attack
 * @return qtrue When aircraft hits ufo.
 * @return qfalse When ufo hits aircraft.
 * TODO : display an attack popup
 */
static qboolean AIR_Fight (aircraft_t* air, aircraft_t* ufo)
{
	/* variables here */

	/* some asserts */
	assert(air);
	assert(ufo);

	/* FIXME: */
	return qtrue;
}

/**
 * @brief Shows all aircraft in all bases on the game console (debug)
 * @note use the command with a parameter (the baseid) to show only one specific
 * base
 */
extern void CL_ListAircraft_f (void)
{
	int i, j, k, baseid = -1;
	base_t *base;
	aircraft_t *aircraft;
	character_t *chr;

	if (Cmd_Argc() == 2)
		baseid = atoi(Cmd_Argv(1));

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		if (baseid != -1 && baseid != base->idx)
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
			Com_Printf("...team: (%i/%i)\n", *aircraft->teamSize, aircraft->size);
			for (k = 0; k < aircraft->size; k++)
				if (aircraft->teamIdxs[k] != -1) {
					Com_Printf("......idx (in global array): %i\n", aircraft->teamIdxs[k]);
					chr = E_GetHiredCharacter(base, EMPL_SOLDIER, aircraft->teamIdxs[k]);
					if (chr)
						Com_Printf(".........name: %s\n", chr->name);
					else
						Com_Printf(".........ERROR: Could not get character for %i\n", k);
				}
		}
	}
}

/**
 * @brief Start an aircraft or stops the current mission and let the aircraft idle around
 */
void CL_AircraftStart_f (void)
{
	aircraft_t *aircraft;

	assert(baseCurrent);

	if (baseCurrent->aircraftCurrent < 0 || baseCurrent->aircraftCurrent >= baseCurrent->numAircraftInBase) {
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
	/* return to geoscape */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

/**
 * @brief Assigns the tech pointers, homebase and teamsize pointers to all aircraft
 */
void CL_AircraftInit (void)
{
	aircraft_t *air_samp;
	int i = 0;
	technology_t *tech = NULL;
	aircraftItem_t *aircraftitem = NULL;

	Com_Printf("Initializing aircraft and aircraft-items ...\n");

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

		if (*air_samp->item_string) {
			/* link with tech pointer */
			Com_DPrintf("....item: %s\n", air_samp->item_string);
			air_samp->item = RS_GetTechByID(air_samp->item_string);
		} else
			 air_samp->item = NULL;

		air_samp->homebase = &gd.bases[air_samp->idxBase]; /* TODO: looks like a nonsense */
		air_samp->teamSize = &gd.bases[air_samp->idxBase].teamNum[air_samp->idxInBase];
	}

	/* Link technologies for craftitems. */
	for (i = 0; i < numAircraftItems; i++) {
		aircraftitem = &aircraftItems[i];
		aircraftitem->tech_idx = -1; /* Default is -1 so it can be checked. */
		if (aircraftitem) {
			tech = RS_GetTechByID(aircraftitem->tech);
			if (tech)
				aircraftitem->tech_idx = tech->idx;
			else
				Com_Printf("CL_AircraftInit: No tech with the name '%s' found for craftitem '%s'.\n",  aircraftitem->tech, aircraftitem->id);
		}
	}

	Com_Printf("...aircraft and aircraft-items inited\n");
}

/**
 * @brief Translates the aircraft status id to a translateable string
 * @param[in] aircraft Aircraft to translate the status of
 */
char *CL_AircraftStatusToName (aircraft_t * aircraft)
{
	assert(aircraft);

	/* display special status if base-attack affects aircraft */
	if ( gd.bases[aircraft->idxBase].baseStatus == BASE_UNDER_ATTACK &&
		CL_IsAircraftInBase(aircraft) )
		return _("ON RED ALERT");

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
		return _("Pursuing a UFO");
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
 * @brief Returns true if aircraft is parked in its base
 */
qboolean CL_IsAircraftInBase (aircraft_t * aircraft)
{
	if (aircraft->status == AIR_HOME || aircraft->status == AIR_REFUEL)
		return qtrue;
	return qfalse;
}

/**
 * @brief Determines the state of the equip soldier menu button:
 * returns 1 if no aircraft in base else 2 if no soldiers
 * available otherwise 3
 */
int CL_EquipSoldierState (aircraft_t * aircraft)
{
	if (!CL_IsAircraftInBase(aircraft)) {
		return 1;
	} else {
		if (E_CountHired(baseCurrent, EMPL_SOLDIER) <= 0)
			return 2;
		else
			return 3;
	}
}

/**
 * @brief Calls CL_NewAircraft for given base with given aircraft type
 * @sa CL_NewAircraft
 */
extern void CL_NewAircraft_f (void)
{
	int i = -1;
	base_t *b = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: aircraft_new <type> <baseID>\n");
		return;
	}

	if (Cmd_Argc() == 3)
		i = atoi(Cmd_Argv(2));

	if (!baseCurrent || (i >= 0)) {
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
 * @brief Restores aircraft cvars after going back from aircraft buy menu.
 * @sa CL_MarketAircraftDescription()
 */
extern void CL_ResetAircraftCvars_f (void)
{
	Cvar_Set("mn_aircraftname", Cvar_VariableString("mn_aircraftname_before"));
	Cvar_Set("mn_aircraft_model", Cvar_VariableString("mn_aircraftmodel_before"));
}

/**
 * @brief Switch to next aircraft in base
 * @sa CL_AircraftSelect
 */
extern void MN_NextAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID < 0) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf("MN_NextAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		CL_AircraftSelect(NULL);
		return;
	}


	if (aircraftID < baseCurrent->numAircraftInBase - 1) {
		Cvar_SetValue("mn_aircraft_idx", aircraftID + 1);
		CL_AircraftSelect(NULL);
	} else {
		Com_DPrintf("MN_NextAircraft_f: we are at the end of the list already -> mn_aircraft_idx: %i - numAircraftInBase: %i\n", aircraftID, baseCurrent->numAircraftInBase);
	}
}

/**
 * @brief Switch to previous aircraft in base
 * @sa CL_AircraftSelect
 */
extern void MN_PrevAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID < 0) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf("MN_PrevAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		CL_AircraftSelect(NULL);
		return;
	}

	if (aircraftID >= 1) {
		Cvar_SetValue("mn_aircraft_idx", aircraftID - 1);
		CL_AircraftSelect(NULL);
	} else {
		Com_DPrintf("MN_PrevAircraft_f: we are at the beginning of the list already -> mn_aircraft_idx: %i - numAircraftInBase: %i\n", aircraftID, baseCurrent->numAircraftInBase);
	}
}

/**
 * @brief Returns the given aircraft back to homebase
 *
 * call this from baseview via "aircraft_return"
 * calculates the way back to homebase
 */
extern void CL_AircraftReturnToBase (aircraft_t *aircraft)
{
	base_t *base;

	if (aircraft && aircraft->status != AIR_HOME) {
		base = (base_t *) aircraft->homebase;
		Com_DPrintf("return '%s' (%i) to base ('%s')\n", aircraft->name, aircraft->idx, base->name);
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
 * @sa CL_GameAutoGo_f
 * @sa CL_GameResults_f
 */
extern void CL_AircraftReturnToBase_f (void)
{
	aircraft_t *aircraft;

	if (baseCurrent && (baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		CL_AircraftReturnToBase(aircraft);
		CL_AircraftSelect(aircraft);
	}
}

/**
 * @brief Sets aircraftCurrent and updates cvars
 * @note uses cvar mn_aircraft_idx to determine which aircraft to select (if aircraft
 * pointer is NULL)
 * @param[in] aircraft If this is NULL the cvar mn_aircraft_idx will be used
 * to determine the aircraft which should be displayed - if this will fail, too,
 * the first aircraft in the base is taken (if there is one)
 */
extern void CL_AircraftSelect (aircraft_t* aircraft)
{
	menuNode_t *node = NULL;
	int aircraftID = Cvar_VariableInteger("mn_aircraft_idx");
	static char aircraftInfo[256];

	/* calling from console? with no baseCurrent? */
	if (!baseCurrent || !baseCurrent->numAircraftInBase)
		return;

	if (!aircraft) {
		/**
		 * Selecting the first aircraft in base (every base has at least one
		 * aircraft at this point because baseCurrent->numAircraftInBase was zero)
		 * if a non-sane idx was found.
		 */
		if ((aircraftID < 0) || (aircraftID >= baseCurrent->numAircraftInBase)) {
			aircraftID = 0;
			Cvar_SetValue("mn_aircraft_idx", aircraftID);
		}
		aircraft = &baseCurrent->aircraft[aircraftID];
	} else {
		aircraftID = aircraft->idxInBase;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
	}

	node = MN_GetNodeFromCurrentMenu("aircraft");

	/* we were not in the aircraft menu yet */
	if (node) {
		/* copy the menu align values */
		VectorCopy(aircraft->scale, node->scale);
		VectorCopy(aircraft->center, node->center);
		VectorCopy(aircraft->angles, node->angles);
		rotateAngles = aircraft->angles;
	}

	baseCurrent->aircraftCurrent = aircraftID;

	CL_UpdateHireVar();

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_SetValue("mn_scientists_in_base", E_CountHired(baseCurrent, EMPL_SCIENTIST));
	Cvar_Set("mn_aircraftstatus", CL_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", CL_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", va("%s (%i/%i)", aircraft->name, (aircraftID + 1), baseCurrent->numAircraftInBase));
	Cvar_Set("mn_aircraft_model", aircraft->model);
	Cvar_Set("mn_aircraft_weapon", aircraft->weapon ? aircraft->weapon->name : "");
	Cvar_Set("mn_aircraft_shield", aircraft->shield ? aircraft->shield->name : "");
	Cvar_Set("mn_aircraft_weapon_img", aircraft->weapon ? aircraft->weapon->image_top : "menu/airequip_no_weapon");
	Cvar_Set("mn_aircraft_shield_img", aircraft->shield ? aircraft->shield->image_top : "menu/airequip_no_shield");
	Cvar_Set("mn_aircraft_item_img", aircraft->item ? aircraft->item->image_top : "menu/airequip_no_item");

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%.0f\n"), aircraft->speed);
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), aircraft->fuel / 1000, aircraft->fuelSize / 1000), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapon:\t%s\n"), aircraft->weapon ? _(aircraft->weapon->name) : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Shield:\t%s\n"), aircraft->shield ? _(aircraft->shield->name) : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Equipment:\t%s"), aircraft->item ? _(aircraft->item->name) : _("None")), sizeof(aircraftInfo));
	menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;
}

/**
 * @brief Console command binding
 */
extern void CL_AircraftSelect_f (void)
{
	/* calling from console? with no baseCurrent? */
	if (!baseCurrent || !baseCurrent->numAircraftInBase || !baseCurrent->hasHangar) {
		MN_PopMenu(qfalse);
		return;
	}

	baseCurrent->aircraftCurrent = -1;
	CL_AircraftSelect(NULL);
	if (baseCurrent->aircraftCurrent == -1)
		MN_PopMenu(qfalse);
}

/**
 * @brief Searches the global array of aircraft types for a given aircraft
 * @param[in] name Aircraft id
 * @return aircraft_t pointer or NULL if not found
 */
extern aircraft_t *CL_GetAircraft (const char *name)
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
 * @TODO: What about credits? maybe handle them in CL_NewAircraft_f?
 */
extern void CL_NewAircraft (base_t *base, const char *name)
{
	aircraft_t *aircraft;

	assert(base);

	/* First aircraft in base is default aircraft. */
	base->aircraftCurrent = 0;

	aircraft = CL_GetAircraft(name);
	if (aircraft && base->numAircraftInBase < MAX_AIRCRAFT) {
		/* copy from global aircraft list to base aircraft list */
		/* we do this because every aircraft can have its own parameters */
		memcpy(&base->aircraft[base->numAircraftInBase], aircraft, sizeof(aircraft_t));
		/* now lets use the aircraft array for the base to set some parameters */
		aircraft = &base->aircraft[base->numAircraftInBase];
		aircraft->idx = gd.numAircraft++;
		aircraft->homebase = base;
		/* for saving and loading a base */
		aircraft->idxBase = base->idx;
		/* this is the aircraft array id in current base */
		/* NOTE: when we send the aircraft to another base this has to be changed, too */
		aircraft->idxInBase = base->numAircraftInBase;
		/* link the teamSize pointer in */
		aircraft->teamSize = &base->teamNum[base->numAircraftInBase];
		CL_ResetAircraftTeam(aircraft);

		Q_strncpyz(messageBuffer, va(_("You've got a new aircraft (a %s) in base %s"), aircraft->name, base->name), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf("Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, aircraft->pos);
		Radar_Initialise(&(aircraft->radar), AIRCRAFT_RADAR_RANGE);

		base->numAircraftInBase++;
		Com_DPrintf("Adding new aircraft %s with IDX %i for base %s\n", aircraft->name, aircraft->idx, base->name);
		/* now update the aircraft list - maybe there is a popup active */
		Cbuf_ExecuteText(EXEC_NOW, "aircraft_list");
	}
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] base Pointer to aircraft that should be removed.
 * @note The assigned soldiers are removed from the aircraft ... not fired. If you wan them fired/deleted do it before calling this function.
*/
extern void CL_DeleteAircraft (aircraft_t *aircraft)
{
	int i = 0;
	base_t *base = NULL;
	aircraft_t *aircraft_temp = NULL;
	transferlist_t *transferlist_temp = NULL;
	int previous_aircraftCurrent = baseCurrent->aircraftCurrent;

	if (!aircraft) {
		Com_DPrintf("AIR_DeleteAircraft: no aircraft given (NULL)\n");
		/* TODO: Return deletion status here. */
		return;
	}
	/* Check if this aircraft is currently transferred. */
	if (aircraft->status == AIR_TRANSIT) {
		Com_DPrintf("CL_DeleteAircraft: this aircraft is currently transferred. We can not remove it.\n");
		/* TODO: Return deletion status here. */
		return;
	}

	base = &gd.bases[aircraft->idxBase];

	if (!base) {
		Com_DPrintf("AIR_DeleteAircraft: No homebase found for aircraft.\n");
		/* TODO: Return deletion status here. */
		return;
	}

	/* Remove all soldiers from the aircraft (the employees are still hired after this). */
	CL_RemoveSoldiersFromAircraft(aircraft->idx, aircraft->idxBase);

	for (i = aircraft->idx; i < gd.numAircraft-1; i++) {
		/* Decrease the global index of aircrafts that have a higher index than the deleted one. */
		aircraft_temp = CL_AircraftGetFromIdx(i);
		if (aircraft_temp) {
			aircraft_temp->idx--;
		} else {
			/* For some reason there was no aircraft found for this index. */
			Com_DPrintf("AIR_DeleteAircraft: No aircraft found for this global index: %i\n", i);
		}

		/* Update transfer list (i.e. remove the one for the deleted aircraft). */
		transferlist_temp = &gd.alltransfers[i];
		memcpy(transferlist_temp, &gd.alltransfers[i+1], sizeof(transferlist_t));
	}

	gd.numAircraft--;	/**< Decrease the global number of aircraft. */

	/* Finally remove the aircraft-struct itself from the base-array and update the order. */
	base->numAircraftInBase--;
	for (i = aircraft->idxInBase; i < base->numAircraftInBase; i++) {
		/* Remove aircraft and rearrange the aircraft-list (in base). */
		aircraft_temp = &base->aircraft[i];
		memcpy(aircraft_temp, &base->aircraft[i+1], sizeof(aircraft_t));
		aircraft_temp->idxInBase = i;	/**< Re-set the number of aircraft in the base. */

		/* Update number of team members for each aircraft.*/
		base->teamNum[i] = base->teamNum[i+1];

		/* Update index of aircraftCurrent in base if it is affected by the index-change. */
		if (i == previous_aircraftCurrent)
			baseCurrent->aircraftCurrent--;
	}

	if (base->numAircraftInBase < 1) {
		Cvar_SetValue("mn_equipsoldierstate", 0);
		Cvar_Set("mn_aircraftstatus", "");
		Cvar_Set("mn_aircraftinbase", "0");
		Cvar_Set("mn_aircraftname", "");
		Cvar_Set("mn_aircraft_model", "");
		baseCurrent->aircraftCurrent = -1;
	} 

	/* Q_strncpyz(messageBuffer, va(_("You've sold your aircraft (a %s) in base %s"), aircraft->name, base->name), MAX_MESSAGE_TEXT);
	MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);*/

	/* TODO: Return successful deletion status here. */
}

/**
 * @brief Set pos to a random position on geoscape
 * @param[in] pos Pointer to vec2_t for aircraft position
 * @note Used to place UFOs on geoscape
 */
extern void CP_GetRandomPosForAircraft (float *pos)
{
	pos[0] = (rand() % 180) - (rand() % 180);
	pos[1] = (rand() % 90) - (rand() % 90);
}

/**
 * @brief Move the specified aircraft
 * @param[in] dt
 * Return true if the aircraft reached its destination
 */
extern qboolean CL_AircraftMakeMove (int dt, aircraft_t* aircraft)
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
 * @param[in] dt
 * TODO: Fuel
 */
#define GROUND_MISSION 0.5
void CL_CampaignRunAircraft (int dt)
{
	aircraft_t *aircraft;
	base_t *base;
	int i, j;
	const char *zoneType = NULL;
	char missionName[MAX_VAR];
	byte *color;
	qboolean battleStatus = qfalse;

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
						Vector2Copy(end, aircraft->pos);

						switch (aircraft->status) {
						case AIR_MISSION:
							/* Aircraft reach its mission */
							aircraft->mission->def->active = qtrue;
							aircraft->status = AIR_DROP;
							/* set baseCurrent information so the
							 * correct team goes to the mission */
							baseCurrent = (base_t*)aircraft->homebase;
							assert(i == aircraft->idxInBase); /* Just in case the index is out of sync. */
							baseCurrent->aircraftCurrent = aircraft->idxInBase;
							MAP_SelectMission(aircraft->mission);
							gd.interceptAircraft = aircraft->idx;
							Com_DPrintf("gd.interceptAircraft: %i\n", gd.interceptAircraft);
							MN_PushMenu("popup_intercept_ready");
							break;
						case AIR_RETURNING:
							/* aircraft enter in  homebase */
							CL_DropshipReturned((base_t*)aircraft->homebase, aircraft);
							aircraft->status = AIR_REFUEL;
							break;
						case AIR_TRANSPORT:
							TR_TransferEnd(aircraft);
							break;
						default:
							aircraft->status = AIR_IDLE;
							break;
						}
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
				if (aircraft->fuel <= 0 && aircraft->status >= AIR_IDLE && aircraft->status != AIR_RETURNING) {
					MN_AddNewMessage(_("Notice"), _("Your dropship is low on fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
					CL_AircraftReturnToBase(aircraft);
				}

				/* Aircraft purchasing ufo */
				if (aircraft->status == AIR_UFO) {
					aircraft_t* ufo = NULL;
					mission_t* ms = NULL;

					ufo = gd.ufos + aircraft->ufo;
					if (abs(ufo->pos[0] - aircraft->pos[0]) < DISTANCE && abs(ufo->pos[1] - aircraft->pos[1]) < DISTANCE) {
						/* Display airfight sequence */
						Cbuf_ExecuteText(EXEC_NOW, "seq_start airfight;");

						/* The aircraft can attack the ufo */
						battleStatus = AIR_Fight(aircraft, ufo);

						if (battleStatus) {
							color = CL_GetMapColor(ufo->pos, MAPTYPE_CLIMAZONE);
							if (!MapIsWater(color) && frand() <= GROUND_MISSION) {
								/* spawn new mission */
								/* some random data like alien race, alien count (also depends on ufo-size) */
								/* TODO: We should have a ufo crash theme for random map assembly */
								/* TODO: call the map assembly theme with the right parameter, e.g.: +ufocrash [climazone] */
								zoneType = MAP_GetZoneType(color);
								Com_sprintf(missionName, sizeof(missionName), "ufocrash%.0f:%.0f", ufo->pos[0], ufo->pos[1]);
								ms = CL_AddMission(missionName);
								/* the size if somewhat random, because not all map tiles would have
								 * alien spawn points */
								ms->aliens = ufo->size;
								/* 1-4 civilians */
								ms->civilians = (frand() * 10);
								ms->civilians %= 4;
								ms->civilians += 1;

								/* FIXME: */
								Q_strncpyz(ms->alienTeam, "ortnok", sizeof(ms->alienTeam));
								Q_strncpyz(ms->civTeam, "european", sizeof(ms->civTeam));
							}
							/* now remove the ufo from geoscape */
							UFO_RemoveUfoFromGeoscape(ufo);
							/* and send our aircraft back to base */
							CL_AircraftReturnToBase(aircraft);
							MN_AddNewMessage(_("Interception"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
						} else {
							/* TODO: destroy the aircraft and all soldiers in it */
							/* TODO: maybe rescue some of the soldiers */
							/* FIXME: remove this */
							CL_AircraftReturnToBase(aircraft);

							MN_AddNewMessage(_("Interception"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
						}
					} else {
						/* TODO : Find better system to make the aircraft purchasing ufo */
						CL_SendAircraftPurchasingUfo(aircraft, ufo);
					}
				}
			}
	}
}

/**
 * @brief Returns a list of craftitem technologies for the given type.
 * @note this list is terminated by a NULL pointer
 * @param[in] type Type of the craft-items to return.
 * @param[in] usetypedef Defines if the type param should be handled as a aircraftItemType_t (qtrue) or not (qfalse - See the code).
 */
static technology_t **AC_GetCraftitemTechsByType (int type, qboolean usetypedef)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	aircraftItem_t *aircraftitem = NULL;
	int i, j = 0;

	for (i = 0; i < numAircraftItems; i++) {
		aircraftitem = &aircraftItems[i];
		if (usetypedef) {
			if (aircraftitem->type == type) {
				techList[j] = &gd.technologies[aircraftitem->tech_idx];
				j++;
			}
		} else {
			switch (type) {
			case 1: /* armour */
				if (aircraftitem->type == AC_ITEM_ARMOUR) {
					techList[j] = &gd.technologies[aircraftitem->tech_idx];
					j++;
				}
				break;
			case 2:	/* items */
				if (aircraftitem->type == AC_ITEM_ELECTRONICS)	{
					techList[j] = &gd.technologies[aircraftitem->tech_idx];
					j++;
				}
				break;
			default:
				if (aircraftitem->type == AC_ITEM_WEAPON) {
					techList[j] = &gd.technologies[aircraftitem->tech_idx];
					j++;
				}
				break;
			}
		}
		/* j+1 because last item have to be NULL */
		if (j + 1 >= MAX_TECHNOLOGIES) {
			Com_Printf("AC_GetCraftitemTechsByType: MAX_TECHNOLOGIES limit hit.\n");
			break;
		}
	}
	/* terminate the list */
	techList[j] = NULL;
	return techList;
}

/**
 * @brief Fills the weapon and shield list of the aircraft equip menu
 * @sa CL_AircraftEquipmenuMenuWeaponsClick_f
 * @sa CL_AircraftEquipmenuMenuShieldsClick_f
 */
void CL_AircraftEquipmenuMenuInit_f (void)
{
	static char buffer[1024];
	technology_t **list;
	int type;
	menuNode_t *node;
	aircraft_t *aircraft;

	if (Cmd_Argc() != 2 || noparams) {
		if (airequipID == -1) {
			Com_Printf("Usage: airequip_init <num>\n");
			return;
		} else {
			switch (airequipID) {
			case AC_ITEM_ARMOUR:
				/* armour */
				type = 1;
				break;
			case AC_ITEM_ELECTRONICS:
				/* items */
				type = 2;
				break;
			default:
				type = 0;
				break;
			}
		}
	} else {
		type = atoi(Cmd_Argv(1));
	}

	node = MN_GetNodeFromCurrentMenu("aircraftequip");

	/* we are not in the aircraft menu */
	if (!node) {
		Com_DPrintf("CL_AircraftEquipmenuMenuInit_f: Error - node aircraftequip not found\n");
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];

	assert(aircraft);
	assert(node);

	/* copy the menu align values */
	VectorCopy(aircraft->scaleEquip, node->scale);
	VectorCopy(aircraft->centerEquip, node->center);
	VectorCopy(aircraft->anglesEquip, node->angles);
	rotateAngles = aircraft->angles;

	Com_sprintf(buffer, sizeof(buffer), _("None\n"));

	switch (type) {
	case 1: /* armour */
		list = AC_GetCraftitemTechsByType(type, qfalse);
		airequipID = AC_ITEM_ARMOUR;
		break;
	case 2:	/* items */
		list = AC_GetCraftitemTechsByType(type, qfalse);
		airequipID = AC_ITEM_ELECTRONICS;
		break;
	default:
		list = AC_GetCraftitemTechsByType(type, qfalse);
		airequipID = AC_ITEM_WEAPON;
		break;
	}

	while (*list) {
		/*Com_Printf("%s\n", (*list)->id);*/
		if (RS_IsResearched_ptr(*list))
			Q_strcat(buffer, va("%s\n", _((*list)->name)), sizeof(buffer) );
		list++;
	}
	menuText[TEXT_LIST] = buffer;

	/* shield / weapon description */
	menuText[TEXT_STANDARD] = NULL;
	noparams = qfalse;
}

/**
 * @brief Assigns the weapon to current selected aircraft when clicked on the list
 * @sa CL_AircraftEquipmenuMenuInit_f
 */
void CL_AircraftEquipmenuMenuClick_f (void)
{
	aircraft_t *aircraft;
	int num;
	static char desc[512];
	technology_t **list;

	if (baseCurrent->aircraftCurrent < 0 || airequipID == -1)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: airequip_list_click <arg>\n");
		return;
	}

	/* Which entry in the list? */
	num = atoi(Cmd_Argv(1));

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if (num < 1) {
		switch (airequipID) {
		case AC_ITEM_WEAPON:
			Com_sprintf(desc, sizeof(desc), _("No weapon assigned"));
			aircraft->weapon = NULL;
			break;
		case AC_ITEM_ELECTRONICS:
			Com_sprintf(desc, sizeof(desc), _("No item assigned"));
			aircraft->item = NULL;
			break;
		case AC_ITEM_ARMOUR:
			Com_sprintf(desc, sizeof(desc), _("No shield assigned"));
			aircraft->shield = NULL;
			break;
		}
		CL_AircraftSelect(aircraft);
	} else {
		/* build the list of all aircraft items of type airequipID - null terminated */
		list = AC_GetCraftitemTechsByType(airequipID, qtrue);
		/* to prevent overflows we go through the list instead of address it directly */
		while (*list) {
			if (RS_IsResearched_ptr(*list))
				num--;
			/* found it */
			if (num <= 0) {
				switch (airequipID) {
				/* store the item string ids to be able to restore them after
				 * loading a savegame */
				case AC_ITEM_WEAPON:
					aircraft->weapon = *list;
					Q_strncpyz(aircraft->weapon_string, (*list)->id, MAX_VAR);
					break;
				case AC_ITEM_ELECTRONICS:
					aircraft->item = *list;
					Q_strncpyz(aircraft->item_string, (*list)->id, MAX_VAR);
					break;
				case AC_ITEM_ARMOUR:
					aircraft->shield = *list;
					Q_strncpyz(aircraft->shield_string, (*list)->id, MAX_VAR);
					break;
				default:
					Com_Printf("CL_AircraftEquipmenuMenuClick_f: Unknown airequipID: %i\n", airequipID);
				}
				Com_sprintf(desc, sizeof(desc), _((*list)->name));
				CL_AircraftSelect(aircraft);
				noparams = qtrue; /* used for CL_AircraftEquipmenuMenuInit_f */
				CL_AircraftEquipmenuMenuInit_f();
				break;
			}
			/* next item in the tech pointer list */
			list++;
		}
	}
	menuText[TEXT_STANDARD] = desc;
}

/**
 * @brief Return an aircraft from its global idx,
 * @param[in] idx Global aircraft index.
 */
extern aircraft_t* CL_AircraftGetFromIdx (int idx)
{
	base_t*		base;
	aircraft_t*	aircraft;

	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
			if (aircraft->idx == idx) {
				Com_DPrintf("CL_AircraftGetFromIdx: aircraft idx: %i - base idx: %i (%s)\n", aircraft->idx, base->idx, base->name);
				return aircraft;
			}

	return NULL;
}

/**
 * @brief Sends the specified aircraft to specified mission
 */
extern qboolean CL_SendAircraftToMission (aircraft_t* aircraft, actMis_t* mission)
{
	if (!aircraft || !mission)
		return qfalse;

	if (!*(aircraft->teamSize)) {
		MN_Popup(_("Notice"), _("Assign a team to aircraft"));
		return qfalse;
	}

	/* ensure interceptAircraft IDX is set correctly */
	gd.interceptAircraft = aircraft->idx;

	/* if mission is a base-attack and aircraft already in base, launch
	 * mission immediately */
	if (gd.bases[aircraft->idxBase].baseStatus == BASE_UNDER_ATTACK &&
		CL_IsAircraftInBase(aircraft)) {
		aircraft->mission = mission;
		mission->def->active = qtrue;
		MN_PushMenu("popup_baseattack");
		return qtrue;
	}

	MAP_MapCalcLine(aircraft->pos, mission->realPos, &(aircraft->route));
	aircraft->status = AIR_MISSION;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->mission = mission;

	return qtrue;
}

/** @brief valid aircraft items (craftitem) definition values from script files */
static const value_t aircraftitems_vals[] = {
	{"tech", V_TRANSLATION_STRING, offsetof(aircraftItem_t, tech)}
	,
	{"speed", V_FLOAT, offsetof(aircraftItem_t, speed)}
	,
	{"price", V_INT, offsetof(aircraftItem_t, price)}
	,
	{"shield", V_RELABS, offsetof(aircraftItem_t, shield)}
	,
	{"wrange", V_INT, offsetof(aircraftItem_t, weaponRange)}
	,
	{"range", V_RELABS, offsetof(aircraftItem_t, range)}
	,
	{"damage", V_INT, offsetof(aircraftItem_t, damage)}
	,
	{"accuracy", V_INT, offsetof(aircraftItem_t, accuracy)}
	,
	{"ecm", V_RELABS, offsetof(aircraftItem_t, ecm)}
	,
	{"weight", V_STRING, offsetof(aircraftItem_t, weight)}
	,
	{"weapon", V_STRING, offsetof(aircraftItem_t, weapon)}
	,

	{NULL, 0, 0}
};

/**
 * @brief Parses all aircraft items that are defined in our UFO-scripts
 * @sa CL_ParseClientData
 */
extern void CL_ParseAircraftItem (const char *name, char **text)
{
	const char *errhead = "CL_ParseAircraftItem: unexpected end of file (aircraft ";
	aircraftItem_t *airItem;
	const value_t *vp;
	char *token;

	if (numAircraftItems >= MAX_AIRCRAFTITEMS) {
		Com_Printf("CL_ParseAircraftItem: too many craftitem definitions; def \"%s\" ignored\n", name);
		return;
	}

	/* initialize the menu */
	airItem = &aircraftItems[numAircraftItems];
	memset(airItem, 0, sizeof(aircraftItem_t));

	Com_DPrintf("...found craftitem %s\n", name);
	airItem->idx = numAircraftItems;
	numAircraftItems++;
	Q_strncpyz(airItem->id, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseAircraftItem: craftitem def \"%s\" without body ignored\n", name);
		numAircraftItems--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Q_strncmp(token, "type", MAX_VAR)) {
			/* Craftitem type definition. */
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;

			/* Check which type it is and store the correct one.*/
			if (!Q_strncmp(token, "weapon", MAX_VAR))
				airItem->type = AC_ITEM_WEAPON;
			else if (!Q_strncmp(token, "ammo", MAX_VAR))
				airItem->type = AC_ITEM_WEAPON;	/* NOTE: Same as weapon right now. Might change int he future. */
			else if (!Q_strncmp(token, "armour", MAX_VAR))
				airItem->type = AC_ITEM_ARMOUR;
			else if (!Q_strncmp(token, "electronics", MAX_VAR))
				airItem->type = AC_ITEM_ELECTRONICS;
			else
				Com_Printf("CL_ParseAircraftItem: \"%s\" unknown craftitem type: \"%s\" - ignored.\n", name, token);
		} else {
			/* Check for some standard values. */
			for (vp = aircraftitems_vals; vp->string; vp++) {
				if (!Q_strcmp(token, vp->string)) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					Com_ParseValue(airItem, token, vp->type, vp->ofs);
					break;
				}
			}
			if (!vp->string) {
				Com_Printf("CL_ParseAircraftItem: unknown token \"%s\" ignored (craftitem %s)\n", token, name);
				COM_EParse(text, errhead, name);
			}
		}
	} while (*text);
}

/** @brief valid aircraft definition values from script files */
static const value_t aircraft_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(aircraft_t, name)}
	,
	{"shortname", V_TRANSLATION_STRING, offsetof(aircraft_t, shortname)}
	,
	{"speed", V_FLOAT, offsetof(aircraft_t, speed)}
	,
	{"size", V_INT, offsetof(aircraft_t, size)}
	,
	{"fuel", V_INT, offsetof(aircraft_t, fuel)}
	,
	{"fuelsize", V_INT, offsetof(aircraft_t, fuelSize)}
	,
	{"angles", V_VECTOR, offsetof(aircraft_t, angles)}
	,
	{"center", V_VECTOR, offsetof(aircraft_t, center)}
	,
	{"scale", V_VECTOR, offsetof(aircraft_t, scale)}
	,
	{"angles_equip", V_VECTOR, offsetof(aircraft_t, anglesEquip)}
	,
	{"center_equip", V_VECTOR, offsetof(aircraft_t, centerEquip)}
	,
	{"scale_equip", V_VECTOR, offsetof(aircraft_t, scaleEquip)}
	,
	{"image", V_STRING, offsetof(aircraft_t, image)}
	,

	/* pointer to technology_t */
	{"weapon", V_STRING, offsetof(aircraft_t, weapon_string)}
	,
	{"shield", V_STRING, offsetof(aircraft_t, shield_string)}
	,
	{"item", V_STRING, offsetof(aircraft_t, item_string)}
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
 * @brief Parses all aircraft that are defined in our UFO-scripts
 * @sa CL_ParseClientData
 * @note parses the aircraft into our aircraft_sample array to use as reference
 */
extern void CL_ParseAircraft (const char *name, char **text)
{
	const char *errhead = "CL_ParseAircraft: unexpected end of file (aircraft ";
	aircraft_t *air_samp;
	const value_t *vp;
	char *token;

	if (numAircraft_samples >= MAX_AIRCRAFT) {
		Com_Printf("CL_ParseAircraft: too many aircraft definitions; def \"%s\" ignored\n", name);
		return;
	}

	/* initialize the menu */
	air_samp = &aircraft_samples[numAircraft_samples];
	memset(air_samp, 0, sizeof(aircraft_t));

	Com_DPrintf("...found aircraft %s\n", name);
	air_samp->idx = gd.numAircraft;
	air_samp->idx_sample = numAircraft_samples;
	Q_strncpyz(air_samp->id, name, sizeof(air_samp->id));
	air_samp->status = AIR_HOME;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseAircraft: aircraft def \"%s\" without body ignored\n", name);
		return;
	}

	numAircraft_samples++;
	gd.numAircraft++;

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
extern void CL_AircraftsNotifyMissionRemoved (const actMis_t *const mission)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently moving to the mission will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--) {

			if (aircraft->status == AIR_MISSION) {
				if (aircraft->mission == mission) {
					CL_AircraftReturnToBase(aircraft);
				} else if (aircraft->mission > mission) {
					(aircraft->mission)--;
				}
			}
		}
	}
}

/**
 * @brief Notify that an ufo has been removed
 */
extern void CL_AircraftsNotifyUfoRemoved (const aircraft_t *const ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently purchasing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
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
extern void CL_AircraftsUfoDisappear (const aircraft_t *const ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently pursuing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO)
				if (ufo - gd.ufos == aircraft->ufo)
					CL_AircraftReturnToBase(aircraft);
}

/**
 * @brief Make the specified aircraft purchasing an ufo
 */
extern void CL_SendAircraftPurchasingUfo (aircraft_t* aircraft, aircraft_t* ufo)
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

/**
 * @brief
 * @param[in] aircraft
 */
void CL_ResetAircraftTeam (aircraft_t *aircraft)
{
	memset(aircraft->teamIdxs, -1, aircraft->size * sizeof(int));
}

/**
 * @brief
 * @param[in] aircraft
 * @param[in] employee_idx
 */
extern void CL_AddToAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;

	if (aircraft == NULL) {
		Com_DPrintf("CL_AddToAircraftTeam: null aircraft \n");
		return ;
	}
	if (*(aircraft->teamSize) < aircraft->size) {
		for (i = 0; i < aircraft->size; i++)
			if (aircraft->teamIdxs[i] == -1) {
				aircraft->teamIdxs[i] = employee_idx;
				Com_DPrintf("CL_AddToAircraftTeam: added idx '%d'\n", employee_idx);
				break;
			}
		if (i >= aircraft->size)
			Com_DPrintf("CL_AddToAircraftTeam: couldnt find space\n");
	} else {
		Com_DPrintf("CL_AddToAircraftTeam: error: no space in aircraft\n");
	}
}

/**
 * @brief
 * @param[in] aircraft
 * @param[in] employee_idx
 */
void CL_RemoveFromAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;

	assert(aircraft);

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] == employee_idx)	{
			aircraft->teamIdxs[i] = -1;
			Com_DPrintf("CL_RemoveFromAircraftTeam: removed idx '%d' \n", employee_idx);
			return;
		}
	Com_Printf("CL_RemoveFromAircraftTeam: error: idx '%d' not on aircraft %i (base: %i) in base %i\n", employee_idx, aircraft->idx, aircraft->idxInBase, baseCurrent->idx);
}

/**
 * @brief Called when an employee is going to be deleted - we have to modify the
 * teamdIdxs array for the aircraft - because with the deleting of an employee
 * these indices will change - so we are searching for every indices that is
 * greater the the one we have deleted and decrease the following indices by one
 * @param[in] aircraft change teamIdxs array in this aircraft
 * @param[in] employee_idx deleted employee idx
 * @sa E_DeleteEmployee
 */
extern void CL_DecreaseAircraftTeamIdxGreaterThan (aircraft_t *aircraft, int employee_idx)
{
	int i;

	if (aircraft == NULL)
		return ;

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] > employee_idx) {
			aircraft->teamIdxs[i]--;
			Com_DPrintf("CL_DecreaseAircraftTeamIdxGreaterThan: decreased idx '%d' \n", aircraft->teamIdxs[i]+1);
		}
}

/**
 * @brief
 * @param[in] aircraft
 * @param[in] employee_idx
 */
extern qboolean CL_IsInAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;
#if defined (DEBUG) || defined (PARANOID)
	base_t* base;
#endif

	if (aircraft == NULL) {
		Com_DPrintf("CL_IsInAircraftTeam: No aircraft given\n");
		return qfalse;
	}
#ifdef PARANOID
	else {
		base = aircraft->homebase;
		Com_DPrintf("CL_IsInAircraftTeam: aircraft: '%s' (base: '%s')\n", aircraft->name, base->name);
	}
#endif

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] == employee_idx) {
#ifdef DEBUG
			base = (base_t*)(aircraft->homebase);
			Com_DPrintf("CL_IsInAircraftTeam: found idx '%d' (homebase: '%s' - baseCurrent: '%s') \n", employee_idx, base ? base->name : "", baseCurrent ? baseCurrent->name : "");
#endif
			return qtrue;
		}
	Com_DPrintf("CL_IsInAircraftTeam:not found idx '%d' \n", employee_idx);
	return qfalse;
}


/**
 * @brief
 */
extern void CL_AircraftListDebug_f (void)
{
	base_t*		base;
	aircraft_t*	aircraft;

	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1; aircraft >= base->aircraft; aircraft--)
			Com_Printf("aircraft idx: %i (in base [idx]: %i) - base idx: %i (%s)\n", aircraft->idx, aircraft->idxInBase, base->idx, base->name);
}
