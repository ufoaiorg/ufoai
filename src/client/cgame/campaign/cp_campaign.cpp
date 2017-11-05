/**
 * @file
 * @brief Single player campaign control.
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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
#include "cp_character.h"
#include "cp_overlay.h"
#include "cp_mapfightequip.h"
#include "cp_hospital.h"
#include "cp_hospital_callbacks.h"
#include "cp_base_callbacks.h"
#include "cp_basedefence_callbacks.h"
#include "cp_team.h"
#include "cp_team_callbacks.h"
#include "cp_popup.h"
#include "cp_geoscape.h"
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
#include "missions/cp_mission_baseattack.h"
#include "missions/cp_mission_ufocarrier.h"

memPool_t* cp_campaignPool;		/**< reset on every game restart */
ccs_t ccs;
cvar_t* cp_missiontest;

/* fraction of nation that can be below min happiness before the game is lost */
#define NATIONBELOWLIMITPERCENTAGE 0.5f

/**
 * @brief delay between actions that must be executed independently of time scale
 * @sa RADAR_CheckUFOSensored
 * @sa UFO_UpdateAlienInterestForAllBasesAndInstallations
 * @sa AB_UpdateStealthForAllBase
 */
const int DETECTION_INTERVAL = (SECONDS_PER_HOUR / 2);

/**
 * @brief Checks whether a campaign mode game is running
 */
bool CP_IsRunning (void)
{
	return ccs.curCampaign != nullptr;
}

/**
 * @brief Function to handle the campaign end
 * @param[in] won If the player won the game
 */
void CP_EndCampaign (bool won)
{
	cgi->Cmd_ExecuteString("game_save slotend \"End of game\"");
	cgi->Cbuf_AddText(va("ui_set_endgame %s;", won ? "won" : "lose"));
	cgi->Cbuf_AddText("ui_initstack endgame;");

	cgi->Com_Drop();
}

/**
 * @brief Checks whether the player has lost the campaign
 */
void CP_CheckLostCondition (const campaign_t* campaign)
{
	bool endCampaign = false;

	if (cp_missiontest->integer)
		return;

	if (!endCampaign && ccs.credits < -campaign->negativeCreditsUntilLost) {
		cgi->UI_RegisterText(TEXT_STANDARD, _("You've gone too far into debt."));
		endCampaign = true;
	}

	/** @todo Should we make the campaign lost when a player loses all his bases?
	 * until he has set up a base again, the aliens might have invaded the whole
	 * world ;) - i mean, removing the credits check here. */
	if (ccs.credits < campaign->basecost - campaign->negativeCreditsUntilLost && !B_AtLeastOneExists()) {
		cgi->UI_RegisterText(TEXT_STANDARD, _("You've lost your bases and don't have enough money to build new ones."));
		endCampaign = true;
	}

	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > campaign->maxAllowedXVIRateUntilLost) {
			cgi->UI_RegisterText(TEXT_STANDARD, _("You have failed in your charter to protect Earth."
				" Our home and our people have fallen to the alien infection. Only a handful"
				" of people on Earth remain human, and the remaining few no longer have a"
				" chance to stem the tide. Your command is no more; PHALANX is no longer"
				" able to operate as a functioning unit. Nothing stands between the aliens"
				" and total victory."));
			endCampaign = true;
		} else {
			/* check for nation happiness */
			int nationBelowLimit = 0;
			for (int j = 0; j < ccs.numNations; j++) {
				const nation_t* nation = NAT_GetNationByIDX(j);
				const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
				if (stats->happiness < campaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= NATIONBELOWLIMITPERCENTAGE * ccs.numNations) {
				/* lost the game */
				cgi->UI_RegisterText(TEXT_STANDARD, _("Under your command, PHALANX operations have"
					" consistently failed to protect nations."
					" The UN, highly unsatisfied with your performance, has decided to remove"
					" you from command and subsequently disbands the PHALANX project as an"
					" effective task force. No further attempts at global cooperation are made."
					" Earth's nations each try to stand alone against the aliens, and eventually"
					" fall one by one."));
				endCampaign = true;
			}
		}
	}

	if (endCampaign)
		CP_EndCampaign(false);
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
static void CP_CampaignFunctionPeriodicCall (campaign_t* campaign, int dt, bool updateRadarOverlay)
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
bool CP_OnGeoscape (void)
{
	return Q_streq("geoscape", cgi->UI_GetActiveWindowName());
}

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
static inline bool CP_IsBudgetDue (const dateLong_t* oldDate, const dateLong_t* date)
{
	if (oldDate->year < date->year) {
		return true;
	}
	return oldDate->month < date->month;
}

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types cgi->UI_MAP and cgi->UI_3DMAP
 * @sa NAT_HandleBudget
 * @sa AIR_CampaignRun
 */
void CP_CampaignRun (campaign_t* campaign, float secondsSinceLastFrame)
{
	/* advance time */
	ccs.frametime = secondsSinceLastFrame;
	ccs.timer += secondsSinceLastFrame * ccs.gameTimeScale;

	UP_GetUnreadMails();

	if (ccs.timer < 1.0)
		return;

	/* calculate new date */
	int currentsecond = ccs.date.sec;
	int currentday = ccs.date.day;
	const int currentinterval = currentsecond % DETECTION_INTERVAL;
	int dt = DETECTION_INTERVAL - currentinterval;
	dateLong_t date, oldDate;
	const int timer = (int)floor(ccs.timer);
	const int checks = (currentinterval + timer) / DETECTION_INTERVAL;

	CP_DateConvertLong(&ccs.date, &oldDate);

	int currenthour = currentsecond / SECONDS_PER_HOUR;
	int currentmin = currentsecond / SECONDS_PER_MINUTE;

	/* Execute every actions that needs to be independent of time speed : every DETECTION_INTERVAL
	 *	- Run UFOs and craft at least every DETECTION_INTERVAL. If detection occurred, break.
	 *	- Check if any new mission is detected
	 *	- Update stealth value of phalanx bases and installations ; alien bases */
	for (int i = 0; i < checks; i++) {
		ccs.timer -= dt;
		currentsecond += dt;
		CP_AdvanceTimeBySeconds(dt);
		CP_CampaignFunctionPeriodicCall(campaign, dt, false);

		/* if something stopped time, we must stop here the loop */
		if (CP_IsTimeStopped()) {
			ccs.timer = 0.0f;
			break;
		}
		dt = DETECTION_INTERVAL;
	}

	dt = timer;

	CP_AdvanceTimeBySeconds(dt);
	currentsecond += dt;
	ccs.timer -= dt;

	/* compute minutely events  */
	/* (this may run multiple times if the time stepping is > 1 minute at a time) */
	const int newmin = currentsecond / SECONDS_PER_MINUTE;
	while (currentmin < newmin) {
		currentmin++;
		PR_ProductionRun();
		B_UpdateBuildingConstructions();
	}

	/* compute hourly events  */
	/* (this may run multiple times if the time stepping is > 1 hour at a time) */
	const int newhour = currentsecond / SECONDS_PER_HOUR;
	while (currenthour < newhour) {
		currenthour++;
		RS_ResearchRun();
		UR_ProcessActive();
		AII_UpdateInstallationDelay();
		AII_RepairAircraft();
		TR_TransferRun();
		INT_IncreaseAlienInterest(campaign);
	}

	/* daily events */
	for (int i = currentday; i < ccs.date.day; i++) {
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
		CP_TriggerEvent(NEW_DAY);
	}

	if (dt > 0) {
		/* check for campaign events
		 * aircraft and UFO already moved during radar detection (see above),
		 * just make them move the missing part -- if any */
		CP_CampaignFunctionPeriodicCall(campaign, dt, true);
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
		ccs.paid = false;
	} else if (date.day > 1)
		ccs.paid = true;

	CP_UpdateXVIMapButton();
	/* set time cvars */
	CP_UpdateTime();
}

/**
 * @brief Checks whether you have enough credits for something
 * @param[in] costs costs to check
 */
bool CP_CheckCredits (int costs)
{
	if (costs > ccs.credits)
		return false;
	return true;
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
	cgi->Cvar_Set("mn_credits", _("%i c"), ccs.credits);
}

/**
 * @brief Load mapDef statistics
 * @param[in] parent XML Node structure, where we get the information from
 */
static bool CP_LoadMapDefStatXML (xmlNode_t* parent)
{
	xmlNode_t* node;

	for (node = cgi->XML_GetNode(parent, SAVE_CAMPAIGN_MAPDEF); node; node = cgi->XML_GetNextNode(node, parent, SAVE_CAMPAIGN_MAPDEF)) {
		const char* s = cgi->XML_GetString(node, SAVE_CAMPAIGN_MAPDEF_ID);
		mapDef_t* map;

		if (s[0] == '\0') {
			Com_Printf("Warning: MapDef with no id in xml!\n");
			continue;
		}
		map = cgi->Com_GetMapDefinitionByID(s);
		if (!map) {
			Com_Printf("Warning: No MapDef with id '%s'!\n", s);
			continue;
		}
		map->timesAlreadyUsed = cgi->XML_GetInt(node, SAVE_CAMPAIGN_MAPDEF_COUNT, 0);
	}

	return true;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] parent XML Node structure, where we get the information from
 */
bool CP_LoadXML (xmlNode_t* parent)
{
	xmlNode_t* campaignNode;
	xmlNode_t* mapNode;
	const char* name;
	campaign_t* campaign;
	xmlNode_t* mapDefStat;

	campaignNode = cgi->XML_GetNode(parent, SAVE_CAMPAIGN_CAMPAIGN);
	if (!campaignNode) {
		Com_Printf("Did not find campaign entry in xml!\n");
		return false;
	}
	if (!(name = cgi->XML_GetString(campaignNode, SAVE_CAMPAIGN_ID))) {
		Com_Printf("couldn't locate campaign name in savegame\n");
		return false;
	}

	campaign = CP_GetCampaign(name);
	if (!campaign) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		return false;
	}

	CP_CampaignInit(campaign, true);
	/* init the map images and reset the map actions */
	GEO_Reset(campaign->map);

	/* read credits */
	CP_UpdateCredits(cgi->XML_GetLong(campaignNode, SAVE_CAMPAIGN_CREDITS, 0));
	ccs.paid = cgi->XML_GetBool(campaignNode, SAVE_CAMPAIGN_PAID, false);

	cgi->SetNextUniqueCharacterNumber(cgi->XML_GetInt(campaignNode, SAVE_CAMPAIGN_NEXTUNIQUECHARACTERNUMBER, 0));

	cgi->XML_GetDate(campaignNode, SAVE_CAMPAIGN_DATE, &ccs.date.day, &ccs.date.sec);

	/* read other campaign data */
	ccs.civiliansKilled = cgi->XML_GetInt(campaignNode, SAVE_CAMPAIGN_CIVILIANSKILLED, 0);
	ccs.aliensKilled = cgi->XML_GetInt(campaignNode, SAVE_CAMPAIGN_ALIENSKILLED, 0);

	Com_DPrintf(DEBUG_CLIENT, "CP_LoadXML: Getting position\n");

	/* read map view */
	mapNode = cgi->XML_GetNode(campaignNode, SAVE_CAMPAIGN_MAP);

	/* Compatibility code for savegames */
	int oldOverlayStatus = cgi->XML_GetInt(mapNode, SAVE_CAMPAIGN_CL_GEOSCAPE_OVERLAY, -1);
	if (oldOverlayStatus >= 0) {
		const int overlayNation = 1 << 0;
		const int overlayXvi = 1 << 1;
		const int overlayRadar = 1 << 2;

		cgi->Cvar_SetValue("geo_overlay_radar", oldOverlayStatus & overlayRadar);
		cgi->Cvar_SetValue("geo_overlay_nation", oldOverlayStatus & overlayNation);
		cgi->Cvar_SetValue("geo_overlay_xvi", oldOverlayStatus & overlayXvi);
	} else {
		cgi->Cvar_SetValue("geo_overlay_radar", cgi->XML_GetInt(mapNode, SAVE_CAMPAIGN_GEO_OVERLAY_RADAR, 0));
		cgi->Cvar_SetValue("geo_overlay_nation", cgi->XML_GetInt(mapNode, SAVE_CAMPAIGN_GEO_OVERLAY_NATION, 0));
		cgi->Cvar_SetValue("geo_overlay_xvi", cgi->XML_GetInt(mapNode, SAVE_CAMPAIGN_GEO_OVERLAY_XVI, 0));
	}
	radarOverlayWasSet = cgi->XML_GetBool(mapNode, SAVE_CAMPAIGN_RADAROVERLAYWASSET, false);
	ccs.startXVI = cgi->XML_GetBool(mapNode, SAVE_CAMPAIGN_XVISTARTED, false);

	mapDefStat = cgi->XML_GetNode(campaignNode, SAVE_CAMPAIGN_MAPDEFSTAT);
	if (mapDefStat && !CP_LoadMapDefStatXML(mapDefStat))
		return false;

	mxmlDelete(campaignNode);
	return true;
}

/**
 * @brief Save mapDef statistics
 * @param[out] parent XML Node structure, where we write the information to
 */
static bool CP_SaveMapDefStatXML (xmlNode_t* parent)
{
	const mapDef_t* md;

	MapDef_ForeachSingleplayerCampaign(md) {
		if (md->timesAlreadyUsed > 0) {
			xmlNode_t* node = cgi->XML_AddNode(parent, SAVE_CAMPAIGN_MAPDEF);
			cgi->XML_AddString(node, SAVE_CAMPAIGN_MAPDEF_ID, md->id);
			cgi->XML_AddInt(node, SAVE_CAMPAIGN_MAPDEF_COUNT, md->timesAlreadyUsed);
		}
	}

	return true;
}

/**
 * @brief Save callback for savegames in XML Format
 * @param[out] parent XML Node structure, where we write the information to
 */
bool CP_SaveXML (xmlNode_t* parent)
{
	xmlNode_t* campaign;
	xmlNode_t* map;
	xmlNode_t* mapDefStat;

	campaign = cgi->XML_AddNode(parent, SAVE_CAMPAIGN_CAMPAIGN);

	cgi->XML_AddString(campaign, SAVE_CAMPAIGN_ID, ccs.curCampaign->id);
	cgi->XML_AddDate(campaign, SAVE_CAMPAIGN_DATE, ccs.date.day, ccs.date.sec);
	cgi->XML_AddLong(campaign, SAVE_CAMPAIGN_CREDITS, ccs.credits);
	cgi->XML_AddShort(campaign, SAVE_CAMPAIGN_PAID, ccs.paid);
	cgi->XML_AddShortValue(campaign, SAVE_CAMPAIGN_NEXTUNIQUECHARACTERNUMBER, cgi->GetNextUniqueCharacterNumber());

	cgi->XML_AddIntValue(campaign, SAVE_CAMPAIGN_CIVILIANSKILLED, ccs.civiliansKilled);
	cgi->XML_AddIntValue(campaign, SAVE_CAMPAIGN_ALIENSKILLED, ccs.aliensKilled);

	/* Map and user interface */
	map = cgi->XML_AddNode(campaign, SAVE_CAMPAIGN_MAP);
	cgi->XML_AddShort(map, SAVE_CAMPAIGN_GEO_OVERLAY_RADAR, cgi->Cvar_GetInteger("geo_overlay_radar"));
	cgi->XML_AddShort(map, SAVE_CAMPAIGN_GEO_OVERLAY_NATION, cgi->Cvar_GetInteger("geo_overlay_nation"));
	cgi->XML_AddShort(map, SAVE_CAMPAIGN_GEO_OVERLAY_XVI, cgi->Cvar_GetInteger("geo_overlay_xvi"));
	cgi->XML_AddBool(map, SAVE_CAMPAIGN_RADAROVERLAYWASSET, radarOverlayWasSet);
	cgi->XML_AddBool(map, SAVE_CAMPAIGN_XVISTARTED, CP_IsXVIStarted());

	mapDefStat = cgi->XML_AddNode(campaign, SAVE_CAMPAIGN_MAPDEFSTAT);
	if (!CP_SaveMapDefStatXML(mapDefStat))
		return false;

	return true;
}

/**
 * @brief Starts a selected mission
 * @note Checks whether a dropship is near the landing zone and whether
 * it has a team on board
 * @sa BATTLE_SetVars
 */
void CP_StartSelectedMission (void)
{
	mission_t* mis;
	aircraft_t* aircraft = GEO_GetMissionAircraft();
	base_t* base;
	battleParam_t* battleParam = &ccs.battleParameters;

	if (!aircraft) {
		Com_Printf("CP_StartSelectedMission: No mission aircraft\n");
		return;
	}

	base = aircraft->homebase;

	if (GEO_GetSelectedMission() == nullptr)
		GEO_SetSelectedMission(aircraft->mission);

	mis = GEO_GetSelectedMission();
	if (!mis) {
		Com_Printf("CP_StartSelectedMission: No mission selected\n");
		return;
	}

	/* Before we start, we should clear the missionResults array. */
	OBJZERO(mis->missionResults);

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
	cgi->SV_Shutdown("Server quit.", false);
	cgi->CL_Disconnect();

	CP_CreateBattleParameters(mis, battleParam, aircraft);
	BATTLE_SetVars(battleParam);

	/* manage inventory */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CP_CleanTempInventory(base);
	CP_CleanupAircraftTeam(aircraft, &ccs.eMission);
	BATTLE_Start(mis, battleParam);
}
#ifdef DEBUG
/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_listitem
 */
static void CP_DebugShowItems_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", cgi->Cmd_Argv(0));
		return;
	}

	int i = atoi(cgi->Cmd_Argv(1));
	if (i >= B_GetCount()) {
		Com_Printf("invalid baseID (%s)\n", cgi->Cmd_Argv(1));
		return;
	}
	base_t* base = B_GetBaseByIDX(i);

	for (i = 0; i < cgi->csi->numODs; i++) {
		const objDef_t* obj = INVSH_GetItemByIDX(i);
		Com_Printf("%i. %s: %i\n", i, obj->id, B_ItemInBase(obj, base));
	}
}

/**
 * @brief Debug function to add certain items to base storage
 * @note Call this with debug_itemadd <baseIDX> <itemID> <count>
 * @note This function is not for antimatter
 * @sa CP_DebugAddAntimatter_f
 */
static void CP_DebugAddItem_f (void)
{
	if (cgi->Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <baseID> <itemid> <count>\n", cgi->Cmd_Argv(0));
		return;
	}

	base_t* base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	const objDef_t* obj = INVSH_GetItemByID(cgi->Cmd_Argv(2));
	const int count = atoi(cgi->Cmd_Argv(3));

	if (!base) {
		Com_Printf("Invalid base index given\n");
		return;
	}
	if (!obj) {
		/* INVSH_GetItemByIDX prints warning already */
		return;
	}

	const int amount = B_AddToStorage(base, obj, count);
	Com_Printf("%s %s %d\n", base->name, obj->id, amount);
	if (B_ItemInBase(obj, base) > 0) {
		technology_t* tech = RS_GetTechForItem(obj);
		RS_MarkCollected(tech);
	}
}

/**
 * @brief Debug function to add some antimatter to base container
 * @note Call this with debug_antimatteradd <baseIDX> <amount>
 * @note 0 amount will reset the container
 * @sa CP_DebugAddItem_f
 */
static void CP_DebugAddAntimatter_f (void)
{
	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <baseID> <amount>\n", cgi->Cmd_Argv(0));
		return;
	}

	base_t* base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	const int amount = atoi(cgi->Cmd_Argv(2));

	if (!base) {
		Com_Printf("Invalid base index given\n");
		return;
	}

	B_AddAntimatter(base, amount);
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
	{"stats_update", CP_StatsUpdate_f, nullptr},
	{"game_go", CP_StartSelectedMission, nullptr},
	{"game_timestop", CP_GameTimeStop, nullptr},
	{"game_timeslow", CP_GameTimeSlow, nullptr},
	{"game_timefast", CP_GameTimeFast, nullptr},
	{"game_settimeid", CP_SetGameTime_f, nullptr},
	{"map_center", GEO_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
	{"cp_spawn_ufocarrier", CP_SpawnUFOCarrier_f, "Spawns a UFO-Carrier on the geoscape"},
	{"cp_attack_ufocarrier", CP_AttackUFOCarrier_f, "Attack the UFO-Carrier"},
#ifdef DEBUG
	{"debug_fullcredits", CP_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_itemadd", CP_DebugAddItem_f, "Debug function to add certain items to base storage"},
	{"debug_antimatteradd", CP_DebugAddAntimatter_f, "Debug function to add some antimatter to base container"},
	{"debug_listitem", CP_DebugShowItems_f, "Debug function to show all items in base storage"},
#endif
	{nullptr, nullptr, nullptr}
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
	cgi->Cmd_TableAddList(game_commands);
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
	cgi->Cmd_TableRemoveList(game_commands);

	CP_RemoveCampaignCallbackCommands();
}

/**
 * @brief Called at new game and load game
 * @param[in] load @c true if we are loading game, @c false otherwise
 * @param[in] campaign Pointer to campaign - it will be set to @c ccs.curCampaign here.
 */
void CP_CampaignInit (campaign_t* campaign, bool load)
{
	ccs.curCampaign = campaign;

	CP_ReadCampaignData(campaign);

	CP_UpdateTime();

	RS_InitTree(campaign, load);		/**< Initialise all data in the research tree. */

	CP_AddCampaignCommands();

	CP_GameTimeStop();

	/* Init popup and map/geoscape */
	CL_PopupInit();

	CP_XVIInit();

	if (load) {
		return;
	}

	cgi->UI_InitStack("geoscape", "campaign_main");

	CL_EventAddMail("prolog");

	RS_MarkResearchable(nullptr, true);
	BS_InitMarket(campaign);

	/* create initial employees */
	E_InitialEmployees(campaign);

	GEO_Reset(campaign->map);

	CP_UpdateCredits(campaign->credits);

	/* Initialize alien interest */
	INT_ResetAlienInterest();

	/* Initialize XVI overlay */
	CP_UpdateXVIMapButton();
	CP_InitializeXVIOverlay();

	/* create a base as first step */
	B_SelectBase(nullptr);

	/* Spawn first missions of the game */
	CP_InitializeSpawningDelay();

	/* now check the parsed values for errors that are not caught at parsing stage */
	CP_ScriptSanityCheck();
}

/**
 * @brief Campaign closing actions
 */
void CP_Shutdown (void)
{
	if (CP_IsRunning()) {
		AB_Shutdown();
		UFO_Shutdown();
		AIR_Shutdown();
		INS_Shutdown();
		INT_Shutdown();
		NAT_Shutdown();
		MIS_Shutdown();
		TR_Shutdown();
		UR_Shutdown();
		AM_Shutdown();
		E_Shutdown();
		CHAR_Shutdown();

		/** @todo Where does this belong? */
		for (int i = 0; i < ccs.numAlienCategories; i++) {
			alienTeamCategory_t* alienCat = &ccs.alienCategories[i];
			cgi->LIST_Delete(&alienCat->equipment);
		}

		cgi->Cvar_SetValue("geo_overlay_radar", 0);
		cgi->Cvar_SetValue("geo_overlay_nations", 0);
		cgi->Cvar_SetValue("geo_overlay_xvi", 0);
		/* singleplayer commands are no longer available */
		CP_RemoveCampaignCommands();
	}

	GEO_Shutdown();
}

/**
 * @brief Returns the campaign pointer from global campaign array
 * @param name Name of the campaign
 * @return campaign_t pointer to campaign with name or nullptr if not found
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
		return nullptr;
	}
	return campaign;
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa initInventory
 * @sa CP_ParseCampaignData
 */
void CP_ResetCampaignData (void)
{
	mapDef_t* md;

	cgi->UI_MessageResetStack();

	/* cleanup dynamic mails */
	CP_FreeDynamicEventMail();

	AIR_Foreach(aircraft) {
		AIR_Delete(nullptr, aircraft);
	}

	base_t* base = nullptr;
	while((base = B_GetNext(base)) != nullptr) {
		B_Delete(base);
	}

	cgi->FreePool(cp_campaignPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetTechs();

	OBJZERO(ccs);

	ccs.missionSpawnCallback = CP_SpawnNewMissions;

	/* Clear mapDef usage statistics */
	MapDef_ForeachSingleplayerCampaign(md) {
		md->timesAlreadyUsed = 0;
	}
}

int CP_GetSalaryAdministrative (const salary_t* salary)
{
	int costs = salary->adminInitial;
	for (int i = 0; i < MAX_EMPL; i++) {
		const employeeType_t type = (employeeType_t)i;
		costs += E_CountByType(type) * CP_GetSalaryAdminEmployee(salary, type);
	}
	return costs;
}

int CP_GetSalaryBaseEmployee (const salary_t* salary, employeeType_t type)
{
	return salary->base[type];
}

int CP_GetSalaryAdminEmployee (const salary_t* salary, employeeType_t type)
{
	return salary->admin[type];
}

int CP_GetSalaryRankBonusEmployee (const salary_t* salary, employeeType_t type)
{
	return salary->rankBonus[type];
}

int CP_GetSalaryUpKeepBase (const salary_t* salary, const base_t* base)
{
	int cost = salary->baseUpkeep;	/* base cost */
	building_t* building = nullptr;
	while ((building = B_GetNextBuilding(base, building))) {
		if (building->buildingStatus == B_STATUS_WORKING
		 || building->buildingStatus == B_STATUS_CONSTRUCTION_FINISHED)
			cost += building->varCosts;
	}
	return cost;
}

/** @todo remove me and move all the included stuff to proper places */
void CP_InitStartup (void)
{
	cp_campaignPool = cgi->CreatePool("Client: Local (per game)");

	SAV_Init();


	cp_missiontest = cgi->Cvar_Get("cp_missiontest", "0", CVAR_DEVELOPER, "This will never stop the time on geoscape and print information about spawned missions");

	/* init subsystems */
	MS_MessageInit();
	CP_InitializeXVIOverlay();

	MIS_InitStartup();
	UP_InitStartup();
	B_InitStartup();
	INS_InitStartup();
	RS_InitStartup();
	E_InitStartup();
	HOS_InitStartup();
	INT_InitStartup();
	AC_InitStartup();
	GEO_InitStartup();
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
	CHAR_InitStartup();
}
