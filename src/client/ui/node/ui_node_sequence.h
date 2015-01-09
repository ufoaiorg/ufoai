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
#include "../../cinematic/cl_sequence.h"

class uiSequenceNode : public uiLocatedNode {
	void draw(uiNode_t* node) override;
	void onLeftClick(uiNode_t* node, int x, int y) override;
	void onWindowOpened(uiNode_t* node, linkedList_t* params) override;
	void onWindowClosed(uiNode_t* node) override;
	void onPropertyChanged(uiNode_t* node, const value_t* property) override;
};

void UI_RegisterSequenceNode(uiBehaviour_t* behaviour);

typedef struct {
	bool playing;
	const char* source;

	/** Called when the sequence end */
	struct uiAction_s* onEnd;

	/** Link to the current sequence context, else nullptr */
	pSequenceContext_t context;
} sequenceExtraData_t;
