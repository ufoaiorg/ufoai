/**
 * @file cp_cgame_callbacks.c
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
#include "cp_cgame_callbacks.h"
#include "cp_campaign.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_team.h"
#include "cp_map.h"
#include "../../battlescape/cl_hud.h"
#include "../../ui/ui_main.h"
#include "../../ui/node/ui_node_text.h"
#include "cp_mission_callbacks.h"

const cgame_import_t *cgImport;

/**
 * @sa CL_ParseResults
 * @sa CP_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void GAME_CP_Results_f (void)
{
	mission_t *mission = MAP_GetSelectedMission();

	if (!mission)
		return;

	/* check for replay */
	if (Cvar_GetInteger("cp_mission_tryagain")) {
		/* don't collect things and update stats --- we retry the mission */
		CP_StartSelectedMission();
		return;
	}
	/* check for win */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <won:true|false>\n", Cmd_Argv(0));
		return;
	}

	CP_MissionEnd(ccs.curCampaign, mission, &ccs.battleParameters, Com_ParseBoolean(Cmd_Argv(1)));
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param[in] difficulty The difficulty level of the game
 */
static inline const char* CP_ToDifficultyName (const int difficulty)
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
		Com_Error(ERR_DROP, "Unknown difficulty id %i", difficulty);
	}
}

/**
 * @brief Fill the campaign list with available campaigns
 */
static void GAME_CP_GetCampaigns_f (void)
{
	int i;
	uiNode_t *campaignOption = NULL;

	for (i = 0; i < ccs.numCampaigns; i++) {
		const campaign_t *c = &ccs.campaigns[i];
		if (c->visible)
			UI_AddOption(&campaignOption, "", va("_%s", c->name), c->id);
	}

	UI_RegisterOption(OPTION_CAMPAIGN_LIST, campaignOption);
}

#define MAXCAMPAIGNTEXT 4096
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Script function to show description of the selected a campaign
 */
static void GAME_CP_CampaignDescription_f (void)
{
	const char *racetype;
	const campaign_t *campaign;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <campaign_id>\n", Cmd_Argv(0));
		return;
	}

	campaign = CP_GetCampaign(Cmd_Argv(1));
	if (!campaign) {
		Com_Printf("Invalid Campaign id: %s\n", Cmd_Argv(1));
		return;
	}

	/* Make sure that this campaign is selected */
	Cvar_Set("cp_campaign", campaign->id);

	if (campaign->team == TEAM_PHALANX)
		racetype = _("Human");
	else
		racetype = _("Aliens");

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("%s\n\nRace: %s\nRecruits: %i %s, %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"), _(campaign->name), racetype,
		campaign->soldiers, ngettext("soldier", "soldiers", campaign->soldiers),
		campaign->scientists, ngettext("scientist", "scientists", campaign->scientists),
		campaign->workers, ngettext("worker", "workers", campaign->workers),
		campaign->pilots, ngettext("pilot", "pilots", campaign->pilots),
		campaign->credits, CP_ToDifficultyName(campaign->difficulty),
		(int)(round(campaign->minhappiness * 100.0f)), campaign->negativeCreditsUntilLost,
		_(campaign->text));
	UI_RegisterText(TEXT_STANDARD, campaignDesc);
}

/**
 * @brief Starts a new single-player game
 * @sa CP_CampaignInit
 * @sa CP_InitMarket
 */
static void GAME_CP_Start_f (void)
{
	campaign_t *campaign;

	campaign = CP_GetCampaign(Cmd_Argv(1));
	if (!campaign) {
		Com_Printf("Invalid Campaign id: %s\n", Cmd_Argv(1));
		return;
	}

	/* Make sure that this campaign is selected */
	Cvar_Set("cp_campaign", campaign->id);

	CP_CampaignInit(campaign, qfalse);

	/* Intro sentences */
	Cbuf_AddText("seq_start intro\n");
}

static inline void AL_AddAlienTypeToAircraftCargo_ (void *data, const teamDef_t *teamDef, int amount, qboolean dead)
{
	AL_AddAlienTypeToAircraftCargo((aircraft_t *) data, teamDef, amount, dead);
}

/**
 * @brief After a mission was finished this function is called
 * @param msg The network message buffer
 * @param winner The winning team
 * @param numSpawned The amounts of all spawned actors per team
 * @param numAlive The amount of survivors per team
 * @param numKilled The amount of killed actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 * @param numStunned The amount of stunned actors for all teams. The first dimension contains
 * the attacker team, the second the victim team
 */
void GAME_CP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], qboolean nextmap)
{
	int i, j;
	int ownSurvived, ownKilled, ownStunned;
	int aliensSurvived, aliensKilled, aliensStunned;
	int civiliansSurvived, civiliansKilled, civiliansStunned;
	const int currentTeam = cgi->GAME_GetCurrentTeam();
	const qboolean won = (winner == currentTeam);
	missionResults_t *results;
	aircraft_t *aircraft = MAP_GetMissionAircraft();
	const battleParam_t *bp = &ccs.battleParameters;

	CP_ParseCharacterData(msg);

	ownSurvived = ownKilled = ownStunned = 0;
	aliensSurvived = aliensKilled = aliensStunned = 0;
	civiliansSurvived = civiliansKilled = civiliansStunned = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		if (i == currentTeam)
			ownSurvived = numAlive[i];
		else if (i == TEAM_CIVILIAN)
			civiliansSurvived = numAlive[i];
		else
			aliensSurvived += numAlive[i];
		for (j = 0; j < MAX_TEAMS; j++)
			if (j == currentTeam) {
				ownKilled += numKilled[i][j];
				ownStunned += numStunned[i][j]++;
			} else if (j == TEAM_CIVILIAN) {
				civiliansKilled += numKilled[i][j];
				civiliansStunned += numStunned[i][j]++;
			} else {
				aliensKilled += numKilled[i][j];
				aliensStunned += numStunned[i][j]++;
			}
	}
	/* if we won, our stunned are alive */
	if (won) {
		ownSurvived += ownStunned;
		ownStunned = 0;
	} else
		/* if we lost, they revive stunned */
		aliensStunned = 0;

	/* we won, and we're not the dirty aliens */
	if (won)
		civiliansSurvived += civiliansStunned;
	else
		civiliansKilled += civiliansStunned;

	/* Collect items from the battlefield. */
	AII_CollectingItems(aircraft, won);
	if (won)
		/* Collect aliens from the battlefield. */
		cgi->CollectAliens(aircraft, AL_AddAlienTypeToAircraftCargo_);

	ccs.aliensKilled += aliensKilled;

	results = &ccs.missionResults;
	results->mission = bp->mission;

	if (nextmap) {
		assert(won);
		results->aliensKilled += aliensKilled;
		results->aliensStunned += aliensStunned;
		results->aliensSurvived += aliensSurvived;
		results->ownKilled += ownKilled - numKilled[currentTeam][currentTeam] - numKilled[TEAM_CIVILIAN][currentTeam];
		results->ownStunned += ownStunned;
		results->ownKilledFriendlyFire += numKilled[currentTeam][currentTeam] + numKilled[TEAM_CIVILIAN][currentTeam];
		results->ownSurvived += ownSurvived;
		results->civiliansKilled += civiliansKilled;
		results->civiliansKilledFriendlyFire += numKilled[currentTeam][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
		results->civiliansSurvived += civiliansSurvived;
		return;
	}

	results->won = won;
	results->aliensKilled = aliensKilled;
	results->aliensStunned = aliensStunned;
	results->aliensSurvived = aliensSurvived;
	results->ownKilled = ownKilled - numKilled[currentTeam][currentTeam] - numKilled[TEAM_CIVILIAN][currentTeam];
	results->ownStunned = ownStunned;
	results->ownKilledFriendlyFire = numKilled[currentTeam][currentTeam] + numKilled[TEAM_CIVILIAN][currentTeam];
	results->ownSurvived = ownSurvived;
	results->civiliansKilled = civiliansKilled;
	results->civiliansKilledFriendlyFire = numKilled[currentTeam][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
	results->civiliansSurvived = civiliansSurvived;

	MIS_InitResultScreen(results);
	if (ccs.missionResultCallback) {
		ccs.missionResultCallback(results);
	}

	UI_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	if (won)
		UI_PushWindow("won", NULL, NULL);
	else
		UI_PushWindow("lost", NULL, NULL);

	cgi->CL_Disconnect();
	SV_Shutdown("Mission end", qfalse);
}

qboolean GAME_CP_Spawn (chrList_t *chrList)
{
	aircraft_t *aircraft = MAP_GetMissionAircraft();
	base_t *base;

	if (!aircraft)
		return qfalse;

	/* convert aircraft team to character list */
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		chrList->chr[chrList->num] = &employee->chr;
		chrList->num++;
	}

	base = aircraft->homebase;

	/* clean temp inventory */
	CP_CleanTempInventory(base);

	/* activate hud */
	HUD_InitUI(NULL, qfalse);

	return qtrue;
}

qboolean GAME_CP_ItemIsUseable (const objDef_t *od)
{
	const technology_t *tech = RS_GetTechForItem(od);
	return RS_IsResearched_ptr(tech);
}

/**
 * @brief Checks whether the team is known at this stage already
 * @param[in] teamDef The team definition of the alien team
 * @return @c true if known, @c false otherwise.
 */
qboolean GAME_CP_TeamIsKnown (const teamDef_t *teamDef)
{
	if (!CHRSH_IsTeamDefAlien(teamDef))
		return qtrue;

	if (!ccs.teamDefTechs[teamDef->idx])
		Com_Error(ERR_DROP, "Could not find tech for teamdef '%s'", teamDef->id);

	return RS_IsResearched_ptr(ccs.teamDefTechs[teamDef->idx]);
}

void GAME_CP_Drop (void)
{
	/** @todo maybe create a savegame? */
	UI_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	SV_Shutdown("Mission end", qfalse);
	cgi->CL_Disconnect();
}

void GAME_CP_Frame (float secondsSinceLastFrame)
{
	if (!CP_IsRunning())
		return;

	if (!CP_OnGeoscape())
		return;

	/* advance time */
	CP_CampaignRun(ccs.curCampaign, secondsSinceLastFrame);
}

const char* GAME_CP_GetTeamDef (void)
{
	const int team = ccs.curCampaign->team;
	return Com_ValueToStr(&team, V_TEAM, 0);
}

/**
 * @brief Changes some actor states for a campaign game
 * @param team The team to change the states for
 */
struct dbuffer *GAME_CP_InitializeBattlescape (const chrList_t *team)
{
	int i;
	struct dbuffer *msg = new_dbuffer();

	NET_WriteByte(msg, clc_initactorstates);
	NET_WriteByte(msg, team->num);

	for (i = 0; i < team->num; i++) {
		const character_t *chr = team->chr[i];
		NET_WriteShort(msg, chr->ucn);
		NET_WriteShort(msg, chr->state);
		NET_WriteShort(msg, chr->RFmode.hand);
		NET_WriteShort(msg, chr->RFmode.fmIdx);
		NET_WriteShort(msg, chr->RFmode.weapon != NULL ? chr->RFmode.weapon->idx : NONE);
	}

	return msg;
}

equipDef_t *GAME_CP_GetEquipmentDefinition (void)
{
	return &ccs.eMission;
}

void GAME_CP_CharacterCvars (const character_t *chr)
{
	/* Display rank if the character has one. */
	if (chr->score.rank >= 0) {
		char buf[MAX_VAR];
		const rank_t *rank = CL_GetRankByIdx(chr->score.rank);
		Com_sprintf(buf, sizeof(buf), _("Rank: %s"), _(rank->name));
		Cvar_Set("mn_chrrank", buf);
		Cvar_Set("mn_chrrank_img", rank->image);
		Com_sprintf(buf, sizeof(buf), "%s ", _(rank->name));
		Cvar_Set("mn_chrrankprefix", buf);
	} else {
		Cvar_Set("mn_chrrank_img", "");
		Cvar_Set("mn_chrrank", "");
		Cvar_Set("mn_chrrankprefix", "");
	}

	Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ENEMIES]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));
}

const char* GAME_CP_GetItemModel (const char *string)
{
	const aircraft_t *aircraft = AIR_GetAircraftSilent(string);
	if (aircraft) {
		assert(aircraft->tech);
		return aircraft->tech->mdl;
	} else {
		const technology_t *tech = RS_GetTechByProvided(string);
		if (tech)
			return tech->mdl;
		return NULL;
	}
}

void GAME_CP_InitStartup (void)
{
	Cmd_AddCommand("cp_results", GAME_CP_Results_f, "Parses and shows the game results");
	Cmd_AddCommand("cp_getdescription", GAME_CP_CampaignDescription_f, NULL);
	Cmd_AddCommand("cp_getcampaigns", GAME_CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	Cmd_AddCommand("cp_start", GAME_CP_Start_f, "Start the new campaign");

	CP_InitStartup(cgImport);

	cp_start_employees = Cvar_Get("cp_start_employees", "1", CVAR_ARCHIVE, "Start with hired employees");
	/* cvars might have been changed by other gametypes */
	Cvar_ForceSet("sv_maxclients", "1");
	Cvar_ForceSet("sv_ai", "1");

	/* reset campaign data */
	CP_ResetCampaignData();
	CP_ParseCampaignData();
}

void GAME_CP_Shutdown (void)
{
	Cmd_RemoveCommand("cp_results");
	Cmd_RemoveCommand("cp_getdescription");
	Cmd_RemoveCommand("cp_getcampaigns");
	Cmd_RemoveCommand("cp_start");

	CP_Shutdown();

	CP_ResetCampaignData();

	SV_Shutdown("Quitting server.", qfalse);
}
