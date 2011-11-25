/**
 * @file cp_mission_recon.c
 * @brief Campaign mission
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
#include "../cp_alienbase.h"
#include "../cp_map.h"
#include "../cp_ufo.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_xvi.h"
#include "../cp_alien_interest.h"

/**
 * @brief Recon mission is over and is a success: change interest values.
 * @note Recon mission
 */
static void CP_ReconMissionIsSuccess (mission_t *mission)
{
	INT_ChangeIndividualInterest(-0.2f, INTERESTCATEGORY_RECON);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);
	if (AB_GetAlienBaseNumber())
		INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_SUPPLY);
	if (CP_IsXVIResearched())
		INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_XVI);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission is over and is a failure: change interest values.
 * @note Recon mission
 */
void CP_ReconMissionIsFailure (mission_t *mission)
{
	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_RECON);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_TERROR_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission ends: UFO leave earth.
 * @note Recon mission -- Stage 2
 */
void CP_ReconMissionLeave (mission_t *mission)
{
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
 * @brief Choose between aerial and ground mission.
 * @note Recon mission -- Stage 1
 * @return qtrue if recon mission is aerial, qfalse if this is a ground mission
 * @sa CP_ReconMissionSelect
 */
static qboolean CP_ReconMissionChoose (mission_t *mission)
{
	/* mission without UFO is always a ground mission */
	if (!mission->ufo)
		return qfalse;

	return (frand() > 0.5f);
}

/**
 * @brief Set aerial mission.
 * @note Recon mission -- Stage 1
 * @sa CP_ReconMissionSelect
 */
void CP_ReconMissionAerial (mission_t *mission)
{
	const date_t minReconDelay = {1, 0};
	const date_t reconDelay = {2, 0};		/* How long the UFO will fly on earth */

	assert(mission->ufo);

	mission->stage = STAGE_RECON_AIR;

	mission->finalDate = Date_Add(ccs.date, Date_Random(minReconDelay, reconDelay));
}

/**
 * @brief Set ground mission, and go to ground mission pos.
 * @note Recon mission -- Stage 1
 * @note ground mission can be spawned without UFO
 * @sa CP_ReconMissionSelect
 */
void CP_ReconMissionGroundGo (mission_t *mission)
{
	const nation_t *nation;

	mission->stage = STAGE_MISSION_GOTO;

	/* maybe the UFO just finished a ground mission and starts a new one? */
	if (mission->ufo) {
		CP_MissionRemoveFromGeoscape(mission);
		mission->ufo->landed = qfalse;
	}

	/* Choose a map */
	if (CP_ChooseMap(mission, NULL)) {
		int counter;
		for (counter = 0; counter < MAX_POS_LOOP; counter++) {
			if (!CP_GetRandomPosOnGeoscapeWithParameters(mission->pos, mission->mapDef->terrains, mission->mapDef->cultures, mission->mapDef->populations, NULL))
				continue;
			if (MAP_PositionCloseToBase(mission->pos))
				continue;
			mission->posAssigned = qtrue;
			break;
		}
		if (counter >= MAX_POS_LOOP) {
			Com_Printf("CP_ReconMissionGroundGo: Error, could not set position.\n");
			CP_MissionRemove(mission);
			return;
		}
	} else {
		Com_Printf("CP_ReconMissionGroundGo: No map found, remove mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	nation = MAP_GetNation(mission->pos);
	if (nation) {
		Com_sprintf(mission->location, sizeof(mission->location), "%s", _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), "%s", _("No nation"));
	}

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SendToDestination(mission->ufo, mission->pos);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}

/**
 * @brief Start ground mission.
 * @note Recon mission -- Stage 1
 */
static void CP_ReconMissionGround (mission_t *mission)
{
	const date_t minMissionDelay = {2, 0};
	const date_t missionDelay = {3, 0};

	mission->stage = STAGE_RECON_GROUND;
	mission->posAssigned = qtrue;

	mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	if (mission->ufo)
		CP_UFORemoveFromGeoscape(mission, qfalse);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Choose if a new ground mission should be started.
 * @note Recon mission -- Stage 1
 * @note Already one ground mission has been made
 * @sa CP_ReconMissionSelect
 */
static qboolean CP_ReconMissionNewGroundMission (mission_t *mission)
{
	return (frand() > 0.7f);
}

/**
 * @brief Set recon mission type (aerial or ground).
 * @note Recon mission -- Stage 1
 */
static void CP_ReconMissionSelect (mission_t *mission)
{
	if (mission->stage == STAGE_COME_FROM_ORBIT) {
		/* this is the begining of the mission: choose between aerial or ground mission */
		if (CP_ReconMissionChoose(mission))
			/* This is a aerial mission */
			CP_ReconMissionAerial(mission);
		else
			/* This is a ground mission */
			CP_ReconMissionGroundGo(mission);
	} else if (mission->stage == STAGE_RECON_GROUND) {
		/* Ground mission may occur several times */
		if (CP_ReconMissionNewGroundMission(mission))
			CP_ReconMissionGroundGo(mission);
		else
			CP_ReconMissionLeave(mission);
	}
}

/**
 * @brief Fill an array with available UFOs for recon mission type (aerial or ground).
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Recon mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
int CP_ReconMissionAvailableUFOs (const mission_t const *mission, ufoType_t *ufoTypes)
{
	int num = 0;

	ufoTypes[num++] = UFO_SCOUT;
	ufoTypes[num++] = UFO_FIGHTER;

	return num;
}

/**
 * @brief Determine what action should be performed when a Recon mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_ReconMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create Recon mission */
		CP_MissionBegin(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
	case STAGE_RECON_GROUND:
		/* Choose if a new ground mission should be started */
		CP_ReconMissionSelect(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on a new ground mission: start it */
		CP_ReconMissionGround(mission);
		break;
	case STAGE_RECON_AIR:
		/* Leave earth */
		CP_ReconMissionLeave(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_ReconMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_ReconMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}
