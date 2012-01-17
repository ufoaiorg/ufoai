/**
 * @file ui_node_linechart.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_internal.h"
#include "../ui_render.h"
#include "ui_node_abstractnode.h"
#include "ui_node_linechart.h"

#include "../../renderer/r_draw.h"

#define EXTRADATA_TYPE lineChartExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

static void UI_LineChartNodeDraw (uiNode_t *node)
{
	lineStrip_t *lineStrip;
	const int dataId = EXTRADATA(node).dataId;
	vec3_t pos;

	if (dataId == 0)
		return;

	if (ui_global.sharedData[dataId].type != UI_SHARED_LINESTRIP) {
		Com_Printf("UI_LineStripNodeDraw: Node '%s' have use an invalide dataId type. LineStrip expected. dataId invalidated\n", UI_GetPath(node));
		EXTRADATA(node).dataId = 0;
		return;
	}

	UI_GetNodeAbsPos(node, pos);
	pos[2] = 0;

	UI_Transform(pos, NULL, NULL);

	/* Draw axes */
	if (EXTRADATA(node).displayAxes) {
		int axes[6];
		axes[0] = 0;
		axes[1] = 0;
		axes[2] = 0;
		axes[3] = (int) node->size[1];
		axes[4] = (int) node->size[0];
		axes[5] = (int) node->size[1];
		R_Color(EXTRADATA(node).axesColor);
		R_DrawLineStrip(3, axes);
	}

	/* Draw all linestrips. */
	lineStrip = ui_global.sharedData[dataId].data.lineStrip;
	for (; lineStrip; lineStrip = lineStrip->next) {
		/* Draw this line if it's valid. */
		if (lineStrip->pointList && lineStrip->numPoints > 0) {
			R_Color(lineStrip->color);
			R_DrawLineStrip(lineStrip->numPoints, lineStrip->pointList);
		}
	}
	R_Color(NULL);

	UI_Transform(NULL, NULL, NULL);
}

void UI_RegisterLineChartNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "linechart";
	behaviour->draw = UI_LineChartNodeDraw;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Identity the shared data the node use. It must be a LINESTRIP data. */
	UI_RegisterExtradataNodeProperty(behaviour, "dataid", V_UI_DATAID, lineChartExtraData_t, dataId);
	/* If true, it display axes of the chart. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayaxes", V_BOOL, lineChartExtraData_t, displayAxes);
	/* Axe color. */
	UI_RegisterExtradataNodeProperty(behaviour, "axescolor", V_COLOR, lineChartExtraData_t, axesColor);
}
