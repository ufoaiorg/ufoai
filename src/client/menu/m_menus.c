/**
 * @file m_menus.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "m_main.h"
#include "m_internal.h"
#include "m_input.h"
#include "node/m_node_abstractnode.h"

#include "../client.h"
#include "../cl_cinematic.h"

/**
 * @brief Returns the ID of the last fullscreen ID. Before this, window should be hidden.
 * @return The last full screen window on the screen, else 0. If the stack is empty, return -1
 */
int MN_GetLastFullScreenWindow (void)
{
	/* stack pos */
	int pos = mn.menuStackPos - 1;
	while (pos > 0) {
		if (MN_WindowIsFullScreen(mn.menuStack[pos]))
			break;
		pos--;
	}
	/* if we find nothing we return 0 */
	return pos;
}

/**
 * @brief Remove the menu from the menu stack
 * @param[in] menu The menu to remove from the stack
 * @sa MN_PushMenuDelete
 */
static void MN_DeleteMenuFromStack (menuNode_t * menu)
{
	int i;

	for (i = 0; i < mn.menuStackPos; i++)
		if (mn.menuStack[i] == menu) {
			/** @todo (menu) don't leave the loop even if we found it - there still
			 * may be other copies around in the stack of the same menu
			 * @sa MN_PushCopyMenu_f */
			for (mn.menuStackPos--; i < mn.menuStackPos; i++)
				mn.menuStack[i] = mn.menuStack[i + 1];
			MN_InvalidateMouse();
			return;
		}
}

/**
 * @brief Searches the position in the current menu stack for a given menu id
 * @return -1 if the menu is not on the stack
 */
static inline int MN_GetMenuPositionFromStackByName (const char *name)
{
	int i;
	for (i = 0; i < mn.menuStackPos; i++)
		if (!Q_strcmp(mn.menuStack[i]->name, name))
			return i;

	return -1;
}

/**
 * @brief Insert a menu at a position of the stack
 * @param[in] menu The menu to insert
 * @param[in] position Where we want to add the menu (0 is the deeper element of the stack)
 */
static inline void MN_InsertMenuIntoStack (menuNode_t *menu, int position)
{
	int i;
	assert(position <= mn.menuStackPos);
	assert(position > 0);
	assert(menu != NULL);

	/* create space for the new menu */
	for (i = mn.menuStackPos; i > position; i--) {
		mn.menuStack[i] = mn.menuStack[i - 1];
	}
	/* insert */
	mn.menuStack[position] = menu;
	mn.menuStackPos++;
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @param[in] delete Delete the menu from the menu stack before adding it again
 * @return pointer to menuNode_t
 * @todo Replace "i" by a menuNode_t, more easy to read
 */
static menuNode_t* MN_PushMenuDelete (const char *name, const char *parent, qboolean delete)
{
	menuNode_t *menu;

	MN_ReleaseInput();

	menu = MN_GetMenu(name);
	if (menu == NULL) {
		Com_Printf("Didn't find menu \"%s\"\n", name);
		return NULL;
	}

	/* found the correct add it to stack or bring it on top */
	if (delete)
		MN_DeleteMenuFromStack(menu);

	if (mn.menuStackPos < MAX_MENUSTACK)
		if (parent) {
			const int parentPos = MN_GetMenuPositionFromStackByName(parent);
			if (parentPos == -1) {
				Com_Printf("Didn't find parent menu \"%s\"\n", parent);
				return NULL;
			}
			MN_InsertMenuIntoStack(menu, parentPos + 1);
			menu->u.window.parent = mn.menuStack[parentPos];
		} else
			mn.menuStack[mn.menuStackPos++] = menu;
	else
		Com_Printf("Menu stack overflow\n");

	if (menu->behaviour->init) {
		menu->behaviour->init(menu);
	}

	/** @todo what does it mean? why here? */
	Key_SetDest(key_game);

	MN_InvalidateMouse();
	return menu;
}

/**
 * @brief Complete function for mn_push
 * @sa Cmd_AddParamCompleteFunction
 * @sa MN_PushMenu
 * @note Does not really complete the input - but shows at least all parsed menus
 */
int MN_CompleteWithMenu (const char *partial, const char **match)
{
	int i;
	int matches = 0;
	const char *localMatch[MAX_COMPLETE];
	const size_t len = strlen(partial);

	if (len == 0) {
		for (i = 0; i < mn.numMenus; i++)
			Com_Printf("%s\n", mn.menus[i]->name);
		return 0;
	}

	/* check for partial matches */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(partial, mn.menus[i]->name, len)) {
			Com_Printf("%s\n", mn.menus[i]->name);
			localMatch[matches++] = mn.menus[i]->name;
			if (matches >= MAX_COMPLETE) {
				Com_Printf("MN_CompleteWithMenu: hit MAX_COMPLETE\n");
				break;
			}
		}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @return pointer to menuNode_t
 */
menuNode_t* MN_PushMenu (const char *name, const char *parentName)
{
	return MN_PushMenuDelete(name, parentName, qtrue);
}

/**
 * @brief Console function to push a child menu onto the menu stack
 * @sa MN_PushMenu
 */
static void MN_PushChildMenu_f (void)
{
	if (Cmd_Argc() > 1)
		MN_PushMenu(Cmd_Argv(1), Cmd_Argv(2));
	else
		Com_Printf("Usage: %s <name> <parentname>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a menu onto the menu stack
 * @sa MN_PushMenu
 */
static void MN_PushMenu_f (void)
{
	if (Cmd_Argc() > 1)
		MN_PushMenu(Cmd_Argv(1), NULL);
	else
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to push a dropdown menu at a position. It work like MN_PushMenu but move the menu at the right position
 * @sa MN_PushMenu
 * @note The usage is "mn_push_dropdown sourcenode pointposition destinationnode pointposition"
 * sourcenode must be a node into the menu we want to push,
 * we will move the menu to have sourcenode over destinationnode
 * pointposition select for each node a position (like a corner).
 */
static void MN_PushDropDownMenu_f (void)
{
	vec2_t source;
	vec2_t destination;
	menuNode_t *node;
	byte pointPosition;
	size_t writedByte;
	int result;

	if (Cmd_Argc() != 4 && Cmd_Argc() != 5) {
		Com_Printf("Usage: %s <source-anchor> <point-in-source-anchor> <dest-anchor> <point-in-dest-anchor>\n", Cmd_Argv(0));
		return;
	}

	/* get the source anchor */
	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_PushDropDownMenu_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	result = Com_ParseValue(&pointPosition, Cmd_Argv(2), V_ALIGN, 0, sizeof(pointPosition), &writedByte);
	if (result != RESULT_OK) {
		Com_Printf("MN_PushDropDownMenu_f: '%s' in not a V_ALIGN\n", Cmd_Argv(2));
		return;
	}
	MN_NodeGetPoint(node, source, pointPosition);
	MN_NodeRelativeToAbsolutePoint(node, source);

	/* get the destination anchor */
	if (!strcmp(Cmd_Argv(4), "mouse")) {
		destination[0] = mousePosX;
		destination[1] = mousePosY;
	} else {
		/* get the source anchor */
		node = MN_GetNodeByPath(Cmd_Argv(3));
		if (node == NULL) {
			Com_Printf("MN_PushDropDownMenu_f: Node '%s' doesn't exist\n", Cmd_Argv(3));
			return;
		}
		result = Com_ParseValue (&pointPosition, Cmd_Argv(4), V_ALIGN, 0, sizeof(pointPosition), &writedByte);
		if (result != RESULT_OK) {
			Com_Printf("MN_PushDropDownMenu_f: '%s' in not a V_ALIGN\n", Cmd_Argv(4));
			return;
		}
		MN_NodeGetPoint(node, destination, pointPosition);
		MN_NodeRelativeToAbsolutePoint(node, destination);
	}

	/* update the menu and push it */
	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_PushDropDownMenu_f: Node '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}
	/** @todo Every node should have a menu; menu too */
	if (node->menu)
		node = node->menu;
	node->pos[0] += destination[0] - source[0];
	node->pos[1] += destination[1] - source[1];
	MN_PushMenu(node->name, NULL);
}

/**
 * @brief Console function to hide the HUD in battlescape mode
 * Note: relies on a "nohud" menu existing
 * @sa MN_PushMenu
 */
static void MN_PushNoHud_f (void)
{
	/* can't hide hud if we are not in battlescape */
	if (!CL_OnBattlescape())
		return;

	MN_PushMenu("nohud", NULL);
}

/**
 * @brief Console function to push a menu, without deleting its copies
 * @sa MN_PushMenu
 */
static void MN_PushCopyMenu_f (void)
{
	if (Cmd_Argc() > 1) {
		MN_PushMenuDelete(Cmd_Argv(1), NULL, qfalse);
	} else {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
	}
}

static void MN_RemoveMenuAtPositionFromStack (int position)
{
	int i;
	assert(position < mn.menuStackPos);
	assert(position >= 0);

	/* create space for the new menu */
	for (i = position; i < mn.menuStackPos; i++) {
		mn.menuStack[i] = mn.menuStack[i + 1];
	}
	mn.menuStack[mn.menuStackPos--] = NULL;
}

static void MN_CloseAllMenu (void)
{
	int i;
	for (i = mn.menuStackPos - 1; i >= 0; i--) {
		menuNode_t *menu = mn.menuStack[i];

		if (menu->u.window.onClose)
			MN_ExecuteEventActions(menu, menu->u.window.onClose);

		/* safe: unlink window */
		menu->u.window.parent = NULL;
		mn.menuStackPos--;
		mn.menuStack[mn.menuStackPos] = NULL;
	}
}

/**
 * @brief Check if a named menu is on the stack if active menus
 */
qboolean MN_IsMenuOnStack(const char* name)
{
	return MN_GetMenuPositionFromStackByName(name) != -1;
}

/**
 * @todo FInd  better name
 */
static void MN_CloseMenuByRef (menuNode_t *menu)
{
	int i;

	/** @todo If the focus is not on the menu we close, we don't need to remove it */
	MN_ReleaseInput();

	assert(mn.menuStackPos);
	i = MN_GetMenuPositionFromStackByName(menu->name);
	if (i == -1) {
		Com_Printf("Menu '%s' is not on the active stack\n", menu->name);
		return;
	}

	/* close child */
	while (i + 1 < mn.menuStackPos) {
		menuNode_t *m = mn.menuStack[i + 1];
		if (m->u.window.parent != menu) {
			break;
		}
		if (m->u.window.onClose)
			MN_ExecuteEventActions(m, m->u.window.onClose);
		m->u.window.parent = NULL;
		MN_RemoveMenuAtPositionFromStack(i + 1);
	}

	/* close the menu */
	if (menu->u.window.onClose)
		MN_ExecuteEventActions(menu, menu->u.window.onClose);
	menu->u.window.parent = NULL;
	MN_RemoveMenuAtPositionFromStack(i);

	MN_InvalidateMouse();
}

void MN_CloseMenu (const char* name)
{
	menuNode_t *menu = MN_GetMenu(name);
	if (menu == NULL) {
		Com_Printf("Menu '%s' doesn't exist\n", name);
		return;
	}

	/* found the correct add it to stack or bring it on top */
	MN_CloseMenuByRef(menu);
}

/**
 * @brief Pops a menu from the menu stack
 * @param[in] all If true pop all menus from stack
 * @sa MN_PopMenu_f
 */
void MN_PopMenu (qboolean all)
{
	menuNode_t *oldfirst = mn.menuStack[0];
	assert(mn.menuStackPos);

	if (all) {
		MN_CloseAllMenu();
	} else {
		menuNode_t *mainMenu = mn.menuStack[mn.menuStackPos - 1];
		if (mainMenu->u.window.parent)
			mainMenu = mainMenu->u.window.parent;
		MN_CloseMenuByRef(mainMenu);
	}

	if (!all && mn.menuStackPos == 0) {
		/* mn_main contains the menu that is the very first menu and should be
		 * pushed back onto the stack (otherwise there would be no menu at all
		 * right now) */
		if (!Q_strcmp(oldfirst->name, mn_main->string)) {
			if (mn_active->string[0] != '\0')
				MN_PushMenu(mn_active->string, NULL);
			if (!mn.menuStackPos)
				MN_PushMenu(mn_main->string, NULL);
		} else {
			if (mn_main->string[0] != '\0')
				MN_PushMenu(mn_main->string, NULL);
			if (!mn.menuStackPos)
				MN_PushMenu(mn_active->string, NULL);
		}
	}

	Key_SetDest(key_game);

	/* when we leave a menu and a menu cinematic is running... we should stop it */
	if (cls.playingCinematic == CIN_STATUS_MENU)
		CIN_StopCinematic();
}

/**
 * @brief Console function to close a named menu
 * @sa MN_PushMenu
 */
static void MN_CloseMenu_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	MN_CloseMenu(Cmd_Argv(1));
}

/**
 * @brief Console function to pop a menu from the menu stack
 * @sa MN_PopMenu
 */
static void MN_PopMenu_f (void)
{
	if (Cmd_Argc() > 2) {
		Com_Printf("Usage: %s\n", Cmd_Argv(0));
		return;
	}

	/** @todo move it into another function (mn_pop_esc) */
	if (Cmd_Argc() == 2 && !Q_strcmp(Cmd_Argv(1), "esc")) {
		const menuNode_t *menu = mn.menuStack[mn.menuStackPos - 1];
		assert(mn.menuStackPos);

		/* some window can prevent escape */
		if (menu->u.window.preventTypingEscape)
			return;
	}

	MN_PopMenu(qfalse);
}

/**
 * @brief Returns the current active menu from the menu stack or NULL if there is none
 * @return menuNode_t pointer from menu stack
 * @sa MN_GetMenu
 */
menuNode_t* MN_GetActiveMenu (void)
{
	return (mn.menuStackPos > 0 ? mn.menuStack[mn.menuStackPos - 1] : NULL);
}

/**
 * @brief Returns the name of the current menu
 * @return Active menu name, else empty string
 * @sa MN_GetActiveMenu
 */
const char* MN_GetActiveMenuName (void)
{
	const menuNode_t* menu = MN_GetActiveMenu();
	if (menu == NULL)
		return "";
	return menu->name;
}

/**
 * @brief Searches a given node in the current menu
 * @todo deprecated, we should not use it because its hard to predict result of "current" (we use popups...)
 * plus, if we need, we can use MN_GetActiveMenu and MN_GetNode
 * @sa MN_GetNode
 * @sa MN_GetActiveMenu
 */
menuNode_t* MN_GetNodeFromCurrentMenu (const char *name)
{
	return MN_GetNode(MN_GetActiveMenu(), name);
}

/**
 * @brief Check if a point is over a menu from the stack
 * @sa IN_Parse
 */
qboolean MN_IsPointOnMenu (int x, int y)
{
	const menuNode_t *hovered;

	if (MN_GetMouseCapture() != NULL)
		return qtrue;

	if (mn.menuStackPos != 0) {
		if (mn.menuStack[mn.menuStackPos - 1]->u.window.dropdown)
			return qtrue;
	}

	hovered = MN_GetHoveredNode();
	if (hovered) {
		/* else if it is a render node */
		if (hovered->menu && hovered == hovered->menu->u.window.renderNode) {
			return qfalse;
		}
		return qtrue;
	}

	return qtrue;
}

/**
 * @brief Searches all menus for the specified one
 * @param[in] name If name is NULL this function will return the current menu
 * on the stack - otherwise it will search the hole stack for a menu with the
 * id name
 * @return NULL if not found or no menu on the stack
 * @sa MN_GetActiveMenu
 */
menuNode_t *MN_GetMenu (const char *name)
{
	int i;

	assert(name);

	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strcmp(mn.menus[i]->name, name))
			return mn.menus[i];

	return NULL;
}

/**
 * @brief Invalidate all menus of the current stack.
 */
void MN_InvalidateStack (void)
{
	int pos;
	for (pos = 0; pos < mn.menuStackPos; pos++) {
		MN_Invalidate(mn.menuStack[pos]);
	}
}

/**
 * @brief Sets new x and y coordinates for a given menu
 */
void MN_SetNewMenuPos (menuNode_t* menu, int x, int y)
{
	if (menu)
		Vector2Set(menu->pos, x, y);
}

/**
 * @brief Console command for moving a menu
 */
void MN_SetNewMenuPos_f (void)
{
	menuNode_t* menu = MN_GetActiveMenu();

	if (Cmd_Argc() < 3)
		Com_Printf("Usage: %s <x> <y>\n", Cmd_Argv(0));

	MN_SetNewMenuPos(menu, atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)));
}

/**
 * @brief This will reinitialize the current visible menu
 * @note also available as script command mn_reinit
 * @todo replace that by a common action "call *ufopedia.init"
 */
static void MN_FireInit_f (void)
{
	const menuNode_t* menu;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <menu>\n", Cmd_Argv(0));
		return;
	}

	menu = MN_GetNodeByPath(Cmd_Argv(1));
	if (menu == NULL) {
		Com_Printf("MN_FireInit_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	if (!MN_NodeInstanceOf(menu, "menu")) {
		Com_Printf("MN_FireInit_f: Node '%s' is not a 'menu'\n", Cmd_Argv(1));
		return;
	}

	/* initialize it */
	if (menu) {
		if (menu->u.window.onInit)
			MN_ExecuteEventActions(menu, menu->u.window.onInit);
		Com_DPrintf(DEBUG_CLIENT, "Reinitialize %s\n", menu->name);
	}
}

void MN_InitMenus (void)
{
	/* add command */
	Cmd_AddCommand("mn_fireinit", MN_FireInit_f, "Call the init function of a menu");
	Cmd_AddCommand("mn_push", MN_PushMenu_f, "Push a menu to the menustack");
	Cmd_AddParamCompleteFunction("mn_push", MN_CompleteWithMenu);
	Cmd_AddCommand("mn_push_dropdown", MN_PushDropDownMenu_f, "Push a dropdown menu at a position");
	Cmd_AddCommand("mn_push_child", MN_PushChildMenu_f, "Push a menu to the menustack with a big dependancy to a parent menu");
	Cmd_AddCommand("mn_push_copy", MN_PushCopyMenu_f, NULL);
	Cmd_AddCommand("mn_pop", MN_PopMenu_f, "Pops the current menu from the stack");
	Cmd_AddCommand("mn_close", MN_CloseMenu_f, "Close a menu");
	Cmd_AddCommand("hidehud", MN_PushNoHud_f, _("Hide the HUD (press ESC to reactivate HUD)"));
	Cmd_AddCommand("mn_move", MN_SetNewMenuPos_f, "Moves the menu to a new position.");
}
