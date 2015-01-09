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

class uiCheckBoxNode : public uiAbstractValueNode {
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void onActivate(uiNode_t* node) override;
};

struct checkboxExtraData_t {
	abstractValueExtraData_t super;

	/** Sprite used as an icon for checked state */
	uiSprite_t* iconChecked;
	/** Sprite used as an icon for unchecked state */
	uiSprite_t* iconUnchecked;
	/** Sprite used as an icon for indeterminate state */
	uiSprite_t* iconIndeterminate;
	/** Sprite used as a background */
	uiSprite_t* background;
};

void UI_RegisterCheckBoxNode(uiBehaviour_t* behaviour);
