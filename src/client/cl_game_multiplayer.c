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
#include "cl_game_multiplayer.h"
#include "menu/m_popup.h"

static aircraft_t multiplayerFakeAircraft;

static void B_MultiplayerAssignInitial_f (void)
{
	if (GAME_IsMultiplayer())
		B_AssignInitial(cls.missionaircraft, cl_equip->string);
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void MN_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;

	if (!GAME_IsMultiplayer())
		return;

	if (!sv_dedicated->integer && !B_GetNumOnTeam(cls.missionaircraft)) {
		B_AssignInitial(cls.missionaircraft, cl_equip->string);
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
static void MN_UpdateGametype_f (void)
{
	Com_SetGameType();
}

/**
 * @brief Switch to the next multiplayer game type
 * @sa MN_PrevGametype_f
 */
static void MN_ChangeGametype_f (void)
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

void GAME_MP_InitStartup (void)
{
	const char *max_s = Cvar_VariableStringOld("sv_maxsoldiersperteam");
	const char *max_spp = Cvar_VariableStringOld("sv_maxsoldiersperplayer");
	const char *type, *name, *text;

	Cvar_ForceSet("sv_maxclients", "2");

	Cmd_AddCommand("mn_startserver", MN_StartServer_f, NULL);
	Cmd_AddCommand("mn_updategametype", MN_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mn_nextgametype", MN_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mn_prevgametype", MN_ChangeGametype_f, "Switch to the previous multiplayer game type");
	Cmd_AddCommand("assign_initial", B_MultiplayerAssignInitial_f, "Assign initial multiplayer equipment to soldiers");

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

	Com_Printf("Changing to Multiplayer\n");
	/* disconnect already running session - when entering the
		* multiplayer menu while you are still connected */
	if (cls.state >= ca_connecting)
		CL_Disconnect();

	/* pre-stage parsing */
	FS_BuildFileList("ufos/*.ufo");
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	if (!gd.numTechnologies) {
		while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
			if (!Q_strncmp(type, "tech", 4))
				RS_ParseTechnologies(name, &text);

		/* fill in IDXs for required research techs */
		RS_RequiredLinksAssign();
		Com_AddObjectLinks();	/* Add tech links + ammo<->weapon links to items.*/
	}
}

void GAME_MP_Shutdown (void)
{
	Cmd_RemoveCommand("mn_startserver");
	Cmd_RemoveCommand("mn_updategametype");
	Cmd_RemoveCommand("mn_nextgametype");
	Cmd_RemoveCommand("mn_prevgametype");
	Cmd_RemoveCommand("assign_initial");
}
