/**
 * @file cl_game_multiplayer.c
 * @brief Multiplayer game type code
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
#include "../cl_game.h"
#include "cl_game_multiplayer.h"
#include "mp_callbacks.h"
#include "mp_serverlist.h"
#include "../../ui/ui_data.h"

static const cgame_import_t *cgi;

CGAME_HARD_LINKED_FUNCTIONS

static void GAME_MP_StartBattlescape (qboolean isTeamPlay)
{
	cgi->UI_ExecuteConfunc("multiplayer_setTeamplay %i", isTeamPlay);
	cgi->UI_InitStack("multiplayer_wait", NULL, qtrue, qtrue);
}

static void GAME_MP_NotifyEvent (event_t eventType)
{
	if (eventType == EV_RESET)
		cgi->HUD_InitUI("multiplayerInGame", qtrue);
}

static void GAME_MP_EndRoundAnnounce (int playerNum, int team)
{
	char buf[128];

	/* it was our own turn */
	if (cgi->CL_GetPlayerNum() == playerNum) {
		Com_sprintf(buf, sizeof(buf), _("You've ended your turn.\n"));
	} else {
		const char *playerName = cgi->CL_PlayerGetName(playerNum);
		Com_sprintf(buf, sizeof(buf), _("%s ended his turn (team %i).\n"), playerName, team);
	}
	/* add translated message to chat buffer */
	cgi->HUD_DisplayMessage(buf);
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void GAME_MP_StartServer_f (void)
{
	char map[MAX_VAR];
	const mapDef_t *md;

	if (!cgi->Cvar_GetInteger("sv_dedicated") && cgi->GAME_IsTeamEmpty())
		cgi->GAME_AutoTeam("multiplayer_initial", cgi->GAME_GetCharacterArraySize());

	if (cgi->Cvar_GetInteger("sv_teamplay")
	 && cgi->Cvar_GetValue("sv_maxsoldiersperplayer") > cgi->Cvar_GetValue("sv_maxsoldiersperteam")) {
		cgi->UI_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = cgi->GAME_GetCurrentSelectedMap();
	if (!md || !md->multiplayer)
		return;
	assert(md->map);

	Com_sprintf(map, sizeof(map), "map %s %s %s", cgi->Cvar_GetInteger("mn_serverday") ? "day" : "night", md->map, md->param ? md->param : "");

	/** @todo implement different ufo and dropship support for multiplayer, too (see skirmish) */
	cgi->Cvar_Set("rm_drop", "");
	cgi->Cvar_Set("rm_ufo", "");

	if (md->hurtAliens)
		cgi->Cvar_Set("sv_hurtaliens", "1");
	else
		cgi->Cvar_Set("sv_hurtaliens", "0");

	if (md->teams)
		cgi->Cvar_SetValue("sv_maxteams", md->teams);
	else
		cgi->Cvar_SetValue("sv_maxteams", 2);

	cgi->Cmd_ExecuteString(map);

	cgi->UI_InitStack("multiplayer_wait", "multiplayerInGame", qfalse, qtrue);
}

/**
 * @brief Update the menu values with current gametype values
 */
static void GAME_MP_UpdateGametype_f (void)
{
	cgi->Com_SetGameType();
}

/**
 * @brief Switch to the next multiplayer game type
 * @sa UI_PrevGametype_f
 */
static void GAME_MP_ChangeGametype_f (void)
{
	const mapDef_t *md;
	const char *newGameTypeID = NULL;
	qboolean next = qtrue;
	const int numGTs = cgi->csi->numGTs;
	const char *gameType = cgi->Cvar_GetString("sv_gametype");

	/* no types defined or parsed */
	if (numGTs == 0)
		return;

	md = cgi->GAME_GetCurrentSelectedMap();
	if (!md || !md->multiplayer) {
		cgi->Com_Printf("UI_ChangeGametype_f: No mapdef for the map\n");
		return;
	}

	/* previous? */
	if (Q_streq(cgi->Cmd_Argv(0), "mp_prevgametype")) {
		next = qfalse;
	}

	if (md->gameTypes) {
		linkedList_t *list = md->gameTypes;
		linkedList_t *old = NULL;
		while (list) {
			if (Q_streq((const char*)list->data, gameType)) {
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
		int i;
		for (i = 0; i < numGTs; i++) {
			const gametype_t *gt = &cgi->csi->gts[i];
			if (Q_streq(gt->id, gameType)) {
				int newType;
				if (next) {
					newType = (i + 1);
					if (newType >= numGTs)
						newType = 0;
				} else {
					newType = (i - 1);
					if (newType < 0)
						newType = numGTs - 1;
				}

				newGameTypeID = cgi->csi->gts[newType].id;
				break;
			}
		}
	}
	if (newGameTypeID) {
		cgi->Cvar_Set("sv_gametype", newGameTypeID);
		cgi->Com_SetGameType();
	}
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
static void GAME_MP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], qboolean nextmap)
{
	linkedList_t *list = NULL;
	int enemiesKilled, enemiesStunned;
	int i;
	const int team = cgi->GAME_GetCurrentTeam();

	enemiesKilled = enemiesStunned = 0;
	for (i = 0; i < MAX_TEAMS; i++) {
		if (i != team) {
			enemiesKilled += numKilled[team][i];
			enemiesStunned += numStunned[team][i];
		}
	}

	cgi->LIST_AddString(&list, va(_("Enemies killed:\t%i"), enemiesKilled + enemiesStunned));
	cgi->LIST_AddString(&list, va(_("Team survivors:\t%i"), numAlive[team]));
	cgi->UI_RegisterLinkedListText(TEXT_LIST2, list);
	if (winner == team) {
		cgi->UI_PushWindow("won", NULL, NULL);
	} else {
		cgi->UI_PushWindow("lost", NULL, NULL);
	}
}

static const mapDef_t* GAME_MP_MapInfo (int step)
{
	const mapDef_t *md;
	int i = 0;

	while (!cgi->GAME_GetCurrentSelectedMap()->multiplayer) {
		i++;
		cgi->GAME_SwitchCurrentSelectedMap(step ? step : 1);
		if (i > 100000)
			cgi->Com_Error(ERR_DROP, "no multiplayer map found");
	}

	md = cgi->GAME_GetCurrentSelectedMap();

	if (md->gameTypes) {
		char buf[256] = "";
		LIST_Foreach(md->gameTypes, char, gameTypeStr) {
			Q_strcat(buf, va("%s ", gameTypeStr), sizeof(buf));
		}
		cgi->Cvar_Set("mn_mapgametypes", buf);
	} else {
		cgi->Cvar_Set("mn_mapgametypes", _("all"));
	}

	return md;
}


static linkedList_t *mp_chatMessageStack = NULL;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
static void GAME_MP_AddChatMessage (const char *text)
{
	char message[2048];
	const char *msg;
	Q_strncpyz(message, text, sizeof(message));

	msg = Com_Trim(message);
	cgi->LIST_AddString(&mp_chatMessageStack, msg);
	cgi->HUD_DisplayMessage(msg);
	cgi->UI_RegisterLinkedListText(TEXT_CHAT_WINDOW, mp_chatMessageStack);
	cgi->UI_TextScrollEnd("hud_chat.allchats.chatscreen.chat");
}

static qboolean GAME_MP_HandleServerCommand (const char *command, struct dbuffer *msg)
{
	/* teaminfo command */
	if (Q_streq(command, "teaminfo")) {
		CL_ParseTeamInfoMessage(msg);
		return qtrue;
	}

	return qfalse;
}

static void GAME_MP_InitStartup (void)
{
	const char *max_s = cgi->Cvar_VariableStringOld("sv_maxsoldiersperteam");
	const char *max_spp = cgi->Cvar_VariableStringOld("sv_maxsoldiersperplayer");

	cgi->Cvar_ForceSet("sv_maxclients", "2");

	cgi->Cmd_AddCommand("mp_startserver", GAME_MP_StartServer_f, NULL);
	cgi->Cmd_AddCommand("mp_updategametype", GAME_MP_UpdateGametype_f, "Update the menu values with current gametype values");
	cgi->Cmd_AddCommand("mp_nextgametype", GAME_MP_ChangeGametype_f, "Switch to the next multiplayer game type");
	cgi->Cmd_AddCommand("mp_prevgametype", GAME_MP_ChangeGametype_f, "Switch to the previous multiplayer game type");
	MP_CallbacksInit(cgi);
	MP_ServerListInit(cgi);

	/* restore old sv_maxsoldiersperplayer and sv_maxsoldiersperteam
	 * cvars if values were previously set */
	if (max_s[0] != '\0')
		cgi->Cvar_Set("sv_maxsoldiersperteam", max_s);
	if (max_spp[0] != '\0')
		cgi->Cvar_Set("sv_maxsoldiersperplayer", max_spp);
}

static void GAME_MP_Shutdown (void)
{
	cgi->Cmd_RemoveCommand("mp_startserver");
	cgi->Cmd_RemoveCommand("mp_updategametype");
	cgi->Cmd_RemoveCommand("mp_nextgametype");
	cgi->Cmd_RemoveCommand("mp_prevgametype");
	MP_CallbacksShutdown();
	MP_ServerListShutdown();

	cgi->SV_Shutdown("Game mode shutdown", qfalse);

	OBJZERO(teamData);
}

#ifndef HARD_LINKED_CGAME
const cgame_export_t *GetCGameAPI (const cgame_import_t *import)
#else
const cgame_export_t *GetCGameMultiplayerAPI (const cgame_import_t *import)
#endif
{
	static cgame_export_t e;

	OBJZERO(e);

	e.name = "Multiplayer mode";
	e.menu = "multiplayer";
	e.isMultiplayer = 1;
	e.Init = GAME_MP_InitStartup;
	e.Shutdown = GAME_MP_Shutdown;
	e.MapInfo = GAME_MP_MapInfo;
	e.Results = GAME_MP_Results;
	e.EndRoundAnnounce = GAME_MP_EndRoundAnnounce;
	e.StartBattlescape = GAME_MP_StartBattlescape;
	e.NotifyEvent = GAME_MP_NotifyEvent;
	e.AddChatMessage = GAME_MP_AddChatMessage;
	e.HandleServerCommand = GAME_MP_HandleServerCommand;

	cgi = import;

	return &e;
}
