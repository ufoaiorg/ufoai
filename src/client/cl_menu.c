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
	node = MN_GetNodeByPath("options_keys.keylist");
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
	/* print the keybindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	MN_Init();
}
