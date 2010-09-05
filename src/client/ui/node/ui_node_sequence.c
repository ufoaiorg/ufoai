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
#include "../ui_actions.h"
#include "../ui_draw.h"
#include "../../client.h"
#include "../../renderer/r_misc.h"
#include "../../renderer/r_draw.h"
#include "../../cl_sequence.h"
#include "ui_node_abstractnode.h"
#include "ui_node_sequence.h"

#define EXTRADATA_TYPE sequenceExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t *propertySource;

static const uiBehaviour_t const *localBehaviour;


static void UI_SequenceNodeDraw (uiNode_t *node)
{
	if (EXTRADATA(node).context != NULL && EXTRADATA(node).playing) {
		qboolean finished = qfalse;
		vec2_t pos;
		UI_GetNodeAbsPos(node, pos);

		R_PushMatrix();
		R_CleanupDepthBuffer(pos[0], pos[1], node->size[0], node->size[1]);
		R_PushClipRect(pos[0], pos[1], node->size[0], node->size[1]);

		SEQ_SetView(EXTRADATA(node).context, pos, node->size);
		finished = !SEQ_Render(EXTRADATA(node).context);

		R_PopClipRect();
		R_PopMatrix();

		if (finished && EXTRADATA(node).onEnd) {
			UI_ExecuteEventActions(node, EXTRADATA(node).onEnd);
			EXTRADATA(node).playing = qtrue;
		}
	}
}

static void UI_SequenceNodeInit (uiNode_t *node)
{
	if (EXTRADATA(node).context == NULL)
		EXTRADATA(node).context = SEQ_AllocContext();
	if (node->image != NULL) {
		SEQ_InitContext(EXTRADATA(node).context, node->image);
		EXTRADATA(node).playing = qtrue;
	}
}

static void UI_SequenceNodeClose (uiNode_t *node)
{
	if (EXTRADATA(node).context != NULL) {
		SEQ_FreeContext(EXTRADATA(node).context);
		EXTRADATA(node).context = NULL;
	}
	EXTRADATA(node).playing = qfalse;
}

static void UI_SequenceNodeLeftClick (uiNode_t *node, int x, int y)
{
	if (EXTRADATA(node).context != NULL) {
		SEQ_SendClickEvent(EXTRADATA(node).context);
	}
}

static void UI_SequencePropertyChanged (uiNode_t *node, const value_t *property)
{
	if (property == propertySource) {
		if (node->image != NULL) {
			UI_SequenceNodeInit(node);
		} else if (EXTRADATA(node).context != NULL) {
			UI_SequenceNodeClose(node);
		}
		return;
	}
	localBehaviour->super->propertyChanged(node, property);
}

static const value_t properties[] = {
	/** Source of the video. File name without prefix ./base/videos and without extension */
	{"src", V_CVAR_OR_STRING, offsetof(uiNode_t, image), 0},

	/** Called when the sequence end */
	{"onend", V_UI_ACTION, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, onEnd), MEMBER_SIZEOF(EXTRADATA_TYPE, onEnd)},

	{NULL, V_NULL, 0, 0}
};

void UI_RegisterSequenceNode (uiBehaviour_t* behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "sequence";
	behaviour->draw = UI_SequenceNodeDraw;
	behaviour->properties = properties;
	behaviour->init = UI_SequenceNodeInit;
	behaviour->close = UI_SequenceNodeClose;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->propertyChanged = UI_SequencePropertyChanged;
	behaviour->leftClick = UI_SequenceNodeLeftClick;

	propertySource = UI_GetPropertyFromBehaviour(behaviour, "src");
}
