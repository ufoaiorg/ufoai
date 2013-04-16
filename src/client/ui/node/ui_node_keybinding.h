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


#ifndef CLIENT_UI_UI_NODE_KEY_BINDING_H
#define CLIENT_UI_UI_NODE_KEY_BINDING_H

#include "../ui_nodes.h"
#include "../../../common/common.h"
#include "../../input/cl_keys.h"

class uiKeyBindingNode : public uiLocatedNode {
	void onLeftClick(uiNode_t* node, int x, int y) override;
	bool onKeyPressed(uiNode_t* node, unsigned int key, unsigned short unicode) override;
	void draw(uiNode_t* node) override;
	void onLoading(uiNode_t* node) override;
};

/**
 * @brief extradata for the keybinding node
 */
typedef struct keyBindingExtraData_s {
	keyBindSpace_t keySpace;
	int bindingWidth;
	/** Sprite used as a background */
	uiSprite_t *background;
} keyBindingExtraData_t;

void UI_RegisterKeyBindingNode(uiBehaviour_t *behaviour);

#endif
