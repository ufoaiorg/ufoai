/**
 * @file m_node_cinematic.c
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
#include "m_node_cinematic.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../cinematic/cl_cinematic.h"

static void MN_CinematicNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);

	if (!node->image)
		return;

	if (cls.playingCinematic == CIN_STATUS_FULLSCREEN) {
		MN_CaptureDrawOver(node);
		return;
	}

	if (cls.playingCinematic == CIN_STATUS_NONE)
		CIN_PlayCinematic(va("videos/%s", (const char *)node->image));
	if (cls.playingCinematic) {
		/* only set replay to true if video was found and is running */
		CIN_SetParameters(pos[0], pos[1], node->size[0], node->size[1], CIN_STATUS_MENU, qtrue);
		CIN_RunCinematic();
	}
}

static void MN_CinematicNodeDrawOverMenu (menuNode_t *node)
{
	CIN_RunCinematic();
}

static void MN_CinematicNodeInit (menuNode_t *node)
{
	if (!node->image)
		return;
	CIN_PlayCinematic(va("videos/%s", (const char *)node->image));
}

static void MN_CinematicNodeClose (menuNode_t *node)
{
	/* If playing a cinematic, stop it */
	CIN_StopCinematic();
}

static const value_t properties[] = {
	/** Source of the video. File name without prefix ./base/videos and without extension */
	{"src", V_CVAR_OR_STRING, offsetof(menuNode_t, image), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCinematicNode (nodeBehaviour_t* behaviour)
{
	behaviour->name = "video";
	behaviour->draw = MN_CinematicNodeDraw;
	behaviour->properties = properties;
	behaviour->init = MN_CinematicNodeInit;
	behaviour->close = MN_CinematicNodeClose;
	behaviour->drawOverMenu = MN_CinematicNodeDrawOverMenu;
}
