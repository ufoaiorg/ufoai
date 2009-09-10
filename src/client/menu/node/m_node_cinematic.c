/**
 * @file m_node_cinematic.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "m_node_model.h"
#include "m_node_cinematic.h"
#include "m_node_abstractnode.h"

#include "../../client.h"
#include "../../cl_cinematic.h"

static void MN_CinematicNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	MN_GetNodeAbsPos(node, pos);
	if (node->cvar) {
		assert(cls.playingCinematic != CIN_STATUS_FULLSCREEN);
		if (cls.playingCinematic == CIN_STATUS_NONE)
			CIN_PlayCinematic(va("videos/%s", (const char *)node->cvar));
		if (cls.playingCinematic) {
			/* only set replay to true if video was found and is running */
			CIN_SetParameters(pos[0], pos[1], node->size[0], node->size[1], CIN_STATUS_MENU, qtrue);
			CIN_RunCinematic();
		}
	}
}

static const value_t properties[] = {
	/** @todo Please document it */
	{"video", V_CVAR_OR_STRING, offsetof(menuNode_t, cvar), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCinematicNode (nodeBehaviour_t* behaviour)
{
	behaviour->name = "cinematic";
	behaviour->draw = MN_CinematicNodeDraw;
	behaviour->properties = properties;
}
