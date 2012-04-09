/**
 * @file
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_TEXTLIST_H
#define CLIENT_UI_UI_NODE_TEXTLIST_H

class uiTextListNode : public uiTextNode {
	void draw(uiNode_t* node) OVERRIDE;
	void leftClick(uiNode_t* node, int x, int y) OVERRIDE;
	void rightClick(uiNode_t* node, int x, int y) OVERRIDE;
	void mouseMove(uiNode_t* node, int x, int y) OVERRIDE;
	void loading(uiNode_t* node) OVERRIDE;
protected:
	void drawText (uiNode_t* node, const linkedList_t* list);
};

struct uiBehaviour_s;
struct uiAction_s;

void UI_RegisterTextListNode(struct uiBehaviour_s *behaviour);

#endif
