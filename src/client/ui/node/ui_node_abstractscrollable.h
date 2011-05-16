/**
 * @file ui_node_abstractscrollable.h
 * @brief base code for scrollable node
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

#ifndef CLIENT_UI_UI_NODE_ABSTRACTSCROLLABLE_H
#define CLIENT_UI_UI_NODE_ABSTRACTSCROLLABLE_H

#include "../../../shared/mathlib.h"

struct uiBehaviour_s;
struct uiAction_s;
struct uiNode_s;

/**
 * @brief Scroll representation
 */
typedef struct {
	int viewPos;			/**< Position of the view */
	int viewSize;			/**< Visible size */
	int fullSize;			/**< Full size allowed */
} uiScroll_t;

qboolean UI_SetScroll(uiScroll_t *scroll, int viewPos, int viewSize, int fullSize);

typedef struct {
	vec2_t cacheSize;		/**< check the size change while we dont have a realy event from property setter */

	/** not yet implemented */
	uiScroll_t scrollX;
	uiScroll_t scrollY;

	struct uiAction_s *onViewChange;	/**< called when view change (number of elements...) */
} abstractScrollableExtraData_t;

qboolean UI_AbstractScrollableNodeIsSizeChange(struct uiNode_s *node);
qboolean UI_AbstractScrollableNodeScrollY(struct uiNode_s *node, int offset);
qboolean UI_AbstractScrollableNodeSetY(struct uiNode_s *node, int viewPos, int viewSize, int fullSize);

void UI_RegisterAbstractScrollableNode(struct uiBehaviour_s *behaviour);

#endif
