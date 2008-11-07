/**
 * @file m_node_window.c
 * @note this file is about menu function. Its not yet a real node,
 * but it may become one. Think the code like that will help to merge menu and node.
 * @note It used 'window' instead of 'menu', because a menu is not this king of widget
 * @todo merge here 'windowpanel' when it will be a real node
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

#include "m_node_window.h"

/**
 * @brief Searches all nodes in the given menu for a given nodename
 * @sa MN_GetNodeFromCurrentMenu
 */
menuNode_t *MN_GetNode (const menu_t* const menu, const char *name)
{
	menuNode_t *node = NULL;

	if (!menu)
		return NULL;

	for (node = menu->firstNode; node; node = node->next)
		if (!Q_strncmp(name, node->name, sizeof(node->name)))
			break;

	return node;
}

/**
 * @brief Return the last node from the menu
 * @return The last node, else NULL if no nodes
 */
menuNode_t *MN_GetLastNode (const menu_t* const menu)
{
	menuNode_t *prev;

	if (!menu->firstNode)
		return NULL;

	prev = menu->firstNode;
	while (prev->next) {
		prev = prev->next;
	}
	return prev;
}

/**
 * @brief Insert a node next another one into a menu. If prevNode is NULL add the node on the heap of the menu
 */
void MN_InsertNode (menu_t* const menu, menuNode_t *prevNode, menuNode_t *newNode)
{
	if (prevNode == NULL) {
		menu->firstNode = newNode;
		return;
	}
	newNode->next = prevNode->next;
	prevNode->next = newNode;
}

/**
 * @brief Called at the begin of the load from script
 */
void MN_WindowNodeLoading (menu_t *menu)
{
}

/**
 * @brief Called at the end of the load from script
 */
void MN_WindowNodeLoaded (menu_t *menu)
{
	menuNode_t *prev;

	/* if it need, construct the drag button */
	if (menu->dragButton) {
		menuNode_t *control = MN_AllocNode(MN_CONTROLS);
		Q_strncpyz(control->name, "move_window_button", sizeof(control->name));
		control->menu = menu;
		control->dataImageOrModel = "menu/move";
		control->size[0] = 17;
		control->size[1] = 17;
		control->pos[0] = menu->size[0] - 22 - control->size[0];
		control->pos[1] = 22;
		prev = MN_GetLastNode(menu);
		MN_InsertNode(menu, prev, control);
	}
}

/**
 * @brief Valid properties for a window node (called yet 'menu')
 */
static const value_t windowNodeProperties[] = {
	{"noticepos", V_POS, offsetof(menu_t, noticePos), MEMBER_SIZEOF(menu_t, noticePos)},
	{"pos", V_POS, offsetof(menu_t, pos), MEMBER_SIZEOF(menu_t, pos)},
	{"origin", V_POS, offsetof(menu_t, pos), MEMBER_SIZEOF(menu_t, pos)},
	{"size", V_POS, offsetof(menu_t, size), MEMBER_SIZEOF(menu_t, size)},
	{"dragbutton", V_BOOL, offsetof(menu_t, dragButton), MEMBER_SIZEOF(menu_t, dragButton)},
	{"closebutton", V_BOOL, offsetof(menu_t, closeButton), MEMBER_SIZEOF(menu_t, closeButton)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterNodeWindow (nodeBehaviour_t *behaviour)
{
	behaviour->name = "menu";
	/* behaviour->loading = MN_WindowNodeLoading; */
	/* behaviour->loaded = MN_WindowNodeLoaded; */
	behaviour->properties = windowNodeProperties;
}
