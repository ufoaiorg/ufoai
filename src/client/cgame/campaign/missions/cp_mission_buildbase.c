/**
 * @file cp_mission_buildbase.c
 * @brief Campaign mission code for alien bases
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

/** Overall alien interest value for starting constructing alien bases. */
const int STARTING_BASEBUILD_INTEREST = 300;

/**
 * This mission type has no alienbase set
 * @param mission The mission to check
 * @return
 */
qboolean CP_BasemissionIsSubvertingGovernmentMission (const mission_t *mission)
{
	return mission->initialOverallInterest < STARTING_BASEBUILD_INTEREST;
}

/**
 * @brief Build Base mission is over and is a success (from an alien point of view): change interest values.
 * @note Build Base mission
 */
void CP_BuildBaseMissionIsSuccess (mission_t *mission)
{
	if (CP_BasemissionIsSubvertingGovernmentMission(mission)) {
		/* This is a subverting government mission */
		INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	} else {
		/* An alien base has been built */
		const alienBase_t *base = mission->data.alienBase;
		assert(base);
		CP_SpreadXVIAtPos(base->pos);

		INT_ChangeIndividualInterest(0.4f, INTERESTCATEGORY_XVI);
		INT_ChangeIndividualInterest(0.4f, INTERESTCATEGORY_SUPPLY);
		INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);
	}

	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission is over and is a failure (from an alien point of view): change interest values.
 * @note Build Base mission
 */
void CP_BuildBaseMissionIsFailure (mission_t *mission)
{
	/* Restore some alien interest for build base that has been removed when mission has been created */
	INT_ChangeIndividualInterest(0.5f, INTERESTCATEGORY_BUILDING);
	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Run when the mission is spawned.
 */
void CP_BuildBaseMissionOnSpawn (void)
{
	INT_ChangeIndividualInterest(-0.7f, INTERESTCATEGORY_BUILDING);
}

/**
 * @brief Alien base has been destroyed: change interest values.
 * @note Build Base mission
 */
void CP_BuildBaseMissionBaseDestroyed (mission_t *mission)
{
	/* An alien base has been built */
	alienBase_t *base = mission->data.alienBase;
	assert(base);

	INT_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_BUILDING);
	INT_ChangeIndividualInterest(+0.3f, INTERESTCATEGORY_INTERCEPT);

	AB_DestroyBase(base);
	mission->data.alienBase = NULL;
	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission ends: UFO leave earth.
 * @note Build Base mission -- Stage 3
 */
static void CP_BuildBaseMissionLeave (mission_t *mission)
{
	assert(mission->ufo);
	/* there must be an alien base set */
	assert(mission->data.alienBase);

	mission->stage = STAGE_RETURN_TO_ORBIT;

	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	/* Display UFO on geoscape if it is detected */
	mission->ufo->landed = qfalse;
}

/**
 * @brief UFO arrived on new base destination: build base.
 * @param[in,out] mission Pointer to the mission
 * @note Build Base mission -- Stage 2
 */
static void CP_BuildBaseSetUpBase (mission_t *mission)
{
	alienBase_t *base;
	const date_t minBuildingTime = {5, 0};	/**< Minimum time needed to start a new base construction */
	const date_t buildingTime = {10, 0};	/**< Maximum time needed to start a new base construction */

	assert(mission->ufo);

	mission->stage = STAGE_BUILD_BASE;

	mission->finalDate = Date_Add(ccs.date, Date_Random(minBuildingTime, buildingTime));

	base = AB_BuildBase(mission->pos);
	if (!base) {
		Com_DPrintf(DEBUG_CLIENT, "CP_BuildBaseSetUpBase: could not create alien base\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data.alienBase = base;

	/* ufo becomes invisible on geoscape */
	CP_UFORemoveFromGeoscape(mission, qfalse);
}

/**
 * @brief Go to new base position.
 * @param[in,out] mission Pointer to the mission
 * @note Build Base mission -- Stage 1
 */
static void CP_BuildBaseGoToBase (mission_t *mission)
{
	assert(mission->ufo);

	mission->stage = STAGE_MISSION_GOTO;

	AB_SetAlienBasePosition(mission->pos);

	UFO_SendToDestination(mission->ufo, mission->pos);
}

/**
 * @brief Subverting Mission ends: UFO leave earth.
 * @note Build Base mission -- Stage 3
 */
static void CP_BuildBaseGovernmentLeave (const campaign_t *campaign, mission_t *mission)
{
	nation_t *nation;

	assert(mission);
	assert(mission->ufo);

	mission->stage = STAGE_RETURN_TO_ORBIT;

	/* Mission is a success: government is subverted => lower happiness */
	nation = MAP_GetNation(mission->pos);
	/** @todo when the mission is created, we should select a position where nation exists,
	 * otherwise subverting a government is meaningless */
	if (nation) {
		const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
		NAT_SetHappiness(campaign->minhappiness, nation, stats->happiness + HAPPINESS_SUBVERSION_LOSS);
	}

	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	/* Display UFO on geoscape if it is detected */
	mission->ufo->landed = qfalse;
}

/**
 * @brief Start Subverting Mission.
 * @note Build Base mission -- Stage 2
 */
static void CP_BuildBaseSubvertGovernment (mission_t *mission)
{
	const date_t minMissionDelay = {3, 0};
	const date_t missionDelay = {5, 0};

	assert(mission->ufo);

	mission->stage = STAGE_SUBVERT_GOV;

	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);

	mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission, qfalse);
}

/**
 * @brief Choose if the mission should be an alien infiltration or a build base mission.
 * @note Build Base mission -- Stage 1
 */
static void CP_BuildBaseChooseMission (mission_t *mission)
{
	if (CP_BasemissionIsSubvertingGovernmentMission(mission))
		CP_ReconMissionGroundGo(mission);
	else
		CP_BuildBaseGoToBase(mission);
}

/**
 * @brief Fill an array with available UFOs for build base mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Base Attack mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
int CP_BuildBaseMissionAvailableUFOs (const mission_t const *mission, ufoType_t *ufoTypes)
{
	int num = 0;

	if (CP_BasemissionIsSubvertingGovernmentMission(mission)) {
		/* This is a subverting government mission */
		ufoTypes[num++] = UFO_SCOUT;
	} else {
		/* This is a Building base mission */
		ufoTypes[num++] = UFO_SUPPLY;
	}

	return num;
}

/**
 * @brief Determine what action should be performed when a Build Base mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_BuildBaseMissionNextStage (const campaign_t *campaign, mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create mission */
		CP_MissionBegin(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Choose type of mission */
		CP_BuildBaseChooseMission(mission);
		break;
	case STAGE_MISSION_GOTO:
		if (CP_BasemissionIsSubvertingGovernmentMission(mission))
			/* subverting mission */
			CP_BuildBaseSubvertGovernment(mission);
		else
			/* just arrived on base location: build base */
			CP_BuildBaseSetUpBase(mission);
		break;
	case STAGE_BUILD_BASE:
		/* Leave earth */
		CP_BuildBaseMissionLeave(mission);
		break;
	case STAGE_SUBVERT_GOV:
		/* Leave earth */
		CP_BuildBaseGovernmentLeave(campaign, mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_BuildBaseMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_BuildBaseMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}
