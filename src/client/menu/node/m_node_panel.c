/**
 * @file m_node_panel.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_render.h"
#include "m_node_abstractnode.h"
#include "m_node_panel.h"

#define EXTRADATA(node) node->u.panel

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

static const nodeBehaviour_t const *localBehaviour;

/**
 * @brief Handles Button draw
 */
static void MN_PanelNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *image;
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);
}

/**
 * @brief Create a top-down flow layout with child of the node.
 * Child position is automatically set, child height don't change
 * and child width is set according to node width and padding
 * @param[in,out] node The panel node to render the children for
 * @param[in] marge The margin between all children nodes in their y-position of the panel
 * @note test only
 */
static void MN_TopDownFlowLayout (menuNode_t *node, int marge)
{
	const int width = node->size[0] - node->padding - node->padding;
	int positionY = node->padding;
	menuNode_t *child = node->firstChild;
	vec2_t newSize = {width, 0};

	while (child) {
		newSize[1] = child->size[1];
		MN_NodeSetSize(child, newSize);
		child->pos[0] = node->padding;
		child->pos[1] = positionY;
		positionY += child->size[1] + marge;
		child = child->next;
	}
}

static void MN_PanelNodeDoLayout (menuNode_t *node)
{
	switch (EXTRADATA(node).layout) {
	case LAYOUT_NONE:
		break;
	case LAYOUT_TOP_DOWN:
		MN_TopDownFlowLayout(node, EXTRADATA(node).margeLayout);
		break;
	default:
		Com_Printf("MN_PanelNodeDoLayout: layout '%d' unsupported.", EXTRADATA(node).layout);
	}

	localBehaviour->super->doLayout(node);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
static void MN_PanelNodeLoaded (menuNode_t *node)
{
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", MN_GetPath(node));
#endif
}

/**
 * @brief Valid properties for a window node (called yet 'menu')
 */
static const value_t properties[] = {
	/**
	 * Select a layout manager to set position and size of child.
	 * <ul>0: no layout manager,
	 * <ul>1: top-down flow layout manager
	 * @Todo use const string to set it
	 */
	{"layout", V_INT, MN_EXTRADATA_OFFSETOF(panelExtraData_t, layout), MEMBER_SIZEOF(panelExtraData_t, layout)},
	/**
	 * Marge use to layout child
	 */
	{"margeLayout", V_INT, MN_EXTRADATA_OFFSETOF(panelExtraData_t, margeLayout), MEMBER_SIZEOF(panelExtraData_t, margeLayout)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterPanelNode (nodeBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "panel";
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(panelExtraData_t);
	behaviour->draw = MN_PanelNodeDraw;
	behaviour->loaded = MN_PanelNodeLoaded;
	behaviour->doLayout = MN_PanelNodeDoLayout;
}
