/**
 * @file cl_aircraft.c
 * @brief Most of the aircraft related stuff.
 * @note Aircraft management functions prefix: AIR_
 * @note Aircraft menu(s) functions prefix: AIM_
 * @note Aircraft equipement handling functions prefix: AII_
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

aircraft_t aircraft_samples[MAX_AIRCRAFT];		/**< Available aircraft types. */
int numAircraft_samples = 0; /* @todo: should be reset to 0 each time scripts are read anew; also aircraft_samples memory should be freed at that time, or old memory used for new records */
static int airequipID = -1;				/**< FIXME: document me. */
static qboolean noparams = qfalse;			/**< FIXME: document me. */
static int numAircraftItems = 0;			/**< FIXME: document me. */
static aircraftItem_t aircraftItems[MAX_AIRCRAFTITEMS];	/**< FIXME: document me. */

#define AIRCRAFT_RADAR_RANGE	20			/* FIXME: const */

/* =========================================================== */
#define DISTANCE 15					/* FIXME: const */

/**
 * @brief Updates base capacities after buying new aircraft.
 * @param[in] aircraftID aircraftID Index of aircraft type in aircraft_samples.
 * @param[in] base_idx Index of base in global array.
 * @sa AIR_NewAircraft
 * @sa AIR_UpdateHangarCapForAll
 */
static void AIR_UpdateHangarCapForOne (int aircraftID, int base_idx)
{
	int aircraftSize = 0, freespace = 0;
	base_t *base = NULL;

	aircraftSize = aircraft_samples[aircraftID].weight;
	base = &gd.bases[base_idx];

	if (aircraftSize < 1) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne()... aircraft weight is wrong!\n");
#endif
		return;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne()... base does not exist!\n");
#endif
		return;
	}
	assert (base);

	freespace = base->capacities[CAP_AIRCRAFTS_SMALL].max - base->capacities[CAP_AIRCRAFTS_SMALL].cur;
	Com_DPrintf("AIR_UpdateHangarCapForOne()... freespace: %i aircraft weight: %i\n", freespace, aircraftSize);
	/* If the aircraft size is less than 8, we will try to update CAP_AIRCRAFTS_SMALL. */
	if (aircraftSize < 8) {
		if (freespace >= aircraftSize) {
			base->capacities[CAP_AIRCRAFTS_SMALL].cur += aircraftSize;
		} else {
			/* Not enough space in small hangar. Aircraft will go to big hangar. */
			freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur;
			Com_DPrintf("AIR_UpdateHangarCapForOne()... freespace: %i aircraft weight: %i\n", freespace, aircraftSize);
			if (freespace >= aircraftSize) {
				base->capacities[CAP_AIRCRAFTS_BIG].cur += aircraftSize;
			} else {
				/* No free space for this aircraft. This should never happen here. */
				Com_Printf("AIR_UpdateHangarCapForOne()... no free space!\n");
			}
		}
	} else {
		/* The aircraft is too big for small hangar. Update big hangar capacities. */
		freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur;
		Com_DPrintf("AIR_UpdateHangarCapForOne()... freespace: %i aircraft weight: %i\n", freespace, aircraftSize);
		if (freespace >= aircraftSize) {
			base->capacities[CAP_AIRCRAFTS_BIG].cur += aircraftSize;
		} else {
			/* No free space for this aircraft. This should never happen here. */
			Com_Printf("AIR_UpdateHangarCapForOne()... no free space!\n");
		}
	}
	/* @todo: introduce capacities for UFO hangars and do space checks for them here. */
	Com_DPrintf("AIR_UpdateHangarCapForOne()... base capacities.cur: small: %i big: %i\n", base->capacities[CAP_AIRCRAFTS_SMALL].cur, base->capacities[CAP_AIRCRAFTS_BIG].cur);
}

/**
 * @brief Updates current capacities for hangars in given base.
 * @param[in] base_idx Index of base in global array.
 * @note Call this function whenever you sell/loss aircraft in given base.
 * @todo Remember to call this function when you lost aircraft in air fight.
 * @sa BS_SellAircraft_f
 */
void AIR_UpdateHangarCapForAll (int base_idx)
{
	int i;
	base_t *base = NULL;
	aircraft_t *aircraft = NULL;

	base = &gd.bases[base_idx];

	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForAll()... base does not exist!\n");
#endif
		return;
	}
	assert (base);
	/* Reset current capacities for hangar. */
	base->capacities[CAP_AIRCRAFTS_BIG].cur = 0;
	base->capacities[CAP_AIRCRAFTS_SMALL].cur = 0;

	for (i = 0; i < base->numAircraftInBase; i++) {
		aircraft = &base->aircraft[i];
		Com_DPrintf("AIR_UpdateHangarCapForAll()... base: %s, aircraft: %s\n", base->name, aircraft->id);
		AIR_UpdateHangarCapForOne(aircraft->idx_sample, base->idx);
	}
	Com_DPrintf("AIR_UpdateHangarCapForAll()... base capacities.cur: small: %i big: %i\n", base->capacities[CAP_AIRCRAFTS_SMALL].cur, base->capacities[CAP_AIRCRAFTS_BIG].cur);
}

#ifdef DEBUG
/**
 * @brief Debug function which lists all aircrafts in all bases.
 * @note Use with baseID as a parameter to display aircrafts in given base.
 */
extern void AIR_ListAircraft_f (void)
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
			Com_Printf("...idx cur=base/global %i=%i/%i\n", i, aircraft->idxInBase, aircraft->idx);
			Com_Printf("...name %s\n", aircraft->id);
			Com_Printf("...speed %0.2f\n", aircraft->speed);
			Com_Printf("...type %i\n", aircraft->type);
			Com_Printf("...size %i\n", aircraft->size);
			Com_Printf("...status %s\n", AIR_AircraftStatusToName(aircraft));
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
#endif

/**
 * @brief Starts an aircraft or stops the current mission and let the aircraft idle around.
 */
void AIM_AircraftStart_f (void)
{
	aircraft_t *aircraft;

	assert(baseCurrent);

	if (baseCurrent->aircraftCurrent < 0 || baseCurrent->aircraftCurrent >= baseCurrent->numAircraftInBase) {
#ifdef DEBUG
		Com_Printf("Error - there is no aircraftCurrent in this base\n");
#endif
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if (aircraft->status < AIR_IDLE) {
		aircraft->pos[0] = baseCurrent->pos[0] + 2;
		aircraft->pos[1] = baseCurrent->pos[1] + 2;
		HOS_RemoveEmployeesInHospital(aircraft);
	}
	MN_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
	/* Return to geoscape. */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

/**
 * @brief Assigns the tech pointers, homebase and teamsize pointers to all aircrafts.
 */
void AIR_AircraftInit (void)
{
	aircraft_t *air_samp;
	int i = 0;
	technology_t *tech = NULL;
	aircraftItem_t *aircraftitem = NULL;

	Com_Printf("Initializing aircrafts and aircraft-items ...\n");

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

		air_samp->homebase = &gd.bases[air_samp->idxBase]; /* @todo: looks like a nonsense */
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
				Com_Printf("AIR_AircraftInit: No tech with the name '%s' found for craftitem '%s'.\n",  aircraftitem->tech, aircraftitem->id);
		}
	}

	Com_Printf("...aircraft and aircraft-items inited\n");
}

/**
 * @brief Translates the aircraft status id to a translateable string
 * @param[in] aircraft Aircraft to translate the status of
 * @return Translation string of given status.
 * @note Called in: CL_AircraftList_f(), AIR_ListAircraft_f(), AIR_AircraftSelect(),
 * @note MAP_DrawMap(), CL_DisplayPopupIntercept()
 */
char *AIR_AircraftStatusToName (aircraft_t * aircraft)
{
	assert(aircraft);

	/* display special status if base-attack affects aircraft */
	if ( gd.bases[aircraft->idxBase].baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft) )
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
 * @brief Checks whether given aircraft is in its homebase.
 * @param[in] *aircraft Pointer to an aircraft.
 * @return qtrue if given aircraft is in its homebase.
 * @return qfalse if given aircraft is not in its homebase.
 * @todo Add check for AIR_REARM when aircraft items will be implemented.
 */
qboolean AIR_IsAircraftInBase (aircraft_t * aircraft)
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
	if (!AIR_IsAircraftInBase(aircraft)) {
		return 1;
	} else {
		if (E_CountHired(baseCurrent, EMPL_SOLDIER) <= 0)
			return 2;
		else
			return 3;
	}
}

/**
 * @brief Calls AIR_NewAircraft for given base with given aircraft type.
 * @sa AIR_NewAircraft
 */
extern void AIR_NewAircraft_f (void)
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
		AIR_NewAircraft(b, Cmd_Argv(1));
}

/**
 * @brief Restores aircraft cvars after going back from aircraft buy menu.
 * @sa BS_MarketAircraftDescription()
 */
extern void AIM_ResetAircraftCvars_f (void)
{
	Cvar_Set("mn_aircraftname", Cvar_VariableString("mn_aircraftname_before"));
}

/**
 * @brief Switch to next aircraft in base.
 * @sa AIR_AircraftSelect
 */
extern void AIM_NextAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID < 0) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf("AIM_NextAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		AIR_AircraftSelect(NULL);
		return;
	}


	if (aircraftID < baseCurrent->numAircraftInBase - 1) {
		Cvar_SetValue("mn_aircraft_idx", aircraftID + 1);
		AIR_AircraftSelect(NULL);
	} else {
		Com_DPrintf("AIM_NextAircraft_f: we are at the end of the list already -> mn_aircraft_idx: %i - numAircraftInBase: %i\n", aircraftID, baseCurrent->numAircraftInBase);
	}
}

/**
 * @brief Switch to previous aircraft in base.
 * @sa AIR_AircraftSelect
 */
extern void AIM_PrevAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID < 0) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf("AIM_PrevAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		AIR_AircraftSelect(NULL);
		return;
	}

	if (aircraftID >= 1) {
		Cvar_SetValue("mn_aircraft_idx", aircraftID - 1);
		AIR_AircraftSelect(NULL);
	} else {
		Com_DPrintf("AIM_PrevAircraft_f: we are at the beginning of the list already -> mn_aircraft_idx: %i - numAircraftInBase: %i\n", aircraftID, baseCurrent->numAircraftInBase);
	}
}

/**
 * @brief check if aircraft has enough fuel to go to destination, and then come back home
 * @param[in] *aircraft Pointer to the aircraft
 * @param[in] *position Pointer to the position the aircraft should go to
 * @sa MAP_MapCalcLine
 * @return qtrue if the aircraft can go to the position, qfalse else
 */
extern qboolean AIR_AircraftHasEnoughFuel (aircraft_t *aircraft, const vec2_t destination)
{
	base_t *base;
	float distance = 0;
	mapline_t line;

	assert(aircraft);
	base = (base_t *) aircraft->homebase;
	assert(base);

	/* Calculate the line that the aircraft should follow to go to destination */
	MAP_MapCalcLine(aircraft->pos, destination, &(line));
	distance = line.distance * (line.numPoints - 1);

	/* Calculate the line that the aircraft should then follow to go back home */
	MAP_MapCalcLine(destination, base->pos, &(line));
	distance += line.distance * (line.numPoints - 1);

	/* Check if the aircraft has enough fuel to go to destination and then go back home */
	if (distance <= aircraft->speed * aircraft->fuel / 3600)
		return qtrue;
	else {
		/* @todo Should check if another base is closer than homeBase and have a hangar */
		MN_AddNewMessage(_("Notice"), _("Your aircraft doesn't have enough fuel to go there and then come back to its home base"), qfalse, MSG_STANDARD, NULL);
	}

	return qfalse;
}

/**
 * @brief Calculates the way back to homebase for given aircraft and returns it.
 * @param[in] *aircraft Pointer to aircraft, which should return to base.
 * @note Command to call this: "aircraft_return".
 */
extern void AIR_AircraftReturnToBase (aircraft_t *aircraft)
{
	base_t *base;

	if (aircraft && aircraft->status != AIR_HOME) {
		base = (base_t *) aircraft->homebase;
		Com_DPrintf("return '%s' (%i) to base ('%s').\n", aircraft->name, aircraft->idx, base->name);
		MAP_MapCalcLine(aircraft->pos, base->pos, &aircraft->route);
		aircraft->status = AIR_RETURNING;
		aircraft->time = 0;
		aircraft->point = 0;
		aircraft->mission = NULL;
	}
}

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase.
 * @note This function updates some cvars for current aircraft.
 * @sa CL_GameAutoGo_f
 * @sa CL_GameResults_f
 */
extern void AIR_AircraftReturnToBase_f (void)
{
	aircraft_t *aircraft;

	if (baseCurrent && (baseCurrent->aircraftCurrent >= 0) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		AIR_AircraftReturnToBase(aircraft);
		AIR_AircraftSelect(aircraft);
	}
}

/**
 * @brief Sets aircraftCurrent and updates related cvars.
 * @param[in] *aircraft Pointer to given aircraft.
 * @note If param[in] is NULL, it uses mn_aircraft_idx to determine aircraft.
 * @note If either pointer is NULL or no aircraft in mn_aircraft_idx, it takes
 * @note first aircraft in base (if there is any).
 */
extern void AIR_AircraftSelect (aircraft_t* aircraft)
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

	assert(aircraft);
	CL_UpdateHireVar(aircraft);

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_SetValue("mn_scientists_in_base", E_CountHired(baseCurrent, EMPL_SCIENTIST));
	Cvar_Set("mn_aircraftstatus", AIR_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", va("%s (%i/%i)", aircraft->name, (aircraftID + 1), baseCurrent->numAircraftInBase));
	Cvar_Set("mn_aircraft_model", aircraft->model);
	Cvar_Set("mn_aircraft_weapon", aircraft->weapon ? aircraft->weapon->name : "");
	Cvar_Set("mn_aircraft_shield", aircraft->shield ? aircraft->shield->name : "");
	Cvar_Set("mn_aircraft_weapon_img", aircraft->weapon ? aircraft->weapon->image_top : "menu/airequip_no_weapon");
	Cvar_Set("mn_aircraft_shield_img", aircraft->shield ? aircraft->shield->image_top : "menu/airequip_no_shield");
	Cvar_Set("mn_aircraft_item_img", aircraft->item ? aircraft->item->image_top : "menu/airequip_no_item");

	/* generate aircraft info text */
	/* @todo: reimplement me when aircraft equipment will be implemented. */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%.0f\n"), aircraft->speed);
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), aircraft->fuel / 1000, aircraft->fuelSize / 1000), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapon:\t%s\n"), aircraft->weapon ? _(aircraft->weapon->name) : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Shield:\t%s\n"), aircraft->shield ? _(aircraft->shield->name) : _("None")), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Equipment:\t%s"), aircraft->item ? _(aircraft->item->name) : _("None")), sizeof(aircraftInfo));
	menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;
}

/**
 * @brief Console command binding for AIR_AircraftSelect().
 */
extern void AIR_AircraftSelect_f (void)
{
	/* calling from console? with no baseCurrent? */
	if (!baseCurrent || !baseCurrent->numAircraftInBase || !baseCurrent->hasHangar) {
		MN_PopMenu(qfalse);
		return;
	}

	baseCurrent->aircraftCurrent = -1;
	AIR_AircraftSelect(NULL);
	if (baseCurrent->aircraftCurrent == -1)
		MN_PopMenu(qfalse);
}

/**
 * @brief Searches the global array of aircraft types for a given aircraft.
 * @param[in] *name Aircraft id.
 * @return aircraft_t pointer or NULL if not found.
 */
extern aircraft_t *AIR_GetAircraft (const char *name)
{
	int i;

	assert(name);
	for (i = 0; i < numAircraft_samples; i++) {
		if (!Q_strncmp(aircraft_samples[i].id, name, MAX_VAR))
			return &aircraft_samples[i];
	}

	Com_Printf("Aircraft %s not found (%i).\n", name, numAircraft_samples);
	return NULL;
}

/**
 * @brief Places a new aircraft in the given base.
 * @param[in] base Pointer to base where aircraft should be added.
 * @param[in] name Type of the aircraft to add.
 * @sa B_Load
 */
extern void AIR_NewAircraft (base_t *base, const char *name)
{
	aircraft_t *aircraft;

	assert(base);

	/* First aircraft in base is default aircraft. */
	base->aircraftCurrent = 0;

	aircraft = AIR_GetAircraft(name);
	if (aircraft && base->numAircraftInBase < MAX_AIRCRAFT) {
		/* copy from global aircraft list to base aircraft list */
		/* we do this because every aircraft can have its own parameters */
		memcpy(&base->aircraft[base->numAircraftInBase], aircraft, sizeof(aircraft_t));
		/* now lets use the aircraft array for the base to set some parameters */
		aircraft = &base->aircraft[base->numAircraftInBase];
		aircraft->idx = gd.numAircraft;
		aircraft->homebase = base;
		/* for saving and loading a base */
		aircraft->idxBase = base->idx;
		/* this is the aircraft array id in current base */
		/* NOTE: when we send the aircraft to another base this has to be changed, too */
		aircraft->idxInBase = base->numAircraftInBase;
		/* set initial direction of the aircraft */
		VectorSet(aircraft->direction, 1, 0, 0);
		/* link the teamSize pointer in */
		aircraft->teamSize = &base->teamNum[base->numAircraftInBase];
		AIR_ResetAircraftTeam(aircraft);

		Q_strncpyz(messageBuffer, va(_("You've got a new aircraft (a %s) in base %s"), aircraft->name, base->name), MAX_MESSAGE_TEXT);
		MN_AddNewMessage(_("Notice"), messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf("Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, aircraft->pos);
		Radar_Initialise(&(aircraft->radar), AIRCRAFT_RADAR_RANGE);

		gd.numAircraft++;		/**< Increase the global number of aircraft. */
		base->numAircraftInBase++;	/**< Increase the number of aircraft in the base. */
		/* Update base capacities. */
		Com_DPrintf("idx_sample: %i name: %s weight: %i\n", aircraft->idx_sample, aircraft->id, aircraft->weight);
		AIR_UpdateHangarCapForOne(aircraft->idx_sample, base->idx);
		Com_DPrintf("Adding new aircraft %s with IDX %i for base %s\n", aircraft->name, aircraft->idx, base->name);
		/* Now update the aircraft list - maybe there is a popup active */
		Cbuf_ExecuteText(EXEC_NOW, "aircraft_list");
	}
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] aircraft Pointer to aircraft that should be removed.
 * @note The assigned soldiers (if any) are removed/unassinged from the aircraft - not fired.
 * @note If you want to do something different (kill, fire, etc...) do it before calling this function.
 * @todo Return status of deletion for better error handling.
 */
extern void AIR_DeleteAircraft (aircraft_t *aircraft)
{
	int i = 0;
	base_t *base = NULL;
	aircraft_t *aircraft_temp = NULL;
	transferlist_t *transferlist_temp = NULL;
	int previous_aircraftCurrent = baseCurrent->aircraftCurrent;

	if (!aircraft) {
		Com_DPrintf("AIR_DeleteAircraft: no aircraft given (NULL)\n");
		/* @todo: Return deletion status here. */
		return;
	}

	/* Check if this aircraft is currently transferred. */
	if (aircraft->status == AIR_TRANSIT) {
		Com_DPrintf("CL_DeleteAircraft: this aircraft is currently transferred. We can not remove it.\n");
		/* @todo: Return deletion status here. */
		return;
	}

	base = &gd.bases[aircraft->idxBase];

	if (!base) {
		Com_DPrintf("AIR_DeleteAircraft: No homebase found for aircraft.\n");
		/* @todo: Return deletion status here. */
		return;
	}

	/* Remove all soldiers from the aircraft (the employees are still hired after this). */
	CL_RemoveSoldiersFromAircraft(aircraft->idx, aircraft->idxBase);

	for (i = aircraft->idx; i < gd.numAircraft-1; i++) {
		/* Decrease the global index of aircrafts that have a higher index than the deleted one. */
		aircraft_temp = AIR_AircraftGetFromIdx(i);
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

	/* Now update the aircraft list - maybe there is a popup active. */
	Cbuf_ExecuteText(EXEC_NOW, "aircraft_list");

	/* @todo: Return successful deletion status here. */
}

/**
 * @brief Set pos to a random position on geoscape
 * @param[in] pos Pointer to vec2_t for aircraft position
 * @note Used to place UFOs on geoscape
 * @todo move this to cl_ufo.c - only ufos will get "random position"
 */
extern void CP_GetRandomPosForAircraft (float *pos)
{
	pos[0] = (rand() % 180) - (rand() % 180);
	pos[1] = (rand() % 90) - (rand() % 90);
}

/**
 * @brief Moves given aircraft.
 * @param[in] dt
 * @param[in] *aircraft Pointer to aircraft on its way.
 * @return true if the aircraft reached its destination.
 */
extern qboolean AIR_AircraftMakeMove (int dt, aircraft_t* aircraft)
{
	float dist, frac;
	int p;

	/* calc distance */
	aircraft->time += dt;
	aircraft->fuel -= dt;
	dist = aircraft->speed * aircraft->time / 3600;

	/* Check if destination reached */
	if (dist >= aircraft->route.distance * (aircraft->route.numPoints - 1))
		return qtrue;

	/* calc new position */
	frac = dist / aircraft->route.distance;
	p = (int) frac;
	frac -= p;
	aircraft->point = p;
	aircraft->pos[0] = (1 - frac) * aircraft->route.point[p][0] + frac * aircraft->route.point[p + 1][0];
	aircraft->pos[1] = (1 - frac) * aircraft->route.point[p][1] + frac * aircraft->route.point[p + 1][1];

	return qfalse;
}

/**
 * @brief
 * @param[in] dt
 * @todo: Fuel
 */
void CL_CampaignRunAircraft (int dt)
{
	aircraft_t *aircraft;
	base_t *base;
	int i, j;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded)
			continue;

		/* Run each aircraft */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
			if (aircraft->homebase) {
				if (aircraft->status > AIR_IDLE) {
					/* Aircraft is moving */
					if (AIR_AircraftMakeMove(dt, aircraft)) {
						/* aircraft reach its destination */
						float *end;

						end = aircraft->route.point[aircraft->route.numPoints - 1];
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
				if (aircraft->status >= AIR_IDLE && aircraft->status != AIR_RETURNING && !AIR_AircraftHasEnoughFuel(aircraft, aircraft->pos)) {
					MN_AddNewMessage(_("Notice"), _("Your dropship is low on fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
					AIR_AircraftReturnToBase(aircraft);
				}

				/* Aircraft purchasing ufo */
				if (aircraft->status == AIR_UFO) {
					aircraft_t* ufo = NULL;

					ufo = gd.ufos + aircraft->ufo;
					if (CP_GetDistance(ufo->pos, aircraft->pos) < DISTANCE) {
#if 0							
						/* Display airfight sequence */
						Cbuf_ExecuteText(EXEC_NOW, "seq_start airfight;");
#endif

						/* Solve the fight */
						AIRFIGHT_ExecuteActions(aircraft, ufo);
					} else {
						/* @todo : Find better system to make the aircraft purchasing ufo */
						AIR_SendAircraftPurchasingUfo(aircraft, ufo);
					}
				}
			}
	}
}

/**
 * @brief Returns a list of craftitem technologies for the given type.
 * @note This list is terminated by a NULL pointer.
 * @param[in] type Type of the craft-items to return.
 * @param[in] usetypedef Defines if the type param should be handled as a aircraftItemType_t (qtrue) or not (qfalse - See the code).
 */
static technology_t **AII_GetCraftitemTechsByType (int type, qboolean usetypedef)
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
			Com_Printf("AII_GetCraftitemTechsByType: MAX_TECHNOLOGIES limit hit.\n");
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
void AIM_AircraftEquipmenuInit_f (void)
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
		Com_DPrintf("AIM_AircraftEquipmenuInit_f: Error - node aircraftequip not found\n");
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
		list = AII_GetCraftitemTechsByType(type, qfalse);
		airequipID = AC_ITEM_ARMOUR;
		break;
	case 2:	/* items */
		list = AII_GetCraftitemTechsByType(type, qfalse);
		airequipID = AC_ITEM_ELECTRONICS;
		break;
	default:
		list = AII_GetCraftitemTechsByType(type, qfalse);
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
 * @brief Assigns the weapon to current selected aircraft when clicked on the list.
 * @sa AIM_AircraftEquipmenuInit_f
 */
void AIM_AircraftEquipmenuClick_f (void)
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
		AIR_AircraftSelect(aircraft);
	} else {
		/* build the list of all aircraft items of type airequipID - null terminated */
		list = AII_GetCraftitemTechsByType(airequipID, qtrue);
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
					Com_Printf("AIM_AircraftEquipmenuMenuClick_f: Unknown airequipID: %i\n", airequipID);
				}
				Com_sprintf(desc, sizeof(desc), _((*list)->name));
				AIR_AircraftSelect(aircraft);
				noparams = qtrue; /* used for AIM_AircraftEquipmenuMenuInit_f */
				AIM_AircraftEquipmenuInit_f();
				break;
			}
			/* next item in the tech pointer list */
			list++;
		}
	}
	menuText[TEXT_STANDARD] = desc;
}

/**
 * @brief Returns aircraft for a given global index.
 * @param[in] idx Global aircraft index.
 * @return An aircraft pointer (to a struct in a base) that has the given index or NULL if no aircraft found.
 */
extern aircraft_t* AIR_AircraftGetFromIdx (int idx)
{
	base_t*		base;
	aircraft_t*	aircraft;

	if (idx < 0) {
		Com_DPrintf("AIR_AircraftGetFromIdx: bad aircraft index: %i\n", idx);
		return NULL;
	}

#ifdef PARANOID
	if (gd.numBases < 1) {
		Com_DPrintf("AIR_AircraftGetFromIdx: no base(s) found!\n");
	}
#endif

	for (base = gd.bases; base < (gd.bases + gd.numBases); base++) {
		for (aircraft = base->aircraft; aircraft < (base->aircraft + base->numAircraftInBase); aircraft++) {
			if (aircraft->idx == idx) {
				Com_DPrintf("AIR_AircraftGetFromIdx: aircraft idx: %i - base idx: %i (%s)\n", aircraft->idx, base->idx, base->name);
				return aircraft;
			}
		}
	}

	return NULL;
}

/**
 * @brief Sends the specified aircraft to specified mission.
 * @param[in] *aircraft Pointer to aircraft to send to mission.
 * @param[in] *mission Pointer to given mission.
 * @return qtrue if sending an aircraft to specified mission is possible.
 */
extern qboolean AIR_SendAircraftToMission (aircraft_t* aircraft, actMis_t* mission)
{
	if (!aircraft || !mission)
		return qfalse;

	if (!*(aircraft->teamSize)) {
		MN_Popup(_("Notice"), _("Assign a team to aircraft"));
		return qfalse;
	}

	/* if aircraft was in base, remove soldier aboard from hospital */
	if (aircraft->status == AIR_REFUEL || aircraft->status == AIR_HOME)
		HOS_RemoveEmployeesInHospital(aircraft);

	/* ensure interceptAircraft IDX is set correctly */
	gd.interceptAircraft = aircraft->idx;

	/* if mission is a base-attack and aircraft already in base, launch
	 * mission immediately */
	if (gd.bases[aircraft->idxBase].baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft)) {
		aircraft->mission = mission;
		mission->def->active = qtrue;
		MN_PushMenu("popup_baseattack");
		return qtrue;
	}

	if (!AIR_AircraftHasEnoughFuel(aircraft, mission->realPos))
		return qfalse;

	MAP_MapCalcLine(aircraft->pos, mission->realPos, &(aircraft->route));
	aircraft->status = AIR_MISSION;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->mission = mission;

	return qtrue;
}

/** @brief Valid aircraft items (craftitem) definition values from script files. */
static const value_t aircraftitems_vals[] = {
	{"tech", V_TRANSLATION_STRING, offsetof(aircraftItem_t, tech), 0},
	{"speed", V_FLOAT, offsetof(aircraftItem_t, speed), MEMBER_SIZEOF(aircraftItem_t, speed)},
	{"price", V_INT, offsetof(aircraftItem_t, price), MEMBER_SIZEOF(aircraftItem_t, price)},
	{"shield", V_RELABS, offsetof(aircraftItem_t, shield), MEMBER_SIZEOF(aircraftItem_t, shield)},
	{"wrange", V_INT, offsetof(aircraftItem_t, weaponRange), MEMBER_SIZEOF(aircraftItem_t, weaponRange)},
	{"range", V_RELABS, offsetof(aircraftItem_t, range), MEMBER_SIZEOF(aircraftItem_t, range)},
	{"damage", V_INT, offsetof(aircraftItem_t, damage), MEMBER_SIZEOF(aircraftItem_t, damage)},
	{"accuracy", V_INT, offsetof(aircraftItem_t, accuracy), MEMBER_SIZEOF(aircraftItem_t, accuracy)},
	{"ecm", V_RELABS, offsetof(aircraftItem_t, ecm), MEMBER_SIZEOF(aircraftItem_t, ecm)},
	{"weight", V_STRING, offsetof(aircraftItem_t, weight), 0},
	{"weapon", V_STRING, offsetof(aircraftItem_t, weapon), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses all aircraft items that are defined in our UFO-scripts.
 * @sa CL_ParseClientData
 */
extern void AII_ParseAircraftItem (const char *name, char **text)
{
	const char *errhead = "AII_ParseAircraftItem: unexpected end of file (aircraft ";
	aircraftItem_t *airItem;
	const value_t *vp;
	char *token;

	if (numAircraftItems >= MAX_AIRCRAFTITEMS) {
		Com_Printf("AII_ParseAircraftItem: too many craftitem definitions; def \"%s\" ignored\n", name);
		return;
	}

	/* initialize the menu */
	airItem = &aircraftItems[numAircraftItems];
	memset(airItem, 0, sizeof(aircraftItem_t));

	Com_DPrintf("...found craftitem %s\n", name);
	airItem->idx = numAircraftItems;
	numAircraftItems++;
	Q_strncpyz(airItem->id, name, sizeof(airItem->id));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("AII_ParseAircraftItem: craftitem def \"%s\" without body ignored\n", name);
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
				Com_Printf("AII_ParseAircraftItem: \"%s\" unknown craftitem type: \"%s\" - ignored.\n", name, token);
		} else {
			/* Check for some standard values. */
			for (vp = aircraftitems_vals; vp->string; vp++) {
				if (!Q_strcmp(token, vp->string)) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					Com_ParseValue(airItem, token, vp->type, vp->ofs, vp->size);
					break;
				}
			}
			if (!vp->string) {
				Com_Printf("AII_ParseAircraftItem: unknown token \"%s\" ignored (craftitem %s)\n", token, name);
				COM_EParse(text, errhead, name);
			}
		}
	} while (*text);
}

/** @brief Valid aircraft definition values from script files. */
static const value_t aircraft_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(aircraft_t, name), 0},
	{"shortname", V_TRANSLATION_STRING, offsetof(aircraft_t, shortname), 0},
	{"speed", V_FLOAT, offsetof(aircraft_t, speed), MEMBER_SIZEOF(aircraft_t, speed)},
	{"size", V_INT, offsetof(aircraft_t, size), MEMBER_SIZEOF(aircraft_t, size)},
	{"weight", V_INT, offsetof(aircraft_t, weight), MEMBER_SIZEOF(aircraft_t, weight)},
	{"fuel", V_INT, offsetof(aircraft_t, fuel), MEMBER_SIZEOF(aircraft_t, fuel)},
	{"fuelsize", V_INT, offsetof(aircraft_t, fuelSize), MEMBER_SIZEOF(aircraft_t, fuelSize)},
	{"angles", V_VECTOR, offsetof(aircraft_t, angles), MEMBER_SIZEOF(aircraft_t, angles)},
	{"center", V_VECTOR, offsetof(aircraft_t, center), MEMBER_SIZEOF(aircraft_t, center)},
	{"scale", V_VECTOR, offsetof(aircraft_t, scale), MEMBER_SIZEOF(aircraft_t, scale)},
	{"angles_equip", V_VECTOR, offsetof(aircraft_t, anglesEquip), MEMBER_SIZEOF(aircraft_t, anglesEquip)},
	{"center_equip", V_VECTOR, offsetof(aircraft_t, centerEquip), MEMBER_SIZEOF(aircraft_t, centerEquip)},
	{"scale_equip", V_VECTOR, offsetof(aircraft_t, scaleEquip), MEMBER_SIZEOF(aircraft_t, scaleEquip)},
	{"image", V_STRING, offsetof(aircraft_t, image), 0},

	/* pointer to technology_t */
	{"weapon", V_STRING, offsetof(aircraft_t, weapon_string), 0},
	{"shield", V_STRING, offsetof(aircraft_t, shield_string), 0},
	{"item", V_STRING, offsetof(aircraft_t, item_string), 0},

	{"model", V_STRING, offsetof(aircraft_t, model), 0},
	/* price for selling/buying */
	{"price", V_INT, offsetof(aircraft_t, price), MEMBER_SIZEOF(aircraft_t, price)},
	/* this is needed to let the buy and sell screen look for the needed building */
	/* to place the aircraft in */
	{"building", V_STRING, offsetof(aircraft_t, building), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses all aircraft that are defined in our UFO-scripts.
 * @sa CL_ParseClientData
 * @note parses the aircraft into our aircraft_sample array to use as reference
 */
extern void AIR_ParseAircraft (const char *name, char **text)
{
	const char *errhead = "AIR_ParseAircraft: unexpected end of file (aircraft ";
	aircraft_t *air_samp;
	const value_t *vp;
	char *token;

	if (numAircraft_samples >= MAX_AIRCRAFT) {
		Com_Printf("AIR_ParseAircraft: too many aircraft definitions; def \"%s\" ignored\n", name);
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
		Com_Printf("AIR_ParseAircraft: aircraft def \"%s\" without body ignored\n", name);
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

				Com_ParseValue(air_samp, token, vp->type, vp->ofs, vp->size);
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
		} else if (!Q_strncmp(token, "ufotype", 7)) {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				return;
			if (!Q_strncmp(token, "scout", 5))
				air_samp->ufotype = UFO_SCOUT;
			else if (!Q_strncmp(token, "fighter", 7))
				air_samp->ufotype = UFO_FIGHTER;
			else if (!Q_strncmp(token, "harvester", 9))
				air_samp->ufotype = UFO_HARVESTER;
			else if (!Q_strncmp(token, "condor", 6))
				air_samp->ufotype = UFO_CONDOR;
			else if (!Q_strncmp(token, "carrier", 7))
				air_samp->ufotype = UFO_CARRIER;
		} else if (!vp->string) {
			Com_Printf("AIR_ParseAircraft: unknown token \"%s\" ignored (aircraft %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/*===============================================
Aircraft functions related to UFOs or missions.
===============================================*/

/**
 * @brief Notify that a mission has been removed.
 * @param[in] *mission Pointer to the mission that has been removed.
 */
extern void AIR_AircraftsNotifyMissionRemoved (const actMis_t *const mission)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently moving to the mission will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--) {

			if (aircraft->status == AIR_MISSION) {
				if (aircraft->mission == mission) {
					AIR_AircraftReturnToBase(aircraft);
				} else if (aircraft->mission > mission) {
					(aircraft->mission)--;
				}
			}
		}
	}
}

/**
 * @brief Notify that an UFO has been removed.
 * @param[in] *ufo Pointer to UFO that has been removed.
 */
extern void AIR_AircraftsNotifyUfoRemoved (const aircraft_t *const ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently purchasing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO) {
				if (ufo - gd.ufos == aircraft->ufo)
					AIR_AircraftReturnToBase(aircraft);
				else if (ufo - gd.ufos < aircraft->ufo)
					aircraft->ufo--;
			}
}

/**
 * @brief Notify that an ufo disappear from radars.
 * @param[in] *ufo Pointer to an UFO that has disappeared.
 */
extern void AIR_AircraftsUfoDisappear (const aircraft_t *const ufo)
{
	base_t*		base;
	aircraft_t*	aircraft;

	/* Aircrafts currently pursuing the specified ufo will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO)
				if (ufo - gd.ufos == aircraft->ufo)
					AIR_AircraftReturnToBase(aircraft);
}

/**
 * @brief Make the specified aircraft purchasing an UFO.
 * @param[in] *aircraft Pointer to an aircraft which will hunt for an UFO.
 * @param[in] *ufo Pointer to an UFO.
 */
extern void AIR_SendAircraftPurchasingUfo (aircraft_t* aircraft, aircraft_t* ufo)
{
	int num = ufo - gd.ufos;

	if (num < 0 || num >= gd.numUfos || ! aircraft || ! ufo)
		return;

	/* check if the aircraft has enough fuel to go to the ufo and then come back */
	/* @todo if you have enough fuel to go where the ufo is atm, that doesn't mean that you'll have enough to pursue it ! */
	if (!AIR_AircraftHasEnoughFuel(aircraft, ufo->pos))
		return;

	MAP_MapCalcLine(aircraft->pos, ufo->pos, &(aircraft->route));
	aircraft->status = AIR_UFO;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->ufo = num;
}

/*============================================
Aircraft functions related to team handling.
============================================*/

/**
 * @brief Resets team in given aircraft.
 * @param[in] *aircraft Pointer to an aircraft, where the team will be reset.
 */
void AIR_ResetAircraftTeam (aircraft_t *aircraft)
{
	memset(aircraft->teamIdxs, -1, aircraft->size * sizeof(int));
}

/**
 * @brief Adds given employee to given aircraft.
 * @param[in] *aircraft Pointer to an aircraft, to which we will add employee.
 * @param[in] employee_idx Index of an employee in global array (?)
 * FIXME: is this responsible for adding soldiers to a team in dropship?
 */
extern void AIR_AddToAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;

	if (aircraft == NULL) {
		Com_DPrintf("AIR_AddToAircraftTeam: null aircraft \n");
		return;
	}
	if (*(aircraft->teamSize) < aircraft->size) {
		for (i = 0; i < aircraft->size; i++)
			if (aircraft->teamIdxs[i] == -1) {
				aircraft->teamIdxs[i] = employee_idx;
				Com_DPrintf("AIR_AddToAircraftTeam: added idx '%d'\n", employee_idx);
				break;
			}
		if (i >= aircraft->size)
			Com_DPrintf("AIR_AddToAircraftTeam: couldnt find space\n");
	} else {
		Com_DPrintf("AIR_AddToAircraftTeam: error: no space in aircraft\n");
	}
}

/**
 * @brief Removes given employee to given aircraft.
 * @param[in] *aircraft Pointer to an aircraft, from which we will remove employee.
 * @param[in] employee_idx Index of an employee in global array (?)
 * FIXME: is this responsible for removing soldiers from a team in dropship?
 */
void AIR_RemoveFromAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;

	assert(aircraft);

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] == employee_idx)	{
			aircraft->teamIdxs[i] = -1;
			Com_DPrintf("AIR_RemoveFromAircraftTeam: removed idx '%d' \n", employee_idx);
			return;
		}
	Com_Printf("AIR_RemoveFromAircraftTeam: error: idx '%d' not on aircraft %i (base: %i) in base %i\n", employee_idx, aircraft->idx, aircraft->idxInBase, baseCurrent->idx);
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
extern void AIR_DecreaseAircraftTeamIdxGreaterThan (aircraft_t *aircraft, int employee_idx)
{
	int i;

	if (aircraft == NULL)
		return;

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] > employee_idx) {
			aircraft->teamIdxs[i]--;
			Com_DPrintf("AIR_DecreaseAircraftTeamIdxGreaterThan: decreased idx '%d' \n", aircraft->teamIdxs[i]+1);
		}
}

/**
 * @brief Checks whether given employee is in given aircraft (onboard).
 * @param[in] *aircraft Pointer to an aircraft.
 * @param[in] employee_idx Employee index in global array.
 * @return qtrue if an employee with given index is assigned to given aircraft.
 */
extern qboolean AIR_IsInAircraftTeam (aircraft_t *aircraft, int employee_idx)
{
	int i;
#if defined (DEBUG) || defined (PARANOID)
	base_t* base;
#endif

	if (aircraft == NULL) {
		Com_DPrintf("AIR_IsInAircraftTeam: No aircraft given\n");
		return qfalse;
	}
#ifdef PARANOID
	else {
		base = aircraft->homebase;
		Com_DPrintf("AIR_IsInAircraftTeam: aircraft: '%s' (base: '%s')\n", aircraft->name, base->name);
	}
#endif

	for (i = 0; i < aircraft->size; i++)
		if (aircraft->teamIdxs[i] == employee_idx) {
#ifdef DEBUG
			base = (base_t*)(aircraft->homebase);
			Com_DPrintf("AIR_IsInAircraftTeam: found idx '%d' (homebase: '%s' - baseCurrent: '%s') \n", employee_idx, base ? base->name : "", baseCurrent ? baseCurrent->name : "");
#endif
			return qtrue;
		}
	Com_DPrintf("AIR_IsInAircraftTeam:not found idx '%d' \n", employee_idx);
	return qfalse;
}

/**
 * @brief Save callback for savegames
 * @note Nothing to save here at the moment
 * @sa AIR_Load
 * @sa B_Save
 * @sa SAV_GameSave
 */
extern qboolean AIR_Save (sizebuf_t* sb, void* data)
{
	int i, j;

	/* save the ufos on geoscape */
	MSG_WriteByte(sb, gd.numUfos);
	for (i = 0; i < gd.numUfos; i++) {
		MSG_WriteString(sb, gd.ufos[i].id);
		MSG_WriteByte(sb, gd.ufos[i].visible);
		MSG_WritePos(sb, gd.ufos[i].pos);
		MSG_WriteByte(sb, gd.ufos[i].status);
		MSG_WriteFloat(sb, gd.ufos[i].speed);
		MSG_WriteLong(sb, gd.ufos[i].fuel);
		MSG_WriteLong(sb, gd.ufos[i].fuelSize);
		MSG_WriteShort(sb, gd.ufos[i].time);
		MSG_WriteShort(sb, gd.ufos[i].point);
		MSG_WriteShort(sb, gd.ufos[i].route.numPoints);
		MSG_WriteFloat(sb, gd.ufos[i].route.distance);
		for (j = 0; j < gd.ufos[i].route.numPoints; j++)
			MSG_Write2Pos(sb, gd.ufos[i].route.point[j]);
		MSG_WritePos(sb, gd.ufos[i].direction);
		/* @todo more? */
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @note Nothing to load here at the moment
 * @note employees and bases must have been loaded already
 * @sa AIR_Save
 * @sa B_Load
 * @sa SAV_GameLoad
 */
extern qboolean AIR_Load (sizebuf_t* sb, void* data)
{
	base_t *base;
	aircraft_t *ufo, *aircraft;
	employee_t *employee;
	int i, j, p, hired;
	const char *s;
	/* vars, if aircraft wasn't found */
	vec3_t tmp_vec3t;
	vec2_t tmp_vec2t;
	int tmp_int;

	/* load the ufos on geoscape */
	gd.numUfos = MSG_ReadByte(sb);
	for (i = 0; i < gd.numUfos; i++) {
		s = MSG_ReadString(sb);
		ufo = AIR_GetAircraft(s);
		if (!ufo) {
			Com_Printf("Could not find ufo '%s'\n", s);
			MSG_ReadByte(sb); /* visible */
			MSG_ReadPos(sb, tmp_vec3t);	/* pos */
			MSG_ReadByte(sb);	/* status */
			MSG_ReadFloat(sb);	/* speed */
			MSG_ReadLong(sb);	/* fuel */
			MSG_ReadLong(sb);	/* fuelsize */
			MSG_ReadShort(sb);	/* time */
			MSG_ReadShort(sb);	/* point */
			tmp_int = MSG_ReadShort(sb);	/* numPoints */
			MSG_ReadFloat(sb);	/* distance */
			for (j = 0; j < tmp_int; j++)
				MSG_Read2Pos(sb, tmp_vec2t);	/* route points */
			if (*(int*)data >= 2) { /* >= 2.2 */
				MSG_ReadPos(sb, tmp_vec3t);	/* direction */
			}
		} else {
			memcpy(&gd.ufos[i], ufo, sizeof(aircraft_t));
			ufo = &gd.ufos[i];
			ufo->visible = MSG_ReadByte(sb);
			MSG_ReadPos(sb, ufo->pos);
			ufo->status = MSG_ReadByte(sb);
			ufo->speed = MSG_ReadFloat(sb);
			ufo->fuel = MSG_ReadLong(sb);
			ufo->fuelSize = MSG_ReadLong(sb);
			ufo->time = MSG_ReadShort(sb);
			ufo->point = MSG_ReadShort(sb);
			ufo->route.numPoints = MSG_ReadShort(sb);
			ufo->route.distance = MSG_ReadFloat(sb);
			for (j = 0; j < ufo->route.numPoints; j++)
				MSG_Read2Pos(sb, ufo->route.point[j]);
			if (*(int*)data >= 2) { /* >= 2.2 */
				MSG_ReadPos(sb, ufo->direction);
			}
		}
		/* @todo more? */
	}

	/* now fix the curTeam pointers */
	/* this needs already loaded bases and employees */
	for (i = 0; i < gd.numBases; i++) {
		if (!gd.bases[i].numAircraftInBase)
			continue;
		base = &gd.bases[i];
		/* FIXME: EMPL_ROBOTS => ugvs */
		aircraft = &base->aircraft[base->aircraftCurrent];
		hired = E_CountHired(base, EMPL_SOLDIER);

		for (j = 0, p = 0; j < hired; j++) {
			employee = E_GetHiredEmployee(base, EMPL_SOLDIER, -(j+1));
			/* If employee was incorrectly saved, no point in further updating. */
			if (!employee)
				Sys_Error("AIR_Load()... Could not get employee %i\n", i);

			if (CL_SoldierInAircraft(employee->idx, aircraft->idx)) {
				base->curTeam[p] = &employee->chr;
				p++;
			}
		}

		for (; p < MAX_ACTIVETEAM; p++)
			base->curTeam[p] = NULL;
	}
	return qtrue;
}
