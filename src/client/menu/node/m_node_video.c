/**
 * @file m_node_video.c
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_draw.h"
#include "m_node_video.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../cinematic/cl_cinematic.h"

#define EXTRADATA_TYPE videoExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_VideoNodeDraw (uiNode_t *node)
{
	vec2_t pos;

	if (!node->image)
		return;

	if (EXTRADATA(node).cin.fullScreen) {
		UI_CaptureDrawOver(node);
		return;
	}

	UI_GetNodeAbsPos(node, pos);

	if (EXTRADATA(node).cin.status == CIN_STATUS_NONE)
		CIN_PlayCinematic(&(EXTRADATA(node).cin), va("videos/%s", (const char *)node->image));
	if (EXTRADATA(node).cin.status) {
		/* only set replay to true if video was found and is running */
		CIN_SetParameters(&(EXTRADATA(node).cin), pos[0], pos[1], node->size[0], node->size[1], CIN_STATUS_PLAYING, qtrue);
		CIN_RunCinematic(&(EXTRADATA(node).cin));
	}
}

static void UI_VideoNodeDrawOverMenu (uiNode_t *node)
{
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	CIN_SetParameters(&(EXTRADATA(node).cin), pos[0], pos[1], node->size[0], node->size[1], CIN_STATUS_PLAYING, qtrue);
	CIN_RunCinematic(&(EXTRADATA(node).cin));
}

static void UI_VideoNodeInit (uiNode_t *node)
{
	CIN_InitCinematic(&(EXTRADATA(node).cin));

	if (!node->image)
		return;
	CIN_PlayCinematic(&(EXTRADATA(node).cin), va("videos/%s", (const char *)node->image));
}

static void UI_VideoNodeClose (uiNode_t *node)
{
	/* If playing a cinematic, stop it */
	CIN_StopCinematic(&(EXTRADATA(node).cin));
}

static const value_t properties[] = {
	/** Source of the video. File name without prefix ./base/videos and without extension */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},
	{NULL, V_NULL, 0, 0}
};

void UI_RegisterVideoNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "video";
	behaviour->draw = UI_VideoNodeDraw;
	behaviour->properties = properties;
	behaviour->init = UI_VideoNodeInit;
	behaviour->close = UI_VideoNodeClose;
	behaviour->drawOverMenu = UI_VideoNodeDrawOverMenu;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
