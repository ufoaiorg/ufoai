/**
 * @file m_main.c
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
#include "m_draw.h"
#include "m_data.h"
#include "m_input.h"
#include "m_timer.h"
#include "m_condition.h"
#include "node/m_node_abstractnode.h"
#include "../cl_cinematic.h"
#include "../client.h"

menuGlobal_t mn;
static cvar_t *mn_escpop;

/**
 * @brief Returns the ID of the last fullscreen ID. Before this, window should be hiden.
 * @return The last full screen window on the screen, else 0. If the stack is empty, return -1
 */
int MN_GetLastFullScreenWindow (void)
{
	/* stack pos */
	int windowId = mn.menuStackPos - 1;
	while (windowId > 0) {
		if (MN_WindowIsFullScreen(mn.menuStack[windowId]))
			break;
		windowId--;
	}
	/* if we find nothing we return 0 */
	return windowId;
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
 * @param[in] delete Delete the menu from the menu stack before readd it
 * @return pointer to menuNode_t
 * @todo Replace "i" by a menuNode_t, more easy to read
 */
static menuNode_t* MN_PushMenuDelete (const char *name, const char *parent, qboolean delete)
{
	menuNode_t *node;
	menuNode_t *menu = NULL;

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

	/* initialize it */
	if (menu->u.window.onInit)
		MN_ExecuteEventActions(menu, menu->u.window.onInit);

	if (cls.key_dest == key_input && msg_mode == MSG_MENU)
		Key_Event(K_ENTER, 0, qtrue, cls.realtime);
	Key_SetDest(key_game);

	/* if there is a timeout value set, init the menu with current client time */
	for (node = menu->firstChild; node; node = node->next) {
		if (node->timeOut)
			node->timePushed = cl.time;
	}

	/* callback into nodes */
	for (node = menu->firstChild; node; node = node->next) {
		if (node->behaviour->init) {
			node->behaviour->init(node);
		}
	}

	MN_InvalidateMouse();
	return menu;
}

/**
 * @brief Complete function for mn_push
 * @sa Cmd_AddParamCompleteFunction
 * @sa MN_PushMenu
 * @note Does not really complete the input - but shows at least all parsed menus
 */
int MN_CompletePushMenu (const char *partial, const char **match)
{
	int i, matches = 0;
	const char *localMatch[MAX_COMPLETE];
	const size_t len = strlen(partial);

	if (!len) {
		for (i = 0; i < mn.numMenus; i++)
			Com_Printf("%s\n", mn.menus[i]->name);
		return 0;
	}

	localMatch[matches] = NULL;

	/* check for partial matches */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(partial, mn.menus[i]->name, len)) {
			Com_Printf("%s\n", mn.menus[i]->name);
			localMatch[matches++] = mn.menus[i]->name;
			if (matches >= MAX_COMPLETE)
				break;
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
		Cvar_SetValue("mn_escpop", mn_escpop->value + 1);
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

qboolean MN_MenuIsOnStack(const char* name)
{
	return MN_GetMenuPositionFromStackByName(name) != -1;
}

/**
 * @todo FInd  better name
 */
static void MN_CloseMenuByRef (menuNode_t *menu)
{
	int i;

	MN_MouseRelease();
	MN_RemoveFocus();

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
		Com_Printf("Menu '%s' dont exists\n", name);
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

	/* make sure that we end all input buffers */
	if (cls.key_dest == key_input && msg_mode == MSG_MENU)
		Key_Event(K_ENTER, 0, qtrue, cls.realtime);

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
 * @note The cvar mn_escpop defined how often the MN_PopMenu function is called.
 * This is useful for e.g. nodes that doesn't have a render node (for example: video options)
 */
static void MN_PopMenu_f (void)
{
	if (Cmd_Argc() < 2 || !Q_strncmp(Cmd_Argv(1), "esc", 3)) {
		/** @todo we can do the same in a better way: event returning agreement */
		/* some window can prevent escape */
		const menuNode_t *menu = mn.menuStack[mn.menuStackPos - 1];
		assert(mn.menuStackPos);
		if (!menu->u.window.preventTypingEscape) {
			MN_PopMenu(qfalse);
		}
	} else {
		int i;

		for (i = 0; i < mn_escpop->integer; i++)
			MN_PopMenu(qfalse);

		Cvar_Set("mn_escpop", "1");
	}
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
 * @sa MN_GetNode
 */
menuNode_t* MN_GetNodeFromCurrentMenu (const char *name)
{
	return MN_GetNode(MN_GetActiveMenu(), name);
}

/**
 * @sa IN_Parse
 * @todo cleanup this function
 */
qboolean MN_CursorOnMenu (int x, int y)
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

	return qfalse;
}

/**
 * @brief Searches all menus for the specified one
 * @param[in] name If name is NULL this function will return the current menu
 * on the stack - otherwise it will search the hole stack for a menu with the
 * id name
 * @return NULL if not found or no menu on the stack
 * @sa MN_GetActiveMenu
 * @todo merge it with FindMenuByName, its the same function
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
 * @brief This will reinit the current visible menu
 * @note also available as script command mn_reinit
 */
static void MN_ReinitCurrentMenu_f (void)
{
	const menuNode_t* menu = MN_GetActiveMenu();
	/* initialize it */
	if (menu) {
		if (menu->u.window.onInit)
			MN_ExecuteEventActions(menu, menu->u.window.onInit);
		Com_DPrintf(DEBUG_CLIENT, "Reinit %s\n", menu->name);
	}
}

static void MN_Modify_f (void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(3));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(4));

	Cvar_SetValue(Cmd_Argv(1), value);
}


static void MN_ModifyWrap_f (void)
{
	float value;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));
		return;
	}

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(4));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(3));

	Cvar_SetValue(Cmd_Argv(1), value);
}


static void MN_ModifyString_f (void)
{
	qboolean next;
	const char *current, *list;
	char *tp;
	char token[MAX_VAR], last[MAX_VAR], first[MAX_VAR];
	int add;

	if (Cmd_Argc() < 4)
		Com_Printf("Usage: %s <name> <amount> <list>\n", Cmd_Argv(0));

	current = Cvar_VariableString(Cmd_Argv(1));
	add = atoi(Cmd_Argv(2));
	list = Cmd_Argv(3);
	last[0] = 0;
	first[0] = 0;
	next = qfalse;

	while (add) {
		tp = token;
		while (list[0] != '\0' && list[0] != ':') {
			/** @todo overflow check */
			*tp++ = *list++;
		}
		if (list[0] != '\0')
			list++;
		*tp = '\0';

		if (token[0] != '\0' && !first[0])
			Q_strncpyz(first, token, MAX_VAR);

		if (token[0] == '\0') {
			if (add < 0 || next)
				Cvar_Set(Cmd_Argv(1), last);
			else
				Cvar_Set(Cmd_Argv(1), first);
			return;
		}

		if (next) {
			Cvar_Set(Cmd_Argv(1), token);
			return;
		}

		if (!Q_strncmp(token, current, MAX_VAR)) {
			if (add < 0) {
				if (last[0])
					Cvar_Set(Cmd_Argv(1), last);
				else
					Cvar_Set(Cmd_Argv(1), first);
				return;
			} else
				next = qtrue;
		}
		Q_strncpyz(last, token, MAX_VAR);
	}
}


/**
 * @brief Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
 */
static void MN_Translate_f (void)
{
	qboolean next;
	const char *current, *list;
	char *orig, *trans;
	char original[MAX_VAR], translation[MAX_VAR];

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <source> <dest> <list>\n", Cmd_Argv(0));
		return;
	}

	current = Cvar_VariableString(Cmd_Argv(1));
	list = Cmd_Argv(3);
	next = qfalse;

	while (list[0] != '\0') {
		orig = original;
		while (list[0] != '\0' && list[0] != ':') {
			/** @todo overflow check */
			*orig++ = *list++;
		}
		*orig = '\0';
		list++;

		trans = translation;
		while (list[0] != '\0' && list[0] != ',') {
			/** @todo overflow check */
			*trans++ = *list++;
		}
		*trans = '\0';
		list++;

		if (!Q_strcmp(current, original)) {
			Cvar_Set(Cmd_Argv(2), _(translation));
			return;
		}
	}

	if (current[0] != '\0') {
		/* nothing found, copy value */
		Cvar_Set(Cmd_Argv(2), _(current));
	} else {
		Cvar_Set(Cmd_Argv(2), "");
	}
}

#ifdef DEBUG
/**
 * @brief display info about menu memory
 * @todo it can be nice to have number of nodes per behaviours
 */
static void MN_Memory_f (void)
{
	Com_Printf("\tAllocation:\n");
	Com_Printf("\t-Option allocation: %i/%i\n", mn.numOptions, MAX_MENUOPTIONS);
	Com_Printf("\t-Node allocation: %i/%i\n", mn.numNodes, MAX_MENUNODES);
	Com_Printf("\t-Menu allocation: %i/%i\n", mn.numMenus, MAX_MENUS);
	Com_Printf("\t-Rendering menu stack slot: %i\n", MAX_MENUSTACK);
	Com_Printf("\t-Action allocation: %i/%i\n", mn.numActions, MAX_MENUACTIONS);
	Com_Printf("\t-Model allocation: %i/%i\n", mn.numMenuModels, MAX_MENUMODELS);
	Com_Printf("\t-Exclude rect allocation: %i/%i\n", mn.numExcludeRect, MAX_EXLUDERECTS);
	Com_Printf("\t-Condition allocation: %i/%i\n", mn.numConditions, MAX_MENUCONDITIONS);
	Com_Printf("\t-AData allocation: "UFO_SIZE_T"/%i B\n", mn.curadata - mn.adata, mn.adataize);
	Com_Printf("\tMemory:\n");
	Com_Printf("\t-Option structure size: "UFO_SIZE_T" B\n", sizeof(menuOption_t));
	Com_Printf("\t-Node structure size: "UFO_SIZE_T" B\n", sizeof(menuNode_t));
	Com_Printf("\t-Extra data node structure size: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u));
	Com_Printf("\t\t-abstractvalue: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.abstractvalue));
	Com_Printf("\t\t-abstractscrollbar: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.abstractscrollbar));
	Com_Printf("\t\t-container: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.container));
	Com_Printf("\t\t-linestrip: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.linestrip));
	Com_Printf("\t\t-model: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.model));
	Com_Printf("\t\t-option: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.option));
	Com_Printf("\t\t-textentry: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.textentry));
	Com_Printf("\t\t-text: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.text));
	Com_Printf("\t\t-window: "UFO_SIZE_T" B\n", MEMBER_SIZEOF(menuNode_t, u.window));
	Com_Printf("\t-Action structure size: "UFO_SIZE_T" B\n", sizeof(menuAction_t));
	Com_Printf("\t-Model structure size: "UFO_SIZE_T" B\n", sizeof(menuModel_t));
	Com_Printf("\t-Condition structure size: "UFO_SIZE_T" B\n", sizeof(menuDepends_t));
	Com_Printf("\t-AData size: %i B\n", mn.adataize);
	Com_Printf("\t-Full size: "UFO_SIZE_T" B\n", sizeof(menuGlobal_t) + mn.adataize);
}
#endif

/**
 * @brief Reset and free the menu data hunk
 * @note Even called in case of an error when CL_Shutdown was called - maybe even
 * before CL_InitLocal (and thus MN_InitStartup) was called
 * @sa CL_Shutdown
 * @sa MN_InitStartup
 */
void MN_Shutdown (void)
{
	if (mn.adataize)
		Mem_Free(mn.adata);
	mn.adata = NULL;
	mn.adataize = 0;
}

#define MENU_HUNK_SIZE 0x40000

void MN_Init (void)
{
#ifdef DEBUG
	MN_UnittestTimer();
#endif

	/* reset menu structures */
	memset(&mn, 0, sizeof(mn));

	/* add cvars */
	mn_escpop = Cvar_Get("mn_escpop", "1", 0, NULL);

	/* add menu comamnds */
	Cmd_AddCommand("mn_reinit", MN_ReinitCurrentMenu_f, "This will reinit the current menu (recall the init function)");
	Cmd_AddCommand("mn_modify", MN_Modify_f, NULL);
	Cmd_AddCommand("mn_modifywrap", MN_ModifyWrap_f, NULL);
	Cmd_AddCommand("mn_modifystring", MN_ModifyString_f, NULL);
	Cmd_AddCommand("mn_translate", MN_Translate_f, NULL);
#ifdef DEBUG
	Cmd_AddCommand("debug_mnmemory", MN_Memory_f, "Display info about menu memory allocation");
#endif
	Cmd_AddCommand("mn_push", MN_PushMenu_f, "Push a menu to the menustack");
	Cmd_AddParamCompleteFunction("mn_push", MN_CompletePushMenu);
	Cmd_AddCommand("mn_push_child", MN_PushChildMenu_f, "Push a menu to the menustack with a big dependancy to a parent menu");
	Cmd_AddCommand("mn_push_copy", MN_PushCopyMenu_f, NULL);
	Cmd_AddCommand("mn_pop", MN_PopMenu_f, "Pops the current menu from the stack");
	Cmd_AddCommand("mn_close", MN_CloseMenu_f, "Close a menu");
	Cmd_AddCommand("hidehud", MN_PushNoHud_f, _("Hide the HUD (press ESC to reactivate HUD)"));

	Cmd_AddCommand("mn_move", MN_SetNewMenuPos_f, "Moves the menu to a new position.");

	MN_InitData();

	/* 256kb */
	/** @todo (menu) Get rid of adata, curadata and adataize */
	mn.adataize = MENU_HUNK_SIZE;
	mn.adata = (byte*)Mem_PoolAlloc(mn.adataize, cl_menuSysPool, CL_TAG_MENU);
	mn.curadata = mn.adata;

	MN_InitNodes();
	MN_DrawMenusInit();
}
