/**
 * @file cp_aircraft_callbacks.c
 * @brief Menu related console command callbacks
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

#include "../client.h"
#include "../cl_global.h"
#include "../menu/m_popup.h"
#include "cl_map.h"
#include "cp_aircraft_callbacks.h"
#include "cp_aircraft.h"

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa GAME_CP_MissionAutoGo_f
 * @sa GAME_CP_Results_f
 */
void AIM_AircraftReturnToBase_f (void)
{
	if (baseCurrent && baseCurrent->aircraftCurrent) {
		AIR_AircraftReturnToBase(baseCurrent->aircraftCurrent);
		AIR_AircraftSelect(baseCurrent->aircraftCurrent);
	}
}


/**
 * @brief Restores aircraft cvars after going back from aircraft buy menu.
 * @sa BS_MarketAircraftDescription
 */
void AIM_ResetAircraftCvars_f (void)
{
	if (!baseCurrent || baseCurrent->numAircraftInBase < 0)
		return;

	/* Maybe we sold displayed aircraft ? */
	if (baseCurrent->numAircraftInBase == 0) {
		/* no more aircraft in base */
		Cbuf_AddText("mn_pop\n");
		return;
	}

	AIR_AircraftSelect(baseCurrent->aircraftCurrent);
}

/**
 * @brief Switch to next aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 */
void AIM_NextAircraft_f (void)
{
	base_t *base = baseCurrent;

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[base->numAircraftInBase - 1])
		base->aircraftCurrent = &base->aircraft[0];
	else
		base->aircraftCurrent++;

	AIR_AircraftSelect(base->aircraftCurrent);
}

/**
 * @brief Switch to previous aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_NextAircraft_f
 */
void AIM_PrevAircraft_f (void)
{
	base_t *base = baseCurrent;

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[0])
		base->aircraftCurrent = &base->aircraft[base->numAircraftInBase - 1];
	else
		base->aircraftCurrent--;

	AIR_AircraftSelect(base->aircraftCurrent);
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
		Com_Printf("Usage: %s <type> <baseIdx>\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 3)
		i = atoi(Cmd_Argv(2));

	if (!baseCurrent || (i >= 0)) {
		if (i < 0 || i >= MAX_BASES)
			return;

		if (gd.bases[i].founded)
			b = B_GetBaseByIDX(i);
	} else
		b = baseCurrent;

	if (b)
		AIR_NewAircraft(b, Cmd_Argv(1));
}

/**
 * @brief Starts an aircraft or stops the current mission and let the aircraft idle around.
 */
void AIM_AircraftStart_f (void)
{
	aircraft_t *aircraft;

	if (!baseCurrent)
		return;

	if (!baseCurrent->aircraftCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "Error - there is no current aircraft in this base\n");
		return;
	}

	/* Aircraft cannot start without Command Centre operational. */
	if (!B_GetBuildingStatus(baseCurrent, B_COMMAND)) {
		MN_Popup(_("Notice"), _("No operational Command Centre in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft = baseCurrent->aircraftCurrent;

	/* Aircraft cannot start without a pilot. */
	if (!aircraft->pilot) {
		MN_Popup(_("Notice"), _("There is no pilot assigned to this aircraft.\n\nAircraft can not start.\n"));
		return;
	}

	if (AIR_IsAircraftInBase(aircraft)) {
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
