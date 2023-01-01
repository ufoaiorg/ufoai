/**
 * @file
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

#include "../../../common/scripts_lua.h"

#define EXTRADATA_TYPE lineChartExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

/**
 * @brief Structure describes a line
 */
typedef struct line_s {
	char id[MAX_VAR];			/**< identifier of the line */
	bool visible;				/**< whether the line should be visible or not */
	vec4_t lineColor;			/**< color of the line */
	bool dots;					/**< whether to draw dots for data points or not */
	int* pointList;				/**< list of coordinates */
	int maxPoints;				/**< max number of coordinates */
	int numPoints;				/**< number of actual coordinates */
} line_t;

/**
 * @brief Drawing routine of the lineChart node
 * @param[in] node UI node to draw
 */
void uiLineChartNode::draw (uiNode_t* node)
{
	vec3_t pos;
	UI_GetNodeAbsPos(node, pos);
	pos[2] = 0;

	UI_Transform(pos, nullptr, nullptr);

	/* Draw axes */
	if (EXTRADATA(node).displayAxes) {
		int axes[6];
		axes[0] = 0;
		axes[1] = 0;
		axes[2] = 0;
		axes[3] = (int) node->box.size[1];
		axes[4] = (int) node->box.size[0];
		axes[5] = (int) node->box.size[1];
		R_Color(EXTRADATA(node).axesColor);
		R_DrawLineStrip(3, axes);
	}

	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (!line->visible)
			continue;
		R_Color(line->lineColor);
		R_DrawLineStrip(line->numPoints, line->pointList);

		if (!line->dots)
			continue;

		for (int i = 0; i < line->numPoints; i++) {
			R_DrawFill(line->pointList[i * 2] - 2, line->pointList[i * 2 + 1] - 2, 5, 5, line->lineColor);
		}
	}
	R_Color(nullptr);

	UI_Transform(nullptr, nullptr, nullptr);
}

/**
 * @brief Cleanup tasks on removing a lineChart
 * @param node The node to delete
 */
void uiLineChartNode::deleteNode (uiNode_t* node)
{
	UI_ClearLineChart(node);
	LIST_Delete(&(EXTRADATA(node).lines));
}

/**
 * @brief Clears all drawings froma lineChart
 * @param node The node to clear
 */
bool UI_ClearLineChart (uiNode_t* node)
{
	assert(node);

	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (line->numPoints > 0) {
			line->numPoints = 0;
			Mem_Free(line->pointList);
		}
	}
	LIST_Delete(&(EXTRADATA(node).lines));

	return false;
}

/**
 * @brief Add a line to the chart
 * @param[in, out] node Chart node to extend
 * @param[in] id Name of the chart line to add
 * @param[in] visible Whether the chart line be rendered
 * @param[in] color RGBA Color of the chart line
 * @param[in] dots Whether to draw little squares to the datapoints
 * @param[in] numPoints Number of datapoints to allocate
 */
bool UI_AddLineChartLine (uiNode_t* node, const char* id, bool visible, const vec4_t color, bool dots, int numPoints)
{
	assert(node);

	if (Q_strnull(id)) {
		Com_Printf("UI_AddLineChartLine: Missing line identifier for extending lineChart '%s'\n", UI_GetPath(node));
		return false;
	}

	line_t newLine;
	Q_strncpyz(newLine.id, id, MAX_VAR);
	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (Q_streq(line->id, newLine.id)) {
			Com_Printf("UI_AddLineChartLine: A line with id '%s' in lineChart '%s' already exists\n", newLine.id, UI_GetPath(node));
			return false;
		}
	}

	newLine.visible = visible;
	Vector4Copy(color, newLine.lineColor);
	newLine.dots = dots;
	newLine.maxPoints = 0;
	newLine.numPoints = 0;
	newLine.pointList = nullptr;

	if (numPoints >= 0) {
		newLine.pointList = Mem_PoolAllocTypeN(int, numPoints * 2, ui_dynPool);
		if (newLine.pointList != nullptr) {
			newLine.maxPoints = numPoints;
		}
	}

	return (nullptr != LIST_Add(&(EXTRADATA(node).lines), &newLine, sizeof(newLine)));
}

/**
 * @brief Add a data point to a line chart
 * @param[in, out] node Chart node to extend
 * @param[in] id Line ID to extend
 * @param[in] x Horizontal coordinate of the data point
 * @param[in] y Vertical coordinate of the data point
 * @note You cannot add more data points to a line than what was originally set
 */
bool UI_AddLineChartCoord (uiNode_t* node, const char* id, int x, int y)
{
	assert(node);

	if (Q_strnull(id)) {
		Com_Printf("UI_AddLineChartCoord: Missing line identifier for extending lineChart '%s'\n", UI_GetPath(node));
		return false;
	}

	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (!Q_strneq(line->id, id, MAX_VAR))
			continue;

		if (line->numPoints < line->maxPoints) {
			line->pointList[line->numPoints * 2] = x;
			line->pointList[line->numPoints * 2 + 1] = y;
			line->numPoints++;
			return true;
		} else {
			Com_Printf("UI_AddLineChartCoord: Line '%s' has already reached it's max capacity %d in lineChart '%s'\n", id, line->maxPoints, UI_GetPath(node));
		}
	}

	Com_Printf("UI_AddLineChartCoord: Line '%s' was not found in lineChart '%s'\n", id, UI_GetPath(node));
	return false;
}

/**
 * @brief Shows/hides a chart line
 * @param[in, out] node Chart node in which the line is
 * @param[in] id Line ID to show/hide
 * @param[in] visible Boolean switch of line's visibility (including dots)
 */
bool UI_ShowChartLine (uiNode_t* node, const char* id, bool visible)
{
	assert(node);

	if (Q_strnull(id)) {
		Com_Printf("UI_ShowChartLine: Missing line identifier for configuring lineChart '%s'\n", UI_GetPath(node));
		return false;
	}

	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (!Q_strneq(line->id, id, MAX_VAR))
			continue;

		line->visible = visible;
		return true;
	}

	Com_Printf("UI_ShowChartLine: Line '%s' was not found in lineChart '%s'\n", id, UI_GetPath(node));
	return false;
}

/**
 * @brief Shows/hides small dots at data points of a chart
 * @param[in, out] node Chart node in which the line is
 * @param[in] id Line ID to alter
 * @param[in] visible Boolean switch of the dot's visibility
 */
bool UI_ShowChartDots (uiNode_t* node, const char* id, bool visible)
{
	assert(node);

	if (Q_strnull(id)) {
		Com_Printf("UI_ShowChartDots: Missing line identifier for configuring lineChart '%s'\n", UI_GetPath(node));
		return false;
	}

	LIST_Foreach(EXTRADATA(node).lines, line_t, line) {
		if (!Q_strneq(line->id, id, MAX_VAR))
			continue;

		line->dots = visible;
		return true;
	}

	Com_Printf("UI_ShowChartDots: Line '%s' was not found in lineChart '%s'\n", id, UI_GetPath(node));
	return false;
}

/**
 * @brief Registers lineChart node
 * @param[out] behaviour UI node behaviour structure
 */
void UI_RegisterLineChartNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "linechart";
	behaviour->manager = UINodePtr(new uiLineChartNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiLineChartNode_t *");

	/* If true, it display axes of the chart. */
	UI_RegisterExtradataNodeProperty(behaviour, "displayaxes", V_BOOL, lineChartExtraData_t, displayAxes);
	/* Axe color. */
	UI_RegisterExtradataNodeProperty(behaviour, "axescolor", V_COLOR, lineChartExtraData_t, axesColor);
}
