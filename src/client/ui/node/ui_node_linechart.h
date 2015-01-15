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

#define MAX_LINESTRIPS 16

class uiLineChartNode : public uiLocatedNode {
	void draw(uiNode_t* node) override;
};

/**
 * @brief an element of the line chart
 */
typedef struct lineStrip_s {
	int* pointList;				/**< list of value */
	int numPoints;				/**< number of values */
	vec4_t color;				/**< color of the line strip */
	struct lineStrip_s* next;	/**< next line strip */
} lineStrip_t;

/**
 * @brief extradata for the linechart node
 * @todo add info about axes min-max...
 */
typedef struct lineChartExtraData_s {
	int dataId;					/**< ID of the line strips */
	bool displayAxes;			/**< If true the node display axes */
	vec4_t axesColor;			/**< color of the axes */
} lineChartExtraData_t;

void UI_RegisterLineChartNode(uiBehaviour_t* behaviour);
