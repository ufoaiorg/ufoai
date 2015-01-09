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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_render.h"
#include "ui_node_base.h"

#include "../../cl_shared.h"
#include "../../cgame/cl_game.h"
#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"
#include "../../sound/s_main.h"

#define EXTRADATA_TYPE baseExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

// TODO: remove me - duplicated in cp_base.h
#define BASE_SIZE		5

/** 20 is the height of the part where the images overlap */
#define BASE_IMAGE_OVERLAY 20

void uiAbstractBaseNode::onLoading (uiNode_t* node)
{
	EXTRADATA(node).baseid = -1;
}

/**
 * @brief Called after the node is completely loaded from the ufo-script (all data and/or children are set)
 */
void uiAbstractBaseNode::onLoaded (uiNode_t* node)
{
}

/**
 * @brief Draw a small square with the layout of the given base
 */
void uiBaseLayoutNode::draw (uiNode_t* node)
{
	const int totalMarge = node->padding * (BASE_SIZE + 1);
	const int width = (node->box.size[0] - totalMarge) / BASE_SIZE;
	const int height = (node->box.size[1] - totalMarge) / BASE_SIZE;

	vec2_t nodepos;
	UI_GetNodeAbsPos(node, nodepos);

	GAME_DrawBaseLayout(EXTRADATA(node).baseid, nodepos[0], nodepos[1], totalMarge, width, height, node->padding, node->bgcolor, node->color);
}

/**
 * @brief Return col and row of a cell, at an absolute position
 * @param[in] node Node definition for the base map
 * @param[in] x,y Absolute x- and y-position requested
 * @param[out] col Col of the cell at the position (-1 if no cell)
 * @param[out] row Row of the cell at the position (-1 if no cell)
 */
void uiBaseMapNode::getCellAtPos (const uiNode_t* node, int x, int y, int* col, int* row) const
{
	assert(col);
	assert(row);
	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	if (x < 0 || y < 0 || x >= node->box.size[0] || y >= node->box.size[1]) {
		*col = -1;
		*row = -1;
		return;
	}
	*col = x / (node->box.size[0] / BASE_SIZE);
	*row = y / (node->box.size[1] / BASE_SIZE);
	assert(*col >= 0 && *col < BASE_SIZE);
	assert(*row >= 0 && *row < BASE_SIZE);
}

/**
 * @brief Draws a base.
 */
void uiBaseMapNode::draw (uiNode_t* node)
{
	int col, row;
	bool hover = node->state;
	getCellAtPos(node, mousePosX, mousePosY, &col, &row);
	if (col == -1)
		hover = false;

	const int width = node->box.size[0] / BASE_SIZE;
	const int height = node->box.size[1] / BASE_SIZE + BASE_IMAGE_OVERLAY;

	vec2_t nodePos;
	UI_GetNodeAbsPos(node, nodePos);

	GAME_DrawBase(EXTRADATA(node).baseid, nodePos[0], nodePos[1], width, height, col, row, hover, BASE_IMAGE_OVERLAY);
}

/**
 * @brief Custom tooltip
 * @param[in] node Node we request to draw tooltip
 * @param[in] x,y Position of the mouse
 */
void uiBaseMapNode::drawTooltip (const uiNode_t* node, int x, int y) const
{
	int col, row;

	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	GAME_DrawBaseTooltip(EXTRADATACONST(node).baseid, x, y, col, row);
}

/**
 * @brief Left click on the basemap
 * @sa UI_BaseMapRightClick
 * @param[in] node Node definition for the base map
 * @param[in] x,y Absolute mouse position into the screen
 */
void uiBaseMapNode::onLeftClick (uiNode_t* node, int x, int y)
{
	assert(node);
	assert(node->root);

	int row, col;
	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	GAME_HandleBaseClick(EXTRADATACONST(node).baseid, K_MOUSE1, col, row);
}

/**
 * @brief Right click on the basemap
 * @sa UI_BaseMapNodeClick
 * @param[in] node Context node
 * @param[in] x,y Absolute mouse coordinate (screen coordinates)
 */
void uiBaseMapNode::onRightClick (uiNode_t* node, int x, int y)
{
	int row, col;
	assert(node);
	assert(node->root);

	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	GAME_HandleBaseClick(EXTRADATACONST(node).baseid, K_MOUSE2, col, row);
}

/**
 * @brief Middle click on the basemap
 * @sa UI_BaseMapNodeClick
 * @param[in] node Node definition for the base map
 * @param[in] x,y The screen coordinates
 * @note relies on @c baseCurrent
 */
void uiBaseMapNode::onMiddleClick (uiNode_t* node, int x, int y)
{
	assert(node);
	assert(node->root);

	int row, col;
	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	GAME_HandleBaseClick(EXTRADATACONST(node).baseid, K_MOUSE3, col, row);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiBaseLayoutNode::onLoading (uiNode_t* node)
{
	uiAbstractBaseNode::onLoading(node);
	node->padding = 3;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->bgcolor, 0.5, 0.5, 0.5, 1);
}

void UI_RegisterAbstractBaseNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "abstractbase";
	behaviour->isAbstract = true;
	behaviour->manager = UINodePtr(new uiAbstractBaseNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Identify the base, from a base ID, the node use. */
	UI_RegisterExtradataNodeProperty(behaviour, "baseid", V_INT, baseExtraData_t, baseid);
}

void UI_RegisterBaseMapNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "basemap";
	behaviour->extends = "abstractbase";
	behaviour->manager = UINodePtr(new uiBaseMapNode());
}

void UI_RegisterBaseLayoutNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "baselayout";
	behaviour->extends = "abstractbase";
	behaviour->manager = UINodePtr(new uiBaseLayoutNode());
}
