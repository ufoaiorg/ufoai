/**
 * @file m_node_item.c
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
#include "../m_parse.h"
#include "../m_nodes.h"
#include "m_node_model.h"
#include "m_node_item.h"
#include "m_node_container.h"
#include "m_node_abstractnode.h"

/**
 * @brief Draw an item node
 */
static void MN_ItemNodeDraw (menuNode_t *node)
{
	const objDef_t *od;
	const char* ref = MN_GetReferenceString(node, node->model);

	if (!ref || ref[0] == '\0')
		return;

	od = INVSH_GetItemByIDSilent(ref);
	if (od) {
		item_t item = {1, NULL, NULL, 0, 0}; /* 1 so it's not reddish; fake item anyway */
		const vec4_t color = {1, 1, 1, 1};
		vec3_t pos;
		item.t = &csi.ods[od->idx];

		if (!Q_strcmp(item.t->type, "armour")) {
			/* We position the model of the item ourself (in the middle of the item
			 * node). See the "-1, -1" parameter of MN_DrawItem. */
			MN_GetNodeAbsPos(node, pos);
			pos[0] += node->size[0] / 2.0;
			pos[1] += node->size[1] / 2.0;
			pos[2] = 0;
			/** @todo we should not use DrawItem but draw the image with render function (remove dependency with container) */
			MN_DrawItem(node, pos, &item, -1, -1, node->u.model.scale, color);
		} else {
			MN_DrawModelNode(node, item.t->model);
		}
	} else {
		const aircraft_t *aircraft = AIR_GetAircraft(ref);
		if (aircraft) {
			assert(aircraft->tech);
			MN_DrawModelNode(node, aircraft->tech->mdl);
		} else {
			Com_Printf("MN_ItemNodeDraw: Unknown item: '%s'\n", ref);
		}
	}
}

void MN_RegisterItemNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "item";
	behaviour->extends = "model";
	behaviour->draw = MN_ItemNodeDraw;
}
