#include "../client.h"
#include "../cl_global.h"
#include "../cl_map.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_inventory.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_node_model.h"

/**
 * @brief Draw a small square with the menu layout of the given base
 */
void MN_BaseMapLayout (menuNode_t * node)
{
	base_t *base;
	int height, width, x, y;
	int row, col;
	const vec4_t c_gray = {0.5, 0.5, 0.5, 1.0};
	vec2_t size;

	if (node->baseid >= MAX_BASES || node->baseid < 0)
		return;

	height = node->size[1] / BASE_SIZE;
	width = node->size[0] / BASE_SIZE;

	Vector2Copy(node->size, size);
	size[0] += (BASE_SIZE + 1) * node->padding;
	size[1] += (BASE_SIZE + 1) * node->padding;
	R_DrawFill(node->pos[0], node->pos[1], size[0], size[1], node->align, node->bgcolor);

	base = B_GetBaseByIDX(node->baseid);

	for (row = 0; row < BASE_SIZE; row++) {
		for (col = 0; col < BASE_SIZE; col++) {
			x = node->pos[0] + (width * col + node->padding * (col + 1));
			y = node->pos[1] + (height * row + node->padding * (row + 1));
			if (base->map[row][col].blocked) {
				R_DrawFill(x, y, width, height, node->align, c_gray);
			} else if (base->map[row][col].building) {
				/* maybe destroyed in the meantime */
				if (base->founded)
					R_DrawFill(x, y, width, height, node->align, node->color);
			}
		}
	}
}

/** 20 is the height of the part where the images overlap */
#define BASE_IMAGE_OVERLAY 20

/**
 * @brief Draws a base.
 * @sa MN_DrawMenus
 */
void MN_BaseMapDraw (menuNode_t * node)
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
						Sys_Error("Error in ufo-scriptfile - could not find the needed building\n");
					Q_strncpyz(image, secondBuilding->image, sizeof(image));
					building->used = 0;
				}
			}

			if (image[0] != '\0')
				R_DrawNormPic(x, y, width, height, 0, 0, 0, 0, 0, qfalse, image);

			/* check for hovering building name or outline border */
			if (mousePosX > x && mousePosX < x + width && mousePosY > y && mousePosY < y + height - 20) {
				if (!baseCurrent->map[row][col].building
				 && !baseCurrent->map[row][col].blocked) {
					if (gd.baseAction == BA_NEWBUILDING && xHover == -1) {
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
					R_FontDrawString("f_small", 0, x + 10, y + 10, x + 10, y + 10, node->size[0], 0, node->texh[0], va(ngettext("%i day left", "%i days left", time), time), 0, 0, NULL, qfalse);
					break;
				default:
					break;
				}
			}
		}
	}
	if (hoverBuilding) {
		R_Color(color);
		R_FontDrawString("f_small", 0, mousePosX + 3, mousePosY, mousePosX + 3, mousePosY, node->size[0], 0, node->texh[0], _(hoverBuilding->name), 0, 0, NULL, qfalse);
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

void MN_RegisterNodeBaseMap(nodeBehaviour_t *behaviour) {
	behaviour->name = "basemap";
	behaviour->draw = MN_BaseMapDraw;
}

void MN_RegisterNodeBaseLayout(nodeBehaviour_t *behaviour) {
	behaviour->name = "baselayout";
	behaviour->draw = MN_BaseMapLayout;
}
