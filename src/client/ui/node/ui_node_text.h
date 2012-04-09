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

#ifndef CLIENT_UI_UI_NODE_TEXT_H
#define CLIENT_UI_UI_NODE_TEXT_H

#include "ui_node_abstractscrollable.h"

class uiTextNode : public uiAbstractScrollableNode {
public:
	void draw(uiNode_t* node);
	void leftClick(uiNode_t* node, int x, int y) OVERRIDE;
	void rightClick(uiNode_t* node, int x, int y) OVERRIDE;
	bool scroll(uiNode_t* node, int deltaX, int deltaY) OVERRIDE;
	void mouseMove(uiNode_t* node, int x, int y) OVERRIDE;
	void mouseDown(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void mouseUp(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void capturedMouseMove(uiNode_t* node, int x, int y) OVERRIDE;
	void loading(uiNode_t* node) OVERRIDE;
	void loaded(uiNode_t* node) OVERRIDE;
	int getCellHeight (uiNode_t* node) OVERRIDE;
public:
	void validateCache(uiNode_t* node);
protected:
	void drawText (uiNode_t* node, const char *text, const linkedList_t* list, bool noDraw);
	virtual void updateCache (uiNode_t* node);
};

struct uiBehaviour_s;
struct uiAction_s;

void UI_TextScrollEnd(const char* nodePath);
void UI_TextNodeSelectLine(uiNode_t* node, int num);

void UI_RegisterTextNode(struct uiBehaviour_s *behaviour);

typedef struct {
	abstractScrollableExtraData_t super;

	int dataID;					/**< ID of a shared data @sa src/client/ui/ui_data.h */
	int versionId;				/**< Cached version of the shared data, to check update */

	int textLineSelected;		/**< Which line is currenlty selected? This counts only visible lines). Add textScroll to this value to get total linecount. @sa selectedColor below.*/
	int lineUnderMouse;			/**< UI_TEXT: The line under the mouse, when the mouse is over the node */
	int lineHeight;				/**< size between two lines */
	int tabWidth;				/**< max size of a tabulation */
	int longlines;				/**< what to do with long lines */
	qboolean mousefx;

} textExtraData_t;

/**
 * @note text node inherite scrollable node. Scrollable (super) extradata
 * must not move, else we can't call scrollable functions.
 */
CASSERT(offsetof(textExtraData_t, super) == 0);

#endif
