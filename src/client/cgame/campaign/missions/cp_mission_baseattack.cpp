/**
 * @file
 * @brief Campaign mission code
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../../../ui/ui_dataids.h"
#include "../cp_campaign.h"
#include "../cp_capacity.h"
#include "../cp_geoscape.h"
#include "../cp_ufo.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_popup.h"
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
void CP_BaseAttackMissionIsSuccess (mission_t* mission)
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
void CP_BaseAttackMissionIsFailure (mission_t* mission)
{
	base_t* base = mission->data.base;

	if (base) {
		base->baseStatus = BASE_WORKING;

		/* clean up the fakeAircraft */
		cgi->LIST_Delete(&baseAttackFakeAircraft.acTeam);

		base->aircraftCurrent = AIR_GetFirstFromBase(base);
	}
	ccs.mapAction = MA_NONE;

	GEO_SetMissionAircraft(nullptr);

	INT_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	INT_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_BASE_ATTACK);

	/* reset current selected mission */
	GEO_NotifyMissionRemoved(mission);

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
void CP_BaseAttackMissionLeave (mission_t* mission)
{
	mission->stage = STAGE_RETURN_TO_ORBIT;

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SetRandomDest(mission->ufo);
		/* Display UFO on geoscape if it is detected */
		mission->ufo->landed = false;
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
void CP_BaseAttackMissionDestroyBase (mission_t* mission)
{
	base_t* base = mission->data.base;
	assert(base);
	/* Base attack is over, alien won */
	Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Your base: %s has been destroyed! All employees killed and all equipment destroyed."), base->name);
	MS_AddNewMessage(_("Notice"), cp_messageBuffer);

	cgi->LIST_Delete(&baseAttackFakeAircraft.acTeam);
	B_Destroy(base);
	CP_GameTimeStop();

	/* we really don't want to use the fake aircraft anywhere */
	GEO_SetMissionAircraft(nullptr);

	/* HACK This hack is only needed until base will be really destroyed
	 * we must recalculate items in storage because of the items we collected on battlefield */
	CAP_UpdateStorageCap(base);
	base->aircraftCurrent = nullptr;
	base->baseStatus = BASE_WORKING;
}

/**
 * @brief Prepare things for baseattack battle
 * @param[in] mission Mission to prepare battle for
 */
static void CP_BaseAttackPrepareBattle (mission_t* mission)
{
	if (!mission)
		return;

	base_t* base = mission->data.base;

	GEO_SelectMission(mission);
	mission->active = true;
	ccs.mapAction = MA_BASEATTACK;
	Com_DPrintf(DEBUG_CLIENT, "Base attack: %s at %.0f:%.0f\n", mission->id, mission->pos[0], mission->pos[1]);

	/* Fill the fake aircraft */
	OBJZERO(baseAttackFakeAircraft);
	baseAttackFakeAircraft.homebase = base;
	/* needed for transfer of alien corpses */
	VectorCopy(base->pos, baseAttackFakeAircraft.pos);

	/* needed to spawn soldiers on map */
	baseAttackFakeAircraft.maxTeamSize = std::min(MAX_ACTIVETEAM, E_CountByType(EMPL_SOLDIER) + E_CountByType(EMPL_ROBOT));


	base->aircraftCurrent = &baseAttackFakeAircraft;
	GEO_SetMissionAircraft(&baseAttackFakeAircraft);
	/** @todo remove me - this is not needed because we are using the base->aircraftCurrent
	 * pointer for resolving the aircraft - only CP_GameAutoGo needs this */
	GEO_SetInterceptorAircraft(&baseAttackFakeAircraft);	/* needed for updating soldier stats sa CP_UpdateCharacterStats */
	B_SetCurrentSelectedBase(base);						/* needed for equipment menu */

	static char popupText[1024];
	Com_sprintf(popupText, sizeof(popupText), _("Base '%s' is under attack! What to do?"), base->name);
	cgi->UI_RegisterText(TEXT_POPUP, popupText);

	CP_GameTimeStop();
	cgi->UI_PushWindow("popup_baseattack");
}

/**
 * @brief Start Base Attack.
 * @param[in] mission Pointer to the baseattack mission
 */
void CP_BaseAttackStartMission (mission_t* mission)
{
	base_t* base = mission->data.base;
	int soldiers;

	assert(base);
	mission->stage = STAGE_BASE_ATTACK;
	CP_MissionDisableTimeLimit(mission);
	if (mission->ufo) {
		/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
		CP_UFORemoveFromGeoscape(mission, false);
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

	MSO_CheckAddNewMessage(NT_BASE_ATTACK, _("Base attack"), va(_("Base '%s' is under attack!"), base->name), MSG_BASEATTACK);

	base->baseStatus = BASE_UNDER_ATTACK;
	ccs.campaignStats.basesAttacked++;

	soldiers = 0;
	E_Foreach(EMPL_SOLDIER, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		if (employee->isAwayFromBase())
			continue;
		soldiers++;
	}
	if (soldiers == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CP_BaseAttackStartMission: Base '%s' has no soldiers at home: it can't defend itself. Destroy base.\n", base->name);
		CP_BaseAttackMissionDestroyBase(mission);
		return;
	}
	CP_BaseAttackPrepareBattle(mission);
}


/**
 * @brief Check and start baseattack missions
 * @sa CP_BaseAttackStartMission
 */
void CP_CheckBaseAttacks (void)
{
	MIS_Foreach(mission) {
		if (mission->category == INTERESTCATEGORY_BASE_ATTACK && mission->stage == STAGE_BASE_ATTACK)
			CP_BaseAttackPrepareBattle(mission);
	}
}

/**
 * @brief Choose Base that will be attacked
 * @return Pointer to the base, nullptr if no base set
 * @note Base attack mission -- Stage 1
 */
static base_t* CP_BaseAttackChooseBase (void)
{
	float randomNumber;
	float sum = 0.0f;
	base_t* base = nullptr;

	/* Choose randomly a base depending on alienInterest values for those bases */
	while ((base = B_GetNext(base)) != nullptr)
		sum += base->alienInterest;
	randomNumber = frand() * sum;
	while ((base = B_GetNext(base)) != nullptr) {
		randomNumber -= base->alienInterest;
		if (randomNumber < 0)
			break;
	}

	/* Make sure we have a base */
	assert(base && (randomNumber < 0));

	/* base is already under attack */
	if (B_IsUnderAttack(base))
		return nullptr;
	/* base not (yet) working */
	if (!B_GetBuildingStatus(base, B_COMMAND))
		return nullptr;

	return base;
}

/**
 * @brief Set base attack mission, and go to base position.
 * @note Base attack mission -- Stage 1
 */
static void CP_BaseAttackGoToBase (mission_t* mission)
{
	base_t* base;

	mission->stage = STAGE_MISSION_GOTO;

	base = CP_BaseAttackChooseBase();
	if (!base) {
		Com_Printf("CP_BaseAttackGoToBase: no base found\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data.base = base;

	mission->mapDef = cgi->Com_GetMapDefinitionByID("baseattack");
	if (!mission->mapDef) {
		CP_MissionRemove(mission);
		cgi->Com_Error(ERR_DROP, "Could not find mapdef baseattack");
		return;
	}

	Vector2Copy(base->pos, mission->pos);
	mission->posAssigned = true;

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SendToDestination(mission->ufo, mission->pos);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}

/**
 * @brief Determine what action should be performed when a Base Attack mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_BaseAttackMissionNextStage (mission_t* mission)
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
