/**
 * @file m_node_controls.c
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

#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_main.h"
#include "m_node_image.h"
#include "../cl_input.h"
#include "../cl_keys.h"

/**
 * @brief Handled alfer the end of the load of the node from the script (all data and/or child are set)
 */
static void MN_ControlsNodeLoaded (menuNode_t *node) {
	/* update the size when its possible */
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->texl[0] != 0 || node->texh[0] != 0) {
			node->size[0] = node->texh[0] - node->texl[0];
			node->size[1] = node->texh[1] - node->texl[1];
		} else if (node->dataImageOrModel) {
			int sx, sy;
			R_DrawGetPicSize(&sx, &sy, node->dataImageOrModel);
			node->size[0] = sx;
			node->size[1] = sy;
		}
	}
}

static int deltaMouseX;
static int deltaMouseY;

static void MN_ControlsNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);

		/* save position between mouse and menu pos */
		MN_NodeAbsoluteToRelativePos(node, &x, &y);
		deltaMouseX = x + node->pos[0];
		deltaMouseY = y + node->pos[1];
	}
}

static void MN_ControlsNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1)
		MN_MouseRelease();
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void MN_ControlsNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	/* compute new x position of the menu */
	x -= deltaMouseX;
	if (x < 0)
		x = 0;
	if (x + node->menu->size[0] > 1024)
		x = 1024 - node->menu->size[0];

	/* compute new y position of the menu */
	y -= deltaMouseY;
	if (y < 0)
		y = 0;
	if (y + node->menu->size[1] > 768)
		y = 768 - node->menu->size[1];

	MN_SetNewMenuPos(node->menu, x, y);
}

/**
 * @todo remove MN_ImageNodeDraw for a real draw code (copy-paste), image node may change
 */
static void MN_ControlsNodeDraw(menuNode_t *node)
{
	/* as is the node is an image */
	MN_ImageNodeDraw(node);
}

void MN_RegisterNodeControls (nodeBehaviour_t *behaviour)
{
	behaviour->name = "controls";
	behaviour->draw = MN_ControlsNodeDraw;
	behaviour->loaded = MN_ControlsNodeLoaded;
	behaviour->mouseDown = MN_ControlsNodeMouseDown;
	behaviour->mouseUp = MN_ControlsNodeMouseUp;
	behaviour->capturedMouseMove = MN_ControlsNodeCapturedMouseMove;
}
