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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_draw.h"
#include "../ui_render.h"
#include "../../client.h"
#include "../../renderer/r_misc.h"
#include "../../renderer/r_draw.h"
#include "../../cinematic/cl_sequence.h"
#include "ui_node_abstractnode.h"
#include "ui_node_sequence.h"

#define EXTRADATA_TYPE sequenceExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t* propertySource;

static const uiBehaviour_t* localBehaviour;


void uiSequenceNode::draw (uiNode_t* node)
{
	if (EXTRADATA(node).context != nullptr && EXTRADATA(node).playing) {
		bool finished = false;
		vec2_t pos;
		vec2_t screenPos;
		UI_GetNodeAbsPos(node, pos);
		UI_GetNodeScreenPos(node, screenPos);

		R_PushMatrix();
		R_CleanupDepthBuffer(pos[0], pos[1], node->box.size[0], node->box.size[1]);
		UI_PushClipRect(screenPos[0], screenPos[1], node->box.size[0], node->box.size[1]);

		SEQ_SetView(EXTRADATA(node).context, pos, node->box.size);
		finished = !SEQ_Render(EXTRADATA(node).context);

		UI_PopClipRect();
		R_PopMatrix();

		if (finished && EXTRADATA(node).onEnd) {
			UI_ExecuteEventActions(node, EXTRADATA(node).onEnd);
			EXTRADATA(node).playing = true;
		}
	}
}

void uiSequenceNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	if (EXTRADATA(node).context == nullptr)
		EXTRADATA(node).context = SEQ_AllocContext();
	if (EXTRADATA(node).source != nullptr) {
		SEQ_InitContext(EXTRADATA(node).context, EXTRADATA(node).source);
		EXTRADATA(node).playing = true;
	}
}

void uiSequenceNode::onWindowClosed (uiNode_t* node)
{
	if (EXTRADATA(node).context != nullptr) {
		SEQ_FreeContext(EXTRADATA(node).context);
		EXTRADATA(node).context = nullptr;
	}
	EXTRADATA(node).playing = false;
}

void uiSequenceNode::onLeftClick (uiNode_t* node, int x, int y)
{
	if (EXTRADATA(node).context != nullptr) {
		SEQ_SendClickEvent(EXTRADATA(node).context);
	}
}

void uiSequenceNode::onPropertyChanged (uiNode_t* node, const value_t* property)
{
	if (property == propertySource) {
		if (EXTRADATA(node).source != nullptr) {
			onWindowOpened(node, nullptr);
		} else if (EXTRADATA(node).context != nullptr) {
			onWindowClosed(node);
		}
		return;
	}
	uiLocatedNode::onPropertyChanged(node, property);
}

void UI_RegisterSequenceNode (uiBehaviour_t* behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "sequence";
	behaviour->manager = UINodePtr(new uiSequenceNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/** sequence script id */
	propertySource = UI_RegisterExtradataNodeProperty(behaviour, "src", V_CVAR_OR_STRING, EXTRADATA_TYPE, source);

	/** Called when the sequence end */
	UI_RegisterExtradataNodeProperty(behaviour, "onEnd", V_UI_ACTION, EXTRADATA_TYPE, onEnd);

}
