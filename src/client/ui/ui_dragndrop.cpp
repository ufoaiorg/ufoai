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

#include "ui_internal.h"
#include "ui_dragndrop.h"
#include "ui_input.h"
#include "ui_node.h"
#include "ui_sound.h"

#include "node/ui_node_abstractnode.h"
#include "node/ui_node_container.h"

#include "../input/cl_input.h"

static int oldMousePosX = -1;				/**< Save position X of the mouse to know when it move */
static int oldMousePosY = -1;				/**< Save position Y of the mouse to know when it move */

static bool nodeAcceptDND = false;		/**< Save if the current target node can accept the DND object */
static bool positionAcceptDND = false;	/**< Save if the current position accept the DND object */

static uiDNDType_t objectType;				/**< Save the type of the object we are dragging */
static Item draggingItem;					/**< Save the dragging object */

static uiNode_t* sourceNode;				/**< Node where come from the DND object */
static uiNode_t* targetNode;				/**< Current node under the mouse */


/**
 * @brief Return true if we are dragging something
 */
bool UI_DNDIsDragging (void)
{
	return objectType != DND_NOTHING;
}

/**
 * @brief Return true if the requested node is the current target of the DND
 */
bool UI_DNDIsTargetNode (uiNode_t* node)
{
	if (!UI_DNDIsDragging())
		return false;
	return targetNode == node;
}

/**
 * @brief Return true if the requested node is the source of the DND
 */
bool UI_DNDIsSourceNode (uiNode_t* node)
{
	if (!UI_DNDIsDragging())
		return false;
	return sourceNode == node;
}

/**
 * @brief Return the current type of the dragging object, else DND_NOTHING
 */
int UI_DNDGetType (void)
{
	return objectType;
}

/**
 * @brief Return target of the DND
 */
uiNode_t* UI_DNDGetTargetNode (void)
{
	assert(UI_DNDIsDragging());
	return targetNode;
}

/**
 * @brief Return source of the DND
 */
uiNode_t* UI_DNDGetSourceNode (void)
{
	assert(UI_DNDIsDragging());
	return sourceNode;
}

/**
 * @brief Private function to initialize a the start of a DND
 * @sa UI_DNDDragItem
 * @sa UI_DNDDrop
 * @sa UI_DNDAbort
 */
static void UI_DNDDrag (uiNode_t* node)
{
	assert(!UI_DNDIsDragging());
	objectType = DND_SOMETHING;
	sourceNode = node;

	UI_PlaySound("item-drag");
}

/**
 * @brief Start to drag an item
 * @sa UI_DNDDrag
 * @sa UI_DNDDrop
 * @sa UI_DNDAbort
 */
void UI_DNDDragItem (uiNode_t* node, const Item* item)
{
	UI_DNDDrag(node);
	assert(UI_DNDIsDragging());
	objectType = DND_ITEM;
	draggingItem = *item;
}

/**
 * @brief Cleanup data about DND
 */
static inline void UI_DNDCleanup (void)
{
	objectType = DND_NOTHING;
	targetNode = nullptr;
	sourceNode = nullptr;
}

/**
 * @brief Drop the object at the current position
 * @sa UI_DNDStartDrag
 * @sa UI_DNDDrop
 */
void UI_DNDAbort (void)
{
	assert(UI_DNDIsDragging());
	assert(objectType != DND_NOTHING);
	assert(sourceNode != nullptr);

	if (nodeAcceptDND && targetNode) {
		UI_Node_DndLeave(targetNode);
	}
	UI_Node_DndFinished(sourceNode, false);

	UI_DNDCleanup();
	UI_InvalidateMouse();
}

/**
 * @brief Drop the object at the current position
 * @sa UI_DNDStartDrag
 * @sa UI_DNDAbort
 */
void UI_DNDDrop (void)
{
	bool result = false;
	assert(UI_DNDIsDragging());
	assert(objectType != DND_NOTHING);
	assert(sourceNode != nullptr);

	if (!positionAcceptDND) {
		UI_DNDAbort();
		return;
	}

	if (targetNode) {
		result = UI_Node_DndDrop(targetNode, mousePosX, mousePosY);
	}
	UI_Node_DndFinished(sourceNode, result);

	UI_PlaySound("item-drop");

	UI_DNDCleanup();
	UI_InvalidateMouse();
}

Item* UI_DNDGetItem (void)
{
	assert(objectType == DND_ITEM);
	return &draggingItem;
}

/**
 * @brief Manage the DND when we move the mouse
 */
static void UI_DNDMouseMove (int mousePosX, int mousePosY)
{
	uiNode_t* node = UI_GetNodeAtPosition(mousePosX, mousePosY);

	if (node != targetNode) {
		if (nodeAcceptDND && targetNode) {
			UI_Node_DndLeave(targetNode);
		}
		targetNode = node;
		if (targetNode) {
			nodeAcceptDND = UI_Node_DndEnter(targetNode);
		}
	}

	if (targetNode == nullptr) {
		nodeAcceptDND = false;
		positionAcceptDND = false;
		return;
	}

	if (!nodeAcceptDND) {
		positionAcceptDND = false;
		return;
	}

	positionAcceptDND = UI_Node_DndMove(targetNode, mousePosX, mousePosY);
}

/**
 * @brief Draw to dragging object and catch mouse move event
 */
void UI_DrawDragAndDrop (int mousePosX, int mousePosY)
{
	const vec3_t scale = { 3.5, 3.5, 3.5 };
	vec3_t orgine;
	vec4_t color = { 1, 1, 1, 1 };

	/* check mouse move */
	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		UI_DNDMouseMove(mousePosX, mousePosY);
	}

	/* draw the dragging item */

	VectorSet(orgine, mousePosX, mousePosY, -50);

	/* Tune down the opacity of the cursor-item if the preview item is drawn. */
	if (positionAcceptDND)
		color[3] = 0.2;

	switch (objectType) {
	case DND_ITEM:
		UI_DrawItem(nullptr, orgine, &draggingItem, -1, -1, scale, color);
		break;

	default:
		assert(false);
	}
}
