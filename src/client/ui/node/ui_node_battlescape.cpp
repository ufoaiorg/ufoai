/**
 * @file
 * @brief The <code>battlescape</code> node identify the part of the screen the engine
 * use to render battlescape map
 * @todo Maybe we should capture the input with it, instead of using client namespace MS_*
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../ui_nodes.h"
#include "../ui_behaviour.h"
#include "ui_node_abstractnode.h"
#include "ui_node_battlescape.h"
#include "../../client.h"

/**
 * @brief Determine the position and size of the render
 */
static void UI_SetRenderRect (int x, int y, int width, int height)
{
	viddef.x = x * viddef.rx;
	viddef.y = y * viddef.ry;
	viddef.viewWidth = width * viddef.rx;
	viddef.viewHeight = height * viddef.ry;
}

/**
 * @brief Call before the script initialized the node
 */
void uiBattleScapeNode::onLoading (uiNode_t* node)
{
	/* node->ghost = true; */
}

void uiBattleScapeNode::draw (uiNode_t* node)
{
	/** Note: hack to fix everytime renderrect (for example when we close hud_nohud) */
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->box.size[0], node->box.size[1]);
}

void uiBattleScapeNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->box.size[0], node->box.size[1]);
}


void uiBattleScapeNode::onSizeChanged (uiNode_t* node)
{
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->box.size[0], node->box.size[1]);
}

void uiBattleScapeNode::onWindowClosed (uiNode_t* node)
{
	UI_SetRenderRect(0, 0, 0, 0);
}

/**
 * @brief Called when user request scrolling on the battlescape.
 */
bool uiBattleScapeNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	while (deltaY < 0) {
		CL_CameraZoomIn();
		deltaY++;
	}
	while (deltaY > 0) {
		CL_CameraZoomOut();
		deltaY--;
	}
	return true;
}

void UI_RegisterBattlescapeNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "battlescape";
	behaviour->manager = UINodePtr(new uiBattleScapeNode());
}
