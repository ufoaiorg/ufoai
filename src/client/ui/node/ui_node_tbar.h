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

class uiTBarNode : public uiAbstractValueNode {
	void draw(uiNode_t* node) override;
};

void UI_RegisterTBarNode(uiBehaviour_t* behaviour);

typedef struct tbarExtraData_s {
	abstractValueExtraData_t super;
	const char* image;
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
} tbarExtraData_t;
