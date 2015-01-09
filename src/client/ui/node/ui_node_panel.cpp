/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../ui_input.h"
#include "../ui_sprite.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"
#include "../../../common/scripts.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE panelExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t* propertyLayoutMargin;
static const value_t* propertyLayoutColumns;
static const value_t* propertyPadding;

static const uiBehaviour_t* localBehaviour;

/**
 * @brief Handles Button draw
 */
void uiPanelNode::draw (uiNode_t* node)
{
	vec2_t pos;

	UI_GetNodeAbsPos(node, pos);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, SPRITE_STATUS_NORMAL, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}
}

/**
 * @brief Create a top-down flow layout with child of the node.
 * Child position is automatically set, child height don't change
 * and child width is set according to node width and padding
 * @param[in,out] node The panel node to render the children for
 * @param[in] margin The margin between all children nodes in their y-position of the panel
 * @note test only
 */
static void UI_TopDownFlowLayout (uiNode_t* node, int margin)
{
	const int width = node->box.size[0] - node->padding - node->padding;
	int positionY = node->padding;
	uiNode_t* child = node->firstChild;
	vec2_t newSize = Vector2FromInt(width, 0);

	while (child) {
		if (!UI_Node_IsDrawable(child)) {
			child = child->next;
			continue;
		}
		newSize[1] = child->box.size[1];
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = node->padding;
		child->box.pos[1] = positionY;
		positionY += child->box.size[1] + margin;
		child = child->next;
	}

	/* fix scroll */
	{
		bool updated;

		updated = EXTRADATA(node).super.scrollX.set(-1, node->box.size[0], node->box.size[0]);
		updated = EXTRADATA(node).super.scrollY.set(-1, node->box.size[1], positionY + node->padding) || updated;
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
static void UI_LeftRightFlowLayout (uiNode_t* node, int margin)
{
	const int height = node->box.size[1] - node->padding - node->padding;
	int positionX = node->padding;
	uiNode_t* child = node->firstChild;
	vec2_t newSize = Vector2FromInt(0, height);

	while (child) {
		if (!UI_Node_IsDrawable(child)) {
			child = child->next;
			continue;
		}
		newSize[0] = child->box.size[0];
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = positionX;
		child->box.pos[1] = node->padding;
		positionX += child->box.size[0] + margin;
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
static void UI_BorderLayout (uiNode_t* node, int margin)
{
	uiNode_t* child;
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->box.size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->box.size[1] - node->padding;

	/* top */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_TOP)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->box.size[1];
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = minX;
		child->box.pos[1] = minY;
		minY += child->box.size[1] + margin;
	}

	/* bottom */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_BOTTOM)
			continue;
		if (child->invis)
			continue;
		newSize[0] = maxX - minX;
		newSize[1] = child->box.size[1];
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = minX;
		child->box.pos[1] = maxY - child->box.size[1];
		maxY -= child->box.size[1] + margin;
	}

	/* left */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_LEFT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->box.size[0];
		newSize[1] = maxY - minY;
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = minX;
		child->box.pos[1] = minY;
		minX += child->box.size[0] + margin;
	}

	/* right */
	for (child = node->firstChild; child; child = child->next) {
		if (child->align != LAYOUTALIGN_RIGHT)
			continue;
		if (child->invis)
			continue;
		newSize[0] = child->box.size[0];
		newSize[1] = maxY - minY;
		UI_NodeSetSize(child, newSize);
		child->box.pos[0] = maxX - child->box.size[0];
		child->box.pos[1] = minY;
		maxX -= child->box.size[0] + margin;
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
		child->box.pos[0] = minX;
		child->box.pos[1] = minY;
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
static void UI_PackLayout (uiNode_t* node, int margin)
{
	vec2_t newSize;
	int minX = node->padding;
	int maxX = node->box.size[0] - node->padding;
	int minY = node->padding;
	int maxY = node->box.size[1] - node->padding;

	/* top */
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		if (child->invis)
			continue;
		switch (child->align) {
		case LAYOUTALIGN_TOP:
			newSize[0] = maxX - minX;
			newSize[1] = child->box.size[1];
			UI_NodeSetSize(child, newSize);
			child->box.pos[0] = minX;
			child->box.pos[1] = minY;
			minY += child->box.size[1] + margin;
			break;
		case LAYOUTALIGN_BOTTOM:
			newSize[0] = maxX - minX;
			newSize[1] = child->box.size[1];
			UI_NodeSetSize(child, newSize);
			child->box.pos[0] = minX;
			child->box.pos[1] = maxY - child->box.size[1];
			maxY -= child->box.size[1] + margin;
			break;
		case LAYOUTALIGN_LEFT:
			newSize[0] = child->box.size[0];
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
			child->box.pos[0] = minX;
			child->box.pos[1] = minY;
			minX += child->box.size[0] + margin;
			break;
		case LAYOUTALIGN_RIGHT:
			newSize[0] = child->box.size[0];
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
			child->box.pos[0] = maxX - child->box.size[0];
			child->box.pos[1] = minY;
			maxX -= child->box.size[0] + margin;
			break;
		case LAYOUTALIGN_FILL:
			newSize[0] = maxX - minX;
			newSize[1] = maxY - minY;
			UI_NodeSetSize(child, newSize);
			child->box.pos[0] = minX;
			child->box.pos[1] = minY;
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
void UI_StarLayout (uiNode_t* node)
{
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		if (child->align <= LAYOUTALIGN_NONE)
			continue;

		if (child->align == LAYOUTALIGN_FILL) {
			child->box.pos[0] = 0;
			child->box.pos[1] = 0;
			UI_NodeSetSize(child, node->box.size);
			UI_Node_DoLayout(child);
		} else if (child->align < LAYOUTALIGN_SPECIAL) {
			vec2_t source;
			vec2_t destination;

			UI_NodeGetPoint(node, destination, child->align);
			UI_NodeRelativeToAbsolutePoint(node, destination);
			UI_NodeGetPoint(child, source, child->align);
			UI_NodeRelativeToAbsolutePoint(child, source);
			child->box.pos[0] += destination[0] - source[0];
			child->box.pos[1] += destination[1] - source[1];
		}
	}
}

/**
 * Update the client zone
 */
static void UI_ClientLayout (uiNode_t* node)
{
	int width = 0;
	int height = 0;
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		int value;
		value = child->box.pos[0] + child->box.size[0];
		if (value > width)
			width = value;
		value = child->box.pos[1] + child->box.size[1];
		if (value > height)
			height = value;
	}

	width += node->padding;
	height += node->padding;

	bool updated = EXTRADATA(node).super.scrollX.set(-1, node->box.size[0], width);
	updated = EXTRADATA(node).super.scrollY.set(-1, node->box.size[1], height) || updated;
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
static void UI_ColumnLayout (uiNode_t* node)
{
	int rowHeight = 0;
	int i;
	int y;
	uiNode_t* child;

	if (EXTRADATA(node).layoutColumns <= 0) {
		Com_Printf("UI_ColumnLayout: Column number must be positive (%s). Layout ignored.", UI_GetPath(node));
		return;
	}

	int* columnPos = Mem_AllocTypeN(int, EXTRADATA(node).layoutColumns);
	int* columnSize = Mem_AllocTypeN(int, EXTRADATA(node).layoutColumns);

	/* check the first row */
	child = node->firstChild;
	for (i = 0; i < EXTRADATA(node).layoutColumns; i++) {
		if (child == nullptr)
			break;
		columnSize[i] = child->box.size[0];
		if (child->box.size[1] > rowHeight) {
			rowHeight = child->box.size[1];
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
		child->box.pos[0] = columnPos[column];
		child->box.pos[1] = y;
		/*UI_NodeSetSize(child, node->box.size);*/
		UI_Node_DoLayout(child);
		i++;
	}

	/* fix scroll */
	{
		const int column = EXTRADATA(node).layoutColumns;
		int width = columnPos[column - 1] + columnSize[column - 1] + node->padding;
		int height = y + rowHeight + node->padding;
		bool updated;

		updated = EXTRADATA(node).super.scrollX.set(-1, node->box.size[0], width);
		updated = EXTRADATA(node).super.scrollY.set(-1, node->box.size[1], height) || updated;
		if (updated && EXTRADATA(node).super.onViewChange)
			UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);
	}

	Mem_Free(columnPos);
	Mem_Free(columnSize);
}

void uiPanelNode::doLayout (uiNode_t* node)
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

	uiAbstractScrollableNode::doLayout(node);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
void uiPanelNode::onLoading (uiNode_t* node)
{
	uiLocatedNode::onLoading(node);
	EXTRADATA(node).wheelScrollable = true;
}

/* Used for drag&drop-like scrolling */
static int mouseScrollX;
static int mouseScrollY;

bool uiPanelNode::onMouseLongPress(uiNode_t* node, int x, int y, int button)
{
	bool hasSomethingToScroll = EXTRADATA(node).super.scrollX.fullSize > EXTRADATA(node).super.scrollX.viewSize
			|| EXTRADATA(node).super.scrollY.fullSize > EXTRADATA(node).super.scrollY.viewSize;
	if (!UI_GetMouseCapture() && button == K_MOUSE1 && hasSomethingToScroll) {
		UI_SetMouseCapture(node);
		mouseScrollX = x;
		mouseScrollY = y;
		return true;
	}
	return false;
}

bool uiPanelNode::onStartDragging(uiNode_t* node, int startX, int startY, int x, int y, int button)
{
	return onMouseLongPress(node, startX, startY, button);
}

void uiPanelNode::onMouseUp (uiNode_t* node, int x, int y, int button)
{
	if (UI_GetMouseCapture() == node)  /* More checks can never hurt */
		UI_MouseRelease();
}

void uiPanelNode::onCapturedMouseMove (uiNode_t* node, int x, int y)
{
	/** @todo do it as well for x */
	if (mouseScrollY != y) {
		scrollY(node, mouseScrollY - y);
		mouseScrollX = x;
		mouseScrollY = y;
	}
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
void uiPanelNode::onLoaded (uiNode_t* node)
{
	if (EXTRADATA(node).layout != LAYOUT_NONE)
		UI_Invalidate(node);
}

void uiPanelNode::getClientPosition (const uiNode_t* node, vec2_t position)
{
	position[0] = -EXTRADATACONST(node).super.scrollX.viewPos;
	position[1] = -EXTRADATACONST(node).super.scrollY.viewPos;
}

void uiPanelNode::onPropertyChanged (uiNode_t* node, const value_t* property)
{
	/** @todo move it to registration code when it is possible */
	if (propertyPadding == nullptr) {
		propertyPadding = UI_GetPropertyFromBehaviour(node->behaviour, "padding");
	}

	if (property == propertyLayoutColumns ||property == propertyLayoutMargin || property == propertyPadding) {
		UI_Invalidate(node);
		return;
	}
	uiAbstractScrollableNode::onPropertyChanged(node, property);
}

/**
 * @brief Handle mouse wheel scrolling
 * @param[in, out] node UI node to scroll
 * @param[in] deltaX horizontal scrolling value (not used)
 * @param[in] deltaY vertical scrolling value
 */
bool uiPanelNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	bool updated;

	/* @todo remove wheelScrollable after 2.4 release */
	if (!EXTRADATA(node).wheelScrollable || deltaY == 0)
		return false;

	updated = EXTRADATA(node).super.scrollX.moveDelta(deltaX * 50);
	updated |= EXTRADATA(node).super.scrollY.moveDelta(deltaY * 50);
	if (EXTRADATA(node).super.onViewChange && updated)
		UI_ExecuteEventActions(node, EXTRADATA(node).super.onViewChange);

	/* @todo use super behaviour */
	if (node->onWheelUp && deltaY < 0) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		updated = true;
	}
	if (node->onWheelDown && deltaY > 0) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		updated = true;
	}
	if (node->onWheel) {
		UI_ExecuteEventActions(node, node->onWheel);
		updated = true;
	}
	return updated;
}

void UI_RegisterPanelNode (uiBehaviour_t* behaviour)
{
	localBehaviour = behaviour;
	behaviour->extends = "abstractscrollable";
	behaviour->name = "panel";
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->manager = UINodePtr(new uiPanelNode());

	/**
	 * Select a layout manager to set position and size of child. Most of layout manager
	 * do not move or resize child without align property set. In the image, number identify
	 * the position of the child into node, and same color identify the same node. Text on child
	 * display the value of the "align" property of each child.
	 * - 0: no layout manager. Child keep there position and there size.
	 * - LAYOUT_TOP_DOWN_FLOW: layout child from top to down. Only child height do not change.
	 * - LAYOUT_PACK: Pack one by one child into the available space of the node.
	 * - LAYOUT_BORDER: Align nodes at a know position. Its look like pack layout, but the order is not the same.
	 *   top and bottom child first, then left and right, then middle. We can show the difference into the image.
	 * - LAYOUT_STAR: Align the corner of child into the corner of the node. Child size do not change.
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

	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);

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
