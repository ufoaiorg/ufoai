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
#include "cl_hospital.h"
#include "cl_actor.h"
#include "cl_popup.h"
#include "cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_alienbase.h"
#include "menu/m_popup.h"

extern void R_IncreaseXVILevel(const vec2_t pos);
extern void R_InitializeXVIOverlay(byte *data, int width, int height);
extern qboolean R_XVIMapCopy(byte *out, int size);

/* public vars */
mission_t *selectedMission;			/**< Currently selected mission on geoscape */
campaign_t *curCampaign;			/**< Current running campaign */
ccs_t ccs;
base_t *baseCurrent;				/**< Pointer to current base. */
stats_t campaignStats;
missionResults_t missionresults;

static campaign_t campaigns[MAX_CAMPAIGNS];
static int numCampaigns = 0;
static technology_t *rs_alien_xvi;
static salary_t salaries[MAX_CAMPAIGNS];
static cvar_t *cl_campaign;
static const int MAX_POS_LOOP = 10;				/**< Maximum number of loops to choose a mission position (to avoid infinite loops) */

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
	a.day += (a.sec / (3600 * 24)) + b.day;
	a.sec %= 3600 * 24;
	return a;
}

static date_t Date_Random (date_t frame)
{
	frame.sec = (frame.day * 3600 * 24 + frame.sec) * frand();
	frame.day = frame.sec / (3600 * 24);
	frame.sec = frame.sec % (3600 * 24);
	return frame;
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
		base->mapZone = MAP_GetTerrainType(colorTerrain);
		Com_DPrintf(DEBUG_CLIENT, "CL_NewBase: zoneType: '%s'\n", base->mapZone);
	}

	Com_DPrintf(DEBUG_CLIENT, "Colorvalues for base terrain: R:%i G:%i B:%i\n", colorTerrain[0], colorTerrain[1], colorTerrain[2]);

	/* build base */
	Vector2Copy(pos, base->pos);

	gd.numBases++;

	/* set up the base with buildings that have the autobuild flag set */
	B_SetUpBase(base);

	return qtrue;
}

/*****************************************************************************
 *
 * Alien interest functions
 *
 *****************************************************************************/

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

	/* Set initial individual interests
	 * @todo These values should probably be set in campaign.ufo */
	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		ccs.interest[i] = 0;
	ccs.interest[INTERESTCATEGORY_RECON] = 20;
	ccs.interest[INTERESTCATEGORY_NONE] = 20;
}

/**
 * @brief Change individual interest value
 * @param[in] percentage Maybe pe > 0 or < 0
 * @param[in] category Category of interest for aliens that should increase / decrease
 * @note Should be used when a mission is over (success or failure)
 */
static void CL_ChangeIndividualInterest (float percentage, interestCategory_t category)
{
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
}

/**
 * @brief Increase alien overall interest.
 * @sa CL_CampaignRun
 * @note hourly called
 */
static void CP_IncreaseAlienInterest (void)
{
	const int delayBetweenIncrease = 28;	/**< Number of hours between 2 overall interest increase
											 * @todo This should depend on difficulty */

	ccs.lastInterestIncreaseDelay++;

	if (ccs.lastInterestIncreaseDelay > delayBetweenIncrease) {
		ccs.overallInterest++;
		ccs.lastInterestIncreaseDelay -= delayBetweenIncrease;
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

#ifdef DEBUG
/**
 * @brief List alien interest values.
 * @sa function called with debug_interestlist
 */
static void CP_AlienInterestList_f (void)
{
	int i;

	Com_Printf("Overall interest: %i\n", ccs.overallInterest);
	Com_Printf("Individual interest:\n");

	for (i = 0; i < INTERESTCATEGORY_MAX; i++)
		Com_Printf("\t%i. -- %i\n", i, ccs.interest[i]);
}
#endif

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
 * @return qfalse if map is not selectable
 */
static qboolean CP_MapIsSelectable (mission_t *mission, int mapIdx, vec2_t pos, qboolean ufoCrashed)
{
	if (csi.mds[mapIdx].storyRelated)
		return qfalse;

	if (pos && !MAP_PositionFitsTCPNTypes(pos, csi.mds[mapIdx].terrains,
		csi.mds[mapIdx].cultures,csi.mds[mapIdx].populations, NULL))
		return qfalse;

	if (!mission->ufo) {
		/* a mission without UFO should not use a map with UFO */
		if (csi.mds[mapIdx].ufos)
			return qfalse;
	} else {
		/* A mission with UFO should use a map with UFO
		 * first check that list is not empty */
		if (!csi.mds[mapIdx].ufos)
			return qfalse;
		if (ufoCrashed) {
			if (!LIST_ContainsString(csi.mds[mapIdx].ufos, UFO_CrashedTypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		} else {
			if (!LIST_ContainsString(csi.mds[mapIdx].ufos, UFO_TypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		}
	}

	return qtrue;
}

/**
 * @brief Choose a map for given mission.
 * @param[in] mission Pointer to the mission where a new map should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] ufoCrashed true if the ufo is crashed
 * @return qfalse if could not set mission
 */
static qboolean CP_ChooseMap (mission_t *mission, vec2_t pos, qboolean ufoCrashed)
{
	int i;
	int maxHits = 1;
	int hits = 0;
	int minMissionAppearance = 9999;
	int randomNum;

	mission->mapDef = NULL;

	while (maxHits) {
		maxHits = 0;
		for (i = 0; i < csi.numMDs; i++) {

			/* Check if mission fulfill conditions */
			if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
				continue;

			maxHits++;
			if (csi.mds[i].timesAlreadyUsed < minMissionAppearance) {
				hits = 0;
				minMissionAppearance = csi.mds[i].timesAlreadyUsed;
				break;
			} else if (csi.mds[i].timesAlreadyUsed == minMissionAppearance)
				hits++;
		}

		if (i >= csi.numMDs)
			break;
	}

	if (!maxHits) {
		Com_Printf("CP_ChooseMap: Could not find map with required conditions:\n");
		Com_Printf("  ufo: %s -- pos: %s\n", ufoCrashed ? UFO_CrashedTypeToShortName(mission->ufo->ufotype) : UFO_TypeToShortName(mission->ufo->ufotype), pos ? va(("(%f, %f)"), pos[0], pos[1]) : "none");
		return qfalse;
	}

	randomNum = rand() % hits;

	for (i = 0; randomNum >= 0; i++) {
		/* Check if mission fulfill conditions */
		if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
			continue;

		if (csi.mds[i].timesAlreadyUsed == minMissionAppearance)
			randomNum--;
	}

	mission->mapDef = &csi.mds[i - 1];
	Com_DPrintf(DEBUG_CLIENT, "Selected map '%s' (among %i possible maps)\n", mission->mapDef->id, hits);

/* @todo Use me */
	Q_strncpyz(gd.oldMis3, gd.oldMis2, sizeof(gd.oldMis3));
	Q_strncpyz(gd.oldMis2, gd.oldMis1, sizeof(gd.oldMis2));
	Q_strncpyz(gd.oldMis1, mission->mapDef->id, sizeof(gd.oldMis1));
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
 * @todo implement me
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
 * @brief Removes a mission from geoscape: make it non visible and call notify functions
 */
static void CP_MissionRemoveFromGeoscape (mission_t *mission)
{
	mission->onGeoscape = qfalse;

	/* Notifications */
	MAP_NotifyMissionRemoved(mission);
	AIR_AircraftsNotifyMissionRemoved(mission);
}

/**
 * @brief Add a mission to geoscape: make it visible and stop time
 */
static inline void CP_MissionAddToGeoscape (mission_t *mission)
{
	mission->onGeoscape = qtrue;
	CL_GameTimeStop();
}

/**
 * @brief Removes (temporarily) a UFO from geoscape: make it non visible and call notify functions
 * @note We don't destroy the UFO because we can use later, e.g. if it takes off
 */
static void CP_UFORemoveFromGeoscape (mission_t *mission)
{
	mission->ufo->notOnGeoscape = qtrue;

	/* Notications */
	AIR_AircraftsNotifyUFORemoved(mission->ufo);
	MAP_NotifyUFORemoved(mission->ufo);
}

/**
 * @brief Removes a mission from mission global array.
 * @sa UFO_RemoveFromGeoscape
 */
static void CP_MissionRemove (mission_t *mission)
{
	linkedList_t *list = ccs.missions;
	mission_t *removedMission;

	/* Destroy UFO */
	if (mission->ufo) {
		/* Update UFO idx */
		for (; list; list = list->next) {
			removedMission = (mission_t *)list->data;
			if (removedMission->ufo && (removedMission->ufo > mission->ufo))
				removedMission->ufo--;
		}

		CP_UFORemoveFromGeoscape(mission);		/* for the notifications */
		UFO_RemoveFromGeoscape(mission->ufo);
	}

	/* Remove from ccs.battleParameters */
	if (mission == ccs.battleParameters.mission)
		ccs.battleParameters.mission = NULL;

	/* Notifications */
	CP_MissionRemoveFromGeoscape(mission);

	list = ccs.missions;
	for (; list; list = list->next) {
		removedMission = (mission_t *)list->data;
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
		removedMission = (mission_t *)list->data;
		Com_Printf("'%s', ", removedMission->id);
	}
	Com_Printf("\n");
#endif
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
 * @brief Choose UFO type for a given mission category.
 * @param[in] Pointer to the mission where the UFO will be added
 * @todo Parse those values from script files ?
 */
static int CP_MissionChooseUFO (const mission_t *mission)
{
	switch (mission->category) {
	case INTERESTCATEGORY_TERROR_ATTACK:
	case INTERESTCATEGORY_INTERCEPT:
	case INTERESTCATEGORY_BASE_ATTACK:
		return UFO_FIGHTER;
	case INTERESTCATEGORY_BUILDING:
	case INTERESTCATEGORY_SUPPLY:
		return UFO_SUPPLY;
	case INTERESTCATEGORY_XVI:
		return UFO_CORRUPTER;
	case INTERESTCATEGORY_HARVEST:
		return UFO_HARVESTER;
	case INTERESTCATEGORY_NONE:
	case INTERESTCATEGORY_MAX:
		Sys_Error("CP_MissionChooseUFO: Wrong mission category %i\n", mission->category);
		break;
	case INTERESTCATEGORY_RECON:
		break;
	}

	return UFO_SCOUT;
}

/**
 * @brief mission begins: UFO arrive on earth.
 * @note Stage 0 -- This function is common to several mission category
 * @return true if mission was created, false else.
 */
static qboolean CP_MissionCreate (mission_t *mission)
{
	int ufoType;

	CP_MissionDisableTimeLimit(mission);
	ufoType = CP_MissionChooseUFO(mission);
	mission->ufo = UFO_AddToGeoscape(ufoType, NULL, mission);
	if (!mission->ufo) {
		Com_Printf("CP_MissionCreate: Could not add UFO '%s', remove mission\n", UFO_TypeToShortName(ufoType));
		CP_MissionRemove(mission);
		return qfalse;
	}

	mission->stage = STAGE_COME_FROM_ORBIT;
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
	base_t *base;
	int i;

	for (i = 0, base = gd.bases; i < gd.numBases; i++, base++)
		if (MAP_GetDistance(pos, base->pos) < MIN_DIST_BASE) {
			return base;
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
	CL_ChangeIndividualInterest(-0.5f, INTERESTCATEGORY_RECON);
	if (ccs.XVISpreadActivated)
		CL_ChangeIndividualInterest(0.15f, INTERESTCATEGORY_BUILDING);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);
	CL_ChangeIndividualInterest(-0.05f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission is over and is a failure: change interest values.
 * @note Recon mission
 */
static void CP_ReconMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	if (ccs.XVISpreadActivated)
		CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);

	CP_MissionRemove(mission);
}

/**
 * @brief Recon mission ends: UFO leave earth.
 * @note Recon mission -- Stage 2
 */
static void CP_ReconMissionLeave (mission_t *mission)
{
	CP_MissionDisableTimeLimit(mission);
	assert(mission->ufo);
	UFO_SetRandomDest(mission->ufo);
	CP_MissionRemoveFromGeoscape(mission);
	/* Display UFO on geoscape if it is visible */
	mission->ufo->notOnGeoscape = qfalse;

	mission->stage = STAGE_RETURN_TO_ORBIT;
}

/**
 * @brief Choose between aerial and ground mission.
 * @note Recon mission -- Stage 1
 * @return qtrue if recon mission is aerial, qfalse if this is a ground mission
 * @sa CP_ReconMissionSelect
 * @todo implement me
 */
static qboolean CP_ReconMissionChoose (mission_t *mission)
{
	return (frand() > 0.5f);
}

/**
 * @brief Set aerial mission.
 * @note Recon mission -- Stage 1
 * @sa CP_ReconMissionSelect
 */
static void CP_ReconMissionAerial (mission_t *mission)
{
	const date_t reconDelay = {2, 0};		/* How long the UFO will fly on earth */

	mission->finalDate = Date_Add(ccs.date, Date_Random(reconDelay));
	mission->stage = STAGE_RECON_AIR;
}

/**
 * @brief Set ground mission, and go to ground mission pos.
 * @note Recon mission -- Stage 1
 * @sa CP_ReconMissionSelect
 */
static void CP_ReconMissionGroundGo (mission_t *mission)
{
	const nation_t *nation;

	assert(mission->ufo);

	CP_MissionRemoveFromGeoscape(mission);
	mission->ufo->notOnGeoscape = qfalse;

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
		Com_sprintf(mission->location, sizeof(mission->location), _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), _("No nation"));
	}

	UFO_SendToDestination(mission->ufo, mission->pos);

	mission->stage = STAGE_MISSION_GOTO;
}

/**
 * @brief Start ground mission.
 * @note Recon mission -- Stage 1
 */
static void CP_ReconMissionGround (mission_t *mission)
{
	const date_t missionDelay = {3, 0};

	assert(mission->ufo);

	mission->finalDate = Date_Add(ccs.date, Date_Random(missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission);

	mission->stage = STAGE_RECON_GROUND;
}

/**
 * @brief Choose if a new ground mission should be started.
 * @note Recon mission -- Stage 1
 * @note Already one ground mission has been made
 * @todo implement me (for now, there's only one ground mission possible)
 * @sa CP_ReconMissionSelect
 * @todo implement me
 */
static qboolean CP_ReconMissionNewGroundMission (mission_t *mission)
{
	return (frand() > 0.7f);
}

/**
 * @brief Set recon mission type (aerial or ground).
 * @note Recon mission -- Stage 1
 * @todo implement me
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
	CL_ChangeIndividualInterest(-0.05f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Terror attack mission is over and is a failure: change interest values.
 * @note Terror attack mission
 */
static void CP_TerrorMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_TERROR_ATTACK);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	if (ccs.XVISpreadActivated)
		CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);

	CP_MissionRemove(mission);
}

/**
 * @brief Start Terror attack mission.
 * @note Terror attack mission -- Stage 2
 */
static void CP_TerrorMissionStart (mission_t *mission)
{
	const date_t missionDelay = {3, 0};

	assert(mission->ufo);

	mission->finalDate = Date_Add(ccs.date, Date_Random(missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission);

	/* Notify the player */
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Alien activity has been detected in %s."), mission->location);
	MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);

	mission->stage = STAGE_TERROR_MISSION;
}

/**
 * @brief Set Terror attack mission, and go to Terror attack mission pos.
 * @note Terror attack mission -- Stage 1
 */
static void CP_TerrorMissionGo (mission_t *mission)
{
	const nation_t *nation;

	assert(mission->ufo);

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
			Com_Printf("CP_TerrorMissionGo: Error, could not set position.\n");
			CP_MissionRemove(mission);
			return;
		}
	} else {
		Com_Printf("CP_TerrorMissionGo: No map found, remove mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->mapDef->timesAlreadyUsed++;

	nation = MAP_GetNation(mission->pos);
	if (nation) {
		Com_sprintf(mission->location, sizeof(mission->location), _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), _("No nation"));
	}

	UFO_SendToDestination(mission->ufo, mission->pos);

	mission->stage = STAGE_MISSION_GOTO;
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
 * @todo implement me
 */
static void CP_BaseAttackMissionIsSuccess (mission_t *mission)
{
	CL_ChangeIndividualInterest(+0.2f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(-0.8f, INTERESTCATEGORY_BASE_ATTACK);
	CL_ChangeIndividualInterest(0.4f, INTERESTCATEGORY_XVI);

	CP_MissionRemove(mission);
}

/**
 * @brief Base attack mission is over and is a failure (from an alien point of view): change interest values.
 * @note Base attack mission
 * @todo implement me
 */
static void CP_BaseAttackMissionIsFailure (mission_t *mission)
{
	base_t *base;

	base = (base_t *)mission->data;

	if (base)
		B_BaseResetStatus(base);
	gd.mapAction = MA_NONE;

	CP_MissionRemove(mission);
}

/**
 * @brief Base attack mission ends: UFO leave earth.
 * @note Base attack mission -- Stage 3
 * @todo destroy base
 */
static void CP_BaseAttackMissionLeave (mission_t *mission)
{
	base_t *base;

	base = (base_t *)mission->data;
	assert(base);
	/* Base attack is over, alien won */
	CL_BaseRansacked(base);
	base->baseStatus = BASE_WORKING;
	gd.mapAction = MA_NONE;

	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	/* Display UFO on geoscape if it is visible */
	mission->ufo->notOnGeoscape = qfalse;

	mission->stage = STAGE_RETURN_TO_ORBIT;
}

/**
 * @brief Start Base Attack.
 * @note Base attack mission -- Stage 2
 * @todo Base attack should start right away
 * @todo Base attack can't be selected in map anymore: remove all base attack code from cl_map.c
 */
static void CP_BaseAttackStartMission (mission_t *mission)
{
	base_t *base;
	base = (base_t *)mission->data;

	assert(base);
	assert(mission->ufo);

	CP_MissionDisableTimeLimit(mission);

	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission);

	/* we always need at least one command centre in the base - because the
	 * phalanx soldiers have their starting positions here
	 * @note There should also always be an entrance - the aliens start there */
	if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, B_COMMAND)) {
		Com_Printf("B_BaseAttack: FIXME: This base (%s) can not be set under attack - because there are no Command Center in this base\n", base->name);
		CP_BaseAttackMissionLeave(mission);
		return;
	}

	base->baseStatus = BASE_UNDER_ATTACK;
	campaignStats.basesAttacked++;

#if 0
	/* @todo: implement onattack: add it to basemanagement.ufo and implement functions */
	if (b->onAttack)
		Cbuf_AddText(va("%s %i", b->onAttack, b->id));
#endif

	MAP_SelectMission(mission);
	selectedMission->active = qtrue;
	gd.mapAction = MA_BASEATTACK;
	Com_DPrintf(DEBUG_CLIENT, "Base attack: %s at %.0f:%.0f\n", selectedMission->id, selectedMission->pos[0], selectedMission->pos[1]);
	cls.missionaircraft = &base->aircraft[0]; /* @todo FIXME */
	assert(cls.missionaircraft);
	assert(cls.missionaircraft->homebase == base);

	mission->stage = STAGE_BASE_ATTACK;

	CL_GameTimeStop();
	MN_PushMenu("popup_base_attack");
}

/**
 * @brief Choose Base that will be attacked, and add it to mission description.
 * @note Base attack mission -- Stage 1
 * @return Pointer to the base, NULL if no base set
 */
static base_t* CP_BaseAttackChooseBase (const mission_t *mission)
{
	float randomNumber, sum = 0.0f;
	base_t *base;

	assert(mission);

	/* Choose randomly a base depending on alienIterest values for those bases */
	for (base = gd.bases; base < gd.bases + gd.numBases; base++) {
		if (!base->founded)
			continue;
		sum += base->alienInterest;
	}
	randomNumber = frand() * sum;
	for (base = gd.bases; base < gd.bases + gd.numBases; base++) {
		if (!base->founded)
			continue;
		randomNumber -= base->alienInterest;
		if (randomNumber < 0)
			break;
	}

	/* base is already under attack */
	if (base->baseStatus == BASE_UNDER_ATTACK)
		return NULL;
	/* HACK FIXME */
	if (!base->numAircraftInBase) {
		Com_Printf("CP_BaseAttackChooseBase: FIXME: This base (%s) can not be set under attack - because there are no crafts in this base\n", base->name);
		return NULL;
	}

	return base;
}

/**
 * @brief Set base attack mission, and go to base position.
 * @note Base attack mission -- Stage 1
 */
static void CP_BaseAttackGoToBase (mission_t *mission)
{
	base_t *base;

	assert(mission->ufo);

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

	Com_sprintf(mission->location, sizeof(mission->location), base->name);

	UFO_SendToDestination(mission->ufo, mission->pos);

	mission->stage = STAGE_MISSION_GOTO;
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
 * @todo implement me
 */
static void CP_BuildBaseMissionIsSuccess (mission_t *mission)
{
	alienBase_t *base;

	CL_ChangeIndividualInterest(+0.2f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(+0.4f, INTERESTCATEGORY_SUPPLY);
	CL_ChangeIndividualInterest(-0.8f, INTERESTCATEGORY_BUILDING);

	/* Spread XVI */
	base = (alienBase_t *) mission->data;
	assert(base);
	CP_SpreadXVIAtPos(base->pos);

	CP_MissionRemove(mission);
}

/**
 * @brief Build Base mission is over and is a failure (from an alien point of view): change interest values.
 * @note Build Base mission
 * @todo implement me
 */
static void CP_BuildBaseMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(+0.1f, INTERESTCATEGORY_BUILDING);

	CP_MissionRemove(mission);
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
	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	/* Display UFO on geoscape if it is visible */
	mission->ufo->notOnGeoscape = qfalse;

	mission->stage = STAGE_RETURN_TO_ORBIT;
}

/**
 * @brief UFO arrived on new base destination: build base.
 * @param[in|out] mission Pointer to the mission
 * @note Build Base mission -- Stage 2
 */
static void CP_BuildBaseSetUpBase (mission_t *mission)
{
	alienBase_t *base;
	const date_t buildingTime = {10, 0};	/**< Time needed to start a new base construction */

	mission->finalDate = Date_Add(ccs.date, Date_Random(buildingTime));

	base = AB_BuildBase(mission->pos);
	if (!base) {
		Com_Printf("CP_BuildBaseSetUpBase: could not create base\n");
		CP_MissionRemove(mission);
		return;
	}
	mission->data = (void *) base;

	/* ufo becomes invisible on geoscape */
	CP_UFORemoveFromGeoscape(mission);

	mission->stage = STAGE_BUILD_BASE;
}

/**
 * @brief Go to new base position.
 * @param[in|out] mission Pointer to the mission
 * @note Build Base mission -- Stage 1
 */
static void CP_BuildBaseGoToBase (mission_t *mission)
{
	AB_SetAlienBasePosition(mission->pos);

	UFO_SendToDestination(mission->ufo, mission->pos);

	mission->stage = STAGE_MISSION_GOTO;
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
		/* Go to new base position */
		CP_BuildBaseGoToBase(mission);
		break;
	case STAGE_MISSION_GOTO:
		/* just arrived on base location: build base */
		CP_BuildBaseSetUpBase(mission);
		break;
	case STAGE_BUILD_BASE:
		/* Leave earth */
		CP_BuildBaseMissionLeave(mission);
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
 * @todo implement me
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
 * @todo implement me
 */
static void CP_SupplyMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(+0.2f, INTERESTCATEGORY_SUPPLY);

	CP_MissionRemove(mission);
}

/**
 * @brief Supply mission ends: UFO leave earth.
 * @note Supply mission -- Stage 3
 */
static void CP_SupplyMissionLeave (mission_t *mission)
{
	CP_MissionDisableTimeLimit(mission);
	UFO_SetRandomDest(mission->ufo);
	/* Display UFO on geoscape if it is visible */
	mission->ufo->notOnGeoscape = qfalse;

	mission->stage = STAGE_RETURN_TO_ORBIT;
}

/**
 * @brief UFO arrived on new base destination: Supply base.
 * @param[in|out] mission Pointer to the mission
 * @note Supply mission -- Stage 2
 */
static void CP_SupplySetStayAtBase (mission_t *mission)
{
	const date_t supplyTime = {10, 0};	/**< Max time needed to supply base */

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->finalDate = Date_Add(ccs.date, Date_Random(supplyTime));

	AB_SupplyBase((alienBase_t *) mission->data, mission->ufo->visible);

	/* ufo becomes invisible on geoscape */
	CP_UFORemoveFromGeoscape(mission);

	mission->stage = STAGE_SUPPLY;
}

/**
 * @brief Go to base position.
 * @param[in|out] mission Pointer to the mission
 * @note Supply mission -- Stage 1
 */
static void CP_SupplyGoToBase (mission_t *mission)
{
	assert(mission->ufo);

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->data = (void *) AB_ChooseBaseToSupply(mission->pos);

	UFO_SendToDestination(mission->ufo, mission->pos);

	mission->stage = STAGE_MISSION_GOTO;
}

/**
 * @brief Supply mission begins: UFO arrive on earth.
 * @note Supply mission -- Stage 0
 */
static void CP_SupplyMissionCreate (mission_t *mission)
{
	int ufoType;

	/* Maybe base has been destroyed since mission creation ? */
	if (!AB_CheckSupplyMissionPossible()) {
		Com_DPrintf(DEBUG_CLIENT, "No base in game: removing supply mission.\n");
		CP_MissionRemove(mission);
		return;
	}

	CP_MissionDisableTimeLimit(mission);
	ufoType = CP_MissionChooseUFO(mission);
	mission->ufo = UFO_AddToGeoscape(ufoType, NULL, mission);
	if(!mission->ufo) {
		Com_Printf("CP_BuildBaseMissionCreate: Could not add UFO for base attack mission, remove mission\n");
		CP_MissionRemove(mission);
		return;
	}

	mission->stage = STAGE_COME_FROM_ORBIT;
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
	CL_ChangeIndividualInterest(-0.2f, INTERESTCATEGORY_XVI);

	CP_MissionRemove(mission);
}

/**
 * @brief XVI Spreading mission is over and is a failure: change interest values.
 * @note XVI Spreading mission
 */
static void CP_XVIMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_XVI);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BUILDING);

	CP_MissionRemove(mission);
}

/**
 * @brief Start XVI Spreading mission.
 * @note XVI Spreading mission -- Stage 2
 */
static void CP_XVIMissionStart (mission_t *mission)
{
	const date_t missionDelay = {3, 0};

	assert(mission->ufo);

	mission->finalDate = Date_Add(ccs.date, Date_Random(missionDelay));
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission);

	mission->stage = STAGE_SPREAD_XVI;
}

/**
 * @brief Determine what action should be performed when a XVI Spreading mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
static void CP_XVIMissionNextStage (mission_t *mission)
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
	CL_ChangeIndividualInterest(0.05f, INTERESTCATEGORY_BASE_ATTACK);
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_RECON);
	CL_ChangeIndividualInterest(-0.05f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Intercept mission is over and is a failure: change interest values.
 * @note Intercept mission
 */
static void CP_InterceptMissionIsFailure (mission_t *mission)
{
	CL_ChangeIndividualInterest(0.1f, INTERESTCATEGORY_INTERCEPT);

	CP_MissionRemove(mission);
}

/**
 * @brief Intercept mission ends: UFO leave earth.
 * @note Intercept mission -- Stage 2
 */
static void CP_InterceptMissionLeave (mission_t *mission)
{
	CP_MissionDisableTimeLimit(mission);
	assert(mission->ufo);
	UFO_SetRandomDest(mission->ufo);
	CP_MissionRemoveFromGeoscape(mission);
	/* Display UFO on geoscape if it is visible */
	mission->ufo->notOnGeoscape = qfalse;

	mission->stage = STAGE_RETURN_TO_ORBIT;
}

/**
 * @brief Set Intercept mission: UFO looks for new target.
 * @note Intercept mission -- Stage 1
 */
static void CP_InterceptMissionSet (mission_t *mission)
{
	const date_t reconDelay = {6, 0};		/* How long the UFO should stay on earth */

	mission->finalDate = Date_Add(ccs.date, Date_Random(reconDelay));
	mission->stage = STAGE_INTERCEPT;
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
	case STAGE_INTERCEPT:
		/* Leave earth */
		if (AIRFIGHT_ChooseWeapon(mission->ufo->weapons, mission->ufo->maxWeapons, mission->ufo->pos, mission->ufo->pos) !=
			AIRFIGHT_WEAPON_CAN_NEVER_SHOOT && mission->ufo->status == AIR_UFO) {
			/* UFO is fighting and has still ammo, wait a little bit before leaving */
			const date_t AdditionalDelay = {1, 0};	/* 1 day */
			mission->finalDate = Date_Add(ccs.date, AdditionalDelay);
		} else
			CP_InterceptMissionLeave(mission);
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

/*****	General Mission Code *****/

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
	default:
		Com_Printf("CP_MissionStageEnd: Unknown type of mission (%i), remove mission '%s'\n", mission->category, mission->id);
		CP_MissionRemove(mission);
	}
}

/**
 * @brief Mission is finished because Phalanx team ended it.
 * @param[in] mission Pointer to the mission that is ended.
 */
static inline void CP_MissionIsOver (mission_t *mission)
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
		CP_InterceptMissionIsFailure(mission);
		break;
	default:
		Com_Printf("CP_MissionIsOver: Unknown type of mission (%i), remove mission\n", mission->category);
		CP_MissionRemove(mission);
	}
}

/**
 * @brief Mission is finished because Phalanx team ended it.
 * @param[in] ufocraft Pointer to the UFO involved in this mission
 */
void CP_MissionIsOverByUfo (aircraft_t *ufocraft)
{
	assert(ufocraft->mission);
	CP_MissionIsOver(ufocraft->mission);
}

/**
 * @brief Stage of mission is finished.
 * @param[in] ufocraft Pointer to the UFO involved in this mission
 */
void CP_MissionStageEndByUfo (aircraft_t *ufocraft)
{
	assert(ufocraft->mission);
	CP_MissionStageEnd(ufocraft->mission);
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
		mission.startDate = Date_Add(ccs.date, Date_Random(nextSpawningDate));
	mission.finalDate = mission.startDate;
	CP_SetMissionName(&mission);

	/* Add mission to global array */
	LIST_Add(&ccs.missions, (byte*) &mission, sizeof(mission_t));
}

/**
 * @brief Spawn new missions.
 * @sa CL_CampaignRun
 * @note daily called
 */
static void CP_SpawnNewMissions (void)
{
	const int delayBetweenIncrease = 3;		/* Number of days between events are spawned */
	int i;

	ccs.lastMissionSpawnedDelay++;

	if (ccs.lastMissionSpawnedDelay > delayBetweenIncrease) {
		/* How many missions will be spawned until next cycle ? */
		const int newMissionNum = (int) (pow(ccs.overallInterest / 10.0f, 0.6));
		for (i = 0; i < newMissionNum; i++) {
			const int type = CP_SelectNewMissionType();
			CP_CreateNewMission(type, qfalse);
		}

		ccs.lastMissionSpawnedDelay -= delayBetweenIncrease;
	}
}

/**
 * @brief Start XVI spreading in campaign.
 * @note This is called when 'a new twist' technology is discovered
 */
static void CP_StartXVISpreading_f (void)
{
	int i, numAlienBases;

	ccs.XVISpreadActivated = qtrue;

	/* Spawn a few alien bases depending on difficulty level */
	if (difficulty->integer > 0)
		numAlienBases = 3;
	else if (difficulty->integer < 0)
		numAlienBases = 1;
	else
		numAlienBases = 2;

	for (i = 0; i < numAlienBases; i++)
		CP_CreateNewMission(INTERESTCATEGORY_BUILDING, qfalse);

	/* Is it still needed ? I don't even know what it's used for... Kracken */
	RS_ResearchFinish(RS_GetTechByID("rs_alien_xvi_event"));
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
		return "Building Base";
		break;
	case INTERESTCATEGORY_SUPPLY:
		return "Supply base";
		break;
	case INTERESTCATEGORY_XVI:
		return "XVI propagation";
		break;
	case INTERESTCATEGORY_INTERCEPT:
		return "Aircraft interception";
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
 */
static const char* CP_MissionStageToName (missionStage_t stage)
{
	switch (stage) {
	case STAGE_NOT_ACTIVE:
		return "Not active yet";
		break;
	case STAGE_COME_FROM_ORBIT:
		return "UFO coming from orbit";
		break;
	case STAGE_RECON_AIR:
		return "Aerial recon underway";
		break;
	case STAGE_MISSION_GOTO:
		return "Going to mission position";
		break;
	case STAGE_RECON_GROUND:
		return "Ground recon mission underway";
		break;
	case STAGE_TERROR_MISSION:
		return "Terror mission underway";
		break;
	case STAGE_BUILD_BASE:
		return "Building base";
		break;
	case STAGE_BASE_ATTACK:
		return "Attacking a base";
		break;
	case STAGE_SUBVERT_GOV:
		return "Subverting a government";
		break;
	case STAGE_SUPPLY:
		return "Supplying";
		break;
	case STAGE_SPREAD_XVI:
		return "Spreading XVI";
		break;
	case STAGE_INTERCEPT:
		return "Intercepting aircraft";
		break;
	case STAGE_RETURN_TO_ORBIT:
		return "Leaving earth";
		break;
	case STAGE_BASE_DISCOVERED:
		return "Base visible";
		break;
	case STAGE_OVER:
		return "Mission over";
		break;
	}
	assert(0);
	return "";
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
			{
				/* Start ground mission */
				if (!CP_MissionCreate(mission))
					return;
				if (type == 1)
					/* Aerial mission */
					CP_ReconMissionAerial(mission);
				else
					/* This is a ground mission */
					CP_ReconMissionGroundGo(mission);
			}
			break;
		default:
			Com_Printf("Type is not implemented for this category.\n");
		}
	}
}

/**
 * @brief List all current mission to console.
 * @note Use with debug_missionlist
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
		if (mission->onGeoscape)
			Com_Printf("...pos (%f, %f) -- on Geoscape\n", mission->pos[0], mission->pos[1]);
		else
			Com_Printf("...pos (%f, %f) -- not on Geoscape\n", mission->pos[0], mission->pos[1]);
		Com_Printf("...UFO %s\n", mission->ufo ? mission->ufo->id : "No UFO");
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
 * @brief Spawn a new crash site after a UFO has been destroyed.
 */
void CP_SpawnCrashSiteMission (aircraft_t *ufo)
{
	const nation_t *nation;
	mission_t *mission;
	const base_t *base;

	mission = ufo->mission;
	if (!mission)
		Sys_Error("CP_SpawnCrashSiteMission: No mission correspond to ufo '%s'", ufo->id);

	assert(mission);

	if (!CP_ChooseMap(mission, ufo->pos, qtrue)) {
		Com_Printf("CP_SpawnCrashSiteMission: No map found, remove mission.\n");
		CP_MissionRemove(mission);
		return;
	}
	/* Check if new mission is close from an existing base */
	base = CP_PositionCloseToBase(mission->pos);
	if (base) {
		const float dist = MAP_GetDistance(ufo->pos, base->pos);
		if (dist > UFO_EPSILON) {
			mission->pos[0] = base->pos[0] + (ufo->pos[0] - base->pos[0]) * MIN_DIST_BASE / dist;
			mission->pos[1] = base->pos[1] + (ufo->pos[1] - base->pos[1]) * MIN_DIST_BASE / dist;
		} else {
			mission->pos[0] = base->pos[0];
			mission->pos[1] = base->pos[1] + MIN_DIST_BASE;
		}
	} else
		Vector2Copy(ufo->pos, mission->pos);

	nation = MAP_GetNation(mission->pos);
	if (nation) {
		Com_sprintf(mission->location, sizeof(mission->location), _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), _("No nation"));
	}

	CP_MissionDisableTimeLimit(mission);
	/* ufo becomes invisible on geoscape, but don't remove it from ufo global array (may reappear)*/
	CP_UFORemoveFromGeoscape(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission);
}

/**
 * @brief Spawn a new alien base mission after it has been discovered.
 */
void CP_SpawnAlienBaseMission (alienBase_t *alienBase)
{
	const nation_t *nation;
	mission_t *mission;

	CP_CreateNewMission(INTERESTCATEGORY_BUILDING, qtrue);
	mission = CP_GetLastMissionAdded();
	if (!mission) {
		Com_Printf("CP_SpawnAlienBaseMission: Could not add mission, abort\n");
		return;
	}

	mission->data = (void *) alienBase;

	mission->mapDef = Com_GetMapDefinitionByID("alienbase");
	if (!mission->mapDef)
		Sys_Error("Could not find mapdef alienbase");

	Vector2Copy(alienBase->pos, mission->pos);

	nation = MAP_GetNation(mission->pos);
	if (nation) {
		Com_sprintf(mission->location, sizeof(mission->location), _(nation->name));
	} else {
		Com_sprintf(mission->location, sizeof(mission->location), _("Alien base"));
	}

	/* Alien base stay until it's destroyed */
	CP_MissionDisableTimeLimit(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission);

	mission->stage = STAGE_BASE_DISCOVERED;
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
	case STAGE_BUILD_BASE:
	case STAGE_BASE_ATTACK:
	case STAGE_SUBVERT_GOV:
	case STAGE_SUPPLY:
	case STAGE_SPREAD_XVI:
		return qfalse;
	default:
		return qtrue;
	}
}

/**
 * @brief Returns the alien race for a mission (depends on the interest values)
 * @return Alien team id string (team_aliens.ufo)
 */
static const char* CP_GetAlienByInterest (const mission_t *mission)
{
	int i, max;
	interestCategory_t maxInterest;

	max = maxInterest = 0;

	for (i = 0; i < INTERESTCATEGORY_MAX; i++) {
		if (ccs.interest[i] > max) {
			maxInterest = i;
			max = ccs.interest[i];
		}
	}

	/* FIXME: Set the correct alien teams here
	 * take the values from the script files */
	switch (maxInterest) {
	case INTERESTCATEGORY_NONE:
		return "ortnok";
	case INTERESTCATEGORY_RECON:
		return "ortnok";
	case INTERESTCATEGORY_TERROR_ATTACK:
		return "ortnok";
	case INTERESTCATEGORY_BASE_ATTACK:
		return "ortnok";
	case INTERESTCATEGORY_BUILDING:
		return "ortnok";
	case INTERESTCATEGORY_SUPPLY:
		return "ortnok";
	case INTERESTCATEGORY_XVI:
		return "ortnok";
	case INTERESTCATEGORY_INTERCEPT:
		return "ortnok";
	case INTERESTCATEGORY_HARVEST:
		return "ortnok";
	case INTERESTCATEGORY_MAX:
		break;
	}
	assert(0);
	return "";
}

/**
 * @brief Set alien equipment for a mission (depends on the interest values)
 * @note This function is used to know which equipment pack described in equipment_missions.ufo should be used
 */
static void CP_SetAlienEquipmentByInterest (const mission_t *mission)
{
	int stage = 1 + ccs.overallInterest / (330 - difficulty->integer * 20);

	if (stage > 4)
		stage = 4;
	if (stage < 1)
		stage = 1;

	Com_sprintf(ccs.battleParameters.alienEquipment, sizeof(ccs.battleParameters.alienEquipment), "stage%i_%s", stage, "soldiers");
}

/**
 * @brief Create alien team.
 * @param[in] mission Pointer to the mission that generates the battle
 * @todo implement me
 */
static void CP_CreateAlienTeam (mission_t *mission)
{
	const char *alienType;
	int numAliens;

	assert(mission->pos);

	numAliens = 4 + rand() % (2 + ccs.overallInterest / 100);
	if (numAliens > mission->mapDef->maxAliens)
		numAliens = mission->mapDef->maxAliens;
	ccs.battleParameters.aliens = numAliens;

	alienType = CP_GetAlienByInterest(mission);

	/* @todo alien team should vary depending on interest values */
	ccs.battleParameters.numAlienTeams = 0;
	ccs.battleParameters.alienTeams[0] = Com_GetTeamDefinitionByID(alienType);
	if (ccs.battleParameters.alienTeams[0])
		ccs.battleParameters.numAlienTeams++;
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

	/* @todo I've no idea what civilian number should be, please set it to desired value -- Kracken */
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

	ccs.battleParameters.mission = mission;
	color = MAP_GetColor(mission->pos, MAPTYPE_TERRAIN);
	zoneType = MAP_GetTerrainType(color);
	ccs.battleParameters.zoneType = zoneType; /* store to terrain type for texture replacement */
	/* Is there a UFO to recover ? */
	if (selectedMission->ufo) {
		if (CP_UFOIsCrashed(mission)) {
			Com_sprintf(mission->onwin, sizeof(mission->onwin), "cp_ufocrashed %i;", mission->ufo->ufotype);
			/* Set random map UFO if this is a random map */
			if (mission->mapDef->map[0] == '+')
				Cvar_Set("rm_ufo", va("+%s", UFO_TypeToShortName(selectedMission->ufo->ufotype)));
#if 0
/* @todo replace previous line by this one when random map assemblies will have crashed map tiles */
				Cvar_Set("rm_ufo", va("+%s", UFO_CrashedTypeToShortName(selectedMission->ufo->ufotype)));
#endif
		} else {
			Com_sprintf(mission->onwin, sizeof(mission->onwin), "cp_uforecovery %i;", mission->ufo->ufotype);
			/* Set random map UFO if this is a random map */
			if (mission->mapDef->map[0] == '+')
				Cvar_Set("rm_ufo", va("+%s", UFO_TypeToShortName(selectedMission->ufo->ufotype)));
		}
	}
	/* @todo change dropship to any possible aircraft when random assembly tiles will be created */
	/* Set random map aircraft if this is a random map */
	if (mission->mapDef->map[0] == '+')
		Cvar_Set("rm_drop", "+drop_firebird");
}

/**
 * @brief Get mission model string
 * @param[in] mission Pointer to the mission drawn on geoscape
 * @sa MAP_DrawMapMarkers
 */
const char* MAP_GetMissionModel (const mission_t *mission)
{
	/* Mission shouldn't be drawn on geoscape if mapDef is not defined */
	assert(mission->mapDef);

	if (mission->ufo && CP_UFOIsCrashed(mission))
		/* @todo Should be a special crashed UFO mission model */
		return "mission";

	if (mission->mapDef->storyRelated) {
#if 0
/* @todo alienbase md2 is broken, fix it before decommenting thoses lines */
		if (mission->category == INTERESTCATEGORY_BUILDING)
			return "alienbase";
		else
#endif
			/* @todo Should be a special story related mission model */
			return "mission";
	}

	switch (mission->category) {
	/* @todo each category should have a its own model */
	case INTERESTCATEGORY_RECON:
	case INTERESTCATEGORY_XVI:
	case INTERESTCATEGORY_HARVEST:
	case INTERESTCATEGORY_TERROR_ATTACK:
		return "mission";
	case INTERESTCATEGORY_BUILDING:		/* Should not be reached: alien base mission is storyrelated */
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
	return RS_IsResearched_ptr(rs_alien_xvi) ? rs_alien_xvi : NULL;
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo otehr stuff to back up?
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

		for (i = MONTHS_PER_YEAR-1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			memcpy(&nation->stats[i], &nation->stats[i-1], sizeof(nationInfo_t));
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

	if (!gd.numBases && ccs.credits < BASE_COSTS - curCampaign->negativeCreditsUntilLost) {
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
				nation_t *nation = &gd.nations[j];
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

/**
 * @brief Return a nation-pointer by the nations id (nation_t->id) text.
 * @param[in] nationID nation id as defined in (nation_t->id)
 * @return nation_t pointer or NULL if nothing found (=error).
 */
nation_t *CL_GetNationByID (const char *nationID)
{
	int i;
	nation_t *nation;

	for (i = 0; i < gd.numNations; i++) {
		nation = &gd.nations[i];
		if (!Q_strcmp(nation->id, nationID))
			return nation;
	}

	/* No matching nation found - ERROR */
	return NULL;
}

/* Initial fraction of the population in the country where a mission has been lost / won */
#define XVI_LOST_START_PERCENTAGE	0.20f
#define XVI_WON_START_PERCENTAGE	0.05f

/**
 * @brief Updates each nation's happiness and mission win/loss stats.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 */
static void CL_HandleNationData (qboolean lost, int civiliansSurvived, int civiliansKilled, int aliensSurvived, int aliensKilled, mission_t * mis)
{
	int i, is_on_Earth = 0;
	int civilianSum = civiliansKilled + civiliansSurvived;
	float civilianRatio = civilianSum ? (float)civiliansSurvived / (float)civilianSum : 0.;
	int alienSum = aliensKilled + aliensSurvived;
	float alienRatio = alienSum ? (float)aliensKilled / (float)alienSum : 0.;
	float performance = civilianRatio * 0.5 + alienRatio * 0.5;
	float delta_happiness;

	if (lost) {
		campaignStats.missionsLost++;
		ccs.civiliansKilled += civiliansKilled;
	} else
		campaignStats.missionsWon++;

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		float alienHostile = 1.0f - nation->stats[0].alienFriendly;
		delta_happiness = 0.0f;

		if (lost) {
			if (nation == ccs.battleParameters.nation) {
				/* Strong negative reaction (happiness_factor must be < 1) */
				delta_happiness = - (1.0f - performance) * nation->stats[0].alienFriendly * nation->stats[0].happiness;
				is_on_Earth++;
			} else {
				/* Minor negative reaction (10 times lower than if the failed mission were in the nation) */
				delta_happiness = - 0.1f * (1.0f - performance) * nation->stats[0].alienFriendly * nation->stats[0].happiness;
			}
		} else {
			if (nation == ccs.battleParameters.nation) {
				/* Strong positive reaction (happiness_factor must be > 1) */
				delta_happiness = performance * alienHostile * (1.0f - nation->stats[0].happiness);
				is_on_Earth++;
			} else {
				/* Minor positive reaction (10 times lower than if the mission were in the nation) */
				delta_happiness = 0.1f * performance * alienHostile * (1.0f - nation->stats[0].happiness);
			}
			/* the happiness you can gain depends on the difficulty of the campaign */
			delta_happiness *= (0.2f + pow(4.0f - difficulty->integer, 2) / 32.0f);
		}

		/* update happiness */
		nation->stats[0].happiness += delta_happiness;
		/* Nation happiness is between 0 and 1 */
		if (nation->stats[0].happiness > 1.0f)
			nation->stats[0].happiness = 1.0f;
		else if (nation->stats[0].happiness < 0.0f)
			nation->stats[0].happiness = 0.0f;
	}
	if (!is_on_Earth)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Warning, mission '%s' located in an unknown country '%s'.\n", mis->id, ccs.battleParameters.nation ? ccs.battleParameters.nation->id : "no nation");
	else if (is_on_Earth > 1)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Error, mission '%s' located in many countries '%s'.\n", mis->id, ccs.battleParameters.nation->id);
}

/**
 * @brief Deletes employees from a base along with all base equipment.
 * Called when invading forces overrun a base after a base-attack mission
 * @param[in] base base_t base to be ransacked
 */
void CL_BaseRansacked (base_t *base)
{
	int item, ac;

	if (!base)
		return;

	/* Delete all employees from the base & the global list. */
	E_DeleteAllEmployees(base);

	/* Destroy all items in storage */
	for (item = 0; item < csi.numODs; item++)
		/* reset storage and capacity */
		B_UpdateStorageAndCapacity(base, item, 0, qtrue, qfalse);

	/* Remove all aircraft from the base. */
	for (ac = base->numAircraftInBase - 1; ac >= 0; ac--)
		AIR_DeleteAircraft(&base->aircraft[ac]);

	/* @todo: Maybe reset research in progress. ... needs playbalance
	 * need another value in technology_t to remember researched
	 * time from other bases?
	 * @todo: Destroy (or better: just damage) some random buildings. */

	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Your base: %s has been ransacked! All employees killed and all equipment destroyed."), base->name);
	MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_GameTimeStop();
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

	/* Check UFOs events. */
	UFO_CampaignCheckEvents(qtrue);

	/* humans start to attacking player */
	if (ccs.overallInterest > 700) {
		ccs.humansAttackActivated = qtrue;
		/* Mark prequesite of "rs_enemy_on_earth" as met. */
		RS_ResearchFinish(RS_GetTechByID("rs_enemy_on_earth_event"));
	}
}

/**
 * @brief Converts a number of second into a char to display.
 * @param[in] second Number of second.
 */
const char* CL_SecondConvert (int second)
{
	int hour, min;

	hour = second / 3600;
	min = (second - hour * 3600) / 60;
	return va("%i:%02i", hour, min);
}

static const int monthLength[MONTHS_PER_YEAR] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Converts a number of days into the number of the current month and the current day in this month.
 * @note The seconds from "date" are ignored here.
 * @note The function always starts calculation from Jan. and also catches new years.
 * @param[in] date Contains the number of days to be converted.
 * @param[out] month The month.
 * @param[out] day The day in the month above.
 */
void CL_DateConvert (const date_t * date, int *day, int *month)
{
	int i, d;

	/* Get the day in the year. Other years are ignored. */
	d = date->day % DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	for (i = 0; d >= monthLength[i]; i++)
		d -= monthLength[i];

	/* Prepare return values. */
	*day = d + 1;
	*month = i;
}

/**
 * @brief Returns the monatname to the given month index
 * @param[in] month The month index - starts at 0 - ends at 11
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

/**
 * @brief Translates the nation happiness float value to a string
 * @param[in] nation
 * @return Translated happiness string
 */
static const char* CL_GetNationHappinessString (const nation_t* nation)
{
	if (nation->stats[0].happiness < 0.015)
		return _("Giving up");
	else if (nation->stats[0].happiness < 0.025)
		return _("Furious");
	else if (nation->stats[0].happiness < 0.04)
		return _("Angry");
	else if (nation->stats[0].happiness < 0.06)
		return _("Mad");
	else if (nation->stats[0].happiness < 0.10)
		return _("Upset");
	else if (nation->stats[0].happiness < 0.20)
		return _("Tolerant");
	else if (nation->stats[0].happiness < 0.30)
		return _("Neutral");
	else if (nation->stats[0].happiness < 0.50)
		return _("Content");
	else if (nation->stats[0].happiness < 0.70)
		return _("Pleased");
	else if (nation->stats[0].happiness < 1.0)
		return _("Happy");
	else
		return _("Exuberant");
}

/**
 * @brief Get the actual funding of a nation
 * @param[in] nation Pointer to the nation
 * @param[in] month idx of the month -- 0 for current month (sa nation_t)
 * @return actual funding of a nation
 * @sa CL_NationsMaxFunding
 */
static int CL_GetNationFunding (const nation_t* const nation, int month)
{
	return nation->maxFunding * nation->stats[month].happiness;
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
	int funding;
	int cost;
	nation_t *nation;
	int initial_credits = ccs.credits;
	int new_scientists, new_medics, new_soldiers, new_workers;

	for (i = 0; i < gd.numNations; i++) {
		nation = &gd.nations[i];
		funding = CL_GetNationFunding(nation, 0);
		CL_UpdateCredits(ccs.credits + funding);

		new_scientists = new_medics = new_soldiers = new_workers = 0;

		for (j = 0; 0.25 + j < (float) nation->maxScientists * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST, nation, NULL);
			new_scientists++;
		}

		for (j = 0; 0.25 + j * 3 < (float) nation->maxScientists * nation->stats[0].happiness; j++) {
			/* Create a medic. */
			E_CreateEmployee(EMPL_MEDIC, nation, NULL);
			new_medics++;
		}

		for (j = 0; 0.25 + j < (float) nation->maxSoldiers * nation->stats[0].happiness * nation->stats[0].happiness * nation->stats[0].happiness; j++) {
			/* Create a soldier. */
			E_CreateEmployee(EMPL_SOLDIER, nation, NULL);
			new_soldiers++;
		}

		for (j = 0; 0.25 + j * 2 < (float) nation->maxSoldiers * nation->stats[0].happiness; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER, nation, NULL);
			new_workers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
					funding, ngettext("credit", "credits", funding),
					new_scientists, ngettext("scientist", "scientists", new_scientists),
					new_medics, ngettext("medic", "medics", new_medics),
					new_soldiers, ngettext("soldier", "soldiers", new_soldiers),
					new_workers, ngettext("worker", "workers", new_workers),
					_(nation->name), CL_GetNationHappinessString(nation));
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
	for (i = 0; i < gd.numEmployees[EMPL_MEDIC]; i++) {
		if (gd.employees[EMPL_MEDIC][i].hired)
			cost += SALARY_MEDIC_BASE + gd.employees[EMPL_MEDIC][i].chr.score.rank * SALARY_MEDIC_RANKBONUS;
	}

	Com_sprintf(message, sizeof(message), _("Paid %i credits to medics"), cost);
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
	for (i = 0; i < gd.numBases; i++) {
		for (j = 0; j < gd.bases[i].numAircraftInBase; j++) {
			cost += gd.bases[i].aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		CL_UpdateCredits(ccs.credits - cost);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
	}

	for (i = 0; i < gd.numBases; i++) {
		cost = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			cost += gd.buildings[i][j].varCosts;
		}

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of base %s"), cost, gd.bases[i].name);
		MN_AddNewMessage(_("Notice"), message, qfalse, MSG_STANDARD, NULL);
		CL_UpdateCredits(ccs.credits - cost);
	}

	cost = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_MEDIC] * SALARY_ADMIN_MEDIC + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
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

	/* find the relevant market */
	for (i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++)
		if (!Q_strncmp(curCampaign->market, ed->name, MAX_VAR)) {
			curCampaign->marketDef = ed;
			break;
		}

	if (!curCampaign->marketDef)
		Sys_Error("CL_CampaignInitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->market);

	/* the savegame loading process will get the following values from savefile */
	if (load)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if (ccs.eMarket.ask[i] == 0) {
			ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i]*BID_FACTOR);
		}

		if (!ed->num[i])
			continue;
		if (!RS_ItemIsResearched(csi.ods[i].id))
			Com_Printf("CL_CampaignInitMarket: Could not add item %s to the market - not marked as researched\n", csi.ods[i].id);
		else
			/* the other relevant values were already set in CL_CampaignInitMarket */
			ccs.eMarket.num[i] = ed->num[i];
	}
}

/**
 * @brief simulates one hour of supply and demand on the market (adds items and sets prices)
 * @sa CL_CampaignRun
 * @sa CL_GameNew_f
 */
static void CL_CampaignRunMarket (void)
{
	int i, market_action;
	double research_factor, price_factor, curr_supp_diff;
	/* supply and demand */
	const double mrs1 = 0.1, mpr1 = 400, mrg1 = 0.002, mrg2 = 0.03;

	assert(curCampaign->marketDef);

	/* @todo: Find out why there is a 2 days discrepancy in reasearched_date*/
	for (i = 0; i < csi.numODs; i++) {
		if (RS_ItemIsResearched(csi.ods[i].id)) {
			/* supply balance */
			technology_t *tech = RS_GetTechByProvided(csi.ods[i].id);
			int reasearched_date;
			if (!tech)
				Sys_Error("No tech that provides '%s'\n", csi.ods[i].id);
			reasearched_date = tech->researchedDateDay + tech->researchedDateMonth * 30 +  tech->researchedDateYear * DAYS_PER_YEAR - 2;
			if (reasearched_date <= curCampaign->date.sec / 86400 + curCampaign->date.day)
				reasearched_date -= 100;
			research_factor = mrs1 * sqrt(ccs.date.day - reasearched_date);
			price_factor = mpr1 / sqrt(csi.ods[i].price+1);
			curr_supp_diff = floor(research_factor*price_factor - ccs.eMarket.num[i]);
			if (curr_supp_diff != 0)
				ccs.eMarket.cumm_supp_diff[i] += curr_supp_diff;
			else
				ccs.eMarket.cumm_supp_diff[i] *= 0.9;
			market_action = floor(mrg1 * ccs.eMarket.cumm_supp_diff[i] + mrg2 * curr_supp_diff);
			ccs.eMarket.num[i] += market_action;
			if (ccs.eMarket.num[i] < 0)
				ccs.eMarket.num[i] = 0;

			/* set item price based on supply imbalance */
			if (research_factor*price_factor >= 1)
				ccs.eMarket.ask[i] = floor(csi.ods[i].price * (1 - (1 - BID_FACTOR) / 2 * (1 / (1 + exp(curr_supp_diff / (research_factor * price_factor))) * 2 - 1)) );
			else
				ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i] * BID_FACTOR);
		}
	}
}

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
	/* advance time */
	ccs.timer += cls.frametime * gd.gameTimeScale;
	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int dt, day, month, currenthour;

		dt = (int)floor(ccs.timer);
		currenthour = (int)floor(ccs.date.sec / 3600);
		ccs.date.sec += dt;
		ccs.timer -= dt;

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(ccs.date.sec / 3600)) {
			currenthour++;
			CL_CheckResearchStatus();
			PR_ProductionRun();
			CL_CampaignRunMarket();
			UFO_Recovery();
			AII_UpdateInstallationDelay();
			TR_TransferCheck();
			CP_IncreaseAlienInterest();
		}

		/* daily events */
		while (ccs.date.sec > 3600 * 24) {
			ccs.date.sec -= 3600 * 24;
			ccs.date.day++;
			/* every day */
			B_UpdateBaseData();
			HOS_HospitalRun();
			BDEF_ReloadBattery();
			CP_SpawnNewMissions();
			CP_SpreadXVI();
			AB_BaseSearchedByNations();
		}

		/* check for campaign events */
		CL_CampaignRunAircraft(dt);
		UFO_CampaignRunUFOs(dt);
		AIRFIGHT_CampaignRunBaseDefense(dt);
		CP_CheckEvents();
		CP_CheckLostCondition(qtrue, NULL, 0);
		AIRFIGHT_CampaignRunProjectiles(dt);

		/* set time cvars */
		CL_DateConvert(&ccs.date, &day, &month);
		/* every first day of a month */
		if (day == 1 && gd.fund != qfalse && gd.numBases) {
			CL_BackupMonthlyData();	/* Back up monthly data. */
			CL_HandleBudget();
			gd.fund = qfalse;
		} else if (day > 1)
			gd.fund = qtrue;

		UP_GetUnreadMails();
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%i %s %02i"), ccs.date.day / DAYS_PER_YEAR, CL_DateGetMonthName(month), day);
		Cvar_Set("mn_mapdate", mn.messageBuffer);
		Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("%02i:%02i"), ccs.date.sec / 3600, ((ccs.date.sec % 3600) / 60));
		Cvar_Set("mn_maptime", mn.messageBuffer);
	}
}


/* =========================================================== */

typedef struct gameLapse_s {
	const char *name;
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 7

/** @brief The possible geoscape time intervalls */
static const gameLapse_t lapse[NUM_TIMELAPSE] = {
	{"stopped", 0},
	{"5 sec", 5},
	{"5 mins", 5 * 60},
	{"1 hour", 60 * 60},
	{"12 hour", 12 * 3600},
	{"1 day", 24 * 3600},
	{"5 days", 5 * 24 * 3600}
};

static int gameLapse;

/**
 * @brief Stop game time speed
 */
void CL_GameTimeStop (void)
{
	const menu_t *menu = MN_GetActiveMenu();

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && !Q_strcmp(menu->name, "map")) {
		gameLapse = 0;
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
	}
}


/**
 * @brief Decrease game time speed
 *
 * Decrease game time speed - only works when there is already a base available
 */
void CL_GameTimeSlow (void)
{
	const menu_t *menu = MN_GetActiveMenu();

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == 0)
		return;

	assert(gameLapse >= 0);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && !Q_strcmp(menu->name, "map")) {
		gameLapse--;
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
	}
}

/**
 * @brief Increase game time speed
 *
 * Increase game time speed - only works when there is already a base available
 */
void CL_GameTimeFast (void)
{
	const menu_t *menu = MN_GetActiveMenu();

	/* check the stats value - already build bases might have been destroyed
	 * so the gd.numBases values is pointless here */
	if (!campaignStats.basesBuild)
		return;

	if (gameLapse == NUM_TIMELAPSE - 1)
		return;

	assert(gameLapse < NUM_TIMELAPSE);

	/* don't allow time scale in tactical mode - only on the geoscape */
	if (menu && !Q_strcmp(menu->name, "map")) {
		gameLapse++;
		Cvar_Set("mn_timelapse", _(lapse[gameLapse].name));
		gd.gameTimeScale = lapse[gameLapse].scale;
	}
}

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

static void CP_NationStatsClick_f (void)
{
	int num;

	if (!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	/* Which entry in the list? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= gd.numNations)
		return;

	MN_PushMenu("nations");
	Cbuf_AddText(va("nation_select %i;", num));
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
	int i = 0, j, costs = 0, sum = 0;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	mn.menuText[TEXT_STATS_1] = pos;
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), campaignStats.missionsWon, campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_2] = pos;
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
		campaignStats.basesBuild, gd.numBases, campaignStats.basesAttacked),

	/* nations */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_3] = pos;
	for (i = 0; i < gd.numNations; i++) {
		Q_strcat(pos, va(_("%s\t%s\n"), _(gd.nations[i].name), CL_GetNationHappinessString(&gd.nations[i])), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
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
	for (i = 0; i < gd.numEmployees[EMPL_MEDIC]; i++) {
		if (gd.employees[EMPL_MEDIC][i].hired) {
			costs += SALARY_MEDIC_BASE + gd.employees[EMPL_MEDIC][i].chr.score.rank * SALARY_MEDIC_RANKBONUS;
			hired[EMPL_MEDIC]++;
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
	mn.menuText[TEXT_STATS_4] = pos;
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	mn.menuText[TEXT_STATS_5] = pos;
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	for (i = 0; i < gd.numBases; i++) {
		for (j = 0; j < gd.bases[i].numAircraftInBase; j++) {
			costs += gd.bases[i].aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	for (i = 0; i < gd.numBases; i++) {
		costs = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			costs += gd.buildings[i][j].varCosts;
		}
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), gd.bases[i].name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_MEDIC] * SALARY_ADMIN_MEDIC + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		float interest = ccs.credits * SALARY_DEBT_INTEREST;

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

static screenPoint_t fundingPts[MAX_NATIONS][MONTHS_PER_YEAR]; /* Space for month-lines with 12 points for each nation. */
static int usedFundPtslist = 0;
static screenPoint_t colorLinePts[MAX_NATIONS][2]; /* Space for 1 line (2 points) for each nation. */
static int usedColPtslists = 0;

static screenPoint_t coordAxesPts[3];	/* Space for 2 lines (3 points) */

static const vec4_t graphColors[MAX_NATIONS] = {
	{1.0, 0.5, 0.5, 1.0},
	{0.5, 1.0, 0.5, 1.0},
	{0.5, 0.5, 1.0, 1.0},
	{1.0, 0.0, 0.0, 1.0},
	{0.0, 1.0, 0.0, 1.0},
	{0.0, 0.0, 1.0, 1.0},
	{1.0, 1.0, 0.0, 1.0},
	{0.0, 1.0, 1.0, 1.0}
};
static const vec4_t graphColorSelected = {1, 1, 1, 1};

/**
 * @brief Search the maximum (current) funding from all the nations (in all logged months).
 * @note nation->maxFunding is _not_ the real funding value.
 * @return The maximum funding value.
 * @todo Extend to other values?
 * @sa CL_GetNationFunding
 */
static int CL_NationsMaxFunding (void)
{
	nation_t *nation;
	int m, n;
	int funding;
	int max = 0;

	for (n = 0; n < gd.numNations; n++) {
		nation = &gd.nations[n];
		for (m = 0; m < MONTHS_PER_YEAR; m++) {
			if (nation->stats[m].inuse) {
				/** nation->stats[m].happiness = sqrt((float)m / 12.0);  @todo  DEBUG */
				funding = CL_GetNationFunding(nation, m);
				if (max < funding)
					max = funding;
			} else {
				/* Abort this months-loops */
				break;
			}
		}
	}
	return max;
}

static int selectedNation = 0;

/**
 * @brief Draws a graph for the funding values over time.
 * @param[in] nation The nation to draw the graph for.
 * @param[in] node A pointer to a "linestrip" node that we want to draw in.
 * @param[in] maxFunding The upper limit of the graph - all values willb e scaled to this one.
 * @param[in] color If this is -1 draw the line for the current selected nation
 * @todo Somehow the display of the months isnt really correct right now (straight line :-/)
 */
static void CL_NationDrawStats (nation_t *nation, menuNode_t *node, int maxFunding, int color)
{
	int width, height, x, y, dx;
	int m;

	int minFunding = 0;
	int funding;
	int ptsNumber = 0;

	if (!nation || !node)
		return;

	width	= (int)node->size[0];
	height	= (int)node->size[1];
	x = node->pos[0];
	y = node->pos[1];
	dx = (int)(width / MONTHS_PER_YEAR);

	if (minFunding == maxFunding) {
		Com_Printf("CL_NationDrawStats: Given maxFunding value is the same as minFunding (min=%i, max=%i) - abort.\n", minFunding, maxFunding);
		return;
	}

	/* Generate pointlist. */
	/** @todo Sort this in reverse? -> Having current month on the right side? */
	for (m = 0; m < MONTHS_PER_YEAR; m++) {
		if (nation->stats[m].inuse) {
			funding = CL_GetNationFunding(nation, m);
			fundingPts[usedFundPtslist][m].x = x + (m * dx);
			fundingPts[usedFundPtslist][m].y = y - height * (funding - minFunding) / (maxFunding - minFunding);
			ptsNumber++;
		} else {
			break;
		}
	}

	/* Guarantee displayable data even for only one month */
	if (ptsNumber == 1) {
		/* Set the second point haft the distance to the next month to the right - small horiz line. */
		fundingPts[usedFundPtslist][1].x = fundingPts[usedFundPtslist][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
		fundingPts[usedFundPtslist][1].y = fundingPts[usedFundPtslist][0].y;
		ptsNumber++;
	}

	/* Break if we reached the max strip number. */
	if (node->linestrips.numStrips >= MAX_LINESTRIPS - 1)
		return;

	/* Link graph to node */
	node->linestrips.pointList[node->linestrips.numStrips] = (int*)fundingPts[usedFundPtslist];
	node->linestrips.numPoints[node->linestrips.numStrips] = ptsNumber;
	if (color < 0) {
		Cvar_Set("mn_nat_symbol", va("nations/%s", gd.nations[selectedNation].id));
		Vector4Copy(graphColorSelected, node->linestrips.color[node->linestrips.numStrips]);
	} else {
		Vector4Copy(graphColors[color], node->linestrips.color[node->linestrips.numStrips]);
	}
	node->linestrips.numStrips++;

	usedFundPtslist++;
}

/**
 * @brief Shows the current nation list + statistics.
 * @note See menu_stats.ufo
 */
static void CL_NationStatsUpdate_f (void)
{
	int i;
	int funding, maxFunding;
	menuNode_t *colorNode;
	menuNode_t *graphNode;
	const vec4_t colorAxes = {1, 1, 1, 0.5};
	int dy = 10;

	usedColPtslists = 0;

	colorNode = MN_GetNodeFromCurrentMenu("nation_graph_colors");
	if (colorNode) {
		dy = (int)(colorNode->size[1] / MAX_NATIONS);
		colorNode->linestrips.numStrips = 0;
	}

	for (i = 0; i < gd.numNations; i++) {
		funding = CL_GetNationFunding(&(gd.nations[i]), 0);

		if (selectedNation == i) {
			Cbuf_AddText(va("nation_marksel%i;",i));
		} else {
			Cbuf_AddText(va("nation_markdesel%i;",i));
		}
		Cvar_Set(va("mn_nat_name%i",i), _(gd.nations[i].name));
		Cvar_Set(va("mn_nat_fund%i",i), va("%i", funding));

		if (colorNode) {
			colorLinePts[usedColPtslists][0].x = colorNode->pos[0];
			colorLinePts[usedColPtslists][0].y = colorNode->pos[1] - (int)colorNode->size[1] + dy * i;
			colorLinePts[usedColPtslists][1].x = colorNode->pos[0] + (int)colorNode->size[0];
			colorLinePts[usedColPtslists][1].y = colorLinePts[usedColPtslists][0].y;

			colorNode->linestrips.pointList[colorNode->linestrips.numStrips] = (int*)colorLinePts[usedColPtslists];
			colorNode->linestrips.numPoints[colorNode->linestrips.numStrips] = 2;

			if (i == selectedNation) {
				Vector4Copy(graphColorSelected, colorNode->linestrips.color[colorNode->linestrips.numStrips]);
			} else {
				Vector4Copy(graphColors[i], colorNode->linestrips.color[colorNode->linestrips.numStrips]);
			}

			usedColPtslists++;
			colorNode->linestrips.numStrips++;
		}
	}

	/* Hide unused nation-entries. */
	for (i = gd.numNations; i < MAX_NATIONS; i++) {
		Cbuf_AddText(va("nation_hide%i;",i));
	}

	/** @todo Display summary of nation info */

	/* Display graph of nations-values so far. */
	graphNode = MN_GetNodeFromCurrentMenu("nation_graph_funding");
	if (graphNode) {
		usedFundPtslist = 0;
		graphNode->linestrips.numStrips = 0;

		/* Generate axes & link to node. */
		/** @todo Maybe create a margin toward the axes? */
		coordAxesPts[0].x = graphNode->pos[0];	/* x */
		coordAxesPts[0].y = graphNode->pos[1] - (int)graphNode->size[1];	/* y - height */
		coordAxesPts[1].x = graphNode->pos[0];	/* x */
		coordAxesPts[1].y = graphNode->pos[1];	/* y */
		coordAxesPts[2].x = graphNode->pos[0] + (int)graphNode->size[0];	/* x + width */
		coordAxesPts[2].y = graphNode->pos[1];	/* y */
		graphNode->linestrips.pointList[graphNode->linestrips.numStrips] = (int*)coordAxesPts;
		graphNode->linestrips.numPoints[graphNode->linestrips.numStrips] = 3;
		Vector4Copy(colorAxes, graphNode->linestrips.color[graphNode->linestrips.numStrips]);
		graphNode->linestrips.numStrips++;

		maxFunding = CL_NationsMaxFunding();
		for (i = 0; i < gd.numNations; i++) {
			if (i == selectedNation) {
				CL_NationDrawStats(&gd.nations[i], graphNode, maxFunding, -1);
			} else {
				CL_NationDrawStats(&gd.nations[i], graphNode, maxFunding, i);
			}
		}
	}
}

/**
 * @brief Select nation and display all relevant information for it.
 */
static void CL_NationSelect_f (void)
{
	int nat;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nat_idx>\n", Cmd_Argv(0));
		return;
	}

	nat = atoi(Cmd_Argv(1));
	if (nat < 0 || nat >=  gd.numNations) {
		Com_Printf("Invalid nation index: %is\n",nat);
		return;
	}

	selectedNation = nat;
	CL_NationStatsUpdate_f();
}

/**
 * @brief Load callback for campaign data
 * @sa CP_Save
 * @sa SAV_GameSave
 * @sa CP_SpawnBaseAttackMission
 * @sa CP_SpawnCrashSiteMission
 */
qboolean CP_Load (sizebuf_t *sb, void *data)
{
	int i, j, num;
	const char *name, *missionId;
	char val[32];
	base_t *base;

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

	memset(&ccs, 0, sizeof(ccs_t));
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

	Q_strncpyz(gd.oldMis1, MSG_ReadString(sb), sizeof(gd.oldMis1));
	Q_strncpyz(gd.oldMis2, MSG_ReadString(sb), sizeof(gd.oldMis2));
	Q_strncpyz(gd.oldMis3, MSG_ReadString(sb), sizeof(gd.oldMis3));

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
	ccs.humansAttackActivated = MSG_ReadByte(sb);

	/* read missions */
	num = MSG_ReadShort(sb);
	for (i = 0; i < num; i++) {
		mission_t mission;
		int ufoIdx;

		memset(&mission, 0, sizeof(mission));

		name = MSG_ReadString(sb);
		if (*name) {
			mission.mapDef = Com_GetMapDefinitionByID(name);
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
				base = B_GetBase(baseidx);
				if (base->baseStatus != BASE_UNDER_ATTACK)
					Com_Printf("......warning: base %i (%s) is supposedly under attack but base status doesn't match!\n", baseidx, base->name);
				else
					mission.data = (void *) base;
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission.stage >= STAGE_MISSION_GOTO) {
				alienBase_t *alienBase;
				const int baseidx = MSG_ReadByte(sb);
				/* don't check baseidx value here: alien bases are not loaded yet */
				alienBase = AB_GetBase(baseidx, 0);
				if (alienBase)
					mission.data = (void *) alienBase;
				else
					Com_Printf("Error while loading Alien Base mission (mission %i)\n", i);
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
		if (ufoIdx == AIRFIGHT_NO_TARGET)
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
	if (*missionId) {
		ccs.battleParameters.mission = CP_GetMissionById(missionId);
		ccs.battleParameters.numAlienTeams = MSG_ReadShort(sb);
		for (i = 0; i < ccs.battleParameters.numAlienTeams; i++)
			ccs.battleParameters.alienTeams[i] = Com_GetTeamDefinitionByID(MSG_ReadString(sb));
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
	if (*missionId)
		selectedMission = CP_GetMissionById(missionId);
	else
		selectedMission = NULL;

	/* and now fix the mission pointers for e.g. ufocrash sites
	 * this is needed because the base load function which loads the aircraft
	 * doesn't know anything (at that stage) about the new missions that were
	 * add in this load function */
	for (base = gd.bases, i = 0; i < gd.numBases; i++, base++) {
		for (j = 0; j < base->numAircraftInBase; j++) {
			if (base->aircraft[j].status == AIR_MISSION) {
				assert(base->aircraft[j].missionID);
				base->aircraft[j].mission = CP_GetMissionById(base->aircraft[j].missionID);

				/* not found */
				if (!base->aircraft[j].mission) {
					Com_Printf("Could not link mission '%s' in aircraft\n", base->aircraft[j].missionID);
					Mem_Free(base->aircraft[j].missionID);
					return qfalse;
				}
				Mem_Free(base->aircraft[j].missionID);
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

	MSG_WriteString(sb, gd.oldMis1);
	MSG_WriteString(sb, gd.oldMis2);
	MSG_WriteString(sb, gd.oldMis3);

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
				assert(base);
				MSG_WriteByte(sb, base->idx);
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
			MSG_WriteShort(sb, AIRFIGHT_NO_TARGET);
		MSG_WriteByte(sb, mission->onGeoscape);
	}

	/* store ccs.battleParameters */
	if (ccs.battleParameters.mission) {
		MSG_WriteString(sb, ccs.battleParameters.mission->id);
		MSG_WriteShort(sb, ccs.battleParameters.numAlienTeams);
		for (i = 0; i < ccs.battleParameters.numAlienTeams; i++)
			MSG_WriteString(sb, ccs.battleParameters.alienTeams[i]->id);
		MSG_WriteString(sb, ccs.battleParameters.alienEquipment);
		MSG_WriteString(sb, ccs.battleParameters.civTeam);
		MSG_WriteByte(sb, ccs.battleParameters.day);
		MSG_WriteShort(sb, ccs.battleParameters.ugv);
		MSG_WriteShort(sb, ccs.battleParameters.aliens);
		MSG_WriteShort(sb, ccs.battleParameters.civilians);
	} else
		MSG_WriteString(sb, "");

	/* stores the select mission on geoscape */
	if (selectedMission)
		MSG_WriteString(sb, selectedMission->id);
	else
		MSG_WriteString(sb, "");

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
 * @brief Nation saving callback
 */
qboolean NA_Save (sizebuf_t* sb, void* data)
{
	int i, j;
	for (i = 0; i < presaveArray[PRE_NATION]; i++) {
		for (j = 0; j < MONTHS_PER_YEAR; j++) {
			MSG_WriteByte(sb, gd.nations[i].stats[j].inuse);
			MSG_WriteFloat(sb, gd.nations[i].stats[j].happiness);
			MSG_WriteLong(sb, gd.nations[i].stats[j].xviInfection);
			MSG_WriteFloat(sb, gd.nations[i].stats[j].alienFriendly);
		}
	}
	return qtrue;
}

/**
 * @brief Nation loading callback
 */
qboolean NA_Load (sizebuf_t* sb, void* data)
{
	int i, j;

	for (i = 0; i < presaveArray[PRE_NATION]; i++) {
		for (j = 0; j < MONTHS_PER_YEAR; j++) {
			gd.nations[i].stats[j].inuse = MSG_ReadByte(sb);
			gd.nations[i].stats[j].happiness = MSG_ReadFloat(sb);
			gd.nations[i].stats[j].xviInfection = MSG_ReadLong(sb);
			gd.nations[i].stats[j].alienFriendly = MSG_ReadFloat(sb);
		}
	}
	return qtrue;
}

/**
 * @brief XVI map saving callback
 * @note Only save transparency
 */
qboolean XVI_Save (sizebuf_t *sb, void *data)
{
	byte out[512 * 256];
	int x, y;
	int width = 512;
	int height = 256;

	if (!R_XVIMapCopy(out, sizeof(out))) {
		/* Size of out is not the same than XVI map */
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

	return qtrue;
}

/**
 * @brief XVI map loading callback
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

	R_InitializeXVIOverlay(out, width, height);

	Mem_Free(out);

	return qtrue;
}

/**
 * @brief Set some needed cvars from mission definition
 * @param[in] mission mission definition pointer with the needed data to set the cvars to
 * @sa CL_GameGo
 */
static void CP_SetMissionVars (void)
{
	int i;
	const mission_t *mission;

	assert(ccs.battleParameters.mission);
	mission = ccs.battleParameters.mission;
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
 */
static void CP_StartMissionMap (mission_t* mission)
{
	char expanded[MAX_QPATH];
	base_t *bAttack;

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

	Cbuf_AddText(va("map %s %s %s\n", (MAP_IsNight(mission->pos) ? "night" : "day"),
		mission->mapDef->map, mission->mapDef->param ? mission->mapDef->param : ""));

	/* let the (local) server know which map we are running right now */
	csi.currentMD = mission->mapDef;
}

/**
 * @brief Starts a selected mission
 *
 * @note Checks whether a dropship is near the landing zone and whether it has a team on board
 * @sa CP_SetMissionVars
 */
static void CL_GameGo (void)
{
	mission_t *mis;
	aircraft_t *aircraft;
	base_t *base;

	if (!curCampaign) {
		Com_DPrintf(DEBUG_CLIENT, "curCampaign: %p, selectedMission: %p, interceptAircraft: %i\n", (void*)curCampaign, (void*)selectedMission, gd.interceptAircraft);
		return;
	}

	aircraft = cls.missionaircraft;
	base = CP_GetMissionBase();

	if (!selectedMission)
		selectedMission = aircraft->mission;

	if (!selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "No selectedMission\n");
		return;
	}

	mis = selectedMission;
	map_maxlevel_base = 0;
	assert(mis && aircraft);

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
	CP_SetMissionVars();
	/* Set the states of mission Cvars to proper values. */
	Cvar_SetValue("mission_homebase", base->idx);
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
 * @brief For things like craft_ufo_scout that are no real items this function will
 * increase the collected counter by one
 * @note Mission trigger function
 * @sa CP_MissionTriggerFunctions
 * @sa CP_ExecuteMissionTrigger
 */
static void CP_AddItemAsCollected (void)
{
	int i, baseID;
	objDef_t *item;
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
		item = &csi.ods[i];
		if (!Q_strncmp(id, item->id, MAX_VAR)) {
			gd.bases[baseID].storage.num[i]++;
			Com_DPrintf(DEBUG_CLIENT, "add item: '%s'\n", item->id);
			assert(item->tech);
			RS_MarkCollected((technology_t*)(item->tech));
		}
	}
}

/**
 * @brief Changes nation happiness by given multiplier.
 * @note There must be argument passed to this function being converted to float.
 */
static void CP_ChangeNationHappiness_f (void)
{
	float multiplier;
	nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <multiplier>\n", Cmd_Argv(0));
		return;
	}
	multiplier = atof(Cmd_Argv(1));

	/* we can use an assert here - because this script function will only
	 * be available as trigger command - selectedMission must be set at that stage */
	assert(selectedMission);
	nation = MAP_GetNation(selectedMission->pos);
	assert(nation);

	nation->stats[0].happiness = nation->stats[0].happiness * multiplier;

	/* Nation happiness cannot be greater than 1 */
	if (nation->stats[0].happiness > 1.0f)
		nation->stats[0].happiness = 1.0f;
}

/** @brief mission trigger functions */
static const cmdList_t cp_commands[] = {
	{"cp_add_item", CP_AddItemAsCollected, "Add an item as collected"},
	{"cp_changehappiness", CP_ChangeNationHappiness_f, "Function to raise or lower nation hapiness."},

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
void CP_ExecuteMissionTrigger (mission_t * m, qboolean won)
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
 *
 * You can mark a mission as story related.
 * If a mission is story related the cvar game_autogo is set to 0
 * If this cvar is 1 - the mission dialog will have a auto mission button
 * @sa CL_GameAutoGo_f
 */
static void CL_GameAutoCheck_f (void)
{
	if (!curCampaign || !selectedMission || gd.interceptAircraft == AIRCRAFT_INVALID || gd.interceptAircraft >= gd.numAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoCheck_f: No update after automission\n");
		return;
	}

	switch (selectedMission->mapDef->storyRelated) {
	case qtrue:
		Com_DPrintf(DEBUG_CLIENT, "story related - auto mission is disabled\n");
		Cvar_Set("game_autogo", "0");
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "auto mission is enabled\n");
		Cvar_Set("game_autogo", "1");
		break;
	}
}

/**
 * @brief Handles the auto mission for none storyrelated missions or missions that
 * failed to assembly
 * @sa CL_GameAutoGo_f
 * @sa CL_Drop
 * @sa AL_CollectingAliens
 */
void CL_GameAutoGo (mission_t *mis)
{
	qboolean won;
	aircraft_t *aircraft;
	int i, amount = 0;
	int civiliansKilled = 0; /* @todo: fill this for the case you won the game */
	aliensTmp_t *cargo;

	assert(mis);

	CP_CreateAlienTeam(mis);
	CP_CreateCivilianTeam(mis);
	CP_CreateBattleParameters(mis);

	if (mis->stage != STAGE_BASE_ATTACK) {
		if (gd.interceptAircraft == AIRCRAFT_INVALID || gd.interceptAircraft >= gd.numAircraft) {
			Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo: No update after automission\n");
			return;
		}

		aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);
		assert(aircraft);
		baseCurrent = aircraft->homebase;
		assert(baseCurrent);
		baseCurrent->aircraftCurrent = aircraft;	/* Might not be needed, but it's used later on in AIR_AircraftReturnToBase_f */

		if (!mis->active) {
			MN_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mis->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("game_autogo", "0");
			return;
		}
		/* FIXME: This needs work */
		won = ccs.battleParameters.aliens * difficulty->integer > aircraft->teamSize ? qfalse : qtrue;
		Com_DPrintf(DEBUG_CLIENT, "Aliens: %i (count as %i) - Soldiers: %i\n", ccs.battleParameters.aliens, ccs.battleParameters.aliens * difficulty->integer, aircraft->teamSize);
	} else {
		baseCurrent = (base_t*)mis->data;
		assert(baseCurrent);
		aircraft = NULL;
		/* FIXME: This needs work */
		won = qtrue;
	}

	MN_PopMenu(qfalse);

	/* update nation opinions */
	if (won) {
		CL_HandleNationData(!won, ccs.battleParameters.civilians, 0, 0, ccs.battleParameters.aliens, selectedMission);
		CP_CheckLostCondition(!won, mis, civiliansKilled);
	} else {
		CL_HandleNationData(!won, 0, ccs.battleParameters.civilians, ccs.battleParameters.aliens, 0, selectedMission);
		CP_CheckLostCondition(!won, mis, ccs.battleParameters.civilians);
	}

	if (mis->stage != STAGE_BASE_ATTACK) {
		assert(aircraft);

		/* collect all aliens as dead ones */
		cargo = aircraft->aliencargo;

		/* Make sure dropship aliencargo is empty. */
		memset(aircraft->aliencargo, 0, sizeof(aircraft->aliencargo));

		/* @todo: Check whether there are already aliens
		* @sa AL_CollectingAliens */
		/*if (aircraft->alientypes) {
		}*/

		aircraft->alientypes = ccs.battleParameters.numAlienTeams;
		for (i = 0; i < aircraft->alientypes; i++) {
			cargo[i].teamDef = ccs.battleParameters.alienTeams[i];
			/* FIXME: This could lead to more aliens in their sum */
			cargo[i].amount_dead = rand() % ccs.battleParameters.aliens;
			amount += cargo[i].amount_dead;
		}
		if (amount)
			MN_AddNewMessage(_("Notice"), va(_("Collected %i dead alien bodies"), amount), qfalse, MSG_STANDARD, NULL);

		if (aircraft->alientypes && !B_GetBuildingStatus(aircraft->homebase, B_ALIEN_CONTAINMENT)) {
			/* We have captured/killed aliens, but the homebase of this aircraft does not have alien containment. Popup aircraft transer dialog. */
			TR_TransferAircraftMenu(aircraft);
		}

		AIR_AircraftReturnToBase(aircraft);
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
			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Defense of base: %s successful!"), base->name);
			MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(selectedMission);
			/* @todo: @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CP_BaseAttackMissionLeave(selectedMission);
	} else if (won)
		CP_MissionIsOver(mis);

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
 * @note Soldier skill upgrade is being done here.
 * @sa CL_GameResults_f
 *
 * FIXME: See @todo and FIXME included
 */
static void CL_UpdateCharacterStats (int won)
{
	character_t *chr;
	rank_t *rank;
	aircraft_t *aircraft;
	int i, j, idx = 0;

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: numTeamList: %i\n", cl.numTeamList);

	/* aircraft = &baseCurrent->aircraft[gd.interceptAircraft]; remove this @todo: check if baseCurrent has the currect aircraftCurrent.  */
	aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: baseCurrent: %s\n", baseCurrent->name);

	/** @todo What about UGVs/Tanks? */
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++)
		if (CL_SoldierInAircraft(i, EMPL_SOLDIER, gd.interceptAircraft) ) {
			Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: searching for soldier: %i\n", i);
			chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, -(idx + 1));
			assert(chr);
			/* count every hired soldier in aircraft */
			idx++;
			chr->score.assignedMissions++;

			/** CL_UpdateCharacterSkills(chr);
			This is now done in g_client.c:G_UpdateCharacterSkills
			@todo I think we can remove this later on. I've left it here just FYI for now. */

			/* @todo: use chrScore_t to determine negative influence on soldier here,
			   like killing too many civilians and teammates can lead to unhire and disband
			   such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			   and do a promotion. */
			/* @todo: use param[in] in some way. */
			if (gd.numRanks >= 2) {
				for (j = gd.numRanks - 1; j > chr->score.rank; j--) {
					rank = &gd.ranks[j];
					/* FIXME: (Zenerka 20080301) extend ranks and change calculations here. */
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
		/* also count the employees that are only hired but not in the aircraft */
		} else if (E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, i))
			idx++;
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBase(i);

	for (i = 0; i < csi.numODs; i++) {
		if (!csi.ods[i].weapon && !csi.ods[i].numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, i, 1, qfalse, qtrue);
		if (base->storage.num[i] > 0) {
			tech = csi.ods[i].tech;
			if (!tech)
				Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", csi.ods[i].id, csi.ods[i].name);
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_itemlist
 */
static void CL_DebugShowItems_f (void)
{
	int i;
	base_t *base;
	technology_t *tech;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBase(i);

	for (i = 0; i < csi.numODs; i++) {
		tech = csi.ods[i].tech;
		if (!tech)
			Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", csi.ods[i].id, csi.ods[i].name);
		Com_Printf("%i. %s: (%s) %i\n", i, csi.ods[i].name, csi.ods[i].id, base->storage.num[i]);
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
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST, NULL, NULL);

	for (j = 0; j < 5; j++)
		/* Create a medic. */
		E_CreateEmployee(EMPL_MEDIC, NULL, NULL);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER, NULL, NULL);

	for (j = 0; j < 5; j++)
		/* Create a ares  ugv. */
		E_CreateEmployee(EMPL_ROBOT, NULL, CL_GetUgvById("ugv_ares_w"));

	for (j = 0; j < 5; j++)
		/* Create a phoenix  ugv. */
		E_CreateEmployee(EMPL_ROBOT, NULL, CL_GetUgvById("ugv_phoenix"));
}

/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f (void)
{
	int i, j;
	character_t *chr;

	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
		if (chr) {
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				chr->score.kills[j]++;
		}
	}
	CL_UpdateCharacterStats(1);
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
	employee_t* employee;
	int numberofsoldiers = 0; /* DEBUG */
	character_t *chr;

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f\n");

	/* multiplayer? */
	if (!curCampaign || !selectedMission || !baseCurrent)
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
		baseCurrent = (base_t*)selectedMission->data;
	} else {
		assert(gd.interceptAircraft != AIRCRAFT_INVALID);
		baseCurrent = AIR_AircraftGetFromIdx(gd.interceptAircraft)->homebase;
		baseCurrent->aircraftCurrent = AIR_AircraftGetFromIdx(gd.interceptAircraft);
	}

	/* add the looted goods to base storage and market */
	baseCurrent->storage = ccs.eMission; /* copied, including the arrays! */

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
	CL_UpdateCharacterStats(won);

	/* Backward loop because gd.numEmployees[EMPL_SOLDIER] is decremented by E_DeleteEmployee */
	for (i = gd.numEmployees[EMPL_SOLDIER] - 1; i >= 0; i--) {
		/* if employee is marked as dead */
		if (CL_SoldierInAircraft(i, EMPL_SOLDIER, gd.interceptAircraft))	/* DEBUG? */
			numberofsoldiers++;

		Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - try to get player %i \n", i);
		employee = E_GetHiredEmployee(baseCurrent, EMPL_SOLDIER, i);

		if (employee != NULL) {
			chr = E_GetHiredCharacter(baseCurrent, EMPL_SOLDIER, i);
			assert(chr);
			Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - idx %d hp %d\n",chr->ucn, chr->HP);
			if (chr->HP <= 0) { /* @todo: <= -50, etc. */
				/* Delete the employee. */
				/* sideeffect: gd.numEmployees[EMPL_SOLDIER] and teamNum[] are decremented by one here. */
				Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f: Delete this dead employee: %i (%s)\n", i, gd.employees[EMPL_SOLDIER][i].chr.name);
				E_DeleteEmployee(employee, EMPL_SOLDIER);
			} /* if dead */
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - num %i\n", numberofsoldiers); /* DEBUG */

	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f - done removing dead players\n");

	/* no transfer or campaign effects for base attack missions */
	if (selectedMission->stage != STAGE_BASE_ATTACK) {
		/* Check for alien containment in aircraft homebase. */
		if (baseCurrent->aircraftCurrent->alientypes && !B_GetBuildingStatus(baseCurrent, B_ALIEN_CONTAINMENT)) {
			/* We have captured/killed aliens, but the homebase of this aircraft does not have alien containment. Popup aircraft transer dialog. */
			TR_TransferAircraftMenu(baseCurrent->aircraftCurrent);
		} else {
			/* The aircraft can be savely sent to its homebase without losing aliens */

			/* @todo: Is this really needed? At the beginning of CL_GameResults_f we already have this status (if I read this correctly). */
			baseCurrent->aircraftCurrent->homebase = baseCurrent;
		}
		AIR_AircraftReturnToBase_f();
	}

	/* handle base attack mission */
	if (selectedMission->stage == STAGE_BASE_ATTACK) {
		base = (base_t*)selectedMission->data;
		assert(base);
		if (won) {
			Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Defense of base: %s successful!"), base->name);
			MN_AddNewMessage(_("Notice"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(selectedMission);
			/* @todo: @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CP_BaseAttackMissionLeave(selectedMission);
	} else if (won)
		CP_MissionIsOver(selectedMission);
}

/* =========================================================== */

/**
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseMission (const char *name, const char **text)
{
/* @todo Parse story related missions */
#if 0
	const char *errhead = "CL_ParseMission: unexpected end of file (mission ";
	mission_t *ms;
	const value_t *vp;
	const char *token;

	ms = CL_AddMission(name);
	if (!ms->mapDef) {
		numMissions--;
		return;
	}

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMission: mission def \"%s\" without body ignored\n", name);
		numMissions--;
		return;
	}

	if (!ms) {
		Com_Printf("CL_ParseMission: could not add mission \"%s\"\n", name);
		numMissions--;
		FS_SkipBlock(text);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (vp = mission_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				default:
					Com_ParseValue(ms, token, vp->type, vp->ofs, vp->size);
					break;
				case V_TRANSLATION_MANUAL_STRING:
					if (*token == '_')
						token++;
				/* fall through */
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)ms + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseMission: unknown token \"%s\" ignored (mission %s)\n", token, name);
		}

	} while (*text);
#endif
}

/* =========================================================== */

/**
 * @brief This function parses a list of items that should be set to researched = true after campaign start
 * @todo: Implement the use of this function
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
					Com_Printf("@todo: Mark unresearchable");
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
	{"medic_base", V_INT, offsetof(salary_t, medic_base), MEMBER_SIZEOF(salary_t, medic_base)},
	{"medic_rankbonus", V_INT, offsetof(salary_t, medic_rankbonus), MEMBER_SIZEOF(salary_t, medic_rankbonus)},
	{"robot_base", V_INT, offsetof(salary_t, robot_base), MEMBER_SIZEOF(salary_t, robot_base)},
	{"robot_rankbonus", V_INT, offsetof(salary_t, robot_rankbonus), MEMBER_SIZEOF(salary_t, robot_rankbonus)},
	{"aircraft_factor", V_INT, offsetof(salary_t, aircraft_factor), MEMBER_SIZEOF(salary_t, aircraft_factor)},
	{"aircraft_divisor", V_INT, offsetof(salary_t, aircraft_divisor), MEMBER_SIZEOF(salary_t, aircraft_divisor)},
	{"base_upkeep", V_INT, offsetof(salary_t, base_upkeep), MEMBER_SIZEOF(salary_t, base_upkeep)},
	{"admin_initial", V_INT, offsetof(salary_t, admin_initial), MEMBER_SIZEOF(salary_t, admin_initial)},
	{"admin_soldier", V_INT, offsetof(salary_t, admin_soldier), MEMBER_SIZEOF(salary_t, admin_soldier)},
	{"admin_worker", V_INT, offsetof(salary_t, admin_worker), MEMBER_SIZEOF(salary_t, admin_worker)},
	{"admin_scientist", V_INT, offsetof(salary_t, admin_scientist), MEMBER_SIZEOF(salary_t, admin_scientist)},
	{"admin_medic", V_INT, offsetof(salary_t, admin_medic), MEMBER_SIZEOF(salary_t, admin_medic)},
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

				Com_ParseValue(s, token, vp->type, vp->ofs, vp->size);
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
	{"medics", V_INT, offsetof(campaign_t, medics), MEMBER_SIZEOF(campaign_t, medics)},
	{"xvirate", V_INT, offsetof(campaign_t, maxAllowedXVIRateUntilLost), MEMBER_SIZEOF(campaign_t, maxAllowedXVIRateUntilLost)},
	{"maxdebts", V_INT, offsetof(campaign_t, negativeCreditsUntilLost), MEMBER_SIZEOF(campaign_t, negativeCreditsUntilLost)},
	{"minhappiness", V_FLOAT, offsetof(campaign_t, minhappiness), MEMBER_SIZEOF(campaign_t, minhappiness)},
	{"scientists", V_INT, offsetof(campaign_t, scientists), MEMBER_SIZEOF(campaign_t, scientists)},
	{"ugvs", V_INT, offsetof(campaign_t, ugvs), MEMBER_SIZEOF(campaign_t, ugvs)},
	{"equipment", V_STRING, offsetof(campaign_t, equipment), 0},
	{"market", V_STRING, offsetof(campaign_t, market), 0},
	{"researched", V_STRING, offsetof(campaign_t, researched), 0},
	{"difficulty", V_INT, offsetof(campaign_t, difficulty), MEMBER_SIZEOF(campaign_t, difficulty)},
	{"map", V_STRING, offsetof(campaign_t, map), 0},
	{"credits", V_INT, offsetof(campaign_t, credits), MEMBER_SIZEOF(campaign_t, credits)},
	{"visible", V_BOOL, offsetof(campaign_t, visible), MEMBER_SIZEOF(campaign_t, visible)},
	{"text", V_TRANSLATION_MANUAL_STRING, offsetof(campaign_t, text), 0}, /* just a gettext placeholder */
	{"name", V_TRANSLATION_STRING, offsetof(campaign_t, name), 0},
	{"date", V_DATE, offsetof(campaign_t, date), 0},
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
	memset(cp, 0, sizeof(campaign_t));

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
	memset(s, 0, sizeof(salary_t));
	s->soldier_base = 3000;
	s->soldier_rankbonus = 500;
	s->worker_base = 3000;
	s->worker_rankbonus = 500;
	s->scientist_base = 3000;
	s->scientist_rankbonus = 500;
	s->medic_base = 3000;
	s->medic_rankbonus = 500;
	s->robot_base = 7500;
	s->robot_rankbonus = 1500;
	s->aircraft_factor = 1;
	s->aircraft_divisor = 25;
	s->base_upkeep = 20000;
	s->admin_initial = 1000;
	s->admin_soldier = 75;
	s->admin_worker = 75;
	s->admin_scientist = 75;
	s->admin_medic = 75;
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

				Com_ParseValue(cp, token, vp->type, vp->ofs, vp->size);
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

/* =========================================================== */

static const value_t nation_vals[] = {
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(nation_t, name), 0},
	{"pos", V_POS, offsetof(nation_t, pos), MEMBER_SIZEOF(nation_t, pos)},
	{"color", V_COLOR, offsetof(nation_t, color), MEMBER_SIZEOF(nation_t, color)},
	{"funding", V_INT, offsetof(nation_t, maxFunding), MEMBER_SIZEOF(nation_t, maxFunding)},
	{"happiness", V_FLOAT, offsetof(nation_t, stats[0].happiness), MEMBER_SIZEOF(nation_t, stats[0].happiness)},
	{"alien_friendly", V_FLOAT, offsetof(nation_t, stats[0].alienFriendly), MEMBER_SIZEOF(nation_t, stats[0].alienFriendly)},
	{"soldiers", V_INT, offsetof(nation_t, maxSoldiers), MEMBER_SIZEOF(nation_t, maxSoldiers)},
	{"scientists", V_INT, offsetof(nation_t, maxScientists), MEMBER_SIZEOF(nation_t, maxScientists)},

	{NULL, 0, 0, 0}
};

/**
 * @brief Parse the nation data from script file
 * @param[in] name Name or ID of the found nation
 * @param[in] text The text of the nation node
 * @sa nation_vals
 * @sa CL_ParseScriptFirst
 * @note write into cl_localPool - free on every game restart and reparse
 */
void CL_ParseNations (const char *name, const char **text)
{
	const char *errhead = "CL_ParseNations: unexpected end of file (nation ";
	nation_t *nation;
	const value_t *vp;
	const char *token;

	if (gd.numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation = &gd.nations[gd.numNations++];
	memset(nation, 0, sizeof(nation_t));

	Com_DPrintf(DEBUG_CLIENT, "...found nation %s\n", name);
	nation->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);

	nation->stats[0].inuse = qtrue;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseNations: nation def \"%s\" without body ignored\n", name);
		gd.numNations--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* check for some standard values */
		for (vp = nation_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				case V_TRANSLATION_MANUAL_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)nation + (int)vp->ofs), cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
					break;
				default:
					if (Com_ParseValue(nation, token, vp->type, vp->ofs, vp->size) == -1)
						Com_Printf("CL_ParseNations: Wrong size for value %s\n", vp->string);
					break;
				}
				break;
			}

		if (!vp->string) {
			Com_Printf("CL_ParseNations: unknown token \"%s\" ignored (nation %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/**
 * @brief Check whether we are in a tactical mission as server or as client
 * @note handles multiplayer and singleplayer
 *
 * @return true when we are not in battlefield
 * @todo: Check cvar mn_main for value
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

/**
 * @brief Scriptfunction to list all parsed nations with their current values
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

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"mn_next_aircraft", AIM_NextAircraft_f, NULL},
	{"mn_prev_aircraft", AIM_PrevAircraft_f, NULL},
	{"aircraft_new", AIR_NewAircraft_f, NULL},
	{"mn_reset_air", AIM_ResetAircraftCvars_f, NULL},
	{"aircraft_return", AIR_AircraftReturnToBase_f, NULL},
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
	{"basedef_initmenu", BDEF_MenuInit_f, "Inits base defense menu"},
	{"basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defense menu"},
	{"basedef_slot_list_click", BDEF_ListClick_f, "Inits base defense menu"},
	{"basedef_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"stats_update", CL_StatsUpdate_f, NULL},
	{"nationlist", CL_NationList_f, "List all nations on the game console"},
	{"nation_stats_click", CP_NationStatsClick_f, NULL},
	{"nation_update", CL_NationStatsUpdate_f, "Shows the current nation list + statistics."},
	{"nation_select", CL_NationSelect_f, "Select nation and display all relevant information for it."},
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
	{"map_zoom", MAP_Zoom_f, ""},
	{"map_scroll", MAP_Scroll_f, ""},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
#ifdef DEBUG
	{"debug_aircraftlist", AIR_ListAircraft_f, "Debug function to list all aircraft in all bases"},
	{"debug_fullcredits", CL_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_newemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
	{"debug_additems", CL_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_itemlist", CL_DebugShowItems_f, "Debug function to show all items in base storage"},
	{"debug_missionlist", CL_DebugMissionList_f, "Debug function to show all missions"},
	{"debug_interestlist", CP_AlienInterestList_f, "Debug function to show alien interest values"},
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

		/* @todo: make sure all of gd is empty */
		gd.numBases = 0;
		E_ResetEmployees();
	}
	curCampaign = NULL;
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

	Com_AddObjectLinks();	/* Add tech links + ammo<->weapon links to items.*/
	RS_InitTree(load);	/* Initialise all data in the research tree. */

	CL_CampaignInitMarket(load);

	/* Init popup and map/geoscape */
	CL_PopupInit();
	MAP_GameInit();

	rs_alien_xvi = RS_GetTechByID("rs_alien_xvi");
	if (!rs_alien_xvi)
		Sys_Error("CL_ResetCampaign: Could not find tech definition for rs_alien_xvi");

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
 * @todo Check the stuff in this function - maybe not every functioncall
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

	if (!cl_start_employees->integer || !cl_start_buildings->integer) {
		Com_Printf("Activate initial employees and buildings in game options to play a skirmish game\n");
		return;
	}

	md = &csi.mds[cls.multiplayerMapDefinitionIndex];
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

	memset(&ccs, 0, sizeof(ccs_t));
	CL_StartSingleplayer(qtrue);
	CL_ReadSinglePlayerData();

	/* create employees and clear bases */
	B_NewBases();

	base = B_GetBase(0);
	baseCurrent = base;
	gd.numAircraft = 0;

	CL_GameInit(qfalse);
	RS_MarkResearchedAll();

	gd.numBases = 1;

	/* even in skirmish mode we need a little money to build the base */
	CL_UpdateCredits(MAX_CREDITS);

	/* build our pseudo base */
	B_SetUpBase(base);

	/* execute the pending commands - e.g. aircraft are added via commandbuffer */
	Cbuf_Execute();

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

	if (!cls.missionaircraft || !cls.missionaircraft->homebase) {
		Com_Printf("CL_GameSkirmish_f: Error - could not set the mission aircraft: %i\n", base->numAircraftInBase);
		return;
	}

	INV_InitialEquipment(base, curCampaign);

	Cvar_Set("mn_main", "multiplayerInGame");

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

	memset(&ccs, 0, sizeof(ccs_t));
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
	PR_ProductionInit();

	/* ensure ccs.singleplayer is set to true */
	CL_StartSingleplayer(qtrue);

	/* get day */
	while (ccs.date.sec > 3600 * 24) {
		ccs.date.sec -= 3600 * 24;
		ccs.date.day++;
	}

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(curCampaign->credits);

	/* Initialize alien interest */
	CL_ResetAlienInterest();

	/* Initialize XVI overlay */
	R_InitializeXVIOverlay(NULL, 0, 0);

	/* Reset alien bases */
	AB_ResetAlienBases();

	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	MN_PushMenu("map");

	/* create a base as first step */
	Cmd_ExecuteString("mn_select_base -1");

	CL_GameInit(qfalse);
	Cmd_ExecuteString("addeventmail prolog");

	CL_CampaignRunMarket();
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/*which building? */
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

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("Race: %s\nRecruits: %i %s, %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"),
			racetype,
			campaigns[num].soldiers, ngettext("soldier", "soldiers", campaigns[num].soldiers),
			campaigns[num].scientists, ngettext("scientist", "scientists", campaigns[num].scientists),
			campaigns[num].workers, ngettext("worker", "workers", campaigns[num].workers),
			campaigns[num].medics, ngettext("medic", "medics", campaigns[num].medics),
			campaigns[num].credits, CL_ToDifficultyName(campaigns[num].difficulty),
			(int)(round(campaigns[num].minhappiness * 100.0f)), campaigns[num].negativeCreditsUntilLost,
			_(campaigns[num].text));
	mn.menuText[TEXT_STANDARD] = campaignDesc;
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

	LIST_Delete(ccs.missions);
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
}

#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 * call this function via debug_campaignstats
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
	Com_Printf("...medic_base: %i\n", SALARY_MEDIC_BASE);
	Com_Printf("...medic_rankbonus: %i\n", SALARY_MEDIC_RANKBONUS);
	Com_Printf("...robot_base: %i\n", SALARY_ROBOT_BASE);
	Com_Printf("...robot_rankbonus: %i\n", SALARY_ROBOT_RANKBONUS);
	Com_Printf("...aircraft_factor: %i\n", SALARY_AIRCRAFT_FACTOR);
	Com_Printf("...aircraft_divisor: %i\n", SALARY_AIRCRAFT_DIVISOR);
	Com_Printf("...base_upkeep: %i\n", SALARY_BASE_UPKEEP);
	Com_Printf("...admin_initial: %i\n", SALARY_ADMIN_INITIAL);
	Com_Printf("...admin_soldier: %i\n", SALARY_ADMIN_SOLDIER);
	Com_Printf("...admin_worker: %i\n", SALARY_ADMIN_WORKER);
	Com_Printf("...admin_scientist: %i\n", SALARY_ADMIN_SCIENTIST);
	Com_Printf("...admin_medic: %i\n", SALARY_ADMIN_MEDIC);
	Com_Printf("...admin_robot: %i\n", SALARY_ADMIN_ROBOT);
	Com_Printf("...debt_interest: %.5f\n", SALARY_DEBT_INTEREST);
}
#endif /* DEBUG */

/* ======================== */
/* Onwin mission functions. */
/*
1. CP_UFORecovered_f() is triggered by mission "onwin" cp_uforecovery value.
2. CP_UFORecoveredStore_f() prepares UFO recovery process.
3. CP_UFORecoveryBaseSelectPopup_f() allows to select desired base.
4. CP_UFORecoveredStart_f() starts UFO recovery process.
5. CP_UFORecoveryNationSelectPopup_f() allows to select desired nation.
6. CP_UFOSellStart_f() starts UFO sell process.
7. CP_UFORecoveredSell_f() allows to sell UFO to desired nation.
8. CP_UFORecoveredDestroy_f() destroys UFO.
9. CP_UFORecoveryDone() finishes the recovery process and updates buttons on menu won.
*/
/* ======================== */

/**
 * @brief Send an email to list all recovered item.
 * @sa CL_EventAddMail_f
 * @sa CL_NewEventMail
 */
void CP_UFOSendMail (const aircraft_t *ufocraft, const base_t *base)
{
	int i;
	components_t *comp;
	objDef_t *compOd;
	eventMail_t *mail;
	char body[512] = "";

	assert(ufocraft);
	assert(base);

	if (missionresults.crashsite) {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_crashed_report", va("ufo_crashed_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("CP_UFOSendMail: ufo_crashed_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("CP_UFOSendMail: ufo_crashed_report has no mail body");

		/* Find components definition. */
		comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id)->idx);
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->item_amount2[i] > 0)
				Q_strcat(body, va(_("  * %i x\t%s\n"), comp->item_amount2[i], compOd->name), sizeof(body));
		}
	} else {
		/* take the source mail and create a copy of it */
		mail = CL_NewEventMail("ufo_recovery_report", va("ufo_recovery_report%i", ccs.date.sec), NULL);
		if (!mail)
			Sys_Error("CP_UFOSendMail: ufo_recovery_report wasn't found");
		/* we need the source mail body here - this may not be NULL */
		if (!mail->body)
			Sys_Error("CP_UFOSendMail: ufo_recovery_report has no mail body");

		/* Find components definition. */
		comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id)->idx);
		assert(comp);

		/* List all components of crashed UFO. */
		for (i = 0; i < comp->numItemtypes; i++) {
			compOd = comp->items[i];
			assert(compOd);
			if (comp->item_amount[i] > 0)
				Q_strcat(body, va(_("  * %s\n"), compOd->name), sizeof(body));
		}
	}
	assert(mail);

	/* don't free the old mail body here - it's the string of the source mail */
	mail->body = Mem_PoolStrDup(va(_(mail->body), UFO_TypeToName(missionresults.ufotype), base->name, body), cl_localPool, CL_TAG_NONE);

	/* update subject */
	/* Insert name of the mission in the template */
	mail->subject = Mem_PoolStrDup(va(_(mail->subject), base->name), cl_localPool, CL_TAG_NONE);

	/* Add the mail to unread mail */
	Cmd_ExecuteString(va("addeventmail %s", mail->id));
}

/**
 * @brief Function to trigger UFO Recovered event.
 * @note This function prepares related cvars for won menu.
 * @note Command to call this: cp_uforecovery.
 */
static void CP_UFORecovered_f (void)
{
	int i;
	ufoType_t UFOtype;
	base_t *base;
	aircraft_t *ufocraft;
	qboolean store = qfalse, ufofound = qfalse;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		Com_Printf("CP_UFORecovered()... UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* At the beginning we enable all UFO recovery options. */
	Cmd_ExecuteString("menuwon_update_buttons");

	ufocraft = NULL;
	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == UFOtype) {
			ufofound = qtrue;
			break;
		}
	}
	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecovered()... UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	base = NULL;
	/* Now we have to check whether we can store the UFO in any base. */
	for (i = 0; i < gd.numBases; i++) {
		base = B_GetBase(i);
		if (!base->founded)
			continue;
		if (UFO_ConditionsForStoring(base, ufocraft)) {
			store = qtrue;
			break;
		}
	}

	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qfalse;
	missionresults.ufotype = ufocraft->ufotype;

	/* Do nothing without any base. */
	if (!base)
		return;

	/* Prepare related cvars. */
	Cvar_SetValue("mission_uforecovered", 1);	/* This is used in menus to enable UFO Recovery nodes. */
	Cvar_SetValue("mission_uforecoverydone", 0);	/* This is used in menus to block UFO Recovery nodes. */
	Cvar_SetValue("mission_ufotype", UFOtype);
	Cvar_SetValue("mission_recoverynation", -1);
	Cvar_SetValue("mission_recoverybase", -1);
	/* @todo block Sell button if no nation with requirements */
	if (!store) {
		/* Block store option if storing not possible. */
		Cmd_ExecuteString("disufostore");
		Cvar_SetValue("mission_noufohangar", 1);
	} else
		Cvar_SetValue("mission_noufohangar", 0);
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecovered_f()...: base: %s, UFO: %i\n", base->name, UFOtype);
}

/**
 * @brief Updates UFO recovery process and disables buttons.
 */
static void CP_UFORecoveryDone (void)
{
	/* Disable Try Again a mission. */
	Cvar_SetValue("mission_tryagain", 1);
	Cbuf_AddText("distryagain\n");
	/* Disable UFORecovery buttons. */
	Cbuf_AddText("disallrecovery\n");

	/* Set done cvar for function updating. */
	Cvar_SetValue("mission_uforecoverydone", 1);
}

/** @brief Array of base indexes where we can store UFO. */
static int UFObases[MAX_BASES];

/**
 * @brief Finds the destination base for UFO recovery.
 * @note The base selection is being done here.
 * @note Callback command: cp_uforecovery_baselist_click.
 */
static void CP_UFORecoveryBaseSelectPopup_f (void)
{
	int num;
	base_t* base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= MAX_BASES || UFObases[num] == -1)
		return;

	base = B_GetBase(UFObases[num]);

	assert(base);

	/* Pop the menu and launch it again - now with updated value of selected base. */
	Cvar_SetValue("mission_recoverybase", base->idx);
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryBaseSelectPopup_f: picked base: %s\n", base->name);
	MN_PopMenu(qfalse);
	Cmd_ExecuteString("cp_uforecoverystore");
}

/**
 * @brief Function to start UFO recovery process.
 * @note Command to call this: cp_uforecoverystart.
 */
static void CP_UFORecoveredStart_f (void)
{
	base_t *base;
	int i;

	i = Cvar_VariableInteger("mission_recoverybase");

	if (i < 0 || i > MAX_BASES)
		return;

	base = B_GetBase(i);
	assert(base);
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer),
		_("Recovered %s from the battlefield. UFO is being transported to base %s."),
		UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")), base->name);
	MN_AddNewMessage(_("UFO Recovery"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
	UFO_PrepareRecovery(base);

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to store recovered UFO in desired base.
 * @note Command to call this: cp_uforecoverystore.
 * @sa UFO_ConditionsForStoring
 */
static void CP_UFORecoveredStore_f (void)
{
	int i, baseHasUFOHangarCount = 0, recoveryBase = -1;
	base_t *base;
	aircraft_t *ufocraft;
	static char recoveryBaseSelectPopup[512];
	qboolean ufofound = qfalse;

	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype")) {
			ufofound = qtrue;
			break;
		}
	}

	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFORecoveredStore_f... UFOType: %i does not have valid craft definition!\n", Cvar_VariableInteger("mission_ufotype"));
		return;
	}

	/* Clear UFObases. */
	memset(UFObases, -1, sizeof(UFObases));

	recoveryBaseSelectPopup[0] = '\0';
	/* Check how many bases can store this UFO. */
	for (i = 0; i < gd.numBases; i++) {
		base = B_GetBase(i);
		if (!base->founded)
			continue;
		if (UFO_ConditionsForStoring(base, ufocraft)) {
			Q_strcat(recoveryBaseSelectPopup, base->name, sizeof(recoveryBaseSelectPopup));
			Q_strcat(recoveryBaseSelectPopup, "\n", sizeof(recoveryBaseSelectPopup));
			UFObases[baseHasUFOHangarCount++] = i;
		}
	}

	/* Do nothing without any base. */
	if (!base)
		return;

	/* If only one base with UFO hangars, the recovery will be done in this base. */
	switch (baseHasUFOHangarCount) {
	case 0:
		/* No UFO base with proper conditions, do nothing. */
		return;
	case 1:
		/* there should only be one entry in UFObases - so use that one. */
		Cvar_SetValue("mission_recoverybase", UFObases[0]);
		CP_UFORecoveredStart_f();
		break;
	default:
		recoveryBase = Cvar_VariableInteger("mission_recoverybase");
		if (recoveryBase < 0) {
			/* default selection: make sure you select one base before leaving popup window */
			Cvar_SetValue("mission_recoverybase", UFObases[0]);
			recoveryBase = UFObases[0];
		}
		Q_strcat(recoveryBaseSelectPopup, _("\n\nSelected base:\t\t\t"), sizeof(recoveryBaseSelectPopup));
		if (recoveryBase >= 0 && recoveryBase < MAX_BASES) {
			for (i = 0; i < baseHasUFOHangarCount; i++) {
				if (UFObases[i] == recoveryBase) {
					Q_strcat(recoveryBaseSelectPopup, gd.bases[UFObases[i]].name, sizeof(recoveryBaseSelectPopup));
					break;
				}
			}
		}
		/* If more than one - popup with list to select base. */
		mn.menuText[TEXT_LIST] = recoveryBaseSelectPopup;
		MN_PushMenu("popup_recoverybaselist");
		break;
	}
}

/** @brief Array of prices proposed by nation. */
static int UFOprices[MAX_NATIONS];

/**
 * @brief Finds the nation to which recovered UFO will be sold.
 * @note The nation selection is being done here.
 * @note Callback command: cp_uforecovery_nationlist_click.
 */
static void CP_UFORecoveryNationSelectPopup_f (void)
{
	int i, j = -1, num;
	const nation_t *nation;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nationid>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));

	for (i = 0, nation = gd.nations; i < gd.numNations; i++, nation++) {
		j++;
		if (j == num) {
			Cvar_SetValue("mission_recoverynation", i);
			break;
		}
	}
	assert(nation);

	/* Pop the menu and launch it again - now with updated value of selected nation. */
	MN_PopMenu(qfalse);
	Com_DPrintf(DEBUG_CLIENT, "CP_UFORecoveryNationSelectPopup_f: picked nation: %s\n", nation->name);
	Cmd_ExecuteString("cp_uforecoverysell");
}

/**
 * @brief Function to start UFO selling process.
 * @note Command to call this: cp_ufosellstart.
 */
static void CP_UFOSellStart_f (void)
{
	nation_t *nation;
	int i;

	nation = &gd.nations[Cvar_VariableInteger("mission_recoverynation")];
	assert(nation);
	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Recovered %s from the battlefield. UFO sold to nation %s, gained %i credits."),
		UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")), _(nation->name), UFOprices[Cvar_VariableInteger("mission_recoverynation")]);
	MN_AddNewMessage(_("UFO Recovery"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);
	CL_UpdateCredits(ccs.credits + UFOprices[Cvar_VariableInteger("mission_recoverynation")]);

	/* update nation happiness */
	for (i = 0; i < gd.numNations; i++) {
		if (gd.nations + i == nation) {
			/* nation is happy because it got the UFO */
			gd.nations[i].stats[0].happiness += 0.3f * (1.0f - gd.nations[i].stats[0].happiness);
			/* Nation happiness cannot be greater than 1 */
			if (nation->stats[0].happiness > 1.0f)
				nation->stats[0].happiness = 1.0f;
		} else
			/* nation is unhappy because it wanted the UFO */
			gd.nations[i].stats[0].happiness *= .95f;
	}

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to sell recovered UFO to desired nation.
 * @note Command to call this: cp_uforecoverysell.
 */
static void CP_UFORecoveredSell_f (void)
{
	int i, nations = 0;
	aircraft_t *ufocraft;
	static char recoveryNationSelectPopup[512];

	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	ufocraft = NULL;
	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == Cvar_VariableInteger("mission_ufotype"))
			break;
	}

	if (Cvar_VariableInteger("mission_recoverynation") == -1)
		memset(UFOprices, 0, sizeof(UFOprices));

	recoveryNationSelectPopup[0] = '\0';

	for (i = 0; i < gd.numNations; i++) {
		const nation_t *nation = &gd.nations[i];
		/* @todo only nations with proper alien infiltration values */
		nations++;
		/* Calculate price offered by nation only if this is first popup opening. */
		if (Cvar_VariableInteger("mission_recoverynation") == -1)
			UFOprices[i] = ufocraft->price + (int)(frand() * 100000);
		Q_strcat(recoveryNationSelectPopup, _(nation->name), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\t\t\t", sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, va("%i", UFOprices[i]), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\t\t", sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, CL_GetNationHappinessString(nation), sizeof(recoveryNationSelectPopup));
		Q_strcat(recoveryNationSelectPopup, "\n", sizeof(recoveryNationSelectPopup));
	}

	Q_strcat(recoveryNationSelectPopup, _("\n\nSelected nation:\t\t\t"), sizeof(recoveryNationSelectPopup));
	if (Cvar_VariableInteger("mission_recoverynation") != -1) {
		for (i = 0; i < gd.numNations; i++) {
			if (i == Cvar_VariableInteger("mission_recoverynation")) {
				Q_strcat(recoveryNationSelectPopup, _(gd.nations[i].name), sizeof(recoveryNationSelectPopup));
				break;
			}
		}
	}

	/* Do nothing without at least one nation. */
	if (nations == 0)
		return;

	mn.menuText[TEXT_LIST] = recoveryNationSelectPopup;
	MN_PushMenu("popup_recoverynationlist");
}

/**
 * @brief Function to destroy recovered UFO.
 * @note Command to call this: cp_uforecoverydestroy.
 */
static void CP_UFORecoveredDestroy_f (void)
{
	/* Do nothing if recovery process is finished. */
	if (Cvar_VariableInteger("mission_uforecoverydone") == 1)
		return;

	Com_sprintf(mn.messageBuffer, sizeof(mn.messageBuffer), _("Secured %s was destroyed."),
		UFO_TypeToName(Cvar_VariableInteger("mission_ufotype")));
	MN_AddNewMessage(_("UFO Recovery"), mn.messageBuffer, qfalse, MSG_STANDARD, NULL);

	/* UFO recovery process is done, disable buttons. */
	CP_UFORecoveryDone();
}

/**
 * @brief Function to process crashed UFO.
 * @note Command to call this: cp_ufocrashed.
 */
static void CP_UFOCrashed_f (void)
{
	int i, j;
	ufoType_t UFOtype;
	aircraft_t *aircraft, *ufocraft;
	qboolean ufofound = qfalse;
	components_t *comp;
	objDef_t *compOd;
	itemsTmp_t *cargo;

	if (!baseCurrent || gd.interceptAircraft == AIRCRAFT_INVALID)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <UFOType>\n", Cmd_Argv(0));
		return;
	}

	if ((atoi(Cmd_Argv(1)) >= 0) && (atoi(Cmd_Argv(1)) < UFO_MAX)) {
		UFOtype = atoi(Cmd_Argv(1));
	} else {
		UFOtype = UFO_ShortNameToID(Cmd_Argv(1));
		if (UFOtype == UFO_MAX) {
			Com_Printf("CP_UFOCrashed_f()... UFOType: %i does not exist!\n", atoi(Cmd_Argv(1)));
			return;
		}
	}

	/* Find ufo sample of given ufotype. */
	for (i = 0; i < numAircraft_samples; i++) {
		ufocraft = &aircraft_samples[i];
		if (ufocraft->type != AIRCRAFT_UFO)
			continue;
		if (ufocraft->ufotype == UFOtype) {
			ufofound = qtrue;
			break;
		}
	}

	/* Do nothing without UFO of this type. */
	if (!ufofound) {
		Com_Printf("CP_UFOCrashed_f()... UFOType: %i does not have valid craft definition!\n", atoi(Cmd_Argv(1)));
		return;
	}

	/* Find dropship. */
	aircraft = AIR_AircraftGetFromIdx(gd.interceptAircraft);
	assert(aircraft);
	cargo = aircraft->itemcargo;

	/* Find components definition. */
	comp = INV_GetComponentsByItemIdx(INVSH_GetItemByID(ufocraft->id)->idx);
	assert(comp);

	/* Add components of crashed UFO to dropship. */
	for (i = 0; i < comp->numItemtypes; i++) {
		compOd = comp->items[i];
		assert(compOd);
		Com_DPrintf(DEBUG_CLIENT, "CP_UFOCrashed_f()... Collected %i of %s\n", comp->item_amount2[i], comp->items[i]->id);
		/* Add items to cargo, increase itemtypes. */
		cargo[aircraft->itemtypes].idx = j;
		cargo[aircraft->itemtypes].amount = comp->item_amount2[i];
		aircraft->itemtypes++;
	}
	/* Put relevant info into missionresults array. */
	missionresults.recovery = qtrue;
	missionresults.crashsite = qtrue;
	missionresults.ufotype = ufocraft->ufotype;

	/* send mail */
	CP_UFOSendMail(ufocraft, aircraft->homebase);
}

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
 * @param[out] pos The position that will be overwritten
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
 * @param[out] pos The position that will be overwritten with the random point fulfilling the criterias
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
	float posX, posY;

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
		posY = asin(2.0 * y / numPoints + offsetY) * todeg;	/* Use non-uniform distribution otherwise we favour the poles */
		for (x = 0; x < numPoints; x++) {
			posX = x * RASTER - 180.0 + offsetX;

			Vector2Set(posT, posX, posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				/* the location given in pos belongs to the terrain, culture, population types and nations
				 * that are acceptable, so count it */
				/* @todo - cache the counted hits */
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
		posY = asin(2.0 * y / numPoints + offsetY) * todeg;
		for (x = 0; x < numPoints; x++) {
			posX = x * RASTER - 180.0 + offsetX;

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

	return qtrue;
}

void CL_ResetCampaign (void)
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
	Cmd_AddCommand("cp_uforecovery", CP_UFORecovered_f, "Function to trigger UFO Recovered event");
	Cmd_AddCommand("cp_uforecovery_baselist_click", CP_UFORecoveryBaseSelectPopup_f, "Callback for recovery base list popup.");
	Cmd_AddCommand("cp_uforecoverystart", CP_UFORecoveredStart_f, "Function to start UFO recovery processing.");
	Cmd_AddCommand("cp_uforecoverystore", CP_UFORecoveredStore_f, "Function to store recovered UFO in desired base.");
	Cmd_AddCommand("cp_uforecovery_nationlist_click", CP_UFORecoveryNationSelectPopup_f, "Callback for recovery nation list popup.");
	Cmd_AddCommand("cp_ufosellstart", CP_UFOSellStart_f, "Function to start UFO selling processing.");
	Cmd_AddCommand("cp_uforecoverysell", CP_UFORecoveredSell_f, "Function to sell recovered UFO to desired nation.");
	Cmd_AddCommand("cp_uforecoverydestroy", CP_UFORecoveredDestroy_f, "Function to destroy recovered UFO.");
	Cmd_AddCommand("cp_ufocrashed", CP_UFOCrashed_f, "Function to process crashed UFO after a mission.");
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
	Cmd_AddCommand("debug_campaignstats", CP_CampaignStats_f, "Print campaign stats to game console");
	Cmd_AddCommand("debug_addmission", CP_SpawnNewMissions_f, "Add a new mission");
	Cmd_AddCommand("debug_delmissions", CP_DeleteMissions_f, "Remove all missions from global array");
#endif
}
