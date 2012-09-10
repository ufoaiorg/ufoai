/**
 * @file
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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_render.h"
#include "ui_node_base.h"

#include "../../cl_shared.h"
#include "../../input/cl_input.h"
#include "../../sound/s_main.h"
#include "../../cgame/campaign/cp_campaign.h"

#define EXTRADATA_TYPE baseExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const vec4_t white = {1.0f, 1.0f, 1.0f, 0.8f};

void uiAbstractBaseNode::onLoading (uiNode_t * node)
{
	EXTRADATA(node).baseid = -1;
}

/**
 * @brief Called after the node is completely loaded from the ufo-script (all data and/or children are set)
 */
void uiAbstractBaseNode::onLoaded (uiNode_t * node)
{
}

base_t* uiAbstractBaseNode::getBase (const uiNode_t * node)
{
	if (EXTRADATACONST(node).baseid == -1) {
		return B_GetCurrentSelectedBase();
	}

	return B_GetBaseByIDX(EXTRADATACONST(node).baseid);
}

/**
 * @brief Draw a small square with the layout of the given base
 */
void uiBaseLayoutNode::draw (uiNode_t * node)
{
	if (EXTRADATA(node).baseid >= MAX_BASES || EXTRADATA(node).baseid < 0)
		return;

	const int totalMarge = node->padding * (BASE_SIZE + 1);
	const int width = (node->box.size[0] - totalMarge) / BASE_SIZE;
	const int height = (node->box.size[1] - totalMarge) / BASE_SIZE;

	vec2_t nodepos;
	UI_GetNodeAbsPos(node, nodepos);

	const base_t *base = getBase(node);
	int y = nodepos[1] + node->padding;
	for (int row = 0; row < BASE_SIZE; row++) {
		int x = nodepos[0] + node->padding;
		for (int col = 0; col < BASE_SIZE; col++) {
			if (B_IsTileBlocked(base, col, row)) {
				UI_DrawFill(x, y, width, height, node->bgcolor);
			} else if (B_GetBuildingAt(base, col, row) != NULL) {
				/* maybe destroyed in the meantime */
				if (base->founded)
					UI_DrawFill(x, y, width, height, node->color);
			}
			x += width + node->padding;
		}
		y += height + node->padding;
	}
}

/** 20 is the height of the part where the images overlap */
#define BASE_IMAGE_OVERLAY 20

/**
 * @brief Return col and row of a cell, at an absolute position
 * @param[in] node Node definition for the base map
 * @param[in] x Absolute x-position requested
 * @param[in] y Absolute y-position requested
 * @param[out] col Col of the cell at the position (-1 if no cell)
 * @param[out] row Row of the cell at the position (-1 if no cell)
 */
void uiBaseMapNode::getCellAtPos (const uiNode_t *node, int x, int y, int *col, int *row)
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
void uiBaseMapNode::draw (uiNode_t * node)
{
	const base_t *base = getBase(node);
	if (!base) {
		UI_PopWindow(false);
		return;
	}

	bool used[MAX_BUILDINGS];
	/* reset the used flag */
	OBJZERO(used);

	const int width = node->box.size[0] / BASE_SIZE;
	const int height = node->box.size[1] / BASE_SIZE + BASE_IMAGE_OVERLAY;

	vec2_t nodePos;
	UI_GetNodeAbsPos(node, nodePos);

	int row, col;
	for (row = 0; row < BASE_SIZE; row++) {
		const char *image = NULL;
		for (col = 0; col < BASE_SIZE; col++) {
			const vec2_t pos = { nodePos[0] + col * width, nodePos[1] + row * (height - BASE_IMAGE_OVERLAY) };
			const building_t *building;
			/* base tile */
			if (B_IsTileBlocked(base, col, row)) {
				building = NULL;
				image = "base/invalid";
			} else if (B_GetBuildingAt(base, col, row) == NULL) {
				building = NULL;
				image = "base/grid";
			} else {
				building = B_GetBuildingAt(base, col, row);
				assert(building);

				if (building->image)
					image = building->image;

				/* some buildings are drawn with two tiles - e.g. the hangar is no square map tile.
				 * These buildings have the needs parameter set to the second building part which has
				 * its own image set, too. We are searching for this second building part here. */
				if (B_BuildingGetUsed(used, building->idx))
					continue;
				B_BuildingSetUsed(used, building->idx);
			}

			/* draw tile */
			if (image != NULL)
				UI_DrawNormImageByName(false, pos[0], pos[1], width * (building ? building->size[0] : 1), height * (building ? building->size[1] : 1), 0, 0, 0, 0, image);
			if (building) {
				switch (building->buildingStatus) {
				case B_STATUS_UNDER_CONSTRUCTION: {
					const float remaining = B_GetConstructionTimeRemain(building);
					const float time = std::max(0.0f, remaining);
					const char* text = va(ngettext("%3.1f day left", "%3.1f days left", time), time);
					UI_DrawString("f_small", ALIGN_UL, pos[0] + 10, pos[1] + 10, pos[0] + 10, node->box.size[0], 0, text);
					break;
				}
				default:
					break;
				}
			}
		}
	}

	if (!node->state)
		return;

	getCellAtPos(node, mousePosX, mousePosY, &col, &row);
	if (col == -1)
		return;

	/* if we are building */
	if (ccs.baseAction == BA_NEWBUILDING) {
		int y, x;
		const building_t *building = base->buildingCurrent;
		const vec2_t& size = building->size;
		assert(building);

		for (y = row; y < row + size[1]; y++) {
			for (x = col; x < col + size[0]; x++) {
				if (!B_MapIsCellFree(base, x, y))
					return;
			}
		}

		vec2_t pos;
		UI_GetNodeAbsPos(node, pos);
		const int xCoord = pos[0] + col * width;
		const int yCoord = pos[1] + row * (height - BASE_IMAGE_OVERLAY);
		const int widthRect = size[0] * width;
		const int heigthRect = size[1] * (height - BASE_IMAGE_OVERLAY);
		UI_DrawRect(xCoord, yCoord, widthRect, heigthRect, white, 3, 1);
	}
}

/**
 * @brief Custom tooltip
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
void uiBaseMapNode::drawTooltip (uiNode_t *node, int x, int y)
{
	int col, row;

	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	const base_t *base = getBase(node);
	building_t *building = base->map[row][col].building;
	if (!building)
		return;

	char const* tooltipText = _(building->name);
	if (!B_CheckBuildingDependencesStatus(building))
		tooltipText = va(_("%s\nnot operational, depends on %s"), _(building->name), _(building->dependsBuilding->name));
	const int itemToolTipWidth = 250;
	UI_DrawTooltip(tooltipText, x, y, itemToolTipWidth);
}

/**
 * @brief Left click on the basemap
 * @sa UI_BaseMapRightClick
 * @param[in] node Node definition for the base map
 * @param[in] x Absolute X mouse position into the screen
 * @param[in] y Absolute Y mouse position into the screen
 */
void uiBaseMapNode::onLeftClick (uiNode_t *node, int x, int y)
{
	assert(node);
	assert(node->root);

	int row, col;
	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	base_t *base = getBase(node);
	assert(base);

	if (ccs.baseAction == BA_NEWBUILDING) {
		const building_t *building = base->buildingCurrent;

		assert(building);

		if (col + building->size[0] > BASE_SIZE)
			return;
		if (row + building->size[1] > BASE_SIZE)
			return;
		for (int y = row; y < row + building->size[1]; y++)
			for (int x = col; x < col + building->size[0]; x++)
				if (B_GetBuildingAt(base, x, y) != NULL || B_IsTileBlocked(base, x, y))
					return;
		B_SetBuildingByClick(base, building, row, col);
		S_StartLocalSample("geoscape/build-place", 1.0f);
		return;
	}

	const building_t *entry = B_GetBuildingAt(base, col, row);
	if (entry != NULL) {
		if (B_IsTileBlocked(base, col, row))
			Com_Error(ERR_DROP, "tile with building is not blocked");

		B_BuildingOpenAfterClick(entry);
		ccs.baseAction = BA_NONE;
		return;
	}
}

/**
 * @brief Right click on the basemap
 * @sa UI_BaseMapNodeClick
 * @param[in] node Context node
 * @param[in] x Absolute x mouse coordinate (screen coordinates)
 * @param[in] y Absolute y mouse coordinate (screen coordinates)
 */
void uiBaseMapNode::onRightClick (uiNode_t *node, int x, int y)
{
	int row, col;
	assert(node);
	assert(node->root);

	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	const base_t *base = getBase(node);
	assert(base);
	if (!base->map[row][col].building)
		return;

	Cmd_ExecuteString(va("building_destroy %i %i", base->idx, base->map[row][col].building->idx));
}

/**
 * @brief Middle click on the basemap
 * @sa UI_BaseMapNodeClick
 * @param[in] node Node definition for the base map
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 * @note relies on @c baseCurrent
 */
void uiBaseMapNode::onMiddleClick (uiNode_t *node, int x, int y)
{
	assert(node);
	assert(node->root);

	int row, col;
	getCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	const base_t *base = getBase(node);
	if (base == NULL)
		return;

	const building_t *entry = base->map[row][col].building;
	if (entry) {
		assert(!B_IsTileBlocked(base, col, row));
		B_DrawBuilding(entry);
	}
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiBaseLayoutNode::onLoading (uiNode_t *node)
{
	uiAbstractBaseNode::onLoading(node);
	node->padding = 3;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->bgcolor, 0.5, 0.5, 0.5, 1);
}

void UI_RegisterAbstractBaseNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractbase";
	behaviour->isAbstract = true;
	behaviour->manager = new uiAbstractBaseNode();
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Identify the base, from a base ID, the node use. */
	UI_RegisterExtradataNodeProperty(behaviour, "baseid", V_INT, baseExtraData_t, baseid);
}

void UI_RegisterBaseMapNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "basemap";
	behaviour->extends = "abstractbase";
	behaviour->manager = new uiBaseMapNode();
}

void UI_RegisterBaseLayoutNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "baselayout";
	behaviour->extends = "abstractbase";
	behaviour->manager = new uiBaseLayoutNode();
}
