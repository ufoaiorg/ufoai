/**
 * @file ui_node_panel.c
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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_render.h"
#include "../ui_actions.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"
#include "../../../common/scripts.h"

#define EXTRADATA_TYPE panelExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

static const value_t *propertyLayoutMargin;
static const value_t *propertyLayoutColumns;
static const value_t *propertyPadding;

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

	/* fix scroll */
	{
		qboolean updated;

		updated = UI_SetScroll(&EXTRADATA(node).super.scrollX, -1, node->size[0], node->size[0]);
		updated = UI_SetScroll(&EXTRADATA(node).super.scrollY, -1, node->size[1], positionY + node->padding) || updated;
		if (updated && EXTRADATA(node).super.onViewChange)
			UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
	}

}

/**
 * @brief Create a left-right flow layout with child of the node.
 * Child position is automatically set, child width don't change
 * and child height is set according to node width and padding
 * @param[in,out] node The panel node to render the children for
 * @param[in] margin The margin between all children nodes in their x-position of the panel
 */
static void UI_LeftRightFlowLayout (uiNode_t *node, int margin)
{
	const int height = node->size[1] - node->padding - node->padding;
	int positionX = node->padding;
	uiNode_t *child = node->firstChild;
	vec2_t newSize = {0, height};

	while (child) {
		newSize[0] = child->size[0];
		UI_NodeSetSize(child, newSize);
		child->pos[0] = positionX;
		child->pos[1] = node->padding;
		positionX += child->size[0] + margin;
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

		if (child->align <= LAYOUTALIGN_NONE)
			continue;

		if (child->align == LAYOUTALIGN_FILL) {
			child->pos[0] = 0;
			child->pos[1] = 0;
			UI_NodeSetSize(child, node->size);
			child->behaviour->doLayout(child);
		} else if (child->align < LAYOUTALIGN_SPECIAL) {
			UI_NodeGetPoint(node, destination, child->align);
			UI_NodeRelativeToAbsolutePoint(node, destination);
			UI_NodeGetPoint(child, source, child->align);
			UI_NodeRelativeToAbsolutePoint(child, source);
			child->pos[0] += destination[0] - source[0];
			child->pos[1] += destination[1] - source[1];
		}
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

/**
 * @brief Do column layout. A grid layout according to a fixed number of column.
 * Check to first row to see the needed size of columns, and the height. All rows
 * will use the same row.
 * @todo Use child\@align to align each nodes inside respective cell.
 * @image html http://ufoai.org/wiki/images/Layout_column.png
 */
static void UI_ColumnLayout (uiNode_t *node)
{
	int columnPos[EXTRADATA(node).layoutColumns];
	int columnSize[EXTRADATA(node).layoutColumns];
	int rowHeight = 0;
	int i;
	int y;
	uiNode_t *child;

	if (EXTRADATA(node).layoutColumns <= 0) {
		Com_Printf("UI_ColumnLayout: Column number must be positive (%s). Layout ignored.", UI_GetPath(node));
		return;
	}

	/* check the first row */
	child = node->firstChild;
	for (i = 0; i < EXTRADATA(node).layoutColumns; i++) {
		if (child == NULL)
			break;
		columnSize[i] = child->size[0];
		if (child->size[1] > rowHeight) {
			rowHeight = child->size[1];
		}
		child = child->next;
	}

	/* compute column position */
	columnPos[0] = node->padding;
	for (i = 1; i < EXTRADATA(node).layoutColumns; i++) {
		columnPos[i] = columnPos[i - 1] + columnSize[i - 1] + EXTRADATA(node).layoutMargin;
	}

	/* fix child position */
	i = 0;
	y = -1;
	for (child = node->firstChild; child; child = child->next) {
		const int column = i % EXTRADATA(node).layoutColumns;
		if (column == 0) {
			if (y < 0) {
				y = node->padding;
			} else {
				y += rowHeight + EXTRADATA(node).layoutMargin;
			}
		}
		child->pos[0] = columnPos[column];
		child->pos[1] = y;
		/*UI_NodeSetSize(child, node->size);*/
		child->behaviour->doLayout(child);
		i++;
	}

	/* fix scroll */
	{
		const int column = EXTRADATA(node).layoutColumns;
		int width = columnPos[column - 1] + columnSize[column - 1] + node->padding;
		int height = y + rowHeight + node->padding;
		qboolean updated;

		updated = UI_SetScroll(&EXTRADATA(node).super.scrollX, -1, node->size[0], width);
		updated = UI_SetScroll(&EXTRADATA(node).super.scrollY, -1, node->size[1], height) || updated;
		if (updated && EXTRADATA(node).super.onViewChange)
			UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
	}
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
	case LAYOUT_LEFT_RIGHT_FLOW:
		UI_LeftRightFlowLayout(node, EXTRADATA(node).layoutMargin);
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
	case LAYOUT_COLUMN:
		UI_ColumnLayout(node);
		break;
	default:
		Com_Printf("UI_PanelNodeDoLayout: layout '%d' unsupported.", EXTRADATA(node).layout);
		break;
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

static void UI_PanelNodeGetClientPosition (const uiNode_t *node, vec2_t position)
{
	position[0] = -EXTRADATACONST(node).super.scrollX.viewPos;
	position[1] = -EXTRADATACONST(node).super.scrollY.viewPos;
}

static void UI_PanelPropertyChanged (uiNode_t *node, const value_t *property)
{
	/** @todo move it to registration code when it is possible */
	if (propertyPadding == NULL) {
		propertyPadding = UI_GetPropertyFromBehaviour(node->behaviour, "padding");
	}

	if (property == propertyLayoutColumns ||property == propertyLayoutMargin || property == propertyPadding) {
		UI_Invalidate(node);
		return;
	}
	localBehaviour->super->propertyChanged(node, property);
}

/**
 * @brief Handle mouse wheel scrolling
 * @param[in, out] node UI node to scroll
 * @param[in] deltaX horizontal scrolling value (not used)
 * @param[in] deltaX vertical scrolling value
 */
static qboolean UI_PanelNodeMouseWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	qboolean updated;

	/* @todo remove wheelScrollable after 2.4 release */
	if (!EXTRADATA(node).wheelScrollable || deltaY == 0)
		return qfalse;

	updated = UI_SetScroll(&EXTRADATA(node).super.scrollY, EXTRADATA(node).super.scrollY.viewPos + deltaY * 50, -1, -1);
	if (EXTRADATA(node).super.onViewChange && updated)
		UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);

	/* @todo use super behaviour */
	if (node->onWheelUp && !down) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		updated = qtrue;
	}
	if (node->onWheelDown && down) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		updated = qtrue;
	}
	if (node->onWheel) {
		UI_ExecuteEventActions(node, node->onWheel);
		updated = qtrue;
	}
	return updated;
}

void UI_RegisterPanelNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->extends = "abstractscrollable";
	behaviour->name = "panel";
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->draw = UI_PanelNodeDraw;
	behaviour->loaded = UI_PanelNodeLoaded;
	behaviour->doLayout = UI_PanelNodeDoLayout;
	behaviour->getClientPosition = UI_PanelNodeGetClientPosition;
	behaviour->propertyChanged = UI_PanelPropertyChanged;
	behaviour->scroll = UI_PanelNodeMouseWheel;

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
	 * @image html http://ufoai.org/wiki/images/Layout.png
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "layout", V_INT, panelExtraData_t, layout);
	/**
	 * Margin use to layout children (margin between children)
	 */
	propertyLayoutMargin = UI_RegisterExtradataNodeProperty(behaviour, "layoutMargin", V_INT, panelExtraData_t, layoutMargin);
	/**
	 * Number of column use to layout children (used with LAYOUT_COLUMN)
	 */
	propertyLayoutColumns = UI_RegisterExtradataNodeProperty(behaviour, "layoutColumns", V_INT, panelExtraData_t, layoutColumns);
	/**
	 * If scrolling via mousewheel is enabled
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "wheelscrollable", V_BOOL, panelExtraData_t, wheelScrollable);

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
	Com_RegisterConstInt("LAYOUT_LEFT_RIGHT_FLOW", LAYOUT_LEFT_RIGHT_FLOW);
	Com_RegisterConstInt("LAYOUT_BORDER", LAYOUT_BORDER);
	Com_RegisterConstInt("LAYOUT_PACK", LAYOUT_PACK);
	Com_RegisterConstInt("LAYOUT_STAR", LAYOUT_STAR);
	Com_RegisterConstInt("LAYOUT_CLIENT", LAYOUT_CLIENT);
	Com_RegisterConstInt("LAYOUT_COLUMN", LAYOUT_COLUMN);
}
