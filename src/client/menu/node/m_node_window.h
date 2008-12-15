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

#include "../../../game/q_shared.h"
#include "../m_nodes.h"

/** @brief menu with all it's nodes linked in */
typedef struct menu_s {
	char name[MAX_VAR];
	int eventTime;

	struct menu_s *super; /**< Menu inherited, else NULL */

	vec2_t pos;	/**< the position of the menu */
	vec2_t size;	/**< the size of the menu */
	vec2_t noticePos; /**< the position where the cl.msgText messages are rendered */
	qboolean dragButton;
	qboolean closeButton;
	qboolean modal;

	menuNode_t *firstChild;	/**< first element of linked list of child */
	menuNode_t *lastChild;	/**< last element of linked list of child */

	menuNode_t *popupNode;
	menuNode_t *renderNode;

	/** @todo think about converting it to action instead of node */
	menuNode_t *eventNode;	/**< single 'func' node, or NULL */
	struct menuAction_s *onInit; 	/**< Call when the menu is push */
	struct menuAction_s *onClose;	/**< Call when the menu is pop */
	struct menuAction_s *onTimeOut;	/**< Call when the own timer of the menu out */
	struct menuAction_s *onLeave;	/**< Call when mouse leave the window? call by cl_input */

} menu_t;

void MN_RegisterWindowNode(nodeBehaviour_t *behaviour);

menuNode_t *MN_GetNode(const menu_t* const menu, const char *name);
void MN_InsertNode(menu_t* const menu, menuNode_t *prevNode, menuNode_t *newNode);
void MN_AppendNode(menu_t* const menu, menuNode_t *newNode);

void MN_WindowNodeLoading(menu_t *menu);
void MN_WindowNodeLoaded(menu_t *menu);

#endif
