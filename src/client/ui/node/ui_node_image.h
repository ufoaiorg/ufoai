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

#include "ui_node_abstractnode.h"

void UI_RegisterImageNode(uiBehaviour_t* behaviour);
void UI_ImageNodeDraw(uiNode_t* node);

class uiImageNode : public uiLocatedNode {
public:
	void onLoaded(uiNode_t* node) override;
	void draw(uiNode_t* node) override;
};

typedef struct imageExtraData_s {
	const char* source;
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
	bool preventRatio;
	bool mousefx;
} imageExtraData_t;
