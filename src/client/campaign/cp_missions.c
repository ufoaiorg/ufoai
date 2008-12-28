/**
 * @file cp_missions.c
 * @brief Campaign missions code
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
#include "cl_map.h"
#include "cl_ufo.h"
#include "cp_missions.h"
#include "cp_time.h"
#include "cp_xvi.h"

/** Maximum number of loops to choose a mission position (to avoid infinite loops) */
const int MAX_POS_LOOP = 10;

/**
 * @brief Set some needed cvars from mission definition
 * @param[in] mission mission definition pointer with the needed data to set the cvars to
 * @sa CL_GameGo
 */
void CP_SetMissionVars (const mission_t *mission)
{
	int i;

	assert(mission->mapDef);

	/* start the map */
	Cvar_SetValue("ai_numaliens", (float) ccs.battleParameters.aliens);
	Cvar_SetValue("ai_numcivilians", (float) ccs.battleParameters.civilians);
	Cvar_Set("ai_civilian", ccs.battleParameters.civTeam);
	Cvar_Set("ai_equipment", ccs.battleParameters.alienEquipment);

	/* now store the alien teams in the shared csi struct to let the game dll
	 * have access to this data, too */
	for (i = 0; i < MAX_TEAMS_PER_MISSION; i++)
		csi.alienTeams[i] = ccs.battleParameters.alienTeams[i];
	csi.numAlienTeams = ccs.battleParameters.numAlienTeams;

	Com_DPrintf(DEBUG_CLIENT, "..numAliens: %i\n..numCivilians: %i\n..alienTeams: '%i'\n..civTeam: '%s'\n..alienEquip: '%s'\n",
		ccs.battleParameters.aliens,
		ccs.battleParameters.civilians,
		ccs.battleParameters.numAlienTeams,
		ccs.battleParameters.civTeam,
		ccs.battleParameters.alienEquipment);
}

/**
 * @brief Select the mission type and start the map from mission definition
 * @param[in] mission Mission definition to start the map from
 * @sa CL_GameGo
 * @note Also sets the terrain textures
 * @sa Mod_LoadTexinfo
 * @sa B_AssembleMap_f
 */
void CP_StartMissionMap (mission_t* mission)
{
	char expanded[MAX_QPATH];
	base_t *bAttack;
	const char *param = NULL;

	/* prepare */
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main_afterdrop", "singleplayermission");

	assert(mission->mapDef->map);

	/** @note set the mapZone - this allows us to replace the ground texture
	 * with the suitable terrain texture - just use tex_terrain/dummy for the
	 * brushes you want the terrain textures on
	 * @sa Mod_LoadTexinfo */
	refdef.mapZone = ccs.battleParameters.zoneType;

	switch (mission->mapDef->map[0]) {
	case '+':
		Com_sprintf(expanded, sizeof(expanded), "maps/%s.ump", mission->mapDef->map + 1);
		break;
	/* base attack maps starts with a dot */
	case '.':
		bAttack = (base_t*)mission->data;
		if (!bAttack) {
			/* assemble a random base and set the base status to BASE_UNDER_ATTACK */
			Cbuf_AddText("base_assemble_rand 1;");
			return;
		} else if (bAttack->baseStatus != BASE_UNDER_ATTACK) {
			Com_Printf("Base is not under attack\n");
			return;
		}
		/* check whether there are founded bases */
		if (B_GetFoundedBaseCount() > 0)
			Cbuf_AddText(va("base_assemble %i 1;", bAttack->idx));
		/* quick save is called when base is really assembled
		 * @sa B_AssembleMap_f */
		return;
	default:
		Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", mission->mapDef->map);
		break;
	}

	SAV_QuickSave();

	if (ccs.battleParameters.param)
		param = ccs.battleParameters.param;
	else
		param = mission->mapDef->param;

	Cbuf_AddText(va("map %s %s %s\n", (MAP_IsNight(mission->pos) ? "night" : "day"),
		mission->mapDef->map, param ? param : ""));

	/* let the (local) server know which map we are running right now */
	csi.currentMD = mission->mapDef;
}

/**
 * @brief Get a mission in ccs.missions by Id.
 */
mission_t* CP_GetMissionById (const char *missionId)
{
	const linkedList_t *list = ccs.missions;

	for (;list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (!Q_strncmp(mission->id, missionId, MAX_VAR))
			return mission;
	}

	Com_Printf("CP_GetMissionById: Could not find mission %s\n", missionId);
	return NULL;
}

/**
 * @brief Get last mission added to ccs.missions.
 * @return Last mission added to ccs.missions, NULL if no mission.
 */
mission_t* CP_GetLastMissionAdded (void)
{
	const linkedList_t *list = ccs.missions;
	mission_t *mission = NULL;

	for (;list; list = list->next)
		mission = (mission_t *)list->data;

	return mission;
}

/**
 * @brief Return the type of mission.
 */
const char* CP_MissionToTypeString (const mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_RECON_GROUND:
	case STAGE_SPREAD_XVI:
		return _("Landed UFO");
	case STAGE_TERROR_MISSION:
		return _("Terror mission");
	case STAGE_BASE_ATTACK:
		return _("Base attack");
	case STAGE_BASE_DISCOVERED:
		return _("Alien base");
	case STAGE_HARVEST:
		return _("Harvesting mission");
	default:
		return _("Crashed UFO");
	}
}

#ifdef DEBUG
/**
 * @brief Return Name of the category of a mission.
 */
static const char* CP_MissionCategoryToName (interestCategory_t category)
{
	switch (category) {
	case INTERESTCATEGORY_NONE:
		return "None";
	case INTERESTCATEGORY_RECON:
		return "Recon Mission";
	case INTERESTCATEGORY_TERROR_ATTACK:
		return "Terror mission";
	case INTERESTCATEGORY_BASE_ATTACK:
		return "Base attack";
	case INTERESTCATEGORY_BUILDING:
		return "Building Base or Subverting Government";
	case INTERESTCATEGORY_SUPPLY:
		return "Supply base";
	case INTERESTCATEGORY_XVI:
		return "XVI propagation";
	case INTERESTCATEGORY_INTERCEPT:
		return "Interception";
	case INTERESTCATEGORY_HARVEST:
		return "Harvest";
	case INTERESTCATEGORY_MAX:
		return "Unknown mission category";
	default:
		Sys_Error("Invalid mission category given");
	}
}

/**
 * @brief Return Name of the category of a mission.
 * @note Not translated yet - only for console usage
 */
static const char* CP_MissionStageToName (const missionStage_t stage)
{
	switch (stage) {
	case STAGE_NOT_ACTIVE:
		return "Not active yet";
	case STAGE_COME_FROM_ORBIT:
		return "UFO coming from orbit";
	case STAGE_RECON_AIR:
		return "Aerial recon underway";
	case STAGE_MISSION_GOTO:
		return "Going to mission position";
	case STAGE_RECON_GROUND:
		return "Ground recon mission underway";
	case STAGE_TERROR_MISSION:
		return "Terror mission underway";
	case STAGE_BUILD_BASE:
		return "Building base";
	case STAGE_BASE_ATTACK:
		return "Attacking a base";
	case STAGE_SUBVERT_GOV:
		return "Subverting a government";
	case STAGE_SUPPLY:
		return "Supplying";
	case STAGE_SPREAD_XVI:
		return "Spreading XVI";
	case STAGE_INTERCEPT:
		return "Intercepting or attacking installation";
	case STAGE_RETURN_TO_ORBIT:
		return "Leaving earth";
	case STAGE_BASE_DISCOVERED:
		return "Base visible";
	case STAGE_HARVEST:
		return "Harvesting";
	case STAGE_OVER:
		return "Mission over";
	default:
		Sys_Error("Unknown state type given: %i", stage);
	}
}
#endif

/**
 * @brief Count the number of mission created.
 * @return Number of created mission
 * @sa CP_CountMissionActive
 * @sa CP_CountMissionOnGeoscape
 */
int CP_CountMission (void)
{
	int counterMission = 0;
	const linkedList_t *list = ccs.missions;
#ifdef DEBUG
	int counterInvalidMission = 0;
#endif

	for (; list; list = list->next, counterMission++) {
#ifdef DEBUG
		const mission_t *mission = (mission_t *)list->data;
		/* check whether current selected gametype is a valid one */
		if (mission->stage == STAGE_OVER) {
			counterInvalidMission++;
		}
#endif
	}

#ifdef DEBUG
	if (counterInvalidMission)
		Com_Printf("CP_CountMission: Warning, there are %i mission that should have been removed from global mission array\n", counterInvalidMission);
#endif
	return counterMission;
}

/**
 * @brief Count the number of mission created and active.
 * @return Number of active mission
 * @sa CP_CountMission
 * @sa CP_CountMissionOnGeoscape
 */
int CP_CountMissionActive (void)
{
	int counterMission = 0;
	int counterActiveMission = 0;

	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next, counterMission++) {
		const mission_t *mission = (mission_t *)list->data;
		/* check whether current selected gametype is a valid one */
		if (mission->stage != STAGE_NOT_ACTIVE && mission->stage != STAGE_OVER) {
			counterActiveMission++;
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "Total number of mission: %i -- Number of active mission: %i\n", counterMission, counterActiveMission);
	return counterActiveMission;
}

/**
 * @brief Count the number of mission active and displayed on geoscape.
 * @return Number of active mission visible on geoscape
 * @sa CP_CountMission
 * @sa CP_CountMissionActive
 */
int CP_CountMissionOnGeoscape (void)
{
	int counterVisibleMission = 0;

	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		/* check whether current selected gametype is a valid one */
		if (mission->stage != STAGE_NOT_ACTIVE && mission->stage != STAGE_OVER && mission->onGeoscape) {
			counterVisibleMission++;
		}
	}

	return counterVisibleMission;
}

/**
 * @brief Check if a mission should be visible on geoscape.
 * @param[in] mission Pointer to mission we want to check visibility.
 * @return see missionDetectionStatus_t.
 */
static missionDetectionStatus_t CP_CheckMissionVisibleOnGeoscape (const mission_t const *mission)
{
	/* This function could be called before position of the mission is defined */
	if (!mission->pos)
		return qfalse;

	switch (mission->stage) {
	case STAGE_TERROR_MISSION:
	case STAGE_BASE_DISCOVERED:
		return MISDET_ALWAYS_DETECTED;
	/* Crash site mission */
	case STAGE_COME_FROM_ORBIT:
	case STAGE_MISSION_GOTO:
	case STAGE_INTERCEPT:
	case STAGE_RECON_AIR:
	case STAGE_RETURN_TO_ORBIT:
		if (mission->ufo && !mission->ufo->landed)
			return MISDET_CANT_BE_DETECTED;
		else {
			/* UFO is crashed : always show crash site */
			return MISDET_ALWAYS_DETECTED;
		}
	case STAGE_RECON_GROUND:
	case STAGE_SUBVERT_GOV:
	case STAGE_SPREAD_XVI:
	case STAGE_HARVEST:
		if (RADAR_CheckRadarSensored(mission->pos))
			return MISDET_MAY_BE_DETECTED;
		break;
	case STAGE_NOT_ACTIVE:
	case STAGE_BUILD_BASE:
	case STAGE_BASE_ATTACK:
	case STAGE_OVER:
	case STAGE_SUPPLY:
		break;
	}
	return MISDET_CANT_BE_DETECTED;
}

/**
 * @brief Removes a mission from geoscape: make it non visible and call notify functions
 */
void CP_MissionRemoveFromGeoscape (mission_t *mission)
{
	if (!mission->onGeoscape && mission->category != INTERESTCATEGORY_BASE_ATTACK)
		return;

	mission->onGeoscape = qfalse;

	/* Notifications */
	MAP_NotifyMissionRemoved(mission);
	AIR_AircraftsNotifyMissionRemoved(mission);
}

/**
 * @brief Add a mission to geoscape: make it visible and stop time
 * @param[in] mission Pointer to added mission.
 * @param[in] force true if the mission should be added even for mission needing a probabilty test to be seen.
 * @sa CP_CheckNewMissionDetectedOnGeoscape
 */
void CP_MissionAddToGeoscape (mission_t *mission, qboolean force)
{
	const missionDetectionStatus_t status = CP_CheckMissionVisibleOnGeoscape(mission);

	/* don't show a mission spawned by a undetected ufo, unless forced : this may be done at next detection stage */
	if (status == MISDET_CANT_BE_DETECTED || (!force && status == MISDET_MAY_BE_DETECTED && mission->ufo && !mission->ufo->detected))
		return;

#ifdef DEBUG
	/* UFO that spawned this mission should be close of mission */
	if (mission->ufo && ((fabs(mission->ufo->pos[0] - mission->pos[0]) > 1.0f) || (fabs(mission->ufo->pos[1] - mission->pos[1]) > 1.0f))) {
		Com_Printf("Error: mission (stage: %s) spawned is not at the same location as UFO\n", CP_MissionStageToName(mission->stage));
	}
#endif

	/* Notify the player */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Alien activity has been detected in %s."), mission->location);
	MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);

	mission->onGeoscape = qtrue;
	CL_GameTimeStop();
}

/**
 * @brief Check if mission has been detected by radar.
 * @note called every @c DETECTION_INTERVAL.
 * @sa RADAR_CheckUFOSensored
 * @return True if a new mission was detected.
 */
qboolean CP_CheckNewMissionDetectedOnGeoscape (void)
{
	const linkedList_t *list = ccs.missions;
	/** @brief Probability to detect UFO each @c DETECTION_INTERVAL
	 * @note This correspond to 40 percents each 30 minutes (coded this way to be able to
	 * change @c DETECTION_INTERVAL without changing the way radar works) */
	const float missionDetectionProbability = 0.000125f * DETECTION_INTERVAL;
	qboolean newDetection = qfalse;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		const missionDetectionStatus_t status = CP_CheckMissionVisibleOnGeoscape(mission);

		/* only check mission that can be detected, and that are not already detected */
		if (status != MISDET_MAY_BE_DETECTED || mission->onGeoscape == qtrue)
			continue;

		/* if there is a ufo assigned, it must not be detected */
		assert(!mission->ufo || !mission->ufo->detected);

		if (frand() <= missionDetectionProbability) {
			/* mission is detected */
			CP_MissionAddToGeoscape(mission, qtrue);

			/* maybe radar is not activated yet (as ufo wasn't detected before) */
			if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
				MAP_SetOverlay("radar");

			/* if mission has a UFO, detect the UFO when it takes off */
			if (mission->ufo)
				mission->ufo->detected = qtrue;

			newDetection = qtrue;
		}
	}
	return newDetection;
}

/**
 * @brief Update all mission visible on geoscape (in base radar range).
 * @note you can't see a mission with aircraft radar.
 * @sa CP_CheckMissionAddToGeoscape
 */
void CP_UpdateMissionVisibleOnGeoscape (void)
{
	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (mission->onGeoscape && CP_CheckMissionVisibleOnGeoscape(mission) == MISDET_CANT_BE_DETECTED) {
			/* remove a mission when radar is destroyed */
			CP_MissionRemoveFromGeoscape(mission);
		} else if (!mission->onGeoscape && CP_CheckMissionVisibleOnGeoscape(mission) == MISDET_ALWAYS_DETECTED) {
			/* only show mission that are always detected: mission that have a probability to be detected will be
			 * tested at next half hour */
			CP_MissionAddToGeoscape(mission, qfalse);
		}
	}
}

/**
 * @brief Removes (temporarily) a UFO from geoscape: make it landed and call notify functions
 * @param[in] mission Pointer to mission
 * @param[in] destroyed True if the UFO has been destroyed, false if it's only landed.
 * @note We don't destroy the UFO because we can use later, e.g. if it takes off
 * @sa UFO_RemoveFromGeoscape
 */
void CP_UFORemoveFromGeoscape (mission_t *mission, qboolean destroyed)
{
	linkedList_t *list;

	assert(mission->ufo);

	/* UFO is landed even if it's destroyed: crash site mission is spawned */
	mission->ufo->landed = qtrue;

	/* Notications */
	AIR_AircraftsNotifyUFORemoved(mission->ufo, destroyed);
	MAP_NotifyUFORemoved(mission->ufo, destroyed);

	/* maybe we use a high speed time: UFO was detected, but flied out of radar
	 * range since last calculation, and mission is spawned outside radar range */
	if (!RADAR_CheckRadarSensored(mission->ufo->pos) && mission->ufo->detected)
		RADAR_NotifyUFORemoved(mission->ufo, destroyed);

	if (destroyed) {
		if (mission->ufo->detected)
			RADAR_NotifyUFORemoved(mission->ufo, destroyed);

		/* Update UFO idx */
		list = ccs.missions;
		for (; list; list = list->next) {
			mission_t *removedMission = (mission_t *)list->data;
			if (removedMission->ufo && (removedMission->ufo > mission->ufo))
				removedMission->ufo--;
		}

		UFO_RemoveFromGeoscape(mission->ufo);
	}
}

/**
 * @brief Removes a mission from mission global array.
 * @sa UFO_RemoveFromGeoscape
 */
void CP_MissionRemove (mission_t *mission)
{
	linkedList_t *list;

	/* Destroy UFO */
	if (mission->ufo)
		CP_UFORemoveFromGeoscape(mission, qtrue);		/* for the notifications */

	/* Remove from ccs.battleParameters */
	if (mission == ccs.battleParameters.mission)
		ccs.battleParameters.mission = NULL;

	/* Notifications */
	CP_MissionRemoveFromGeoscape(mission);

	list = ccs.missions;
	for (; list; list = list->next) {
		mission_t *removedMission = (mission_t *)list->data;
		if (removedMission == mission) {
			LIST_Remove(&ccs.missions, list);
			Com_DPrintf(DEBUG_CLIENT, "Mission removed from global array: %i missions left\n", CP_CountMission());
			return;
		}
	}

	Com_Printf("CP_MissionRemove: Could not find mission '%s' to remove.\n", mission->id);
#ifdef DEBUG
	Com_Printf("   missions in list are: ");
	list = ccs.missions;
	for (; list; list = list->next) {
		const mission_t *removedMission = (mission_t *)list->data;
		Com_Printf("'%s', ", removedMission->id);
	}
	Com_Printf("\n");
#endif
}

/**
 * @brief Notify that a base has been removed.
 */
void CP_MissionNotifyBaseDestroyed (const base_t *base)
{
	linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (mission->category == INTERESTCATEGORY_BASE_ATTACK && mission->data) {
			base_t *missionBase = (base_t *) mission->data;
			if (base == missionBase) {
				/* Aimed base has been destroyed, abort mission */
				CP_MissionRemove(mission);
			}
		}
	}
}

/**
 * @brief Disable time limit for given mission.
 * @note This function is used for better readibility.
 * @sa CP_CheckNextStageDestination
 * @sa CP_CheckMissionLimitedInTime
 */
void CP_MissionDisableTimeLimit (mission_t *mission)
{
	mission->finalDate.day = 0;
}

/**
 * @brief Check if mission should end because of limited time.
 * @note This function is used for better readibility.
 * @sa CP_MissionDisableTimeLimit
 * @return qtrue if function should end after finalDate
 */
qboolean CP_CheckMissionLimitedInTime (const mission_t *mission)
{
	return mission->finalDate.day != 0;
}

/**
 * @brief mission begins: UFO arrive on earth.
 * @note Stage 0 -- This function is common to several mission category.
 * @sa CP_MissionChooseUFO
 * @return true if mission was created, false else.
 */
qboolean CP_MissionCreate (mission_t *mission)
{
	int ufoType;

	mission->stage = STAGE_COME_FROM_ORBIT;

	ufoType = CP_MissionChooseUFO(mission);
	if (ufoType == UFO_MAX) {
		mission->ufo = NULL;
		/* Mission starts from ground: no UFO. Go to next stage on next frame (skip UFO arrives on earth) */
		mission->finalDate = ccs.date;
	} else {
		mission->ufo = UFO_AddToGeoscape(ufoType, NULL, mission);
		if (!mission->ufo) {
			Com_Printf("CP_MissionCreate: Could not add UFO '%s', remove mission\n", UFO_TypeToShortName(ufoType));
			CP_MissionRemove(mission);
			return qfalse;
		}
		/* Stage will finish when UFO arrives at destination */
		CP_MissionDisableTimeLimit(mission);
	}

	return qtrue;
}


/**
 * @brief Choose UFO type for a given mission category.
 * @param[in] mission Pointer to the mission where the UFO will be added
 * @sa CP_MissionChooseUFO
 * @sa CP_SupplyMissionCreate
 * @return ufoType_t of the UFO spawning the mission, UFO_MAX if the mission is spawned from ground
 */
int CP_MissionChooseUFO (const mission_t *mission)
{
	int ufoTypes[UFO_MAX];
	int numTypes = 0;
	int idx;
	qboolean canBeSpawnedFromGround = qfalse;
	float groundProbability = 0.0f;	/**< Probability to start a mission from earth (without UFO) */
	float randNumber;

	switch (mission->category) {
	case INTERESTCATEGORY_RECON:
		numTypes = CP_ReconMissionAvailableUFOs(mission, ufoTypes);
		canBeSpawnedFromGround = qtrue;
		break;
	case INTERESTCATEGORY_TERROR_ATTACK:
		numTypes = CP_TerrorMissionAvailableUFOs(mission, ufoTypes);
		canBeSpawnedFromGround = qtrue;
		break;
	case INTERESTCATEGORY_BASE_ATTACK:
		numTypes = CP_BaseAttackMissionAvailableUFOs(mission, ufoTypes);
		canBeSpawnedFromGround = qtrue;
		break;
	case INTERESTCATEGORY_BUILDING:
		numTypes = CP_BuildBaseMissionAvailableUFOs(mission, ufoTypes);
		break;
	case INTERESTCATEGORY_SUPPLY:
		numTypes = CP_SupplyMissionAvailableUFOs(mission, ufoTypes);
		break;
	case INTERESTCATEGORY_XVI:
		numTypes = CP_XVIMissionAvailableUFOs(mission, ufoTypes);
		canBeSpawnedFromGround = qtrue;
		break;
	case INTERESTCATEGORY_INTERCEPT:
		numTypes = CP_InterceptMissionAvailableUFOs(mission, ufoTypes);
		break;
	case INTERESTCATEGORY_HARVEST:
		numTypes = CP_HarvestMissionAvailableUFOs(mission, ufoTypes);
		break;
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Sys_Error("CP_MissionChooseUFO: Wrong mission category %i\n", mission->category);
		break;
	}
	if (numTypes > UFO_MAX)
		Sys_Error("CP_MissionChooseUFO: Too many values UFOs (%i/%i)\n", numTypes, UFO_MAX);

	/* Roll the random number */
	randNumber = frand();

	/* Check if the mission is spawned without UFO */
	if (canBeSpawnedFromGround) {
		const int XVI_PARAM = 10;		/**< Typical XVI average value for spreading mission from earth */
		/* The higher the XVI rate, the higher the probability to have a mission spawned from ground */
		groundProbability = 1.0f - exp(-CP_GetAverageXVIRate() / XVI_PARAM);

		/* Mission spawned from ground */
		if (randNumber < groundProbability)
			return UFO_MAX;
	}

	/* If we reached this point, then mission will be spawned from space: choose UFO */
	assert(numTypes);
	idx = (int) (numTypes * (randNumber - groundProbability) / (1.0f - groundProbability));
	assert(idx < numTypes);

	return ufoTypes[idx];
}

/**
 * @brief Determine what action should be performed when a mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_MissionStageEnd (mission_t *mission)
{
	Com_DPrintf(DEBUG_CLIENT, "Ending mission category %i, stage %i (time: %i day, %i sec)\n",
		mission->category, mission->stage, ccs.date.day, ccs.date.sec);
	switch (mission->category) {
	case INTERESTCATEGORY_RECON:
		CP_ReconMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_TERROR_ATTACK:
		CP_TerrorMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_BASE_ATTACK:
		CP_BaseAttackMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_BUILDING:
		CP_BuildBaseMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_SUPPLY:
		CP_SupplyMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_XVI:
		CP_XVIMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_INTERCEPT:
		CP_InterceptNextStage(mission);
		break;
	case INTERESTCATEGORY_HARVEST:
		CP_HarvestMissionNextStage(mission);
		break;
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Com_Printf("CP_MissionStageEnd: Invalid type of mission (%i), remove mission '%s'\n", mission->category, mission->id);
		CP_MissionRemove(mission);
	}
}

/**
 * @brief Mission is finished because Phalanx team ended it.
 * @param[in] mission Pointer to the mission that is ended.
 */
void CP_MissionIsOver (mission_t *mission)
{
	switch (mission->category) {
	case INTERESTCATEGORY_RECON:
		CP_ReconMissionIsFailure(mission);
		break;
	case INTERESTCATEGORY_TERROR_ATTACK:
		CP_TerrorMissionIsFailure(mission);
		break;
	case INTERESTCATEGORY_BASE_ATTACK:
		if (mission->stage <= STAGE_BASE_ATTACK)
			CP_BaseAttackMissionIsFailure(mission);
		else
			CP_BaseAttackMissionIsSuccess(mission);
		break;
	case INTERESTCATEGORY_BUILDING:
		if (mission->stage <= STAGE_BUILD_BASE)
			CP_BuildBaseMissionIsFailure(mission);
		else if (mission->stage == STAGE_BASE_DISCOVERED)
			CP_BuildBaseMissionBaseDestroyed(mission);
		else
			CP_BuildBaseMissionIsSuccess(mission);
		break;
	case INTERESTCATEGORY_SUPPLY:
		if (mission->stage <= STAGE_SUPPLY)
			CP_SupplyMissionIsFailure(mission);
		else
			CP_SupplyMissionIsSuccess(mission);
		break;
	case INTERESTCATEGORY_XVI:
		if (mission->stage <= STAGE_SPREAD_XVI)
			CP_XVIMissionIsFailure(mission);
		else
			CP_XVIMissionIsSuccess(mission);
		break;
	case INTERESTCATEGORY_INTERCEPT:
		if (mission->stage <= STAGE_INTERCEPT)
			CP_InterceptMissionIsFailure(mission);
		else
			CP_InterceptMissionIsSuccess(mission);
		break;
	case INTERESTCATEGORY_HARVEST:
		CP_HarvestMissionIsFailure(mission);
		break;
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Com_Printf("CP_MissionIsOver: Invalid type of mission (%i), remove mission\n", mission->category);
		CP_MissionRemove(mission);
	}
}

/**
 * @brief Mission is finished because Phalanx team ended it.
 * @param[in] ufocraft Pointer to the UFO involved in this mission
 */
void CP_MissionIsOverByUFO (aircraft_t *ufocraft)
{
	assert(ufocraft->mission);
	CP_MissionIsOver(ufocraft->mission);
}

/**
 * @brief Set mission name.
 * @note that mission name must be unique in mission global array
 * @param[out] mission The mission to set the name for
 * @sa CP_CreateNewMission
 */
static inline void CP_SetMissionName (mission_t *mission)
{
	int num = 0;

	do {
		Com_sprintf(mission->id, sizeof(mission->id), "cat%i_interest%i_%i",
			mission->category, mission->initialOverallInterest, num);

		/* if mission list is empty, the id is unique for sure */
		if (!ccs.missions)
			return;

		/* check whether a mission with the same id already exists in the list
		 * if so, generate a new name by using an increased num values - otherwise
		 * just use the mission->id we just stored - it should be unique now */
		if (!LIST_ContainsString(ccs.missions, mission->id))
			break;

		num++;
	} while (num); /* fake condition */
}

/**
 * @brief Create a new mission of given category.
 * @param[in] category category of the mission
 * @param[in] beginNow true if the mission should begin now
 * @sa CP_SpawnNewMissions
 * @sa CP_MissionStageEnd
 */
void CP_CreateNewMission (interestCategory_t category, qboolean beginNow)
{
	mission_t mission;
	const date_t minNextSpawningDate = {0, 0};
	const date_t nextSpawningDate = {3, 0};	/* Delay between 2 mission spawning */

	/* Some event are non-occurrence */
	if (category == INTERESTCATEGORY_NONE)
		return;

	memset(&mission, 0, sizeof(mission));

	/* First fill common datas between all type of missions */
	mission.category = category;
	mission.initialOverallInterest = ccs.overallInterest;
	mission.initialIndividualInterest = ccs.interest[category];
	mission.stage = STAGE_NOT_ACTIVE;
	mission.ufo = NULL;
	if (beginNow) {
		mission.startDate.day = ccs.date.day;
		mission.startDate.sec = ccs.date.sec;
	} else
		mission.startDate = Date_Add(ccs.date, Date_Random(minNextSpawningDate, nextSpawningDate));
	mission.finalDate = mission.startDate;
	CP_SetMissionName(&mission);

	/* Lower alien interest in base building if a base building mission is
	 * started to avoid several bases at the same time */
	if (mission.category == INTERESTCATEGORY_BUILDING)
		CP_BuildBaseMissionStart(&mission);
	else if (mission.category == INTERESTCATEGORY_BASE_ATTACK)
		CP_BaseAttackMissionStart(&mission);

	/* Add mission to global array */
	LIST_Add(&ccs.missions, (byte*) &mission, sizeof(mission_t));
}

/**
 * @brief Select new mission type.
 * @sa CP_SpawnNewMissions
 */
static int CP_SelectNewMissionType (void)
{
	int sum = 0;
	int i, randomNumber;

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		sum += ccs.interest[i];

	randomNumber = (int) (frand() * (float) sum);

	for (i = 0; randomNumber >= 0; i++) {
		randomNumber -= ccs.interest[i];
	}

	return i - 1;
}

/** @brief Number of days between events are spawned */
static const int DELAY_BETWEEN_MISSION_SPAWNING = 3;

/**
 * @brief Spawn new missions.
 * @sa CL_CampaignRun
 * @note daily called
 */
void CP_SpawnNewMissions (void)
{
	int i;

	ccs.lastMissionSpawnedDelay++;

	if (ccs.lastMissionSpawnedDelay > DELAY_BETWEEN_MISSION_SPAWNING) {
		/* How many missions will be spawned until next cycle ? */
		const int newMissionNum = (int) (pow(ccs.overallInterest / 10.0f, 0.6));
		for (i = 0; i < newMissionNum; i++) {
			const int type = CP_SelectNewMissionType();
			CP_CreateNewMission(type, qfalse);
		}

		ccs.lastMissionSpawnedDelay -= DELAY_BETWEEN_MISSION_SPAWNING;
	}
}

/**
 * @brief Initialize spawning delay.
 * @sa CP_SpawnNewMissions
 * @note only called when new game is started, in order to spawn new event on the beginning of the game.
 */
void CP_InitializeSpawningDelay (void)
{
	ccs.lastMissionSpawnedDelay = DELAY_BETWEEN_MISSION_SPAWNING;

	CP_SpawnNewMissions();
}

#ifdef DEBUG
/**
 * @brief Debug function for creating a mission.
 * @note called with debug_addmission
 */
static void CP_SpawnNewMissions_f (void)
{
	int category, type = 0;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category>\n", Cmd_Argv(0));
		for (category = INTERESTCATEGORY_RECON; category < INTERESTCATEGORY_MAX; category++) {
			Com_Printf("...%i: %s", category, CP_MissionCategoryToName(category));
			if (category == INTERESTCATEGORY_RECON)
				Com_Printf(" <0:Random, 1:Aerial, 2:Ground>");
			else if (category == INTERESTCATEGORY_BUILDING)
				Com_Printf(" <0:Subverse Government, 1:Build Base>");
			else if (category == INTERESTCATEGORY_INTERCEPT)
				Com_Printf(" <0:Intercept aircraft, 1:Attack installation>");
			Com_Printf("\n");
		}
		return;
	}

	if (Cmd_Argc() == 3)
		type = atoi(Cmd_Argv(2));

	category = atoi(Cmd_Argv(1));

	if (category < INTERESTCATEGORY_NONE || category >= INTERESTCATEGORY_MAX)
		return;

	CP_CreateNewMission(category, qtrue);

	if (type) {
		mission_t *mission = CP_GetLastMissionAdded();
		if (!mission) {
			Com_Printf("CP_SpawnNewMissions_f: Could not add mission, abort\n");
			return;
		}
		switch (category) {
		case INTERESTCATEGORY_RECON:
			/* Start mission */
			if (!CP_MissionCreate(mission))
				return;
			if (type == 1)
				/* Aerial mission */
				CP_ReconMissionAerial(mission);
			else
				/* This is a ground mission */
				CP_ReconMissionGroundGo(mission);
			break;
		case INTERESTCATEGORY_BUILDING:
			if (type == 1)
				mission->initialOverallInterest = STARTING_BASEBUILD_INTEREST + 1;
			break;
		case INTERESTCATEGORY_INTERCEPT:
			/* Start mission */
			if (!CP_MissionCreate(mission))
				return;
			if (type == 1) {
				mission->ufo->ufotype = UFO_HARVESTER;
				CP_InterceptGoToInstallation(mission);
			} else {
				CP_InterceptAircraftMissionSet(mission);
			}
			break;
		default:
			Com_Printf("Type is not implemented for this category.\n");
		}
	}
}

/**
 * @brief List all current mission to console.
 * @note Use with debug_listmission
 */
static void CP_MissionList_f (void)
{
	const linkedList_t *list = ccs.missions;
	qboolean noMission = qtrue;

	for (; list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		Com_Printf("mission: '%s'\n", mission->id);
		Com_Printf("...category %i. '%s' -- stage %i. '%s'\n", mission->category,
			CP_MissionCategoryToName(mission->category), mission->stage, CP_MissionStageToName(mission->stage));
		Com_Printf("...mapDef: '%s'\n", mission->mapDef ? mission->mapDef->id : "No mapDef defined");
		Com_Printf("...location: '%s'\n", mission->location);
		Com_Printf("...start (day = %i, sec = %i), ends (day = %i, sec = %i)\n",
			mission->startDate.day, mission->startDate.sec, mission->finalDate.day, mission->finalDate.sec);
		Com_Printf("...pos (%.02f, %.02f) -- mission %son Geoscape\n", mission->pos[0], mission->pos[1], mission->onGeoscape ? "" : "not ");
		if (mission->ufo)
			Com_Printf("...UFO: %s (%i/%i)\n", mission->ufo->id, (int) (mission->ufo - gd.ufos), gd.numUFOs - 1);
		else
			Com_Printf("...UFO: no UFO\n");
		noMission = qfalse;
	}
	if (noMission)
		Com_Printf("No mission currently in game.\n");
}

/**
 * @brief Debug function for deleting all mission in global array.
 * @note called with debug_delmissions
 */
static void CP_DeleteMissions_f (void)
{
	int n = CP_CountMission();
	const linkedList_t *list = ccs.missions;

	for (;list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		CP_MissionRemove(mission);
	}
	Com_Printf("Removed %i mission(s) from global array\n", n);

	if (gd.numUFOs != 0) {
		Com_Printf("CP_DeleteMissions_f: Error, there are still %i UFO in game afer removing all missions. Force removal.\n", gd.numUFOs);
		while (gd.numUFOs)
			UFO_RemoveFromGeoscape(gd.ufos);
	}
}

/**
 * @brief List alien interest values.
 * @sa function called with debug_listinterest
 */
static void CP_AlienInterestList_f (void)
{
	int i;

	Com_Printf("Overall interest: %i\n", ccs.overallInterest);
	Com_Printf("Individual interest:\n");

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		Com_Printf("...%i. %s -- %i\n", i, CP_MissionCategoryToName(i), ccs.interest[i]);
}
#endif

void CP_MissionsInit (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_addmission", CP_SpawnNewMissions_f, "Add a new mission");
	Cmd_AddCommand("debug_delmissions", CP_DeleteMissions_f, "Remove all missions from global array");
	Cmd_AddCommand("debug_listmission", CP_MissionList_f, "Debug function to show all missions");
	Cmd_AddCommand("debug_listinterest", CP_AlienInterestList_f, "Debug function to show alien interest values");
#endif
}
