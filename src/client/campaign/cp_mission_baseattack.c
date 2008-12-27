/**
 * @file cp_mission_baseattack.c
 * @brief Campaign mission code
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
#include "../cl_map.h"
#include "../menu/m_popup.h"
#include "cp_missions.h"
#include "cp_time.h"

/**
 * @brief This fake aircraft is used to assign soldiers for a base attack mission
 * @sa CP_BaseAttackStartMission
 * @sa AIR_AddToAircraftTeam
 */
aircraft_t baseAttackFakeAircraft;

/**
 * @brief Base attack mission is over and is a success (from an alien point of view): change interest values.
 * @note Base attack mission
 * @sa CP_BaseAttackMissionStart
 */
void CP_BaseAttackMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(+0.3f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_HARVEST);

	CP_MissionRemove(mission);
}

/**
 * @brief Base attack mission is over and is a failure (from an alien point of view): change interest values.
 * @note Base attack mission
 */
void CP_BaseAttackMissionIsFailure (mission_t *mission)
{
	base_t *base;

	base = (base_t *)mission->data;

	if (base)
		B_BaseResetStatus(base);
	gd.mapAction = MA_NONE;

	/* we really don't want to use the fake aircraft anywhere */
	cls.missionaircraft = NULL;

	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	/* Restore some alien interest for base attacks that has been removed when
	 * mission has been created */
	CL_ChangeIndividualInterest(0.5f, INTERESTCATEGORY_BASE_ATTACK);

	/* reset selectedMission */
	MAP_NotifyMissionRemoved(mission);

	CP_MissionRemove(mission);
}

/**
 * @brief Base attack mission just started: change interest values.
 * @note This function is intended to avoid attack on several bases at the same time
 */
void CP_BaseAttackMissionStart (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.7f, INTERESTCATEGORY_BASE_ATTACK);
}

/**
 * @brief Base attack mission ends: UFO leave earth.
 * @note Base attack mission -- Stage 3
 */
void CP_BaseAttackMissionLeave (mission_t *mission)
{
	base_t *base;

	mission->stage = STAGE_RETURN_TO_ORBIT;

	base = (base_t *)mission->data;
	assert(base);
	/* Base attack is over, alien won */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Your base: %s has been destroyed! All employees killed and all equipment destroyed."), base->name);
	MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_BaseDestroy(base);
	CL_GameTimeStop();

	/* we really don't want to use the fake aircraft anywhere */
	cls.missionaircraft = NULL;

	/* HACK This hack only needed until base will be really destroyed */
	base->baseStatus = BASE_WORKING;
}

/**
 * @brief Start Base Attack.
 * @note Base attack mission -- Stage 2
 * @todo Base attack should start right away
 * @todo Base attack can't be selected in map anymore: remove all base attack code from cl_map.c
 */
void CP_BaseAttackStartMission (mission_t *mission)
{
	int i;
	base_t *base = (base_t *)mission->data;
	linkedList_t *hiredSoldiersInBase = NULL, *pos;

	assert(base);

	mission->stage = STAGE_BASE_ATTACK;

	CP_MissionDisableTimeLimit(mission);

	if (mission->ufo) {
		/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
		CP_UFORemoveFromGeoscape(mission, qfalse);
	}

	/* we always need at least one command centre in the base - because the
	 * phalanx soldiers have their starting positions here
	 * @note There should also always be an entrance - the aliens start there */
	if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, B_COMMAND)) {
		/** @todo handle command centre properly */
		Com_Printf("CP_BaseAttackStartMission: This base (%s) can not be set under attack - because there are no Command Center in this base\n", base->name);
		CP_BaseAttackMissionLeave(mission);
		return;
	}

	base->baseStatus = BASE_UNDER_ATTACK;
	campaignStats.basesAttacked++;

#if 0
	/** @todo implement onattack: add it to basemanagement.ufo and implement functions */
	if (base->onAttack[0] != '\0')
		/* execute next frame */
		Cbuf_AddText(va("%s %i", base->onAttack, base->id));
#endif

	MAP_SelectMission(mission);
	selectedMission->active = qtrue;
	gd.mapAction = MA_BASEATTACK;
	Com_DPrintf(DEBUG_CLIENT, "Base attack: %s at %.0f:%.0f\n", selectedMission->id, selectedMission->pos[0], selectedMission->pos[1]);

	/* Fill the fake aircraft */
	memset(&baseAttackFakeAircraft, 0, sizeof(baseAttackFakeAircraft));
	baseAttackFakeAircraft.homebase = base;
	VectorCopy(base->pos, baseAttackFakeAircraft.pos);				/* needed for transfer of alien corpses */
	/** @todo EMPL_ROBOT */
	baseAttackFakeAircraft.maxTeamSize = MAX_ACTIVETEAM;			/* needed to spawn soldiers on map */
	E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredSoldiersInBase);
	for (i = 0, pos = hiredSoldiersInBase; i < MAX_ACTIVETEAM && pos; i++, pos = pos->next)
		AIR_AddToAircraftTeam(&baseAttackFakeAircraft, (employee_t *)pos->data);

	LIST_Delete(&hiredSoldiersInBase);
	cls.missionaircraft = &baseAttackFakeAircraft;
	gd.interceptAircraft = &baseAttackFakeAircraft; /* needed for updating soldier stats sa CL_UpdateCharacterStats*/

	if (base->capacities[CAP_ALIENS].cur) {
		Com_sprintf(popupText, sizeof(popupText), _("Base '%s' is under attack - you can enter this base to change soldiers equipment. What to do ?"), base->name);
	} else {
		Com_sprintf(popupText, sizeof(popupText), _("Base '%s' is under attack - you can enter this base to change soldiers equipment or to kill aliens in Alien Containment Facility. What to do ?"), base->name);
	}
	mn.menuText[TEXT_POPUP] = popupText;

	CL_GameTimeStop();
	B_SelectBase(base);
	MN_PopMenu(qfalse);
	MN_PushMenu("popup_baseattack", NULL);
}

