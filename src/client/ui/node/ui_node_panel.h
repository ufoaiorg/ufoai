/**
 * @file ui_node_panel.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_PANEL_H
#define CLIENT_UI_UI_NODE_PANEL_H

#include "ui_node_abstractscrollable.h"

struct uiNode_s;
struct uiBehaviour_s;

typedef enum {
	LAYOUT_NONE,
	LAYOUT_TOP_DOWN_FLOW,
	LAYOUT_BORDER,
	LAYOUT_PACK,
	LAYOUT_STAR,
	LAYOUT_CLIENT,
	LAYOUT_COLUMN,

	LAYOUT_MAX,
	LAYOUT_ENSURE_32BIT = 0x7FFFFFFF
} panelLayout_t;

/**
 * @brief extradata for the panel node
 */
typedef struct {
	abstractScrollableExtraData_t super;
	panelLayout_t layout;	/**< The layout manager the panel is using to render all its children */
	int layoutMargin;		/**< The margin between all children nodes of the panel */
	int layoutColumns;		/**< The number of columns (only used by LAYOUT_COLUMN)  */
} panelExtraData_t;

void UI_RegisterPanelNode(struct uiBehaviour_s *behaviour);

void UI_StarLayout(struct uiNode_s *node);

#endif
