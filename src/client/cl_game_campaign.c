/**
 * @file cl_game_campaign.c
 * @brief Singleplayer campaign game type code
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
#include "cl_le.h"
#include "cl_game.h"
#include "cl_team.h"
#include "cl_menu.h"
#include "campaign/cl_campaign.h"
#include "campaign/cp_missions.h"
#include "campaign/cp_mission_triggers.h"
#include "campaign/cp_team.h"
#include "campaign/cl_ufo.h"
#include "menu/m_nodes.h"

/**
 * @brief Checks whether a campaign mode game is running
 */
qboolean GAME_CP_IsRunning (void)
{
	if (curCampaign && !GAME_IsCampaign())
		Sys_Error("curCampaign and game mode out of sync");
	return curCampaign != NULL;
}

/**
 * @sa GAME_CP_MissionAutoCheck_f
 * @sa CP_StartSelectedMission
 */
static void GAME_CP_MissionAutoGo_f (void)
{
	if (!GAME_CP_IsRunning() || !ccs.selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoGo_f: No update after automission\n");
		return;
	}

	if (!cls.missionaircraft) {
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
	if (!GAME_CP_IsRunning() || !ccs.selectedMission || !ccs.interceptAircraft) {
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
	Com_DPrintf(DEBUG_CLIENT, "GAME_CP_Results_f\n");

	/* multiplayer? */
	if (!GAME_CP_IsRunning() || !ccs.selectedMission)
		return;

	/* check for replay */
	if (Cvar_VariableInteger("cp_mission_tryagain")) {
		/* don't collect things and update stats --- we retry the mission */
		CP_StartSelectedMission();
		return;
	}
	/* check for win */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <won>\n", Cmd_Argv(0));
		return;
	}

	CP_MissionEnd(ccs.selectedMission, atoi(Cmd_Argv(1)));
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
		Sys_Error("Unknown difficulty id %i\n", difficulty);
	}
}

#define MAXCAMPAIGNTEXT 4096
static char campaignText[MAXCAMPAIGNTEXT];
static char campaignDesc[MAXCAMPAIGNTEXT];
/**
 * @brief Fill the campaign list with available campaigns
 */
static void GAME_CP_GetCampaigns_f (void)
{
	int i;

	*campaignText = *campaignDesc = '\0';
	for (i = 0; i < numCampaigns; i++) {
		if (campaigns[i].visible)
			Q_strcat(campaignText, va("%s\n", _(campaigns[i].name)), MAXCAMPAIGNTEXT);
	}
	/* default campaign */
	MN_RegisterText(TEXT_STANDARD, campaignDesc);
	MN_RegisterText(TEXT_CAMPAIGN_LIST, campaignText);

	/* select main as default */
	for (i = 0; i < numCampaigns; i++)
		if (!Q_strcmp(campaigns[i].id, "main")) {
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
	if (campaigns[num].team == TEAM_PHALANX)
		racetype = _("Human");
	else
		racetype = _("Aliens");

	Com_sprintf(campaignDesc, sizeof(campaignDesc), _("%s\n\nRace: %s\nRecruits: %i %s, %i %s, %i %s\n"
		"Credits: %ic\nDifficulty: %s\n"
		"Min. happiness of nations: %i %%\n"
		"Max. allowed debts: %ic\n"
		"%s\n"), _(campaigns[num].name), racetype,
			campaigns[num].soldiers, ngettext("soldier", "soldiers", campaigns[num].soldiers),
			campaigns[num].scientists, ngettext("scientist", "scientists", campaigns[num].scientists),
			campaigns[num].workers, ngettext("worker", "workers", campaigns[num].workers),
			campaigns[num].credits, CP_ToDifficultyName(campaigns[num].difficulty),
			(int)(round(campaigns[num].minhappiness * 100.0f)), campaigns[num].negativeCreditsUntilLost,
			_(campaigns[num].text));
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
	curCampaign = CL_GetCampaign(cl_campaign->string);
	if (!curCampaign)
		return;

	CP_CampaignInit(qfalse);

	/* Intro sentences */
	Cbuf_AddText("seq_start intro;\n");
}

/**
 * @brief Function to issue Try Again a Mission.
 * @note Command to call this: cp_tryagain.
 */
static void GAME_CP_TryAgain_f (void)
{
	/* Do nothing if user did other stuff. */
	if (!ccs.mission_tryagain)
		return;
	Cvar_Set("cp_mission_tryagain", "1");
	MN_PopMenu(qfalse);
}

void GAME_CP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	static char resultText[MAX_SMALLMENUTEXTLEN];
	int i, j;
	int our_survivors, our_killed, our_stunned;
	int their_survivors, their_killed, their_stunned;
	int civilian_survivors, civilian_killed, civilian_stunned;

	CP_ParseCharacterData(msg);

	/* init result text */
	MN_RegisterText(TEXT_STANDARD, resultText);

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
	AII_CollectingItems(cls.missionaircraft, winner == cls.team);				/**< Collect items from the battlefield. */
	if (winner == cls.team)
		AL_CollectingAliens(cls.missionaircraft);	/**< Collect aliens from the battlefield. */

	/* clear unused LE inventories */
	LE_Cleanup();

	/* needs to be cleared and then append to it */
	Com_sprintf(resultText, sizeof(resultText), _("Aliens killed\t%i\n"), their_killed);
	ccs.aliensKilled += their_killed;

	Q_strcat(resultText, va(_("Aliens captured\t%i\n"), their_stunned), sizeof(resultText));
	Q_strcat(resultText, va(_("Alien survivors\t%i\n\n"), their_survivors), sizeof(resultText));

	/* team stats */
	Q_strcat(resultText, va(_("PHALANX soldiers killed by Aliens\t%i\n"), our_killed - numKilled[cls.team][cls.team] - numKilled[TEAM_CIVILIAN][cls.team]), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX soldiers missing in action\t%i\n"), our_stunned), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX friendly fire losses\t%i\n"), numKilled[cls.team][cls.team] + numKilled[TEAM_CIVILIAN][cls.team]), sizeof(resultText));
	Q_strcat(resultText, va(_("PHALANX survivors\t%i\n\n"), our_survivors), sizeof(resultText));

	Q_strcat(resultText, va(_("Civilians killed by Aliens\t%i\n"), civilian_killed), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians killed by friendly fire\t%i\n"), numKilled[cls.team][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN]), sizeof(resultText));
	Q_strcat(resultText, va(_("Civilians saved\t%i\n\n"), civilian_survivors), sizeof(resultText));
	Q_strcat(resultText, va(_("Gathered items (types/all)\t%i/%i\n"), missionresults.itemtypes,  missionresults.itemamount), sizeof(resultText));

	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	MN_PushMenu("map", NULL);

	/* Make sure that at this point we are able to 'Try Again' a mission. */
	ccs.mission_tryagain = qtrue;
	CP_ExecuteMissionTrigger(ccs.selectedMission, winner == cls.team);

	if (winner == cls.team) {
		/* We need to update menu 'won' with UFO recovery stuff. */
		if (missionresults.recovery) {
			if (missionresults.crashsite)
				Q_strcat(resultText, va(_("\nSecured crashed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
			else
				Q_strcat(resultText, va(_("\nSecured landed %s\n"), UFO_TypeToName(missionresults.ufotype)), sizeof(resultText));
		}
		MN_PushMenu("won", NULL);
	} else
		MN_PushMenu("lost", NULL);

	SV_Shutdown("Mission end", qfalse);
	CL_Disconnect();
}

qboolean GAME_CP_Spawn (void)
{
	aircraft_t *aircraft = cls.missionaircraft;
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
	MN_PushMenu(mn_hud->string, NULL);
	Cvar_Set("mn_active", mn_hud->string);

	return qtrue;
}

void GAME_CP_InitStartup (void)
{
	Cmd_AddCommand("cp_results", GAME_CP_Results_f, "Parses and shows the game results");
	Cmd_AddCommand("cp_missionauto_check", GAME_CP_MissionAutoCheck_f, "Checks whether this mission can be done automatically");
	Cmd_AddCommand("cp_mission_autogo", GAME_CP_MissionAutoGo_f, "Let the current selection mission be done automatically");
	Cmd_AddCommand("campaignlist_click", GAME_CP_CampaignListClick_f, NULL);
	Cmd_AddCommand("cp_getcampaigns", GAME_CP_GetCampaigns_f, "Fill the campaign list with available campaigns");
	Cmd_AddCommand("cp_start", GAME_CP_Start_f, "Start the new campaign");
	Cmd_AddCommand("cp_exit", CP_CampaignExit, "Stop the current running campaign");
	Cmd_AddCommand("cp_tryagain", GAME_CP_TryAgain_f, "Try again a mission");

	CP_InitStartup();

	/* cvars might have been changed by other gametypes */
	Cvar_ForceSet("sv_maxclients", "1");
	Cvar_ForceSet("sv_ai", "1");

	/* reset sv_maxsoldiersperplayer and sv_maxsoldiersperteam to default values */
	/** @todo do we have to set sv_maxsoldiersperteam for campaign mode? */
	if (Cvar_VariableInteger("sv_maxsoldiersperteam") != MAX_ACTIVETEAM / 2)
		Cvar_SetValue("sv_maxsoldiersperteam", MAX_ACTIVETEAM / 2);
	if (Cvar_VariableInteger("sv_maxsoldiersperplayer") != MAX_ACTIVETEAM)
		Cvar_SetValue("sv_maxsoldiersperplayer", MAX_ACTIVETEAM);

	/* reset campaign data */
	CL_ResetSinglePlayerData();
	CL_ReadSinglePlayerData();
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
	assert(curCampaign);
	return curCampaign->team;
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
		Com_sprintf(buf, sizeof(buf), _("Rank: %s"), _(ccs.ranks[chr->score.rank].name));
		Cvar_Set("mn_chrrank", buf);
		Cvar_Set("mn_chrrank_img", ccs.ranks[chr->score.rank].image);
	}
}

void GAME_CP_Shutdown (void)
{
	Cmd_RemoveCommand("cp_results");

	/* exit running game */
	CP_CampaignExit();

	CL_ResetSinglePlayerData();
}
