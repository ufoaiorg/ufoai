/**
 * @file m_dragndrop.c
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

#include "../client.h"
#include "m_dragndrop.h"
#include "m_input.h"
#include "node/m_node_container.h"
#include "node/m_node_abstractnode.h"

/**
 * @todo is it really needed to init this structure?
 */
dragInfo_t dragInfo = {
	DND_NOTHING,
	NULL,
	0,
	0,

	NULL,	/* toNode */

	qfalse,

	/* bellow that, nothing is generic */

	{NONE_AMMO, NULL, NULL, 1, 0},	/* item */
	NULL,	/* ic */
	NULL,	/* from */
	-1,		/* fromX */
	-1,		/* fromY */

	NULL,	/* to */
	-1,		/* toX */
	-1		/* toY */
};

/**
 * @brief Retrun true if we are dragging something
 */
qboolean MN_DNDIsDragging (void)
{
	return mouseSpace == MS_DRAGITEM && dragInfo.type != DND_NOTHING;
}

/**
 * @brief Retrun true if the requested node is the current target of the DND
 */
qboolean MN_DNDIsTargetNode (struct menuNode_s *node)
{
	if (mouseSpace != MS_DRAGITEM)
		return qfalse;
	return dragInfo.targetNode == node;
}

/**
 * @brief Start a drag and drop
 * @sa MN_DNDDragItem
 * @sa MN_DNDDrop
 * @sa MN_DNDAbort
 */
void MN_DNDStartDrag (menuNode_t *node)
{
	assert(mouseSpace != MS_DRAGITEM);
	mouseSpace = MS_DRAGITEM;
	dragInfo.sourceNode = node;
	dragInfo.sourceAbsoluteX = mousePosX;
	dragInfo.sourceAbsoluteY = mousePosY;
}

/**
 * @brief Cleanup data about DND
 */
static inline void MN_DNDCleanup (void)
{
	dragInfo.type = DND_NOTHING;
	dragInfo.targetNode = NULL;
	dragInfo.sourceNode = NULL;
}

/**
 * @brief Drop the object at the current position
 * @sa MN_DNDStartDrag
 * @sa MN_DNDDrop
 */
void MN_DNDAbort (void)
{
	assert(mouseSpace == MS_DRAGITEM);
	assert(dragInfo.type != DND_NOTHING);
	assert(dragInfo.sourceNode != NULL);

	if (dragInfo.targetNode) {
		dragInfo.targetNode->behaviour->dndLeave(dragInfo.targetNode);
	}
	dragInfo.sourceNode->behaviour->dndFinished(dragInfo.sourceNode, qfalse);

	MN_DNDCleanup();
	mouseSpace = MS_NULL;
}

/**
 * @brief Drop the object at the current position
 * @sa MN_DNDStartDrag
 * @sa MN_DNDAbort
 */
void MN_DNDDrop (void)
{
	qboolean result = qfalse;
	assert(mouseSpace == MS_DRAGITEM);
	assert(dragInfo.type != DND_NOTHING);
	assert(dragInfo.sourceNode != NULL);

	if (dragInfo.targetNode) {
		result = dragInfo.targetNode->behaviour->dndDrop(dragInfo.targetNode, mousePosX, mousePosY);
	}
	dragInfo.sourceNode->behaviour->dndFinished(dragInfo.sourceNode, result);

	MN_DNDCleanup();
	mouseSpace = MS_NULL;
}

/**
 * @brief Intitialize the dnd provide the object to drag
 */
void MN_DNDDragItem (item_t *item, invList_t *fromInventory)
{
	assert(mouseSpace == MS_DRAGITEM);
	assert(dragInfo.type == DND_NOTHING);
	dragInfo.type = DND_ITEM;
	dragInfo.item = *item;
	dragInfo.ic = fromInventory;
}

/**
 * @brief Provide extradata to the current draging object
 */
void MN_DNDFromContainer (invDef_t *container, int ownFromX, int ownFromY)
{
	assert(mouseSpace == MS_DRAGITEM);
	dragInfo.from = container;
	/* Store grid-position (in the container) of the mouse. */
	dragInfo.fromX = ownFromX;
	dragInfo.fromY = ownFromY;
}


static int oldMousePosX = -1;
static int oldMousePosY = -1;
static qboolean nodeAcceptDND = qfalse;
static qboolean positionAcceptDND = qfalse;

/**
 * @brief Manage the DND when we move the mouse
 */
static void MN_DNDMouseMove (int mousePosX, int mousePosY)
{
	menuNode_t *node = MN_GetNodeByPosition(mousePosX, mousePosY);

	if (node != dragInfo.targetNode) {
		if (dragInfo.targetNode) {
			dragInfo.targetNode->behaviour->dndLeave(dragInfo.targetNode);
		}
		dragInfo.targetNode = node;
		if (dragInfo.targetNode) {
			nodeAcceptDND = dragInfo.targetNode->behaviour->dndEnter(dragInfo.targetNode);
		}
	}

	if (dragInfo.targetNode == NULL) {
		nodeAcceptDND = qfalse;
		positionAcceptDND = qfalse;
		return;
	}

	if (!nodeAcceptDND) {
		positionAcceptDND = qfalse;
		return;
	}

	positionAcceptDND = node->behaviour->dndMove(dragInfo.targetNode, mousePosX, mousePosY);
}


/**
 * @todo at least, the draw of the preview must be down by the requested node
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

	/* draw the draging item */

	VectorSet(orgine, mousePosX, mousePosY, -50);

	/* Tune down the opacity of the cursor-item if the preview item is drawn. */
	if (positionAcceptDND)
		color[3] = 0.2;

	switch (dragInfo.type) {
	case DND_ITEM:
		MN_DrawItem(NULL, orgine, &dragInfo.item, -1, -1, scale, color);
		break;

	default:
		assert(qfalse);
	}

#ifdef PARANOID
	/** Debugging only - Will draw a marker in the upper left corner of the
	 * dragged item (which is the one used for calculating the resulting grid-coordinates).
	 * @todo Maybe we could make this a feature in some way. i.e. we could draw
	 * a special cursor at this place when dragging as a hint*/

	Vector4Set(color, 1, 0, 0, 1);
	if (dragInfo.item.t)
		VectorSet(orgine,
			mousePosX - (C_UNIT * dragInfo.item.t->sx - C_UNIT) / 2,
			mousePosY - (C_UNIT * dragInfo.item.t->sy - C_UNIT) / 2,
			-50);
	R_DrawCircle2D(orgine[0] * viddef.rx, orgine[1] * viddef.ry, 2.0, qtrue, color, 1.0);
#endif
}
