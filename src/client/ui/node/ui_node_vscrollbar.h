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


#ifndef CLIENT_UI_UI_NODE_VSCROLLBAR_H
#define CLIENT_UI_UI_NODE_VSCROLLBAR_H

class uiVScrollbarNode : public uiAbstractScrollbarNode {
	void draw(uiNode_t* node) OVERRIDE;
	void onLoading(uiNode_t* node) OVERRIDE;
	void onLoaded(uiNode_t* node) OVERRIDE;
	void onMouseDown(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void onMouseUp(uiNode_t* node, int x, int y, int button) OVERRIDE;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) OVERRIDE;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) OVERRIDE;
	void onCapturedMouseLost(uiNode_t* node) OVERRIDE;
};

void UI_RegisterVScrollbarNode(uiBehaviour_t *behaviour);

#endif
