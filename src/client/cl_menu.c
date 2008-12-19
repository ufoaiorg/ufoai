/**
 * @file cl_menu.c
 * @brief Client menu functions.
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
#include "menu/m_main.h"
#include "menu/m_popup.h"

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

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void MN_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;
	aircraft_t *aircraft;
	base_t *base;

	if (ccs.singleplayer)
		return;

	base = B_GetBaseByIDX(0);
	aircraft = &base->aircraft[0];
	assert(aircraft);

	if (!sv_dedicated->integer && !B_GetNumOnTeam(aircraft)) {
		Com_Printf("MN_StartServer_f: Multiplayer team not loaded, please choose your team now.\n");
		B_AssignInitial(NULL);
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
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 */
void MN_SetViewRect (const menu_t* menu)
{
	menuNode_t* menuNode = menu ? (menu->renderNode ? menu->renderNode : (menu->popupNode ? menu->popupNode : NULL)): NULL;

	if (!menuNode) {
		/* render the full screen */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.width;
		viddef.viewHeight = viddef.height;
	} else if (menuNode->invis) {
		/* don't draw the scene */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.viewHeight = 0;
	} else {
		/* the menu has a view size specified */
		viddef.x = menuNode->pos[0] * viddef.rx;
		viddef.y = menuNode->pos[1] * viddef.ry;
		viddef.viewWidth = menuNode->size[0] * viddef.rx;
		viddef.viewHeight = menuNode->size[1] * viddef.ry;
	}
}

/**
 * @brief Prints a list of tab and newline seperated string to keylist char array that hold the key and the command desc
 * @todo Use a linked list here, no static buffer
 */
static void MN_InitKeyList_f (void)
{
	menuNode_t *node;
	static char keylist[2048];
	int i;

	*keylist = '\0';

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++) {
		if (keybindings[i] && keybindings[i][0]) {
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(keybindings[i])), sizeof(keylist));
		}
	}

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (menukeybindings[i] && menukeybindings[i][0])
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(menukeybindings[i])), sizeof(keylist));

	mn.menuText[TEXT_LIST] = keylist;

	/* @todo bad size computation, the text node only know the number of line */
	MN_ExecuteConfunc("mn_textupdated keylist");
	node = MN_GetNodeFromCurrentMenu("keylist");
	MN_ExecuteConfunc("optionkey_count %i", node->u.text.textLines);
}

/**
 * @brief Prints the map info for the server creation dialogue
 * @todo Skip special map that start with a '.' (e.g. .baseattack)
 */
static void MN_MapInfo (int step)
{
	int i = 0;
	mapDef_t *md;
	const char *mapname;

	if (!csi.numMDs)
		return;

	cls.currentSelectedMap += step;

	if (cls.currentSelectedMap < 0)
		cls.currentSelectedMap = csi.numMDs - 1;

	cls.currentSelectedMap %= csi.numMDs;

	if (!ccs.singleplayer) {
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

	if (!ccs.singleplayer) {
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
			if (!list)
				MN_ChangeGametype_f();
		} else {
			Cvar_Set("mn_mapgametypes", _("all"));
		}
	}
}

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_NextMap_f (void)
{
	MN_MapInfo(1);
}

static void MN_PrevMap_f (void)
{
	MN_MapInfo(-1);
}

/**
 * @brief Initialize the menu data hunk, add cvars and commands
 * @note Also calls the 'reset' functions for production, basemanagement,
 * aliencontainmenu, employee, hospital and a lot more subfunctions
 * @note This function is called once
 * @sa MN_Shutdown
 * @sa B_InitStartup
 * @sa CL_InitLocal
 */
void MN_InitStartup (void)
{
	/* add commands and cvars */
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_sequence", "sequence");

	/* print the keybindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	Cmd_AddCommand("mn_startserver", MN_StartServer_f, NULL);
	Cmd_AddCommand("mn_updategametype", MN_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mn_nextgametype", MN_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mn_prevgametype", MN_ChangeGametype_f, "Switch to the previous multiplayer game type");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_NextMap_f, "Switch to the next multiplayer map");
	Cmd_AddCommand("mn_prevmap", MN_PrevMap_f, "Switch to the previous multiplayer map");

	mn_main = Cvar_Get("mn_main", "main", 0, "Which is the main menu id to return to - also see mn_active");
	mn_sequence = Cvar_Get("mn_sequence", "sequence", 0, "Which is the sequence menu to render the sequence in");
	mn_active = Cvar_Get("mn_active", "", 0, "The active menu can will return to when hitting esc - also see mn_main");
	mn_afterdrop = Cvar_Get("mn_afterdrop", "", 0, "The menu that should be pushed after the drop function was called");
	mn_main_afterdrop = Cvar_Get("mn_main_afterdrop", "", 0, "The main menu that should be returned to after the drop function was called - will be the new mn_main value then");
	mn_hud = Cvar_Get("mn_hud", "hud", CVAR_ARCHIVE, "Which is the current selected hud");
	mn_serverday = Cvar_Get("mn_serverday", "1", CVAR_ARCHIVE, "Decides whether the server starts the day or the night version of the selected map");
	mn_inputlength = Cvar_Get("mn_inputlength", "32", 0, "Limit the input length for messagemenu input");
	mn_inputlength->modified = qfalse;

	MN_Init();
}
