/**
 * @file cp_mission_baseattack.c
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
#include "../../../ui/ui_main.h" /* TEXT_POPUP */
#include "../../../ui/ui_popup.h" /* popupText */
#include "../cp_campaign.h"
#include "../cp_capacity.h"
#include "../cp_map.h"
#include "../cp_ufo.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_alien_interest.h"

/**
 * @brief This fake aircraft is used to assign soldiers for a base attack mission
 * @sa CP_BaseAttackStartMission
 * @sa AIR_AddToAircraftTeam
 */
static aircraft_t baseAttackFakeAircraft;

/**
 * @brief Base attack mission is over and is a success (from an alien point of view): change interest values.
 * @note Base attack mission
 * @sa CP_BaseAttackMissionStart
 */
void CP_BaseAttackMissionIsSuccess (mission_t *mission)
{
	INT_ChangeIndividualInterest(0.3f, INTERESTCATEGORY_RECON);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);
	INT_ChangeIndividualInterest(-0.5f, INTERESTCATEGORY_TERROR_ATTACK);
	INT_ChangeIndividualInterest(-0.5f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Base attack mission is over and is a failure (from an alien point of view): change interest values.
 */
void CP_BaseAttackMissionIsFailure (mission_t *mission)
{
	base_t *base = mission->data.base;

	if (base)
		base->baseStatus = BASE_WORKING;
	ccs.mapAction = MA_NONE;

	/* we really don't want to use the fake aircraft anywhere */
	if (base)
		base->aircraftCurrent = AIR_GetFirstFromBase(base);
	MAP_SetMissionAircraft(NULL);

	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_BASE_ATTACK);

	/* reset current selected mission */
	MAP_NotifyMissionRemoved(mission);

	CP_MissionRemove(mission);
}

/**
 * @brief Run when the mission is spawned.
 */
void CP_BaseAttackMissionOnSpawn (void)
{
	/* Prevent multiple bases from being attacked. by resetting interest. */
	INT_ChangeIndividualInterest(-1.0f, INTERESTCATEGORY_BASE_ATTACK);
}

/**
 * @brief Base attack mission ends: UFO leave earth.
 * @note Base attack mission -- Stage 3
 */
void CP_BaseAttackMissionLeave (mission_t *mission)
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
}

/**
 * @brief Base attack mission ends: UFO leave earth.
 * @note Base attack mission -- Stage 3
 * @note UFO attacking this base will be redirected when notify function will be called, don't set new destination here.
 */
void CP_BaseAttackMissionDestroyBase (mission_t *mission)
{
	base_t *base = mission->data.base;
	assert(base);
	/* Base attack is over, alien won */
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Your base: %s has been destroyed! All employees killed and all equipment destroyed."), base->name);
	MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
	B_Destroy(base);
	CP_GameTimeStop();

	/* we really don't want to use the fake aircraft anywhere */
	MAP_SetMissionAircraft(NULL);

	/* HACK This hack is only needed until base will be really destroyed
	 * we must recalculate items in storage because of the items we collected on battlefield */
	CAP_UpdateStorageCap(base);
	base->aircraftCurrent = NULL;
	base->baseStatus = BASE_WORKING;
}

/**
 * @brief Start Base Attack.
 * @note Base attack mission -- Stage 2
 */
void CP_BaseAttackStartMission (mission_t *mission)
{
	base_t *base = mission->data.base;
	linkedList_t *hiredSoldiersInBase = NULL;
	int soldiers;

	assert(base);

	mission->stage = STAGE_BASE_ATTACK;

	CP_MissionDisableTimeLimit(mission);

	if (mission->ufo) {
		/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
		CP_UFORemoveFromGeoscape(mission, qfalse);
	}

	/* we always need at least one command centre in the base - because the
	 * phalanx soldiers have their starting positions here.
	 * There should also always be an entrance - the aliens start there
	 * but we don't need to check that as entrance can't be destroyed */
	if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, B_COMMAND)) {
		/** @todo handle command centre properly */
		Com_DPrintf(DEBUG_CLIENT, "CP_BaseAttackStartMission: Base '%s' has no Command Center: it can't defend itself. Destroy base.\n", base->name);
		CP_BaseAttackMissionDestroyBase(mission);
		return;
	}

	MSO_CheckAddNewMessage(NT_BASE_ATTACK, _("Base attack"), va(_("Base '%s' is under attack!"), base->name), qfalse, MSG_BASEATTACK, NULL);

	base->baseStatus = BASE_UNDER_ATTACK;
	ccs.campaignStats.basesAttacked++;

	MAP_SelectMission(mission);
	mission->active = qtrue;
	ccs.mapAction = MA_BASEATTACK;
	Com_DPrintf(DEBUG_CLIENT, "Base attack: %s at %.0f:%.0f\n", mission->id, mission->pos[0], mission->pos[1]);

	/** @todo EMPL_ROBOT */
	E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredSoldiersInBase);

	/* Fill the fake aircraft */
	LIST_Delete(&baseAttackFakeAircraft.acTeam);
	OBJZERO(baseAttackFakeAircraft);
	baseAttackFakeAircraft.homebase = base;
	/* needed for transfer of alien corpses */
	VectorCopy(base->pos, baseAttackFakeAircraft.pos);
#if 0
	/** @todo active this once more than 8 soldiers are working */
	/* needed to spawn soldiers on map */
	baseAttackFakeAircraft.maxTeamSize = LIST_Count(hiredSoldiersInBase);
#else
	baseAttackFakeAircraft.maxTeamSize = MAX_ACTIVETEAM;
#endif

	if (!hiredSoldiersInBase) {
		Com_DPrintf(DEBUG_CLIENT, "CP_BaseAttackStartMission: Base '%s' has no soldiers: it can't defend itself. Destroy base.\n", base->name);
		CP_BaseAttackMissionDestroyBase(mission);
		return;
	}

	UI_ExecuteConfunc("soldierlist_clear");
	soldiers = 0;
	LIST_Foreach(hiredSoldiersInBase, employee_t, employee) {
		const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);

		if (E_IsAwayFromBase(employee))
			continue;
		UI_ExecuteConfunc("soldierlist_add %d \"%s %s\"", employee->chr.ucn, (rank) ? _(rank->shortname) : "", employee->chr.name);
		if (soldiers < 1)
			UI_ExecuteConfunc("team_select_ucn %d", employee->chr.ucn);
		soldiers++;
	}
	if (soldiers == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CP_BaseAttackStartMission: Base '%s' has no soldiers at home: it can't defend itself. Destroy base.\n", base->name);
		CP_BaseAttackMissionDestroyBase(mission);
		return;
	}
#if 0
	/** @todo active this once more than 8 soldiers are working */
	/* all soldiers in the base should get used */
	baseAttackFakeAircraft.maxTeamSize = AIR_GetTeamSize(&baseAttackFakeAircraft);
#endif

	LIST_Delete(&hiredSoldiersInBase);
	base->aircraftCurrent = &baseAttackFakeAircraft;
	MAP_SetMissionAircraft(&baseAttackFakeAircraft);
	/** @todo remove me - this is not needed because we are using the base->aircraftCurrent
	 * pointer for resolving the aircraft - only CP_GameAutoGo needs this */
	MAP_SetInterceptorAircraft(&baseAttackFakeAircraft);	/* needed for updating soldier stats sa CP_UpdateCharacterStats*/
	B_SetCurrentSelectedBase(base);						/* needed for equipment menu */

	Com_sprintf(popupText, sizeof(popupText), _("Base '%s' is under attack! What to do ?"), base->name);
	UI_RegisterText(TEXT_POPUP, popupText);

	CP_GameTimeStop();
	UI_PushWindow("popup_baseattack", NULL, NULL);
}


/**
 * @brief Check and start baseattack missions
 * @sa CP_BaseAttackStartMission
 */
void CP_CheckBaseAttacks (void)
{
	MIS_Foreach(mission) {
		if (mission->category == INTERESTCATEGORY_BASE_ATTACK && mission->stage == STAGE_BASE_ATTACK)
			CP_BaseAttackStartMission(mission);
	}
}

/**
 * @brief Choose Base that will be attacked
 * @return Pointer to the base, NULL if no base set
 * @note Base attack mission -- Stage 1
 */
static base_t *CP_BaseAttackChooseBase (void)
{
	float randomNumber;
	float sum = 0.0f;
	base_t *base = NULL;

	/* Choose randomly a base depending on alienInterest values for those bases */
	while ((base = B_GetNext(base)) != NULL)
		sum += base->alienInterest;
	randomNumber = frand() * sum;
	while ((base = B_GetNext(base)) != NULL) {
		randomNumber -= base->alienInterest;
		if (randomNumber < 0)
			break;
	}

	/* Make sure we have a base */
	assert(base && (randomNumber < 0));

	/* base is already under attack */
	if (B_IsUnderAttack(base))
		return NULL;
	/* base not (yet) working */
	if (!B_GetBuildingStatus(base, B_COMMAND))
		return NULL;

	return base;
}

/**
 * @brief Set base attack mission, and go to base position.
 * @note Base attack mission -- Stage 1
 */
static void CP_BaseAttackGoToBase (mission_t *mission)
{
	base_t *base;

	mission->stage = STAGE_MISSION_GOTO;

	base = CP_BaseAttackChooseBase();
	if (!base) {
		Com_Printf("CP_BaseAttackGoToBase: no base found\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data.base = base;

	mission->mapDef = Com_GetMapDefinitionByID("baseattack");
	if (!mission->mapDef) {
		CP_MissionRemove(mission);
		Com_Error(ERR_DROP, "Could not find mapdef baseattack");
		return;
	}

	Vector2Copy(base->pos, mission->pos);
	mission->posAssigned = qtrue;

	Com_sprintf(mission->location, sizeof(mission->location), "%s", base->name);

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SendToDestination(mission->ufo, mission->pos);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}

/**
 * @brief Fill an array with available UFOs for Base Attack mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Base Attack mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
int CP_BaseAttackMissionAvailableUFOs (const mission_t const *mission, ufoType_t *ufoTypes)
{
	int num = 0;
	ufoTypes[num++] = UFO_FIGHTER;
	if (UFO_ShouldAppearOnGeoscape(UFO_BOMBER))
		ufoTypes[num++] = UFO_BOMBER;

	return num;
}

/**
 * @brief Determine what action should be performed when a Base Attack mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_BaseAttackMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create mission */
		CP_MissionBegin(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Choose a base to attack and go to this base */
		CP_BaseAttackGoToBase(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on base location: attack it */
		CP_BaseAttackStartMission(mission);
		break;
	case STAGE_BASE_ATTACK:
		/* Leave earth */
		CP_BaseAttackMissionDestroyBase(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_BaseAttackMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_BaseAttackMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}
