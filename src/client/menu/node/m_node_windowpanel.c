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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_main.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "m_node_windowpanel.h"
#include "m_node_abstractnode.h"

/* constants defining all tile of the texture */

#define LEFT_WIDTH 20
#define MID_WIDTH 1
#define RIGHT_WIDTH 19

#define TOP_HEIGHT 46
#define MID_HEIGHT 1
#define BOTTOM_HEIGHT 19

#define MARGE 3

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
	static const int panelTemplate[] = {
		LEFT_WIDTH, MID_WIDTH, RIGHT_WIDTH,
		TOP_HEIGHT, MID_HEIGHT, BOTTOM_HEIGHT,
		MARGE
	};
	vec2_t pos;
	const char* image;

	image = MN_GetReferenceString(node->menu, node->image);
	if (!image)
		return;

	MN_GetNodeAbsPos(node, pos);
	R_DrawPanel(pos, node->size, image, node->blend, 0, 0, panelTemplate);
}

void MN_RegisterWindowPanelNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "windowpanel";
	behaviour->draw = MN_WindowPanelNodeDraw;
	behaviour->loaded = MN_WindowPanelNodeLoaded;
}
