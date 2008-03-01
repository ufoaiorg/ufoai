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
#include "cl_team.h"
#include "cl_mapfightequip.h"
#include "cl_hospital.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "menu/m_popup.h"

aircraft_t aircraft_samples[MAX_AIRCRAFT];		/**< Available aircraft types. */
/**
 * @todo: should be reset to 0 each time scripts are read anew; also
 * aircraft_samples memory should be freed at that time,
 * or old memory used for new records
 */
int numAircraft_samples = 0;

static const int DISTANCE = 15;

/**
 * @brief Updates hangar capacities for one aircraft in given base.
 * @param[in] aircraftID aircraftID Index of aircraft type in aircraft_samples.
 * @param[in] base Pointer to base.
 * @return AIRCRAFT_HANGAR_BIG if aircraft was placed in big hangar
 * @return AIRCRAFT_HANGAR_SMALL if small
 * @return AIRCRAFT_HANGAR_ERROR if error or not possible
 * @sa AIR_NewAircraft
 * @sa AIR_UpdateHangarCapForAll
 */
static int AIR_UpdateHangarCapForOne (int aircraftID, base_t *base)
{
	int aircraftSize = 0, freespace = 0;

	assert(aircraftID != AIRCRAFT_INBASE_INVALID);

	aircraftSize = aircraft_samples[aircraftID].weight;

	if (aircraftSize < AIRCRAFT_SMALL) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne()... aircraft weight is wrong!\n");
#endif
		return AIRCRAFT_HANGAR_ERROR;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForOne()... base does not exist!\n");
#endif
		return AIRCRAFT_HANGAR_ERROR;
	}
	assert(base);
	if (!base->hasBuilding[B_HANGAR] && !base->hasBuilding[B_SMALL_HANGAR]) {
		Com_Printf("AIR_UpdateHangarCapForOne()... base does not have any hangar - error!\n");
		return AIRCRAFT_HANGAR_ERROR;
	}

	if (aircraftSize >= AIRCRAFT_LARGE) {
		if (!base->hasBuilding[B_HANGAR]) {
			Com_Printf("AIR_UpdateHangarCapForOne()... base does not have big hangar - error!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
		freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur;
		if (freespace > 0) {
			base->capacities[CAP_AIRCRAFTS_BIG].cur++;
			return AIRCRAFT_HANGAR_BIG;
		} else {
			/* No free space for this aircraft. This should never happen here. */
			Com_Printf("AIR_UpdateHangarCapForOne()... no free space!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
	} else {
		if (!base->hasBuilding[B_SMALL_HANGAR]) {
			Com_Printf("AIR_UpdateHangarCapForOne()... base does not have small hangar - error!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}
		freespace = base->capacities[CAP_AIRCRAFTS_SMALL].max - base->capacities[CAP_AIRCRAFTS_SMALL].cur;
		if (freespace > 0) {
			base->capacities[CAP_AIRCRAFTS_SMALL].cur++;
			return AIRCRAFT_HANGAR_SMALL;
		} else {
			/* No free space for this aircraft. This should never happen here. */
			Com_Printf("AIR_UpdateHangarCapForOne()... no free space!\n");
			return AIRCRAFT_HANGAR_ERROR;
		}

	}
	Com_DPrintf(DEBUG_CLIENT, "AIR_UpdateHangarCapForOne()... base capacities.cur: small: %i big: %i\n", base->capacities[CAP_AIRCRAFTS_SMALL].cur, base->capacities[CAP_AIRCRAFTS_BIG].cur);
}

/**
 * @brief Updates current capacities for hangars in given base.
 * @param[in] base_idx Index of base in global array.
 * @note Call this function whenever you sell/loss aircraft in given base.
 * @sa BS_SellAircraft_f
 */
void AIR_UpdateHangarCapForAll (int base_idx)
{
	int i;
	base_t *base;
	aircraft_t *aircraft;

	base = B_GetBase(base_idx);

	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_UpdateHangarCapForAll()... base does not exist!\n");
#endif
		return;
	}
	assert(base);
	/* Reset current capacities for hangar. */
	base->capacities[CAP_AIRCRAFTS_BIG].cur = 0;
	base->capacities[CAP_AIRCRAFTS_SMALL].cur = 0;

	for (i = 0; i < base->numAircraftInBase; i++) {
		aircraft = &base->aircraft[i];
		Com_DPrintf(DEBUG_CLIENT, "AIR_UpdateHangarCapForAll()... base: %s, aircraft: %s\n", base->name, aircraft->id);
		AIR_UpdateHangarCapForOne(aircraft->idx_sample, base);
	}
	Com_DPrintf(DEBUG_CLIENT, "AIR_UpdateHangarCapForAll()... base capacities.cur: small: %i big: %i\n", base->capacities[CAP_AIRCRAFTS_SMALL].cur, base->capacities[CAP_AIRCRAFTS_BIG].cur);
}

#ifdef DEBUG
/**
 * @brief Debug function which lists all aircraft in all bases.
 * @note Use with baseID as a parameter to display all aircraft in given base.
 */
void AIR_ListAircraft_f (void)
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

		Com_Printf("Aircraft in base %s: %i\n", base->name, base->numAircraftInBase);
		for (i = 0; i < base->numAircraftInBase; i++) {
			aircraft = &base->aircraft[i];
			Com_Printf("Aircraft %s\n", aircraft->name);
			Com_Printf("...idx cur=base/global %i=%i/%i\n", i, aircraft->idxInBase, aircraft->idx);
			for (k = 0; k < aircraft->maxWeapons; k++) {
				if (aircraft->weapons[k].itemIdx != NONE) {
					Com_Printf("...weapon slot %i contains %s", k, csi.ods[aircraft->weapons[k].itemIdx].id);
					if (!aircraft->weapons[k].installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->weapons[k].installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->weapons[k].installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->weapons[k].installationTime);
					if (aircraft->weapons[k].ammoIdx != NONE)
						if (aircraft->weapons[k].ammoLeft > 1)
							Com_Printf("......this weapon is loaded with ammo %s\n", csi.ods[aircraft->weapons[k].ammoIdx].id);
						else
							Com_Printf("......no more ammo (%s)\n", csi.ods[aircraft->weapons[k].ammoIdx].id);
					else
						Com_Printf("......this weapon isn't loaded with ammo\n");
				}
				else
					Com_Printf("...weapon slot %i is empty\n", k);
			}
			if (aircraft->shield.itemIdx != NONE) {
				Com_Printf("...armour slot contains %s", csi.ods[aircraft->shield.itemIdx].id);
				if (!aircraft->shield.installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->shield.installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->shield.installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->shield.installationTime);
			} else
				Com_Printf("...armour slot is empty\n");
			for (k = 0; k < aircraft->maxElectronics; k++) {
				if (aircraft->electronics[k].itemIdx != NONE) {
					Com_Printf("...electronics slot %i contains %s", k, csi.ods[aircraft->electronics[k].itemIdx].id);
					if (!aircraft->electronics[k].installationTime)
						Com_Printf(" (functional)\n");
					else if (aircraft->electronics[k].installationTime > 0)
						Com_Printf(" (%i hours before installation is finished)\n",aircraft->electronics[k].installationTime);
					else
						Com_Printf(" (%i hours before removing is finished)\n",aircraft->electronics[k].installationTime);
				}
				else
					Com_Printf("...electronics slot %i is empty\n", k);
			}
			Com_Printf("...stats: ");
			for (k = 0; k < AIR_STATS_MAX; k++)
				Com_Printf("%i ", aircraft->stats[k]);
			Com_Printf("\n");
			Com_Printf("...name %s\n", aircraft->id);
			Com_Printf("...type %i\n", aircraft->type);
			Com_Printf("...size %i\n", aircraft->maxTeamSize);
			Com_Printf("...fuel %i\n", aircraft->fuel);
			Com_Printf("...status %s\n", AIR_AircraftStatusToName(aircraft));
			Com_Printf("...pos %.0f:%.0f\n", aircraft->pos[0], aircraft->pos[1]);
			Com_Printf("...team: (%i/%i)\n", aircraft->teamSize, aircraft->maxTeamSize);
			for (k = 0; k < aircraft->maxTeamSize; k++)
				if (aircraft->teamIdxs[k] != -1) {
					Com_Printf("......idx (in global array): %i\n", aircraft->teamIdxs[k]);
					chr = E_GetHiredCharacter(base, aircraft->teamTypes[k] , aircraft->teamIdxs[k]);
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

	if (!baseCurrent)
		return;

	if (baseCurrent->aircraftCurrent == AIRCRAFT_INBASE_INVALID || baseCurrent->aircraftCurrent >= baseCurrent->numAircraftInBase) {
#ifdef DEBUG
		Com_Printf("Error - there is no aircraftCurrent in this base\n");
#endif
		return;
	}

	/* Aircraft cannot start without Command Centre operational. */
	if (!baseCurrent->hasBuilding[B_COMMAND]) {
		MN_Popup(_("Notice"), _("No Command Centre operational in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
	if (aircraft->status < AIR_IDLE) {
		aircraft->pos[0] = baseCurrent->pos[0] + 2;
		aircraft->pos[1] = baseCurrent->pos[1] + 2;
		/* remove soldier aboard from hospital */
		HOS_RemoveEmployeesInHospital(aircraft);
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}
	MN_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
	/* Return to geoscape. */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

/**
 * @brief Translates the aircraft status id to a translateable string
 * @param[in] aircraft Aircraft to translate the status of
 * @return Translation string of given status.
 * @note Called in: CL_AircraftList_f(), AIR_ListAircraft_f(), AIR_AircraftSelect(),
 * @note MAP_DrawMap(), CL_DisplayPopupIntercept()
 */
const char *AIR_AircraftStatusToName (const aircraft_t * aircraft)
{
	assert(aircraft);

	/* display special status if base-attack affects aircraft */
	if (aircraft->homebase->baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft))
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
	case AIR_TRANSFER:
		return _("Being transfered");
	case AIR_RETURNING:
		return _("Back to base");
	default:
		Com_Printf("Error: Unknown aircraft status for %s\n", aircraft->name);
	}
	return NULL;
}

/**
 * @brief Checks whether given aircraft is in its homebase.
 * @param[in] aircraft Pointer to an aircraft.
 * @return qtrue if given aircraft is in its homebase.
 * @return qfalse if given aircraft is not in its homebase.
 * @todo Add check for AIR_REARM when aircraft items will be implemented.
 */
qboolean AIR_IsAircraftInBase (const aircraft_t * aircraft)
{
	if (aircraft->status == AIR_HOME || aircraft->status == AIR_REFUEL)
		return qtrue;
	return qfalse;
}

/**
 * @brief Determines the state of the equip soldier menu button:
 * @returns 1 if no aircraft in base else 2 if no soldiers
 * available otherwise 3
 * @todo use defined constants - no magic numbers
 * @todo Use aircraft->homebase - not baseCurrent, @sa todo in AIR_AircraftSelect
 */
static int CL_EquipSoldierState (const aircraft_t * aircraft)
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
void AIR_NewAircraft_f (void)
{
	int i = -1;
	base_t *b = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <type> <baseID>\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 3)
		i = atoi(Cmd_Argv(2));

	if (!baseCurrent || (i >= 0)) {
		if (i < 0 || i >= MAX_BASES)
			return;

		if (gd.bases[i].founded)
			b = B_GetBase(i);
	} else
		b = baseCurrent;

	if (b)
		AIR_NewAircraft(b, Cmd_Argv(1));
}

/**
 * @brief Restores aircraft cvars after going back from aircraft buy menu.
 * @sa BS_MarketAircraftDescription
 */
void AIM_ResetAircraftCvars_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase < 0))
		return;

	/* Maybe we sold displayed aircraft ? */

	if (baseCurrent->numAircraftInBase == 0) {
		/* no more aircraft in base */
		Cbuf_AddText("mn_pop\n");
		return;
	}

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID == AIRCRAFT_INBASE_INVALID) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf(DEBUG_CLIENT, "AIM_ResetAircraftCvars_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
	}

	AIR_AircraftSelect(NULL);
}

/**
 * @brief Switch to next aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 */
void AIM_NextAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID == AIRCRAFT_INBASE_INVALID) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf(DEBUG_CLIENT, "AIM_NextAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		AIR_AircraftSelect(NULL);
		return;
	}


	Cvar_SetValue("mn_aircraft_idx",(aircraftID + 1) % baseCurrent->numAircraftInBase);
	AIR_AircraftSelect(NULL);
}

/**
 * @brief Switch to previous aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_NextAircraft_f
 */
void AIM_PrevAircraft_f (void)
{
	int aircraftID;

	if (!baseCurrent || (baseCurrent->numAircraftInBase <= 0))
		return;

	aircraftID = Cvar_VariableInteger("mn_aircraft_idx");

	if ((aircraftID == AIRCRAFT_INBASE_INVALID) || (aircraftID >= baseCurrent->numAircraftInBase)) {
		/* Bad aircraft idx found (no or no sane aircraft).
		 * Setting it to the first aircraft since numAircraftInBase has been checked to be at least 1. */
		Com_DPrintf(DEBUG_CLIENT, "AIM_PrevAircraft_f: bad aircraft idx found.\n");
		aircraftID = 0;
		Cvar_SetValue("mn_aircraft_idx", aircraftID);
		AIR_AircraftSelect(NULL);
		return;
	}

	Cvar_SetValue("mn_aircraft_idx",(aircraftID - 1 + baseCurrent->numAircraftInBase) % baseCurrent->numAircraftInBase);
	AIR_AircraftSelect(NULL);
}

/**
 * @brief Calculates the amount of aircraft (of the given type) in the selected base
 * @param[in] base The base to count the aircraft in
 * @param[in] aircraftType The type of the aircraft you want
 */
int AIR_CountTypeInBase (const base_t *base, aircraftType_t aircraftType)
{
	int i, count = 0;

	assert(base);

	for (i = 0; i < base->numAircraftInBase; i++) {
		if (base->aircraft[i].type == aircraftType)
			count++;
	}

	return count;
}

/**
 * @brief Returns the string that matches the given aircraft type
 */
const char *AIR_GetAircraftString (aircraftType_t aircraftType)
{
	switch (aircraftType) {
	case AIRCRAFT_INTERCEPTOR:
		return _("Interceptor");
	case AIRCRAFT_TRANSPORTER:
		return _("Transporter");
	case AIRCRAFT_UFO:
		return _("UFO");
	}
	return "";
}

/**
 * @brief Some of the aircraft values needs special calculations when they
 * are shown in the menus
 * @sa CL_AircraftStatToName
 */
int CL_AircraftMenuStatsValues (const int value, const int stat)
{
	switch (stat) {
	case AIR_STATS_SPEED:
		/* Convert into km/h, and round this value */
		return 10 * (int) (111.2 * value / 10.0f);
	case AIR_STATS_FUELSIZE:
		return value / 1000;
	case AIR_STATS_OP_RANGE:
		/* the 2.0f factor is for going to destination and then come back */
		return 100 * (int) (111.2 * value / (2.0f * 3600.0f * 100.0f));
	default:
		return value;
	}
}

/**
 * @brief check if aircraft has enough fuel to go to destination, and then come back home
 * @param[in] aircraft Pointer to the aircraft
 * @param[in] position Pointer to the position the aircraft should go to
 * @sa MAP_MapCalcLine
 * @return qtrue if the aircraft can go to the position, qfalse else
 */
qboolean AIR_AircraftHasEnoughFuel (const aircraft_t *aircraft, const vec2_t destination)
{
	base_t *base;
	float distance = 0;
	mapline_t line;

	assert(aircraft);
	base = (base_t *) aircraft->homebase;
	assert(base);

	/* Calculate the line that the aircraft should follow to go to destination */
	MAP_MapCalcLine(aircraft->pos, destination, &line);
	distance = line.distance * (line.numPoints - 1);

	/* Calculate the line that the aircraft should then follow to go back home */
	MAP_MapCalcLine(destination, base->pos, &line);
	distance += line.distance * (line.numPoints - 1);

	/* Check if the aircraft has enough fuel to go to destination and then go back home */
	if (distance <= aircraft->stats[AIR_STATS_SPEED] * aircraft->fuel / 3600.0f)
		return qtrue;

	return qfalse;
}

/**
 * @brief Calculates the way back to homebase for given aircraft and returns it.
 * @param[in] aircraft Pointer to aircraft, which should return to base.
 * @note Command to call this: "aircraft_return".
 */
void AIR_AircraftReturnToBase (aircraft_t *aircraft)
{
	base_t *base;

	if (aircraft && aircraft->status != AIR_HOME) {
		base = aircraft->homebase;
		assert(base);
		Com_DPrintf(DEBUG_CLIENT, "return '%s' (%i) to base ('%s').\n", aircraft->name, aircraft->idx, base->name);
		MAP_MapCalcLine(aircraft->pos, base->pos, &aircraft->route);
		aircraft->status = AIR_RETURNING;
		aircraft->time = 0;
		aircraft->point = 0;
		aircraft->mission = NULL;
	}
}

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa CL_GameAutoGo_f
 * @sa CL_GameResults_f
 */
void AIR_AircraftReturnToBase_f (void)
{
	aircraft_t *aircraft;

	if (baseCurrent && (baseCurrent->aircraftCurrent != AIRCRAFT_INBASE_INVALID) && (baseCurrent->aircraftCurrent < baseCurrent->numAircraftInBase)) {
		aircraft = &baseCurrent->aircraft[baseCurrent->aircraftCurrent];
		AIR_AircraftReturnToBase(aircraft);
		AIR_AircraftSelect(aircraft);
	}
}

/**
 * @brief Sets aircraftCurrent and updates related cvars.
 * @param[in] aircraft Pointer to given aircraft.
 * @note If param[in] is NULL, it uses mn_aircraft_idx to determine aircraft.
 * @note If either pointer is NULL or no aircraft in mn_aircraft_idx, it takes
 * @note first aircraft in base (if there is any).
 * @todo If the assert about homebase and baseCurrent isn't hit, we should not
 * use baseCurrent but aircraft->homebase
 */
void AIR_AircraftSelect (aircraft_t* aircraft)
{
	menuNode_t *node = NULL;
	int aircraftID = Cvar_VariableInteger("mn_aircraft_idx");
	static char aircraftInfo[256];

	/* calling from console? with no baseCurrent? */
	if (!baseCurrent || !baseCurrent->numAircraftInBase) {
		MN_MenuTextReset(TEXT_AIRCRAFT_INFO);
		return;
	}

	if (!aircraft) {
		/**
		 * Selecting the first aircraft in base (every base has at least one
		 * aircraft at this point because baseCurrent->numAircraftInBase was zero)
		 * if a non-sane idx was found.
		 */
		if ((aircraftID == AIRCRAFT_INBASE_INVALID) || (aircraftID >= baseCurrent->numAircraftInBase)) {
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
	baseCurrent->aircraftCurrent = aircraftID;

	assert(aircraft);
	assert(aircraft->homebase == baseCurrent);
	CL_UpdateHireVar(aircraft, EMPL_SOLDIER);

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_Set("mn_aircraftstatus", AIR_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", va("%s (%i/%i)", _(aircraft->name), (aircraftID + 1), baseCurrent->numAircraftInBase));
	Cvar_Set("mn_aircraft_model", aircraft->model);

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), va(_("Speed:\t%i km/h\n"),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), CL_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Operational range:\t%i km\n"), CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE] *
		aircraft->stats[AIR_STATS_SPEED], AIR_STATS_OP_RANGE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapons:\t%i on %i\n"), AII_GetSlotItems(AC_ITEM_WEAPON, aircraft), aircraft->maxWeapons), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Armours:\t%i on 1\n"), AII_GetSlotItems(AC_ITEM_SHIELD, aircraft)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Electronics:\t%i on %i"), AII_GetSlotItems(AC_ITEM_ELECTRONICS, aircraft), aircraft->maxElectronics), sizeof(aircraftInfo));

	mn.menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;
}

/**
 * @brief Console command binding for AIR_AircraftSelect().
 */
void AIR_AircraftSelect_f (void)
{
	/* calling from console? with no baseCurrent? */
	if (!baseCurrent || !baseCurrent->numAircraftInBase || (!baseCurrent->hasBuilding[B_HANGAR] && !baseCurrent->hasBuilding[B_SMALL_HANGAR])) {
		MN_PopMenu(qfalse);
		return;
	}

	baseCurrent->aircraftCurrent = AIRCRAFT_INBASE_INVALID;
	AIR_AircraftSelect(NULL);
	if (baseCurrent->aircraftCurrent == AIRCRAFT_INBASE_INVALID)
		MN_PopMenu(qfalse);
}

/**
 * @brief Searches the global array of aircraft types for a given aircraft.
 * @param[in] name Aircraft id.
 * @return aircraft_t pointer or NULL if not found.
 */
aircraft_t *AIR_GetAircraft (const char *name)
{
	int i;

	assert(name);
	for (i = 0; i < numAircraft_samples; i++) {
		if (!Q_strncmp(aircraft_samples[i].id, name, MAX_VAR))
			return &aircraft_samples[i];
	}

	Com_Printf("Aircraft '%s' not found (%i).\n", name, numAircraft_samples);
	return NULL;
}

/**
 * @brief Places a new aircraft in the given base.
 * @param[in] base Pointer to base where aircraft should be added.
 * @param[in] name Id of aircraft to add (aircraft->id).
 * @sa B_Load
 */
aircraft_t* AIR_NewAircraft (base_t *base, const char *name)
{
	aircraft_t *aircraft;

	assert(base);

	/* First aircraft in base is default aircraft. */
	base->aircraftCurrent = 0;

	aircraft = AIR_GetAircraft(name);
	if (aircraft && base->numAircraftInBase < MAX_AIRCRAFT) {
		/* copy generic aircraft description to individal aircraft in base */
		/* we do this because every aircraft can have its own parameters */
		base->aircraft[base->numAircraftInBase] = *aircraft;
		/* now lets use the aircraft array for the base to set some parameters */
		aircraft = &base->aircraft[base->numAircraftInBase];
		aircraft->idx = gd.numAircraft;
		aircraft->homebase = base;
		/* Update the values of its stats */
		AII_UpdateAircraftStats(aircraft);
		/* give him some fuel */
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
		/* this is the aircraft array id in current base */
		/* NOTE: when we send the aircraft to another base this has to be changed, too */
		aircraft->idxInBase = base->numAircraftInBase;
		/* set initial direction of the aircraft */
		VectorSet(aircraft->direction, 1, 0, 0);

		AIR_ResetAircraftTeam(aircraft);

		Q_strncpyz(mn.messageBuffer, va(_("You've got a new aircraft (a %s) in base %s"), _(aircraft->name), base->name), sizeof(mn.messageBuffer));
		MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
		Com_DPrintf(DEBUG_CLIENT, "Setting aircraft to pos: %.0f:%.0f\n", base->pos[0], base->pos[1]);
		Vector2Copy(base->pos, aircraft->pos);
		Radar_Initialise(&aircraft->radar, aircraftRadarRange, 1.0f);

		gd.numAircraft++;		/**< Increase the global number of aircraft. */
		base->numAircraftInBase++;	/**< Increase the number of aircraft in the base. */
		/* Update base capacities. */
		Com_DPrintf(DEBUG_CLIENT, "idx_sample: %i name: %s weight: %i\n", aircraft->idx_sample, aircraft->id, aircraft->weight);
		Com_DPrintf(DEBUG_CLIENT, "Adding new aircraft %s with IDX %i for base %s\n", aircraft->name, aircraft->idx, base->name);
		if (ccs.singleplayer) {
			aircraft->hangar = AIR_UpdateHangarCapForOne(aircraft->idx_sample, base);
			if (aircraft->hangar == AIRCRAFT_HANGAR_ERROR)
				Com_Printf("AIR_NewAircraft()... ERROR, new aircraft but no free space in hangars!\n");
			/* also update the base menu buttons */
			Cmd_ExecuteString("base_init");
		}
		return aircraft;
	}
	return NULL;
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] aircraft Pointer to aircraft that should be removed.
 * @note The assigned soldiers (if any) are removed/unassinged from the aircraft - not fired.
 * @note If you want to do something different (kill, fire, etc...) do it before calling this function.
 * @todo Return status of deletion for better error handling.
 * @todo also remove the assigned items in the slots - like weapons, armours and so on
 */
void AIR_DeleteAircraft (aircraft_t *aircraft)
{
	int i;
	base_t *base;
	aircraft_t *aircraft_temp;
	const int previous_aircraftCurrent = baseCurrent->aircraftCurrent;

	if (!aircraft) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_DeleteAircraft: no aircraft given (NULL)\n");
		/* @todo: Return deletion status here. */
		return;
	}

	/* Check if this aircraft is currently transferred. */
	if (aircraft->status == AIR_TRANSIT) {
		Com_DPrintf(DEBUG_CLIENT, "CL_DeleteAircraft: this aircraft is currently transferred. We can not remove it.\n");
		/* @todo: Return deletion status here. */
		return;
	}

	base = aircraft->homebase;

	if (!base) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_DeleteAircraft: No homebase found for aircraft.\n");
		/* @todo: Return deletion status here. */
		return;
	}

	/* Remove all soldiers from the aircraft (the employees are still hired after this). */
	if (aircraft->teamSize > 0)
		CL_RemoveSoldiersFromAircraft(aircraft->idx, base);

	for (i = aircraft->idx; i < gd.numAircraft; i++) {
		/* Decrease the global index of aircraft that have a higher index than the deleted one. */
		aircraft_temp = AIR_AircraftGetFromIdx(i);
		if (aircraft_temp) {
			aircraft_temp->idx--;
		} else {
			/* For some reason there was no aircraft found for this index. */
			Com_DPrintf(DEBUG_CLIENT, "AIR_DeleteAircraft: No aircraft found for this global index: %i\n", i);
		}
	}

	gd.numAircraft--;	/**< Decrease the global number of aircraft. */

	/* Finally remove the aircraft-struct itself from the base-array and update the order. */
	/**
	 * @todo We need to update _all_ aircraft references here.
	 * Every single idnext that points to an aircraft after this one will need to be updated.
	 */
	base->numAircraftInBase--;
	for (i = aircraft->idxInBase; i < base->numAircraftInBase; i++) {
		/* Remove aircraft and rearrange the aircraft-list (in base). */
		aircraft_temp = &base->aircraft[i];
		memcpy(aircraft_temp, &base->aircraft[i+1], sizeof(aircraft_t));
		aircraft_temp->idxInBase = i;	/**< Re-set the number of aircraft in the base. */

		/* Update index of aircraftCurrent in base if it is affected by the index-change. */
		if (i == previous_aircraftCurrent)
			base->aircraftCurrent--;
	}

	if (base->numAircraftInBase < 1) {
		Cvar_SetValue("mn_equipsoldierstate", 0);
		Cvar_Set("mn_aircraftstatus", "");
		Cvar_Set("mn_aircraftinbase", "0");
		Cvar_Set("mn_aircraftname", "");
		Cvar_Set("mn_aircraft_model", "");
		base->aircraftCurrent = AIRCRAFT_INBASE_INVALID;
	}

	/* also update the base menu buttons */
	Cmd_ExecuteString("base_init");

	/* @todo: Return successful deletion status here. */

	/* update hangar capacities */
	AIR_UpdateHangarCapForAll(base->idx);
}

/**
 * @brief Removes an aircraft from its base and the game.
 * @param[in] aircraft Pointer to aircraft that should be removed.
 * @note aircraft and assigned soldiers (if any) are removed from game.
 * @todo Return status of deletion for better error handling.
 */
void AIR_DestroyAircraft (aircraft_t *aircraft)
{
	int i;
	employee_t *employee;

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->teamIdxs[i] > -1) {
			employee = &(gd.employees[aircraft->teamTypes[i]][aircraft->teamIdxs[i]]);
			assert(employee);
			E_DeleteEmployee(employee, aircraft->teamTypes[i]);
			aircraft->teamIdxs[i] = -1;
			aircraft->teamTypes[i] = MAX_EMPL;
		}
	}

	aircraft->status = AIR_HOME;

	AIR_DeleteAircraft(aircraft);
}

/**
 * @brief Moves given aircraft.
 * @param[in] dt time delta
 * @param[in] aircraft Pointer to aircraft on its way.
 * @return true if the aircraft reached its destination.
 */
qboolean AIR_AircraftMakeMove (int dt, aircraft_t* aircraft)
{
	float dist, frac;
	int p;

	/* calc distance */
	aircraft->time += dt;
	aircraft->fuel -= dt;
	dist = (float) aircraft->stats[AIR_STATS_SPEED] * aircraft->time / 3600.0f;

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

	while (aircraft->pos[0] > 180.0)
		aircraft->pos[0] -= 360.0;
	while (aircraft->pos[0] < -180.0)
		aircraft->pos[0] += 360.0;
	while (aircraft->pos[1] > 90.0)
		aircraft->pos[1] -= 180.0;
	while (aircraft->pos[1] < -90.0)
		aircraft->pos[1] += 180.0;

	return qfalse;
}

/**
 * @brief Handles aircraft movement and actions in geoscape mode
 * @sa CL_CampaignRun
 * @param[in] dt time delta
 */
void CL_CampaignRunAircraft (int dt)
{
	aircraft_t *aircraft;
	base_t *base;
	int i, j, k;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (!base->founded) {
			if (base->numAircraftInBase) {
				/* @todo if a base was destroyed, but there are still
				 * aircraft on their way... */
			}
			continue;
		}

		/* Run each aircraft */
		for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
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
							/* Aircraft reached its mission */
							assert(aircraft->mission);
							aircraft->mission->active = qtrue;
							aircraft->status = AIR_DROP;
							cls.missionaircraft = aircraft;
							MAP_SelectMission(cls.missionaircraft->mission);
							gd.interceptAircraft = cls.missionaircraft->idx;
							Com_DPrintf(DEBUG_CLIENT, "gd.interceptAircraft: %i\n", gd.interceptAircraft);
							CL_GameTimeStop();
							MN_PushMenu("popup_intercept_ready");
							break;
						case AIR_RETURNING:
							/* aircraft entered in homebase */
							CL_AircraftReturnedToHomeBase(aircraft);
							aircraft->status = AIR_REFUEL;
							break;
						case AIR_TRANSFER:
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
					/* Aircraft is refueling at base */
					aircraft->fuel += dt;
					if (aircraft->fuel >= aircraft->stats[AIR_STATS_FUELSIZE]) {
						aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];
						aircraft->status = AIR_HOME;
					}
				}

				/* Check aircraft low fuel (only if aircraft is not already returning to base or in base) */
				if ((aircraft->status != AIR_RETURNING) && (aircraft->status >= AIR_IDLE) &&
					!AIR_AircraftHasEnoughFuel(aircraft, aircraft->pos)) {
					/* @todo: check if aircraft can go to a closer base with free space */
					MN_AddNewMessage(_("Notice"), _("Your dropship is low on fuel and returns to base"), qfalse, MSG_STANDARD, NULL);
					AIR_AircraftReturnToBase(aircraft);
				}

				/* Aircraft purchasing ufo */
				if (aircraft->status == AIR_UFO) {
					/* Solve the fight */
					AIRFIGHT_ExecuteActions(aircraft, aircraft->aircraftTarget);
				}

				/* Update delay to launch next projectile */
				if (aircraft->status >= AIR_IDLE) {
					for (k = 0; k < aircraft->maxWeapons; k++) {
						if (aircraft->weapons[k].delayNextShot > 0)
							aircraft->weapons[k].delayNextShot -= dt;
					}
				}
			} else {
				/* FIXME: Maybe even Sys_Error? */
				Com_Printf("CL_CampaignRunAircraft: aircraft with no homebase (base: %i, aircraft '%s')\n", j, aircraft->id);
			}
	}
}

/**
 * @brief Returns the index of this aircraft item in the list of aircraft Items.
 * @note id may not be null or empty
 * @param[in] id the item id in our csi.ods array
 */
int AII_GetAircraftItemByID (const char *id)
{
	int i;

#ifdef DEBUG
	if (!id || !*id) {
		Com_Printf("AII_GetAircraftItemByID: Called with empty id\n");
		return NONE;
	}
#endif

	for (i = 0; i < csi.numODs; i++) {	/* i = item index */
		if (!Q_strncmp(id, csi.ods[i].id, MAX_VAR)) {
			if (csi.ods[i].craftitem.type < 0)
				Sys_Error("Same name for a none aircraft item object or not the correct buytype for this object (%s)\n", id);
			return i;
		}
	}

	Com_Printf("AII_GetAircraftItemByID: Aircraft Item \"%s\" not found.\n", id);
	return NONE;
}

/**
 * @brief Returns aircraft for a given global index.
 * @param[in] idx Global aircraft index.
 * @return An aircraft pointer (to a struct in a base) that has the given index or NULL if no aircraft found.
 */
aircraft_t* AIR_AircraftGetFromIdx (int idx)
{
	base_t* base;
	aircraft_t* aircraft;

	if (idx == AIRCRAFT_INVALID) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIdx: bad aircraft index: %i\n", idx);
		return NULL;
	}

#ifdef PARANOID
	if (gd.numBases < 1) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIdx: no base(s) found!\n");
	}
#endif

	for (base = gd.bases; base < (gd.bases + gd.numBases); base++) {
		for (aircraft = base->aircraft; aircraft < (base->aircraft + base->numAircraftInBase); aircraft++) {
			if (aircraft->idx == idx) {
				Com_DPrintf(DEBUG_CLIENT, "AIR_AircraftGetFromIdx: aircraft idx: %i - base idx: %i (%s)\n", aircraft->idx, base->idx, base->name);
				return aircraft;
			}
		}
	}

	return NULL;
}

/**
 * @brief Sends the specified aircraft to specified mission.
 * @param[in] aircraft Pointer to aircraft to send to mission.
 * @param[in] mission Pointer to given mission.
 * @return qtrue if sending an aircraft to specified mission is possible.
 */
qboolean AIR_SendAircraftToMission (aircraft_t *aircraft, mission_t *mission)
{
	if (!aircraft || !mission)
		return qfalse;

	if (!aircraft->teamSize) {
		MN_Popup(_("Notice"), _("Assign a team to aircraft"));
		return qfalse;
	}

	/* if aircraft was in base */
	if (aircraft->status == AIR_REFUEL || aircraft->status == AIR_HOME) {
		/* remove soldier aboard from hospital */
		HOS_RemoveEmployeesInHospital(aircraft);
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}

	/* ensure interceptAircraft IDX is set correctly */
	gd.interceptAircraft = aircraft->idx;

	/* if mission is a base-attack and aircraft already in base, launch
	 * mission immediately */
	if (aircraft->homebase->baseStatus == BASE_UNDER_ATTACK &&
		AIR_IsAircraftInBase(aircraft)) {
		aircraft->mission = mission;
		mission->active = qtrue;
		MN_PushMenu("popup_baseattack");
		return qtrue;
	}

	if (!AIR_AircraftHasEnoughFuel(aircraft, mission->pos)) {
		MN_AddNewMessage(_("Notice"), _("Your aircraft doesn't have enough fuel to go there and then come back to its home base."), qfalse, MSG_STANDARD, NULL);
		return qfalse;
	}

	MAP_MapCalcLine(aircraft->pos, mission->pos, &aircraft->route);
	aircraft->status = AIR_MISSION;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->mission = mission;

	return qtrue;
}

/**
 * @brief Initialise all values of an aircraft slot.
 * @param[in] aircraft Pointer to the aircraft which needs initalisation of its slots.
 */
static void AII_InitialiseAircraftSlots (aircraft_t *aircraft)
{
	int i;

	/* initialise weapon slots */
	for (i = 0; i < MAX_AIRCRAFTSLOT; i++) {
		AII_InitialiseSlot(aircraft->weapons + i, aircraft->idx, AC_ITEM_WEAPON);
		AII_InitialiseSlot(aircraft->electronics + i, aircraft->idx, AC_ITEM_ELECTRONICS);
	}
	AII_InitialiseSlot(&aircraft->shield, aircraft->idx, AC_ITEM_SHIELD);
}

/**
 * @brief List of valid strings for itemPos_t
 * @note must be in the same order than itemPos_t in cl_aircraft.h
 */
static const char *air_position_strings[AIR_POSITIONS_MAX] = {
	"nose_left",
	"nose_center",
	"nose_right",
	"wing_left",
	"wing_right",
	"rear_left",
	"rear_center",
	"rear_right"
};

/** @brief Valid aircraft parameter definitions from script files. */
static const value_t aircraft_param_vals[] = {
	{"speed", V_INT, offsetof(aircraft_t, stats[AIR_STATS_SPEED]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"shield", V_INT, offsetof(aircraft_t, stats[AIR_STATS_SHIELD]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"ecm", V_INT, offsetof(aircraft_t, stats[AIR_STATS_ECM]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"damage", V_INT, offsetof(aircraft_t, stats[AIR_STATS_DAMAGE]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"accuracy", V_INT, offsetof(aircraft_t, stats[AIR_STATS_ACCURACY]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"fuelsize", V_INT, offsetof(aircraft_t, stats[AIR_STATS_FUELSIZE]), MEMBER_SIZEOF(aircraft_t, stats[0])},
	{"wrange", V_INT, offsetof(aircraft_t, stats[AIR_STATS_WRANGE]), MEMBER_SIZEOF(aircraft_t, stats[0])},

	{NULL, 0, 0, 0}
};

/** @brief Valid aircraft definition values from script files. */
static const value_t aircraft_vals[] = {
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(aircraft_t, name), 0},
	{"shortname", V_TRANSLATION_MANUAL_STRING, offsetof(aircraft_t, shortname), 0},
	{"size", V_INT, offsetof(aircraft_t, maxTeamSize), MEMBER_SIZEOF(aircraft_t, maxTeamSize)},
	{"weight", V_INT, offsetof(aircraft_t, weight), MEMBER_SIZEOF(aircraft_t, weight)},
	{"nogeoscape", V_BOOL, offsetof(aircraft_t, notOnGeoscape), MEMBER_SIZEOF(aircraft_t, notOnGeoscape)},

	{"image", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, image), 0},
	{"model", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, model), 0},
	/* price for selling/buying */
	{"price", V_INT, offsetof(aircraft_t, price), MEMBER_SIZEOF(aircraft_t, price)},
	/* this is needed to let the buy and sell screen look for the needed building */
	/* to place the aircraft in */
	{"building", V_CLIENT_HUNK_STRING, offsetof(aircraft_t, building), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parses all aircraft that are defined in our UFO-scripts.
 * @sa CL_ParseClientData
 * @sa CL_ParseScriptSecond
 * @note parses the aircraft into our aircraft_sample array to use as reference
 * @note This parsing function writes into two different memory pools
 * one is the cl_localPool which is cleared on every new game, the other is
 * cl_genericPool which is existant until you close the game
 */
void AIR_ParseAircraft (const char *name, const char **text, qboolean assignAircraftItems)
{
	const char *errhead = "AIR_ParseAircraft: unexpected end of file (aircraft ";
	aircraft_t *air_samp = NULL;
	const value_t *vp;
	const char *token;
	int i;
	technology_t *tech;
	aircraftItemType_t itemType = MAX_ACITEMS;

	if (numAircraft_samples >= MAX_AIRCRAFT) {
		Com_Printf("AIR_ParseAircraft: too many aircraft definitions; def \"%s\" ignored\n", name);
		return;
	}

	if (!assignAircraftItems) {
		for (i = 0; i < numAircraft_samples; i++) {
			if (!Q_strcmp(aircraft_samples[i].id, name)) {
				Com_Printf("AIR_ParseAircraft: Second aircraft with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* initialize the menu */
		air_samp = &aircraft_samples[numAircraft_samples];
		memset(air_samp, 0, sizeof(aircraft_t));

		Com_DPrintf(DEBUG_CLIENT, "...found aircraft %s\n", name);
		/* FIXME: is this needed here? I think not, because the index of available aircraft
		 * are set when we create these aircraft from the samples - but i might be wrong here
		 * if i'm not wrong, the gd.numAircraft++ from a few lines below can go into trashbin, too */
		air_samp->idx = gd.numAircraft;
		air_samp->idx_sample = numAircraft_samples;
		air_samp->id = Mem_PoolStrDup(name, cl_genericPool, CL_TAG_NONE);
		air_samp->status = AIR_HOME;
		/* default is no ufo */
		air_samp->ufotype = UFO_MAX;
		AII_InitialiseAircraftSlots(air_samp);

		/* TODO: document why do we have two values for this
		 * gd.numAircraft is reset for every new game */
		numAircraft_samples++;
		gd.numAircraft++;
	} else {
		for (i = 0; i < numAircraft_samples; i++) {
			if (!Q_strcmp(aircraft_samples[i].id, name)) {
				air_samp = &aircraft_samples[i];
				/* initialize slot numbers (useful when restarting a single campaign) */
				air_samp->maxWeapons = 0;
				air_samp->maxElectronics = 0;

				if (air_samp->type == AIRCRAFT_UFO)
					air_samp->ufotype = UFO_ShortNameToID(air_samp->id);

				break;
			}
		}
		if (i == numAircraft_samples) {
			for (i = 0; i < numAircraft_samples; i++) {
				Com_Printf("aircraft id: %s\n", aircraft_samples[i].id);
			}
			Sys_Error("AIR_ParseAircraft: aircraft not found - can not link (%s) - parsed aircraft amount: %i\n", name, numAircraft_samples);
		}
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("AIR_ParseAircraft: aircraft def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (assignAircraftItems) {
			/* write into cl_localPool - this data is reparsed on every new game */
			/* blocks like param { [..] } - otherwise we would leave the loop too early */
			if (*token == '{') {
				FS_SkipBlock(text);
			} else if (!Q_strncmp(token, "shield", 6)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				Com_DPrintf(DEBUG_CLIENT, "use shield %s for aircraft %s\n", token, air_samp->id);
				tech = RS_GetTechByID(token);
				if (tech)
					air_samp->shield.itemIdx = AII_GetAircraftItemByID(tech->provides);
			} else if (!Q_strncmp(token, "slot", 4)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid slot value for aircraft: %s\n", name);
					return;
				}
				do {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;

					if (!Q_strcmp(token, "type")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						for (i = 0; i < MAX_ACITEMS; i++) {
							if (!Q_strcmp(token, air_slot_type_strings[i])) {
								itemType = i;
								switch (itemType) {
								case AC_ITEM_WEAPON:
									air_samp->maxWeapons++;
									break;
								case AC_ITEM_ELECTRONICS:
									air_samp->maxElectronics++;
									break;
								default:
									itemType = MAX_ACITEMS;
								}
								break;
							}
						}
						if (i == MAX_ACITEMS)
							Sys_Error("Unknown value '%s' for slot type\n", token);
					} else if (!Q_strcmp(token, "position")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						for (i = 0; i < AIR_POSITIONS_MAX; i++) {
							if (!Q_strcmp(token, air_position_strings[i])) {
								switch (itemType) {
								case AC_ITEM_WEAPON:
									air_samp->weapons[air_samp->maxWeapons - 1].pos = i;
									break;
								case AC_ITEM_ELECTRONICS:
									air_samp->electronics[air_samp->maxElectronics - 1].pos = i;
									break;
								default:
									i = AIR_POSITIONS_MAX;
								}
								break;
							}
						}
						if (i == AIR_POSITIONS_MAX)
							Sys_Error("Unknown value '%s' for slot position\n", token);
					} else if (!Q_strcmp(token, "contains")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						tech = RS_GetTechByID(token);
						if (tech) {
							switch (itemType) {
							case AC_ITEM_WEAPON:
								air_samp->weapons[air_samp->maxWeapons - 1].itemIdx = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use weapon %s for aircraft %s\n", token, air_samp->id);
								break;
							case AC_ITEM_ELECTRONICS:
								air_samp->electronics[air_samp->maxElectronics - 1].itemIdx = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use electronics %s for aircraft %s\n", token, air_samp->id);
								break;
							default:
								Com_Printf("Ignoring item value '%s' due to unknown slot type\n", token);
							}
						}
					} else if (!Q_strcmp(token, "ammo")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						tech = RS_GetTechByID(token);
						if (tech) {
							if (itemType == AC_ITEM_WEAPON) {
								air_samp->weapons[air_samp->maxWeapons - 1].ammoIdx = AII_GetAircraftItemByID(tech->provides);
								Com_DPrintf(DEBUG_CLIENT, "use ammo %s for aircraft %s\n", token, air_samp->id);
							} else
								Com_Printf("Ignoring ammo value '%s' due to unknown slot type\n", token);
						}
					} else if (!Q_strncmp(token, "size", MAX_VAR)) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (itemType == AC_ITEM_WEAPON) {
							if (!Q_strncmp(token, "light", MAX_VAR))
								air_samp->weapons[air_samp->maxWeapons - 1].size = ITEM_LIGHT;
							else if (!Q_strncmp(token, "medium", MAX_VAR))
								air_samp->weapons[air_samp->maxWeapons - 1].size = ITEM_MEDIUM;
							else if (!Q_strncmp(token, "heavy", MAX_VAR))
								air_samp->weapons[air_samp->maxWeapons - 1].size = ITEM_HEAVY;
							else
								Com_Printf("Unknown size value for aircraft slot: '%s'\n", token);
						} else
							Com_Printf("Ignoring size parameter '%s' for non-weapon aircraft slots\n", token);
					} else
						Com_Printf("AIR_ParseAircraft: Ignoring unknown slot value '%s'\n", token);
				} while (*text); /* dummy condition */
			}
		} else {
			if (!Q_strcmp(token, "shield")) {
				COM_EParse(text, errhead, name);
				continue;
			}
			/* check for some standard values */
			for (vp = aircraft_vals; vp->string; vp++)
				if (!Q_strcmp(token, vp->string)) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					switch (vp->type) {
					case V_TRANSLATION_MANUAL_STRING:
						token++;
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)air_samp + (int)vp->ofs), cl_genericPool, CL_TAG_NONE);
						break;
					default:
						Com_ParseValue(air_samp, token, vp->type, vp->ofs, vp->size);
					}

					break;
				}

			if (vp->string && !Q_strncmp(vp->string, "size", 4)) {
				if (air_samp->maxTeamSize > MAX_ACTIVETEAM) {
					Com_DPrintf(DEBUG_CLIENT, "AIR_ParseAircraft: Set size for aircraft to the max value of %i\n", MAX_ACTIVETEAM);
					air_samp->maxTeamSize = MAX_ACTIVETEAM;
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
			} else if (!Q_strncmp(token, "slot", 5)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid slot value for aircraft: %s\n", name);
					return;
				}
				FS_SkipBlock(text);
			} else if (!Q_strncmp(token, "param", 5)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token != '{') {
					Com_Printf("AIR_ParseAircraft: Invalid param value for aircraft: %s\n", name);
					return;
				}
				do {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;

					for (vp = aircraft_param_vals; vp->string; vp++)
						if (!Q_strcmp(token, vp->string)) {
							/* found a definition */
							token = COM_EParse(text, errhead, name);
							if (!*text)
								return;
							switch (vp->type) {
							case V_TRANSLATION_MANUAL_STRING:
								token++;
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)air_samp + (int)vp->ofs), cl_genericPool, CL_TAG_NONE);
								break;
							default:
								Com_ParseValue(air_samp, token, vp->type, vp->ofs, vp->size);
							}
							break;
						}
					if (!vp->string)
						Com_Printf("AIR_ParseAircraft: Ignoring unknown param value '%s'\n", token);
				} while (*text); /* dummy condition */
			} else if (!vp->string) {
				Com_Printf("AIR_ParseAircraft: unknown token \"%s\" ignored (aircraft %s)\n", token, name);
				COM_EParse(text, errhead, name);
			}
		} /* assignAircraftItems */
	} while (*text);
}

#ifdef DEBUG
/**
 * @brief Debug function that prints aircraft to game console
 */
void AIR_ListAircraftSamples_f (void)
{
	int i = 0, max = numAircraft_samples;
	const value_t *vp;
	aircraft_t *air_samp;

	Com_Printf("%i aircraft\n", max);
	if (Cmd_Argc() == 2) {
		max = atoi(Cmd_Argv(1));
		if (max >= numAircraft_samples || max < 0)
			return;
		i = max - 1;
	}
	for (; i < max; i++) {
		air_samp = &aircraft_samples[i];
		Com_Printf("aircraft: '%s'\n", air_samp->id);
		for (vp = aircraft_vals; vp->string; vp++) {
			Com_Printf("..%s: %s\n", vp->string, Com_ValueToStr(air_samp, vp->type, vp->ofs));
		}
		for (vp = aircraft_param_vals; vp->string; vp++) {
			Com_Printf("..%s: %s\n", vp->string, Com_ValueToStr(air_samp, vp->type, vp->ofs));
		}
	}
}
#endif

/**
 * @brief Reload the weapon of an aircraft
 * @param[in] aircraft Pointer to the aircraft to reload
 * @todo check if there is still ammo in storage, and remove them from it
 * @todo: this should costs credits
 * @sa AIRFIGHT_AddProjectile for the basedefense reload code
 */
void AII_ReloadWeapon (aircraft_t *aircraft)
{
	int idx;
	int i;

	assert(aircraft);

	/* Reload all ammos of aircraft */
	for (i = 0; i < aircraft->maxWeapons; i++) {
		if (aircraft->weapons[i].ammoIdx != NONE) {
			idx = aircraft->weapons[i].ammoIdx;
			aircraft->weapons[i].ammoLeft = csi.ods[idx].ammo;
		}
	}
}

/*===============================================
Aircraft functions related to UFOs or missions.
===============================================*/

/**
 * @brief Notify that a mission has been removed.
 * @param[in] mission Pointer to the mission that has been removed.
 */
void AIR_AircraftsNotifyMissionRemoved (const mission_t *const mission)
{
	base_t* base;
	aircraft_t* aircraft;

	/* Aircraft currently moving to the mission will be redirect to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--) {
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--) {

			if (aircraft->status == AIR_MISSION && aircraft->mission == mission)
				AIR_AircraftReturnToBase(aircraft);
		}
	}
}

/**
 * @brief Notify that a UFO has been removed.
 * @param[in] ufo Pointer to UFO that has been removed.
 */
void AIR_AircraftsNotifyUFORemoved (const aircraft_t *const ufo)
{
	base_t* base;
	aircraft_t* aircraft;
	int i, num;

	num = ufo - gd.ufos;
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		/* Base currently targeting the specified ufo loose their target */
		for (i = 0; i < base->maxBatteries; i++) {
			if (base->targetMissileIdx[i] == num)
				base->targetMissileIdx[i] = AIRFIGHT_BASE_CAN_T_FIRE;
			else if (base->targetMissileIdx[i] > num)
				base->targetMissileIdx[i]--;
		}
		for (i = 0; i < base->maxLasers; i++) {
			if (base->targetLaserIdx[i] == num)
				base->targetLaserIdx[i] = AIRFIGHT_BASE_CAN_T_FIRE;
			else if (base->targetLaserIdx[i] > num)
				base->targetLaserIdx[i]--;
		}

		/* Aircraft currently purchasing the specified ufo will be redirect to base */
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO) {
				if (ufo == aircraft->aircraftTarget)
					AIR_AircraftReturnToBase(aircraft);
				else if (ufo < aircraft->aircraftTarget)
					aircraft->aircraftTarget--;
			}
}

/**
 * @brief Notify that a UFO disappear from radars.
 * @param[in] ufo Pointer to a UFO that has disappeared.
 */
void AIR_AircraftsUFODisappear (const aircraft_t *const ufo)
{
	base_t* base;
	aircraft_t* aircraft;

	/* Aircraft currently pursuing the specified UFO will be redirected to base */
	for (base = gd.bases + gd.numBases - 1; base >= gd.bases; base--)
		for (aircraft = base->aircraft + base->numAircraftInBase - 1;
			aircraft >= base->aircraft; aircraft--)
			if (aircraft->status == AIR_UFO)
				if (ufo == aircraft->aircraftTarget)
					AIR_AircraftReturnToBase(aircraft);
}

/**
 * @brief Make the specified aircraft purchasing a UFO.
 * @param[in] aircraft Pointer to an aircraft which will hunt for a UFO.
 * @param[in] ufo Pointer to a UFO.
 */
qboolean AIR_SendAircraftPursuingUFO (aircraft_t* aircraft, aircraft_t* ufo)
{
	const int num = ufo - gd.ufos;

	if (num < 0 || num >= gd.numUFOs || ! aircraft || ! ufo)
		return qfalse;

	/* if aircraft was in base */
	if (aircraft->status == AIR_REFUEL || aircraft->status == AIR_HOME) {
		/* remove soldier aboard from hospital */
		HOS_RemoveEmployeesInHospital(aircraft);
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}

	/* don't check if the aircraft has enough fuel: maybe UFO will come closer */

	MAP_MapCalcLine(aircraft->pos, ufo->pos, &aircraft->route);
	aircraft->status = AIR_UFO;
	aircraft->time = 0;
	aircraft->point = 0;
	aircraft->aircraftTarget = ufo;
	aircraft->baseTarget = NULL;
	return qtrue;
}

/*============================================
Aircraft functions related to team handling.
============================================*/

/**
 * @brief Resets team in given aircraft.
 * @param[in] aircraft Pointer to an aircraft, where the team will be reset.
 */
void AIR_ResetAircraftTeam (aircraft_t *aircraft)
{
	int i;

	for (i = 0; i < MAX_ACTIVETEAM; i++) {
		aircraft->teamIdxs[i] = -1;
		aircraft->teamTypes[i] = MAX_EMPL;
	}
	aircraft->teamSize = 0;
}

/**
 * @brief Adds given employee to given aircraft.
 * @param[in] aircraft Pointer to an aircraft, to which we will add employee.
 * @param[in] employee_idx Index of an employee in global array (?)
 * @todo FIXME: is this responsible for adding soldiers to a team in dropship?
 * 	ANSWER: yes it seems to be.
 */
void AIR_AddToAircraftTeam (aircraft_t *aircraft, int employee_idx, employeeType_t employeeType)
{
	int i;

	assert(employeeType <= MAX_EMPL);

	if (aircraft == NULL) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: null aircraft \n");
		return;
	}
	if (aircraft->teamSize < aircraft->maxTeamSize) {
		/* Search for unused place in aircraft and fill it with  employee-data. */
		/** @todo  do we need to update aircraft->teamSize here as well? */
		for (i = 0; i < aircraft->maxTeamSize; i++)
			if (aircraft->teamIdxs[i] == -1) {
				aircraft->teamIdxs[i] = employee_idx;
				aircraft->teamTypes[i] = employeeType;
				Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: added idx '%d'\n", employee_idx);
				aircraft->teamSize++;
				break;
			}
		if (i >= aircraft->maxTeamSize)
			Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: couldnt find space\n");
	} else {
		Com_DPrintf(DEBUG_CLIENT, "AIR_AddToAircraftTeam: error: no space in aircraft\n");
	}
}

/**
 * @brief Removes given employee from given aircraft team.
 * @param[in] aircraft Pointer to an aircraft, from which we will remove employee.
 * @param[in] employee_idx Index of an employee in global array (?)
 * @note This is responsible for removing soldiers from a team in dropship
 */
void AIR_RemoveFromAircraftTeam (aircraft_t *aircraft, int employee_idx, employeeType_t employeeType)
{
	int i;

	assert(aircraft);

	if (aircraft->teamSize <= 0) {
		Com_Printf("AIR_RemoveFromAircraftTeam()... aircraft->teamSize is less than or equal to 0, we should not be here!\n");
		return;
	}

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		/* Search for this exact employee in the aircraft and make his place "unused". */
		/** @todo  do we need to update aircraft->teamSize here as well? */
		if (aircraft->teamIdxs[i] != -1 && aircraft->teamIdxs[i] == employee_idx && aircraft->teamTypes[i] == employeeType)	{
			aircraft->teamIdxs[i] = -1;
			aircraft->teamTypes[i] = MAX_EMPL;
			Com_DPrintf(DEBUG_CLIENT, "AIR_RemoveFromAircraftTeam: removed idx '%d' \n", employee_idx);
			aircraft->teamSize--;
			return;
		}
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
void AIR_DecreaseAircraftTeamIdxGreaterThan (aircraft_t *aircraft, int employee_idx, employeeType_t employeeType)
{
	int i;

	if (aircraft == NULL)
		return;

	for (i = 0; i < aircraft->maxTeamSize; i++)
		if (aircraft->teamIdxs[i] > employee_idx && aircraft->teamTypes[i] == employeeType) {
			aircraft->teamIdxs[i]--;
			Com_DPrintf(DEBUG_CLIENT, "AIR_DecreaseAircraftTeamIdxGreaterThan: decreased idx '%d' \n", aircraft->teamIdxs[i]+1);
		}
}

/**
 * @brief Checks whether given employee is in given aircraft (onboard).
 * @param[in] aircraft Pointer to an aircraft.
 * @param[in] employee_idx Employee index in global array.
 * @return qtrue if an employee with given index is assigned to given aircraft.
 */
qboolean AIR_IsInAircraftTeam (const aircraft_t *aircraft, int employee_idx, employeeType_t employeeType)
{
	int i;

	if (aircraft == NULL) {
		Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam: No aircraft given\n");
		return qfalse;
	}

	if (employee_idx < 0 || employee_idx > gd.numEmployees[employeeType]) {
		Com_Printf("AIR_IsInAircraftTeam: No valid employee_idx given %i\n", employee_idx);
		return qfalse;
	}

#ifdef PARANOID
	else {
		if (aircraft->homebase)
			Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam: aircraft: '%s' (base: '%s')\n", aircraft->name, aircraft->homebase->name);
	}
#endif

	for (i = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->teamIdxs[i] == employee_idx && aircraft->teamTypes[i] == employeeType) {
			/** @note This also skips the -1 entries in teamIdxs. */
#ifdef DEBUG
			Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam: found idx '%d' (homebase: '%s' - baseCurrent: '%s') \n", employee_idx, aircraft->homebase ? aircraft->homebase->name : "", baseCurrent ? baseCurrent->name : "");
#endif
			return qtrue;
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "AIR_IsInAircraftTeam:not found idx '%d' \n", employee_idx);
	return qfalse;
}

/**
 * @brief Save callback for savegames
 * @sa AIR_Load
 * @sa B_Save
 * @sa SAV_GameSave
 */
qboolean AIR_Save (sizebuf_t* sb, void* data)
{
	int i, j;

	/* save the ufos on geoscape */
	for (i = 0; i < presaveArray[PRE_NUMUFO]; i++) {
		MSG_WriteString(sb, gd.ufos[i].id);
		MSG_WriteByte(sb, gd.ufos[i].visible);
		MSG_WriteByte(sb, gd.ufos[i].notOnGeoscape);
		MSG_WritePos(sb, gd.ufos[i].pos);
		MSG_WriteByte(sb, gd.ufos[i].status);
		MSG_WriteLong(sb, gd.ufos[i].fuel);
		MSG_WriteShort(sb, gd.ufos[i].time);
		MSG_WriteShort(sb, gd.ufos[i].point);
		MSG_WriteShort(sb, gd.ufos[i].route.numPoints);
		MSG_WriteFloat(sb, gd.ufos[i].route.distance);
		for (j = 0; j < gd.ufos[i].route.numPoints; j++)
			MSG_Write2Pos(sb, gd.ufos[i].route.point[j]);
		MSG_WritePos(sb, gd.ufos[i].direction);
		MSG_WriteString(sb, gd.ufos[i].mission->id);
		for (j = 0; j < presaveArray[PRE_AIRSTA]; j++) {
#ifdef DEBUG
			if (gd.ufos[i].stats[j] < 0)
				Com_Printf("Warning: ufo '%s' stats %i is smaller than 0\n", gd.ufos[i].id, gd.ufos[i].stats[j]);
#endif
			MSG_WriteLong(sb, gd.ufos[i].stats[j]);
		}
		/* Save target of the ufo */
		if (gd.ufos[i].baseTarget)
			MSG_WriteShort(sb, gd.ufos[i].baseTarget->idx);
		else
			MSG_WriteShort(sb, -1);
		if (gd.ufos[i].aircraftTarget)
			MSG_WriteShort(sb, gd.ufos[i].aircraftTarget->idx);
		else
			MSG_WriteShort(sb, -1);

		/* save weapon slots */
		MSG_WriteByte(sb, gd.ufos[i].maxWeapons);
		for (j = 0; j < gd.ufos[i].maxWeapons; j++) {
			if (gd.ufos[i].weapons[j].itemIdx != NONE) {
				MSG_WriteString(sb, csi.ods[gd.ufos[i].weapons[j].itemIdx].id);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].ammoLeft);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].delayNextShot);
				MSG_WriteShort(sb, gd.ufos[i].weapons[j].installationTime);
				/* if there is no ammo MSG_WriteString will write an empty string */
				MSG_WriteString(sb, csi.ods[gd.ufos[i].weapons[j].ammoIdx].id);
			} else {
				MSG_WriteString(sb, "");
				MSG_WriteShort(sb, 0);
				MSG_WriteShort(sb, 0);
				MSG_WriteShort(sb, 0);
				/* if there is no ammo MSG_WriteString will write an empty string */
				MSG_WriteString(sb, "");
			}
		}
		/* save shield slots - currently only one */
		MSG_WriteByte(sb, 1);
		if (gd.ufos[i].shield.itemIdx != NONE) {
			MSG_WriteString(sb, csi.ods[gd.ufos[i].shield.itemIdx].id);
			MSG_WriteShort(sb, gd.ufos[i].shield.installationTime);
		} else {
			MSG_WriteString(sb, "");
			MSG_WriteShort(sb, 0);
		}
		/* save electronics slots */
		MSG_WriteByte(sb, gd.ufos[i].maxElectronics);
		for (j = 0; j < gd.ufos[i].maxElectronics; j++) {
			if (gd.ufos[i].electronics[j].itemIdx != NONE) {
				MSG_WriteString(sb, csi.ods[gd.ufos[i].electronics[j].itemIdx].id);
				MSG_WriteShort(sb, gd.ufos[i].electronics[j].installationTime);
			} else {
				MSG_WriteString(sb, "");
				MSG_WriteShort(sb, 0);
			}
		}
		/* @todo more? */
	}

	/* Save projectiles. */
	MSG_WriteByte(sb, gd.numProjectiles);
	for (i = 0; i < gd.numProjectiles; i++) {
		MSG_WriteString(sb, csi.ods[gd.projectiles[i].aircraftItemsIdx].id);
		MSG_WritePos(sb, gd.projectiles[i].pos);
		MSG_WritePos(sb, gd.projectiles[i].idleTarget);
		if (gd.projectiles[i].attackingAircraft) {
			MSG_WriteByte(sb, gd.projectiles[i].attackingAircraft->type == AIRCRAFT_UFO);
			if (gd.projectiles[i].attackingAircraft->type == AIRCRAFT_UFO)
				MSG_WriteShort(sb, gd.projectiles[i].attackingAircraft - gd.ufos);
			else
				MSG_WriteShort(sb, gd.projectiles[i].attackingAircraft->idx);
		} else
			MSG_WriteByte(sb, 2);
		if (gd.projectiles[i].aimedBase)
			MSG_WriteShort(sb, gd.projectiles[i].aimedBase->idx);
		else
			MSG_WriteShort(sb, -1);
		if (gd.projectiles[i].aimedAircraft) {
			MSG_WriteByte(sb, gd.projectiles[i].aimedAircraft->type == AIRCRAFT_UFO);
			if (gd.projectiles[i].aimedAircraft->type == AIRCRAFT_UFO)
				MSG_WriteShort(sb, gd.projectiles[i].aimedAircraft - gd.ufos);
			else
				MSG_WriteShort(sb, gd.projectiles[i].aimedAircraft->idx);
		} else
			MSG_WriteByte(sb, 2);
		MSG_WriteShort(sb, gd.projectiles[i].time);
		MSG_WriteFloat(sb, gd.projectiles[i].angle);
		MSG_WriteByte(sb, gd.projectiles[i].bullets);
		for (j = 0; j < presaveArray[PRE_MAXBUL]; j++)
			MSG_Write2Pos(sb, gd.projectiles[i].bulletPos[j]);
	}

	/* Save recoveries. */
	for (i = 0; i < presaveArray[PRE_MAXREC]; i++) {
		MSG_WriteByte(sb, gd.recoveries[i].active);
		MSG_WriteByte(sb, gd.recoveries[i].baseID);
		MSG_WriteByte(sb, gd.recoveries[i].ufotype);
		MSG_WriteLong(sb, gd.recoveries[i].event.day);
		MSG_WriteLong(sb, gd.recoveries[i].event.sec);
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @note employees and bases must have been loaded already
 * @sa AIR_Save
 * @sa B_Load
 * @sa SAV_GameLoad
 */
qboolean AIR_Load (sizebuf_t* sb, void* data)
{
	aircraft_t *ufo;
	int i, j;
	const char *s;
	/* vars, if aircraft wasn't found */
	vec3_t tmp_vec3t;
	vec2_t tmp_vec2t;
	int tmp_int;
	technology_t *tech;

	/* load the amount of ufos on geoscape */
	gd.numUFOs = presaveArray[PRE_NUMUFO];
	/* load the ufos on geoscape */
	for (i = 0; i < presaveArray[PRE_NUMUFO]; i++) {
		s = MSG_ReadString(sb);
		ufo = AIR_GetAircraft(s);
		if (!ufo) {
			Com_Printf("AIR_Load: Could not find ufo '%s'\n", s);
			MSG_ReadByte(sb); 			/* visible */
			MSG_ReadByte(sb); 			/* notOnGeoscape */
			MSG_ReadPos(sb, tmp_vec3t);		/* pos */
			MSG_ReadByte(sb);			/* status */
			MSG_ReadLong(sb);			/* fuel */
			MSG_ReadShort(sb);			/* time */
			MSG_ReadShort(sb);			/* point */
			tmp_int = MSG_ReadShort(sb);		/* numPoints */
			MSG_ReadFloat(sb);			/* distance */
			for (j = 0; j < tmp_int; j++)
				MSG_Read2Pos(sb, tmp_vec2t);	/* route points */
			MSG_ReadPos(sb, tmp_vec3t);		/* direction */
			MSG_ReadString(sb);			/* Mission id */
			for (j = 0; j < presaveArray[PRE_AIRSTA]; j++)
				MSG_ReadLong(sb);
			MSG_ReadShort(sb);			/* baseTarget index */
			MSG_ReadShort(sb);			/* aircraftTarget index */
			/* read slots */
			tmp_int = MSG_ReadByte(sb);
			for (j = 0; j < tmp_int; j++) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadShort(sb);
				MSG_ReadString(sb);
			}
			/* shield */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
			}
			/* electro */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				MSG_ReadString(sb);
				MSG_ReadShort(sb);
			}
		} else {
			memcpy(&gd.ufos[i], ufo, sizeof(aircraft_t));
			ufo = &gd.ufos[i];
			ufo->visible = MSG_ReadByte(sb);
			ufo->notOnGeoscape = MSG_ReadByte(sb);
			MSG_ReadPos(sb, ufo->pos);
			ufo->status = MSG_ReadByte(sb);
			ufo->fuel = MSG_ReadLong(sb);
			ufo->time = MSG_ReadShort(sb);
			ufo->point = MSG_ReadShort(sb);
			ufo->route.numPoints = MSG_ReadShort(sb);
			ufo->route.distance = MSG_ReadFloat(sb);
			for (j = 0; j < ufo->route.numPoints; j++)
				MSG_Read2Pos(sb, ufo->route.point[j]);
			MSG_ReadPos(sb, ufo->direction);
			gd.ufos[i].mission = CP_GetMissionById(MSG_ReadString(sb));
			for (j = 0; j < presaveArray[PRE_AIRSTA]; j++)
				ufo->stats[j] = MSG_ReadLong(sb);
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int == -1)
				ufo->baseTarget = NULL;
			else
				ufo->baseTarget = B_GetBase(tmp_int);
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int == -1)
				ufo->aircraftTarget = NULL;
			else
				ufo->aircraftTarget = AIR_AircraftGetFromIdx(tmp_int);
			/* read weapon slot */
			tmp_int = MSG_ReadByte(sb);
			for (j = 0; j < tmp_int; j++) {
				/* check that there are enough slots in this aircraft */
				if (j < ufo->maxWeapons) {
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						AII_AddItemToSlot(NULL, tech, ufo->weapons + j);
					ufo->weapons[j].ammoLeft = MSG_ReadShort(sb);
					ufo->weapons[j].delayNextShot = MSG_ReadShort(sb);
					ufo->weapons[j].installationTime = MSG_ReadShort(sb);
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						ufo->weapons[j].ammoIdx = AII_GetAircraftItemByID(tech->provides);
				} else {
					/* just in case there are less slots in new game than in saved one */
					MSG_ReadString(sb);
					MSG_ReadShort(sb);
					MSG_ReadShort(sb);
					MSG_ReadShort(sb);
					MSG_ReadString(sb);
				}
			}
			/* check for shield slot */
			/* there is only one shield - but who knows - breaking the savegames if this changes
			 * isn't worth it */
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int) {
				tech = RS_GetTechByProvided(MSG_ReadString(sb));
				if (tech)
					AII_AddItemToSlot(NULL, tech, &ufo->shield);
				ufo->shield.installationTime = MSG_ReadShort(sb);
			}
			/* read electronics slot */
			tmp_int = MSG_ReadByte(sb);
			for (j = 0; j < tmp_int; j++) {
				/* check that there are enough slots in this aircraft */
				if (j < ufo->maxElectronics) {
					tech = RS_GetTechByProvided(MSG_ReadString(sb));
					if (tech)
						AII_AddItemToSlot(NULL, tech, ufo->electronics + j);
					ufo->electronics[j].installationTime = MSG_ReadShort(sb);
				} else {
					MSG_ReadString(sb);
					MSG_ReadShort(sb);
				}
			}
		}
		/* @todo more? */
	}

	/* Load projectiles. */
	gd.numProjectiles = MSG_ReadByte(sb);
	if (gd.numProjectiles > MAX_PROJECTILESONGEOSCAPE)
		Sys_Error("AIR_Load()... Too many projectiles on map (%i)\n", gd.numProjectiles);

	for (i = 0; i < gd.numProjectiles; i++) {
		tech = RS_GetTechByProvided(MSG_ReadString(sb));
		if (tech) {
			gd.projectiles[i].aircraftItemsIdx = AII_GetAircraftItemByID(tech->provides);
			gd.projectiles[i].idx = i;
			MSG_ReadPos(sb, gd.projectiles[i].pos);
			MSG_ReadPos(sb, gd.projectiles[i].idleTarget);
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int == 2)
				gd.projectiles[i].attackingAircraft = NULL;
			else if (tmp_int == 1)
				gd.projectiles[i].attackingAircraft = gd.ufos + MSG_ReadShort(sb);
			else
				gd.projectiles[i].attackingAircraft = AIR_AircraftGetFromIdx(MSG_ReadShort(sb));
			tmp_int = MSG_ReadShort(sb);
			if (tmp_int >= 0)
				gd.projectiles[i].aimedBase = B_GetBase(tmp_int);
			else
				gd.projectiles[i].aimedBase = NULL;
			tmp_int = MSG_ReadByte(sb);
			if (tmp_int == 2)
				gd.projectiles[i].aimedAircraft = NULL;
			else if (tmp_int == 1)
				gd.projectiles[i].aimedAircraft = gd.ufos + MSG_ReadShort(sb);
			else
				gd.projectiles[i].aimedAircraft = AIR_AircraftGetFromIdx(MSG_ReadShort(sb));
			gd.projectiles[i].time = MSG_ReadShort(sb);
			gd.projectiles[i].angle = MSG_ReadFloat(sb);
			gd.projectiles[i].bullets = MSG_ReadByte(sb);
			for (j = 0; j < presaveArray[PRE_MAXBUL]; j++)
				MSG_Read2Pos(sb, gd.projectiles[i].bulletPos[j]);
		} else
			Sys_Error("AIR_Load()... Could not get technology of projectile %i\n", i);
	}

	/* Load recoveries. */
	for (i = 0; i < presaveArray[PRE_MAXREC]; i++) {
		gd.recoveries[i].active = MSG_ReadByte(sb);
		gd.recoveries[i].baseID = MSG_ReadByte(sb);
		gd.recoveries[i].ufotype = MSG_ReadByte(sb);
		gd.recoveries[i].event.day = MSG_ReadLong(sb);
		gd.recoveries[i].event.sec = MSG_ReadLong(sb);
	}

	for (i = gd.numUFOs - 1; i >= 0; i--) {
		if (gd.ufos[i].time < 0 || gd.ufos[i].stats[AIR_STATS_SPEED] <= 0) {
			Com_Printf("AIR_Load: Found invalid ufo entry - remove it - time: %i - speed: %i\n",
				gd.ufos[i].time, gd.ufos[i].stats[AIR_STATS_SPEED]);
			UFO_RemoveFromGeoscape(&gd.ufos[i]);
		}
	}

	return qtrue;
}

/**
 * @brief Returns true if the current base is able to handle aircraft
 * @sa B_BaseInit_f
 * @todo if base is under attack, if there is no command center, if there is no power ?
 */
qboolean AIR_AircraftAllowed (const base_t* base)
{
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && (base->hasBuilding[B_HANGAR] || base->hasBuilding[B_SMALL_HANGAR])) {
		return qtrue;
	} else {
		return qfalse;
	}
}

/**
 * @brief Checks the parsed buildings for errors
 * @return false if there are errors - true otherwise
 */
qboolean AIR_ScriptSanityCheck (void)
{
	int i, j, k, error = 0;
	int var;
	aircraft_t* a;

	for (i = 0, a = aircraft_samples; i < numAircraft_samples; i++, a++) {
		if (!a->name) {
			error++;
			Com_Printf("...... aircraft '%s' has no name\n", a->id);
		}
		if (!a->shortname) {
			error++;
			Com_Printf("...... aircraft '%s' has no shortname\n", a->id);
		}

		/* check that every weapons fits slot */
		for (j = 0; j < a->maxWeapons - 1; j++)
			if (a->weapons[j].itemIdx != NONE && AII_GetItemWeightBySize(&csi.ods[a->weapons[j].itemIdx]) > a->weapons[j].size) {
				error++;
				Com_Printf("...... aircraft '%s' has an item (%s) too heavy for its slot\n", a->id, csi.ods[a->weapons[j].itemIdx].id);
			}

		/* check that every slots has a different location for PHALANX aircraft (not needed for UFOs) */
		if (a->type != AIRCRAFT_UFO) {
			for (j = 0; j < a->maxWeapons - 1; j++) {
				var = a->weapons[j].pos;
				for (k = j + 1; k < a->maxWeapons; k++)
					if (var == a->weapons[k].pos) {
						error++;
						Com_Printf("...... aircraft '%s' has 2 weapons slots at the same location\n", a->id);
					}
			}
			for (j = 0; j < a->maxElectronics - 1; j++) {
				var = a->electronics[j].pos;
				for (k = j + 1; k < a->maxElectronics; k++)
					if (var == a->electronics[k].pos) {
						error++;
						Com_Printf("...... aircraft '%s' has 2 electronics slots at the same location\n", a->id);
					}
			}
		}
	}

	if (!error)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Calculates free space in hangars in given base.
 * @param[in] aircraftID aircraftID Index of aircraft type in aircraft_samples.
 * @param[in] base_idx Index of base in global array.
 * @param[in] used Additional space "used" in hangars (use that when calculating space for more than one aircraft).
 * @return Amount of free space in hangars suitable for given aircraft type.
 * @note Returns -1 in case of error. Returns 0 if no error but no free space.
 */
int AIR_CalculateHangarStorage (int aircraftID, base_t *base, int used)
{
	int aircraftSize = 0, freespace = 0;

	assert(aircraftID != AIRCRAFT_INBASE_INVALID);

	aircraftSize = aircraft_samples[aircraftID].weight;

	if (aircraftSize < AIRCRAFT_SMALL) {
#ifdef DEBUG
		Com_Printf("AIR_CalculateHangarStorage()... aircraft weight is wrong!\n");
#endif
		return -1;
	}
	if (!base) {
#ifdef DEBUG
		Com_Printf("AIR_CalculateHangarStorage()... base does not exist!\n");
#endif
		return -1;
	}
	assert(base);

	/* If this is a small aircraft, we will check space in small hangar. */
	if (aircraftSize == AIRCRAFT_SMALL) {
		freespace = base->capacities[CAP_AIRCRAFTS_SMALL].max - base->capacities[CAP_AIRCRAFTS_SMALL].cur - used;
		Com_DPrintf(DEBUG_CLIENT, "AIR_CalculateHangarStorage()... freespace (small): %i aircraft weight: %i (max: %i, cur: %i)\n", freespace, aircraftSize,
			base->capacities[CAP_AIRCRAFTS_SMALL].max, base->capacities[CAP_AIRCRAFTS_SMALL].cur);
		if (freespace > 0) {
			return freespace;
		} else {
			/* No free space for this aircraft. */
			return 0;
		}
	} else {
		/* If this is a large aircraft, we will check space in big hangar. */
		freespace = base->capacities[CAP_AIRCRAFTS_BIG].max - base->capacities[CAP_AIRCRAFTS_BIG].cur - used;
		Com_DPrintf(DEBUG_CLIENT, "AIR_CalculateHangarStorage()... freespace (big): %i aircraft weight: %i (max: %i, cur: %i)\n", freespace, aircraftSize,
			base->capacities[CAP_AIRCRAFTS_BIG].max, base->capacities[CAP_AIRCRAFTS_BIG].cur);
		if (freespace > 0) {
			return freespace;
		} else {
			/* No free space for this aircraft. */
			return 0;
		}
	}
}
