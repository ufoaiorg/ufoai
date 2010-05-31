/**
 * @file m_node_selectbox.c
 * @todo manage disabled option
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

#include "../m_main.h"
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_draw.h"
#include "../m_data.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"

#define EXTRADATA_TYPE abstractOptionExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)

const nodeBehaviour_t *abstractOptionBehaviour;

/**
 * @brief Sort options by alphabet
 */
void MN_OptionNodeSortOptions (menuNode_t *node)
{
	menuNode_t *option;
	assert(MN_NodeInstanceOf(node, "abstractoption"));
	MN_SortOptions(&node->firstChild);

	/** update lastChild */
	/** TODO the sort option should do it itself */
	option = node->firstChild;
	while (option->next)
		option = option->next;
	node->lastChild = option;
}

static void MN_UpdateOption_f (void)
{
	menuNode_t *node;
	menuNode_t *option;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <optionname> <hide|display|enable|disable>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_UpdateOption_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}

	if (!MN_NodeInstanceOf(node, "abstractoption")) {
		Com_Printf("MN_UpdateOption_f: '%s' node is not an 'abstractoption'.\n", Cmd_Argv(1));
		return;
	}

	option = node->firstChild;
	while (option) {
		if (!strcmp(option->name, Cmd_Argv(2)))
			break;
		option = option->next;
	}
	if (option == NULL) {
		Com_Printf("MN_UpdateOption_f: option '%s' from '%s' node not found.\n", Cmd_Argv(2), Cmd_Argv(1));
		return;
	}

	if (!strcmp("disable", Cmd_Argv(3))) {
		option->disabled = qtrue;
	} else if (!strcmp("enable", Cmd_Argv(3))) {
		option->disabled = qfalse;
	} else if (!strcmp("hide", Cmd_Argv(3))) {
		option->invis = qtrue;
	} else if (!strcmp("display", Cmd_Argv(3))) {
		option->invis = qfalse;
	} else {
		Com_Printf("MN_AddListener_f: '%s' command do not exists.\n", Cmd_Argv(3));
		return;
	}
}

static const value_t properties[] = {
	/** Optional. Data ID we want to use. It must be an option list. It substitute to the inline options */
	{"dataid", V_UI_DATAID, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, dataId), MEMBER_SIZEOF(EXTRADATA_TYPE, dataId)},
	/** Optional. We can define the height of the block containing an option. */
	{"lineheight", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, lineHeight),  MEMBER_SIZEOF(EXTRADATA_TYPE, lineHeight)},

	/* position of the vertical view (into the full number of elements the node contain) */
	{"viewpos", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.viewPos),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.viewPos)},
	/* size of the vertical view (proportional to the number of elements the node can display without moving) */
	{"viewsize", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.viewSize),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.viewSize)},
	/* full vertical size (proportional to the number of elements the node contain) */
	{"fullsize", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, scrollY.fullSize),  MEMBER_SIZEOF(EXTRADATA_TYPE, scrollY.fullSize)},

	/* number of elements contain the node */
	{"count", V_INT, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, count),  MEMBER_SIZEOF(EXTRADATA_TYPE, count)},

	/* Define the cvar containing the value of the current selected option */
	{"cvar", V_UI_CVAR, offsetof(menuNode_t, cvar), 0},

	/* Called when one of the properties viewpos/viewsize/fullsize change */
	{"onviewchange", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, onViewChange), MEMBER_SIZEOF(EXTRADATA_TYPE, onViewChange)},

	{NULL, V_NULL, 0, 0}
};

static void MN_AbstractOptionDoLayout (menuNode_t *node) {
	menuNode_t *option = node->firstChild;
	int count = 0;

	if (EXTRADATA(node).dataId == 0) {
		while(option && option->behaviour == optionBehaviour) {
			MN_Validate(option);
			if (!option->invis)
				count++;
			option = option->next;
		}

		EXTRADATA(node).count = count;
	}

	node->invalidated = qfalse;
}

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
menuNode_t*  MN_AbstractOptionGetFirstOption (menuNode_t * node)
{
	if (node->firstChild && node->firstChild->behaviour == optionBehaviour) {
		return node->firstChild;
	} else {
		const int v = MN_GetDataVersion(EXTRADATA(node).dataId);
		if (v != EXTRADATA(node).dataId) {
			int count = 0;
			menuNode_t *option = MN_GetOption(EXTRADATA(node).dataId);
			while (option) {
				if (option->invis == qfalse)
					count++;
				option = option->next;
			}
			EXTRADATA(node).count = count;
			EXTRADATA(node).versionId = v;
		}
		return MN_GetOption(EXTRADATA(node).dataId);
	}
}

void MN_RegisterAbstractOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->drawItselfChild = qtrue;
	behaviour->doLayout = MN_AbstractOptionDoLayout;
	Cmd_AddCommand("mn_updateoption", MN_UpdateOption_f, "Update some option status");
	abstractOptionBehaviour = behaviour;
}
