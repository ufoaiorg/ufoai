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
#include "../../../common/scripts.h"

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
 * @param[in] margin The margin between all children nodes in their y-position of the panel
 * @note test only
 */
static void MN_TopDownFlowLayout (menuNode_t *node, int margin)
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
		positionY += child->size[1] + margin;
		child = child->next;
	}
}

/** @todo rework all this thing. Take a look at STARLAYOUT too */
#define BORDERLAYOUT_MIDDLE	1
#define BORDERLAYOUT_TOP	2
#define BORDERLAYOUT_BOTTOM	3
#define BORDERLAYOUT_LEFT	4
#define BORDERLAYOUT_RIGHT	5

#define PACKLAYOUT_FILL	1
#define PACKLAYOUT_TOP	2
#define PACKLAYOUT_BOTTOM	3
#define PACKLAYOUT_LEFT	4
#define PACKLAYOUT_RIGHT	5

/**
 * @brief Create a border layout with child of the node.
 * Child with BORDERLAYOUT_TOP and BORDERLAYOUT_BOTTOM num
 * are first positioned, there height do not change but the width fill
 * the parent node. Then child with BORDERLAYOUT_LEFT and BORDERLAYOUT_RIGHT
 * are positioned between positioned nodes. Finally BORDERLAYOUT_MIDDLE is filled
 * into available free space.
 * @param[in,out] node The panel node to render the children for
 * @param[in] margin The margin between all children nodes in their y-position of the panel
 * @note test only
 */
static void MN_BorderLayout (menuNode_t *node, int margin)
{
	menuNode_t *child;
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->size[1] - node->padding;

	// top
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != BORDERLAYOUT_TOP)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->size[1];
		MN_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = minY;
		minY += child->size[1] + margin;
	}

	// bottom
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != BORDERLAYOUT_BOTTOM)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->size[1];
		MN_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = maxY - child->size[1];
		maxY -= child->size[1] + margin;
	}

	// left
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != BORDERLAYOUT_LEFT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->size[0];
		newSize[1] = maxY - minY;
		MN_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = minY;
		minX += child->size[0] + margin;
	}

	// right
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != BORDERLAYOUT_RIGHT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->size[0];
		newSize[1] = maxY - minY;
		MN_NodeSetSize(child, newSize);
		child->pos[0] = maxX - child->size[0];
		child->pos[1] = minY;
		maxX -= child->size[0] + margin;
	}

	// middle
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != BORDERLAYOUT_MIDDLE)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = maxY - minY;
		MN_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = minY;
	}
}

/**
 * @brief Create a pack layout with child of the node.
 * Set position and size of nodes one by one.
 * It is a usefull layout manager because it can create nice
 * layout without sub panel
 *
 * @param[in,out] node The panel node to render the children for
 * @param[in] margin The margin between all children nodes
 * @note test only
 */
static void MN_PackLayout (menuNode_t *node, int margin)
{
	menuNode_t *child;
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->size[1] - node->padding;

	// top
	for (child = node->firstChild; child; child = child->next) {
		if (child->invis)
			continue;
		switch (child->align) {
		case PACKLAYOUT_TOP:
			newSize[0] = maxX - minX;
			newSize[1] = child->size[1];
			MN_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = minY;
			minY += child->size[1] + margin;
			break;
		case PACKLAYOUT_BOTTOM:
			newSize[0] = maxX - minX;
			newSize[1] = child->size[1];
			MN_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = maxY - child->size[1];
			maxY -= child->size[1] + margin;
			break;
		case PACKLAYOUT_LEFT:
			newSize[0] = child->size[0];
			newSize[1] = maxY - minY;
			MN_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = minY;
			minX += child->size[0] + margin;
			break;
		case PACKLAYOUT_RIGHT:
			newSize[0] = child->size[0];
			newSize[1] = maxY - minY;
			MN_NodeSetSize(child, newSize);
			child->pos[0] = maxX - child->size[0];
			child->pos[1] = minY;
			maxX -= child->size[0] + margin;
			break;
		case PACKLAYOUT_FILL:
			newSize[0] = maxX - minX;
			newSize[1] = maxY - minY;
			MN_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = minY;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief map for star layout from num to align
 */
static const align_t starlayoutmap[] = {
	ALIGN_UL,
	ALIGN_UC,
	ALIGN_UR,
	ALIGN_CL,
	ALIGN_CC,
	ALIGN_CR,
	ALIGN_LL,
	ALIGN_LC,
	ALIGN_LR,
};

#define ALIGN_FILL	10

/**
 * @brief Do a star layout with child according to there num
 * @note 1=top-left 2=top-middle 3=top-right
 * 4=middle-left 5=middle-middle 6=middle-right
 * 7=bottom-left 8=bottom-middle 9=bottom-right
 * 10=fill
 * @todo Tag it static when it is possible
 */
void MN_StarLayout (menuNode_t *node)
{
	menuNode_t *child;
	for (child = node->firstChild; child; child = child->next) {
		vec2_t source;
		vec2_t destination;
		align_t align;

		if (child->align <= 0 || child->align > 10)
			continue;

		if (child->align == ALIGN_FILL) {
			child->pos[0] = 0;
			child->pos[1] = 0;
			MN_NodeSetSize(child, node->size);
			child->behaviour->doLayout(child);
			continue;
		}

		align = starlayoutmap[child->align - 1];
		MN_NodeGetPoint(node, destination, align);
		MN_NodeRelativeToAbsolutePoint(node, destination);
		MN_NodeGetPoint(child, source, align);
		MN_NodeRelativeToAbsolutePoint(child, source);
		child->pos[0] += destination[0] - source[0];
		child->pos[1] += destination[1] - source[1];
	}
}

static void MN_PanelNodeDoLayout (menuNode_t *node)
{
	switch (EXTRADATA(node).layout) {
	case LAYOUT_NONE:
		break;
	case LAYOUT_TOP_DOWN_FLOW:
		MN_TopDownFlowLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_BORDER:
		MN_BorderLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_PACK:
		MN_PackLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_STAR:
		MN_StarLayout(node);
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
	 * Margin use to layout children (margin between children)
	 */
	{"layoutMargin", V_INT, MN_EXTRADATA_OFFSETOF(panelExtraData_t, layoutMargin), MEMBER_SIZEOF(panelExtraData_t, layoutMargin)},

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

	Com_RegisterConstInt("BORDERLAYOUT_TOP", BORDERLAYOUT_TOP);
	Com_RegisterConstInt("BORDERLAYOUT_BOTTOM", BORDERLAYOUT_BOTTOM);
	Com_RegisterConstInt("BORDERLAYOUT_LEFT", BORDERLAYOUT_LEFT);
	Com_RegisterConstInt("BORDERLAYOUT_RIGHT", BORDERLAYOUT_RIGHT);
	Com_RegisterConstInt("BORDERLAYOUT_MIDDLE", BORDERLAYOUT_MIDDLE);

	Com_RegisterConstInt("PACKLAYOUT_TOP", PACKLAYOUT_TOP);
	Com_RegisterConstInt("PACKLAYOUT_BOTTOM", PACKLAYOUT_BOTTOM);
	Com_RegisterConstInt("PACKLAYOUT_LEFT", PACKLAYOUT_LEFT);
	Com_RegisterConstInt("PACKLAYOUT_RIGHT", PACKLAYOUT_RIGHT);
	Com_RegisterConstInt("PACKLAYOUT_FILL", PACKLAYOUT_FILL);

	/** @todo remove the "+1" */
	Com_RegisterConstInt("STARLAYOUT_ALIGN_TOPLEFT", ALIGN_UL+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_TOP", ALIGN_UC+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_TOPRIGHT", ALIGN_UR+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_LEFT", ALIGN_CL+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_MIDDLE", ALIGN_CC+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_RIGHT", ALIGN_CR+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_BOTTOMLEFT", ALIGN_LL+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_BOTTOM", ALIGN_LC+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_BOTTOMRIGHT", ALIGN_LR+1);
	Com_RegisterConstInt("STARLAYOUT_ALIGN_FILL", ALIGN_FILL);

	Com_RegisterConstInt("LAYOUT_TOP_DOWN_FLOW", LAYOUT_TOP_DOWN_FLOW);
	Com_RegisterConstInt("LAYOUT_BORDER", LAYOUT_BORDER);
	Com_RegisterConstInt("LAYOUT_PACK", LAYOUT_PACK);
	Com_RegisterConstInt("LAYOUT_STAR", LAYOUT_STAR);
}
