/**
 * @file ui_node_panel.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../ui_render.h"
#include "../ui_actions.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"
#include "../../../common/scripts.h"

#define EXTRADATA_TYPE panelExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

static const uiBehaviour_t const *localBehaviour;

/**
 * @brief Handles Button draw
 */
static void UI_PanelNodeDraw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *image;
	vec2_t pos;

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);
}

typedef enum {
	LAYOUTALIGN_NONE = 0,

	/* common alignment */
	/** @note there is +1 because STAR layout do some fast computation */
	/** @note remove this +1 if possible, and use the struct auto numeration */
	LAYOUTALIGN_TOPLEFT = ALIGN_UL + 1,
	LAYOUTALIGN_TOP = ALIGN_UC + 1,
	LAYOUTALIGN_TOPRIGHT = ALIGN_UR + 1,
	LAYOUTALIGN_LEFT = ALIGN_CL + 1,
	LAYOUTALIGN_MIDDLE = ALIGN_CC + 1,
	LAYOUTALIGN_RIGHT = ALIGN_CR + 1,
	LAYOUTALIGN_BOTTOMLEFT = ALIGN_LL + 1,
	LAYOUTALIGN_BOTTOM = ALIGN_LC + 1,
	LAYOUTALIGN_BOTTOMRIGHT = ALIGN_LR + 1,

	/* pack and star layout manager only */
	LAYOUTALIGN_FILL,

	LAYOUTALIGN_MAX,
	LAYOUTALIGN_ENSURE_32BIT = 0x7FFFFFFF
} layoutAlign_t;

/**
 * @brief Create a top-down flow layout with child of the node.
 * Child position is automatically set, child height don't change
 * and child width is set according to node width and padding
 * @param[in,out] node The panel node to render the children for
 * @param[in] margin The margin between all children nodes in their y-position of the panel
 * @note test only
 */
static void UI_TopDownFlowLayout (uiNode_t *node, int margin)
{
	const int width = node->size[0] - node->padding - node->padding;
	int positionY = node->padding;
	uiNode_t *child = node->firstChild;
	vec2_t newSize = {width, 0};

	while (child) {
		newSize[1] = child->size[1];
		UI_NodeSetSize(child, newSize);
		child->pos[0] = node->padding;
		child->pos[1] = positionY;
		positionY += child->size[1] + margin;
		child = child->next;
	}
}

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
static void UI_BorderLayout (uiNode_t *node, int margin)
{
	uiNode_t *child;
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->size[1] - node->padding;

	/* top */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_TOP)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->size[1];
		UI_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = minY;
		minY += child->size[1] + margin;
	}

	/* bottom */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_BOTTOM)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->size[1];
		UI_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = maxY - child->size[1];
		maxY -= child->size[1] + margin;
	}

	/* left */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_LEFT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->size[0];
		newSize[1] = maxY - minY;
		UI_NodeSetSize(child, newSize);
		child->pos[0] = minX;
		child->pos[1] = minY;
		minX += child->size[0] + margin;
	}

	/* right */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_RIGHT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->size[0];
		newSize[1] = maxY - minY;
		UI_NodeSetSize(child, newSize);
		child->pos[0] = maxX - child->size[0];
		child->pos[1] = minY;
		maxX -= child->size[0] + margin;
	}

	/* middle */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_MIDDLE)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = maxY - minY;
		UI_NodeSetSize(child, newSize);
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
static void UI_PackLayout (uiNode_t *node, int margin)
{
	uiNode_t *child;
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->size[1] - node->padding;

	/* top */
	for (child = node->firstChild; child; child = child->next) {
		if (child->invis)
			continue;
		switch (child->align) {
		case LAYOUTALIGN_TOP:
			newSize[0] = maxX - minX;
			newSize[1] = child->size[1];
			UI_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = minY;
			minY += child->size[1] + margin;
			break;
		case LAYOUTALIGN_BOTTOM:
			newSize[0] = maxX - minX;
			newSize[1] = child->size[1];
			UI_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = maxY - child->size[1];
			maxY -= child->size[1] + margin;
			break;
		case LAYOUTALIGN_LEFT:
			newSize[0] = child->size[0];
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
			child->pos[0] = minX;
			child->pos[1] = minY;
			minX += child->size[0] + margin;
			break;
		case LAYOUTALIGN_RIGHT:
			newSize[0] = child->size[0];
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
			child->pos[0] = maxX - child->size[0];
			child->pos[1] = minY;
			maxX -= child->size[0] + margin;
			break;
		case LAYOUTALIGN_FILL:
			newSize[0] = maxX - minX;
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
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
void UI_StarLayout (uiNode_t *node)
{
	uiNode_t *child;
	for (child = node->firstChild; child; child = child->next) {
		vec2_t source;
		vec2_t destination;
		align_t align;

		if (child->align <= 0 || child->align > 10)
			continue;

		if (child->align == LAYOUTALIGN_FILL) {
			child->pos[0] = 0;
			child->pos[1] = 0;
			UI_NodeSetSize(child, node->size);
			child->behaviour->doLayout(child);
			continue;
		}

		align = starlayoutmap[child->align - 1];
		UI_NodeGetPoint(node, destination, align);
		UI_NodeRelativeToAbsolutePoint(node, destination);
		UI_NodeGetPoint(child, source, align);
		UI_NodeRelativeToAbsolutePoint(child, source);
		child->pos[0] += destination[0] - source[0];
		child->pos[1] += destination[1] - source[1];
	}
}

/**
 * Update the client zone
 */
static void UI_ClientLayout (uiNode_t *node)
{
	int width = 0;
	int height = 0;
	uiNode_t *child;
	qboolean updated;
	for (child = node->firstChild; child; child = child->next) {
		int value;
		value = child->pos[0] + child->size[0];
		if (value > width)
			width = value;
		value = child->pos[1] + child->size[1];
		if (value > height)
			height = value;
	}

	width += node->padding;
	height += node->padding;

	updated = UI_SetScroll(&EXTRADATA(node).super.scrollX, -1, node->size[0], width);
	updated = UI_SetScroll(&EXTRADATA(node).super.scrollY, -1, node->size[1], height) || updated;
	if (updated && EXTRADATA(node).super.onViewChange)
		UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
}

static void UI_PanelNodeDoLayout (uiNode_t *node)
{
	if (!node->invalidated)
		return;

	switch (EXTRADATA(node).layout) {
	case LAYOUT_NONE:
		break;
	case LAYOUT_TOP_DOWN_FLOW:
		UI_TopDownFlowLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_BORDER:
		UI_BorderLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_PACK:
		UI_PackLayout(node, EXTRADATA(node).layoutMargin);
		break;
	case LAYOUT_STAR:
		UI_StarLayout(node);
		break;
	case LAYOUT_CLIENT:
		UI_ClientLayout(node);
		break;
	default:
		Com_Printf("UI_PanelNodeDoLayout: layout '%d' unsupported.", EXTRADATA(node).layout);
	}

	localBehaviour->super->doLayout(node);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
static void UI_PanelNodeLoaded (uiNode_t *node)
{
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", UI_GetPath(node));
#endif
	if (EXTRADATA(node).layout != LAYOUT_NONE)
		UI_Invalidate(node);
}

static void UI_PanelNodeGetClientPosition (uiNode_t *node, vec2_t position)
{
	position[0] = -EXTRADATA(node).super.scrollX.viewPos;
	position[1] = -EXTRADATA(node).super.scrollY.viewPos;
}

/**
 * @brief Valid properties for a panel node
 */
static const value_t properties[] = {
	/**
	 * Select a layout manager to set position and size of child. Most of layout manager
	 * do not move or resize child without align property set. In the image, number identify
	 * the position of the child into node, and same color identify the same node. Text on child
	 * display the value of the "align" property of each child.
	 * <li>0: no layout manager. Child keep there position and there size.
	 * <li>LAYOUT_TOP_DOWN_FLOW: layout child from top to down. Only child height do not change.
	 * <li>LAYOUT_PACK: Pack one by one child into the available space of the node.
	 * <li>LAYOUT_BORDER: Align nodes at a know position. Its look like pack layout, but the order is not the same.
	 * top and bottom child first, then left and right, then middle. We can show the difference into the image.
	 * <li>LAYOUT_STAR: Align the corner of child into the corner of the node. Child size do not change.
	 * @image html http://ufoai.ninex.info/wiki/images/Layout.png
	 */
	{"layout", V_INT, UI_EXTRADATA_OFFSETOF(panelExtraData_t, layout), MEMBER_SIZEOF(panelExtraData_t, layout)},
	/**
	 * Margin use to layout children (margin between children)
	 */
	{"layoutMargin", V_INT, UI_EXTRADATA_OFFSETOF(panelExtraData_t, layoutMargin), MEMBER_SIZEOF(panelExtraData_t, layoutMargin)},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterPanelNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->extends = "abstractscrollable";
	behaviour->name = "panel";
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->draw = UI_PanelNodeDraw;
	behaviour->loaded = UI_PanelNodeLoaded;
	behaviour->doLayout = UI_PanelNodeDoLayout;
	behaviour->getClientPosition = UI_PanelNodeGetClientPosition;

	Com_RegisterConstInt("LAYOUTALIGN_TOPLEFT", LAYOUTALIGN_TOPLEFT);
	Com_RegisterConstInt("LAYOUTALIGN_TOP", LAYOUTALIGN_TOP);
	Com_RegisterConstInt("LAYOUTALIGN_TOPRIGHT", LAYOUTALIGN_TOPRIGHT);
	Com_RegisterConstInt("LAYOUTALIGN_LEFT", LAYOUTALIGN_LEFT);
	Com_RegisterConstInt("LAYOUTALIGN_MIDDLE", LAYOUTALIGN_MIDDLE);
	Com_RegisterConstInt("LAYOUTALIGN_RIGHT", LAYOUTALIGN_RIGHT);
	Com_RegisterConstInt("LAYOUTALIGN_BOTTOMLEFT", LAYOUTALIGN_BOTTOMLEFT);
	Com_RegisterConstInt("LAYOUTALIGN_BOTTOM", LAYOUTALIGN_BOTTOM);
	Com_RegisterConstInt("LAYOUTALIGN_BOTTOMRIGHT", LAYOUTALIGN_BOTTOMRIGHT);
	Com_RegisterConstInt("LAYOUTALIGN_FILL", LAYOUTALIGN_FILL);

	Com_RegisterConstInt("LAYOUT_TOP_DOWN_FLOW", LAYOUT_TOP_DOWN_FLOW);
	Com_RegisterConstInt("LAYOUT_BORDER", LAYOUT_BORDER);
	Com_RegisterConstInt("LAYOUT_PACK", LAYOUT_PACK);
	Com_RegisterConstInt("LAYOUT_STAR", LAYOUT_STAR);
	Com_RegisterConstInt("LAYOUT_CLIENT", LAYOUT_CLIENT);
}
