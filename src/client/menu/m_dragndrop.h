/**
 * @file m_dragndrop.h
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

#ifndef CLIENT_MENU_M_DRAGNDROP_H
#define CLIENT_MENU_M_DRAGNDROP_H

#include "../../game/inventory.h"

struct menuNode_s;
struct item_s;

typedef enum {
	DND_NOTHING,
	DND_SOMETHING,	/**< Untyped object */
	DND_ITEM,
} dndType_t;

/* management */
void MN_DrawDragAndDrop(int mousePosX, int mousePosY);

/* command */
void MN_DNDDragItem(struct menuNode_s *node, const struct item_s *item);
void MN_DNDDrop(void);
void MN_DNDAbort(void);

/*  getter */
qboolean MN_DNDIsDragging(void);
qboolean MN_DNDIsTargetNode(struct menuNode_s *node);
qboolean MN_DNDIsSourceNode(struct menuNode_s *node);
struct menuNode_s *MN_DNDGetTargetNode(void);
struct menuNode_s *MN_DNDGetSourceNode(void);
int MN_DNDGetType(void);
struct item_s *MN_DNDGetItem(void);

#endif
