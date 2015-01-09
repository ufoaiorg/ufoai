/**
 * @file
 * @brief base code for scrollable node
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "../../../shared/mathlib.h"
#include "ui_node_abstractnode.h"

struct uiBehaviour_t;
struct uiAction_s;
struct uiNode_t;

class uiAbstractScrollableNode : public uiLocatedNode {
public:
	/**
	 * @todo remove it, use property listener
	 */
	bool isSizeChange(uiNode_t* node);
	bool scrollY(uiNode_t* node, int offset);
	bool setScrollY(uiNode_t* node, int viewPos, int viewSize, int fullSize);
};

/**
 * @brief Scroll representation
 */
struct uiScroll_t {
	int viewPos;			/**< Position of the view */
	int viewSize;			/**< Visible size */
	int fullSize;			/**< Full size allowed */

	bool set(int viewPos, int viewSize, int fullSize);
	bool move(int viewPos);
	/**
	 * @brief Move the position with a delta position
	 * @param[in] deltaPos Variation of the position
	 * @return True, if something have changed
	 */
	bool moveDelta(int deltaPos) {
		return move(this->viewPos + deltaPos);
	}
};

typedef struct {
	vec2_t cacheSize;		/**< check the size change while we don't have a real event from property setter */

	/** not yet implemented */
	uiScroll_t scrollX;
	uiScroll_t scrollY;

	struct uiAction_s* onViewChange;	/**< called when view change (number of elements...) */
} abstractScrollableExtraData_t;

void UI_RegisterAbstractScrollableNode(uiBehaviour_t* behaviour);
