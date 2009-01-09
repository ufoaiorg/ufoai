/**
 * @file cl_game_multiplayer.c
 * @brief Multiplayer game type code
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
#include "cl_game.h"
#include "multiplayer/mp_callbacks.h"
#include "multiplayer/mp_serverlist.h"
#include "cl_team.h"
#include "cl_team_multiplayer.h"
#include "menu/m_popup.h"

static aircraft_t multiplayerFakeAircraft;

static void GAME_MP_AutoTeam_f (void)
{
	int i;

	if (!GAME_IsMultiplayer())
		return;

	for (i = 0; i < MAX_ACTIVETEAM; i++) {
		employee_t *employee = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
		equipDef_t *ed = INV_GetEquipmentDefinitionByID(cl_equip->string);
		CL_AssignSoldierToAircraft(employee, cls.missionaircraft);
		B_PackInitialEquipment(cls.missionaircraft, ed);
	}
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void GAME_MP_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;

	if (!GAME_IsMultiplayer())
		return;

	if (!sv_dedicated->integer && !B_GetNumOnTeam(cls.missionaircraft)) {
		CL_ResetMultiplayerTeamInAircraft(cls.missionaircraft);
		Cvar_Set("mn_teamname", _("NewTeam"));
		B_AssignInitial(cls.missionaircraft, cl_equip->string);
		MN_PushMenu("team", NULL);
		MN_Popup(_("No team assigned"), _("Please choose and equip your team first"));
		return;
	}

	if (Cvar_VariableInteger("sv_teamplay")
	 && Cvar_VariableValue("sv_maxsoldiersperplayer") > Cvar_VariableValue("sv_maxsoldiersperteam")) {
		MN_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = &csi.mds[cls.currentSelectedMap];
	if (!md || !md->multiplayer)
		return;
	assert(md->map);

	Com_sprintf(map, sizeof(map), "map %s %s %s", mn_serverday->integer ? "day" : "night", md->map, md->param ? md->param : "");

	/* let the (local) server know which map we are running right now */
	csi.currentMD = md;

	Cmd_ExecuteString(map);

	Cvar_Set("mn_main", "multiplayerInGame");
	MN_PushMenu("multiplayer_wait", NULL);
	Cvar_Set("mn_active", "multiplayer_wait");
}

/**
 * @brief Update the menu values with current gametype values
 */
static void GAME_MP_UpdateGametype_f (void)
{
	Com_SetGameType();
}

/**
 * @brief Switch to the next multiplayer game type
 * @sa MN_PrevGametype_f
 */
static void GAME_MP_ChangeGametype_f (void)
{
	int i, newType;
	const mapDef_t *md;
	const char *newGameTypeID = NULL;
	qboolean next = qtrue;

	/* no types defined or parsed */
	if (numGTs == 0)
		return;

	md = &csi.mds[cls.currentSelectedMap];
	if (!md || !md->multiplayer) {
		Com_Printf("MN_ChangeGametype_f: No mapdef for the map\n");
		return;
	}

	/* previous? */
	if (!Q_strcmp(Cmd_Argv(0), "mn_prevgametype")) {
		next = qfalse;
	}

	if (md->gameTypes) {
		linkedList_t *list = md->gameTypes;
		linkedList_t *old = NULL;
		while (list) {
			if (!Q_strcmp((const char*)list->data, sv_gametype->string)) {
				if (next) {
					if (list->next)
						newGameTypeID = (const char *)list->next->data;
					else
						newGameTypeID = (const char *)md->gameTypes->data;
				} else {
					/* only one or the first entry */
					if (old) {
						newGameTypeID = (const char *)old->data;
					} else {
						while (list->next)
							list = list->next;
						newGameTypeID = (const char *)list->data;
					}
				}
				break;
			}
			old = list;
			list = list->next;
		}
		/* current value is not valid for this map, set to first valid gametype */
		if (!list)
			newGameTypeID = (const char *)md->gameTypes->data;
	} else {
		for (i = 0; i < numGTs; i++) {
			const gametype_t *gt = &gts[i];
			if (!Q_strncmp(gt->id, sv_gametype->string, MAX_VAR)) {
				if (next) {
					newType = (i + 1);
					if (newType >= numGTs)
						newType = 0;
				} else {
					newType = (i - 1);
					if (newType < 0)
						newType = numGTs - 1;
				}

				newGameTypeID = gts[newType].id;
				break;
			}
		}
	}
	if (newGameTypeID) {
		Cvar_Set("sv_gametype", newGameTypeID);
		Com_SetGameType();
	}
}

void GAME_MP_Results (int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	char resultText[MAX_SMALLMENUTEXTLEN];
	int their_killed, their_stunned;
	int i;

	if (winner == 0) {
		Q_strncpyz(popupText, _("The game was a draw!\n\nNo survivors left on any side."), sizeof(popupText));
		MN_Popup(_("Game Drawn!"), popupText);
		return;
	}

	their_killed = their_stunned = 0;
	for (i = 0; i < MAX_TEAMS; i++) {
		if (i != cls.team) {
			their_killed += numKilled[cls.team][i];
			their_stunned += numStunned[cls.team][i];
		}
	}

	Com_sprintf(resultText, sizeof(resultText), _("\n\nEnemies killed:  %i\nTeam survivors:  %i"), their_killed + their_stunned, numAlive[cls.team]);
	if (winner == cls.team) {
		Com_sprintf(popupText, lengthof(popupText), "%s%s", _("You won the game!"), resultText);
		MN_Popup(_("Congratulations"), popupText);
	} else {
		Com_sprintf(popupText, lengthof(popupText), "%s%s", _("You've lost the game!"), resultText);
		MN_Popup(_("Better luck next time"), popupText);
	}
}

qboolean GAME_MP_Spawn (chrList_t *chrList)
{
	const int n = cl_teamnum->integer;

	if (!teamData.parsed) {
		Com_Printf("GAME_MP_Spawn: teaminfo unparsed\n");
		return qfalse;
	}

	/* we are already connected and in this list */
	if (n <= TEAM_CIVILIAN || teamData.maxplayersperteam < teamData.teamCount[n]) {
		mn.menuText[TEXT_STANDARD] = _("Invalid or full team");
		Com_Printf("GAME_MP_Spawn: Invalid or full team %i\n"
			"  maxplayers per team: %i - players on team: %i",
			n, teamData.maxplayersperteam, teamData.teamCount[n]);
		return qfalse;
	}

	MN_PushMenu("multiplayer_wait", NULL);
	return qtrue;
}

void GAME_MP_InitStartup (void)
{
	const char *max_s = Cvar_VariableStringOld("sv_maxsoldiersperteam");
	const char *max_spp = Cvar_VariableStringOld("sv_maxsoldiersperplayer");

	Cvar_ForceSet("sv_maxclients", "2");

	Cmd_AddCommand("mp_startserver", GAME_MP_StartServer_f, NULL);
	Cmd_AddCommand("mp_updategametype", GAME_MP_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mp_nextgametype", GAME_MP_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mp_prevgametype", GAME_MP_ChangeGametype_f, "Switch to the previous multiplayer game type");
	Cmd_AddCommand("mp_autoteam", GAME_MP_AutoTeam_f, "Assign initial multiplayer equipment to soldiers");
	MP_CallbacksInit();
	MP_ServerListInit();

	/* restore old sv_maxsoldiersperplayer and sv_maxsoldiersperteam
	 * cvars if values were previously set */
	if (strlen(max_s))
		Cvar_Set("sv_maxsoldiersperteam", max_s);
	if (strlen(max_spp))
		Cvar_Set("sv_maxsoldiersperplayer", max_spp);

	RS_ResetTechs();

	cls.missionaircraft = &multiplayerFakeAircraft;
	memset(&multiplayerFakeAircraft, 0, sizeof(multiplayerFakeAircraft));
	gd.numAircraft = 1;

	/* disconnect already running session - when entering the
	 * multiplayer menu while you are still connected */
	if (cls.state >= ca_connecting)
		CL_Disconnect();
}

void GAME_MP_Shutdown (void)
{
	/* shutdown server */
	SV_Shutdown("Quitting multiplayer.", qfalse);
	/* or disconnect from server */
	CL_Disconnect();

	Cmd_RemoveCommand("mp_startserver");
	Cmd_RemoveCommand("mp_updategametype");
	Cmd_RemoveCommand("mp_nextgametype");
	Cmd_RemoveCommand("mp_prevgametype");
	Cmd_RemoveCommand("mp_autoteam");
	MP_CallbacksShutdown();
	MP_ServerListShutdown();

	memset(&teamData, 0, sizeof(teamData));
}
