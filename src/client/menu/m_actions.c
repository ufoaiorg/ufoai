/**
 * @file m_actions.c
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

#include "../../game/q_shared.h"
#include "../client.h"
#include "../cl_input.h"
#include "m_main.h"

menuNode_t *focusNode;

/**
 * @sa MN_ParseAction
 */
void MN_ExecuteActions (const menu_t* const menu, menuAction_t* const first)
{
	menuAction_t *action;
	byte *data;

	for (action = first; action; action = action->next)
		switch (action->type) {
		case EA_NULL:
			/* do nothing */
			break;
		case EA_CMD:
			/* execute a command */
			if (action->data)
				Cbuf_AddText(va("%s\n", (char*)action->data));
			break;
		case EA_CALL:
			/* call another function */
			MN_ExecuteActions(menu, **(menuAction_t ***) action->data);
			break;
		case EA_VAR:
			break;
		case EA_NODE:
			/* set a property */
			if (action->data) {
				menuNode_t *node;

				data = action->data;
				data += ALIGN(strlen(action->data) + 1);

				/* search the node */
				node = MN_GetNode(menu, (char *) action->data);

				if (!node) {
					/* didn't find node -> "kill" action and print error */
					action->type = EA_NULL;
					Com_Printf("MN_ExecuteActions: node \"%s\" doesn't exist\n", (char *) action->data);
					break;
				}

				/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
				if (action->scriptValues->ofs > 0 && (action->scriptValues->ofs < (size_t)-5))
					Com_SetValue(node, (char *) data, action->scriptValues->type, action->scriptValues->ofs, action->scriptValues->size);
				else
					node->data[-((int)action->scriptValues->ofs)] = data;
			}
			break;
		default:
			Sys_Error("unknown action type\n");
			break;
		}
}

void MN_Command_f (void)
{
	menuNode_t *node;
	const char *name;
	int i;

	name = Cmd_Argv(0);

	/* first search all menus on the stack */
	for (i = 0; i < mn.menuStackPos; i++)
		for (node = mn.menuStack[i]->firstNode; node; node = node->next)
			if (node->type == MN_CONFUNC && !Q_strncmp(node->name, name, sizeof(node->name))) {
				/* found the node */
				MN_ExecuteActions(mn.menuStack[i], node->click);
				return;
			}

	/* not found - now query all in the menu definitions */
	for (i = 0; i < mn.numMenus; i++)
		for (node = mn.menus[i].firstNode; node; node = node->next)
			if (node->type == MN_CONFUNC && !Q_strncmp(node->name, name, sizeof(node->name))) {
				/* found the node */
				MN_ExecuteActions(&mn.menus[i], node->click);
				return;
			}

	Com_Printf("MN_Command_f: confunc '%s' was not found in any menu\n", name);
}


/**
 * @sa MN_FocusExecuteActionNode
 * @note node must not be in menu
 */
static menuNode_t *MN_GetNextActionNode (menuNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (MN_CheckCondition(node) && !node->invis
		&& ((node->click && node->mouseIn) || node->mouseIn))
			return node;
		node = node->next;
	}
	return NULL;
}

/**
 * @sa MN_FocusExecuteActionNode
 * @sa MN_FocusNextActionNode
 * @sa IN_Frame
 * @sa Key_Event
 */
void MN_FocusRemove (void)
{
	if (mouseSpace != MS_MENU)
		return;

	if (focusNode)
		MN_ExecuteActions(focusNode->menu, focusNode->mouseOut);
	focusNode = NULL;
}

/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusNextActionNode
 */
qboolean MN_FocusExecuteActionNode (void)
{
	if (mouseSpace != MS_MENU)
		return qfalse;

	if (focusNode) {
		if (focusNode->click) {
			MN_ExecuteActions(focusNode->menu, focusNode->click);
		}
		MN_ExecuteActions(focusNode->menu, focusNode->mouseOut);
		focusNode = NULL;
		return qtrue;
	}

	return qfalse;
}

/**
 * @sa MN_FocusRemove
 */
static qboolean MN_FocusSetNode (menuNode_t* node)
{
	if (!node) {
		MN_FocusRemove();
		return qfalse;
	}

	/* reset already focused node */
	MN_FocusRemove();

	focusNode = node;
	MN_ExecuteActions(node->menu, node->mouseIn);

	return qtrue;
}

/**
 * @brief Set the focus to the next action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusExecuteActionNode
 */
qboolean MN_FocusNextActionNode (void)
{
	menu_t* menu;
	static int i = MAX_MENUSTACK + 1;	/* to cycle between all menus */

	if (mouseSpace != MS_MENU)
		return qfalse;

	if (i >= mn.menuStackPos)
		i = MN_GetVisibleMenuCount();

	assert(i >= 0);

	if (focusNode) {
		menuNode_t *node = MN_GetNextActionNode(focusNode);
		if (node)
			return MN_FocusSetNode(node);
	}

	while (i < mn.menuStackPos) {
		menu = mn.menuStack[i++];
		if (MN_FocusSetNode(MN_GetNextActionNode(menu->firstNode)))
			return qtrue;
	}
	i = MN_GetVisibleMenuCount();

	/* no node to focus */
	MN_FocusRemove();

	return qfalse;
}

/**
 * @brief Set a new action to a node->menuAction_t pointer
 * @param[in] type EA_CMD
 * @param[in] data The data for this action - in case of EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in mn.menuActions array
 */
menuAction_t *MN_SetMenuAction (menuAction_t** action, int type, const void *data)
{
	menuAction_t *newAction;

	if (*action)
		Sys_Error("There is already an action assigned\n");
	newAction = (menuAction_t *)Mem_PoolAlloc(sizeof(*newAction), cl_menuSysPool, CL_TAG_MENU);
	newAction->type = type;
	switch (type) {
	case EA_CMD:
		newAction->data = Mem_PoolStrDup((const char *)data, cl_menuSysPool, CL_TAG_MENU);;
		break;
	default:
		Sys_Error("Action type %i is not yet implemented", type);
	}

	return newAction;
}
