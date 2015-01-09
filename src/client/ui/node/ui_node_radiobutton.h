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

#include "../ui_nodes.h"

class uiRadioButtonNode : public uiLocatedNode {
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void draw(uiNode_t* node) override;
	void onActivate(uiNode_t* node) override;
};

typedef struct radioButtonExtraData_s {
	char* cvar;
	float value;
	char* string;
	uiSprite_t* background;
	uiSprite_t* icon;	/**< Link to an icon */
	bool flipIcon;			/**< Flip the icon rendering (horizontal) */
} radioButtonExtraData_t;

void UI_RegisterRadioButtonNode(uiBehaviour_t* behaviour);
