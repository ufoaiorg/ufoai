/**
 * @file cp_mission_supply.c
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
#include "../cp_ufo.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_xvi.h"
#include "../cp_alien_interest.h"

/**
 * @brief Supply mission is over and is a success (from an alien point of view): change interest values.
 * @note Supply mission
 */
void CP_SupplyMissionIsSuccess (mission_t *mission)
{
	alienBase_t *base;
	INT_ChangeIndividualInterest(-0.2f, INTERESTCATEGORY_SUPPLY);

	/* Spread XVI */
	base = mission->data.alienBase;
	assert(base);
	CP_SpreadXVIAtPos(base->pos);

	CP_MissionRemove(mission);
}

/**
 * @brief Supply mission is over and is a failure (from an alien point of view): change interest values.
 * @note Supply mission
 */
void CP_SupplyMissionIsFailure (mission_t *mission)
{
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_SUPPLY);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Supply mission ends: UFO leave earth.
 * @note Supply mission -- Stage 3
 */
static void CP_SupplyMissionLeave (mission_t *mission)
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
 * @brief UFO arrived on new base destination: Supply base.
 * @param[in,out] mission Pointer to the mission
 * @note Supply mission -- Stage 2
 */
static void CP_SupplySetStayAtBase (mission_t *mission)
{
	const date_t minSupplyTime = {3, 0};
	const date_t supplyTime = {10, 0};	/**< Max time needed to supply base */

	assert(mission->ufo);
	/* there must be an alien base set */
	assert(mission->data.alienBase);

	mission->stage = STAGE_SUPPLY;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->finalDate = Date_Add(ccs.date, Date_Random(minSupplyTime, supplyTime));

	AB_SupplyBase(mission->data.alienBase, mission->ufo->detected);

	/* ufo becomes invisible on geoscape */
	CP_UFORemoveFromGeoscape(mission, qfalse);
}

/**
 * @brief Go to base position.
 * @param[in,out] mission Pointer to the mission
 * @note Supply mission -- Stage 1
 */
static void CP_SupplyGoToBase (mission_t *mission)
{
	alienBase_t *alienBase;

	assert(mission->ufo);

	mission->stage = STAGE_MISSION_GOTO;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	alienBase = AB_ChooseBaseToSupply();
	assert(alienBase);
	mission->data.alienBase = alienBase;
	Vector2Copy(alienBase->pos, mission->pos);

	UFO_SendToDestination(mission->ufo, mission->pos);
}

/**
 * @brief Supply mission begins: UFO arrive on earth.
 * @note Supply mission -- Stage 0
 */
static void CP_SupplyMissionCreate (mission_t *mission)
{
	int ufoType;

	mission->stage = STAGE_COME_FROM_ORBIT;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	ufoType = CP_MissionChooseUFO(mission);
	if (ufoType == UFO_MAX) {
		Com_DPrintf(DEBUG_CLIENT, "Supply mission can't be spawned without UFO: removing supply mission.\n");
		CP_MissionRemove(mission);
	} else {
		CP_MissionDisableTimeLimit(mission);
		mission->ufo = UFO_AddToGeoscape(ufoType, NULL, mission);
		if (!mission->ufo) {
			Com_Printf("CP_SupplyMissionCreate: Could not add UFO '%s', remove mission\n", Com_UFOTypeToShortName(ufoType));
			CP_MissionRemove(mission);
		}
	}
}

/**
 * @brief Fill an array with available UFOs for supply mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Supply mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
int CP_SupplyMissionAvailableUFOs (const mission_t const *mission, ufoType_t *ufoTypes)
{
	int num = 0;

	ufoTypes[num++] = UFO_SUPPLY;

	return num;
}

/**
 * @brief Determine what action should be performed when a Supply mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_SupplyMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create mission */
		CP_SupplyMissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Go to base position */
		CP_SupplyGoToBase(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on base location: Supply base */
		CP_SupplySetStayAtBase(mission);
		break;
	case STAGE_SUPPLY:
		/* Leave earth */
		CP_SupplyMissionLeave(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_SupplyMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_SupplyMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}
