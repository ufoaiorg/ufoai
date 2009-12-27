/**
 * @file cl_game_campaign.c
 * @brief Singleplayer campaign game type code
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

#include "client.h"
#include "battlescape/cl_localentity.h"
#include "cl_game.h"
#include "cl_team.h"
#include "campaign/cp_campaign.h"
#include "campaign/cp_missions.h"
#include "campaign/cp_mission_triggers.h"
#include "campaign/cp_team.h"
#include "menu/m_main.h"
#include "menu/m_nodes.h"

/**
 * @brief Checks whether a campaign mode game is running
 */
qboolean GAME_CP_IsRunning (void)
{
	return ccs.curCampaign != NULL;
}

/**
 * @sa GAME_CP_MissionAutoCheck_f
 * @sa CP_StartSelectedMission
 */
static void GAME_CP_MissionAutoGo_f (void)
{
	if (!ccs.selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoGo_f: No update after automission\n");
		return;
	}

	if (!ccs.missionAircraft) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No aircraft at target pos\n");
		return;
	}

	/* start the map */
	CL_GameAutoGo(ccs.selectedMission);
}

/**
 * @brief Checks whether you have to play this mission
 * You can mark a mission as story related.
 * If a mission is story related the cvar @c cp_mission_autogo_available is set to @c 0
 * If this cvar is @c 1 - the mission dialog will have a auto mission button
 * @sa GAME_CP_MissionAutoGo_f
 */
static void GAME_CP_MissionAutoCheck_f (void)
{
	if (!ccs.selectedMission || !ccs.interceptAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: No update after automission\n");
		return;
	}

	if (ccs.selectedMission->mapDef->storyRelated) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: story related - auto mission is disabled\n");
		Cvar_Set("cp_mission_autogo_available", "0");
	} else {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: auto mission is enabled\n");
		Cvar_Set("cp_mission_autogo_available", "1");
	}
}

/**
 * @sa CL_ParseResults
 * @sa CP_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void GAME_CP_Results_f (void)
{
	/* multiplayer? */
	if (!ccs.selectedMission)
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

	CP_MissionEnd(ccs.selectedMission, Com_ParseBoolean(Cmd_Argv(1)));
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param difficulty the difficulty integer value
 */
static inline const char* CP_ToDifficultyName (int difficulty)
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

#define MAXCAMPAIGNTEXT 4096
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Fill the campaign list with available campaigns
 */
static void GAME_CP_GetCampaigns_f (void)
{
	int i;

	linkedList_t *campaignList = NULL;
	*campaignDesc = '\0';
	for (i = 0; i < ccs.numCampaigns; i++) {
		if (ccs.campaigns[i].visible)
			LIST_AddString(&campaignList, va("%s", _(ccs.campaigns[i].name)));
	}
	/* default campaign */
	MN_RegisterText(TEXT_STANDARD, campaignDesc);
	MN_RegisterLinkedListText(TEXT_CAMPAIGN_LIST, campaignList);

	/* select main as default */
	for (i = 0; i < ccs.numCampaigns; i++)
		if (!strcmp(ccs.campaigns[i].id, "main")) {
			Cmd_ExecuteString(va("campaignlist_click %i", i));
			return;
		}
	Cmd_ExecuteString("campaignlist_click 0");
}

/**
 * @brief Script function to select a campaign from campaign list
 */
static void GAME_CP_CampaignListClick_f (void)
{
	int num;
	const char *racetype;
	menuNode_t *campaignlist;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <campaign list index>\n", Cmd_Argv(0));
		return;
	}

	/* Which campaign in the list? */
	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= ccs.numCampaigns)
		return;

	/* jump over all invisible campaigns */
	while (!ccs.campaigns[num].visible) {
		num++;
		if (num >= ccs.numCampaigns)
			return;
	}

	Cvar_Set("cp_campaign", ccs.campaigns[num].id);
	if (ccs.campaigns[num].team == TEAM_PHALANX)
		racetype = _("Human");
	else
		racetype = _("Aliens");

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("%s\n\nRace: %s\nRecruits: %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"), _(ccs.campaigns[num].name), racetype,
			ccs.campaigns[num].soldiers, ngettext("soldier", "soldiers", ccs.campaigns[num].soldiers),
			ccs.campaigns[num].scientists, ngettext("scientist", "scientists", ccs.campaigns[num].scientists),
			ccs.campaigns[num].workers, ngettext("worker", "workers", ccs.campaigns[num].workers),
			ccs.campaigns[num].credits, CP_ToDifficultyName(ccs.campaigns[num].difficulty),
			(int)(round(ccs.campaigns[num].minhappiness * 100.0f)), ccs.campaigns[num].negativeCreditsUntilLost,
			_(ccs.campaigns[num].text));
	MN_RegisterText(TEXT_STANDARD, campaignDesc);

	/* Highlight currently selected entry */
	campaignlist = MN_GetNodeByPath("campaigns.campaignlist");
	MN_TextNodeSelectLine(campaignlist, num);
}

/**
 * @brief Starts a new single-player game
 * @sa CP_CampaignInit
 * @sa CP_InitMarket
 */
static void GAME_CP_Start_f (void)
{
	/* get campaign - they are already parsed here */
	campaign_t *campaign = CL_GetCampaign(cp_campaign->string);
	if (!campaign)
		return;

	CP_CampaignInit(campaign, qfalse);

	/* Intro sentences */
	Cbuf_AddText("seq_start intro;\n");
}

void GAME_CP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	int i, j;
	/** @todo remove standalone ints if possible */
	int our_survivors, our_killed, our_stunned;
	int their_survivors, their_killed, their_stunned;
	int civilian_survivors, civilian_killed, civilian_stunned;
	int counts[MAX_MISSIONRESULTCOUNT];

	CP_ParseCharacterData(msg);

	our_survivors = our_killed = our_stunned = 0;
	their_survivors = their_killed = their_stunned = 0;
	civilian_survivors = civilian_killed = civilian_stunned = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		if (i == cls.team)
			our_survivors = numAlive[i];
		else if (i == TEAM_CIVILIAN)
			civilian_survivors = numAlive[i];
		else
			their_survivors += numAlive[i];
		for (j = 0; j < MAX_TEAMS; j++)
			if (j == cls.team) {
				our_killed += numKilled[i][j];
				our_stunned += numStunned[i][j]++;
			} else if (j == TEAM_CIVILIAN) {
				civilian_killed += numKilled[i][j];
				civilian_stunned += numStunned[i][j]++;
			} else {
				their_killed += numKilled[i][j];
				their_stunned += numStunned[i][j]++;
			}
	}
	/* if we won, our stunned are alive */
	if (winner == cls.team) {
		our_survivors += our_stunned;
		our_stunned = 0;
	} else
		/* if we lost, they revive stunned */
		their_stunned = 0;

	/* we won, and we're not the dirty aliens */
	if (winner == cls.team)
		civilian_survivors += civilian_stunned;
	else
		civilian_killed += civilian_stunned;

	/* loot the battlefield */
	AII_CollectingItems(ccs.missionAircraft, winner == cls.team);				/**< Collect items from the battlefield. */
	if (winner == cls.team)
		AL_CollectingAliens(ccs.missionAircraft);	/**< Collect aliens from the battlefield. */

	ccs.aliensKilled += their_killed;

	counts[MRC_ALIENS_KILLED] = their_killed;
	counts[MRC_ALIENS_STUNNED] = their_stunned;
	counts[MRC_ALIENS_SURVIVOR] = their_survivors;
	counts[MRC_PHALANX_KILLED] = our_killed - numKilled[cls.team][cls.team] - numKilled[TEAM_CIVILIAN][cls.team];
	counts[MRC_PHALANX_MIA] = our_stunned;
	counts[MRC_PHALANX_FF_KILLED] = numKilled[cls.team][cls.team] + numKilled[TEAM_CIVILIAN][cls.team];
	counts[MRC_PHALANX_SURVIVOR] = our_survivors;
	counts[MRC_CIVILIAN_KILLED] = civilian_killed;
	counts[MRC_CIVILIAN_FF_KILLED] = numKilled[cls.team][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
	counts[MRC_CIVILIAN_SURVIVOR] = civilian_survivors;
	counts[MRC_ITEM_GATHEREDTYPES] = ccs.missionResults.itemtypes;
	counts[MRC_ITEM_GATHEREDAMOUNT] = ccs.missionResults.itemamount;
	CP_InitMissionResults(counts, winner == cls.team);

	MN_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	CP_ExecuteMissionTrigger(ccs.selectedMission, winner == cls.team);

	if (winner == cls.team) {
		MN_PushMenu("won", NULL);
	} else
		MN_PushMenu("lost", NULL);

	SV_Shutdown("Mission end", qfalse);
	CL_Disconnect();
}

qboolean GAME_CP_Spawn (void)
{
	aircraft_t *aircraft = ccs.missionAircraft;
	base_t *base;
	int i;

	if (!aircraft)
		return qfalse;

	/* convert aircraft team to chr_list */
	for (i = 0, cl.chrList.num = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i]) {
			cl.chrList.chr[cl.chrList.num] = &aircraft->acTeam[i]->chr;
			cl.chrList.num++;
		}
	}

	base = CP_GetMissionBase();

	/* clean temp inventory */
	CL_CleanTempInventory(base);

	/* activate hud */
	MN_InitStack(mn_hud->string, NULL, qfalse, qtrue);

	return qtrue;
}

const mapDef_t* GAME_CP_MapInfo (int step)
{
	return &csi.mds[cls.currentSelectedMap];
}

qboolean GAME_CP_ItemIsUseable (const objDef_t *od)
{
	return RS_IsResearched_ptr(od->tech);
}

int GAME_CP_GetTeam (void)
{
	assert(ccs.curCampaign);
	return ccs.curCampaign->team;
}

qboolean GAME_CP_TeamIsKnown (const teamDef_t *teamDef)
{
	if (!ccs.teamDefTechs[teamDef->idx])
		ccs.teamDefTechs[teamDef->idx] = RS_GetTechByID(teamDef->tech);

	if (!ccs.teamDefTechs[teamDef->idx])
		Com_Error(ERR_DROP, "Could not find tech for teamdef '%s'", teamDef->tech);

	return RS_IsResearched_ptr(ccs.teamDefTechs[teamDef->idx]);
}

void GAME_CP_Drop (void)
{
	/** @todo maybe create a savegame? */
	MN_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	SV_Shutdown("Mission end", qfalse);
	CL_Disconnect();
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
	}
}

void GAME_CP_DisplayItemInfo (menuNode_t *node, const char *string)
{
	const aircraft_t *aircraft = AIR_GetAircraftSilent(string);
	if (aircraft) {
		assert(aircraft->tech);
		MN_DrawModelNode(node, aircraft->tech->mdl);
	} else {
		const technology_t *tech = RS_GetTechByProvided(string);
		if (tech)
			MN_DrawModelNode(node, tech->mdl);
		else
			Com_Printf("MN_ItemNodeDraw: Unknown item: '%s'\n", string);
	}
}

void GAME_CP_InitStartup (void)
{
	Cmd_AddCommand("cp_results", GAME_CP_Results_f, "Parses and shows the game results");
	Cmd_AddCommand("cp_missionauto_check", GAME_CP_MissionAutoCheck_f, "Checks whether this mission can be done automatically");
	Cmd_AddCommand("cp_mission_autogo", GAME_CP_MissionAutoGo_f, "Let the current selection mission be done automatically");
	Cmd_AddCommand("campaignlist_click", GAME_CP_CampaignListClick_f, NULL);
	Cmd_AddCommand("cp_getcampaigns", GAME_CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	Cmd_AddCommand("cp_start", GAME_CP_Start_f, "Start the new campaign");

	CP_InitStartup();

	cp_campaign = Cvar_Get("cp_campaign", "main", 0, "Which is the current selected campaign id");
	cp_start_employees = Cvar_Get("cp_start_employees", "1", CVAR_ARCHIVE, "Start with hired employees");
	/* cvars might have been changed by other gametypes */
	Cvar_ForceSet("sv_maxclients", "1");
	Cvar_ForceSet("sv_ai", "1");

	/* reset campaign data */
	CL_ResetSinglePlayerData();
	CL_ReadSinglePlayerData();
}

void GAME_CP_Shutdown (void)
{
	Cmd_RemoveCommand("cp_results");
	Cmd_RemoveCommand("cp_missionauto_check");
	Cmd_RemoveCommand("cp_mission_autogo");
	Cmd_RemoveCommand("campaignlist_click");
	Cmd_RemoveCommand("cp_getcampaigns");
	Cmd_RemoveCommand("cp_start");

	CP_CampaignExit();

	CL_ResetSinglePlayerData();

	SV_Shutdown("Quitting server.", qfalse);
}
