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
#include "menu/m_main.h"
#include "menu/m_nodes.h"
#include "menu/m_popup.h"

/**
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 * @todo understand if this function is realy need
 */
void MN_SetViewRect (const menuNode_t* menu)
{
	viddef.x = viddef.y = 0;
	viddef.viewWidth = viddef.width;
	viddef.viewHeight = viddef.height;
#if 0
	/* It make no sens. If we open a popup over the battlefield,
	 * we anyway can't read the right render node
	 */
	if (!menu) {
		/* render the full screen */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.width;
		viddef.viewHeight = viddef.height;
	} else {
		/* the menu has a view size specified */
		viddef.x = menu->pos[0] * viddef.rx;
		viddef.y = menu->pos[1] * viddef.ry;
		viddef.viewWidth = menu->size[0] * viddef.rx;
		viddef.viewHeight = menu->size[1] * viddef.ry;
	}
#endif
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

	for (i = 0; i < K_LAST_KEY; i++)
		if (battlekeybindings[i] && battlekeybindings[i][0])
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(battlekeybindings[i])), sizeof(keylist));

	MN_RegisterText(TEXT_LIST, keylist);

	/* @todo bad size computation, the text node only know the number of line */
	MN_ExecuteConfunc("mn_textupdated keylist");
	node = MN_GetNodeFromCurrentMenu("keylist");
	MN_ExecuteConfunc("optionkey_count %i", node->u.text.textLines);
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
