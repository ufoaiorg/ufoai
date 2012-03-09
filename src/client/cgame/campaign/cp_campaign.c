/**
 * @file cp_campaign.c
 * @brief Single player campaign control.
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

#include "../../cl_shared.h"
#include "../../ui/ui_main.h"
#include "../cgame.h"
#include "cp_campaign.h"
#include "cp_capacity.h"
#include "cp_overlay.h"
#include "cp_mapfightequip.h"
#include "cp_hospital.h"
#include "cp_hospital_callbacks.h"
#include "cp_base_callbacks.h"
#include "cp_basedefence_callbacks.h"
#include "cp_team.h"
#include "cp_team_callbacks.h"
#include "cp_popup.h"
#include "cp_map.h"
#include "cp_ufo.h"
#include "cp_installation_callbacks.h"
#include "cp_alien_interest.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_nation.h"
#include "cp_statistics.h"
#include "cp_time.h"
#include "cp_xvi.h"
#include "cp_fightequip_callbacks.h"
#include "cp_produce_callbacks.h"
#include "cp_transfer.h"
#include "cp_market_callbacks.h"
#include "cp_research_callbacks.h"
#include "cp_uforecovery.h"
#include "save/save_campaign.h"
#include "cp_auto_mission.h"

struct memPool_s *cp_campaignPool;		/**< reset on every game restart */
ccs_t ccs;
cvar_t *cp_campaign;
cvar_t *cp_start_employees;
cvar_t *cp_missiontest;
const cgame_import_t *cgi;

typedef struct {
	int ucn;
	int HP;
	int STUN;
	int morale;

	chrScoreGlobal_t chrscore;
} updateCharacter_t;

/**
 * @brief Parses the character data which was send by G_MatchSendResults using G_SendCharacterData
 * @param[in] msg The network buffer message. If this is NULL the character is updated, if this
 * is not NULL the data is stored in a temp buffer because the player can choose to retry
 * the mission and we have to catch this situation to not update the character data in this case.
 * @sa G_SendCharacterData
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa E_Save
 */
void CP_ParseCharacterData (struct dbuffer *msg)
{
	static linkedList_t *updateCharacters = NULL;

	if (!msg) {
		LIST_Foreach(updateCharacters, updateCharacter_t, c) {
			employee_t *employee = E_GetEmployeeFromChrUCN(c->ucn);
			character_t* chr;

			if (!employee) {
				Com_Printf("Warning: Could not get character with ucn: %i.\n", c->ucn);
				continue;
			}

			chr = &employee->chr;
			chr->HP = min(c->HP, chr->maxHP);
			chr->STUN = c->STUN;
			chr->morale = c->morale;

			memcpy(chr->score.experience, c->chrscore.experience, sizeof(chr->score.experience));
			memcpy(chr->score.skills, c->chrscore.skills, sizeof(chr->score.skills));
			memcpy(chr->score.kills, c->chrscore.kills, sizeof(chr->score.kills));
			memcpy(chr->score.stuns, c->chrscore.stuns, sizeof(chr->score.stuns));
			chr->score.assignedMissions = c->chrscore.assignedMissions;
		}
		LIST_Delete(&updateCharacters);
	} else {
		int i, j;
		const int num = NET_ReadByte(msg);

		if (num < 0)
			Com_Error(ERR_DROP, "CP_ParseCharacterData: NET_ReadShort error (%i)\n", num);

		LIST_Delete(&updateCharacters);

		for (i = 0; i < num; i++) {
			updateCharacter_t c;
			OBJZERO(c);
			c.ucn = NET_ReadShort(msg);
			c.HP = NET_ReadShort(msg);
			c.STUN = NET_ReadByte(msg);
			c.morale = NET_ReadByte(msg);

			for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
				c.chrscore.experience[j] = NET_ReadLong(msg);
			for (j = 0; j < SKILL_NUM_TYPES; j++)
				c.chrscore.skills[j] = NET_ReadByte(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				c.chrscore.kills[j] = NET_ReadShort(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				c.chrscore.stuns[j] = NET_ReadShort(msg);
			c.chrscore.assignedMissions = NET_ReadShort(msg);
			LIST_Add(&updateCharacters, (const byte*)&c, sizeof(c));
		}
	}
}

/**
 * @brief Checks whether a campaign mode game is running
 */
qboolean CP_IsRunning (void)
{
	return ccs.curCampaign != NULL;
}

/**
 * @brief Check if a map may be selected for mission.
 * @param[in] mission Pointer to the mission where mapDef should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] mapIdx idx of the map in the mapdef array
 * @return qfalse if map is not selectable
 */
static qboolean CP_MapIsSelectable (mission_t *mission, mapDef_t *md, const vec2_t pos)
{
	if (md->storyRelated)
		return qfalse;

	if (pos && !MAP_PositionFitsTCPNTypes(pos, md->terrains, md->cultures, md->populations, NULL))
		return qfalse;

	if (!mission->ufo) {
		/* a mission without UFO should not use a map with UFO */
		if (md->ufos)
			return qfalse;
	} else if (md->ufos) {
		/* A mission with UFO should use a map with UFO
		 * first check that list is not empty */
		const ufoType_t type = mission->ufo->ufotype;
		const char *ufoID;

		if (mission->crashed)
			ufoID = Com_UFOCrashedTypeToShortName(type);
		else
			ufoID = Com_UFOTypeToShortName(type);

		if (!LIST_ContainsString(md->ufos, ufoID))
			return qfalse;
	} else {
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Choose a map for given mission.
 * @param[in,out] mission Pointer to the mission where a new map should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @return qfalse if could not set mission
 */
qboolean CP_ChooseMap (mission_t *mission, const vec2_t pos)
{
	int maxHits = 1;	/**< Total number of maps fulfilling mission conditions. */
	int hits = 0;		/**< Number of maps fulfilling mission conditions and that appeared less often during game. */
	int minMissionAppearance = 9999;
	int randomNum;

	if (mission->mapDef)
		return qtrue;

	/* Set maxHits and hits. */
	while (maxHits) {
		mapDef_t *md;
		maxHits = 0;
		MapDef_ForeachSingleplayerCampaign(md) {
			/* Check if mission fulfill conditions */
			if (!CP_MapIsSelectable(mission, md, pos))
				continue;

			maxHits++;
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

		if (md == NULL) {
			/* We scanned all maps in memory without finding a map used less than minMissionAppearance: exit while loop */
			break;
		}
	}

	if (!maxHits) {
		/* no map fulfill the conditions */
		if (mission->category == INTERESTCATEGORY_RESCUE) {
			/* default map for rescue mission is the rescue random map assembly */
			mission->mapDef = Com_GetMapDefinitionByID("rescue");
			if (!mission->mapDef)
				Com_Error(ERR_DROP, "Could not find mapdef: rescue");
			mission->mapDef->timesAlreadyUsed++;
			return qtrue;
		} else if (mission->crashed) {
			/* default map for crashsite mission is the crashsite random map assembly */
			mission->mapDef = Com_GetMapDefinitionByID("ufocrash");
			if (!mission->mapDef)
				Com_Error(ERR_DROP, "Could not find mapdef: ufocrash");
			mission->mapDef->timesAlreadyUsed++;
			return qtrue;
		} else {
			Com_Printf("CP_ChooseMap: Could not find map with required conditions:\n");
			Com_Printf("  ufo: %s -- pos: ", mission->ufo ? Com_UFOTypeToShortName(mission->ufo->ufotype) : "none");
			if (pos)
				Com_Printf("%s", MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)) ? " (in water) " : "");
			if (pos)
				Com_Printf("(%.02f, %.02f)\n", pos[0], pos[1]);
			else
				Com_Printf("none\n");
			return qfalse;
		}
	}

	/* If we reached this point, that means that at least 1 map fulfills the conditions of the mission
	 * set number of mission to select randomly between 0 and hits - 1 */
	randomNum = rand() % hits;

	/* Select mission mission number 'randomnumber' that fulfills the conditions */
	mapDef_t *md;
	MapDef_ForeachSingleplayerCampaign(md) {
		/* Check if mission fulfill conditions */
		if (!CP_MapIsSelectable(mission, md, pos))
			continue;

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
	mission->mapDef = md;
	mission->mapDef->timesAlreadyUsed++;
	if (cp_missiontest->integer)
		Com_Printf("Selected map '%s' (among %i possible maps)\n", mission->mapDef->id, hits);
	else
		Com_DPrintf(DEBUG_CLIENT, "Selected map '%s' (among %i possible maps)\n", mission->mapDef->id, hits);

	return qtrue;
}

/**
 * @brief Function to handle the campaign end
 */
void CP_EndCampaign (qboolean won)
{
	Cmd_ExecuteString("game_exit");

	if (won)
		UI_InitStack("endgame", NULL, qtrue, qtrue);
	else
		UI_InitStack("lostgame", NULL, qtrue, qtrue);

	Com_Drop();
}

/**
 * @brief Checks whether the player has lost the campaign
 */
void CP_CheckLostCondition (const campaign_t *campaign)
{
	qboolean endCampaign = qfalse;
	/* fraction of nation that can be below min happiness before the game is lost */
	const float nationBelowLimitPercentage = 0.5f;

	if (cp_missiontest->integer)
		return;

	if (!endCampaign && ccs.credits < -campaign->negativeCreditsUntilLost) {
		UI_RegisterText(TEXT_STANDARD, _("You've gone too far into debt."));
		endCampaign = qtrue;
	}

	/** @todo Should we make the campaign lost when a player loses all his bases?
	 * until he has set up a base again, the aliens might have invaded the whole
	 * world ;) - i mean, removing the credits check here. */
	if (ccs.credits < campaign->basecost - campaign->negativeCreditsUntilLost && !B_AtLeastOneExists()) {
		UI_RegisterText(TEXT_STANDARD, _("You've lost your bases and don't have enough money to build new ones."));
		endCampaign = qtrue;
	}

	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > campaign->maxAllowedXVIRateUntilLost) {
			UI_RegisterText(TEXT_STANDARD, _("You have failed in your charter to protect Earth."
				" Our home and our people have fallen to the alien infection. Only a handful"
				" of people on Earth remain human, and the remaining few no longer have a"
				" chance to stem the tide. Your command is no more; PHALANX is no longer"
				" able to operate as a functioning unit. Nothing stands between the aliens"
				" and total victory."));
			endCampaign = qtrue;
		} else {
			/* check for nation happiness */
			int j, nationBelowLimit = 0;
			for (j = 0; j < ccs.numNations; j++) {
				const nation_t *nation = NAT_GetNationByIDX(j);
				const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
				if (stats->happiness < campaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= nationBelowLimitPercentage * ccs.numNations) {
				/* lost the game */
				UI_RegisterText(TEXT_STANDARD, _("Under your command, PHALANX operations have"
					" consistently failed to protect nations."
					" The UN, highly unsatisfied with your performance, has decided to remove"
					" you from command and subsequently disbands the PHALANX project as an"
					" effective task force. No further attempts at global cooperation are made."
					" Earth's nations each try to stand alone against the aliens, and eventually"
					" fall one by one."));
				endCampaign = qtrue;
			}
		}
	}

	if (endCampaign)
		CP_EndCampaign(qfalse);
}

/* Initial fraction of the population in the country where a mission has been lost / won */
#define XVI_LOST_START_PERCENTAGE	0.20f
#define XVI_WON_START_PERCENTAGE	0.05f

/**
 * @brief Updates each nation's happiness.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 * @todo Scoring should eventually be expanded to include such elements as
 * infected humans and mission objectives other than xenocide.
 */
void CP_HandleNationData (float minHappiness, mission_t * mis, const nation_t *affectedNation, const missionResults_t *results)
{
	int i;
	const float civilianSum = (float) (results->civiliansSurvived + results->civiliansKilled + results->civiliansKilledFriendlyFire);
	const float alienSum = (float) (results->aliensSurvived + results->aliensKilled + results->aliensStunned);
	float performance, performanceAlien, performanceCivilian;
	float deltaHappiness = 0.0f;
	float happinessDivisor = 5.0f;

	/** @todo HACK: This should be handled properly, i.e. civilians should only factor into the scoring
	 * if the mission objective is actually to save civilians. */
	if (civilianSum == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CP_HandleNationData: Warning, civilianSum == 0, score for this mission will default to 0.\n");
		performance = 0.0f;
	} else {
		/* Calculate how well the mission went. */
		performanceCivilian = (2 * civilianSum - results->civiliansKilled - 2
				* results->civiliansKilledFriendlyFire) * 3 / (2 * civilianSum) - 2;
		/** @todo The score for aliens is always negative or zero currently, but this
		 * should be dependent on the mission objective.
		 * In a mission that has a special objective, the amount of killed aliens should
		 * only serve to increase the score, not reduce the penalty. */
		performanceAlien = results->aliensKilled + results->aliensStunned - alienSum;
		performance = performanceCivilian + performanceAlien;
	}

	/* Calculate the actual happiness delta. The bigger the mission, the more potential influence. */
	deltaHappiness = 0.004 * civilianSum + 0.004 * alienSum;

	/* There is a maximum base happiness delta. */
	if (deltaHappiness > HAPPINESS_MAX_MISSION_IMPACT)
		deltaHappiness = HAPPINESS_MAX_MISSION_IMPACT;

	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = NAT_GetNationByIDX(i);
		const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
		float happinessFactor;

		/* update happiness. */
		if (nation == affectedNation)
			happinessFactor = deltaHappiness;
		else
			happinessFactor = deltaHappiness / happinessDivisor;

		NAT_SetHappiness(minHappiness, nation, stats->happiness + performance * happinessFactor);
	}
}

/**
 * @brief Check for missions that have a timeout defined
 */
static void CP_CheckMissionEnd (const campaign_t* campaign)
{
	MIS_Foreach(mission) {
		if (CP_CheckMissionLimitedInTime(mission) && Date_LaterThan(&ccs.date, &mission->finalDate))
			CP_MissionStageEnd(campaign, mission);
	}
}

/* =========================================================== */

/**
 * @brief Functions that should be called with a minimum time lapse (will be called at least every DETECTION_INTERVAL)
 * @param[in] campaign The campaign data structure
 * @param[in] dt Elapsed game seconds since last call.
 * @param[in] updateRadarOverlay true if radar overlay should be updated (only for drawing purpose)
 * @sa CP_CampaignRun
 */
static void CP_CampaignFunctionPeriodicCall (campaign_t* campaign, int dt, qboolean updateRadarOverlay)
{
	UFO_CampaignRunUFOs(campaign, dt);
	AIR_CampaignRun(campaign, dt, updateRadarOverlay);

	AIRFIGHT_CampaignRunBaseDefence(dt);
	AIRFIGHT_CampaignRunProjectiles(campaign, dt);
	CP_CheckNewMissionDetectedOnGeoscape();

	/* Update alien interest for bases */
	UFO_UpdateAlienInterestForAllBasesAndInstallations();

	/* Update how phalanx troop know alien bases */
	AB_UpdateStealthForAllBase();

	UFO_CampaignCheckEvents();
}

/**
 * @brief Returns if we are currently on the Geoscape
 * @todo This relies on scripted content. Should work other way!
 */
qboolean CP_OnGeoscape (void)
{
	return Q_streq("geoscape", UI_GetActiveWindowName());
}

/**
 * @brief delay between actions that must be executed independently of time scale
 * @sa RADAR_CheckUFOSensored
 * @sa UFO_UpdateAlienInterestForAllBasesAndInstallations
 * @sa AB_UpdateStealthForAllBase
 */
const int DETECTION_INTERVAL = (SECONDS_PER_HOUR / 2);

/**
 * @brief Ensure that the day always matches the seconds. If the seconds
 * per day limit is reached, the seconds are reset and the day is increased.
 * @param seconds The seconds to add to the campaign date
 */
static inline void CP_AdvanceTimeBySeconds (int seconds)
{
	ccs.date.sec += seconds;
	while (ccs.date.sec >= SECONDS_PER_DAY) {
		ccs.date.sec -= SECONDS_PER_DAY;
		ccs.date.day++;
	}
}

/**
 * @return @c true if a month has passed
 */
static inline qboolean CP_IsBudgetDue (const dateLong_t *oldDate, const dateLong_t *date)
{
	if (oldDate->year < date->year) {
		return qtrue;
	}
	return oldDate->month < date->month;
}

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types UI_MAP and UI_3DMAP
 * @sa NAT_HandleBudget
 * @sa B_UpdateBaseData
 * @sa AIR_CampaignRun
 */
void CP_CampaignRun (campaign_t *campaign, float secondsSinceLastFrame)
{
	/* advance time */
	ccs.frametime = secondsSinceLastFrame;
	ccs.timer += secondsSinceLastFrame * ccs.gameTimeScale;

	UP_GetUnreadMails();

	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int currenthour;
		int currentmin;
		int currentsecond = ccs.date.sec;
		int currentday = ccs.date.day;
		int i;
		const int currentinterval = (int)floor(currentsecond) % DETECTION_INTERVAL;
		int dt = DETECTION_INTERVAL - currentinterval;
		dateLong_t date, oldDate;
		const int checks = (currentinterval + (int)floor(ccs.timer)) / DETECTION_INTERVAL;

		CP_DateConvertLong(&ccs.date, &oldDate);

		currenthour = (int)floor(currentsecond / SECONDS_PER_HOUR);
		currentmin = (int)floor(currentsecond / SECONDS_PER_MINUTE);

		/* Execute every actions that needs to be independent of time speed : every DETECTION_INTERVAL
		 *	- Run UFOs and craft at least every DETECTION_INTERVAL. If detection occurred, break.
		 *	- Check if any new mission is detected
		 *	- Update stealth value of phalanx bases and installations ; alien bases */
		for (i = 0; i < checks; i++) {
			ccs.timer -= dt;
			currentsecond += dt;
			CP_AdvanceTimeBySeconds(dt);
			CP_CampaignFunctionPeriodicCall(campaign, dt, qfalse);

			/* if something stopped time, we must stop here the loop */
			if (CP_IsTimeStopped()) {
				ccs.timer = 0.0f;
				break;
			}
			dt = DETECTION_INTERVAL;
		}

		dt = (int)floor(ccs.timer);

		CP_AdvanceTimeBySeconds(dt);
		currentsecond += dt;
		ccs.timer -= dt;

		/* compute minutely events  */
		/* (this may run multiple times if the time stepping is > 1 minute at a time) */
		while (currentmin < (int)floor(currentsecond / SECONDS_PER_MINUTE)) {
			currentmin++;
			PR_ProductionRun();
			B_UpdateBaseData();
		}

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(currentsecond / SECONDS_PER_HOUR)) {
			currenthour++;
			RS_ResearchRun();
			UR_ProcessActive();
			AII_UpdateInstallationDelay();
			AII_RepairAircraft();
			TR_TransferRun();
			INT_IncreaseAlienInterest(campaign);
		}

		/* daily events */
		for (i = currentday; i < ccs.date.day; i++) {
			/* every day */
			INS_UpdateInstallationData();
			HOS_HospitalRun();
			ccs.missionSpawnCallback();
			CP_SpreadXVI();
			NAT_UpdateHappinessForAllNations(campaign->minhappiness);
			AB_BaseSearchedByNations();
			CP_CampaignRunMarket(campaign);
			CP_CheckCampaignEvents(campaign);
			CP_ReduceXVIEverywhere();
			/* should be executed after all daily event that could
			 * change XVI overlay */
			CP_UpdateNationXVIInfection();
		}

		if (dt > 0) {
			/* check for campaign events
			 * aircraft and UFO already moved during radar detection (see above),
			 * just make them move the missing part -- if any */
			CP_CampaignFunctionPeriodicCall(campaign, dt, qtrue);
		}

		CP_CheckMissionEnd(campaign);
		CP_CheckLostCondition(campaign);
		/* Check if there is a base attack mission */
		CP_CheckBaseAttacks();
		/* check if any stores are full */
		CAP_CheckOverflow();
		BDEF_AutoSelectTarget();

		CP_DateConvertLong(&ccs.date, &date);
		/* every new month we have to handle the budget */
		if (CP_IsBudgetDue(&oldDate, &date) && ccs.paid && B_AtLeastOneExists()) {
			NAT_BackupMonthlyData();
			NAT_HandleBudget(campaign);
			ccs.paid = qfalse;
		} else if (date.day > 1)
			ccs.paid = qtrue;

		CP_UpdateXVIMapButton();
		/* set time cvars */
		CP_UpdateTime();
	}
}

/**
 * @brief Checks whether you have enough credits for something
 * @param[in] costs costs to check
 */
qboolean CP_CheckCredits (int costs)
{
	if (costs > ccs.credits)
		return qfalse;
	return qtrue;
}

/**
 * @brief Sets credits and update mn_credits cvar
 * @param[in] credits The new credits value
 * Checks whether credits are bigger than MAX_CREDITS
 */
void CP_UpdateCredits (int credits)
{
	/* credits */
	if (credits > MAX_CREDITS)
		credits = MAX_CREDITS;
	ccs.credits = credits;
	Cvar_Set("mn_credits", va(_("%i c"), ccs.credits));
}

/**
 * @brief Load mapDef statistics
 * @param[in] parent XML Node structure, where we get the information from
 */
static qboolean CP_LoadMapDefStatXML (xmlNode_t *parent)
{
	xmlNode_t *node;

	for (node = XML_GetNode(parent, SAVE_CAMPAIGN_MAPDEF); node; node = XML_GetNextNode(node, parent, SAVE_CAMPAIGN_MAPDEF)) {
		const char *s = XML_GetString(node, SAVE_CAMPAIGN_MAPDEF_ID);
		mapDef_t *map;

		if (s[0] == '\0') {
			Com_Printf("Warning: MapDef with no id in xml!\n");
			continue;
		}
		map = Com_GetMapDefinitionByID(s);
		if (!map) {
			Com_Printf("Warning: No MapDef with id '%s'!\n", s);
			continue;
		}
		map->timesAlreadyUsed = XML_GetInt(node, SAVE_CAMPAIGN_MAPDEF_COUNT, 0);
	}

	return qtrue;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
qboolean CP_LoadXML (xmlNode_t *parent)
{
	xmlNode_t *campaignNode;
	xmlNode_t *mapNode;
	const char *name;
	campaign_t *campaign;
	xmlNode_t *mapDefStat;

	campaignNode = XML_GetNode(parent, SAVE_CAMPAIGN_CAMPAIGN);
	if (!campaignNode) {
		Com_Printf("Did not find campaign entry in xml!\n");
		return qfalse;
	}
	if (!(name = XML_GetString(campaignNode, SAVE_CAMPAIGN_ID))) {
		Com_Printf("couldn't locate campaign name in savegame\n");
		return qfalse;
	}

	campaign = CP_GetCampaign(name);
	if (!campaign) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		return qfalse;
	}

	CP_CampaignInit(campaign, qtrue);
	/* init the map images and reset the map actions */
	MAP_Reset(campaign->map);

	/* read credits */
	CP_UpdateCredits(XML_GetLong(campaignNode, SAVE_CAMPAIGN_CREDITS, 0));
	ccs.paid = XML_GetBool(campaignNode, SAVE_CAMPAIGN_PAID, qfalse);

	cgi->SetNextUniqueCharacterNumber(XML_GetInt(campaignNode, SAVE_CAMPAIGN_NEXTUNIQUECHARACTERNUMBER, 0));

	XML_GetDate(campaignNode, SAVE_CAMPAIGN_DATE, &ccs.date.day, &ccs.date.sec);

	/* read other campaign data */
	ccs.civiliansKilled = XML_GetInt(campaignNode, SAVE_CAMPAIGN_CIVILIANSKILLED, 0);
	ccs.aliensKilled = XML_GetInt(campaignNode, SAVE_CAMPAIGN_ALIENSKILLED, 0);

	Com_DPrintf(DEBUG_CLIENT, "CP_LoadXML: Getting position\n");

	/* read map view */
	mapNode = XML_GetNode(campaignNode, SAVE_CAMPAIGN_MAP);
	ccs.center[0] = XML_GetFloat(mapNode, SAVE_CAMPAIGN_CENTER0, 0.0);
	ccs.center[1] = XML_GetFloat(mapNode, SAVE_CAMPAIGN_CENTER1, 0.0);
	ccs.angles[0] = XML_GetFloat(mapNode, SAVE_CAMPAIGN_ANGLES0, 0.0);
	ccs.angles[1] = XML_GetFloat(mapNode, SAVE_CAMPAIGN_ANGLES1, 0.0);
	ccs.zoom = XML_GetFloat(mapNode, SAVE_CAMPAIGN_ZOOM, 0.0);
	/* restore the overlay.
	* do not use Cvar_SetValue, because this function check if value->string are equal to skip calculation
	* and we never set r_geoscape_overlay->string in game: cl_geoscape_overlay won't be updated if the loaded
	* value is 0 (and that's a problem if you're loading a game when cl_geoscape_overlay is set to another value */
	cl_geoscape_overlay->integer = XML_GetInt(mapNode, SAVE_CAMPAIGN_CL_GEOSCAPE_OVERLAY, 0);
	radarOverlayWasSet = XML_GetBool(mapNode, SAVE_CAMPAIGN_RADAROVERLAYWASSET, qfalse);
	ccs.XVIShowMap = XML_GetBool(mapNode, SAVE_CAMPAIGN_XVISHOWMAP, qfalse);
	CP_UpdateXVIMapButton();

	mapDefStat = XML_GetNode(campaignNode, SAVE_CAMPAIGN_MAPDEFSTAT);
	if (mapDefStat && !CP_LoadMapDefStatXML(mapDefStat))
		return qfalse;

	mxmlDelete(campaignNode);
	return qtrue;
}

/**
 * @brief Save mapDef statistics
 * @param[out] parent XML Node structure, where we write the information to
 */
static qboolean CP_SaveMapDefStatXML (xmlNode_t *parent)
{
	const mapDef_t const* md;

	MapDef_ForeachSingleplayerCampaign(md) {
		if (md->timesAlreadyUsed > 0) {
			xmlNode_t *node = XML_AddNode(parent, SAVE_CAMPAIGN_MAPDEF);
			XML_AddString(node, SAVE_CAMPAIGN_MAPDEF_ID, md->id);
			XML_AddInt(node, SAVE_CAMPAIGN_MAPDEF_COUNT, md->timesAlreadyUsed);
		}
	}

	return qtrue;
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
qboolean CP_SaveXML (xmlNode_t *parent)
{
	xmlNode_t *campaign;
	xmlNode_t *map;
	xmlNode_t *mapDefStat;

	campaign = XML_AddNode(parent, SAVE_CAMPAIGN_CAMPAIGN);

	XML_AddString(campaign, SAVE_CAMPAIGN_ID, ccs.curCampaign->id);
	XML_AddDate(campaign, SAVE_CAMPAIGN_DATE, ccs.date.day, ccs.date.sec);
	XML_AddLong(campaign, SAVE_CAMPAIGN_CREDITS, ccs.credits);
	XML_AddShort(campaign, SAVE_CAMPAIGN_PAID, ccs.paid);
	XML_AddShortValue(campaign, SAVE_CAMPAIGN_NEXTUNIQUECHARACTERNUMBER, cgi->GetNextUniqueCharacterNumber());

	XML_AddIntValue(campaign, SAVE_CAMPAIGN_CIVILIANSKILLED, ccs.civiliansKilled);
	XML_AddIntValue(campaign, SAVE_CAMPAIGN_ALIENSKILLED, ccs.aliensKilled);

	/* Map and user interface */
	map = XML_AddNode(campaign, SAVE_CAMPAIGN_MAP);
	XML_AddFloat(map, SAVE_CAMPAIGN_CENTER0, ccs.center[0]);
	XML_AddFloat(map, SAVE_CAMPAIGN_CENTER1, ccs.center[1]);
	XML_AddFloat(map, SAVE_CAMPAIGN_ANGLES0, ccs.angles[0]);
	XML_AddFloat(map, SAVE_CAMPAIGN_ANGLES1, ccs.angles[1]);
	XML_AddFloat(map, SAVE_CAMPAIGN_ZOOM, ccs.zoom);
	XML_AddShort(map, SAVE_CAMPAIGN_CL_GEOSCAPE_OVERLAY, cl_geoscape_overlay->integer);
	XML_AddBool(map, SAVE_CAMPAIGN_RADAROVERLAYWASSET, radarOverlayWasSet);
	XML_AddBool(map, SAVE_CAMPAIGN_XVISHOWMAP, ccs.XVIShowMap);

	mapDefStat = XML_AddNode(campaign, SAVE_CAMPAIGN_MAPDEFSTAT);
	if (!CP_SaveMapDefStatXML(mapDefStat))
		return qfalse;

	return qtrue;
}

/**
 * @brief Starts a selected mission
 * @note Checks whether a dropship is near the landing zone and whether
 * it has a team on board
 * @sa CP_SetMissionVars
 */
void CP_StartSelectedMission (void)
{
	mission_t *mis;
	aircraft_t *aircraft = MAP_GetMissionAircraft();
	base_t *base;
	battleParam_t *battleParam = &ccs.battleParameters;

	if (!aircraft) {
		Com_Printf("CP_StartSelectedMission: No mission aircraft\n");
		return;
	}

	base = aircraft->homebase;

	if (MAP_GetSelectedMission() == NULL)
		MAP_SetSelectedMission(aircraft->mission);

	mis = MAP_GetSelectedMission();
	if (!mis) {
		Com_Printf("CP_StartSelectedMission: No mission selected\n");
		return;
	}

	/* Before we start, we should clear the missionResults array. */
	OBJZERO(ccs.missionResults);

	/* Various sanity checks. */
	if (!mis->active) {
		Com_Printf("CP_StartSelectedMission: Dropship not near landing zone: mis->active: %i\n", mis->active);
		return;
	}
	if (AIR_GetTeamSize(aircraft) == 0) {
		Com_Printf("CP_StartSelectedMission: No team in dropship.\n");
		return;
	}

	/* if we retry a mission we have to drop from the current game before */
	SV_Shutdown("Server quit.", qfalse);
	cgi->CL_Disconnect();

	CP_CreateBattleParameters(mis, battleParam, aircraft);
	CP_SetMissionVars(mis, battleParam);

	/* manage inventory */
	/**
	 * @todo find out why this black-magic with inventory is needed and clean up
	 * @sa AM_Go
	 * @sa CP_MissionEnd
	 */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CP_CleanTempInventory(base);
	CP_CleanupAircraftCrew(aircraft, &ccs.eMission);
	CP_StartMissionMap(mis, battleParam);
}

/**
 * @brief Checks whether a soldier should be promoted
 * @param[in] rank The rank to check for
 * @param[in] chr The character to check a potential promotion for
 * @todo (Zenerka 20080301) extend ranks and change calculations here.
 */
static qboolean CP_ShouldUpdateSoldierRank (const rank_t *rank, const character_t* chr)
{
	if (rank->type != EMPL_SOLDIER)
		return qfalse;

	/* mind is not yet enough */
	if (chr->score.skills[ABILITY_MIND] < rank->mind)
		return qfalse;

	/* not enough killed enemies yet */
	if (chr->score.kills[KILLED_ENEMIES] < rank->killedEnemies)
		return qfalse;

	/* too many civilians and team kills */
	if (chr->score.kills[KILLED_CIVILIANS] + chr->score.kills[KILLED_TEAM] > rank->killedOthers)
		return qfalse;

	return qtrue;
}

/**
 * @brief Update employees stats after mission.
 * @param[in] base The base where the team lives.
 * @param[in] aircraft The aircraft used for the mission.
 * @note Soldier promotion is being done here.
 */
void CP_UpdateCharacterStats (const base_t *base, const aircraft_t *aircraft)
{
	assert(aircraft);

	/* only soldiers have stats and ranks, ugvs not */
	E_Foreach(EMPL_SOLDIER, employee) {
		if (!E_IsInBase(employee, aircraft->homebase))
			continue;
		if (AIR_IsEmployeeInAircraft(employee, aircraft)) {
			character_t *chr = &employee->chr;

			/* Remember the number of assigned mission for this character. */
			chr->score.assignedMissions++;

			/** @todo use chrScore_t to determine negative influence on soldier here,
			 * like killing too many civilians and teammates can lead to unhire and disband
			 * such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			 * and do a promotion. */
			if (ccs.numRanks >= 2) {
				int j;
				for (j = ccs.numRanks - 1; j > chr->score.rank; j--) {
					const rank_t *rank = CL_GetRankByIdx(j);
					if (CP_ShouldUpdateSoldierRank(rank, chr)) {
						chr->score.rank = j;
						if (chr->HP > 0)
							Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been promoted to %s.\n"), chr->name, _(rank->name));
						else
							Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been awarded the posthumous rank of %s\nfor inspirational gallantry in the face of overwhelming odds.\n"), chr->name, _(rank->name));
						MS_AddNewMessage(_("Soldier promoted"), cp_messageBuffer, qfalse, MSG_PROMOTION, NULL);
						break;
					}
				}
			}
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "CP_UpdateCharacterStats: Done\n");
}

#ifdef DEBUG
/**
 * @brief Debug function to add one item of every type to base storage and mark them collected.
 * @note Command to call this: debug_additems
 */
static void CP_DebugAllItems_f (void)
{
	int i;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);
		if (!obj->weapon && !obj->numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, obj, 1, qtrue);
		if (B_ItemInBase(obj, base) > 0) {
			technology_t *tech = RS_GetTechForItem(obj);
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_listitem
 */
static void CP_DebugShowItems_f (void)
{
	int i;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = INVSH_GetItemByIDX(i);
		Com_Printf("%i. %s: %i\n", i, obj->id, B_ItemInBase(obj, base));
	}
}

/**
 * @brief Debug function to set the credits to max
 */
static void CP_DebugFullCredits_f (void)
{
	CP_UpdateCredits(MAX_CREDITS);
}
#endif

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"update_base_radar_coverage", RADAR_UpdateBaseRadarCoverage_f, "Update base radar coverage"},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"stats_update", CP_StatsUpdate_f, NULL},
	{"game_go", CP_StartSelectedMission, NULL},
	{"game_timestop", CP_GameTimeStop, NULL},
	{"game_timeslow", CP_GameTimeSlow, NULL},
	{"game_timefast", CP_GameTimeFast, NULL},
	{"game_settimeid", CP_SetGameTime_f, NULL},
	{"map_center", MAP_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"map_zoom", MAP_Zoom_f, NULL},
	{"map_scroll", MAP_Scroll_f, NULL},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
#ifdef DEBUG
	{"debug_fullcredits", CP_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_additems", CP_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_listitem", CP_DebugShowItems_f, "Debug function to show all items in base storage"},
#endif
	{NULL, NULL, NULL}
};

/**
 * @brief registers callback commands that are used by campaign
 * @todo callbacks should be registered on menu push
 * (what about sideeffects for commands that are called from different menus?)
 * @sa CP_AddCampaignCommands
 * @sa CP_RemoveCampaignCallbackCommands
 */
static void CP_AddCampaignCallbackCommands (void)
{
	AIM_InitCallbacks();
	B_InitCallbacks();
	BDEF_InitCallbacks();
	BS_InitCallbacks();
	CP_TEAM_InitCallbacks();
	HOS_InitCallbacks();
	INS_InitCallbacks();
	PR_InitCallbacks();
	RS_InitCallbacks();
}

static void CP_AddCampaignCommands (void)
{
	const cmdList_t *commands;

	for (commands = game_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);

	CP_AddCampaignCallbackCommands();
}

/**
 * @brief registers callback commands that are used by campaign
 * @todo callbacks should be removed on menu pop
 * (what about sideeffects for commands that are called from different menus?)
 * @sa CP_AddCampaignCommands
 * @sa CP_RemoveCampaignCallbackCommands
 */
static void CP_RemoveCampaignCallbackCommands (void)
{
	AIM_ShutdownCallbacks();
	B_ShutdownCallbacks();
	BDEF_ShutdownCallbacks();
	BS_ShutdownCallbacks();
	CP_TEAM_ShutdownCallbacks();
	HOS_ShutdownCallbacks();
	INS_ShutdownCallbacks();
	PR_ShutdownCallbacks();
	RS_ShutdownCallbacks();
	MSO_Shutdown();
	UP_Shutdown();
}

static void CP_RemoveCampaignCommands (void)
{
	const cmdList_t *commands;

	for (commands = game_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

	CP_RemoveCampaignCallbackCommands();
}

/**
 * @brief Called at new game and load game
 * @param[in] load @c true if we are loading game, @c false otherwise
 * @param[in] campaign Pointer to campaign - it will be set to @c ccs.curCampaign here.
 */
void CP_CampaignInit (campaign_t *campaign, qboolean load)
{
	ccs.curCampaign = campaign;

	CP_ReadCampaignData(campaign);

	/* initialise date */
	ccs.date = campaign->date;
	/* get day */
	while (ccs.date.sec > SECONDS_PER_DAY) {
		ccs.date.sec -= SECONDS_PER_DAY;
		ccs.date.day++;
	}
	CP_UpdateTime();

	RS_InitTree(campaign, load);		/**< Initialise all data in the research tree. */

	CP_AddCampaignCommands();

	CP_GameTimeStop();

	/* Init popup and map/geoscape */
	CL_PopupInit();

	CP_InitOverlay();

	CP_XVIInit();

	UI_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	if (load) {
		return;
	}

	Cmd_ExecuteString("addeventmail prolog");

	RS_MarkResearchable(qtrue, NULL);
	BS_InitMarket(campaign);

	/* create initial employees */
	E_InitialEmployees(campaign);
	/* initialise view angle for 3D geoscape so that europe is seen */
	ccs.angles[YAW] = GLOBE_ROTATE;

	MAP_Reset(campaign->map);
	PR_ProductionInit();

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;
	Vector2Set(ccs.smoothFinal2DGeoscapeCenter, 0.5, 0.5);
	VectorSet(ccs.smoothFinalGlobeAngle, 0, GLOBE_ROTATE, 0);

	CP_UpdateCredits(campaign->credits);

	/* Initialize alien interest */
	INT_ResetAlienInterest();

	/* Initialize XVI overlay */
	Cvar_SetValue("mn_xvimap", ccs.XVIShowMap);
	CP_InitializeXVIOverlay(NULL);

	/* create a base as first step */
	B_SelectBase(NULL);

	/* Spawn first missions of the game */
	CP_InitializeSpawningDelay();

	/* now check the parsed values for errors that are not caught at parsing stage */
	if (!load)
		CP_ScriptSanityCheck();
}

/**
 * @brief Campaign closing actions
 */
void CP_Shutdown (void)
{
	if (CP_IsRunning()) {
		int i;

		AB_Shutdown();
		AIR_Shutdown();
		INS_Shutdown();
		INT_Shutdown();
		NAT_Shutdown();
		MIS_Shutdown();
		TR_Shutdown();
		UR_Shutdown();
		AM_Shutdown();

		/** @todo Where does this belong? */
		for (i = 0; i < ccs.numAlienCategories; i++) {
			alienTeamCategory_t *alienCat = &ccs.alienCategories[i];
			LIST_Delete(&alienCat->equipment);
		}

		cl_geoscape_overlay->integer = 0;
		/* singleplayer commands are no longer available */
		Com_DPrintf(DEBUG_CLIENT, "Remove game commands\n");
		CP_RemoveCampaignCommands();
	}

	MAP_Shutdown();
	CP_ShutdownOverlay();
}

/**
 * @brief Returns the campaign pointer from global campaign array
 * @param name Name of the campaign
 * @return campaign_t pointer to campaign with name or NULL if not found
 */
campaign_t* CP_GetCampaign (const char* name)
{
	campaign_t* campaign;
	int i;

	for (i = 0, campaign = ccs.campaigns; i < ccs.numCampaigns; i++, campaign++)
		if (Q_streq(name, campaign->id))
			break;

	if (i == ccs.numCampaigns) {
		Com_Printf("CL_GetCampaign: Campaign \"%s\" doesn't exist.\n", name);
		return NULL;
	}
	return campaign;
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa INV_InitInventory
 * @sa CP_ParseCampaignData
 */
void CP_ResetCampaignData (void)
{
	int i;
	mapDef_t *md;

	cp_messageStack = NULL;

	/* cleanup dynamic mails */
	CP_FreeDynamicEventMail();

	Mem_FreePool(cp_campaignPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetTechs();
	E_ResetEmployees();

	OBJZERO(ccs);

	ccs.missionSpawnCallback = CP_SpawnNewMissions;

	/* Collect and count Alien team definitions. */
	for (i = 0; i < csi.numTeamDefs; i++) {
		teamDef_t *td = &csi.teamDef[i];
		if (CHRSH_IsTeamDefAlien(td))
			ccs.alienTeams[ccs.numAliensTD++] = td;
	}
	/* Clear mapDef usage statistics */
	MapDef_ForeachSingleplayerCampaign(md) {
		md->timesAlreadyUsed = 0;
	}
}

#ifdef DEBUG
/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CP_DebugChangeCharacterStats_f (void)
{
	int j;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	E_Foreach(EMPL_SOLDIER, employee) {
		character_t *chr;

		if (!E_IsInBase(employee, base))
			continue;

		chr = &(employee->chr);
		assert(chr);

		for (j = 0; j < KILLED_NUM_TYPES; j++)
			chr->score.kills[j]++;
	}
	if (base->aircraftCurrent)
		CP_UpdateCharacterStats(base, base->aircraftCurrent);
}

#endif /* DEBUG */

/**
 * @brief Determines a random position on geoscape
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
	} while (noWater && MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL)));

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscape: Get random position on geoscape %.2f:%.2f\n", pos[0], pos[1]);
}

/**
 * @brief Determines a random position on geoscape that fulfills certain criteria given via parameters
 * @param[out] pos The position that will be overwritten with the random point fulfilling the criteria. pos[0] is within -180, +180. pos[1] within -90, +90.
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
	/* Number of points in latitude and longitude that will be tested. Therefore, the total number of position tried
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

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscapeWithParameters: New random coordinates for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
		pos[0], pos[1], num, hits);

	/** @todo add EQUAL_EPSILON here? */
	/* Make sure that position is within bounds */
	assert(pos[0] >= -180);
	assert(pos[0] <= 180);
	assert(pos[1] >= -90);
	assert(pos[1] <= 90);

	return qtrue;
}

const city_t * CP_GetCity (const char *id)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		if (Q_streq(city->id, id))
			return city;
	}

	return NULL;
}

int CP_GetSalaryAdministrative (const salary_t *salary)
{
	int i, costs;

	costs = salary->adminInitial;
	for (i = 0; i < MAX_EMPL; i++)
		costs += E_CountByType(i) * CP_GetSalaryAdminEmployee(salary, i);
	return costs;
}

int CP_GetSalaryBaseEmployee (const salary_t *salary, employeeType_t type)
{
	return salary->base[type];
}

int CP_GetSalaryAdminEmployee (const salary_t *salary, employeeType_t type)
{
	return salary->admin[type];
}

int CP_GetSalaryRankBonusEmployee (const salary_t *salary, employeeType_t type)
{
	return salary->rankBonus[type];
}

int CP_GetSalaryUpKeepBase (const salary_t *salary, const base_t *base)
{
	int cost = salary->baseUpkeep;	/* base cost */
	building_t *building = NULL;
	while ((building = B_GetNextBuilding(base, building))) {
		if (building->buildingStatus == B_STATUS_WORKING
		 || building->buildingStatus == B_STATUS_CONSTRUCTION_FINISHED)
			cost += building->varCosts;
	}
	return cost;
}

/** @todo remove me and move all the included stuff to proper places */
void CP_InitStartup (const cgame_import_t *import)
{
	cgi = import;
	cp_campaignPool = Mem_CreatePool("Client: Local (per game)");

	SAV_Init();

	/* commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CP_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
#endif

	cp_missiontest = Cvar_Get("cp_missiontest", "0", CVAR_DEVELOPER, "This will never stop the time on geoscape and print information about spawned missions");

	/* init subsystems */
	MS_MessageInit();

	MIS_InitStartup();
	UP_InitStartup();
	B_InitStartup();
	INS_InitStartup();
	RS_InitStartup();
	E_InitStartup();
	HOS_InitStartup();
	INT_InitStartup();
	AC_InitStartup();
	MAP_InitStartup();
	UFO_InitStartup();
	TR_InitStartup();
	AB_InitStartup();
	AIR_InitStartup();
	AIRFIGHT_InitStartup();
	NAT_InitStartup();
	TR_InitStartup();
	STATS_InitStartup();
	UR_InitStartup();
	AM_InitStartup();
}
