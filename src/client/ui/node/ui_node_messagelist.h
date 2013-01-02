/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_MESSAGELIST_H
#define CLIENT_UI_UI_NODE_MESSAGELIST_H

#include "ui_node_abstractscrollable.h"

class uiMessageListNode : public uiAbstractScrollableNode {
	void draw(uiNode_t* node) OVERRIDE;
	void onLoading(uiNode_t* node) OVERRIDE;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) OVERRIDE;
	void onMouseDown(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void onMouseUp(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) OVERRIDE;
	int getCellHeight (uiNode_t *node) OVERRIDE;
};

struct uiMessageListNodeMessage_s;
struct uiBehaviour_t;
struct uiAction_s;

void UI_RegisterMessageListNode(uiBehaviour_t *behaviour);
struct uiMessageListNodeMessage_s* UI_MessageGetStack (void);
void UI_MessageAddStack(struct uiMessageListNodeMessage_s* message);
void UI_MessageResetStack(void);

#endif
