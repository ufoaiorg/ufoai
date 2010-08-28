/**
 * @file ui_node_sequence.c
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
#include "../../client.h"
#include "../../cl_sequence.h"
#include "ui_node_abstractnode.h"
#include "ui_node_sequence.h"

#define EXTRADATA_TYPE sequenceExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_SequenceNodeDrawOverWindow (uiNode_t *node)
{
	// ...
}

static void UI_SequenceNodeDraw (uiNode_t *node)
{
	// ...
}

static void UI_SequenceNodeInit (uiNode_t *node)
{
	// ...
}

static void UI_SequenceNodeClose (uiNode_t *node)
{
	// ...
}

static const value_t properties[] = {
	/** Source of the video. File name without prefix ./base/videos and without extension */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterSequenceNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "sequence";
	behaviour->draw = UI_SequenceNodeDraw;
	behaviour->properties = properties;
	behaviour->init = UI_SequenceNodeInit;
	behaviour->close = UI_SequenceNodeClose;
	behaviour->drawOverWindow = UI_SequenceNodeDrawOverWindow;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
