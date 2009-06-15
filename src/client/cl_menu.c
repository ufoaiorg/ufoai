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
#include "cl_menu.h"
#include "menu/m_main.h"
#include "menu/m_nodes.h"
#include "menu/m_popup.h"
#include "menu/node/m_node_abstractnode.h"

/**
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 */
void MN_SetViewRect (void)
{
	menuNode_t *menu = MN_GetActiveMenu();

	/** @todo the better way is to add a 'battlescape' node */
	if (!menu || !menu->u.window.renderNode)
		if (MN_IsMenuOnStack(mn_hud->string))
			menu = MN_GetMenu(mn_hud->string);

	if (menu && menu->u.window.renderNode) {
		menuNode_t* node = menu->u.window.renderNode;
		vec2_t pos;
		MN_GetNodeAbsPos(node, pos);
		viddef.x = pos[0] * viddef.rx;
		viddef.y = pos[1] * viddef.ry;
		viddef.viewWidth = node->size[0] * viddef.rx;
		viddef.viewHeight = node->size[1] * viddef.ry;
	} else {
		viddef.x = viddef.y = 0;
		viddef.viewWidth = 0;
		viddef.viewHeight = 0;
	}
}

/**
 * @brief Prints a list of tab and newline separated string to keylist char array that hold the key and the command desc
 * @todo Use a linked list here, no static buffer
 */
static void MN_InitKeyList_f (void)
{
	static char keylist[2048];
	int i;

	keylist[0] = '\0';

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (keyBindings[i] && keyBindings[i][0])
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(keyBindings[i])), sizeof(keylist));

	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (menuKeyBindings[i] && menuKeyBindings[i][0])
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(menuKeyBindings[i])), sizeof(keylist));

	for (i = 0; i < K_LAST_KEY; i++)
		if (battleKeyBindings[i] && battleKeyBindings[i][0])
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(battleKeyBindings[i])), sizeof(keylist));

	MN_RegisterText(TEXT_LIST, keylist);
}

/**
 * @brief Initialize the menu data hunk, add cvars and commands
 * @note This function is called once
 * @sa MN_Shutdown
 * @sa CL_InitLocal
 */
void MN_InitStartup (void)
{
	/* print the keyBindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	MN_Init();
}
