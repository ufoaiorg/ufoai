/**
 * @file m_node_linestrip.c
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

void MN_DrawLineStripNode (menuNode_t *node)
{
	int i;
	if (node->linestrips.numStrips > 0) {
		/* Draw all linestrips. */
		for (i = 0; i < node->linestrips.numStrips; i++) {
			/* Draw this line if it's valid. */
			if (node->linestrips.pointList[i] && (node->linestrips.numPoints[i] > 0)) {
				R_ColorBlend(node->linestrips.color[i]);
				R_DrawLineStrip(node->linestrips.numPoints[i], node->linestrips.pointList[i]);
			}
		}
	}
}

void MN_RegisterNodeLineStrip (nodeBehaviour_t *behaviour)
{
	behaviour->name = "linestrip";
	behaviour->draw = MN_DrawLineStripNode;
}
