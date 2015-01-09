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

#include "../ui_nodes.h"

class uiTextEntryNode : public uiLocatedNode {
	void onLoading(uiNode_t* node) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void draw(uiNode_t* node) override;
	void onFocusGained(uiNode_t* node) override;
	void onFocusLost(uiNode_t* node) override;
	bool onKeyPressed(uiNode_t* node, unsigned int key, unsigned short unicode) override;
};

/**
 * @brief extradata for the textentry, to custom draw and behaviour
 */
typedef struct textEntryExtraData_s {
	bool isPassword;		/**< Display '*' instead of the real text */
	bool clickOutAbort;		/**< If we click out an activated node, it abort the edition */
	int cursorPosition;		/**< Cursor position (offset of next UTF-8 char to the right) */
	struct uiAction_s* onAbort;
	uiSprite_t* background;	/**< Sprite used as a background */
} textEntryExtraData_t;

void UI_RegisterTextEntryNode(uiBehaviour_t* behaviour);
