/**
 * @file
 * @brief Multiplayer game type code
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

static const cgame_import_t* cgi;

CGAME_HARD_LINKED_FUNCTIONS

static void GAME_MP_StartBattlescape (bool isTeamPlay)
{
	cgi->UI_ExecuteConfunc("multiplayer_setTeamplay %i", isTeamPlay);
	cgi->UI_InitStack("multiplayer_wait", nullptr);
	rcon_client_password->modified = true;
}

static void GAME_MP_NotifyEvent (event_t eventType)
{
	if (eventType != EV_RESET)
		return;

	cgi->HUD_InitUI("missionoptions");
}

static void GAME_MP_EndRoundAnnounce (int playerNum, int team)
{
	char buf[128];

	/* it was our own turn */
	if (cgi->CL_GetPlayerNum() == playerNum) {
		Com_sprintf(buf, sizeof(buf), _("You've ended your turn.\n"));
	} else {
		const char* playerName = cgi->CL_PlayerGetName(playerNum);
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
	const mapDef_t* md;

	if (!cgi->Cvar_GetInteger("sv_dedicated") && cgi->GAME_IsTeamEmpty())
		cgi->GAME_AutoTeam("multiplayer_initial", cgi->GAME_GetCharacterArraySize());

	if (cgi->Cvar_GetInteger("sv_teamplay")
	 && cl_maxsoldiersperplayer->integer > cl_maxsoldiersperteam->integer) {
		cgi->UI_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = cgi->GAME_GetCurrentSelectedMap();
	if (!md || !md->multiplayer)
		return;
	assert(md->mapTheme);

	/** @todo implement different ufo and dropship support for multiplayer, too (see skirmish) */
	cgi->Cvar_Set("rm_drop", "");
	cgi->Cvar_Set("rm_ufo", "");
	cgi->Cvar_Set("sv_hurtaliens", "0");

	if (md->mapTheme[0] == '+') {
		const linkedList_t* const ufos = md->ufos;
		const linkedList_t* const crafts = md->aircraft;
		if (ufos)
			cgi->Cvar_Set("rm_ufo", "%s",
					cgi->Com_GetRandomMapAssemblyNameForCraft((const char*)ufos->data));
		if (crafts)
			cgi->Cvar_Set("rm_drop", "%s",
					cgi->Com_GetRandomMapAssemblyNameForCraft((const char*)crafts->data));
	}

	if (md->teams)
		cgi->Cvar_SetValue("sv_maxteams", md->teams);
	else
		cgi->Cvar_SetValue("sv_maxteams", 2);

	cgi->Cmd_ExecuteString("map %s %s %s", cgi->Cvar_GetInteger("mn_serverday") ? "day" : "night", md->mapTheme, md->params ? (const char*)cgi->LIST_GetRandom(md->params) : "");

	cgi->UI_InitStack("multiplayer_wait", "missionoptions");
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
 * @param nextmap Indicates if there is another map to follow within the same mission
 */
static void GAME_MP_Results (dbuffer* msg, int winner, int* numSpawned, int* numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS], bool nextmap)
{
	linkedList_t* list = nullptr;
	int enemiesKilled, enemiesStunned;
	const int team = cgi->GAME_GetCurrentTeam();

	enemiesKilled = enemiesStunned = 0;
	for (int i = 0; i < MAX_TEAMS; i++) {
		if (i == team)
			continue;
		enemiesKilled += numKilled[team][i];
		enemiesStunned += numStunned[team][i];
	}

	cgi->LIST_AddString(&list, va(_("Enemies killed:\t%i"), enemiesKilled + enemiesStunned));
	cgi->LIST_AddString(&list, va(_("Team survivors:\t%i"), numAlive[team]));
	cgi->UI_RegisterLinkedListText(TEXT_LIST2, list);
	if (winner == team) {
		cgi->UI_PushWindow("won");
	} else {
		cgi->UI_PushWindow("lost");
	}
}

static const mapDef_t* GAME_MP_MapInfo (int step)
{
	int i = 0;
	const char* gameType = cgi->Cvar_GetString("sv_gametype");
	for (;;) {
		i++;
		if (i > 100000)
			break;

		const mapDef_t* md = cgi->GAME_GetCurrentSelectedMap();
		if (md == nullptr)
			break;
		if (!md->multiplayer || !cgi->LIST_ContainsString(md->gameTypes, gameType)) {
			cgi->GAME_SwitchCurrentSelectedMap(step ? step : 1);
			continue;
		}
		linkedList_t* gameNames = nullptr;
		for (int j = 0; j < cgi->csi->numGTs; j++) {
			const gametype_t* gt = &cgi->csi->gts[j];
			if (cgi->LIST_ContainsString(md->gameTypes, gt->id)) {
				cgi->LIST_AddString(&gameNames, _(gt->name));
			}
		}
		cgi->UI_RegisterLinkedListText(TEXT_LIST2, gameNames);
		cgi->Cvar_SetValue("ai_singleplayeraliens", md->maxAliens);

		return md;
	}

	cgi->Com_Printf("no multiplayer map found for the current selected gametype: '%s'", gameType);
	return nullptr;
}

/**
 * @brief Update the map according to the gametype
 */
static void GAME_MP_UpdateGametype_f (void)
{
	const int numGTs = cgi->csi->numGTs;
	/* no types defined or parsed */
	if (numGTs == 0)
		return;

	cgi->Com_SetGameType();

	const char* gameType = cgi->Cvar_GetString("sv_gametype");
	const mapDef_t* md = cgi->GAME_GetCurrentSelectedMap();
	if (md != nullptr && md->multiplayer && cgi->LIST_ContainsString(md->gameTypes, gameType)) {
		/* no change needed, gametype is supported */
		return;
	}

	GAME_MP_MapInfo(1);
}

static linkedList_t* mp_chatMessageStack = nullptr;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
static void GAME_MP_AddChatMessage (const char* text)
{
	char message[2048];
	Q_strncpyz(message, text, sizeof(message));

	const char* msg = Com_Trim(message);
	cgi->LIST_AddString(&mp_chatMessageStack, msg);
	cgi->HUD_DisplayMessage(msg);
	cgi->UI_RegisterLinkedListText(TEXT_CHAT_WINDOW, mp_chatMessageStack);
	cgi->UI_TextScrollEnd("hud_chat.allchats.chatscreen.chat");
}

static bool GAME_MP_HandleServerCommand (const char* command, dbuffer* msg)
{
	if (Q_streq(command, SV_CMD_TEAMINFO)) {
		GAME_MP_ParseTeamInfoMessage(msg);
		return true;
	}

	return false;
}

static const cmdList_t multiplayerCmds[] = {
	{"mp_startserver", GAME_MP_StartServer_f, nullptr},
	{"mp_updategametype", GAME_MP_UpdateGametype_f, "Update the menu values with current gametype values"},
	{nullptr, nullptr, nullptr}
};
static void GAME_MP_InitStartup (void)
{
	cgi->Cvar_ForceSet("sv_maxclients", "2");
	/** @todo make equipment configurable for multiplayer */
	cgi->Cvar_Set("cl_equip", "multiplayer_initial");

	cgi->Cmd_TableAddList(multiplayerCmds);
	GAME_MP_CallbacksInit(cgi);
	GAME_MP_ServerListInit(cgi);
}

static void GAME_MP_Shutdown (void)
{
	cgi->Cmd_TableRemoveList(multiplayerCmds);
	GAME_MP_CallbacksShutdown();
	GAME_MP_ServerListShutdown();

	cgi->SV_Shutdown("Game mode shutdown", false);

	OBJZERO(teamData);
}

static void GAME_MP_RunFrame (float secondsSinceLastFrame)
{
	if (!cgi->Com_ServerState() && cgi->CL_GetClientState() < ca_connected && Q_strnull(cgi->Cvar_GetString("rcon_address")))
		return;

	if (rcon_client_password->modified) {
		rcon_client_password->modified = false;
		if (!cgi->Com_ServerState() && Q_strnull(rcon_client_password->string)) {
			cgi->UI_ExecuteConfunc("multiplayer_admin_panel 0");
		} else {
			cgi->UI_ExecuteConfunc("multiplayer_admin_panel 1");
		}
	}

	cvar_t* cvars[] = {cl_maxsoldiersperteam, cl_maxsoldiersperplayer, cl_roundtimelimit};
	for (int i = 0; i < lengthof(cvars); i++) {
		if (!cvars[i]->modified) {
			continue;
		}
		cvars[i]->modified = false;
		if (!cgi->Com_ServerState()) {
			cgi->Cmd_ExecuteString(SV_CMD_RCON " set %s %s", cvars[i]->name, cvars[i]->string);
		}
	}
}

#ifndef HARD_LINKED_CGAME
const cgame_export_t* GetCGameAPI (const cgame_import_t* import)
#else
const cgame_export_t* GetCGameMultiplayerAPI (const cgame_import_t* import)
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
	e.RunFrame = GAME_MP_RunFrame;

	cgi = import;

	return &e;
}
