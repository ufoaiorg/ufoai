/**
 * @file ui_node_video.c
 * @todo add function to play/stop/pause
 * @todo fix fullscreen, looped video
 * @todo event when video end
 * @todo function to move the video by position
 * @todo function or cvar to know the video position
 * @todo cvar or property to know the size of the video
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../ui_draw.h"
#include "ui_node_video.h"
#include "ui_node_abstractnode.h"

#include "../../client.h"
#include "../../cinematic/cl_cinematic.h"

#define EXTRADATA_TYPE videoExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_VideoNodeDrawOverWindow (uiNode_t *node)
{
	if (EXTRADATA(node).cin.status == CIN_STATUS_NONE) {
		vec2_t pos;
		qboolean nosound = UI_VIDEOEXTRADATACONST(node).nosound;

		CIN_PlayCinematic(&(EXTRADATA(node).cin), va("videos/%s", (const char *)node->image));

		UI_GetNodeAbsPos(node, pos);
		CIN_SetParameters(&(EXTRADATA(node).cin), pos[0], pos[1], node->size[0], node->size[1], CIN_STATUS_PLAYING, nosound);
	}

	if (EXTRADATA(node).cin.status) {
		/* only set replay to true if video was found and is running */
		CIN_RunCinematic(&(EXTRADATA(node).cin));
	}
}

static void UI_VideoNodeDraw (uiNode_t *node)
{
	if (!node->image)
		return;

	if (EXTRADATA(node).cin.fullScreen) {
		UI_CaptureDrawOver(node);
		return;
	}

	UI_VideoNodeDrawOverWindow(node);
}

static void UI_VideoNodeInit (uiNode_t *node)
{
	CIN_InitCinematic(&(EXTRADATA(node).cin));
}

static void UI_VideoNodeClose (uiNode_t *node)
{
	/* If playing a cinematic, stop it */
	CIN_StopCinematic(&(EXTRADATA(node).cin));
}

static const value_t properties[] = {
	/** Source of the video. File name without prefix ./base/videos and without extension */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},
	{"nosound", V_BOOL, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, nosound), MEMBER_SIZEOF(EXTRADATA_TYPE, nosound)},
	{NULL, V_NULL, 0, 0}
};

void UI_RegisterVideoNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "video";
	behaviour->draw = UI_VideoNodeDraw;
	behaviour->properties = properties;
	behaviour->windowOpened = UI_VideoNodeInit;
	behaviour->windowClosed = UI_VideoNodeClose;
	behaviour->drawOverWindow = UI_VideoNodeDrawOverWindow;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
