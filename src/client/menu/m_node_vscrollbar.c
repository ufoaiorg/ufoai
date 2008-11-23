/**
 * @file m_node_vscrollbar.c
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
#include "m_node_abstractscrollbar.h"
#include "m_node_vscrollbar.h"

static void MN_VScrollbarNodeClick (menuNode_t *node, int x, int y)
{
}

static void MN_VScrollbarNodeDraw (menuNode_t *node)
{
}

static void MN_VScrollbarNodeLoaded (menuNode_t *node)
{
	node->size[1] = 27;
}

void MN_RegisterVScrollbarNode (nodeBehaviour_t *behaviour)
{
	/* inheritance */
	MN_RegisterAbstractScrollbarNode(behaviour);
	/* overwrite */
	behaviour->name = "vscrollbar";
	behaviour->id = MN_VSCROLLBAR;
	behaviour->leftClick = MN_VScrollbarNodeClick;
	behaviour->draw = MN_VScrollbarNodeDraw;
	behaviour->loaded = MN_VScrollbarNodeLoaded;
}
