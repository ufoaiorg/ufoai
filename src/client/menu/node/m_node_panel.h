/**
 * @file m_node_panel.h
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

#ifndef CLIENT_MENU_M_NODE_PANEL_H
#define CLIENT_MENU_M_NODE_PANEL_H

struct menuNode_s;
struct nodeBehaviour_s;

typedef enum {
	LAYOUT_NONE,
	LAYOUT_TOP_DOWN_FLOW,
	LAYOUT_BORDER,
	LAYOUT_PACK,
	LAYOUT_STAR,

	LAYOUT_MAX,
	LAYOUT_ENSURE_32BIT = 0x7FFFFFFF
} panelLayout_t;

/**
 * @brief extradata for the panel node
 */
typedef struct {
	panelLayout_t layout;	/**< The layout manager the panel is using to render all its children */
	int layoutMargin;		/**< The margin between all children nodes of the panel */
} panelExtraData_t;

void MN_RegisterPanelNode(struct nodeBehaviour_s *behaviour);

void MN_StarLayout(struct menuNode_s *node);

#endif
