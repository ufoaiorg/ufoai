/**
 * @file cl_game.c
 * @brief Shared game type code
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
#include "cl_game.h"
#include "cl_game_campaign.h"
#include "cl_game_skirmish.h"
#include "cl_game_multiplayer.h"

typedef struct gameTypeList_s {
	const char *name;
	const char *menu;
	int gametype;
	void (*init)(void);
	void (*shutdown)(void);
} gameTypeList_t;

static const gameTypeList_t gameTypeList[] = {
	{"Multiplayer mode", "multiplayer", GAME_MULTIPLAYER, GAME_MP_InitStartup, GAME_MP_Shutdown},
	{"Campaign mode", "campaigns", GAME_CAMPAIGN, GAME_CP_InitStartup, GAME_CP_Shutdown},
	{"Skirmish mode", "skirmish", GAME_SKIRMISH, GAME_SK_InitStartup, GAME_SK_Shutdown},

	{NULL, NULL, 0, NULL, NULL}
};

void GAME_SetMode (int gametype)
{
	const gameTypeList_t *list = gameTypeList;

	if (gametype == GAME_NONE) {
		ccs.gametype = gametype;
		return;
	}

	if (gametype < 0 || gametype > GAME_MAX) {
		Com_Printf("Invalid gametype %i given\n", gametype);
		return;
	}

	if (ccs.gametype == gametype)
		return;

	while (list->name) {
		if (list->gametype == gametype) {
			Com_Printf("Change gametype to '%s'\n", list->name);
			list->init();
		} else if (list->gametype == ccs.gametype) {
			Com_Printf("Shutdown gametype '%s'\n", list->name);
			list->shutdown();
		}
		list++;
	}

	ccs.gametype = gametype;
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void MN_MapInfo (int step)
{
	int i = 0;
	const mapDef_t *md;
	const char *mapname;

	if (!csi.numMDs)
		return;

	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = csi.numMDs - 1;

	cls.currentSelectedMap %= csi.numMDs;

	if (GAME_IsMultiplayer()) {
		while (!csi.mds[cls.currentSelectedMap].multiplayer) {
			i++;
			cls.currentSelectedMap += (step ? step : 1);
			if (cls.currentSelectedMap < 0)
				cls.currentSelectedMap = csi.numMDs - 1;
			cls.currentSelectedMap %= csi.numMDs;
			if (i >= csi.numMDs)
				Sys_Error("MN_MapInfo: There is no multiplayer map in any mapdef\n");
		}
	}

	md = &csi.mds[cls.currentSelectedMap];

	mapname = md->map;
	/* skip random map char */
	if (mapname[0] == '+')
		mapname++;

	Cvar_Set("mn_svmapname", md->map);
	if (FS_CheckFile(va("pics/maps/shots/%s.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s", mapname));
	else
		Cvar_Set("mn_mappic", va("maps/shots/default"));

	if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2", mapname));
	else
		Cvar_Set("mn_mappic2", va("maps/shots/default"));

	if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", mapname)) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3", mapname));
	else
		Cvar_Set("mn_mappic3", va("maps/shots/default"));

	if (GAME_IsMultiplayer()) {
		if (md->gameTypes) {
			const linkedList_t *list = md->gameTypes;
			char buf[256] = "";
			while (list) {
				Q_strcat(buf, va("%s ", (const char *)list->data), sizeof(buf));
				list = list->next;
			}
			Cvar_Set("mn_mapgametypes", buf);
			list = LIST_ContainsString(md->gameTypes, sv_gametype->string);
			/* the map doesn't support the current selected gametype - switch to the next valid */
#if 0
			/** @todo still needed? if yes - clean this up */
			if (!list)
				MN_ChangeGametype_f();
#endif
		} else {
			Cvar_Set("mn_mapgametypes", _("all"));
		}
	}
}

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_ChangeMap_f (void)
{
	if (!Q_strcmp(Cmd_Argv(0), "mn_nextmap"))
		MN_MapInfo(1);
	else
		MN_MapInfo(-1);
}

/**
 * @brief Decides with game mode should be set - takes the menu as reference
 */
static void GAME_SetMode_f (void)
{
	const menu_t *menu = MN_GetActiveMenu();
	const gameTypeList_t *list = gameTypeList;

	while (list->name) {
		if (!Q_strcmp(list->menu, menu->name)) {
			GAME_SetMode(list->gametype);
			return;
		}
		list++;
	}
}

/**
 * @brief Let the aliens win the match
 */
static void GAME_GameAbort_f (void)
{
	/* aborting means letting the aliens win */
	Cbuf_AddText(va("sv win %i\n", TEAM_ALIEN));
}

void GAME_InitStartup (void)
{
	Cmd_AddCommand("game_setmode", GAME_SetMode_f, "Decides with game mode should be set - takes the menu as reference");
	Cmd_AddCommand("game_abort", GAME_GameAbort_f, "Abort the game and let the aliens/opponents win");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_ChangeMap_f, "Switch to the next multiplayer map");
	Cmd_AddCommand("mn_prevmap", MN_ChangeMap_f, "Switch to the previous multiplayer map");
}
