/**
 * @file m_node_abstractscrollable.h
 * @brief base code for scrollable node
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTSCROLLABLE_H
#define CLIENT_MENU_M_NODE_ABSTRACTSCROLLABLE_H

#include "../../../shared/mathlib.h"

struct nodeBehaviour_s;
struct menuAction_s;
struct menuNode_s;

typedef struct {
	vec2_t cacheSize;		/**< check the size change while we dont have a realy event from property setter */
	int viewPosX;			/**< Position of the view */
	int viewSizeX;			/**< Visible size */
	int fullSizeX;			/**< Full size allowed */

	/** yet implemented nowhere */
	int viewPosY;			/**< Position of the view */
	int viewSizeY;			/**< Visible size */
	int fullSizeY;			/**< Full size allowed */

	struct menuAction_s *onViewChange;	/**< called when view change (number of elements...) */
} abstractScrollableExtraData_t;

qboolean MN_AbstractScrollableNodeIsSizeChange(struct menuNode_s *node);
void MN_RegisterAbstractScrollableNode(struct nodeBehaviour_s *behaviour);

#endif
