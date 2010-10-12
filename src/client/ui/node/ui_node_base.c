/**
 * @file ui_node_base.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_render.h"
#include "ui_node_base.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../campaign/cp_campaign.h"

#define EXTRADATA_TYPE baseExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, baseExtraData_t)

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
			if (base->map[row][col].blocked) {
				UI_DrawFill(x, y, width, height, c_gray);
			} else if (base->map[row][col].building) {
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
	*row = y / (node->size[0] / BASE_SIZE);
	assert(*col >= 0 && *col < BASE_SIZE);
	assert(*row >= 0 && *row < BASE_SIZE);
}

/**
 * @brief Check a base cell
 * @return True if the cell is free to build
 */
static inline qboolean UI_BaseMapIsCellFree (const base_t *base, int col, int row)
{
	return col >= 0 && col < BASE_SIZE
	 && row >= 0 && row < BASE_SIZE
	 && base->map[row][col].building == NULL
	 && !base->map[row][col].blocked;
}

/**
 * @brief Draws a base.
 */
static void UI_BaseMapNodeDraw (uiNode_t * node)
{
	int width, height, row, col;
	const building_t *building;
	const building_t *secondBuilding = NULL;
	const base_t *base = B_GetCurrentSelectedBase();
	qboolean used[MAX_BUILDINGS];

	if (!base) {
		UI_PopWindow(qfalse);
		return;
	}

	/* reset the used flag */
	memset(used, 0, sizeof(used));

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
			if (base->map[row][col].blocked) {
				building = NULL;
				image = "base/invalid";
			} else if (!base->map[row][col].building) {
				building = NULL;
				image = "base/grid";
			} else {
				building = base->map[row][col].building;
				secondBuilding = NULL;
				assert(building);

				/* some buildings are drawn with two tiles - e.g. the hangar is no square map tile.
				 * These buildings have the needs parameter set to the second building part which has
				 * its own image set, too. We are searching for this second building part here. */
				if (!B_BuildingGetUsed(used, building->idx)) {
					if (building->needs)
						B_BuildingSetUsed(used, building->idx);
					if (building->image)
						image = building->image;
				} else {
					secondBuilding = B_GetBuildingTemplate(building->needs);
					if (!secondBuilding)
						Com_Error(ERR_DROP, "Error in ufo-scriptfile - could not find the needed building");
					image = secondBuilding->image;
				}
			}

			/* draw tile */
			if (image != NULL)
				UI_DrawNormImageByName(pos[0], pos[1], width, height, 0, 0, 0, 0, image);

			/* only draw for first part of building */
			if (building && !secondBuilding) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
				{
					const int time = building->buildTime - (ccs.date.day - building->timeStart);
					UI_DrawString("f_small", ALIGN_UL, pos[0] + 10, pos[1] + 10, pos[0] + 10, node->size[0], 0, va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse, 0);
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
		qboolean isLarge;
		assert(base->buildingCurrent);
		/** @todo we should not compute here if we can (or not build something) the map model know it better */
		if (!UI_BaseMapIsCellFree(base, col, row))
			return;

		isLarge = base->buildingCurrent->needs != NULL;

		/* large building */
		if (isLarge) {
			if (UI_BaseMapIsCellFree(base, col + 1, row)) {
				/* ok */
			} else if (UI_BaseMapIsCellFree(base, col - 1, row)) {
				/* fix col at the left cell */
				col--;
			} else {
				col = -1;
			}
		}
		if (col != -1) {
			vec2_t pos;
			UI_GetNodeAbsPos(node, pos);
			if (isLarge) {
				UI_DrawNormImageByName(pos[0] + col * width, pos[1] + row * (height - BASE_IMAGE_OVERLAY), width + width, height, 0, 0, 0, 0, "base/hover2");
			} else
				UI_DrawNormImageByName(pos[0] + col * width, pos[1] + row * (height - BASE_IMAGE_OVERLAY), width, height, 0, 0, 0, 0, "base/hover");
		}
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
	if (!B_CheckBuildingDependencesStatus(base, building))
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
		assert(base->buildingCurrent);
		if (!base->map[row][col].building && !base->map[row][col].blocked) {
			if (!base->buildingCurrent->needs
			 || (col < BASE_SIZE - 1 && !base->map[row][col + 1].building && !base->map[row][col + 1].blocked)
			 || (col > 0 && !base->map[row][col - 1].building && !base->map[row][col - 1].blocked))
			/* Set position for a new building */
			B_SetBuildingByClick(base, base->buildingCurrent, row, col);
		}
		return;
	}

	if (base->map[row][col].building) {
		const building_t *entry = base->map[row][col].building;
		if (!entry)
			Com_Error(ERR_DROP, "UI_BaseMapNodeClick: no entry at %i:%i", x, y);

		assert(!base->map[row][col].blocked);

		B_BuildingOpenAfterClick(base, entry);
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

	if (base->map[row][col].building) {
		building_t *entry = base->map[row][col].building;
		if (!entry)
			Sys_Error("UI_BaseMapNodeRightClick: no entry at %i:%i\n", x, y);

		assert(!base->map[row][col].blocked);
		B_MarkBuildingDestroy(base, entry);
		return;
	}
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
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	UI_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	if (base->map[row][col].building) {
		building_t *entry = base->map[row][col].building;
		if (!entry)
			Com_Error(ERR_DROP, "UI_BaseMapNodeMiddleClick: no entry at %i:%i", x, y);

		assert(!base->map[row][col].blocked);
		B_DrawBuilding(base, entry);
		return;
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

static const value_t properties[] = {
	/* Identify the base, from a base ID, the node use. */
	{"baseid", V_INT, UI_EXTRADATA_OFFSETOF(baseExtraData_t, baseid), MEMBER_SIZEOF(baseExtraData_t, baseid)},
	{NULL, V_NULL, 0, 0}
};

void UI_RegisterAbstractBaseNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "abstractbase";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->loaded = UI_AbstractBaseNodeLoaded;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
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
