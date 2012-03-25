/**
 * @file ui_node_item.c
 * @brief An item is only a model node allowing to display soldier armour.
 * Soldier armour is an image, not a model.
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
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "ui_node_model.h"
#include "ui_node_item.h"
#include "ui_node_container.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../cgame/cl_game.h"
#include "../../renderer/r_draw.h"

#define EXTRADATA(node) UI_EXTRADATA(node, modelExtraData_t)

/**
 * @brief Draw an item node
 */
static void UI_ItemNodeDraw (uiNode_t *node)
{
	const objDef_t *od;
	const char* ref = UI_GetReferenceString(node, EXTRADATA(node).model);
	vec2_t pos;

	if (Q_strnull(ref))
		return;

	UI_GetNodeAbsPos(node, pos);
	R_CleanupDepthBuffer(pos[0], pos[1], node->size[0], node->size[1]);

	od = INVSH_GetItemByIDSilent(ref);
	if (od) {
		item_t item = {1, NULL, NULL, 0, 0}; /* 1 so it's not reddish; fake item anyway */
		item.t = INVSH_GetItemByIDX(od->idx);

		if (EXTRADATA(node).containerLike || INV_IsArmour(item.t)) {
			const vec4_t color = {1, 1, 1, 1};
			vec3_t itemNodePos;
			/* We position the model of the item ourself (in the middle of the item
			 * node). See the "-1, -1" parameter of UI_DrawItem. */
			UI_GetNodeAbsPos(node, itemNodePos);
			itemNodePos[0] += node->size[0] / 2.0;
			itemNodePos[1] += node->size[1] / 2.0;
			itemNodePos[2] = 0;
			/** @todo we should not use DrawItem but draw the image with render function (remove dependency with container) */
			UI_DrawItem(node, itemNodePos, &item, -1, -1, EXTRADATA(node).scale, color);
		} else {
			UI_DrawModelNode(node, GAME_GetModelForItem(item.t, NULL));
		}
	} else {
		GAME_DisplayItemInfo(node, ref);
	}
}

void UI_RegisterItemNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "item";
	behaviour->extends = "model";
	behaviour->draw = UI_ItemNodeDraw;

	/* Display an item like a container node do it */
	UI_RegisterExtradataNodeProperty(behaviour, "containerlike", V_BOOL, modelExtraData_t, containerLike);
}
