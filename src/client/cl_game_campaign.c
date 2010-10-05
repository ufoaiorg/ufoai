/**
 * @file cl_game_campaign.c
 * @brief Singleplayer campaign game type code
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "cl_game.h"
#include "cl_team.h"
#include "campaign/cp_campaign.h"
#include "campaign/cp_missions.h"
#include "campaign/cp_mission_triggers.h"
#include "campaign/cp_team.h"
#include "campaign/cp_map.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "ui/node/ui_node_model.h"
#include "ui/node/ui_node_text.h"

/**
 * @sa GAME_CP_MissionAutoCheck_f
 * @sa CP_StartSelectedMission
 */
static void GAME_CP_MissionAutoGo_f (void)
{
	mission_t *mission = ccs.selectedMission;
	missionResults_t *results = &ccs.missionResults;

	if (!mission) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoGo_f: No update after automission\n");
		return;
	}

	if (!ccs.missionAircraft) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No aircraft at target pos\n");
		return;
	}

	if (!ccs.interceptAircraft) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No intercept aircraft given\n");
		return;
	}

	UI_PopWindow(qfalse);

	if (mission->stage != STAGE_BASE_ATTACK) {
		if (!mission->active) {
			MS_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mission->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("cp_mission_autogo_available", "0");
			return;
		}
	}

	/* start the map */
	CP_CreateBattleParameters(mission, &ccs.battleParameters);

	results->won = qfalse;
	CL_GameAutoGo(mission, ccs.interceptAircraft, &ccs.battleParameters, results);

	if (results->won) {
		Cvar_SetValue("mn_autogo", 1);
		UI_PushWindow("won", NULL);
		MS_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	} else {
		UI_PushWindow("lost", NULL);
		MS_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
	}

	MAP_ResetAction();
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
		const campaign_t *c = &ccs.campaigns[i];
		if (c->visible)
			LIST_AddString(&campaignList, va("%s", _(c->name)));
	}
	/* default campaign */
	UI_RegisterText(TEXT_STANDARD, campaignDesc);
	UI_RegisterLinkedListText(TEXT_CAMPAIGN_LIST, campaignList);

	/* select main as default */
	for (i = 0; i < ccs.numCampaigns; i++) {
		const campaign_t *c = &ccs.campaigns[i];
		if (!strcmp(c->id, "main")) {
			Cmd_ExecuteString(va("campaignlist_click %i", i));
			return;
		}
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
	uiNode_t *campaignlist;

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
	UI_RegisterText(TEXT_STANDARD, campaignDesc);

	/* Highlight currently selected entry */
	campaignlist = UI_GetNodeByPath("campaigns.campaignlist");
	UI_TextNodeSelectLine(campaignlist, num);
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
	Cbuf_AddText("seq_start intro\n");
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
void GAME_CP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	int i, j;
	int ownSurvived, ownKilled, ownStunned;
	int aliensSurvived, aliensKilled, aliensStunned;
	int civiliansSurvived, civiliansKilled, civiliansStunned;
	const qboolean won = (winner == cls.team);

	CP_ParseCharacterData(msg);

	ownSurvived = ownKilled = ownStunned = 0;
	aliensSurvived = aliensKilled = aliensStunned = 0;
	civiliansSurvived = civiliansKilled = civiliansStunned = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		if (i == cls.team)
			ownSurvived = numAlive[i];
		else if (i == TEAM_CIVILIAN)
			civiliansSurvived = numAlive[i];
		else
			aliensSurvived += numAlive[i];
		for (j = 0; j < MAX_TEAMS; j++)
			if (j == cls.team) {
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
	AII_CollectingItems(ccs.missionAircraft, won);
	if (won)
		/* Collect aliens from the battlefield. */
		AL_CollectingAliens(ccs.missionAircraft);

	ccs.aliensKilled += aliensKilled;

	ccs.missionResults.won = won;
	ccs.missionResults.aliensKilled = aliensKilled;
	ccs.missionResults.aliensStunned = aliensStunned;
	ccs.missionResults.aliensSurvived = aliensSurvived;
	ccs.missionResults.ownKilled = ownKilled - numKilled[cls.team][cls.team] - numKilled[TEAM_CIVILIAN][cls.team];
	ccs.missionResults.ownStunned = ownStunned;
	ccs.missionResults.ownKilledFriendlyFire = numKilled[cls.team][cls.team] + numKilled[TEAM_CIVILIAN][cls.team];
	ccs.missionResults.ownSurvived = ownSurvived;
	ccs.missionResults.civiliansKilled = civiliansKilled;
	ccs.missionResults.civiliansKilledFriendlyFire = numKilled[cls.team][TEAM_CIVILIAN] + numKilled[TEAM_CIVILIAN][TEAM_CIVILIAN];
	ccs.missionResults.civiliansSurvived = civiliansSurvived;

	CP_InitMissionResults(won, &ccs.missionResults);

	UI_InitStack("geoscape", "campaign_main", qtrue, qtrue);

	CP_ExecuteMissionTrigger(ccs.selectedMission, won);

	if (won)
		UI_PushWindow("won", NULL);
	else
		UI_PushWindow("lost", NULL);

	CL_Disconnect();
	SV_Shutdown("Mission end", qfalse);
}

qboolean GAME_CP_Spawn (void)
{
	aircraft_t *aircraft = ccs.missionAircraft;
	base_t *base;
	linkedList_t* l;

	if (!aircraft)
		return qfalse;

	/* convert aircraft team to character list */
	for (l = aircraft->acTeam; l != NULL; l = l->next) {
		employee_t *employee = (employee_t *)l->data;
		cl.chrList.chr[cl.chrList.num] = &employee->chr;
		cl.chrList.num++;
	}

	base = aircraft->homebase;

	/* clean temp inventory */
	CL_CleanTempInventory(base);

	/* activate hud */
	UI_InitStack(mn_hud->string, NULL, qfalse, qtrue);

	return qtrue;
}

const mapDef_t* GAME_CP_MapInfo (int step)
{
	return Com_GetMapDefByIDX(cls.currentSelectedMap);
}

qboolean GAME_CP_ItemIsUseable (const objDef_t *od)
{
	const technology_t *tech = RS_GetTechForItem(od);
	return RS_IsResearched_ptr(tech);
}

int GAME_CP_GetTeam (void)
{
	assert(ccs.curCampaign);
	return ccs.curCampaign->team;
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
	CL_Disconnect();
}

void GAME_CP_Frame (void)
{
	/* don't run the campaign in console mode */
	if (cls.keyDest == key_console)
		return;

	/* advance time */
	CL_CampaignRun();
}

/**
 * @brief Sets the item model for an object assigned technology. If no
 * technology is set or the technology has no model assigned, this function
 * is returning @c NULL to tell the caller to use the default object model.
 * @param[in] od The object definition to get the model from.
 * @param[out] menuModel The menu model pointer.
 * @return The model path for the item. Never @c NULL.
 */
const char* GAME_CP_GetModelForItem (const objDef_t *od)
{
	const technology_t *tech = RS_GetTechForItem(od);
	if (tech->mdl) {
		/* the model from the tech structure has higher priority, because the item model itself
		 * is mainly for the battlescape or the geoscape - only use that as a fallback */
		return tech->mdl;
	}
	return NULL;
}

/**
 * @brief Changes some actor states for a campaign game
 * @param team The team to change the states for
 */
void GAME_CP_InitializeBattlescape (const chrList_t *team)
{
	int i;
	struct dbuffer *msg = new_dbuffer();

	NET_WriteByte(msg, clc_initactorstates);
	NET_WriteByte(msg, team->num);

	for (i = 0; i < team->num; i++) {
		const character_t *chr = team->chr[i];
		NET_WriteShort(msg, chr->ucn);
		NET_WriteShort(msg, chr->state);
	}

	NET_WriteMsg(cls.netStream, msg);
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
	} else {
		Cvar_Set("mn_chrrank_img", "");
		Cvar_Set("mn_chrrank", "");
	}

	Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ENEMIES]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));
}

void GAME_CP_DisplayItemInfo (uiNode_t *node, const char *string)
{
	const aircraft_t *aircraft = AIR_GetAircraftSilent(string);
	if (aircraft) {
		assert(aircraft->tech);
		UI_DrawModelNode(node, aircraft->tech->mdl);
	} else {
		const technology_t *tech = RS_GetTechByProvided(string);
		if (tech)
			UI_DrawModelNode(node, tech->mdl);
		else
			Com_Printf("UI_ItemNodeDraw: Unknown item: '%s'\n", string);
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
