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

class uiSpinnerNode : public uiAbstractValueNode {
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	void onCapturedMouseLost(uiNode_t* node) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
public:
	void repeat (uiNode_t* node, struct uiTimer_s* timer);
protected:
	bool isPositionIncrease(uiNode_t* node, int x, int y);
	bool step (uiNode_t* node, bool down);
};

struct spinnerExtraData_t {
	abstractValueExtraData_s super;

	uiSprite_t* background;		/**< Link to the background */
	uiSprite_t* bottomIcon;		/**< Link to the icon used for the bottom button */
	uiSprite_t* topIcon;		/**< Link to the icon used for the top button */
	int mode;					/**< The way the node react to input (see spinnerMode_t) */
	bool horizontal;			/**< Horizontal orientation for spinner images */
	bool inverted;				/**< Invert spinner directions */
};

void UI_RegisterSpinnerNode(uiBehaviour_t* behaviour);
