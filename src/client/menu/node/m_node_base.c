/**
 * @file m_node_base.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_tooltip.h"
#include "../m_nodes.h"
#include "../m_render.h"
#include "m_node_model.h"
#include "m_node_base.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../campaign/cp_campaign.h"

#define EXTRADATA(node) (node->u.base)

/**
 * @brief Called after the end of the node load from script (all data and/or child are set)
 */
static void MN_AbstractBaseNodeLoaded (menuNode_t * node)
{
	const int id = EXTRADATA(node).baseid;
	if (id < 0 || id >= MAX_BASES) {
		Com_Printf("MN_AbstractBaseNodeLoaded: Invalid baseid given %i", id);
	}
}

/**
 * @brief Draw a small square with the menu layout of the given base
 */
static void MN_BaseLayoutNodeDraw (menuNode_t * node)
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

	MN_GetNodeAbsPos(node, nodepos);

	base = B_GetBaseByIDX(EXTRADATA(node).baseid);

	y = nodepos[1] + node->padding;
	for (row = 0; row < BASE_SIZE; row++) {
		int x = nodepos[0] + node->padding;
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col].blocked) {
				MN_DrawFill(x, y, width, height, c_gray);
			} else if (base->map[row][col].building) {
				/* maybe destroyed in the meantime */
				if (base->founded)
					MN_DrawFill(x, y, width, height, node->color);
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
 * @param[in] node The menu node definition for the base map
 * @param[in] x Absolute x-position requested
 * @param[in] y Absolute y-position requested
 * @param[out] col Col of the cell at the position (-1 if no cell)
 * @param[out] row Row of the cell at the position (-1 if no cell)
 */
static void MN_BaseMapGetCellAtPos (const menuNode_t *node, int x, int y, int *col, int *row)
{
	assert(col);
	assert(row);
	MN_NodeAbsoluteToRelativePos(node, &x, &y);
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
static inline qboolean MN_BaseMapIsCellFree (const base_t *base, int col, int row)
{
	return col >= 0 && col < BASE_SIZE
	 && row >= 0 && row < BASE_SIZE
	 && base->map[row][col].building == NULL
	 && !base->map[row][col].blocked;
}

/**
 * @brief Draws a base.
 */
static void MN_BaseMapNodeDraw (menuNode_t * node)
{
	int width, height, row, col;
	char image[MAX_QPATH];		/**< this buffer should not be need */
	building_t *building;
	const building_t *secondBuilding = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base) {
		MN_PopMenu(qfalse);
		return;
	}

	width = node->size[0] / BASE_SIZE;
	height = node->size[1] / BASE_SIZE + BASE_IMAGE_OVERLAY;

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			vec2_t pos;
			MN_GetNodeAbsPos(node, pos);
			pos[0] += col * width;
			pos[1] += row * (height - BASE_IMAGE_OVERLAY);

			/* base tile */
			image[0] = '\0';
			if (base->map[row][col].blocked) {
				building = NULL;
				Q_strncpyz(image, "base/invalid", sizeof(image));
			} else if (!base->map[row][col].building) {
				building = NULL;
				Q_strncpyz(image, "base/grid", sizeof(image));
			} else {
				building = base->map[row][col].building;
				secondBuilding = NULL;
				assert(building);

				if (!building->used) {
					if (building->needs)
						/** @todo the view should not edit the model */
						building->used = 1;
					if (building->image)
						Q_strncpyz(image, building->image, sizeof(image));
				} else if (building->needs) {
					/** @todo Understand this code */
					/* some buildings are drawn with two tiles - e.g. the hangar is no square map tile.
					 * These buildings have the needs parameter set to the second building part which has
					 * its own image set, too. We are searching for this second building part here. */
					secondBuilding = B_GetBuildingTemplate(building->needs);
					if (!secondBuilding)
						Com_Error(ERR_DROP, "Error in ufo-scriptfile - could not find the needed building");
					Q_strncpyz(image, secondBuilding->image, sizeof(image));
					/** @todo the view should not edit the model */
					building->used = 0;
				}
			}

			/* draw tile */
			if (image[0] != '\0')
				MN_DrawNormImageByName(pos[0], pos[1], width, height, 0, 0, 0, 0, image);

			/* only draw for first part of building */
			if (building && !secondBuilding) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
				{
					const int time = building->buildTime - (ccs.date.day - building->timeStart);
					MN_DrawString("f_small", ALIGN_UL, pos[0] + 10, pos[1] + 10, pos[0] + 10, pos[1] + 10, node->size[0], 0, node->texh[0], va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse, 0);
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

	MN_BaseMapGetCellAtPos(node, mousePosX, mousePosY, &col, &row);
	if (col == -1)
		return;

	/* if we are building */
	if (ccs.baseAction == BA_NEWBUILDING) {
		qboolean isLarge;
		assert(base->buildingCurrent);
		/** @todo we should not compute here if we can (or not build something) the map model know it better */
		if (!MN_BaseMapIsCellFree(base, col, row))
			return;

		isLarge = base->buildingCurrent->needs != NULL;

		/* large building */
		if (isLarge) {
			if (MN_BaseMapIsCellFree(base, col + 1, row)) {
				/* ok */
			} else if (MN_BaseMapIsCellFree(base, col - 1, row)) {
				/* fix col at the left cell */
				col--;
			} else {
				col = -1;
			}
		}
		if (col != -1) {
			vec2_t pos;
			MN_GetNodeAbsPos(node, pos);
			if (isLarge) {
				MN_DrawNormImageByName(pos[0] + col * width, pos[1] + row * (height - BASE_IMAGE_OVERLAY), width + width, height, 0, 0, 0, 0, "base/hover2");
			} else
				MN_DrawNormImageByName(pos[0] + col * width, pos[1] + row * (height - BASE_IMAGE_OVERLAY), width, height, 0, 0, 0, 0, "base/hover");
		}
	}
}

/**
 * @brief Custom tooltip
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void MN_BaseMapNodeDrawTooltip (menuNode_t *node, int x, int y)
{
	int col, row;
	building_t *building;
	const int itemToolTipWidth = 250;
	char *tooltipText;
	base_t *base = B_GetCurrentSelectedBase();

	MN_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	building = base->map[row][col].building;
	if (!building)
		return;

	tooltipText = _(building->name);
	if (!B_CheckBuildingDependencesStatus(base, building))
		tooltipText = va("%s\n%s %s", tooltipText, _("not operational, depends on"), _(building->dependsBuilding->name));
	MN_DrawTooltip(tooltipText, x, y, itemToolTipWidth, 0);
}

/**
 * @brief Left click on the basemap
 * @sa MN_BaseMapRightClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapNodeClick (menuNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	MN_BaseMapGetCellAtPos(node, x, y, &col, &row);
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
			Com_Error(ERR_DROP, "MN_BaseMapNodeClick: no entry at %i:%i", x, y);

		assert(!base->map[row][col].blocked);

		B_BuildingOpenAfterClick(base, entry);
		ccs.baseAction = BA_NONE;
		return;
	}
}

/**
 * @brief Right click on the basemap
 * @sa MN_BaseMapNodeClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapNodeRightClick (menuNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	MN_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	if (base->map[row][col].building) {
		building_t *entry = base->map[row][col].building;
		if (!entry)
			Sys_Error("MN_BaseMapNodeRightClick: no entry at %i:%i\n", x, y);

		assert(!base->map[row][col].blocked);
		B_MarkBuildingDestroy(base, entry);
		return;
	}
}

/**
 * @brief Middle click on the basemap
 * @sa MN_BaseMapNodeClick
 * @param[in] node The menu node definition for the base map
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 * @note relies on @c baseCurrent
 */
static void MN_BaseMapNodeMiddleClick (menuNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = B_GetCurrentSelectedBase();

	assert(base);
	assert(node);
	assert(node->root);

	MN_BaseMapGetCellAtPos(node, x, y, &col, &row);
	if (col == -1)
		return;

	if (base->map[row][col].building) {
		building_t *entry = base->map[row][col].building;
		if (!entry)
			Com_Error(ERR_DROP, "MN_BaseMapNodeMiddleClick: no entry at %i:%i", x, y);

		assert(!base->map[row][col].blocked);
		B_DrawBuilding(base, entry);
		return;
	}
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_BaseLayoutNodeLoading (menuNode_t *node)
{
	node->padding = 3;
	Vector4Set(node->color, 1, 1, 1, 1);
}

static const value_t properties[] = {
	/* Identify the base, from a base ID, the node use. */
	{"baseid", V_INT, MN_EXTRADATA_OFFSETOF(baseExtraData_t, baseid), MEMBER_SIZEOF(baseExtraData_t, baseid)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractBaseNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractbase";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->loaded = MN_AbstractBaseNodeLoaded;
}

void MN_RegisterBaseMapNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "basemap";
	behaviour->extends = "abstractbase";
	behaviour->draw = MN_BaseMapNodeDraw;
	behaviour->leftClick = MN_BaseMapNodeClick;
	behaviour->rightClick = MN_BaseMapNodeRightClick;
	behaviour->drawTooltip = MN_BaseMapNodeDrawTooltip;
	behaviour->middleClick = MN_BaseMapNodeMiddleClick;
}

void MN_RegisterBaseLayoutNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "baselayout";
	behaviour->extends = "abstractbase";
	behaviour->draw = MN_BaseLayoutNodeDraw;
	behaviour->loading = MN_BaseLayoutNodeLoading;
	behaviour->extraDataSize = sizeof(baseExtraData_t);
}
