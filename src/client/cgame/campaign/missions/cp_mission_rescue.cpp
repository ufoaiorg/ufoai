/**
 * @file cp_mission_rescue.c
 * @brief Campaign mission code
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

#include "../../../cl_shared.h"
#include "../cp_campaign.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_ufo.h"

/**
 * @brief Actions to be done when UFO arrived at rescue mission location
 * @param[in,out] mission Pointer to the finished mission
 */
static void CP_BeginRescueMission (mission_t *mission)
{
	/* How long the rescue mission will stay before aliens leave / die */
	const date_t minCrashDelay = {7, 0};
	const date_t crashDelay = {14, 0};

	assert(mission->ufo);
	mission->posAssigned = qtrue;

	mission->mapDef = Com_GetMapDefinitionByID("rescue");
	if (!mission->mapDef) {
		CP_MissionRemove(mission);
		Com_Error(ERR_DROP, "Could not find mapdef rescue");
		return;
	}

	mission->ufo->landed = qtrue;
	mission->stage = STAGE_RECON_GROUND;
	mission->finalDate = Date_Add(ccs.date, Date_Random(minCrashDelay, crashDelay));
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Actions to be done when rescue mission finished/expired
 * @param[in,out] mission Pointer to the finished mission
 * @param[in,out] aircraft Pointer to the dropship done the mission
 * @param[in] won Boolean flag if thew mission was successful (from PHALANX's PoV)
 */
void CP_EndRescueMission (mission_t *mission, aircraft_t *aircraft, qboolean won)
{
	aircraft_t *crashedAircraft = mission->data.aircraft;

	assert(crashedAircraft);
	if (won) {
		assert(aircraft);
		/* Save the pilot */
		if (crashedAircraft->pilot)
			AIR_RemoveEmployee(crashedAircraft->pilot, crashedAircraft);
		/* Save the soldiers */
		LIST_Foreach(crashedAircraft->acTeam, employee_t, employee) {
			AIR_RemoveEmployee(employee, crashedAircraft);
		}

		/* return to collect goods and aliens from the crashed aircraft */
		/** @todo it should be dumped to the cargo loot of the dropship done
		 *  the rescue mission not the base directly */
		B_DumpAircraftToHomeBase(crashedAircraft);
	}

	if (won || (CP_CheckMissionLimitedInTime(mission) && Date_LaterThan(&ccs.date, &mission->finalDate)))
		AIR_DestroyAircraft(crashedAircraft);
}

/**
 * @brief Rescue mission expired, UFO leave earth.
 * @param[in,out] mission Pointer to the mission
 */
static void CP_LeaveRescueMission (mission_t *mission)
{
	CP_EndRescueMission(mission, NULL, qfalse);
	mission->stage = STAGE_RETURN_TO_ORBIT;

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SetRandomDest(mission->ufo);
		/* Display UFO on geoscape if it is detected */
		mission->ufo->landed = qfalse;
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
	CP_MissionRemoveFromGeoscape(mission);
}

/**
 * @brief Determine what action should be performed when a Rescue mission stage ends.
 * @param[in,out] mission Pointer to the mission which stage ended.
 */
void CP_RescueNextStage (mission_t *mission)
{
	assert(mission);
	switch (mission->stage) {
	case STAGE_MISSION_GOTO:
		CP_BeginRescueMission(mission);
		break;
	case STAGE_RECON_GROUND:
		CP_LeaveRescueMission(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		CP_MissionRemove(mission);
		break;
	default:
		Com_Printf("CP_RescueNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}
