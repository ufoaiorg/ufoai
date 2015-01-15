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

#pragma once

#include "../../shared/ufotypes.h"

struct uiNode_t;
class Item;

typedef enum {
	DND_NOTHING,
	DND_SOMETHING,	/**< Untyped object */
	DND_ITEM
} uiDNDType_t;

/* management */
void UI_DrawDragAndDrop(int mousePosX, int mousePosY);

/* command */
void UI_DNDDragItem(uiNode_t* node, const Item* item);
void UI_DNDDrop(void);
void UI_DNDAbort(void);

/*  getter */
bool UI_DNDIsDragging(void);
bool UI_DNDIsTargetNode(uiNode_t* node);
bool UI_DNDIsSourceNode(uiNode_t* node);
uiNode_t* UI_DNDGetTargetNode(void);
uiNode_t* UI_DNDGetSourceNode(void);
int UI_DNDGetType(void);
Item* UI_DNDGetItem(void);
