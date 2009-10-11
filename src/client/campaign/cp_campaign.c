/**
 * @file cp_campaign.c
 * @brief Single player campaign control.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../cl_game.h"
#include "../cl_team.h"
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "../menu/node/m_node_container.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_overlay.h"
#include "cp_campaign.h"
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
#include "cp_installation.h"
#include "cp_installation_callbacks.h"
#include "cp_alien_interest.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_nations.h"
#include "cp_time.h"
#include "cp_xvi.h"
#include "cp_aircraft_callbacks.h"
#include "cp_fightequip_callbacks.h"
#include "cp_produce_callbacks.h"
#include "cp_transfer_callbacks.h"
#include "cp_market.h"
#include "cp_employee_callbacks.h"
#include "cp_market_callbacks.h"
#include "cp_research_callbacks.h"
#include "cp_uforecovery_callbacks.h"

struct memPool_s *cp_campaignPool;		/**< reset on every game restart */
ccs_t ccs;
cvar_t *cp_campaign;
cvar_t *cp_start_employees;

typedef struct {
	int ucn;
	int HP;
	int STUN;
	int morale;

	chrScoreGlobal_t chrscore;
} updateCharacter_t;

/**
 * @brief Parses the character data which was send by G_EndGame using G_SendCharacterData
 * @param[in] msg The network buffer message. If this is NULL the character is updated, if this
 * is not NULL the data is stored in a temp buffer because the player can choose to retry
 * the mission and we have to catch this situation to not update the character data in this case.
 * @sa G_SendCharacterData
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa G_EndGame
 * @sa E_Save
 */
void CP_ParseCharacterData (struct dbuffer *msg)
{
	static updateCharacter_t updateCharacterArray[MAX_WHOLETEAM];
	static int num = 0;
	int i, j;
	character_t* chr;

	if (!msg) {
		for (i = 0; i < num; i++) {
			employee_t *employee = E_GetEmployeeFromChrUCN(updateCharacterArray[i].ucn);
			if (!employee) {
				Com_Printf("Warning: Could not get character with ucn: %i.\n", updateCharacterArray[i].ucn);
				continue;
			}
			chr = &employee->chr;
			chr->HP = updateCharacterArray[i].HP;
			chr->STUN = updateCharacterArray[i].STUN;
			chr->morale = updateCharacterArray[i].morale;

			/** Scores @sa inv_shared.h:chrScoreGlobal_t */
			memcpy(chr->score.experience, updateCharacterArray[i].chrscore.experience, sizeof(chr->score.experience));
			memcpy(chr->score.skills, updateCharacterArray[i].chrscore.skills, sizeof(chr->score.skills));
			memcpy(chr->score.kills, updateCharacterArray[i].chrscore.kills, sizeof(chr->score.kills));
			memcpy(chr->score.stuns, updateCharacterArray[i].chrscore.stuns, sizeof(chr->score.stuns));
			chr->score.assignedMissions = updateCharacterArray[i].chrscore.assignedMissions;
		}
		num = 0;
	} else {
		/* invalidate ucn in the array first */
		for (i = 0; i < MAX_WHOLETEAM; i++) {
			updateCharacterArray[i].ucn = -1;
		}
		/* number of soldiers */
		num = NET_ReadByte(msg);
		if (num > MAX_WHOLETEAM)
			Com_Error(ERR_DROP, "CP_ParseCharacterData: num exceeded MAX_WHOLETEAM\n");
		else if (num < 0)
			Com_Error(ERR_DROP, "CP_ParseCharacterData: NET_ReadShort error (%i)\n", num);

		for (i = 0; i < num; i++) {
			/* updateCharacter_t */
			updateCharacterArray[i].ucn = NET_ReadShort(msg);
			updateCharacterArray[i].HP = NET_ReadShort(msg);
			updateCharacterArray[i].STUN = NET_ReadByte(msg);
			updateCharacterArray[i].morale = NET_ReadByte(msg);

			/** Scores @sa inv_shared.h:chrScoreGlobal_t */
			for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
				updateCharacterArray[i].chrscore.experience[j] = NET_ReadLong(msg);
			for (j = 0; j < SKILL_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.skills[j] = NET_ReadByte(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.kills[j] = NET_ReadShort(msg);
			for (j = 0; j < KILLED_NUM_TYPES; j++)
				updateCharacterArray[i].chrscore.stuns[j] = NET_ReadShort(msg);
			updateCharacterArray[i].chrscore.assignedMissions = NET_ReadShort(msg);
		}
	}
}

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
			if (!LIST_ContainsString(md->ufos, Com_UFOCrashedTypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		} else {
			if (!LIST_ContainsString(md->ufos, Com_UFOTypeToShortName(mission->ufo->ufotype)))
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
qboolean CP_ChooseMap (mission_t *mission, const vec2_t pos, qboolean ufoCrashed)
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
				Com_Error(ERR_DROP, "Could not find mapdef ufocrash");
			return qtrue;
		} else {
			Com_Printf("CP_ChooseMap: Could not find map with required conditions:\n");
			Com_Printf("  ufo: %s -- pos: ", mission->ufo ? Com_UFOTypeToShortName(mission->ufo->ufotype) : "none");
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
 * @brief Function to handle the campaign end
 */
void CP_EndCampaign (qboolean won)
{
	GAME_SetMode(GAME_NONE);

	if (won)
		MN_InitStack("endgame", NULL, qtrue, qtrue);
	else
		MN_InitStack("lostgame", NULL, qtrue, qtrue);

	Com_Drop();
}

/**
 * @brief Checks whether the player has lost the campaign
 */
void CP_CheckLostCondition (void)
{
	qboolean endCampaign = qfalse;
	/* fraction of nation that can be below min happiness before the game is lost */
	const float nationBelowLimitPercentage = 0.5f;

	if (!endCampaign && ccs.credits < -ccs.curCampaign->negativeCreditsUntilLost) {
		MN_RegisterText(TEXT_STANDARD, _("You've gone too far into debt."));
		endCampaign = qtrue;
	}

	/** @todo Should we make the campaign lost when a player loses all his bases?
	 * until he has set up a base again, the aliens might have invaded the whole
	 * world ;) - i mean, removing the credits check here. */
	if (!ccs.numBases && ccs.credits < ccs.curCampaign->basecost - ccs.curCampaign->negativeCreditsUntilLost) {
		MN_RegisterText(TEXT_STANDARD, _("You've lost your bases and don't have enough money to build new ones."));
		endCampaign = qtrue;
	}
#if 0
	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > ccs.curCampaign->maxAllowedXVIRateUntilLost) {
			MN_RegisterText(TEXT_STANDARD, _("You have failed in your charter to protect Earth."
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
				const nation_t *nation = &ccs.nations[j];
				if (nation->stats[0].happiness < ccs.curCampaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= nationBelowLimitPercentage * ccs.numNations) {
				/* lost the game */
				MN_RegisterText(TEXT_STANDARD, _("Under your command, PHALANX operations have"
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
#endif
	if (endCampaign) {
		Cvar_SetValue("mission_uforecovered", 0);
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
void CL_HandleNationData (qboolean lost, int civiliansSurvived, int civiliansKilled, int aliensSurvived, int aliensKilled, mission_t * mis)
{
	int i, is_on_Earth = 0;
	const int civilianSum = civiliansKilled + civiliansSurvived;
	const float civilianRatio = civilianSum ? (float)civiliansSurvived / (float)civilianSum : 0.;
	const int alienSum = aliensKilled + aliensSurvived;
	const float alienRatio = alienSum ? (float)aliensKilled / (float)alienSum : 0.;
	const float performance = civilianRatio * 0.5 + alienRatio * 0.5;

	if (lost) {
		ccs.campaignStats.missionsLost++;
		ccs.civiliansKilled += civiliansKilled;
	} else
		ccs.campaignStats.missionsWon++;

	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = &ccs.nations[i];
		const float difficultyModifier = (float)ccs.curCampaign->difficulty * 0.01f;
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
 * @brief Check for missions that have a timeout defined
 */
static void CP_CheckMissionEnd (void)
{
	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (CP_CheckMissionLimitedInTime(mission) && Date_LaterThan(ccs.date, mission->finalDate)) {
			CP_MissionStageEnd(mission);
		}
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
	/** @todo Make this conversion a function as well (DateConvertSeconds?) See also CL_SecondConvert */
	dateLong->hour = date->sec / SECONDS_PER_HOUR;
	dateLong->min = (date->sec - dateLong->hour * SECONDS_PER_HOUR) / 60;
	dateLong->sec = date->sec - dateLong->hour * SECONDS_PER_HOUR - dateLong->min * 60;
}

/**
 * @brief sets market prices at start of the game
 * @sa CP_CampaignInit
 * @sa BS_Load (Market load function)
 * @param[in] load It this an attempt to init the market for a savegame?
 */
void CP_InitMarket (qboolean load)
{
	int i;
	campaign_t *campaign = ccs.curCampaign;

	/* find the relevant markets */
	campaign->marketDef = INV_GetEquipmentDefinitionByID(campaign->market);
	if (!campaign->marketDef)
		Com_Error(ERR_DROP, "CP_InitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
				campaign->id, campaign->market);
	campaign->asymptoticMarketDef = INV_GetEquipmentDefinitionByID(campaign->asymptoticMarket);
	if (!ccs.curCampaign->asymptoticMarketDef)
		Com_Error(ERR_DROP, "CP_InitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
				campaign->id, campaign->asymptoticMarket);

	/* the savegame loading process will get the following values from savefile */
	if (load)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if (ccs.eMarket.ask[i] == 0) {
			ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i] * BID_FACTOR);
		}

		if (!ccs.curCampaign->marketDef->num[i])
			continue;

		if (!RS_IsResearched_ptr(csi.ods[i].tech) && campaign->marketDef->num[i] > 0)
			Com_Error(ERR_DROP, "CP_InitMarket: Could not add item %s to the market - not marked as researched in campaign %s", csi.ods[i].id, campaign->id);
		else
			/* the other relevant values were already set above */
			ccs.eMarket.num[i] = campaign->marketDef->num[i];
	}
}

/**
 * @brief make number of items change every day.
 * @sa CL_CampaignRun
 * @sa daily called
 * @note This function makes items number on market slowly reach the asymptotic number of items defined in equipment.ufo
 * If an item has just been researched, it's not available on market until RESEARCH_LIMIT_DELAY days is reached.
 */
static void CL_CampaignRunMarket (void)
{
	int i;
	campaign_t *campaign = ccs.curCampaign;

	assert(campaign->marketDef);
	assert(campaign->asymptoticMarketDef);

	for (i = 0; i < csi.numODs; i++) {
		const technology_t *tech = csi.ods[i].tech;
		const float TYPICAL_TIME = 10.f;			/**< Number of days to reach the asymptotic number of items */
		const int RESEARCH_LIMIT_DELAY = 30;		/**< Numbers of days after end of research to wait in order to have
													 * items added on market */
		int asymptoticNumber;

		if (!tech)
			Com_Error(ERR_DROP, "No tech that provides '%s'\n", csi.ods[i].id);

		if (RS_IsResearched_ptr(tech) && (campaign->marketDef->num[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if items are researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = campaign->asymptoticMarketDef->num[i] ? campaign->asymptoticMarketDef->num[i] : campaign->marketDef->num[i];
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
 * @brief delay between actions that must be executed independently of time scale
 * @sa RADAR_CheckUFOSensored
 * @sa UFO_UpdateAlienInterestForAllBasesAndInstallations
 * @sa AB_UpdateStealthForAllBase
 */
const int DETECTION_INTERVAL = (SECONDS_PER_HOUR / 2);

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types MN_MAP and MN_3DMAP
 * @sa CP_NationHandleBudget
 * @sa B_UpdateBaseData
 * @sa CL_CampaignRunAircraft
 */
void CL_CampaignRun (void)
{
	const int currentinterval = (int)floor(ccs.date.sec) % DETECTION_INTERVAL;
	int checks, dt, i;
	int timeAlreadyFlied = 0;	/**< Time already flied by UFO or aircraft due to detection each detection interval */

	/* advance time */
	ccs.timer += cls.frametime * ccs.gameTimeScale;
	checks = currentinterval + (int)floor(ccs.timer);
	checks = (int)(checks / DETECTION_INTERVAL);
	dt = DETECTION_INTERVAL - currentinterval;

	UP_GetUnreadMails();

	/* Execute every actions that needs to be independent of time speed : every DETECTION_INTERVAL
	 *	- Run UFOs and craft at least every DETECTION_INTERVAL. If detection occurred, break.
	 *	- Check if any new mission is detected
	 *	- Update stealth value of phalanx bases and installations ; alien bases */
	for (i = 0; i < checks; i++) {
		qboolean detection;
		UFO_CampaignRunUFOs(dt);
		CL_CampaignRunAircraft(dt, qfalse);
		detection = CP_CheckNewMissionDetectedOnGeoscape();

		/* Update alien interest for bases */
		UFO_UpdateAlienInterestForAllBasesAndInstallations();

		/* Update how phalanx troop know alien bases */
		AB_UpdateStealthForAllBase();

		timeAlreadyFlied += dt;
		detection |= UFO_CampaignCheckEvents();
		if (detection) {
			ccs.timer = (i + 1) * DETECTION_INTERVAL - currentinterval;
			break;
		}
		dt = DETECTION_INTERVAL;
	}

	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int currenthour;
		int currentmin;
		dateLong_t date;

		dt = (int)floor(ccs.timer);
		currenthour = (int)floor(ccs.date.sec / SECONDS_PER_HOUR);
		currentmin = (int)floor(ccs.date.sec / SECONDS_PER_MINUTE);
		ccs.date.sec += dt;
		ccs.timer -= dt;

		/* compute minutely events  */
		/* (this may run multiple times if the time stepping is > 1 minute at a time) */
		while (currentmin < (int)floor(ccs.date.sec / SECONDS_PER_MINUTE)) {
			currentmin++;
			PR_ProductionRun();
		}

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(ccs.date.sec / SECONDS_PER_HOUR)) {
			currenthour++;
			RS_ResearchRun();
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
			CP_CheckCampaignEvents();
			CP_ReduceXVIEverywhere();
		}

		/* check for campaign events
		 * aircraft and UFO already moved during radar detection (see above),
		 * just make them move the missing part -- if any */
		UFO_CampaignRunUFOs(dt - timeAlreadyFlied);
		/* must be called even if dt = timeAlreadyFlied in order to update radar overlay */
		CL_CampaignRunAircraft(dt - timeAlreadyFlied, qtrue);
		UFO_CampaignCheckEvents();
		AIRFIGHT_CampaignRunBaseDefense(dt);
		CP_CheckMissionEnd();
		CP_CheckLostCondition();
		AIRFIGHT_CampaignRunProjectiles(dt);
		/* Check if there is a base attack mission */
		CP_CheckBaseAttacks_f();
		BDEF_AutoSelectTarget();

		/* set time cvars */
		CL_DateConvertLong(&ccs.date, &date);
		/* every first day of a month */
		if (date.day == 1 && ccs.fund && ccs.numBases) {
			CP_NationBackupMonthlyData();
			CP_NationHandleBudget();
			ccs.fund = qfalse;
		} else if (date.day > 1)
			ccs.fund = qtrue;

		CP_UpdateXVIMapButton();
		CL_UpdateTime();
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

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 */
static void CL_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i, j, costs = 0, sum = 0, totalfunds = 0;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	MN_RegisterText(TEXT_STATS_MISSION, pos);
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), ccs.campaignStats.missionsWon, ccs.campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_BASES, pos);
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
			ccs.campaignStats.basesBuild, ccs.numBases, ccs.campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_INSTALLATIONS, pos);
	for (i = 0; i < ccs.numInstallations; i++) {
		const installation_t *inst = &ccs.installations[i];
		Q_strcat(pos, va("%s\n", inst->name), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* nations */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_NATIONS, pos);
	for (i = 0; i < ccs.numNations; i++) {
		Q_strcat(pos, va(_("%s\t%s\n"), _(ccs.nations[i].name), NAT_GetHappinessString(&ccs.nations[i])), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		totalfunds += NAT_GetFunding(&ccs.nations[i], 0);
	}
	Q_strcat(pos, va(_("\nFunding this month:\t%d"), totalfunds), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* costs */
	for (i = 0; i < ccs.numEmployees[EMPL_SCIENTIST]; i++) {
		if (ccs.employees[EMPL_SCIENTIST][i].hired) {
			costs += SALARY_SCIENTIST_BASE + ccs.employees[EMPL_SCIENTIST][i].chr.score.rank * SALARY_SCIENTIST_RANKBONUS;
			hired[EMPL_SCIENTIST]++;
		}
	}
	for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++) {
		if (ccs.employees[EMPL_SOLDIER][i].hired) {
			costs += SALARY_SOLDIER_BASE + ccs.employees[EMPL_SOLDIER][i].chr.score.rank * SALARY_SOLDIER_RANKBONUS;
			hired[EMPL_SOLDIER]++;
		}
	}
	for (i = 0; i < ccs.numEmployees[EMPL_WORKER]; i++) {
		if (ccs.employees[EMPL_WORKER][i].hired) {
			costs += SALARY_WORKER_BASE + ccs.employees[EMPL_WORKER][i].chr.score.rank * SALARY_WORKER_RANKBONUS;
			hired[EMPL_WORKER]++;
		}
	}
	for (i = 0; i < ccs.numEmployees[EMPL_PILOT]; i++) {
		if (ccs.employees[EMPL_PILOT][i].hired) {
			costs += SALARY_PILOT_BASE + ccs.employees[EMPL_PILOT][i].chr.score.rank * SALARY_PILOT_RANKBONUS;
			hired[EMPL_PILOT]++;
		}
	}
	for (i = 0; i < ccs.numEmployees[EMPL_ROBOT]; i++) {
		if (ccs.employees[EMPL_ROBOT][i].hired) {
			costs += SALARY_ROBOT_BASE + ccs.employees[EMPL_ROBOT][i].chr.score.rank * SALARY_ROBOT_RANKBONUS;
			hired[EMPL_ROBOT]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_EMPLOYEES, pos);
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_COSTS, pos);
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
		for (j = 0; j < ccs.numBuildings[i]; j++) {
			costs += ccs.buildings[i][j].varCosts;
		}
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), base->name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = SALARY_ADMIN_INITIAL + ccs.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + ccs.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + ccs.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + ccs.numEmployees[EMPL_PILOT] * SALARY_ADMIN_PILOT + ccs.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
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
	MN_RegisterText(TEXT_GENERIC, pos);
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), ccs.curCampaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (CP_IsXVIResearched()) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			ccs.curCampaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

qboolean CP_LoadXML (mxml_node_t *parent)
{
	mxml_node_t *campaignNode, *n_ccs, *act_node, *bparam;
	int i;
	const char *name, *missionId;
	campaign_t *campaign;

	campaignNode = mxml_GetNode(parent, "campaign");
	if (!campaignNode) {
		Com_Printf("Did not find campaign entry in xml!\n");
		return qfalse;
	}
	if (!(name = mxml_GetString(campaignNode, "name"))) {
		Com_Printf("couldn't locate campaign name in savegame\n");
		return qfalse;
	}

	campaign = CL_GetCampaign(name);
	if (!campaign) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		return qfalse;
	}

	CP_CampaignInit(campaign, qtrue);
	/* init the map images and reset the map actions */
	MAP_Init();

	ccs.fund = mxml_GetBool(campaignNode, "fund", qfalse);
	cls.nextUniqueCharacterNumber = mxml_GetInt(campaignNode, "nextuniquecharacternumber", 0);

	/* read date */
	n_ccs = mxml_GetNode(campaignNode, "ccs");
	ccs.date.day = mxml_GetInt(n_ccs, "dateday", 0);
	ccs.date.sec = mxml_GetInt(n_ccs, "datesec", 0);

	Com_DPrintf(DEBUG_CLIENT, "CP_LoadXML: Getting position\n");
	/* read map view */
	ccs.center[0] = mxml_GetFloat(n_ccs, "center0", 0.0);
	ccs.center[1] = mxml_GetFloat(n_ccs, "center1", 0.0);
	ccs.angles[0] = mxml_GetFloat(n_ccs, "angles0", 0.0);
	ccs.angles[1] = mxml_GetFloat(n_ccs, "angles1", 0.0);
	ccs.zoom = mxml_GetFloat(n_ccs, "zoom", 0.0);

	/* restore the overlay.
	* do not use Cvar_SetValue, because this function check if value->string are equal to skip calculation
	* and we never set r_geoscape_overlay->string in game: r_geoscape_overlay won't be updated if the loaded
	* value is 0 (and that's a problem if you're loading a game when r_geoscape_overlay is set to another value */
	r_geoscape_overlay->integer = mxml_GetInt(campaignNode, "r_geoscape_overlay", 0);

	radarOverlayWasSet = mxml_GetBool(campaignNode, "radaroverlaywasset", qfalse);

	CL_UpdateCredits(mxml_GetLong(n_ccs, "credits", 0));

	/* read credits */
	/* store interest values */
	ccs.lastInterestIncreaseDelay = mxml_GetInt(n_ccs, "lastinterestincreasedelay", 0);
	ccs.lastMissionSpawnedDelay = mxml_GetInt(n_ccs, "lastmissionspawneddelay", 0);

	ccs.overallInterest = mxml_GetInt(n_ccs, "overall", 0);
	/*mxml_GetInt(interest, "count", INTERESTCATEGORY_MAX);*/

	for (i = 0, act_node = mxml_GetNode(n_ccs, "interest"); i < INTERESTCATEGORY_MAX && act_node;
			i++, (act_node = mxml_GetNextNode(act_node, n_ccs, "interest"))) {
		ccs.interest[i]= mxml_GetInt(act_node, "val", 0);
	}

	/* read other campaign data */
	ccs.civiliansKilled = mxml_GetInt(n_ccs, "civilianskilled", 0);
	ccs.aliensKilled = mxml_GetInt(n_ccs, "alienskilled", 0);

 	ccs.XVIShowMap = mxml_GetBool(n_ccs, "xvishowmap", qfalse);

	CP_UpdateXVIMapButton();

	/* read missions */
	for (act_node = mxml_GetNode(campaignNode, "mission"), i = 0; act_node;
			act_node = mxml_GetNextNode(act_node, campaignNode, "mission"), i++) {
		mission_t mission;
		int ufoIdx;
		qboolean defaultAssigned = qfalse;

		memset(&mission, 0, sizeof(mission));
		name = mxml_GetString(act_node, "mapDef_id");
		if (name && name[0] != '\0') {
			mission.mapDef = Com_GetMapDefinitionByID(name);
			if (!mission.mapDef) {
				Com_Printf("......mapdef \"%s\" doesn't exist.\n", name);
				return qfalse;
			}
			mission.mapDef->timesAlreadyUsed = mxml_GetInt(act_node, "mapdeftimes", 0);
		} else
			mission.mapDef = NULL;

		Q_strncpyz(mission.id, mxml_GetString(act_node, "id"), sizeof(mission.id));
		mission.active = mxml_GetBool(act_node, "active", qfalse);
		Q_strncpyz(mission.onwin, mxml_GetString(act_node, "onwin"), sizeof(mission.onwin));
		Q_strncpyz(mission.onlose, mxml_GetString(act_node, "onlose"), sizeof(mission.onlose));
		mission.category = mxml_GetInt(act_node, "category", 0);
		mission.stage = mxml_GetShort(act_node, "stage", 0);

		mission.initialOverallInterest = mxml_GetInt(act_node, "initialoverallinterest", 0);
		mission.initialIndividualInterest = mxml_GetInt(act_node, "initialindividualinterest", 0);

		switch (mission.category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission.stage == STAGE_MISSION_GOTO || mission.stage == STAGE_BASE_ATTACK) {
				/* Load IDX of base under attack */
				const int baseidx = mxml_GetInt(act_node, "baseindex", BYTES_NONE);
				if (baseidx != BYTES_NONE) {
					base_t *base = B_GetBaseByIDX(baseidx);
					assert(base);
					if (mission.stage == STAGE_BASE_ATTACK && base->baseStatus != BASE_UNDER_ATTACK)
						Com_Printf("......warning: base %i (%s) is supposedly under attack but base status doesn't match!\n", baseidx, base->name);
					mission.data = (void *) base;
				} else
					Com_Printf("......warning: Missing BaseIndex\n");
			}
			break;
		case INTERESTCATEGORY_INTERCEPT:
			if (mission.stage == STAGE_MISSION_GOTO || mission.stage == STAGE_INTERCEPT) {
				const int installationIdx = mxml_GetInt(act_node, "installationindex", BYTES_NONE);
				if (installationIdx != BYTES_NONE) {
					installation_t *installation = INS_GetInstallationByIDX(installationIdx);
					if (installation)
						mission.data = (void *) installation;
					else {
						Com_Printf("Mission on non-existent installation\n");
						return qfalse;
					}
				}
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission.stage >= STAGE_MISSION_GOTO) {
				int baseIDX = mxml_GetInt(act_node, "alienbaseindex", BYTES_NONE);
				/* don't check baseidx value here: alien bases are not loaded yet */
				if (baseIDX != BYTES_NONE) {
					alienBase_t *alienBase = AB_GetBase(baseIDX, qfalse);
					mission.data = (void *) alienBase;
				}
				if (!mission.data && !CP_BasemissionIsSubvertingGovernmentMission(&mission)) {
					Com_Printf("Error while loading Alien Base mission (mission %i, baseidx: %i, category: %i, stage: %i)\n",
							i, baseIDX, mission.category, mission.stage);
					return qfalse;
				}
			}
			break;
		default:
			break;
		}

		Q_strncpyz(mission.location, mxml_GetString(act_node, "location"), sizeof(mission.location));

		mission.startDate.day = mxml_GetInt(act_node, "startdate.day", 0);
		mission.startDate.sec = mxml_GetInt(act_node, "startdate.sec", 0);
		mission.finalDate.day = mxml_GetInt(act_node, "finaldate.day", 0);
		mission.finalDate.sec = mxml_GetInt(act_node, "finaldate.sec", 0);

		mxml_GetPos2(act_node, "pos", mission.pos);

		ufoIdx = mxml_GetInt(act_node, "ufo", -1);
		if (ufoIdx <= -1)
			mission.ufo = NULL;
		else
			mission.ufo = ccs.ufos + ufoIdx;

		mission.crashed = mxml_GetBool(act_node, "crashed", qfalse);
		mission.onGeoscape = mxml_GetBool(act_node, "ongeoscape", qfalse);

		if (mission.pos[0] > 0.001 || mission.pos[1] > 0.001)
			defaultAssigned = qtrue;
		mission.posAssigned = mxml_GetBool(act_node, "posassigned", defaultAssigned);
		/* Add mission to global array */
		LIST_Add(&ccs.missions, (byte*) &mission, sizeof(mission));
	}

	Com_DPrintf(DEBUG_CLIENT, "CP_LoadXML: Reading Battle Parameters\n");
	/* read ccs.battleParameters */
	memset(&ccs.battleParameters, 0, sizeof(ccs.battleParameters));

	if ((bparam = mxml_GetNode(n_ccs, "battleparameter"))) {
		int j;
		ccs.battleParameters.mission = CP_GetMissionByID(mxml_GetString(bparam, "mission.id"));
		name = mxml_GetString(bparam, "aliencategories.id");

		/* get corresponding category */
		for (i = 0; i < ccs.numAlienCategories; i++)
			if (!strncmp(name, ccs.alienCategories[i].id, sizeof(ccs.alienCategories[i].id)))
				break;

		if (i >= ccs.numAlienCategories) {
			Com_Printf("CP_LoadXML: alien category def \"%s\" doesn't exist\n", name);
			return qfalse;
		}

		j = mxml_GetInt(bparam, "alienteamgroupidx", 0);
		if (j >= INTERESTCATEGORY_MAX) {
			Com_Printf("CP_LoadXML: Undefined alien team (category '%s', group '%i')\n", name, j);
			return qfalse;
		}
		ccs.battleParameters.alienTeamGroup = &ccs.alienCategories[i].alienTeamGroups[j];
		name = mxml_GetString(bparam, "param");
		if (name && name[0] != '\0')
			ccs.battleParameters.param = Mem_PoolStrDup(name, cp_campaignPool, 0);
		else
			ccs.battleParameters.param = NULL;

		Q_strncpyz(ccs.battleParameters.alienEquipment, mxml_GetString(bparam, "alienequipment"), sizeof(ccs.battleParameters.alienEquipment));
		Q_strncpyz(ccs.battleParameters.civTeam, mxml_GetString(bparam, "civteam"), sizeof(ccs.battleParameters.civTeam));

		ccs.battleParameters.day = mxml_GetBool(bparam, "day", qfalse);
		ccs.battleParameters.ugv = mxml_GetInt(bparam, "ugv", 0);
		ccs.battleParameters.aliens = mxml_GetInt(bparam, "aliens", 0);
		ccs.battleParameters.civilians = mxml_GetInt(bparam, "civilians", 0);
	}

	missionId = mxml_GetString(n_ccs, "selectedmission");
	if (missionId && missionId[0] != '\0')
		ccs.selectedMission = CP_GetMissionByID(missionId);
	else
		ccs.selectedMission = NULL;

	/* and now fix the mission pointers for e.g. ufocrash sites
	* this is needed because the base load function which loads the aircraft
	* doesn't know anything (at that stage) about the new missions that were
	* add in this load function */
	for (i = 0; i < MAX_BASES; i++) {
		int j;
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		for (j = 0; j < base->numAircraftInBase; j++) {
			if (base->aircraft[j].status == AIR_MISSION) {
				assert(base->aircraft[j].missionID);
				base->aircraft[j].mission = CP_GetMissionByID(base->aircraft[j].missionID);

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

	mxmlDelete(campaignNode);
	return qtrue;
}

qboolean CP_SaveXML (mxml_node_t *parent)
{
	mxml_node_t * campaign;
	mxml_node_t * structure_ccs;
	int i;
	const linkedList_t *list = ccs.missions;

	campaign = mxml_AddNode(parent, "campaign");

	mxml_AddString(campaign, "name", ccs.curCampaign->id);
	mxml_AddShort(campaign, "fund", ccs.fund);
	mxml_AddShort(campaign, "nextuniquecharacternumber", cls.nextUniqueCharacterNumber);

	structure_ccs = mxml_AddNode(campaign, "ccs");
	mxml_AddInt(structure_ccs, "dateday", ccs.date.day);
	mxml_AddInt(structure_ccs, "datesec", ccs.date.sec);

	mxml_AddFloat(structure_ccs, "center0", ccs.center[0]);
	mxml_AddFloat(structure_ccs, "center1", ccs.center[1]);
	mxml_AddFloat(structure_ccs, "angles0", ccs.angles[0]);
	mxml_AddFloat(structure_ccs, "angles1", ccs.angles[1]);
	mxml_AddFloat(structure_ccs, "zoom", ccs.zoom);

	mxml_AddShort(campaign, "r_geoscape_overlay", r_geoscape_overlay->integer);
	mxml_AddBool(campaign, "radaroverlaywasset", radarOverlayWasSet);

	mxml_AddLong(structure_ccs, "credits", ccs.credits);

	mxml_AddShort(structure_ccs, "lastinterestincreasedelay", ccs.lastInterestIncreaseDelay);
	mxml_AddShort(structure_ccs, "lastmissionspawneddelay", ccs.lastMissionSpawnedDelay);

	mxml_AddShort(structure_ccs, "overall", ccs.overallInterest);
	/*mxml_AddInt(substructure, "count", INTERESTCATEGORY_MAX);*/
	for (i = 0; i < INTERESTCATEGORY_MAX; i++) {
		mxml_node_t * substructure = mxml_AddNode(structure_ccs, "interest");
		mxml_AddShort(substructure, "val", ccs.interest[i]);
	}

	/* store other campaign data */
	mxml_AddInt(structure_ccs, "civilianskilled", ccs.civiliansKilled);
	mxml_AddInt(structure_ccs, "alienskilled", ccs.aliensKilled);
	mxml_AddBool(structure_ccs, "xvishowmap", ccs.XVIShowMap);

	/* store missions */
	mxml_AddShort(campaign, "missioncount", CP_CountMission());
	for (; list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;
		mxml_node_t * missions = mxml_AddNode(campaign, "mission");
		if (mission->mapDef) {
			mxml_AddString(missions, "mapDef_id", mission->mapDef->id);
			mxml_AddInt(missions, "mapdeftimes", mission->mapDef->timesAlreadyUsed);
		}
		mxml_AddString(missions, "id", mission->id);
		mxml_AddBool(missions, "active", mission->active);
		mxml_AddBool(missions, "posassigned", mission->posAssigned);
		mxml_AddBool(missions, "crashed", mission->crashed);
		mxml_AddString(missions, "onwin", mission->onwin);
		mxml_AddString(missions, "onlose", mission->onlose);
		mxml_AddShort(missions, "category", mission->category);
		mxml_AddShort(missions, "stage", mission->stage);
		switch (mission->category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission->stage == STAGE_MISSION_GOTO || mission->stage == STAGE_BASE_ATTACK) {
				const base_t *base;
				/* save IDX of base under attack if required */
				base = (base_t*)mission->data;
				assert(base);
				mxml_AddShort(missions, "baseindex", base->idx);
			}
			break;
		case INTERESTCATEGORY_INTERCEPT:
			if (mission->stage == STAGE_MISSION_GOTO || mission->stage == STAGE_INTERCEPT) {
				const installation_t *installation = (installation_t*)mission->data;
				if (installation)
					mxml_AddShort(missions, "installationindex", installation->idx);
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission->stage >= STAGE_MISSION_GOTO) {
				/* save IDX of alien base if required */
				const alienBase_t *base = (alienBase_t*)mission->data;
				/* there may be no base is the mission is a subverting government */
				if (base)
					mxml_AddShort(missions, "alienbaseindex", base->idx);
			}
			break;
		default:
			break;
		}
		mxml_AddString(missions, "location", mission->location);
		mxml_AddShort(missions, "initialoverallinterest", mission->initialOverallInterest);
		mxml_AddShort(missions, "initialindividualinterest", mission->initialIndividualInterest);
		mxml_AddLong(missions, "startdate.day", mission->startDate.day);
		mxml_AddLong(missions, "startdate.sec", mission->startDate.sec);
		mxml_AddLong(missions, "finaldate.day", mission->finalDate.day);
		mxml_AddLong(missions, "finaldate.sec", mission->finalDate.sec);
		mxml_AddPos2(missions, "pos", mission->pos);
		if (mission->ufo)
			mxml_AddShort(missions, "ufo", mission->ufo - ccs.ufos);
		mxml_AddBool(missions, "ongeoscape", mission->onGeoscape);
	}

	/* store ccs.battleParameters */
	if (ccs.battleParameters.mission) {
		mxml_node_t *battleparam = mxml_AddNode(structure_ccs, "battleparameter");
		mxml_AddString(battleparam, "mission.id", ccs.battleParameters.mission->id);
		mxml_AddString(battleparam, "aliencategories.id", ccs.alienCategories[ccs.battleParameters.alienTeamGroup->categoryIdx].id);
		mxml_AddShort(battleparam, "alienteamgroupidx", ccs.battleParameters.alienTeamGroup->idx);
		mxml_AddString(battleparam, "param", (ccs.battleParameters.param ? ccs.battleParameters.param : ""));
		mxml_AddString(battleparam, "alienequipment", ccs.battleParameters.alienEquipment);
		mxml_AddString(battleparam, "civteam", ccs.battleParameters.civTeam);
		mxml_AddBool(battleparam, "day", ccs.battleParameters.day);
		mxml_AddInt(battleparam, "ugv", ccs.battleParameters.ugv);
		mxml_AddInt(battleparam, "aliens", ccs.battleParameters.aliens);
		mxml_AddInt(battleparam, "civilians", ccs.battleParameters.civilians);
	}
	/* stores the select mission on geoscape */
	mxml_AddString(structure_ccs, "selectedmission", ccs.selectedMission ? ccs.selectedMission->id : "");
	return qtrue;
}

qboolean STATS_SaveXML (mxml_node_t *parent)
{
	mxml_node_t * stats;

	stats = mxml_AddNode(parent, "stats");

	mxml_AddInt(stats, "missionswon", ccs.campaignStats.missionsWon);
	mxml_AddInt(stats, "missionslost", ccs.campaignStats.missionsLost);
	mxml_AddInt(stats, "basesbuild", ccs.campaignStats.basesBuild);
	mxml_AddInt(stats, "basesattacked", ccs.campaignStats.basesAttacked);
	mxml_AddInt(stats, "interceptions", ccs.campaignStats.interceptions);
	mxml_AddInt(stats, "soldierslost", ccs.campaignStats.soldiersLost);
	mxml_AddInt(stats, "soldiersnew", ccs.campaignStats.soldiersNew);
	mxml_AddInt(stats, "killedaliens", ccs.campaignStats.killedAliens);
	mxml_AddInt(stats, "rescuedcivilians", ccs.campaignStats.rescuedCivilians);
	mxml_AddInt(stats, "researchedtechnologies", ccs.campaignStats.researchedTechnologies);
	mxml_AddInt(stats, "moneyinterceptions", ccs.campaignStats.moneyInterceptions);
	mxml_AddInt(stats, "moneybases", ccs.campaignStats.moneyBases);
	mxml_AddInt(stats, "moneyresearch", ccs.campaignStats.moneyResearch);
	mxml_AddInt(stats, "moneyweapons", ccs.campaignStats.moneyWeapons);
	mxml_AddInt(stats, "ufosdetected", ccs.campaignStats.ufosDetected);
	return qtrue;
}

qboolean STATS_LoadXML (mxml_node_t *tree)
{
	mxml_node_t * stats;

	stats = mxml_GetNode(tree, "stats");
	if (!stats) {
		Com_Printf("Did not find stats entry in xml!\n");
		return qfalse;
	}
	ccs.campaignStats.missionsWon = mxml_GetInt(stats, "missionswon", 0);
	ccs.campaignStats.missionsLost = mxml_GetInt(stats, "missionslost", 0);
	ccs.campaignStats.basesBuild = mxml_GetInt(stats, "basesbuild", 0);
	ccs.campaignStats.basesAttacked = mxml_GetInt(stats, "basesattacked", 0);
	ccs.campaignStats.interceptions = mxml_GetInt(stats, "interceptions", 0);
	ccs.campaignStats.soldiersLost = mxml_GetInt(stats, "soldierslost", 0);
	ccs.campaignStats.soldiersNew = mxml_GetInt(stats, "soldiersnew", 0);
	ccs.campaignStats.killedAliens = mxml_GetInt(stats, "killedaliens", 0);
	ccs.campaignStats.rescuedCivilians = mxml_GetInt(stats, "rescuedcivilians", 0);
	ccs.campaignStats.researchedTechnologies = mxml_GetInt(stats, "researchedtechnologies", 0);
	ccs.campaignStats.moneyInterceptions = mxml_GetInt(stats, "moneyinterceptions", 0);
	ccs.campaignStats.moneyBases = mxml_GetInt(stats, "moneybases", 0);
	ccs.campaignStats.moneyResearch = mxml_GetInt(stats, "moneyresearch", 0);
	ccs.campaignStats.moneyWeapons = mxml_GetInt(stats, "moneyweapons", 0);
	ccs.campaignStats.ufosDetected = mxml_GetInt(stats, "ufosdetected", 0);
	/* freeing the memory below this node */
	mxmlDelete(stats);
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
	aircraft_t *aircraft;
	base_t *base;

	if (!ccs.missionaircraft)
		return;

	aircraft = ccs.missionaircraft;
	base = CP_GetMissionBase();

	if (!ccs.selectedMission)
		ccs.selectedMission = aircraft->mission;

	if (!ccs.selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "No ccs.selectedMission\n");
		return;
	}

	mis = ccs.selectedMission;

	/* Before we start, we should clear the missionResults array. */
	memset(&ccs.missionResults, 0, sizeof(ccs.missionResults));

	/* Various sanity checks. */
	if (!mis->active) {
		Com_DPrintf(DEBUG_CLIENT, "CP_StartSelectedMission: Dropship not near landing zone: mis->active: %i\n", mis->active);
		return;
	}
	if (aircraft->teamSize <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "CP_StartSelectedMission: No team in dropship. teamSize=%i\n", aircraft->teamSize);
		return;
	}

	/* if we retry a mission we have to drop from the current game before */
	SV_Shutdown("Server quit.", qfalse);
	CL_Disconnect();

	CP_CreateBattleParameters(mis);
	CP_SetMissionVars(mis);
	/* Set the states of mission Cvars to proper values. */
	Cvar_SetValue("mission_uforecovered", 0);

	/* manage inventory */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CL_CleanTempInventory(base);
	CL_CleanupAircraftCrew(aircraft, &ccs.eMission);

	CP_StartMissionMap(mis);
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
			winProbability = exp((0.5 - .15 * ccs.curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		case INTERESTCATEGORY_XVI:
			/* not that hard to win this, they want to spread xvi - no real terror mission */
			/** @todo change the formular here to reflect the above comment */
			winProbability = exp((0.5 - .15 * ccs.curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		default:
			/** @todo change the formular here to reflect the above comments */
			winProbability = exp((0.5 - .15 * ccs.curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
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
						const rank_t *rank = CL_GetRankByIdx(score->rank);
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
					const rank_t *rank = CL_GetRankByIdx(score->rank);
					/* @sa CHRSH_CharGetMaxExperiencePerMission */
					if (score->experience[SKILL_CLOSE] > 70) { /** @todo fix this value */
						increaseWinProbability *= rank->factor;
					}
				}
				listPos = listPos->next;
			}

			winProbability = exp((0.5 - .15 * ccs.curCampaign->difficulty) * numSoldiers - ccs.battleParameters.aliens);
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
 * @brief Collect alien bodies for auto missions
 * @note collect all aliens as dead ones
 */
static void CL_AutoMissionAlienCollect (aircraft_t *aircraft)
{
	int i;
	int aliens = ccs.battleParameters.aliens;

	if (!aliens)
		return;

	MS_AddNewMessage(_("Notice"), _("Collected dead alien bodies"), qfalse, MSG_STANDARD, NULL);

	while (aliens > 0) {
		for (i = 0; i < ccs.battleParameters.alienTeamGroup->numAlienTeams; i++) {
			const teamDef_t *teamDef = ccs.battleParameters.alienTeamGroup->alienTeams[i];
			const int addDeadAlienAmount = aliens > 1 ? rand() % aliens : aliens;
			if (!addDeadAlienAmount)
				continue;
			assert(i < MAX_CARGO);
			assert(ccs.battleParameters.alienTeamGroup->alienTeams[i]);
			AL_AddAlienTypeToAircraftCargo(aircraft, teamDef, addDeadAlienAmount, qtrue);
			aliens -= addDeadAlienAmount;
			if (!aliens)
				break;
		}
	}
}

/**
 * @brief Handles the auto mission for none storyrelated missions
 * @sa GAME_CP_MissionAutoGo_f
 * @sa CL_Drop
 * @sa CP_MissionEnd
 * @sa AL_CollectingAliens
 */
void CL_GameAutoGo (mission_t *mis)
{
	qboolean won;
	float winProbability;
	/* maybe ccs.interceptAircraft is changed in some functions we call here
	 * so store a local pointer to guarantee that we access the right aircraft
	 * note that ccs.interceptAircraft is a fake aircraft for base attack missions */
	aircraft_t *aircraft = ccs.interceptAircraft;

	assert(mis);

	CP_CreateBattleParameters(mis);

	if (!ccs.interceptAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo: No update after automission\n");
		return;
	}

	if (mis->stage != STAGE_BASE_ATTACK) {
		if (!mis->active) {
			MS_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mis->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("cp_mission_autogo_available", "0");
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
		CL_HandleNationData(!won, ccs.battleParameters.civilians, 0, 0, ccs.battleParameters.aliens, ccs.selectedMission);
		CP_CheckLostCondition();
	} else {
		CL_HandleNationData(!won, 0, ccs.battleParameters.civilians, ccs.battleParameters.aliens, 0, ccs.selectedMission);
		CP_CheckLostCondition();
	}

	CL_AutoMissionAlienCollect(aircraft);

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(ccs.selectedMission, won);

	/* if a UFO has been recovered, send it to a base */
	if (won && ccs.missionResults.recovery) {
		int counts[MAX_MISSIONRESULTCOUNT];
		memset(counts,0,sizeof(counts));
		/** @todo set other counts, use the counts above */
		counts[MRC_ALIENS_KILLED] = ccs.battleParameters.aliens;
		counts[MRC_CIVILIAN_SURVIVOR] = ccs.battleParameters.civilians;
		CP_InitMissionResults(counts, won);
		MN_PushMenu("won", NULL);
	}

	/* handle base attack mission */
	if (ccs.selectedMission->stage == STAGE_BASE_ATTACK) {
		const base_t *base = (base_t*)ccs.selectedMission->data;
		assert(base);

		if (won) {
			/* fake an aircraft return to collect goods and aliens */
			B_AircraftReturnedToHomeBase(ccs.interceptAircraft);

			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Defence of base: %s successful!"), base->name);
			MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(ccs.selectedMission);
			/** @todo @sa AIRFIGHT_ProjectileHitsBase notes */
		} else {
			CP_BaseAttackMissionDestroyBase(ccs.selectedMission);
		}
	} else {
		AIR_AircraftReturnToBase(ccs.interceptAircraft);
		if (won)
			CP_MissionIsOver(ccs.selectedMission);
	}

	if (won)
		MS_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	else
		MS_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);

	MAP_ResetAction();
}

/**
 * Updates mission result menu text with appropriate values
 * @param resultCounts result counts
 */
void CP_InitMissionResults (int resultCounts[MAX_MISSIONRESULTCOUNT], qboolean won)
{
	static char resultText[MAX_SMALLMENUTEXTLEN];
	/* init result text */
	MN_RegisterText(TEXT_STANDARD, resultText);

	/* needs to be cleared and then append to it */
	Com_sprintf(resultText, sizeof(resultText), _("Aliens killed\t%i\n"), resultCounts[MRC_ALIENS_KILLED]);
	Q_strcat(resultText, va(_("Aliens captured\t%i\n"), resultCounts[MRC_ALIENS_STUNNED]), sizeof(resultText));
	Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), resultCounts[MRC_ALIENS_SURVIVOR]), sizeof(resultText));
	/* team stats */
	Q_strcat(resultText, va(_("PHALANX soldiers killed by Aliens\t%i\n"), resultCounts[MRC_PHALANX_KILLED]), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX soldiers missing in action\t%i\n"), resultCounts[MRC_PHALANX_MIA]), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX friendly fire losses\t%i\n"), resultCounts[MRC_PHALANX_FF_KILLED]), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX survivors\t%i\n\n"), resultCounts[MRC_PHALANX_SURVIVOR]), sizeof(resultText));

	Q_strcat(resultText, va(_("Civilians killed by Aliens\t%i\n"), resultCounts[MRC_CIVILIAN_KILLED]), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians killed by friendly fire\t%i\n"), resultCounts[MRC_CIVILIAN_FF_KILLED]), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians saved\t%i\n\n"), resultCounts[MRC_CIVILIAN_SURVIVOR]), sizeof(resultText));
	Q_strcat(resultText, va(_("Gathered items (types/all)\t%i/%i\n"), resultCounts[MRC_ITEM_GATHEREDTYPES],
			resultCounts[MRC_ITEM_GATHEREDAMOUNT]), sizeof(resultText));

	if (won && ccs.missionResults.recovery)
		Q_strcat(resultText, UFO_MissionResultToString(), sizeof(resultText));
}

/**
 * @brief Update employeer stats after mission.
 * @param[in] won Determines whether we won the mission or not.
 * @note Soldier promotion is being done here.
 * @sa GAME_CP_Results_f
 */
void CL_UpdateCharacterStats (const base_t *base, int won, const aircraft_t *aircraft)
{
	int i, j;

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: base: '%s' numTeamList: %i\n",
		base->name, cl.numTeamList);

	assert(aircraft);

	/* only soldiers have stats and ranks, ugvs not */
	for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++)
		if (AIR_IsEmployeeInAircraft(&ccs.employees[EMPL_SOLDIER][i], aircraft)) {
			character_t *chr = &ccs.employees[EMPL_SOLDIER][i].chr;
			assert(chr);
			if (!ccs.employees[EMPL_SOLDIER][i].hired) {
				Com_Error(ERR_DROP, "Employee %s is reported as being on the aircraft (%s), but he is not hired (%i/%i)",
					chr->name, aircraft->id, i, ccs.numEmployees[EMPL_SOLDIER]);
			}
			assert(ccs.employees[EMPL_SOLDIER][i].baseHired == aircraft->homebase);

			Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: searching for soldier: %i\n", i);

			/* Remember the number of assigned mission for this character. */
			chr->score.assignedMissions++;

			/** @todo use chrScore_t to determine negative influence on soldier here,
			 * like killing too many civilians and teammates can lead to unhire and disband
			 * such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			 * and do a promotion. */
			if (ccs.numRanks >= 2) {
				for (j = ccs.numRanks - 1; j > chr->score.rank; j--) {
					const rank_t *rank = CL_GetRankByIdx(j);
					/** @todo (Zenerka 20080301) extend ranks and change calculations here. */
					if (rank->type == EMPL_SOLDIER && (chr->score.skills[ABILITY_MIND] >= rank->mind)
					 && (chr->score.kills[KILLED_ALIENS] >= rank->killedEnemies)
					 && ((chr->score.kills[KILLED_CIVILIANS] + chr->score.kills[KILLED_TEAM]) <= rank->killedOthers)) {
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= ccs.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		objDef_t *obj = &csi.ods[i];
		if (!obj->weapon && !obj->numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, obj, 1, qfalse, qtrue);
		if (base->storage.num[i] > 0) {
			technology_t *tech = obj->tech;
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

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= ccs.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];
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
	nation_t *nation = &ccs.nations[0];	/**< This is just a debuging function, nation does not matter */

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
		/* Create a ares ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUGVByID("ugv_ares_w"));

	for (j = 0; j < 5; j++)
		/* Create a phoenix ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUGVByID("ugv_phoenix"));
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnation
 */
static void CL_NationList_f (void)
{
	int i;
	for (i = 0; i < ccs.numNations; i++) {
		Com_Printf("Nation ID: %s\n", ccs.nations[i].id);
		Com_Printf("...max-funding %i c\n", ccs.nations[i].maxFunding);
		Com_Printf("...alienFriendly %0.2f\n", ccs.nations[i].stats[0].alienFriendly);
		Com_Printf("...happiness %0.2f\n", ccs.nations[i].stats[0].happiness);
		Com_Printf("...xviInfection %i\n", ccs.nations[i].stats[0].xviInfection);
		Com_Printf("...max-soldiers %i\n", ccs.nations[i].maxSoldiers);
		Com_Printf("...max-scientists %i\n", ccs.nations[i].maxScientists);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", ccs.nations[i].color[0], ccs.nations[i].color[1], ccs.nations[i].color[2], ccs.nations[i].color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", ccs.nations[i].pos[0], ccs.nations[i].pos[1]);
	}
}
#endif

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"update_base_radar_coverage", RADAR_UpdateBaseRadarCoverage_f, "Update base radar coverage"},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"stats_update", CL_StatsUpdate_f, NULL},
	{"game_go", CP_StartSelectedMission, NULL},
	{"game_timestop", CL_GameTimeStop, NULL},
	{"game_timeslow", CL_GameTimeSlow, NULL},
	{"game_timefast", CL_GameTimeFast, NULL},
	{"game_settimeid", CL_SetGameTime_f, NULL},
	{"mn_mapaction_reset", MAP_ResetAction, NULL},
	{"map_center", MAP_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"map_zoom", MAP_Zoom_f, NULL},
	{"map_scroll", MAP_Scroll_f, NULL},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
#ifdef DEBUG
	{"debug_listaircraftsample", AIR_ListAircraftSamples_f, "Show aircraft parameter on game console"},
	{"debug_listnation", CL_NationList_f, "List all nations on the game console"},
	{"debug_listaircraft", AIR_ListAircraft_f, "Debug function to list all aircraft in all bases"},
	{"debug_listaircraftidx", AIR_ListCraftIndexes_f, "Debug function to list local/global aircraft indexes"},
	{"debug_fullcredits", CL_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_addemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
	{"debug_additems", CL_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_listitem", CL_DebugShowItems_f, "Debug function to show all items in base storage"},
	{"debug_changexvi", CP_DebugChangeXVI_f, "Debug function to spread or reduce xvi at a given location"},
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
	AIR_InitCallbacks();
	B_InitCallbacks();
	BDEF_InitCallbacks();
	BS_InitCallbacks();
	CP_TEAM_InitCallbacks();
	E_InitCallbacks();
	HOS_InitCallbacks();
	INS_InitCallbacks();
	TR_InitCallbacks();
	PR_InitCallbacks();
	RS_InitCallbacks();
	UR_InitCallbacks();
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
	AIR_ShutdownCallbacks();
	B_ShutdownCallbacks();
	BDEF_ShutdownCallbacks();
	BS_ShutdownCallbacks();
	CP_TEAM_ShutdownCallbacks();
	E_ShutdownCallbacks();
	HOS_ShutdownCallbacks();
	INS_ShutdownCallbacks();
	TR_ShutdownCallbacks();
	PR_ShutdownCallbacks();
	RS_ShutdownCallbacks();
	UR_ShutdownCallbacks();
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
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 */
void CP_CampaignInit (campaign_t *campaign, qboolean load)
{
	ccs.curCampaign = campaign;

	RS_InitTree(load);		/**< Initialise all data in the research tree. */

	CP_AddCampaignCommands();

	CL_GameTimeStop();

	/* Init popup and map/geoscape */
	CL_PopupInit();

	CP_XVIInit();

	MN_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	if (load) {
		/** @todo move into CP_Load? */
		CP_InitMarket(load);
		return;
	}

	/* initialise view angle for 3D geoscape so that europe is seen */
	ccs.angles[YAW] = GLOBE_ROTATE;
	/* initialise date */
	ccs.date = campaign->date;

	MAP_Init();
	PR_ProductionInit();

	/* get day */
	while (ccs.date.sec > SECONDS_PER_DAY) {
		ccs.date.sec -= SECONDS_PER_DAY;
		ccs.date.day++;
	}
	CL_UpdateTime();

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(campaign->credits);

	/* Initialize alien interest */
	CL_ResetAlienInterest();

	/* Initialize XVI overlay */
	Cvar_SetValue("mn_xvimap", ccs.XVIShowMap);
	R_InitializeXVIOverlay(NULL);

	/* create a base as first step */
	B_SelectBase(NULL);

	Cmd_ExecuteString("addeventmail prolog");

	/* Spawn first missions of the game */
	CP_InitializeSpawningDelay();

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();
}

void CP_CampaignExit (void)
{
	if (GAME_CP_IsRunning()) {
		r_geoscape_overlay->integer = 0;
		/* singleplayer commands are no longer available */
		Com_DPrintf(DEBUG_CLIENT, "Remove game commands\n");
		CP_RemoveCampaignCommands();
	}
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

	for (i = 0, campaign = ccs.campaigns; i < ccs.numCampaigns; i++, campaign++)
		if (!strcmp(name, campaign->id))
			break;

	if (i == ccs.numCampaigns) {
		Com_Printf("CL_GetCampaign: Campaign \"%s\" doesn't exist.\n", name);
		return NULL;
	}
	return campaign;
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa INVSH_InitInventory
 * @sa CL_ReadSinglePlayerData
 */
void CL_ResetSinglePlayerData (void)
{
	int i;

	memset(&ccs, 0, sizeof(ccs));

	LIST_Delete(&ccs.missions);
	cp_messageStack = NULL;

	/* cleanup dynamic mails */
	CL_FreeDynamicEventMail();

	Mem_FreePool(cp_campaignPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetTechs();
	E_ResetEmployees();

	/* Count Alien team definitions. */
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (CHRSH_IsTeamDefAlien(&csi.teamDef[i]))
			ccs.numAliensTD++;
	}
}

#ifdef DEBUG
/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f (void)
{
	int i, j;
	character_t *chr;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (i = 0; i < ccs.numEmployees[EMPL_SOLDIER]; i++) {
		employee_t *employee = &ccs.employees[EMPL_SOLDIER][i];

		if (!employee->hired && employee->baseHired != base)
			continue;

		chr = &(employee->chr);
		assert(chr);

		for (j = 0; j < KILLED_NUM_TYPES; j++)
			chr->score.kills[j]++;
	}
	if (base->aircraftCurrent)
		CL_UpdateCharacterStats(base, 1, base->aircraftCurrent);
}

/**
 * @brief Show campaign stats in console
 */
static void CP_CampaignStats_f (void)
{
	campaign_t *campaign = ccs.curCampaign;

	if (!GAME_CP_IsRunning()) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", campaign->id);
	Com_Printf("..research list: %s\n", campaign->researched);
	Com_Printf("..equipment: %s\n", campaign->equipment);
	Com_Printf("..team: %i\n", campaign->team);

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
 * @brief Returns "homebase" of the mission.
 * @note see struct client_static_s
 * @note This might be @c NULL for skirmish and multiplayer
 */
base_t *CP_GetMissionBase (void)
{
	if (!ccs.missionaircraft)
		Com_Error(ERR_DROP, "CP_GetMissionBase: No missionaircraft given");
	if (!ccs.missionaircraft->homebase)
		Com_Error(ERR_DROP, "CP_GetMissionBase: Missionaircraft has no homebase set");
	return ccs.missionaircraft->homebase;
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

/** @todo remove me and move all the included stuff to proper places */
void CP_InitStartup (void)
{
	cp_campaignPool = Mem_CreatePool("Client: Local (per game)");

	SAV_Init();

	/* commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
	Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
#endif
	Cmd_AddCommand("check_baseattacks", CP_CheckBaseAttacks_f, "Check if baseattack mission available and start it.");

	CP_MissionsInit();
	MS_MessageInit();

	/* init UFOpaedia, basemanagement and other subsystems */
	UP_InitStartup();
	B_InitStartup();
	INS_InitStartup();
	RS_InitStartup();
	E_InitStartup();
	HOS_InitStartup();
	AC_InitStartup();
	MAP_InitStartup();
	UFO_InitStartup();
	TR_InitStartup();
	AB_InitStartup();
	AIRFIGHT_InitStartup();
	UR_InitStartup();
	NAT_InitStartup();
}

