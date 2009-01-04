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
#include "cl_global.h"
#include "cl_game_campaign.h"
#include "campaign/cp_missions.h"

/**
 * @brief Checks whether a campaign mode game is running
 */
qboolean GAME_CP_IsRunning (void)
{
	return curCampaign != NULL;
}

/**
 * @sa CL_GameAutoCheck_f
 * @sa CL_GameGo
 */
static void CL_GameAutoGo_f (void)
{
	if (!GAME_CP_IsRunning() || !selectedMission) {
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
 * @brief Checks whether you have to play this mission
 * You can mark a mission as story related.
 * If a mission is story related the cvar @c game_autogo is set to @c 0
 * If this cvar is @c 1 - the mission dialog will have a auto mission button
 * @sa CL_GameAutoGo_f
 */
static void CL_GameAutoCheck_f (void)
{
	if (!GAME_CP_IsRunning() || !selectedMission || !gd.interceptAircraft) {
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
 * @sa CL_ParseResults
 * @sa CL_ParseCharacterData
 * @sa CL_GameAbort_f
 */
static void CL_GameResults_f (void)
{
	Com_DPrintf(DEBUG_CLIENT, "CL_GameResults_f\n");

	/* multiplayer? */
	if (!GAME_CP_IsRunning() || !selectedMission)
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

	CP_MissionEnd(selectedMission, atoi(Cmd_Argv(1)));
}

void GAME_CP_InitStartup (void)
{
	Cmd_AddCommand("game_results", CL_GameResults_f, "Parses and shows the game results");
	Cmd_AddCommand("game_auto_check", CL_GameAutoCheck_f, "Checks whether this mission can be done automatically");
	Cmd_AddCommand("game_auto_go", CL_GameAutoGo_f, "Let the current selection mission be done automatically");

	if (Com_ServerState()) {
		/* shutdown server */
		SV_Shutdown("Server was killed (switched to singleplayer).", qfalse);
	} else if (cls.state >= ca_connecting) {
		Com_Printf("Disconnect from current server\n");
		CL_Disconnect();
	}
	Cvar_ForceSet("sv_maxclients", "1");

	/* reset sv_maxsoldiersperplayer and sv_maxsoldiersperteam to default values */
	/** @todo these should probably default to something bigger */
	if (Cvar_VariableInteger("sv_maxsoldiersperteam") != 4)
		Cvar_SetValue("sv_maxsoldiersperteam", 4);
	if (Cvar_VariableInteger("sv_maxsoldiersperplayer") != 8)
		Cvar_SetValue("sv_maxsoldiersperplayer", 8);
}

void GAME_CP_Shutdown (void)
{
	Cmd_RemoveCommand("game_results");

	if (curCampaign)
		CL_GameExit();
}
