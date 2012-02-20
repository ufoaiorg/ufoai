/**
 * @file cl_menu.c
 * @brief Client menu functions.
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "input/cl_keys.h"
#include "ui/ui_main.h"
#include "ui/ui_input.h"
#include "ui/ui_nodes.h"
#include "ui/ui_popup.h"
#include "ui/node/ui_node_abstractnode.h"

static inline void CLMN_AddBindings (linkedList_t **list, char **bindings)
{
	int i;
	for (i = K_FIRST_KEY; i < K_LAST_KEY; i++)
		if (bindings[i] && bindings[i][0] != '\0') {
			const char *binding = Cmd_GetCommandDesc(bindings[i]);
			if (binding != NULL && binding[0] != '\0')
				binding = _(binding);
			LIST_AddString(list, va("%s\t%s", Key_KeynumToString(i), binding));
		}
}

/**
 * @brief Adds UI Keybindings to the list for the Keylist UI
 * @param[in,out] list Linked list of strings to add to
 */
static inline void CLMN_AddUIBindings (linkedList_t **list)
{
	int i;

	for (i = 0; i < UI_GetKeyBindingCount(); i++) {
		const uiKeyBinding_t* binding = UI_GetKeyBindingByIndex(i);

		if (binding == NULL)
			continue;
		if (binding->inherited)
			continue;
		if (!Q_strvalid(binding->description))
			continue;

		LIST_AddString(list, va("%s\t%s", Key_KeynumToString(binding->key), _(binding->description)));
	}
}

/**
 * @brief Prints a list of tab and newline separated string to keylist char array that hold the key and the command desc
 */
static void CLMN_InitKeyList_f (void)
{
	linkedList_t *list = NULL;

	LIST_AddString(&list, _("^BGlobal bindings"));
	LIST_AddString(&list, "");
	CLMN_AddBindings(&list, keyBindings);
	LIST_AddString(&list, "");
	LIST_AddString(&list, "");
	LIST_AddString(&list, _("^BMenu bindings"));
	LIST_AddString(&list, "");
	CLMN_AddBindings(&list, menuKeyBindings);
	CLMN_AddUIBindings(&list);
	LIST_AddString(&list, "");
	LIST_AddString(&list, "");
	LIST_AddString(&list, _("^BBattlescape bindings"));
	LIST_AddString(&list, "");
	CLMN_AddBindings(&list, battleKeyBindings);

	UI_RegisterLinkedListText(TEXT_LIST, list);
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

	UI_Init();
}
