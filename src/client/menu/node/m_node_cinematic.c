/**
 * @file m_node_cinematic.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../cl_cinematic.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "m_node_model.h"
#include "m_node_cinematic.h"
#include "m_node_abstractnode.h"

static void MN_CinematicNodeDraw (menuNode_t *node)
{
	vec2_t nodepos;
	MN_GetNodeAbsPos(node, nodepos);
	if (node->cvar) {
		assert(cls.playingCinematic != CIN_STATUS_FULLSCREEN);
		if (cls.playingCinematic == CIN_STATUS_NONE)
			CIN_PlayCinematic(node->cvar);
		if (cls.playingCinematic) {
			/* only set replay to true if video was found and is running */
			CIN_SetParameters(nodepos[0], nodepos[1], node->size[0], node->size[1], CIN_STATUS_MENU, qtrue);
			CIN_RunCinematic();
		}
	}
}

static const value_t properties[] = {
	{"roq", V_CVAR_OR_STRING, offsetof(menuNode_t, cvar), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCinematicNode (nodeBehaviour_t* behaviour)
{
	behaviour->name = "cinematic";
	behaviour->draw = MN_CinematicNodeDraw;
	behaviour->properties = properties;
}
