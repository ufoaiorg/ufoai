/**
 * @file m_node_windowpanel.c
 * @note the marge on the texture look smaller (1 pixel), but
 * to prevent problems with loosy compression, we add some pixels
 * so 1 pixel in each side of marges a not realy used into the code
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

#include "../client.h"
#include "m_main.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"
#include "m_node_windowpanel.h"

/* constants defining all tile of the texture */

static const int LEFT_WIDTH = 20;
static const int MID_WIDTH = 1;
static const int RIGHT_WIDTH = 19;

static const int TOP_HEIGHT = 46;
static const int MID_HEIGHT = 1;
static const int BOTTOM_HEIGHT = 19;

static const int MARGE = 3;

static const int LEFT_POSX = 0;
#define MID_POSX (LEFT_POSX + LEFT_WIDTH + MARGE)
#define RIGHT_POSX (MID_POSX + MID_WIDTH + MARGE)

static const int TOP_POSY = 0;
#define MID_POSY (TOP_POSY + TOP_HEIGHT + MARGE)
#define BOTTOM_POSY (MID_POSY + MID_HEIGHT + MARGE)

/**
 * @brief Handled alfer the end of the load of the node from the script (all data and/or child are set)
 */
static void MN_WindowPanelNodeLoaded (menuNode_t *node)
{
#ifdef DEBUG
	if (node->size[0] < LEFT_WIDTH + MID_WIDTH + RIGHT_WIDTH || node->size[1] < TOP_HEIGHT + MID_HEIGHT + BOTTOM_HEIGHT)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical bugs\n", node->name);
#endif
}

static void MN_WindowPanelNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	const char* image;
	int y, h;

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!image)
		return;

	MN_GetNodeAbsPos(node, pos);

	/* draw top (from left to right) */
	R_DrawNormPic(pos[0], pos[1], LEFT_WIDTH, TOP_HEIGHT, LEFT_POSX + LEFT_WIDTH, TOP_POSY + TOP_HEIGHT,
		LEFT_POSX, TOP_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + LEFT_WIDTH, pos[1], node->size[0] - LEFT_WIDTH - RIGHT_WIDTH, TOP_HEIGHT, MID_POSX + MID_WIDTH, TOP_POSY + TOP_HEIGHT,
		MID_POSX, TOP_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + node->size[0] - RIGHT_WIDTH, pos[1], RIGHT_WIDTH, TOP_HEIGHT, RIGHT_POSX + RIGHT_WIDTH, TOP_POSY + TOP_HEIGHT,
		RIGHT_POSX, TOP_POSY, ALIGN_UL, node->blend, image);

	/* draw middle (from left to right) */
	y = pos[1] + TOP_HEIGHT;
	h = node->size[1] - TOP_HEIGHT - BOTTOM_HEIGHT; /*< height of middle */
	R_DrawNormPic(pos[0], y, LEFT_WIDTH, h, LEFT_POSX + LEFT_WIDTH, MID_POSY + MID_HEIGHT,
		LEFT_POSX, MID_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + LEFT_WIDTH, y, node->size[0] - LEFT_WIDTH - RIGHT_WIDTH, h, MID_POSX + MID_WIDTH, MID_POSY + MID_HEIGHT,
		MID_POSX, MID_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + node->size[0] - RIGHT_WIDTH, y, RIGHT_WIDTH, h, RIGHT_POSX + RIGHT_WIDTH, MID_POSY + MID_HEIGHT,
		RIGHT_POSX, MID_POSY, ALIGN_UL, node->blend, image);

	/* draw bottom (from left to right) */
	y = pos[1] + node->size[1] - BOTTOM_HEIGHT;
	R_DrawNormPic(pos[0], y, LEFT_WIDTH, BOTTOM_HEIGHT, LEFT_POSX + LEFT_WIDTH, BOTTOM_POSY + BOTTOM_HEIGHT,
		LEFT_POSX, BOTTOM_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + LEFT_WIDTH, y, node->size[0] - LEFT_WIDTH - RIGHT_WIDTH, BOTTOM_HEIGHT, MID_POSX + MID_WIDTH, BOTTOM_POSY + BOTTOM_HEIGHT,
		MID_POSX, BOTTOM_POSY, ALIGN_UL, node->blend, image);
	R_DrawNormPic(pos[0] + node->size[0] - RIGHT_WIDTH, y, RIGHT_WIDTH, BOTTOM_HEIGHT, RIGHT_POSX + RIGHT_WIDTH, BOTTOM_POSY + BOTTOM_HEIGHT,
		RIGHT_POSX, BOTTOM_POSY, ALIGN_UL, node->blend, image);
}

void MN_RegisterWindowPanelNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "windowpanel";
	behaviour->id = MN_WINDOWPANEL;
	behaviour->draw = MN_WindowPanelNodeDraw;
	behaviour->loaded = MN_WindowPanelNodeLoaded;
}
