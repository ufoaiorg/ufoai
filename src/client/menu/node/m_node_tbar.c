/**
 * @file m_node_tbar.c
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "m_node_tbar.h"
#include "m_node_abstractvalue.h"
#include "m_node_abstractnode.h"

static void MN_TBarNodeDraw (menuNode_t *node)
{
	/* dataImageOrModel is the texture name */
	float shx;
	vec2_t nodepos;
	menu_t *menu = node->menu;
	const char* ref = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!ref || ref[0] == '\0')
		return;

	MN_GetNodeAbsPos(node, nodepos);

	if (node->pointWidth) {
		const float ps = MN_GetReferenceFloat(menu, node->u.abstractvalue.value) - MN_GetReferenceFloat(menu, node->u.abstractvalue.min);
		shx = node->texl[0] + round(ps * node->pointWidth) + (ps > 0 ? floor((ps - 1) / 10) * node->gapWidth : 0);
	} else
		shx = node->texh[0];

	R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
		shx, node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref);
}

void MN_RegisterTBarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "tbar";
	behaviour->extends = "abstractvalue";
	behaviour->id = MN_TBAR;
	behaviour->draw = MN_TBarNodeDraw;
}
