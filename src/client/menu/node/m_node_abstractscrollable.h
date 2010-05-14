/**
 * @file m_node_abstractscrollable.h
 * @brief base code for scrollable node
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTSCROLLABLE_H
#define CLIENT_MENU_M_NODE_ABSTRACTSCROLLABLE_H

#include "../../../shared/mathlib.h"

struct nodeBehaviour_s;
struct menuAction_s;
struct menuNode_s;

/**
 * @brief Scroll representation
 */
typedef struct {
	int viewPos;			/**< Position of the view */
	int viewSize;			/**< Visible size */
	int fullSize;			/**< Full size allowed */
} menuScroll_t;

qboolean MN_SetScroll(menuScroll_t *scroll, int viewPos, int viewSize, int fullSize);

typedef struct {
	vec2_t cacheSize;		/**< check the size change while we dont have a realy event from property setter */

	/** yet implemented nowhere */
	menuScroll_t scrollX;
	menuScroll_t scrollY;

	struct menuAction_s *onViewChange;	/**< called when view change (number of elements...) */
} abstractScrollableExtraData_t;

qboolean MN_AbstractScrollableNodeIsSizeChange(struct menuNode_s *node);
qboolean MN_AbstractScrollableNodeScrollY(struct menuNode_s *node, int offset);
qboolean MN_AbstractScrollableNodeSetY(struct menuNode_s *node, int viewPos, int viewSize, int fullSize);

void MN_RegisterAbstractScrollableNode(struct nodeBehaviour_s *behaviour);

#endif
