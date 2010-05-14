/**
 * @file m_dragndrop.c
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

#include "m_dragndrop.h"
#include "m_input.h"
#include "node/m_node_abstractnode.h"

#include "../input/cl_input.h"

static int oldMousePosX = -1;				/**< Save position X of the mouse to know when it move */
static int oldMousePosY = -1;				/**< Save position Y of the mouse to know when it move */

static qboolean nodeAcceptDND = qfalse;		/**< Save if the current target node can accept the DND object */
static qboolean positionAcceptDND = qfalse;	/**< Save if the current position accept the DND object */

static dndType_t objectType;				/**< Save the type of the object we are dragging */
static item_t draggingItem;					/**< Save the dragging object */

static menuNode_t *sourceNode;				/**< Node where come from the DND object */
static menuNode_t *targetNode;				/**< Current node under the mouse */


/**
 * @brief Return true if we are dragging something
 */
qboolean MN_DNDIsDragging (void)
{
	return objectType != DND_NOTHING;
}

/**
 * @brief Return true if the requested node is the current target of the DND
 */
qboolean MN_DNDIsTargetNode (struct menuNode_s *node)
{
	if (!MN_DNDIsDragging())
		return qfalse;
	return targetNode == node;
}

/**
 * @brief Return true if the requested node is the source of the DND
 */
qboolean MN_DNDIsSourceNode (struct menuNode_s *node)
{
	if (!MN_DNDIsDragging())
		return qfalse;
	return sourceNode == node;
}

/**
 * @brief Return the current type of the dragging object, else DND_NOTHING
 */
int MN_DNDGetType (void)
{
	return objectType;
}

/**
 * @brief Return target of the DND
 */
menuNode_t *MN_DNDGetTargetNode (void)
{
	assert(MN_DNDIsDragging());
	return targetNode;
}

/**
 * @brief Return source of the DND
 */
menuNode_t *MN_DNDGetSourceNode (void)
{
	assert(MN_DNDIsDragging());
	return sourceNode;
}

/**
 * @brief Private function to initialize a the start of a DND
 * @sa MN_DNDDragItem
 * @sa MN_DNDDrop
 * @sa MN_DNDAbort
 */
static void MN_DNDDrag (menuNode_t *node)
{
	assert(!MN_DNDIsDragging());
	objectType = DND_SOMETHING;
	sourceNode = node;
}

/**
 * @brief Start to drag an item
 * @sa MN_DNDDrag
 * @sa MN_DNDDrop
 * @sa MN_DNDAbort
 */
void MN_DNDDragItem (menuNode_t *node, const item_t *item)
{
	MN_DNDDrag(node);
	assert(MN_DNDIsDragging());
	objectType = DND_ITEM;
	draggingItem = *item;
}

/**
 * @brief Cleanup data about DND
 */
static inline void MN_DNDCleanup (void)
{
	objectType = DND_NOTHING;
	targetNode = NULL;
	sourceNode = NULL;
}

/**
 * @brief Drop the object at the current position
 * @sa MN_DNDStartDrag
 * @sa MN_DNDDrop
 */
void MN_DNDAbort (void)
{
	assert(MN_DNDIsDragging());
	assert(objectType != DND_NOTHING);
	assert(sourceNode != NULL);

	if (nodeAcceptDND && targetNode) {
		targetNode->behaviour->dndLeave(targetNode);
	}
	sourceNode->behaviour->dndFinished(sourceNode, qfalse);

	MN_DNDCleanup();
	MN_InvalidateMouse();
}

/**
 * @brief Drop the object at the current position
 * @sa MN_DNDStartDrag
 * @sa MN_DNDAbort
 */
void MN_DNDDrop (void)
{
	qboolean result = qfalse;
	assert(MN_DNDIsDragging());
	assert(objectType != DND_NOTHING);
	assert(sourceNode != NULL);

	if (!positionAcceptDND) {
		MN_DNDAbort();
		return;
	}

	if (targetNode) {
		result = targetNode->behaviour->dndDrop(targetNode, mousePosX, mousePosY);
	}
	sourceNode->behaviour->dndFinished(sourceNode, result);

	MN_DNDCleanup();
	MN_InvalidateMouse();
}

item_t *MN_DNDGetItem (void)
{
	assert(objectType == DND_ITEM);
	return &draggingItem;
}

/**
 * @brief Manage the DND when we move the mouse
 */
static void MN_DNDMouseMove (int mousePosX, int mousePosY)
{
	menuNode_t *node = MN_GetNodeAtPosition(mousePosX, mousePosY);

	if (node != targetNode) {
		if (nodeAcceptDND && targetNode) {
			targetNode->behaviour->dndLeave(targetNode);
		}
		targetNode = node;
		if (targetNode) {
			nodeAcceptDND = targetNode->behaviour->dndEnter(targetNode);
		}
	}

	if (targetNode == NULL) {
		nodeAcceptDND = qfalse;
		positionAcceptDND = qfalse;
		return;
	}

	if (!nodeAcceptDND) {
		positionAcceptDND = qfalse;
		return;
	}

	positionAcceptDND = node->behaviour->dndMove(targetNode, mousePosX, mousePosY);
}


/**
 * @brief Draw to dragging object and catch mouse move event
 */
void MN_DrawDragAndDrop (int mousePosX, int mousePosY)
{
	const vec3_t scale = { 3.5, 3.5, 3.5 };
	vec3_t orgine;
	vec4_t color = { 1, 1, 1, 1 };

	/* check mouse move */
	if (mousePosX != oldMousePosX || mousePosY != oldMousePosY) {
		oldMousePosX = mousePosX;
		oldMousePosY = mousePosY;
		MN_DNDMouseMove(mousePosX, mousePosY);
	}

	/* draw the dragging item */

	VectorSet(orgine, mousePosX, mousePosY, -50);

	/* Tune down the opacity of the cursor-item if the preview item is drawn. */
	if (positionAcceptDND)
		color[3] = 0.2;

	switch (objectType) {
	case DND_ITEM:
		MN_DrawItem(NULL, orgine, &draggingItem, -1, -1, scale, color);
		break;

	default:
		assert(qfalse);
	}
}
