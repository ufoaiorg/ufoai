/**
 * @file ui_node_panel.h
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

#ifndef CLIENT_UI_UI_NODE_PANEL_H
#define CLIENT_UI_UI_NODE_PANEL_H

#include "ui_node_abstractscrollable.h"

struct uiNode_s;
struct uiBehaviour_s;

typedef enum {
	LAYOUT_NONE,
	LAYOUT_TOP_DOWN_FLOW,
	LAYOUT_LEFT_RIGHT_FLOW,
	LAYOUT_BORDER,
	LAYOUT_PACK,
	LAYOUT_STAR,
	LAYOUT_CLIENT,
	LAYOUT_COLUMN,

	LAYOUT_MAX,
	LAYOUT_ENSURE_32BIT = 0x7FFFFFFF
} panelLayout_t;

typedef enum {
	LAYOUTALIGN_NONE = 0,

	/* vertical and horizontal flag bits */
	LAYOUTALIGN_H_MASK    = 0x03,
	LAYOUTALIGN_H_LEFT    = 0x01,
	LAYOUTALIGN_H_MIDDLE  = 0x02,
	LAYOUTALIGN_H_RIGHT   = 0x03,
	LAYOUTALIGN_V_MASK    = 0x0C,
	LAYOUTALIGN_V_TOP     = 0x04,
	LAYOUTALIGN_V_MIDDLE  = 0x08,
	LAYOUTALIGN_V_BOTTOM  = 0x0C,

	/* common alignment */
	LAYOUTALIGN_TOPLEFT     = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_TOP         = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_TOPRIGHT    = (LAYOUTALIGN_V_TOP | LAYOUTALIGN_H_RIGHT),
	LAYOUTALIGN_LEFT        = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_MIDDLE      = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_RIGHT       = (LAYOUTALIGN_V_MIDDLE | LAYOUTALIGN_H_RIGHT),
	LAYOUTALIGN_BOTTOMLEFT  = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_LEFT),
	LAYOUTALIGN_BOTTOM      = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_MIDDLE),
	LAYOUTALIGN_BOTTOMRIGHT = (LAYOUTALIGN_V_BOTTOM | LAYOUTALIGN_H_RIGHT),

	/* special align, everything bigger 0x10 */
	LAYOUTALIGN_SPECIAL     = 0x10,

	/* pack and star layout manager only */
	LAYOUTALIGN_FILL,

	LAYOUTALIGN_MAX,
	LAYOUTALIGN_ENSURE_32BIT = 0x7FFFFFFF
} layoutAlign_t;

/**
 * @param align a layoutAlign_t
 * @return LAYOUTALIGN_V_TOP, LAYOUTALIGN_V_MIDDLE, LAYOUTALIGN_V_BOTTOM, else LAYOUTALIGN_NONE
 */
#define UI_GET_VERTICAL_ALIGN(align) ((align >= LAYOUTALIGN_SPECIAL)?LAYOUTALIGN_NONE:(LAYOUTALIGN_V_MASK & align))
/**
 * @param align a layoutAlign_t
 * @return LAYOUTALIGN_H_LEFT, LAYOUTALIGN_H_MIDDLE, LAYOUTALIGN_H_RIGHT, else LAYOUTALIGN_NONE
 */
#define UI_GET_HORIZONTAL_ALIGN(align) ((align >= LAYOUTALIGN_SPECIAL)?LAYOUTALIGN_NONE:(LAYOUTALIGN_H_MASK & align))

/**
 * @brief extradata for the panel node
 */
typedef struct {
	abstractScrollableExtraData_t super;
	panelLayout_t layout;		/**< The layout manager the panel is using to render all its children */
	int layoutMargin;			/**< The margin between all children nodes of the panel */
	int layoutColumns;			/**< The number of columns (only used by LAYOUT_COLUMN)  */
	qboolean wheelScrollable;	/**< If scrolling with mouse wheel is enabled */
} panelExtraData_t;

void UI_RegisterPanelNode(struct uiBehaviour_s *behaviour);

void UI_StarLayout(struct uiNode_s *node);

#endif
