/**
 * @file cl_game_multiplayer.c
 * @brief Multiplayer game type code
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
#include "cl_inventory.h"
#include "multiplayer/mp_callbacks.h"
#include "multiplayer/mp_serverlist.h"
#include "multiplayer/mp_team.h"
#include "menu/m_main.h"
#include "menu/m_popup.h"

void GAME_MP_AutoTeam (void)
{
	const equipDef_t *ed = INV_GetEquipmentDefinitionByID("multiplayer_initial");
	const char *teamDefID = GAME_GetTeamDef();

	GAME_GenerateTeam(teamDefID, ed, MAX_ACTIVETEAM);
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void GAME_MP_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;

	if (!sv_dedicated->integer && !chrDisplayList.num)
		GAME_MP_AutoTeam();

	if (Cvar_GetInteger("sv_teamplay")
	 && Cvar_GetValue("sv_maxsoldiersperplayer") > Cvar_GetValue("sv_maxsoldiersperteam")) {
		MN_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = &csi.mds[cls.currentSelectedMap];
	if (!md || !md->multiplayer)
		return;
	assert(md->map);

	Com_sprintf(map, sizeof(map), "map %s %s %s", Cvar_GetInteger("mn_serverday") ? "day" : "night", md->map, md->param ? md->param : "");

	/* let the (local) server know which map we are running right now */
	csi.currentMD = md;

	/** @todo implement different ufo and dropship support for multiplayer, too (see skirmish) */
	Cvar_Set("rm_drop", "");
	Cvar_Set("rm_ufo", "");

	Cmd_ExecuteString(map);

	MN_InitStack("multiplayer_wait", "multiplayerInGame", qfalse, qtrue);
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
	if (!strcmp(Cmd_Argv(0), "mp_prevgametype")) {
		next = qfalse;
	}

	if (md->gameTypes) {
		linkedList_t *list = md->gameTypes;
		linkedList_t *old = NULL;
		while (list) {
			if (!strcmp((const char*)list->data, sv_gametype->string)) {
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
			if (!strcmp(gt->id, sv_gametype->string)) {
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
void GAME_MP_Results (struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS])
{
	char resultText[MAX_SMALLMENUTEXTLEN];
	int their_killed, their_stunned;
	int i;

	CL_Drop();

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

const mapDef_t* GAME_MP_MapInfo (int step)
{
	const mapDef_t *md;
	int i = 0;

	while (!csi.mds[cls.currentSelectedMap].multiplayer) {
		i++;
		cls.currentSelectedMap += (step ? step : 1);
		if (cls.currentSelectedMap < 0)
			cls.currentSelectedMap = csi.numMDs - 1;
		cls.currentSelectedMap %= csi.numMDs;
		if (i >= csi.numMDs)
			Com_Error(ERR_DROP, "GAME_MP_MapInfo: There is no multiplayer map in any mapdef");
	}

	md = &csi.mds[cls.currentSelectedMap];

	if (md->gameTypes) {
		const linkedList_t *list = md->gameTypes;
		char buf[256] = "";
		while (list) {
			Q_strcat(buf, va("%s ", (const char *)list->data), sizeof(buf));
			list = list->next;
		}
		Cvar_Set("mn_mapgametypes", buf);
		list = LIST_ContainsString(md->gameTypes, sv_gametype->string);
	} else {
		Cvar_Set("mn_mapgametypes", _("all"));
	}

	return md;
}

int GAME_MP_GetTeam (void)
{
	return cl_team->integer;
}

static equipDef_t equipDefMultiplayer;

equipDef_t *GAME_MP_GetEquipmentDefinition (void)
{
	return &equipDefMultiplayer;
}

void GAME_MP_InitStartup (void)
{
	const char *max_s = Cvar_VariableStringOld("sv_maxsoldiersperteam");
	const char *max_spp = Cvar_VariableStringOld("sv_maxsoldiersperplayer");

	chrDisplayList.num = 0;

	Cvar_ForceSet("sv_maxclients", "2");

	Cmd_AddCommand("mp_startserver", GAME_MP_StartServer_f, NULL);
	Cmd_AddCommand("mp_updategametype", GAME_MP_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mp_nextgametype", GAME_MP_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mp_prevgametype", GAME_MP_ChangeGametype_f, "Switch to the previous multiplayer game type");
	MP_CallbacksInit();
	MP_ServerListInit();

	/* restore old sv_maxsoldiersperplayer and sv_maxsoldiersperteam
	 * cvars if values were previously set */
	if (strlen(max_s))
		Cvar_Set("sv_maxsoldiersperteam", max_s);
	if (strlen(max_spp))
		Cvar_Set("sv_maxsoldiersperplayer", max_spp);
}

void GAME_MP_Shutdown (void)
{
	Cmd_RemoveCommand("mp_startserver");
	Cmd_RemoveCommand("mp_updategametype");
	Cmd_RemoveCommand("mp_nextgametype");
	Cmd_RemoveCommand("mp_prevgametype");
	MP_CallbacksShutdown();
	MP_ServerListShutdown();

	SV_Shutdown("Game mode shutdown", qfalse);

	memset(&teamData, 0, sizeof(teamData));
}
