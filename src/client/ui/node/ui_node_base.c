/**
 * @file ui_node_base.c
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
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../cgame/campaign/cp_campaign.h"
#include "../../renderer/r_draw.h"

#define EXTRADATA_TYPE baseExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, baseExtraData_t)

static const vec4_t white = {1.0f, 1.0f, 1.0f, 0.8f};

/**
 * @brief Called after the end of the node load from script (all data and/or child are set)
 */
static void UI_AbstractBaseNodeLoaded (uiNode_t * node)
{
	const int id = EXTRADATA(node).baseid;
	if (B_GetBaseByIDX(id) == NULL)
		Com_Printf("UI_AbstractBaseNodeLoaded: Invalid baseid given %i", id);
}

/**
 * @brief Draw a small square with the layout of the given base
 */
static void UI_BaseLayoutNodeDraw (uiNode_t * node)
{
	base_t *base;
	int height, width, y;
	int row, col;
	const vec4_t c_gray = {0.5, 0.5, 0.5, 1.0};
	vec2_t nodepos;
	int totalMarge;

	if (EXTRADATA(node).baseid >= MAX_BASES || EXTRADATA(node).baseid < 0)
		return;

	totalMarge = node->padding * (BASE_SIZE + 1);
	width = (node->size[0] - totalMarge) / BASE_SIZE;
	height = (node->size[1] - totalMarge) / BASE_SIZE;

	UI_GetNodeAbsPos(node, nodepos);

	base = B_GetBaseByIDX(EXTRADATA(node).baseid);

	y = nodepos[1] + node->padding;
	for (row = 0; row < BASE_SIZE; row++) {
		int x = nodepos[0] + node->padding;
		for (col = 0; col < BASE_SIZE; col++) {
			if (B_IsTileBlocked(base, col, row)) {
				UI_DrawFill(x, y, width, height, c_gray);
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
static void UI_BaseMapGetCellAtPos (const uiNode_t *node, int x, int y, int *col, int *row)
{
	assert(col);
	assert(row);
	UI_NodeAbsoluteToRelativePos(node, &x, &y);
	if (x < 0 || y < 0 || x >= node->size[0] || y >= node->size[1]) {
		*col = -1;
		*row = -1;
		return;
	}
	*col = x / (node->size[0] / BASE_SIZE);
	*row = y / (node->size[1] / BASE_SIZE);
	assert(*col >= 0 && *col < BASE_SIZE);
	assert(*row >= 0 && *row < BASE_SIZE);
}

/**
 * @brief Draws a base.
 */
static void UI_BaseMapNodeDraw (uiNode_t * node)
{
	int width, height, row, col;
	const building_t *building;
	const base_t *base = B_GetCurrentSelectedBase();
	qboolean used[MAX_BUILDINGS];

	if (!base) {
		UI_PopWindow(qfalse);
		return;
	}

	/* reset the used flag */
	OBJZERO(used);

	width = node->size[0] / BASE_SIZE;
	height = node->size[1] / BASE_SIZE + BASE_IMAGE_OVERLAY;

	for (row = 0; row < BASE_SIZE; row++) {
		const char *image = NULL;
		for (col = 0; col < BASE_SIZE; col++) {
			vec2_t pos;
			UI_GetNodeAbsPos(node, pos);
			pos[0] += col * width;
			pos[1] += row * (height - BASE_IMAGE_OVERLAY);

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
				UI_DrawNormImageByName(qfalse, pos[0], pos[1], width * (building ? building->size[0] : 1), height * (building ? building->size[1] : 1), 0, 0, 0, 0, image);
			if (building) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
					{
						const float time = max(0.0, B_GetConstructionTimeRemain(building));
						UI_DrawString("f_small", ALIGN_UL, pos[0] + 10, pos[1] + 10, pos[0] + 10, node->size[0], 0, va(ngettext("%3.1f day left", "%3.1f days left", time), time), 0, 0, NULL, qfalse, 0);
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

	UI_BaseMapGetCellAtPos(node, mousePosX, mousePosY, &col, &row);
	if (col == -1)
		return;

	/* if we are building */
	if (ccs.baseAction == BA_NEWBUILDING) {
		int y, x;
		int xCoord, yCoord, widthRect, heigthRect;
		vec2_t pos;

		assert(base->buildingCurrent);

		for (y = row; y < row + base->buildingCurrent->size[1]; y++) {
			for (x = col; x < col + base->buildingCurrent->size[0]; x++) {
				if (!B_MapIsCellFree(base, x, y))
					return;
			}
		}

		UI_GetNodeAbsPos(node, pos);
		xCoord = pos[0] + col * width;
		yCoord = pos[1] + row * (height - BASE_IMAGE_OVERLAY);
		widthRect = base->buildingCurrent->size[0] * width;
		heigthRect = base->buildingCurrent->size[1] * (height - BASE_IMAGE_OVERLAY);
		R_DrawRect(xCoord, yCoord, widthRect, heigthRect, white, 3, 1);
	}
}

/**
 * @brief Custom tooltip
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void UI_BaseMapNodeDrawTooltip (uiNode_t *node, int x, int y)
{
	int col, row;
	building_t *building;
	const int itemToolTipWidth = 250;
	char *tooltipText;
	base_t *base = B_GetCurrentSelectedBase();

	UI_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	building = base->map[row][col].building;
	if (!building)
		return;

	tooltipText = _(building->name);
	if (!B_CheckBuildingDependencesStatus(building))
		tooltipText = va("%s\n%s %s", tooltipText, _("not operational, depends on"), _(building->dependsBuilding->name));
	UI_DrawTooltip(tooltipText, x, y, itemToolTipWidth);
}

/**
 * @brief Left click on the basemap
 * @sa UI_BaseMapRightClick
 * @param[in] node Node definition for the base map
 * @param[in] x Absolute X mouse position into the screen
 * @param[in] y Absolute Y mouse position into the screen
 */
static void UI_BaseMapNodeClick (uiNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	UI_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	if (ccs.baseAction == BA_NEWBUILDING) {
		building_t *building = base->buildingCurrent;
		int y, x;

		assert(building);

		if (col + building->size[0] > BASE_SIZE)
			return;
		if (row + building->size[1] > BASE_SIZE)
			return;
		for (y = row; y < row + building->size[1]; y++)
			for (x = col; x < col + building->size[0]; x++)
				if (B_GetBuildingAt(base, x, y) != NULL || B_IsTileBlocked(base, x, y))
					return;
		B_SetBuildingByClick(base, building, row, col);
		S_StartLocalSample("geoscape/build-place", 1.0f);
		return;
	}

	if (B_GetBuildingAt(base, col, row) != NULL) {
		const building_t *entry = B_GetBuildingAt(base, col, row);

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
static void UI_BaseMapNodeRightClick (uiNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	UI_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;
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
static void UI_BaseMapNodeMiddleClick (uiNode_t *node, int x, int y)
{
	int row, col;
	const base_t *base = B_GetCurrentSelectedBase();
	const building_t *entry;
	if (base == NULL)
		return;

	assert(node);
	assert(node->root);

	UI_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	entry = base->map[row][col].building;
	if (entry) {
		assert(!B_IsTileBlocked(base, col, row));
		B_DrawBuilding(entry);
	}
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_BaseLayoutNodeLoading (uiNode_t *node)
{
	node->padding = 3;
	Vector4Set(node->color, 1, 1, 1, 1);
}

void UI_RegisterAbstractBaseNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractbase";
	behaviour->isAbstract = qtrue;
	behaviour->loaded = UI_AbstractBaseNodeLoaded;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Identify the base, from a base ID, the node use. */
	UI_RegisterExtradataNodeProperty(behaviour, "baseid", V_INT, baseExtraData_t, baseid);
}

void UI_RegisterBaseMapNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "basemap";
	behaviour->extends = "abstractbase";
	behaviour->draw = UI_BaseMapNodeDraw;
	behaviour->leftClick = UI_BaseMapNodeClick;
	behaviour->rightClick = UI_BaseMapNodeRightClick;
	behaviour->drawTooltip = UI_BaseMapNodeDrawTooltip;
	behaviour->middleClick = UI_BaseMapNodeMiddleClick;
}

void UI_RegisterBaseLayoutNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "baselayout";
	behaviour->extends = "abstractbase";
	behaviour->draw = UI_BaseLayoutNodeDraw;
	behaviour->loading = UI_BaseLayoutNodeLoading;
}
