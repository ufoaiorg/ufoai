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

#include "../ui_timer.h"
#include "../ui_parse.h"
#include "../ui_actions.h"
#include "../ui_behaviour.h"
#include "ui_node_timer.h"

#define EXTRADATA_TYPE timerExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * @brief Called when we init the node on the screen
 */
void uiTimerNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	EXTRADATA(node).lastTime = CL_Milliseconds();
}

void uiTimerNode::onWindowClosed (uiNode_t* node)
{
}

/**
 * @todo Use a real timer and not that hack
 */
void uiTimerNode::draw (uiNode_t* node)
{
	uiLocatedNode::draw(node);
	timerExtraData_t& data = EXTRADATA(node);
	/* embedded timer */
	if (data.onTimeOut && data.timeOut) {
		if (data.lastTime == 0)
			data.lastTime = CL_Milliseconds();
		if (data.lastTime + data.timeOut < CL_Milliseconds()) {
			/* allow to reset timeOut on the event, and restart it, with an uptodate lastTime */
			data.lastTime = 0;
			Com_DPrintf(DEBUG_CLIENT, "uiTimerNode::draw: Timeout for node '%s'\n", node->name);
			UI_ExecuteEventActions(node, data.onTimeOut);
		}
	}
}

void UI_RegisterTimerNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "timer";
	behaviour->manager = UINodePtr(new uiTimerNode());
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* This property control milliseconds between each calls of <code>onEvent</code>.
	 * If the value is 0 (the default value) nothing is called. We can change the
	 * value at runtime.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "timeout", V_INT, EXTRADATA_TYPE, timeOut);

	/* Invoked periodically. See <code>timeout</code>. */
	UI_RegisterExtradataNodeProperty(behaviour, "onEvent", V_UI_ACTION, EXTRADATA_TYPE, onTimeOut);
}
