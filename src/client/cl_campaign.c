/**
 * @file cl_campaign.c
 * @brief Single player campaign control.
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

#include "client.h"
#include "cl_global.h"
#include "cl_team.h"
#include "cl_mapfightequip.h"
#include "cl_airfightmap.h"
#include "cl_hospital.h"
#include "cl_actor.h"
#include "cl_popup.h"
#include "cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_uforecovery.h"
#include "cl_alienbase.h"
#include "menu/m_popup.h"
#include "menu/node/m_node_container.h"
#include "cl_installation.h"

/**
 * @brief This fake aircraft is used to assign soldiers for a base attack mission
 * @sa CP_BaseAttackStartMission
 * @sa AIR_AddToAircraftTeam
 */
static aircraft_t baseAttackFakeAircraft;

void R_IncreaseXVILevel(const vec2_t pos);
void R_InitializeXVIOverlay(const char *mapname, byte *data, int width, int height);
byte* R_XVIMapCopy(int *width, int *height);
void R_CreateRadarOverlay(void);

/* public vars */
mission_t *selectedMission;			/**< Currently selected mission on geoscape */
campaign_t *curCampaign;			/**< Current running campaign */
ccs_t ccs;
base_t *baseCurrent;				/**< Pointer to current base. */
stats_t campaignStats;
missionResults_t missionresults;

static campaign_t campaigns[MAX_CAMPAIGNS];
static int numCampaigns = 0;
static technology_t *rsAlienXVI;
#define XVI_EVENT_NAME "rs_alien_xvi_event"
static salary_t salaries[MAX_CAMPAIGNS];
static cvar_t *cl_campaign;
static const int MAX_POS_LOOP = 10;				/**< Maximum number of loops to choose a mission position (to avoid infinite loops) */

static const int STARTING_BASEBUILD_INTEREST = 300;	/**< Overall alien interest value for starting constructing alien bases. */

/**
 * @brief Check wheter given date and time is later than current date.
 * @param[in] now Current date.
 * @param[in] compare Date to compare.
 * @return True if current date is later than given one.
 */
static qboolean Date_LaterThan (date_t now, date_t compare)
{
	if (now.day > compare.day)
		return qtrue;
	if (now.day < compare.day)
		return qfalse;
	if (now.sec > compare.sec)
		return qtrue;
	return qfalse;
}

/**
 * @brief Add two dates and return the result.
 * @param[in] a First date.
 * @param[in] b Second date.
 * @return The result of adding date_ b to date_t a.
 */
static date_t Date_Add (date_t a, date_t b)
{
	a.sec += b.sec;
	a.day += (a.sec / SECONDS_PER_DAY) + b.day;
	a.sec %= SECONDS_PER_DAY;
	return a;
}

/**
 * @brief Return a random relative date which lies between a lower and upper limit.
 * @param[in] minFrame Minimal date.
 * @param[in] maxFrame Maximal date.
 * @return A date value between minFrame and maxFrame.
 */
static date_t Date_Random (date_t minFrame, date_t maxFrame)
{
	maxFrame.sec = max(minFrame.day * SECONDS_PER_DAY + minFrame.sec,
		(maxFrame.day * SECONDS_PER_DAY + maxFrame.sec) * frand());

	maxFrame.day = maxFrame.sec / SECONDS_PER_DAY;
	maxFrame.sec = maxFrame.sec % SECONDS_PER_DAY;
	return maxFrame;
}

/**
 * @brief Check conditions for new base and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the base has been build.
 * @sa B_BuildBase
 */
qboolean CL_NewBase (base_t* base, vec2_t pos)
{
	byte *colorTerrain;

	assert(base);

	if (base->founded) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: base already founded: %i\n", base->idx);
		return qfalse;
	} else if (gd.numBases == MAX_BASES) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: max base limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MN_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: zoneType: '%s'\n", MAP_GetTerrainType(colorTerrain));
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for base terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build base */
	Vector2Copy(pos, base->pos);

	gd.numBases++;

	/* set up the base with buildings that have the autobuild flag set */
	B_SetUpBase(base, cl_start_employees->integer, cl_start_buildings->integer);

	return qtrue;
}

/**
 * @brief Check conditions for new installation and build it.
 * @param[in] pos Position on the geoscape.
 * @return True if the base has been build.
 * @sa INS_BuildInstallation
 */
qboolean CL_NewInstallation (installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos)
{
	byte *colorTerrain;

	assert(installation);

	if (installation->founded) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: installation already founded: %i\n", installation->idx);
		return qfalse;
	} else if (gd.numInstallations == MAX_INSTALLATIONS) {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: max installation limit hit\n");
		return qfalse;
	}

	colorTerrain = MAP_GetColor(pos, MAPTYPE_TERRAIN);

	if (MapIsWater(colorTerrain)) {
		/* This should already have been catched in MAP_MapClick (cl_menu.c), but just in case. */
		MN_AddNewMessage(_("Notice"), _("Could not set up your installation at this location"), qfalse, MSG_INFO, NULL);
		return qfalse;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "CL_NewInstallation: zoneType: '%s'\n", MAP_GetTerrainType(colorTerrain));
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for installation terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build installation */
	Vector2Copy(pos, installation->pos);

	gd.numInstallations++;

	return qtrue;
}


/*****************************************************************************
 *
 * Alien interest functions
 *
 *****************************************************************************/

/**
 * @brief Typical value of the overall alien interest at the end of the game.
 */
static const int FINAL_OVERALL_INTEREST = 1000;

/**
 * @brief Initialize alien interest values and mission cycle
 * @note Should be used when a new single player campaign starts
 * @sa CL_GameNew_f
 */
static void CL_ResetAlienInterest (void)
{
	int i;

	ccs.lastInterestIncreaseDelay = 0;
	ccs.lastMissionSpawnedDelay = 0;
	ccs.overallInterest = 20;

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		ccs.interest[i] = 0;
	ccs.interest[INTERESTCATEGORY_NONE] = 6;
	ccs.interest[INTERESTCATEGORY_RECON] = 20;
}

/**
 * @brief Change individual interest value
 * @param[in] percentage Maybe pe > 0 or < 0
 * @param[in] category Category of interest for aliens that should increase / decrease
 * @note Should be used when a mission is over (success or failure)
 */
static void CL_ChangeIndividualInterest (float percentage, interestCategory_t category)
{
	const float nonOccurrencePercent = 0.3f;		/**< Non occurence probability */

	if (category == INTERESTCATEGORY_MAX) {
		Com_Printf("CL_ChangeIndividualInterest: Unsupported value of category\n");
		return;
	}

	if (percentage > 0.0f) {
		const int gain = (int) (percentage * ccs.overallInterest);
		const int diff = ccs.overallInterest - ccs.interest[category];
		const float slowerIncreaseFraction = 0.5f; /**< Fraction of individual interest that will be won if
									individal interest becomes higher than overall interest. 0 means no increase */
		/* Value increases: percentage is taken from the overall interest value
			But it increase slower if the individual interest becomes higher than the overall interest value */
		if (diff > gain)
			/* Final value of individual interest is below overall interest */
			ccs.interest[category] += gain;
		else if (diff > 0)
			/* Initial value of individual interest is below overall interest */
			ccs.interest[category] = ccs.overallInterest + (int) (slowerIncreaseFraction * (gain - diff));
		else
			/* Final value of individual interest is above overall interest */
			ccs.interest[category] += (int) (slowerIncreaseFraction * gain);
	} else {
		/* Value decreases: percentage is taken from the individual interest value */
		ccs.interest[category] += (int) (percentage * ccs.interest[category]);
		if (ccs.interest[category] < 0) {
			/* this may be reached if percentage is below -1 */
			ccs.interest[category] = 0;
		}
	}

	/* Set new non-occurence value. The aim is to have roughly a steady number
	 * of none occurrence during the game, in order to include some randomness.
	 * If overall alien interest becomes higher than FINAL_OVERALL_INTEREST,
	 * then the none occurence goes to zero, to make more pressure on the player */
	if (ccs.overallInterest < FINAL_OVERALL_INTEREST)
		ccs.interest[INTERESTCATEGORY_NONE] = (int) (nonOccurrencePercent * ccs.overallInterest);
	else
		ccs.interest[INTERESTCATEGORY_NONE] = (int) (nonOccurrencePercent * FINAL_OVERALL_INTEREST * exp((ccs.overallInterest - FINAL_OVERALL_INTEREST) / 30));
}

/**
 * @brief Increase alien overall interest.
 * @sa CL_CampaignRun
 * @note hourly called
 */
static void CP_IncreaseAlienInterest (void)
{
	const int delayBetweenIncrease = 28 - curCampaign->difficulty;	/**< Number of hours between 2 overall interest increase */

	ccs.lastInterestIncreaseDelay++;

	if (ccs.lastInterestIncreaseDelay > delayBetweenIncrease) {
		ccs.overallInterest++;
		ccs.lastInterestIncreaseDelay %= delayBetweenIncrease;
	}
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

/*****************************************************************************
 *
 *	Mission functions
 *
  *****************************************************************************/

/**
 * @brief Check if a map may be selected for mission.
 * @param[in] mission Pointer to the mission where mapDef should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] mapIdx idx of the map in csi.mds[]
 * @param[in] ufoCrashed Search the mission definition for crash ufo id if true
 * @return qfalse if map is not selectable
 */
static qboolean CP_MapIsSelectable (mission_t *mission, int mapIdx, const vec2_t pos, qboolean ufoCrashed)
{
	mapDef_t *md;

	assert(mapIdx >= 0);
	assert(mapIdx < csi.numMDs);

	md = &csi.mds[mapIdx];
	if (md->storyRelated)
		return qfalse;

	if (pos && !MAP_PositionFitsTCPNTypes(pos, md->terrains, md->cultures, md->populations, NULL))
		return qfalse;

	if (!mission->ufo) {
		/* a mission without UFO should not use a map with UFO */
		if (md->ufos)
			return qfalse;
	} else {
		/* A mission with UFO should use a map with UFO
		 * first check that list is not empty */
		if (!md->ufos)
			return qfalse;
		if (ufoCrashed) {
			if (!LIST_ContainsString(md->ufos, UFO_CrashedTypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		} else {
			if (!LIST_ContainsString(md->ufos, UFO_TypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		}
	}

	return qtrue;
}

/**
 * @brief Choose a map for given mission.
 * @param[in|out] mission Pointer to the mission where a new map should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] ufoCrashed true if the ufo is crashed
 * @return qfalse if could not set mission
 */
static qboolean CP_ChooseMap (mission_t *mission, const vec2_t pos, qboolean ufoCrashed)
{
	int i;
	int maxHits = 1;	/**< Total number of maps fulfilling mission conditions. */
	int hits = 0;		/**< Number of maps fulfilling mission conditions and that appeared less often during game. */
	int minMissionAppearance = 9999;
	int randomNum;

	mission->mapDef = NULL;

	/* Set maxHits and hits. */
	while (maxHits) {
		maxHits = 0;
		for (i = 0; i < csi.numMDs; i++) {
			mapDef_t *md;

			/* Check if mission fulfill conditions */
			if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
				continue;

			maxHits++;
			md = &csi.mds[i];
			if (md->timesAlreadyUsed < minMissionAppearance) {
				/* at least one fulfilling mission as been used less time than minMissionAppearance:
				 * restart the loop with this number of time.
				 * note: this implies that hits must be > 0 after the loop */
				hits = 0;
				minMissionAppearance = md->timesAlreadyUsed;
				break;
			} else if (md->timesAlreadyUsed == minMissionAppearance)
				hits++;
		}

		if (i >= csi.numMDs) {
			/* We scanned all maps in memory without finding a map used less than minMissionAppearance: exit while loop */
			break;
		}
	}

	if (!maxHits) {
		/* no map fulfill the conditions */
		if (ufoCrashed) {
			/* default map for crashsite mission is the crashsite random map assembly */
			mission->mapDef = Com_GetMapDefinitionByID("ufocrash");
			if (!mission->mapDef)
				Sys_Error("Could not find mapdef ufocrash");
			return qtrue;
		} else {
			Com_Printf("CP_ChooseMap: Could not find map with required conditions:\n");
			Com_Printf("  ufo: %s -- pos: ", mission->ufo ? UFO_TypeToShortName(mission->ufo->ufotype) : "none");
			if (pos)
				Com_Printf("%s",MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)) ? " (in water) " : "");
			if (pos)
				Com_Printf("(%.02f, %.02f)\n", pos[0], pos[1]);
			else
				Com_Printf("none\n");
			return qfalse;
		}
	}

	/* If we reached this point, that means that at least 1 map fulfills the conditions of the mission */
	assert(hits);
	assert(hits < csi.numMDs);

	/* set number of mission to select randomly between 0 and hits - 1 */
	randomNum = rand() % hits;

	/* Select mission mission number 'randomnumber' that fulfills the conditions */
	for (i = 0; i < csi.numMDs; i++) {
		mapDef_t *md;

		/* Check if mission fulfill conditions */
		if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
			continue;

		md = &csi.mds[i];
		if (md->timesAlreadyUsed > minMissionAppearance)
			continue;

		/* There shouldn't be mission fulfilling conditions used less time than minMissionAppearance */
		assert(md->timesAlreadyUsed == minMissionAppearance);

		if (!randomNum)
			break;
		else
			randomNum--;
	}

	/* A mission must have been selected */
	assert(i < csi.numMDs);

	mission->mapDef = &csi.mds[i];
	Com_DPrintf(DEBUG_CLIENT, "Selected map '%s' (among %i possible maps)\n", mission->mapDef->id, hits);

	return qtrue;
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
static mission_t* CP_GetLastMissionAdded (void)
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
static void CP_MissionRemoveFromGeoscape (mission_t *mission)
{
	if (!mission->onGeoscape && mission->category != INTERESTCATEGORY_BASE_ATTACK)
		return;

	mission->onGeoscape = qfalse;

	/* Notifications */
	MAP_NotifyMissionRemoved(mission);
	AIR_AircraftsNotifyMissionRemoved(mission);
}

#ifdef DEBUG
static const char* CP_MissionStageToName(const missionStage_t stage);
#endif

/**
 * @brief Add a mission to geoscape: make it visible and stop time
 * @param[in] mission Pointer to added mission.
 * @param[in] force true if the mission should be added even for mission needing a probabilty test to be seen.
 * @sa CP_CheckNewMissionDetectedOnGeoscape
 */
static inline void CP_MissionAddToGeoscape (mission_t *mission, qboolean force)
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
 * @note called every 30 minutes
 */
static void CP_CheckNewMissionDetectedOnGeoscape (void)
{
	const linkedList_t *list = ccs.missions;
	const float missionDetectionProbability = 0.4f;	/**< Probability to detect mission each 30 minutes */

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		const missionDetectionStatus_t status = CP_CheckMissionVisibleOnGeoscape(mission);

		/* only check mission that can be detected, and that are not already detected */
		if (status != MISDET_MAY_BE_DETECTED || mission->onGeoscape == qtrue)
			continue;

#ifdef DEBUG
		if (mission->ufo)
			assert(!mission->ufo->detected);
#endif

		if (frand() <= missionDetectionProbability) {
			/* mission is detected */
			CP_MissionAddToGeoscape(mission, qtrue);

			/* maybe radar is not activated yet (as ufo wasn't detected before) */
			if (!(r_geoscape_overlay->integer & OVERLAY_RADAR))
				MAP_SetOverlay("radar");

			/* if mission has a UFO, detect the UFO when it takes off (you're carefull on this location) */
			if (mission->ufo)
				mission->ufo->detected = qtrue;
		}
	}
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
		if (mission->onGeoscape && !CP_CheckMissionVisibleOnGeoscape(mission))
			CP_MissionRemoveFromGeoscape(mission);
		else if (!mission->onGeoscape && CP_CheckMissionVisibleOnGeoscape(mission) == MISDET_ALWAYS_DETECTED) {
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
static void CP_UFORemoveFromGeoscape (mission_t *mission, qboolean destroyed)
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
static void CP_MissionRemove (mission_t *mission)
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
static inline void CP_MissionDisableTimeLimit (mission_t *mission)
{
	mission->finalDate.day = 0;
}

/**
 * @brief Check if mission should end because of limited time.
 * @note This function is used for better readibility.
 * @sa CP_MissionDisableTimeLimit
 * @return qtrue if function should end after finalDate
 */
static inline qboolean CP_CheckMissionLimitedInTime (const mission_t *mission)
{
	return mission->finalDate.day != 0;
}

/**
 * @brief Sread XVI at a given position.
 * @param[in] pos Position where XVI should be spread.
 */
static void CP_SpreadXVIAtPos (const vec2_t pos)
{
	nation_t *nation;

#ifdef DEBUG
	if (!ccs.XVISpreadActivated) {
		Com_Printf("CP_SpreadXVIAtPos: Try to spread XVI but XVISpread is not activated\n");
		return;
	}
#endif

	nation = MAP_GetNation(pos);
	if (nation)
		nation->stats[0].xviInfection++;

	R_IncreaseXVILevel(pos);
}

/**
 * @brief Return the average XVI rate
 * @note XVI = eXtraterrestial Viral Infection
 */
static int CP_GetAverageXVIRate (void)
{
	int XVIRate = 0;
	int i;
	nation_t* nation;

	assert(gd.numNations);

	/* check for XVI infection rate */
	for (i = 0, nation = gd.nations; i < gd.numNations; i++, nation++) {
		XVIRate += nation->stats[0].xviInfection;
	}
	XVIRate /= gd.numNations;
	return (int) XVIRate;
}


static int CP_MissionChooseUFO(const mission_t *mission);

/**
 * @brief mission begins: UFO arrive on earth.
 * @note Stage 0 -- This function is common to several mission category.
 * @sa CP_MissionChooseUFO
 * @return true if mission was created, false else.
 */
static qboolean CP_MissionCreate (mission_t *mission)
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
 * @brief Minimum distance between a new mission and an existing base.
 */
static const float MIN_DIST_BASE = 4.0f;

/**
 * @brief Check if given pos is close to an existing base.
 * @return Pointer to the base if one base is closer than MIN_DIST_BASE from pos, NULL else
 */
base_t* CP_PositionCloseToBase (const vec2_t pos)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		if (MAP_GetDistance(pos, base->pos) < MIN_DIST_BASE) {
			return base;
		}
	}

	return NULL;
}

/*****	Recon Mission *****/

/**
 * @brief Recon mission is over and is a success: change interest values.
 * @note Recon mission
 */
static void CP_ReconMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.20f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	if (AB_GetAlienBaseNumber())
		CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_SUPPLY);
	if (ccs.XVISpreadActivated)
		CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_XVI);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission is over and is a failure: change interest values.
 * @note Recon mission
 */
static void CP_ReconMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission ends: UFO leave earth.
 * @note Recon mission -- Stage 2
 */
static void CP_ReconMissionLeave (mission_t *mission)
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
static void CP_ReconMissionAerial (mission_t *mission)
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
static void CP_ReconMissionGroundGo (mission_t *mission)
{
	const nation_t *nation;

	mission->stage = STAGE_MISSION_GOTO;

	/* maybe the UFO just finished a ground mission and starts a new one? */
	if (mission->ufo) {
		CP_MissionRemoveFromGeoscape(mission);
		mission->ufo->landed = qfalse;
	}

	/* Choose a map */
	if (CP_ChooseMap(mission, NULL, qfalse)) {
		int counter;
		for (counter = 0; counter < MAX_POS_LOOP; counter++) {
			if (!CP_GetRandomPosOnGeoscapeWithParameters(mission->pos, mission->mapDef->terrains, mission->mapDef->cultures, mission->mapDef->populations, NULL))
				continue;
			if (CP_PositionCloseToBase(mission->pos))
				continue;
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

	mission->mapDef->timesAlreadyUsed++;

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
static int CP_ReconMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
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
static void CP_ReconMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create Recon mission */
		CP_MissionCreate(mission);
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

/*****	Terror attack Mission *****/

/**
 * @brief Terror attack mission is over and is a success: change interest values.
 * @note Terror attack mission
 */
static void CP_TerrorMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.2f, INTERESTCATEGORY_TERROR_ATTACK);
	CL_ChangeIndividualInterest(0.03f, INTERESTCATEGORY_HARVEST);

	CP_MissionRemove(mission);
}

/**
 * @brief Terror attack mission is over and is a failure: change interest values.
 * @note Terror attack mission
 */
static void CP_TerrorMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_TERROR_ATTACK);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(0.02f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Start Terror attack mission.
 * @note Terror attack mission -- Stage 2
 */
static void CP_TerrorMissionStart (mission_t *mission)
{
	const date_t minMissionDelay = {2, 0};
	const date_t missionDelay = {3, 0};

	mission->stage = STAGE_TERROR_MISSION;

	mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	if (mission->ufo)
		CP_UFORemoveFromGeoscape(mission, qfalse);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Choose nation if needed for given mission.
 * @param[in] mission Pointer to the mission we are creating.
 * @param[out] nationList linkedList that will contain the name of the nation where the mission should take place.
 * @note nationList should be empty if no nation should be favoured.
 * @return True if nationList has been filled, false else.
 */
static qboolean CP_ChooseNation (const mission_t *mission, linkedList_t **nationList)
{
	nation_t *nation;
	int randomNumber, max = 0;
	const int OFFSET = 1;	/**< Increase this factor to make probability to select non-infected nation higher
							 * Used to make sure that non-infected nation can still be attacked */

	if (mission->ufo)
		return qfalse;

	/* favour mission with higher XVI level */
	for (nation = gd.nations; nation < gd.nations + gd.numNations; nation++)
		max += OFFSET + nation->stats[0].xviInfection;

	randomNumber = (int) (frand() * (float) max);

	/* Select the corresponding nation */
	for (nation = gd.nations; nation < gd.nations + gd.numNations; nation++) {
		randomNumber -= OFFSET + nation->stats[0].xviInfection;
		if (randomNumber < 0) {
			LIST_AddString(nationList, nation->id);
			return qtrue;
		}
	}

	return qfalse;
}

/**
 * @brief Choose a city for terror mission.
 * @return chosen city in gd.cities
 */
static const city_t* CP_ChooseCity (void)
{
	int randnumber = rand() % gd.numCities;

	return &gd.cities[randnumber];
}

/**
 * @brief Set Terror attack mission, and go to Terror attack mission pos.
 * @note Terror attack mission -- Stage 1
 * @note Terror missions can only take place in city: pick one in gd.cities.
 */
static void CP_TerrorMissionGo (mission_t *mission)
{
	const nation_t *nation;
	int counter;

	mission->stage = STAGE_MISSION_GOTO;

	/* Choose a map */
	for (counter = 0; counter < MAX_POS_LOOP; counter++) {
		const city_t const *city = CP_ChooseCity();

		if (CP_PositionCloseToBase(city->pos))
			continue;

		if (!CP_ChooseMap(mission, city->pos, qfalse))
			continue;

		Vector2Set(mission->pos, city->pos[0], city->pos[1]);
		Com_sprintf(mission->location, sizeof(mission->location), "%s", _(city->name));
		break;
	}
	if (counter >= MAX_POS_LOOP) {
		Com_Printf("CP_TerrorMissionGo: Error, could not set position.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->mapDef->timesAlreadyUsed++;

	nation = MAP_GetNation(mission->pos);

	if (mission->ufo) {
		CP_MissionDisableTimeLimit(mission);
		UFO_SendToDestination(mission->ufo, mission->pos);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}

/**
 * @brief Fill an array with available UFOs for Terror attack mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Terror attack mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
static int CP_TerrorMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;
	const float CORRUPTER_PROBABILITY = 0.2;	/**< Probability to get a corrupter. Note that the probability
													to get a corrupter among all possible UFO is this number
													divided by the number of possible UFO */

	ufoTypes[num++] = UFO_SCOUT;
	ufoTypes[num++] = UFO_FIGHTER;

	/* give a small chance to get a corrupter when XVI spreading started */
	if (ccs.XVISpreadActivated && (frand() < CORRUPTER_PROBABILITY))
		ufoTypes[num++] = UFO_CORRUPTER;

	return num;
}

/**
 * @brief Determine what action should be performed when a Terror attack mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_TerrorMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create Terror attack mission */
		CP_MissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Go to mission */
		CP_TerrorMissionGo(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on a new Terror attack mission: start it */
		CP_TerrorMissionStart(mission);
		break;
	case STAGE_TERROR_MISSION:
		/* Leave earth */
		CP_ReconMissionLeave(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_TerrorMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_TerrorMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}

/*****	Base attack Mission *****/

/**
 * @brief Base attack mission is over and is a success (from an alien point of view): change interest values.
 * @note Base attack mission
 * @sa CP_BaseAttackMissionStart
 */
static void CP_BaseAttackMissionIsSuccess (mission_t *mission)
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
static void CP_BaseAttackMissionIsFailure (mission_t *mission)
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
static void CP_BaseAttackMissionStart (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.7f, INTERESTCATEGORY_BASE_ATTACK);
}

/**
 * @brief Base attack mission ends: UFO leave earth.
 * @note Base attack mission -- Stage 3
 */
static void CP_BaseAttackMissionLeave (mission_t *mission)
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
static void CP_BaseAttackStartMission (mission_t *mission)
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
	MN_PushMenu("popup_baseattack");
}

/**
 * @brief Check and start baseattack missions
 * @sa CP_BaseAttackStartMission
 */
static void CP_CheckBaseAttacks_f (void)
{
	linkedList_t *missionlist = ccs.missions;
	base_t *base = NULL;

	if (Cmd_Argc() == 2) {
		base = B_GetFoundedBaseByIDX(atoi(Cmd_Argv(1)));
	}

	while (missionlist) {
		mission_t *mission = (mission_t*) missionlist->data;

		if (mission->category == INTERESTCATEGORY_BASE_ATTACK && mission->stage == STAGE_BASE_ATTACK) {
			if (base && ((base_t*) mission->data != base))
				continue;
			CP_BaseAttackStartMission(mission);
		}
		missionlist = missionlist->next;
	}
}

/**
 * @brief Choose Base that will be attacked, and add it to mission description.
 * @note Base attack mission -- Stage 1
 * @return Pointer to the base, NULL if no base set
 */
static base_t* CP_BaseAttackChooseBase (const mission_t *mission)
{
	float randomNumber, sum = 0.0f;
	int baseIdx;
	base_t *base = NULL;

	assert(mission);

	/* Choose randomly a base depending on alienInterest values for those bases */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		sum += base->alienInterest;
	}
	randomNumber = frand() * sum;
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		randomNumber -= base->alienInterest;
		if (randomNumber < 0)
			break;
	}

	/* Make sure we have a base */
	assert(base && (randomNumber < 0));

	/* base is already under attack */
	if (base->baseStatus == BASE_UNDER_ATTACK)
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

	base = CP_BaseAttackChooseBase(mission);
	if (!base) {
		Com_Printf("CP_BaseAttackGoToBase: no base found\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data = (void *)base;

	mission->mapDef = Com_GetMapDefinitionByID("baseattack");
	if (!mission->mapDef) {
		CP_MissionRemove(mission);
		Sys_Error("Could not find mapdef baseattack");
		return;
	}

	Vector2Copy(base->pos, mission->pos);

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
static int CP_BaseAttackMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;
#if 0
	/** @todo use me when we have a geoscape model for bomber, as well as at least one map using it */
	const int BOMBER_INTEREST_MIN = 500;	/**< Minimum value interest to have a mission spawned by bomber */
#endif

	ufoTypes[num++] = UFO_FIGHTER;

#if 0
	/** @todo use me when we have a geoscape model for bomber, as well as at least one map using it */
	if (mission->initialOverallInterest > BOMBER_INTEREST_MIN)
		ufoTypes[num++] = UFO_BOMBER;
#endif

	return num;
}

/**
 * @brief Determine what action should be performed when a Base Attack mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_BaseAttackMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create mission */
		CP_MissionCreate(mission);
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
		CP_BaseAttackMissionLeave(mission);
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

/*****	Build Base Mission *****/

/**
 * @brief Build Base mission is over and is a success (from an alien point of view): change interest values.
 * @note Build Base mission
 */
static void CP_BuildBaseMissionIsSuccess (mission_t *mission)
{
	if (mission->initialOverallInterest < STARTING_BASEBUILD_INTEREST) {
		/* This is a subverting government mission */
		CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	} else {
		/* An alien base has been built */
		const alienBase_t *base = (alienBase_t *) mission->data;
		assert(base);
		CP_SpreadXVIAtPos(base->pos);

		CL_ChangeIndividualInterest(+0.4f, INTERESTCATEGORY_XVI);
		CL_ChangeIndividualInterest(+0.4f, INTERESTCATEGORY_SUPPLY);
		CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_HARVEST);
	}

	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission is over and is a failure (from an alien point of view): change interest values.
 * @note Build Base mission
 */
static void CP_BuildBaseMissionIsFailure (mission_t *mission)
{
	/* Restore some alien interest for build base that has been removed when mission has been created */
	CL_ChangeIndividualInterest(0.5f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission just started: change interest values.
 * @note This function is intended to avoid the building of several alien bases at the same time
 */
static void CP_BuildBaseMissionStart (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.7f, INTERESTCATEGORY_BUILDING);
}

/**
 * @brief Alien base has been destroyed: change interest values.
 * @note Build Base mission
 */
static void CP_BuildBaseMissionBaseDestroyed (mission_t *mission)
{
	CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(+0.3f, INTERESTCATEGORY_INTERCEPT);

	AB_DestroyBase((alienBase_t *) mission->data);
	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission ends: UFO leave earth.
 * @note Build Base mission -- Stage 3
 */
static void CP_BuildBaseMissionLeave (mission_t *mission)
{
	assert(mission->ufo);

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
	const date_t minBuildingTime = {5, 0};	/**< Time needed to start a new base construction */
	const date_t buildingTime = {10, 0};	/**< Time needed to start a new base construction */

	assert(mission->ufo);

	mission->stage = STAGE_BUILD_BASE;

	mission->finalDate = Date_Add(ccs.date, Date_Random(minBuildingTime, buildingTime));

	base = AB_BuildBase(mission->pos);
	if (!base) {
		Com_Printf("CP_BuildBaseSetUpBase: could not create base\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data = (void *) base;

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
static void CP_BuildBaseGovernmentLeave (mission_t *mission)
{
	nation_t *nation;

	assert(mission);
	assert(mission->ufo);

	mission->stage = STAGE_RETURN_TO_ORBIT;

	/* Mission is a success: government is subverted => lower happiness */
	nation = MAP_GetNation(mission->pos);
	/** @todo when the mission is created, we should select a position where nation exists,
	 * otherwise suverting a government is meaningless */
	if (nation)
		NAT_SetHappiness(nation, nation->stats[0].happiness + HAPPINESS_SUBVERSION_LOSS);

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

	mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission, qfalse);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Choose if the mission should be an alien infiltration or a build base mission.
 * @note Build Base mission -- Stage 1
 */
static void CP_BuildBaseChooseMission (mission_t *mission)
{
	if (mission->initialOverallInterest < STARTING_BASEBUILD_INTEREST)
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
static int CP_BuildBaseMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;

	if (mission->initialOverallInterest < STARTING_BASEBUILD_INTEREST) {
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
static void CP_BuildBaseMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create mission */
		CP_MissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Choose type of mission */
		CP_BuildBaseChooseMission(mission);
		break;
	case STAGE_MISSION_GOTO:
		if (mission->initialOverallInterest < STARTING_BASEBUILD_INTEREST)
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
		CP_BuildBaseGovernmentLeave(mission);
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

/*****	Supply Mission *****/

/**
 * @brief Supply mission is over and is a success (from an alien point of view): change interest values.
 * @note Supply mission
 */
static void CP_SupplyMissionIsSuccess (mission_t *mission)
{
	alienBase_t *base;
	CL_ChangeIndividualInterest(-0.2f, INTERESTCATEGORY_SUPPLY);

	/* Spread XVI */
	base = (alienBase_t *) mission->data;
	assert(base);
	CP_SpreadXVIAtPos(base->pos);

	CP_MissionRemove(mission);
}

/**
 * @brief Supply mission is over and is a failure (from an alien point of view): change interest values.
 * @note Supply mission
 */
static void CP_SupplyMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_SUPPLY);
	CL_ChangeIndividualInterest(0.3f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Supply mission ends: UFO leave earth.
 * @note Supply mission -- Stage 3
 */
static void CP_SupplyMissionLeave (mission_t *mission)
{
	assert(mission->ufo);

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

	mission->stage = STAGE_SUPPLY;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->finalDate = Date_Add(ccs.date, Date_Random(minSupplyTime, supplyTime));

	AB_SupplyBase((alienBase_t *) mission->data, mission->ufo->detected);

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
	assert(mission->ufo);

	mission->stage = STAGE_MISSION_GOTO;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->data = (void *) AB_ChooseBaseToSupply(mission->pos);

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
			Com_Printf("CP_SupplyMissionCreate: Could not add UFO '%s', remove mission\n", UFO_TypeToShortName(ufoType));
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
static int CP_SupplyMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;

	ufoTypes[num++] = UFO_SUPPLY;

	return num;
}

/**
 * @brief Determine what action should be performed when a Supply mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_SupplyMissionNextStage (mission_t *mission)
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

/*****	XVI Spreading Mission *****/

/**
 * @brief XVI Spreading mission is over and is a success: change interest values.
 * @note XVI Spreading mission
 */
static void CP_XVIMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.1f, INTERESTCATEGORY_XVI);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);

	CP_MissionRemove(mission);
}

/**
 * @brief XVI Spreading mission is over and is a failure: change interest values.
 * @note XVI Spreading mission
 */
static void CP_XVIMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_XVI);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(0.01f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Start XVI Spreading mission.
 * @note XVI Spreading mission -- Stage 2
 */
static void CP_XVIMissionStart (mission_t *mission)
{
	const date_t minMissionDelay = {2, 0};
	const date_t missionDelay = {3, 0};

	mission->stage = STAGE_SPREAD_XVI;

	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);

	if (mission->ufo) {
		mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
		/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
		CP_UFORemoveFromGeoscape(mission, qfalse);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}

/**
 * @brief Fill an array with available UFOs for XVI Spreading mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note XVI Spreading mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
static int CP_XVIMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;

	ufoTypes[num++] = UFO_SCOUT;
	ufoTypes[num++] = UFO_FIGHTER;
	ufoTypes[num++] = UFO_CORRUPTER;

	return num;
}

/** @todo Remove me when CP_XVIMissionGo will be implemented
 * This function should take a location close to an XVI infection point
 * see gameplay proposal on wiki */
static void CP_HarvestMissionGo(mission_t *mission);

/**
 * @brief Determine what action should be performed when a XVI Spreading mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_XVIMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create XVI Spreading mission */
		CP_MissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Go to mission */
		CP_HarvestMissionGo(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on a new XVI Spreading mission: start it */
		CP_XVIMissionStart(mission);
		break;
	case STAGE_SPREAD_XVI:
		/* Leave earth */
		CP_ReconMissionLeave(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_XVIMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_XVIMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}

/*****	Intercept Mission *****/

/**
 * @brief Intercept mission is over and is a success: change interest values.
 * @note Intercept mission
 */
static void CP_InterceptMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.3f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	CL_ChangeIndividualInterest(-0.05f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);

	CP_MissionRemove(mission);
}

/**
 * @brief Intercept mission is over and is a failure: change interest values.
 * @note Intercept mission
 */
static void CP_InterceptMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Intercept mission ends: UFO leave earth.
 * @param[in] mission Pointer to the mission
 * @param[in] destroyed true if the UFO actually destroyed the installation, false else
 * @note Intercept mission -- Stage 3
 */
static void CP_InterceptMissionLeave (mission_t *mission, qboolean destroyed)
{
	installation_t *installation;

	assert(mission->ufo);

	mission->stage = STAGE_RETURN_TO_ORBIT;

	/* if the mission was an attack of an installation, destroy it */
	installation = mission->data;
	if (destroyed && installation && installation->founded && VectorCompareEps(mission->pos, installation->pos, UFO_EPSILON)) {
		INS_DestroyInstallation(installation);
	}

	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	CP_MissionRemoveFromGeoscape(mission);
	/* Display UFO on geoscape if it is detected */
	mission->ufo->landed = qfalse;
}

/**
 * @brief UFO starts to attack the installation.
 * @note Intercept mission -- Stage 2
 */
static void CP_InterceptAttackInstallation (mission_t *mission)
{
	const date_t minAttackDelay = {0, 3600};
	const date_t attackDelay = {0, 21600};		/* How long the UFO should stay on earth */
	installation_t *installation;

	mission->stage = STAGE_INTERCEPT;

	/* Check that installation is not already destroyed */
	installation = (installation_t *)mission->data;
	if (!installation->founded) {
		mission->finalDate = ccs.date;
		return;
	}

	/* Maybe installation has been destroyed, but rebuild somewhere else? */
	if (!VectorCompareEps(mission->pos, installation->pos, UFO_EPSILON)) {
		mission->finalDate = ccs.date;
		return;
	}

	/* Make round around the position of the mission */
	UFO_SetRandomDestAround(mission->ufo, mission->pos);
	mission->finalDate = Date_Add(ccs.date, Date_Random(minAttackDelay, attackDelay));
}

/**
 * @brief Set Intercept mission: UFO looks for new aircraft target.
 * @note Intercept mission -- Stage 1
 */
static void CP_InterceptAircraftMissionSet (mission_t *mission)
{
	const date_t minReconDelay = {3, 0};
	const date_t reconDelay = {6, 0};		/* How long the UFO should stay on earth */

	mission->stage = STAGE_INTERCEPT;
	mission->finalDate = Date_Add(ccs.date, Date_Random(minReconDelay, reconDelay));
}

/**
 * @brief Choose Base that will be attacked, and add it to mission description.
 * @note Base attack mission -- Stage 1
 * @return Pointer to the base, NULL if no base set
 */
static installation_t* CP_InterceptChooseInstallation (const mission_t *mission)
{
	float randomNumber, sum = 0.0f;
	int installationIdx;
	installation_t *installation = NULL;

	assert(mission);

	/* Choose randomly a base depending on alienInterest values for those bases */
	for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
		installation = INS_GetFoundedInstallationByIDX(installationIdx);
		if (!installation)
			continue;
		sum += installation->alienInterest;
	}
	randomNumber = frand() * sum;
	for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
		installation = INS_GetFoundedInstallationByIDX(installationIdx);
		if (!installation)
			continue;
		randomNumber -= installation->alienInterest;
		if (randomNumber < 0)
			break;
	}

	/* Make sure we have a base */
	assert(installation && (randomNumber < 0));

	return installation;
}

/**
 * @brief Set Intercept mission: UFO chooses an installation an flies to it.
 * @note Intercept mission -- Stage 1
 */
static void CP_InterceptGoToInstallation (mission_t *mission)
{
	installation_t *installation;
	assert(mission->ufo);

	mission->stage = STAGE_MISSION_GOTO;

	installation = CP_InterceptChooseInstallation(mission);
	if (!installation) {
		Com_Printf("CP_InterceptGoToInstallation: no installation found\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data = (void *)installation;

	Vector2Copy(installation->pos, mission->pos);

	Com_sprintf(mission->location, sizeof(mission->location), "%s", installation->name);

	CP_MissionDisableTimeLimit(mission);
	UFO_SendToDestination(mission->ufo, mission->pos);
}

/**
 * @brief Set Intercept mission: choose between attacking aircraft or installations.
 * @note Intercept mission -- Stage 1
 */
static void CP_InterceptMissionSet (mission_t *mission)
{
	assert(mission->ufo);

	/* Only harvesters can attack installations -- if there are installations to attack */
	if (mission->ufo->ufotype == UFO_HARVESTER && gd.numInstallations) {
		CP_InterceptGoToInstallation(mission);
	}

	CP_InterceptAircraftMissionSet(mission);
}

/**
 * @brief Fill an array with available UFOs for Intercept mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Intercept mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
static int CP_InterceptMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;
	const float HARVESTER_PROBABILITY = 0.25;	/**< Probability to get a harvester. Note that the probability
													to get a corrupter among all possible UFO is this number
													divided by the number of possible UFO */

	ufoTypes[num++] = UFO_FIGHTER;

	/* don't make attack on installation happens too often */
	if (frand() < HARVESTER_PROBABILITY)
		ufoTypes[num++] = UFO_HARVESTER;

	return num;
}

/**
 * @brief Determine what action should be performed when a Intercept mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_InterceptNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create Intercept mission */
		CP_MissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* UFO start looking for target */
		CP_InterceptMissionSet(mission);
		break;
	case STAGE_MISSION_GOTO:
		CP_InterceptAttackInstallation(mission);
		break;
	case STAGE_INTERCEPT:
		assert(mission->ufo);
		/* Leave earth */
		if (AIRFIGHT_ChooseWeapon(mission->ufo->weapons, mission->ufo->maxWeapons, mission->ufo->pos, mission->ufo->pos) !=
			AIRFIGHT_WEAPON_CAN_NEVER_SHOOT && mission->ufo->status == AIR_UFO && !mission->data) {
			/* UFO is fighting and has still ammo, wait a little bit before leaving (UFO is not attacking an installation) */
			const date_t AdditionalDelay = {1, 0};	/* 1 day */
			mission->finalDate = Date_Add(ccs.date, AdditionalDelay);
		} else
			CP_InterceptMissionLeave(mission, qtrue);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_InterceptMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_InterceptNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}


/**
 * @brief Notify missions that an installation has been destroyed.
 * @param[in] installation Pointer to the installation that has been destroyed.
 */
void CP_MissionNotifyInstallationDestroyed (const installation_t const *installation)
{
	linkedList_t *missionlist = ccs.missions;

	while (missionlist) {
		mission_t *mission = (mission_t*) missionlist->data;

		if (mission->category == INTERESTCATEGORY_INTERCEPT && mission->data) {
			if ((installation_t*) mission->data != installation)
				continue;
			CP_InterceptMissionLeave(mission, qfalse);
		}
		missionlist = missionlist->next;
	}
}

/*****	Harvesting Mission *****/

/**
 * @brief Harvesting mission is over and is a success: change interest values.
 * @note Harvesting mission
 */
static void CP_HarvestMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(-0.1f, INTERESTCATEGORY_HARVEST);
	if (ccs.XVISpreadActivated)
		CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_XVI);

	CP_MissionRemove(mission);
}

/**
 * @brief Harvesting mission is over and is a failure: change interest values.
 * @note Harvesting mission
 */
static void CP_HarvestMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_HARVEST);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(0.02f, INTERESTCATEGORY_BASE_ATTACK);

	CP_MissionRemove(mission);
}

/**
 * @brief Start Harvesting mission.
 * @note Harvesting mission -- Stage 2
 */
static void CP_HarvestMissionStart (mission_t *mission)
{
	const date_t minMissionDelay = {2, 0};
	const date_t missionDelay = {3, 0};

	mission->stage = STAGE_HARVEST;

	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);

	if (mission->ufo) {
		mission->finalDate = Date_Add(ccs.date, Date_Random(minMissionDelay, missionDelay));
		/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
		CP_UFORemoveFromGeoscape(mission, qfalse);
	} else {
		/* Go to next stage on next frame */
		mission->finalDate = ccs.date;
	}
}


/**
 * @brief Set Harvest mission, and go to mission pos.
 * @note Harvesting attack mission -- Stage 1
 */
static void CP_HarvestMissionGo (mission_t *mission)
{
	const nation_t *nation;

	mission->stage = STAGE_MISSION_GOTO;

	/* Choose a map */
	if (CP_ChooseMap(mission, NULL, qfalse)) {
		int counter;
		linkedList_t *nationList = NULL;
		const qboolean nationTest = CP_ChooseNation(mission, &nationList);
		for (counter = 0; counter < MAX_POS_LOOP; counter++) {
			if (!CP_GetRandomPosOnGeoscapeWithParameters(mission->pos, mission->mapDef->terrains, mission->mapDef->cultures, mission->mapDef->populations, nationTest ? nationList : NULL))
				continue;
			if (CP_PositionCloseToBase(mission->pos))
				continue;
			break;
		}
		if (counter >= MAX_POS_LOOP) {
			Com_Printf("CP_HarvestMissionGo: Error, could not set position.\n");
			CP_MissionRemove(mission);
			return;
		}
		LIST_Delete(&nationList);
	} else {
		Com_Printf("CP_HarvestMissionGo: No map found, remove mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->mapDef->timesAlreadyUsed++;

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
 * @brief Fill an array with available UFOs for Harvesting mission type.
 * @param[in] mission Pointer to the mission we are currently creating.
 * @param[out] ufoTypes Array of ufoType_t that may be used for this mission.
 * @note Harvesting mission -- Stage 0
 * @return number of elements written in @c ufoTypes
 */
static int CP_HarvestMissionAvailableUFOs (const mission_t const *mission, int *ufoTypes)
{
	int num = 0;

	ufoTypes[num++] = UFO_HARVESTER;

	return num;
}

/**
 * @brief Determine what action should be performed when a Harvesting mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_HarvestMissionNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		/* Create Harvesting mission */
		CP_MissionCreate(mission);
		break;
	case STAGE_COME_FROM_ORBIT:
		/* Go to mission */
		CP_HarvestMissionGo(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on a new Harvesting mission: start it */
		CP_HarvestMissionStart(mission);
		break;
	case STAGE_HARVEST:
		/* Leave earth */
		CP_ReconMissionLeave(mission);
		break;
	case STAGE_RETURN_TO_ORBIT:
		/* mission is over, remove mission */
		CP_HarvestMissionIsSuccess(mission);
		break;
	default:
		Com_Printf("CP_HarvestMissionNextStage: Unknown stage: %i, removing mission.\n", mission->stage);
		CP_MissionRemove(mission);
		break;
	}
}

/*****	General Mission Code *****/

/**
 * @brief Choose UFO type for a given mission category.
 * @param[in] mission Pointer to the mission where the UFO will be added
 * @sa CP_MissionChooseUFO
 * @sa CP_SupplyMissionCreate
 * @return ufoType_t of the UFO spawning the mission, UFO_MAX if the mission is spawned from ground
 */
static int CP_MissionChooseUFO (const mission_t *mission)
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
static void CP_MissionStageEnd (mission_t *mission)
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
static void CP_MissionIsOver (mission_t *mission)
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
static void CP_CreateNewMission (interestCategory_t category, qboolean beginNow)
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

/** @brief Number of days between events are spawned */
static const int DELAY_BETWEEN_MISSION_SPAWNING = 3;

/**
 * @brief Spawn new missions.
 * @sa CL_CampaignRun
 * @note daily called
 */
static void CP_SpawnNewMissions (void)
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
static void CP_InitializeSpawningDelay (void)
{
	ccs.lastMissionSpawnedDelay = DELAY_BETWEEN_MISSION_SPAWNING;

	CP_SpawnNewMissions();
}

/**
 * @brief Start XVI spreading in campaign.
 * @note This is called when 'a new twist' technology is discovered
 */
static void CP_StartXVISpreading_f (void)
{
	int i, numAlienBases;

	/** @todo ccs.XVIShowMap should not be enabled at the same time than
	 * ccs.XVISpreadActivated: ccs.XVIShowMap means that PHALANX has a map of
	 * XVI, whereas ccs.XVISpreadActivated  means that aliens started
	 * spreading XVI */
	ccs.XVISpreadActivated = qtrue;
	ccs.XVIShowMap = qtrue;

	/* Spawn a few alien bases depending on difficulty level */
	if (difficulty->integer > 0)
		numAlienBases = 3;
	else if (difficulty->integer < 0)
		numAlienBases = 1;
	else
		numAlienBases = 2;

	for (i = 0; i < numAlienBases; i++)
		CP_CreateNewMission(INTERESTCATEGORY_BUILDING, qfalse);
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
		break;
	case INTERESTCATEGORY_RECON:
		return "Recon Mission";
		break;
	case INTERESTCATEGORY_TERROR_ATTACK:
		return "Terror mission";
		break;
	case INTERESTCATEGORY_BASE_ATTACK:
		return "Base attack";
		break;
	case INTERESTCATEGORY_BUILDING:
		return "Building Base or Subverting Government";
		break;
	case INTERESTCATEGORY_SUPPLY:
		return "Supply base";
		break;
	case INTERESTCATEGORY_XVI:
		return "XVI propagation";
		break;
	case INTERESTCATEGORY_INTERCEPT:
		return "Interception";
		break;
	case INTERESTCATEGORY_HARVEST:
		return "Harvest";
		break;
	case INTERESTCATEGORY_MAX:
		return "Unknown mission category";
		break;
	}
	assert(0);
	return "";
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
static void CL_DebugMissionList_f (void)
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

/**
 * @brief Check if a stage mission is over when UFO reached destination.
 * @param[in] ufocraft Pointer to the ufo that reached destination
 * @sa UFO_CampaignRunUFOs
 */
void CP_CheckNextStageDestination (aircraft_t *ufocraft)
{
	mission_t *mission;

	mission = ufocraft->mission;
	assert(mission);

	switch (mission->stage) {
	case STAGE_COME_FROM_ORBIT:
	case STAGE_RETURN_TO_ORBIT:
	case STAGE_MISSION_GOTO:
		CP_MissionStageEnd(mission);
		break;
	default:
		/* Do nothing */
		break;
	}
}

/**
 * @brief Make UFO proceed with its mission when the is no more target.
 * @param[in] ufocraft Pointer to the ufo that should proceed a mission.
 */
void CP_UFOProceedMission (aircraft_t *ufocraft)
{
	/* Every UFO on geoscape should have a mission assigned */
	assert(ufocraft->mission);

	if (ufocraft->mission->category == INTERESTCATEGORY_INTERCEPT && !ufocraft->mission->data) {
		/* Interception mission is over */
		ufocraft->status = AIR_TRANSIT;			/* continue stage */
		CP_MissionStageEnd(ufocraft->mission);
	} else if (ufocraft->mission->stage < STAGE_MISSION_GOTO ||
		ufocraft->mission->stage >= STAGE_RETURN_TO_ORBIT) {
		UFO_SetRandomDest(ufocraft);
	} else {
		UFO_SendToDestination(ufocraft, ufocraft->mission->pos);
	}
}

/**
 * @brief Spawn a new crash site after a UFO has been destroyed.
 */
void CP_SpawnCrashSiteMission (aircraft_t *ufo)
{
	const nation_t *nation;
	mission_t *mission;

	mission = ufo->mission;
	if (!mission)
		Sys_Error("CP_SpawnCrashSiteMission: No mission correspond to ufo '%s'", ufo->id);

	assert(mission);

	if (!CP_ChooseMap(mission, ufo->pos, qtrue)) {
		Com_Printf("CP_SpawnCrashSiteMission: No map found, remove mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	Vector2Copy(ufo->pos, mission->pos);

	nation = MAP_GetNation(mission->pos);
	if (nation) {
		Com_sprintf(mission->location, sizeof(mission->location), "%s", _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), "%s", _("No nation"));
	}

	CP_MissionDisableTimeLimit(mission);
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array
	  (may be used to know what items are collected from battlefield)*/
	CP_UFORemoveFromGeoscape(mission, qfalse);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Spawn a new alien base mission after it has been discovered.
 */
void CP_SpawnAlienBaseMission (alienBase_t *alienBase)
{
	mission_t *mission;

	CP_CreateNewMission(INTERESTCATEGORY_BUILDING, qtrue);
	mission = CP_GetLastMissionAdded();
	if (!mission) {
		Com_Printf("CP_SpawnAlienBaseMission: Could not add mission, abort\n");
		return;
	}

	mission->stage = STAGE_BASE_DISCOVERED;
	mission->data = (void *) alienBase;

	mission->mapDef = Com_GetMapDefinitionByID("alienbase");
	if (!mission->mapDef)
		Sys_Error("Could not find mapdef alienbase");

	Vector2Copy(alienBase->pos, mission->pos);

	Com_sprintf(mission->location, sizeof(mission->location), _("Alien base"));

	/* Alien base stay until it's destroyed */
	CP_MissionDisableTimeLimit(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}

/**
 * @brief Find mission corresponding to idx
 */
mission_t* MAP_GetMissionByIdx (int id)
{
	int i;
	const linkedList_t *list = ccs.missions;

	for (i = 0; list; list = list->next, i++) {
		mission_t *mission = (mission_t *)list->data;
		if (i == id)
			return mission;
	}

	Com_Printf("MAP_GetMissionByIdx: No mission of id %i\n", id);
	return NULL;
}

/**
 * @brief Find idx corresponding to mission
 */
int MAP_GetIdxByMission (const mission_t *mis)
{
	int i;
	const linkedList_t *list = ccs.missions;

	for (i = 0; list; list = list->next, i++) {
		mission_t *mission = (mission_t *)list->data;
		if (mission == mis)
			return i;
	}

	Com_Printf("MAP_GetIdxByMission: Mission does not exist\n");
	return 0;
}

/*****************************************************************************
Set Battle
*****************************************************************************/

/**
 * @brief Check if UFO of selected mission is crashed.
 * @param[in] mission Pointer to the mission that generates the battle
 * @sa CP_CreateBattleParameters
 * @return true if the UFO is crashed, false if it's intact
 */
static qboolean CP_UFOIsCrashed (const mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_RECON_GROUND:
	case STAGE_TERROR_MISSION:
	case STAGE_BUILD_BASE:
	case STAGE_BASE_ATTACK:
	case STAGE_SUBVERT_GOV:
	case STAGE_SUPPLY:
	case STAGE_SPREAD_XVI:
	case STAGE_HARVEST:
		return qfalse;
	default:
		return qtrue;
	}
}

/**
 * @brief Returns the alien race for a mission (depends on the interest values)
 * @return Alien team id string (team_aliens.ufo)
 */
static void CP_SetAlienTeamByInterest (const mission_t *mission)
{
	int i, level = 0;
	int alienTeam = ALIENTEAM_DEFAULT;

	switch (mission->category) {
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Sys_Error("CP_GetAlienByInterest: Try to set alien team for invalid mission category '%i'\n", mission->category);
	case INTERESTCATEGORY_HARVEST:
		alienTeam = ALIENTEAM_HARVEST;
		break;
	case INTERESTCATEGORY_XVI:
		alienTeam = ALIENTEAM_XVI;
		break;
	case INTERESTCATEGORY_TERROR_ATTACK:
	case INTERESTCATEGORY_BASE_ATTACK:
		level = 1;
		break;
	case INTERESTCATEGORY_RECON:
		break;
	case INTERESTCATEGORY_BUILDING:
	case INTERESTCATEGORY_SUPPLY:
	case INTERESTCATEGORY_INTERCEPT:
		level = -1;
		break;
	}

	/* Choose the level of the alien team:
	 * use mission->initialOverallInterest and not ccs.overallInterest: the alien team should not change depending on
	 * when you encounter it
	 * Use the last level only if player goes over FINAL_OVERALL_INTEREST overall interest or if category mission increases the level */
	level += mission->initialOverallInterest / FINAL_OVERALL_INTEREST * (gd.numAlienTeams[alienTeam] - 2);

	/* Check level boundary */
	if (level < 0)
		level = 0;
	else if (level >= gd.numAlienTeams[alienTeam])
		level = gd.numAlienTeams[alienTeam] - 1;

	/* Copy the alien team definition to the battle parameters struct */
	ccs.battleParameters.numAlienTeams = 0;
	for (i = 0; i < ALIENTEAM_MAX; i++) {
		if (!gd.alienTeams[alienTeam][level][i])
			break;
		ccs.battleParameters.alienTeams[i] = gd.alienTeams[alienTeam][level][i];
		ccs.battleParameters.numAlienTeams++;
	}
}

/**
 * @brief Set alien equipment for a mission (depends on the interest values)
 * @note This function is used to know which equipment pack described in equipment_missions.ufo should be used
 */
static void CP_SetAlienEquipmentByInterest (const mission_t *mission)
{
	int i, randomNum, availableEquipDef = 0;

	Com_sprintf(ccs.battleParameters.alienEquipment, sizeof(ccs.battleParameters.alienEquipment), "alien_");

	switch (mission->category) {
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Sys_Error("CP_GetAlienByInterest: Try to set alien team for invalid mission category '%i'\n", mission->category);
	case INTERESTCATEGORY_XVI:
	case INTERESTCATEGORY_TERROR_ATTACK:
	case INTERESTCATEGORY_BASE_ATTACK:
	case INTERESTCATEGORY_INTERCEPT:
		Q_strcat(ccs.battleParameters.alienEquipment, "soldiers", sizeof(ccs.battleParameters.alienEquipment));
		break;
	case INTERESTCATEGORY_HARVEST:
	case INTERESTCATEGORY_RECON:
	case INTERESTCATEGORY_BUILDING:
	case INTERESTCATEGORY_SUPPLY:
		Q_strcat(ccs.battleParameters.alienEquipment, "workers", sizeof(ccs.battleParameters.alienEquipment));
		break;
	}

	/* look for all available alien equipement definitions
	 * use mission->initialOverallInterest and not ccs.overallInterest: the alien equipment should not change depending on
	 * when you encounter it */
	for (i = 0; i < csi.numEDs; i++) {
		if (mission->initialOverallInterest <= csi.eds[i].maxInterest &&
			mission->initialOverallInterest >= csi.eds[i].minInterest &&
			!Q_strncmp(csi.eds[i].name, ccs.battleParameters.alienEquipment, 13)) {
			availableEquipDef++;
		}
	}

	/* Choose an alien equipment definition -- between 0 and availableStage - 1 */
	randomNum = rand() % availableEquipDef;

	availableEquipDef = 0;
	for (i = 0; i < csi.numEDs; i++) {
		if (mission->initialOverallInterest <= csi.eds[i].maxInterest &&
			mission->initialOverallInterest >= csi.eds[i].minInterest &&
			!Q_strncmp(csi.eds[i].name, ccs.battleParameters.alienEquipment, 13)) {
			if (availableEquipDef == randomNum) {
				Com_sprintf(ccs.battleParameters.alienEquipment, sizeof(ccs.battleParameters.alienEquipment), "%s", csi.eds[i].name);
				break;
			} else
				availableEquipDef++;
		}
	}
}

/**
 * @brief Create alien team.
 * @param[in] mission Pointer to the mission that generates the battle
 */
static void CP_CreateAlienTeam (mission_t *mission)
{
	int numAliens;

	assert(mission->pos);

	numAliens = 4 + rand() % (2 + ccs.overallInterest / 100);
	if (numAliens > mission->mapDef->maxAliens)
		numAliens = mission->mapDef->maxAliens;
	ccs.battleParameters.aliens = numAliens;

	CP_SetAlienTeamByInterest(mission);

	CP_SetAlienEquipmentByInterest(mission);
}

/**
 * @brief Create alien team.
 * @param[in] mission Pointer to the mission that generates the battle
 */
static void CP_CreateCivilianTeam (mission_t *mission)
{
	nation_t *nation;

	assert(mission->pos);

	/** @todo I've no idea what civilian number should be, please set it to desired value -- Kracken */
	ccs.battleParameters.civilians = (frand() * 10);
	ccs.battleParameters.civilians %= 4;
	ccs.battleParameters.civilians += 1;

	nation = MAP_GetNation(mission->pos);
	ccs.battleParameters.nation = nation;
	if (nation) {
		Q_strncpyz(ccs.battleParameters.civTeam, nation->id, sizeof(ccs.battleParameters.civTeam));
	} else {
		Q_strncpyz(ccs.battleParameters.civTeam, "europe", sizeof(ccs.battleParameters.civTeam));
	}
}

/**
 * @brief Create parameters needed for battle.
 * @param[in] mission Pointer to the mission that generates the battle
 * @sa CP_CreateAlienTeam
 * @sa CP_CreateCivilianTeam
 */
static void CP_CreateBattleParameters (mission_t *mission)
{
	const char *zoneType;
	const byte *color;

	assert(mission->pos);

	/* Reset parameters */
	if (ccs.battleParameters.param) {
		Mem_Free(ccs.battleParameters.param);
		ccs.battleParameters.param = NULL;
	}

	ccs.battleParameters.mission = mission;
	color = MAP_GetColor(mission->pos, MAPTYPE_TERRAIN);
	zoneType = MAP_GetTerrainType(color);
	ccs.battleParameters.zoneType = zoneType; /* store to terrain type for texture replacement */
	/* Is there a UFO to recover ? */
	if (selectedMission->ufo) {
		const char *shortUFOType;
		const char *missionType;
		if (CP_UFOIsCrashed(mission)) {
			shortUFOType = UFO_CrashedTypeToShortName(selectedMission->ufo->ufotype);
			missionType = "cp_ufocrashed";
			/* Set random map UFO if this is a random map */
			if (mission->mapDef->map[0] == '+') {
				/* set battleParameters.param to the ufo type: used for ufocrash random map */
				if (!Q_strcmp(mission->mapDef->id, "ufocrash"))
					ccs.battleParameters.param = Mem_PoolStrDup(shortUFOType, cl_localPool, 0);
			}
		} else {
			shortUFOType = UFO_TypeToShortName(selectedMission->ufo->ufotype);
			missionType = "cp_uforecovery";
		}

		Com_sprintf(mission->onwin, sizeof(mission->onwin), "cp_uforecovery %i;", mission->ufo->ufotype);
		/* Set random map UFO if this is a random map */
		if (mission->mapDef->map[0] == '+') {
			/* set rm_ufo to the ufo type used */
			Cvar_Set("rm_ufo", va("+%s", shortUFOType));
		}
	}
	/** @todo change dropship to any possible aircraft when random assembly tiles will be created */
	/* Set random map aircraft if this is a random map */
	if (mission->mapDef->map[0] == '+')
		Cvar_Set("rm_drop", "+drop_firebird");
}

/**
 * @brief Get mission model that should be shown on the geoscape
 * @param[in] mission Pointer to the mission drawn on geoscape
 * @sa MAP_DrawMapMarkers
 */
const char* MAP_GetMissionModel (const mission_t *mission)
{
	/* Mission shouldn't be drawn on geoscape if mapDef is not defined */
	assert(mission->mapDef);

	if (mission->ufo && CP_UFOIsCrashed(mission))
		/** @todo Should be a special crashed UFO mission model */
		return "geoscape/mission";

	if (mission->mapDef->storyRelated) {
		if ((mission->category == INTERESTCATEGORY_BUILDING) && (mission->stage == STAGE_BASE_DISCOVERED))
			return "geoscape/alienbase";
		else
			/** @todo Should be a special story related mission model */
			return "geoscape/mission";
	}

	switch (mission->category) {
	/** @todo each category should have a its own model */
	case INTERESTCATEGORY_RECON:
	case INTERESTCATEGORY_XVI:
	case INTERESTCATEGORY_HARVEST:
	case INTERESTCATEGORY_TERROR_ATTACK:
	case INTERESTCATEGORY_BUILDING:
		return "geoscape/mission";
	case INTERESTCATEGORY_BASE_ATTACK:	/* Should not be reached, this mission category is not drawn on geoscape */
	case INTERESTCATEGORY_SUPPLY:		/* Should not be reached, this mission category is not drawn on geoscape */
	case INTERESTCATEGORY_INTERCEPT:	/* Should not be reached, this mission category is not drawn on geoscape */
	case INTERESTCATEGORY_NONE:			/* Should not be reached, this mission category is not drawn on geoscape */
	case INTERESTCATEGORY_MAX:			/* Should not be reached, this mission category is not drawn on geoscape */
		break;
	}
	assert(0);
	return "";
}

/**
 * @brief Spread XVI for each mission that needs to be daily spread.
 * @note Daily called
 */
static void CP_SpreadXVI (void)
{
	const linkedList_t *list = ccs.missions;

	/* don't check if XVI spreading didn't start yet */
	if (!ccs.XVISpreadActivated)
		return;

	for (;list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		if (mission->stage == STAGE_SPREAD_XVI)
			CP_SpreadXVIAtPos(mission->pos);
	}
}

/**
 * @brief Returns the alien XVI tech if the tech was already researched
 */
technology_t *CP_IsXVIResearched (void)
{
	return RS_IsResearched_ptr(rsAlienXVI) ? rsAlienXVI : NULL;
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo other stuff to back up?
 */
static void CL_BackupMonthlyData (void)
{
	int i, nat;

	/**
	 * Back up nation relationship .
	 * "inuse" is copied as well so we do not need to set it anywhere.
	 */
	for (nat = 0; nat < gd.numNations; nat++) {
		nation_t *nation = &gd.nations[nat];

		for (i = MONTHS_PER_YEAR - 1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			nation->stats[i] = nation->stats[i - 1];
		}
	}
}

/**
 * @brief Function to handle the campaign end
 */
static void CP_EndCampaign (qboolean won)
{
	CL_GameExit();
	assert(!curCampaign);

	if (won)
		Cvar_Set("mn_afterdrop", "endgame");
	else
		Cvar_Set("mn_afterdrop", "lostgame");
	Com_Drop();
}

/**
 * @brief Checks whether the player has lost the campaign
 */
static void CP_CheckLostCondition (qboolean lost, const mission_t* mission, int civiliansKilled)
{
	qboolean endCampaign = qfalse;
	/* fraction of nation that can be below min happiness before the game is lost */
	const float nationBelowLimitPercentage = 0.5f;

	assert(curCampaign);

	if (!endCampaign && ccs.credits < -curCampaign->negativeCreditsUntilLost) {
		mn.menuText[TEXT_STANDARD] = _("You've gone too far into debt.");
		endCampaign = qtrue;
	}

	/** @todo Should we make the campaign lost when a player loses all his bases?
	 * until he has set up a base again, the aliens might have invaded the whole
	 * world ;) - i mean, removing the credits check here. */
	if (!gd.numBases && ccs.credits < curCampaign->basecost - curCampaign->negativeCreditsUntilLost) {
		mn.menuText[TEXT_STANDARD] = _("You've lost your bases and don't have enough money to build new ones.");
		endCampaign = qtrue;
	}

	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > curCampaign->maxAllowedXVIRateUntilLost) {
			mn.menuText[TEXT_STANDARD] = _("You have failed in your charter to protect Earth."
				" Our home and our people have fallen to the alien infection. Only a handful"
				" of people on Earth remain human, and the remaining few no longer have a"
				" chance to stem the tide. Your command is no more; PHALANX is no longer"
				" able to operate as a functioning unit. Nothing stands between the aliens"
				" and total victory.");
			endCampaign = qtrue;
		} else {
			/* check for nation happiness */
			int j, nationBelowLimit = 0;
			for (j = 0; j < gd.numNations; j++) {
				const nation_t *nation = &gd.nations[j];
				if (nation->stats[0].happiness < curCampaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= nationBelowLimitPercentage * gd.numNations) {
				/* lost the game */
				mn.menuText[TEXT_STANDARD] = _("Under your command, PHALANX operations have"
					" consistently failed to protect nations."
					" The UN, highly unsatisfied with your performance, has decided to remove"
					" you from command and subsequently disbands the PHALANX project as an"
					" effective task force. No further attempts at global cooperation are made."
					" Earth's nations each try to stand alone against the aliens, and eventually"
					" fall one by one.");
				endCampaign = qtrue;
			}
		}
	}

	if (endCampaign) {
		CP_EndCampaign(qfalse);
	}
}

/* Initial fraction of the population in the country where a mission has been lost / won */
#define XVI_LOST_START_PERCENTAGE	0.20f
#define XVI_WON_START_PERCENTAGE	0.05f

/**
 * @brief Updates each nation's happiness and mission win/loss stats.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 * @todo This function needs to be updated/simplified once mission scoring is implemented.
 * The mission score will directly affect the nation happiness as per
 * happiness += missionScore * missionWeight
 * where missionScore is the relative score for the mission (a value between -1 and 1) and
 * missionWeight is the absolute effect this mission has (a function of the maximum absolute score possible for the mission)
 */
static void CL_HandleNationData (qboolean lost, int civiliansSurvived, int civiliansKilled, int aliensSurvived, int aliensKilled, mission_t * mis)
{
	int i, is_on_Earth = 0;
	const int civilianSum = civiliansKilled + civiliansSurvived;
	const float civilianRatio = civilianSum ? (float)civiliansSurvived / (float)civilianSum : 0.;
	const int alienSum = aliensKilled + aliensSurvived;
	const float alienRatio = alienSum ? (float)aliensKilled / (float)alienSum : 0.;
	const float performance = civilianRatio * 0.5 + alienRatio * 0.5;

	if (lost) {
		campaignStats.missionsLost++;
		ccs.civiliansKilled += civiliansKilled;
	} else
		campaignStats.missionsWon++;

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		const float difficultyModifier = (float)curCampaign->difficulty * 0.01f;
		float delta_happiness = 0.0f;

		if (lost) {
			if (nation == ccs.battleParameters.nation) {
				/* Strong negative reaction (happiness_factor must be < 0) */
				delta_happiness = - 0.3f * (1.0f - performance);
				is_on_Earth++;
			} else {
				/* Minor negative reaction (5 times lower than if the failed mission were in the nation) */
				delta_happiness = - 0.06f * (1.0f - performance);
			}
		} else {
			if (nation == ccs.battleParameters.nation) {
				/* Strong positive reaction (happiness_factor must be > 0) */
				delta_happiness = 0.3f * performance;
				is_on_Earth++;
			} else {
				/* Minor positive reaction (5 times lower than if the mission were in the nation) */
				delta_happiness = 0.06f * performance;
			}
		}
		/* One mission can only change nation happiness so much in either direction */
		/* the happiness you can gain depends on the difficulty of the campaign */
		if (delta_happiness < - HAPPINESS_MAX_MISSION_IMPACT - difficultyModifier)
			delta_happiness = - HAPPINESS_MAX_MISSION_IMPACT - difficultyModifier;
		else if (delta_happiness > HAPPINESS_MAX_MISSION_IMPACT + difficultyModifier)
			delta_happiness = HAPPINESS_MAX_MISSION_IMPACT + difficultyModifier;

		/* update happiness */
		NAT_SetHappiness(nation, nation->stats[0].happiness + delta_happiness);
	}
	if (!is_on_Earth)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Warning, mission '%s' located in an unknown country '%s'.\n", mis->id, ccs.battleParameters.nation ? ccs.battleParameters.nation->id : "no nation");
	else if (is_on_Earth > 1)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Error, mission '%s' located in many countries '%s'.\n", mis->id, ccs.battleParameters.nation->id);
}

/**
 * @brief Check for mission start, of change of status of mission
 */
static void CP_CheckEvents (void)
{
	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (CP_CheckMissionLimitedInTime(mission) && Date_LaterThan(ccs.date, mission->finalDate)) {
			CP_MissionStageEnd(mission);
		}
	}

	/* Humans start to attacking player. */
	if (ccs.overallInterest > 450) {
		ccs.humansAttackActivated = qtrue;
		/* Mark prequesite of "rs_enemy_on_earth" as met. */
		RS_ResearchFinish(RS_GetTechByID("rs_enemy_on_earth_event"));
	}

	/** XVI infection begins.
	 * This triggers cp_start_xvi_spreading (aka CP_StartXVISpreading_f)
	 * as well after news_new_twist is marked as researched)
	 * @todo We could actually call CP_StartXVISpreading_f directly here as well ... dunno if it helps?
	 *		It's at least more script-able if we keep it as-is. Not that that says much ;) */
	if (ccs.overallInterest > 400) {
		/* Mark prequesite of "news_new_twist" as met. */
		RS_ResearchFinish(RS_GetTechByID(XVI_EVENT_NAME));
	}
}

/* =========================================================== */

/**
 * @brief Converts a number of second into a char to display.
 * @param[in] second Number of second.
 * @todo Abstract the code into an extra function (DateConvertSeconds?) see also CL_DateConvertLong
 */
const char* CL_SecondConvert (int second)
{
	static char buffer[6];
	const int hour = second / SECONDS_PER_HOUR;
	const int min = (second - hour * SECONDS_PER_HOUR) / 60;
	Com_sprintf(buffer, sizeof(buffer), "%2i:%02i", hour, min);
	return buffer;
}

static const int monthLength[MONTHS_PER_YEAR] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Converts a number of days into the number of the current month and the current day in this month.
 * @note The seconds from "date" are ignored here.
 * @note The function always starts calculation from Jan. and also catches new years.
 * @param[in] date Contains the number of days to be converted.
 * @param[out] day The day in the month above.
 * @param[out] month The month. [1-12]
 */
void CL_DateConvert (const date_t * date, byte *day, byte *month, short *year)
{
	byte i;
	int d;

	/* Get the year */
	*year = date->day / DAYS_PER_YEAR;

	/* Get the days in the year. */
	d = date->day % DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	for (i = 0; i < MONTHS_PER_YEAR; i++) {
		if (d < monthLength[i])
			break;
		d -= monthLength[i];
	}

	/* Prepare return values. */
	*day = d + 1;
	*month = i + 1;	/**< Return month in range [1-12] */
	assert(*month >= 1 && *month <= MONTHS_PER_YEAR);
	assert(*day >= 1 && *day <= monthLength[i]);
}

/**
 * @brief Converts a number of years+months+days into an "day" integer as used in date_t.
 * @param[in] years The number of years to sum up.
 * @param[in] months The number of months to sum up [1-12]
 * @param[in] days The number of days to sum up.
 * @return The total number of days.
 */
int CL_DateCreateDay (const short years, const byte months, const byte days)
{
	int i;
	int day;

	/* Add days of years */
	day = DAYS_PER_YEAR * years;

	/* Add days until no full month is left. */
	for (i = 0; i < months; i++)
		day += monthLength[i];

	day += days - 1;

	return day;
}

/**
 * @brief Converts a number of hours+minutes+seconds into an "sec" integer as used in date_t.
 * @param[in] hours The number of hours to sum up.
 * @param[in] minutes The number of minutes to sum up.
 * @param[in] seconds The number of seconds to sum up.
 * @return The total number of seconds.
 */
int CL_DateCreateSeconds (byte hours, byte minutes, byte seconds)
{
	int sec;

	/* Add seconds of the hours */
	sec = SECONDS_PER_HOUR * hours;

	/* Add seconds of the minutes. */
	sec += 60 * minutes;

	/* Add the rest of the seconds */
	sec += seconds;

	return sec;
}

/**
 * @brief Converts a date from the engine in a (longer) human-readable format.
 * @param[in] date Contains the date to be converted.
 * @param[out] dateLong The converted date.
  */
void CL_DateConvertLong (const date_t * date, dateLong_t * dateLong)
{
	CL_DateConvert(date, &dateLong->day, &dateLong->month, &dateLong->year);
	/**@todo Make this conversion a function as well (DateConvertSeconds?) See also CL_SecondConvert */
	dateLong->hour = date->sec / SECONDS_PER_HOUR;
	dateLong->min = (date->sec - dateLong->hour * SECONDS_PER_HOUR) / 60;
	dateLong->sec = date->sec - dateLong->hour * SECONDS_PER_HOUR - dateLong->min * 60;
}

/**
 * @brief Returns the monatname to the given month index
 * @param[in] month The month index - [0-11]
 * @return month name as char*
 */
const char *CL_DateGetMonthName (int month)
{
	switch (month) {
	case 0:
		return _("Jan");
	case 1:
		return _("Feb");
	case 2:
		return _("Mar");
	case 3:
		return _("Apr");
	case 4:
		return _("May");
	case 5:
		return _("Jun");
	case 6:
		return _("Jul");
	case 7:
		return _("Aug");
	case 8:
		return _("Sep");
	case 9:
		return _("Oct");
	case 10:
		return _("Nov");
	case 11:
		return _("Dec");
	default:
		return "Error";
	}
}

typedef struct gameLapse_s {
	const char *name;
	int scale;
	int type;
} gameLapse_t;

#define LAPSETYPE_GEOSCAPE    1       /**< The gameLapse_t type flag for geoscape */
#define LAPSETYPE_COMBATZOOM  2       /**< The gameLapse_t type flag for combat zoom */
#define LAPSETYPE_ALL         3       /**< The gameLapse_t type flag for both geoscape and combat zoom */


#define NUM_TIMELAPSE 11

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{N_("stopped"), 0, LAPSETYPE_GEOSCAPE},
	{N_("5 sec"), 5, LAPSETYPE_ALL},
	{N_("10 sec"), 10, LAPSETYPE_COMBATZOOM},
	{N_("1 min"), 60, LAPSETYPE_COMBATZOOM},
	{N_("5 mins"), 5 * 60, LAPSETYPE_ALL},
	{N_("10 mins"), 10 * 60, LAPSETYPE_COMBATZOOM},
	{N_("20 mins"), SECONDS_PER_HOUR / 3, LAPSETYPE_GEOSCAPE},
	{N_("1 hour"), SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("12 hours"), 12 * SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("1 day"), 24 * SECONDS_PER_HOUR, LAPSETYPE_GEOSCAPE},
	{N_("5 days"), 5 * SECONDS_PER_DAY, LAPSETYPE_GEOSCAPE}
};

/** @todo move into ccs_t - and also save this? needed? */
static int gameLapse;

/**
 * @brief Gets the maximum game lapse array position that matches the given lapse type.
 * @param[in] type The game lapse type.
 * @return The Maximum game lapse array position.
 */
static int CL_MaxGameLapseForType (int type)
{
	int idx;

	for (idx = NUM_TIMELAPSE - 1; idx >= 0; idx--) {
		if ((lapse[idx].type & type) == type) {
			break;
		}
	}
	return idx;
}

/**
 * @brief Gets the minimum game lapse array position that matches the given lapse type.
 * @param[in] type The game lapse type.
 * @return The Minimum game lapse array position.
 */
static int CL_MinGameLapseForType (int type)
{
	int idx;

	for (idx = 0; idx < NUM_TIMELAPSE; idx++) {
		if ((lapse[idx].type & type) == type) {
			break;
		}
	}
	return idx;
}

/**
 * @brief Ensures that the current gameLapse setting is valid for the specified type.
 * If it isn't then it sets it to the minimum for that type.
 * @param[in] type The game lapse type.
 */
static void CL_EnsureValidGameLapseForType (int type)
{
	if ((lapse[gameLapse].type & type) != type) {
		gameLapse = CL_MinGameLapseForType(type);
		CL_UpdateTime();
	}
}

/**
 * @brief Ensures that the current gameLapse setting is valid for combat zoom.
 * If it isn't then it sets it to the minimum for combat zoom.
 */
void CL_EnsureValidGameLapseForCombatZoom (void)
{
	CL_EnsureValidGameLapseForType(LAPSETYPE_COMBATZOOM);
}

/**
 * @brief Ensures that the current gameLapse setting is valid for the geoscape.
 * If it isn't then it sets it to the minimum for the geoscape.
 */
void CL_EnsureValidGameLapseForGeoscape (void)
{
	CL_EnsureValidGameLapseForType(LAPSETYPE_GEOSCAPE);
}

/**
 * @brief Updates date/time and timescale (=timelapse) on the geoscape menu
 * @sa SAV_GameLoad
 * @sa CL_CampaignRun
 */
void CL_UpdateTime (void)
{
	dateLong_t date;
	CL_DateConvertLong(&ccs.date, &date);

	/* Update the timelapse text */
	if (gameLapse >= 0 && gameLapse < NUM_TIMELAPSE) {
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
	}

	/* Update the date */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%i %s %02i"), date.year, CL_DateGetMonthName(date.month - 1), date.day);
	Cvar_Set("mn_mapdate", mn.messageBuffer);

	/* Update the time. */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%02i:%02i"), date.hour, date.min);
	Cvar_Set("mn_maptime", mn.messageBuffer);
}

/**
 * @brief Stop game time speed
 */
void CL_GameTimeStop (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;

	if ((lapse[0].type & lapseType) != lapseType)
		return;

	/* don't allow time scale in tactical mode - only on the geoscape */
	/* @todo check whether we really need to identify menu with CL_OnBattlescape - was checking menu name before */
	if (menu && !CL_OnBattlescape()) {
		gameLapse = 0;
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}


/**
 * @brief Decrease game time speed
 *
 * Decrease game time speed - only works when there is already a base available
 */
void CL_GameTimeSlow (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;
	const int minGameLapse = CL_MinGameLapseForType(lapseType);

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == minGameLapse)
		return;

	assert(gameLapse >= minGameLapse);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && (!Q_strncmp(menu->name, "map", 3) || !Q_strncmp(menu->name, "airfight", 8))) {
		gameLapse--;
		while ((lapse[gameLapse].type & lapseType) != lapseType) {
			if (gameLapse <= minGameLapse) {
				gameLapse = minGameLapse;
				break;
			} else {
				gameLapse--;
			}
		}
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}

/**
 * @brief Increase game time speed
 *
 * Increase game time speed - only works when there is already a base available
 */
void CL_GameTimeFast (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const int lapseType = gd.combatZoomedUFO ? LAPSETYPE_COMBATZOOM : LAPSETYPE_GEOSCAPE;
	const int maxGameLapse = CL_MaxGameLapseForType(lapseType);

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == maxGameLapse)
		return;

	assert(gameLapse <= maxGameLapse);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && (!Q_strncmp(menu->name, "map", 3) || !Q_strncmp(menu->name, "airfight", 8))) {
		gameLapse++;
		while ((lapse[gameLapse].type & lapseType) != lapseType) {
			if (gameLapse >= maxGameLapse) {
				gameLapse = maxGameLapse;
				break;
			} else {
				gameLapse++;
			}
		}
	}

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}

/**
 * @brief Set game time speed
 * @param[in] gameLapseValue The value to set the game time to.
 * @note
 */
void CL_SetGameTime (int gameLapseValue)
{
	if (gameLapseValue == gameLapse)
		return;

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == NUM_TIMELAPSE - 1)
		return;

	assert(gameLapse < NUM_TIMELAPSE);

	gameLapse = gameLapseValue;

	/* Make sure the new lapse state is updated and it (and the time) is show in the menu. */
	CL_UpdateTime();
}

/**
 * @brief Update the nation data from all parsed nation each month
 * @note give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * @note Called from CL_CampaignRun
 * @sa CL_CampaignRun
 * @sa B_CreateEmployee
 */
static void CL_HandleBudget (void)
{
	int i, j;
	char message[1024];
	int cost;
	int initial_credits = ccs.credits;

	/* Refreshes the pilot global list.  Pilots who are already hired are unchanged, but all other
	 * pilots are replaced.  The new pilots is evenly distributed between the nations that are happy (happiness > 0). */
	E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, qtrue);

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		const int funding = NAT_GetFunding(nation, 0);
		int new_scientists = 0, new_soldiers = 0, new_workers = 0;
		CL_UpdateCredits(ccs.credits + funding);

		for (j = 0; 0.25 + j < (float) nation->maxScientists * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST, nation, NULL);
			new_scientists++;
		}


		if (nation->stats[0].happiness > 0) {
			for (j = 0; 0.25 + j < (float) nation->maxSoldiers * nation->stats[0].happiness * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
				/* Create a soldier. */
				E_CreateEmployee(EMPL_SOLDIER, nation, NULL);
				new_soldiers++;
			}
		}

		for (j = 0; 0.25 + j * 2 < (float) nation->maxSoldiers * nation->stats[0].happiness; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER, nation, NULL);
			new_workers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
					funding, ngettext("credit", "credits", funding),
					new_scientists, ngettext("scientist", "scientists", new_scientists),
					new_soldiers, ngettext("soldier", "soldiers", new_soldiers),
					new_workers, ngettext("worker", "workers", new_workers),
					_(nation->name), NAT_GetHappinessString(nation));
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		if (gd.employees[EMPL_SOLDIER][i].hired)
			cost += SALARY_SOLDIER_BASE + gd.employees[EMPL_SOLDIER][i].chr.score.rank * SALARY_SOLDIER_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to soldiers"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_WORKER]; i++) {
		if (gd.employees[EMPL_WORKER][i].hired)
			cost += SALARY_WORKER_BASE + gd.employees[EMPL_WORKER][i].chr.score.rank * SALARY_WORKER_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to workers"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_SCIENTIST]; i++) {
		if (gd.employees[EMPL_SCIENTIST][i].hired)
			cost += SALARY_SCIENTIST_BASE + gd.employees[EMPL_SCIENTIST][i].chr.score.rank * SALARY_SCIENTIST_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to scientists"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_PILOT]; i++) {
		if (gd.employees[EMPL_PILOT][i].hired)
			cost += SALARY_PILOT_BASE + gd.employees[EMPL_PILOT][i].chr.score.rank * SALARY_PILOT_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to pilots"), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	cost = 0;
	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		if (gd.employees[EMPL_ROBOT][i].hired)
			cost += SALARY_ROBOT_BASE + gd.employees[EMPL_ROBOT][i].chr.score.rank * SALARY_ROBOT_RANKBONUS;
	}

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for robots"), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	cost = 0;
	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		for (j = 0; j < base->numAircraftInBase; j++) {
			cost += base->aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		cost = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			cost += gd.buildings[i][j].varCosts;
		}

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of base %s"), cost, base->name);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
		CL_UpdateCredits(ccs.credits - cost);
	}

	cost = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_PILOT] * SALARY_ADMIN_PILOT + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Com_sprintf(message, sizeof(message), _("Paid %i credits for administrative overhead."), cost);
	CL_UpdateCredits(ccs.credits - cost);
	MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);

	if (initial_credits < 0) {
		float interest = initial_credits * SALARY_DEBT_INTEREST;

		cost = (int)ceil(interest);
		Com_sprintf(message, sizeof(message), _("Paid %i credits in interest on your debt."), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}
	CL_GameTimeStop();
}

/**
 * @brief sets market prices at start of the game
 * @sa CL_GameInit
 * @sa BS_Load (Market load function)
 * @param[in] load It this an attempt to init the market for a savegame?
 */
static void CL_CampaignInitMarket (qboolean load)
{
	int i;
	equipDef_t *ed;

	assert(curCampaign);

	/* find the relevant markets */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->market, ed->name, MAX_VAR)) {
			curCampaign->marketDef = ed;
			break;
		}
	if (!curCampaign->marketDef)
		Sys_Error("CL_CampaignInitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->market);
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->asymptoticMarket, ed->name, MAX_VAR)) {
			curCampaign->asymptoticMarketDef = ed;
			break;
		}
	if (!curCampaign->asymptoticMarketDef)
		Sys_Error("CL_CampaignInitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->asymptoticMarket);

	/* the savegame loading process will get the following values from savefile */
	if (load)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if (ccs.eMarket.ask[i] == 0) {
			ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i] * BID_FACTOR);
		}

		if (!curCampaign->marketDef->num[i])
			continue;

		if (!RS_IsResearched_ptr(csi.ods[i].tech) && curCampaign->marketDef->num[i] > 0)
			Com_Printf("CL_CampaignInitMarket: Could not add item %s to the market - not marked as researched in campaign %s\n", csi.ods[i].id, curCampaign->id);
		else
			/* the other relevant values were already set in CL_CampaignInitMarket */
			ccs.eMarket.num[i] = curCampaign->marketDef->num[i];
	}
}

/**
 * @brief make number of items change every day.
 * @sa CL_CampaignRun
 * @sa daily called
 * @note This function makes items number on market slowly reach the asymptotic number of items defined in equipment.ufo
 * If an item has just been researched, it's available on market until RESEARCH_LIMIT_DELAY days is reached.
 */
static void CL_CampaignRunMarket (void)
{
	int i;

	assert(curCampaign->marketDef);
	assert(curCampaign->asymptoticMarketDef);

	for (i = 0; i < csi.numODs; i++) {
		technology_t *tech = csi.ods[i].tech;
		const float TYPICAL_TIME = 10.f;			/**< Number of days to reach the asymptotic number of items */
		const int RESEARCH_LIMIT_DELAY = 30;		/**< Numbers of days after end of research to wait in order to have
													 * items added on market */
		int asymptoticNumber;

		if (!tech)
			Sys_Error("No tech that provides '%s'\n", csi.ods[i].id);

		if (RS_IsResearched_ptr(tech) && (curCampaign->marketDef->num[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if items are researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = curCampaign->asymptoticMarketDef->num[i] ? curCampaign->asymptoticMarketDef->num[i] : curCampaign->marketDef->num[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}

		/* Store the evolution of the market in currentEvolution */
		ccs.eMarket.currentEvolution[i] += (asymptoticNumber - ccs.eMarket.num[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(ccs.eMarket.currentEvolution[i]) >= 1.0f) {
			const int num = (int)(ccs.eMarket.currentEvolution[i]);
			ccs.eMarket.num[i] += num;
			ccs.eMarket.currentEvolution[i] -= num;
		}
		if (ccs.eMarket.num[i] < 0)
			ccs.eMarket.num[i] = 0;
	}
}

/**
 * @brief This will hide or show the geoscape button for handling the xvi overlay map
 */
static inline void CP_UpdateXVIMapButton (void)
{
	Cvar_SetValue("mn_xvimap", ccs.XVIShowMap);
}

/**
 * @brief delay between actions that must be executed independently of time scale
 * @sa RADAR_CheckUFOSensored
 * @sa UFO_UpdateAlienInterestForAllBasesAndInstallations
 * @sa AB_UpdateStealthForAllBase
 */
const int DETECTION_INTERVAL = (SECONDS_PER_HOUR / 2);

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types MN_MAP and MN_3DMAP
 * @sa MN_DrawMenus
 * @sa CL_HandleBudget
 * @sa B_UpdateBaseData
 * @sa CL_CampaignRunAircraft
 * @sa CP_CheckEvents
 */
void CL_CampaignRun (void)
{
	const int currentinterval = (int)floor(ccs.date.sec) % DETECTION_INTERVAL;
	int checks, dt, i;
	int timeAlreadyFlied = 0;	/**< Time already flied by UFO or aircraft due to detection each detectioninterval */

#ifdef DEBUG
	/* temporary check to find out bug: [1999099] no pilots available */
	assert(gd.numEmployees[EMPL_PILOT]);
#endif

	/* advance time */
	ccs.timer += cls.frametime * gd.gameTimeScale;
	checks = currentinterval + (int)floor(ccs.timer);
	checks = (int)(checks / DETECTION_INTERVAL);
	dt = DETECTION_INTERVAL - currentinterval;

	/* Execute every actions that needs to be independant of time speed : every DETECTION_INTERVAL
	 *	- Run UFOs and craft at least every DETECTION_INTERVAL. If detection occurred, break.
	 *	- Check if any new mission is detected
	 *	- Update stealth value of phalanx bases and installations ; alien bases */
	for (i = 0; i < checks; i++) {
		qboolean detection;
		UFO_CampaignRunUFOs(dt);
		CL_CampaignRunAircraft(dt, qfalse);
		CP_CheckNewMissionDetectedOnGeoscape();

		/* Update alien interest for bases */
		UFO_UpdateAlienInterestForAllBasesAndInstallations();

		/* Update how phalanx troop know alien bases */
		AB_UpdateStealthForAllBase();

		timeAlreadyFlied += dt;
		detection = UFO_CampaignCheckEvents();
		if (detection) {
			ccs.timer = (i + 1) * DETECTION_INTERVAL - currentinterval;
			break;
		}
		dt = DETECTION_INTERVAL;
	}

	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int currenthour;
		dateLong_t date;

		dt = (int)floor(ccs.timer);
		currenthour = (int)floor(ccs.date.sec / SECONDS_PER_HOUR);
		ccs.date.sec += dt;
		ccs.timer -= dt;

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(ccs.date.sec / SECONDS_PER_HOUR)) {
			currenthour++;
			RS_ResearchRun();
			PR_ProductionRun();
			UR_ProcessActive();
			AII_UpdateInstallationDelay();
			AII_RepairAircraft();
			TR_TransferCheck();
			CP_IncreaseAlienInterest();
		}

		/* daily events */
		while (ccs.date.sec > SECONDS_PER_DAY) {
			ccs.date.sec -= SECONDS_PER_DAY;
			ccs.date.day++;
			/* every day */
			B_UpdateBaseData();
			INS_UpdateInstallationData();
			HOS_HospitalRun();
			BDEF_ReloadBattery();
			CP_SpawnNewMissions();
			CP_SpreadXVI();
			NAT_UpdateHappinessForAllNations();
			AB_BaseSearchedByNations();
			CL_CampaignRunMarket();
		}

		/* check for campaign events */
		if (dt > timeAlreadyFlied) {
			/** aircraft and UFO already moved during radar detection (see above),
			 *  just make them move the missing part -- if any */
			CL_CampaignRunAircraft(dt - timeAlreadyFlied, qtrue);
			UFO_CampaignRunUFOs(dt - timeAlreadyFlied);
		}
		UFO_CampaignCheckEvents();
		AIRFIGHT_CampaignRunBaseDefense(dt);
		CP_CheckEvents();
		CP_CheckLostCondition(qtrue, NULL, 0);
		AIRFIGHT_CampaignRunProjectiles(dt);
		/* Check if there is a base attack mission */
		CP_CheckBaseAttacks_f();

		/* set time cvars */
		CL_DateConvertLong(&ccs.date, &date);
		/* every first day of a month */
		if (date.day == 1 && gd.fund != qfalse && gd.numBases) {
			CL_BackupMonthlyData();	/* Back up monthly data. */
			CL_HandleBudget();
			gd.fund = qfalse;
		} else if (date.day > 1)
			gd.fund = qtrue;

		CP_UpdateXVIMapButton();
		UP_GetUnreadMails();
		CL_UpdateTime();
	}
}


/* =========================================================== */


#define MAX_CREDITS 10000000
/**
 * @brief Sets credits and update mn_credits cvar
 *
 * Checks whether credits are bigger than MAX_CREDITS
 */
void CL_UpdateCredits (int credits)
{
	/* credits */
	if (credits > MAX_CREDITS)
		credits = MAX_CREDITS;
	ccs.credits = credits;
	Cvar_Set("mn_credits", va(_("%i c"), ccs.credits));
}

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 */
static void CL_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i, j, costs = 0, sum = 0;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	mn.menuText[TEXT_STATS_MISSION] = pos;
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), campaignStats.missionsWon, campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_BASES] = pos;
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
		campaignStats.basesBuild, gd.numBases, campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_INSTALLATIONS] = pos;
	for (i = 0; i < gd.numInstallations; i++) {
		const installation_t *inst = &gd.installations[i];
		Q_strcat(pos, va(_("%s\n"), inst->name), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* nations */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_NATIONS] = pos;
	for (i = 0; i < gd.numNations; i++) {
		Q_strcat(pos, va(_("%s\t%s\n"), _(gd.nations[i].name), NAT_GetHappinessString(&gd.nations[i])), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs */
	for (i = 0; i < gd.numEmployees[EMPL_SCIENTIST]; i++) {
		if (gd.employees[EMPL_SCIENTIST][i].hired) {
			costs += SALARY_SCIENTIST_BASE + gd.employees[EMPL_SCIENTIST][i].chr.score.rank * SALARY_SCIENTIST_RANKBONUS;
			hired[EMPL_SCIENTIST]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		if (gd.employees[EMPL_SOLDIER][i].hired) {
			costs += SALARY_SOLDIER_BASE + gd.employees[EMPL_SOLDIER][i].chr.score.rank * SALARY_SOLDIER_RANKBONUS;
			hired[EMPL_SOLDIER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_WORKER]; i++) {
		if (gd.employees[EMPL_WORKER][i].hired) {
			costs += SALARY_WORKER_BASE + gd.employees[EMPL_WORKER][i].chr.score.rank * SALARY_WORKER_RANKBONUS;
			hired[EMPL_WORKER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_PILOT]; i++) {
		if (gd.employees[EMPL_PILOT][i].hired) {
			costs += SALARY_PILOT_BASE + gd.employees[EMPL_PILOT][i].chr.score.rank * SALARY_PILOT_RANKBONUS;
			hired[EMPL_PILOT]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		if (gd.employees[EMPL_ROBOT][i].hired) {
			costs += SALARY_ROBOT_BASE + gd.employees[EMPL_ROBOT][i].chr.score.rank * SALARY_ROBOT_RANKBONUS;
			hired[EMPL_ROBOT]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_EMPLOYEES] = pos;
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_COSTS] = pos;
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		for (j = 0; j < base->numAircraftInBase; j++) {
			costs += base->aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		costs = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			costs += gd.buildings[i][j].varCosts;
		}
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), base->name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_PILOT] * SALARY_ADMIN_PILOT + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		const float interest = ccs.credits * SALARY_DEBT_INTEREST;

		costs = (int)ceil(interest);
		Q_strcat(pos, va(_("Debt:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}
	Q_strcat(pos, va(_("\n\t-------\nSum:\t%i c\n"), sum), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* campaign */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_GENERIC] = pos;
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), curCampaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (ccs.XVISpreadActivated) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			curCampaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

/**
 * @brief Load callback for campaign data
 * @sa CP_Save
 * @sa SAV_GameSave
 * @sa CP_SpawnCrashSiteMission
 */
qboolean CP_Load (sizebuf_t *sb, void *data)
{
	int i, j, num;
	const char *name, *missionId;
	char val[32];

	/* read campaign name */
	name = MSG_ReadString(sb);

	for (i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++)
		if (!Q_strncmp(name, curCampaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		curCampaign = NULL;
		return qfalse;
	}

	CL_GameInit(qtrue);

	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	/* init the map images and reset the map actions */
	MAP_Init();

	memset(&ccs, 0, sizeof(ccs));
	ccs.singleplayer = qtrue;

	gd.fund = MSG_ReadByte(sb);
	gd.nextUCN = MSG_ReadShort(sb);

	/* read date */
	ccs.date.day = MSG_ReadLong(sb);
	ccs.date.sec = MSG_ReadLong(sb);

	/* read map view */
	ccs.center[0] = MSG_ReadFloat(sb);
	ccs.center[1] = MSG_ReadFloat(sb);
	ccs.angles[0] = MSG_ReadFloat(sb);
	ccs.angles[1] = MSG_ReadFloat(sb);
	ccs.zoom = MSG_ReadFloat(sb);

	/* restore the overlay */
	Cvar_SetValue("r_geoscape_overlay", MSG_ReadShort(sb));
	radarOverlayWasSet = MSG_ReadShort(sb);

	/* read credits */
	CL_UpdateCredits(MSG_ReadLong(sb));

	/* store interest values */
	ccs.lastInterestIncreaseDelay = MSG_ReadShort(sb);
	ccs.lastMissionSpawnedDelay = MSG_ReadShort(sb);
	ccs.overallInterest = MSG_ReadShort(sb);
	for (i = 0; i < presaveArray[PRE_MAXCAT]; i++)
		ccs.interest[i] = MSG_ReadShort(sb);

	/* read other campaign data */
	ccs.civiliansKilled = MSG_ReadShort(sb);
	ccs.aliensKilled = MSG_ReadShort(sb);
	ccs.XVISpreadActivated = MSG_ReadByte(sb);
	ccs.XVIShowMap = MSG_ReadByte(sb);
	ccs.humansAttackActivated = MSG_ReadByte(sb);

	CP_UpdateXVIMapButton();

	/* read missions */
	num = MSG_ReadShort(sb);
	for (i = 0; i < num; i++) {
		mission_t mission;
		int ufoIdx;

		memset(&mission, 0, sizeof(mission));

		name = MSG_ReadString(sb);
		if (name[0] != '\0') {
			mission.mapDef = Com_GetMapDefinitionByID(name);
			if (!mission.mapDef) {
				Com_Printf("......mapdef \"%s\" doesn't exist.\n", name);
				return qfalse;
			}
			mission.mapDef->timesAlreadyUsed = MSG_ReadLong(sb);
		} else
			mission.mapDef = NULL;
		Q_strncpyz(mission.id, MSG_ReadString(sb), sizeof(mission.id));
		mission.active = MSG_ReadByte(sb);
		Q_strncpyz(mission.onwin, MSG_ReadString(sb), sizeof(mission.onwin));
		Q_strncpyz(mission.onlose, MSG_ReadString(sb), sizeof(mission.onlose));
		mission.category = MSG_ReadShort(sb);
		mission.stage = MSG_ReadShort(sb);
		switch (mission.category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission.stage == STAGE_MISSION_GOTO || mission.stage == STAGE_BASE_ATTACK) {
				/* Load IDX of base under attack */
				const int baseidx = MSG_ReadByte(sb);
				base_t *base = B_GetBaseByIDX(baseidx);
				assert(base);
				if (mission.stage == STAGE_BASE_ATTACK && base->baseStatus != BASE_UNDER_ATTACK)
					Com_Printf("......warning: base %i (%s) is supposedly under attack but base status doesn't match!\n", baseidx, base->name);
				mission.data = (void *) base;
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission.stage >= STAGE_MISSION_GOTO) {
				const int baseidx = MSG_ReadByte(sb);
				if (baseidx != BYTES_NONE) {
					/* don't check baseidx value here: alien bases are not loaded yet */
					alienBase_t *alienBase = AB_GetBase(baseidx, 0);
					if (alienBase)
						mission.data = (void *) alienBase;
					else
						Com_Printf("Error while loading Alien Base mission (mission %i)\n", i);
				}
			}
			break;
		default:
			break;
		}
		Q_strncpyz(mission.location, MSG_ReadString(sb), sizeof(mission.location));
		mission.initialOverallInterest = MSG_ReadShort(sb);
		mission.initialIndividualInterest = MSG_ReadShort(sb);
		mission.startDate.day = MSG_ReadLong(sb);
		mission.startDate.sec = MSG_ReadLong(sb);
		mission.finalDate.day = MSG_ReadLong(sb);
		mission.finalDate.sec = MSG_ReadLong(sb);
		mission.pos[0] = MSG_ReadFloat(sb);
		mission.pos[1] = MSG_ReadFloat(sb);
		ufoIdx = MSG_ReadShort(sb);
		if (ufoIdx <= -1)
			mission.ufo = NULL;
		else
			mission.ufo = gd.ufos + ufoIdx;
		mission.onGeoscape = MSG_ReadByte(sb);

		/* Add mission to global array */
		LIST_Add(&ccs.missions, (byte*) &mission, sizeof(mission_t));
	}

	/* read ccs.battleParameters */
	memset(&ccs.battleParameters, 0, sizeof(ccs.battleParameters));
	missionId = MSG_ReadString(sb);
	if (missionId[0] != '\0') {
		ccs.battleParameters.mission = CP_GetMissionById(missionId);
		ccs.battleParameters.numAlienTeams = MSG_ReadShort(sb);
		for (i = 0; i < ccs.battleParameters.numAlienTeams; i++)
			ccs.battleParameters.alienTeams[i] = Com_GetTeamDefinitionByID(MSG_ReadString(sb));
		name = MSG_ReadString(sb);
		if (name[0] != '\0')
			ccs.battleParameters.param = Mem_PoolStrDup(name, cl_localPool, 0);
		else
			ccs.battleParameters.param = NULL;
		Q_strncpyz(ccs.battleParameters.alienEquipment, MSG_ReadString(sb), sizeof(ccs.battleParameters.alienEquipment));
		Q_strncpyz(ccs.battleParameters.civTeam, MSG_ReadString(sb), sizeof(ccs.battleParameters.civTeam));
		ccs.battleParameters.day = MSG_ReadByte(sb);
		ccs.battleParameters.ugv = MSG_ReadShort(sb);
		ccs.battleParameters.aliens = MSG_ReadShort(sb);
		ccs.battleParameters.civilians = MSG_ReadShort(sb);
	}

	/* reset team */
	Cvar_Set("cl_team", curCampaign->team);

	/* stores the select mission on geoscape */
	missionId = MSG_ReadString(sb);
	if (missionId[0] != '\0')
		selectedMission = CP_GetMissionById(missionId);
	else
		selectedMission = NULL;

	/* and now fix the mission pointers for e.g. ufocrash sites
	 * this is needed because the base load function which loads the aircraft
	 * doesn't know anything (at that stage) about the new missions that were
	 * add in this load function */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		for (j = 0; j < base->numAircraftInBase; j++) {
			if (base->aircraft[j].status == AIR_MISSION) {
				assert(base->aircraft[j].missionID);
				base->aircraft[j].mission = CP_GetMissionById(base->aircraft[j].missionID);

				/* not found */
				if (!base->aircraft[j].mission) {
					Com_Printf("Could not link mission '%s' in aircraft\n", base->aircraft[j].missionID);
					Mem_Free(base->aircraft[j].missionID);
					base->aircraft[j].missionID = NULL;
					return qfalse;
				}
				Mem_Free(base->aircraft[j].missionID);
				base->aircraft[j].missionID = NULL;
			}
		}
	}

	return qtrue;
}

/**
 * @brief Save callback for campaign data
 * @sa CP_Load
 * @sa SAV_GameSave
 */
qboolean CP_Save (sizebuf_t *sb, void *data)
{
	int i;
	const linkedList_t *list = ccs.missions;

	/* store campaign name */
	MSG_WriteString(sb, curCampaign->id);

	MSG_WriteByte(sb, gd.fund);
	MSG_WriteShort(sb, gd.nextUCN);

	/* store date */
	MSG_WriteLong(sb, ccs.date.day);
	MSG_WriteLong(sb, ccs.date.sec);

	/* store map view */
	MSG_WriteFloat(sb, ccs.center[0]);
	MSG_WriteFloat(sb, ccs.center[1]);
	MSG_WriteFloat(sb, ccs.angles[0]);
	MSG_WriteFloat(sb, ccs.angles[1]);
	MSG_WriteFloat(sb, ccs.zoom);

	/* save the overlay state */
	MSG_WriteShort(sb, r_geoscape_overlay->integer);
	MSG_WriteShort(sb, radarOverlayWasSet);

	/* store credits */
	MSG_WriteLong(sb, ccs.credits);

	/* store interest values */
	MSG_WriteShort(sb, ccs.lastInterestIncreaseDelay);
	MSG_WriteShort(sb, ccs.lastMissionSpawnedDelay);
	MSG_WriteShort(sb, ccs.overallInterest);
	for (i = 0; i < presaveArray[PRE_MAXCAT]; i++)
		MSG_WriteShort(sb, ccs.interest[i]);

	/* store other campaign data */
	MSG_WriteShort(sb, ccs.civiliansKilled);
	MSG_WriteShort(sb, ccs.aliensKilled);
	MSG_WriteByte(sb, ccs.XVISpreadActivated);
	MSG_WriteByte(sb, ccs.XVIShowMap);
	MSG_WriteByte(sb, ccs.humansAttackActivated);

	/* store missions */
	MSG_WriteShort(sb, CP_CountMission());
	for (; list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;

		if (mission->mapDef) {
			MSG_WriteString(sb, mission->mapDef->id);
			MSG_WriteLong(sb, mission->mapDef->timesAlreadyUsed);
		} else
			MSG_WriteString(sb, "");
		MSG_WriteString(sb, mission->id);
		MSG_WriteByte(sb, mission->active);
		MSG_WriteString(sb, mission->onwin);
		MSG_WriteString(sb, mission->onlose);
		MSG_WriteShort(sb, mission->category);
		MSG_WriteShort(sb, mission->stage);
		switch (mission->category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission->stage == STAGE_MISSION_GOTO || mission->stage == STAGE_BASE_ATTACK) {
				const base_t *base;
				/* save IDX of base under attack if required */
				base = (base_t*)mission->data;
				assert(base);
				MSG_WriteByte(sb, base->idx);
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission->stage >= STAGE_MISSION_GOTO) {
				const alienBase_t *base;
				/* save IDX of base under attack if required */
				base = (alienBase_t*)mission->data;
				if (base)
					MSG_WriteByte(sb, base->idx);
				else
					/* there may be no base is the mission is a subverting government */
					MSG_WriteByte(sb, BYTES_NONE);
			}
			break;
		default:
			break;
		}
		MSG_WriteString(sb, mission->location);
		MSG_WriteShort(sb, mission->initialOverallInterest);
		MSG_WriteShort(sb, mission->initialIndividualInterest);
		MSG_WriteLong(sb, mission->startDate.day);
		MSG_WriteLong(sb, mission->startDate.sec);
		MSG_WriteLong(sb, mission->finalDate.day);
		MSG_WriteLong(sb, mission->finalDate.sec);
		MSG_WriteFloat(sb, mission->pos[0]);
		MSG_WriteFloat(sb, mission->pos[1]);
		if (mission->ufo)
			MSG_WriteShort(sb, mission->ufo - gd.ufos);
		else
			MSG_WriteShort(sb, -1);
		MSG_WriteByte(sb, mission->onGeoscape);
	}

	/* store ccs.battleParameters */
	if (ccs.battleParameters.mission) {
		MSG_WriteString(sb, ccs.battleParameters.mission->id);
		MSG_WriteShort(sb, ccs.battleParameters.numAlienTeams);
		for (i = 0; i < ccs.battleParameters.numAlienTeams; i++)
			MSG_WriteString(sb, ccs.battleParameters.alienTeams[i]->id);
		MSG_WriteString(sb, (ccs.battleParameters.param ? ccs.battleParameters.param : ""));
		MSG_WriteString(sb, ccs.battleParameters.alienEquipment);
		MSG_WriteString(sb, ccs.battleParameters.civTeam);
		MSG_WriteByte(sb, ccs.battleParameters.day);
		MSG_WriteShort(sb, ccs.battleParameters.ugv);
		MSG_WriteShort(sb, ccs.battleParameters.aliens);
		MSG_WriteShort(sb, ccs.battleParameters.civilians);
	} else
		MSG_WriteString(sb, "");

	/* stores the select mission on geoscape */
	MSG_WriteString(sb, selectedMission ? selectedMission->id : "");

	return qtrue;
}


qboolean STATS_Save (sizebuf_t* sb, void* data)
{
	MSG_WriteShort(sb, campaignStats.missionsWon);
	MSG_WriteShort(sb, campaignStats.missionsLost);
	MSG_WriteShort(sb, campaignStats.basesBuild);
	MSG_WriteShort(sb, campaignStats.basesAttacked);
	MSG_WriteShort(sb, campaignStats.interceptions);
	MSG_WriteShort(sb, campaignStats.soldiersLost);
	MSG_WriteShort(sb, campaignStats.soldiersNew);
	MSG_WriteShort(sb, campaignStats.killedAliens);
	MSG_WriteShort(sb, campaignStats.rescuedCivilians);
	MSG_WriteShort(sb, campaignStats.researchedTechnologies);
	MSG_WriteShort(sb, campaignStats.moneyInterceptions);
	MSG_WriteShort(sb, campaignStats.moneyBases);
	MSG_WriteShort(sb, campaignStats.moneyResearch);
	MSG_WriteShort(sb, campaignStats.moneyWeapons);
	return qtrue;
}

qboolean STATS_Load (sizebuf_t* sb, void* data)
{
	campaignStats.missionsWon = MSG_ReadShort(sb);
	campaignStats.missionsLost = MSG_ReadShort(sb);
	campaignStats.basesBuild = MSG_ReadShort(sb);
	campaignStats.basesAttacked = MSG_ReadShort(sb);
	campaignStats.interceptions = MSG_ReadShort(sb);
	campaignStats.soldiersLost = MSG_ReadShort(sb);
	campaignStats.soldiersNew = MSG_ReadShort(sb);
	campaignStats.killedAliens = MSG_ReadShort(sb);
	campaignStats.rescuedCivilians = MSG_ReadShort(sb);
	campaignStats.researchedTechnologies = MSG_ReadShort(sb);
	campaignStats.moneyInterceptions = MSG_ReadShort(sb);
	campaignStats.moneyBases = MSG_ReadShort(sb);
	campaignStats.moneyResearch = MSG_ReadShort(sb);
	campaignStats.moneyWeapons = MSG_ReadShort(sb);
	return qtrue;
}

/**
 * @brief XVI map saving callback
 * @note Only save transparency
 * @sa Savegame callback
 * @sa SAV_Init
 * @sa XVI_Load
 */
qboolean XVI_Save (sizebuf_t *sb, void *data)
{
	byte *out;
	int x, y;
	int width;
	int height;

	out = R_XVIMapCopy(&width, &height);
	if (!out) {
		MSG_WriteShort(sb, 0);
		MSG_WriteShort(sb, 0);
		return qtrue;
	}

	MSG_WriteShort(sb, width);
	MSG_WriteShort(sb, height);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			MSG_WriteByte(sb, out[y * width + x]);
	}
	Mem_Free(out);
	return qtrue;
}

/**
 * @brief Load the XVI map from the savegame.
 * @sa Savegame callback
 * @sa SAV_Init
 * @sa XVI_Save
 */
qboolean XVI_Load (sizebuf_t *sb, void *data)
{
	byte *out;
	int i, j;
	int width, height;

	width = MSG_ReadShort(sb);
	height = MSG_ReadShort(sb);

	out = (byte *)Mem_PoolAlloc(width * height, vid_imagePool, 0);
	if (!out)
		Sys_Error("TagMalloc: failed on allocation of %i bytes for XVI_Load", width * height);

	for (i = 0; i < width; i++) {
		for (j = 0; j < height; j++) {
			out[j * width + i] = MSG_ReadByte(sb);
		}
	}

	R_InitializeXVIOverlay(curCampaign->map, out, width, height);

	Mem_Free(out);

	return qtrue;
}

/**
 * @brief Set some needed cvars from mission definition
 * @param[in] mission mission definition pointer with the needed data to set the cvars to
 * @sa CL_GameGo
 */
static void CP_SetMissionVars (const mission_t *mission)
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
static void CP_StartMissionMap (mission_t* mission)
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
 * @brief Starts a selected mission
 * @note Checks whether a dropship is near the landing zone and whether
 * it has a team on board
 * @sa CP_SetMissionVars
 */
static void CL_GameGo (void)
{
	mission_t *mis;
	aircraft_t *aircraft;
	base_t *base;

	if (!curCampaign || !cls.missionaircraft)
		return;

	aircraft = cls.missionaircraft;
	base = CP_GetMissionBase();

	if (!selectedMission)
		selectedMission = aircraft->mission;

	if (!selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "No selectedMission\n");
		return;
	}

	mis = selectedMission;
	assert(mis);
	assert(aircraft);

	/* Before we start, we should clear the missionresults array. */
	memset(&missionresults, 0, sizeof(missionresults));

	/* Various sanity checks. */
	if (ccs.singleplayer) {
		if (!mis->active) {
			Com_DPrintf(DEBUG_CLIENT, "CL_GameGo: Dropship not near landing zone: mis->active: %i\n", mis->active);
			return;
		}
		if (aircraft->teamSize <= 0) {
			Com_DPrintf(DEBUG_CLIENT, "CL_GameGo: No team in dropship. teamSize=%i\n", aircraft->teamSize);
			return;
		}
	} else {
		if (B_GetNumOnTeam(aircraft) == 0) {
			/** Multiplayer; but we never reach this far!
			 * @todo Why is that? GameGo can be called via console.*/
			MN_Popup(_("Note"), _("Assemble or load a team"));
			return;
		}
	}

	CP_CreateAlienTeam(mis);
	CP_CreateCivilianTeam(mis);
	CP_CreateBattleParameters(mis);
	CP_SetMissionVars(mis);
	/* Set the states of mission Cvars to proper values. */
	Cvar_SetValue("mission_uforecovered", 0);

	/* manage inventory */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CL_CleanTempInventory(base);
	CL_ReloadAndRemoveCarried(aircraft, &ccs.eMission);
	/* remove inventory of any old temporary LEs */
	LE_Cleanup();

	CP_StartMissionMap(mis);
}

/**
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddTechAsResearchable_f (void)
{
	const char *techID;
	technology_t *tech;

	/* baseid is appened in mission trigger function */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <tech>\n", Cmd_Argv(0));
		return;
	}

	techID = Cmd_Argv(1);
	tech = RS_GetTechByID(techID);
	RS_MarkOneResearchable(tech);
}

/**
 * @brief For things like craft_ufo_scout that are no real items this function will
 * increase the collected counter by one
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddItemAsCollected_f (void)
{
	int i, baseID;
	const char* id;

	/* baseid is appened in mission trigger function */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <item>\n", Cmd_Argv(0));
		return;
	}

	id = Cmd_Argv(1);
	baseID = atoi(Cmd_Argv(2));

	/* i = item index */
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];
		if (!Q_strncmp(id, item->id, MAX_VAR)) {
			gd.bases[baseID].storage.num[i]++;
			Com_DPrintf(DEBUG_CLIENT, "add item: '%s'\n", item->id);
			assert(item->tech);
			RS_MarkCollected(item->tech);
		}
	}
}

/**
 * @brief Changes nation happiness by given value.
 * @note There must be argument passed to this function being converted to float.
 */
static void CP_ChangeNationHappiness_f (void)
{
	float change;
	nation_t *nation;

	/* baseid is appened in mission trigger function */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <absolute change value>\n", Cmd_Argv(0));
		return;
	}
	change = atof(Cmd_Argv(1));

	if (!selectedMission) {
		Com_Printf("No mission selected - could not determine nation to use\n");
		return;
	}

	/* we can use an assert here - because this script function will only
	 * be available as trigger command - selectedMission must be set at that stage */
	assert(selectedMission);
	nation = MAP_GetNation(selectedMission->pos);
	assert(nation);

	NAT_SetHappiness(nation, nation->stats[0].happiness + change);
}

/**
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_EndGame_f (void)
{
	mn.menuText[TEXT_STANDARD] = _("The game ends here.");
	CP_EndCampaign(qfalse);
}

/** @brief mission trigger functions */
static const cmdList_t cp_commands[] = {
	{"cp_add_researchable", CP_AddTechAsResearchable_f, "Add a tech as researchable"},
	{"cp_add_item", CP_AddItemAsCollected_f, "Add an item as collected"},
	{"cp_changehappiness", CP_ChangeNationHappiness_f, "Function to raise or lower nation hapiness."},
	{"cp_endgame", CP_EndGame_f, "This command will end the current campaign"},

	{NULL, NULL, NULL}
};

/**
 * @brief Add/Remove temporary mission trigger functions
 * @note These function can be defined via onwin/onlose parameters in missions.ufo
 * @param[in] add If true, add the trigger functions, otherwise delete them
 */
static void CP_MissionTriggerFunctions (qboolean add)
{
	const cmdList_t *commands;

	for (commands = cp_commands; commands->name; commands++)
		if (add)
			Cmd_AddCommand(commands->name, commands->function, commands->description);
		else
			Cmd_RemoveCommand(commands->name);
}

/**
 * @brief Executes console commands after a mission
 *
 * @param m Pointer to mission_t
 * @param won Int value that is one when you've won the game, and zero when the game was lost
 * Can execute console commands (triggers) on win and lose
 * This can be used for story dependent missions
 */
void CP_ExecuteMissionTrigger (mission_t *m, qboolean won)
{
	/* we add them only here - and remove them afterwards to prevent cheating */
	CP_MissionTriggerFunctions(qtrue);
	Com_DPrintf(DEBUG_CLIENT, "Execute mission triggers\n");
	if (won && m->onwin[0] != '\0') {
		Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", m->onwin);
		Cbuf_AddText(va("%s\n", m->onwin));
	} else if (!won && m->onlose[0] != '\0') {
		Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", m->onlose);
		Cbuf_AddText(va("%s\n", m->onlose));
	}
	Cbuf_Execute();
	CP_MissionTriggerFunctions(qfalse);
}

/**
 * @brief Checks whether you have to play this mission
 * You can mark a mission as story related.
 * If a mission is story related the cvar @c game_autogo is set to @c 0
 * If this cvar is @c 1 - the mission dialog will have a auto mission button
 * @sa CL_GameAutoGo_f
 */
static void CL_GameAutoCheck_f (void)
{
	if (!curCampaign || !selectedMission || !gd.interceptAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoCheck_f: No update after automission\n");
		return;
	}

	if (selectedMission->mapDef->storyRelated) {
		Com_DPrintf(DEBUG_CLIENT, "story related - auto mission is disabled\n");
		Cvar_Set("game_autogo", "0");
	} else {
		Com_DPrintf(DEBUG_CLIENT, "auto mission is enabled\n");
		Cvar_Set("game_autogo", "1");
	}
}

/**
 * @brief Calculates the win probability for an auto mission
 * @todo This needs work - also take mis->initialIndividualInterest into account?
 * @returns a float value that is between 0 and 1
 * @param[in] mis The mission we are calculating the probability for
 * @param[in] base The base we are trying to defend in case of a base attack mission
 * @param[in] aircraft Your aircraft that has reached the mission location in case
 * this is no base attack mission
 */
static float CP_GetWinProbabilty (const mission_t *mis, const base_t *base, const aircraft_t *aircraft)
{
	float winProbability;

	if (mis->stage != STAGE_BASE_ATTACK) {
		assert(aircraft);

		switch (mis->category) {
		case INTERESTCATEGORY_TERROR_ATTACK:
			/* very hard to win this */
			/** @todo change the formular here to reflect the above comment */
			winProbability = exp((0.5 - .15 * difficulty->integer) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		case INTERESTCATEGORY_XVI:
			/* not that hard to win this, they want to spread xvi - no real terror mission */
			/** @todo change the formular here to reflect the above comment */
			winProbability = exp((0.5 - .15 * difficulty->integer) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		default:
			/** @todo change the formular here to reflect the above comments */
			winProbability = exp((0.5 - .15 * difficulty->integer) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		}
		Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: %i -- probability to win: %.02f\n", ccs.battleParameters.aliens, aircraft->teamSize, winProbability);

		return winProbability;
	} else {
		linkedList_t *hiredSoldiers = NULL;
		linkedList_t *ugvs = NULL;
		linkedList_t *listPos;
		const int numSoldiers = E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredSoldiers);
		const int numUGVs = E_GetHiredEmployees(base, EMPL_ROBOT, &ugvs);

		assert(base);

		/* a base defence mission can only be won if there are soldiers that
		 * defend the attacked base */
		if (numSoldiers || numUGVs) {
			float increaseWinProbability = 1.0f;
			listPos = hiredSoldiers;
			while (listPos) {
				const employee_t *employee = (employee_t *)listPos->data;
				/* don't use an employee that is currently being transfered */
				if (E_EmployeeIsCurrentlyInBase(employee)) {
					const character_t *chr = &employee->chr;
					const chrScoreGlobal_t *score = &chr->score;
					/* if the soldier was ever on a mission */
					if (score->assignedMissions) {
						const rank_t *rank = &gd.ranks[score->rank];
						/* @sa CHRSH_CharGetMaxExperiencePerMission */
						if (score->experience[SKILL_CLOSE] > 70) { /** @todo fix this value */
							increaseWinProbability *= rank->factor;
						}
					}
				}
				listPos = listPos->next;
			}
			/* now handle the ugvs */
			listPos = ugvs;
			while (listPos) {
				const employee_t *employee = (employee_t *)listPos->data;
				/* don't use an employee that is currently being transfered */
				if (E_EmployeeIsCurrentlyInBase(employee)) {
					const character_t *chr = &employee->chr;
					const chrScoreGlobal_t *score = &chr->score;
					const rank_t *rank = &gd.ranks[score->rank];
					/* @sa CHRSH_CharGetMaxExperiencePerMission */
					if (score->experience[SKILL_CLOSE] > 70) { /** @todo fix this value */
						increaseWinProbability *= rank->factor;
					}
				}
				listPos = listPos->next;
			}

			winProbability = exp((0.5 - .15 * difficulty->integer) * numSoldiers - ccs.battleParameters.aliens);
			winProbability += increaseWinProbability;

			Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: %i - UGVs: %i -- probability to win: %.02f\n",
				ccs.battleParameters.aliens, numSoldiers, numUGVs, winProbability);

			LIST_Delete(&hiredSoldiers);
			LIST_Delete(&ugvs);

			return winProbability;
		} else {
			/* No soldier to defend the base */
			Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: 0  -- battle lost\n", ccs.battleParameters.aliens);
			return 0.0f;
		}
	}
}

/**
 * @brief Handles the auto mission for none storyrelated missions or missions
 * that failed to assembly
 * @sa CL_GameAutoGo_f
 * @sa CL_Drop
 * @sa AL_CollectingAliens
 */
void CL_GameAutoGo (mission_t *mis)
{
	qboolean won;
	float winProbability;
	/* maybe gd.interceptAircraft is changed in some functions we call here
	 * so store a local pointer to guarantee that we access the right aircraft
	 * note that gd.interceptAircraft is a fake aircraft for base attack missions */
	aircraft_t *aircraft = gd.interceptAircraft;
	int i;

	assert(mis);

	CP_CreateAlienTeam(mis);
	CP_CreateCivilianTeam(mis);
	CP_CreateBattleParameters(mis);

	if (!gd.interceptAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo: No update after automission\n");
		return;
	}

	if (mis->stage != STAGE_BASE_ATTACK) {
		if (!mis->active) {
			MN_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mis->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("game_autogo", "0");
			return;
		}

		winProbability = CP_GetWinProbabilty(mis, NULL, aircraft);
	} else {
		winProbability = CP_GetWinProbabilty(mis, (base_t *)mis->data, NULL);
	}

	MN_PopMenu(qfalse);

	won = frand() < winProbability;

	/* update nation opinions */
	if (won) {
		int civiliansKilled = 0; /** @todo fill this for the case you won the game */
		CL_HandleNationData(!won, ccs.battleParameters.civilians, 0, 0, ccs.battleParameters.aliens, selectedMission);
		CP_CheckLostCondition(!won, mis, civiliansKilled);
	} else {
		CL_HandleNationData(!won, 0, ccs.battleParameters.civilians, ccs.battleParameters.aliens, 0, selectedMission);
		CP_CheckLostCondition(!won, mis, ccs.battleParameters.civilians);
	}

	/* Collect alien bodies */
	{
		int amount;
		int aliensLeft = ccs.battleParameters.aliens;
		aliensTmp_t *cargo;
		aliensTmp_t aliencargoTemp[MAX_CARGO];
		int aliencargoTypes = 0;

		assert(aircraft);

		/* collect all aliens as dead ones */
		cargo = aircraft->aliencargo;

		/* check whether there are already dead aliens on board */
		if (aircraft->alientypes) {
			*aliencargoTemp = *aircraft->aliencargo;
			aliencargoTypes = aircraft->alientypes;
		}

		/* Make sure dropship aliencargo is empty. */
		memset(aircraft->aliencargo, 0, sizeof(aircraft->aliencargo));

		aircraft->alientypes = ccs.battleParameters.numAlienTeams;
		amount = 0;
		while (aliensLeft > 0) {
			for (i = 0; i < aircraft->alientypes; i++) {
				assert(i < MAX_CARGO);
				assert(ccs.battleParameters.alienTeams[i]);
				cargo[i].teamDef = ccs.battleParameters.alienTeams[i];
				cargo[i].amount_dead += rand() % aliensLeft;
				aliensLeft -= cargo[i].amount_dead;
				amount += cargo[i].amount_dead;
				if (!aliensLeft)
					break;
			}
		}
		if (amount)
			MN_AddNewMessage(_("Notice"), va(_("Collected %i dead alien bodies"), amount), qfalse, MSG_STANDARD, NULL);

		/* put the old aliens back into the cargo */
		for (i = 0; i < aliencargoTypes; i++) {
			int j;
			for (j = 0; j < aircraft->alientypes; j++) {
				if (aliencargoTemp[i].teamDef == cargo[j].teamDef)
					break;
			}
			if (j < aircraft->alientypes) {
				/* 'old' race was collected during this mission, too */
				cargo[j].amount_dead += aliencargoTemp[i].amount_dead;
			} else if (j < MAX_CARGO) {
				/* add the 'old' race as a 'new' race to the cargo
				 * it wasn't collected during this mission */
				cargo[j].amount_dead += aliencargoTemp[i].amount_dead;
				cargo[j].teamDef = aliencargoTemp[i].teamDef;
				aircraft->alientypes++;
			} else {
				Com_DPrintf(DEBUG_CLIENT, "Could not readd the 'old' alien race to the cargo\n");
			}
		}

		/* Check for alien containment in aircraft homebase. */
		if (aircraft->alientypes && !B_GetBuildingStatus(aircraft->homebase, B_ALIEN_CONTAINMENT)) {
			/* We have captured/killed aliens, but the homebase of this aircraft
			 * does not have alien containment. Popup aircraft transer dialog to
			 * choose a better base. */
			TR_TransferAircraftMenu(aircraft);
		}
	}

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(selectedMission, won);

	/* if a UFO has been recovered, send it to a base */
	if (won && missionresults.recovery)
		Cmd_ExecuteString("cp_uforecoverystore");

	/* handle base attack mission */
	if (selectedMission->stage == STAGE_BASE_ATTACK) {
		const base_t *base = (base_t*)selectedMission->data;
		assert(base);

		if (won) {
			/* fake an aircraft return to collect goods and aliens */
			CL_AircraftReturnedToHomeBase(gd.interceptAircraft);

			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Defence of base: %s successful!"), base->name);
			MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(selectedMission);
			/** @todo @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CP_BaseAttackMissionLeave(selectedMission);
	} else {
		AIR_AircraftReturnToBase(gd.interceptAircraft);
		if (won)
			CP_MissionIsOver(selectedMission);
	}

	if (won)
		MN_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	else
		MN_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);

	MAP_ResetAction();
}

/**
 * @sa CL_GameAutoCheck_f
 * @sa CL_GameGo
 */
static void CL_GameAutoGo_f (void)
{
	if (!curCampaign || !selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo_f: No update after automission\n");
		return;
	}

	if (!cls.missionaircraft) {
		Com_Printf("CL_GameAutoGo_f: No aircraft at target pos\n");
		return;
	}

	/* start the map */
	CL_GameAutoGo(selectedMission);
}

/**
 * @brief Let the aliens win the match
 */
static void CL_GameAbort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

/**
 * @brief Update employeer stats after mission.
 * @param[in] won Determines whether we won the mission or not.
 * @note Soldier promotion is being done here.
 * @sa CL_GameResults_f
 */
static void CL_UpdateCharacterStats (const base_t *base, int won, const aircraft_t *aircraft)
{
	int i, j;

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: base: '%s' numTeamList: %i\n",
		base->name, cl.numTeamList);

	assert(aircraft);

	/* only soldiers have stats and ranks, ugvs not */
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++)
		if (CL_SoldierInAircraft(&gd.employees[EMPL_SOLDIER][i], aircraft)) {
			character_t *chr = &gd.employees[EMPL_SOLDIER][i].chr;
			assert(chr);
			if (!gd.employees[EMPL_SOLDIER][i].hired) {
				Sys_Error("Employee %s is reported as being on the aircraft (%s), but he is not hired (%i/%i)",
					chr->name, aircraft->id, i, gd.numEmployees[EMPL_SOLDIER]);
			}
			assert(gd.employees[EMPL_SOLDIER][i].baseHired == aircraft->homebase);

			Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: searching for soldier: %i\n", i);

			/* Remember the number of assigned mission for this character. */
			chr->score.assignedMissions++;

			/** @todo use chrScore_t to determine negative influence on soldier here,
			 * like killing too many civilians and teammates can lead to unhire and disband
			 * such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			 * and do a promotion. */
			if (gd.numRanks >= 2) {
				for (j = gd.numRanks - 1; j > chr->score.rank; j--) {
					const rank_t *rank = &gd.ranks[j];
					/** @todo (Zenerka 20080301) extend ranks and change calculations here. */
					if (rank->type == EMPL_SOLDIER && (chr->score.skills[ABILITY_MIND] >= rank->mind)
						&& (chr->score.kills[KILLED_ALIENS] >= rank->killed_enemies)
						&& ((chr->score.kills[KILLED_CIVILIANS] + chr->score.kills[KILLED_TEAM]) <= rank->killed_others)) {
						chr->score.rank = j;
						if (chr->HP > 0)
							Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%s has been promoted to %s.\n"), chr->name, rank->name);
						else
							Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%s has been awarded the posthumous rank of %s\\for inspirational gallantry in the face of overwhelming odds.\n"), chr->name, rank->name);
						MN_AddNewMessage(_("Soldier promoted"), mn.messageBuffer, qfalse, MSG_PROMOTION, NULL);
						break;
					}
				}
			}
		}
	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: Done\n");
}

#ifdef DEBUG
/**
 * @brief Debug function to add one item of every type to base storage and mark them collected.
 * @note Command to call this: debug_additems
 */
static void CL_DebugAllItems_f (void)
{
	int i;
	base_t *base;
	technology_t *tech;
	objDef_t *obj;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		obj = &csi.ods[i];
		if (!obj->weapon && !obj->numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, obj, 1, qfalse, qtrue);
		if (base->storage.num[i] > 0) {
			tech = obj->tech;
			if (!tech)
				Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", obj->id, obj->name);
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_listitem
 */
static void CL_DebugShowItems_f (void)
{
	int i;
	base_t *base;
	objDef_t *obj;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		obj = &csi.ods[i];
		if (!obj->tech)
			Sys_Error("CL_DebugAllItems_f: No tech for %s\n", obj->id);
		Com_Printf("%i. %s: %i\n", i, obj->id, base->storage.num[i]);
	}
}

/**
 * @brief Debug function to set the credits to max
 */
static void CL_DebugFullCredits_f (void)
{
	CL_UpdateCredits(MAX_CREDITS);
}

/**
 * @brief Debug function to add 5 new unhired employees of each type
 * @note called with debug_addemployees
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;
	nation_t *nation = &gd.nations[0];	/**< This is just a debuging function, nation does not matter */

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a pilot. */
		E_CreateEmployee(EMPL_PILOT, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a ares  ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUgvByID("ugv_ares_w"));

	for (j = 0; j < 5; j++)
		/* Create a phoenix  ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUgvByID("ugv_phoenix"));
}

/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f (void)
{
	int i, j;
	character_t *chr;

	if (!baseCurrent)
		return;

	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		employee_t *employee = &gd.employees[EMPL_SOLDIER][i];

		if (!employee->hired && employee->baseHired != baseCurrent)
			continue;

		chr = &(employee->chr);
		assert(chr);

		for (j = 0; j < KILLED_NUM_TYPES; j++)
			chr->score.kills[j]++;
	}
	if (baseCurrent->aircraftCurrent)
		CL_UpdateCharacterStats(baseCurrent, 1, baseCurrent->aircraftCurrent);
}
#endif

/**
 * @sa CL_ParseResults
 * @sa CL_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void CL_GameResults_f (void)
{
	int won;
	int civilians_killed;
	int aliens_killed;
	int i;
	base_t *base;
	aircraft_t *aircraft;
	int numberofsoldiers = 0; /* DEBUG */

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f\n");

	/* multiplayer? */
	if (!curCampaign || !selectedMission)
		return;

	/* check for replay */
	if (Cvar_VariableInteger("game_tryagain")) {
		/* don't collect things and update stats --- we retry the mission */
		CL_GameGo();
		return;
	}
	/* check for win */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <won>\n", Cmd_Argv(0));
		return;
	}
	won = atoi(Cmd_Argv(1));

	if (selectedMission->stage == STAGE_BASE_ATTACK) {
		base = (base_t *)selectedMission->data;
		aircraft = &baseAttackFakeAircraft;
		assert(base);
	} else {
		/* note that gd.interceptAircraft is baseAttackFakeAircraft in case of base Attack */
		assert(gd.interceptAircraft);
		aircraft = gd.interceptAircraft;
		base = aircraft->homebase;
		assert(base);
	}

	/* add the looted goods to base storage and market */
	base->storage = ccs.eMission; /* copied, including the arrays! */

	civilians_killed = ccs.civiliansKilled;
	aliens_killed = ccs.aliensKilled;
	Com_DPrintf(DEBUG_CLIENT, "Won: %d   Civilians: %d/%d   Aliens: %d/%d\n",
		won, ccs.battleParameters.civilians - civilians_killed, civilians_killed,
		ccs.battleParameters.aliens - aliens_killed, aliens_killed);
	CL_HandleNationData(!won, ccs.battleParameters.civilians - civilians_killed, civilians_killed, ccs.battleParameters.aliens - aliens_killed, aliens_killed, selectedMission);
	CP_CheckLostCondition(!won, selectedMission, civilians_killed);

	/* update the character stats */
	CL_ParseCharacterData(NULL);

	/* update stats */
	CL_UpdateCharacterStats(base, won, aircraft);

	/* Backward loop because gd.numEmployees[EMPL_SOLDIER] is decremented by E_DeleteEmployee */
	for (i = gd.numEmployees[EMPL_SOLDIER] - 1; i >= 0; i--) {
		employee_t *employee = &gd.employees[EMPL_SOLDIER][i];

		if (CL_SoldierInAircraft(employee, aircraft))
			numberofsoldiers++;

		Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - try to get player %i \n", i);

		if (employee->hired && employee->baseHired == base) {
			const character_t *chr = &(employee->chr);
			assert(chr);
			Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - idx %d hp %d\n", chr->ucn, chr->HP);
			/* if employee is marked as dead */
			if (chr->HP <= 0) { /** @todo <= -50, etc. (implants) */
				/* Delete the employee. */
				/* sideeffect: gd.numEmployees[EMPL_SOLDIER] and teamNum[] are decremented by one here. */
				Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f: Delete this dead employee: %i (%s)\n", i, chr->name);
				E_DeleteEmployee(employee, EMPL_SOLDIER);
			} /* if dead */
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - num %i\n", numberofsoldiers); /* DEBUG */

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - done removing dead players\n");

	/* Check for alien containment in aircraft homebase. */
	if (aircraft->alientypes && !B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		/* We have captured/killed aliens, but the homebase of this aircraft does not have alien containment.
		 * Popup aircraft transer dialog to choose a better base. */
		TR_TransferAircraftMenu(aircraft);
	} else {
		/* The aircraft can be savely sent to its homebase without losing aliens */
	}

	/* handle base attack mission */
	if (selectedMission->stage == STAGE_BASE_ATTACK) {
		if (won) {
			/* fake an aircraft return to collect goods and aliens */
			CL_AircraftReturnedToHomeBase(aircraft);

			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Defence of base: %s successful!"), base->name);
			MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(selectedMission);
			/** @todo @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CP_BaseAttackMissionLeave(selectedMission);
	} else {
		AIR_AircraftReturnToBase(aircraft);
		if (won)
			CP_MissionIsOver(selectedMission);
	}
}

/**
 * @return Alien Team Type
 * @sa alienTeamType_t
 */
static int CL_GetAlienTeamTypeByID (const char *type)
{
	if (!Q_strncmp(type, "default", MAX_VAR))
		return ALIENTEAM_DEFAULT;
	else if (!Q_strncmp(type, "xvi", MAX_VAR))
		return ALIENTEAM_XVI;
	else if (!Q_strncmp(type, "harvest", MAX_VAR))
		return ALIENTEAM_HARVEST;
	else {
		Com_Printf("CL_GetAlienTeamTypePerId: unknown alien type '%s'\n", type);
		return ALIENTEAM_MAX;
	}
}

/**
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseAlienTeam (const char *name, const char **text)
{
	const char *errhead = "CL_ParseAlienTeam: unexpected end of file (alienteam ";
	const char *token;
	int alienType;
	int num;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseAlienTeam: alien team def \"%s\" without body ignored\n", name);
		return;
	}

	alienType = CL_GetAlienTeamTypeByID(name);
	if (alienType == ALIENTEAM_MAX)
		return;

	if (gd.numAlienTeams[alienType] >= MAX_ALIEN_TEAM_LEVEL) {
		Com_Printf("CL_ParseAlienTeam: maximum number of alien team type reached (%i)\n", MAX_ALIEN_TEAM_LEVEL);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (Q_strcmp(token, "team")) {
			Com_Printf("CL_ParseAlienTeam: unknown token \"%s\" ignored (alienteam %s)\n", token, name);
			continue;
		}
		token = COM_EParse(text, errhead, name);
		if (!*text || *token != '{') {
			Com_Printf("CL_ParseAlienTeam: alienteam type \"%s\" has team with no opening brace\n", name);
			break;
		}
		for (num = 0; *text; num++) {
			token = COM_EParse(text, errhead, name);
			if (!*text || *token == '}')
				break;
			if (num >= MAX_TEAMS_PER_MISSION)
				Sys_Error("CL_ParseAlienTeam: MAX_TEAMS_PER_MISSION hit");
			if (Com_GetTeamDefinitionByID(token))
				gd.alienTeams[alienType][gd.numAlienTeams[alienType]][num] = Com_GetTeamDefinitionByID(token);
		}
		gd.numAlienTeams[alienType]++;
	} while (*text);
}

/**
 * @brief This function parses a list of items that should be set to researched = true after campaign start
 * @todo Implement the use of this function
 */
void CL_ParseResearchedCampaignItems (const char *name, const char **text)
{
	const char *errhead = "CL_ParseResearchedCampaignItems: unexpected end of file (equipment ";
	const char *token;
	int i;
	campaign_t* campaign;
	technology_t *tech;

	campaign = CL_GetCampaign(cl_campaign->string);
	if (!campaign) {
		Com_Printf("CL_ParseResearchedCampaignItems: failed\n");
		return;
	}
	/* Don't parse if it is not definition for current type of campaign. */
	if ((Q_strncmp(campaign->researched, name, MAX_VAR)) != 0)
		return;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseResearchedCampaignItems: equipment def \"%s\" without body ignored (%s)\n", name, token);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign research list '%s'\n", name);
	do {
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			assert(tech);
			if (!Q_strncmp(token, tech->id, MAX_VAR)) {
				tech->mailSent = MAILSENT_FINISHED;
				tech->markResearched.markOnly[tech->markResearched.numDefinitions] = qtrue;
				tech->markResearched.campaign[tech->markResearched.numDefinitions] = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
				tech->markResearched.numDefinitions++;
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
		}

		if (i == gd.numTechnologies)
			Com_Printf("CL_ParseResearchedCampaignItems: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/**
 * @brief This function parses a list of items that should be set to researchable = true after campaign start
 * @param researchable Mark them researchable or not researchable
 * @sa CL_ParseScriptFirst
 */
void CL_ParseResearchableCampaignStates (const char *name, const char **text, qboolean researchable)
{
	const char *errhead = "CL_ParseResearchableCampaignStates: unexpected end of file (equipment ";
	const char *token;
	int i;
	campaign_t* campaign;
	technology_t *tech;

	campaign = CL_GetCampaign(cl_campaign->string);
	if (!campaign) {
		Com_Printf("CL_ParseResearchableCampaignStates: failed\n");
		return;
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseResearchableCampaignStates: equipment def \"%s\" without body ignored\n", name);
		return;
	}

	if (Q_strncmp(campaign->researched, name, MAX_VAR)) {
		Com_DPrintf(DEBUG_CLIENT, "..don't use '%s' as researchable list\n", name);
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "..campaign researchable list '%s'\n", name);
	do {
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < gd.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			if (!Q_strncmp(token, tech->id, MAX_VAR)) {
				if (researchable) {
					tech->mailSent = MAILSENT_PROPOSAL;
					RS_MarkOneResearchable(tech);
				} else
					Com_Printf("@todo Mark unresearchable");
				Com_DPrintf(DEBUG_CLIENT, "...tech %s\n", tech->id);
				break;
			}
		}

		if (i == gd.numTechnologies)
			Com_Printf("CL_ParseResearchableCampaignStates: unknown token \"%s\" ignored (tech %s)\n", token, name);

	} while (*text);
}

/* =========================================================== */

static const value_t salary_vals[] = {
	{"soldier_base", V_INT, offsetof(salary_t, soldier_base), MEMBER_SIZEOF(salary_t, soldier_base)},
	{"soldier_rankbonus", V_INT, offsetof(salary_t, soldier_rankbonus), MEMBER_SIZEOF(salary_t, soldier_rankbonus)},
	{"worker_base", V_INT, offsetof(salary_t, worker_base), MEMBER_SIZEOF(salary_t, worker_base)},
	{"worker_rankbonus", V_INT, offsetof(salary_t, worker_rankbonus), MEMBER_SIZEOF(salary_t, worker_rankbonus)},
	{"scientist_base", V_INT, offsetof(salary_t, scientist_base), MEMBER_SIZEOF(salary_t, scientist_base)},
	{"scientist_rankbonus", V_INT, offsetof(salary_t, scientist_rankbonus), MEMBER_SIZEOF(salary_t, scientist_rankbonus)},
	{"pilot_base", V_INT, offsetof(salary_t, pilot_base), MEMBER_SIZEOF(salary_t, pilot_base)},
	{"pilot_rankbonus", V_INT, offsetof(salary_t, pilot_rankbonus), MEMBER_SIZEOF(salary_t, pilot_rankbonus)},
	{"robot_base", V_INT, offsetof(salary_t, robot_base), MEMBER_SIZEOF(salary_t, robot_base)},
	{"robot_rankbonus", V_INT, offsetof(salary_t, robot_rankbonus), MEMBER_SIZEOF(salary_t, robot_rankbonus)},
	{"aircraft_factor", V_INT, offsetof(salary_t, aircraft_factor), MEMBER_SIZEOF(salary_t, aircraft_factor)},
	{"aircraft_divisor", V_INT, offsetof(salary_t, aircraft_divisor), MEMBER_SIZEOF(salary_t, aircraft_divisor)},
	{"base_upkeep", V_INT, offsetof(salary_t, base_upkeep), MEMBER_SIZEOF(salary_t, base_upkeep)},
	{"admin_initial", V_INT, offsetof(salary_t, admin_initial), MEMBER_SIZEOF(salary_t, admin_initial)},
	{"admin_soldier", V_INT, offsetof(salary_t, admin_soldier), MEMBER_SIZEOF(salary_t, admin_soldier)},
	{"admin_worker", V_INT, offsetof(salary_t, admin_worker), MEMBER_SIZEOF(salary_t, admin_worker)},
	{"admin_scientist", V_INT, offsetof(salary_t, admin_scientist), MEMBER_SIZEOF(salary_t, admin_scientist)},
	{"admin_pilot", V_INT, offsetof(salary_t, admin_pilot), MEMBER_SIZEOF(salary_t, admin_pilot)},
	{"admin_robot", V_INT, offsetof(salary_t, admin_robot), MEMBER_SIZEOF(salary_t, admin_robot)},
	{"debt_interest", V_FLOAT, offsetof(salary_t, debt_interest), MEMBER_SIZEOF(salary_t, debt_interest)},
	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the salaries from campaign definition
 * @param[in] name Name or ID of the found character skill and ability definition
 * @param[in] text The text of the nation node
 * @param[in] campaignID Current campaign id (idx)
 * @note Example:
 * <code>salary {
 *  soldier_base 3000
 * }</code>
 */
static void CL_ParseSalary (const char *name, const char **text, int campaignID)
{
	const char *errhead = "CL_ParseSalary: unexpected end of file ";
	salary_t *s;
	const value_t *vp;
	const char *token;

	/* initialize the campaign */
	s = &salaries[campaignID];

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseSalary: salary def without body ignored\n");
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = salary_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_EParseValue(s, token, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string) {
			Com_Printf("CL_ParseSalary: unknown token \"%s\" ignored (campaignID %i)\n", token, campaignID);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/* =========================================================== */

static const value_t campaign_vals[] = {
	{"team", V_STRING, offsetof(campaign_t, team), 0},
	{"soldiers", V_INT, offsetof(campaign_t, soldiers), MEMBER_SIZEOF(campaign_t, soldiers)},
	{"workers", V_INT, offsetof(campaign_t, workers), MEMBER_SIZEOF(campaign_t, workers)},
	{"xvirate", V_INT, offsetof(campaign_t, maxAllowedXVIRateUntilLost), MEMBER_SIZEOF(campaign_t, maxAllowedXVIRateUntilLost)},
	{"maxdebts", V_INT, offsetof(campaign_t, negativeCreditsUntilLost), MEMBER_SIZEOF(campaign_t, negativeCreditsUntilLost)},
	{"minhappiness", V_FLOAT, offsetof(campaign_t, minhappiness), MEMBER_SIZEOF(campaign_t, minhappiness)},
	{"scientists", V_INT, offsetof(campaign_t, scientists), MEMBER_SIZEOF(campaign_t, scientists)},
	{"ugvs", V_INT, offsetof(campaign_t, ugvs), MEMBER_SIZEOF(campaign_t, ugvs)},
	{"equipment", V_STRING, offsetof(campaign_t, equipment), 0},
	{"market", V_STRING, offsetof(campaign_t, market), 0},
	{"asymptotic_market", V_STRING, offsetof(campaign_t, asymptoticMarket), 0},
	{"researched", V_STRING, offsetof(campaign_t, researched), 0},
	{"difficulty", V_INT, offsetof(campaign_t, difficulty), MEMBER_SIZEOF(campaign_t, difficulty)},
	{"map", V_STRING, offsetof(campaign_t, map), 0},
	{"credits", V_INT, offsetof(campaign_t, credits), MEMBER_SIZEOF(campaign_t, credits)},
	{"visible", V_BOOL, offsetof(campaign_t, visible), MEMBER_SIZEOF(campaign_t, visible)},
	{"text", V_TRANSLATION_MANUAL_STRING, offsetof(campaign_t, text), 0}, /* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name), 0},
	{"date", V_DATE, offsetof(campaign_t, date), 0},
	{"basecost", V_INT, offsetof(campaign_t, basecost), MEMBER_SIZEOF(campaign_t, basecost)},
	{"firstbase", V_STRING, offsetof(campaign_t, firstBaseTemplate), 0},
	{NULL, 0, 0, 0}
};

/**
 * @sa CL_ParseClientData
 */
void CL_ParseCampaign (const char *name, const char **text)
{
	const char *errhead = "CL_ParseCampaign: unexpected end of file (campaign ";
	campaign_t *cp;
	const value_t *vp;
	const char *token;
	int i;
	salary_t *s;

	/* search for campaigns with same name */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp(name, campaigns[i].id, sizeof(campaigns[i].id)))
			break;

	if (i < numCampaigns) {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (numCampaigns >= MAX_CAMPAIGNS) {
		Com_Printf("CL_ParseCampaign: Max campaigns reached (%i)\n", MAX_CAMPAIGNS);
		return;
	}

	/* initialize the campaign */
	cp = &campaigns[numCampaigns++];
	memset(cp, 0, sizeof(*cp));

	cp->idx = numCampaigns - 1;
	Q_strncpyz(cp->id, name, sizeof(cp->id));

	/* some default values */
	Q_strncpyz(cp->team, "human", sizeof(cp->team));
	Q_strncpyz(cp->researched, "researched_human", sizeof(cp->researched));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseCampaign: campaign def \"%s\" without body ignored\n", name);
		numCampaigns--;
		return;
	}

	/* some default values */
	s = &salaries[cp->idx];
	memset(s, 0, sizeof(*s));
	s->soldier_base = 3000;
	s->soldier_rankbonus = 500;
	s->worker_base = 3000;
	s->worker_rankbonus = 500;
	s->scientist_base = 3000;
	s->scientist_rankbonus = 500;
	s->pilot_base = 3000;
	s->pilot_rankbonus = 500;
	s->robot_base = 7500;
	s->robot_rankbonus = 1500;
	s->aircraft_factor = 1;
	s->aircraft_divisor = 25;
	s->base_upkeep = 20000;
	s->admin_initial = 1000;
	s->admin_soldier = 75;
	s->admin_worker = 75;
	s->admin_scientist = 75;
	s->admin_pilot = 75;
	s->admin_robot = 150;
	s->debt_interest = 0.005;

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = campaign_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_EParseValue(cp, token, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!Q_strncmp(token, "salary", MAX_VAR)) {
			CL_ParseSalary(token, text, cp->idx);
		} else if (!vp->string) {
			Com_Printf("CL_ParseCampaign: unknown token \"%s\" ignored (campaign %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @brief Check whether we are in a tactical mission as server or as client
 * @note handles multiplayer and singleplayer
 *
 * @return true when we are in battlefield
 * @todo Check cvar mn_main for value
 */
qboolean CL_OnBattlescape (void)
{
	/* server_state is set to zero (ss_dead) on every battlefield shutdown */
	if (Com_ServerState())
		return qtrue; /* server */

	/* client */
	if (cls.state >= ca_connected)
		return qtrue;

	return qfalse;
}

#ifdef DEBUG
/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnation
 */
static void CL_NationList_f (void)
{
	int i;
	for (i = 0; i < gd.numNations; i++) {
		Com_Printf("Nation ID: %s\n", gd.nations[i].id);
		Com_Printf("...max-funding %i c\n", gd.nations[i].maxFunding);
		Com_Printf("...alienFriendly %0.2f\n", gd.nations[i].stats[0].alienFriendly);
		Com_Printf("...happiness %0.2f\n", gd.nations[i].stats[0].happiness);
		Com_Printf("...xviInfection %i\n", gd.nations[i].stats[0].xviInfection);
		Com_Printf("...max-soldiers %i\n", gd.nations[i].maxSoldiers);
		Com_Printf("...max-scientists %i\n", gd.nations[i].maxScientists);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", gd.nations[i].color[0], gd.nations[i].color[1], gd.nations[i].color[2], gd.nations[i].color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", gd.nations[i].pos[0], gd.nations[i].pos[1]);
	}
}
#endif

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"mn_next_aircraft", AIM_NextAircraft_f, NULL},
	{"mn_prev_aircraft", AIM_PrevAircraft_f, NULL},
	{"aircraft_new", AIR_NewAircraft_f, NULL},
	{"mn_reset_air", AIM_ResetAircraftCvars_f, NULL},
	{"aircraft_return", AIR_AircraftReturnToBase_f, "Sends the current aircraft back to homebase"},
	{"aircraft_start", AIM_AircraftStart_f, NULL},
	{"aircraft_select", AIR_AircraftSelect_f, NULL},
	{"airequip_updatemenu", AIM_AircraftEquipMenuUpdate_f, "Init function for the aircraft equip menu"},
	{"airequip_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"airequip_slot_select", AIM_AircraftEquipSlotSelect_f, NULL},
	{"airequip_zone_select", AIM_AircraftEquipZoneSelect_f, NULL},
	{"airequip_add_item", AIM_AircraftEquipAddItem_f, "Add item to slot"},
	{"airequip_del_item", AIM_AircraftEquipDeleteItem_f, "Remove item from slot"},
	{"add_battery", BDEF_AddBattery_f, "Add a new battery to base"},
	{"remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base"},
	{"basedef_initmenu", BDEF_MenuInit_f, "Inits base defence menu"},
	{"basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defence menu"},
	{"basedef_slot_list_click", BDEF_ListClick_f, "Inits base defence menu"},
	{"basedef_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"update_base_radar_coverage", RADAR_UpdateBaseRadarCoverage_f, "Update base radar coverage"},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"stats_update", CL_StatsUpdate_f, NULL},
	{"combatzoom_exit", MAP_CombatZoomExit_f, "Exit combat zoom mode."},
	{"airfightmap_init", AFM_Init_f, "Exit air fight map mode."},
	{"airfightmap_exit", AFM_Exit_f, "Exit air fight map mode."},
	{"game_go", CL_GameGo, NULL},
	{"game_auto_check", CL_GameAutoCheck_f, "Checks whether this mission can be done automatically"},
	{"game_auto_go", CL_GameAutoGo_f, "Let the current selection mission be done automatically"},
	{"game_abort", CL_GameAbort_f, "Abort the game and let the aliens win"},
	{"game_results", CL_GameResults_f, "Parses and shows the game results"},
	{"game_timestop", CL_GameTimeStop, NULL},
	{"game_timeslow", CL_GameTimeSlow, NULL},
	{"game_timefast", CL_GameTimeFast, NULL},
	{"mn_mapaction_reset", MAP_ResetAction, NULL},
	{"map_center", MAP_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"map_zoom", MAP_Zoom_f, NULL},
	{"map_scroll", MAP_Scroll_f, NULL},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
#ifdef DEBUG
	{"debug_listnation", CL_NationList_f, "List all nations on the game console"},
	{"debug_listaircraft", AIR_ListAircraft_f, "Debug function to list all aircraft in all bases"},
	{"debug_fullcredits", CL_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_addemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
	{"debug_additems", CL_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_listitem", CL_DebugShowItems_f, "Debug function to show all items in base storage"},
	{"debug_listmission", CL_DebugMissionList_f, "Debug function to show all missions"},
	{"debug_listinterest", CP_AlienInterestList_f, "Debug function to show alien interest values"},
#endif
	{NULL, NULL, NULL}
};

/**
 * @sa CL_GameNew_f
 * @sa SAV_GameLoad
 */
void CL_GameExit (void)
{
	const cmdList_t *commands;

	if (Com_ServerState())
		SV_Shutdown("Game exit", qfalse);
	else
		CL_Disconnect();
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_active", "");

	/* singleplayer commands are no longer available */
	if (curCampaign) {
		Com_DPrintf(DEBUG_CLIENT, "Remove game commands\n");
		for (commands = game_commands; commands->name; commands++) {
			Com_DPrintf(DEBUG_CLIENT, "...%s\n", commands->name);
			Cmd_RemoveCommand(commands->name);
		}

		CL_ResetSinglePlayerData();
	}
	curCampaign = NULL;
	selActor = NULL;
	cls.missionaircraft = NULL;

	/* maybe this is not the best place - but it is needed */
	menuInventory = NULL;
}

/**
 * @brief Called at new game and load game
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 * @sa SAV_GameLoad
 * @sa CL_GameNew_f
 */
void CL_GameInit (qboolean load)
{
	const cmdList_t *commands;

	assert(curCampaign);

	Com_DPrintf(DEBUG_CLIENT, "Init game commands\n");
	for (commands = game_commands; commands->name; commands++) {
		Com_DPrintf(DEBUG_CLIENT, "...%s\n", commands->name);
		Cmd_AddCommand(commands->name, commands->function, commands->description);
	}

	CL_GameTimeStop();

	Com_AddObjectLinks();	/**< Add tech links + ammo<->weapon links to items.*/
	RS_InitTree(load);		/**< Initialise all data in the research tree. */

	CL_CampaignInitMarket(load);

	/* Init popup and map/geoscape */
	CL_PopupInit();

	rsAlienXVI = RS_GetTechByID(XVI_EVENT_NAME);
	if (!rsAlienXVI)
		Sys_Error("CL_GameInit: Could not find tech definition for " XVI_EVENT_NAME);

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();
}

/**
 * @brief Returns the campaign pointer from global campaign array
 * @param name Name of the campaign
 * @return campaign_t pointer to campaign with name or NULL if not found
 */
campaign_t* CL_GetCampaign (const char* name)
{
	campaign_t* campaign;
	int i;

	for (i = 0, campaign = campaigns; i < numCampaigns; i++, campaign++)
		if (!Q_strncmp(name, campaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("CL_GetCampaign: Campaign \"%s\" doesn't exist.\n", name);
		return NULL;
	}
	return campaign;
}

/**
 * @brief Starts a new skirmish game
 * @todo Check the stuff in this function - maybe not every function call
 * is needed here or maybe some are missing
 */
static void CL_GameSkirmish_f (void)
{
	base_t* base;
	char map[MAX_VAR];
	mapDef_t *md;
	int i;

	if (!ccs.singleplayer)
		return;

	assert(cls.currentSelectedMap >= 0);
	assert(cls.currentSelectedMap < MAX_MAPDEFS);

	md = &csi.mds[cls.currentSelectedMap];
	if (!md)
		return;

	assert(md->map);
	Com_sprintf(map, sizeof(map), "map %s %s %s;", mn_serverday->integer ? "day" : "night", md->map, md->param ? md->param : "");

	/* exit running game */
	if (curCampaign)
		CL_GameExit();

	/* get campaign - they are already parsed here */
	curCampaign = CL_GetCampaign(cl_campaign->string);
	if (!curCampaign) {
		Com_Printf("CL_GameSkirmish_f: Could not find campaign '%s'\n", cl_campaign->string);
		return;
	}

	Cvar_Set("cl_team", curCampaign->team);

	memset(&ccs, 0, sizeof(ccs));
	CL_StartSingleplayer(qtrue);
	CL_ReadSinglePlayerData();

	/* create employees and clear bases */
	B_NewBases();

	base = B_GetBaseByIDX(0);
	baseCurrent = base;
	gd.numAircraft = 0;

	CL_GameInit(qfalse);
	RS_MarkResearchedAll();

	gd.numBases = 1;

	/* even in skirmish mode we need a little money to build the base */
	CL_UpdateCredits(MAX_CREDITS);

	/* only needed for base dummy creation */
	R_CreateRadarOverlay();

	/* build our pseudo base */
	B_SetUpBase(base, qtrue, qtrue);

	if (!base->numAircraftInBase) {
		Com_Printf("CL_GameSkirmish_f: Error - there is no dropship in base\n");
		return;
	}

	/* we have to set this manually here */
	for (i = 0; i < base->numAircraftInBase; i++) {
		if (base->aircraft[i].type == AIRCRAFT_TRANSPORTER) {
			cls.missionaircraft = &base->aircraft[i];
			break;
		}
	}

	base->aircraftCurrent = cls.missionaircraft;

	if (!cls.missionaircraft || !cls.missionaircraft->homebase) {
		Com_Printf("CL_GameSkirmish_f: Error - could not set the mission aircraft: %i\n", base->numAircraftInBase);
		return;
	}

	/* prepare */
	MN_PopMenu(qtrue);
	Cvar_Set("mn_main_afterdrop", "singleplayermission");

	/* make sure, that we are not trying to switch to singleplayer a second time */
	sv_maxclients->modified = qfalse;

	/* this is no real campaign - but we need the pointer for some of
	 * the previous actions */
	curCampaign = NULL;
	Cbuf_AddText(map);
}

/**
 * @brief Starts a new single-player game
 * @sa CL_GameInit
 * @sa CL_CampaignInitMarket
 */
static void CL_GameNew_f (void)
{
	char val[8];

	/* exit running game */
	if (curCampaign)
		CL_GameExit();

	/* get campaign - they are already parsed here */
	curCampaign = CL_GetCampaign(cl_campaign->string);
	if (!curCampaign)
		return;

	/* reset, set time */
	selectedMission = NULL;

	memset(&ccs, 0, sizeof(ccs));
	CL_StartSingleplayer(qtrue);

	/* initialise view angle for 3D geoscape so that europe is seen */
	ccs.angles[YAW] = GLOBE_ROTATE;
	/* initialise date */
	ccs.date = curCampaign->date;

	CL_ReadSinglePlayerData();

	Cvar_Set("cl_team", curCampaign->team);

	/* difficulty */
	Com_sprintf(val, sizeof(val), "%i", curCampaign->difficulty);
	Cvar_ForceSet("difficulty", val);

	if (difficulty->integer < -4)
		Cvar_ForceSet("difficulty", "-4");
	else if (difficulty->integer > 4)
		Cvar_ForceSet("difficulty", "4");

	MAP_Init();

	/* base setup */
	gd.numBases = 0;
	Cvar_Set("mn_base_count", "0");
	gd.numAircraft = 0;

	B_NewBases();
	INS_NewInstallations();
	PR_ProductionInit();
	R_CreateRadarOverlay();

	/* ensure ccs.singleplayer is set to true */
	CL_StartSingleplayer(qtrue);

	/* get day */
	while (ccs.date.sec > SECONDS_PER_DAY) {
		ccs.date.sec -= SECONDS_PER_DAY;
		ccs.date.day++;
	}

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(curCampaign->credits);

	/* Initialize alien interest */
	CL_ResetAlienInterest();

	/* Initialize XVI overlay */
	R_InitializeXVIOverlay(curCampaign->map, NULL, 0, 0);

	/* Reset alien bases */
	AB_ResetAlienBases();

	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	MN_PushMenu("map");

	/* create a base as first step */
	B_SelectBase(NULL);

	CL_GameInit(qfalse);
	Cmd_ExecuteString("addeventmail prolog");

	/* Spawn first missions of the game */
	CP_InitializeSpawningDelay();

	/* Intro sentences */
	Cbuf_AddText("seq_start intro;\n");
}

#define MAXCAMPAIGNTEXT 4096
static char campaignText[MAXCAMPAIGNTEXT];
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Fill the campaign list with available campaigns
 */
static void CP_GetCampaigns_f (void)
{
	int i;

	*campaignText = *campaignDesc = '\0';
	for (i = 0; i < numCampaigns; i++) {
		if (campaigns[i].visible)
			Q_strcat(campaignText, va("%s\n", campaigns[i].name), MAXCAMPAIGNTEXT);
	}
	/* default campaign */
	mn.menuText[TEXT_STANDARD] = campaignDesc;

	/* select main as default */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strncmp("main", campaigns[i].id, MAX_VAR)) {
			Cmd_ExecuteString(va("campaignlist_click %i", i));
			return;
		}
	Cmd_ExecuteString("campaignlist_click 0");
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param difficulty the difficulty integer value
 */
static inline const char* CL_ToDifficultyName (int difficulty)
{
	switch (difficulty) {
	case -4:
		return _("Chicken-hearted");
	case -3:
		return _("Very Easy");
	case -2:
	case -1:
		return _("Easy");
	case 0:
		return _("Normal");
	case 1:
	case 2:
		return _("Hard");
	case 3:
		return _("Very Hard");
	case 4:
		return _("Insane");
	default:
		Sys_Error("Unknown difficulty id %i\n", difficulty);
		break;
	}
	return NULL;
}

/**
 * @brief Script function to select a campaign from campaign list
 */
static void CP_CampaignsClick_f (void)
{
	int num;
	const char *racetype;
	menuNode_t *campaignlist;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* Which campaign in the list? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= numCampaigns)
		return;

	/* jump over all invisible campaigns */
	while (!campaigns[num].visible) {
		num++;
		if (num >= numCampaigns)
			return;
	}

	Cvar_Set("cl_campaign", campaigns[num].id);
	if (!Q_strncmp(campaigns[num].team, "human", 5))
		racetype = _("Human");
	else
		racetype = _("Aliens");

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("%s\n\nRace: %s\nRecruits: %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"), campaigns[num].name, racetype,
			campaigns[num].soldiers, ngettext("soldier", "soldiers", campaigns[num].soldiers),
			campaigns[num].scientists, ngettext("scientist", "scientists", campaigns[num].scientists),
			campaigns[num].workers, ngettext("worker", "workers", campaigns[num].workers),
			campaigns[num].credits, CL_ToDifficultyName(campaigns[num].difficulty),
			(int)(round(campaigns[num].minhappiness * 100.0f)), campaigns[num].negativeCreditsUntilLost,
			_(campaigns[num].text));
	mn.menuText[TEXT_STANDARD] = campaignDesc;

	/* Highlight currently selected entry */
	campaignlist = MN_GetNodeFromCurrentMenu("campaignlist");
	MN_TextNodeSelectLine(campaignlist, num);
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa INVSH_InitInventory
 * @sa CL_ReadSinglePlayerData
 */
void CL_ResetSinglePlayerData (void)
{
	int i;

	memset(&gd, 0, sizeof(gd));
	memset(&campaignStats, 0, sizeof(campaignStats));

	LIST_Delete(&ccs.missions);
	memset(&invList, 0, sizeof(invList));
	mn.messageStack = NULL;

	/* cleanup dynamic mails */
	CL_FreeDynamicEventMail();

	Mem_FreePool(cl_localPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetHash();
	E_ResetEmployees();
	INVSH_InitInventory(invList);
	/* Count Alien team definitions. */
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (csi.teamDef[i].alien)
			gd.numAliensTD++;
	}
	menuAircraft = NULL;
}

#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 * call this function via debug_listcampaign
 */
static void CP_CampaignStats_f (void)
{
	if (!curCampaign) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", curCampaign->id);
	Com_Printf("..research list: %s\n", curCampaign->researched);
	Com_Printf("..equipment: %s\n", curCampaign->equipment);
	Com_Printf("..team: %s\n", curCampaign->team);

	Com_Printf("..salaries:\n");
	Com_Printf("...soldier_base: %i\n", SALARY_SOLDIER_BASE);
	Com_Printf("...soldier_rankbonus: %i\n", SALARY_SOLDIER_RANKBONUS);
	Com_Printf("...worker_base: %i\n", SALARY_WORKER_BASE);
	Com_Printf("...worker_rankbonus: %i\n", SALARY_WORKER_RANKBONUS);
	Com_Printf("...scientist_base: %i\n", SALARY_SCIENTIST_BASE);
	Com_Printf("...scientist_rankbonus: %i\n", SALARY_SCIENTIST_RANKBONUS);
	Com_Printf("...pilot_base: %i\n", SALARY_PILOT_BASE);
	Com_Printf("...pilot_rankbonus: %i\n", SALARY_PILOT_RANKBONUS);
	Com_Printf("...robot_base: %i\n", SALARY_ROBOT_BASE);
	Com_Printf("...robot_rankbonus: %i\n", SALARY_ROBOT_RANKBONUS);
	Com_Printf("...aircraft_factor: %i\n", SALARY_AIRCRAFT_FACTOR);
	Com_Printf("...aircraft_divisor: %i\n", SALARY_AIRCRAFT_DIVISOR);
	Com_Printf("...base_upkeep: %i\n", SALARY_BASE_UPKEEP);
	Com_Printf("...admin_initial: %i\n", SALARY_ADMIN_INITIAL);
	Com_Printf("...admin_soldier: %i\n", SALARY_ADMIN_SOLDIER);
	Com_Printf("...admin_worker: %i\n", SALARY_ADMIN_WORKER);
	Com_Printf("...admin_scientist: %i\n", SALARY_ADMIN_SCIENTIST);
	Com_Printf("...admin_pilot: %i\n", SALARY_ADMIN_PILOT);
	Com_Printf("...admin_robot: %i\n", SALARY_ADMIN_ROBOT);
	Com_Printf("...debt_interest: %.5f\n", SALARY_DEBT_INTEREST);
}
#endif /* DEBUG */

/**
 * @brief Function to issue Try Again a Mission.
 * @note Command to call this: cp_tryagain.
 */
static void CP_TryAgain_f (void)
{
	/* Do nothing if user did other stuff. */
	if (Cvar_VariableInteger("mission_tryagain") == 1)
		return;
	Cbuf_AddText("set game_tryagain 1;mn_pop\n");
}

/**
 * @brief Returns "homebase" of the mission.
 * @note see struct client_static_s
 */
base_t *CP_GetMissionBase (void)
{
	assert(cls.missionaircraft && cls.missionaircraft->homebase);
	return cls.missionaircraft->homebase;
}

/**
 * @brief Determines a random position on Geoscape
 * @param[out] pos The position that will be overwritten. pos[0] is within -180, +180. pos[1] within -90, +90.
 * @param[in] noWater True if the position should not be on water
 * @sa CP_GetRandomPosOnGeoscapeWithParameters
 * @note The random positions should be roughly uniform thanks to the non-uniform distribution used.
 * @note This function always returns a value.
 */
void CP_GetRandomPosOnGeoscape (vec2_t pos, qboolean noWater)
{
	do {
		pos[0] = (frand() - 0.5f) * 360.0f;
		pos[1] = asin((frand() - 0.5f) * 2.0f) * todeg;
	} while (noWater && MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscape: Get random position on geoscape %.2f:%.2f\n", pos[0], pos[1]);
}

/**
 * @brief Determines a random position on Geoscape that fulfills certain criteria given via parameters
 * @param[out] pos The position that will be overwritten with the random point fulfilling the criterias. pos[0] is within -180, +180. pos[1] within -90, +90.
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the acceptable terrain types (e.g. "grass") May be NULL.
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the acceptable culture types (e.g. "western") May be NULL.
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the acceptable population types (e.g. "suburban") May be NULL.
 * @param[in] nations A linkedList_t containing a list of strings determining the acceptable nations (e.g. "asia"). May be NULL
 * @return true if a location was found, otherwise false
 * @note There may be no position fitting the parameters. The higher RASTER, the lower the probability to find a position.
 * @sa LIST_AddString
 * @sa LIST_Delete
 * @note When all parameters are NULL, the algorithm assumes that it does not need to include "water" terrains when determining a random position
 * @note You should rather use CP_GetRandomPosOnGeoscape if there are no parameters (except water) to choose a random position
 */
qboolean CP_GetRandomPosOnGeoscapeWithParameters (vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	float x, y;
	int num;
	int randomNum;

	/* RASTER might reduce amount of tested locations to get a better performance */
	/**< Number of points in latitude and longitude that will be tested. Therefore, the total number of position tried
	 * will be numPoints * numPoints */
	const float numPoints = 360.0 / RASTER;
	/* RASTER is minimizing the amount of locations, so an offset is introduced to enable access to all locations, depending on a random factor */
	const float offsetX = frand() * RASTER;
	const float offsetY = -1.0 + frand() * 2.0 / numPoints;
	vec2_t posT;
	int hits = 0;

	/* check all locations for suitability in 2 iterations */
	/* prepare 1st iteration */

	/* ITERATION 1 */
	for (y = 0; y < numPoints; y++) {
		const float posY = asin(2.0 * y / numPoints + offsetY) * todeg;	/* Use non-uniform distribution otherwise we favour the poles */
		for (x = 0; x < numPoints; x++) {
			const float posX = x * RASTER - 180.0 + offsetX;

			Vector2Set(posT, posX, posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				/* the location given in pos belongs to the terrain, culture, population types and nations
				 * that are acceptable, so count it */
				/** @todo - cache the counted hits */
				hits++;
			}
		}
	}

	/* if there have been no hits, the function failed to find a position */
	if (hits == 0)
		return qfalse;

	/* the 2nd iteration goes through the locations again, but does so only until a random point */
	/* prepare 2nd iteration */
	randomNum = num = rand() % hits;

	/* ITERATION 2 */
	for (y = 0; y < numPoints; y++) {
		const float posY = asin(2.0 * y / numPoints + offsetY) * todeg;
		for (x = 0; x < numPoints; x++) {
			const float posX = x * RASTER - 180.0 + offsetX;

			Vector2Set(posT,posX,posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				num--;

				if (num < 1) {
					Vector2Set(pos, posX, posY);
					Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscapeWithParameters: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
						pos[0], pos[1], randomNum, hits);
					return qtrue;
				}
			}
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscapeWithParameters: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
		pos[0], pos[1], num, hits);

	/* Make sure that position is within bounds */
	assert(pos[0] >= -180);
	assert(pos[0] <= 180);
	assert(pos[1] >= -90);
	assert(pos[1] <= 90);

	return qtrue;
}

void CP_InitStartup (void)
{
	/* reset some vars */
	curCampaign = NULL;
	baseCurrent = NULL;
	mn.menuText[TEXT_CAMPAIGN_LIST] = campaignText;

	cl_campaign = Cvar_Get("cl_campaign", "main", 0, "Which is the current selected campaign id");

	/* commands */
	Cmd_AddCommand("campaignlist_click", CP_CampaignsClick_f, NULL);
	Cmd_AddCommand("cp_getcampaigns", CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	Cmd_AddCommand("game_skirmish", CL_GameSkirmish_f, "Start the new skirmish game");
	Cmd_AddCommand("game_new", CL_GameNew_f, "Start the new campaign");
	Cmd_AddCommand("game_exit", CL_GameExit, NULL);
	Cmd_AddCommand("cp_tryagain", CP_TryAgain_f, "Try again a mission");
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
	Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
	Cmd_AddCommand("debug_addmission", CP_SpawnNewMissions_f, "Add a new mission");
	Cmd_AddCommand("debug_delmissions", CP_DeleteMissions_f, "Remove all missions from global array");
#endif
	Cmd_AddCommand("check_baseattacks", CP_CheckBaseAttacks_f, "Check if baseattack mission available and start it.");
}
