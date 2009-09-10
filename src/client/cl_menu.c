/**
 * @file cl_menu.c
 * @brief Client menu functions.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
 * @brief Prints a list of tab and newline separated string to keylist char array that hold the key and the command desc
 * @todo Use a linked list here, no static buffer
 */
static void CLMN_InitKeyList_f (void)
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
void CLMN_InitStartup (void)
{
	/* print the keyBindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", CLMN_InitKeyList_f, NULL);

	MN_Init();
}
