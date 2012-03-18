/**
 * @file ui_node_battlescape.c
 * @brief The <code>battlescape</code> node identify the part of the screen the engine
 * use to render battlescape map
 * @todo Maybe we should capture the input with it, instead of using client namespace MS_*
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

const struct uiBehaviour_s const *ui_battleScapeBehaviour;

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
static void UI_BattlescapeNodeLoading (uiNode_t *node)
{
	/* node->ghost = qtrue; */
}

static void UI_BattlescapeNodeDraw (uiNode_t *node)
{
	/** Note: hack to fix everytime renderrect (for example when we close hud_nohud) */
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->size[0], node->size[1]);
}

static void UI_BattlescapeNodeInit (uiNode_t *node, linkedList_t *params)
{
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->size[0], node->size[1]);
}


static void UI_BattlescapeNodeSizeChanged (uiNode_t *node)
{
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	UI_SetRenderRect(pos[0], pos[1], node->size[0], node->size[1]);
}

static void UI_BattlescapeNodeClose (uiNode_t *node)
{
	UI_SetRenderRect(0, 0, 0, 0);
}

/**
 * @brief Called when user request scrolling on the battlescape.
 */
static qboolean UI_BattlescapeNodeScroll (uiNode_t *node, int deltaX, int deltaY)
{
	while (deltaY < 0) {
		CL_CameraZoomIn();
		deltaY++;
	}
	while (deltaY > 0) {
		CL_CameraZoomOut();
		deltaY--;
	}
	return qtrue;
}

void UI_RegisterBattlescapeNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "battlescape";
	behaviour->loading = UI_BattlescapeNodeLoading;
	behaviour->windowOpened = UI_BattlescapeNodeInit;
	behaviour->windowClosed = UI_BattlescapeNodeClose;
	behaviour->scroll = UI_BattlescapeNodeScroll;
	behaviour->sizeChanged = UI_BattlescapeNodeSizeChanged;
	behaviour->draw = UI_BattlescapeNodeDraw;
	ui_battleScapeBehaviour = behaviour;
}
