/**
 * @file m_inventory.h
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

#ifndef CLIENT_MENU_M_INVENTORY_H
#define CLIENT_MENU_M_INVENTORY_H

#include "../../game/inv_shared.h"

struct base_s;

/* One unit in the containers is 25x25. */
#define C_UNIT				25
#define	C_ROW_OFFSET		15	/**< Offset between two rows (and the top of the container to
								 * the first row) of items in a scrollable container.
								 * Right now only used for vertical containers.
								 */

extern inventory_t *menuInventory;

typedef struct dragInfo_s {
	item_t item;	/**< The item that is currently dragged. */
	const invDef_t * from;		/**< The container the items is dragged out of (i.e. from). */
	int fromX;		/**< The X position in the container the item was/is located.
					 * More exactly this is the position the user clicked on. (i.e. NOT necessarily invList_t->x) */
	int fromY;		/**< The Y position in the container the item was/is located. */

	/* The "to" variables are only used in cl_screen.c to draw the preview. */
	menuNode_t *toNode;
	const invDef_t * to;
	int toX;
	int toY;
} dragInfo_t;

extern dragInfo_t dragInfo;

void MN_FindContainer(menuNode_t* const node);
void MN_Drag(const menuNode_t* const node, struct base_s *base, int x, int y, qboolean rightClick);
void MN_DrawItem(const vec3_t org, const item_t *item, int x, int y, const vec3_t scale, const vec4_t color);
const invList_t* MN_DrawContainerNode(menuNode_t *node);
void MN_DrawItemNode(menuNode_t *node, const char *itemName);

void MN_DrawDisabled(const menuNode_t* node);
void MN_DrawFree(int container, const menuNode_t *node, int posx, int posy, int sizex, int sizey, qboolean showTUs);
void MN_InvDrawFree(inventory_t *inv, const menuNode_t *node);
void MN_GetItemTooltip(item_t item, char *tooltiptext, size_t string_maxlength);

#endif
