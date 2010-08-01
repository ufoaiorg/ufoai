/**
 * @file m_node_controls.c
 * @brief Controls is a special <code>pic</code> entity with which
 * the windows can be moved (drag & drop).
 * @todo Remove it. Window node can manage itself dragging.
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_main.h"
#include "m_node_image.h"
#include "m_node_controls.h"
#include "m_node_abstractnode.h"

#include "../../input/cl_keys.h"
#include "../../cl_video.h"

static int deltaMouseX;
static int deltaMouseY;

static void UI_ControlsNodeMouseDown (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1) {
		UI_SetMouseCapture(node);

		/* save position between mouse and menu pos */
		UI_NodeAbsoluteToRelativePos(node, &x, &y);
		deltaMouseX = x + node->pos[0];
		deltaMouseY = y + node->pos[1];
	}
}

static void UI_ControlsNodeMouseUp (uiNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1)
		UI_MouseRelease();
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void UI_ControlsNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	/* compute new x position of the menu */
	x -= deltaMouseX;
	if (x < 0)
		x = 0;
	if (x + node->root->size[0] > viddef.virtualWidth)
		x = viddef.virtualWidth - node->root->size[0];

	/* compute new y position of the menu */
	y -= deltaMouseY;
	if (y < 0)
		y = 0;
	if (y + node->root->size[1] > viddef.virtualHeight)
		y = viddef.virtualHeight - node->root->size[1];

	UI_SetNewWindowPos(node->root, x, y);
}

void UI_RegisterControlsNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "controls";
	behaviour->extends = "image";
	behaviour->mouseDown = UI_ControlsNodeMouseDown;
	behaviour->mouseUp = UI_ControlsNodeMouseUp;
	behaviour->capturedMouseMove = UI_ControlsNodeCapturedMouseMove;
}
