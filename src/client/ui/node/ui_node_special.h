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

class uiConFuncNode : public uiNode {
	void onLoaded(uiNode_t* node) override;
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onWindowClosed(uiNode_t* node) override;
	void clone(uiNode_t const* source, uiNode_t* clone) override;
};

class uiFuncNode : public uiNode {
	void onLoaded(uiNode_t* node);
};

class uiCvarNode : public uiNode {
	void onWindowOpened(uiNode_t* node, linkedList_t* params);
	void onWindowClosed(uiNode_t* node);
	void clone(uiNode_t const* source, uiNode_t* clone);
	void deleteNode(uiNode_t* node);
};

void UI_RegisterConFuncNode(uiBehaviour_t* behaviour);
void UI_RegisterCvarFuncNode(uiBehaviour_t* behaviour);
void UI_RegisterFuncNode(uiBehaviour_t* behaviour);
void UI_RegisterNullNode(uiBehaviour_t* behaviour);
