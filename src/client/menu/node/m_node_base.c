/**
 * @file m_node_base.c
 * @todo use MN_GetNodeAbsPos instead of menu->pos
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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_tooltip.h"
#include "../m_nodes.h"
#include "m_node_model.h"
#include "m_node_base.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../renderer/r_draw.h"

/**
 * @brief Called after the end of the node load from script (all data and/or child are set)
 */
static void MN_BaseLayoutNodeLoaded (menuNode_t * node)
{
	/* if it exists a better size that the size requested on the .ufo
	 * (for example: rounding error) we can force a better size here
	 */
}

/**
 * @brief Draw a small square with the menu layout of the given base
 */
static void MN_BaseLayoutNodeDraw (menuNode_t * node)
{
	base_t *base;
	int height, width, x, y;
	int row, col;
	const vec4_t c_gray = {0.5, 0.5, 0.5, 1.0};
	vec2_t nodepos;
	int totalMarge;

	if (node->baseid >= MAX_BASES || node->baseid < 0)
		return;

	totalMarge = node->padding * (BASE_SIZE + 1);
	width = (node->size[0] - totalMarge) / BASE_SIZE;
	height = (node->size[1] - totalMarge) / BASE_SIZE;

	MN_GetNodeAbsPos(node, nodepos);

	base = B_GetBaseByIDX(node->baseid);

	y = nodepos[1] + node->padding;
	for (row = 0; row < BASE_SIZE; row++) {
		x = nodepos[0] + node->padding;
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col].blocked) {
				R_DrawFill(x, y, width, height, ALIGN_UL, c_gray);
			} else if (base->map[row][col].building) {
				/* maybe destroyed in the meantime */
				if (base->founded)
					R_DrawFill(x, y, width, height, ALIGN_UL, node->color);
			}
			x += width + node->padding;
		}
		y += height + node->padding;
	}
}

/** 20 is the height of the part where the images overlap */
#define BASE_IMAGE_OVERLAY 20

/**
 * @brief Draws a base.
 * @sa MN_DrawMenus
 */
static void MN_BaseMapDraw (menuNode_t * node)
{
	int xHover = -1, yHover = -1, widthHover = 1;
	int width, height, row, col, time, colSecond;
	const vec4_t color = { 0.5f, 1.0f, 0.5f, 1.0 };
	char image[MAX_QPATH];
	building_t *building, *secondBuilding = NULL, *hoverBuilding = NULL;

	if (!baseCurrent) {
		MN_PopMenu(qfalse);
		return;
	}

	width = node->size[0] / BASE_SIZE;
	height = (node->size[1] + BASE_SIZE * BASE_IMAGE_OVERLAY) / BASE_SIZE;

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			const int x = node->pos[0] + col * width;
			const int y = node->pos[1] + row * height - row * BASE_IMAGE_OVERLAY;

			baseCurrent->map[row][col].posX = x;
			baseCurrent->map[row][col].posY = y;
			image[0] = '\0';

			if (baseCurrent->map[row][col].blocked) {
				building = NULL;
				Q_strncpyz(image, "base/invalid", sizeof(image));
			} else if (!baseCurrent->map[row][col].building) {
				building = NULL;
				Q_strncpyz(image, "base/grid", sizeof(image));
			} else {
				building = baseCurrent->map[row][col].building;
				secondBuilding = NULL;
				assert(building);

				if (!building->used) {
					if (building->needs)
						building->used = 1;
					if (building->image)
						Q_strncpyz(image, building->image, sizeof(image));
				} else if (building->needs) {
					secondBuilding = B_GetBuildingTemplate(building->needs);
					if (!secondBuilding)
						Sys_Error("Error in ufo-scriptfile - could not find the needed building");
					Q_strncpyz(image, secondBuilding->image, sizeof(image));
					building->used = 0;
				}
			}

			if (image[0] != '\0')
				R_DrawNormPic(x, y, width, height, 0, 0, 0, 0, 0, qfalse, image);

			/* check for hovering building name or outline border */
			if (node->state && mousePosX > x && mousePosX < x + width && mousePosY > y && mousePosY < y + height - 20) {
				if (!baseCurrent->map[row][col].building
				 && !baseCurrent->map[row][col].blocked) {
					if (ccs.baseAction == BA_NEWBUILDING && xHover == -1) {
						assert(baseCurrent->buildingCurrent);
						colSecond = col;
						if (baseCurrent->buildingCurrent->needs) {
							if (colSecond + 1 == BASE_SIZE) {
								if (!baseCurrent->map[row][colSecond - 1].building
								 && !baseCurrent->map[row][colSecond - 1].blocked)
									colSecond--;
							} else if (baseCurrent->map[row][colSecond + 1].building) {
								if (!baseCurrent->map[row][colSecond - 1].building
								 && !baseCurrent->map[row][colSecond - 1].blocked)
									colSecond--;
							} else {
								colSecond++;
							}
							if (colSecond != col) {
								if (colSecond < col)
									xHover = node->pos[0] + colSecond * width;
								else
									xHover = x;
								widthHover = 2;
							}
						} else
							xHover = x;
						yHover = y;
					}
				} else {
					hoverBuilding = building;
				}
			}

			/* only draw for first part of building */
			if (building && !secondBuilding) {
				switch (building->buildingStatus) {
				case B_STATUS_DOWN:
				case B_STATUS_CONSTRUCTION_FINISHED:
					break;
				case B_STATUS_UNDER_CONSTRUCTION:
					time = building->buildTime - (ccs.date.day - building->timeStart);
					R_FontDrawString("f_small", 0, x + 10, y + 10, x + 10, y + 10, node->size[0], 0, node->texh[0], va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse, 0);
					break;
				default:
					break;
				}
			}
		}
	}
	if (node->state && hoverBuilding) {
		R_Color(color);
		R_FontDrawString("f_small", 0, mousePosX + 3, mousePosY, mousePosX + 3, mousePosY, node->size[0], 0, node->texh[0], _(hoverBuilding->name), 0, 0, NULL, qfalse, 0);
		R_Color(NULL);
	}
	if (xHover != -1) {
		if (widthHover == 1) {
			Q_strncpyz(image, "base/hover", sizeof(image));
			R_DrawNormPic(xHover, yHover, width, height, 0, 0, 0, 0, 0, qfalse, image);
		} else {
			Com_sprintf(image, sizeof(image), "base/hover%i", widthHover);
			R_DrawNormPic(xHover, yHover, width * widthHover, height, 0, 0, 0, 0, 0, qfalse, image);
		}
	}
}

/**
 * @brief Left click on the basemap
 * @sa MN_BaseMapRightClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapClick (menuNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = baseCurrent;

	assert(base);
	assert(node);
	assert(node->menu);

	if (ccs.baseAction == BA_NEWBUILDING) {
		assert(base->buildingCurrent);
		for (row = 0; row < BASE_SIZE; row++)
			for (col = 0; col < BASE_SIZE; col++) {
				if (x >= base->map[row][col].posX
				 && x < base->map[row][col].posX + node->size[0] / BASE_SIZE
				 && y >= base->map[row][col].posY
				 && y < base->map[row][col].posY + node->size[1] / BASE_SIZE) {
					/* we're on the tile the player clicked */
					if (!base->map[row][col].building && !base->map[row][col].blocked) {
						if (!base->buildingCurrent->needs
						 || (col < BASE_SIZE - 1 && !base->map[row][col + 1].building && !base->map[row][col + 1].blocked)
						 || (col > 0 && !base->map[row][col - 1].building && !base->map[row][col - 1].blocked))
						/* Set position for a new building */
						B_SetBuildingByClick(base, base->buildingCurrent, row, col);
					}
					return;
				}
			}
	}

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (base->map[row][col].building && x >= base->map[row][col].posX
			 && x < base->map[row][col].posX + node->size[0] / BASE_SIZE && y >= base->map[row][col].posY
			 && y < base->map[row][col].posY + node->size[1] / BASE_SIZE) {
				const building_t *entry = base->map[row][col].building;
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i", x, y);

				assert(!base->map[row][col].blocked);

				B_BuildingOpenAfterClick(base, entry);
				ccs.baseAction = BA_NONE;
				return;
			}
}

/**
 * @brief Right click on the basemap
 * @sa MN_BaseMapClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapRightClick (menuNode_t *node, int x, int y)
{
	int row, col;
	base_t *base = baseCurrent;

	assert(base);
	assert(node);
	assert(node->menu);

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (base->map[row][col].building && x >= base->map[row][col].posX + node->menu->pos[0]
			 && x < base->map[row][col].posX + node->menu->pos[0] + node->size[0] / BASE_SIZE && y >= base->map[row][col].posY + node->menu->pos[1]
			 && y < base->map[row][col].posY + node->menu->pos[1] + node->size[1] / BASE_SIZE) {
				building_t *entry = base->map[row][col].building;
				if (!entry)
					Sys_Error("MN_BaseMapRightClick: no entry at %i:%i\n", x, y);

				assert(!base->map[row][col].blocked);
				B_MarkBuildingDestroy(base, entry);
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

void MN_RegisterBaseMapNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "basemap";
	behaviour->draw = MN_BaseMapDraw;
	behaviour->leftClick = MN_BaseMapClick;
	behaviour->rightClick = MN_BaseMapRightClick;
}

void MN_RegisterBaseLayoutNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "baselayout";
	behaviour->draw = MN_BaseLayoutNodeDraw;
	behaviour->loaded = MN_BaseLayoutNodeLoaded;
	behaviour->loading = MN_BaseLayoutNodeLoading;
}
