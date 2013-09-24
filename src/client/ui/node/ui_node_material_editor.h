/**
 * @file
 * @brief Material editor related header
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

class uiMaterialEditorNode : public uiAbstractScrollableNode {
	void draw(uiNode_t* node) override;
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
protected:
	void updateView (uiNode_t* node, bool reset);
};

struct uiBehaviour_t;
void UI_RegisterMaterialEditorNode(uiBehaviour_t* behaviour);
