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

#include "ui_node_image.h"

class uiEkgNode : public uiImageNode {
public:
	void onLoading(uiNode_t* node) override;
	void draw(uiNode_t* node) override;
};

typedef struct ekgExtraData_s {
	imageExtraData_t super;
	float scrollSpeed;
	float scaleCvarValue;
} ekgExtraData_t;

void UI_RegisterEKGNode(uiBehaviour_t* behaviour);
