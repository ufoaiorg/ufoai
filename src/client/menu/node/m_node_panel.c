/**
 * @file m_node_panel.c
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
#include "m_node_abstractnode.h"
#include "m_node_panel.h"

#include "../../renderer/r_draw.h"

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief draw a panel from a texture as we can see on the image
 * The function is inline because there are often 3 or 5 const param, with it a lot of var became const too
 * @image html http://ufoai.ninex.info/wiki/images/Inline_draw_panel.png
 * @param[in] pos Position of the output panel
 * @param[in] size Size of the output panel
 * @param[in] texture Texture contain the template of the panel
 * @param[in] blend True if the texture must use alpha channel for per pixel transparency
 * @param[in] texX Position x of the panel template into the texture
 * @param[in] texY Position y of the panel template into the texture
 * @param[in] panelDef Array of seven elements define the panel template used in the texture.
 * From the first to the last: left width, mid width, right width,
 * top height, mid height, bottom height, and margin
 * @todo can we improve the code? is it need?
 */
void MN_DrawPanel (const vec2_t pos, const vec2_t size, const char *texture, qboolean blend, int texX, int texY, const int *panelDef)
{
	const int leftWidth = panelDef[0];
	const int midWidth = panelDef[1];
	const int rightWidth = panelDef[2];
	const int topHeight = panelDef[3];
	const int midHeight = panelDef[4];
	const int bottomHeight = panelDef[5];
	const int marge =  panelDef[6];

	/** @todo merge texX and texY here */
	const int firstPos = 0;
	const int secondPos = firstPos + leftWidth + marge;
	const int thirdPos = secondPos + midWidth + marge;
	const int firstPosY = 0;
	const int secondPosY = firstPosY + topHeight + marge;
	const int thirdPosY = secondPosY + midHeight + marge;

	int y, h;

	/* draw top (from left to right) */
	R_DrawNormPic(pos[0], pos[1], leftWidth, topHeight, texX + firstPos + leftWidth, texY + firstPosY + topHeight,
		texX + firstPos, texY + firstPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, pos[1], size[0] - leftWidth - rightWidth, topHeight, texX + secondPos + midWidth, texY + firstPosY + topHeight,
		texX + secondPos, texY + firstPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - rightWidth, pos[1], rightWidth, topHeight, texX + thirdPos + rightWidth, texY + firstPosY + topHeight,
		texX + thirdPos, texY + firstPosY, ALIGN_UL, blend, texture);

	/* draw middle (from left to right) */
	y = pos[1] + topHeight;
	h = size[1] - topHeight - bottomHeight; /* height of middle */
	R_DrawNormPic(pos[0], y, leftWidth, h, texX + firstPos + leftWidth, texY + secondPosY + midHeight,
		texX + firstPos, texY + secondPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, h, texX + secondPos + midWidth, texY + secondPosY + midHeight,
		texX + secondPos, texY + secondPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - rightWidth, y, rightWidth, h, texX + thirdPos + rightWidth, texY + secondPosY + midHeight,
		texX + thirdPos, texY + secondPosY, ALIGN_UL, blend, texture);

	/* draw bottom (from left to right) */
	y = pos[1] + size[1] - bottomHeight;
	R_DrawNormPic(pos[0], y, leftWidth, bottomHeight, texX + firstPos + leftWidth, texY + thirdPosY + bottomHeight,
		texX + firstPos, texY + thirdPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, bottomHeight, texX + secondPos + midWidth, texY + thirdPosY + bottomHeight,
		texX + secondPos, texY + thirdPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - bottomHeight, y, rightWidth, bottomHeight, texX + thirdPos + rightWidth, texY + thirdPosY + bottomHeight,
		texX + thirdPos, texY + thirdPosY, ALIGN_UL, blend, texture);
}

/**
 * @brief Handles Button draw
 */
static void MN_PanelNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *image;
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, node->blend, 0, 0, panelTemplate);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
static void MN_PanelNodeLoaded (menuNode_t *node)
{
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", MN_GetPath(node));
#endif
}

void MN_RegisterPanelNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "panel";
	behaviour->draw = MN_PanelNodeDraw;
	behaviour->loaded = MN_PanelNodeLoaded;
}
