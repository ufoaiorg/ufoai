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
#include "../cl_map.h"
#include "../cl_ufo.h"
#include "cp_missions.h"
#include "cp_time.h"

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
		break;
	}
	return _("Crashed UFO");
}

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
