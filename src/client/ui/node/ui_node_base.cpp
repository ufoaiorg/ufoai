/**
 * @file
 */

/*
Copyright (C) 2002-2023 UFO: Alien Invasion.

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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_tooltip.h"
#include "../ui_nodes.h"
#include "../ui_render.h"
#include "ui_node_base.h"

#include "../../cl_shared.h"
#include "../../cgame/cl_game.h"
#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"
#include "../../sound/s_main.h"

#include "../../../common/scripts_lua.h"

#define EXTRADATA_TYPE baseExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

// TODO: remove me - duplicated in cp_base.h
#define BASE_SIZE	5

/**
 * @brief Custom tooltip for baseayout
 * @param[in] node Node we request to draw tooltip
 * @param[in] x,y Position of the mouse
 */
void uiBaseLayoutNode::drawTooltip (const uiNode_t* node, int x, int y) const
{
	GAME_DrawBaseLayoutTooltip(EXTRADATACONST(node).baseid, x, y);
}

/**
 * @brief Draw a small square with the layout of the given base
 */
void uiBaseLayoutNode::draw (uiNode_t* node)
{
	const int totalMarge = node->padding * (BASE_SIZE + 1);
	const int width = (node->box.size[0] - totalMarge) / BASE_SIZE;
	const int height = (node->box.size[1] - totalMarge) / BASE_SIZE;

	vec2_t nodepos;
	UI_GetNodeAbsPos(node, nodepos);

	GAME_DrawBaseLayout(EXTRADATA(node).baseid, nodepos[0], nodepos[1], totalMarge, width, height, node->padding, node->bgcolor, node->color);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiBaseLayoutNode::onLoading (uiNode_t* node)
{
	EXTRADATA(node).baseid = -1;
	node->padding = 3;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->bgcolor, 0.5, 0.5, 0.5, 1);
}

void UI_RegisterBaseLayoutNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "baselayout";
	behaviour->manager = UINodePtr(new uiBaseLayoutNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->lua_SWIG_typeinfo = UI_SWIG_TypeQuery("uiBaseLayoutNode_t *");

	/* Identify the base, from a base ID, the node use. */
	UI_RegisterExtradataNodeProperty(behaviour, "baseid", V_INT, baseExtraData_t, baseid);
}
