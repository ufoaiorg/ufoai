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

#include "../../../shared/mathlib.h"
#include "ui_node_abstractnode.h"

/* prototype */
struct uiNode_t;
struct uiAction_s;

class uiTimerNode : public uiLocatedNode {
public:
	void draw(uiNode_t* node);
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onWindowClosed(uiNode_t* node) override;
};

/**
 * @brief extradata for the window node
 */
struct timerExtraData_t {
	int timeOut;					/**< ms value until calling onTimeOut (see cl.time) */
	int lastTime;					/**< when a window was pushed this value is set to cl.time */
	struct uiAction_s* onTimeOut;	/**< Call when the own timer of the window out */
};

void UI_RegisterTimerNode(uiBehaviour_t* behaviour);
