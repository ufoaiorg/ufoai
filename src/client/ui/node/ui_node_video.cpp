/**
 * @file
 * @todo add function to play/stop/pause
 * @todo fix fullscreen, looped video
 * @todo event when video end
 * @todo function to move the video by position
 * @todo function or cvar to know the video position
 * @todo cvar or property to know the size of the video
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
#include "../ui_draw.h"
#include "../ui_actions.h"
#include "ui_node_video.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../cinematic/cl_cinematic.h"

#define EXTRADATA_TYPE videoExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

void uiVideoNode::drawOverWindow (uiNode_t* node)
{
	if (EXTRADATA(node).cin.status == CIN_STATUS_INVALID) {
		/** @todo Maybe draw a black screen? */
		return;
	}

	if (EXTRADATA(node).cin.status == CIN_STATUS_NONE) {
		vec2_t pos;
		bool nosound = UI_VIDEOEXTRADATACONST(node).nosound;

		CIN_OpenCinematic(&(EXTRADATA(node).cin), va("videos/%s", EXTRADATA(node).source));
		if (EXTRADATA(node).cin.status == CIN_STATUS_INVALID) {
			UI_ExecuteEventActions(node, EXTRADATA(node).onEnd);
			return;
		}

		UI_GetNodeAbsPos(node, pos);
		CIN_SetParameters(&(EXTRADATA(node).cin), pos[0], pos[1], node->box.size[0], node->box.size[1], CIN_STATUS_PLAYING, nosound);
	}

	if (EXTRADATA(node).cin.status == CIN_STATUS_PLAYING || EXTRADATA(node).cin.status == CIN_STATUS_PAUSE) {
		/* only set replay to true if video was found and is running */
		CIN_RunCinematic(&(EXTRADATA(node).cin));
		if (EXTRADATA(node).cin.status == CIN_STATUS_NONE) {
			UI_ExecuteEventActions(node, EXTRADATA(node).onEnd);
		}
	}
}

void uiVideoNode::draw (uiNode_t* node)
{
	if (!EXTRADATA(node).source)
		return;

	if (EXTRADATA(node).cin.fullScreen) {
		UI_CaptureDrawOver(node);
		return;
	}

	drawOverWindow(node);
}

void uiVideoNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	CIN_InitCinematic(&(EXTRADATA(node).cin));
}

void uiVideoNode::onWindowClosed (uiNode_t* node)
{
	/* If playing a cinematic, stop it */
	CIN_CloseCinematic(&(EXTRADATA(node).cin));
}

void UI_RegisterVideoNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "video";
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->manager = UINodePtr(new uiVideoNode());

	/** Source of the video. File name without prefix ./base/videos and without extension */
	UI_RegisterExtradataNodeProperty(behaviour, "src", V_CVAR_OR_STRING, EXTRADATA_TYPE, source);
	/** Use or not the music from the video. */
	UI_RegisterExtradataNodeProperty(behaviour, "nosound", V_BOOL, EXTRADATA_TYPE, nosound);
	/** Invoked when video end. */
	UI_RegisterExtradataNodeProperty(behaviour, "onEnd", V_UI_ACTION, EXTRADATA_TYPE, onEnd);
}
