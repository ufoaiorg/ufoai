/**
 * @file
 * @brief Client menu functions.
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

#include "cl_menu.h"
#include "cl_shared.h"
#include "cgame/cl_game.h"
#include "input/cl_keys.h"
#include "input/cl_joystick.h"
#include "cl_video.h"
#include "cl_language.h"
#include "ui/ui_main.h"
#include "ui/ui_input.h"
#include "ui/ui_nodes.h"
#include "ui/ui_popup.h"
#include "ui/node/ui_node_abstractnode.h"

static inline void CLMN_AddBindings (keyBindSpace_t scope, char** bindings, int offset = 0)
{
	for (int i = K_FIRST_KEY; i < K_LAST_KEY; i++) {
		if (Q_strnull(bindings[i]))
			continue;
		const char* binding = Cmd_GetCommandDesc(bindings[i]);
		if (Q_strvalid(binding))
			binding = _(binding);
		UI_ExecuteConfunc("keybinding_add %i %i \"%s\" \"%s\"", i + offset, scope, Key_KeynumToString(i), binding);
	}
}

/**
 * @brief Adds UI Keybindings to the list for the Keylist UI
 */
static inline int CLMN_AddUIBindings (keyBindSpace_t scope)
{
	int cnt = 0;
	const int num = UI_GetKeyBindingCount();
	for (int i = 0; i < num; i++) {
		const uiKeyBinding_t* binding = UI_GetKeyBindingByIndex(i);
		if (binding == nullptr)
			continue;
		if (binding->inherited)
			continue;
		if (!Q_strvalid(binding->description))
			continue;

		UI_ExecuteConfunc("keybinding_add %i %i \"%s\" \"%s\"", cnt++, scope, Key_KeynumToString(binding->key), _(binding->description));
	}
	return cnt;
}

/**
 * @brief Prints a list of tab and newline separated string to keylist char array that hold the key and the command desc
 */
static void CLMN_InitKeyList_f (void)
{
	UI_ExecuteConfunc("keybinding_clear");

	CLMN_AddBindings(KEYSPACE_GAME, keyBindings);
	CLMN_AddBindings(KEYSPACE_UI, menuKeyBindings);
	const int uiBindings = CLMN_AddUIBindings(KEYSPACE_UI);
	CLMN_AddBindings(KEYSPACE_BATTLE, battleKeyBindings, uiBindings);
}

static void CLMN_Mods_f (void)
{
	linkedList_t* mods = nullptr;
	FS_GetModList(&mods);
	LIST_Foreach(mods, const char, mod) {
		Cbuf_AddText("mn_mods_result %s\n", mod);
	}
}

static void CL_VideoInitMenu (void)
{
	uiNode_t* option = UI_GetOption(OPTION_VIDEO_RESOLUTIONS);
	if (option != nullptr) {
		return;
	}
	const int length = VID_GetModeNums();
	for (int i = 0; i < length; i++) {
		vidmode_t vidmode;
		if (VID_GetModeInfo(i, &vidmode))
			UI_AddOption(&option, va("r%ix%i", vidmode.width, vidmode.height), va("%i x %i", vidmode.width, vidmode.height), va("%i", i));
	}
	UI_RegisterOption(OPTION_VIDEO_RESOLUTIONS, option);
}

static void CL_TeamDefInitMenu (void)
{
	uiNode_t* option = UI_GetOption(OPTION_TEAMDEFS);
	if (option != nullptr)
		return;

	for (int i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t* td = &csi.teamDef[i];
		if (td->team != TEAM_CIVILIAN)
			UI_AddOption(&option, td->id, va("_%s", td->name), td->id);
	}
	UI_RegisterOption(OPTION_TEAMDEFS, option);
}

/**
 * @brief Initialize the menu data hunk, add cvars and commands
 * @note This function is called once
 * @sa MN_Shutdown
 * @sa CL_InitLocal
 */
void CLMN_Init (void)
{
	/* print the keyBindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", CLMN_InitKeyList_f);
	Cmd_AddCommand("mn_mods", CLMN_Mods_f);

	CL_TeamDefInitMenu();
	CL_VideoInitMenu();
	IN_JoystickInitMenu();
	CL_LanguageInitMenu();
	GAME_InitUIData();
}

void CLMN_Shutdown (void)
{
	Cmd_RemoveCommand("mn_init_keylist");
	Cmd_RemoveCommand("mn_mods");
}
