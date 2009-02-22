/**
 * @file m_node_linechart.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_internal.h"
#include "m_node_abstractnode.h"
#include "m_node_linechart.h"

#include "../../renderer/r_draw.h"
#include "../../renderer/r_misc.h"

static void MN_LineChartNodeDraw (menuNode_t *node)
{
	lineStrip_t *lineStrip;
	const int dataId = node->u.linechart.dataId;
	vec3_t pos;

	if (dataId == 0)
		return;

	if (mn.sharedData[dataId].type != MN_SHARED_LINESTRIP) {
		Com_Printf("MN_LineStripNodeDraw: Node '%s' have use an invalide dataId type. LineStrip expected. dataId invalidated\n", MN_GetPath(node));
		node->u.linechart.dataId = 0;
		return;
	}

	MN_GetNodeAbsPos(node, pos);
	pos[2] = 0;

	R_PushMatrix();
	R_Transform(pos, NULL, NULL);

	/* Draw axes */
	if (node->u.linechart.displayAxes) {
		int axes[6];
		axes[0] = 0;
		axes[1] = 0;
		axes[2] = 0;
		axes[3] = (int) node->size[1];
		axes[4] = (int) node->size[0];
		axes[5] = (int) node->size[1];
		R_ColorBlend(node->u.linechart.axesColor);
		R_DrawLineStrip(3, axes);
	}

	/* Draw all linestrips. */
	lineStrip = mn.sharedData[dataId].data.lineStrip;
	for (; lineStrip; lineStrip = lineStrip->next) {
		/* Draw this line if it's valid. */
		if (lineStrip->pointList && lineStrip->numPoints > 0) {
			R_ColorBlend(lineStrip->color);
			R_DrawLineStrip(lineStrip->numPoints, lineStrip->pointList);
		}
	}
	R_ColorBlend(NULL);

	R_PopMatrix();
}

static const value_t properties[] = {
	{"dataid", V_SPECIAL_DATAID, offsetof(menuNode_t, u.linechart.dataId), MEMBER_SIZEOF(menuNode_t, u.linechart.dataId)},
	{"displayaxes", V_BOOL, offsetof(menuNode_t, u.linechart.displayAxes), MEMBER_SIZEOF(menuNode_t, u.linechart.displayAxes)},
	{"axescolor", V_COLOR, offsetof(menuNode_t, u.linechart.axesColor), MEMBER_SIZEOF(menuNode_t, u.linechart.axesColor)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterLineChartNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "linechart";
	behaviour->draw = MN_LineChartNodeDraw;
	behaviour->properties = properties;
}
