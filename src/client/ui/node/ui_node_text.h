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

#include "ui_node_abstractscrollable.h"

class uiTextNode : public uiAbstractScrollableNode {
public:
	void draw(uiNode_t* node);
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void onRightClick(uiNode_t* node, int x, int y) override;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) override;
	void onMouseMove(uiNode_t* node, int x, int y) override;
	void onMouseDown(uiNode_t* node, int x, int y, int button) override;
	void onMouseUp(uiNode_t* node, int x, int y, int button) override;
	void onCapturedMouseMove(uiNode_t* node, int x, int y) override;
	void onLoading(uiNode_t* node) override;
	void onLoaded(uiNode_t* node) override;
	int getCellHeight (uiNode_t* node) override;
public:
	void validateCache(uiNode_t* node);
protected:
	void drawText (uiNode_t* node, const char* text, const linkedList_t* list, bool noDraw);
	virtual void updateCache (uiNode_t* node);
};

struct uiBehaviour_t;
struct uiAction_s;

void UI_TextScrollEnd(const char* nodePath);
void UI_TextNodeSelectLine(uiNode_t* node, int num);
const char* UI_TextNodeGetSelectedText(uiNode_t* node, int num);

void UI_RegisterTextNode(uiBehaviour_t* behaviour);

typedef struct {
	abstractScrollableExtraData_t super;

	int dataID;					/**< ID of a shared data @sa src/client/ui/ui_data.h */
	int versionId;				/**< Cached version of the shared data, to check update */

	int textLineSelected;		/**< Which line is currently selected? This counts only visible lines). Add textScroll to this value to get total linecount. @sa selectedColor below.*/
	const char* textSelected;	/**< Text of the current selected line */
	int lineUnderMouse;			/**< UI_TEXT: The line under the mouse, when the mouse is over the node */
	int lineHeight;				/**< size between two lines */
	int tabWidth;				/**< max size of a tabulation */
	int longlines;				/**< what to do with long lines */
	bool mousefx;

} textExtraData_t;

/**
 * @note text node inherite scrollable node. Scrollable (super) extradata
 * must not move, else we can't call scrollable functions.
 */
CASSERT(offsetof(textExtraData_t, super) == 0);
