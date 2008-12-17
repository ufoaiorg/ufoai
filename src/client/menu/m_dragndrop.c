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
 * @todo is it realy need to init this structure?
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
 * @brief Retrun true if the requested node is the current destination of a dnd
 */
qboolean MN_DNDIsDestinationNode(struct menuNode_s *node)
{
	if (mouseSpace != MS_DRAGITEM)
		return qfalse;
	return dragInfo.toNode == node;
}

/**
 * @brief Start a drag and drop
 * @sa MN_DNDDragItem
 * @sa MN_DNDStop
 */
void MN_DNDStart (menuNode_t *node)
{
	assert(mouseSpace != MS_DRAGITEM);
	mouseSpace = MS_DRAGITEM;
	dragInfo.fromNode = node;
	dragInfo.fromAbsoluteX = mousePosX;
	dragInfo.fromAbsoluteY = mousePosY;
}

/**
 * @brief Stop the drag and drop
 * @sa MN_DNDStart
 */
void MN_DNDStop (void)
{
	assert(mouseSpace == MS_DRAGITEM);
	assert(dragInfo.type != DND_NOTHING);
	dragInfo.type = DND_NOTHING;
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

/**
 * @todo at least, the draw of the preview must be down by the requested node
 */
void MN_DrawDragAndDrop (int mousePosX, int mousePosY)
{
	const vec3_t scale = { 3.5, 3.5, 3.5 };
	vec3_t org;
	vec4_t color = { 1, 1, 1, 1 };

	/** @todo move it at a better plce when its possible */
	dragInfo.toNode = MN_GetNodeByPosition(mousePosX, mousePosY);

	/* draw the draging item */

	VectorSet(org, mousePosX, mousePosY, -50);
	if (dragInfo.toNode && dragInfo.isPlaceFound)
		Vector4Set(color, 1, 1, 1, 0.2);		/**< Tune down the opacity of the cursor-item if the preview item is drawn. */
	MN_DrawItem(NULL, org, &dragInfo.item, -1, -1, scale, color);

#ifdef PARANOID
	/** Debugging only - Will draw a marker in the upper left corner of the
	 * dragged item (which is the one used for calculating the resulting grid-coordinates).
	 * @todo Maybe we could make this a feature in some way. i.e. we could draw
	 * a special cursor at this place when dragging as a hint*/

	Vector4Set(color, 1, 0, 0, 1);
	if (dragInfo.item.t)
		VectorSet(org,
			mousePosX - (C_UNIT * dragInfo.item.t->sx - C_UNIT) / 2,
			mousePosY - (C_UNIT * dragInfo.item.t->sy - C_UNIT) / 2,
			-50);
	R_DrawCircle2D(org[0] * viddef.rx, org[1] * viddef.ry, 2.0, qtrue, color, 1.0);
#endif

}
