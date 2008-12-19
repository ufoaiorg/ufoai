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

#include "../../client.h"
#include "../m_parse.h"
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

	for (node = menu->firstChild; node; node = node->next)
		if (!Q_strncmp(name, node->name, sizeof(node->name)))
			break;

	return node;
}

/**
 * @brief Insert a node next another one into a menu. If prevNode is NULL add the node on the heap of the menu
 * @param[in] menu Menu where inserting a node
 * @param[in] prevNode previous node, will became before the newNode; else NULL if newNode will become the first child of the menu
 * @param[in] newNode node we insert
 */
void MN_InsertNode (menu_t* const menu, menuNode_t *prevNode, menuNode_t *newNode)
{
	assert(menu);
	assert(newNode);
	assert(!newNode->next);	/*< insert only a single element */
	assert(!prevNode || (prevNode->menu == menu && newNode->menu == menu));	/*< everything come from the same menu (force the dev to update himself this links) */
	if (prevNode == NULL) {
		newNode->next = menu->firstChild;
		menu->firstChild = newNode;
		if (menu->lastChild == NULL) {
			menu->lastChild = newNode;
		}
		return;
	}
	newNode->next = prevNode->next;
	prevNode->next = newNode;
	if (prevNode == menu->lastChild) {
		menu->lastChild = newNode;
	}
}

/**
 * @brief add a node at the end of the menu child
 * @todo after an update of the linked list of nodes (next, +prev, firstChild, +lastChild), we can improve this function O(1)
 */
void MN_AppendNode (menu_t* const menu, menuNode_t *newNode)
{
	MN_InsertNode(menu, menu->lastChild, newNode);
}

/**
 * @brief Called at the begin of the load from script
 */
void MN_WindowNodeLoading (menu_t *menu)
{
	menu->pos[0] = 0;
	menu->pos[1] = 0;
	menu->size[0] = VID_NORM_WIDTH;
	menu->size[1] = VID_NORM_HEIGHT;
}

static const int CONTROLS_IMAGE_DIMENSIONS = 17;
static const int CONTROLS_PADDING = 22;
static const int CONTROLS_SPACING = 5;

#define TOP_HEIGHT 46

/**
 * @brief Called at the end of the load from script
 */
void MN_WindowNodeLoaded (menu_t *menu)
{
	/* if it need, construct the drag button */
	if (menu->dragButton) {
		menuNode_t *control = MN_AllocNode("controls");
		Q_strncpyz(control->name, "move_window_button", sizeof(control->name));
		control->menu = menu;
		control->dataImageOrModel = NULL;
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		control->size[0] = menu->size[0];
		control->size[1] = TOP_HEIGHT;
		control->pos[0] = 0;
		control->pos[1] = 0;
		control->tooltip = _("Drag to move window");
		MN_AppendNode(menu, control);
	}

	/* if the menu should have a close button, add it here */
	if (menu->closeButton) {
		menuNode_t *button = MN_AllocNode("pic");
		const int positionFromRight = CONTROLS_PADDING;
		const char* command = MN_AllocString(va("mn_close %s;", menu->name), 0);
		Q_strncpyz(button->name, "close_window_button", sizeof(button->name));
		button->menu = menu;
		button->dataImageOrModel = "menu/close";
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		button->size[0] = CONTROLS_IMAGE_DIMENSIONS;
		button->size[1] = CONTROLS_IMAGE_DIMENSIONS;
		button->pos[0] = menu->size[0] - positionFromRight - button->size[0];
		button->pos[1] = CONTROLS_PADDING;
		button->tooltip = _("Close the window");
		MN_SetMenuAction(&button->onClick, EA_CMD, command);
		MN_AppendNode(menu, button);
	}
}

/**
 * @brief Valid properties for a window node (called yet 'menu')
 */
static const value_t windowNodeProperties[] = {
	{"noticepos", V_POS, offsetof(menu_t, noticePos), MEMBER_SIZEOF(menu_t, noticePos)},
	{"pos", V_POS, offsetof(menu_t, pos), MEMBER_SIZEOF(menu_t, pos)},
	{"size", V_POS, offsetof(menu_t, size), MEMBER_SIZEOF(menu_t, size)},
	{"dragbutton", V_BOOL, offsetof(menu_t, dragButton), MEMBER_SIZEOF(menu_t, dragButton)},
	{"closebutton", V_BOOL, offsetof(menu_t, closeButton), MEMBER_SIZEOF(menu_t, closeButton)},
	{"modal", V_BOOL, offsetof(menu_t, modal), MEMBER_SIZEOF(menu_t, modal)},

	{"init", V_SPECIAL_ACTION, offsetof(menu_t, onInit), MEMBER_SIZEOF(menu_t, onInit)},
	{"close", V_SPECIAL_ACTION, offsetof(menu_t, onClose), MEMBER_SIZEOF(menu_t, onClose)},
	{"leave", V_SPECIAL_ACTION, offsetof(menu_t, onLeave), MEMBER_SIZEOF(menu_t, onLeave)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterWindowNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "menu";
	behaviour->id = MN_WINDOW;
	/** @todo waithing for the merge of menu and node */
	/* behaviour->loading = MN_WindowNodeLoading; */
	/* behaviour->loaded = MN_WindowNodeLoaded; */
	behaviour->properties = windowNodeProperties;
}
