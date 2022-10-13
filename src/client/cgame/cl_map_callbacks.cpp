/**
 * @file
 */

/*
Copyright (C) 2002-2022 UFO: Alien Invasion.

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

#include "cl_map_callbacks.h"
#include "../client.h"
#include "cl_game.h"

typedef enum ui_gamemode_s {
	GAMEMODE_NONE,
	GAMEMODE_SKIRMISH,
	GAMEMODE_MULTIPLAYER,
	GAMEMODE_CAMPAIGN,
	MAX_GAMEMODE
} ui_gamemode_t;

static void UI_MapInfoGetNext (int step)
{
	int ref = cls.currentSelectedMap;

	for (;;) {
		cls.currentSelectedMap += step;
		if (cls.currentSelectedMap < 0)
			cls.currentSelectedMap = csi.numMDs - 1;
		cls.currentSelectedMap %= csi.numMDs;

		const mapDef_t* md = Com_GetMapDefByIDX(cls.currentSelectedMap);

		/* avoid infinite loop */
		if (ref == cls.currentSelectedMap)
			break;
		/* special purpose maps are not startable without the specific context */
		if (md->mapTheme[0] == '.')
			continue;

		if (md->mapTheme[0] != '+' && FS_CheckFile("maps/%s.bsp", md->mapTheme) != -1)
			break;
		if (md->mapTheme[0] == '+' && FS_CheckFile("maps/%s.ump", md->mapTheme + 1) != -1)
			break;
	}
}

/**
 * @brief Prints the map info for the server creation dialogue
 */
static void UI_MapInfo (int step)
{
	const cgame_export_t* list = cls.gametype;

	if (!list)
		return;

	if (!csi.numMDs)
		return;

	UI_MapInfoGetNext(step);

	const mapDef_t* md = list->MapInfo(step);
	if (!md)
		return;

	const char* mapname = md->mapTheme;
	/* skip random map char. */
	Cvar_Set("mn_svmapid", "%s", md->id);
	if (mapname[0] == '+') {
		Cvar_Set("mn_svmapname", "%s %s", md->mapTheme, md->params ? (const char*)LIST_GetRandom(md->params) : "");
		mapname++;
	} else {
		Cvar_Set("mn_svmapname", "%s", md->mapTheme);
	}

	if (R_ImageExists("pics/maps/shots/%s", mapname))
		Cvar_Set("mn_mappic", "maps/shots/%s", mapname);
	else
		Cvar_Set("mn_mappic", "maps/shots/default");

	if (R_ImageExists("pics/maps/shots/%s_2", mapname))
		Cvar_Set("mn_mappic2", "maps/shots/%s_2", mapname);
	else
		Cvar_Set("mn_mappic2", "maps/shots/default");

	if (R_ImageExists("pics/maps/shots/%s_3", mapname))
		Cvar_Set("mn_mappic3", "maps/shots/%s_3", mapname);
	else
		Cvar_Set("mn_mappic3", "maps/shots/default");
}

static void UI_GetMaps_f (void)
{
	UI_MapInfo(0);
}

/**
 * @brief Select the next available map.
 */
static void UI_NextMap_f (void)
{
	UI_MapInfo(1);
}

/**
 * @brief Select the previous available map.
 */
static void UI_PreviousMap_f (void)
{
	UI_MapInfo(-1);
}

static void UI_SelectMap_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		return;
	}

	if (!csi.numMDs)
		return;

	const char* mapname = Cmd_Argv(1);
	int i = 0;
	const mapDef_t* md;

	MapDef_Foreach(md) {
		i++;
		if (!Q_streq(md->mapTheme, mapname))
			continue;
		cls.currentSelectedMap = i - 1;
		UI_MapInfo(0);
		return;
	}

	i = 0;
	MapDef_Foreach(md) {
		i++;
		if (!Q_streq(md->id, mapname))
			continue;
		cls.currentSelectedMap = i - 1;
		UI_MapInfo(0);
		return;
	}

	Com_Printf("Could not find map %s\n", mapname);
}

/**
 * @brief Helper function to apply gameMode filter on map definitions
 * @param[in] gameMode game mode constant
 * @param[in] mapDef Map definition pointer
 * @return @c true if the definition is enabled in the specified game mode @c false otherwise
 * @sa UI_ListMaps_f
 */
static inline bool UI_MapGameModeCondition (ui_gamemode_t gameMode, const mapDef_t *mapDef)
{
	if (mapDef == nullptr)
		return false;
	if (gameMode == GAMEMODE_SKIRMISH)
		return mapDef->singleplayer;
	else if (gameMode == GAMEMODE_MULTIPLAYER)
		return mapDef->multiplayer;
	else if (gameMode == GAMEMODE_CAMPAIGN)
		return mapDef->campaign;
	return false;
}

/**
 * @brief List available maps
 */
static void UI_ListMaps_f (void)
{
	if (!csi.numMDs) {
		Com_Printf("%s No mapDefinitions found\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <gamemode> <callback>\n", Cmd_Argv(0));
		return;
	}

	const char* gameMode = Cmd_Argv(1);
	ui_gamemode_t mode = GAMEMODE_NONE;
	if (Q_streq("skirmish", gameMode))
		mode = GAMEMODE_SKIRMISH;
	else if (Q_streq("multiplayer", gameMode))
		mode = GAMEMODE_MULTIPLAYER;
	else if (Q_streq("campaign", gameMode))
		mode = GAMEMODE_CAMPAIGN;

	char callback[MAX_VAR];
	Q_strncpyz(callback, Cmd_Argv(2), sizeof(callback));

	const mapDef_t* md;
	MapDef_ForeachCondition(md, UI_MapGameModeCondition(mode, md)) {
		/* special purpose maps are not startable without the specific context */
		if (md->mapTheme[0] == '.')
			continue;
		/* do we have the map file? */
		if (md->mapTheme[0] != '+' && FS_CheckFile("maps/%s.bsp", md->mapTheme) == -1)
			continue;
		if (md->mapTheme[0] == '+' && FS_CheckFile("maps/%s.ump", md->mapTheme + 1) == -1)
			continue;

		const char* image = md->mapTheme;
		if (image[0] == '+')
			image++;
		if (!R_ImageExists("pics/maps/shots/%s", image))
			image = "default";

		UI_ExecuteConfunc("%s %s %s \"%s\" %s",
			callback,
			md->id,
			md->mapTheme,
			md->description,
			image
		);
	}
}

/**
 * @brief List of UI Callbacks of battlescape map selection
 */
static const cmdList_t mapCallbacks[] = {
//	{"ui_aircraft_changename", AIR_ChangeAircraftName_f, "Callback to change the name of the aircraft."},
	{"mn_getmaps", UI_GetMaps_f, "The initial map to show"},

	{"mn_nextmap", UI_NextMap_f, "Switch to the next valid map for the selected gametype"},
	{"mn_prevmap", UI_PreviousMap_f, "Switch to the previous valid map for the selected gametype"},
	{"mn_selectmap", UI_SelectMap_f, "Switch to the map given by the parameter - may be invalid for the current gametype"},

	{"ui_listmaps", UI_ListMaps_f, "List available maps with some basic information for the UI"},

	{nullptr, nullptr, nullptr}
};

/**
 * @brief Registers UI Callbacks for battlescape map selection
 */
void MAP_InitCallbacks (void)
{
	Cmd_TableAddList(mapCallbacks);
}

/**
 * @brief Unregisters UI Callbacks of battlescape map selection
 */
void MAP_ShutdownCallbacks (void)
{
	Cmd_TableRemoveList(mapCallbacks);
}
