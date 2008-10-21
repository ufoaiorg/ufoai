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

static void MN_ControlsNodeClick (menuNode_t *node, int x, int y)
{
	if (mouseSpace == MS_MENU) {
		mouseSpace = MS_DRAGMENU;
	}
}

static void MN_ControlsNodeDraw(menuNode_t *node)
{
	if (mouseSpace == MS_DRAGMENU){
		MN_SetNewMenuPos(node->menu, mousePosX - node->pos[0], mousePosY - node->pos[1]);
	}
	MN_DrawImageNode(node);
}

void MN_RegisterNodeControls (nodeBehaviour_t *behaviour)
{
	behaviour->name = "controls";
	behaviour->leftClick = MN_ControlsNodeClick;
	behaviour->draw = MN_ControlsNodeDraw;
}
