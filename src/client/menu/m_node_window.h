/**
 * @file m_node_window.h
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

#ifndef CLIENT_MENU_M_NODE_WINDOW_H
#define CLIENT_MENU_M_NODE_WINDOW_H

#include "../../game/q_shared.h"
#include "m_nodes.h"

/** @brief menu with all it's nodes linked in */
typedef struct menu_s {
	char name[MAX_VAR];
	int eventTime;
	vec2_t pos;	/**< the position of the menu */
	vec2_t size;	/**< the size of the menu */
	vec2_t noticePos; /**< the position where the cl.msgText messages are rendered */
	qboolean dragButton;
	qboolean closeButton;

	menuNode_t *firstChild;	/**< first element of linked list of child */

	/** @todo remove it after the cleanup of the input handler */
	menuNode_t *hoverNode;	/**< current hovered node */

	menuNode_t *popupNode;
	menuNode_t *renderNode;

	/** @todo think about converting it to action instead of node */
	menuNode_t *initNode;	/**< node to execute on init. Single 'func' node, or NULL */
	menuNode_t *closeNode;	/**< node to execute on close. Single 'func' node, or NULL */
	menuNode_t *eventNode;	/**< single 'func' node, or NULL */
	menuNode_t *leaveNode;	/**< single 'func' node, or NULL */
} menu_t;

void MN_RegisterWindowNode(nodeBehaviour_t *behaviour);

menuNode_t *MN_GetNode(const menu_t* const menu, const char *name);
menuNode_t *MN_GetLastNode(const menu_t* const menu);
void MN_InsertNode(menu_t* const menu, menuNode_t *prevNode, menuNode_t *newNode);
void MN_AppendNode(menu_t* const menu, menuNode_t *newNode);
menuNode_t* MN_CloneNode(const menuNode_t* node, struct menu_s *newMenu, qboolean recursive);

void MN_WindowNodeLoading(menu_t *menu);
void MN_WindowNodeLoaded(menu_t *menu);

#endif
