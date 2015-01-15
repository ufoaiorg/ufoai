/**
 * @file
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

#include "ui_node_abstractvalue.h"

class uiBarNode : public uiAbstractValueNode {
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
};

/* prototype */
struct uiBehaviour_t;

/**
 * @brief extradata for the panel node
 */
typedef struct {
	abstractValueExtraData_t super;
	align_t orientation;	/**< Orientation of the bar (left, right, top, down) */
	bool readOnly;	/**< True if the user can't edit the content */
	bool noHover;	/**< True to show the bar at 100% opacity - even if not hovered */
} barExtraData_t;

void UI_RegisterBarNode(uiBehaviour_t* behaviour);
