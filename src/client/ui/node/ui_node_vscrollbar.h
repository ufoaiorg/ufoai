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

class uiVScrollbarNode : public uiAbstractScrollbarNode {
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
	void onLoaded(uiNode_t* node) override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
	void onCapturedMouseLost(uiNode_t* node) override;
};

void UI_RegisterVScrollbarNode(uiBehaviour_t* behaviour);
