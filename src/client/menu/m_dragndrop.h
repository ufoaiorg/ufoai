/**
 * @file m_dragndrop.h
 * @todo create generic function usable by every nodes
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

#ifndef CLIENT_MENU_M_DRAGNDROP_H
#define CLIENT_MENU_M_DRAGNDROP_H

#include "../../game/inv_shared.h"

struct invList_s;
struct invDef_s;
struct menuNode_s;
struct invDef_s;

typedef enum {
	DND_NOTHING,
	DND_ITEM,
} dndType_t;


typedef struct dragInfo_s {
	dndType_t type;					/**< What are we dragging */
	struct menuNode_s *fromNode;	/**< Node providing the drag item */
	int fromAbsoluteX;				/**< Where the drag start */
	int fromAbsoluteY;				/**< Where the drag start */

	/* bellow that, nothing is generic :/ */

	item_t item;	/**< The item that is currently dragged. */
	const struct invList_s *ic;	/**< The current invList pointer (only used for ignoring the dragged item for finding free space right now) */
	const struct invDef_s * from;	/**< The container the items is dragged out of (i.e. from). */
	int fromX;		/**< The X position in the container the item was/is located.
					 * More exactly this is the position the user clicked on. (i.e. NOT necessarily invList_t->x) */
	int fromY;		/**< The Y position in the container the item was/is located. */

	/* The "to" variables are only used in cl_screen.c to draw the preview. */
	const struct menuNode_s *toNode;
	const struct invDef_s *to;
	int toX;
	int toY;
} dragInfo_t;

extern dragInfo_t dragInfo;

void MN_DrawDragAndDrop(int mousePosX, int mousePosY);
qboolean MN_DNDIsDragging(void);
void MN_DNDStart(struct menuNode_s *from);
void MN_DNDDragItem(item_t *item, struct invList_s *fromInventory);
void MN_DNDFromContainer(struct invDef_s *container, int ownFromX, int ownFromY);
void MN_DNDStop(void);

#endif
